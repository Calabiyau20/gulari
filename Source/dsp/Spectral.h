#pragma once
#include "Common.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <vector>

// Spectral oscillator: a bank of up to 64 sine partials whose amplitudes come from a
// "spectral table" (frames x partials). Tables are built in (20 sounds) or analysed from
// an audio file (pitch detection + one harmonic spectrum per frame).
namespace pg
{
struct SpectralTable
{
    static constexpr int kPartials = 64;
    juce::String name;
    int numFrames = 0;
    float baseHz = 0.0f;                 // detected pitch for analysed files (0 = built in)
    std::vector<float> amp;              // numFrames * kPartials, linear amplitude
    const float* frame (int f) const { return amp.data() + (size_t) f * kPartials; }
};
using SpectralPtr = std::shared_ptr<const SpectralTable>;

const std::vector<SpectralPtr>& builtInSpectra();
SpectralPtr analyseSpectralFile (const juce::File& file, juce::String& error);

class SpectralOsc
{
public:
    static constexpr int kN = SpectralTable::kPartials;
    struct Params
    {
        float pos = 0, scan = 0, tilt = 0, stretch = 0, shift = 0, oddEven = 0, blur = 0, shimmer = 0, width = 0.4f;
        int partials = 48;
        bool freeze = false;
        int warp = 0;
        float warpAmt = 0.0f, partialDecay = 0.0f, jitter = 0.0f;
    };
    void prepare (double sampleRate, uint32_t seed);
    void noteOn (float startPos);
    // once per control block: new targets for every partial
    void update (const SpectralTable& t, const Params& p, float freqHz, int blockLen);
    inline void tick (float& outL, float& outR)
    {
        float l = 0.0f, r = 0.0f;
        for (int k = 0; k < active; ++k)
        {
            const float s = sn[k];
            l += s * aL[k];
            r += s * aR[k];
            aL[k] += dL[k];
            aR[k] += dR[k];
            const float c = cs[k];
            cs[k] = c * rc[k] - s * rs[k];
            sn[k] = c * rs[k] + s * rc[k];
        }
        outL = l;
        outR = r;
    }
    // phase-modulated version: the analytic (quadrature) sum lets the whole spectrum be phase shifted
    inline void tickPM (float pm, float& outL, float& outR)
    {
        float l = 0.0f, r = 0.0f, ql = 0.0f, qr = 0.0f;
        for (int k = 0; k < active; ++k)
        {
            const float s = sn[k], c = cs[k];
            l += s * aL[k]; r += s * aR[k];
            ql += c * aL[k]; qr += c * aR[k];
            aL[k] += dL[k]; aR[k] += dR[k];
            cs[k] = c * rc[k] - s * rs[k];
            sn[k] = c * rs[k] + s * rc[k];
        }
        const float cp = std::cos (kTwoPi * pm), sp = std::sin (kTwoPi * pm);
        outL = l * cp + ql * sp;
        outR = r * cp + qr * sp;
    }
    float position() const { return playPos; }

private:
    double sr = 48000.0;
    float cs[kN] {}, sn[kN] {}, rc[kN] {}, rs[kN] {};
    float aL[kN] {}, aR[kN] {}, dL[kN] {}, dR[kN] {};
    float smoothAmp[kN] {};
    float flicker[kN] {}, flickerTarget[kN] {};
    float playPos = 0.0f, noteTime = 0.0f;
    float jit[kN] {};
    int active = 0;
    Rng rng;
};
} // namespace pg
