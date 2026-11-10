#pragma once
// =====================================================================================================
// Model 259 timbre network: five parallel op-amp folding cells plus a direct path, summed with the
// weights of the original circuit, then the one-pole low pass of the upper summing amplifier.
//
// Source: Esqueda, Poentynen, Valimaki, Parker, "Virtual Analog Buchla 259 Wavefolder", DAFx-17.
// Component values (Table 1): C = 100 pF, R7 = 24.9k, RF1 = 24.9k, RF2 = 1.2M, Vs (cells) = 6 V.
//
//               +--[cell 1: G=0.8333 T=0.600 V]--+
//               +--[cell 2: G=0.3768 T=2.994 V]--+--[upper summer  -RF2 || C]--+--> V'out --> 1 pole 1.33 kHz
//   A*sin+Vsym -+--[cell 3: G=0.2829 T=5.460 V]--+                               ^
//               +--[cell 4: G=0.5743 T=1.800 V]--+                               |
//               +--[cell 5: G=0.2673 T=4.080 V]--+--[lower summer V7]--[R7]------+
//               +--[direct path R6,3]------------+
//
//   cell k:   V_k = G_k (Vin - T_k sgn Vin)   if |Vin| > T_k,   else 0      (dead zone + linear segment)
//   V'out = -12.000 V1 - 27.777 V2 - 21.428 V3 + 17.647 V4 + 36.363 V5 + 5.000 Vin            (eq. 18)
//
// Antialiasing: first-order antiderivative (ADAA1). The nonlinearity is piecewise linear, so its
// antiderivative is piecewise quadratic and exact:  F_k(x) = G_k (|x| - T_k)^2 / 2  for |x| > T_k.
// =====================================================================================================
#include "Dsp.h"

namespace wc
{
struct Wavefolder259
{
    static constexpr int kCells = 5;
    static constexpr float G[kCells] = { 0.8333f, 0.3768f, 0.2829f, 0.5743f, 0.2673f };
    static constexpr float T[kCells] = { 0.6000f, 2.9940f, 5.4600f, 1.8000f, 4.0800f };
    static constexpr float W[kCells] = { -12.000f, -27.777f, -21.428f, 17.647f, 36.363f };
    static constexpr float kDirect = 5.000f;
    static constexpr float kFc = 1326.3f;   // 1 / (2 pi RF2 C) = 1 / (2 pi 1.2M 100p)

    // static transfer V'out(Vin)
    static float transfer (float x) noexcept
    {
        const float ax = std::fabs (x), s = dsp::sgn (x);
        float y = kDirect * x;
        for (int k = 0; k < kCells; ++k)
            if (ax > T[k]) y += W[k] * G[k] * (x - T[k] * s);
        return y;
    }
    // its antiderivative
    static double antiderivative (double x) noexcept
    {
        const double ax = std::fabs (x);
        double y = 0.5 * kDirect * x * x;
        for (int k = 0; k < kCells; ++k)
            if (ax > T[k]) { const double d = ax - T[k]; y += W[k] * G[k] * 0.5 * d * d; }
        return y;
    }

    // ---- running state ----
    double xPrev = 0.0, fPrev = 0.0;
    dsp::OnePoleLP lp;

    void prepare (float fs) noexcept { lp.set (kFc, fs); reset(); }
    void reset() noexcept { xPrev = 0.0; fPrev = 0.0; lp.reset(); }

    // one sample at the oversampled rate. x in volts (A * sine + symmetry offset), returns V'out after the pole
    float process (float x) noexcept
    {
        const double xd = x;
        const double F = antiderivative (xd);
        const double dx = xd - xPrev;
        float y;
        if (std::fabs (dx) > 1.0e-5)
            y = (float) ((F - fPrev) / dx);
        else
            y = transfer ((float) (0.5 * (xd + xPrev)));   // ill-conditioned: midpoint
        xPrev = xd;
        fPrev = F;
        return lp.process (y);
    }
};
} // namespace wc
