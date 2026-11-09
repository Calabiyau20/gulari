#include "Meter.h"
#include <cmath>
#include <algorithm>

namespace pg
{
static constexpr double kPiD = 3.14159265358979323846;
static inline float toDb (double e) { return e > 1.0e-12 ? (float) (10.0 * std::log10 (e)) : -120.0f; }

void MasterMeter::prepare (double sampleRate)
{
    sr = sampleRate;
    // 4x interpolator: windowed sinc, 48 taps split in 4 phases
    for (int ph = 0; ph < 4; ++ph)
    {
        float sum = 0.0f;
        for (int t = 0; t < kTaps; ++t)
        {
            const double x = (double) (t - kTaps / 2 + 1) - (double) ph / 4.0;   // distance in input samples
            const double sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (kPiD * x) / (kPiD * x);
            const double w = 0.42 + 0.5 * std::cos (kPiD * x / (kTaps / 2.0)) + 0.08 * std::cos (2.0 * kPiD * x / (kTaps / 2.0));
            coef[ph][t] = (float) (sinc * std::max (0.0, w));
            sum += coef[ph][t];
        }
        for (auto& c : coef[ph]) c /= sum;
    }
    peakDecay = (float) std::exp (-1.0 / (0.3 * sr));   // ~20 dB per second fall-back

    // K-weighting (BS.1770), coefficients for any sample rate
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (kPiD * f0 / sr), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        for (auto& b : shelf)
        {
            b.b0 = (Vh + Vb * K / Q + K * K) / a0; b.b1 = 2.0 * (K * K - Vh) / a0; b.b2 = (Vh - Vb * K / Q + K * K) / a0;
            b.a1 = 2.0 * (K * K - 1.0) / a0; b.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (kPiD * f0 / sr), a0 = 1.0 + K / Q + K * K;
        for (auto& b : hp)
        {
            b.b0 = 1.0; b.b1 = -2.0; b.b2 = 1.0;
            b.a1 = 2.0 * (K * K - 1.0) / a0; b.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }
    blockLen = std::max (1, (int) std::round (sr * 0.1));
    resetAll();
}

void MasterMeter::resetAll()
{
    for (auto& h : hist) for (auto& v : h) v = 0.0f;
    peakHold[0] = peakHold[1] = peakMax = 0.0f;
    for (int c = 0; c < 2; ++c) { shelf[c].z1 = shelf[c].z2 = hp[c].z1 = hp[c].z2 = 0.0; }
    blockFill = 0; blockEnergy = 0.0; blocks.fill (0.0); blockCount = 0; blockIdx = 0;
    histE.fill (0.0); histN.fill (0);
    tpMax.store (-120.0f); lufsI.store (-120.0f);
}

float MasterMeter::truePeak (int ch, float x)
{
    float* h = hist[ch];
    for (int t = 0; t < kTaps - 1; ++t) h[t] = h[t + 1];
    h[kTaps - 1] = x;
    float mx = 0.0f;
    for (int ph = 0; ph < 4; ++ph)
    {
        float y = 0.0f;
        for (int t = 0; t < kTaps; ++t) y += h[t] * coef[ph][kTaps - 1 - t];
        mx = std::max (mx, std::abs (y));
    }
    return mx;
}

void MasterMeter::process (const float* L, const float* R, int n)
{
    if (resetFlag.exchange (false)) resetAll();
    int wp = ringPos.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i)
    {
        const float l = L[i], r = R[i];
        // spectrum feed (mono)
        ring[(size_t) wp] = 0.5f * (l + r);
        wp = (wp + 1) & (kRing - 1);

        // true peak with hold / fall-back
        const float pl = truePeak (0, l), pr = truePeak (1, r);
        peakHold[0] = std::max (pl, peakHold[0] * peakDecay);
        peakHold[1] = std::max (pr, peakHold[1] * peakDecay);
        peakMax = std::max (peakMax, std::max (pl, pr));

        // loudness
        const double kl = hp[0].tick (shelf[0].tick (l)), kr = hp[1].tick (shelf[1].tick (r));
        blockEnergy += kl * kl + kr * kr;
        if (++blockFill >= blockLen)
        {
            blocks[(size_t) blockIdx] = blockEnergy / blockLen;
            blockIdx = (blockIdx + 1) % (int) blocks.size();
            blockCount = std::min (blockCount + 1, 1 << 30);
            blockEnergy = 0.0;
            blockFill = 0;

            auto meanOf = [&] (int count)
            {
                double s = 0.0;
                const int c = std::min (count, std::min (blockCount, (int) blocks.size()));
                for (int k = 1; k <= c; ++k) s += blocks[(size_t) ((blockIdx - k + (int) blocks.size()) % (int) blocks.size())];
                return c > 0 ? s / c : 0.0;
            };
            const double eM = meanOf (4), eS = meanOf (30);
            const float lm = -0.691f + toDb (eM);
            lufsM.store (blockCount >= 4 ? lm : -120.0f);
            lufsS.store (blockCount >= 30 ? -0.691f + toDb (eS) : lufsM.load());

            // integrated: every 100 ms add the 400 ms block (75 % overlap) above the -70 LUFS gate
            if (blockCount >= 4 && lm > -70.0f)
            {
                const int bin = std::max (0, std::min (kBins - 1, (int) ((lm + 70.0f) * 10.0f)));
                histE[(size_t) bin] += eM;
                histN[(size_t) bin] += 1;
            }
            double sE = 0.0; long long sN = 0;
            for (int b = 0; b < kBins; ++b) { sE += histE[(size_t) b]; sN += histN[(size_t) b]; }
            if (sN > 0)
            {
                const float rel = -0.691f + toDb (sE / (double) sN) - 10.0f;
                const int start = std::max (0, std::min (kBins - 1, (int) std::ceil ((rel + 70.0f) * 10.0f)));
                double gE = 0.0; long long gN = 0;
                for (int b = start; b < kBins; ++b) { gE += histE[(size_t) b]; gN += histN[(size_t) b]; }
                if (gN > 0) lufsI.store (-0.691f + toDb (gE / (double) gN));
            }
        }
    }
    ringPos.store (wp, std::memory_order_release);
    tpL.store (20.0f * std::log10 (std::max (1.0e-6f, peakHold[0])));
    tpR.store (20.0f * std::log10 (std::max (1.0e-6f, peakHold[1])));
    tpMax.store (20.0f * std::log10 (std::max (1.0e-6f, peakMax)));
}
} // namespace pg
