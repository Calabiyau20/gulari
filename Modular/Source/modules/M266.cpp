#include "M266.h"

namespace wc
{
M266::M266 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    rateP = apvts.getRawParameterValue (p + "rate");
    rate2P = apvts.getRawParameterValue (p + "rate2");
    smoothP = apvts.getRawParameterValue (p + "smooth");
    stepsP = apvts.getRawParameterValue (p + "steps");
    distP = apvts.getRawParameterValue (p + "dist");
    jassert (rateP && rate2P && smoothP && stepsP && distP);
}

void M266::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }

void M266::reset()
{
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
    fl[0] = fl[1] = Fluct();
    shWas = qWas = sWas = false;
    pulseA = pulseB = 0;
}

void M266::beginBlock (int)
{
    rateK = rateP->load(); rate2K = rate2P->load(); smoothK = smoothP->load(); stepsK = stepsP->load(); distK = distP->load();
}

// distribution of the right stored output: CCW favours low voltages, centre is a bell curve, CW favours high
float M266::shaped (float d) noexcept
{
    const float bell = (uni() + uni() + uni()) / 3.0f;
    if (d < 0.5f)
    {
        const float low = std::pow (uni(), 3.0f);
        return dsp::lerp (low, bell, d / 0.5f);
    }
    const float high = 1.0f - std::pow (uni(), 3.0f);
    return dsp::lerp (bell, high, (d - 0.5f) / 0.5f);
}

void M266::tick (const float* in, float* out) noexcept
{
    // ---- noise: white, pink (-3 dB/oct, Kellet filter), blue (+3 dB/oct) ----
    const float w = uni() * 2.0f - 1.0f;
    b0 = 0.99886f * b0 + w * 0.0555179f;
    b1 = 0.99332f * b1 + w * 0.0750759f;
    b2 = 0.96900f * b2 + w * 0.1538520f;
    b3 = 0.86650f * b3 + w * 0.3104856f;
    b4 = 0.55000f * b4 + w * 0.5329522f;
    b5 = -0.7616f * b5 - w * 0.0168980f;
    const float pink = (b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f) * 0.11f;
    b6 = w * 0.115926f;
    // blue (+3 dB/oct) = derivative of pink (-3 dB/oct + 6 dB/oct)
    const float blue = (pink - lastWhite) * 2.2f;
    lastWhite = pink;
    out[Out_White] = 0.7f * w;
    out[Out_Pink] = pink;
    out[Out_Blue] = blue;

    // ---- fluctuating random voltages (two channels): new levels arrive at random times, at a probable rate ----
    for (int c = 0; c < 2; ++c)
    {
        auto& f = fl[c];
        const float rate = fluctHz (dsp::clampf ((c == 0 ? rateK : rate2K) + in[c == 0 ? In_Fluct1Cv : In_Fluct2Cv] / 10.0f, 0.0f, 1.0f));
        if (uni() < rate / fs) f.target = 10.0f * uni();
        // two smoothing stages tied to the rate: slow settings drift, fast ones jitter
        const float k = 1.0f - std::exp (-dsp::kTwoPi * rate * 0.6f / fs);
        f.slew += (f.target - f.slew) * k;
        f.value += (f.slew - f.value) * k;
        out[c == 0 ? Out_Fluct1 : Out_Fluct2] = f.value;
        leds[(size_t) (c == 0 ? Led_Fluct1 : Led_Fluct2)].store (f.value / 10.0f, std::memory_order_relaxed);
    }

    // ---- sample & hold with alternating outputs ----
    const bool shHigh = in[In_ShPulse] > 3.0f;
    if (shHigh && ! shWas)
    {
        sh = in[In_ShCv];
        shToggle = ! shToggle;
        if (shToggle) { shA = sh; pulseA = (int) (0.005f * fs); }
        else { shB = sh; pulseB = (int) (0.005f * fs); }
    }
    shWas = shHigh;
    out[Out_Sh] = sh;
    out[Out_ShAltA] = shA;
    out[Out_ShAltB] = shB;
    out[Out_ShPulseA] = pulseA > 0 ? 10.0f : 0.0f;
    out[Out_ShPulseB] = pulseB > 0 ? 10.0f : 0.0f;
    if (pulseA > 0) --pulseA;
    if (pulseB > 0) --pulseB;

    // ---- integrator (RC lag): 0.01 s = no smoothing, 10 s = long glissandi ----
    const float t = smoothSec (dsp::clampf (smoothK + in[In_IntCv] / 10.0f, 0.0f, 1.0f));
    integ += (in[In_IntIn] - integ) * (1.0f - std::exp (-1.0f / (t * 0.25f * fs)));
    out[Out_Int] = integ;

    // ---- quantized random voltages: each pulse picks new levels on both scales ----
    const bool qHigh = in[In_QPulse] > 3.0f;
    if (qHigh && ! qWas)
    {
        const int n = juce::jlimit (1, 6, (int) std::lround (stepsK + in[In_QCv] * 0.5f));
        const int states = 1 << n;                                               // 2^n states across 10 V
        quant2n = 10.0f * (float) juce::jmin (states - 1, (int) (uni() * (float) states)) / (float) (states - 1);
        quantn1 = 5.0f * (float) juce::jmin (n, (int) (uni() * (float) (n + 1))) / (float) n;   // n+1 states within 5 V
        flash[0] = flash[1] = 1.0f;
    }
    qWas = qHigh;
    out[Out_Q2n] = quant2n;
    out[Out_Qn1] = quantn1;

    // ---- stored random voltages ----
    const bool sHigh = in[In_SPulse] > 3.0f;
    if (sHigh && ! sWas)
    {
        storedU = 10.0f * uni();
        storedD = 10.0f * shaped (dsp::clampf (distK + in[In_SCv] / 10.0f, 0.0f, 1.0f));
        flash[2] = flash[3] = 1.0f;
    }
    sWas = sHigh;
    out[Out_StoredUni] = storedU;
    out[Out_StoredDist] = storedD;
    // LEDs: a flash on every new value, fading over ~150 ms, on top of the level
    const float fade = 1.0f - 1.0f / (0.15f * fs);
    const float lv[4] = { quant2n / 10.0f, quantn1 / 5.0f, storedU / 10.0f, storedD / 10.0f };
    for (int i = 0; i < 4; ++i)
    {
        flash[i] *= fade;
        leds[(size_t) (Led_Q2n + i)].store (juce::jmax (flash[i], 0.25f * lv[i]), std::memory_order_relaxed);
    }
}
} // namespace wc
