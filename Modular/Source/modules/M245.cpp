#include "M245.h"

namespace wc
{
M245::M245 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    const char rows[4] = { 'a', 'b', 'c', 'd' };
    for (int r = 0; r < 4; ++r)
        for (int s = 0; s < 5; ++s)
        {
            knob[r][s] = apvts.getRawParameterValue (p + juce::String::charToString ((juce::juce_wchar) rows[r]) + juce::String (s + 1));
            jassert (knob[r][s] != nullptr);
        }
    for (int i = 0; i < 4; ++i) sw[i] = apvts.getRawParameterValue (p + "sw" + juce::String (i + 1));
    lenP = apvts.getRawParameterValue (p + "len");
    tcvP = apvts.getRawParameterValue (p + "tcv");
    timeP = apvts.getRawParameterValue (p + "time");
}

void M245::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }

void M245::reset()
{
    stage = 0;
    phase = 0.0;
    spikeLeft = gateLeft = 0;
    running = true;
}

void M245::beginBlock (int)
{
    for (int r = 0; r < 4; ++r)
        for (int s = 0; s < 5; ++s) vals[r][s] = 10.0f * knob[r][s]->load();
    for (int i = 0; i < 4; ++i) swOn[i] = sw[i]->load() > 0.5f;
    lenK = lenP->load(); tcvK = tcvP->load(); timeK = timeP->load();
}

void M245::firePulse (float period) noexcept
{
    // pulse length: knob + c.v. (10 V = 100 %)
    spikeLeft = juce::jmax (1, (int) (0.0001f * fs));                          // ~100 us at 10 V
    const float len = dsp::clampf (lenK + lenCv / 10.0f, 0.0f, 0.98f);
    gateLeft = (int) (len * period * fs);                                       // then a 5 V gate
    pulsedThisSample = true;
    startFlash = 1.0f;
}

void M245::tick (const float* in, float* out) noexcept
{
    pulsedThisSample = false;
    lenCv = in[In_LenCv];
    // ---- pulser: start / stop (buttons and pulses); stop while stopped = a single pulse ----
    auto rising = [] (bool now, bool& was) { const bool r = now && ! was; was = now; return r; };
    const bool startIn = rising (in[In_Start] > 3.0f, startWas) | rising (btnStart.load (std::memory_order_relaxed), btnStartWas);
    const bool stopIn = rising (in[In_Stop] > 3.0f, stopWas) | rising (btnStop.load (std::memory_order_relaxed), btnStopWas);
    const float k = dsp::clampf (timeK + tcvK * in[In_TimeCv] / 10.0f, -0.3f, 1.2f);     // c.v. reaches ~1 ms .. ~50 s
    const float period = juce::jlimit (0.001f, 50.0f, periodSec (k));
    if (startIn && ! running) { running = true; phase = 0.0; firePulse (period); }
    if (stopIn) { if (running) running = false; else firePulse (period); }
    if (running)
    {
        phase += 1.0 / (period * fs);
        if (phase >= 1.0) { phase -= std::floor (phase); firePulse (period); }
    }
    float pulse = 0.0f;
    if (spikeLeft > 0) { pulse = 10.0f; --spikeLeft; }
    else if (gateLeft > 0) { pulse = 5.0f; --gateLeft; }
    out[Out_Pulse] = pulse;
    out[Out_Ref] = 10.0f * (float) (1.0 - phase);

    // ---- stage addressing ----
    const bool advPatched = isPatched (In_Advance);
    const bool adv = advPatched ? rising (in[In_Advance] > 3.0f, advWas) : pulsedThisSample;   // pulser normalled to advance
    const float analog = in[In_Analog];
    if (analog > 0.75f)
    {
        // analog addressing overrides advance and stage select
        stage = analog < 2.5f ? 0 : (analog < 4.25f ? 1 : (analog < 6.0f ? 2 : (analog < 7.75f ? 3 : 4)));
    }
    else
    {
        if (adv) stage = (stage < 4 && swOn[stage]) ? stage + 1 : 0;   // switch k off: back to stage 1 after stage k
        for (int s = 0; s < 5; ++s)
            if (rising (in[In_Sel1 + s] > 3.0f, selWas[s])) stage = s;
    }
    for (int s = 0; s < 5; ++s)
    {
        out[Out_Gate1 + s] = stage == s ? 10.0f : 0.0f;
        leds[(size_t) (Led_Stage1 + s)].store (stage == s ? 1.0f : 0.0f, std::memory_order_relaxed);
    }
    for (int r = 0; r < 4; ++r) out[Out_A + r] = vals[r][stage];

    startFlash *= 1.0f - 1.0f / (0.06f * fs);
    leds[Led_Stop].store (running ? 0.0f : 1.0f, std::memory_order_relaxed);
    leds[Led_Start].store (running ? juce::jmax (0.35f, startFlash) : 0.0f, std::memory_order_relaxed);
}
} // namespace wc
