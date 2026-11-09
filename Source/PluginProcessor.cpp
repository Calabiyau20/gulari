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
    }
    std::atomic_store (&sample, makeDefaultSample());
    builtInWavetables();   // build the tables now, not on the audio thread
    builtInSpectra();
    builtInShots();
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
    meter.prepare (sampleRate);
    work.setSize (2, samplesPerBlock);
}

void SynthgularityProcessor::refreshCache()
{
    for (int i = 0; i < kNumParams; ++i) cache[(size_t) i] = raw[(size_t) i]->load (std::memory_order_relaxed);
}

void SynthgularityProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    refreshCache();

    auto smp = std::atomic_load (&sample);
    WavetablePtr tabs[3] = { getActiveTable (0), getActiveTable (1), getActiveTable (2) };
    const Wavetable* tptr[3] = { tabs[0].get(), tabs[1].get(), tabs[2].get() };
    auto spec = getActiveSpectral();
    auto shot = getActiveShot();

    juce::Optional<juce::AudioPlayHead::PositionInfo> pos;
    if (auto* ph = getPlayHead()) pos = ph->getPosition();

    synth.render (buffer, midi, cache.data(), smp.get(), tptr, spec.get(), shot.get(), pos ? &*pos : nullptr);

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
    meter.process (buffer.getReadPointer (0), buffer.getReadPointer (1), n);
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

void SynthgularityProcessor::retire (std::shared_ptr<const void> p)
{
    std::lock_guard<std::mutex> lk (graveyardLock);
    graveyard.push_back (std::move (p));
}

static const char* const kTableIds[3] = { "wtA_table", "wtB_table", "wtC_table" };

bool SynthgularityProcessor::loadUserWavetable (int osc, const juce::File& file, juce::String& error)
{
    osc = juce::jlimit (0, 2, osc);
    auto t = loadWavetableFile (file, error);
    if (t == nullptr) return false;
    retire (std::atomic_exchange (&userTable[osc], t));
    userTablePath[osc] = file.getFullPathName();
    // switch the oscillator to the User table
    if (auto* prm = apvts.getParameter (kTableIds[osc]))
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (choices::wavetables().size() - 1)));
    return true;
}

WavetablePtr SynthgularityProcessor::getActiveTable (int osc) const
{
    static const int ids[3] = { wtA_table, wtB_table, wtC_table };
    osc = juce::jlimit (0, 2, osc);
    const int idx = (int) cache[(size_t) ids[osc]];
    const auto& b = builtInWavetables();
    if (idx >= (int) b.size())
    {
        auto u = std::atomic_load (&userTable[osc]);
        return u != nullptr ? u : b[0];
    }
    return b[(size_t) juce::jmax (0, idx)];
}

bool SynthgularityProcessor::loadSpectralFile (const juce::File& file, juce::String& error)
{
    auto t = analyseSpectralFile (file, error);
    if (t == nullptr) return false;
    retire (std::atomic_exchange (&userSpectral, t));
    spectralPath = file.getFullPathName();
    if (auto* prm = apvts.getParameter ("sp_sound"))
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (choices::spectralSounds().size() - 1)));
    return true;
}

SpectralPtr SynthgularityProcessor::getActiveSpectral() const
{
    const int idx = (int) cache[sp_sound];
    const auto& b = builtInSpectra();
    if (idx >= (int) b.size())
    {
        auto u = std::atomic_load (&userSpectral);
        return u != nullptr ? u : b[0];
    }
    return b[(size_t) juce::jmax (0, idx)];
}

bool SynthgularityProcessor::loadShot (const juce::File& file, juce::String& error)
{
    auto t = loadShotFile (file, error);
    if (t == nullptr) return false;
    retire (std::atomic_exchange (&userShot, t));
    shotPath = file.getFullPathName();
    if (auto* prm = apvts.getParameter ("smp_sound"))
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (choices::samplerSounds().size() - 1)));
    return true;
}

ShotPtr SynthgularityProcessor::getActiveShot() const
{
    const int idx = (int) cache[smp_sound];
    const auto& b = builtInShots();
    if (idx >= (int) b.size())
    {
        auto u = std::atomic_load (&userShot);
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
// State
// ---------------------------------------------------------------------------
juce::ValueTree SynthgularityProcessor::buildState()
{
    juce::ValueTree root ("SynthgularityState");
    root.setProperty ("version", 1, nullptr);
    root.setProperty ("presetName", currentPresetName, nullptr);
    root.appendChild (apvts.copyState(), nullptr);
    root.appendChild (synth.steps.toTree(), nullptr);

    juce::ValueTree src ("Sources");
    if (auto s = getSample()) src.setProperty ("sample", s->path, nullptr);
    static const char* const tk[3] = { "userTableA", "userTableB", "userTableC" };
    for (int o = 0; o < 3; ++o)
        if (userTablePath[o].isNotEmpty()) src.setProperty (tk[o], userTablePath[o], nullptr);
    if (spectralPath.isNotEmpty()) src.setProperty ("spectral", spectralPath, nullptr);
    if (shotPath.isNotEmpty()) src.setProperty ("shot", shotPath, nullptr);
    root.appendChild (src, nullptr);
    return root;
}

void SynthgularityProcessor::applyState (const juce::ValueTree& root)
{
    auto prm = root.getChildWithName (apvts.state.getType());
    if (prm.isValid()) apvts.replaceState (prm);
    synth.steps.fromTree (root.getChildWithName ("Steps"));
    currentPresetName = root.getProperty ("presetName", "Init").toString();

    auto src = root.getChildWithName ("Sources");
    const juce::String path = src.getProperty ("sample").toString();
    if (path.isNotEmpty())
    {
        juce::File f (path);
        juce::String err;
        if (f.existsAsFile()) loadSample (f, err);
    }
    else
        retire (std::atomic_exchange (&sample, makeDefaultSample()));

    // user files of the other oscillators (kept by path; the menu choice is restored by the parameters)
    auto reload = [&] (const char* key, auto&& loader)
    {
        const juce::String pth = src.getProperty (key).toString();
        juce::File f (pth);
        juce::String err;
        if (pth.isNotEmpty() && f.existsAsFile()) loader (f, err);
    };
    static const char* const tk[3] = { "userTableA", "userTableB", "userTableC" };
    for (int o = 0; o < 3; ++o)
        reload (tk[o], [this, o] (const juce::File& f, juce::String& e)
        {
            if (auto t = loadWavetableFile (f, e)) { retire (std::atomic_exchange (&userTable[o], t)); userTablePath[o] = f.getFullPathName(); }
        });
    reload ("spectral", [this] (const juce::File& f, juce::String& e)
    {
        if (auto t = analyseSpectralFile (f, e)) { retire (std::atomic_exchange (&userSpectral, t)); spectralPath = f.getFullPathName(); }
    });
    reload ("shot", [this] (const juce::File& f, juce::String& e)
    {
        if (auto t = loadShotFile (f, e)) { retire (std::atomic_exchange (&userShot, t)); shotPath = f.getFullPathName(); }
    });
}

void SynthgularityProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = buildState().createXml()) copyXmlToBinary (*xml, dest);
}

void SynthgularityProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        applyState (juce::ValueTree::fromXml (*xml));
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
    applyState (juce::ValueTree::fromXml (*xml));
    currentPresetName = file.getFileNameWithoutExtension();
    return true;
}

void SynthgularityProcessor::initPreset()
{
    for (int i = 0; i < kNumParams; ++i)
        params[(size_t) i]->setValueNotifyingHost (params[(size_t) i]->getDefaultValue());
    synth.steps.reset();
    currentPresetName = "Init";
}

juce::StringArray SynthgularityProcessor::factoryPresetNames()
{
    return { "Init", "GRAIN - Cloud Pad", "GRAIN - Pitched Choir", "GRAIN - Frozen Scan", "GRAIN - Glitch Rain",
             "BASS - Psy Bite", "LEAD - Hyper Reese", "ARP - Plaits Chiptune", "ATMOS - Ringing Cave", "ATMOS - Clouds Drift",
             "SPECTRAL - Choir Bloom", "SPECTRAL - Singularity", "BASS - Click Growl", "ARP - Dorian Lorenz" };
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
        case 10:   // Spectral choir
            set ("gr_on", 0); set ("sp_on", 1); set ("sp_sound", 3); set ("sp_scan", 0.15f); set ("sp_blur", 0.5f); set ("sp_width", 0.8f);
            set ("amp_a", 0.8f); set ("amp_r", 2.5f); set ("ff1_on", 1); set ("ff1_type", 5); set ("ff1_mix", 0.4f);
            set ("fx_rv_on", 1); set ("fx_rv_mix", 0.35f); set ("lfo1_rate", 0.2f); route (1, MS_Lfo1, MD_SpShift, 0.2f);
            break;
        case 11:   // Singularity
            set ("gr_on", 0); set ("sp_on", 1); set ("sp_sound", 19); set ("sp_shimmer", 0.4f); set ("sp_stretch", 0.15f);
            set ("amp_a", 1.5f); set ("amp_r", 4.0f); route (1, MS_Chaos2, MD_SpPos, 0.5f); set ("sp_pos", 0.5f); set ("chaos2_rate", 0.4f);
            set ("ff1_on", 1); set ("ff1_type", 1); set ("ff1_res", 0.6f); set ("ff1_mix", 0.35f); set ("ff1_poles", 2);
            set ("fx_rv_on", 1); set ("fx_rv_decay", 6.0f); set ("fx_rv_mix", 0.4f);
            break;
        case 12:   // Click growl bass
            set ("gr_on", 0); set ("wtA_on", 1); set ("wtA_table", 0); set ("wtA_pos", 0.6f); set ("wtA_oct", -1);
            set ("wtA_warp", 1); set ("wtA_wamt", 0.4f); set ("wtA_warp2", 6); set ("wtA_wamt2", 0.3f);
            set ("wtC_on", 1); set ("wtC_table", 0); set ("wtC_oct", -2); set ("wtC_level", 0.5f);
            set ("smp_on", 1); set ("smp_sound", 2); set ("smp_level", 0.6f); set ("smp_decay", 0.08f);
            set ("voice_mode", 1); set ("amp_a", 0.001f); set ("amp_d", 0.3f); set ("amp_s", 0.6f); set ("amp_r", 0.08f);
            set ("flt_cutoff", 900); set ("flt_env", 0.5f); set ("fenv_d", 0.15f); set ("fenv_s", 0.1f); set ("flt_model", 1);
            route (1, MS_ModEnv, MD_WtAWarp, 0.4f); set ("menv_d", 0.25f);
            break;
        case 13:   // Dorian arp, Lorenz modulated
            set ("gr_on", 0); set ("wtB_on", 1); set ("wtB_table", 2); set ("wtB_pos", 0.4f); set ("amp_d", 0.25f); set ("amp_s", 0.2f);
            set ("arp_on", 1); set ("arp_mode", 5); set ("arp_oct", 3); set ("arp_div", 4); set ("arp_scale", 2); set ("arp_root", 9);
            set ("chaos2_rate", 2.0f); route (1, MS_Chaos2, MD_Cutoff, 0.3f); route (2, MS_Chaos2Z, MD_WtBPos, 0.4f);
            set ("flt_cutoff", 3000); set ("flt_res", 0.3f); set ("fx_dl_on", 1); set ("fx_dl_div", 6); set ("fx_rv_on", 1);
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
