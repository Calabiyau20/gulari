#pragma once
#include "Voice.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>

namespace pg
{
// Step data for the 4 modulation lanes and the arpeggiator (edited by the UI, read by audio)
struct StepData
{
    static constexpr int kLanes = 8, kSteps = 32;
    std::array<std::atomic<float>, kLanes * kSteps> seq {};
    std::array<std::atomic<bool>, kLanes * kSteps> seqLock {};
    std::array<std::atomic<bool>, kSteps> arpOn {}, arpTie {};
    std::array<std::atomic<int>, kSteps> arpTranspose {}, arpRatchet {};
    std::array<std::atomic<float>, kSteps> arpVel {}, arpProb {}, arpGate {};
    std::array<std::atomic<int>, kLanes> seqPlayStep {};
    std::atomic<int> arpPlayStep { -1 };

    StepData() { reset(); }
    void reset();
    juce::ValueTree toTree() const;
    void fromTree (const juce::ValueTree& t);

};

// Synth engine: arpeggiator -> voice allocation -> voices, plus the global modulators.
class Synth
{
public:
    static constexpr int kMaxVoices = 16;

    void prepare (double sampleRate, int maxBlock);
    void render (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, const float* params,
                 const SampleData* sample, const Wavetable* const* tables, const SpectralTable* spectral,
                 const ShotData* shot, const juce::AudioPlayHead::PositionInfo* pos);
    void allNotesOff();

    StepData steps;
    GrainViz viz;
    GlobalMod global;
    std::atomic<int> lastNote { 48 };
    std::atomic<int> noteOnCounter { 0 };   // incremented on every voice start (Rings strum)
    std::atomic<int> activeVoices { 0 };

private:
    void noteOn (int note, float vel);
    void noteOff (int note);
    void handleMidi (const juce::MidiMessage& m);
    void updateGlobalMods (int numSamples, double beatAtStart, double beatsPerSample);
    void runArp (int numSamples, double beatAtStart, double beatsPerSample, std::vector<std::pair<int, juce::MidiMessage>>& events);

    double sr = 48000.0;
    const float* p = nullptr;
    VoiceContext ctx;
    std::array<Voice, kMaxVoices> voices;
    std::vector<int> monoStack;   // held notes for mono / legato

    // arpeggiator
    std::vector<int> arpHeld;        // in press order
    std::vector<int> arpLatched;
    bool arpAllReleased = true;
    double arpNextBeat = 0.0;
    int arpStep = 0, arpCounter = 0, arpDir = 1;
    struct PendingOff { double beat; int note; };
    std::vector<PendingOff> arpOffs;
    std::vector<int> arpSounding;
    struct RatchetOn { double beat; int note; float vel; };
    std::vector<RatchetOn> arpRatchetOns;
    int arpNoteIndex = 0;
    int arpCounterNote (int c) const { return c > 0 ? arpNoteIndex % c : 0; }
    bool arpWasOn = false;

    // clocks
    double internalBeat = 0.0;
    bool wasPlaying = false;

    // global modulators
    Lfo gLfo[4];
    float chaos1State[3] { 0.1f, 0.0f, 0.0f };
    float lorenz[3] { -0.1f, 0.2f, 0.1f };
    float chaosSmooth[4] {};
    float seqSmooth[8] {};
    int quantize (int note) const;
    Rng rng;
};
} // namespace pg
