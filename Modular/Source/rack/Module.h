#pragma once
// A West Coast module: a spec (jacks, controls, panel layout) shared by the DSP and the UI,
// plus the per-sample processing.
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace wc
{
// Banana jacks carry control (DC, 0..10 V, pulses), mini jacks carry audio, exactly like a Series 200 system.
enum class JackKind { Banana, Mini };

enum ModuleType { MT_Midi = 0, MT_259, MT_Output, MT_281, MT_292, MT_266, MT_245, MT_242, MT_Count };

struct JackSpec
{
    const char* label;
    JackKind kind;
    bool output;
    float x, y;          // panel position, in panel units (1 unit = 1 px at 100% zoom)
    bool pulse = false;  // timing signal (gates, triggers): red jack, as on the 200 series
    juce::uint32 tint = 0;   // explicit jack colour (0 = by role: red pulse, black c.v. in, blue c.v. out)
};

// Push = momentary button (not a parameter); Button = white button that steps a parameter
enum class CtrlStyle { BigKnob, Knob, SmallKnob, Toggle, Switch3, Push, Button };

struct ControlSpec
{
    const char* suffix;  // parameter id suffix (full id = prefix + suffix)
    const char* label;
    CtrlStyle style;
    float x, y;
    juce::uint32 colour; // knob cap colour
    const char* scale = nullptr;   // printed scale around the knob, "|" separated, e.g. ".001|.01|.1|1|10"
    const char* scale2 = nullptr;  // second (inner) scale ring
};

struct SectionSpec { const char* title; float x, y, w, h; };

// silkscreen elements, all in panel units
struct TextSpec { const char* text; float x, y; float size; int style; int just; };   // style 0 italic, 1 bold italic, 2 bold caps; just 0 centre, 1 left, 2 right
struct LineSpec { float x1, y1, x2, y2; float width; bool dashed; bool arrow; };
struct LedSpec { float x, y; int id; juce::uint32 colour; };

struct ModuleSpec
{
    ModuleType type;
    const char* name;      // panel title
    const char* model;     // model code on the panel
    const char* after;     // "after Buchla 259" (historical reference), or ""
    float width, height;
    std::vector<JackSpec> jacks;      // inputs first, then outputs (order = DSP indices)
    std::vector<ControlSpec> controls;
    std::vector<SectionSpec> sections;   // rounded frames with a title (MIDI / Output)
    std::vector<TextSpec> texts;
    std::vector<LineSpec> lines;
    std::vector<LedSpec> leds;
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
    virtual float led (int) const noexcept { return 0.0f; }    // panel LEDs (0..1), read by the UI
    const bool* patched = nullptr;   // per input: is a cable plugged in (set by the rack)
    bool isPatched (int in) const noexcept { return patched != nullptr && patched[in]; }
};
} // namespace wc
