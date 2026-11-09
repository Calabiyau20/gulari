#include "RProcessor.h"
#include "RPresets.h"
#if ! CYR_HEADLESS
 #include "REditor.h"
#endif

using namespace cyr;

ResonatorProcessor::ResonatorProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CalabiYauResonator", createLayout())
{
    for (int i = 0; i < kNumR; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (idOf (i));
        jassert (raw[(size_t) i] != nullptr);
    }
    // the FX rack runs on a full Synthgularity parameter cache: defaults everywhere, hosted ids live
    for (int i = 0; i < pg::kNumParams; ++i)
    {
        const auto& inf = pg::paramInfos()[(size_t) i];
        pgCache[(size_t) i] = inf.def;
        if (isHostedFxParam (inf.id))
            if (auto* r = apvts.getRawParameterValue (inf.id)) hosted.push_back ({ i, r });
    }
    // the matrix of the FX chain must stay silent: no slot routes anywhere
    refresh();
    initPreset();
}

bool ResonatorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void ResonatorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    baseRate = sampleRate;
    maxBlock = juce::jmax (1, samplesPerBlock);
    for (int k = 0; k < 2; ++k)
    {
        os[k] = std::make_unique<juce::dsp::Oversampling<float>> (2, k + 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        os[k]->initProcessing ((size_t) maxBlock);
    }
    osMidi.ensureSize (4096);
    osMode = -1;
    refresh();
    applyQuality ((int) rc[quality]);
    fx.prepare (sampleRate, maxBlock);
    meter.prepare (sampleRate);
    masterGain.reset (sampleRate, 0.02);
    masterGain.setCurrentAndTargetValue (pg::dbToGain (rc[master]));
    gainRamp.assign ((size_t) maxBlock, 1.0f);
    fxSmoothReady = false;
}

void ResonatorProcessor::applyQuality (int mode)
{
    osMode = juce::jlimit (0, 2, mode);
    const int factor = 1 << osMode;
    synth.prepare (baseRate * factor, maxBlock * factor);
    if (osMode > 0) os[osMode - 1]->reset();
    setLatencySamples (osMode > 0 ? juce::roundToInt (os[osMode - 1]->getLatencyInSamples()) : 0);
}

void ResonatorProcessor::refresh()
{
    for (int i = 0; i < kNumR; ++i) rc[(size_t) i] = raw[(size_t) i]->load (std::memory_order_relaxed);
    // FX knobs glide toward their value (the rack reads them once per block)
    for (auto& h : hosted)
    {
        const float v = h.second->load (std::memory_order_relaxed);
        auto& c = pgCache[(size_t) h.first];
        const bool cont = pg::paramInfos()[(size_t) h.first].type == pg::PType::Float;
        c = (cont && fxSmoothReady) ? c + (v - c) * fxSmoothK : v;
    }
    fxSmoothReady = true;
}

void ResonatorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    refresh();
    juce::Optional<juce::AudioPlayHead::PositionInfo> pos;
    if (auto* ph = getPlayHead()) pos = ph->getPosition();

    const int want = juce::jlimit (0, 2, (int) rc[quality]);
    if (n > maxBlock) prepareToPlay (baseRate, n);
    else if (want != osMode) applyQuality (want);
    fxSmoothK = 1.0f - std::exp (-(float) n / (float) (baseRate * 0.015));

    if (osMode == 0)
        synth.render (buffer, midi, rc.data(), pos ? &*pos : nullptr);
    else
    {
        auto& o = *os[osMode - 1];
        const int factor = (int) o.getOversamplingFactor();
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) n);
        auto up = o.processSamplesUp (block);
        float* chans[2] = { up.getChannelPointer (0), up.getChannelPointer (1) };
        juce::AudioBuffer<float> ub (chans, 2, n * factor);
        ub.clear();
        osMidi.clear();
        for (const auto meta : midi) osMidi.addEvent (meta.getMessage(), meta.samplePosition * factor);
        synth.render (ub, osMidi, rc.data(), pos ? &*pos : nullptr);
        o.processSamplesDown (block);
    }

    // FX rack + MIXBUS (Synthgularity's chain)
    fxGlobal.bpm = synth.global.bpm;
    fxGlobal.beat = synth.global.beat;
    fxGlobal.modWheel = synth.global.modWheel;
    const int strums = synth.noteOnCounter.load();
    const bool strum = strums != lastStrum;
    lastStrum = strums;
    fx.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, pgCache.data(), fxGlobal, synth.lastNote.load(), strum);

    masterGain.setTargetValue (pg::dbToGain (rc[master]));
    for (int i = 0; i < n; ++i) gainRamp[(size_t) i] = masterGain.getNextValue();
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float v = d[i] * gainRamp[(size_t) i];
            if (! std::isfinite (v)) v = 0.0f;
            const float a = std::abs (v);
            if (a > 0.7f) v = std::copysign (0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f), v);
            d[i] = v;
        }
    }
    meter.process (buffer.getReadPointer (0), buffer.getReadPointer (1), n);
    int w = scopePos.load (std::memory_order_relaxed);
    const float* l = buffer.getReadPointer (0);
    const float* r = buffer.getReadPointer (1);
    for (int i = 0; i < n; ++i) { scopeL[(size_t) w] = l[i]; scopeR[(size_t) w] = r[i]; w = (w + 1) & (kScopeRing - 1); }
    scopePos.store (w, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Modulation slots
// ---------------------------------------------------------------------------
void ResonatorProcessor::setParam (const juce::String& id, float plain)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

int ResonatorProcessor::addModulation (int source, int paramIndex, float amount)
{
    const int dst = destIndexOfParam (paramIndex);
    if (source <= MS_None || source >= MS_Count || dst <= 0) return -1;
    for (int s = 0; s < kSlots; ++s)
        if ((int) value (slotSrc (s)) == source && (int) value (slotDst (s)) == dst) return s;   // already there
    for (int s = 0; s < kSlots; ++s)
        if ((int) value (slotSrc (s)) == MS_None || (int) value (slotDst (s)) == 0)
        {
            setParam (idOf (slotSrc (s)), (float) source);
            setParam (idOf (slotDst (s)), (float) dst);
            setParam (idOf (slotAmt (s)), amount);
            return s;
        }
    return -1;
}

void ResonatorProcessor::removeModulation (int slot)
{
    if (slot < 0 || slot >= kSlots) return;
    setParam (idOf (slotSrc (slot)), 0.0f);
    setParam (idOf (slotDst (slot)), 0.0f);
    setParam (idOf (slotAmt (slot)), 0.0f);
}

void ResonatorProcessor::setModAmount (int slot, float amount)
{
    if (slot >= 0 && slot < kSlots) setParam (idOf (slotAmt (slot)), juce::jlimit (-1.0f, 1.0f, amount));
}

juce::Array<int> ResonatorProcessor::slotsFor (int paramIndex) const
{
    juce::Array<int> out;
    const int dst = destIndexOfParam (paramIndex);
    if (dst <= 0) return out;
    for (int s = 0; s < kSlots; ++s)
        if ((int) value (slotDst (s)) == dst && (int) value (slotSrc (s)) > 0) out.add (s);
    return out;
}

// ---------------------------------------------------------------------------
// State & presets
// ---------------------------------------------------------------------------
void ResonatorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("skin", uiSkin, nullptr);
    state.setProperty ("presetName", currentPresetName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ResonatorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            uiSkin = (int) vt.getProperty ("skin", 0);
            currentPresetName = vt.getProperty ("presetName", "Init").toString();
            apvts.replaceState (vt);
        }
}

juce::StringArray ResonatorProcessor::factoryPresetNames()
{
    juce::StringArray a;
    for (auto& p : factoryPresets()) a.add (p.name);
    return a;
}

void ResonatorProcessor::initPreset()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    // a playable starting point: oscillator A on
    setParam ("osca_on", 1.0f);
    currentPresetName = "Init";
}

void ResonatorProcessor::loadFactoryPreset (int index)
{
    const auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    initPreset();
    setParam ("osca_on", 0.0f);
    for (auto& kv : list[(size_t) index].values) setParam (kv.first, kv.second);
    currentPresetName = list[(size_t) index].name;
}

juce::File ResonatorProcessor::presetFolder() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Calabi Yau").getChildFile ("Calabi-Yau Resonator").getChildFile ("Presets");
}

juce::Array<juce::File> ResonatorProcessor::listPresets() const
{
    auto files = presetFolder().findChildFiles (juce::File::findFiles, true, "*.cyrpreset");
    files.sort();
    return files;
}

bool ResonatorProcessor::savePreset (const juce::String& name)
{
    auto dir = presetFolder();
    dir.createDirectory();
    auto state = apvts.copyState();
    state.setProperty ("presetName", name, nullptr);
    auto xml = state.createXml();
    if (xml == nullptr) return false;
    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".cyrpreset");
    if (! xml->writeTo (file)) return false;
    currentPresetName = name;
    return true;
}

bool ResonatorProcessor::loadPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return false;
    auto vt = juce::ValueTree::fromXml (*xml);
    initPreset();
    for (int i = 0; i < vt.getNumChildren(); ++i)
    {
        auto c = vt.getChild (i);
        const auto id = c.getProperty ("id").toString();
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) c.getProperty ("value")));
    }
    currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

juce::AudioProcessorEditor* ResonatorProcessor::createEditor()
{
   #if CYR_HEADLESS
    return nullptr;
   #else
    return new ResonatorEditor (*this);
   #endif
}

#if ! CYR_HEADLESS && ! CYR_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ResonatorProcessor(); }
#endif
