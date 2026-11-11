#include "M267.h"

namespace wc
{
M267::M267 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    const char* ab[2] = { "a", "b" };
    for (int i = 0; i < 2; ++i)
    {
        rateP[i] = apvts.getRawParameterValue (p + "rate" + ab[i]); randP[i] = apvts.getRawParameterValue (p + "rand" + ab[i]);
        freqP[i] = apvts.getRawParameterValue (p + "freq" + ab[i]); bwP[i] = apvts.getRawParameterValue (p + "bw" + ab[i]);
        idxP[i] = apvts.getRawParameterValue (p + "idx" + ab[i]); modP[i] = apvts.getRawParameterValue (p + "mod" + ab[i]);
        jassert (rateP[i] && randP[i] && freqP[i] && bwP[i] && idxP[i] && modP[i]);
    }
    remP = apvts.getRawParameterValue (p + "remote");
    white.seed (0x2671u); chance.seed (0xc0ffeeu);
}

void M267::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }

void M267::reset()
{
    for (auto& f : filt) f.reset();
    r[0] = Rnd(); r[1] = Rnd();
    blueZ = 0.0f;
}

void M267::beginBlock (int)
{
    for (int i = 0; i < 2; ++i)
    {
        rate[i] = rateP[i]->load(); rnd[i] = randP[i]->load(); freq[i] = freqP[i]->load(); bw[i] = bwP[i]->load();
        idx[i] = idxP[i]->load(); mode[i] = (int) std::lround (modP[i]->load());
    }
}

void M267::tick (const float* in, float* out) noexcept
{
    // noise sources (audio level, about +-1)
    const float w = white.next();
    const float pk = pink.process (w);
    const float bl = (pk - blueZ) * 2.2f;   // +3 dB/oct = the derivative (+6) of the -3 dB/oct source
    blueZ = pk;
    out[Out_Low] = pk; out[Out_Flat1] = out[Out_Flat2] = 0.6f * w; out[Out_High] = bl;

    // random voltages
    for (int i = 0; i < 2; ++i)
    {
        auto& s = r[i];
        const int pi = i == 0 ? In_ProbA : In_ProbB, si = i == 0 ? In_SmplA : In_SmplB, ei = i == 0 ? In_ExtA : In_ExtB;
        const float hz = rateHz (rate[i]) * std::exp2 (dsp::clampf (in[pi], -10.0f, 10.0f) * 0.5f);
        // a change happens with probability hz / fs per sample (Poisson): the "probable rate of change"
        bool changed = false;
        if (chance.uni() < hz / fs) { s.target = 10.0f * chance.uni(); changed = true; }
        // the slew follows the rate: slow sources glide, fast ones step
        const float coef = 1.0f - std::exp (-dsp::kTwoPi * juce::jmin (2000.0f, 4.0f * hz) / fs);
        s.value += (s.target - s.value) * coef;
        const float ext = isPatched (ei) ? in[ei] : 5.0f;
        const float mix = ext + (s.value - ext) * rnd[i];
        const bool smpl = in[si] > 3.0f;
        if (isPatched (si)) { if (smpl && ! s.smplWas) s.sh = mix; }
        else if (changed) s.sh = ext + (s.target - ext) * rnd[i];
        s.smplWas = smpl;
        out[i == 0 ? Out_MixA : Out_MixB] = mix;
        out[i == 0 ? Out_ShA : Out_ShB] = s.sh;
        ledV[2 * i].store (mix * 0.1f, std::memory_order_relaxed);
        ledV[2 * i + 1].store (s.sh * 0.1f, std::memory_order_relaxed);
    }

    // filters: the coefficients are refreshed every 8 samples (their c.v. inputs are control rate)
    float sigIn[2];
    sigIn[0] = isPatched (In_SigA) ? in[In_SigA] : out[Out_Flat1];
    sigIn[1] = isPatched (In_SigB) ? in[In_SigB] : sigIn[0];
    const bool upd = (ctr++ & 7) == 0;
    for (int i = 0; i < 2; ++i)
    {
        const int base = i == 0 ? In_SigA : In_SigB;
        const float mod = in[base + 4];
        const float ix = dsp::clampf (idx[i] + in[base + 1] / 10.0f, 0.0f, 1.0f);
        float f = freqHz (freq[i]) * std::exp2 (dsp::clampf (in[base + 2], -12.0f, 12.0f) / 1.2f);
        float bwk = dsp::clampf (bw[i] + in[base + 3] / 10.0f, 0.0f, 1.0f);
        if (mode[i] == 1) f *= std::exp2 (3.0f * ix * mod);                       // frequency modulation
        if (mode[i] == 2) bwk = dsp::clampf (bwk + 0.5f * ix * mod, 0.0f, 1.0f);  // bandwidth modulation
        const float q = 0.6f + 40.0f * (1.0f - bwk) * (1.0f - bwk);
        if (upd || mode[i] != 0) { fEff[i] = f; qEff[i] = q; filt[i].set (f, q, fs); }
        float y = filt[i].bp (sigIn[i]) * (0.7f + 0.15f * std::sqrt (q));
        if (mode[i] == 0) y *= dsp::clampf (1.0f + ix * mod * 1.5f, 0.0f, 3.0f);   // amplitude modulation
        y = std::tanh (y);
        out[i == 0 ? Out_SigA1 : Out_SigB1] = out[i == 0 ? Out_SigA2 : Out_SigB2] = y;
    }
}

float M267::led (int id) const noexcept
{
    if (id >= 0 && id < 4) return dsp::clampf (ledV[id].load (std::memory_order_relaxed), 0.0f, 1.0f);
    if (id >= 10 && id < 16)
    {
        const int f = (id - 10) / 3, k = (id - 10) % 3;
        return (int) std::lround (modP[f]->load()) == k ? 1.0f : 0.0f;
    }
    if (id == 20) return remP != nullptr && remP->load() > 0.5f ? 1.0f : 0.0f;
    return 0.0f;
}
} // namespace wc
