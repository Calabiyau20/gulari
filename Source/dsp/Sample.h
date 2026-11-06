#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>

namespace pg
{
// Immutable audio sample shared by all voices of the granular oscillator.
struct SampleData
{
    juce::AudioBuffer<float> audio;   // always 2 channels, plus 3 guard samples at the end
    int length = 0;                   // frames, without guard samples
    double sampleRate = 44100.0;
    juce::String name, path;
    std::vector<float> peaks;         // 1024 peak values for the waveform display
};

using SamplePtr = std::shared_ptr<const SampleData>;

// Loads WAV, AIFF, FLAC, MP3 and Ogg (and anything CoreAudio reads on macOS).
// Returns nullptr and fills 'error' on failure. Long files are cut at maxSeconds.
SamplePtr loadSampleFile (const juce::File& file, juce::String& error, double maxSeconds = 180.0);

// Builds a sample from raw interleaved data (used for embedded / generated audio).
SamplePtr makeSample (juce::AudioBuffer<float>&& source, int numFrames, double sampleRate, const juce::String& name);

// A built-in evolving texture, so the oscillator sounds before any file is loaded.
SamplePtr makeDefaultSample();

juce::String supportedAudioWildcard();
} // namespace pg
