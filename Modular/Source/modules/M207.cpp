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
    lagA.set (60.0f, fs); lagF.set (60.0f, fs);
}

void M207::beginBlock (int)
{
    for (int c = 0; c < 6; ++c)
    {
        const float f = lvlP[c]->load();                       // fader 0..1 = printed 0..10
        g[c] = f * f;                                           // audio taper, 10 = unity
        if (c >= 1 && c <= 4)
        {
            const float pan = panP[c - 1]->load();
            gl[c] = g[c] * std::cos (0.5f * dsp::kPi * pan);
            gr[c] = g[c] * std::sin (0.5f * dsp::kPi * pan);
        }
        prog[c] = progP[c]->load() > 0.5f;
        mon[c] = monP[c]->load() > 0.5f;
    }
    const float db = gainP->load() * (rangeP->load() > 0.5f ? 70.0f : 55.0f) - 20.0f;
    preGain = std::pow (10.0f, db / 20.0f);
}

void M207::tick (const float* in, float* out) noexcept
{
    // A and F: voltage-controlled assignment (0 V = own side, 10 V = the other side)
    const float pa = lagA.process (dsp::clampf (in[In_AssignA] / 10.0f, 0.0f, 1.0f));
    const float pf = 1.0f - lagF.process (dsp::clampf (in[In_AssignF] / 10.0f, 0.0f, 1.0f));
    const float aL = std::cos (0.5f * dsp::kPi * pa), aR = std::sin (0.5f * dsp::kPi * pa);
    const float fL = std::cos (0.5f * dsp::kPi * pf), fR = std::sin (0.5f * dsp::kPi * pf);
    gl[0] = g[0] * aL; gr[0] = g[0] * aR; gl[5] = g[5] * fL; gr[5] = g[5] * fR;
    float l = in[In_ExpL], r = in[In_ExpR], m = 0.0f;
    for (int c = 0; c < 6; ++c)
    {
        const float x = in[In_A + c];
        if (prog[c]) { l += x * gl[c]; r += x * gr[c]; }
        if (mon[c]) m += x;                                     // monitor is pre-fader
    }
    out[Out_L] = l; out[Out_R] = r;
    out[Out_Mon1] = out[Out_Mon2] = m;
    out[Out_Pre] = std::tanh (in[In_Pre] * preGain);
    leds[Led_AL].store (aL, std::memory_order_relaxed); leds[Led_AR].store (aR, std::memory_order_relaxed);
    leds[Led_FL].store (fL, std::memory_order_relaxed); leds[Led_FR].store (fR, std::memory_order_relaxed);
}
} // namespace wc
