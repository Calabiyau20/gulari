#include "PluginProcessor.h"
#if ! PG_HEADLESS
 #include "PluginEditor.h"
#endif

using namespace pg;

SynthgularityProcessor::SynthgularityProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Synthgularity", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
    {
        raw[(size_t) i] = apvts.getRawParameterValue (idOf (i));
        params[(size_t) i] = apvts.getParameter (idOf (i));
        jassert (raw[(size_t) i] != nullptr);
        for (auto& c : orbCorners) c[(size_t) i].store (0.0f);
    }
    std::atomic_store (&sample, makeDefaultSample());
    builtInWavetables();   // build the tables now, not on the audio thread
    refreshCache();
    startTimerHz (2);
}

SynthgularityProcessor::~SynthgularityProcessor() { stopTimer(); }

bool SynthgularityProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void SynthgularityProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate, samplesPerBlock);
    fx.prepare (sampleRate, samplesPerBlock);
    work.setSize (2, samplesPerBlock);
}

void SynthgularityProcessor::refreshCache()
{
    const bool orb = raw[orb_on]->load() > 0.5f;
    int filled = 0;
    for (auto& f : orbFilled) filled += f.load() ? 1 : 0;

    if (! orb || filled == 0)
    {
        for (int i = 0; i < kNumParams; ++i) cache[(size_t) i] = raw[(size_t) i]->load (std::memory_order_relaxed);
        return;
    }

    // bilinear weights: corner 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right (y = 1 is top)
    const float x = raw[orb_x]->load(), y = raw[orb_y]->load();
    const float w[4] = { (1 - x) * y, x * y, (1 - x) * (1 - y), x * (1 - y) };
    int nearest = 0;
    for (int c = 1; c < 4; ++c) if (w[c] > w[nearest]) nearest = c;

    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& info = paramInfos()[(size_t) i];
        auto* prm = params[(size_t) i];
        const float current = prm->convertTo0to1 (raw[(size_t) i]->load (std::memory_order_relaxed));
        if (i == orb_on || i == orb_x || i == orb_y || i == polyphony || i == voice_mode)
        {
            cache[(size_t) i] = raw[(size_t) i]->load();
            continue;
        }
        auto cornerVal = [&] (int c) { return orbFilled[(size_t) c].load() ? orbCorners[(size_t) c][(size_t) i].load (std::memory_order_relaxed) : current; };
        float norm;
        if (info.type == PType::Float)
        {
            norm = 0.0f;
            for (int c = 0; c < 4; ++c) norm += w[c] * cornerVal (c);
        }
        else
            norm = cornerVal (nearest);   // menus and switches follow the closest corner
        cache[(size_t) i] = prm->convertFrom0to1 (clamp01 (norm));
    }
}

void SynthgularityProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    refreshCache();

    auto smp = std::atomic_load (&sample);
    auto ta = getActiveTable (0), tb = getActiveTable (1);

    juce::Optional<juce::AudioPlayHead::PositionInfo> pos;
    if (auto* ph = getPlayHead()) pos = ph->getPosition();

    synth.render (buffer, midi, cache.data(), smp.get(), ta.get(), tb.get(), pos ? &*pos : nullptr);

    const int strums = synth.noteOnCounter.load();
    const bool strum = strums != lastStrumCount;
    lastStrumCount = strums;
    fx.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, cache.data(), synth.global, synth.lastNote.load(), strum);

    const float gain = dbToGain (cache[master_gain]);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float v = d[i] * gain;
            if (! std::isfinite (v)) v = 0.0f;
            // transparent below -3 dBFS, soft above
            const float a = std::abs (v);
            if (a > 0.7f) v = std::copysign (0.7f + 0.3f * fastTanh ((a - 0.7f) / 0.3f), v);
            d[i] = v;
        }
    }
}

// ---------------------------------------------------------------------------
// Sources
// ---------------------------------------------------------------------------
bool SynthgularityProcessor::loadSample (const juce::File& file, juce::String& error)
{
    auto s = loadSampleFile (file, error);
    if (s == nullptr) return false;
    auto old = std::atomic_exchange (&sample, s);
    std::lock_guard<std::mutex> lk (graveyardLock);
    graveyard.push_back (old);
    return true;
}

bool SynthgularityProcessor::loadUserWavetable (int osc, const juce::File& file, juce::String& error)
{
    auto t = loadWavetableFile (file, error);
    if (t == nullptr) return false;
    auto old = std::atomic_exchange (&userTable[osc & 1], t);
    std::lock_guard<std::mutex> lk (graveyardLock);
    graveyard.push_back (old);
    // switch the oscillator to the User table
    if (auto* prm = apvts.getParameter (osc == 0 ? "wtA_table" : "wtB_table"))
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (choices::wavetables().size() - 1)));
    return true;
}

WavetablePtr SynthgularityProcessor::getActiveTable (int osc) const
{
    const int idx = (int) cache[osc == 0 ? wtA_table : wtB_table];
    const auto& b = builtInWavetables();
    if (idx >= (int) b.size())
    {
        auto u = std::atomic_load (&userTable[osc & 1]);
        return u != nullptr ? u : b[0];
    }
    return b[(size_t) juce::jmax (0, idx)];
}

void SynthgularityProcessor::timerCallback()
{
    std::lock_guard<std::mutex> lk (graveyardLock);
    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(), [] (const auto& p) { return p.use_count() <= 1; }), graveyard.end());
}

// ---------------------------------------------------------------------------
// ORB
// ---------------------------------------------------------------------------
void SynthgularityProcessor::captureCorner (int c)
{
    for (int i = 0; i < kNumParams; ++i)
        orbCorners[(size_t) c][(size_t) i].store (params[(size_t) i]->getValue());
    orbFilled[(size_t) c].store (true);
}

void SynthgularityProcessor::clearCorner (int c) { orbFilled[(size_t) c].store (false); }

bool SynthgularityProcessor::loadPresetIntoCorner (int c, const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    auto prm = tree.getChildWithName (apvts.state.getType());
    if (! prm.isValid()) return false;
    // start from the current sound, overwrite with the preset's parameters
    for (int i = 0; i < kNumParams; ++i)
        orbCorners[(size_t) c][(size_t) i].store (params[(size_t) i]->getValue());
    for (auto child : prm)
    {
        const int idx = indexOf (child.getProperty ("id").toString());
        if (idx >= 0)
            orbCorners[(size_t) c][(size_t) idx].store (params[(size_t) idx]->convertTo0to1 ((float) child.getProperty ("value")));
    }
    orbFilled[(size_t) c].store (true);
    return true;
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
juce::ValueTree SynthgularityProcessor::buildState()
{
    juce::ValueTree root ("SynthgularityState");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("presetName", currentPresetName, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (synth.steps.toTree(), nullptr);

    juce::ValueTree orb ("Orb");
    for (int c = 0; c < 4; ++c)
    {
        juce::ValueTree corner ("Corner");
        corner.setProperty ("index", c, nullptr);
        corner.setProperty ("filled", orbFilled[(size_t) c].load(), nullptr);
        if (orbFilled[(size_t) c].load())
        {
            juce::StringArray v;
            for (int i = 0; i < kNumParams; ++i) v.add (idOf (i) + "=" + juce::String (orbCorners[(size_t) c][(size_t) i].load(), 5));
            corner.setProperty ("values", v.joinIntoString (";"), nullptr);
        }
        orb.appendChild (corner, nullptr);
    }
    root.appendChild (orb, nullptr);

    juce::ValueTree src ("Sources");
    if (auto s = getSample()) src.setProperty ("sample", s->path, nullptr);
    for (int o = 0; o < 2; ++o)
        if (auto t = std::atomic_load (&userTable[o])) src.setProperty (o == 0 ? "userTableA" : "userTableB", t->name, nullptr);
    root.appendChild (src, nullptr);
    return root;
}

void SynthgularityProcessor::applyState (const juce::ValueTree& root, bool includeOrb)
{
    auto prm = root.getChildWithName (apvts.state.getType());
    if (prm.isValid()) apvts.replaceState (prm);
    synth.steps.fromTree (root.getChildWithName ("Steps"));
    currentPresetName = root.getProperty ("presetName", "Init").toString();

    if (includeOrb)
    {
        for (auto& f : orbFilled) f.store (false);
        for (auto corner : root.getChildWithName ("Orb"))
        {
            const int c = juce::jlimit (0, 3, (int) corner.getProperty ("index"));
            if (! (bool) corner.getProperty ("filled")) continue;
            for (auto& kv : juce::StringArray::fromTokens (corner.getProperty ("values").toString(), ";", ""))
            {
                const int idx = indexOf (kv.upToFirstOccurrenceOf ("=", false, false));
                if (idx >= 0) orbCorners[(size_t) c][(size_t) idx].store (kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
            }
            orbFilled[(size_t) c].store (true);
        }
    }

    auto src = root.getChildWithName ("Sources");
    const juce::String path = src.getProperty ("sample").toString();
    if (path.isNotEmpty())
    {
        juce::File f (path);
        juce::String err;
        if (f.existsAsFile()) loadSample (f, err);
    }
    else
    {
        auto old = std::atomic_exchange (&sample, makeDefaultSample());
        std::lock_guard<std::mutex> lk (graveyardLock);
        graveyard.push_back (old);
    }
}

void SynthgularityProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = buildState().createXml()) copyXmlToBinary (*xml, dest);
}

void SynthgularityProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        applyState (juce::ValueTree::fromXml (*xml), true);
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------
juce::File SynthgularityProcessor::presetFolder() const
{
    auto f = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                 .getChildFile (PG_COMPANY_NAME).getChildFile (PG_PRODUCT_NAME).getChildFile ("Presets");
    f.createDirectory();
    return f;
}

juce::Array<juce::File> SynthgularityProcessor::listPresets() const
{
    auto files = presetFolder().findChildFiles (juce::File::findFiles, true, "*.sgpreset");
    files.sort();
    return files;
}

bool SynthgularityProcessor::savePreset (const juce::String& name, juce::String& error)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty()) { error = "Type a name for the preset."; return false; }
    currentPresetName = clean;
    auto xml = buildState().createXml();
    auto file = presetFolder().getChildFile (clean + ".sgpreset");
    if (! xml->writeTo (file)) { error = "Could not write " + file.getFullPathName(); return false; }
    return true;
}

bool SynthgularityProcessor::loadPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;
    applyState (juce::ValueTree::fromXml (*xml), true);
    currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

void SynthgularityProcessor::initPreset()
{
    for (int i = 0; i < kNumParams; ++i)
        params[(size_t) i]->setValueNotifyingHost (params[(size_t) i]->getDefaultValue());
    synth.steps.reset();
    for (auto& f : orbFilled) f.store (false);
    currentPresetName = "Init";
}

juce::StringArray SynthgularityProcessor::factoryPresetNames()
{
    return { "Init", "GRAIN - Cloud Pad", "GRAIN - Pitched Choir", "GRAIN - Frozen Scan", "GRAIN - Glitch Rain",
             "BASS - Psy Bite", "LEAD - Hyper Reese", "ARP - Plaits Chiptune", "ATMOS - Ringing Cave", "ATMOS - Clouds Drift" };
}

void SynthgularityProcessor::loadFactoryPreset (int index)
{
    initPreset();
    auto set = [&] (const char* id, float value)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (value));
    };
    auto route = [&] (int slot, int src, int dst, float amt)
    {
        const juce::String s = juce::String (slot).paddedLeft ('0', 2);
        set (("mtx" + s + "_src").toRawUTF8(), (float) src);
        set (("mtx" + s + "_dst").toRawUTF8(), (float) dst);
        set (("mtx" + s + "_amt").toRawUTF8(), amt);
    };

    switch (index)
    {
        case 1:   // Cloud Pad
            set ("gr_size", 220); set ("gr_density", 40); set ("gr_spray", 0.2f); set ("gr_stereo", 0.9f); set ("gr_scan", 0.05f);
            set ("amp_a", 1.2f); set ("amp_r", 3.0f); set ("flt_cutoff", 4500); set ("fx_dl_on", 1); set ("fx_gr_on", 1);
            set ("fx_gr_mix", 0.35f); set ("fx_gr_reverb", 0.6f);
            set ("lfo1_rate", 0.15f); route (1, MS_Lfo1, MD_GrPos, 0.15f);
            break;
        case 2:   // Pitched Choir
            set ("gr_mode", 1); set ("gr_size", 40); set ("gr_window", 3); set ("gr_stereo", 0.6f); set ("gr_scan", 0.02f);
            set ("amp_a", 0.4f); set ("amp_r", 1.5f); set ("fx_st_on", 1); set ("fx_rn_on", 0); set ("fx_dl_on", 1); set ("fx_dl_mix", 0.18f);
            set ("lfo2_rate", 0.3f); route (1, MS_Lfo2, MD_GrPos, 0.08f); route (2, MS_ModWheel, MD_GrSize, 0.8f);
            break;
        case 3:   // Frozen Scan
            set ("gr_scan", 0.0f); set ("gr_size", 500); set ("gr_density", 18); set ("gr_spray", 0.02f); set ("gr_reverse", 0.3f);
            set ("amp_a", 2.0f); set ("amp_r", 5.0f); set ("fx_gr_on", 1); set ("fx_gr_mode", 0); set ("fx_gr_mix", 0.5f); set ("fx_gr_freeze", 0);
            route (1, MS_Chaos1, MD_GrPos, 0.25f); set ("chaos1_rate", 0.3f); set ("chaos1_smooth", 0.8f);
            break;
        case 4:   // Glitch Rain
            set ("gr_size", 25); set ("gr_density", 90); set ("gr_spray", 0.9f); set ("gr_prand", 7); set ("gr_window", 4);
            set ("gr_reverse", 0.5f); set ("bite_crush", 0.35f); set ("fx_dl_on", 1); set ("fx_dl_pp", 1); set ("fx_dl_fb", 0.55f);
            route (1, MS_Seq1, MD_GrDensity, 0.6f); set ("seq1_div", 4);
            break;
        case 5:   // Psy Bite bass
            set ("gr_on", 0); set ("wtA_on", 1); set ("wtA_table", 1); set ("wtA_pos", 0.3f); set ("wtA_oct", -1); set ("sub_level", 0.6f);
            set ("voice_mode", 1); set ("amp_a", 0.001f); set ("amp_d", 0.18f); set ("amp_s", 0.0f); set ("amp_r", 0.05f);
            set ("flt_cutoff", 400); set ("flt_res", 0.35f); set ("flt_env", 0.65f); set ("fenv_d", 0.12f); set ("fenv_s", 0.0f);
            set ("flt_model", 1); set ("bite_drive", 0.35f); set ("bite_soft", 0.6f); set ("arp_on", 1); set ("arp_div", 4); set ("arp_gate", 0.55f);
            break;
        case 6:   // Hyper Reese
            set ("gr_on", 0); set ("wtA_on", 1); set ("wtB_on", 1); set ("wtA_table", 0); set ("wtA_pos", 0.66f); set ("wtB_table", 0); set ("wtB_pos", 0.66f);
            set ("wtA_unison", 5); set ("wtA_detune", 0.35f); set ("wtB_unison", 5); set ("wtB_detune", 0.45f); set ("wtB_fine", 9);
            set ("voice_mode", 2); set ("glide", 0.08f); set ("flt_cutoff", 2500); set ("flt_res", 0.2f); set ("bite_fold", 0.2f);
            set ("fx_st_on", 1); set ("fx_ph_on", 1); set ("fx_ph_mix", 0.3f);
            break;
        case 7:   // Plaits chiptune arp
            set ("gr_on", 0); set ("pl_on", 1); set ("pl_engine", 7); set ("pl_harm", 0.3f); set ("pl_timbre", 0.6f);
            set ("arp_on", 1); set ("arp_mode", 2); set ("arp_oct", 2); set ("arp_div", 4); set ("amp_r", 0.2f);
            set ("fx_dl_on", 1); set ("fx_dl_div", 6);
            break;
        case 8:   // Ringing Cave
            set ("gr_size", 60); set ("gr_density", 12); set ("gr_spray", 0.4f); set ("amp_a", 0.6f); set ("amp_r", 3.0f);
            set ("fx_rn_on", 1); set ("fx_rn_model", 1); set ("fx_rn_mix", 0.7f); set ("fx_rn_structure", 0.55f);
            set ("fx_gr_on", 1); set ("fx_gr_reverb", 0.7f); set ("fx_gr_mix", 0.3f);
            break;
        case 9:   // Clouds drift
            set ("gr_size", 300); set ("gr_density", 8); set ("gr_spray", 0.5f); set ("gr_scan", 0.1f); set ("amp_a", 3.0f); set ("amp_r", 6.0f);
            set ("pl_on", 1); set ("pl_engine", 14); set ("pl_level", 0.35f);
            set ("fx_gr_on", 1); set ("fx_gr_mode", 2); set ("fx_gr_mix", 0.6f); set ("fx_gr_texture", 0.7f);
            route (1, MS_Lfo1, MD_GrfxPos, 0.4f); set ("lfo1_rate", 0.05f); set ("lfo1_retrig", 0);
            break;
        default: break;
    }
    currentPresetName = factoryPresetNames()[index];
}

juce::AudioProcessorEditor* SynthgularityProcessor::createEditor()
{
#if PG_HEADLESS
    return nullptr;
#else
    return new SynthgularityEditor (*this);
#endif
}

bool SynthgularityProcessor::hasEditor() const
{
#if PG_HEADLESS
    return false;
#else
    return true;
#endif
}

#if ! PG_HEADLESS && ! PG_NO_PLUGIN_ENTRY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SynthgularityProcessor(); }
#endif
