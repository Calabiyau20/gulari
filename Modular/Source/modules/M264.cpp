#include "M264.h"

namespace wc
{
M264::M264 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    for (int c = 0; c < 4; ++c) modeP[c] = apvts.getRawParameterValue (p + "track" + juce::String (c + 1));
    commonP = apvts.getRawParameterValue (p + "common");
    voicesP = apvts.getRawParameterValue (p + "voices");
    offP = apvts.getRawParameterValue (p + "offset");
    modP = apvts.getRawParameterValue (p + "mod");
}

void M264::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }

void M264::reset()
{
    for (int c = 0; c < 4; ++c) { held[c] = 0.0f; was[c] = false; polyPulse[c] = 0; flash[c] = 0.0f; }
    polyWas = false; polyNext = 0;
}

void M264::beginBlock (int)
{
    for (int c = 0; c < 4; ++c) track[c] = modeP[c]->load() > 0.5f;
    common = commonP->load() > 0.5f;
    voices = juce::jlimit (1, 4, (int) std::lround (voicesP->load()));
    off = offP->load();
    modK = modP->load();
}

void M264::tick (const float* in, float* out) noexcept
{
    const float mod = 2.0f * modK * in[In_Mod];                         // up to +-2 V of modulation
    // polyphonic adaptor: round robin over the first `voices` channels, common c.v. (input 4)
    const bool polyHi = in[In_Poly] > 3.0f;
    if (polyHi && ! polyWas)
    {
        const int v = polyNext % voices;
        held[v] = in[In_Cv4] + mod;
        polyPulse[v] = juce::jmax (1, (int) (0.001f * fs));
        flash[v] = 1.0f;
        polyNext = (v + 1) % voices;
    }
    polyWas = polyHi;
    for (int c = 0; c < 4; ++c)
    {
        const bool hi = in[In_Pulse1 + c] > 3.0f;
        const float src = (common ? in[In_Cv4] : in[In_Cv1 + c]) + mod;
        if (track[c] ? hi : (hi && ! was[c])) { held[c] = src; if (! was[c]) flash[c] = 1.0f; }
        was[c] = hi;
        const bool pOut = hi || polyPulse[c] > 0;
        if (polyPulse[c] > 0) --polyPulse[c];
        out[Out_Pulse1 + c] = pOut ? 10.0f : 0.0f;
        out[Out_Cv1 + c] = held[c] + off;
        flash[c] *= 1.0f - 1.0f / (0.08f * fs);
        leds[(size_t) c].store (juce::jmax (flash[c], pOut ? 1.0f : 0.0f), std::memory_order_relaxed);
    }
}
} // namespace wc
