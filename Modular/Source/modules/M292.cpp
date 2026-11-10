#include "M292.h"

namespace wc
{
M292::M292 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    for (int n = 0; n < 4; ++n)
    {
        const juce::String s (n + 1);
        offP[n] = apvts.getRawParameterValue (p + "off" + s);
        cvP[n] = apvts.getRawParameterValue (p + "cv" + s);
        resP[n] = apvts.getRawParameterValue (p + "res" + s);
        modeP[n] = apvts.getRawParameterValue (p + "mode" + s);
        jassert (offP[n] != nullptr && cvP[n] != nullptr && resP[n] != nullptr && modeP[n] != nullptr);
    }
}

void M292::prepare (double sampleRate)
{
    fs = (float) sampleRate;
    rampLen = juce::jmax (1, (int) (0.012f * fs));
    first = true;
    reset();
}

void M292::reset()
{
    for (auto& c : ch) { c.ledI = 1.0e-5f; c.s1 = c.s2 = 0.0f; }
    for (auto& l : led) l.store (0.0f);
}

void M292::beginBlock (int)
{
    for (int n = 0; n < 4; ++n)
    {
        auto set = [&] (dsp::Ramp& r, float v) { if (first) r.snap (v); else r.to (v, rampLen); };
        set (ch[n].off, offP[n]->load());
        set (ch[n].cv, cvP[n]->load());
        set (ch[n].res, resP[n]->load());
        ch[n].mode = juce::jlimit (0, 2, (int) std::lround (modeP[n]->load()));   // 0 LOPASS, 1 BOTH, 2 VCA
    }
    first = false;
}

void M292::tick (const float* in, float* out) noexcept
{
    constexpr float C1 = 1.0e-9f, C2 = 220.0e-12f, C3lp = 4.7e-9f;
    constexpr float Imin = 1.0e-5f, Imax = 0.040f;
    constexpr float wOn = 5.0f / 0.012f, wOff = 5.0f / 0.250f;   // rad/s, from the datasheet rise / fall times
    float all = 0.0f;
    for (int n = 0; n < 4; ++n)
    {
        auto& c = ch[n];
        // ---- control path: offset + CV (0..10 V) -> LED current ----
        // The LED sits in its exponential (Shockley) region below the driver's zener knee, so the current
        // grows exponentially with the control voltage: 0 V -> 10 uA (dark), 10 V -> 40 mA (fully lit).
        const float v = dsp::clampf (10.0f * c.off.next() + c.cv.next() * in[inCtl (n)], 0.0f, 10.0f);
        const float target = Imin * std::pow (Imax / Imin, v / 10.0f);
        // ---- vactrol dynamics: fast rise, slow fall, both faster when the LED is brighter ----
        const float rel = c.ledI / Imax;
        const float w = target > c.ledI ? wOn * (0.5f + 0.5f * target / Imax) : wOff * (0.6f + 2.0f * rel);
        c.ledI += (target - c.ledI) * (1.0f - std::exp (-w / fs));
        const float Rf = ldrOhms (c.ledI);

        // ---- audio path: component values per mode ----
        const float Ra = c.mode == 2 ? 5.0e3f : 5.0e6f;
        const float C3 = c.mode == 0 ? C3lp : 0.0f;
        float aFb = 0.0f;
        if (C3 > 0.0f)
        {
            const float aMax = (2.0f * C1 * Ra + (C2 + C3) * (Ra + Rf)) / (C3 * Ra);
            aFb = 0.97f * c.res.next() * aMax;
        }
        else c.res.next();
        const float a1 = 1.0f + 2.0f * Rf / Ra;
        const float a2 = Rf * (2.0f * C1 + C2 - C3 * (aFb - 1.0f) + (C2 + C3) * Rf / Ra);
        const float a3 = Rf * Rf * C1 * (C2 + C3);
        // H(s) = (1/a1) * w0^2 / (s^2 + (w0/Q) s + w0^2),  w0^2 = a1/a3,  w0/Q = a2/a3
        const float w0 = std::sqrt (a1 / a3);
        const float k = (a2 / a3) / w0;                                  // 1/Q
        const float f0 = juce::jmin (w0 / dsp::kTwoPi, 0.45f * fs);
        const float g = std::tan (dsp::kPi * f0 / fs);
        // TPT state-variable filter (Zavalishin), low-pass output
        const float x = in[inSig (n)];
        const float hp = (x - (k + g) * c.s1 - c.s2) / (1.0f + g * (k + g));
        const float bp = g * hp + c.s1;
        const float lp = g * bp + c.s2;
        c.s1 = g * hp + bp;
        c.s2 = g * bp + lp;
        float y = lp / a1;
        if (! std::isfinite (y)) { c.s1 = c.s2 = 0.0f; y = 0.0f; }
        out[outSig (n)] = y;
        all += y;
        led[n].store (rel, std::memory_order_relaxed);
    }
    out[outAll] = all;
}
} // namespace wc
