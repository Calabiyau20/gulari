#include "M259.h"
#include "../WcParams.h"

namespace wc
{
using namespace dsp;

M259::M259 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    auto get = [&] (const char* s) { P x; x.v = apvts.getRawParameterValue (p + s); jassert (x.v != nullptr); return x; };
    moFreq = get ("mo_freq"); moFine = get ("mo_fine"); moRange = get ("mo_range"); moWave = get ("mo_wave");
    moCvAmt = get ("mo_cv"); moFmAmt = get ("mo_fm");
    index = get ("idx"); indexCvAmt = get ("idx_cv"); modAmp = get ("mod_amp"); modPitch = get ("mod_pitch");
    modTimbre = get ("mod_timbre"); prog = get ("prog"); lockAmt = get ("lock");
    poFreq = get ("po_freq"); poFine = get ("po_fine"); poCvAmt = get ("po_cv"); poFmAmt = get ("po_fm");
    timbre = get ("timbre"); timbreCvAmt = get ("timbre_cv"); sym = get ("sym"); order = get ("order");
    lockOn = get ("lock_on"); volt1 = get ("volt1");
    taps = Decimator4::makeTaps();
}

void M259::prepare (double sampleRate)
{
    fs = (float) sampleRate;
    fsOs = fs * (float) kOs;
    rampLen = juce::jmax (1, (int) (0.012f * fs));   // 12 ms glide on every knob: no zipper, no clicks
    folder.prepare (fsOs);
    amVactrol.set (3.0f, 40.0f, fs);                 // LED/LDR: fast on, slow off
    for (auto* d : { &acSine, &acSquare, &acFinal, &acMo, &acTri }) d->set (8.0f, fs);   // AC-coupled outputs

    // folder level compensation: RMS of the static transfer for a sine of amplitude A (no filter),
    // partially compensated (70 %) so higher Timbre still gets brighter and a little louder, like the hardware
    const int N = 2048;
    for (int k = 0; k <= 32; ++k)
    {
        const float A = 0.25f + 9.75f * (float) k / 32.0f;
        double acc = 0.0;
        for (int n = 0; n < N; ++n)
        {
            const float y = Wavefolder259::transfer (A * std::sin (kTwoPi * (float) n / (float) N));
            acc += (double) y * y;
        }
        foldGain[k] = (float) std::sqrt (acc / N);
    }
    // partial compensation (85 %): the folded wave keeps getting slightly louder and brighter with Timbre
    for (int k = 0; k <= 32; ++k) foldGain[k] = std::pow (0.7071f / foldGain[k], 0.85f);
    reset();
}

float M259::foldComp (float A) const noexcept
{
    const float k = clampf ((A - 0.25f) / 9.75f * 32.0f, 0.0f, 32.0f);
    const int i = juce::jmin (31, (int) k);
    return lerp (foldGain[i], foldGain[i + 1], k - (float) i);
}

void M259::reset()
{
    moPhase = poPhase = 0.0;
    moLast = 0.0f;
    folder.reset();
    for (auto* d : { &acSine, &acSquare, &acFinal, &acMo, &acTri }) d->reset();
    for (auto* d : { &decFinal, &decSine, &decSquare, &decMo, &decTri }) d->reset();
    std::fill (std::begin (prevIn), std::end (prevIn), 0.0f);
    lockPrev = 0.0f; lockSamplesSince = 0.0; lockPeriod = 0.0; lockDivCount = 0; lockRatio = 1.0f;
    firstBlock = true;
}

void M259::beginBlock (int)
{
    lowRange = moRange.get() > 0.5f;
    const float moHz = coarseHz (moFreq.get(), lowRange) * std::exp2 (moFine.get() / 12.0f);
    const float poHz = coarseHz (poFreq.get(), false) * std::exp2 (poFine.get() / 12.0f);
    auto set = [&] (Ramp& r, float v) { if (firstBlock) r.snap (v); else r.to (v, rampLen); };
    // frequencies glide in the log domain (octaves), so a big knob move sweeps evenly
    set (sMoHz, std::log2 (moHz)); set (sPoHz, std::log2 (poHz));
    set (sIndex, index.get()); set (sTimbre, timbre.get()); set (sSym, sym.get()); set (sOrder, order.get());
    set (sMoFm, moFmAmt.get()); set (sPoFm, poFmAmt.get()); set (sLock, lockAmt.get());
    set (sMoCv, moCvAmt.get()); set (sPoCv, poCvAmt.get()); set (sTimbreCv, timbreCvAmt.get()); set (sIndexCv, indexCvAmt.get());
    moWaveLocal = juce::jlimit (0, 2, (int) std::lround (moWave.get()));
    ampLocal = modAmp.get() > 0.5f; pitchLocal = modPitch.get() > 0.5f; timbreLocal = modTimbre.get() > 0.5f;
    progMode = juce::jlimit (0, 2, (int) std::lround (prog.get()));
    lockOnV = lockOn.get() > 0.5f;
    pitchDiv = volt1.get() > 0.5f ? 1.0f : 1.2f;     // 1V/OCT button: Eurorack 1 V/oct, else Buchla 1.2 V/oct
    leds[Led_Low].store (lowRange ? 1.0f : 0.0f); leds[Led_High].store (lowRange ? 0.0f : 1.0f);
    leds[Led_1V].store (pitchDiv < 1.1f ? 1.0f : 0.0f);
    firstBlock = false;
}

void M259::tick (const float* in, float* out) noexcept
{
    // ---- stage 01: control summing + expo (1.2 V/oct) ----
    const float moLog = sMoHz.next(), poLog = sPoHz.next();
    const float moHz = std::exp2 (moLog + (in[In_MoPitch] + sMoCv.next() * in[In_MoCv]) / pitchDiv);
    float poHz = std::exp2 (poLog + (in[In_PoPitch] + sPoCv.next() * in[In_PoCv]) / pitchDiv);
    const float idxV = clampf (sIndex.next() + sIndexCv.next() * in[In_IndexCv] / 10.0f, 0.0f, 1.0f);
    const float timbreBase = sTimbre.next() + sTimbreCv.next() * in[In_TimbreCv] / 10.0f;
    const float symV = clampf (sSym.next() + in[In_SymCv] / 10.0f, 0.0f, 1.0f);
    const float orderV = clampf (sOrder.next() + in[In_OrderCv] / 10.0f, 0.0f, 1.0f);
    const float moFm = sMoFm.next(), poFm = sPoFm.next(), lockA = lockOnV ? sLock.next() : (sLock.next(), 0.0f);

    // ---- stage 04: programming (Local / Remote / Both) ----
    bool ampOn = ampLocal, pitchOn = pitchLocal, timbreOn = timbreLocal;
    int wave = moWaveLocal;
    if (progMode > 0)
    {
        const bool rA = in[In_ProgAmp] > 3.0f, rP = in[In_ProgPitch] > 3.0f, rT = in[In_ProgTimbre] > 3.0f;
        const int rW = juce::jlimit (0, 2, (int) (in[In_ProgWave] / 3.34f));
        if (progMode == 1) { ampOn = rA; pitchOn = rP; timbreOn = rT; wave = rW; }
        else { ampOn |= rA; pitchOn |= rP; timbreOn |= rT; if (in[In_ProgWave] > 0.5f) wave = rW; }
    }

    // vactrol on the AM depth (host rate: it is a control path)
    const float amDepth = amVactrol.process (ampOn ? idxV : 0.0f);

    // ---- stage 07: phase lock (host-rate detector on the lock input) ----
    {
        const float x = in[In_Lock];
        lockSamplesSince += 1.0;
        bool event = false;
        if (lockPrev <= 0.0f && x > 0.0f && lockA > 0.0005f)
        {
            // sub-sample crossing time
            const double frac = (double) (-lockPrev / (x - lockPrev + 1.0e-12f));
            const double period = lockSamplesSince - 1.0 + frac;
            lockSamplesSince = 1.0 - frac;
            if (period > 2.0) lockPeriod = lockPeriod <= 0.0 ? period : lockPeriod * 0.7 + period * 0.3;
            event = true;
        }
        lockPrev = x;
        if (lockPeriod > 0.0 && lockA > 0.0005f && lockSamplesSince < lockPeriod * 4.0)
        {
            const float fin = fs / (float) lockPeriod;
            const float r = fin / juce::jmax (1.0e-3f, poHz);
            float target;
            int div = 1;
            if (r >= 1.0f) { div = juce::jmax (1, (int) std::lround (r)); target = fin / (float) div; }
            else target = fin * (float) juce::jmax (1, (int) std::lround (1.0f / r));
            const float want = std::pow (target / poHz, lockA);
            lockRatio += (want - lockRatio) * 0.02f;
            if (event && ++lockDivCount >= div)
            {
                lockDivCount = 0;
                // pull the phase toward the input's zero crossing (phase 0 of the triangle = rising zero)
                double e = poPhase - 0.25;   // our rising zero crossing of the sine is at phase 0.25 (tri = 0 rising)
                e -= std::floor (e + 0.5);
                poPhase -= 0.25 * lockA * e;
            }
            lockLed.store (std::fabs (want - lockRatio) < 0.003f && lockA > 0.2f, std::memory_order_relaxed);
        }
        else { lockRatio += (1.0f - lockRatio) * 0.02f; lockLed.store (false, std::memory_order_relaxed); }
        poHz *= lockRatio;
    }

    // ---- oversampled audio path ----
    const float moInc0 = moHz / fsOs;
    for (int j = 0; j < kOs; ++j)
    {
        const float t = (float) (j + 1) / (float) kOs;
        const float moFmIn = lerp (prevIn[In_MoFm], in[In_MoFm], t);
        const float poFmIn = lerp (prevIn[In_PoFm], in[In_PoFm], t);

        // ---- stage 03: modulation oscillator (linear FM on the core current, no through-zero) ----
        const float moInc = juce::jmax (0.0f, moInc0 * (1.0f + 4.0f * moFm * moFmIn));
        moPhase += moInc;
        moPhase -= std::floor (moPhase);
        const float mp = (float) moPhase;
        float mo;
        if (wave == 0) mo = 2.0f * mp - 1.0f - polyBlep (mp, moInc);                       // sawtooth
        else if (wave == 1)                                                                 // square
        {
            mo = mp < 0.5f ? 1.0f : -1.0f;
            mo += polyBlep (mp, moInc);
            float m2 = mp + 0.5f; m2 -= std::floor (m2);
            mo -= polyBlep (m2, moInc);
        }
        else mo = 1.0f - 4.0f * std::fabs (mp - 0.5f);                                     // triangle
        moLast = mo;

        // ---- stage 04: pitch modulation + external FM (linear, on the core current) ----
        float poInc = poHz / fsOs;
        const float fmTotal = (pitchOn ? 4.0f * idxV * mo : 0.0f) + 4.0f * poFm * poFmIn;
        poInc = juce::jmax (0.0f, poInc * (1.0f + fmTotal));
        poPhase += poInc;
        poPhase -= std::floor (poPhase);
        const float pp = (float) poPhase;

        // ---- stage 02: triangle core -> sine shaper, square comparator ----
        const float tri = 1.0f - 4.0f * std::fabs (pp - 0.5f);           // -1 at phase 0, +1 at 0.5
        const float sineIdeal = std::sin (0.5f * kPi * tri);
        const float sineOta = std::tanh (1.5f * tri) / std::tanh (1.5f);
        const float sine = lerp (sineIdeal, sineOta, 0.08f);             // residual of the diff-pair shaper (TR1)
        float sq = tri >= 0.0f ? 1.0f : -1.0f;                           // comparator: high while tri > 0 (phase .25 .. .75)
        {
            float a = pp - 0.25f; a -= std::floor (a);
            float b = pp - 0.75f; b -= std::floor (b);
            sq += polyBlep (a, poInc) - polyBlep (b, poInc);
        }

        // ---- stage 04/05: timbre with modulation ----
        const float timbreEff = clampf (timbreBase + (timbreOn ? 0.5f * idxV * mo : 0.0f), 0.0f, 1.0f);

        // ---- stage 05: wavefolder (circuit model) ----
        const float A = 0.25f + 9.75f * timbreEff;
        const float vin = A * sine + (symV - 0.5f) * 6.0f;
        const float folded = folder.process (vin) * foldComp (A);

        // ---- stage 06: quasi-saw and M shapers, Order crossfade ----
        // saw resets at phase 0.25 (rising zero crossing of the sine), so it stays in phase with the sine
        float sp = pp - 0.25f; sp -= std::floor (sp);
        float saw = std::tanh (1.2f * (2.0f * sp - 1.0f)) / std::tanh (1.2f);
        saw -= polyBlep (sp, poInc);                   // jump from +1 to -1, as a plain saw
        // "M": a square whose plateaus dip in the middle of each half cycle
        const float s2 = sineIdeal * sineIdeal;
        float mShape = (tri >= 0.0f ? 1.0f : -1.0f) * (1.0f - 0.6f * s2);
        {
            float a = pp - 0.25f; a -= std::floor (a);
            float b = pp - 0.75f; b -= std::floor (b);
            mShape += polyBlep (a, poInc) - polyBlep (b, poInc);
        }
        const float sawM = lerp (saw, mShape, symV);
        const float shaped = lerp (sine, sawM, timbreEff);
        float fin = lerp (folded, shaped, orderV);

        // ---- stage 04: amplitude modulation, vactrol balanced modulator (wet - dry) after Order ----
        fin = fin * (1.0f - amDepth) + amDepth * fin * mo;

        decFinal.push (fin);
        decSine.push (sine);
        decSquare.push (sq * 0.8f);
        decMo.push (mo);
        decTri.push (1.0f - 4.0f * std::fabs (mp - 0.5f));
    }
    std::copy (in, in + In_Count, prevIn);

    // ---- stage 08: outputs (AC coupled audio, DC control) ----
    const float f = acFinal.process (decFinal.output (taps));
    out[Out_Final1] = out[Out_Final2] = f;
    out[Out_Sine] = acSine.process (decSine.output (taps));
    out[Out_Square] = acSquare.process (decSquare.output (taps));
    const float m = decMo.output (taps);
    const float moTriOs = lowRange ? decTri.output (taps) : acTri.process (decTri.output (taps));
    out[Out_MoSignal] = lowRange ? m : acMo.process (m);
    out[Out_MoCv] = 5.0f + 5.0f * moLast;
    out[Out_MoTri] = moTriOs;
    out[Out_MoPulse] = moPhase < 0.5 ? 10.0f : 0.0f;          // GATE OUT: the modulation oscillator as a pulse
    out[Out_PoPulse] = poPhase >= 0.25 && poPhase < 0.75 ? 10.0f : 0.0f;
    moLed.store (moLast, std::memory_order_relaxed);
    leds[Led_Saw].store (wave == 0 ? 1.0f : 0.0f, std::memory_order_relaxed);
    leds[Led_Sqr].store (wave == 1 ? 1.0f : 0.0f, std::memory_order_relaxed);
    leds[Led_Tri].store (wave == 2 ? 1.0f : 0.0f, std::memory_order_relaxed);
    leds[Led_Lock].store (lockOnV ? (lockLed.load (std::memory_order_relaxed) ? 1.0f : 0.35f) : 0.0f, std::memory_order_relaxed);
    leds[Led_Amp].store (ampOn ? 1.0f : 0.0f, std::memory_order_relaxed);
    leds[Led_Pitch].store (pitchOn ? 1.0f : 0.0f, std::memory_order_relaxed);
    leds[Led_Timbre].store (timbreOn ? 1.0f : 0.0f, std::memory_order_relaxed);
    leds[Led_Sym].store (clampf (std::fabs (in[In_SymCv]) / 10.0f, 0.0f, 1.0f), std::memory_order_relaxed);
    leds[Led_Order].store (clampf (std::fabs (in[In_OrderCv]) / 10.0f, 0.0f, 1.0f), std::memory_order_relaxed);
    leds[Led_MoGate].store (moPhase < 0.5 ? 1.0f : 0.0f, std::memory_order_relaxed);
    leds[Led_PoGate].store (0.5f, std::memory_order_relaxed);
}
} // namespace wc
