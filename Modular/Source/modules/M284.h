#pragma once
// QE-284 Quad Voltage Controlled Envelope Generator (Calabi Yau 284).
// Four channels. A pulse at the red input starts: delay -> attack -> duration -> decay. Each time runs .002 - 10 s.
// With "sustain" on, the duration holds for as long as the pulse input stays high. The control input (black) scales
// all four times of its channel through the control knob (an attenuverter: right shortens, left lengthens).
// Outputs: the envelope on two bridged jacks, and a pulse when the envelope has finished (for cascades).
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M284 : public Module
{
public:
    static constexpr int kCh = 4;
    static constexpr int inCtl (int k) { return 2 * k; }
    static constexpr int inTrig (int k) { return 2 * k + 1; }
    static constexpr int outEnv (int k, int j) { return 3 * k + j; }
    static constexpr int outPulse (int k) { return 3 * k + 2; }
    static constexpr int kNumIn = 2 * kCh, kNumOut = 3 * kCh;

    M284 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;
    float led (int id) const noexcept override;   // 0..3: envelope level of each channel
    static float knobTime (float k) noexcept { return 0.002f * std::pow (5000.0f, dsp::clampf (k, 0.0f, 1.0f)); }

    std::atomic<bool> manual[kCh] {};

private:
    enum Stage { Idle, Delay, Attack, Hold, Decay };
    struct Ch { Stage st = Idle; float level = 0.0f, t = 0.0f, from = 0.0f; bool was = false, bWas = false; int pulse = 0; };
    std::atomic<float>* p[5][kCh] {};
    std::atomic<float>* susRaw[kCh] {};
    float fs = 48000.0f;
    float knob[5][kCh] {};
    bool sus[kCh] {};
    Ch ch[kCh];
    std::atomic<float> ledLevel[kCh] {};
};
} // namespace wc
