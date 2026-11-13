#include "M275.h"

namespace wc
{
M275::M275 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    for (int c = 0; c < 2; ++c)
    {
        const juce::String n (c + 1);
        trebP[c] = apvts.getRawParameterValue (p + "treb" + n); bassP[c] = apvts.getRawParameterValue (p + "bass" + n);
        mixP[c] = apvts.getRawParameterValue (p + "mix" + n); recP[c] = apvts.getRawParameterValue (p + "rec" + n);
        hotP[c] = apvts.getRawParameterValue (p + "hot" + n);
        jassert (trebP[c] && bassP[c] && mixP[c] && recP[c] && hotP[c]);
    }
}

void M275::Tank::prepare (float fs, float spread)
{
    static const float combMs[4] = { 29.7f, 37.1f, 41.1f, 43.7f }, apMs[2] = { 5.0f, 1.7f };
    for (int i = 0; i < 4; ++i)
    {
        combLen[i] = juce::jmax (8, (int) ((combMs[i] + spread) * 0.001f * fs));
        comb[i].b.assign ((size_t) combLen[i] + 2, 0.0f); comb[i].w = 0;
    }
    for (int i = 0; i < 2; ++i)
    {
        apLen[i] = juce::jmax (4, (int) ((apMs[i] + 0.3f * spread) * 0.001f * fs));
        ap[i].b.assign ((size_t) apLen[i] + 2, 0.0f); ap[i].w = 0;
    }
    clear();
}

void M275::Tank::clear()
{
    for (auto& l : comb) std::fill (l.b.begin(), l.b.end(), 0.0f);
    for (auto& l : ap) std::fill (l.b.begin(), l.b.end(), 0.0f);
    for (auto& z : combLp) z = 0.0f;
    for (auto& z : disp) z = 0.0f;
}

float M275::Tank::process (float x) noexcept
{
    // dispersion: a cascade of first-order allpasses smears the transient into the "boing" of a spring
    float y = x;
    for (auto& z : disp)
    {
        const float a = 0.62f;
        const float o = -a * y + z;
        z = y + a * o;
        y = o;
    }
    float sum = 0.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float d = comb[i].read (combLen[i]);
        combLp[i] += (d - combLp[i]) * 0.42f;          // high-frequency damping in the loop
        comb[i].push (y + combLp[i] * 0.84f);
        sum += d;
    }
    float o = sum * 0.25f;
    for (int i = 0; i < 2; ++i)
    {
        const float d = ap[i].read (apLen[i]);
        const float v = o + 0.5f * d;
        ap[i].push (v);
        o = d - 0.5f * v;
    }
    return o;
}

void M275::prepare (double sampleRate)
{
    fs = (float) sampleRate;
    tank[0].prepare (fs, 0.0f);
    tank[1].prepare (fs, 2.3f);
    for (auto& d : darken) d.set (2500.0f, fs);
    for (auto& r : sMix) r = 0.0f;
    rampLen = juce::jmax (1, (int) (0.012f * fs));
    lastTreb[0] = lastTreb[1] = lastBass[0] = lastBass[1] = 99.0f;
    reset();
}

void M275::reset()
{
    for (int c = 0; c < 2; ++c) { tank[c].clear(); treb[c].reset(); bass[c].reset(); darken[c].reset(); }
    first = true;
}

void M275::beginBlock (int numSamples)
{
    for (int c = 0; c < 2; ++c)
    {
        const float t = trebP[c]->load(), b = bassP[c]->load();
        // the panel is calibrated in dB AT 10 kHz and AT 50 Hz, so each shelf turns well before its label:
        // an RBJ shelf passes through half its gain at the corner and reaches the full amount about two octaves on
        // the shelf gains glide 12 ms, as the 292; tick redraws the shelf while they move
        if (first) { trebDb[c].snap (t); bassDb[c].snap (b); } else { trebDb[c].to (t, rampLen); bassDb[c].to (b, rampLen); }
        mix[c] = mixP[c]->load();
        recede[c] = recP[c]->load() > 0.5f;
        hot[c] = hotP[c]->load() > 0.5f;
        juce::ignoreUnused (numSamples);
    }
    first = false;
}

void M275::tick (const float* in, float* out) noexcept
{
    for (int c = 0; c < 2; ++c)
    {
        const int ai = c == 0 ? In_Audio1 : In_Audio2, ci = c == 0 ? In_MixCv1 : In_MixCv2;
        const float x = in[ai] * (hot[c] ? 0.32f : 1.0f);
        // shortcut: the shelf is redrawn every sample while its gain glides (until the ramp lands), fine for four
        // shelves; interpolate the coefficients instead if it ever shows in a profile
        const float t = trebDb[c].next(), b = bassDb[c].next();
        if (t != lastTreb[c]) { treb[c].shelf (true, juce::jmin (2800.0f, 0.4f * fs), t, fs); lastTreb[c] = t; }
        if (b != lastBass[c]) { bass[c].shelf (false, 180.0f, b, fs); lastBass[c] = b; }
        const float eq = treb[c].process (bass[c].process (x));
        const float wet = 1.6f * tank[c].process (eq);
        float m = mix[c] + (isPatched (ci) ? in[ci] / 10.0f : 0.0f);
        m = dsp::clampf (m, 0.0f, 1.0f);
        // the same arithmetic the old linear Ramp did when re-aimed every sample (bit for bit): a one-pole, not a ramp
        sMix[c] += (m - sMix[c]) / 64.0f;
        const float mm = sMix[c];
        const float dark = darken[c].process (eq);
        const float dry = recede[c] ? (1.0f - 0.8f * mm) * (eq + (dark - eq) * mm) : eq;
        const float vc = dry + mm * wet;
        out[c == 0 ? Out_Mix1a : Out_Mix2a] = out[c == 0 ? Out_Mix1b : Out_Mix2b] = vc;
        out[c == 0 ? Out_Eq1 : Out_Eq2] = eq;
        out[c == 0 ? Out_Rev1 : Out_Rev2] = wet;
    }
}
} // namespace wc
