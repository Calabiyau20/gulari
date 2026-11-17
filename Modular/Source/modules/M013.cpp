#include "M013.h"

namespace wc
{
M013::M013 (const Params& params, const juce::String& p)
{
    xP = params.getRawParameterValue (p + "x");
    yP = params.getRawParameterValue (p + "y");
    sensP = params.getRawParameterValue (p + "sens");
    sxP = params.getRawParameterValue (p + "scalex");
    syP = params.getRawParameterValue (p + "scaley");
    modeP = params.getRawParameterValue (p + "mode");
    int i = 0;
    for (auto* s : { "x", "y", "sens", "scalex", "scaley" }) knob[i++] = params.apvts.getRawParameterValue (p + s);
    jassert (xP && yP && sensP && sxP && syP && modeP && knob[4]);
}

void M013::compute (float bx, float by, float s, float scx, float scy, const float* k, float& pu, float& pv, float* volts) noexcept
{
    float u = dsp::clampf (0.5f + (bx - 0.5f) * scx, 0.0f, 1.0f), v = dsp::clampf (0.5f + (by - 0.5f) * scy, 0.0f, 1.0f);
    if (k != nullptr)
    {
        float du = 0.0f, dv = 0.0f, sum = 0.0f;
        for (int n = 0; n < kNodes; ++n) { du += k[n] * (nodeU[n] - u); dv += k[n] * (nodeV[n] - v); sum += std::abs (k[n]); }
        const float d = 1.0f / juce::jmax (1.0f, sum);
        u = dsp::clampf (u + du * d, 0.0f, 1.0f);
        v = dsp::clampf (v + dv * d, 0.0f, 1.0f);
    }
    pu = u; pv = v;
    const float r = 0.15f + 1.35f * s;
    for (int n = 0; n < kNodes; ++n)
    {
        const float dist = std::hypot (u - nodeU[n], v - nodeV[n]);
        volts[n] = dist < r ? 5.0f * (1.0f + std::cos (dsp::kPi * dist / r)) : 0.0f;
    }
}

void M013::beginBlock (int)
{
    auto set = [&] (dsp::Ramp& r, float v) { if (first) r.snap (v); else r.to (v, rampLen); };   // 12 ms, as the 257
    set (x, xP->load()); set (y, yP->load()); set (sens, sensP->load()); set (sx, sxP->load()); set (sy, syP->load());
    set (mode, modeP->load() > 0.5f ? 1.0f : 0.0f);   // 0 Tactile, 1 Modulated: it scales the pushes
    if (first) { compute (x.v, y.v, sens.v, sx.v, sy.v, nullptr, lastU, lastV, last); }
    first = false;
    shownU.store (lastU, std::memory_order_relaxed); shownV.store (lastV, std::memory_order_relaxed);
    for (int n = 0; n < kNodes; ++n) shown[(size_t) n].store (last[n], std::memory_order_relaxed);
    blocks.fetch_add (1, std::memory_order_relaxed);
}

void M013::tick (const float* in, float* out) noexcept
{
    const float m = mode.next();
    float k[kNodes];
    for (int n = 0; n < kNodes; ++n) k[n] = m * dsp::clampf (in[n] * 0.1f, -1.0f, 1.0f);
    compute (x.next(), y.next(), sens.next(), sx.next(), sy.next(), k, lastU, lastV, last);
    out[Out_X] = 10.0f * lastU;
    out[Out_Y] = 10.0f * lastV;
    for (int n = 0; n < kNodes; ++n) out[Out_Node1 + n] = last[n];
}

void M013::view (float& pu, float& pv, float* volts) const
{
    const auto now = juce::Time::getMillisecondCounter();
    const auto b = blocks.load (std::memory_order_relaxed);
    if (b != seenBlocks) { seenBlocks = b; seenAt = now; }
    // shortcut: a host block longer than 250 ms (16384 samples at 44.1 kHz) makes the panel flicker between the audio and the knobs, measure the block length if it is seen
    if (now - seenAt > 250)
    {
        compute (knob[0]->load(), knob[1]->load(), knob[2]->load(), knob[3]->load(), knob[4]->load(), nullptr, pu, pv, volts);
        return;
    }
    pu = shownU.load (std::memory_order_relaxed); pv = shownV.load (std::memory_order_relaxed);
    for (int n = 0; n < kNodes; ++n) volts[n] = shown[(size_t) n].load (std::memory_order_relaxed);
}

float M013::led (int id) const noexcept
{
    if (id < 0 || id >= kNodes) return 0.0f;
    float u, v, volts[kNodes];
    view (u, v, volts);
    return volts[id] * 0.1f;
}
} // namespace wc
