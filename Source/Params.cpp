#include "Params.h"

namespace pg
{
// knobs whose drop goes to an existing per-voice (or FX) destination: same 0..1 meaning as the knob
int mdForParam (int pid)
{
    switch (pid)
    {
        case wtA_pos: return MD_WtAPos;   case wtA_level: return MD_WtALevel; case wtA_wamt: return MD_WtAWarp;
        case wtB_pos: return MD_WtBPos;   case wtB_level: return MD_WtBLevel; case wtB_wamt: return MD_WtBWarp;
        case wtC_pos: return MD_WtCPos;   case wtC_level: return MD_WtCLevel; case wtC_wamt: return MD_WtCWarp;
        case sp_pos: return MD_SpPos;     case sp_level: return MD_SpLevel;   case sp_wamt: return MD_SpWarp;
        case gr_pos: return MD_GrPos;     case gr_spray: return MD_GrSpray;   case gr_level: return MD_GrLevel;
        case fm_amount: return MD_FmAmount; case sub_level: return MD_SubLevel; case noise_level: return MD_NoiseLevel;
        case smp_level: return MD_SmpLevel;
        case pl_harm: return MD_PlHarm;   case pl_timbre: return MD_PlTimbre; case pl_morph: return MD_PlMorph; case pl_level: return MD_PlLevel;
        case bite_drive: return MD_BiteDrive; case bite_fold: return MD_BiteFold; case bite_crush: return MD_BiteCrush;
        case flt_cutoff: return MD_Cutoff; case flt_res: return MD_Resonance;
        case fx_dl_mix: return MD_DelayMix; case fx_dl_fb: return MD_DelayFb;
        case fx_rn_structure: return MD_RnStructure; case fx_rn_bright: return MD_RnBright; case fx_rn_pos: return MD_RnPos;
        case fx_gr_pos: return MD_GrfxPos; case fx_gr_size: return MD_GrfxSize; case fx_gr_density: return MD_GrfxDensity;
        case fx_gr_texture: return MD_GrfxTexture; case fx_gr_mix: return MD_GrfxMix;
        case fx_fl_mix: return MD_FlMix;  case fx_ph_mix: return MD_PhMix;
        case fx_rv_size: return MD_RvSize; case fx_rv_mix: return MD_RvMix;
        case mb_dj_pos: return MD_MbDjFilter; case mb_tg_depth: return MD_MbGateDepth; case mb_tp_drive: return MD_MbTapeDrive;
        default: return MD_None;
    }
}

// every continuous parameter that drag and drop / the matrix may move, straight from the list (no paramInfos(),
// which itself needs the size of the destination list)
static bool genericAllowed (const char* id)
{
    const juce::String s (id);
    for (auto* pre : { "mtx", "seq", "arp_", "master_gain_unused" })
        if (s.startsWith (pre)) return false;
    return true;
}

static const std::vector<std::pair<int, juce::String>>& genericCandidates()
{
    static const std::vector<std::pair<int, juce::String>> v = []
    {
        std::vector<std::pair<int, juce::String>> out;
       #define GC_F(id, nm, mn, mx, df, sk) if (genericAllowed (#id) && mdForParam (id) == MD_None) out.push_back ({ (int) id, juce::String (nm) });
       #define GC_I(id, nm, mn, mx, df)     if (genericAllowed (#id) && (juce::String (#id).endsWith ("_oct") || juce::String (#id).endsWith ("_semi"))) out.push_back ({ (int) id, juce::String (nm) });
       #define GC_X(...)
        PG_PARAM_LIST (GC_F, GC_I, GC_X, GC_X)
       #undef GC_F
       #undef GC_I
       #undef GC_X
        return out;
    }();
    return v;
}

namespace choices
{
    juce::StringArray voiceModes()    { return { "Poly", "Mono", "Legato" }; }
    juce::StringArray wavetables()    { return { "Basic", "PWM", "Harmonics", "Formant", "Sync", "FM", "Fold", "Digital", "User" }; }
    juce::StringArray warpModes()
    {
        return { "Off", "Bend +", "Bend -", "Bend +/-", "Bloat +", "Bloat -", "Hard Sync", "Sync Window", "Pulse Width",
                 "Phase FM", "FM Window", "Flip", "Mirror", "Breed", "Brew", "Sinc", "Crush", "Smooth", "Window" };
    }
    juce::StringArray grainModShapes() { return { "Sine", "Triangle", "Saw", "Square" }; }
    juce::StringArray grainWindows()  { return { "Hann", "Triangle", "Tukey", "Gauss", "Perc", "Swell" }; }
    juce::StringArray grainModes()    { return { "Cloud", "Pitched" }; }
    juce::StringArray subWaves()      { return { "Sine", "Triangle", "Square" }; }
    juce::StringArray noiseTypes()    { return { "White", "Pink", "Crackle" }; }
    juce::StringArray plaitsEngines()
    {
        return { "VA + VCF", "Phase Distortion", "6-OP FM 1", "6-OP FM 2", "6-OP FM 3", "Wave Terrain",
                 "String Machine", "Chiptune", "Virtual Analog", "Waveshaping", "2-OP FM", "Granular Formant",
                 "Harmonic", "Wavetable", "Chords", "Speech", "Swarm", "Filtered Noise", "Particle Noise",
                 "Inharmonic String", "Modal Resonator", "Bass Drum", "Snare Drum", "Hi-Hat" };
    }
    juce::StringArray fmModes()       { return { "Phase Mod", "Thru-Zero FM", "Phase Distortion", "Amplitude Mod", "Ring Mod" }; }
    juce::StringArray filterTypes()   { return { "Low Pass", "Band Pass", "High Pass", "Notch" }; }
    juce::StringArray filterModels()  { return { "Clean SVF", "Ladder" }; }
    juce::StringArray filterSlopes()  { return { "12 dB", "24 dB" }; }
    juce::StringArray lfoShapes()     { return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth Random" }; }
    juce::StringArray divisions()
    {
        return { "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/16D", "1/8T", "1/8", "1/8D",
                 "1/4T", "1/4", "1/4D", "1/2", "1/1", "2/1", "4/1" };
    }
    double divisionInBeats (int i)
    {
        static const double b[] = { 1.0 / 16, 1.0 / 12, 1.0 / 8, 1.0 / 6, 1.0 / 4, 3.0 / 8, 1.0 / 3, 1.0 / 2, 3.0 / 4,
                                    2.0 / 3, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0 };
        return b[juce::jlimit (0, 15, i)];
    }
    juce::StringArray retrigModes()   { return { "Free", "Note" }; }
    juce::StringArray modSources()
    {
        return { "None", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Chaos 1", "Chaos 2 Lorenz X", "Chaos 2 Lorenz Y", "Chaos 2 Lorenz Z",
                 "Seq 1", "Seq 2", "Seq 3", "Seq 4", "Seq 5", "Seq 6", "Seq 7", "Seq 8",
                 "Mod Env 1", "Filter Env", "Amp Env", "Velocity", "Note", "Mod Wheel",
                 "Aftertouch", "Pitch Bend", "Random", "Mod Env 2", "Mod Env 3", "Mod Env 4" };
    }
    juce::StringArray modDestsClassic()
    {
        return { "None",
                 "WT A Position", "WT A Pitch", "WT A Level", "WT A Warp",
                 "WT B Position", "WT B Pitch", "WT B Level", "WT B Warp",
                 "WT C Position", "WT C Pitch", "WT C Level", "WT C Warp",
                 "Spectral Position", "Spectral Pitch", "Spectral Tilt", "Spectral Stretch", "Spectral Shift", "Spectral Level", "Spectral Warp",
                 "Grain Position", "Grain Size", "Grain Density", "Grain Pitch", "Grain Spray", "Grain Level",
                 "FM Amount", "Sub Level", "Noise Level", "Sampler Pitch", "Sampler Level",
                 "Plaits Harmonics", "Plaits Timbre", "Plaits Morph", "Plaits Level",
                 "Distortion Drive", "Distortion Fold", "Distortion Crush",
                 "Cutoff", "Resonance", "Pan", "Volume",
                 "FX Delay Mix", "FX Delay Feedback", "FX Delay Time", "FX Resonator Structure", "FX Resonator Brightness",
                 "FX Resonator Position", "FX Clouds Position", "FX Clouds Size", "FX Clouds Density",
                 "FX Clouds Texture", "FX Clouds Mix", "FX Flanger Mix", "FX Phaser Mix",
                 "FX Filter 1 Freq", "FX Filter 1 Reso", "FX Filter 1 Morph", "FX Filter 2 Freq", "FX Filter 2 Reso", "FX Filter 2 Morph",
                 "FX Reverb Size", "FX Reverb Mix", "MixBus DJ Filter", "MixBus Gate Depth", "MixBus Tape Drive",
                 "MixBus Crusher Bits", "MixBus Width" };
    }
    // the classic destinations plus every continuous parameter (listed by name)
    juce::StringArray modDests()
    {
        auto a = modDestsClassic();
        for (auto& g : genericCandidates()) a.add (g.second);
        return a;
    }
    juce::StringArray modPolarity()   { return { "Native", "Unipolar", "Bipolar" }; }
    juce::StringArray seqModes()      { return { "Unipolar", "Bipolar" }; }
    juce::StringArray arpModes()      { return { "Up", "Down", "Up/Down", "Down/Up", "As Played", "Random", "Chord" }; }
    juce::StringArray ringsModels()   { return { "Modal", "Sympathetic", "String", "FM Voice", "Sympathetic Quantized", "String + Reverb" }; }
    juce::StringArray ringsPoly()     { return { "1", "2", "4" }; }
    juce::StringArray cloudsModes()   { return { "Granular", "Looping Delay", "Spectral" }; }
    juce::StringArray cloudsQuality() { return { "16-bit Stereo", "16-bit Mono", "8-bit Stereo", "8-bit Mono" }; }
    juce::StringArray seqDirections() { return { "Forward", "Backward", "Ping-Pong", "Random", "Even Steps", "Odd Steps", "Evens > Odds" }; }
    juce::StringArray scales()
    {
        return { "Off (Chromatic)", "Ionian (Major)", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian (Minor)", "Locrian",
                 "Harmonic Minor", "Melodic Minor", "Phrygian Dominant", "Hungarian Minor", "Double Harmonic",
                 "Major Pentatonic", "Minor Pentatonic", "Blues", "Whole Tone", "Hirajoshi", "In Sen" };
    }
    juce::StringArray noteNames()     { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
    juce::StringArray fxTypes()       { return { "Empty", "Filter 1", "Filter 2", "Stereo", "Flanger", "Phaser", "Delay", "Resonator", "Clouds", "Reverb" }; }
    juce::StringArray filterFxTypes() { return { "Filterbank", "Comb +", "Comb -", "Morph LP>BP>HP", "Morph LP>Notch>HP", "Formant (Vowel)", "Allpass" }; }
    juce::StringArray spectralSounds()
    {
        return { "Saw Stack", "Hollow Square", "Drawbar Organ", "Choir Ah-Oh", "Vocal Ee-Oo", "Bell Bloom", "Glass Harmonics",
                 "Brass Swell", "String Section", "Pluck Decay", "Bass Growl", "Pad Bloom", "Formant Sweep", "Comb Ripple",
                 "Prime Partials", "Fibonacci", "Odd Shimmer", "Spectral Wind", "Metal Plate", "Singularity", "User File" };
    }
    juce::StringArray samplerSounds()
    {
        return { "Click", "Tick", "Kick Click", "Rim", "Snap", "Noise Burst", "Hat", "Zap", "Blip", "Metal Hit",
                 "Crackle", "Breath", "User File" };
    }
    juce::StringArray spectralWarps()  { return { "Off", "Shift", "Fold", "Sieve", "Comb", "Spread", "Mirror", "Squeeze", "Formant Lock" }; }
    juce::StringArray mixbusTypes()    { return { "Empty", "Tape", "EQ", "Compressor", "Chorus", "Imager", "Trance Gate", "DJ Filter", "Bitcrusher", "Transient", "Limiter" }; }
    juce::StringArray gatePatterns()   { return { "Straight", "Offbeat", "Psy Roll", "Triplet Feel", "3+3+2", "Stutter", "Half Time", "Syncopated" }; }
    juce::StringArray routeOrders()    { return { "Rack > MixBus", "MixBus > Rack" }; }
    juce::StringArray oversamplingModes() { return { "Off", "4x", "8x" }; }
    juce::StringArray distortionPositions() { return { "Pre Filter", "Post Filter" }; }
    juce::StringArray chaosTypes()    { return { "Rossler", "Henon", "Logistic", "Duffing" }; }
}

static std::vector<ParamInfo> buildInfos()
{
    std::vector<ParamInfo> v;
   #define PG_INFO_F(id, nm, mn, mx, df, sk) v.push_back ({ #id, nm, PType::Float, mn, mx, df });
   #define PG_INFO_I(id, nm, mn, mx, df)     v.push_back ({ #id, nm, PType::Int, (float) mn, (float) mx, (float) df });
   #define PG_INFO_C(id, nm, list, df)       v.push_back ({ #id, nm, PType::Choice, 0.0f, (float) (choices::list().size() - 1), (float) df });
   #define PG_INFO_B(id, nm, df)             v.push_back ({ #id, nm, PType::Bool, 0.0f, 1.0f, df ? 1.0f : 0.0f });
    PG_PARAM_LIST (PG_INFO_F, PG_INFO_I, PG_INFO_C, PG_INFO_B)
   #undef PG_INFO_F
   #undef PG_INFO_I
   #undef PG_INFO_C
   #undef PG_INFO_B
    jassert ((int) v.size() == kNumParams);
    return v;
}

const std::vector<int>& genericModParams()
{
    static const std::vector<int> v = []
    {
        std::vector<int> out;
        for (auto& g : genericCandidates()) out.push_back (g.first);
        return out;
    }();
    return v;
}

int destForParam (int pid)
{
    const int md = mdForParam (pid);
    if (md != MD_None) return md;
    const auto& g = genericModParams();
    for (size_t i = 0; i < g.size(); ++i) if (g[i] == pid) return MD_Count + (int) i;
    return -1;
}

int paramForDest (int dst)
{
    if (dst >= MD_Count)
    {
        const auto& g = genericModParams();
        return dst - MD_Count < (int) g.size() ? g[(size_t) (dst - MD_Count)] : -1;
    }
    static const std::vector<int> back = []
    {
        std::vector<int> b ((size_t) MD_Count, -1);
        for (int i = 0; i < kNumParams; ++i) { const int md = mdForParam (i); if (md != MD_None) b[(size_t) md] = i; }
        return b;
    }();
    return dst > 0 && dst < MD_Count ? back[(size_t) dst] : -1;
}

const std::vector<ParamInfo>& paramInfos()
{
    static const std::vector<ParamInfo> infos = buildInfos();
    return infos;
}

int indexOf (const juce::String& id)
{
    static const std::unordered_map<juce::String, int> map = []
    {
        std::unordered_map<juce::String, int> m;
        for (int i = 0; i < kNumParams; ++i)
            m[paramInfos()[(size_t) i].id] = i;
        return m;
    }();
    auto it = map.find (id);
    return it == map.end() ? -1 : it->second;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    addParamsTo (layout, [] (const juce::String&) { return true; });
    return layout;
}

void addParamsTo (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const std::function<bool (const juce::String&)>& keep)
{
    using namespace juce;
    const int version = 1;

   #define PG_LAYOUT_F(id, nm, mn, mx, df, sk) \
    if (keep (#id)) { NormalisableRange<float> r (mn, mx); if (sk > 0.0f) r.setSkewForCentre (sk); \
      layout.add (std::make_unique<AudioParameterFloat> (ParameterID { #id, version }, nm, r, df)); }
   #define PG_LAYOUT_I(id, nm, mn, mx, df) \
      if (keep (#id)) layout.add (std::make_unique<AudioParameterInt> (ParameterID { #id, version }, nm, mn, mx, df));
   #define PG_LAYOUT_C(id, nm, list, df) \
      if (keep (#id)) layout.add (std::make_unique<AudioParameterChoice> (ParameterID { #id, version }, nm, choices::list(), df));
   #define PG_LAYOUT_B(id, nm, df) \
      if (keep (#id)) layout.add (std::make_unique<AudioParameterBool> (ParameterID { #id, version }, nm, df));
    PG_PARAM_LIST (PG_LAYOUT_F, PG_LAYOUT_I, PG_LAYOUT_C, PG_LAYOUT_B)
   #undef PG_LAYOUT_F
   #undef PG_LAYOUT_I
   #undef PG_LAYOUT_C
   #undef PG_LAYOUT_B
}

} // namespace pg
