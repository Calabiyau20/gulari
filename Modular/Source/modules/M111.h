#pragma once
// LF-111 Classic LFO - a Calabi Yau original. Eight waves; the speed free (Hz or seconds), synced to the host's tempo
// (straight, dotted, triplet) or following a clock at CLOCK IN; the phase restarts on RETRIG IN and, with NOTE
// RETRIGGER on, on every MIDI note. LFO OUT x2 and INV OUT: -10..+10 V (bipolar) or 0..+10 V (unipolar), times
// AMOUNT; SYNC OUT: a 5 ms pulse on every new cycle. RATE: see lfoHz / lfoStep in WcParams.h.
#include "../rack/Module.h"
#include "../WcParams.h"
#include "Dsp.h"

namespace wc
{
class M111 : public Module
{
public:
    enum In { In_RateCv, In_Clock, In_Retrig, In_PhaseCv, In_AmountCv, In_Count };
    enum Out { Out_Lfo, Out_Lfo2, Out_Inv, Out_Sync, Out_Count };
    enum Mode { FreeHz, FreeSec, Sync, SyncDotted, SyncTriplet };
    enum Clock { ClockNone, ClockWaiting, ClockLocked, ClockLost };   // CLOCK IN: no cable, no period yet, locked, no pulses
    // LEDs: 0..4 the mode buttons (40 % while a clock sets the speed), 5 NOTE RETRIG, 6 the speed (follows LFO OUT),
    // 10..17 the waves. The buttons' lights read the knobs themselves: they answer with the audio stopped too.
    float led (int id) const noexcept override;

    M111 (const Params& params, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;

    void noteOn() noexcept { noteHit = true; }              // the rack, on every MIDI note-on (audio thread)
    std::atomic<double> hostBpm { 120.0 };                 // set by the processor every block, as on the 250
    std::atomic<bool> hostTempo { false };                 // the host gave a tempo on the last block
    std::atomic<int> clockState { ClockNone };
    std::atomic<float> cycleSecs { 1.0f };                 // the cycle as it runs, in seconds (the readout)

private:
    std::atomic<float>* rateP = nullptr; std::atomic<float>* modeP = nullptr; std::atomic<float>* amountP = nullptr;
    std::atomic<float>* phaseP = nullptr; std::atomic<float>* polarityP = nullptr; std::atomic<float>* retrigP = nullptr;
    std::atomic<float>* waveP = nullptr;
    std::atomic<float> speedLed { 0.0f };
    float fs = 48000.0f;
    float rateK = 0.5f, amountK = 1.0f, phaseK = 0.0f;
    int modeV = 0, waveV = 0;
    bool unipolar = false, retrigOn = false;
    double bpm = 120.0;
    double acc = 0.0;                     // the phase as it runs, 0..1 (PHASE and its c.v. go on top)
    float phiWas = 0.0f;
    bool wrapped = false, retrigWas = false, clockWas = false, noteHit = false;
    int clockPeriod = 0, sinceClock = 0, pulses = 0;   // samples; pulses since the first one (or the last restart)
    bool clockSeen = false;
    int syncLeft = 0, sinceSync = 1 << 30;
    float sPrev = 0.0f, sNext = 0.0f, wPrev = 0.0f, wNext = 0.0f;   // stepped / smooth random, random walk
    juce::Random rnd;                     // its own seed: two LFOs never draw the same
    void pick() noexcept;
};
} // namespace wc
