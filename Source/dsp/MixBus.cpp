#include "MixBus.h"
#include "../Params.h"

namespace pg
{
void MixBus::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    dryL.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
    dryR.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
    wetInit = false;
    for (int c = 0; c < 2; ++c)
    {
        tapeLine[c].init ((int) (0.03 * sr));
        chLine[c].init ((int) (0.05 * sr));
        for (auto* b : { &tapeTone[c], &eqLc[c], &eqLow[c], &eqMid[c], &eqHigh[c], &eqHc[c] }) b->reset();
        imLow[c].reset(); djF[c].reset();
        crHold[c] = 0.0f;
    }
    compEnv = 0.0f; gateGain = 1.0f; trFast = trSlow = 0.0f; limGain = 1.0f;
}

void MixBus::process (float* L, float* R, int n, const float* p, const float* m, double bpm, double beat)
{
    bool used[MB_Count] {};
    for (int s = 0; s < 10; ++s)
    {
        const int t = juce::jlimit (0, MB_Count - 1, (int) p[mb_slot1 + s]);
        if (t == MB_Empty || used[t]) continue;
        used[t] = true;
        static const int onId[MB_Count] = { 0, mb_tp_on, mb_eq_on, mb_cp_on, mb_ch_on, mb_im_on, mb_tg_on, mb_dj_on, mb_bc_on, mb_tr_on, mb_lm_on };
        const bool on = p[onId[t]] > 0.5f;
        float& w = wet[t];
        if (! wetInit) w = on ? 1.0f : 0.0f;
        if (! on && w <= 0.0f) continue;
        auto run = [&]
        {
            switch (t)
            {
                case MB_Tape:      tape (L, R, n, p, m[MD_MbTapeDrive]); break;
                case MB_EQ:        eq (L, R, n, p); break;
                case MB_Comp:      comp (L, R, n, p); break;
                case MB_Chorus:    chorus (L, R, n, p); break;
                case MB_Imager:    imager (L, R, n, p, m[MD_MbWidth]); break;
                case MB_Gate:      gate (L, R, n, p, bpm, beat, m[MD_MbGateDepth]); break;
                case MB_DJ:        dj (L, R, n, p, m[MD_MbDjFilter]); break;
                case MB_Crush:     crush (L, R, n, p, m[MD_MbCrushBits]); break;
                case MB_Transient: transient (L, R, n, p); break;
                case MB_Limiter:   limiter (L, R, n, p); break;
                default: break;
            }
        };
        if (on && w >= 1.0f) { run(); continue; }
        if ((int) dryL.size() < n) { dryL.resize ((size_t) n); dryR.resize ((size_t) n); }
        std::copy (L, L + n, dryL.begin());
        std::copy (R, R + n, dryR.begin());
        run();
        const float step = 1.0f / (0.01f * (float) sr), target = on ? 1.0f : 0.0f;
        for (int i = 0; i < n; ++i)
        {
            w = target > w ? std::min (target, w + step) : std::max (target, w - step);
            L[i] = dryL[(size_t) i] + (L[i] - dryL[(size_t) i]) * w;
            R[i] = dryR[(size_t) i] + (R[i] - dryR[(size_t) i]) * w;
        }
    }
    wetInit = true;
}

// Tape: warm asymmetric saturation, tone tilt, wow and flutter
void MixBus::tape (float* L, float* R, int n, const float* p, float driveMod)
{
    const float drive = clamp01 (p[mb_tp_drive] + driveMod), g = 1.0f + drive * 6.0f, comp = 1.0f / (0.6f + 0.4f * g);
    const float tone = p[mb_tp_tone], mix = p[mb_tp_mix], wow = p[mb_tp_wow];
    for (int c = 0; c < 2; ++c) tapeTone[c].set (4, 3000.0f, 0.7f, tone * 6.0f, (float) sr);   // tone: high shelf -6..+6 dB
    const float base = 0.004f * (float) sr;
    for (int i = 0; i < n; ++i)
    {
        tapePh += 0.55f / (float) sr; if (tapePh >= 1.0f) tapePh -= 1.0f;
        flutPh += 6.3f / (float) sr; if (flutPh >= 1.0f) flutPh -= 1.0f;
        const float d = base + wow * (0.0018f * (float) sr * std::sin (kTwoPi * tapePh) + 0.0002f * (float) sr * std::sin (kTwoPi * flutPh));
        for (int c = 0; c < 2; ++c)
        {
            float& x = c == 0 ? L[i] : R[i];
            float y = x * g;
            y = y >= 0 ? fastTanh (y) : fastTanh (y * 0.85f) / 0.85f;   // slight asymmetry
            y = tapeTone[c].tick (y * comp);
            tapeLine[c].push (y);
            const float wet = wow > 0.001f ? tapeLine[c].read (d) : y;
            x += (wet - x) * mix;
        }
    }
}

void MixBus::eq (float* L, float* R, int n, const float* p)
{
    const float r = (float) sr;
    for (int c = 0; c < 2; ++c)
    {
        eqLc[c].set (1, p[mb_eq_lcut], 0.707f, 0, r);
        eqLow[c].set (3, 110.0f, 0.707f, p[mb_eq_low], r);
        eqMid[c].set (2, p[mb_eq_mfreq], 0.9f, p[mb_eq_mid], r);
        eqHigh[c].set (4, 7000.0f, 0.707f, p[mb_eq_high], r);
        eqHc[c].set (0, p[mb_eq_hcut], 0.707f, 0, r);
    }
    const bool lc = p[mb_eq_lcut] > 21.0f, hc = p[mb_eq_hcut] < 19900.0f;
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < 2; ++c)
        {
            float& x = c == 0 ? L[i] : R[i];
            float y = x;
            if (lc) y = eqLc[c].tick (y);
            y = eqHigh[c].tick (eqMid[c].tick (eqLow[c].tick (y)));
            if (hc) y = eqHc[c].tick (y);
            x = y;
        }
}

// Feed-forward stereo-linked compressor with a soft knee
void MixBus::comp (float* L, float* R, int n, const float* p)
{
    const float thr = p[mb_cp_thr], ratio = p[mb_cp_ratio], mix = p[mb_cp_mix], makeup = dbToGain (p[mb_cp_gain]);
    const float att = std::exp (-1.0f / (p[mb_cp_att] * 0.001f * (float) sr)), rel = std::exp (-1.0f / (p[mb_cp_rel] * 0.001f * (float) sr));
    const float knee = 6.0f;
    for (int i = 0; i < n; ++i)
    {
        const float lvl = std::max (std::abs (L[i]), std::abs (R[i]));
        const float db = 20.0f * std::log10 (std::max (1.0e-6f, lvl));
        float over = db - thr, gr = 0.0f;
        if (over > knee * 0.5f) gr = over * (1.0f - 1.0f / ratio);
        else if (over > -knee * 0.5f) { const float k = over + knee * 0.5f; gr = (1.0f - 1.0f / ratio) * k * k / (2.0f * knee); }
        const float coef = gr > compEnv ? att : rel;
        compEnv = gr + (compEnv - gr) * coef;
        const float g = dbToGain (-compEnv) * makeup;
        L[i] += (L[i] * g - L[i]) * mix;
        R[i] += (R[i] * g - R[i]) * mix;
    }
}

// Three-voice stereo chorus
void MixBus::chorus (float* L, float* R, int n, const float* p)
{
    const float rate = p[mb_ch_rate], depth = p[mb_ch_depth], mix = p[mb_ch_mix];
    const float base = 0.012f * (float) sr, dep = 0.006f * (float) sr * depth;
    for (int i = 0; i < n; ++i)
    {
        chPh += rate / (float) sr; if (chPh >= 1.0f) chPh -= 1.0f;
        chLine[0].push (L[i]); chLine[1].push (R[i]);
        float wl = 0, wr = 0;
        for (int v = 0; v < 3; ++v)
        {
            const float ph = chPh + (float) v / 3.0f;
            wl += chLine[0].read (base + dep * (0.5f + 0.5f * std::sin (kTwoPi * ph)));
            wr += chLine[1].read (base + dep * (0.5f + 0.5f * std::sin (kTwoPi * (ph + 0.25f))));
        }
        L[i] += (wl / 3.0f - L[i]) * mix;
        R[i] += (wr / 3.0f - R[i]) * mix;
    }
}

void MixBus::imager (float* L, float* R, int n, const float* p, float widthMod)
{
    const float width = clampf (p[mb_im_width] + widthMod, 0.0f, 2.0f);
    for (int c = 0; c < 2; ++c) imLow[c].set (p[mb_im_bass], 0.7071f, (float) sr);
    for (int i = 0; i < n; ++i)
    {
        float lpL, bpL, hpL, lpR, bpR, hpR;
        imLow[0].tick (L[i], lpL, bpL, hpL);
        imLow[1].tick (R[i], lpR, bpR, hpR);
        const float low = 0.5f * (lpL + lpR);
        const float hl = L[i] - lpL, hr = R[i] - lpR;
        const float mid = 0.5f * (hl + hr), side = 0.5f * (hl - hr) * width;
        L[i] = low + mid + side;
        R[i] = low + mid - side;
    }
}

// Trance gate: 16-step patterns locked to the beat
void MixBus::gate (float* L, float* R, int n, const float* p, double bpm, double beat, float depthMod)
{
    static const uint16_t pats[] = { 0xFFFF, 0xEEEE, 0x7777, 0xB6DB, 0x4949, 0xF0F3, 0x00FF, 0xD5AA };
    const uint16_t pat = pats[juce::jlimit (0, 7, (int) p[mb_tg_pat])];
    const double div = choices::divisionInBeats ((int) p[mb_tg_div]);
    const float depth = clamp01 (p[mb_tg_depth] + depthMod);
    const float sm = std::exp (-1.0f / ((0.0005f + p[mb_tg_smooth] * 0.03f) * (float) sr));
    const double bps = bpm / 60.0 / sr;
    for (int i = 0; i < n; ++i)
    {
        const double b = beat + bps * i;
        const long long step = (long long) std::floor (b / div);
        const double frac = b / div - (double) step;
        const bool on = (pat >> (int) (step & 15)) & 1;
        const float target = (on && frac < 0.8) ? 1.0f : 1.0f - depth;
        gateGain = target + (gateGain - target) * sm;
        L[i] *= gateGain; R[i] *= gateGain;
    }
}

// One-knob DJ filter: left = low pass, right = high pass
void MixBus::dj (float* L, float* R, int n, const float* p, float posMod)
{
    const float pos = clampf (p[mb_dj_pos] + posMod, -1.0f, 1.0f);
    if (std::abs (pos) < 0.02f) { djLast = pos; return; }
    const float q = 0.6f + p[mb_dj_res] * 8.0f;
    const float f = pos < 0 ? 20000.0f * std::pow (0.006f, -pos) : 20.0f * std::pow (400.0f, pos);
    for (int c = 0; c < 2; ++c) djF[c].set (f, q, (float) sr);
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < 2; ++c)
        {
            float& x = c == 0 ? L[i] : R[i];
            float lp, bp, hp;
            djF[c].tick (x, lp, bp, hp);
            x = pos < 0 ? lp : hp;
        }
    djLast = pos;
}

void MixBus::crush (float* L, float* R, int n, const float* p, float bitsMod)
{
    const float bits = clampf (p[mb_bc_bits] - bitsMod * 12.0f, 2.0f, 16.0f), q = std::exp2 (bits - 1.0f);
    const float down = p[mb_bc_rate], mix = p[mb_bc_mix];
    for (int i = 0; i < n; ++i)
    {
        crCount += 1.0f;
        if (crCount >= down) { crCount -= down; crHold[0] = std::round (L[i] * q) / q; crHold[1] = std::round (R[i] * q) / q; }
        L[i] += (crHold[0] - L[i]) * mix;
        R[i] += (crHold[1] - R[i]) * mix;
    }
}

// Transient shaper: difference of a fast and a slow envelope follower
void MixBus::transient (float* L, float* R, int n, const float* p)
{
    const float a = p[mb_tr_att], s = p[mb_tr_sus];
    const float fc = std::exp (-1.0f / (0.001f * (float) sr)), sc = std::exp (-1.0f / (0.05f * (float) sr));
    for (int i = 0; i < n; ++i)
    {
        const float lvl = 0.5f * (std::abs (L[i]) + std::abs (R[i]));
        trFast = lvl + (trFast - lvl) * (lvl > trFast ? 0.0f : fc);
        trSlow = lvl + (trSlow - lvl) * sc;
        const float tr = clampf ((trFast - trSlow) / (trSlow + 1.0e-4f), 0.0f, 4.0f);
        const float sus = clampf (trSlow / (trFast + 1.0e-4f), 0.0f, 1.0f);
        const float g = clampf (1.0f + a * tr * 0.6f + s * sus * 0.8f - s * 0.4f, 0.1f, 4.0f);
        L[i] *= g; R[i] *= g;
    }
}

// Brickwall-style peak limiter with instant attack and smooth release
void MixBus::limiter (float* L, float* R, int n, const float* p)
{
    const float in = dbToGain (p[mb_lm_in]), ceil = dbToGain (p[mb_lm_ceil]);
    const float rel = std::exp (-1.0f / (p[mb_lm_rel] * 0.001f * (float) sr));
    for (int i = 0; i < n; ++i)
    {
        const float l = L[i] * in, r = R[i] * in;
        const float peak = std::max (std::abs (l), std::abs (r));
        const float want = peak * limGain > ceil ? ceil / std::max (peak, 1.0e-9f) : 1.0f;
        limGain = want < limGain ? want : want + (limGain - want) * rel;
        L[i] = clampf (l * limGain, -ceil, ceil);
        R[i] = clampf (r * limGain, -ceil, ceil);
    }
}
} // namespace pg
