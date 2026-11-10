#include "M266.h"

namespace wc
{
M266::M266 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    rateP = apvts.getRawParameterValue (p + "rate");
    smoothP = apvts.getRawParameterValue (p + "smooth");
    stepsP = apvts.getRawParameterValue (p + "steps");
    qmodeP = apvts.getRawParameterValue (p + "qmode");
    distP = apvts.getRawParameterValue (p + "dist");
    jassert (rateP && smoothP && stepsP && qmodeP && distP);
}

void M266::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }

void M266::reset()
{
    b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f;
    fTarget = fValue = fSlew = 5.0f;
    shWas = qWas = sWas = false;
    pulseA = pulseB = 0;
}

void M266::beginBlock (int)
{
    rateK = rateP->load(); smoothK = smoothP->load(); stepsK = stepsP->load(); distK = distP->load();
    pow2 = qmodeP->load() > 0.5f;
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

    // ---- fluctuating random voltages: new levels arrive at random times, at a probable rate ----
    const float rate = fluctHz (dsp::clampf (rateK + in[In_FluctCv] / 10.0f, 0.0f, 1.0f));
    if (uni() < rate / fs) fTarget = 10.0f * uni();
    // two smoothing stages, time constant tied to the rate, so slow settings drift and fast ones jitter
    const float k = 1.0f - std::exp (-dsp::kTwoPi * rate * 0.6f / fs);
    fSlew += (fTarget - fSlew) * k;
    fValue += (fSlew - fValue) * k;
    out[Out_Fluct] = fValue;
    fluctLed.store (fValue / 10.0f, std::memory_order_relaxed);

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

    // ---- quantized random voltages ----
    const bool qHigh = in[In_QPulse] > 3.0f;
    if (qHigh && ! qWas)
    {
        const int n = juce::jlimit (1, 6, (int) std::lround (stepsK + in[In_QCv] * 0.5f));
        if (! pow2)
        {
            const int kq = juce::jmin (n, (int) (uni() * (float) (n + 1)));       // n+1 states within 5 V
            quant = 5.0f * (float) kq / (float) n;
        }
        else
        {
            const int states = 1 << n;                                           // 2^n states across 10 V
            const int kq = juce::jmin (states - 1, (int) (uni() * (float) states));
            quant = 10.0f * (float) kq / (float) (states - 1);
        }
    }
    qWas = qHigh;
    out[Out_Quant] = quant;

    // ---- stored random voltages ----
    const bool sHigh = in[In_SPulse] > 3.0f;
    if (sHigh && ! sWas)
    {
        storedU = 10.0f * uni();
        storedD = 10.0f * shaped (dsp::clampf (distK + in[In_SCv] / 10.0f, 0.0f, 1.0f));
    }
    sWas = sHigh;
    out[Out_StoredUni] = storedU;
    out[Out_StoredDist] = storedD;
}
} // namespace wc
