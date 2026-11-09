#pragma once
#include "Common.h"
#include "Building.h"
#include "MutableWrappers.h"
#include "Voice.h"
#include "MixBus.h"
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

// Filter FX: filterbank, positive / negative multi-stage combs, morphing SVF, vowel formants, allpass chain
class FilterFx
{
public:
    void prepare (double sampleRate);
    void process (float* L, float* R, int n, int type, float freq, float res, float morph, int poles, float spread, float drive, float mix);
private:
    double sr = 48000.0;
    Svf svf[2][8];
    DelayLine comb[2][8];
    float combLp[2][8] {};
    struct Ap1 { float z = 0.0f; float tick (float x, float a) { const float y = -a * x + z; z = x + a * y; return y; } };
    Ap1 ap[2][16];
    float apFb[2] {};
    Smooth fSmooth;
    int lastType = -1;
};

// Feedback-delay-network reverb (8 lines, Hadamard mix, damped, modulated)
class Reverb
{
public:
    void prepare (double sampleRate);
    void process (float* L, float* R, int n, float size, float decay, float damp, float preMs, float modAmt, float width, float mix);
private:
    double sr = 48000.0;
    DelayLine pre;
    DelayLine line[8];
    float lp[8] {};
    struct Ap { DelayLine d; float g = 0.6f; float tick (float x, float len) { const float v = d.read (len); const float y = -g * x + v; d.push (x + g * y); return y; } };
    Ap diff[4];
    float lfo = 0.0f;
};

class FxChain
{
public:
    void prepare (double sampleRate, int maxBlock);
    // params: parameter cache; global: shared modulators (for FX matrix destinations)
    void process (float* l, float* r, int n, const float* params, const GlobalMod& global, int lastNote, bool strum);

private:
    void computeMods (const float* p, const GlobalMod& g);
    void runStereo (float* L, float* R, int n, const float* p);
    void runFlanger (float* L, float* R, int n, const float* p);
    void runPhaser (float* L, float* R, int n, const float* p);
    void runDelay (float* L, float* R, int n, const float* p, const GlobalMod& g);
    void runRings (float* L, float* R, int n, const float* p, int lastNote, bool strum);
    void runClouds (float* L, float* R, int n, const float* p);
    void runFilter (int which, float* L, float* R, int n, const float* p);
    void runReverb (float* L, float* R, int n, const float* p);
    void runRack (float* L, float* R, int n, const float* p, const GlobalMod& g, int lastNote, bool strum);
    MixBus mixbus;
    double sr = 48000.0;
    float fxMod[MD_Count] {};
    // switching an effect on / off fades its wet path over 10 ms instead of jumping
    float fxWet[FX_Count] {};
    bool wetInit = false;
    std::vector<float> dryL, dryR;

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
    FilterFx filt[2];
    Reverb reverb;
    std::vector<float> tmpL, tmpR;
};
} // namespace pg
