#include "M230.h"

namespace wc
{
M230::M230 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    for (int c = 0; c < 3; ++c)
    {
        const juce::String n (c + 1);
        sensP[c] = apvts.getRawParameterValue (p + "sens" + n);
        decP[c] = apvts.getRawParameterValue (p + "decay" + n);
        thrP[c] = apvts.getRawParameterValue (p + "thr" + n);
        modeP[c] = apvts.getRawParameterValue (p + "mode" + n);
        jassert (sensP[c] && decP[c] && thrP[c] && modeP[c]);
    }
}

void M230::prepare (double sampleRate) { fs = (float) sampleRate; att = 1.0f - std::exp (-1.0f / (0.001f * fs)); reset(); }

void M230::reset() { for (int c = 0; c < 3; ++c) { env[c] = 0.0f; above[c] = false; pulseLeft[c] = 0; } }

void M230::beginBlock (int)
{
    for (int c = 0; c < 3; ++c)
    {
        gain[c] = gainOf (sensP[c]->load());
        rel[c] = 1.0f - std::exp (-1.0f / (decaySec (decP[c]->load()) * fs));
        thr[c] = 10.0f * thrP[c]->load();
        sustained[c] = modeP[c]->load() > 0.5f;
    }
}

void M230::tick (const float* in, float* out) noexcept
{
    for (int c = 0; c < 3; ++c)
    {
        // full-wave rectifier -> peak follower: 1 ms attack, decay set by the knob; full scale audio (1.0) = 10 V at 0 dB
        const float x = juce::jmin (12.0f, 10.0f * std::fabs (in[In_Audio1 + c]) * gain[c]);
        env[c] += (x - env[c]) * (x > env[c] ? att : rel[c]);
        const float e = juce::jmin (10.0f, env[c]);
        // comparator with 10 % hysteresis
        const float t = juce::jmax (0.2f, thr[c]);
        if (! above[c] && e > t) { above[c] = true; pulseLeft[c] = (int) (0.005f * fs); }
        else if (above[c] && e < 0.9f * t) above[c] = false;
        const bool p = sustained[c] ? above[c] : pulseLeft[c] > 0;
        if (pulseLeft[c] > 0) --pulseLeft[c];
        out[Out_Env1 + c] = out[Out_EnvB1 + c] = e;
        out[Out_Pulse1 + c] = p ? 10.0f : 0.0f;
        leds[(size_t) (Led_Env1 + c)].store (e / 10.0f, std::memory_order_relaxed);
        leds[(size_t) (Led_Pulse1 + c)].store (p ? 1.0f : 0.0f, std::memory_order_relaxed);
    }
}
} // namespace wc
