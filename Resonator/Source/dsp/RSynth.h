#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../RParams.h"
#include "CYMath.h"
#include "dsp/Building.h"   // pg::Adsr, pg::Lfo, pg::VoiceFilter
#include <array>
#include <atomic>
#include <vector>

namespace cyr
{
constexpr int kCtrl = 16;   // samples between modulation updates
constexpr int kMaxVoices = 16, kMaxUni = 4;
inline int numDests() { return (int) modDestParams().size() + 1; }

struct RGlobal
{
    float lfo[kLfos] {};    // free-running LFO values (-1..1)
    float lfoPhase[kLfos] {};
    float modWheel = 0, aftertouch = 0, pitchBend = 0;
    double bpm = 120.0, beat = 0.0;
    bool playing = false;
    float oscGain[kOscs] { 1, 1, 1, 1 };   // loudness match per oscillator
};

class RVoice
{
public:
    void prepare (double sampleRate);
    void start (int note, float velocity, float fromNote, const float* p, uint64_t order, uint32_t seed);
    void release();
    void kill() { amp.kill(); for (auto& e : env) e.kill(); }
    bool isActive() const { return amp.active(); }
    bool isHeld() const { return held; }
    float level() const { return amp.current(); }
    void render (float* L, float* R, int n, const float* p, const RGlobal& g);

    int note = -1;
    uint64_t order = 0;
    // display: modulated value of every destination (0..1, -1 when not modulated), source values
    std::array<float, 160> liveNorm {};
    float srcVal[MS_Count] {};
    float orbitPhase[kOscs] {};

private:
    void control (const float* p, const RGlobal& g);
    void setupOsc (int o, const float* e, const RGlobal& g);

    double sr = 48000.0;
    bool held = false;
    float vel = 1.0f, randomVal = 0.0f, pitch = 60.0f, targetPitch = 60.0f, glideCoef = 0.0f;
    float sinceOn = 0.0f;
    pg::Adsr amp, env[kEnvs];
    pg::Lfo lfo[kLfos];
    pg::VoiceFilter filter;
    std::array<float, kNumR> eff {};     // parameters after modulation
    std::array<float, 160> offs {};
    int ctrlLeft = 0;
    std::array<float, 32> lastSet {};   // envelope / filter settings last applied
    int prevTouched[kSlots] {}, prevCount = 0;

    struct Osc
    {
        bool on = false;
        OrbitShape shape;
        int uni = 1;
        float ph[kMaxUni] {}, inc[kMaxUni] {}, layerOff[kMaxUni] {};
        float spin = 0.0f, spinInc = 0.0f;
        float gL = 0.0f, gR = 0.0f;      // level, pan and loudness
        float fmNorm = 1.0f;
        float dcL = 0.0f, dcR = 0.0f;
        float alpha = 0.0f;
        bool first = true;
    };
    Osc osc[kOscs];
    float prevOut[kOscs] {};
    float fm[kOscs][kOscs] {}, pd[kOscs][kOscs] {};
    float dcCoef = 0.001f;
    float ampVel = 1.0f;
    float outGain = 1.0f;
};

class RSynth
{
public:
    void prepare (double sampleRate, int maxBlock);
    void render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const float* p,
                 const juce::AudioPlayHead::PositionInfo* pos);
    void allNotesOff();

    RGlobal global;
    std::atomic<int> activeVoices { 0 }, lastNote { -1 }, noteOnCounter { 0 };
    // newest voice, published for the UI
    std::array<std::atomic<float>, 160> liveNorm {};
    std::array<std::atomic<float>, MS_Count> liveSrc {};
    std::array<std::atomic<float>, kOscs> liveOrbit {};

private:
    void handleMidi (const juce::MidiMessage&, const float* p);
    void noteOn (int note, float vel, const float* p);
    void noteOff (int note);
    void updateGlobal (const float* p, int n, const juce::AudioPlayHead::PositionInfo* pos);
    void updateLoudness (const float* p);

    double sr = 48000.0;
    RVoice voices[kMaxVoices];
    uint64_t orderCounter = 0;
    float lastPitch = -1.0f;
    std::vector<int> heldStack;
    bool sustain = false;
    std::vector<int> sustained;
    pg::Lfo globalLfo[kLfos];
    std::array<float, 16> loudKey[kOscs] {};
    juce::Random rnd;
};
} // namespace cyr
