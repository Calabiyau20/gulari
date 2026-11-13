#pragma once
// VP-257 Dual Voltage Processor - emulation of the Model 257 / 257t.
// Per section: Vout = Va * K + Vb' * (1 - M') + Vc * M' + Voffset, where Vb' = Vb (+ 5 V with the +.5 switch)
// and M' = clamp(M + Vm / 10, 0, 1). Attenuverter + crossfader + offset (0 - 10 V).
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M257 : public Module
{
public:
    enum In { In_Va1, In_Vb1, In_Vm1, In_Vc1, In_Va2, In_Vb2, In_Vm2, In_Vc2, In_Count };
    enum Out { Out_1, Out_2, Out_Count };
    enum Led { Led_1, Led_2, Led_Count };
    static int inOf (int section, int k) { return section * 4 + k; }   // k: 0 a, 1 b, 2 m, 3 c

    M257 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override { rampLen = juce::jmax (1, (int) (0.012 * sampleRate)); first = true; }
    void reset() override { first = true; }
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    float led (int id) const noexcept override { return id >= 0 && id < Led_Count ? leds[(size_t) id].load (std::memory_order_relaxed) : 0.0f; }

    static float process (float va, float vb, float vm, float vc, float k, float m, bool plus5, float off) noexcept
    {
        const float mm = dsp::clampf (m + vm / 10.0f, 0.0f, 1.0f);
        return va * k + (vb + (plus5 ? 5.0f : 0.0f)) * (1.0f - mm) + vc * mm + off;
    }

private:
    std::atomic<float>* kP[2] {}; std::atomic<float>* mP[2] {}; std::atomic<float>* p5P[2] {}; std::atomic<float>* offP[2] {};
    dsp::Ramp k[2], off[2];
    float m[2] {};
    int rampLen = 576;
    bool first = true;
    bool p5[2] {};
    std::array<std::atomic<float>, Led_Count> leds {};
};
} // namespace wc
