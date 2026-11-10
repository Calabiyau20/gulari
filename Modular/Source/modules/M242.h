#pragma once
// PP-242 Programmable Pulser - emulation of the Model 242t.
// Internal clock (.002 - 10 s per stage, or off) with c.v.; a 4 x 12 matrix: row n sets the number of
// stages (cycle length), rows A / B / C are pulse patterns sent to the MATRIX outputs.
// ALL OUT = every clock pulse, ALTER = alternating pulses and the ramp. CLK IN replaces the internal clock
// for the matrix only; RST IN returns to the first stage. Pulses are 1 ms ("long" jumper).
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M242 : public Module
{
public:
    enum In { In_Start, In_Stop, In_Period, In_Clk, In_Rst, In_Count };
    enum Out { Out_Alt1, Out_Alt2, Out_Ramp, Out_A, Out_B, Out_C, Out_All, Out_Count };
    enum Led { Led_Stop = 0, Led_Start = 1, Led_Matrix = 100 };   // matrix: 100 + row * 12 + column, rows n, A, B, C
    static constexpr int kSteps = 12;

    M242 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;
    float led (int id) const noexcept override;

    static float periodSec (float k) { return 0.002f * std::pow (5000.0f, k / 0.92f); }   // knob end (> 0.92) = off

    std::atomic<bool> btnStart { false }, btnStop { false }, btnSingle { false };

private:
    std::atomic<float>* cell[3][kSteps] {};
    std::atomic<float>* lenP = nullptr;
    std::atomic<float>* cvP = nullptr;
    std::atomic<float>* perP = nullptr;
    std::atomic<float>* rowP = nullptr;
    std::atomic<float>* colP = nullptr;
    float fs = 48000.0f;
    std::atomic<bool> cellOn[3][kSteps] {};
    std::atomic<int> len { 8 };
    float cvK = 0.0f, perK = 0.5f;
    bool running = true;
    double phase = 0.0;
    std::atomic<int> step { -1 };
    bool alt = false;
    int pAll = 0, pAlt[2] {}, pRow[3] {};
    bool startWas = false, stopWas = false, clkWas = false, rstWas = false, bStartWas = false, bStopWas = false, bSingleWas = false;
    std::atomic<bool> runningLed { true };
    std::atomic<float> tickFlash { 0.0f };
    void clockTick (bool advanceMatrix) noexcept;
    void advance() noexcept;
};
} // namespace wc
