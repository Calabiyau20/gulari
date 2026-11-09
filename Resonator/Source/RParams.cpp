#include "RParams.h"
#include "Params.h"   // Synthgularity FX rack / MIXBUS parameters

namespace cyr
{
namespace choices
{
    juce::StringArray filterTypes() { return { "Low Pass", "Band Pass", "High Pass", "Notch" }; }
    juce::StringArray lfoShapes()   { return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth Random" }; }
    juce::StringArray divisions()   { return pg::choices::divisions(); }
    double divisionInBeats (int i)  { return pg::choices::divisionInBeats (i); }
    juce::StringArray quality()     { return { "Off", "2x", "4x" }; }
    juce::StringArray oscModes()    { return { "Orbit", "Scan" }; }
    juce::StringArray modSources()
    {
        return { "None", "Env 1", "Env 2", "Env 3", "Env 4", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6",
                 "Amp Env", "Velocity", "Key", "Mod Wheel", "Aftertouch", "Pitch Bend", "Random" };
    }
    juce::StringArray modDests()
    {
        juce::StringArray a { "None" };
        for (int p : modDestParams()) a.add (infos()[(size_t) p].name);
        return a;
    }
}

// modDests() reads the names through infos(), so its size is computed here instead
static int choiceSize (juce::StringArray (*f)())
{
    if (f == &choices::modDests) return 1 + (osca_pos - osca_n + 1) * kOscs + (pd_dd - fm_aa + 1) + 4 + kLfos;
    return f().size();
}

static std::vector<RInfo> buildInfos()
{
    std::vector<RInfo> v;
   #define I_F(id, nm, mn, mx, df, sk) v.push_back ({ #id, nm, mn, mx, df, sk, false, false, false });
   #define I_I(id, nm, mn, mx, df)     v.push_back ({ #id, nm, (float) mn, (float) mx, (float) df, 0.0f, true, false, false });
   #define I_C(id, nm, list, df)       v.push_back ({ #id, nm, 0.0f, (float) (choiceSize (&choices::list) - 1), (float) df, 0.0f, true, true, false });
   #define I_B(id, nm, df)             v.push_back ({ #id, nm, 0.0f, 1.0f, df ? 1.0f : 0.0f, 0.0f, true, false, true });
    CYR_PARAM_LIST (I_F, I_I, I_C, I_B)
   #undef I_F
   #undef I_I
   #undef I_C
   #undef I_B
    jassert ((int) v.size() == kNumR);
    return v;
}

const std::vector<RInfo>& infos()
{
    static const std::vector<RInfo> v = buildInfos();
    return v;
}

const std::vector<juce::NormalisableRange<float>>& ranges()
{
    static const std::vector<juce::NormalisableRange<float>> v = []
    {
        std::vector<juce::NormalisableRange<float>> r;
        for (auto& i : infos())
        {
            juce::NormalisableRange<float> nr (i.min, i.max);
            if (i.skew > 0.0f) nr.setSkewForCentre (i.skew);
            r.push_back (nr);
        }
        return r;
    }();
    return v;
}

int indexOfR (const juce::String& id)
{
    static const std::unordered_map<juce::String, int> map = []
    {
        std::unordered_map<juce::String, int> m;
        for (int i = 0; i < kNumR; ++i) m[infos()[(size_t) i].id] = i;
        return m;
    }();
    auto it = map.find (id);
    return it == map.end() ? -1 : it->second;
}

const std::vector<int>& modDestParams()
{
    static const std::vector<int> v = []
    {
        std::vector<int> d;
        // modDests() reads names through infos(), which never calls back here
        for (int o = 0; o < kOscs; ++o)
            for (int p = osca_n; p <= osca_pos; ++p) d.push_back (p + o * kOscStride);
        for (int p = fm_aa; p <= pd_dd; ++p) d.push_back (p);
        for (int p : { (int) flt_cut, (int) flt_res, (int) flt_drive, (int) master }) d.push_back (p);
        for (int l = 0; l < kLfos; ++l) d.push_back (lfo1_rate + l * kLfoStride);
        jassert ((int) d.size() + 1 == choiceSize (&choices::modDests));
        return d;
    }();
    return v;
}

int destIndexOfParam (int rid)
{
    static const std::vector<int> lookup = []
    {
        std::vector<int> t ((size_t) kNumR, -1);
        const auto& d = modDestParams();
        for (size_t i = 0; i < d.size(); ++i) t[(size_t) d[i]] = (int) i + 1;
        return t;
    }();
    return rid >= 0 && rid < kNumR ? lookup[(size_t) rid] : -1;
}

bool isHostedFxParam (const juce::String& id)
{
    return id.startsWith ("fx_") || id.startsWith ("ff1_") || id.startsWith ("ff2_") || id.startsWith ("mb_") || id == "rt_order";
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    const int version = 1;
   #define L_F(id, nm, mn, mx, df, sk) \
    { NormalisableRange<float> r (mn, mx); if (sk > 0.0f) r.setSkewForCentre (sk); \
      layout.add (std::make_unique<AudioParameterFloat> (ParameterID { #id, version }, nm, r, df)); }
   #define L_I(id, nm, mn, mx, df) layout.add (std::make_unique<AudioParameterInt> (ParameterID { #id, version }, nm, mn, mx, df));
   #define L_C(id, nm, list, df)   layout.add (std::make_unique<AudioParameterChoice> (ParameterID { #id, version }, nm, choices::list(), df));
   #define L_B(id, nm, df)         layout.add (std::make_unique<AudioParameterBool> (ParameterID { #id, version }, nm, df));
    CYR_PARAM_LIST (L_F, L_I, L_C, L_B)
   #undef L_F
   #undef L_I
   #undef L_C
   #undef L_B
    pg::addParamsTo (layout, [] (const String& id) { return isHostedFxParam (id); });
    return layout;
}
} // namespace cyr
