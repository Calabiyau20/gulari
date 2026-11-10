#pragma once
// RG-252 Polyphonic Rhythm Generator - emulation of the Buchla 252e.
// Eleven concentric rings of 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 and 16 cells. Each cell holds red / green / blue pulses, a
// subdivision (0-7 extra pulses, spacing 0 .. 1/3 .. 2/3 .. 1), a behind / ahead offset and up to six c.v. values.
// Three clocks (master + 2) are assigned to rings; sync "on 1" (all rings meet on cell 1 of the master cycle), "on cycl"
// (every cell at the master rate) or free (each clock its own rate, x0.5 - x3 with delta-t). Outputs: red, green, blue,
// sub-div and cell 1 pulses, c.v. 1-6 (a cell leaves an output unchanged when its value is "--").
// The cell data is edited from the panel (cursor, colour buttons, encoders, copy / paste / clear, Euclidean patterns)
// and saved with the plugin state.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M252 : public Module
{
public:
    static constexpr int kRings = 11, kCells = 91;
    static constexpr int kSize[kRings] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 16 };
    static int base (int ring) { int b = 0; for (int r = 0; r < ring; ++r) b += kSize[r]; return b; }
    enum In { In_Clk1Cv, In_Clk1Pulse, In_Clk2Cv, In_Clk2Pulse, In_Clk3Cv, In_Clk3Pulse, In_Count };
    enum Out { Out_R, Out_G, Out_B, Out_Sub, Out_Cell1, Out_Cv1, Out_Cv2, Out_Cv3, Out_Cv4, Out_Cv5, Out_Cv6, Out_Count };

    M252 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;
    float led (int id) const noexcept override;

    // ---- cell data (written by the UI, read by the audio thread) ----
    std::atomic<int> rgb[kCells] {}, sub[kCells] {};
    std::atomic<float> subCurve[kCells] {}, offset[kCells] {}, cv[kCells][6] {};   // cv < 0 = "--"
    void clearAll();
    void clearCell (int c, int layer);         // layer: 0 pulses, 1 cv 1-3, 2 cv 4-6 + subdiv, 3 all
    juce::ValueTree toTree() const;
    void fromTree (const juce::ValueTree& t);
    // UI <-> audio
    std::atomic<bool> btnReset { false };
    std::atomic<int> playCell[3] { -1, -1, -1 }, playRing[3] { -1, -1, -1 };
    std::atomic<float> ledColour[5] {};        // r g b sub cell1 flashes

private:
    std::atomic<float>* bpmP = nullptr; std::atomic<float>* pdurP = nullptr; std::atomic<float>* syncP = nullptr; std::atomic<float>* playP = nullptr;
    std::atomic<float>* clkP[3] {}; std::atomic<float>* dtP[3] {}; std::atomic<float>* dtsP[3] {};
    std::atomic<float>* layerP = nullptr; std::atomic<float>* euclidP = nullptr; std::atomic<float>* editP = nullptr; std::atomic<float>* remP = nullptr;
    float fs = 48000.0f, bpm = 120.0f, pdur = 0.1f;
    int sync = 1; bool play = true;
    int clk[3] { 11, 0, 0 }; bool dt[3] {}; float dts[3] { 1, 1, 1 };
    struct Clock { double pos = 0.0; int lastFired = -100000, ring = -1; bool pulseWas = false; double subNext = 2.0; int subCell = -1, subLeft = 0; };
    Clock c[3];
    int pR = 0, pG = 0, pB = 0, pSub = 0, pCell1 = 0;
    float cvOut[6] {};
    std::atomic<float> cvLed[6] {};
    bool resetWas = false;
    void fire (int k, int ring, int cell, float cellSec) noexcept;
};
} // namespace wc
