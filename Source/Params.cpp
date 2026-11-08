#include "Params.h"

namespace pg
{
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
    juce::StringArray modWaves()      { return { "Sine", "Triangle", "Saw", "Square" }; }
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
        return { "None", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Chaos 1", "Chaos 2",
                 "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Seq 1", "Seq 2", "Seq 3", "Seq 4",
                 "Mod Env", "Filter Env", "Amp Env", "Velocity", "Note", "Mod Wheel",
                 "Aftertouch", "Pitch Bend", "Random" };
    }
    juce::StringArray modDests()
    {
        return { "None",
                 "WT A Position", "WT A Pitch", "WT A Level", "WT B Position", "WT B Pitch", "WT B Level",
                 "Grain Position", "Grain Size", "Grain Density", "Grain Pitch", "Grain Spray", "Grain Level",
                 "Mod Osc Pitch", "Mod Osc Level", "FM Amount", "Sub Level", "Noise Level",
                 "Plaits Harmonics", "Plaits Timbre", "Plaits Morph", "Plaits Level",
                 "Distortion Drive", "Distortion Fold", "Distortion Crush",
                 "Cutoff", "Resonance", "Pan", "Volume",
                 "FX Delay Mix", "FX Delay Feedback", "FX Resonator Structure", "FX Resonator Brightness",
                 "FX Resonator Position", "FX Clouds Position", "FX Clouds Size", "FX Clouds Density",
                 "FX Clouds Texture", "FX Clouds Mix", "FX Flanger Mix", "FX Phaser Mix" };
    }
    juce::StringArray seqModes()      { return { "Unipolar", "Bipolar" }; }
    juce::StringArray arpModes()      { return { "Up", "Down", "Up/Down", "Down/Up", "As Played", "Random", "Chord" }; }
    juce::StringArray ringsModels()   { return { "Modal", "Sympathetic", "String", "FM Voice", "Sympathetic Quantized", "String + Reverb" }; }
    juce::StringArray ringsPoly()     { return { "1", "2", "4" }; }
    juce::StringArray cloudsModes()   { return { "Granular", "Looping Delay", "Spectral" }; }
    juce::StringArray cloudsQuality() { return { "16-bit Stereo", "16-bit Mono", "8-bit Stereo", "8-bit Mono" }; }
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
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const int version = 1;

   #define PG_LAYOUT_F(id, nm, mn, mx, df, sk) \
    { NormalisableRange<float> r (mn, mx); if (sk > 0.0f) r.setSkewForCentre (sk); \
      layout.add (std::make_unique<AudioParameterFloat> (ParameterID { #id, version }, nm, r, df)); }
   #define PG_LAYOUT_I(id, nm, mn, mx, df) \
      layout.add (std::make_unique<AudioParameterInt> (ParameterID { #id, version }, nm, mn, mx, df));
   #define PG_LAYOUT_C(id, nm, list, df) \
      layout.add (std::make_unique<AudioParameterChoice> (ParameterID { #id, version }, nm, choices::list(), df));
   #define PG_LAYOUT_B(id, nm, df) \
      layout.add (std::make_unique<AudioParameterBool> (ParameterID { #id, version }, nm, df));
    PG_PARAM_LIST (PG_LAYOUT_F, PG_LAYOUT_I, PG_LAYOUT_C, PG_LAYOUT_B)
   #undef PG_LAYOUT_F
   #undef PG_LAYOUT_I
   #undef PG_LAYOUT_C
   #undef PG_LAYOUT_B
    return layout;
}

} // namespace pg
