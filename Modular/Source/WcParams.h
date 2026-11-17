#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>

// Host parameters: a fixed bank per module instance, so automation keeps working when modules are added
// or removed from the rack. Prefixes: "midi_", "out_", and "a_" .. "d_" for the four 259 slots.
namespace wc
{
constexpr int kMax259 = 4, kMax281 = 2, kMax292 = 2, kMax266 = 1, kMax245 = 1, kMax242 = 1, kMax257 = 1, kMax258 = 2, kMax264 = 1, kMax230 = 1, kMax285 = 1, kMax277 = 1, kMax207 = 1, kMax296 = 1, kMax248 = 1, kMax218 = 1, kMax250 = 1, kMax252 = 1, kMax267 = 1, kMax275 = 1, kMax284 = 1, kMax291 = 1, kMax666 = 4, kMax111 = 8, kMax108 = 4, kMax013 = 2;
inline const char* prefix259 (int i) { static const char* p[kMax259] = { "a_", "b_", "c_", "d_" }; return p[i]; }
inline const char* prefix281 (int i) { static const char* p[kMax281] = { "fa_", "fb_" }; return p[i]; }
inline const char* prefix258 (int i) { static const char* p[kMax258] = { "oa_", "ob_" }; return p[i]; }
inline const char* prefix666 (int i) { static const char* p[kMax666] = { "s1_", "s2_", "s3_", "s4_" }; return p[i]; }
inline const char* prefix292 (int i) { static const char* p[kMax292] = { "la_", "lb_" }; return p[i]; }
inline const char* prefixLfo (int i) { static const char* p[kMax111] = { "lf1_", "lf2_", "lf3_", "lf4_", "lf5_", "lf6_", "lf7_", "lf8_" }; return p[i]; }
inline const char* prefix108 (int i) { static const char* p[kMax108] = { "sw1_", "sw2_", "sw3_", "sw4_" }; return p[i]; }
inline const char* prefix013 (int i) { static const char* p[kMax013] = { "xy1_", "xy2_" }; return p[i]; }   // "tm_" is the 291's

// coarse frequency knob, normalised 0..1 -> Hz
inline float coarseHz (float k, bool low) { return (low ? 0.25f : 27.5f) * std::exp2 (8.0f * k); }
inline float coarseNorm (float hz, bool low) { return std::log2 (hz / (low ? 0.25f : 27.5f)) / 8.0f; }

// LF-111 RATE: one knob for every mode (normalised 0..1). FREE: 0.01 .. 100 Hz, 1 Hz at the centre. SYNC and CLOCK IN:
// one of 11 steps, round (k * 10): divisions 16/1 .. 1/64 (in quarter notes, 64 .. 1/16), or 1/16 .. 16 cycles per pulse
inline float lfoHz (float k) { return 0.01f * std::pow (10.0f, 4.0f * k); }
inline int lfoStep (float k) { return juce::jlimit (0, 10, (int) std::lround (k * 10.0f)); }
inline double lfoBeats (int step) { return 64.0 / std::exp2 ((double) step); }
inline double lfoRatio (int step) { static const double r[11] = { 1.0 / 16, 1.0 / 8, 1.0 / 4, 1.0 / 3, 1.0 / 2, 1, 2, 3, 4, 8, 16 }; return r[juce::jlimit (0, 10, step)]; }
// its text in a mode: 0 Free Hz, 1 Free Sec, 2 Sync, 3 Sync Dotted, 4 Sync Triplet, 5 a clock at CLOCK IN (only the panel knows)
juce::String lfoRateText (float k, int mode);

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// A value typed by hand ("440 Hz", "1,2k", "250", "+50 %", "off") read back for one parameter: the same reading
// every float and choice parameter gives the host through valueFromStringFunction. `shown` is the text the control
// showed when the typing started: a bare number is taken in its unit ("120 ms" -> "250" = 250 ms). Without it a bare
// number is in the unit the parameter shows at its default. Nothing comes back when the text is not a value for
// this parameter (a unit of another kind, a word it does not know).
struct TypedValue
{
    float plain = 0.0f;   // in the parameter's own range, already limited to it
    int limited = 0;      // -1 / +1: the text asked for less than the minimum / more than the maximum
};
std::optional<TypedValue> parseValueText (const juce::RangedAudioParameter&, const juce::String& text, const juce::String& shown = {});
juce::String valueTextExample (const juce::RangedAudioParameter&, const juce::String& shown = {});

// Can an output jack modulate this knob (modulation by drag)? Not levers and selectors (bool and choice), not the
// knobs that pick what a panel edits, not the scopes. The one rule: the rack, the processor and the editor all ask here.
bool isModulatable (const juce::RangedAudioParameter&);   // what to suggest when the text was not understood: "440 Hz or 1.2k"
} // namespace wc
