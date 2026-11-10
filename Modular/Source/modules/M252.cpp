#include "M252.h"

namespace wc
{
constexpr int M252::kSize[];

M252::M252 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    bpmP = apvts.getRawParameterValue (p + "bpm"); pdurP = apvts.getRawParameterValue (p + "pdur");
    syncP = apvts.getRawParameterValue (p + "sync"); playP = apvts.getRawParameterValue (p + "play");
    for (int k = 0; k < 3; ++k)
    {
        const juce::String n (k + 1);
        clkP[k] = apvts.getRawParameterValue (p + "clk" + n); dtP[k] = apvts.getRawParameterValue (p + "dt" + n); dtsP[k] = apvts.getRawParameterValue (p + "dts" + n);
    }
    layerP = apvts.getRawParameterValue (p + "layer"); euclidP = apvts.getRawParameterValue (p + "euclid");
    editP = apvts.getRawParameterValue (p + "edit"); remP = apvts.getRawParameterValue (p + "remote");
    clearAll();
}

void M252::clearAll() { for (int i = 0; i < kCells; ++i) clearCell (i, 3); }

void M252::clearCell (int i, int layer)
{
    if (i < 0 || i >= kCells) return;
    if (layer == 0 || layer == 3) { rgb[i].store (0); offset[i].store (0.0f); }
    if (layer == 1 || layer == 3) for (int k = 0; k < 3; ++k) cv[i][k].store (-1.0f);
    if (layer == 2 || layer == 3) { for (int k = 3; k < 6; ++k) cv[i][k].store (-1.0f); sub[i].store (0); subCurve[i].store (0.0f); }
}

juce::ValueTree M252::toTree() const
{
    juce::ValueTree t ("RG252");
    for (int i = 0; i < kCells; ++i)
    {
        juce::String s;
        s << rgb[i].load() << " " << sub[i].load() << " " << subCurve[i].load() << " " << offset[i].load();
        for (int k = 0; k < 6; ++k) s << " " << cv[i][k].load();
        t.setProperty ("c" + juce::String (i), s, nullptr);
    }
    return t;
}

void M252::fromTree (const juce::ValueTree& t)
{
    if (! t.hasType ("RG252")) return;
    for (int i = 0; i < kCells; ++i)
    {
        const auto v = juce::StringArray::fromTokens (t.getProperty ("c" + juce::String (i)).toString(), " ", "");
        if (v.size() < 10) { clearCell (i, 3); continue; }
        rgb[i].store (v[0].getIntValue()); sub[i].store (v[1].getIntValue()); subCurve[i].store (v[2].getFloatValue()); offset[i].store (v[3].getFloatValue());
        for (int k = 0; k < 6; ++k) cv[i][k].store (v[4 + k].getFloatValue());
    }
}

void M252::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }

void M252::reset()
{
    for (auto& k : c) k = Clock();
    pR = pG = pB = pSub = pCell1 = 0;
}

void M252::beginBlock (int)
{
    bpm = juce::jlimit (20.0f, 300.0f, bpmP->load()); pdur = pdurP->load();
    sync = (int) std::lround (syncP->load()); play = playP->load() > 0.5f;
    for (int k = 0; k < 3; ++k)
    {
        clk[k] = (int) std::lround (clkP[k]->load());
        dt[k] = dtP[k]->load() > 0.5f; dts[k] = dtsP[k]->load();
    }
}

void M252::fire (int k, int ring, int cell, float cellSec) noexcept
{
    const int i = base (ring) + cell;
    const int bits = rgb[i].load (std::memory_order_relaxed);
    const int w = juce::jmax (1, (int) ((0.002f + pdur * juce::jmax (0.0f, cellSec - 0.002f)) * fs));
    if (bits & 1) pR = w;
    if (bits & 2) pG = w;
    if (bits & 4) pB = w;
    for (int o = 0; o < 6; ++o) { const float v = cv[i][o].load (std::memory_order_relaxed); if (v >= 0.0f) cvOut[o] = v; }
    if (k == 0 && cell == 0) pCell1 = (int) (0.005f * fs);
    // subdivisions: n extra pulses spread over the cell
    const int n = sub[i].load (std::memory_order_relaxed);
    c[k].subCell = n > 0 ? i : -1;
    c[k].subLeft = n;
    for (int col = 0; col < 3; ++col) if (bits & (1 << col)) ledColour[col].store (1.0f, std::memory_order_relaxed);
    if (k == 0 && cell == 0) ledColour[4].store (1.0f, std::memory_order_relaxed);
}

void M252::tick (const float* in, float* out) noexcept
{
    const bool rs = btnReset.load (std::memory_order_relaxed);
    if (rs && ! resetWas) for (auto& k : c) { k.pos = 0.0; k.lastFired = -100000; }
    resetWas = rs;
    const float beat = 60.0f / bpm;
    // master cycle (for "on 1")
    const int mRing = clk[0] >= 1 ? juce::jlimit (0, kRings - 1, clk[0] - 1 + (int) (in[In_Clk1Cv] / 10.0f * 11.0f)) : -1;
    const float masterCycle = (float) (mRing >= 0 ? kSize[mRing] : 4) * beat;
    for (int k = 0; k < 3; ++k)
    {
        auto& ck = c[k];
        int ring = clk[k] >= 1 ? juce::jlimit (0, kRings - 1, clk[k] - 1 + (int) (in[In_Clk1Cv + 2 * k] / 10.0f * 11.0f)) : -1;
        if (ring != ck.ring) { ck.ring = ring; ck.pos = 0.0; ck.lastFired = -100000; }
        const bool pl = in[In_Clk1Pulse + 2 * k] > 3.0f;
        if (pl && ! ck.pulseWas) { ck.pos = 0.0; ck.lastFired = -100000; }   // assign pulse: this clock restarts on cell 1
        ck.pulseWas = pl;
        playRing[k].store (ring, std::memory_order_relaxed);
        if (ring < 0 || ! play) { playCell[k].store (-1, std::memory_order_relaxed); continue; }
        const int n = kSize[ring];
        float cellSec = sync == 1 ? masterCycle / (float) n : beat;
        if (sync == 0 && dt[k]) cellSec /= juce::jlimit (0.5f, 3.0f, dts[k]);
        const double prev = ck.pos;
        ck.pos += 1.0 / (cellSec * fs);
        // cell triggers, shifted by behind / ahead (up to half a cell either way)
        const int cur = (int) std::floor (ck.pos);
        for (int j = cur - 1; j <= cur + 1; ++j)
        {
            if (j <= ck.lastFired) continue;
            const int cell = ((j % n) + n) % n;
            const double tj = (double) j + 0.5 * (double) offset[base (ring) + cell].load (std::memory_order_relaxed);
            if (tj >= prev && tj < ck.pos) { ck.lastFired = j; fire (k, ring, cell, cellSec); ck.subNext = (double) j; }
        }
        // subdivisions of the cell just fired
        if (ck.subCell >= 0 && ck.subLeft > 0)
        {
            const int nsub = sub[ck.subCell].load (std::memory_order_relaxed);
            const int m = nsub - ck.subLeft + 1;                            // 1..n
            const float curve = subCurve[ck.subCell].load (std::memory_order_relaxed);
            const float p = (float) m / (float) (nsub + 1);
            const float warped = std::pow (p, 1.0f / (1.0f + 3.0f * curve));   // clockwise: later and closer together
            if (ck.pos >= ck.subNext + (double) warped) { pSub = juce::jmax (1, (int) (0.003f * fs)); --ck.subLeft; ledColour[3].store (1.0f, std::memory_order_relaxed); }
        }
        if (ck.pos > 1.0e6) ck.pos -= (double) (n * 10000), ck.lastFired -= n * 10000, ck.subNext -= (double) (n * 10000);
        playCell[k].store (((cur % n) + n) % n, std::memory_order_relaxed);
    }
    out[Out_R] = pR > 0 ? 10.0f : 0.0f; out[Out_G] = pG > 0 ? 10.0f : 0.0f; out[Out_B] = pB > 0 ? 10.0f : 0.0f;
    out[Out_Sub] = pSub > 0 ? 10.0f : 0.0f; out[Out_Cell1] = pCell1 > 0 ? 10.0f : 0.0f;
    for (int o = 0; o < 6; ++o) { out[Out_Cv1 + o] = cvOut[o]; cvLed[o].store (cvOut[o], std::memory_order_relaxed); }
    if (pR > 0) --pR; if (pG > 0) --pG; if (pB > 0) --pB; if (pSub > 0) --pSub; if (pCell1 > 0) --pCell1;
    const float d = 1.0f - 1.0f / (0.08f * fs);
    for (auto& l : ledColour) l.store (l.load (std::memory_order_relaxed) * d, std::memory_order_relaxed);
}

float M252::led (int id) const noexcept
{
    // 0..4 pulse output flashes; 10 + k*11 + r: clock k assigned to ring r; 50 + k: delta-t; 60..63 display layer;
    // 64, 65 sync; 66, 67 rhythm select; 68 edit; 69 play; 70 remote; 80 + o (o 0..5): c.v. output o has a value
    if (id >= 0 && id < 5) return ledColour[id].load (std::memory_order_relaxed);
    if (id >= 10 && id < 43) { const int k = (id - 10) / 11, r = (id - 10) % 11; return clk[k] == r + 1 ? 1.0f : 0.0f; }
    if (id >= 50 && id < 53) return dtP[id - 50]->load() > 0.5f ? 1.0f : 0.0f;
    if (id >= 60 && id < 64) return (int) std::lround (layerP->load()) == id - 60 ? 1.0f : 0.0f;
    if (id == 64) return (int) std::lround (syncP->load()) == 1 ? 1.0f : 0.0f;
    if (id == 65) return (int) std::lround (syncP->load()) == 2 ? 1.0f : 0.0f;
    if (id == 66) return euclidP->load() > 0.5f ? 0.0f : 1.0f;
    if (id == 67) return euclidP->load() > 0.5f ? 1.0f : 0.0f;
    if (id == 68) return editP->load() > 0.5f ? 1.0f : 0.0f;
    if (id == 69) return playP->load() > 0.5f ? 1.0f : 0.0f;
    if (id == 70) return remP->load() > 0.5f ? 1.0f : 0.0f;
    if (id >= 80 && id < 86) return std::fabs (cvLed[id - 80].load (std::memory_order_relaxed)) > 0.01f ? 0.8f : 0.0f;
    return 0.0f;
}
} // namespace wc
