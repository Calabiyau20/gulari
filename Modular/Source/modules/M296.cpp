#include "M296.h"

namespace wc
{
constexpr float M296::kCentre[];

M296::M296 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    for (int b = 0; b < kBands; ++b) sliderP[b] = apvts.getRawParameterValue (p + "att" + juce::String (b));
    decayP = apvts.getRawParameterValue (p + "decay"); freqP = apvts.getRawParameterValue (p + "freq");
    fcvP = apvts.getRawParameterValue (p + "fcv"); widthP = apvts.getRawParameterValue (p + "width");
    preEP = apvts.getRawParameterValue (p + "pre_even"); preOP = apvts.getRawParameterValue (p + "pre_odd");
    trEP = apvts.getRawParameterValue (p + "tr_even"); trOP = apvts.getRawParameterValue (p + "tr_odd");
}

void M296::prepare (double sampleRate)
{
    fs = (float) sampleRate;
    for (int b = 0; b < kBands; ++b)
    {
        const float c = kCentre[b];
        if (b == 0) { f[b][0].set (c, 0.7071f, fs); f[b][1].set (c, 0.7071f, fs); f[b][2].set (c, 0.5f, fs); }
        else if (b == kBands - 1) { f[b][0].set (c, 0.7071f, fs); f[b][1].set (c, 0.7071f, fs); f[b][2].set (c, 0.5f, fs); }
        else
        {
            // three stagger-tuned stages around the centre: about a third of an octave of flat top
            f[b][0].set (c * 0.89f, 4.5f, fs); f[b][1].set (c, 4.5f, fs); f[b][2].set (c * 1.12f, 4.5f, fs);
        }
        vcaLag[b].set (80.0f, fs);
    }
    // measure each channel's gain at its centre and normalise to 0 dB
    for (int b = 0; b < kBands; ++b)
    {
        Svf t[3] = { f[b][0], f[b][1], f[b][2] };
        for (auto& x : t) x.reset();
        const float fc = b == 0 ? 50.0f : (b == kBands - 1 ? 14000.0f : kCentre[b]);
        double pk = 0.0;
        const int n = (int) (fs * 0.2f);
        for (int i = 0; i < n; ++i)
        {
            float y = std::sin (dsp::kTwoPi * fc * (float) i / fs), lp, bp, hp;
            for (auto& x : t) { x.run (y, lp, bp, hp); y = b == 0 ? lp : (b == kBands - 1 ? hp : bp * x.k); }
            if (i > n / 2) pk = juce::jmax (pk, (double) std::fabs (y));
        }
        norm[b] = pk > 1.0e-6 ? (float) (1.0 / pk) : 1.0f;
    }
    att = 1.0f - std::exp (-1.0f / (0.002f * fs));
    reset();
}

void M296::reset()
{
    for (auto& band : f) for (auto& x : band) x.reset();
    for (auto& e : env) e = 0.0f;
}

void M296::beginBlock (int)
{
    for (int b = 0; b < kBands; ++b)
    {
        const float k = sliderP[b]->load();
        bandGain[b] = k < 0.02f ? 0.0f : std::pow (10.0f, sliderDb (k) / 20.0f);
    }
    const int d = juce::jlimit (0, 2, (int) std::lround (decayP->load()));   // 0 short, 1 combo, 2 long
    for (int b = 0; b < kBands; ++b)
    {
        const float t = d == 0 ? 0.015f : (d == 2 ? 0.25f : 0.25f * std::pow (0.06f, (float) b / 15.0f));   // combo: long lows, short highs
        rel[b] = 1.0f - std::exp (-1.0f / (t * fs));
    }
    freqK = freqP->load(); fcvK = fcvP->load(); widthK = widthP->load();
    preE = preEP->load(); preO = preOP->load();
    trE = trEP->load() > 0.5f; trO = trOP->load() > 0.5f;
}

void M296::tick (const float* in, float* out) noexcept
{
    const float xa = in[In_SigAll], xe = in[In_SigEven], xo = in[In_SigOdd];
    // program window: centre channel from frequency (100 Hz .. 10 kHz over the knob, c.v. 1 V = 2 channels) and width
    const float centre = 15.0f * freqK + fcvK * 2.0f * in[In_FreqCv];
    const float width = juce::jmax (0.5f, 16.0f * widthK + 1.6f * in[In_WidthCv]);
    float combE = 0, combO = 0, attE = 0, attO = 0, progE = 0, progO = 0;
    float band[kBands];
    for (int b = 0; b < kBands; ++b)
    {
        float y = xa + ((b & 1) ? xo : xe), lp, bp, hp;
        for (auto& s : f[b]) { s.run (y, lp, bp, hp); y = b == 0 ? lp : (b == kBands - 1 ? hp : bp * s.k); }
        y *= norm[b];
        band[b] = y;
        const float r = std::fabs (y) * 10.0f;
        env[b] += (r - env[b]) * (r > env[b] ? att : rel[b]);
    }
    for (int b = 0; b < kBands; ++b)
    {
        const float y = band[b];
        const float e = juce::jmin (10.0f, env[b]);                   // peak follower: a 1.0 sine = 10 V
        out[Out_Env0 + b] = e;
        const float ya = y * bandGain[b];
        out[Out_Band0 + b] = ya;
        if (b & 1) { combO += y; attO += ya; } else { combE += y; attE += ya; }
        // VCA: window (soft edges, one channel wide) + local program input; or the neighbour's envelope (transfer)
        const float dist = std::fabs ((float) b - centre) - 0.5f * width;
        float g = dsp::clampf (0.5f - dist, 0.0f, 1.0f);
        if (isPatched (In_Local0 + b)) g = dsp::clampf (g + in[In_Local0 + b] / 10.0f, 0.0f, 1.0f);
        const bool odd = (b & 1) != 0;
        if ((odd && trO) || (! odd && trE))
        {
            const int nb = odd ? b - 1 : juce::jmin (kBands - 1, b + 1);      // the other half of the pair
            const float pre = odd ? preO : preE;
            const float tilt = 1.0f + 3.0f * pre * (float) nb / 15.0f;
            g *= dsp::clampf (out[Out_Env0 + nb] * tilt / 10.0f * 2.0f, 0.0f, 1.0f);
        }
        vca[b] = vcaLag[b].process (g);
        const float yp = y * vca[b];
        if (odd) progO += yp; else progE += yp;
    }
    out[Out_CombEven] = combE; out[Out_CombOdd] = combO;
    out[Out_AttEven] = attE; out[Out_AttOdd] = attO; out[Out_AttAll] = attE + attO;
    out[Out_ProgEven] = progE; out[Out_ProgOdd] = progO; out[Out_ProgAll] = progE + progO;
    const float k = 1.0f - 1.0f / (0.15f * fs);
    inPeak[0] = juce::jmax (std::fabs (xe), inPeak[0] * k); inPeak[1] = juce::jmax (std::fabs (xa), inPeak[1] * k); inPeak[2] = juce::jmax (std::fabs (xo), inPeak[2] * k);
    for (int i = 0; i < 3; ++i) leds[(size_t) i].store (juce::jmin (1.0f, inPeak[i]), std::memory_order_relaxed);
    for (int b = 0; b < kBands; ++b) leds[(size_t) (Led_Band0 + b)].store (vca[b], std::memory_order_relaxed);
}
} // namespace wc
