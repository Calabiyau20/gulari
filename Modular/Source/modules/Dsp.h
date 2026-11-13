#pragma once
// Small DSP building blocks shared by the West Coast modules.
#include <cmath>
#include <array>
#include <algorithm>

namespace wc::dsp
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float sgn (float v) { return v > 0.0f ? 1.0f : (v < 0.0f ? -1.0f : 0.0f); }

// 2-point polyBLEP residual for a unit step at fractional position t (phase) with increment dt
inline float polyBlep (float t, float dt) noexcept
{
    if (dt <= 0.0f) return 0.0f;
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// one-pole low pass, bilinear (matches the analog RC pole)
struct OnePoleLP
{
    float b = 1.0f, a = 0.0f, z = 0.0f;
    void set (float fc, float fs) noexcept
    {
        const float wcT = kTwoPi * fc / fs;
        b = wcT / (2.0f + wcT);
        a = (wcT - 2.0f) / (wcT + 2.0f);
    }
    // H(z) = b (1 + z^-1) / (1 + a z^-1), transposed direct form II
    float process (float x) noexcept
    {
        const float y = b * x + z;
        z = b * x - a * y;
        return y;
    }
    void reset() noexcept { z = 0.0f; }
};

// AC coupling (one-pole high pass)
struct DcBlock
{
    float k = 0.001f, z = 0.0f;
    void set (float fc, float fs) noexcept { k = 1.0f - std::exp (-kTwoPi * fc / fs); }
    float process (float x) noexcept { z += (x - z) * k; return x - z; }
    void reset() noexcept { z = 0.0f; }
};

// Vactrol (LED + LDR): fast attack, slow decay - used on control signals (AM index, output level)
struct Vactrol
{
    float up = 0.1f, down = 0.01f, y = 0.0f;
    void set (float attackMs, float decayMs, float fs) noexcept
    {
        up = 1.0f - std::exp (-1.0f / (0.001f * attackMs * fs));
        down = 1.0f - std::exp (-1.0f / (0.001f * decayMs * fs));
    }
    float process (float x) noexcept { y += (x - y) * (x > y ? up : down); return y; }
};

// Linear-phase FIR decimator 4:1 (63 taps, Kaiser beta 8, passband to ~0.42 fs_out)
struct Decimator4
{
    static constexpr int kTaps = 63;
    static std::array<float, kTaps> makeTaps()
    {
        std::array<float, kTaps> h {};
        const double fc = 0.105;     // cycles / sample at the 4x rate
        const double beta = 8.0;
        auto bessel0 = [] (double x) { double s = 1.0, t = 1.0; for (int k = 1; k < 30; ++k) { t *= (x / (2.0 * k)) * (x / (2.0 * k)); s += t; } return s; };
        double sum = 0.0;
        for (int n = 0; n < kTaps; ++n)
        {
            const double m = n - (kTaps - 1) / 2.0;
            const double sinc = m == 0.0 ? 2.0 * fc : std::sin (2.0 * 3.14159265358979 * fc * m) / (3.14159265358979 * m);
            const double r = 2.0 * n / (kTaps - 1) - 1.0;
            const double w = bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel0 (beta);
            h[(size_t) n] = (float) (sinc * w);
            sum += sinc * w;
        }
        for (auto& v : h) v = (float) (v / sum);
        return h;
    }
    std::array<float, kTaps * 2> buf {};   // doubled ring so the dot product reads contiguously
    int pos = 0;
    void push (float x) noexcept
    {
        buf[(size_t) pos] = x;
        buf[(size_t) (pos + kTaps)] = x;
        if (++pos >= kTaps) pos = 0;
    }
    float output (const std::array<float, kTaps>& h) const noexcept
    {
        const float* b = buf.data() + pos;   // oldest .. newest
        float s = 0.0f;
        for (int i = 0; i < kTaps; ++i) s += h[(size_t) i] * b[i];
        return s;
    }
    void reset() noexcept { buf.fill (0.0f); pos = 0; }
};

// smoothed control value: raised-cosine ramp to a target over a fixed number of samples (no corner where it starts
// or lands, so a jump leaves far less above a few kHz than a straight line); a new target starts from where it is
struct Ramp
{
    float v = 0.0f, target = 0.0f, from = 0.0f;
    int left = 0, len = 1;
    void snap (float x) noexcept { v = from = target = x; left = 0; }
    void to (float x, int n) noexcept
    {
        if (x == target) return;   // the same target keeps the ramp on course: restarting it never lands with short blocks
        from = v; target = x;
        left = len = std::max (1, n);
    }
    float next() noexcept
    {
        if (left > 0)
        {
            if (--left == 0) v = target;
            else v = from + (target - from) * (0.5f - 0.5f * std::cos (kPi * (float) (len - left) / (float) len));
        }
        return v;
    }
};
// zero-delay-feedback state variable filter (Simper / TPT); g = tan (pi fc / fs), k = 1 / Q
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f, g = 0.1f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    void set (float fc, float q, float fs) noexcept
    {
        g = std::tan (kPi * clampf (fc, 5.0f, 0.45f * fs) / fs);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    // returns band pass; low / high through the references
    float process (float x, float& lp, float& hp) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        lp = v2; hp = x - k * v1 - v2;
        if (! std::isfinite (ic1) || ! std::isfinite (ic2)) { ic1 = ic2 = 0.0f; }
        return v1;
    }
    float bp (float x) noexcept { float l, h; return process (x, l, h); }
    void reset() noexcept { ic1 = ic2 = 0.0f; }
};

// shelving filters (RBJ cookbook biquads), gain in dB
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void shelf (bool high, float fc, float dB, float fs) noexcept
    {
        const float A = std::pow (10.0f, dB / 40.0f), w = kTwoPi * fc / fs, cw = std::cos (w), sw = std::sin (w);
        const float al = sw / 2.0f * std::sqrt (2.0f), sa = 2.0f * std::sqrt (A) * al;
        float B0, B1, B2, A0, A1, A2;
        if (! high) { B0 = A * ((A + 1) - (A - 1) * cw + sa); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sa);
                      A0 = (A + 1) + (A - 1) * cw + sa; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - sa; }
        else        { B0 = A * ((A + 1) + (A - 1) * cw + sa); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sa);
                      A0 = (A + 1) - (A - 1) * cw + sa; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - sa; }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0.0f; }
};

// white noise (xorshift32), uniform -1..1
struct Noise
{
    unsigned int s = 0x9e3779b9u;
    void seed (unsigned int v) noexcept { s = v ? v : 1u; }
    float next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int) s * (1.0f / 2147483648.0f); }
    float uni() noexcept { return 0.5f * next() + 0.5f; }
};

// pink (-3 dB / oct) filter for white noise (Paul Kellet, economy)
struct Pink
{
    float b0 = 0, b1 = 0, b2 = 0;
    float process (float w) noexcept
    {
        b0 = 0.99765f * b0 + w * 0.0990460f; b1 = 0.96300f * b1 + w * 0.2965164f; b2 = 0.57000f * b2 + w * 1.0526913f;
        return (b0 + b1 + b2 + w * 0.1848f) * 0.22f;
    }
};
} // namespace wc::dsp
