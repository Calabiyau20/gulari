#pragma once
// RV-275 Dual Equalizer / Reverb (Calabi Yau 275).
// Two identical channels. Input (with a hot / line level switch) -> treble (+-10 dB at 10 kHz) and bass (+-10 dB at
// 50 Hz) shelving equalizer -> "equalized only" output and the reverberation tank -> "reverberated only" output.
// The reverberation knob plus its control input set the dry / wet mix at the two bridged "voltage controlled mix"
// outputs, in one of two spatial modes: near (the dry signal stays in front, the room is added) or recede (the dry
// signal fades and darkens as the reverberation grows: the source moves away).
#include "../rack/Module.h"
#include "Dsp.h"
#include <vector>

namespace wc
{
class M275 : public Module
{
public:
    enum In { In_MixCv1, In_Audio1, In_MixCv2, In_Audio2, In_Count };
    enum Out { Out_Mix1a, Out_Mix1b, Out_Mix2a, Out_Mix2b, Out_Eq1, Out_Rev1, Out_Rev2, Out_Eq2, Out_Count };

    M275 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;

private:
    // a spring-flavoured tank: dispersion allpasses (the chirp of a spring), four damped combs, two diffusers
    struct Tank
    {
        struct Line { std::vector<float> b; int w = 0; float read (int d) const noexcept { int i = w - d; if (i < 0) i += (int) b.size(); return b[(size_t) i]; } void push (float x) noexcept { b[(size_t) w] = x; if (++w >= (int) b.size()) w = 0; } };
        Line comb[4], ap[2];
        int combLen[4] {}, apLen[2] {};
        float combLp[4] {}, disp[8] {};
        void prepare (float fs, float spread);
        float process (float x) noexcept;
        void clear();
    };
    std::atomic<float>* trebP[2] {}; std::atomic<float>* bassP[2] {}; std::atomic<float>* mixP[2] {}; std::atomic<float>* recP[2] {}; std::atomic<float>* hotP[2] {};
    float fs = 48000.0f;
    dsp::Biquad treb[2], bass[2];
    float lastTreb[2] { 99, 99 }, lastBass[2] { 99, 99 };
    dsp::Ramp trebDb[2], bassDb[2];
    int rampLen = 576;
    bool first = true;
    float mix[2] {};
    bool recede[2] {}, hot[2] {};
    Tank tank[2];
    dsp::OnePoleLP darken[2];
    dsp::Ramp sMix[2];
};
} // namespace wc
