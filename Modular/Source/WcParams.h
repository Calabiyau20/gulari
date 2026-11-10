#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Host parameters: a fixed bank per module instance, so automation keeps working when modules are added
// or removed from the rack. Prefixes: "midi_", "out_", and "a_" .. "d_" for the four 259 slots.
namespace wc
{
constexpr int kMax259 = 4, kMax281 = 2, kMax292 = 2, kMax266 = 1, kMax245 = 1, kMax242 = 1;
inline const char* prefix259 (int i) { static const char* p[kMax259] = { "a_", "b_", "c_", "d_" }; return p[i]; }
inline const char* prefix281 (int i) { static const char* p[kMax281] = { "fa_", "fb_" }; return p[i]; }
inline const char* prefix292 (int i) { static const char* p[kMax292] = { "la_", "lb_" }; return p[i]; }

// coarse frequency knob, normalised 0..1 -> Hz
inline float coarseHz (float k, bool low) { return (low ? 0.25f : 27.5f) * std::exp2 (8.0f * k); }
inline float coarseNorm (float hz, bool low) { return std::log2 (hz / (low ? 0.25f : 27.5f)) / 8.0f; }

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
} // namespace wc
