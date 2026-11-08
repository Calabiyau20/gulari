#include "Voice.h"

namespace pg
{
float modDepth (int dst)
{
    switch (dst)
    {
        case MD_WtAPitch: case MD_WtBPitch: case MD_GrPitch: case MD_ModPitch: return 24.0f;   // semitones
        case MD_Cutoff: return 6.0f;                                                          // octaves
        case MD_GrSize: return 3.0f;                                                          // octaves of size
        case MD_GrDensity: return 4.0f;                                                       // octaves of density
        default: return 1.0f;                                                                 // additive 0..1 ranges
    }
}

void Voice::prepare (double sampleRate, int idx)
{
    sr = sampleRate;
    index = idx;
    const float r = (float) sr;
    amp.setSampleRate (r); fenv.setSampleRate (r); menv.setSampleRate (r);
    grain.prepare (sr, 0x1234u + (uint32_t) idx * 7919u);
    wt[0].prepare (sr, 0x51u + (uint32_t) idx * 31u);
    wt[1].prepare (sr, 0x97u + (uint32_t) idx * 37u);
    noise.rng.seed (0xABCDu + (uint32_t) idx * 101u);
    grainRng.seed (0x5EEDu + (uint32_t) idx * 977u);
    for (int i = 0; i < 4; ++i) lfo[i].rng.seed (0x777u + (uint32_t) (idx * 4 + i));
    plaits.prepare (sr);
    filter.reset();
    kill();
}

void Voice::start (int midiNote, float vel, bool retrigger, const VoiceContext& ctx)
{
    const float* p = ctx.p;
    const bool wasActive = isActive();
    note = midiNote;
    velocity = vel;
    targetPitch = (float) midiNote;
    if (! wasActive || p[glide] <= 0.0f) currentPitch = targetPitch;
    held = true;
    age = ++ageCounter;
    Rng r; r.seed ((uint32_t) (age * 2654435761u + 1u));
    randomValue = r.bi();

    if (retrigger || ! wasActive)
    {
        amp.gateOn (! wasActive);
        fenv.gateOn (! wasActive);
        menv.gateOn (! wasActive);
        grain.noteOn (true);
        grainLfo.reset (0.0f);
        wt[0].noteOn (p[wtA_rphase] > 0.5f);
        wt[1].noteOn (p[wtB_rphase] > 0.5f);
        plaits.trigger();
        for (int i = 0; i < 4; ++i)
        {
            const int base = lfo1_shape + i * (lfo2_shape - lfo1_shape);
            if ((int) p[base + (lfo1_retrig - lfo1_shape)] == 1)
                lfo[i].reset (p[base + (lfo1_phase - lfo1_shape)]);
        }
        if (! wasActive)
        {
            filter.reset();
            for (auto& t : tz) t = 0.0f;
            prevA = prevB = prevM = prevG = prevN = 0.0f;
            plaits.reset();
        }
    }
    plaits.gate (true);
    ctrlCounter = 0;
}

void Voice::moveTo (int midiNote, const VoiceContext& ctx)
{
    juce::ignoreUnused (ctx);
    note = midiNote;
    targetPitch = (float) midiNote;
    held = true;
}

void Voice::release()
{
    held = false;
    amp.gateOff(); fenv.gateOff(); menv.gateOff();
    plaits.gate (false);
}

void Voice::kill()
{
    held = false;
    amp.kill(); fenv.kill(); menv.kill();
}

float Voice::source (int src) const
{
    switch (src)
    {
        case MS_Lfo1: case MS_Lfo2: case MS_Lfo3: case MS_Lfo4: return lfoVal[src - MS_Lfo1];
        case MS_ModEnv: return menv.current();
        case MS_FiltEnv: return fenv.current();
        case MS_AmpEnv: return amp.current();
        case MS_Velocity: return velocity;
        case MS_Note: return (currentPitch - 60.0f) / 48.0f;
        case MS_Random: return randomValue;
        default: return 0.0f;
    }
}

void Voice::controlUpdate (const VoiceContext& ctx)
{
    const float* p = ctx.p;
    const auto& g = *ctx.global;
    const float rate = (float) sr;

    // ---- envelopes ----
    amp.set (p[amp_a], p[amp_d], p[amp_s], p[amp_r]);
    fenv.set (p[fenv_a], p[fenv_d], p[fenv_s], p[fenv_r]);
    menv.set (p[menv_a], p[menv_d], p[menv_s], p[menv_r]);

    // ---- per-voice LFOs (Note retrig) or the shared free-running ones ----
    const float blockSecs = (float) kControlBlock / rate;
    for (int i = 0; i < 4; ++i)
    {
        const int base = lfo1_shape + i * (lfo2_shape - lfo1_shape);
        const int shape = (int) p[base];
        const bool retrig = (int) p[base + (lfo1_retrig - lfo1_shape)] == 1;
        if (! retrig) { lfoVal[i] = g.lfo[i]; continue; }
        float hz = p[base + (lfo1_rate - lfo1_shape)];
        if (p[base + (lfo1_sync - lfo1_shape)] > 0.5f)
            hz = (float) (g.bpm / 60.0 / choices::divisionInBeats ((int) p[base + (lfo1_div - lfo1_shape)]));
        lfoVal[i] = lfo[i].advance (shape, hz * blockSecs);
    }

    // ---- modulation matrix ----
    for (auto& m : mod) m = 0.0f;
    for (int s = 0; s < 16; ++s)
    {
        const int base = mtx01_src + s * (mtx02_src - mtx01_src);
        const int dst = (int) p[base + 1];
        const int src = (int) p[base];
        if (dst == MD_None || src == MS_None || dst >= MD_VoiceCount) continue;
        auto val = [&] (int sIdx) -> float
        {
            switch (sIdx)
            {
                case MS_Chaos1: case MS_Chaos2: return g.chaos[sIdx - MS_Chaos1];
                case MS_Macro1: case MS_Macro2: case MS_Macro3: case MS_Macro4: return g.macro[sIdx - MS_Macro1];
                case MS_Seq1: case MS_Seq2: case MS_Seq3: case MS_Seq4: return g.seq[sIdx - MS_Seq1];
                case MS_ModWheel: return g.modWheel;
                case MS_Aftertouch: return g.aftertouch;
                case MS_PitchBend: return g.pitchBend;
                default: return source (sIdx);
            }
        };
        float v = val (src) * p[base + 2];
        const int via = (int) p[base + 3];
        if (via != MS_None) v *= val (via);
        mod[dst] += v * modDepth (dst);
    }

    // ---- pitch ----
    const float bend = g.pitchBend * p[bend_range];
    const float pitch = currentPitch + bend;

    // ---- granular ----
    grOn = p[gr_on] > 0.5f && ctx.sample != nullptr;

    // two built-in grain modulators: an LFO and a smooth random walk
    const float gm1 = grainLfo.advance ((int) p[gm1_shape], p[gm1_rate] * blockSecs);
    grainRndPhase += p[gm2_rate] * blockSecs;
    if (grainRndPhase >= 1.0f) { grainRndPhase -= std::floor (grainRndPhase); grainRndTarget = grainRng.bi(); }
    {
        const float a = std::exp (-blockSecs / (0.002f + p[gm2_smooth] * p[gm2_smooth] / juce::jmax (0.05f, p[gm2_rate])));
        grainRnd = grainRndTarget + (grainRnd - grainRndTarget) * a;
    }
    const float gm2 = grainRnd;
    const bool frozen = p[gr_freeze] > 0.5f;

    auto& gp = grainParams;
    gp.mode = (int) p[gr_mode];
    gp.pos = p[gr_pos] + mod[MD_GrPos] + (frozen ? 0.0f : 0.5f * gm1 * p[gm1_pos]);
    gp.scan = p[gr_scan];
    gp.spray = clamp01 (p[gr_spray] + mod[MD_GrSpray] + 0.5f * gm2 * p[gm2_spray]);
    gp.sizeMs = clampf (p[gr_size] * std::exp2 (mod[MD_GrSize] + 2.0f * gm1 * p[gm1_size]), 2.0f, 2000.0f);
    gp.density = clampf (p[gr_density] * std::exp2 (mod[MD_GrDensity] + 2.0f * gm1 * p[gm1_density]), 0.5f, 400.0f);
    gp.pitch = p[gr_pitch] + mod[MD_GrPitch] + 12.0f * gm2 * p[gm2_pitch];
    gp.pitchRand = p[gr_prand];
    gp.keytrack = p[gr_keytrack] > 0.5f;
    gp.root = (int) p[gr_root];
    gp.window = (int) p[gr_window];
    gp.reverse = p[gr_reverse];
    gp.stereo = clamp01 (p[gr_stereo] + 0.5f * gm2 * p[gm2_stereo]);
    gp.regionStart = p[gr_start];
    gp.regionEnd = p[gr_end];
    gp.jitter = p[gr_jitter];
    gp.octRand = p[gr_octrand];
    gp.formant = p[gr_formant];
    gp.streams = (int) p[gr_streams];
    gp.streamDetune = p[gr_sdetune];
    gp.tilt = p[gr_tilt];
    gp.freeze = frozen;
    grain.setParams (gp, pitch);
    grLevel = clamp01 (p[gr_level] + mod[MD_GrLevel]);
    panGains (p[gr_pan], grGainL, grGainR);

    // ---- wavetables ----
    for (int o = 0; o < 2; ++o)
    {
        const int b = o == 0 ? (int) wtA_on : (int) wtB_on;
        wtOn[o] = p[b] > 0.5f && ctx.table[o] != nullptr;
        const float pitchMod = o == 0 ? mod[MD_WtAPitch] : mod[MD_WtBPitch];
        const float semis = 12.0f * p[b + (wtA_oct - wtA_on)] + p[b + (wtA_semi - wtA_on)] + p[b + (wtA_fine - wtA_on)] * 0.01f + pitchMod;
        wtFreqMul[o] = semisToRatio (semis);
        wtPos[o] = clamp01 (p[b + (wtA_pos - wtA_on)] + (o == 0 ? mod[MD_WtAPos] : mod[MD_WtBPos]));
        wtLevel[o] = clamp01 (p[b + (wtA_level - wtA_on)] + (o == 0 ? mod[MD_WtALevel] : mod[MD_WtBLevel]));
        wt[o].setup ((int) p[b + (wtA_unison - wtA_on)], p[b + (wtA_detune - wtA_on)], p[b + (wtA_spread - wtA_on)], p[b + (wtA_pan - wtA_on)]);
        wt[o].setWarp ((int) p[b + (wtA_warp - wtA_on)], p[b + (wtA_wamt - wtA_on)]);
    }

    // ---- modulator, sub, noise ----
    modWave = (int) p[mo_wave];
    modFreqMul = semisToRatio (12.0f * p[mo_oct] + p[mo_semi] + p[mo_fine] * 0.01f + mod[MD_ModPitch]);
    modLevel = clamp01 (p[mo_level] + mod[MD_ModLevel]);
    subWave = (int) p[sub_wave];
    subLevel = clamp01 (p[sub_level] + mod[MD_SubLevel]);
    noiseType = (int) p[noise_type];
    noiseLevel = clamp01 (p[noise_level] + mod[MD_NoiseLevel]);

    // ---- Plaits ----
    plOn = p[pl_on] > 0.5f;
    if (plOn)
    {
        PlaitsSource::Params pp;
        pp.engine = (int) p[pl_engine];
        pp.note = pitch + 12.0f * p[pl_oct];
        pp.harmonics = clamp01 (p[pl_harm] + mod[MD_PlHarm]);
        pp.timbre = clamp01 (p[pl_timbre] + mod[MD_PlTimbre]);
        pp.morph = clamp01 (p[pl_morph] + mod[MD_PlMorph]);
        pp.decay = p[pl_decay];
        pp.colour = p[pl_colour];
        pp.lpg = p[pl_lpg] > 0.5f;
        plaits.setParams (pp);
        plLevel = clamp01 (p[pl_level] + mod[MD_PlLevel]);
        panGains (p[pl_pan], plGainL, plGainR);
    }

    // ---- FM matrix ----
    thruZero = (int) p[fm_mode] == 1;
    const float fmAmt = clampf (p[fm_amount] + mod[MD_FmAmount], 0.0f, 2.0f);
    const int firstFm = fm_a_a;
    for (int s = 0; s < 5; ++s)
        for (int d = 0; d < 3; ++d)
        {
            const float v = p[firstFm + s * 3 + d];
            fm[s][d] = v * v * fmAmt * (thruZero ? 8.0f : 1.5f);
        }

    // ---- bite + filter ----
    bite.drive = clamp01 (p[bite_drive] + mod[MD_BiteDrive]);
    bite.fold = clamp01 (p[bite_fold] + mod[MD_BiteFold]);
    bite.crush = clamp01 (p[bite_crush] + mod[MD_BiteCrush]);
    bite.soft = p[bite_soft];
    bite.asym = p[bite_asym];
    bite.mix = p[bite_mix];
    bite.rect = p[bite_rect];
    bite.setTone (p[bite_tone], rate);
    bitePost = p[bite_post] > 0.5f;

    const float octs = p[flt_env] * fenv.current() * 7.0f + p[flt_key] * (pitch - 60.0f) / 12.0f + mod[MD_Cutoff];
    const float cutoff = clampf (p[flt_cutoff] * std::exp2 (octs), 16.0f, 20000.0f);
    filter.set ((int) p[flt_model], (int) p[flt_type], (int) p[flt_slope] == 1, cutoff,
                clamp01 (p[flt_res] + mod[MD_Resonance]), p[flt_drive], rate);

    // ---- output ----
    const float vol = juce::jmax (0.0f, 1.0f + mod[MD_Volume]) * (0.35f + 0.65f * velocity);
    panGains (mod[MD_Pan], volL, volR);
    volL *= vol * 1.4142f;
    volR *= vol * 1.4142f;

    // glide coefficient
    const float gt = p[glide];
    glideCoef = gt <= 0.0f ? 0.0f : std::exp (-1.0f / (gt * rate * 0.25f));
}

void Voice::render (float* outL, float* outR, int numSamples, const VoiceContext& ctx)
{
    if (! isActive()) return;
    const float rate = (float) sr;
    const float bendSemis = ctx.global->pitchBend * ctx.p[bend_range];

    for (int i = 0; i < numSamples; ++i)
    {
        if (ctrlCounter-- <= 0)
        {
            controlUpdate (ctx);
            ctrlCounter = kControlBlock - 1;
        }
        currentPitch = targetPitch + (currentPitch - targetPitch) * glideCoef;
        const float baseHz = midiToHz (currentPitch + bendSemis);

        const float env = amp.next();
        fenv.next();
        menv.next();

        // noise first (it is also an FM source)
        const float n = noise.tick (noiseType);

        // FM inputs from the previous sample
        float pmA = fm[0][0] * prevA + fm[1][0] * prevB + fm[2][0] * prevM + fm[3][0] * prevG + fm[4][0] * n;
        float pmB = fm[0][1] * prevA + fm[1][1] * prevB + fm[2][1] * prevM + fm[3][1] * prevG + fm[4][1] * n;
        float pmM = fm[0][2] * prevA + fm[1][2] * prevB + fm[2][2] * prevM + fm[3][2] * prevG + fm[4][2] * n;
        if (thruZero)
        {
            // linear FM: integrate frequency deviation into an extra phase offset
            tz[0] += pmA * baseHz * wtFreqMul[0] / rate;
            tz[1] += pmB * baseHz * wtFreqMul[1] / rate;
            tz[2] += pmM * baseHz * modFreqMul / rate;
            for (auto& t : tz) t -= std::floor (t);
            pmA = tz[0]; pmB = tz[1]; pmM = tz[2];
        }

        float l = 0.0f, r = 0.0f;

        // modulator oscillator (always runs: it is an FM source even at level 0)
        const float m = modOsc.tick (modWave, baseHz * modFreqMul, rate, pmM);
        prevM = m;
        l += m * modLevel * 0.7f;
        r += m * modLevel * 0.7f;

        for (int o = 0; o < 2; ++o)
        {
            if (! wtOn[o]) continue;
            float wl, wr;
            wt[o].tick (*ctx.table[o], baseHz * wtFreqMul[o], wtPos[o], o == 0 ? pmA : pmB, wl, wr);
            l += wl * wtLevel[o];
            r += wr * wtLevel[o];
        }
        prevA = wtOn[0] ? wt[0].lastMono() : 0.0f;
        prevB = wtOn[1] ? wt[1].lastMono() : 0.0f;

        if (grOn)
        {
            float gl, gr;
            grain.tick (*ctx.sample, gl, gr);
            prevG = (gl + gr) * 0.5f;
            l += gl * grLevel * grGainL * 1.4142f;
            r += gr * grLevel * grGainR * 1.4142f;
        }
        else prevG = 0.0f;

        if (subLevel > 0.0f)
        {
            const float s = subOsc.tick (subWave == 2 ? 3 : subWave, baseHz * (ctx.p[sub_oct] < -1.5f ? 0.25f : 0.5f), rate, 0.0f);
            l += s * subLevel * 0.7f;
            r += s * subLevel * 0.7f;
        }
        if (noiseLevel > 0.0f)
        {
            l += n * noiseLevel * 0.7f;
            r += n * noiseLevel * 0.7f;
        }
        prevN = n;

        if (plOn)
        {
            const float pl = plaits.tick() * plLevel;
            l += pl * plGainL * 1.4142f;
            r += pl * plGainR * 1.4142f;
        }

        // remove the DC that asymmetric warps (Bloat, Mirror, Pulse) add, before distortion and filter
        { const float yl = l - dcX[0] + 0.9995f * dcY[0]; dcX[0] = l; dcY[0] = yl; l = yl; }
        { const float yr = r - dcX[1] + 0.9995f * dcY[1]; dcX[1] = r; dcY[1] = yr; r = yr; }
        if (! bitePost) bite.process (l, r);
        l = filter.tick (0, l);
        r = filter.tick (1, r);
        if (bitePost) bite.process (l, r);

        outL[i] += l * env * volL;
        outR[i] += r * env * volR;

        if (! amp.active()) break;
    }

    if (ctx.viz != nullptr && grOn)
        grain.writeViz (*ctx.sample, *ctx.viz);
}
} // namespace pg
