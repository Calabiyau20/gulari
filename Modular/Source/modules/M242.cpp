#include "M242.h"

namespace wc
{
M242::M242 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    const char rows[3] = { 'a', 'b', 'c' };
    for (int r = 0; r < 3; ++r)
        for (int s = 0; s < kSteps; ++s)
        {
            cell[r][s] = apvts.getRawParameterValue (p + juce::String::charToString ((juce::juce_wchar) rows[r]) + juce::String (s + 1));
            jassert (cell[r][s] != nullptr);
        }
    lenP = apvts.getRawParameterValue (p + "len");
    cvP = apvts.getRawParameterValue (p + "cv");
    perP = apvts.getRawParameterValue (p + "period");
    rowP = apvts.getRawParameterValue (p + "row");
    colP = apvts.getRawParameterValue (p + "col");
}

void M242::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }

void M242::reset()
{
    phase = 0.0;
    step.store (-1);
    pAll = pAlt[0] = pAlt[1] = pRow[0] = pRow[1] = pRow[2] = 0;
    running = true;
}

void M242::beginBlock (int)
{
    for (int r = 0; r < 3; ++r)
        for (int s = 0; s < kSteps; ++s) cellOn[r][s].store (cell[r][s]->load() > 0.5f, std::memory_order_relaxed);
    len.store (juce::jlimit (1, kSteps, (int) std::lround (lenP->load())), std::memory_order_relaxed);
    cvK = cvP->load(); perK = perP->load();
}

void M242::advance() noexcept
{
    int s = step.load (std::memory_order_relaxed) + 1;
    if (s >= len.load (std::memory_order_relaxed)) s = 0;
    step.store (s, std::memory_order_relaxed);
    const int w = (int) (0.001f * fs);   // 1 ms pulses
    for (int r = 0; r < 3; ++r) if (cellOn[r][s].load (std::memory_order_relaxed)) pRow[r] = w;
}

void M242::clockTick (bool advanceMatrix) noexcept
{
    const int w = (int) (0.001f * fs);
    pAll = w;
    pAlt[alt ? 1 : 0] = w;
    alt = ! alt;
    if (advanceMatrix) advance();
    tickFlash.store (1.0f, std::memory_order_relaxed);
}

void M242::tick (const float* in, float* out) noexcept
{
    auto rising = [] (bool now, bool& was) { const bool r = now && ! was; was = now; return r; };
    if (rising (in[In_Start] > 3.0f, startWas) | rising (btnStart.load (std::memory_order_relaxed), bStartWas)) { if (! running) phase = 0.999999; running = true; }
    if (rising (in[In_Stop] > 3.0f, stopWas) | rising (btnStop.load (std::memory_order_relaxed), bStopWas)) running = false;
    const bool extClock = isPatched (In_Clk);
    if (rising (btnSingle.load (std::memory_order_relaxed), bSingleWas)) clockTick (! extClock);
    if (rising (in[In_Rst] > 3.0f, rstWas)) step.store (-1, std::memory_order_relaxed);   // next stage is the first one
    if (extClock && rising (in[In_Clk] > 3.0f, clkWas)) advance();

    // internal clock: c.v. scales the rate (attenuverter), the knob end is OFF
    const bool off = perK > 0.92f;
    if (running && ! off)
    {
        const float rateMul = std::exp2 (cvK * in[In_Period] * 0.5f);         // +-5 octaves over 10 V
        const float period = juce::jlimit (0.0005f, 60.0f, periodSec (perK) / rateMul);
        phase += 1.0 / (period * fs);
        if (phase >= 1.0) { phase -= std::floor (phase); clockTick (! extClock); }
    }
    out[Out_Ramp] = 10.0f * (float) phase;
    out[Out_All] = pAll > 0 ? 10.0f : 0.0f;
    out[Out_Alt1] = pAlt[0] > 0 ? 10.0f : 0.0f;
    out[Out_Alt2] = pAlt[1] > 0 ? 10.0f : 0.0f;
    for (int r = 0; r < 3; ++r) out[Out_A + r] = pRow[r] > 0 ? 10.0f : 0.0f;
    if (pAll > 0) --pAll;
    for (auto& p : pAlt) if (p > 0) --p;
    for (auto& p : pRow) if (p > 0) --p;
    runningLed.store (running && ! off, std::memory_order_relaxed);
    tickFlash.store (tickFlash.load (std::memory_order_relaxed) * (1.0f - 1.0f / (0.05f * fs)), std::memory_order_relaxed);
}

float M242::led (int id) const noexcept
{
    if (id == Led_Stop) return runningLed.load (std::memory_order_relaxed) ? 0.0f : 1.0f;
    if (id == Led_Start) return runningLed.load (std::memory_order_relaxed) ? juce::jmax (0.35f, tickFlash.load (std::memory_order_relaxed)) : 0.0f;
    if (id >= Led_Matrix && id < Led_Matrix + 4 * kSteps)
    {
        const int r = (id - Led_Matrix) / kSteps, c = (id - Led_Matrix) % kSteps;
        const int s = step.load (std::memory_order_relaxed);
        const bool cursor = r == (int) std::lround (rowP->load()) - 1 && c == (int) std::lround (colP->load()) - 1;
        const int n = len.load (std::memory_order_relaxed);
        float v;
        if (r == 0) v = c == n - 1 ? 0.85f : 0.0f;                           // row n: the last stage of the cycle
        else v = cellOn[r - 1][c].load (std::memory_order_relaxed) ? 0.6f : 0.0f;
        if (c == s && c < n) v = v > 0.0f ? 1.0f : 0.22f;                   // playhead
        if (cursor) v = (juce::Time::getMillisecondCounter() / 250) % 2 ? 1.0f : v * 0.3f;   // edit cursor blinks
        return v;
    }
    return 0.0f;
}
} // namespace wc
