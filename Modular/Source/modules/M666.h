#pragma once
// SC-666 Oscilloscope - a Calabi Yau original, up to four in the rack.
// Two audio inputs (A, B) drawn as two traces against a triggered time base (10 divisions, 100 us - 1 s per division),
// or against each other in X-Y mode (Lissajous). Trigger: internal (A crossing the level upwards), external (a pulse at
// EXT TRIG) or free running; modes AUTO / NORMAL / SINGLE / X-Y. Coupling AC / DC / GND. Control inputs: vertical and
// horizontal position, an external sweep (replaces the time base, 0-10 V = left to right), Z-axis (beam intensity) and
// time c.v. Outputs: the sweep ramp, a gate during the sweep, a trigger pulse, and A / B passed through.
// The audio thread writes complete frames; the panel draws the newest one with phosphor persistence.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M666 : public Module
{
public:
    static constexpr int kPoints = 640;
    enum In { In_A, In_B, In_ExtTrig, In_VertPos, In_HorizPos, In_SweepIn, In_ZAxis, In_TimeCv, In_Count };
    enum Out { Out_Sweep, Out_Gate, Out_Trig, Out_AThru, Out_BThru, Out_Count };
    // LEDs: 0 triggered / sweeping, 1 power
    float led (int id) const noexcept override;

    struct Frame
    {
        float a[kPoints] {}, b[kPoints] {}, x[kPoints] {}, z[kPoints] {};
        int n = 0;
        bool xy = false, dual = true;
    };

    M666 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;
    static float timePerDiv (float k) noexcept { return 0.0001f * std::pow (10000.0f, dsp::clampf (k, 0.0f, 1.0f)); }
    static float voltsPerDiv (float k) noexcept { return 0.02f * std::pow (250.0f, dsp::clampf (k, 0.0f, 1.0f)); }

    // UI side: copies the newest complete frame; returns false when nothing new since 'seq'
    bool latest (Frame& dst, int& seq) const;
    std::atomic<bool> rearm { false };          // SINGLE: click on the screen to arm again
    std::atomic<bool> bPatched { false }, zPatched { false };

private:
    std::atomic<float>* vaP = nullptr; std::atomic<float>* vbP = nullptr; std::atomic<float>* hposP = nullptr; std::atomic<float>* xgP = nullptr;
    std::atomic<float>* srcP = nullptr; std::atomic<float>* lvlP = nullptr; std::atomic<float>* timeP = nullptr; std::atomic<float>* modeP = nullptr;
    std::atomic<float>* coupP = nullptr; std::atomic<float>* powP = nullptr;
    float fs = 48000.0f;
    int src = 0, mode = 0, coup = 1;
    float level = 0.0f, tdiv = 0.01f;
    bool power = true;
    dsp::DcBlock acA, acB;
    // sweep
    bool sweeping = false, armed = true, trigWas = false, aWas = false, singleDone = false;
    double sweepT = 0.0, sweepLen = 0.1, sinceTrig = 0.0;
    int pulse = 0, filled = 0;
    double nextPt = 0.0, ptStep = 1.0;
    Frame work;
    Frame buf[2];
    std::atomic<int> ready { -1 }, seqNo { 0 };
    mutable juce::SpinLock lock;
    int xyPos = 0; double xyAcc = 0.0;
    int sincePub = 1 << 30;
    std::atomic<bool> trigLed { false };
    void publish() noexcept;
};
} // namespace wc
