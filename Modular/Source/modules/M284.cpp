#include "M284.h"

namespace wc
{
static const char* k284[5] = { "del", "att", "dur", "dec", "ctl" };

M284::M284 (juce::AudioProcessorValueTreeState& apvts, const juce::String& pre)
{
    for (int k = 0; k < kCh; ++k)
    {
        const juce::String n (k + 1);
        for (int f = 0; f < 5; ++f) { p[f][k] = apvts.getRawParameterValue (pre + k284[f] + n); jassert (p[f][k] != nullptr); }
        susRaw[k] = apvts.getRawParameterValue (pre + "sus" + n);
    }
}

void M284::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }
void M284::reset() { for (auto& c : ch) c = Ch(); }

void M284::beginBlock (int)
{
    for (int k = 0; k < kCh; ++k)
    {
        for (int f = 0; f < 5; ++f) knob[f][k] = p[f][k]->load();
        sus[k] = susRaw[k]->load() > 0.5f;
    }
}

void M284::tick (const float* in, float* out) noexcept
{
    for (int k = 0; k < kCh; ++k)
    {
        auto& c = ch[k];
        const bool gate = in[inTrig (k)] > 3.0f || manual[k].load (std::memory_order_relaxed);
        if (gate && ! c.was) { c.st = Delay; c.t = 0.0f; c.from = c.level; }
        c.was = gate;
        // control: +10 V with the knob fully right = times / 16; fully left = times x 16
        const float mul = std::exp2 (-knob[4][k] * dsp::clampf (in[inCtl (k)], -10.0f, 10.0f) * 0.4f);
        auto len = [&] (int f) { return juce::jlimit (0.0005f, 60.0f, knobTime (knob[f][k]) * mul); };
        const float dt = 1.0f / fs;
        switch (c.st)
        {
            case Idle: break;
            case Delay:
                c.t += dt;
                if (c.t >= len (0)) { c.st = Attack; c.t = 0.0f; c.from = c.level; }
                break;
            case Attack:
            {
                c.t += dt;
                const float x = juce::jmin (1.0f, c.t / len (1));
                c.level = c.from + (10.0f - c.from) * x;
                if (x >= 1.0f) { c.st = Hold; c.t = 0.0f; }
                break;
            }
            case Hold:
                c.level = 10.0f;
                // sustain: hold for as long as the pulse input stays high, then fall straight away
                if (sus[k]) { if (gate) break; c.st = Decay; c.t = 0.0f; c.from = c.level; break; }
                c.t += dt;
                if (c.t >= len (2)) { c.st = Decay; c.t = 0.0f; c.from = c.level; }
                break;
            case Decay:
            {
                c.t += dt;
                const float x = juce::jmin (1.0f, c.t / len (3));
                const float shape = (1.0f - x) * (1.0f - x) * (1.0f - 0.35f * x);   // slightly exponential fall
                c.level = c.from * shape;
                if (x >= 1.0f) { c.st = Idle; c.level = 0.0f; c.pulse = (int) (0.005f * fs); }
                break;
            }
        }
        out[outEnv (k, 0)] = out[outEnv (k, 1)] = c.level;
        out[outPulse (k)] = c.pulse > 0 ? 10.0f : 0.0f;
        if (c.pulse > 0) --c.pulse;
        ledLevel[k].store (c.level * 0.1f, std::memory_order_relaxed);
    }
}

float M284::led (int id) const noexcept
{
    return id >= 0 && id < kCh ? ledLevel[id].load (std::memory_order_relaxed) : 0.0f;
}
} // namespace wc
