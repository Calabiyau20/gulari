#pragma once
#include "Common.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <vector>

namespace pg
{
// Band-limited wavetable: N frames, each stored as a mip chain (one table per octave)
// so high notes never alias.
struct Wavetable
{
    static constexpr int kFrameLen = 2048;
    static constexpr int kLevels = 11;               // max harmonics 1024 >> level
    static int levelLength (int level) { return juce::jmax (64, kFrameLen >> level); }
    static int levelOffset (int level);             // offset inside one frame's storage
    static int frameStorage();                      // floats per frame (all levels + guards)

    juce::String name;
    int numFrames = 0;
    std::vector<float> data;                        // numFrames * frameStorage()

    // Builds from raw single-cycle frames (each kFrameLen long).
    static std::shared_ptr<const Wavetable> fromFrames (const std::vector<std::vector<float>>& frames, const juce::String& name);

    const float* level (int frame, int lvl) const { return data.data() + (size_t) frame * (size_t) frameStorage() + (size_t) levelOffset (lvl); }
};
using WavetablePtr = std::shared_ptr<const Wavetable>;

// The built-in tables (index = choices::wavetables() minus "User").
const std::vector<WavetablePtr>& builtInWavetables();

// WAV/AIFF/FLAC/MP3 -> wavetable. Files whose length is a multiple of 2048 are read as
// 2048-sample frames (Serum style, up to 256 frames); shorter files become one cycle.
WavetablePtr loadWavetableFile (const juce::File& file, juce::String& error);

// One wavetable voice with up to 7 unison copies.
class WavetableOsc
{
public:
    void prepare (double sampleRate, uint32_t seed);
    void noteOn (bool randomPhase);
    // freqHz: base pitch; pos: 0..1; phaseMod: added to the phase (in cycles) of every copy
    void setup (int unison, float detune, float spread, float pan);
    // Warp modes (choices::warpModes): reshape the phase or the read sample of each cycle
    void setWarp (int mode, float amount) { warpMode = mode; warpAmt = clamp01 (amount); }
    void tick (const Wavetable& wt, float freqHz, float pos, float phaseMod, float& outL, float& outR);
    float lastMono() const { return last; }
    // One cycle as the oscillator plays it (warp included), for displays
    static void renderCycle (const Wavetable& wt, float pos, int warpMode, float warpAmount, float* out, int n);

private:
    static constexpr int kMaxUnison = 7;
    double sr = 48000.0;
    float phase[kMaxUnison] {};
    float detuneRatio[kMaxUnison] {};
    float gainL[kMaxUnison] {}, gainR[kMaxUnison] {};
    int voices = 1;
    int warpMode = 0;
    float warpAmt = 0.0f;
    float norm = 1.0f;
    float last = 0.0f;
    Rng rng;
};

// Simple PolyBLEP oscillator for the modulator and sub.
struct BasicOsc
{
    float phase = 0.0f, tri = 0.0f;
    float tick (int shape, float freqHz, float sampleRate, float phaseMod);   // 0 sine 1 tri 2 saw 3 square
};
} // namespace pg
