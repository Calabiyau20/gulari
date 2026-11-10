#pragma once
// CW-259 Complex Waveform Generator - emulation of the Model 259 Programmable Complex Waveform Generator.
// See Modular/docs/259-reverse-engineering.json for the stage-by-stage schematics and sources.
#include "../rack/Module.h"
#include "Dsp.h"
#include "Wavefolder259.h"

namespace wc
{
class M259 : public Module
{
public:
    enum In { In_MoPitch, In_MoCv, In_MoFm, In_IndexCv, In_ProgWave, In_ProgPitch, In_ProgTimbre, In_ProgAmp, In_Lock,
              In_PoPitch, In_PoCv, In_PoFm, In_TimbreCv, In_SymCv, In_OrderCv, In_Count };
    enum Out { Out_MoSignal, Out_MoCv, Out_Sine, Out_Square, Out_Final1, Out_Final2,
               Out_MoTri, Out_MoPulse, Out_PoPulse, Out_Count };   // 259t: fixed triangle, the two GATE OUT pulse jacks
    // panel LEDs
    enum Led { Led_Low, Led_High, Led_Saw, Led_Sqr, Led_Tri, Led_Lock, Led_Amp, Led_Pitch, Led_Timbre, Led_1V,
               Led_Sym, Led_Order, Led_MoGate, Led_PoGate, Led_Count };
    float led (int id) const noexcept override { return id >= 0 && id < Led_Count ? leds[(size_t) id].load (std::memory_order_relaxed) : 0.0f; }

    M259 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);

    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;

    // for the panel LEDs
    std::atomic<float> moLed { 0.0f };
    std::atomic<bool> lockLed { false };

private:
    struct P { std::atomic<float>* v = nullptr; float get() const { return v != nullptr ? v->load() : 0.0f; } };
    std::array<std::atomic<float>, Led_Count> leds {};
    P lockOn, volt1;
    float pitchDiv = 1.2f;
    bool lockOnV = false;
    P moFreq, moFine, moRange, moWave, moCvAmt, moFmAmt, index, indexCvAmt, modAmp, modPitch, modTimbre, prog, lockAmt,
      poFreq, poFine, poCvAmt, poFmAmt, timbre, timbreCvAmt, sym, order;

    static constexpr int kOs = 4;
    float fs = 48000.0f, fsOs = 192000.0f;
    int rampLen = 480;

    // smoothed controls (per host sample)
    dsp::Ramp sMoHz, sPoHz, sIndex, sTimbre, sSym, sOrder, sMoFm, sPoFm, sLock, sMoCv, sPoCv, sTimbreCv, sIndexCv;
    bool firstBlock = true;
    int moWaveLocal = 2;
    bool lowRange = false;
    bool ampLocal = false, pitchLocal = false, timbreLocal = false;
    int progMode = 0;

    // oscillator state
    double moPhase = 0.0, poPhase = 0.0;
    float prevIn[In_Count] {};

    // modulator outputs used by the principal: previous oversampled value (one-sample feedback inside the module)
    float moLast = 0.0f;

    // phase lock
    float lockPrev = 0.0f;
    double lockSamplesSince = 0.0, lockPeriod = 0.0;
    int lockDivCount = 0;
    float lockRatio = 1.0f;   // smoothed frequency correction factor

    Wavefolder259 folder;
    float foldGain[33] {};     // compensation vs A (0.25 .. 10 V)
    dsp::Vactrol amVactrol;
    dsp::DcBlock acSine, acSquare, acFinal, acMo;
    std::array<float, dsp::Decimator4::kTaps> taps {};
    dsp::Decimator4 decFinal, decSine, decSquare, decMo, decTri;
    dsp::DcBlock acTri;

    float foldComp (float A) const noexcept;
};
} // namespace wc
