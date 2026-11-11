#pragma once
// The rack: module instances, the cable graph and the per-sample engine.
//
// Instances are fixed slots processed in a fixed order: MIDI, 218, 242, 245, 248, 250, 252, 266, 230, 264, 257, 281 A-B, 284,
// 258 A-B, 259 A-D, 285, 267, 291, 296, 277, 292 A-B, 275, 207, SC-666 1-4, Output.
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
class M266;
class M245;
class M242;
class M257;
class M258;
class M264;
class M230;
class M285;
class M277;
class M207;
class M296;
class M248;
class M218;
class M250;
class M252;
class M267;
class M275;
class M284;
class M291;
class M666;

// slots, in processing order: control sources first, then oscillators, then gates, then the output
constexpr int kInstMidi = 0;
constexpr int kInst218 = 1;
constexpr int kInst242 = kInst218 + kMax218;
constexpr int kInst245 = kInst242 + kMax242;
constexpr int kInst248 = kInst245 + kMax245;
constexpr int kInst250 = kInst248 + kMax248;
constexpr int kInst252 = kInst250 + kMax250;
constexpr int kInst266 = kInst252 + kMax252;
constexpr int kInst230 = kInst266 + kMax266;
constexpr int kInst264 = kInst230 + kMax230;
constexpr int kInst257 = kInst264 + kMax264;
constexpr int kInst281 = kInst257 + kMax257;
constexpr int kInst284 = kInst281 + kMax281;
constexpr int kInst258 = kInst284 + kMax284;
constexpr int kInst259 = kInst258 + kMax258;
constexpr int kInst285 = kInst259 + kMax259;
constexpr int kInst267 = kInst285 + kMax285;
constexpr int kInst291 = kInst267 + kMax267;
constexpr int kInst296 = kInst291 + kMax291;
constexpr int kInst277 = kInst296 + kMax296;
constexpr int kInst292 = kInst277 + kMax277;
constexpr int kInst275 = kInst292 + kMax292;
constexpr int kInst207 = kInst275 + kMax275;
constexpr int kInst666 = kInst207 + kMax207;
constexpr int kInstOutput = kInst666 + kMax666;
constexpr int kNumInst = kInstOutput + 1;
inline ModuleType typeOfInst (int inst)
{
    if (inst == kInstMidi) return MT_Midi;
    if (inst < kInst242) return MT_218;
    if (inst < kInst245) return MT_242;
    if (inst < kInst248) return MT_245;
    if (inst < kInst250) return MT_248;
    if (inst < kInst252) return MT_250;
    if (inst < kInst266) return MT_252;
    if (inst < kInst230) return MT_266;
    if (inst < kInst264) return MT_230;
    if (inst < kInst257) return MT_264;
    if (inst < kInst281) return MT_257;
    if (inst < kInst284) return MT_281;
    if (inst < kInst258) return MT_284;
    if (inst < kInst259) return MT_258;
    if (inst < kInst285) return MT_259;
    if (inst < kInst267) return MT_285;
    if (inst < kInst291) return MT_267;
    if (inst < kInst296) return MT_291;
    if (inst < kInst277) return MT_296;
    if (inst < kInst292) return MT_277;
    if (inst < kInst275) return MT_292;
    if (inst < kInst207) return MT_275;
    if (inst < kInst666) return MT_207;
    if (inst < kInstOutput) return MT_666;
    return MT_Output;
}
inline int firstInstOf (ModuleType t)
{
    switch (t)
    {
        case MT_218: return kInst218; case MT_242: return kInst242; case MT_245: return kInst245; case MT_248: return kInst248; case MT_250: return kInst250; case MT_252: return kInst252; case MT_266: return kInst266; case MT_230: return kInst230;
        case MT_264: return kInst264; case MT_257: return kInst257; case MT_281: return kInst281; case MT_284: return kInst284; case MT_258: return kInst258;
        case MT_259: return kInst259; case MT_285: return kInst285; case MT_267: return kInst267; case MT_291: return kInst291; case MT_296: return kInst296; case MT_277: return kInst277;
        case MT_292: return kInst292; case MT_275: return kInst275; case MT_207: return kInst207; case MT_666: return kInst666; case MT_Output: return kInstOutput; default: return kInstMidi;
    }
}
inline int subIndex (int inst) { return inst - firstInstOf (typeOfInst (inst)); }
inline juce::String instPrefix (int inst)
{
    switch (typeOfInst (inst))
    {
        case MT_218: return "kb_"; case MT_242: return "pp_"; case MT_245: return "sv_"; case MT_248: return "mf_"; case MT_250: return "sq_"; case MT_252: return "rg_"; case MT_266: return "ua_"; case MT_230: return "ef_"; case MT_264: return "sh_";
        case MT_257: return "vp_"; case MT_281: return prefix281 (subIndex (inst)); case MT_258: return prefix258 (subIndex (inst));
        case MT_285: return "fs_"; case MT_296: return "sp_"; case MT_277: return "dl_"; case MT_207: return "mx_";
        case MT_267: return "us_"; case MT_275: return "rv_"; case MT_284: return "ev_"; case MT_291: return "tm_"; case MT_666: return prefix666 (subIndex (inst));
        case MT_259: return prefix259 (subIndex (inst)); case MT_292: return prefix292 (subIndex (inst)); default: return {};
    }
}
// stable name of a slot in patch files ("a" = 259 A, "fa" = 281 A, "la" = 292 A, "ua" = 266, "midi", "out")
inline juce::String instKey (int inst)
{
    if (inst == kInstMidi) return "midi";
    if (inst == kInstOutput) return "out";
    return instPrefix (inst).dropLastCharacters (1);
}
inline int instFromKey (const juce::String& k)
{
    for (int i = 0; i < kNumInst; ++i) if (instKey (i) == k) return i;
    return -1;
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
    static constexpr int kMaxJacks = 48;

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
    M266* m266() const;
    M245* m245() const;
    M242* m242() const;
    M257* m257() const;
    M258* m258 (int i) const;
    M264* m264() const;
    M230* m230() const;
    M285* m285() const;
    M277* m277() const;
    M207* m207() const;
    M296* m296() const;
    M248* m248() const;
    M218* m218() const;
    M250* m250() const;
    M252* m252() const;
    M267* m267() const;
    M275* m275() const;
    M284* m284() const;
    M291* m291() const;
    M666* m666 (int i) const;
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
    std::array<int, kNumInst> numIn {};
    Patch live, pending;
    juce::SpinLock patchLock;
    std::atomic<bool> patchDirty { false };
    void applyPending() noexcept;
};
} // namespace wc
