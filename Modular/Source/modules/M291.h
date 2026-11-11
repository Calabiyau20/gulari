#pragma once
// TM-291 Triple Morphing Filter (Calabi Yau 291e).
// Three resonant bandpass filters (nodes A, B, C). Eight stages each store the frequency, bandwidth and amplitude of the
// three nodes plus a transition time; the module morphs from stage to stage (stop / play, advance pulse, or a control
// voltage that sweeps continuously through the stages). Editing: "edit select" picks the stage, "node" the filter; the
// AMPLITUDE / BANDWIDTH / FREQ encoders then set that node at that stage, TIME EDIT its transition time. Per-node
// control inputs add to the stored values; the modulation inputs (audio rate) act on amplitude, bandwidth or frequency
// with the index knob. Outputs: the sum ("all", two bridged jacks, plus the expand input) and each node. Solo: only the
// selected node reaches "all".
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M291 : public Module
{
public:
    static constexpr int kStages = 8, kNodes = 3;
    enum In { In_ModA, In_ModB, In_ModC, In_ModAll, In_SigA, In_SigB, In_SigC, In_SigAll, In_Expand,
              In_AmpA, In_AmpB, In_AmpC, In_BwAll, In_BwA, In_BwB, In_BwC, In_FreqA, In_FreqB, In_FreqC, In_StageCv, In_Adv, In_Count };
    enum Out { Out_All1, Out_All2, Out_A, Out_B, Out_C, Out_Count };
    // LEDs: 0..7 stages, 8 play, 10..12 node, 20..22 mod type, 23 solo, 24 remote,
    //       30..35 amplitude ring, 40..45 bandwidth ring, 50..57 frequency ring, 60..65 index ring
    float led (int id) const noexcept override;

    M291 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;
    static float freqHz (float k) noexcept { return 31.0f * std::pow (2.0f, 7.0f * dsp::clampf (k, 0.0f, 1.0f)); }
    static float stageTime (float k) noexcept { return 0.01f * std::pow (1000.0f, dsp::clampf (k, 0.0f, 1.0f)); }

    std::atomic<float> position { 0.0f };   // current morph position 0..8 (for the display)

private:
    std::atomic<float>* fP[kStages][kNodes] {}; std::atomic<float>* bwP[kStages][kNodes] {}; std::atomic<float>* ampP[kStages][kNodes] {};
    std::atomic<float>* timeP[kStages] {};
    std::atomic<float>* nodeP = nullptr; std::atomic<float>* editP = nullptr; std::atomic<float>* playP = nullptr; std::atomic<float>* indexP = nullptr;
    std::atomic<float>* modP = nullptr; std::atomic<float>* soloP = nullptr; std::atomic<float>* remP = nullptr;
    float fs = 48000.0f;
    float f[kStages][kNodes] {}, bwv[kStages][kNodes] {}, amp[kStages][kNodes] {}, tm[kStages] {};
    int node = 0, edit = 0, modType = 0;
    bool play = false, solo = false;
    float index = 0.0f;
    // morph state
    int from = 0, to = 0;
    double t = 0.0; float dur = 1.0f;
    bool advWas = false, playWas = false;
    int lastEdit = -1;
    dsp::Svf filt[kNodes];
    int ctr = 0;
    float cur[3][kNodes] {};   // current freq / bw / amp after morphing (no c.v.)
};
} // namespace wc
