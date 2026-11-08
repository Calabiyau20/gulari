#pragma once
#include "../Params.h"
#include "Common.h"
#include "Building.h"
#include "GranularOsc.h"
#include "Wavetable.h"
#include "MutableWrappers.h"

namespace pg
{
// Values shared by every voice for one control block
struct GlobalMod
{
    float lfo[4] {};       // free-running LFOs (-1..1)
    float lfoPhase[4] {};
    float chaos[2] {};     // -1..1
    float macro[4] {};     // 0..1
    float seq[4] {};       // lane outputs
    float modWheel = 0, aftertouch = 0, pitchBend = 0;   // pitchBend -1..1
    double bpm = 120.0;
};

struct VoiceContext
{
    const float* p = nullptr;        // parameter cache (denormalised values, PID index)
    const SampleData* sample = nullptr;
    const Wavetable* table[2] {};
    const GlobalMod* global = nullptr;
    GrainViz* viz = nullptr;         // only set for the voice that feeds the display
};

// How each modulation destination maps an amount of 1.0
float modDepth (int dst);

class Voice
{
public:
    void prepare (double sampleRate, int index);
    void start (int midiNote, float velocity, bool retrigger, const VoiceContext& ctx);
    void moveTo (int midiNote, const VoiceContext& ctx);   // legato pitch change
    void release();
    void kill();
    bool isActive() const { return amp.active(); }
    bool isHeld() const { return held; }
    int getNote() const { return note; }
    uint64_t getAge() const { return age; }
    void render (float* outL, float* outR, int numSamples, const VoiceContext& ctx);

private:
    void controlUpdate (const VoiceContext& ctx);
    float source (int src) const;

    double sr = 48000.0;
    int index = 0;
    int note = 60;
    float velocity = 1.0f, randomValue = 0.0f;
    float currentPitch = 60.0f, targetPitch = 60.0f, glideCoef = 0.0f;
    bool held = false;
    uint64_t age = 0;
    static inline uint64_t ageCounter = 0;

    Adsr amp, fenv, menv;
    Lfo lfo[4];
    Lfo grainLfo;                 // Grain Mod 1: periodic
    float grainRnd = 0.0f, grainRndTarget = 0.0f, grainRndPhase = 1.0f;   // Grain Mod 2: smooth random
    Rng grainRng;
    float lfoVal[4] {};
    float mod[MD_Count] {};

    GranularOsc grain;
    WavetableOsc wt[2];
    BasicOsc modOsc, subOsc;
    Noise noise;
    PlaitsSource plaits;
    VoiceFilter filter;
    Bite bite;

    // per control block derived values
    GranularOsc::Params grainParams;
    float wtPos[2] {}, wtFreqMul[2] {}, wtLevel[2] {};
    float wtGainL[2] {}, wtGainR[2] {};
    float grLevel = 0, grGainL = 1, grGainR = 1;
    float modFreqMul = 1, modLevel = 0, subLevel = 0, noiseLevel = 0;
    float plLevel = 0, plGainL = 1, plGainR = 1;
    float fm[5][3] {};   // [src a,b,m,g,n][dst a,b,m]
    bool thruZero = false;
    float tz[3] {};      // thru-zero accumulated phase
    float prevA = 0, prevB = 0, prevM = 0, prevG = 0, prevN = 0;
    float volL = 1, volR = 1;
    float cutoffBase = 1000.0f;
    int ctrlCounter = 0;
    float dcX[2] {}, dcY[2] {};
    bool grOn = false, wtOn[2] {}, plOn = false, bitePost = false;
    int modWave = 0, subWave = 0, noiseType = 0;
};
} // namespace pg
