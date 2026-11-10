#include "M258.h"

namespace wc
{
M258::M258 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    for (int o = 0; o < 2; ++o)
    {
        const juce::String n (o + 1);
        freqP[o] = apvts.getRawParameterValue (p + "freq" + n);
        procP[o] = apvts.getRawParameterValue (p + "proc" + n);
        fmP[o] = apvts.getRawParameterValue (p + "fm" + n);
        shapeP[o] = apvts.getRawParameterValue (p + "shape" + n);
        jassert (freqP[o] && procP[o] && fmP[o] && shapeP[o]);
    }
    voltP = apvts.getRawParameterValue (p + "volt1");
}

void M258::prepare (double sampleRate)
{
    fs = (float) sampleRate;
    for (auto& d : dc) d.set (5.0f, fs);
    reset();
}

void M258::beginBlock (int)
{
    for (int o = 0; o < 2; ++o)
    {
        logHz[o] = std::log2 (knobHz (freqP[o]->load()));
        proc[o] = procP[o]->load(); fm[o] = fmP[o]->load(); shape[o] = shapeP[o]->load();
    }
    pitchDiv = voltP != nullptr && voltP->load() > 0.5f ? 1.0f : 1.2f;
}

void M258::tick (const float* in, float* out) noexcept
{
    for (int o = 0; o < 2; ++o)
    {
        const float volts = in[inOf (o, 0)] + proc[o] * in[inOf (o, 1)];
        float hz = std::exp2 (logHz[o] + volts / pitchDiv);
        hz *= 1.0f + 4.0f * fm[o] * in[inOf (o, 2)];                  // through-zero-free linear f.m. (as the 259)
        const float dt = dsp::clampf (hz / fs, 0.0f, 0.45f);
        ph[o] += dt;
        if (ph[o] >= 1.0) ph[o] -= 1.0;
        const float t = (float) ph[o];
        const float s = dsp::clampf (shape[o] + in[inOf (o, 3)] / 10.0f, 0.0f, 1.0f);
        const float sine = std::sin (dsp::kTwoPi * t);
        float other;
        if (o == 0) other = (2.0f * t - 1.0f) - dsp::polyBlep (t, dt);                         // sawtooth
        else
        {
            float sq = t < 0.5f ? 1.0f : -1.0f;
            sq += dsp::polyBlep (t, dt);
            float t2 = t + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
            sq -= dsp::polyBlep (t2, dt);
            other = 0.8f * sq;                                                                  // square, level matched
        }
        // equal-loudness-ish morph: the sine fades as the harmonics come in
        out[o] = dc[o].process ((1.0f - s) * sine + s * other);
    }
}
} // namespace wc
