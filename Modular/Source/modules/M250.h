#pragma once
// AF-250 Dual Arbitrary Function Generator - emulation of the Buchla 250e.
// Sixteen stages on a ring: an outer voltage knob (two voltage sets: set 1 -> cv out 1, set 2 -> cv out 2) and an inner
// time knob per stage. Each stage keeps its own program, edited from the centre: select the stage with the centre knob,
// then quant / interp (time and voltage), time range (.01-.1, .08-.8, .6-6 s), voltage range (0-2 .. 8-10, all),
// pulse 1 / 2, mode (advance, sustain, enable, stop) and external sources (time, voltage from inputs A-D; fix = sample
// the input once when the stage starts). Jump & loop: at the end of the loop stage the sequence jumps back N times.
// Time multiplier (.25 - 4 + c.v.), start / stop (buttons + pulse inputs), stage addressing (strobed / continuous with
// offset + c.v. in). cv out (left) is the time setting of each stage as a voltage.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M250 : public Module
{
public:
    static constexpr int kStages = 16;
    enum In { In_ExtA, In_ExtB, In_ExtC, In_ExtD, In_MultCv, In_Start, In_Stop, In_AddrPulse, In_AddrCv, In_Count };
    enum Out { Out_TimeCv, Out_Cv1, Out_Cv2, Out_All, Out_P1, Out_P2, Out_Count };
    // LEDs: 0..15 stage, 20..35 pulse programmed, 40 run, 41 + k: edit-stage program (see led())
    float led (int id) const noexcept override;

    M250 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;

    std::atomic<bool> btnStart { false }, btnStop { false };
    std::atomic<int> loopsDone { 0 };
    static void timeRange (int r, float& lo, float& hi) { static const float t[3][2] = { { .01f, .1f }, { .08f, .8f }, { .6f, 6.0f } }; r = juce::jlimit (0, 2, r); lo = t[r][0]; hi = t[r][1]; }
    std::atomic<double> hostBpm { 120.0 };   // set by the processor every block (time range 4 = sync to the host tempo)
    static constexpr int kModePulse = 4;     // stage modes: 0 adv, 1 sust, 2 enable, 3 stop, 4 pulse

private:
    std::atomic<float>* vP[2][kStages] {}; std::atomic<float>* tP[kStages] {};
    enum F { F_P1, F_P2, F_TQ, F_TI, F_VQ, F_VI, F_TRng, F_VRng, F_Mode, F_TExt, F_VExt, F_Fix, F_Count };
    std::atomic<float>* fP[F_Count][kStages] {};
    std::atomic<float>* multP = nullptr; std::atomic<float>* mcvP = nullptr; std::atomic<float>* addrP = nullptr; std::atomic<float>* offP = nullptr;
    std::atomic<float>* lEndP = nullptr; std::atomic<float>* lToP = nullptr; std::atomic<float>* lCntP = nullptr; std::atomic<float>* editP = nullptr; std::atomic<float>* setP = nullptr; std::atomic<float>* remP = nullptr; std::atomic<float>* editModeP = nullptr;
    float fs = 48000.0f;
    float v[2][kStages] {}, t[kStages] {};
    int f[F_Count][kStages] {};
    float mult = 1.0f, mcv = 0.0f, off = 0.0f;
    int addrMode = 0, loopEnd = 0, loopTo = 1, loopCount = 0;
    int stage = 0; double elapsed = 0.0; float dur = 1.0f;
    float out1 = 0, out2 = 0, from1 = 0, from2 = 0, timeOut = 0, timeFrom = 0, fixV[2] { 0, 0 };
    bool running = false, startWas = false, stopWas = false, addrWas = false, bStartWas = false, bStopWas = false;
    int loopsLeft = -1, pAll = 0, p1 = 0, p2 = 0, lastAddr = -1;
    std::atomic<int> curStage { 0 }; std::atomic<bool> runLed { false };
    float voltage (int set, int s, const float* in) const noexcept;
    void enter (int s, const float* in) noexcept;
    void next (const float* in) noexcept;
};
} // namespace wc
