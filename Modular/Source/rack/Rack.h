#pragma once
// The rack: module instances, the cable graph and the per-sample engine.
//
// Instances are fixed slots processed in a fixed order: MIDI Interface, 259 A, B, C, D, Output.
// A cable from an earlier slot to a later one has no delay; a cable going backwards (feedback,
// e.g. 259 B -> 259 A FM while A -> B) reads the previous sample, which is the 1-sample delay of
// any digital model of a patch with loops.
#include "Module.h"
#include "../WcParams.h"
#include <memory>

namespace wc
{
class MidiInterface;
class OutputModule;
class M259;
class M281;
class M292;

// slots, in processing order: control sources first, then oscillators, then gates, then the output
constexpr int kInstMidi = 0;
constexpr int kInst281 = 1;
constexpr int kInst259 = kInst281 + kMax281;
constexpr int kInst292 = kInst259 + kMax259;
constexpr int kInstOutput = kInst292 + kMax292;
constexpr int kNumInst = kInstOutput + 1;
inline ModuleType typeOfInst (int inst)
{
    if (inst == kInstMidi) return MT_Midi;
    if (inst < kInst259) return MT_281;
    if (inst < kInst292) return MT_259;
    if (inst < kInstOutput) return MT_292;
    return MT_Output;
}
inline int subIndex (int inst)
{
    switch (typeOfInst (inst)) { case MT_281: return inst - kInst281; case MT_259: return inst - kInst259; case MT_292: return inst - kInst292; default: return 0; }
}
inline juce::String instPrefix (int inst)
{
    switch (typeOfInst (inst)) { case MT_281: return prefix281 (subIndex (inst)); case MT_259: return prefix259 (subIndex (inst)); case MT_292: return prefix292 (subIndex (inst)); default: return {}; }
}
inline bool isRemovable (int inst) { return inst != kInstMidi && inst != kInstOutput; }
juce::String instName (int inst);        // "MIDI", "281 A", "259 A", "292 A", "OUTPUT"

struct Cable
{
    int srcInst = -1, srcJack = -1;    // output jack index of the source module
    int dstInst = -1, dstJack = -1;    // input jack index of the destination module
    int colour = 0;
    bool operator== (const Cable& o) const { return srcInst == o.srcInst && srcJack == o.srcJack && dstInst == o.dstInst && dstJack == o.dstJack; }
};

class Rack
{
public:
    static constexpr int kMaxJacks = 24;

    explicit Rack (juce::AudioProcessorValueTreeState& apvts);
    ~Rack();

    void prepare (double sampleRate);
    void beginBlock (int numSamples);
    void handleMidi (const juce::MidiMessage& m);
    void tick() noexcept;
    float outL() const noexcept;
    float outR() const noexcept;

    // message thread: publish a new patch (cables + which slots are in the rack)
    void setPatch (const std::vector<Cable>& cables, const std::array<bool, kNumInst>& present);

    Module* module (int inst) const { return mods[(size_t) inst].get(); }
    M259* m259 (int i) const;
    M281* m281 (int i) const;
    M292* m292 (int i) const;
    MidiInterface& midi() const;
    OutputModule& output() const;
    // live value of an output jack (for jack LEDs)
    float jackValue (int inst, int outJack) const noexcept { return outVal[(size_t) inst][(size_t) outJack]; }

private:
    struct Patch
    {
        std::array<std::array<int, kMaxJacks>, kNumInst> srcInst {}, srcJack {};
        std::array<bool, kNumInst> present {};
    };
    std::array<std::unique_ptr<Module>, kNumInst> mods;
    std::array<std::array<float, kMaxJacks>, kNumInst> outVal {};
    std::array<std::array<bool, kMaxJacks>, kNumInst> patched {};
    Patch live, pending;
    juce::SpinLock patchLock;
    std::atomic<bool> patchDirty { false };
    void applyPending() noexcept;
};
} // namespace wc
