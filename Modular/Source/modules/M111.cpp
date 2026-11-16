#include "M111.h"

namespace wc
{
M111::M111 (const Params& params, const juce::String& p)
{
    rateP = params.getRawParameterValue (p + "rate");
    modeP = params.getRawParameterValue (p + "mode");
    amountP = params.getRawParameterValue (p + "amount");
    phaseP = params.getRawParameterValue (p + "phase");
    polarityP = params.getRawParameterValue (p + "polarity");
    retrigP = params.getRawParameterValue (p + "retrig");
    waveP = params.getRawParameterValue (p + "wave");
    jassert (rateP && modeP && amountP && phaseP && polarityP && retrigP && waveP);
}

void M111::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }

void M111::reset()
{
    acc = 0.0;
    phiWas = 0.0f;
    wrapped = retrigWas = clockWas = noteHit = clockSeen = false;
    clockPeriod = sinceClock = pulses = syncLeft = 0;
    sinceSync = 1 << 30;
    pick(); pick();   // the random waves start on a value of their own, not on 0
    clockState.store (ClockNone, std::memory_order_relaxed);
    speedLed.store (0.0f, std::memory_order_relaxed);
}

void M111::beginBlock (int)
{
    rateK = rateP->load(); amountK = amountP->load(); phaseK = phaseP->load();
    modeV = juce::jlimit (0, 4, (int) std::lround (modeP->load()));
    waveV = juce::jlimit (0, 7, (int) std::lround (waveP->load()));
    unipolar = polarityP->load() > 0.5f;
    retrigOn = retrigP->load() > 0.5f;
    bpm = juce::jlimit (20.0, 400.0, hostBpm.load (std::memory_order_relaxed));   // the 250's limits
}

float M111::led (int id) const noexcept
{
    if (id >= 0 && id <= 4)
        return (int) std::lround (modeP->load()) != id ? 0.0f : (clockState.load (std::memory_order_relaxed) != ClockNone ? 0.4f : 1.0f);
    if (id == 5) return retrigP->load() > 0.5f ? 1.0f : 0.0f;
    if (id == 6) return speedLed.load (std::memory_order_relaxed);
    if (id >= 10 && id <= 17) return (int) std::lround (waveP->load()) == id - 10 ? 1.0f : 0.0f;
    return 0.0f;
}

// a new cycle draws for every random wave, whichever plays: the generator always runs
void M111::pick() noexcept
{
    sPrev = sNext;
    sNext = 2.0f * rnd.nextFloat() - 1.0f;
    wPrev = wNext;
    float t = wNext + rnd.nextFloat() - 0.5f;   // up to 25 % of the range either way, bouncing off the ends
    if (t > 1.0f) t = 2.0f - t;
    if (t < -1.0f) t = -2.0f - t;
    wNext = t;
}

void M111::tick (const float* in, float* out) noexcept
{
    const float k = dsp::clampf (rateK + in[In_RateCv] * 0.1f, 0.0f, 1.0f);   // FREE: 1 V = 0.4 decade; steps: 1 V = one step
    bool restart = false;

    // CLOCK IN (> 3 V): the period is the time between the last two edges. The lock restarts the phase on every
    // pulse (xN) or every N pulses (x1/N), counted from the first one or the last restart
    if (isPatched (In_Clock))
    {
        if (sinceClock < (1 << 30)) ++sinceClock;
        const bool high = in[In_Clock] > 3.0f;
        if (high && ! clockWas)
        {
            if (clockSeen)
            {
                clockPeriod = sinceClock;
                const double r = lfoRatio (lfoStep (k));
                if (++pulses >= (r < 1.0 ? (int) std::lround (1.0 / r) : 1)) { pulses = 0; restart = true; }
            }
            clockSeen = true;
            sinceClock = 0;
        }
        clockWas = high;
    }
    else if (clockSeen) { clockSeen = clockWas = false; clockPeriod = pulses = 0; }   // unplugged: its mode, the phase goes on

    const bool rt = in[In_Retrig] > 3.0f;
    if ((rt && ! retrigWas) || (noteHit && retrigOn)) { restart = true; pulses = 0; }
    retrigWas = rt;
    noteHit = false;

    double hz;
    if (clockPeriod > 0) hz = lfoRatio (lfoStep (k)) * (double) fs / (double) clockPeriod;   // no pulses: the last speed
    else if (modeV <= FreeSec) hz = lfoHz (k);
    else hz = bpm / (60.0 * lfoBeats (lfoStep (k)) * (modeV == SyncDotted ? 1.5 : (modeV == SyncTriplet ? 2.0 / 3.0 : 1.0)));
    hz = juce::jlimit (0.001, 200.0, hz);

    bool cycle = wrapped;
    wrapped = false;
    if (restart) { acc = 0.0; cycle = true; }   // this very sample is the wave at PHASE
    float phi = (float) acc + phaseK / 360.0f + in[In_PhaseCv] * 0.1f;        // 36 degrees per volt, all the way round
    phi -= std::floor (phi);
    if (restart || phi - phiWas < -0.5f) pick();   // forward through 0; backwards (PHASE c.v.) draws nothing
    phiWas = phi;

    const float glide = 0.5f * (1.0f - std::cos (dsp::kPi * phi));            // cosine, landing as the next cycle starts
    float w;
    switch (waveV)
    {
        case 0:  w = std::sin (dsp::kTwoPi * phi); break;
        case 1:  w = phi < 0.25f ? 4.0f * phi : (phi < 0.75f ? 2.0f - 4.0f * phi : 4.0f * phi - 4.0f); break;
        case 2:  w = phi < 0.5f ? 1.0f : -1.0f; break;
        case 3:  w = 2.0f * phi - 1.0f; break;
        case 4:  w = 1.0f - 2.0f * phi; break;
        case 5:  w = sNext; break;
        case 6:  w = sPrev + (sNext - sPrev) * glide; break;
        default: w = wPrev + (wNext - wPrev) * glide; break;
    }
    const float a = dsp::clampf (amountK + in[In_AmountCv] * 0.1f, 0.0f, 1.0f);   // 10 % per volt
    const float v = unipolar ? 5.0f * a * (w + 1.0f) : 10.0f * a * w;
    out[Out_Lfo] = out[Out_Lfo2] = v;
    out[Out_Inv] = unipolar ? 10.0f * a - v : -v;

    // SYNC OUT: 5 ms (half a cycle when that is shorter) on every new cycle, never two within 2 ms
    if (cycle && sinceSync >= (int) (0.002f * fs))
    {
        syncLeft = juce::jmax (1, (int) juce::jmin (0.005 * (double) fs, 0.5 * (double) fs / hz));
        sinceSync = 0;
    }
    out[Out_Sync] = syncLeft > 0 ? 10.0f : 0.0f;
    if (syncLeft > 0) --syncLeft;
    if (sinceSync < (1 << 30)) ++sinceSync;

    acc += hz / (double) fs;
    if (acc >= 1.0) { acc -= std::floor (acc); wrapped = true; }

    speedLed.store (0.5f * a * (w + 1.0f), std::memory_order_relaxed);
    clockState.store (! isPatched (In_Clock) ? ClockNone : (clockPeriod == 0 ? ClockWaiting : (sinceClock / 2 > clockPeriod ? ClockLost : ClockLocked)),
                      std::memory_order_relaxed);
    cycleSecs.store ((float) (1.0 / hz), std::memory_order_relaxed);
}
} // namespace wc
