#pragma once
#include "Common.h"
#include <juce_core/juce_core.h>

// Small DSP building blocks used by the voice: envelope, filters, bite, noise, LFO.
namespace pg
{
// ---------------------------------------------------------------------------
// ADSR with analogue-style exponential curves
// ---------------------------------------------------------------------------
class Adsr
{
public:
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    void setSampleRate (float r) { sr = r; }
    void set (float a, float d, float s, float r)
    {
        attackInc = 1.0f / juce::jmax (1.0f, a * sr);
        decayCoef = coef (d);
        sustain = s;
        releaseCoef = coef (r);
    }
    void gateOn (bool hardReset) { if (hardReset) value = 0.0f; stage = Attack; }
    void gateOff() { if (stage != Idle) stage = Release; }
    void kill() { stage = Idle; value = 0.0f; }
    bool active() const { return stage != Idle; }
    float current() const { return value; }

    float next()
    {
        switch (stage)
        {
            case Attack:
                value += attackInc * (1.25f - value * 0.25f);
                if (value >= 1.0f) { value = 1.0f; stage = Decay; }
                break;
            case Decay:
                value = sustain + (value - sustain) * decayCoef;
                if (value - sustain < 1.0e-4f) stage = Sustain;
                break;
            case Sustain: value = sustain; break;
            case Release:
                value *= releaseCoef;
                if (value < 1.0e-5f) { value = 0.0f; stage = Idle; }
                break;
            case Idle: default: break;
        }
        return value;
    }

private:
    float coef (float t) const { return std::exp (-4.6f / juce::jmax (1.0f, t * sr)); }   // ~ -40 dB over t
    float sr = 48000.0f, value = 0.0f, attackInc = 1.0f, decayCoef = 0.0f, sustain = 1.0f, releaseCoef = 0.0f;
    Stage stage = Idle;
};

// ---------------------------------------------------------------------------
// Zero-delay-feedback state variable filter (Cytomic / Simper)
// ---------------------------------------------------------------------------
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f, a1 = 0, a2 = 0, a3 = 0, k = 1.0f;
    void set (float cutoff, float q, float sampleRate)
    {
        const float g = std::tan (kPi * clampf (cutoff, 10.0f, sampleRate * 0.48f) / sampleRate);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    // returns lp, bp, hp through refs
    inline void tick (float x, float& lp, float& bp, float& hp)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0.0f; }
};

// ---------------------------------------------------------------------------
// Ladder filter (4 one-pole TPT stages, tanh feedback); modes by pole mixing
// ---------------------------------------------------------------------------
struct Ladder
{
    float s[4] {}, G = 0.0f, g = 0.0f, k = 0.0f;
    void set (float cutoff, float res, float sampleRate)
    {
        g = std::tan (kPi * clampf (cutoff, 10.0f, sampleRate * 0.45f) / sampleRate);
        G = g / (1.0f + g);
        k = res * 3.9f;
    }
    inline float tick (float x, int type, bool steep)
    {
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const float S = (s[0] * G3 + s[1] * G2 + s[2] * G + s[3]) / (1.0f + g);
        const float u = fastTanh ((x - k * S) / (1.0f + k * G4) * 1.0f);
        float y[5];
        y[0] = u;
        float in = u;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (in - s[i]) * G;
            const float out = v + s[i];
            s[i] = out + v;
            y[i + 1] = out;
            in = out;
        }
        switch (type)
        {
            case 0: return steep ? y[4] : y[2];                                                // LP
            case 1: return steep ? (4.0f * y[2] - 8.0f * y[3] + 4.0f * y[4]) : (2.0f * y[1] - 2.0f * y[2]);   // BP
            case 2: return steep ? (y[0] - 4.0f * y[1] + 6.0f * y[2] - 4.0f * y[3] + y[4]) : (y[0] - 2.0f * y[1] + y[2]); // HP
            default: return y[0] - 2.0f * y[1] + 2.0f * y[2];                                  // notch-ish
        }
    }
    void reset() { for (auto& v : s) v = 0.0f; }
};

// Stereo filter: SVF (12/24 dB) or ladder
class VoiceFilter
{
public:
    void set (int model, int type, bool steep, float cutoff, float res, float drive, float sampleRate)
    {
        mdl = model; typ = type; stp = steep;
        driveGain = 1.0f + drive * 6.0f;
        if (model == 0)
        {
            const float q = 0.5f + res * res * 24.0f;
            for (int c = 0; c < 2; ++c)
            {
                a[c].set (cutoff, steep ? juce::jmax (0.5f, q * 0.6f) : q, sampleRate);
                b[c].set (cutoff, 0.7071f, sampleRate);
            }
        }
        else
            for (int c = 0; c < 2; ++c) lad[c].set (cutoff, res, sampleRate);
    }
    inline float tick (int c, float x)
    {
        if (driveGain > 1.001f) x = fastTanh (x * driveGain) / std::sqrt (driveGain);
        if (mdl == 1) return lad[c].tick (x, typ, stp);
        float y = pick (a[c], x);
        if (stp) y = pick (b[c], y);
        return y;
    }
    void reset() { for (int c = 0; c < 2; ++c) { a[c].reset(); b[c].reset(); lad[c].reset(); } }

private:
    inline float pick (Svf& f, float x)
    {
        float lp, bp, hp;
        f.tick (x, lp, bp, hp);
        switch (typ) { case 0: return lp; case 1: return bp; case 2: return hp; default: return lp + hp; }
    }
    Svf a[2], b[2];
    Ladder lad[2];
    int mdl = 0, typ = 0;
    bool stp = false;
    float driveGain = 1.0f;
};

// ---------------------------------------------------------------------------
// Bite: drive -> asymmetry -> fold -> soft clip -> bit/rate crush
// ---------------------------------------------------------------------------
struct Bite
{
    float drive = 0, fold = 0, crush = 0, soft = 0, asym = 0, mix = 1.0f;
    float hold[2] {}, holdCount = 0.0f;
    float dcX[2] {}, dcY[2] {};

    bool bypassed() const { return drive <= 0.0f && fold <= 0.0f && crush <= 0.0f && soft <= 0.0f && asym <= 0.0f; }

    inline void process (float& l, float& r)
    {
        if (bypassed() || mix <= 0.0f) return;
        const float inL = l, inR = r;
        const float g = 1.0f + drive * drive * 24.0f;
        float x[2] = { l * g, r * g };
        for (int c = 0; c < 2; ++c)
        {
            float v = x[c] + asym * 0.6f;
            if (fold > 0.0f)
                v = std::sin (v * (1.0f + fold * 6.0f) * 1.5707963f) * fold + v * (1.0f - fold);
            if (soft > 0.0f)
                v = fastTanh (v * (1.0f + soft * 3.0f)) * soft + clampf (v, -1.5f, 1.5f) * (1.0f - soft);
            // DC blocker (asymmetry adds DC)
            const float y = v - dcX[c] + 0.995f * dcY[c];
            dcX[c] = v; dcY[c] = y;
            x[c] = y / std::sqrt (g);
        }
        if (crush > 0.0f)
        {
            const float levels = std::exp2 (16.0f - crush * 13.0f);
            const float step = 1.0f + crush * crush * 40.0f;
            holdCount += 1.0f;
            if (holdCount >= step)
            {
                holdCount -= step;
                hold[0] = std::round (x[0] * levels) / levels;
                hold[1] = std::round (x[1] * levels) / levels;
            }
            x[0] = hold[0]; x[1] = hold[1];
        }
        l = inL + (x[0] - inL) * mix;
        r = inR + (x[1] - inR) * mix;
    }
};

// ---------------------------------------------------------------------------
// Noise: white, pink (Paul Kellet), crackle
// ---------------------------------------------------------------------------
struct Noise
{
    Rng rng;
    float b0 = 0, b1 = 0, b2 = 0, crackle = 0.0f;
    inline float tick (int type)
    {
        const float w = rng.bi();
        if (type == 0) return w * 0.6f;
        if (type == 1)
        {
            b0 = 0.99765f * b0 + w * 0.0990460f;
            b1 = 0.96300f * b1 + w * 0.2965164f;
            b2 = 0.57000f * b2 + w * 1.0526913f;
            return (b0 + b1 + b2 + w * 0.1848f) * 0.18f;
        }
        crackle *= 0.97f;
        if (rng.uni() < 0.0015f) crackle = rng.bi();
        return crackle;
    }
};

// ---------------------------------------------------------------------------
// LFO
// ---------------------------------------------------------------------------
struct Lfo
{
    float phase = 0.0f, held = 0.0f, prevRand = 0.0f, nextRand = 0.0f;
    Rng rng;
    void reset (float startPhase) { phase = startPhase; }
    // advance by 'inc' cycles; returns -1..1
    float advance (int shape, float inc)
    {
        phase += inc;
        if (phase >= 1.0f)
        {
            phase -= std::floor (phase);
            held = rng.bi();
            prevRand = nextRand;
            nextRand = rng.bi();
        }
        return shapeAt (shape, phase);
    }
    float shapeAt (int shape, float p) const
    {
        switch (shape)
        {
            case 0: return std::sin (kTwoPi * p);
            case 1: return 1.0f - 4.0f * std::abs (p - 0.5f);
            case 2: return 2.0f * p - 1.0f;
            case 3: return 1.0f - 2.0f * p;
            case 4: return p < 0.5f ? 1.0f : -1.0f;
            case 5: return held;
            default:
            {
                const float t = 0.5f - 0.5f * std::cos (kPi * p);
                return prevRand + (nextRand - prevRand) * t;
            }
        }
    }
};
} // namespace pg
