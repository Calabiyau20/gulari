#pragma once
// FS-285 Frequency Shifter / Balanced Modulator - emulation of the Model 285 / 285t.
// Frequency shifter: the signal is split into a quadrature pair (two 4th-order allpass chains, 90 deg from ~15 Hz to
// ~20 kHz) and multiplied by a quadrature reference: SUM = upper sideband (shift up), DIFFERENCE = lower sideband.
// Reference: internal oscillator (0 - 1 kHz knob + control c.v. through an attenuverter, 100 Hz/V, through zero; F.M. from
// the mod. input) or, with INT off, the external ref. input (made quadrature by the same allpass network).
// Balanced modulator: RING = signal x ref; VARIABLE = crossfade from amplitude modulation to ring modulation
// (amplitude knob + control c.v.).
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
// 90 degree phase splitter (Olli Niemitalo's allpass pair)
struct Hilbert
{
    struct AP { float a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0; float process (float x) noexcept { const float y = a2 * (x + y2) - x2; x2 = x1; x1 = x; y2 = y1; y1 = y; return y; } };
    AP a[4], b[4];
    float delayA = 0.0f;
    Hilbert()
    {
        const float ca[4] = { 0.6923878f, 0.9360654322959f, 0.9882295226860f, 0.9987488452737f };
        const float cb[4] = { 0.4021921162426f, 0.8561710882420f, 0.9722909545651f, 0.9952884791278f };
        for (int i = 0; i < 4; ++i) { a[i].a2 = ca[i] * ca[i]; b[i].a2 = cb[i] * cb[i]; }
    }
    void process (float x, float& re, float& im) noexcept
    {
        float ya = x, yb = x;
        for (auto& f : a) ya = f.process (ya);
        for (auto& f : b) yb = f.process (yb);
        re = delayA; delayA = ya;     // the a-path, one sample late, is in quadrature with the b-path
        im = yb;
    }
    void reset() noexcept { for (auto& f : a) f = AP { f.a2 }; for (auto& f : b) f = AP { f.a2 }; delayA = 0.0f; }
};

class M285 : public Module
{
public:
    enum In { In_FsControl, In_FsRef, In_FsSignal, In_FsMod, In_BmControl, In_BmSignal, In_BmRef, In_Count };
    enum Out { Out_Diff, Out_Diff2, Out_Sum, Out_Sum2, Out_Ring, Out_Ring2, Out_Var, Out_Var2, Out_Count };

    M285 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override { hs.reset(); hr.reset(); ph = 0.0; }

    static float refHz (float k) { return 1000.0f * k * k; }   // 0 .. 1 kHz

private:
    std::atomic<float>* ctlP = nullptr; std::atomic<float>* refP = nullptr; std::atomic<float>* fmP = nullptr; std::atomic<float>* intP = nullptr;
    std::atomic<float>* bmCtlP = nullptr; std::atomic<float>* ampP = nullptr;
    float fs = 48000.0f, ctl = 0.0f, ref = 0.0f, fm = 0.0f, bmCtl = 0.0f, amp = 1.0f;
    bool internal = true;
    double ph = 0.0;
    Hilbert hs, hr;
};
} // namespace wc
