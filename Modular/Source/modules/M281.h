#pragma once
// QF-281 Quad Function Generator - emulation of the Model 281.
// Four attack / decay generators (1 ms .. 10 s each), modes Transient / Sustained / Cycle,
// quadrature per pair (A triggers B, C triggers D, at the end of the attack), end-of-decay pulses.
// Outputs 0..+10 V. See Modular/docs/281-292-reverse-engineering.json.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M281 : public Module
{
public:
    // per channel n = 0..3: inputs TRIG, CYCLE, ATT CV, DEC CV; outputs FUNC, EOD
    static int inTrig (int n) { return n * 4; }
    static int inCycle (int n) { return n * 4 + 1; }
    static int inAtt (int n) { return n * 4 + 2; }
    static int inDec (int n) { return n * 4 + 3; }
    static int outFunc (int n) { return n * 2; }
    static int outEod (int n) { return n * 2 + 1; }
    // quadrature section: per pair p (A-B, C-D) a crossfade and its complement
    static int outMix (int pair, bool complement) { return 8 + pair * 2 + (complement ? 1 : 0); }
    // 0.2.0 indices (TRIG, ATT, DEC per channel) -> current
    static int inFromV2 (int old) { const int n = old / 3, k = old % 3; return n * 4 + (k == 0 ? 0 : k + 1); }

    M281 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;

    static float timeSeconds (float norm) { return 0.001f * std::pow (10.0f, 4.0f * norm); }   // 1 ms .. 10 s

    std::atomic<bool> manual[4] {};          // panel trigger buttons
    std::atomic<float> level[4] {};          // for the LEDs (0..1)

private:
    enum Stage { Idle, Attack, Hold, Decay };
    struct Ch
    {
        Stage stage = Idle;
        float v = 0.0f;           // 0..1
        bool gateWas = false;
        int eodLeft = 0;
        bool pendingTrig = false; // quadrature trigger from the partner
        bool held = false;        // gate currently high (sustained mode)
    };
    std::atomic<float>* att[4] {};
    std::atomic<float>* dec[4] {};
    std::atomic<float>* mode[4] {};
    std::atomic<float>* quad[2] {};
    std::atomic<float>* mix[2] {};
    float mixV[2] {};
    float fs = 48000.0f;
    float attK[4] {}, decK[4] {};
    int modeV[4] {};
    bool quadV[2] {};
    Ch ch[4];
    bool manualWas[4] {};
};
} // namespace wc
