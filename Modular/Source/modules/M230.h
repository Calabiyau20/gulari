#pragma once
// EF-230 Triple Envelope Follower - emulation of the Model 230 / 230t.
// Three channels: audio in, sensitivity, decay time; a smooth 0 - 10 V envelope (c.v. out) and a pulse generator
// that fires when the envelope crosses the threshold: TRANSIENT = a short pulse per onset, SUSTAINED = a gate while
// the signal stays above the threshold.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M230 : public Module
{
public:
    enum In { In_Audio1, In_Audio2, In_Audio3, In_Count };
    enum Out { Out_Env1, Out_Env2, Out_Env3, Out_Pulse1, Out_Pulse2, Out_Pulse3, Out_Count };
    enum Led { Led_Env1, Led_Env2, Led_Env3, Led_Pulse1, Led_Pulse2, Led_Pulse3, Led_Count };

    M230 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;
    float led (int id) const noexcept override { return id >= 0 && id < Led_Count ? leds[(size_t) id].load (std::memory_order_relaxed) : 0.0f; }

    static float gainOf (float k) { return std::pow (10.0f, (-20.0f + 50.0f * k) / 20.0f); }   // -20 .. +30 dB
    static float decaySec (float k) { return 0.01f * std::pow (300.0f, k); }                   // 10 ms .. 3 s

private:
    std::atomic<float>* sensP[3] {}; std::atomic<float>* decP[3] {}; std::atomic<float>* thrP[3] {}; std::atomic<float>* modeP[3] {};
    float fs = 48000.0f;
    float gain[3] {}, att = 0.0f, rel[3] {}, thr[3] {};
    bool sustained[3] {};
    float env[3] {};
    bool above[3] {};
    int pulseLeft[3] {};
    std::array<std::atomic<float>, Led_Count> leds {};
};
} // namespace wc
