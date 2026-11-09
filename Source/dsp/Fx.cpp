#include "Fx.h"
#include "../Params.h"

namespace pg
{
void FxChain::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    haas.init ((int) (0.03 * sr));
    for (int c = 0; c < 2; ++c)
    {
        flDelay[c].init ((int) (0.03 * sr));
        dl[c].init ((int) (4.2 * sr));
        stLow[c].reset(); dlLow[c].reset(); dlHigh[c].reset();
        for (auto& a : ph[c]) a.z = 0.0f;
        flFb[c] = phFb[c] = 0.0f;
    }
    dlTime.setTime (0.08f, (float) sr);
    rings.prepare (sr);
    clouds.prepare (sr);
    filt[0].prepare (sr); filt[1].prepare (sr);
    reverb.prepare (sr);
    tmpL.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
    tmpR.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
}

void FxChain::computeMods (const float* p, const GlobalMod& g)
{
    for (auto& m : fxMod) m = 0.0f;
    auto val = [&] (int s) -> float
    {
        switch (s)
        {
            case MS_Lfo1: case MS_Lfo2: case MS_Lfo3: case MS_Lfo4: return g.lfo[s - MS_Lfo1];
            case MS_Chaos1: case MS_Chaos2: case MS_Chaos2Y: case MS_Chaos2Z: return g.chaos[s - MS_Chaos1];
            case MS_Seq1: case MS_Seq2: case MS_Seq3: case MS_Seq4:
            case MS_Seq5: case MS_Seq6: case MS_Seq7: case MS_Seq8: return g.seq[s - MS_Seq1];
            case MS_ModWheel: return g.modWheel;
            case MS_Aftertouch: return g.aftertouch;
            case MS_PitchBend: return g.pitchBend;
            default: return 0.0f;   // per-voice sources do not reach the FX
        }
    };
    for (int s = 0; s < 16; ++s)
    {
        const int base = mtx01_src + s * (mtx02_src - mtx01_src);
        const int dst = (int) p[base + 1];
        if (dst < MD_VoiceCount) continue;
        const int src = (int) p[base], pol = (int) p[base + 4];
        float v = applyPolarity (val (src), src, pol, p) * p[base + 2];
        const int via = (int) p[base + 3];
        if (via != MS_None) v *= applyPolarity (val (via), via, pol == 2 ? 0 : pol, p);
        fxMod[dst] += v;
    }
}

void FxChain::process (float* L, float* R, int n, const float* p, const GlobalMod& g, int lastNote, bool strum)
{
    computeMods (p, g);
    // the rack: every slot runs its effect in order; an effect sits in one slot only
    bool used[FX_Count] {};
    for (int s = 0; s < 10; ++s)
    {
        const int type = juce::jlimit (0, FX_Count - 1, (int) p[fx_slot1 + s]);
        if (type == FX_Empty || used[type]) continue;
        used[type] = true;
        switch (type)
        {
            case FX_Filter1: if (p[ff1_on] > 0.5f) runFilter (0, L, R, n, p); break;
            case FX_Filter2: if (p[ff2_on] > 0.5f) runFilter (1, L, R, n, p); break;
            case FX_Stereo:  if (p[fx_st_on] > 0.5f) runStereo (L, R, n, p); break;
            case FX_Flanger: if (p[fx_fl_on] > 0.5f) runFlanger (L, R, n, p); break;
            case FX_Phaser:  if (p[fx_ph_on] > 0.5f) runPhaser (L, R, n, p); break;
            case FX_Delay:   if (p[fx_dl_on] > 0.5f) runDelay (L, R, n, p, g); break;
            case FX_Rings:   if (p[fx_rn_on] > 0.5f) runRings (L, R, n, p, lastNote, strum); break;
            case FX_Clouds:  if (p[fx_gr_on] > 0.5f) runClouds (L, R, n, p); break;
            case FX_Reverb:  if (p[fx_rv_on] > 0.5f) runReverb (L, R, n, p); break;
            default: break;
        }
    }
}

void FxChain::runStereo (float* L, float* R, int n, const float* p)
{
    const float rate = (float) sr;
    {
        for (int c = 0; c < 2; ++c) stLow[c].set (p[fx_st_bass], 0.7071f, rate);
        const float width = p[fx_st_width];
        const float haasSamples = p[fx_st_haas] * 0.001f * rate;
        for (int i = 0; i < n; ++i)
        {
            float lpL, bpL, hpL, lpR, bpR, hpR;
            stLow[0].tick (L[i], lpL, bpL, hpL);
            stLow[1].tick (R[i], lpR, bpR, hpR);
            const float lowMono = (lpL + lpR) * 0.5f;
            const float hl = L[i] - lpL, hr = R[i] - lpR;
            float mid = (hl + hr) * 0.5f, side = (hl - hr) * 0.5f * width;
            float l = mid + side, r = mid - side;
            haas.push (r);
            if (haasSamples > 1.0f) r = haas.read (haasSamples);
            L[i] = l + lowMono;
            R[i] = r + lowMono;
        }
    }

}

void FxChain::runFlanger (float* L, float* R, int n, const float* p)
{
    const float rate = (float) sr;
    {
        const float mix = clamp01 (p[fx_fl_mix] + fxMod[MD_FlMix]);
        const float base = p[fx_fl_delay] * 0.001f * rate;
        const float depth = p[fx_fl_depth] * base * 0.95f;
        const float fb = p[fx_fl_fb];
        const float inc = p[fx_fl_rate] / rate;
        for (int i = 0; i < n; ++i)
        {
            flPhase += inc; if (flPhase >= 1.0f) flPhase -= 1.0f;
            for (int c = 0; c < 2; ++c)
            {
                float& x = c == 0 ? L[i] : R[i];
                const float lfo = std::sin (kTwoPi * (flPhase + (c ? 0.25f : 0.0f)));
                flDelay[c].push (x + flFb[c] * fb);
                const float y = flDelay[c].read (base + depth * lfo);
                flFb[c] = fastTanh (y);
                x = x + (y - x) * mix;
            }
        }
    }

}

void FxChain::runPhaser (float* L, float* R, int n, const float* p)
{
    const float rate = (float) sr;
    {
        const float mix = clamp01 (p[fx_ph_mix] + fxMod[MD_PhMix]);
        const int stages = juce::jlimit (2, 12, (int) p[fx_ph_stages]);
        const float depth = p[fx_ph_depth], fb = p[fx_ph_fb], center = p[fx_ph_center];
        const float inc = p[fx_ph_rate] / rate;
        for (int i = 0; i < n; ++i)
        {
            phPhase += inc; if (phPhase >= 1.0f) phPhase -= 1.0f;
            for (int c = 0; c < 2; ++c)
            {
                float& x = c == 0 ? L[i] : R[i];
                const float lfo = std::sin (kTwoPi * (phPhase + (c ? 0.25f : 0.0f)));
                const float f = clampf (center * std::exp2 (lfo * depth * 2.5f), 30.0f, rate * 0.45f);
                const float t = std::tan (kPi * f / rate);
                const float a = (t - 1.0f) / (t + 1.0f);
                float y = x + phFb[c] * fb;
                for (int s = 0; s < stages; ++s) y = ph[c][s].tick (y, a);
                phFb[c] = fastTanh (y);
                x = x + (y - x) * mix;
            }
        }
    }

}

void FxChain::runDelay (float* L, float* R, int n, const float* p, const GlobalMod& g)
{
    const float rate = (float) sr;
    {
        const float mix = clamp01 (p[fx_dl_mix] + fxMod[MD_DelayMix]);
        const float fb = clampf (p[fx_dl_fb] + fxMod[MD_DelayFb], 0.0f, 0.98f);
        float timeMs = p[fx_dl_time];
        if (p[fx_dl_sync] > 0.5f) timeMs = (float) (choices::divisionInBeats ((int) p[fx_dl_div]) * 60000.0 / g.bpm);
        timeMs *= std::exp2 (fxMod[MD_DelayTime] * 2.0f);   // matrix: +/- 2 octaves of delay time (tape-style pitch bends)
        const float target = clampf (timeMs * 0.001f * rate, 1.0f, 4.0f * rate);
        const bool pp = p[fx_dl_pp] > 0.5f;
        for (int c = 0; c < 2; ++c)
        {
            dlLow[c].set (p[fx_dl_lowcut], 0.7071f, rate);
            dlHigh[c].set (p[fx_dl_highcut], 0.7071f, rate);
        }
        for (int i = 0; i < n; ++i)
        {
            const float d = dlTime.process (target);
            float yl = dl[0].read (d), yr = dl[1].read (d);
            // filter the feedback path
            float lp, bp, hp;
            dlHigh[0].tick (yl, lp, bp, hp); yl = lp; dlLow[0].tick (yl, lp, bp, hp); yl = hp;
            dlHigh[1].tick (yr, lp, bp, hp); yr = lp; dlLow[1].tick (yr, lp, bp, hp); yr = hp;
            if (pp)
            {
                const float in = (L[i] + R[i]) * 0.5f;
                dl[0].push (in + yr * fb);
                dl[1].push (yl * fb);
            }
            else
            {
                dl[0].push (L[i] + yl * fb);
                dl[1].push (R[i] + yr * fb);
            }
            L[i] += yl * mix;
            R[i] += yr * mix;
        }
    }

}

void FxChain::runRings (float* L, float* R, int n, const float* p, int lastNote, bool strum)
{
    {
        RingsFx::Params rp;
        rp.model = (int) p[fx_rn_model];
        const int pi = (int) p[fx_rn_poly];
        rp.polyphony = pi == 0 ? 1 : (pi == 1 ? 2 : 4);
        rp.followKeys = p[fx_rn_follow] > 0.5f;
        rp.note = rp.followKeys ? (float) lastNote : p[fx_rn_note];
        rp.structure = clamp01 (p[fx_rn_structure] + fxMod[MD_RnStructure]);
        rp.brightness = clamp01 (p[fx_rn_bright] + fxMod[MD_RnBright]);
        rp.damping = p[fx_rn_damping];
        rp.position = clamp01 (p[fx_rn_pos] + fxMod[MD_RnPos]);
        rp.mix = p[fx_rn_mix];
        rings.setParams (rp);
        if (strum) rings.strum();
        rings.process (L, R, n);
    }

}

void FxChain::runClouds (float* L, float* R, int n, const float* p)
{
    {
        CloudsFx::Params cp;
        cp.mode = (int) p[fx_gr_mode];
        cp.quality = (int) p[fx_gr_quality];
        cp.freeze = p[fx_gr_freeze] > 0.5f;
        cp.position = clamp01 (p[fx_gr_pos] + fxMod[MD_GrfxPos]);
        cp.size = clamp01 (p[fx_gr_size] + fxMod[MD_GrfxSize]);
        cp.pitch = p[fx_gr_pitch];
        cp.density = clamp01 (p[fx_gr_density] + fxMod[MD_GrfxDensity]);
        cp.texture = clamp01 (p[fx_gr_texture] + fxMod[MD_GrfxTexture]);
        cp.spread = p[fx_gr_spread];
        cp.feedback = p[fx_gr_fb];
        cp.reverb = p[fx_gr_reverb];
        cp.mix = clamp01 (p[fx_gr_mix] + fxMod[MD_GrfxMix]);
        clouds.setParams (cp);
        clouds.process (L, R, n);
    }
}

void FxChain::runFilter (int w, float* L, float* R, int n, const float* p)
{
    const int b = w == 0 ? (int) ff1_on : (int) ff2_on;
    auto q = [&] (int id) { return p[b + (id - ff1_on)]; };
    const int mf = w == 0 ? MD_Ff1Freq : MD_Ff2Freq, mr = w == 0 ? MD_Ff1Res : MD_Ff2Res, mm = w == 0 ? MD_Ff1Morph : MD_Ff2Morph;
    const float freq = clampf (q (ff1_freq) * std::exp2 (fxMod[mf] * 4.0f), 20.0f, 20000.0f);
    filt[w].process (L, R, n, (int) q (ff1_type), freq, clamp01 (q (ff1_res) + fxMod[mr]), clamp01 (q (ff1_morph) + fxMod[mm]),
                     (int) q (ff1_poles), q (ff1_spread), q (ff1_drive), q (ff1_mix));
}

void FxChain::runReverb (float* L, float* R, int n, const float* p)
{
    reverb.process (L, R, n, clamp01 (p[fx_rv_size] + fxMod[MD_RvSize]), p[fx_rv_decay], p[fx_rv_damp], p[fx_rv_pre],
                    p[fx_rv_mod], p[fx_rv_width], clamp01 (p[fx_rv_mix] + fxMod[MD_RvMix]));
}

// =============================================================================
// Filter FX
// =============================================================================
void FilterFx::prepare (double sampleRate)
{
    sr = sampleRate;
    for (int c = 0; c < 2; ++c)
    {
        for (int k = 0; k < 8; ++k) { svf[c][k].reset(); comb[c][k].init ((int) (sr / 15.0)); combLp[c][k] = 0.0f; }
        for (auto& a : ap[c]) a.z = 0.0f;
        apFb[c] = 0.0f;
    }
    fSmooth.setTime (0.02f, (float) sr / 16.0f);
    fSmooth.y = 800.0f;
}

void FilterFx::process (float* L, float* R, int n, int type, float freq, float res, float morph, int poles, float spread, float drive, float mix)
{
    const float rate = (float) sr;
    if (type != lastType)
    {
        for (int c = 0; c < 2; ++c) { for (auto& f : svf[c]) f.reset(); for (auto& d : comb[c]) d.clear(); for (auto& a : ap[c]) a.z = 0.0f; }
        lastType = type;
        fSmooth.y = freq;
    }
    poles = juce::jlimit (1, 8, poles);
    const float dGain = 1.0f + drive * 8.0f, dComp = 1.0f / std::sqrt (dGain);
    static const float vowelF[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 }, { 570, 840, 2410 }, { 300, 870, 2240 } };
    static const float vowelA[3] = { 1.0f, 0.6f, 0.35f };

    for (int start = 0; start < n; start += 16)
    {
        const int end = juce::jmin (n, start + 16);
        const float f = fSmooth.process (freq);
        for (int c = 0; c < 2; ++c)
        {
            float* x = c == 0 ? L : R;
            const float fc = f * (c == 1 ? std::exp2 (spread * 0.5f) : 1.0f);
            // per-block coefficients
            switch (type)
            {
                case 0:   // filterbank
                {
                    const float spacing = std::exp2 (0.2f + 1.3f * morph);
                    const float qv = 0.7f + res * 24.0f;
                    for (int k = 0; k < poles; ++k) svf[c][k].set (fc * std::pow (spacing, (float) k), qv, rate);
                    break;
                }
                case 3: case 4:
                {
                    const int stages = (poles + 1) / 2;
                    for (int k = 0; k < stages; ++k) svf[c][k].set (fc, k == 0 ? 0.5f + res * 18.0f : 0.707f, rate);
                    break;
                }
                case 5:
                {
                    const float v = morph * 4.0f, ratio = clampf (fc / 800.0f, 0.25f, 4.0f);
                    const int i0 = juce::jmin (3, (int) v);
                    const float fr = v - (float) i0;
                    for (int k = 0; k < 3; ++k)
                        svf[c][k].set ((vowelF[i0][k] + (vowelF[i0 + 1][k] - vowelF[i0][k]) * fr) * ratio, 4.0f + res * 16.0f, rate);
                    break;
                }
                default: break;
            }
            for (int i = start; i < end; ++i)
            {
                const float in = x[i];
                const float xd = drive > 0.0f ? fastTanh (in * dGain) * dComp : in;
                float y = 0.0f;
                switch (type)
                {
                    case 0:
                    {
                        for (int k = 0; k < poles; ++k)
                        {
                            float lp, bp, hp;
                            svf[c][k].tick (xd, lp, bp, hp);
                            y += bp * svf[c][k].k;
                        }
                        y *= 1.0f / std::sqrt ((float) poles) * (1.0f + res * 1.5f);
                        break;
                    }
                    case 1: case 2:   // combs: positive or negative feedback, 'poles' stages in series
                    {
                        const float g = (type == 1 ? 1.0f : -1.0f) * (0.2f + res * 0.78f);
                        const float d = clampf (rate / fc, 2.0f, (float) comb[c][0].mask - 4.0f);
                        const float damp = 0.05f + morph * 0.9f;
                        y = xd;
                        for (int k = 0; k < poles; ++k)
                        {
                            float fb = comb[c][k].read (d);
                            combLp[c][k] += (fb - combLp[c][k]) * (1.0f - damp * 0.95f);
                            const float out = y + g * combLp[c][k];
                            comb[c][k].push (fastTanh (out));
                            y = out * (1.0f - std::abs (g) * 0.85f);
                        }
                        break;
                    }
                    case 3: case 4:
                    {
                        const int stages = (poles + 1) / 2;
                        float s = xd, lp = 0, bp = 0, hp = 0;
                        for (int k = 0; k < stages; ++k)
                        {
                            svf[c][k].tick (s, lp, bp, hp);
                            const float m = morph * 2.0f;
                            if (type == 3) s = m < 1.0f ? lp + (bp * svf[c][k].k - lp) * m : bp * svf[c][k].k + (hp - bp * svf[c][k].k) * (m - 1.0f);
                            else { const float notch = lp + hp; s = m < 1.0f ? lp + (notch - lp) * m : notch + (hp - notch) * (m - 1.0f); }
                        }
                        y = s;
                        break;
                    }
                    case 5:
                    {
                        for (int k = 0; k < 3; ++k)
                        {
                            float lp, bp, hp;
                            svf[c][k].tick (xd, lp, bp, hp);
                            y += bp * svf[c][k].k * vowelA[k];
                        }
                        y *= 1.6f;
                        break;
                    }
                    default:   // allpass chain with feedback
                    {
                        const int stages = poles * 2;
                        float s = xd + apFb[c] * res * 0.9f;
                        for (int k = 0; k < stages; ++k)
                        {
                            const float fk = clampf (fc * std::exp2 (((float) k / (float) juce::jmax (1, stages - 1) - 0.5f) * morph * 4.0f), 20.0f, rate * 0.45f);
                            const float t = std::tan (kPi * fk / rate);
                            s = ap[c][k].tick (s, (t - 1.0f) / (t + 1.0f));
                        }
                        apFb[c] = fastTanh (s);
                        y = s;
                        break;
                    }
                }
                x[i] = in + (y - in) * mix;
            }
        }
    }
}

// =============================================================================
// Reverb
// =============================================================================
void Reverb::prepare (double sampleRate)
{
    sr = sampleRate;
    pre.init ((int) (0.25 * sr));
    for (auto& l : line) l.init ((int) (0.25 * sr));
    for (auto& d : diff) d.d.init ((int) (0.02 * sr));
    for (auto& v : lp) v = 0.0f;
}

void Reverb::process (float* L, float* R, int n, float size, float decay, float damp, float preMs, float modAmt, float width, float mix)
{
    static const float base[8] = { 1117.0f, 1277.0f, 1423.0f, 1559.0f, 1709.0f, 1873.0f, 1999.0f, 2153.0f };   // samples at 48 kHz
    static const float diffLen[4] = { 142.0f, 107.0f, 379.0f, 277.0f };
    const float scale = (float) sr / 48000.0f * (0.35f + 1.6f * size);
    float len[8], g[8];
    for (int k = 0; k < 8; ++k)
    {
        len[k] = base[k] * scale;
        g[k] = std::pow (10.0f, -3.0f * len[k] / ((float) sr * juce::jmax (0.1f, decay)));
    }
    const float dampC = 0.05f + damp * 0.85f;
    const float preS = juce::jmax (1.0f, preMs * 0.001f * (float) sr);
    const float lfoInc = 0.7f / (float) sr;
    for (int i = 0; i < n; ++i)
    {
        pre.push ((L[i] + R[i]) * 0.5f);
        float x = pre.read (preS);
        for (int d = 0; d < 4; ++d) x = diff[d].tick (x, diffLen[d] * (float) sr / 48000.0f);
        lfo += lfoInc; if (lfo >= 1.0f) lfo -= 1.0f;
        const float m = std::sin (kTwoPi * lfo) * modAmt * 12.0f;
        float o[8];
        for (int k = 0; k < 8; ++k)
        {
            const float rl = len[k] + (k == 1 ? m : (k == 5 ? -m : 0.0f));
            float v = line[k].read (rl);
            lp[k] += (v - lp[k]) * (1.0f - dampC);
            o[k] = lp[k] * g[k];
        }
        // 8x8 Hadamard (fast), normalised
        float h[8];
        for (int k = 0; k < 8; ++k) h[k] = o[k];
        for (int sz = 1; sz < 8; sz <<= 1)
            for (int a = 0; a < 8; a += sz * 2)
                for (int b = a; b < a + sz; ++b) { const float u = h[b], v = h[b + sz]; h[b] = u + v; h[b + sz] = u - v; }
        for (int k = 0; k < 8; ++k) line[k].push (h[k] * 0.35355f + (k < 4 ? x : -x) * 0.5f);
        const float wl = o[0] + o[2] + o[4] + o[6], wr = o[1] + o[3] + o[5] + o[7];
        const float mid = (wl + wr) * 0.5f, side = (wl - wr) * 0.5f * width;
        L[i] += ((mid + side) * 0.6f - L[i]) * mix;
        R[i] += ((mid - side) * 0.6f - R[i]) * mix;
    }
}
} // namespace pg
