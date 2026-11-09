#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <array>

// The Calabi-Yau orbit oscillator: Fermat surface z1^n + z2^n = 1, Reusser's parametrisation
//   z1 = e^(2 pi i k1 / n) * cos(u + iv)^(2/n),   z2 = e^(2 pi i k2 / n) * sin(u + iv)^(2/n)
//   (x, y, z) = (Re z1, Re z2, Im z1 cos(alpha) + Im z2 sin(alpha))
// A note walks a closed orbit (u, v) on one patch; the walk is the waveform.
namespace cyr
{
constexpr float kPiF = 3.14159265358979f, kTwoPiF = 6.28318530717959f, kHalfPiF = 1.5707963267949f;

namespace fast
{
    // sin / cos from a 4096-point table, linear interpolation (|err| < 3e-7)
    struct SinTable
    {
        static constexpr int N = 4096;
        std::array<float, N + 1> t {};
        SinTable() { for (int i = 0; i <= N; ++i) t[(size_t) i] = (float) std::sin (2.0 * 3.14159265358979323846 * i / N); }
    };
    inline const SinTable& sinTable() { static const SinTable s; return s; }

    inline float floorf (float x) { const int i = (int) x; return (float) (i - (x < (float) i ? 1 : 0)); }

    // sin(2 pi c), c in cycles (any value)
    inline float sinCycles (float c)
    {
        c -= floorf (c);
        const float x = c * (float) SinTable::N;
        const int i = (int) x;
        const float f = x - (float) i;
        const auto& t = sinTable().t;
        return t[(size_t) i] + (t[(size_t) i + 1] - t[(size_t) i]) * f;
    }
    inline float cosCycles (float c) { return sinCycles (c + 0.25f); }
    inline float sinR (float r) { return sinCycles (r * (1.0f / kTwoPiF)); }
    inline float cosR (float r) { return sinCycles (r * (1.0f / kTwoPiF) + 0.25f); }

    inline float bitsToFloat (uint32_t b) { float f; std::memcpy (&f, &b, 4); return f; }
    inline uint32_t floatToBits (float f) { uint32_t b; std::memcpy (&b, &f, 4); return b; }

    // 2^x, rel. err ~2e-7
    inline float exp2f (float x)
    {
        x = x < -126.0f ? -126.0f : (x > 126.0f ? 126.0f : x);
        const float fl = floorf (x);
        const float f = x - fl;
        const float p = 1.0f + f * (0.69314718f + f * (0.24022650f + f * (0.05550411f + f * (0.00961813f + f * (0.00133336f + f * 0.00015404f)))));
        return p * bitsToFloat ((uint32_t) ((int) fl + 127) << 23);
    }
    inline float expf (float x) { return exp2f (x * 1.44269504f); }

    inline float log2f_m (float m)
    {
        const float t = (m - 1.0f) / (m + 1.0f), t2 = t * t;
        return 2.88539008f * t * (1.0f + t2 * (0.33333333f + t2 * (0.2f + t2 * (0.14285714f + t2 * 0.11111111f))));
    }

    // log2(x) for x > 0, abs. err ~1e-6
    inline float log2f (float x)
    {
        uint32_t b = floatToBits (x);
        const int e = (int) ((b >> 23) & 255) - 127;
        b = (b & 0x007FFFFFu) | 0x3F800000u;
        float m = bitsToFloat (b);                 // [1, 2)
        if (m > 1.41421356f) { m *= 0.5f; return (float) (e + 1) + log2f_m (m); }
        return (float) e + log2f_m (m);
    }
    // atan2, abs. err ~1e-6 rad
    inline float atan2f (float y, float x)
    {
        const float ax = std::abs (x), ay = std::abs (y);
        if (ax < 1.0e-30f && ay < 1.0e-30f) return 0.0f;
        const bool swap = ay > ax;
        const float z = swap ? ax / ay : ay / ax, z2 = z * z;
        float a = z * (0.99997726f + z2 * (-0.33262347f + z2 * (0.19354346f + z2 * (-0.11643287f + z2 * (0.05265332f + z2 * -0.01172120f)))));
        if (swap) a = kHalfPiF - a;
        if (x < 0.0f) a = kPiF - a;
        return y < 0.0f ? -a : a;
    }
}

// Per-oscillator constants for one control block
struct OrbitShape
{
    float pw = 0.4f;          // 2 / n
    float k1 = 0.0f, k2 = 0.0f;   // patch angles 2 pi k / n
    float ruA = 0.5f;         // u radius in radians (around pi / 4)
    float rvA = 0.5f;         // v radius in radians
    float ratio = 1.0f;
    float ca = 1.0f, sa = 0.0f;   // alpha
    float width = 0.6f;
};

// One point of the orbit: phase in cycles, layer offset (radians) for unison, spin rotation (radians).
// Writes the left / right sample (centre = Re z1 + Re z2 + alpha projection, side = Re z1 - Re z2) and the raw point.
inline void orbitSample (const OrbitShape& s, float phase, float layerOff, float spin, float& outL, float& outR,
                         float* xyz = nullptr)
{
    using namespace fast;
    const float u = 0.78539816f + s.ruA * cosCycles (phase);
    const float v = s.rvA * sinR (s.ratio * kTwoPiF * phase + layerOff);
    const float cu = cosR (u), su = sinR (u);
    const float ev = expf (v), emv = 1.0f / ev;
    const float ch = 0.5f * (ev + emv), sh = 0.5f * (ev - emv);
    const float cr = cu * ch, ci = -su * sh, sr = su * ch, si = cu * sh;
    const float m1 = cr * cr + ci * ci + 1.0e-20f, m2 = sr * sr + si * si + 1.0e-20f;
    const float r1 = exp2f (log2f (m1) * s.pw * 0.5f), r2 = exp2f (log2f (m2) * s.pw * 0.5f);
    const float t1 = atan2f (ci, cr) * s.pw + s.k1 + spin;
    const float t2 = atan2f (si, sr) * s.pw + s.k2 - spin;
    const float X = r1 * cosR (t1), Y = r2 * cosR (t2);
    const float Z = r1 * sinR (t1) * s.ca + r2 * sinR (t2) * s.sa;
    // centre: both surface coordinates plus the alpha projection; side: their difference, scaled by width
    const float c = 0.5f * (X + Y) + 0.7f * Z, d = 0.5f * (X - Y) * s.width;
    outL = c + d;
    outR = c - d;
    if (xyz != nullptr) { xyz[0] = X; xyz[1] = Y; xyz[2] = Z; }
}

// Exact complex evaluation for drawing the surface (double precision, not used on the audio path)
inline void surfacePoint (double u, double v, int k1, int k2, int n, double alpha, float* out)
{
    const double ch = std::cosh (v), sh = std::sinh (v), cu = std::cos (u), su = std::sin (u);
    const double cr = cu * ch, ci = -su * sh, sr = su * ch, si = cu * sh;
    const double p = 2.0 / n, tau = 6.283185307179586;
    const double r1 = std::pow (std::hypot (cr, ci), p), t1 = std::atan2 (ci, cr) * p + tau * k1 / n;
    const double r2 = std::pow (std::hypot (sr, si), p), t2 = std::atan2 (si, sr) * p + tau * k2 / n;
    out[0] = (float) (r1 * std::cos (t1));
    out[1] = (float) (r2 * std::cos (t2));
    out[2] = (float) (r1 * std::sin (t1) * std::cos (alpha) + r2 * std::sin (t2) * std::sin (alpha));
}

// Phase distortion: moves the half-cycle point to 'knee' (0.5 = no change)
inline float pdWarp (float ph, float knee)
{
    return ph < knee ? 0.5f * ph / knee : 0.5f + 0.5f * (ph - knee) / (1.0f - knee);
}
} // namespace cyr
