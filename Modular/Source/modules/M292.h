#pragma once
// LG-292 Quad Lowpass Gate - emulation of the Model 292.
//
// Audio path (Parker & D'Angelo, "A Digital Model of the Buchla Lowpass-Gate", DAFx-13):
//
//   IN --[Rf (LDR)]--+--[Rf (LDR)]--+------------------------+--> OUT
//                    |              |                        |
//                   C1             C2 --[buffer]--+          Ra (5 M, or 5 k in VCA mode)
//                    |              |             |          |
//                   GND            +--[C3]--[x a]-+         GND     (C3 = 4.7 nF only in LOPASS)
//
//   C1 = 1 nF, C2 = 220 pF, C3 = 0 / 4.7 nF, Ra = 5 MOhm (BOTH, LOPASS) / 5 kOhm (VCA)
//   H(s) = 1 / (a1 + a2 s + a3 s^2)
//     a1 = 1 + 2 Rf / Ra
//     a2 = Rf (2 C1 + C2 - C3 (a - 1) + (C2 + C3) Rf / Ra)
//     a3 = Rf^2 C1 (C2 + C3)
//   resonance limit a_max = (2 C1 Ra + (C2 + C3)(Ra + Rf)) / (C3 Ra)
//
// Vactrol (VTL5C3/2):  Rf = A / If^1.4 + B,  A = 3.464, B = 1136.212 Ohm, If 10 uA .. 40 mA
//   LED current follows the control with ~12 ms rise and ~250 ms fall, faster when brighter.
//
// The 2nd-order section is solved as a topology-preserving (trapezoidal) state-variable filter with the
// same poles and DC gain, so it stays stable under audio-rate modulation of Rf.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M292 : public Module
{
public:
    // per channel n = 0..3: inputs SIGNAL (mini), CONTROL (banana); output OUT (mini)
    static int inSig (int n) { return n * 2; }
    static int inCtl (int n) { return n * 2 + 1; }
    static int outSig (int n) { return n; }
    static constexpr int outAll = 4;     // 4-to-1 mix of the four outputs

    M292 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;

    std::atomic<float> ledV[4] {};
    // LEDs per channel, as on the 292t: 2n = lopass, 2n+1 = gate (both dark in combo); brightness follows the vactrol
    float led (int id) const noexcept override
    {
        const int n = juce::jlimit (0, 3, id / 2);
        const int m = modeNow[n].load (std::memory_order_relaxed);
        const bool on = id % 2 == 0 ? m == 0 : m == 2;
        return on ? 0.3f + 0.7f * std::sqrt (ledV[n].load (std::memory_order_relaxed)) : 0.0f;
    }
    std::atomic<int> modeNow[4] {};

    // exposed for the tests
    static float ldrOhms (float ledAmps) { return 3.464f / std::pow (juce::jmax (1.0e-5f, ledAmps), 1.4f) + 1136.212f; }

private:
    struct Ch
    {
        float ledI = 1.0e-5f;     // vactrol state: LED current (A)
        float s1 = 0.0f, s2 = 0.0f;   // SVF integrator states
        dsp::Ramp off, cv, res;
        int mode = 1;
    };
    std::atomic<float>* offP[4] {};
    std::atomic<float>* cvP[4] {};
    std::atomic<float>* resP[4] {};
    std::atomic<float>* modeP[4] {};
    float fs = 48000.0f;
    int rampLen = 480;
    bool first = true;
    Ch ch[4];
};
} // namespace wc
