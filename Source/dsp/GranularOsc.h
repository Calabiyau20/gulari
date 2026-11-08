#pragma once
#include "Common.h"
#include "Sample.h"
#include <array>
#include <atomic>

namespace pg
{
// Shared read-only window tables (one per shape)
struct GrainWindows
{
    static constexpr int kSize = 1024;
    static constexpr int kShapes = 6;
    float table[kShapes][kSize + 1];
    GrainWindows();
    static const GrainWindows& get() { static GrainWindows w; return w; }
};

// Live grain positions for the editor's waveform view (written by the audio thread)
struct GrainViz
{
    static constexpr int kSlots = 32;
    std::array<std::atomic<float>, kSlots> pos {};   // -1 = empty, else 0..1
    std::array<std::atomic<float>, kSlots> amp {};
    std::atomic<float> playhead { 0.0f };
    GrainViz() { for (auto& p : pos) p.store (-1.0f); }
};

// Per-voice granular oscillator. Two modes:
//  Cloud   - asynchronous grains at 'density' grains/s, pitch follows the keyboard by
//            changing the playback rate of each grain.
//  Pitched - grains are fired once per period of the played note (pitch-synchronous),
//            so the note sets the pitch while the sample keeps its formants.
class GranularOsc
{
public:
    struct Params
    {
        int mode = 0;          // 0 cloud, 1 pitched
        float pos = 0.3f;      // 0..1
        float scan = 0.0f;     // x realtime, -2..2
        float spray = 0.05f;   // 0..1
        float sizeMs = 120.0f;
        float density = 20.0f; // grains / s (cloud)
        float pitch = 0.0f;    // semitones
        float pitchRand = 0.0f;
        bool keytrack = true;
        int root = 60;
        int window = 0;
        float reverse = 0.0f;  // probability
        float stereo = 0.5f;
        float regionStart = 0.0f, regionEnd = 1.0f;   // part of the file grains may read
        float jitter = 0.3f;    // timing randomness of the cloud
        float octRand = 0.0f;   // probability of a grain jumping +/- one octave
        float formant = 0.0f;   // semitones: shifts the grain playback rate only
        int streams = 1;        // parallel grain streams, each detuned
        float streamDetune = 0.15f;
        float tilt = 0.0f;      // -1..1 skews the window towards the start or the end
        bool freeze = false;    // stops the scan
    };

    void prepare (double sampleRate, uint32_t seed);
    void noteOn (bool resetScan);
    void setParams (const Params& p, float noteSemis) { prm = p; note = noteSemis; }
    void tick (const SampleData& s, float& outL, float& outR);
    void writeViz (const SampleData& s, GrainViz& viz) const;

private:
    struct Grain
    {
        bool active = false;
        double readPos = 0.0;   // in source frames
        double rate = 1.0;      // source frames per output sample (signed)
        int age = 0, length = 1;
        float invLength = 1.0f;
        float gainL = 1.0f, gainR = 1.0f;
        int window = 0;
        float tiltExp = 1.0f;
    };

    void spawn (const SampleData& s);
    static float readFrame (const SampleData& s, int ch, double pos);

    std::array<Grain, 64> grains;
    Params prm;
    float note = 60.0f;
    double sr = 48000.0;
    double countdown = 0.0;
    double scanOffset = 0.0;
    float lastStart = 0.0f;
    int nextStream = 0;
    Rng rng;
};
} // namespace pg
