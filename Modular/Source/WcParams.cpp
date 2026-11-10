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
    group->addChild (std::make_unique<APF> (id ("lock"), n + "Phase Lock", NR (0.0f, 1.0f), 0.0f));
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
    for (int i = 0; i < kMax259; ++i) add259 (l, i);
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
