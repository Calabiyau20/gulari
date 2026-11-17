#pragma once
// SW-108 8 Switches - a Calabi Yau original. Eight channels, each a lever that lets its input through (On) or cuts it.
// 1-3 audio (mini) and 4-6 c.v. (banana): out = in x g, g ramps linearly to 0 or 1 in 5 ms (no click, no jump on a
// parameter the c.v. moves). 7-8 pulse (red banana): cut dry to 0 V; back On with the input already high, nothing
// passes until it falls (<= 3 V) and the next edge goes through whole. Jacks: in (ch) / out (ch), ch 0..7.
#include "../rack/Module.h"

namespace wc
{
class M108 : public Module
{
public:
    static constexpr int kChannels = 8, kFirstPulse = 6;
    static int in (int ch) { return ch; }
    static int out (int ch) { return ch; }

    M108 (const Params& params, const juce::String& prefix);
    void prepare (double sampleRate) override { step = 1.0f / (float) juce::jmax (1, juce::roundToInt (0.005 * sampleRate)); first = true; }
    void reset() override { first = true; }   // in the rack again: the levers as they are, no fade
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;

private:
    std::atomic<float>* onP[kChannels] {};
    bool on[kChannels] {}, armed[kChannels] {};
    float g[kChannels] {};
    float step = 1.0f / 240.0f;
    bool first = true;
};
} // namespace wc
