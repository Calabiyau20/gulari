#include "M248.h"
#include "M264.h"

namespace wc
{
const char* M248::fnName (int f)
{
    static const char* n[F_Count] = { "p1", "p2", "quant", "slope", "range", "vext", "stop", "sust", "enable", "first", "last", "trange", "text" };
    return n[f];
}

M248::M248 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p) : state (apvts), pre (p)
{
    for (int s = 0; s < kStages; ++s)
    {
        const juce::String n (s + 1);
        lvlP[s] = apvts.getRawParameterValue (p + "lvl" + n);
        timP[s] = apvts.getRawParameterValue (p + "tim" + n);
        for (int f = 0; f < F_Count; ++f) { fnP[f][s] = apvts.getRawParameterValue (p + fnName (f) + "_" + n); jassert (fnP[f][s] != nullptr); }
    }
    for (int k = 0; k < 2; ++k)
    {
        const juce::String n (k + 1);
        multP[k] = apvts.getRawParameterValue (p + "mult" + n); mcvP[k] = apvts.getRawParameterValue (p + "mcv" + n);
        addrP[k] = apvts.getRawParameterValue (p + "addr" + n); contP[k] = apvts.getRawParameterValue (p + "cont" + n);
        extP[k] = apvts.getRawParameterValue (p + "ext" + n); artP[k] = apvts.getRawParameterValue (p + "art" + n);
    }
    editP = apvts.getRawParameterValue (p + "edit"); keyP = apvts.getRawParameterValue (p + "key"); scaleP = apvts.getRawParameterValue (p + "scale");
}

void M248::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }

void M248::reset()
{
    for (auto& f : g) { f = Fg(); }
}

void M248::beginBlock (int)
{
    for (int s = 0; s < kStages; ++s)
    {
        lvl[s] = lvlP[s]->load(); tim[s] = timP[s]->load();
        for (int f = 0; f < F_Count; ++f) fn[f][s] = (int) std::lround (fnP[f][s]->load());
    }
    for (int k = 0; k < 2; ++k)
    {
        mult[k] = 0.5f * std::pow (8.0f, multP[k]->load());      // .5 .. 4
        mcv[k] = mcvP[k]->load(); addr[k] = addrP[k]->load();
        cont[k] = contP[k]->load() > 0.5f; extAddr[k] = extP[k]->load() > 0.5f;
        artSrc[k] = (int) std::lround (artP[k]->load());
    }
    key = (int) std::lround (keyP->load()); scale = (int) std::lround (scaleP->load());
}

int M248::firstStage() const noexcept
{
    for (int s = 0; s < kStages; ++s) if (fn[F_First][s]) return s;
    return 0;
}

float M248::target (int s, const float* in) const noexcept
{
    const int r = fn[F_Range][s];
    float v;
    if (fn[F_VExt][s])
    {
        const int src = juce::jlimit (0, 3, (int) (lvl[s] * 4.0f));    // the slider picks input A..D
        const float x = in[In_ExtA + src];
        v = r == 0 ? x : (r == 1 ? 0.5f * x : 2.0f * (float) (r - 2) + dsp::clampf (x, 0.0f, 2.0f));
    }
    else
        v = r == 0 ? 10.0f * lvl[s] : (r == 1 ? 5.0f * lvl[s] : 2.0f * (float) (r - 2) + 2.0f * lvl[s]);
    if (fn[F_Quant][s]) v = M264::quantize (v, scale, key);
    return v;
}

float M248::duration (int fgI, int s, const float* in) const noexcept
{
    float lo, hi;
    range (juce::jlimit (0, 3, fn[F_TRange][s]), lo, hi);
    float k = tim[s];
    if (fn[F_TExt][s])
    {
        const int src = juce::jlimit (0, 3, (int) (tim[s] * 4.0f));
        k = dsp::clampf (in[In_ExtA + src] / 10.0f, 0.0f, 1.0f);       // no c.v. = the fastest time of the range
    }
    const float t = lo * std::pow (hi / lo, k);
    const float m = mult[fgI] * std::exp2 (mcv[fgI] * in[M248::inJ (fgI, 0)] / 5.0f);
    return juce::jlimit (0.0005f, 600.0f, t * m);
}

void M248::enter (int fgI, int s, const float* in) noexcept
{
    auto& f = g[fgI];
    f.stage = s; f.elapsed = 0.0; f.startV = f.v;
    f.dur = duration (fgI, s, in);
    f.allLeft = (int) (0.005f * fs);
    f.atEnd = false; f.haltedOnStop = false;
    const bool upd = artSrc[fgI] == 2 || (artSrc[fgI] == 0 && fn[F_P1][s]) || (artSrc[fgI] == 1 && fn[F_P2][s]);
    if (upd) f.art = M264::quantize (target (s, in), scale, key);
}

void M248::advance (int fgI, const float* in) noexcept
{
    auto& f = g[fgI];
    if (fn[F_Last][f.stage]) { enter (fgI, firstStage(), in); return; }
    if (f.stage >= kStages - 1) { f.status = 2; f.atEnd = true; return; }   // end of the function
    enter (fgI, f.stage + 1, in);
}

void M248::tick (const float* in, float* out) noexcept
{
    auto rising = [] (bool now, bool& was) { const bool r = now && ! was; was = now; return r; };
    for (int k = 0; k < 2; ++k)
    {
        auto& f = g[k];
        const float startV = in[M248::inJ (k, 2)];
        const bool startHi = startV > 3.0f;
        const bool startEdge = rising (startHi, f.startWas) | rising (btnStart[k].load (std::memory_order_relaxed), f.btnStartWas);
        const bool stopEdge = rising (in[M248::inJ (k, 1)] > 3.0f, f.stopWas);
        const bool strobe = rising (in[M248::inJ (k, 3)] > 3.0f, f.strobeWas);
        const int addrStage = juce::jlimit (0, kStages - 1, (int) ((extAddr[k] ? in[M248::inJ (k, 4)] / 10.0f : addr[k]) * (float) kStages));
        if (rising (btnReset[k].load (std::memory_order_relaxed), f.resetWas)) enter (k, firstStage(), in);
        if (cont[k])
        {
            // continuous: the address sweeps the stages, the internal clock is stopped
            if (addrStage != f.stage || f.lastAddr < 0) enter (k, addrStage, in);
            f.lastAddr = addrStage;
            f.status = 2;
        }
        else
        {
            f.lastAddr = -1;
            if (strobe) { enter (k, addrStage, in); if (f.status == 2) f.status = 0; }
            if (stopEdge) f.status = 2;
            if (startEdge && f.status == 2)
            {
                if (f.atEnd) { enter (k, firstStage(), in); f.status = 0; }
                else if (f.haltedOnStop) { f.status = 0; advance (k, in); if (! f.atEnd) f.status = 0; }
                else f.status = 0;
            }
            if (rising (btnAdvance[k].load (std::memory_order_relaxed), f.advWas)) advance (k, in);
            if (f.status != 2)
            {
                if (f.elapsed < f.dur) f.elapsed += 1.0 / fs;
                if (f.elapsed >= f.dur)
                {
                    const int s = f.stage;
                    if (fn[F_Stop][s] && ! f.haltedOnStop) { f.status = 2; f.haltedOnStop = true; }
                    else if (fn[F_Sust][s] && startHi) f.status = 1;
                    else if (fn[F_Enable][s] && startV <= 5.0f) f.status = 1;
                    else { f.status = 0; advance (k, in); }
                }
            }
        }
        const int s = f.stage;
        const float tv = target (s, in);
        f.v = fn[F_Slope][s] ? f.startV + (tv - f.startV) * (float) juce::jmin (1.0, f.elapsed / (double) f.dur) : tv;
        const bool live = ! f.atEnd && ! (cont[k] && false);
        const bool inGate = live && (f.elapsed < (double) f.dur - 0.001 || f.status == 1 || f.haltedOnStop || cont[k]);
        const bool p1 = inGate && fn[F_P1][s], p2 = inGate && fn[F_P2][s];
        out[outJ (k, 0)] = f.art;
        out[outJ (k, 1)] = 10.0f * (1.0f - (float) juce::jmin (1.0, f.elapsed / (double) f.dur));
        out[outJ (k, 2)] = 10.0f * tim[s];
        out[outJ (k, 3)] = f.v;
        out[outJ (k, 4)] = f.allLeft > 0 ? 10.0f : 0.0f;
        out[outJ (k, 5)] = p1 ? 10.0f : 0.0f;
        out[outJ (k, 6)] = p2 ? 10.0f : 0.0f;
        if (f.allLeft > 0) --f.allLeft;
        curStage[k].store (s, std::memory_order_relaxed);
        statusLed[k].store (f.status, std::memory_order_relaxed);
        p1Led[k].store (p1, std::memory_order_relaxed); p2Led[k].store (p2, std::memory_order_relaxed);
    }
}

float M248::led (int id) const noexcept
{
    const int edit = juce::jlimit (0, kStages - 1, (int) std::lround (editP->load()) - 1);
    if (id >= 0 && id < kStages)
    {
        const int d = displayFg.load (std::memory_order_relaxed);
        if (id == curStage[d].load (std::memory_order_relaxed)) return 1.0f;
        if (id == edit) return (juce::Time::getMillisecondCounter() / 300) % 2 ? 0.55f : 0.15f;
        return 0.0f;
    }
    if (id >= 100 && id < 120)
    {
        const int k = (id - 100) / 10, w = (id - 100) % 10;
        const int st = statusLed[k].load (std::memory_order_relaxed);
        switch (w)
        {
            case 0: return st == 0 ? 1.0f : 0.0f;
            case 1: return st == 1 ? 1.0f : 0.0f;
            case 2: return st == 2 ? 1.0f : 0.0f;
            case 3: return p1Led[k].load (std::memory_order_relaxed) ? 1.0f : 0.0f;
            case 4: return p2Led[k].load (std::memory_order_relaxed) ? 1.0f : 0.0f;
            case 5: return displayFg.load (std::memory_order_relaxed) == k ? 1.0f : 0.0f;
            default: return 0.0f;
        }
    }
    if (id >= 200 && id < 200 + F_Count * 8)
    {
        const int f = (id - 200) / 8, v = (id - 200) % 8;
        const int cur = (int) std::lround (fnP[f][edit]->load());
        return (f == F_Range || f == F_TRange) ? (cur == v ? 1.0f : 0.0f) : (cur != 0 ? 1.0f : 0.0f);
    }
    if (id >= 300 && id < 312)
    {
        const int a = armed.load (std::memory_order_relaxed), i = id - 300;
        if (a == 4) return i == (int) std::lround (keyP->load()) ? 1.0f : 0.0f;
        if (a == 3) { const int sc = (int) std::lround (scaleP->load()); return (sc == 2 && i == 9) || (sc == 0 && i == 10) || (sc == 1 && i == 11) ? 1.0f : 0.0f; }
        return 0.0f;
    }
    if (id >= 320 && id < 324) return armed.load (std::memory_order_relaxed) == id - 319 ? 1.0f : 0.0f;
    return 0.0f;
}
} // namespace wc
