#include "MidiInterface.h"

namespace wc
{
MidiInterface::MidiInterface (juce::AudioProcessorValueTreeState& apvts)
{
    glide = apvts.getRawParameterValue ("midi_glide");
    bendRange = apvts.getRawParameterValue ("midi_bend");
    transpose = apvts.getRawParameterValue ("midi_trans");
}

void MidiInterface::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }

void MidiInterface::reset()
{
    stackSize = 0;
    haveNote = false;
    pulseLeft = 0;
    velocity = pressure = mod = bend = 0.0f;
    gateLed.store (false);
}

void MidiInterface::beginBlock (int)
{
    const float g = glide->load();
    const float seconds = 2.0f * g * g;                    // 0 .. 2 s, fine at the low end
    glideK = seconds < 0.0005f ? 1.0f : 1.0f - std::exp (-1.0f / (seconds * 0.25f * fs));
    bendSt = bendRange->load();
    transOct = std::round (transpose->load());
    updateTarget();
}

void MidiInterface::updateTarget() noexcept
{
    if (! haveNote) return;
    const int note = stack[(size_t) juce::jmax (0, stackSize - 1)];
    // 1.2 V/oct = 0.1 V per semitone, 0 V at C2
    targetV = ((float) (note - 36) + 12.0f * transOct + bend * bendSt) * 0.1f;
}

void MidiInterface::handle (const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn())
    {
        const int n = m.getNoteNumber();
        for (int i = 0; i < stackSize; ++i) if (stack[(size_t) i] == n) { std::move (stack.begin() + i + 1, stack.begin() + stackSize, stack.begin() + i); --stackSize; break; }
        if (stackSize < (int) stack.size()) stack[(size_t) stackSize++] = n;
        velocity = m.getFloatVelocity();
        const bool legato = haveNote;
        haveNote = true;
        updateTarget();
        if (! legato && glideK >= 1.0f) pitchV = targetV;
        pulseLeft = (int) (0.005f * fs);                     // 5 ms trigger pulse on every new note
    }
    else if (m.isNoteOff())
    {
        const int n = m.getNoteNumber();
        for (int i = 0; i < stackSize; ++i) if (stack[(size_t) i] == n) { std::move (stack.begin() + i + 1, stack.begin() + stackSize, stack.begin() + i); --stackSize; break; }
        if (stackSize == 0) haveNote = false;   // pitch holds the last note (like a keyboard's sample & hold)
        else updateTarget();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) { stackSize = 0; haveNote = false; }
    else if (m.isPitchWheel()) { bend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f; updateTarget(); }
    else if (m.isController() && m.getControllerNumber() == 1) mod = (float) m.getControllerValue() / 127.0f;
    else if (m.isChannelPressure()) pressure = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch()) pressure = (float) m.getAfterTouchValue() / 127.0f;
}

void MidiInterface::tick (const float*, float* out) noexcept
{
    pitchV += (targetV - pitchV) * glideK;
    out[Out_Pitch] = out[Out_Pitch2] = pitchV;
    out[Out_Gate] = haveNote ? 10.0f : 0.0f;
    out[Out_Pulse] = pulseLeft > 0 ? 10.0f : 0.0f;
    if (pulseLeft > 0) --pulseLeft;
    out[Out_Vel] = 10.0f * velocity;
    out[Out_Press] = 10.0f * pressure;
    out[Out_Mod] = 10.0f * mod;
    out[Out_Bend] = 5.0f + 5.0f * bend;
    gateLed.store (haveNote, std::memory_order_relaxed);
}
} // namespace wc
