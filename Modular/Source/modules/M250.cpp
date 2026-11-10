#include "M250.h"
#include "M264.h"

namespace wc
{
static const char* kF[12] = { "p1", "p2", "tq", "ti", "vq", "vi", "trng", "vrng", "mode", "text", "vext", "fix" };

M250::M250 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    for (int s = 0; s < kStages; ++s)
    {
        const juce::String n (s + 1);
        vP[0][s] = apvts.getRawParameterValue (p + "v1_" + n); vP[1][s] = apvts.getRawParameterValue (p + "v2_" + n);
        tP[s] = apvts.getRawParameterValue (p + "t_" + n);
        for (int k = 0; k < F_Count; ++k) { fP[k][s] = apvts.getRawParameterValue (p + kF[k] + "_" + n); jassert (fP[k][s] != nullptr); }
    }
    multP = apvts.getRawParameterValue (p + "mult"); mcvP = apvts.getRawParameterValue (p + "mcv");
    addrP = apvts.getRawParameterValue (p + "addr"); offP = apvts.getRawParameterValue (p + "offset");
    lEndP = apvts.getRawParameterValue (p + "loopend"); lToP = apvts.getRawParameterValue (p + "loopto"); lCntP = apvts.getRawParameterValue (p + "loops");
    editP = apvts.getRawParameterValue (p + "edit"); setP = apvts.getRawParameterValue (p + "set"); remP = apvts.getRawParameterValue (p + "remote");
}

void M250::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }

void M250::reset() { stage = 0; elapsed = 0.0; running = false; loopsLeft = -1; out1 = out2 = from1 = from2 = 0.0f; }

void M250::beginBlock (int)
{
    for (int s = 0; s < kStages; ++s)
    {
        v[0][s] = vP[0][s]->load(); v[1][s] = vP[1][s]->load(); t[s] = tP[s]->load();
        for (int k = 0; k < F_Count; ++k) f[k][s] = (int) std::lround (fP[k][s]->load());
    }
    mult = 0.25f * std::pow (16.0f, multP->load()); mcv = mcvP->load();
    addrMode = (int) std::lround (addrP->load()); off = offP->load();
    loopEnd = (int) std::lround (lEndP->load()); loopTo = (int) std::lround (lToP->load()); loopCount = (int) std::lround (lCntP->load());
}

float M250::voltage (int set, int s, const float* in) const noexcept
{
    float k = v[set][s];
    if (f[F_VExt][s])
    {
        const int src = juce::jlimit (0, 3, (int) (k * 4.0f));
        const float x = f[F_Fix][s] ? fixV[set] : in[In_ExtA + src];
        return f[F_VQ][s] ? M264::quantize (x, 1, 0) : x;
    }
    const int r = f[F_VRng][s];             // 0..4 = 0-2 .. 8-10, 5 = all
    float volts = r >= 5 ? 10.0f * k : 2.0f * (float) r + 2.0f * k;
    if (f[F_VQ][s]) volts = M264::quantize (volts, 1, 0);
    return volts;
}

void M250::enter (int s, const float* in) noexcept
{
    stage = s; elapsed = 0.0;
    from1 = out1; from2 = out2; timeFrom = timeOut;
    if (f[F_Fix][s] && f[F_VExt][s])
        for (int set = 0; set < 2; ++set) fixV[set] = in[In_ExtA + juce::jlimit (0, 3, (int) (v[set][s] * 4.0f))];
    float lo, hi;
    timeRange (juce::jlimit (0, 2, f[F_TRng][s]), lo, hi);
    float k = t[s];
    if (f[F_TExt][s]) k = dsp::clampf (in[In_ExtA + juce::jlimit (0, 3, (int) (t[s] * 4.0f))] / 10.0f, 0.0f, 1.0f);
    if (f[F_TQ][s]) k = std::round (k * 8.0f) / 8.0f;          // quantized time: nine values per range
    dur = juce::jlimit (0.0005f, 600.0f, lo * std::pow (hi / lo, k) * mult * std::exp2 (mcv * in[In_MultCv] / 5.0f));
    pAll = (int) (0.005f * fs);
    p1 = f[F_P1][s] ? pAll : 0; p2 = f[F_P2][s] ? pAll : 0;
    if (f[F_Mode][s] == 3) running = false;                    // stop stage: the sequence stops here
}

void M250::next (const float* in) noexcept
{
    if (loopEnd > 0 && stage == loopEnd - 1)
    {
        if (loopsLeft < 0) loopsLeft = loopCount;                 // 0 = loop forever
        if (loopCount == 0 || loopsLeft > 0)
        {
            if (loopCount > 0) --loopsLeft;
            loopsDone.store (loopCount - juce::jmax (0, loopsLeft), std::memory_order_relaxed);
            enter (juce::jlimit (0, kStages - 1, loopTo - 1), in);
            return;
        }
        loopsLeft = -1;
    }
    enter ((stage + 1) % kStages, in);
}

void M250::tick (const float* in, float* out) noexcept
{
    auto rising = [] (bool now, bool& was) { const bool r = now && ! was; was = now; return r; };
    const bool startHi = in[In_Start] > 3.0f, stopHi = in[In_Stop] > 3.0f;
    if (rising (startHi, startWas) | rising (btnStart.load (std::memory_order_relaxed), bStartWas))
    {
        if (! running && elapsed <= 0.0) enter (stage, in);    // a fresh start computes the stage time
        running = true;
    }
    if (rising (stopHi, stopWas) | rising (btnStop.load (std::memory_order_relaxed), bStopWas)) running = false;
    const int addrStage = juce::jlimit (0, kStages - 1, (int) ((off + in[In_AddrCv] / 10.0f) * (float) kStages));
    if (addrMode == 2) { if (addrStage != lastAddr) enter (addrStage, in); lastAddr = addrStage; }
    else
    {
        lastAddr = -1;
        if (addrMode == 1 && rising (in[In_AddrPulse] > 3.0f, addrWas)) enter (addrStage, in);
        if (running)
        {
            const int mode = f[F_Mode][stage];
            const bool hold = (mode == 1 && stopHi) || (mode == 2 && ! startHi);   // sustain: gate at stop pauses; enable: runs while start is high
            if (! hold) elapsed += 1.0 / fs;
            if (elapsed >= dur) next (in);
        }
    }
    const int s = stage;
    const float x = (float) juce::jmin (1.0, elapsed / (double) dur);
    const float a1 = voltage (0, s, in), a2 = voltage (1, s, in);
    out1 = f[F_VI][s] ? from1 + (a1 - from1) * x : a1;
    out2 = f[F_VI][s] ? from2 + (a2 - from2) * x : a2;
    const float tv = 10.0f * t[s];
    timeOut = f[F_TI][s] ? timeFrom + (tv - timeFrom) * x : tv;
    out[Out_Cv1] = out1; out[Out_Cv2] = out2; out[Out_TimeCv] = timeOut;
    out[Out_All] = (p1 > 0 || p2 > 0 || pAll > 0) && (f[F_P1][s] || f[F_P2][s]) ? 10.0f : 0.0f;
    out[Out_P1] = p1 > 0 ? 10.0f : 0.0f; out[Out_P2] = p2 > 0 ? 10.0f : 0.0f;
    if (pAll > 0) --pAll; if (p1 > 0) --p1; if (p2 > 0) --p2;
    curStage.store (s, std::memory_order_relaxed);
    runLed.store (running || addrMode == 2, std::memory_order_relaxed);
}

float M250::led (int id) const noexcept
{
    const int edit = juce::jlimit (0, kStages - 1, (int) std::lround (editP->load()) - 1);
    if (id >= 0 && id < kStages)
    {
        if (id == curStage.load (std::memory_order_relaxed)) return 1.0f;
        if (id == edit) return (juce::Time::getMillisecondCounter() / 300) % 2 ? 0.5f : 0.1f;
        return 0.0f;
    }
    if (id >= 20 && id < 20 + kStages) { const int s = id - 20; return (fP[F_P1][s]->load() > 0.5f || fP[F_P2][s]->load() > 0.5f) ? 0.8f : 0.0f; }
    if (id == 40) return runLed.load (std::memory_order_relaxed) ? 1.0f : 0.0f;
    auto fv = [&] (int k) { return (int) std::lround (fP[k][edit]->load()); };
    switch (id)
    {
        case 41: return fv (F_TQ) ? 1.0f : 0.0f;
        case 42: return fv (F_TI) ? 1.0f : 0.0f;
        case 43: case 44: case 45: return fv (F_TRng) == id - 43 ? 1.0f : 0.0f;
        case 46: case 47: return (int) std::lround (setP->load()) == id - 46 ? 1.0f : 0.0f;
        case 48: return fv (F_VQ) ? 1.0f : 0.0f;
        case 49: return fv (F_VI) ? 1.0f : 0.0f;
        case 50: case 51: case 52: case 53: case 54: case 55: return fv (F_VRng) == id - 50 ? 1.0f : 0.0f;
        case 56: return fv (F_P1) ? 1.0f : 0.0f;
        case 57: return fv (F_P2) ? 1.0f : 0.0f;
        case 58: case 59: case 60: case 61: return fv (F_Mode) == id - 58 ? 1.0f : 0.0f;
        case 62: return fv (F_TExt) ? 1.0f : 0.0f;
        case 63: return fv (F_VExt) ? 1.0f : 0.0f;
        case 64: return fv (F_Fix) ? 1.0f : 0.0f;
        case 65: return (int) std::lround (addrP->load()) == 1 ? 1.0f : 0.0f;
        case 66: return (int) std::lround (addrP->load()) == 2 ? 1.0f : 0.0f;
        case 67: return remP->load() > 0.5f ? 1.0f : 0.0f;
        case 68: return 1.0f;
        default: return 0.0f;
    }
}
} // namespace wc
