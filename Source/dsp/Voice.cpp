#include "Voice.h"

namespace pg
{
float modDepth (int dst)
{
    switch (dst)
    {
        case MD_WtAPitch: case MD_WtBPitch: case MD_WtCPitch: case MD_GrPitch: case MD_SpPitch: case MD_SmpPitch: return 48.0f;   // semitones (+/- 4 octaves)
        case MD_SpShift: return 24.0f;
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
    for (auto& e : menvX) e.setSampleRate (r);
    grain.prepare (sr, 0x1234u + (uint32_t) idx * 7919u);
    wt[0].prepare (sr, 0x51u + (uint32_t) idx * 31u);
    wt[1].prepare (sr, 0x97u + (uint32_t) idx * 37u);
    wt[2].prepare (sr, 0xC3u + (uint32_t) idx * 41u);
    spectral.prepare (sr, 0x5BEC7u + (uint32_t) idx * 53u);
    shot.prepare (sr);
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
        for (auto& e : menvX) e.gateOn (! wasActive);
        grain.noteOn (true);
        grainLfo.reset (0.0f);
        wt[0].noteOn (p[wtA_rphase] > 0.5f, p[wtA_phase]);
        wt[1].noteOn (p[wtB_rphase] > 0.5f, p[wtB_phase]);
        wt[2].noteOn (p[wtC_rphase] > 0.5f, p[wtC_phase]);
        spectral.noteOn (0.0f);
        subOsc.phase = p[sub_phase];
        if (p[smp_on] > 0.5f && ctx.shot != nullptr)
        {
            const float semis = p[smp_pitch] + (p[smp_key] > 0.5f ? (float) midiNote - 60.0f : 0.0f);
            shot.trigger (*ctx.shot, p[smp_start], semisToRatio (semis), p[smp_attack], p[smp_decay]);
        }
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
            prevA = prevB = prevC = prevS = prevG = prevN = 0.0f;
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
    for (auto& e : menvX) e.gateOff();
    plaits.gate (false);
}

void Voice::kill()
{
    held = false;
    amp.kill(); fenv.kill(); menv.kill();
    for (auto& e : menvX) e.kill();
}

float Voice::source (int src) const
{
    switch (src)
    {
        case MS_Lfo1: case MS_Lfo2: case MS_Lfo3: case MS_Lfo4: return lfoVal[src - MS_Lfo1];
        case MS_ModEnv: return menv.current();
        case MS_ModEnv2: case MS_ModEnv3: case MS_ModEnv4: return menvX[src - MS_ModEnv2].current();
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
    for (int e = 0; e < 3; ++e)
    {
        const int b = menv2_a + e * (menv3_a - menv2_a);
        menvX[e].set (p[b], p[b + 1], p[b + 2], p[b + 3]);
    }

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
    for (int s = 0; s < kMtxSlots; ++s)
    {
        const int base = mtx01_src + s * (mtx02_src - mtx01_src);
        const int dst = (int) p[base + 1];
        const int src = (int) p[base];
        if (dst == MD_None || src == MS_None || dst >= MD_VoiceCount) continue;
        auto val = [&] (int sIdx) -> float
        {
            switch (sIdx)
            {
                case MS_Chaos1: case MS_Chaos2: case MS_Chaos2Y: case MS_Chaos2Z: return g.chaos[sIdx - MS_Chaos1];
                case MS_Seq1: case MS_Seq2: case MS_Seq3: case MS_Seq4:
                case MS_Seq5: case MS_Seq6: case MS_Seq7: case MS_Seq8: return g.seq[sIdx - MS_Seq1];
                case MS_ModWheel: return g.modWheel;
                case MS_Aftertouch: return g.aftertouch;
                case MS_PitchBend: return g.pitchBend;
                default: return source (sIdx);
            }
        };
        const int pol = (int) p[base + 4];
        float v = applyPolarity (val (src), src, pol, p) * p[base + 2];
        const int via = (int) p[base + 3];
        if (via != MS_None) v *= applyPolarity (val (via), via, pol == 2 ? 0 : pol, p);
        mod[dst] += v * modDepth (dst);
    }
    // SEQ lanes routed from their own page (Route to matrix)
    for (int l = 0; l < 8; ++l)
    {
        const int base = seq1_on + l * (seq2_on - seq1_on);
        const int dst = (int) p[base + (seq1_dst - seq1_on)];
        if (dst == MD_None || dst >= MD_VoiceCount || p[base] < 0.5f) continue;
        mod[dst] += g.seq[l] * p[base + (seq1_amt - seq1_on)] * modDepth (dst);
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
    static const int wtBase[3] = { wtA_on, wtB_on, wtC_on };
    static const int wtDst[3][4] = { { MD_WtAPos, MD_WtAPitch, MD_WtALevel, MD_WtAWarp },
                                     { MD_WtBPos, MD_WtBPitch, MD_WtBLevel, MD_WtBWarp },
                                     { MD_WtCPos, MD_WtCPitch, MD_WtCLevel, MD_WtCWarp } };
    for (int o = 0; o < 3; ++o)
    {
        const int b = wtBase[o];
        auto q = [&] (int id) { return p[b + (id - wtA_on)]; };
        wtOn[o] = q (wtA_on) > 0.5f && ctx.table[o] != nullptr;
        const float semis = 12.0f * q (wtA_oct) + q (wtA_semi) + q (wtA_fine) * 0.01f + mod[wtDst[o][1]];
        wtFreqMul[o] = semisToRatio (semis);
        wtPos[o] = clamp01 (q (wtA_pos) + mod[wtDst[o][0]]);
        wtLevel[o] = clamp01 (q (wtA_level) + mod[wtDst[o][2]]);
        wt[o].setup ((int) q (wtA_unison), q (wtA_detune), q (wtA_spread), q (wtA_pan));
        wt[o].setWarp ((int) q (wtA_warp), q (wtA_wamt) + mod[wtDst[o][3]]);
        wt[o].setWarp2 ((int) q (wtA_warp2), q (wtA_wamt2) + mod[wtDst[o][3]]);
    }

    // ---- spectral ----
    spOn = p[sp_on] > 0.5f && ctx.spectral != nullptr;
    if (spOn)
    {
        auto& sp = spParams;
        sp.pos = clamp01 (p[sp_pos] + mod[MD_SpPos]);
        sp.scan = p[sp_scan];
        sp.partials = (int) p[sp_partials];
        sp.tilt = clampf (p[sp_tilt] + mod[MD_SpTilt], -1.0f, 1.0f);
        sp.stretch = clampf (p[sp_stretch] + mod[MD_SpStretch], -1.0f, 1.0f);
        sp.shift = clampf (p[sp_shift] + mod[MD_SpShift], -36.0f, 36.0f);
        sp.oddEven = p[sp_oddeven];
        sp.blur = p[sp_blur];
        sp.shimmer = p[sp_shimmer];
        sp.width = p[sp_width];
        sp.freeze = p[sp_freeze] > 0.5f;
        sp.warp = (int) p[sp_warp];
        sp.warpAmt = clamp01 (p[sp_wamt] + mod[MD_SpWarp]);
        sp.partialDecay = p[sp_pdecay];
        sp.jitter = p[sp_jitter];
        spFreqMul = semisToRatio (12.0f * p[sp_oct] + p[sp_semi] + p[sp_fine] * 0.01f + mod[MD_SpPitch]);
        spLevel = clamp01 (p[sp_level] + mod[MD_SpLevel]);
        panGains (p[sp_pan], spGainL, spGainR);
        spectral.update (*ctx.spectral, sp, midiToHz (pitch) * spFreqMul, kControlBlock);
    }

    // ---- sampler, sub, noise ----
    smpOn = p[smp_on] > 0.5f && ctx.shot != nullptr;
    if (smpOn)
    {
        smpLevel = clamp01 (p[smp_level] + mod[MD_SmpLevel]);
        panGains (p[smp_pan], smpGainL, smpGainR);
        const float semis = p[smp_pitch] + mod[MD_SmpPitch] + (p[smp_key] > 0.5f ? (float) note - 60.0f : 0.0f);
        shot.setRatio (*ctx.shot, semisToRatio (semis));
    }
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

    // ---- FM matrix: sources A, B, C, Spectral, Grain, Noise -> destinations A, B, C, Spectral ----
    fmType[0] = (int) p[fm_type_a]; fmType[1] = (int) p[fm_type_b]; fmType[2] = (int) p[fm_type_c]; fmType[3] = (int) p[fm_type_s];
    const float fmAmt = clampf (p[fm_amount] + mod[MD_FmAmount], 0.0f, 2.0f);
    const int firstFm = fm_a_a;
    for (int sI = 0; sI < 6; ++sI)
        for (int d = 0; d < 4; ++d)
        {
            const float v = p[firstFm + sI * 4 + d];
            fm[sI][d] = v * v * fmAmt;
        }
    for (int d = 0; d < 4; ++d)
    {
        float t = 0.0f;
        for (int sI = 0; sI < 6; ++sI) t += fm[sI][d];
        fmDepth[d] = juce::jmin (1.0f, t);
    }

    // ---- routing ----
    rtFilter[0] = p[rt_flt_a] > 0.5f; rtFilter[1] = p[rt_flt_b] > 0.5f; rtFilter[2] = p[rt_flt_c] > 0.5f;
    rtFilter[3] = p[rt_flt_sp] > 0.5f; rtFilter[4] = p[rt_flt_gr] > 0.5f; rtFilter[5] = p[rt_flt_pl] > 0.5f;
    rtFilter[6] = p[rt_flt_sub] > 0.5f; rtFilter[7] = p[rt_flt_smp] > 0.5f;
    spDist = p[rt_dist_sp] > 0.5f;

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
        for (auto& e : menvX) e.next();

        // noise first (it is also an FM source)
        const float n = noise.tick (noiseType);

        // Modulation matrix inputs (previous sample of each source), one sum per destination
        const float srcv[6] = { prevA, prevB, prevC, prevS, prevG, n };
        float sum[4];
        for (int d = 0; d < 4; ++d)
        {
            float t = 0.0f;
            for (int sI = 0; sI < 6; ++sI) t += fm[sI][d] * srcv[sI];
            sum[d] = t;
        }
        // turn each sum into the destination's kind of modulation
        float pm[4] = {}, pd[4] = {}, gain[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        const float destHz[4] = { baseHz * wtFreqMul[0], baseHz * wtFreqMul[1], baseHz * wtFreqMul[2], baseHz * spFreqMul };
        for (int d = 0; d < 4; ++d)
        {
            switch (fmType[d])
            {
                case 0: pm[d] = sum[d] * 1.5f; break;                                   // phase modulation
                case 1:                                                                // thru-zero (linear) FM
                    tz[d] += sum[d] * 8.0f * destHz[d] / rate;
                    tz[d] -= std::floor (tz[d]);
                    pm[d] = tz[d];
                    break;
                case 2: pd[d] = clampf (sum[d] * 1.2f, -0.95f, 0.95f); break;          // phase distortion
                case 3: gain[d] = 1.0f - 0.5f * fmDepth[d] + 0.5f * sum[d]; break;    // amplitude modulation
                default: gain[d] = 1.0f - fmDepth[d] + sum[d]; break;                  // ring modulation
            }
        }
        // two buses: F goes through distortion + filter, B bypasses the filter
        float fl = 0.0f, fr = 0.0f, bl = 0.0f, br = 0.0f, nl = 0.0f, nr = 0.0f;   // n = filtered but not distorted
        auto add = [&] (int route, float xl, float xr) { if (rtFilter[route]) { fl += xl; fr += xr; } else { bl += xl; br += xr; } };
        for (int o = 0; o < 3; ++o)
        {
            if (! wtOn[o]) continue;
            float wl, wr;
            wt[o].tick (*ctx.table[o], destHz[o], wtPos[o], pm[o], wl, wr, pd[o]);
            add (o, wl * wtLevel[o] * gain[o], wr * wtLevel[o] * gain[o]);
        }
        prevA = wtOn[0] ? wt[0].lastMono() * gain[0] : 0.0f;
        prevB = wtOn[1] ? wt[1].lastMono() * gain[1] : 0.0f;
        prevC = wtOn[2] ? wt[2].lastMono() * gain[2] : 0.0f;

        if (spOn)
        {
            float sl, sr2;
            if (pm[3] != 0.0f || pd[3] != 0.0f) spectral.tickPM (pm[3] + pd[3] * 0.25f, sl, sr2);
            else spectral.tick (sl, sr2);
            sl *= gain[3]; sr2 *= gain[3];
            prevS = (sl + sr2) * 0.5f;
            const float xl = sl * spLevel * spGainL * 1.4142f, xr = sr2 * spLevel * spGainR * 1.4142f;
            if (rtFilter[3] && ! spDist) { nl += xl; nr += xr; }
            else add (3, xl, xr);
        }
        else prevS = 0.0f;

        if (grOn)
        {
            float gl, gr;
            grain.tick (*ctx.sample, gl, gr);
            prevG = (gl + gr) * 0.5f;
            add (4, gl * grLevel * grGainL * 1.4142f, gr * grLevel * grGainR * 1.4142f);
        }
        else prevG = 0.0f;

        if (subLevel > 0.0f)
        {
            const float s0 = subOsc.tick (subWave == 2 ? 3 : subWave, baseHz * (ctx.p[sub_oct] < -1.5f ? 0.25f : 0.5f), rate, 0.0f);
            add (6, s0 * subLevel * 0.7f, s0 * subLevel * 0.7f);
        }
        if (noiseLevel > 0.0f) add (6, n * noiseLevel * 0.7f, n * noiseLevel * 0.7f);
        prevN = n;

        if (plOn)
        {
            const float pl = plaits.tick() * plLevel;
            add (5, pl * plGainL * 1.4142f, pl * plGainR * 1.4142f);
        }
        float shotL = 0.0f, shotR = 0.0f;
        if (smpOn)
        {
            const float x = shot.tick (*ctx.shot) * smpLevel;
            if (rtFilter[7]) { fl += x * smpGainL * 1.4142f; fr += x * smpGainR * 1.4142f; }
            else { shotL = x * smpGainL * 1.4142f; shotR = x * smpGainR * 1.4142f; }
        }

        // remove the DC that asymmetric warps (Bloat, Mirror, Pulse) add, before distortion and filter
        { const float yl = fl - dcX[0] + 0.9995f * dcY[0]; dcX[0] = fl; dcY[0] = yl; fl = yl; }
        { const float yr = fr - dcX[1] + 0.9995f * dcY[1]; dcX[1] = fr; dcY[1] = yr; fr = yr; }
        if (! bitePost) bite.process (fl, fr);
        float l = filter.tick (0, fl + nl) + bl;
        float r = filter.tick (1, fr + nr) + br;
        if (bitePost) bite.process (l, r);

        outL[i] += l * env * volL;
        outR[i] += r * env * volR;

        // the one-shot sampler has its own envelope: unless routed through the filter it skips the amp envelope so clicks stay sharp
        outL[i] += shotL * volL;
        outR[i] += shotR * volR;

        if (! amp.active() && ! (smpOn && shot.isPlaying())) break;
    }

    if (ctx.viz != nullptr && grOn)
        grain.writeViz (*ctx.sample, *ctx.viz);
}
} // namespace pg
