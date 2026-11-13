#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>

// Host parameters: a fixed bank per module instance, so automation keeps working when modules are added
// or removed from the rack. Prefixes: "midi_", "out_", and "a_" .. "d_" for the four 259 slots.
namespace wc
{
constexpr int kMax259 = 4, kMax281 = 2, kMax292 = 2, kMax266 = 1, kMax245 = 1, kMax242 = 1, kMax257 = 1, kMax258 = 2, kMax264 = 1, kMax230 = 1, kMax285 = 1, kMax277 = 1, kMax207 = 1, kMax296 = 1, kMax248 = 1, kMax218 = 1, kMax250 = 1, kMax252 = 1, kMax267 = 1, kMax275 = 1, kMax284 = 1, kMax291 = 1, kMax666 = 4;
inline const char* prefix259 (int i) { static const char* p[kMax259] = { "a_", "b_", "c_", "d_" }; return p[i]; }
inline const char* prefix281 (int i) { static const char* p[kMax281] = { "fa_", "fb_" }; return p[i]; }
inline const char* prefix258 (int i) { static const char* p[kMax258] = { "oa_", "ob_" }; return p[i]; }
inline const char* prefix666 (int i) { static const char* p[kMax666] = { "s1_", "s2_", "s3_", "s4_" }; return p[i]; }
inline const char* prefix292 (int i) { static const char* p[kMax292] = { "la_", "lb_" }; return p[i]; }

// coarse frequency knob, normalised 0..1 -> Hz
inline float coarseHz (float k, bool low) { return (low ? 0.25f : 27.5f) * std::exp2 (8.0f * k); }
inline float coarseNorm (float hz, bool low) { return std::log2 (hz / (low ? 0.25f : 27.5f)) / 8.0f; }

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
juce::String valueTextExample (const juce::RangedAudioParameter&);   // what to suggest when the text was not understood: "440 Hz or 1.2k"
} // namespace wc
