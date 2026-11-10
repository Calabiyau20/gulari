#pragma once
// SU-266 Source of Uncertainty - emulation of the Model 266.
// Noise (pink / white / blue), fluctuating random voltages, sample & hold with alternating outputs,
// integrator (smoothing), quantized random voltages (n+1 or 2^n states), stored random voltages
// (uniform + adjustable distribution). See Modular/docs/266-reverse-engineering.json.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M266 : public Module
{
public:
    enum In { In_FluctCv, In_ShPulse, In_ShCv, In_IntIn, In_IntCv, In_QPulse, In_QCv, In_SPulse, In_SCv, In_Count };
    enum Out { Out_Pink, Out_White, Out_Blue, Out_Fluct, Out_Sh, Out_ShAltA, Out_ShAltB, Out_ShPulseA, Out_ShPulseB,
               Out_Int, Out_Quant, Out_StoredUni, Out_StoredDist, Out_Count };

    M266 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;

    static float fluctHz (float k) { return 0.05f * std::pow (1000.0f, k); }     // 0.05 .. 50 Hz
    static float smoothSec (float k) { return 0.01f * std::pow (1000.0f, k); }   // 0.01 .. 10 s

    std::atomic<float> fluctLed { 0.0f };

private:
    std::atomic<float>* rateP = nullptr;
    std::atomic<float>* smoothP = nullptr;
    std::atomic<float>* stepsP = nullptr;
    std::atomic<float>* qmodeP = nullptr;
    std::atomic<float>* distP = nullptr;
    float fs = 48000.0f;
    float rateK = 0.5f, smoothK = 0.3f, stepsK = 3.0f, distK = 0.5f;
    bool pow2 = false;
    juce::Random rnd;

    // noise
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;   // pink (Kellet)
    float lastWhite = 0.0f;
    // fluctuating
    float fTarget = 5.0f, fValue = 5.0f, fSlew = 5.0f;
    // sample & hold
    bool shWas = false, shToggle = false;
    float sh = 0.0f, shA = 0.0f, shB = 0.0f;
    int pulseA = 0, pulseB = 0;
    // integrator
    float integ = 0.0f;
    // quantized / stored
    bool qWas = false, sWas = false;
    float quant = 0.0f, storedU = 0.0f, storedD = 0.0f;

    float uni() noexcept { return rnd.nextFloat(); }
    float shaped (float d) noexcept;
};
} // namespace wc
