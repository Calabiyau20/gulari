#pragma once
// KB-218 Touch Activated Voltage Source - emulation of the Buchla 218 / 218e (LEM218).
// 29 touch plates (2 1/3 octaves): each touch gives a pulse, a pressure c.v. (0 - 10 V) and a pitch c.v. (1.2 V/oct,
// 0.1 V a half step; the lowest key is C = 1.2 V). Portamento slope (knob + c.v.). Arpeggiator over the held keys
// (ascending / random / none) at the arpeggiation rate (0 = only pulses at the input advance it). Strip: a
// position-sensitive plate, 0 - 10 V that holds, with its own pulse. Preset voltage source: four pads select one of four
// knobs (0 - 10 V) at the output, with a pulse. ADD TO PITCH: none / octaves (pads 1-4 = +0..+3 octaves) / preset (the
// selected preset voltage). Also plays from MIDI (notes, velocity -> pressure, aftertouch).
#include "../rack/Module.h"
#include "Dsp.h"

namespace wc
{
class M218 : public Module
{
public:
    static constexpr int kKeys = 29;
    enum In { In_PortaCv, In_ArpIn, In_Count };
    enum Out { Out_Pulse, Out_Pressure, Out_Pitch, Out_Strip, Out_StripPulse, Out_Preset, Out_PresetPulse, Out_Count };
    enum Led { Led_Pulse, Led_Pad0 = 10, Led_Key0 = 20, Led_Strip0 = 60, Led_Count = 80 };

    M218 (juce::AudioProcessorValueTreeState& apvts, const juce::String& prefix);
    void prepare (double sampleRate) override;
    void beginBlock (int numSamples) override;
    void tick (const float* in, float* out) noexcept override;
    void reset() override;
    float led (int id) const noexcept override;
    void handleMidi (const juce::MidiMessage& m) noexcept;    // audio thread

    // UI (message thread) -> audio
    std::atomic<int> touchKey { -1 };
    std::atomic<float> touchPressure { 0.0f };
    std::atomic<bool> stripTouch { false };
    std::atomic<float> stripPos { 0.0f };
    std::atomic<int> padTouch { -1 };

private:
    std::atomic<float>* portaP = nullptr; std::atomic<float>* arpP = nullptr; std::atomic<float>* rateP = nullptr; std::atomic<float>* addP = nullptr;
    std::atomic<float>* presetP[4] {}; std::atomic<float>* selP = nullptr;
    float fs = 48000.0f, porta = 0.0f, rate = 0.0f, preset[4] {};
    int arp = 2, add = 2;
    // held notes (as pitch volts) from the plates and MIDI
    float held[16] {}; int nHeld = 0;
    int uiKeyWas = -1, padWas = -1; bool stripWas = false, arpInWas = false;
    float pitchTarget = 1.2f, pitch = 1.2f, pressure = 0.0f, midiPressure = 0.0f;
    float strip = 0.0f;
    int sel = 0;
    double arpPhase = 0.0; int arpIdx = 0;
    int pulseLeft = 0, stripPulseLeft = 0, presetPulseLeft = 0;
    juce::Random rnd { 2180 };
    std::atomic<int> ledKey { -1 }; std::atomic<float> ledPulse { 0.0f }, ledStrip { 0.0f }; std::atomic<int> ledSel { 0 };
    void addHeld (float v) noexcept;
    void removeHeld (float v) noexcept;
    void play (float v) noexcept;
};
} // namespace wc
