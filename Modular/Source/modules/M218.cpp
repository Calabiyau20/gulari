#include "M218.h"

namespace wc
{
M218::M218 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    portaP = apvts.getRawParameterValue (p + "porta"); arpP = apvts.getRawParameterValue (p + "arp");
    rateP = apvts.getRawParameterValue (p + "rate"); addP = apvts.getRawParameterValue (p + "add");
    for (int i = 0; i < 4; ++i) presetP[i] = apvts.getRawParameterValue (p + "preset" + juce::String (i + 1));
    selP = apvts.getRawParameterValue (p + "sel");
}

void M218::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }

void M218::reset()
{
    nHeld = 0; pulseLeft = stripPulseLeft = presetPulseLeft = 0; pressure = midiPressure = 0.0f; arpPhase = 0.0; arpIdx = 0;
}

void M218::beginBlock (int)
{
    porta = portaP->load(); arp = (int) std::lround (arpP->load()); rate = rateP->load(); add = (int) std::lround (addP->load());
    for (int i = 0; i < 4; ++i) preset[i] = 10.0f * presetP[i]->load();
    sel = juce::jlimit (0, 3, (int) std::lround (selP->load()));
}

void M218::addHeld (float v) noexcept
{
    for (int i = 0; i < nHeld; ++i) if (std::fabs (held[i] - v) < 0.01f) return;
    if (nHeld < 16) held[nHeld++] = v;
    std::sort (held, held + nHeld);
}

void M218::removeHeld (float v) noexcept
{
    for (int i = 0; i < nHeld; ++i)
        if (std::fabs (held[i] - v) < 0.01f) { for (int k = i; k < nHeld - 1; ++k) held[k] = held[k + 1]; --nHeld; return; }
}

void M218::play (float v) noexcept
{
    pitchTarget = v;
    pulseLeft = (int) (0.005f * fs);
    ledPulse.store (1.0f, std::memory_order_relaxed);
}

void M218::handleMidi (const juce::MidiMessage& m) noexcept
{
    if (m.isNoteOn())
    {
        const float v = (float) (m.getNoteNumber() - 36) * 0.1f;
        addHeld (v);
        midiPressure = 10.0f * m.getFloatVelocity();
        if (arp == 2) play (v);
    }
    else if (m.isNoteOff()) { removeHeld ((float) (m.getNoteNumber() - 36) * 0.1f); if (nHeld == 0) midiPressure = 0.0f; }
    else if (m.isChannelPressure()) midiPressure = 10.0f * (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch()) midiPressure = 10.0f * (float) m.getAfterTouchValue() / 127.0f;
}

void M218::tick (const float* in, float* out) noexcept
{
    // ---- touch plates (from the panel) ----
    const int k = touchKey.load (std::memory_order_relaxed);
    if (k != uiKeyWas)
    {
        if (uiKeyWas >= 0) removeHeld (1.2f + 0.1f * (float) uiKeyWas);
        if (k >= 0) { const float v = 1.2f + 0.1f * (float) k; addHeld (v); if (arp == 2) play (v); }
        uiKeyWas = k;
        ledKey.store (k, std::memory_order_relaxed);
    }
    const float touchP = k >= 0 ? 10.0f * touchPressure.load (std::memory_order_relaxed) : 0.0f;
    const float pTarget = juce::jmax (touchP, nHeld > 0 ? midiPressure : 0.0f);
    pressure += (pTarget - pressure) * (pTarget > pressure ? 0.02f : 0.004f);
    // ---- arpeggiator ----
    if (arp != 2 && nHeld > 0)
    {
        bool step = false;
        if (in[In_ArpIn] > 3.0f && ! arpInWas) step = true;
        arpInWas = in[In_ArpIn] > 3.0f;
        if (rate > 0.01f)
        {
            arpPhase += (double) (0.5f * std::pow (60.0f, rate)) / fs;     // .5 .. 30 steps per second
            if (arpPhase >= 1.0) { arpPhase -= 1.0; step = true; }
        }
        if (step)
        {
            arpIdx = arp == 1 ? rnd.nextInt (nHeld) : (arpIdx + 1) % nHeld;
            play (held[juce::jlimit (0, nHeld - 1, arpIdx)]);
        }
    }
    else arpInWas = in[In_ArpIn] > 3.0f;
    // ---- strip ----
    const bool st = stripTouch.load (std::memory_order_relaxed);
    if (st) strip = 10.0f * stripPos.load (std::memory_order_relaxed);
    if (st && ! stripWas) stripPulseLeft = (int) (0.005f * fs);
    stripWas = st;
    // ---- preset pads ----
    const int pad = padTouch.load (std::memory_order_relaxed);
    if (pad >= 0 && pad != padWas) { sel = pad; presetPulseLeft = (int) (0.005f * fs); }
    padWas = pad;
    ledSel.store (sel, std::memory_order_relaxed);
    // ---- pitch: portamento + add to pitch ----
    const float t = juce::jmax (0.0f, 3.0f * porta * porta + 0.3f * in[In_PortaCv]);
    const float a = t < 0.001f ? 1.0f : 1.0f - std::exp (-1.0f / (t * 0.25f * fs));
    pitch += (pitchTarget - pitch) * a;
    const float addV = add == 0 ? 1.2f * (float) sel : (add == 1 ? preset[sel] : 0.0f);   // 0 octaves, 1 preset, 2 none
    out[Out_Pitch] = pitch + addV;
    out[Out_Pressure] = pressure;
    out[Out_Pulse] = pulseLeft > 0 ? 10.0f : 0.0f;
    out[Out_Strip] = strip;
    out[Out_StripPulse] = stripPulseLeft > 0 ? 10.0f : 0.0f;
    out[Out_Preset] = preset[sel];
    out[Out_PresetPulse] = presetPulseLeft > 0 ? 10.0f : 0.0f;
    if (pulseLeft > 0) --pulseLeft;
    if (stripPulseLeft > 0) --stripPulseLeft;
    if (presetPulseLeft > 0) --presetPulseLeft;
    ledPulse.store (ledPulse.load (std::memory_order_relaxed) * (1.0f - 1.0f / (0.05f * fs)), std::memory_order_relaxed);
    ledStrip.store (strip / 10.0f, std::memory_order_relaxed);
}

float M218::led (int id) const noexcept
{
    if (id == Led_Pulse) return ledPulse.load (std::memory_order_relaxed);
    if (id >= Led_Pad0 && id < Led_Pad0 + 4) return ledSel.load (std::memory_order_relaxed) == id - Led_Pad0 ? 1.0f : 0.0f;
    if (id >= Led_Key0 && id < Led_Key0 + kKeys) return ledKey.load (std::memory_order_relaxed) == id - Led_Key0 ? 1.0f : 0.0f;
    if (id >= Led_Strip0 && id < Led_Strip0 + 10)
    {
        const float s = ledStrip.load (std::memory_order_relaxed) * 9.0f;
        return std::fabs (s - (float) (id - Led_Strip0)) < 0.6f ? 1.0f : 0.0f;
    }
    return 0.0f;
}
} // namespace wc
