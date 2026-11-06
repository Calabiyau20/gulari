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
            case MS_Chaos1: case MS_Chaos2: return g.chaos[s - MS_Chaos1];
            case MS_Macro1: case MS_Macro2: case MS_Macro3: case MS_Macro4: return g.macro[s - MS_Macro1];
            case MS_Seq1: case MS_Seq2: case MS_Seq3: case MS_Seq4: return g.seq[s - MS_Seq1];
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
        float v = val ((int) p[base]) * p[base + 2];
        const int via = (int) p[base + 3];
        if (via != MS_None) v *= val (via);
        fxMod[dst] += v;
    }
}

void FxChain::process (float* L, float* R, int n, const float* p, const GlobalMod& g, int lastNote, bool strum)
{
    computeMods (p, g);
    const float rate = (float) sr;

    // ---- stereo widener: mono bass, M/S width, optional Haas ----
    if (p[fx_st_on] > 0.5f)
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

    // ---- flanger ----
    if (p[fx_fl_on] > 0.5f)
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

    // ---- phaser ----
    if (p[fx_ph_on] > 0.5f)
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

    // ---- delay ----
    if (p[fx_dl_on] > 0.5f)
    {
        const float mix = clamp01 (p[fx_dl_mix] + fxMod[MD_DelayMix]);
        const float fb = clampf (p[fx_dl_fb] + fxMod[MD_DelayFb], 0.0f, 0.98f);
        float timeMs = p[fx_dl_time];
        if (p[fx_dl_sync] > 0.5f) timeMs = (float) (choices::divisionInBeats ((int) p[fx_dl_div]) * 60000.0 / g.bpm);
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

    // ---- Rings resonator ----
    if (p[fx_rn_on] > 0.5f)
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

    // ---- Clouds ----
    if (p[fx_gr_on] > 0.5f)
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
} // namespace pg
