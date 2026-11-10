#pragma once
// MX-207 Mixer / Preamplifier - emulation of the Model 207 / 207t.
// Six inputs A-F with level faders (0 - 10), PROG switches (the channel level follows the program c.v. in, 0 - 10 V),
// MON switches (the channel goes to the monitor bus) and channel assignment: A is left, F is right, B - E are panned by the
// four assignment knobs. Expansion inputs L / R join the mix. Signal outputs L - R and monitor (two jacks).
// Preamp: input -> gain (55 / 75 dB range switch, soft-limited) -> output. The right c.v. input sets the monitor level.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M207 : public Module
{
public:
    enum In { In_ProgCv, In_MonCv, In_A, In_B, In_C, In_D, In_E, In_F, In_ExpL, In_ExpR, In_Pre, In_Count };
    enum Out { Out_L, Out_R, Out_Mon1, Out_Mon2, Out_Pre, Out_Count };
    enum Led { Led_ProgA, Led_ProgB, Led_MonA, Led_MonB, Led_Count };

    M207 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    float led (int id) const noexcept override { return id >= 0 && id < Led_Count ? leds[(size_t) id].load (std::memory_order_relaxed) : 0.0f; }

private:
    std::atomic<float>* lvlP[6] {}; std::atomic<float>* progP[6] {}; std::atomic<float>* monP[6] {}; std::atomic<float>* panP[4] {};
    std::atomic<float>* gainP = nullptr; std::atomic<float>* rangeP = nullptr;
    float fs = 48000.0f;
    float gl[6] {}, gr[6] {};
    bool prog[6] {}, mon[6] {};
    float preGain = 1.0f;
    dsp::OnePoleLP progLag, monLag;
    float peakL = 0, peakR = 0;
    std::array<std::atomic<float>, Led_Count> leds {};
};
} // namespace wc
