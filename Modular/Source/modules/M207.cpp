#include "M207.h"

namespace wc
{
M207::M207 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    for (int c = 0; c < 6; ++c)
    {
        const juce::String ch = juce::String::charToString ((juce::juce_wchar) ('a' + c));
        lvlP[c] = apvts.getRawParameterValue (p + "lvl_" + ch);
        progP[c] = apvts.getRawParameterValue (p + "prog_" + ch);
        monP[c] = apvts.getRawParameterValue (p + "mon_" + ch);
        jassert (lvlP[c] && progP[c] && monP[c]);
    }
    for (int k = 0; k < 4; ++k) panP[k] = apvts.getRawParameterValue (p + "pan" + juce::String (k + 1));
    gainP = apvts.getRawParameterValue (p + "gain");
    rangeP = apvts.getRawParameterValue (p + "range");
}

void M207::prepare (double sampleRate)
{
    fs = (float) sampleRate;
    progLag.set (60.0f, fs); monLag.set (60.0f, fs);
}

void M207::beginBlock (int)
{
    for (int c = 0; c < 6; ++c)
    {
        const float f = lvlP[c]->load();                       // fader 0..1 = printed 0..10
        const float g = f * f;                                  // audio taper, 10 = unity
        float pan = c == 0 ? 0.0f : (c == 5 ? 1.0f : panP[c - 1]->load());
        gl[c] = g * std::cos (0.5f * dsp::kPi * pan);
        gr[c] = g * std::sin (0.5f * dsp::kPi * pan);
        if (c == 0 || c == 5) { gl[c] = c == 0 ? g : 0.0f; gr[c] = c == 5 ? g : 0.0f; }
        prog[c] = progP[c]->load() > 0.5f;
        mon[c] = monP[c]->load() > 0.5f;
    }
    const float db = gainP->load() * (rangeP->load() > 0.5f ? 75.0f : 55.0f) - 20.0f;
    preGain = std::pow (10.0f, db / 20.0f);
}

void M207::tick (const float* in, float* out) noexcept
{
    const float pc = progLag.process (dsp::clampf (in[In_ProgCv] / 10.0f, 0.0f, 1.0f));
    const float mc = isPatched (In_MonCv) ? monLag.process (dsp::clampf (in[In_MonCv] / 10.0f, 0.0f, 1.0f)) : 1.0f;
    float l = in[In_ExpL], r = in[In_ExpR], m = 0.0f;
    for (int c = 0; c < 6; ++c)
    {
        const float x = in[In_A + c] * (prog[c] ? pc : 1.0f);
        l += x * gl[c]; r += x * gr[c];
        if (mon[c]) m += x * (gl[c] + gr[c]) * 0.7071f;
    }
    out[Out_L] = l; out[Out_R] = r;
    out[Out_Mon1] = out[Out_Mon2] = m * mc;
    out[Out_Pre] = std::tanh (in[In_Pre] * preGain);
    const float k = 1.0f / (0.15f * fs);
    peakL = juce::jmax (std::fabs (l), peakL * (1.0f - k)); peakR = juce::jmax (std::fabs (r), peakR * (1.0f - k));
    leds[Led_ProgA].store (pc, std::memory_order_relaxed); leds[Led_ProgB].store (pc > 0.5f ? 1.0f : 0.0f, std::memory_order_relaxed);
    leds[Led_MonA].store (juce::jmin (1.0f, peakL), std::memory_order_relaxed); leds[Led_MonB].store (juce::jmin (1.0f, peakR), std::memory_order_relaxed);
}
} // namespace wc
