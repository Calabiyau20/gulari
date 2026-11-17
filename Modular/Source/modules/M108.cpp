#include "M108.h"

namespace wc
{
M108::M108 (const Params& params, const juce::String& p)
{
    for (int c = 0; c < kChannels; ++c)
    {
        onP[c] = params.getRawParameterValue (p + "ch" + juce::String (c + 1));
        jassert (onP[c] != nullptr);
    }
}

void M108::beginBlock (int)
{
    for (int c = 0; c < kChannels; ++c)
    {
        on[c] = onP[c]->load() > 0.5f;
        // shortcut: a patch loaded with the module already in the rack fades its levers (5 ms) instead of snapping, as the 257 and the 207 do; snap there if it is ever heard
        if (first) { g[c] = on[c] ? 1.0f : 0.0f; armed[c] = on[c]; }
    }
    first = false;
}

void M108::tick (const float* in, float* out) noexcept
{
    for (int c = 0; c < kFirstPulse; ++c)
    {
        g[c] = on[c] ? juce::jmin (1.0f, g[c] + step) : juce::jmax (0.0f, g[c] - step);
        out[c] = in[c] * g[c];
    }
    for (int c = kFirstPulse; c < kChannels; ++c)
    {
        if (! on[c]) armed[c] = false;
        else if (! armed[c] && in[c] <= 3.0f) armed[c] = true;   // the rack's pulse threshold: the next edge passes whole
        out[c] = armed[c] ? in[c] : 0.0f;
    }
}
} // namespace wc
