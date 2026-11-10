#pragma once
// DO-258 Dual Oscillator - emulation of the Model 258 / 258t: two independent oscillators, 5 Hz - 20 kHz on one knob,
// pitch (kbd) input, a processing input with its attenuverter (scale / invert / expand), f.m. input with depth,
// waveshape knob + c.v.: oscillator 1 morphs sine -> sawtooth, oscillator 2 sine -> square (band-limited, polyBLEP).
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M258 : public Module
{
public:
    enum In { In_Kbd1, In_Proc1, In_Fm1, In_Shape1, In_Kbd2, In_Proc2, In_Fm2, In_Shape2, In_Count };
    enum Out { Out_1, Out_2, Out_Count };
    static int inOf (int osc, int k) { return osc * 4 + k; }    // k: 0 kbd, 1 processing, 2 f.m., 3 waveshape c.v.

    M258 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override { ph[0] = 0.0; ph[1] = 0.5; }

    static float knobHz (float k) { return 5.0f * std::pow (4000.0f, k); }   // 5 Hz .. 20 kHz

private:
    std::atomic<float>* freqP[2] {}; std::atomic<float>* procP[2] {}; std::atomic<float>* fmP[2] {}; std::atomic<float>* shapeP[2] {};
    std::atomic<float>* voltP = nullptr;
    float fs = 48000.0f;
    float logHz[2] {}, proc[2] {}, fm[2] {}, shape[2] {};
    float pitchDiv = 1.2f;
    double ph[2] { 0.0, 0.5 };
    dsp::DcBlock dc[2];
};
} // namespace wc
