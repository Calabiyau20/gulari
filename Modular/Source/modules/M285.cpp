#include "M285.h"

namespace wc
{
M285::M285 (juce::AudioProcessorValueTreeState& apvts, const juce::String& p)
{
    ctlP = apvts.getRawParameterValue (p + "ctl"); refP = apvts.getRawParameterValue (p + "ref");
    fmP = apvts.getRawParameterValue (p + "fm"); intP = apvts.getRawParameterValue (p + "int");
    bmCtlP = apvts.getRawParameterValue (p + "bmctl"); ampP = apvts.getRawParameterValue (p + "amp");
}

void M285::prepare (double sampleRate) { fs = (float) sampleRate; rampLen = juce::jmax (1, (int) (0.012f * fs)); reset(); }

void M285::beginBlock (int)
{
    ctl = ctlP->load(); ref = refHz (refP->load()); fm = fmP->load(); internal = intP->load() > 0.5f;
    bmCtl = bmCtlP->load();
    if (first) amp.snap (ampP->load()); else amp.to (ampP->load(), rampLen);   // 12 ms, as the 292
    first = false;
}

void M285::tick (const float* in, float* out) noexcept
{
    // ---- frequency shifter ----
    float sr, si;
    hs.process (in[In_FsSignal], sr, si);
    float cr, ci;
    if (internal || ! isPatched (In_FsRef))
    {
        const float hz = ref + 100.0f * ctl * in[In_FsControl] + 1000.0f * fm * in[In_FsMod];   // through zero: negative = mirrored
        ph += (double) (hz / fs);
        ph -= std::floor (ph);
        cr = std::cos (dsp::kTwoPi * (float) ph);
        ci = std::sin (dsp::kTwoPi * (float) ph);
    }
    else
        hr.process (in[In_FsRef], cr, ci);
    const float up = sr * cr + si * ci, down = sr * cr - si * ci;   // the b-path leads: + = upper sideband
    out[Out_Sum] = out[Out_Sum2] = up;
    out[Out_Diff] = out[Out_Diff2] = down;
    // ---- balanced modulator ----
    const float x = in[In_BmSignal], r = in[In_BmRef];
    const float ring = x * r;
    const float k = dsp::clampf (amp.next() + bmCtl * in[In_BmControl] / 10.0f, 0.0f, 1.0f);
    const float am = x * 0.5f * (1.0f + r);
    out[Out_Ring] = out[Out_Ring2] = ring;
    out[Out_Var] = out[Out_Var2] = (1.0f - k) * am + k * ring;
}
} // namespace wc
