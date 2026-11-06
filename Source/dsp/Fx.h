#pragma once
#include "Common.h"
#include "Building.h"
#include "MutableWrappers.h"
#include "Voice.h"
#include <vector>

namespace pg
{
// Simple fractional delay line
struct DelayLine
{
    std::vector<float> buf;
    int w = 0, mask = 0;
    void init (int maxSamples)
    {
        int size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read (float delaySamples) const
    {
        const float d = clampf (delaySamples, 1.0f, (float) mask - 3.0f);
        const float rp = (float) w - d;
        const int i = (int) std::floor (rp);
        const float t = rp - (float) i;
        auto at = [&] (int k) { return buf[(size_t) (k & mask)]; };
        return hermite (at (i - 1), at (i), at (i + 1), at (i + 2), t);
    }
};

class FxChain
{
public:
    void prepare (double sampleRate, int maxBlock);
    // params: parameter cache; global: shared modulators (for FX matrix destinations)
    void process (float* l, float* r, int n, const float* params, const GlobalMod& global, int lastNote, bool strum);

private:
    void computeMods (const float* p, const GlobalMod& g);
    double sr = 48000.0;
    float fxMod[MD_Count] {};

    // stereo
    Svf stLow[2];
    DelayLine haas;
    // flanger
    DelayLine flDelay[2];
    float flPhase = 0.0f, flFb[2] {};
    // phaser
    struct Allpass { float z = 0.0f; float tick (float x, float a) { const float y = -a * x + z; z = x + a * y; return y; } };
    Allpass ph[2][12];
    float phPhase = 0.0f, phFb[2] {};
    // delay
    DelayLine dl[2];
    Svf dlLow[2], dlHigh[2];
    Smooth dlTime;
    // Mutable
    RingsFx rings;
    CloudsFx clouds;
    std::vector<float> tmpL, tmpR;
};
} // namespace pg
