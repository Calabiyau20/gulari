#pragma once
// MX-207 Mixer / Preamplifier - emulation of the Model 207 / 207t.
// Six inputs A-F with level faders (0 - 10). Blue switches: the channel goes to the main SIGNAL outputs; yellow switches:
// the channel goes to the mono MONITOR outputs (pre-fader: the sliders do not affect the monitor). Channel assignment:
// B - E are panned by the four ASSIGNMENT knobs; A (left) and F (right) are voltage-controlled: with no c.v. A is left and
// F is right, more c.v. pans them towards the other side; the two LEDs under each c.v. input show where the signal goes.
// EXPANSION IN L / R feed the outputs directly. Preamp: input -> gain (55 / 70 dB range switch, soft-limited) -> output.
// (After the Buchla & Tiptop 207t manual.)
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M207 : public Module
{
public:
    enum In { In_AssignA, In_AssignF, In_A, In_B, In_C, In_D, In_E, In_F, In_ExpL, In_ExpR, In_Pre, In_Count };
    enum Out { Out_L, Out_R, Out_Mon1, Out_Mon2, Out_Pre, Out_Count };
    enum Led { Led_AL, Led_AR, Led_FL, Led_FR, Led_Count };

    M207 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override { first = true; }
    float led (int id) const noexcept override { return id >= 0 && id < Led_Count ? leds[(size_t) id].load (std::memory_order_relaxed) : 0.0f; }

private:
    std::atomic<float>* lvlP[6] {}; std::atomic<float>* progP[6] {}; std::atomic<float>* monP[6] {}; std::atomic<float>* panP[4] {};
    std::atomic<float>* gainP = nullptr; std::atomic<float>* rangeP = nullptr;
    float fs = 48000.0f;
    float gl[6] {}, gr[6] {};
    bool prog[6] {}, mon[6] {};
    dsp::Ramp gR[6], lR[6], rR[6], preGain;   // gain of A and F, left / right of B-E, pre-amp
    int rampLen = 576;
    bool first = true;
    dsp::OnePoleLP lagA, lagF;
    std::array<std::atomic<float>, Led_Count> leds {};
};
} // namespace wc
