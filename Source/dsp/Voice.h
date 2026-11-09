#pragma once
#include "../Params.h"
#include "Common.h"
#include "Building.h"
#include "GranularOsc.h"
#include "Wavetable.h"
#include "MutableWrappers.h"
#include "Spectral.h"
#include "OneShot.h"

namespace pg
{
// Values shared by every voice for one control block
struct GlobalMod
{
    float lfo[4] {};       // free-running LFOs (-1..1)
    float lfoPhase[4] {};
    float chaos[4] {};     // -1..1: Chaos 1, Lorenz X, Y, Z
    float seq[8] {};       // lane outputs
    float modWheel = 0, aftertouch = 0, pitchBend = 0;   // pitchBend -1..1
    double bpm = 120.0;
    double beat = 0.0;     // musical position at the start of the block
};

struct VoiceContext
{
    const float* p = nullptr;        // parameter cache (denormalised values, PID index)
    const SampleData* sample = nullptr;
    const Wavetable* table[3] {};
    const SpectralTable* spectral = nullptr;
    const ShotData* shot = nullptr;
    const GlobalMod* global = nullptr;
    GrainViz* viz = nullptr;         // only set for the voice that feeds the display
};

// How each modulation destination maps an amount of 1.0
float modDepth (int dst);

// Sources that swing -1..1 by nature; the rest run 0..1
inline bool isBipolarSource (int s, const float* p)
{
    switch (s)
    {
        case MS_Lfo1: case MS_Lfo2: case MS_Lfo3: case MS_Lfo4: case MS_Chaos1: case MS_Chaos2: case MS_Chaos2Y: case MS_Chaos2Z:
        case MS_Note: case MS_PitchBend: case MS_Random: return true;
        case MS_Seq1: case MS_Seq2: case MS_Seq3: case MS_Seq4: case MS_Seq5: case MS_Seq6: case MS_Seq7: case MS_Seq8:
            return (int) p[seq1_mode + (s - MS_Seq1) * (seq2_on - seq1_on)] == 1;
        default: return false;
    }
}

// Matrix polarity: 0 native, 1 force unipolar (0..1), 2 force bipolar (-1..1)
inline float applyPolarity (float v, int src, int pol, const float* p)
{
    if (pol == 0) return v;
    const bool bi = isBipolarSource (src, p);
    if (pol == 1) return bi ? 0.5f * (v + 1.0f) : v;
    return bi ? v : 2.0f * v - 1.0f;
}

class Voice
{
public:
    void prepare (double sampleRate, int index);
    void start (int midiNote, float velocity, bool retrigger, const VoiceContext& ctx);
    void moveTo (int midiNote, const VoiceContext& ctx);   // legato pitch change
    void release();
    void kill();
    bool isActive() const { return amp.active() || shot.isPlaying(); }
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

    Adsr amp, fenv, menv, menvX[3];   // menvX = Mod Env 2..4
    Lfo lfo[4];
    Lfo grainLfo;                 // Grain Mod 1: periodic
    float grainRnd = 0.0f, grainRndTarget = 0.0f, grainRndPhase = 1.0f;   // Grain Mod 2: smooth random
    Rng grainRng;
    float lfoVal[4] {};
    float mod[MD_Count] {};

    GranularOsc grain;
    WavetableOsc wt[3];
    BasicOsc subOsc;
    SpectralOsc spectral;
    SpectralOsc::Params spParams;
    OneShotPlayer shot;
    Noise noise;
    PlaitsSource plaits;
    VoiceFilter filter;
    Bite bite;

    // per control block derived values
    GranularOsc::Params grainParams;
    float wtPos[3] {}, wtFreqMul[3] {}, wtLevel[3] {};
    float spFreqMul = 1, spLevel = 0, spGainL = 1, spGainR = 1;
    float smpLevel = 0, smpGainL = 1, smpGainR = 1;
    float grLevel = 0, grGainL = 1, grGainR = 1;
    float subLevel = 0, noiseLevel = 0;
    float plLevel = 0, plGainL = 1, plGainR = 1;
    float fm[6][4] {};   // [src a,b,c,spectral,grain,noise][dst a,b,c,spectral]
    int fmType[4] {};    // per destination: choices::fmModes
    float fmDepth[4] {}; // summed amounts per destination (for AM / ring depth)
    float tz[4] {};      // thru-zero accumulated phase
    float prevA = 0, prevB = 0, prevC = 0, prevS = 0, prevG = 0, prevN = 0;
    bool rtFilter[8] { true, true, true, true, true, true, true, false };
    bool spDist = true;
    float volL = 1, volR = 1;
    float cutoffBase = 1000.0f;
    int ctrlCounter = 0;
    float dcX[2] {}, dcY[2] {};
    bool grOn = false, wtOn[3] {}, plOn = false, spOn = false, smpOn = false, bitePost = false;
    int subWave = 0, noiseType = 0;
};
} // namespace pg
