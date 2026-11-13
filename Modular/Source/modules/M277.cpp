#include "M277.h"

namespace wc
{
M277::M277 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    for (int i = 0; i < 3; ++i) lvlP[i] = apvts.getRawParameterValue (p + "lvl" + juce::String::charToString ((juce::juce_wchar) ('a' + i)));
    timeP = apvts.getRawParameterValue (p + "time"); ctlP = apvts.getRawParameterValue (p + "ctl");
    modP = apvts.getRawParameterValue (p + "mod"); longP = apvts.getRawParameterValue (p + "long");
}

void M277::prepare (double sampleRate)
{
    fs = (float) sampleRate;
    buf.assign ((size_t) (fs * 3.5f) + 8, 0.0f);    // up to 0.8 s x 4 (c.v.) + margin
    slew = 1.0f - std::exp (-1.0f / (0.03f * fs));
    rampLen = juce::jmax (1, (int) (0.012f * fs));
    reset();
}

void M277::reset() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; for (auto& f : lp) f.reset(); first = true; }

void M277::beginBlock (int)
{
    for (int i = 0; i < 3; ++i) { if (first) lvl[i].snap (lvlP[i]->load()); else lvl[i].to (lvlP[i]->load(), rampLen); }   // 12 ms, as the 292
    first = false;
    longMode = longP->load() > 0.5f;
    tBase = timeSec (timeP->load(), longMode);
    ctl = ctlP->load(); modK = modP->load();
    const float fc = longMode ? 4500.0f : 9000.0f;   // clock-limited bandwidth of the delay line
    for (auto& f : lp) f.set (fc, fs);
}

float M277::read (float d) const noexcept
{
    const int n = (int) buf.size();
    const float pos = (float) w - d;
    int i = (int) std::floor (pos);
    const float f = pos - (float) i;
    auto at = [&] (int k) { k %= n; if (k < 0) k += n; return buf[(size_t) k]; };
    const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
    const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + y1;
}

void M277::tick (const float* in, float* out) noexcept
{
    float x = in[In_A] * lvl[0].next() + in[In_B] * lvl[1].next() + in[In_C] * lvl[2].next();
    const float ax = std::fabs (x);
    ovlHold = ax > 1.0f ? 1.0f : ovlHold * (1.0f - 1.0f / (0.2f * fs));
    ovl.store (ovlHold, std::memory_order_relaxed);
    x = std::tanh (x);                                            // the input stage saturates
    buf[(size_t) w] = x;
    const float tTarget = tBase * std::exp2 (-ctl * in[In_Control] / 5.0f) * (1.0f + 0.5f * modK * in[In_Mod]);
    tSmooth += (juce::jlimit (0.002f, 3.2f, tTarget) - tSmooth) * slew;
    const float d = tSmooth * fs;
    for (int k = 0; k < 4; ++k) out[Out_Q1 + k] = lp[k].process (read (juce::jmax (1.0f, d * (float) (k + 1) * 0.25f)));
    if (++w >= (int) buf.size()) w = 0;
}
} // namespace wc
