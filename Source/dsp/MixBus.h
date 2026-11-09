#pragma once
#include "Common.h"
#include "Building.h"
#include <vector>

// MIXBUS: a second cascade of 10 slots with mix-style processors.
namespace pg
{
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline float tick (float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
    // RBJ cookbook
    void set (int type, float f, float q, float gainDb, float sr)
    {
        const float w = kTwoPi * clampf (f, 10.0f, sr * 0.45f) / sr, cw = std::cos (w), sw = std::sin (w);
        const float A = std::pow (10.0f, gainDb / 40.0f), alpha = sw / (2.0f * q);
        float B0, B1, B2, A0, A1, A2;
        switch (type)
        {
            case 0: B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;            // LP
            case 1: B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2; A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha; break;         // HP
            case 2: B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A; A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A; break; // peak
            case 3: { const float s = 2 * std::sqrt (A) * alpha;                                                                       // low shelf
                      B0 = A * ((A + 1) - (A - 1) * cw + s); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - s);
                      A0 = (A + 1) + (A - 1) * cw + s; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - s; break; }
            default: { const float s = 2 * std::sqrt (A) * alpha;                                                                      // high shelf
                      B0 = A * ((A + 1) + (A - 1) * cw + s); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - s);
                      A0 = (A + 1) - (A - 1) * cw + s; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - s; break; }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
};

class MixBus
{
public:
    void prepare (double sampleRate);
    // mods: matrix offsets for MD_Mb* destinations; beat: musical position at block start
    void process (float* L, float* R, int n, const float* p, const float* mods, double bpm, double beat);

private:
    void tape (float* L, float* R, int n, const float* p, float driveMod);
    void eq (float* L, float* R, int n, const float* p);
    void comp (float* L, float* R, int n, const float* p);
    void chorus (float* L, float* R, int n, const float* p);
    void imager (float* L, float* R, int n, const float* p, float widthMod);
    void gate (float* L, float* R, int n, const float* p, double bpm, double beat, float depthMod);
    void dj (float* L, float* R, int n, const float* p, float posMod);
    void crush (float* L, float* R, int n, const float* p, float bitsMod);
    void transient (float* L, float* R, int n, const float* p);
    void limiter (float* L, float* R, int n, const float* p);

    double sr = 48000.0;
    struct Line { std::vector<float> b; int w = 0, mask = 0;
        void init (int len) { int s = 1; while (s < len + 4) s <<= 1; b.assign ((size_t) s, 0.0f); mask = s - 1; w = 0; }
        void push (float x) { b[(size_t) w] = x; w = (w + 1) & mask; }
        float read (float d) const { const float rp = (float) w - clampf (d, 1.0f, (float) mask - 2.0f); const int i = (int) std::floor (rp); const float t = rp - (float) i;
                                     const float a = b[(size_t) (i & mask)], c = b[(size_t) ((i + 1) & mask)]; return a + (c - a) * t; } };
    // tape
    Line tapeLine[2]; float tapePh = 0, flutPh = 0; Biquad tapeTone[2];
    // eq
    Biquad eqLc[2], eqLow[2], eqMid[2], eqHigh[2], eqHc[2];
    // comp
    float compEnv = 0.0f;
    // chorus
    Line chLine[2]; float chPh = 0;
    // imager
    Svf imLow[2];
    // gate
    float gateGain = 1.0f;
    // dj
    Svf djF[2]; float djLast = 0;
    // crush
    float crHold[2] {}; float crCount = 0;
    // transient
    float trFast = 0, trSlow = 0;
    // limiter
    float limGain = 1.0f;
};
} // namespace pg
