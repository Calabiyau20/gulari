#pragma once
// A West Coast module: a spec (jacks, controls, panel layout) shared by the DSP and the UI,
// plus the per-sample processing.
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace wc
{
// Banana jacks carry control (DC, 0..10 V, pulses), mini jacks carry audio, exactly like a Series 200 system.
enum class JackKind { Banana, Mini };

enum ModuleType { MT_Midi = 0, MT_259, MT_Output, MT_Count };

struct JackSpec
{
    const char* label;
    JackKind kind;
    bool output;
    float x, y;          // panel position, in panel units (1 unit = 1 px at 100% zoom)
};

enum class CtrlStyle { BigKnob, Knob, SmallKnob, Toggle, Switch3 };

struct ControlSpec
{
    const char* suffix;  // parameter id suffix (full id = prefix + suffix)
    const char* label;
    CtrlStyle style;
    float x, y;
    juce::uint32 colour; // knob cap colour
};

struct SectionSpec { const char* title; float x, y, w, h; };

struct ModuleSpec
{
    ModuleType type;
    const char* name;      // panel title
    const char* model;     // "Model 259-style" etc
    float width, height;
    std::vector<JackSpec> jacks;      // inputs first, then outputs (order = DSP indices)
    std::vector<ControlSpec> controls;
    std::vector<SectionSpec> sections;
    int numIn() const { int n = 0; for (auto& j : jacks) n += j.output ? 0 : 1; return n; }
    int numOut() const { int n = 0; for (auto& j : jacks) n += j.output ? 1 : 0; return n; }
    int jackIndex (bool output, int idx) const   // DSP index -> position in jacks[]
    {
        int n = 0;
        for (int i = 0; i < (int) jacks.size(); ++i)
            if (jacks[(size_t) i].output == output) { if (n == idx) return i; ++n; }
        return -1;
    }
};

const ModuleSpec& specFor (ModuleType t);

class Module
{
public:
    virtual ~Module() = default;
    virtual void prepare (double sampleRate) = 0;
    virtual void beginBlock (int numSamples) = 0;              // read parameters once per host block
    virtual void tick (const float* in, float* out) noexcept = 0;   // one host sample
    virtual void reset() {}
    const bool* patched = nullptr;   // per input: is a cable plugged in (set by the rack)
    bool isPatched (int in) const noexcept { return patched != nullptr && patched[in]; }
};
} // namespace wc
