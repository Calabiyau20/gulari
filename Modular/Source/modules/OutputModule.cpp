#include "OutputModule.h"

namespace wc
{
OutputModule::OutputModule (juce::AudioProcessorValueTreeState& apvts)
{
    level = apvts.getRawParameterValue ("out_level");
    cvAmt = apvts.getRawParameterValue ("out_cv");
    lag = apvts.getRawParameterValue ("out_lag");
    master = apvts.getRawParameterValue ("out_master");
}

void OutputModule::prepare (double sampleRate)
{
    fs = (float) sampleRate;
    rampLen = juce::jmax (1, (int) (0.012f * fs));
    first = true;
}

void OutputModule::beginBlock (int)
{
    auto set = [&] (dsp::Ramp& r, float v) { if (first) r.snap (v); else r.to (v, rampLen); };
    set (sLevel, level->load());
    set (sCv, cvAmt->load());
    set (sMaster, juce::Decibels::decibelsToGain (master->load()));
    const float ms = lag->load();
    lagV.set (juce::jmax (0.5f, ms * 0.3f), ms, fs);   // attack a third of the decay, like a vactrol
    first = false;
}

void OutputModule::tick (const float* in, float* out) noexcept
{
    juce::ignoreUnused (out);
    float l = in[In_Left], r = in[In_Right];
    const bool hasL = isPatched (In_Left), hasR = isPatched (In_Right);
    if (hasL && ! hasR) r = l;
    if (hasR && ! hasL) l = r;

    const float lv = sLevel.next(), amt = sCv.next(), m = sMaster.next();
    float g = lv;
    if (isPatched (In_LevelCv))
    {
        const float cv = lagV.process (dsp::clampf (in[In_LevelCv] / 10.0f, 0.0f, 1.0f));
        g = lv * ((1.0f - amt) + amt * cv);
    }
    auto clip = [] (float x)   // gentle safety clip above 0.9
    {
        const float a = std::fabs (x);
        return a < 0.9f ? x : std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) / 0.1f), x);
    };
    left = clip (l * g * m);
    right = clip (r * g * m);
    if (! std::isfinite (left)) left = 0.0f;
    if (! std::isfinite (right)) right = 0.0f;

    pkL = juce::jmax (pkL, std::fabs (left));
    pkR = juce::jmax (pkR, std::fabs (right));
    if (++pkCount >= 256)
    {
        peakL.store (juce::jmax (pkL, peakL.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        peakR.store (juce::jmax (pkR, peakR.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        pkL = pkR = 0.0f;
        pkCount = 0;
    }
}
} // namespace wc
