#pragma once
// OUT-2 Output: stereo out of the rack. LEVEL CV goes through a vactrol-like lag, so a gate patched
// to it already shapes the note without clicks.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class OutputModule : public Module
{
public:
    enum In { In_Left, In_Right, In_LevelCv, In_Count };

    explicit OutputModule (juce::AudioProcessorValueTreeState& apvts);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;

    float left = 0.0f, right = 0.0f;           // this sample's stereo output
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };

private:
    std::atomic<float>* level = nullptr;
    std::atomic<float>* cvAmt = nullptr;
    std::atomic<float>* lag = nullptr;
    std::atomic<float>* master = nullptr;
    float fs = 48000.0f;
    dsp::Ramp sLevel, sCv, sMaster;
    dsp::Vactrol lagV;
    bool first = true;
    int rampLen = 480;
    float pkL = 0.0f, pkR = 0.0f;
    int pkCount = 0;
};
} // namespace wc
