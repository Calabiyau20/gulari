#pragma once
// SD-277 Signal Delay Unit - emulation of the Model 277 / 277t.
// Input mixer (A, B, C with level knobs, overload LED) into a delay line read at 1/4 t, 1/2 t, 3/4 t and t.
// Delay time t: .05 - .20 s ("original") or .20 - .80 s ("long"), modulated by the control input (attenuverter,
// +-2 octaves over 10 V) and by the modulation input (audio, depth knob). Time changes are slewed and read with
// cubic interpolation, so sweeping t bends the pitch like the bucket-brigade original; each tap has the BBD lowpass.
#include "../rack/Module.h"
#include "Dsp.h"
#include <vector>

namespace wc
{
class M277 : public Module
{
public:
    enum In { In_A, In_B, In_C, In_Control, In_Mod, In_Count };
    enum Out { Out_Q1, Out_Q2, Out_Q3, Out_T, Out_Count };
    enum Led { Led_Overload, Led_Count };

    M277 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;
    float led (int id) const noexcept override { return id == Led_Overload ? ovl.load (std::memory_order_relaxed) : 0.0f; }

    static float timeSec (float k, bool longMode) { return (0.05f + 0.15f * k) * (longMode ? 4.0f : 1.0f); }

private:
    std::atomic<float>* lvlP[3] {}; std::atomic<float>* timeP = nullptr; std::atomic<float>* ctlP = nullptr; std::atomic<float>* modP = nullptr; std::atomic<float>* longP = nullptr;
    float fs = 48000.0f, lvl[3] {}, tBase = 0.1f, ctl = 0.0f, modK = 0.0f;
    bool longMode = false;
    std::vector<float> buf;
    int w = 0;
    float tSmooth = 0.1f, slew = 0.001f, ovlHold = 0.0f;
    dsp::OnePoleLP lp[4];
    std::atomic<float> ovl { 0.0f };
    float read (float delaySamples) const noexcept;
};
} // namespace wc
