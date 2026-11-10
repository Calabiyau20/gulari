#include "M281.h"

namespace wc
{
M281::M281 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    for (int n = 0; n < 4; ++n)
    {
        const juce::String s (n + 1);
        att[n] = apvts.getRawParameterValue (p + "att" + s);
        dec[n] = apvts.getRawParameterValue (p + "dec" + s);
        mode[n] = apvts.getRawParameterValue (p + "mode" + s);
        jassert (att[n] != nullptr && dec[n] != nullptr && mode[n] != nullptr);
    }
    quad[0] = apvts.getRawParameterValue (p + "quad12");
    quad[1] = apvts.getRawParameterValue (p + "quad34");
}

void M281::prepare (double sampleRate) { fs = (float) sampleRate; reset(); }

void M281::reset()
{
    for (auto& c : ch) c = Ch();
    for (auto& l : level) l.store (0.0f);
}

void M281::beginBlock (int)
{
    for (int n = 0; n < 4; ++n)
    {
        modeV[n] = juce::jlimit (0, 2, (int) std::lround (mode[n]->load()));
        attK[n] = att[n]->load();
        decK[n] = dec[n]->load();
    }
    quadV[0] = quad[0]->load() > 0.5f;
    quadV[1] = quad[1]->load() > 0.5f;
}

void M281::tick (const float* in, float* out) noexcept
{
    for (int n = 0; n < 4; ++n)
    {
        auto& c = ch[n];
        // CV adds to the knob: +10 V moves the time four decades faster (rate CV, as on the 281)
        const float aNorm = dsp::clampf (attK[n] - in[inAtt (n)] / 10.0f, 0.0f, 1.0f);
        const float dNorm = dsp::clampf (decK[n] - in[inDec (n)] / 10.0f, 0.0f, 1.0f);
        const float aInc = 1.0f / (timeSeconds (aNorm) * fs);
        const float dInc = 1.0f / (timeSeconds (dNorm) * fs);

        // trigger / gate: banana > 3 V, or the panel button
        const bool btn = manual[n].load (std::memory_order_relaxed);
        const bool gate = in[inTrig (n)] > 3.0f || btn;
        const bool rise = gate && ! c.gateWas;
        c.gateWas = gate;
        c.held = gate;
        bool trig = rise || c.pendingTrig;
        c.pendingTrig = false;
        if (modeV[n] == 2 && c.stage == Idle) trig = true;        // cycle: free running

        if (trig && c.stage != Attack) c.stage = Attack;          // retrigger restarts the attack from where it is

        bool endAttack = false;
        switch (c.stage)
        {
            case Idle: break;
            case Attack:
                c.v += aInc;
                if (c.v >= 1.0f)
                {
                    c.v = 1.0f;
                    endAttack = true;
                    c.stage = (modeV[n] == 1 && c.held) ? Hold : Decay;
                }
                break;
            case Hold:
                if (! c.held || modeV[n] != 1) c.stage = Decay;
                break;
            case Decay:
                c.v -= dInc;
                if (c.v <= 0.0f)
                {
                    c.v = 0.0f;
                    c.stage = Idle;
                    c.eodLeft = (int) (0.005f * fs);               // end-of-decay pulse, 5 ms
                    if (modeV[n] == 2) c.stage = Attack;           // cycle
                }
                break;
        }
        // quadrature: A (C) fires B (D) at the end of its attack
        if (endAttack && (n == 0 || n == 2) && quadV[n / 2]) ch[n + 1].pendingTrig = true;

        out[outFunc (n)] = 10.0f * c.v;
        out[outEod (n)] = c.eodLeft > 0 ? 10.0f : 0.0f;
        if (c.eodLeft > 0) --c.eodLeft;
        level[n].store (c.v, std::memory_order_relaxed);
    }
}
} // namespace wc
