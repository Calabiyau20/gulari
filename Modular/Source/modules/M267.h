#pragma once
// US-267 Uncertainty Source / Dual Bandpass Filter (Calabi Yau 267e).
// Left: noise sources (low = -3 dB/oct, flat, high = +3 dB/oct) and two random voltage sources. Each random source has
// a probable rate of change (.05 - 50 Hz, plus c.v.), a "random" knob that blends an external control voltage (left)
// with the internal random voltage (right) at the "mix" output, and a sample & hold output clocked by the sample input
// (or, unpatched, by the random source's own changes).
// Right: two voltage controlled bandpass filters (31 Hz - 4 kHz, bandwidth 0 narrow .. 1 wide) with an audio-rate
// modulation input whose index and destination (amplitude / frequency / bandwidth) are set on the panel.
// Normalling: flat noise -> filter A signal input -> filter B signal input.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M267 : public Module
{
public:
    enum In { In_ProbA, In_SmplA, In_ExtA, In_ProbB, In_SmplB, In_ExtB,
              In_SigA, In_IdxA, In_FreqA, In_BwA, In_ModA,
              In_SigB, In_IdxB, In_FreqB, In_BwB, In_ModB, In_Count };
    enum Out { Out_Low, Out_Flat1, Out_Flat2, Out_High, Out_MixA, Out_ShA, Out_MixB, Out_ShB,
               Out_SigA1, Out_SigA2, Out_SigB1, Out_SigB2, Out_Count };
    // LEDs: 0 mix A, 1 s&h A, 2 mix B, 3 s&h B, 10..12 filter A mod type, 13..15 filter B, 20 remote
    float led (int id) const noexcept override;

    M267 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;
    static float rateHz (float k) noexcept { return 0.05f * std::pow (1000.0f, dsp::clampf (k, 0.0f, 1.0f)); }
    static float freqHz (float k) noexcept { return 31.0f * std::pow (2.0f, 7.0f * dsp::clampf (k, 0.0f, 1.0f)); }

private:
    struct Rnd { float target = 5.0f, value = 5.0f, sh = 5.0f; bool smplWas = false; float slew = 0.0f; };
    std::atomic<float>* rateP[2] {}; std::atomic<float>* randP[2] {}; std::atomic<float>* freqP[2] {}; std::atomic<float>* bwP[2] {};
    std::atomic<float>* idxP[2] {}; std::atomic<float>* modP[2] {}; std::atomic<float>* remP = nullptr;
    float fs = 48000.0f;
    float rate[2] {}, rnd[2] {}, freq[2] {}, bw[2] {}, idx[2] {};
    int mode[2] {};
    dsp::Noise white, chance;
    dsp::Pink pink;
    float blueZ = 0.0f;
    Rnd r[2];
    dsp::Svf filt[2];
    int ctr = 0;
    float fEff[2] { 500, 500 }, qEff[2] { 1, 1 };
    std::atomic<float> ledV[4] {};
};
} // namespace wc
