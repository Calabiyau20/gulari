#include "M666.h"

namespace wc
{
M666::M666 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    vaP = apvts.getRawParameterValue (p + "va"); vbP = apvts.getRawParameterValue (p + "vb");
    hposP = apvts.getRawParameterValue (p + "hpos"); xgP = apvts.getRawParameterValue (p + "xgain");
    srcP = apvts.getRawParameterValue (p + "src"); lvlP = apvts.getRawParameterValue (p + "level");
    timeP = apvts.getRawParameterValue (p + "time"); modeP = apvts.getRawParameterValue (p + "mode");
    coupP = apvts.getRawParameterValue (p + "coupling"); powP = apvts.getRawParameterValue (p + "power");
    jassert (vaP && vbP && hposP && xgP && srcP && lvlP && timeP && modeP && coupP && powP);
}

void M666::prepare (double sampleRate)
{
    fs = (float) sampleRate;
    acA.set (4.0f, fs); acB.set (4.0f, fs);
    reset();
}

void M666::reset()
{
    sweeping = false; armed = true; singleDone = false; filled = 0; sinceTrig = 0.0;
    acA.reset(); acB.reset();
}

void M666::beginBlock (int)
{
    src = (int) std::lround (srcP->load());
    mode = (int) std::lround (modeP->load());
    coup = (int) std::lround (coupP->load());
    level = lvlP->load() * 5.0f;            // trigger level, +-5 V on the audio scale x 5
    tdiv = timePerDiv (timeP->load());
    power = powP->load() > 0.5f;
    bPatched.store (isPatched (In_B), std::memory_order_relaxed);
    zPatched.store (isPatched (In_ZAxis), std::memory_order_relaxed);
    if (rearm.exchange (false)) { singleDone = false; armed = true; }
}

void M666::publish() noexcept
{
    if (mode != 2 && sincePub < (int) (fs / 90.0f)) return;   // SINGLE always keeps its one sweep      // the screen refreshes at 30 Hz: no need to copy faster
    sincePub = 0;
    const juce::SpinLock::ScopedLockType sl (lock);
    const int slot = ready.load (std::memory_order_relaxed) == 0 ? 1 : 0;
    buf[slot] = work;
    ready.store (slot, std::memory_order_relaxed);
    seqNo.fetch_add (1, std::memory_order_relaxed);
}

bool M666::latest (Frame& dst, int& seq) const
{
    const int s = seqNo.load (std::memory_order_relaxed);
    if (s == seq) return false;
    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked()) return false;
    const int r = ready.load (std::memory_order_relaxed);
    if (r < 0) return false;
    dst = buf[r];
    seq = s;
    return true;
}

void M666::tick (const float* in, float* out) noexcept
{
    if (sincePub < (1 << 28)) ++sincePub;
    float a = in[In_A], b = in[In_B];
    out[Out_AThru] = a; out[Out_BThru] = b;
    // display coupling
    float da = a, db = b;
    if (coup == 0) { da = acA.process (a); db = acB.process (b); }
    else if (coup == 2) { da = db = 0.0f; }
    const float z = isPatched (In_ZAxis) ? dsp::clampf (in[In_ZAxis] / 10.0f, 0.0f, 1.0f) : 1.0f;

    if (! power) { out[Out_Sweep] = out[Out_Gate] = out[Out_Trig] = 0.0f; trigLed.store (false, std::memory_order_relaxed); return; }

    if (mode == 3)   // X-Y: a rolling window of the last kPoints points spanning ten divisions
    {
        const double step = juce::jmax (1.0, (double) tdiv * 10.0 * fs / kPoints);
        xyAcc += 1.0;
        if (xyAcc >= step)
        {
            xyAcc -= step;
            work.a[xyPos] = da; work.x[xyPos] = db; work.z[xyPos] = z; work.b[xyPos] = 0.0f;
            if (++xyPos >= kPoints)
            {
                xyPos = 0; work.n = kPoints; work.xy = true; work.dual = false;
                publish();
            }
        }
        out[Out_Sweep] = out[Out_Gate] = out[Out_Trig] = 0.0f;
        trigLed.store (true, std::memory_order_relaxed);
        return;
    }

    const bool ext = in[In_ExtTrig] > 3.0f;
    const bool extEdge = ext && ! trigWas; trigWas = ext;
    const bool aUp = da > level;
    const bool intEdge = aUp && ! aWas; aWas = aUp;
    const bool externalSweep = isPatched (In_SweepIn);

    if (externalSweep)
    {
        // the sweep input positions the beam: one frame every kPoints samples
        const float xs = dsp::clampf (in[In_SweepIn] / 10.0f, 0.0f, 1.0f);
        work.a[filled] = da; work.b[filled] = db; work.x[filled] = xs; work.z[filled] = z;
        if (++filled >= kPoints) { work.n = kPoints; work.xy = true; work.dual = isPatched (In_B); publish(); filled = 0; }
        out[Out_Sweep] = 10.0f * xs; out[Out_Gate] = 10.0f; out[Out_Trig] = 0.0f;
        trigLed.store (true, std::memory_order_relaxed);
        return;
    }

    if (! sweeping)
    {
        sinceTrig += 1.0 / fs;
        const bool trig = src == 0 ? intEdge : (src == 1 ? extEdge : true);
        const double tLen = (double) tdiv * 10.0 * std::exp2 (-dsp::clampf (in[In_TimeCv], -10.0f, 10.0f) * 0.3);
        const bool autoFire = mode == 0 && sinceTrig > juce::jmax (0.05, 2.0 * tLen);
        const bool allowed = mode != 2 || ! singleDone;
        if (allowed && armed && (trig || autoFire))
        {
            sweeping = true; sweepT = 0.0; sweepLen = tLen; sinceTrig = 0.0;
            filled = 0; nextPt = 0.0; ptStep = juce::jmax (1.0 / fs, sweepLen / kPoints);
            pulse = (int) (0.002f * fs);
        }
    }
    if (sweeping)
    {
        const float hp = in[In_HorizPos] / 10.0f;
        while (sweepT >= nextPt && filled < kPoints)
        {
            work.a[filled] = da + in[In_VertPos] * 0.1f;
            work.b[filled] = db;
            work.x[filled] = dsp::clampf ((float) (nextPt / sweepLen) + hp, -0.2f, 1.2f);
            work.z[filled] = z;
            ++filled; nextPt += ptStep;
        }
        sweepT += 1.0 / fs;
        out[Out_Sweep] = 10.0f * (float) juce::jmin (1.0, sweepT / sweepLen);
        out[Out_Gate] = 10.0f;
        if (sweepT >= sweepLen || filled >= kPoints)
        {
            work.n = filled; work.xy = false; work.dual = isPatched (In_B);
            publish();
            sweeping = false;
            if (mode == 2) singleDone = true;
        }
    }
    else { out[Out_Sweep] = 0.0f; out[Out_Gate] = 0.0f; }
    out[Out_Trig] = pulse > 0 ? 10.0f : 0.0f;
    if (pulse > 0) --pulse;
    trigLed.store (sweeping, std::memory_order_relaxed);
}

float M666::led (int id) const noexcept
{
    if (id == 0) return trigLed.load (std::memory_order_relaxed) ? 1.0f : 0.0f;
    if (id == 1) return powP->load() > 0.5f ? 1.0f : 0.0f;
    return 0.0f;
}
} // namespace wc
