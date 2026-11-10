#pragma once
// KB-225 MIDI Interface: MIDI -> control voltages for the rack, monophonic, last-note priority.
// Pitch is 1.2 V/oct (0 V = C2, MIDI note 36), gates and pulses 0 / +10 V, other controls 0..10 V.
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class MidiInterface : public Module
{
public:
    enum Out { Out_Pitch, Out_Gate, Out_Pulse, Out_Vel, Out_Press, Out_Mod, Out_Bend, Out_Pitch2, Out_Count };

    explicit MidiInterface (juce::AudioProcessorValueTreeState& apvts);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;

    void handle (const juce::MidiMessage& m) noexcept;
    std::atomic<bool> gateLed { false };

private:
    std::atomic<float>* glide = nullptr;
    std::atomic<float>* bendRange = nullptr;
    std::atomic<float>* transpose = nullptr;
    float fs = 48000.0f;
    std::array<int, 32> stack {};
    int stackSize = 0;
    float targetV = 0.0f, pitchV = 0.0f, glideK = 1.0f;
    float velocity = 0.0f, pressure = 0.0f, mod = 0.0f, bend = 0.0f, bendSt = 2.0f, transOct = 0.0f;
    int pulseLeft = 0;
    bool haveNote = false;
    void updateTarget() noexcept;
};
} // namespace wc
