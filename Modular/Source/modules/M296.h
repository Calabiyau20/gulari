#pragma once
// PS-296 Programmable Spectral Processor - emulation of the Model 296 / 296t.
// Sixteen channels (<100, 150, 250, 350, 500, 630, 800, 1k, 1.3k, 1.6k, 2k, 2.6k, 3.5k, 5k, 8k, >10k Hz). Channel 0 is a low
// pass, channel F a high pass, the others three stagger-tuned 2nd-order band passes (wide flat top, steep skirts), as on the
// original filter cards. Inputs: ALL feeds every channel, EVEN feeds channels 0, 2, 4.. and ODD channels 1, 3, 5..
// Each channel: envelope follower out (0 - 10 V; decay short / combo / long), attenuator slider (-30 .. +6 dB) and its
// own attenuated output. Sums: COMB FILTER even / odd (unattenuated), ATTENUATOR OUTPUTS even / all / odd.
// PROGRAMMED outputs: every channel through a VCA. The VCAs open over a window of channels set by PROGRAM CONTROL
// (frequency + c.v. through an attenuverter, width + c.v.), plus each channel's LOCAL PROGRAM INPUT (10 V = open).
// PROGRAMMED SPECTRUM TRANSFER: with a switch on, the even (or odd) VCAs follow the envelopes of their odd (even) neighbours
// - a vocoder - and the pre-emphasis knobs tilt those envelopes towards the high channels.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M296 : public Module
{
public:
    static constexpr int kBands = 16;
    enum In { In_Local0 = 0, In_SigEven = 16, In_SigAll, In_SigOdd, In_FreqCv, In_WidthCv, In_Count };
    enum Out { Out_Env0 = 0, Out_Band0 = 16, Out_CombEven = 32, Out_CombOdd, Out_AttEven, Out_AttAll, Out_AttOdd, Out_ProgEven, Out_ProgAll, Out_ProgOdd, Out_Count };
    enum Led { Led_Even, Led_All, Led_Odd, Led_Band0 = 10, Led_Count = 26 };
    static constexpr float kCentre[kBands] = { 100, 150, 250, 350, 500, 630, 800, 1000, 1300, 1600, 2000, 2600, 3500, 5000, 8000, 10000 };

    M296 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;
    float led (int id) const noexcept override { return id >= 0 && id < Led_Count ? leds[(size_t) id].load (std::memory_order_relaxed) : 0.0f; }

    static float sliderDb (float k) { return -36.0f + 42.0f * k; }   // printed -30 .. +6, bottom = off

private:
    struct Svf     // TPT state variable filter
    {
        float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
        void set (float fc, float q, float fs) { g = std::tan (dsp::kPi * juce::jmin (fc, 0.45f * fs) / fs); k = 1.0f / q; a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2; }
        void run (float x, float& lp, float& bp, float& hp) noexcept
        {
            const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
        void reset() { ic1 = ic2 = 0; }
    };
    std::atomic<float>* sliderP[kBands] {};
    std::atomic<float>* decayP = nullptr; std::atomic<float>* freqP = nullptr; std::atomic<float>* fcvP = nullptr; std::atomic<float>* widthP = nullptr;
    std::atomic<float>* preEP = nullptr; std::atomic<float>* preOP = nullptr; std::atomic<float>* trEP = nullptr; std::atomic<float>* trOP = nullptr;
    float fs = 48000.0f;
    Svf f[kBands][3];
    float bandGain[kBands] {}, norm[kBands] {}, env[kBands] {}, rel[kBands] {}, att = 0.0f, vca[kBands] {};
    float freqK = 0.5f, fcvK = 0.0f, widthK = 0.3f, preE = 0.0f, preO = 0.0f;
    bool trE = false, trO = false;
    float inPeak[3] {};
    dsp::OnePoleLP vcaLag[kBands];
    std::array<std::atomic<float>, Led_Count> leds {};
};
} // namespace wc
