#pragma once
// A West Coast module: a spec (jacks, controls, panel layout) shared by the DSP and the UI,
// plus the per-sample processing.
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace wc
{
// Banana jacks carry control (DC, 0..10 V, pulses), mini jacks carry audio, exactly like a Series 200 system.
enum class JackKind { Banana, Mini };

enum ModuleType { MT_Midi = 0, MT_259, MT_Output, MT_281, MT_292, MT_266, MT_245, MT_242, MT_257, MT_258, MT_264, MT_230, MT_285, MT_207, MT_277, MT_296, MT_248, MT_218, MT_250, MT_252, MT_267, MT_275, MT_284, MT_291, MT_666, MT_111, MT_108, MT_Count };

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
enum class CtrlStyle { BigKnob, Knob, SmallKnob, Toggle, Switch3, Push, Button, Fader, Prog, Assign };   // Fader: vertical, 26 x 120
// Prog: 248 programming lever (momentary up / down) acting on the stage being edited
// Assign: modulation source button (26 x 26) for the output jack `jack`; label = the source's name (12 characters at
// most: "LFO", "Matrix 3"). Drag it onto a knob or fader to modulate it.

struct ControlSpec
{
    const char* suffix;  // parameter id suffix (full id = prefix + suffix)
    const char* label;
    CtrlStyle style;
    float x, y;
    juce::uint32 colour; // knob cap colour
    const char* scale = nullptr;   // printed scale around the knob, "|" separated, e.g. ".001|.01|.1|1|10"
    const char* scale2 = nullptr;  // second (inner) scale ring
    int jack = -1;                 // Assign: the output jack (DSP index) it modulates from
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
    const char* engr;      // model engraved at the foot of the panel ("277t"), or "" (set from the layout)
    float width, height;
    std::vector<JackSpec> jacks;      // inputs first, then outputs (order = DSP indices)
    std::vector<ControlSpec> controls;
    std::vector<SectionSpec> sections;   // rounded frames with a title (MIDI / Output)
    std::vector<TextSpec> texts;
    std::vector<LineSpec> lines;
    std::vector<LedSpec> leds;
    juce::uint32 bg = 0, ink = 0;       // panel colour / silkscreen colour (0 = aluminium, blue)
    float footY = 0.0f;                 // centre line of the engraved "CALABI YAU (isologo) model" footer
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

// Where a module reads its parameters from, by id (the constructors keep the pointer). A knob that can be modulated
// is read from the rack's mirror: the knob plus its modulation, refreshed on every control tick. The rest is read
// straight from the APVTS, which only ever holds what the knob shows.
struct Params
{
    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float>* eff;   // the mirror, by parameter index (Rack)
    std::atomic<float>* getRawParameterValue (const juce::String& id) const;
};

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
    const bool* outPatched = nullptr;   // per output: does a cable leave it (set by the rack; unknown counts as patched)
    bool isOutPatched (int o) const noexcept { return outPatched == nullptr || outPatched[o]; }
};
} // namespace wc
