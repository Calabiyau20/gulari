#pragma once
// SV-245 Sequential Voltage Source - emulation of the Model 245 (layout of the 245t).
// Five stages x four stored voltages (rows A-D), stage gates, advance / stage select / analog addressing,
// four stage switches (off = the sequence returns to stage 1 after that stage), and the PULSER:
// ref (falling ramp), pulse (10 V spike + 5 V gate, length in % of the period), internal time .002 - 10 s.
// The pulser is normalled to ADVANCE when nothing is patched there.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M245 : public Module
{
public:
    enum In { In_Advance, In_Sel1, In_Sel2, In_Sel3, In_Sel4, In_Sel5, In_Analog, In_Stop, In_Start, In_LenCv, In_TimeCv, In_Count };
    enum Out { Out_Gate1, Out_Gate2, Out_Gate3, Out_Gate4, Out_Gate5, Out_A, Out_B, Out_C, Out_D, Out_Ref, Out_Pulse, Out_Count };
    enum Led { Led_Stage1, Led_Stage2, Led_Stage3, Led_Stage4, Led_Stage5, Led_Stop, Led_Start, Led_Count };

    M245 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;
    float led (int id) const noexcept override { return id >= 0 && id < Led_Count ? leds[(size_t) id].load (std::memory_order_relaxed) : 0.0f; }

    static float periodSec (float k) { return 0.002f * std::pow (5000.0f, k); }   // .002 .. 10 s

    std::atomic<bool> btnStart { false }, btnStop { false };   // panel buttons

private:
    std::atomic<float>* knob[4][5] {};
    std::atomic<float>* sw[4] {};
    std::atomic<float>* lenP = nullptr;
    std::atomic<float>* tcvP = nullptr;
    std::atomic<float>* timeP = nullptr;
    std::array<std::atomic<float>, Led_Count> leds {};
    float fs = 48000.0f;
    float vals[4][5] {};
    bool swOn[4] { true, true, true, true };
    float lenCv = 0.0f;
    float lenK = 0.5f, tcvK = 0.0f, timeK = 0.5f;
    int stage = 0;
    bool advWas = false, selWas[5] {}, stopWas = false, startWas = false, btnStartWas = false, btnStopWas = false;
    bool running = true;
    double phase = 0.0;
    int spikeLeft = 0, gateLeft = 0;
    float startFlash = 0.0f;
    bool pulsedThisSample = false;
    void firePulse (float period) noexcept;
};
} // namespace wc
