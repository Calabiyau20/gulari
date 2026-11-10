#pragma once
// MF-248 Multiple Arbitrary Function Generator (MARF) - emulation of the Model 248 / 248t.
// Sixteen stages, each with an output voltage level and an interval time slider and its own program: output pulses 1 / 2,
// quantize, sloped / stepped, range (full 0-10 V, half 0-5 V, limited 2 V spans at +0 +2 +4 +6 +8), voltage source
// (internal slider, or external input A-D chosen by the slider), operating mode (stop, sustain, enable, cycle first / last),
// time range (.002-.03, .02-.3, .2-3, 2-30 s) and time source (internal slider or external input).
// Two function generators run through the same stages, each with its own outputs (voltage, time, ref ramp, ART = quantized
// pitch, pulses all / 1 / 2), time multiplier (.5 - 4, + c.v.), mode (advance, start, stop, run / hold / stop LEDs) and
// stage address (reset, display, continuous / strobe, internal knob or external c.v.).
// Programming works as on the hardware: the stage-number switch picks a stage, the programming switches set (up) or
// clear (down) its settings and their LEDs show them. Pitch is 1.2 V/oct, as in the rest of the rack.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M248 : public Module
{
public:
    static constexpr int kStages = 16;
    enum In { In_ExtA, In_ExtB, In_ExtC, In_ExtD,
              In_MultCv1, In_Stop1, In_Start1, In_Strobe1, In_Addr1,
              In_MultCv2, In_Stop2, In_Start2, In_Strobe2, In_Addr2, In_Count };
    enum Out { Out_Art1, Out_Ref1, Out_Time1, Out_Volt1, Out_All1, Out_P1_1, Out_P2_1,
               Out_Art2, Out_Ref2, Out_Time2, Out_Volt2, Out_All2, Out_P1_2, Out_P2_2, Out_Count };
    static int inJ (int fg, int k) { return 4 + fg * 5 + k; }      // k: 0 mult c.v., 1 stop, 2 start, 3 strobe, 4 address c.v.
    static int outJ (int fg, int k) { return fg * 7 + k; }         // k: 0 art, 1 ref, 2 time, 3 voltage, 4 all, 5 pulse 1, 6 pulse 2
    // programmable settings per stage ("mf_<name>_<stage>")
    enum Fn { F_P1, F_P2, F_Quant, F_Slope, F_Range, F_VExt, F_Stop, F_Sust, F_Enable, F_First, F_Last, F_TRange, F_TExt, F_Count };
    static const char* fnName (int f);
    // LEDs: 0..15 stage LEDs; 100 + fg*10 + k status (k 0 run, 1 hold, 2 stop, 3 pulse 1, 4 pulse 2, 5 display, 6 reset);
    // 200 + fn*8 + value: programming LED of the edit stage; 300 + i: number buttons, 320 load, 321 save, 322 scale, 323 key
    float led (int id) const noexcept override;

    M248 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;

    std::atomic<bool> btnAdvance[2] {}, btnStart[2] {}, btnReset[2] {};
    std::atomic<int> displayFg { 0 }, armed { 0 };     // armed: 0 none, 1 load, 2 save, 3 scale, 4 key

    static void range (int r, float& lo, float& hi) { static const float t[4][2] = { { .002f, .03f }, { .02f, .3f }, { .2f, 3.0f }, { 2.0f, 30.0f } }; lo = t[r][0]; hi = t[r][1]; }

private:
    juce::AudioProcessorValueTreeState& state;
    juce::String pre;
    std::atomic<float>* lvlP[kStages] {}; std::atomic<float>* timP[kStages] {}; std::atomic<float>* fnP[F_Count][kStages] {};
    std::atomic<float>* multP[2] {}; std::atomic<float>* mcvP[2] {}; std::atomic<float>* addrP[2] {}; std::atomic<float>* contP[2] {};
    std::atomic<float>* extP[2] {}; std::atomic<float>* artP[2] {};
    std::atomic<float>* editP = nullptr; std::atomic<float>* keyP = nullptr; std::atomic<float>* scaleP = nullptr;
    float fs = 48000.0f;
    float lvl[kStages] {}, tim[kStages] {};
    int fn[F_Count][kStages] {};
    float mult[2] {}, mcv[2] {}, addr[2] {};
    bool cont[2] {}, extAddr[2] {};
    int artSrc[2] {}, key = 0, scale = 1;
    struct Fg
    {
        int stage = 0; double elapsed = 0.0; float dur = 1.0f, startV = 0.0f, v = 0.0f, art = 0.0f;
        int status = 2;            // 0 run, 1 hold, 2 stop
        bool atEnd = true, haltedOnStop = false;
        int allLeft = 0; bool startWas = false, stopWas = false, strobeWas = false, advWas = false, resetWas = false, btnStartWas = false;
        int lastAddr = -1;
    } g[2];
    std::atomic<int> curStage[2] {};
    std::atomic<int> statusLed[2] {};
    std::atomic<bool> p1Led[2] {}, p2Led[2] {};
    int firstStage() const noexcept;
    float target (int s, const float* in) const noexcept;
    float duration (int fgI, int s, const float* in) const noexcept;
    void enter (int fgI, int s, const float* in) noexcept;
    void advance (int fgI, const float* in) noexcept;
};
} // namespace wc
