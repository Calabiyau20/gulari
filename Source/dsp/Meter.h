#pragma once
#include "Common.h"
#include <array>
#include <atomic>
#include <vector>

// Master output metering: true peak (4x oversampled, dBTP), loudness (ITU-R BS.1770 /
// EBU R128: momentary, short-term, integrated LUFS) and a mono feed for the spectrum.
namespace pg
{
class MasterMeter
{
public:
    void prepare (double sampleRate);
    void process (const float* L, const float* R, int n);
    void requestReset() { resetFlag.store (true); }

    // read by the UI
    std::atomic<float> tpL { -120.0f }, tpR { -120.0f }, tpMax { -120.0f };   // dBTP
    std::atomic<float> lufsM { -120.0f }, lufsS { -120.0f }, lufsI { -120.0f };

    static constexpr int kRing = 8192;
    std::array<float, kRing> ring {};
    std::atomic<int> ringPos { 0 };
    double sampleRate() const { return sr; }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        inline double tick (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    };
    float truePeak (int ch, float x);

    double sr = 48000.0;
    // true peak: 4x polyphase interpolator
    static constexpr int kTaps = 12;
    float coef[4][kTaps] {};
    float hist[2][kTaps] {};
    float peakHold[2] { 0.0f, 0.0f }, peakMax = 0.0f;
    float peakDecay = 0.999f;

    // loudness
    Biquad shelf[2], hp[2];
    int blockLen = 4800, blockFill = 0;
    double blockEnergy = 0.0;
    std::array<double, 30> blocks {};   // energy of the last 30 x 100 ms
    int blockCount = 0, blockIdx = 0;
    // integrated: histogram of gated 400 ms loudness values (0.1 LU bins from -70 to +5)
    static constexpr int kBins = 750;
    std::array<double, kBins> histE {};
    std::array<int, kBins> histN {};
    std::atomic<bool> resetFlag { false };
    void resetAll();
};
} // namespace pg
