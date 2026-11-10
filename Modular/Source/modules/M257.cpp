#include "M257.h"

namespace wc
{
M257::M257 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    for (int s = 0; s < 2; ++s)
    {
        const juce::String n (s + 1);
        kP[s] = apvts.getRawParameterValue (p + "k" + n);
        mP[s] = apvts.getRawParameterValue (p + "m" + n);
        p5P[s] = apvts.getRawParameterValue (p + "p5" + n);
        offP[s] = apvts.getRawParameterValue (p + "off" + n);
        jassert (kP[s] && mP[s] && p5P[s] && offP[s]);
    }
}

void M257::beginBlock (int)
{
    for (int s = 0; s < 2; ++s) { k[s] = kP[s]->load(); m[s] = mP[s]->load(); p5[s] = p5P[s]->load() > 0.5f; off[s] = 10.0f * offP[s]->load(); }
}

void M257::tick (const float* in, float* out) noexcept
{
    for (int s = 0; s < 2; ++s)
    {
        const float v = process (in[inOf (s, 0)], in[inOf (s, 1)], in[inOf (s, 2)], in[inOf (s, 3)], k[s], m[s], p5[s], off[s]);
        out[s] = v;
        leds[(size_t) s].store (dsp::clampf (std::fabs (v) / 10.0f, 0.0f, 1.0f), std::memory_order_relaxed);
    }
}
} // namespace wc
