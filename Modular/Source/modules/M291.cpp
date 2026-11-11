#include "M291.h"

namespace wc
{
static const char* kNode291[3] = { "a", "b", "c" };

M291::M291 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    for (int s = 0; s < kStages; ++s)
    {
        const juce::String sn (s + 1);
        for (int n = 0; n < kNodes; ++n)
        {
            const juce::String suf = sn + "_" + kNode291[n];
            fP[s][n] = apvts.getRawParameterValue (p + "f_" + suf);
            bwP[s][n] = apvts.getRawParameterValue (p + "bw_" + suf);
            ampP[s][n] = apvts.getRawParameterValue (p + "amp_" + suf);
            jassert (fP[s][n] && bwP[s][n] && ampP[s][n]);
        }
        timeP[s] = apvts.getRawParameterValue (p + "time_" + sn);
    }
    nodeP = apvts.getRawParameterValue (p + "node"); editP = apvts.getRawParameterValue (p + "edit");
    playP = apvts.getRawParameterValue (p + "play"); indexP = apvts.getRawParameterValue (p + "index");
    modP = apvts.getRawParameterValue (p + "modtype"); soloP = apvts.getRawParameterValue (p + "solo");
    remP = apvts.getRawParameterValue (p + "remote");
}

void M291::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }
void M291::reset() { for (auto& x : filt) x.reset(); from = to = 0; t = 0.0; }

void M291::beginBlock (int)
{
    for (int s = 0; s < kStages; ++s)
    {
        for (int n = 0; n < kNodes; ++n) { f[s][n] = fP[s][n]->load(); bwv[s][n] = bwP[s][n]->load(); amp[s][n] = ampP[s][n]->load(); }
        tm[s] = timeP[s]->load();
    }
    node = juce::jlimit (0, 2, (int) std::lround (nodeP->load()));
    edit = juce::jlimit (0, kStages - 1, (int) std::lround (editP->load()) - 1);
    play = playP->load() > 0.5f;
    index = indexP->load();
    modType = juce::jlimit (0, 2, (int) std::lround (modP->load()));
    solo = soloP->load() > 0.5f;
}

void M291::tick (const float* in, float* out) noexcept
{
    // ---- where are we between the stages? ----
    const bool adv = in[In_Adv] > 3.0f;
    const bool advEdge = adv && ! advWas;
    advWas = adv;
    float pos;
    if (isPatched (In_StageCv))
    {
        pos = dsp::clampf (in[In_StageCv] / 10.0f, 0.0f, 1.0f) * (float) (kStages - 1);   // 0..10 V sweeps stage 1..8
        from = (int) pos; to = juce::jmin (kStages - 1, from + 1);
    }
    else
    {
        if (play && ! playWas) { from = to = edit; t = 0.0; dur = 0.0f; }
        playWas = play;
        if (! play && edit != lastEdit) { from = to = edit; t = 0.0; dur = 0.0f; }   // stopped: show the stage being edited
        lastEdit = edit;
        if (advEdge || (play && t >= (double) dur))
        {
            from = to; to = (to + 1) % kStages;
            t = 0.0; dur = stageTime (tm[from]);
        }
        if (t < (double) dur) t += 1.0 / fs;
        const float x = dur > 0.0f ? (float) juce::jmin (1.0, t / (double) dur) : 1.0f;
        if (x >= 1.0f && ! play) from = to;          // stopped after an advance: rest on the new stage
        pos = (float) from + (to == from ? 0.0f : x);
    }
    const float frac = pos - std::floor (pos);
    const int s0 = juce::jlimit (0, kStages - 1, from), s1 = juce::jlimit (0, kStages - 1, to);
    for (int n = 0; n < kNodes; ++n)
    {
        cur[0][n] = f[s0][n] + (f[s1][n] - f[s0][n]) * frac;
        cur[1][n] = bwv[s0][n] + (bwv[s1][n] - bwv[s0][n]) * frac;
        cur[2][n] = amp[s0][n] + (amp[s1][n] - amp[s0][n]) * frac;
    }
    if ((ctr & 63) == 0) position.store (pos, std::memory_order_relaxed);

    // ---- the three nodes ----
    const bool upd = (ctr++ & 7) == 0;
    float all = 0.0f;
    for (int n = 0; n < kNodes; ++n)
    {
        const float mod = in[In_ModA + n] + in[In_ModAll];
        const float fcv = in[In_FreqA + n], bwcv = in[In_BwA + n] + in[In_BwAll], acv = in[In_AmpA + n];
        float hz = freqHz (cur[0][n]) * std::exp2 (dsp::clampf (fcv, -12.0f, 12.0f) / 1.2f);
        float bw = dsp::clampf (cur[1][n] + bwcv / 10.0f, 0.0f, 1.0f);
        float a = dsp::clampf (cur[2][n] + acv / 10.0f, 0.0f, 1.0f);
        if (modType == 2) hz *= std::exp2 (3.0f * index * mod);
        if (modType == 1) bw = dsp::clampf (bw + 0.5f * index * mod, 0.0f, 1.0f);
        if (modType == 0) a = dsp::clampf (a * (1.0f + 1.5f * index * mod), 0.0f, 2.0f);
        const float q = 0.7f + 50.0f * (1.0f - bw) * (1.0f - bw);
        if (upd || modType != 0) filt[n].set (hz, q, fs);
        const float x = in[In_SigA + n] + in[In_SigAll];
        const float y = std::tanh (filt[n].bp (x) * a * (0.8f + 0.12f * std::sqrt (q)));
        out[Out_A + n] = y;
        if (! solo || n == node) all += y;
    }
    out[Out_All1] = out[Out_All2] = all + in[In_Expand];
}

float M291::led (int id) const noexcept
{
    const float pos = position.load (std::memory_order_relaxed);
    const int nd = juce::jlimit (0, 2, (int) std::lround (nodeP->load()));
    const int ed = juce::jlimit (0, kStages - 1, (int) std::lround (editP->load()) - 1);
    const bool pl = playP->load() > 0.5f;
    if (id >= 0 && id < kStages)
    {
        const int s = (int) std::floor (pos + 0.0001f) % kStages;
        if (id == s) return 1.0f;
        if (! pl && id == ed) return (juce::Time::getMillisecondCounter() / 300) % 2 ? 0.6f : 0.15f;
        return 0.0f;
    }
    if (id == 8) return pl ? 1.0f : 0.0f;
    if (id >= 10 && id <= 12) return id - 10 == nd ? 1.0f : 0.0f;
    if (id >= 20 && id <= 22) return (int) std::lround (modP->load()) == id - 20 ? 1.0f : 0.0f;
    if (id == 23) return soloP->load() > 0.5f ? 1.0f : 0.0f;
    if (id == 24) return remP->load() > 0.5f ? 1.0f : 0.0f;
    auto ring = [] (float v, int k, int n) { const float x = v * (float) (n - 1); return x >= (float) k - 0.5f ? 1.0f : (x > (float) k - 1.0f ? 0.4f : 0.0f); };
    if (id >= 30 && id < 36) return ring (ampP[ed][nd]->load(), id - 30, 6);
    if (id >= 40 && id < 46) return ring (bwP[ed][nd]->load(), id - 40, 6);
    if (id >= 50 && id < 58) return ring (fP[ed][nd]->load(), id - 50, 8);
    if (id >= 60 && id < 66) return ring (indexP->load(), id - 60, 6);
    return 0.0f;
}
} // namespace wc
