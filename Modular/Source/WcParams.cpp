#include "WcParams.h"

namespace wc
{
using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using NR = juce::NormalisableRange<float>;

static juce::AudioParameterFloatAttributes hzAttr (bool canBeLow)
{
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([canBeLow] (float v, int)
    {
        juce::ignoreUnused (canBeLow);
        const float hz = coarseHz (v, false);
        return hz < 1000.0f ? juce::String (hz, hz < 100.0f ? 1 : 0) + " Hz" : juce::String (hz / 1000.0f, 2) + " kHz";
    });
}
static juce::AudioParameterFloatAttributes unitAttr (const char* unit, int dec = 2)
{
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([unit, dec] (float v, int) { return juce::String (v, dec) + unit; });
}

static void add259 (juce::AudioProcessorValueTreeState::ParameterLayout& l, int i)
{
    const juce::String p (prefix259 (i)), n ("259 " + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + " ");
    auto id = [&] (const char* s) { return juce::ParameterID (p + s, 1); };
    auto group = std::make_unique<juce::AudioProcessorParameterGroup> (p + "grp", n.trim(), "|");
    // modulation oscillator
    group->addChild (std::make_unique<APF> (id ("mo_freq"), n + "MO Frequency", NR (0.0f, 1.0f), coarseNorm (130.81f, false), hzAttr (true)));
    group->addChild (std::make_unique<APF> (id ("mo_fine"), n + "MO Fine", NR (-3.5f, 3.5f), 0.0f, unitAttr (" st")));
    group->addChild (std::make_unique<APC> (id ("mo_range"), n + "MO Range", juce::StringArray { "High", "Low" }, 0));
    group->addChild (std::make_unique<APC> (id ("mo_wave"), n + "MO Wave", juce::StringArray { "Sawtooth", "Square", "Triangle" }, 2));
    group->addChild (std::make_unique<APF> (id ("mo_cv"), n + "MO CV Amount", NR (-1.0f, 1.0f), 0.0f));
    group->addChild (std::make_unique<APF> (id ("mo_fm"), n + "MO FM Amount", NR (0.0f, 1.0f), 0.0f));
    // modulation section
    group->addChild (std::make_unique<APF> (id ("idx"), n + "Mod Index", NR (0.0f, 1.0f), 0.0f));
    group->addChild (std::make_unique<APF> (id ("idx_cv"), n + "Index CV Amount", NR (0.0f, 1.0f), 0.5f));
    group->addChild (std::make_unique<APB> (id ("mod_amp"), n + "Mod Amplitude", false));
    group->addChild (std::make_unique<APB> (id ("mod_pitch"), n + "Mod Pitch", false));
    group->addChild (std::make_unique<APB> (id ("mod_timbre"), n + "Mod Timbre", false));
    group->addChild (std::make_unique<APC> (id ("prog"), n + "Program", juce::StringArray { "Local", "Remote", "Both" }, 0));
    group->addChild (std::make_unique<APF> (id ("lock"), n + "Phase Lock Gain", NR (0.0f, 1.0f), 0.7f));
    group->addChild (std::make_unique<APB> (id ("lock_on"), n + "Phase Lock", false));
    group->addChild (std::make_unique<APB> (id ("volt1"), n + "1V/Oct", false));
    // principal oscillator
    group->addChild (std::make_unique<APF> (id ("po_freq"), n + "Principal Frequency", NR (0.0f, 1.0f), coarseNorm (65.406f, false), hzAttr (false)));
    group->addChild (std::make_unique<APF> (id ("po_fine"), n + "Principal Fine", NR (-3.5f, 3.5f), 0.0f, unitAttr (" st")));
    group->addChild (std::make_unique<APF> (id ("po_cv"), n + "Principal CV Amount", NR (-1.0f, 1.0f), 0.0f));
    group->addChild (std::make_unique<APF> (id ("po_fm"), n + "Principal FM Amount", NR (0.0f, 1.0f), 0.0f));
    // waveshape
    group->addChild (std::make_unique<APF> (id ("timbre"), n + "Timbre", NR (0.0f, 1.0f), 0.0f));
    group->addChild (std::make_unique<APF> (id ("timbre_cv"), n + "Timbre CV Amount", NR (-1.0f, 1.0f), 0.5f));
    group->addChild (std::make_unique<APF> (id ("sym"), n + "Symmetry", NR (0.0f, 1.0f), 0.5f));
    group->addChild (std::make_unique<APF> (id ("order"), n + "Order", NR (0.0f, 1.0f), 0.0f));
    l.add (std::move (group));
}

static void add281 (juce::AudioProcessorValueTreeState::ParameterLayout& l, int i)
{
    const juce::String p (prefix281 (i)), n ("281 " + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + " ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> (p + "grp", n.trim(), "|");
    auto timeAttr = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        const float t = 0.001f * std::pow (10.0f, 4.0f * v);
        return t < 1.0f ? juce::String (t * 1000.0f, t < 0.01f ? 1 : 0) + " ms" : juce::String (t, 2) + " s";
    });
    const char* ch = "ABCD";
    for (int k = 1; k <= 4; ++k)
    {
        const juce::String s (k), c = juce::String::charToString ((juce::juce_wchar) ch[k - 1]);
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "att" + s, 1), n + c + " Attack", NR (0.0f, 1.0f), 0.15f, timeAttr));
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "dec" + s, 1), n + c + " Decay", NR (0.0f, 1.0f), 0.55f, timeAttr));
        g->addChild (std::make_unique<APC> (juce::ParameterID (p + "mode" + s, 1), n + c + " Mode", juce::StringArray { "Transient", "Sustained", "Cycle" }, 0));
    }
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "quad12", 1), n + "Quadrature A-B", false));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "quad34", 1), n + "Quadrature C-D", false));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "mix12", 1), n + "Quadrature Mix A-B", NR (0.0f, 1.0f), 0.5f));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "mix34", 1), n + "Quadrature Mix C-D", NR (0.0f, 1.0f), 0.5f));
    l.add (std::move (g));
}

static void add292 (juce::AudioProcessorValueTreeState::ParameterLayout& l, int i)
{
    const juce::String p (prefix292 (i)), n ("292 " + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + " ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> (p + "grp", n.trim(), "|");
    for (int k = 1; k <= 4; ++k)
    {
        const juce::String s (k);
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "off" + s, 1), n + s + " Offset", NR (0.0f, 1.0f), 0.0f));
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "cv" + s, 1), n + s + " CV Level", NR (0.0f, 1.0f), 1.0f));
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "res" + s, 1), n + s + " Resonance", NR (0.0f, 1.0f), 0.6f));
        g->addChild (std::make_unique<APC> (juce::ParameterID (p + "mode" + s, 1), n + s + " Mode", juce::StringArray { "LoPass", "Combo", "Gate" }, 1));
    }
    l.add (std::move (g));
}

static void add266 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("ua_"), n ("266 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("ua_grp", "266", "|");
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "rate", 1), n + "Fluctuating Rate", NR (0.0f, 1.0f), 0.45f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (0.05f * std::pow (1000.0f, v), 2) + " Hz"; })));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "rate2", 1), n + "Fluctuating Rate 2", NR (0.0f, 1.0f), 0.3f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (0.05f * std::pow (1000.0f, v), 2) + " Hz"; })));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "smooth", 1), n + "Integrator Smoothing", NR (0.0f, 1.0f), 0.3f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (0.01f * std::pow (1000.0f, v), 2) + " s"; })));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "steps", 1), n + "Quantized Steps", NR (1.0f, 6.0f, 1.0f), 3.0f));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "dist", 1), n + "Stored Probability", NR (0.0f, 1.0f), 0.5f));
    l.add (std::move (g));
}

static void add245 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("sv_"), n ("245 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("sv_grp", "245", "|");
    const char* rows = "abcd";
    for (int r = 0; r < 4; ++r)
        for (int st = 1; st <= 5; ++st)
            g->addChild (std::make_unique<APF> (juce::ParameterID (p + juce::String::charToString ((juce::juce_wchar) rows[r]) + juce::String (st), 1),
                n + juce::String::charToString ((juce::juce_wchar) ('A' + r)) + juce::String (st), NR (0.0f, 1.0f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v * 10.0f, 2) + " V"; })));
    for (int k = 1; k <= 4; ++k)
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "sw" + juce::String (k), 1), n + "Stage Switch " + juce::String (k), true));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "time", 1), n + "Pulser Time", NR (0.0f, 1.0f), 0.55f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { const float t = 0.002f * std::pow (5000.0f, v); return t < 1.0f ? juce::String (t * 1000.0f, 0) + " ms" : juce::String (t, 2) + " s"; })));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "tcv", 1), n + "Pulser Time CV", NR (-1.0f, 1.0f), 0.0f));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "len", 1), n + "Pulse Length", NR (0.0f, 1.0f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v * 100.0f, 0) + " %"; })));
    l.add (std::move (g));
}

static void add242 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("pp_"), n ("242 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("pp_grp", "242", "|");
    const char* rows = "abc";
    for (int r = 0; r < 3; ++r)
        for (int st = 1; st <= 12; ++st)
            g->addChild (std::make_unique<APB> (juce::ParameterID (p + juce::String::charToString ((juce::juce_wchar) rows[r]) + juce::String (st), 1),
                n + "Matrix " + juce::String::charToString ((juce::juce_wchar) ('A' + r)) + juce::String (st), false));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "len", 1), n + "Stages", NR (1.0f, 12.0f, 1.0f), 8.0f));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "period", 1), n + "Period", NR (0.0f, 1.0f), 0.5f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            if (v > 0.92f) return juce::String ("off");
            const float t = 0.002f * std::pow (5000.0f, v / 0.92f);
            return t < 1.0f ? juce::String (t * 1000.0f, 0) + " ms" : juce::String (t, 2) + " s";
        })));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "cv", 1), n + "Period CV", NR (-1.0f, 1.0f), 0.0f));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "row", 1), n + "Edit Row", NR (1.0f, 4.0f, 1.0f), 2.0f));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "col", 1), n + "Edit Column", NR (1.0f, 12.0f, 1.0f), 1.0f));
    l.add (std::move (g));
}

static juce::AudioParameterFloatAttributes voltAttr (float scale, int dec = 2)
{
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([scale, dec] (float v, int) { return juce::String (v * scale, dec) + " V"; });
}

static void add257 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("vp_"), n ("257 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("vp_grp", "257", "|");
    for (int s = 1; s <= 2; ++s)
    {
        const juce::String k (s);
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "k" + k, 1), n + k + " K (attenuverter)", NR (-1.0f, 1.0f), 1.0f));
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "m" + k, 1), n + k + " M (crossfade)", NR (0.0f, 1.0f), 0.0f));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "p5" + k, 1), n + k + " +.5 (Vb + 5 V)", false));
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "off" + k, 1), n + k + " Offset", NR (0.0f, 1.0f), 0.0f, voltAttr (10.0f)));
    }
    l.add (std::move (g));
}

static void add258 (juce::AudioProcessorValueTreeState::ParameterLayout& l, int i)
{
    const juce::String p (prefix258 (i)), n ("258 " + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + " ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> (p + "grp", n.trim(), "|");
    auto hz = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
    {
        const float f = 5.0f * std::pow (4000.0f, v);
        return f < 1000.0f ? juce::String (f, f < 100.0f ? 1 : 0) + " Hz" : juce::String (f / 1000.0f, 2) + " kHz";
    });
    const float defHz[2] = { 130.81f, 196.0f };
    for (int o = 1; o <= 2; ++o)
    {
        const juce::String k (o);
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "freq" + k, 1), n + k + " Frequency", NR (0.0f, 1.0f), std::log (defHz[o - 1] / 5.0f) / std::log (4000.0f), hz));
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "fine" + k, 1), n + k + " Fine", NR (-3.5f, 3.5f), 0.0f, unitAttr (" st")));
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "proc" + k, 1), n + k + " Processing", NR (-1.0f, 1.0f), 0.0f));
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "fm" + k, 1), n + k + " FM Depth", NR (0.0f, 1.0f), 0.0f));
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "shape" + k, 1), n + k + (o == 1 ? " Waveshape (sine-saw)" : " Waveshape (sine-square)"), NR (0.0f, 1.0f), 0.0f));
    }
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "volt1", 1), n + "1V/Oct", false));
    l.add (std::move (g));
}

static void add264 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("sh_"), n ("264 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("sh_grp", "264", "|");
    for (int c = 1; c <= 4; ++c)
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "track" + juce::String (c), 1), n + juce::String (c) + " Track (off = Sample)", false));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "common", 1), n + "Common C.V.", false));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "voices", 1), n + "Number of Voices", NR (1.0f, 4.0f, 1.0f), 4.0f));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "offset", 1), n + "Offset", NR (-2.0f, 2.0f), 0.0f, unitAttr (" V")));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "mod", 1), n + "Modulation", NR (0.0f, 1.0f), 0.0f));
    g->addChild (std::make_unique<APC> (juce::ParameterID (p + "scale", 1), n + "ART Scale", juce::StringArray { "m (minor)", "ch (chromatic)", "M (major)" }, 1));
    g->addChild (std::make_unique<APC> (juce::ParameterID (p + "key", 1), n + "ART Key", juce::StringArray { "c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b" }, 0));
    l.add (std::move (g));
}

static void add230 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("ef_"), n ("230 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("ef_grp", "230", "|");
    for (int c = 1; c <= 3; ++c)
    {
        const juce::String k (c);
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "sens" + k, 1), n + k + " Sensitivity", NR (0.0f, 1.0f), 0.4f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (-20.0f + 50.0f * v, 1) + " dB"; })));
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "decay" + k, 1), n + k + " Decay", NR (0.0f, 1.0f), 0.35f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { const float t = 0.01f * std::pow (300.0f, v); return t < 1.0f ? juce::String (t * 1000.0f, 0) + " ms" : juce::String (t, 2) + " s"; })));
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "thr" + k, 1), n + k + " Pulse Threshold", NR (0.0f, 1.0f), 0.3f, voltAttr (10.0f, 1)));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "mode" + k, 1), n + k + " Sustained (off = Transient)", false));
    }
    l.add (std::move (g));
}

static void add285 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("fs_"), n ("285 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("fs_grp", "285", "|");
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "ctl", 1), n + "Shift Control CV", NR (-1.0f, 1.0f), 0.0f));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "int", 1), n + "Internal Reference", true));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "ref", 1), n + "Reference (shift)", NR (0.0f, 1.0f), 0.25f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (1000.0f * v * v, 1) + " Hz"; })));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "fm", 1), n + "F.M.", NR (0.0f, 1.0f), 0.0f));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "bmctl", 1), n + "Modulation Control CV", NR (0.0f, 1.0f), 0.5f));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "amp", 1), n + "Amplitude - Ring", NR (0.0f, 1.0f), 1.0f));
    l.add (std::move (g));
}

static void add277 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("dl_"), n ("277 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("dl_grp", "277", "|");
    for (int i = 0; i < 3; ++i)
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "lvl" + juce::String::charToString ((juce::juce_wchar) ('a' + i)), 1),
                     n + "Input " + juce::String::charToString ((juce::juce_wchar) ('A' + i)), NR (0.0f, 1.0f), i == 0 ? 0.8f : 0.0f));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "time", 1), n + "Delay Time", NR (0.0f, 1.0f), 0.33f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (0.05f + 0.15f * v, 3) + " s (x4 long)"; })));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "long", 1), n + "Long", false));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "ctl", 1), n + "Control Input", NR (-1.0f, 1.0f), 0.0f));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "mod", 1), n + "Modulation", NR (0.0f, 1.0f), 0.0f));
    l.add (std::move (g));
}

static void add207 (juce::AudioProcessorValueTreeState::ParameterLayout& l)
{
    const juce::String p ("mx_"), n ("207 ");
    auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("mx_grp", "207", "|");
    for (int c = 0; c < 6; ++c)
    {
        const juce::String ch = juce::String::charToString ((juce::juce_wchar) ('a' + c)), CH = ch.toUpperCase();
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "lvl_" + ch, 1), n + CH + " Level", NR (0.0f, 1.0f), 0.8f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v * 10.0f, 1); })));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "prog_" + ch, 1), n + CH + " Prog", false));
        g->addChild (std::make_unique<APB> (juce::ParameterID (p + "mon_" + ch, 1), n + CH + " Mon", false));
    }
    for (int k = 1; k <= 4; ++k)
        g->addChild (std::make_unique<APF> (juce::ParameterID (p + "pan" + juce::String (k), 1), n + juce::String::charToString ((juce::juce_wchar) ('A' + k)) + " Assignment (L-R)", NR (0.0f, 1.0f), 0.5f));
    g->addChild (std::make_unique<APF> (juce::ParameterID (p + "gain", 1), n + "Preamp Gain", NR (0.0f, 1.0f), 0.3f));
    g->addChild (std::make_unique<APB> (juce::ParameterID (p + "range", 1), n + "Preamp Range 75 dB", false));
    l.add (std::move (g));
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("midi_grp", "MIDI Interface", "|");
        g->addChild (std::make_unique<APF> (juce::ParameterID ("midi_glide", 1), "MIDI Glide", NR (0.0f, 1.0f), 0.0f));
        g->addChild (std::make_unique<APF> (juce::ParameterID ("midi_bend", 1), "MIDI Bend Range", NR (0.0f, 12.0f, 1.0f), 2.0f, unitAttr (" st", 0)));
        g->addChild (std::make_unique<APF> (juce::ParameterID ("midi_trans", 1), "MIDI Transpose", NR (-3.0f, 3.0f, 1.0f), 0.0f, unitAttr (" oct", 0)));
        l.add (std::move (g));
    }
    add242 (l);
    add245 (l);
    add266 (l);
    add230 (l);
    add264 (l);
    add257 (l);
    for (int i = 0; i < kMax281; ++i) add281 (l, i);
    for (int i = 0; i < kMax258; ++i) add258 (l, i);
    for (int i = 0; i < kMax259; ++i) add259 (l, i);
    add285 (l);
    add277 (l);
    for (int i = 0; i < kMax292; ++i) add292 (l, i);
    add207 (l);
    {
        auto g = std::make_unique<juce::AudioProcessorParameterGroup> ("out_grp", "Output", "|");
        g->addChild (std::make_unique<APF> (juce::ParameterID ("out_level", 1), "Output Level", NR (0.0f, 1.0f), 0.8f));
        g->addChild (std::make_unique<APF> (juce::ParameterID ("out_cv", 1), "Output Level CV Amount", NR (0.0f, 1.0f), 1.0f));
        g->addChild (std::make_unique<APF> (juce::ParameterID ("out_lag", 1), "Output Lag", NR (1.0f, 300.0f, 0.0f, 0.4f), 12.0f, unitAttr (" ms", 0)));
        g->addChild (std::make_unique<APF> (juce::ParameterID ("out_master", 1), "Master", NR (-48.0f, 6.0f), -3.0f, unitAttr (" dB", 1)));
        l.add (std::move (g));
    }
    return l;
}
} // namespace wc
