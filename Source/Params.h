#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// =============================================================================
// Every automatable parameter is declared once in PG_PARAM_LIST.
//   F(id, name, min, max, default, skewCentre)   continuous (skewCentre 0 = linear)
//   I(id, name, min, max, default)               integer
//   C(id, name, choiceList, default)             choice (index)
//   B(id, name, default)                         on/off
// The list generates the PID enum (PID::id), the APVTS layout and the fast cache.
// =============================================================================

namespace pg
{
namespace choices
{
    juce::StringArray voiceModes();
    juce::StringArray wavetables();
    juce::StringArray warpModes();
    juce::StringArray grainModShapes();
    juce::StringArray grainWindows();
    juce::StringArray grainModes();
    juce::StringArray subWaves();
    juce::StringArray noiseTypes();
    juce::StringArray plaitsEngines();
    juce::StringArray fmModes();
    juce::StringArray filterTypes();
    juce::StringArray filterModels();
    juce::StringArray filterSlopes();
    juce::StringArray lfoShapes();
    juce::StringArray divisions();
    juce::StringArray retrigModes();
    juce::StringArray modSources();
    juce::StringArray modDests();
    juce::StringArray modPolarity();
    juce::StringArray seqModes();
    juce::StringArray arpModes();
    juce::StringArray ringsModels();
    juce::StringArray ringsPoly();
    juce::StringArray cloudsModes();
    juce::StringArray cloudsQuality();
    juce::StringArray seqDirections();
    juce::StringArray scales();
    juce::StringArray noteNames();
    juce::StringArray fxTypes();
    juce::StringArray filterFxTypes();
    juce::StringArray spectralSounds();
    juce::StringArray samplerSounds();
    juce::StringArray chaosTypes();
    juce::StringArray spectralWarps();
    juce::StringArray mixbusTypes();
    juce::StringArray gatePatterns();
    juce::StringArray routeOrders();
    juce::StringArray oversamplingModes();
    juce::StringArray distortionPositions();

    double divisionInBeats (int index);
}

#define PG_LFO(PF, PI, PC, PB, n) \
    PC(lfo##n##_shape, "LFO " #n " Shape", lfoShapes, 0) \
    PF(lfo##n##_rate, "LFO " #n " Rate", 0.02f, 30.0f, 1.0f, 2.0f) \
    PB(lfo##n##_sync, "LFO " #n " Sync", false) \
    PC(lfo##n##_div, "LFO " #n " Division", divisions, 10) \
    PF(lfo##n##_phase, "LFO " #n " Phase", 0.0f, 1.0f, 0.0f, 0.0f) \
    PC(lfo##n##_retrig, "LFO " #n " Retrig", retrigModes, 1)

#define PG_SEQ(PF, PI, PC, PB, n) \
    PB(seq##n##_on, "Seq " #n " On", true) \
    PI(seq##n##_len, "Seq " #n " Length", 1, 32, 16) \
    PC(seq##n##_div, "Seq " #n " Division", divisions, 4) \
    PF(seq##n##_swing, "Seq " #n " Swing", 0.0f, 0.75f, 0.0f, 0.0f) \
    PC(seq##n##_mode, "Seq " #n " Mode", seqModes, 0) \
    PF(seq##n##_glide, "Seq " #n " Glide", 0.0f, 1.0f, 0.0f, 0.0f) \
    PC(seq##n##_dir, "Seq " #n " Direction", seqDirections, 0) \
    PC(seq##n##_dst, "Seq " #n " Destination", modDests, 0) \
    PF(seq##n##_amt, "Seq " #n " Amount", -1.0f, 1.0f, 0.5f, 0.0f)

#define PG_MENV(PF, PI, PC, PB, n) \
    PF(menv##n##_a, "Mod Env " #n " Attack", 0.001f, 10.0f, 0.005f, 0.5f) \
    PF(menv##n##_d, "Mod Env " #n " Decay", 0.001f, 10.0f, 0.5f, 0.5f) \
    PF(menv##n##_s, "Mod Env " #n " Sustain", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(menv##n##_r, "Mod Env " #n " Release", 0.001f, 15.0f, 0.3f, 0.8f)

#define PG_MTX(PF, PI, PC, PB, n) \
    PC(mtx##n##_src, "Matrix " #n " Source", modSources, 0) \
    PC(mtx##n##_dst, "Matrix " #n " Destination", modDests, 0) \
    PF(mtx##n##_amt, "Matrix " #n " Amount", -1.0f, 1.0f, 0.0f, 0.0f) \
    PC(mtx##n##_via, "Matrix " #n " Via", modSources, 0) \
    PC(mtx##n##_pol, "Matrix " #n " Polarity", modPolarity, 0)

#define PG_WT(PF, PI, PC, PB, o, O) \
    PB(wt##o##_on, "WT " #O " On", false) \
    PC(wt##o##_table, "WT " #O " Table", wavetables, 0) \
    PF(wt##o##_pos, "WT " #O " Position", 0.0f, 1.0f, 0.0f, 0.0f) \
    PC(wt##o##_warp, "WT " #O " Warp Mode", warpModes, 0) \
    PF(wt##o##_wamt, "WT " #O " Warp Amount", 0.0f, 1.0f, 0.0f, 0.0f) \
    PC(wt##o##_warp2, "WT " #O " Warp 2 Mode", warpModes, 0) \
    PF(wt##o##_wamt2, "WT " #O " Warp 2 Amount", 0.0f, 1.0f, 0.0f, 0.0f) \
    PI(wt##o##_oct, "WT " #O " Octave", -4, 4, 0) \
    PI(wt##o##_semi, "WT " #O " Semitone", -12, 12, 0) \
    PF(wt##o##_fine, "WT " #O " Fine", -100.0f, 100.0f, 0.0f, 0.0f) \
    PI(wt##o##_unison, "WT " #O " Unison", 1, 7, 1) \
    PF(wt##o##_detune, "WT " #O " Detune", 0.0f, 1.0f, 0.2f, 0.0f) \
    PF(wt##o##_spread, "WT " #O " Spread", 0.0f, 1.0f, 0.6f, 0.0f) \
    PF(wt##o##_level, "WT " #O " Level", 0.0f, 1.0f, 0.7f, 0.0f) \
    PF(wt##o##_pan, "WT " #O " Pan", -1.0f, 1.0f, 0.0f, 0.0f) \
    PB(wt##o##_rphase, "WT " #O " Random Phase", true) \
    PF(wt##o##_phase, "WT " #O " Phase", 0.0f, 1.0f, 0.0f, 0.0f)

#define PG_FF(PF, PI, PC, PB, n) \
    PB(ff##n##_on, "Filter FX " #n " On", false) \
    PC(ff##n##_type, "Filter FX " #n " Type", filterFxTypes, 0) \
    PF(ff##n##_freq, "Filter FX " #n " Frequency", 20.0f, 20000.0f, 800.0f, 1000.0f) \
    PF(ff##n##_res, "Filter FX " #n " Resonance", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(ff##n##_morph, "Filter FX " #n " Morph", 0.0f, 1.0f, 0.5f, 0.0f) \
    PI(ff##n##_poles, "Filter FX " #n " Poles", 1, 8, 2) \
    PF(ff##n##_spread, "Filter FX " #n " Stereo Spread", 0.0f, 1.0f, 0.2f, 0.0f) \
    PF(ff##n##_drive, "Filter FX " #n " Drive", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(ff##n##_mix, "Filter FX " #n " Mix", 0.0f, 1.0f, 1.0f, 0.0f)

#define PG_PARAM_LIST(PF, PI, PC, PB) \
    /* ---- global ---- */ \
    PF(master_gain, "Master", -36.0f, 6.0f, -6.0f, 0.0f) \
    PC(voice_mode, "Voice Mode", voiceModes, 0) \
    PI(polyphony, "Polyphony", 1, 16, 8) \
    PF(glide, "Glide", 0.0f, 2.0f, 0.0f, 0.3f) \
    PI(bend_range, "Bend Range", 0, 24, 2) \
    /* ---- envelopes ---- */ \
    PF(amp_a, "Amp Attack", 0.001f, 10.0f, 0.005f, 0.5f) \
    PF(amp_d, "Amp Decay", 0.001f, 10.0f, 0.3f, 0.5f) \
    PF(amp_s, "Amp Sustain", 0.0f, 1.0f, 0.8f, 0.0f) \
    PF(amp_r, "Amp Release", 0.001f, 15.0f, 0.4f, 0.8f) \
    PF(fenv_a, "Filter Env Attack", 0.001f, 10.0f, 0.005f, 0.5f) \
    PF(fenv_d, "Filter Env Decay", 0.001f, 10.0f, 0.4f, 0.5f) \
    PF(fenv_s, "Filter Env Sustain", 0.0f, 1.0f, 0.3f, 0.0f) \
    PF(fenv_r, "Filter Env Release", 0.001f, 15.0f, 0.4f, 0.8f) \
    PF(menv_a, "Mod Env Attack", 0.001f, 10.0f, 0.005f, 0.5f) \
    PF(menv_d, "Mod Env Decay", 0.001f, 10.0f, 0.5f, 0.5f) \
    PF(menv_s, "Mod Env Sustain", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(menv_r, "Mod Env Release", 0.001f, 15.0f, 0.3f, 0.8f) \
    PG_MENV(PF, PI, PC, PB, 2) PG_MENV(PF, PI, PC, PB, 3) PG_MENV(PF, PI, PC, PB, 4) \
    /* ---- granular oscillator ---- */ \
    PB(gr_on, "Grain On", true) \
    PC(gr_mode, "Grain Mode", grainModes, 0) \
    PF(gr_pos, "Grain Position", 0.0f, 1.0f, 0.3f, 0.0f) \
    PF(gr_scan, "Grain Scan", -2.0f, 2.0f, 0.0f, 0.0f) \
    PF(gr_spray, "Grain Spray", 0.0f, 1.0f, 0.05f, 0.0f) \
    PF(gr_size, "Grain Size", 5.0f, 1000.0f, 120.0f, 100.0f) \
    PF(gr_density, "Grain Density", 1.0f, 200.0f, 20.0f, 20.0f) \
    PF(gr_pitch, "Grain Pitch", -48.0f, 48.0f, 0.0f, 0.0f) \
    PF(gr_prand, "Grain Pitch Random", 0.0f, 12.0f, 0.0f, 0.0f) \
    PB(gr_keytrack, "Grain Keytrack", true) \
    PI(gr_root, "Grain Root Note", 24, 96, 60) \
    PC(gr_window, "Grain Window", grainWindows, 0) \
    PF(gr_reverse, "Grain Reverse", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(gr_stereo, "Grain Stereo", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(gr_level, "Grain Level", 0.0f, 1.0f, 0.8f, 0.0f) \
    PF(gr_pan, "Grain Pan", -1.0f, 1.0f, 0.0f, 0.0f) \
    PF(gr_start, "Grain Region Start", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(gr_end, "Grain Region End", 0.0f, 1.0f, 1.0f, 0.0f) \
    PF(gr_jitter, "Grain Timing Jitter", 0.0f, 1.0f, 0.3f, 0.0f) \
    PF(gr_octrand, "Grain Octave Random", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(gr_formant, "Grain Formant", -24.0f, 24.0f, 0.0f, 0.0f) \
    PI(gr_streams, "Grain Streams", 1, 4, 1) \
    PF(gr_sdetune, "Grain Stream Detune", 0.0f, 1.0f, 0.15f, 0.0f) \
    PF(gr_tilt, "Grain Window Tilt", -1.0f, 1.0f, 0.0f, 0.0f) \
    PB(gr_freeze, "Grain Freeze", false) \
    PC(gm1_shape, "Grain Mod 1 Shape", grainModShapes, 0) \
    PF(gm1_rate, "Grain Mod 1 Rate", 0.01f, 20.0f, 0.3f, 1.0f) \
    PF(gm1_pos, "Grain Mod 1 > Position", -1.0f, 1.0f, 0.0f, 0.0f) \
    PF(gm1_size, "Grain Mod 1 > Size", -1.0f, 1.0f, 0.0f, 0.0f) \
    PF(gm1_density, "Grain Mod 1 > Density", -1.0f, 1.0f, 0.0f, 0.0f) \
    PF(gm2_rate, "Grain Mod 2 Rate", 0.01f, 20.0f, 1.5f, 1.0f) \
    PF(gm2_smooth, "Grain Mod 2 Smooth", 0.0f, 1.0f, 0.6f, 0.0f) \
    PF(gm2_pitch, "Grain Mod 2 > Pitch", -1.0f, 1.0f, 0.0f, 0.0f) \
    PF(gm2_spray, "Grain Mod 2 > Spray", -1.0f, 1.0f, 0.0f, 0.0f) \
    PF(gm2_stereo, "Grain Mod 2 > Stereo", -1.0f, 1.0f, 0.0f, 0.0f) \
    /* ---- wavetables ---- */ \
    PG_WT(PF, PI, PC, PB, A, A) \
    PG_WT(PF, PI, PC, PB, B, B) \
    PG_WT(PF, PI, PC, PB, C, C) \
    /* ---- spectral oscillator ---- */ \
    PB(sp_on, "Spectral On", false) \
    PC(sp_sound, "Spectral Sound", spectralSounds, 0) \
    PF(sp_pos, "Spectral Position", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(sp_scan, "Spectral Scan Speed", -2.0f, 2.0f, 0.0f, 0.0f) \
    PI(sp_partials, "Spectral Partials", 1, 64, 48) \
    PF(sp_tilt, "Spectral Tilt", -1.0f, 1.0f, 0.0f, 0.0f) \
    PF(sp_stretch, "Spectral Stretch", -1.0f, 1.0f, 0.0f, 0.0f) \
    PF(sp_shift, "Spectral Formant Shift", -24.0f, 24.0f, 0.0f, 0.0f) \
    PF(sp_oddeven, "Spectral Odd/Even", -1.0f, 1.0f, 0.0f, 0.0f) \
    PF(sp_blur, "Spectral Blur", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(sp_shimmer, "Spectral Shimmer", 0.0f, 1.0f, 0.0f, 0.0f) \
    PB(sp_freeze, "Spectral Freeze", false) \
    PI(sp_oct, "Spectral Octave", -4, 4, 0) \
    PI(sp_semi, "Spectral Semitone", -12, 12, 0) \
    PF(sp_fine, "Spectral Fine", -100.0f, 100.0f, 0.0f, 0.0f) \
    PF(sp_level, "Spectral Level", 0.0f, 1.0f, 0.7f, 0.0f) \
    PF(sp_pan, "Spectral Pan", -1.0f, 1.0f, 0.0f, 0.0f) \
    PF(sp_width, "Spectral Width", 0.0f, 1.0f, 0.4f, 0.0f) \
    PC(sp_warp, "Spectral Warp Mode", spectralWarps, 0) \
    PF(sp_wamt, "Spectral Warp Amount", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(sp_pdecay, "Spectral Partial Decay", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(sp_jitter, "Spectral Jitter", 0.0f, 1.0f, 0.0f, 0.0f) \
    /* ---- one-shot sampler / sub / noise ---- */ \
    PB(smp_on, "Sampler On", false) \
    PC(smp_sound, "Sampler Sound", samplerSounds, 0) \
    PF(smp_pitch, "Sampler Pitch", -48.0f, 48.0f, 0.0f, 0.0f) \
    PB(smp_key, "Sampler Keytrack", false) \
    PF(smp_start, "Sampler Start", 0.0f, 0.9f, 0.0f, 0.0f) \
    PF(smp_attack, "Sampler Attack", 0.0f, 0.5f, 0.0f, 0.05f) \
    PF(smp_decay, "Sampler Decay", 0.005f, 5.0f, 0.25f, 0.4f) \
    PF(smp_level, "Sampler Level", 0.0f, 1.0f, 0.7f, 0.0f) \
    PF(smp_pan, "Sampler Pan", -1.0f, 1.0f, 0.0f, 0.0f) \
    PC(sub_wave, "Sub Wave", subWaves, 0) \
    PI(sub_oct, "Sub Octave", -2, -1, -1) \
    PF(sub_level, "Sub Level", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(sub_phase, "Sub Phase", 0.0f, 1.0f, 0.0f, 0.0f) \
    PC(noise_type, "Noise Type", noiseTypes, 0) \
    PF(noise_level, "Noise Level", 0.0f, 1.0f, 0.0f, 0.0f) \
    /* ---- Plaits ---- */ \
    PB(pl_on, "Plaits On", false) \
    PC(pl_engine, "Plaits Engine", plaitsEngines, 8) \
    PI(pl_oct, "Plaits Octave", -4, 4, 0) \
    PF(pl_harm, "Plaits Harmonics", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(pl_timbre, "Plaits Timbre", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(pl_morph, "Plaits Morph", 0.0f, 1.0f, 0.5f, 0.0f) \
    PB(pl_lpg, "Plaits LPG", false) \
    PF(pl_decay, "Plaits Decay", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(pl_colour, "Plaits LPG Colour", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(pl_level, "Plaits Level", 0.0f, 1.0f, 0.7f, 0.0f) \
    PF(pl_pan, "Plaits Pan", -1.0f, 1.0f, 0.0f, 0.0f) \
    /* ---- FM matrix: source -> destination (A, B, C) ---- */ \
    PC(fm_type_a, "Mod Matrix Type A", fmModes, 0) \
    PC(fm_type_b, "Mod Matrix Type B", fmModes, 0) \
    PC(fm_type_c, "Mod Matrix Type C", fmModes, 0) \
    PC(fm_type_s, "Mod Matrix Type Spectral", fmModes, 0) \
    PF(fm_amount, "FM Amount", 0.0f, 1.0f, 1.0f, 0.0f) \
    PF(fm_a_a, "FM A>A", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_a_b, "FM A>B", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_a_c, "FM A>C", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_a_s, "FM A>Spectral", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_b_a, "FM B>A", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_b_b, "FM B>B", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_b_c, "FM B>C", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_b_s, "FM B>Spectral", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_c_a, "FM C>A", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_c_b, "FM C>B", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_c_c, "FM C>C", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_c_s, "FM C>Spectral", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_s_a, "FM Spectral>A", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_s_b, "FM Spectral>B", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_s_c, "FM Spectral>C", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_s_s, "FM Spectral>Spectral", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_g_a, "FM Grain>A", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_g_b, "FM Grain>B", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_g_c, "FM Grain>C", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_g_s, "FM Grain>Spectral", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_n_a, "FM Noise>A", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_n_b, "FM Noise>B", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_n_c, "FM Noise>C", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_n_s, "FM Noise>Spectral", 0.0f, 1.0f, 0.0f, 0.0f) \
    /* ---- bite ---- */ \
    PF(bite_drive, "MultiDistortion Drive", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(bite_fold, "MultiDistortion Fold", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(bite_crush, "MultiDistortion Crush", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(bite_soft, "MultiDistortion Saturate", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(bite_asym, "MultiDistortion Asymmetry", 0.0f, 1.0f, 0.0f, 0.0f) \
    PB(bite_post, "MultiDistortion After Filter", false) \
    /* ---- routing ---- */ \
    PB(rt_flt_a, "Route WT A Through Filter", true) PB(rt_flt_b, "Route WT B Through Filter", true) \
    PB(rt_flt_c, "Route WT C Through Filter", true) PB(rt_flt_sp, "Route Spectral Through Filter", true) \
    PB(rt_flt_gr, "Route Grain Through Filter", true) PB(rt_flt_pl, "Route Plaits Through Filter", true) \
    PB(rt_flt_sub, "Route Sub+Noise Through Filter", true) PB(rt_flt_smp, "Route Sampler Through Filter", false) \
    PB(rt_dist_sp, "Route Spectral Through Distortion", true) \
    PC(rt_order, "FX Bus Order", routeOrders, 0) \
    PC(oversampling, "Oversampling", oversamplingModes, 0) \
    PF(bite_mix, "MultiDistortion Mix", 0.0f, 1.0f, 1.0f, 0.0f) \
    PF(bite_rect, "MultiDistortion Rectify", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(bite_tone, "MultiDistortion Tone", -1.0f, 1.0f, 0.0f, 0.0f) \
    /* ---- filter ---- */ \
    PC(flt_type, "Filter Type", filterTypes, 0) \
    PC(flt_model, "Filter Model", filterModels, 0) \
    PC(flt_slope, "Filter Slope", filterSlopes, 0) \
    PF(flt_cutoff, "Cutoff", 20.0f, 20000.0f, 8000.0f, 1000.0f) \
    PF(flt_res, "Resonance", 0.0f, 1.0f, 0.15f, 0.0f) \
    PF(flt_drive, "Filter Drive", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(flt_env, "Filter Env Amount", -1.0f, 1.0f, 0.0f, 0.0f) \
    PF(flt_key, "Filter Keytrack", 0.0f, 1.0f, 0.0f, 0.0f) \
    /* ---- modulation sources ---- */ \
    PG_LFO(PF, PI, PC, PB, 1) PG_LFO(PF, PI, PC, PB, 2) PG_LFO(PF, PI, PC, PB, 3) PG_LFO(PF, PI, PC, PB, 4) \
    PC(chaos1_type, "Chaos 1 Attractor", chaosTypes, 0) \
    PF(chaos1_rate, "Chaos 1 Rate", 0.05f, 20.0f, 1.0f, 2.0f) \
    PF(chaos1_smooth, "Chaos 1 Smooth", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(chaos1_amt, "Chaos 1 Chaos", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(chaos2_rate, "Chaos 2 Lorenz Rate", 0.05f, 20.0f, 1.0f, 2.0f) \
    PF(chaos2_smooth, "Chaos 2 Lorenz Smooth", 0.0f, 1.0f, 0.2f, 0.0f) \
    PF(chaos2_rho, "Chaos 2 Lorenz Rho", 0.0f, 1.0f, 0.4f, 0.0f) \
    PF(chaos2_sigma, "Chaos 2 Lorenz Sigma", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(chaos2_beta, "Chaos 2 Lorenz Beta", 0.0f, 1.0f, 0.5f, 0.0f) \
    PB(chaos2_sync, "Chaos 2 Lorenz Retrig", false) \
    PG_MTX(PF, PI, PC, PB, 01) PG_MTX(PF, PI, PC, PB, 02) PG_MTX(PF, PI, PC, PB, 03) PG_MTX(PF, PI, PC, PB, 04) PG_MTX(PF, PI, PC, PB, 05) PG_MTX(PF, PI, PC, PB, 06) PG_MTX(PF, PI, PC, PB, 07) PG_MTX(PF, PI, PC, PB, 08) \
    PG_MTX(PF, PI, PC, PB, 09) PG_MTX(PF, PI, PC, PB, 10) PG_MTX(PF, PI, PC, PB, 11) PG_MTX(PF, PI, PC, PB, 12) PG_MTX(PF, PI, PC, PB, 13) PG_MTX(PF, PI, PC, PB, 14) PG_MTX(PF, PI, PC, PB, 15) PG_MTX(PF, PI, PC, PB, 16) \
    /* ---- sequencer lanes (modulation) ---- */ \
    PG_SEQ(PF, PI, PC, PB, 1) PG_SEQ(PF, PI, PC, PB, 2) PG_SEQ(PF, PI, PC, PB, 3) PG_SEQ(PF, PI, PC, PB, 4) \
    PG_SEQ(PF, PI, PC, PB, 5) PG_SEQ(PF, PI, PC, PB, 6) PG_SEQ(PF, PI, PC, PB, 7) PG_SEQ(PF, PI, PC, PB, 8) \
    /* ---- arpeggiator ---- */ \
    PB(arp_on, "Arp On", false) \
    PC(arp_mode, "Arp Mode", arpModes, 0) \
    PI(arp_oct, "Arp Octaves", 1, 4, 1) \
    PC(arp_div, "Arp Division", divisions, 4) \
    PI(arp_len, "Arp Steps", 1, 32, 8) \
    PF(arp_gate, "Arp Gate", 0.05f, 1.0f, 0.6f, 0.0f) \
    PF(arp_swing, "Arp Swing", 0.0f, 0.75f, 0.0f, 0.0f) \
    PB(arp_latch, "Arp Latch", false) \
    PC(arp_scale, "Arp Scale", scales, 0) \
    PC(arp_root, "Arp Scale Root", noteNames, 0) \
    /* ---- FX: stereo, flanger, phaser, delay, Rings, Clouds ---- */ \
    PB(fx_st_on, "Stereo On", false) \
    PF(fx_st_width, "Stereo Width", 0.0f, 2.0f, 1.3f, 0.0f) \
    PF(fx_st_bass, "Stereo Mono Bass", 20.0f, 500.0f, 120.0f, 100.0f) \
    PF(fx_st_haas, "Stereo Haas", 0.0f, 20.0f, 0.0f, 0.0f) \
    PB(fx_fl_on, "Flanger On", false) \
    PF(fx_fl_rate, "Flanger Rate", 0.02f, 10.0f, 0.3f, 1.0f) \
    PF(fx_fl_depth, "Flanger Depth", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(fx_fl_fb, "Flanger Feedback", -0.95f, 0.95f, 0.5f, 0.0f) \
    PF(fx_fl_delay, "Flanger Delay", 0.5f, 15.0f, 3.0f, 0.0f) \
    PF(fx_fl_mix, "Flanger Mix", 0.0f, 1.0f, 0.5f, 0.0f) \
    PB(fx_ph_on, "Phaser On", false) \
    PF(fx_ph_rate, "Phaser Rate", 0.02f, 10.0f, 0.3f, 1.0f) \
    PF(fx_ph_depth, "Phaser Depth", 0.0f, 1.0f, 0.6f, 0.0f) \
    PF(fx_ph_fb, "Phaser Feedback", -0.95f, 0.95f, 0.4f, 0.0f) \
    PF(fx_ph_center, "Phaser Center", 100.0f, 8000.0f, 1000.0f, 1000.0f) \
    PI(fx_ph_stages, "Phaser Stages", 2, 12, 6) \
    PF(fx_ph_mix, "Phaser Mix", 0.0f, 1.0f, 0.5f, 0.0f) \
    PB(fx_dl_on, "Delay On", false) \
    PB(fx_dl_sync, "Delay Sync", true) \
    PC(fx_dl_div, "Delay Division", divisions, 8) \
    PF(fx_dl_time, "Delay Time", 1.0f, 2000.0f, 375.0f, 300.0f) \
    PB(fx_dl_pp, "Delay Ping-Pong", true) \
    PF(fx_dl_fb, "Delay Feedback", 0.0f, 0.98f, 0.4f, 0.0f) \
    PF(fx_dl_lowcut, "Delay Low Cut", 20.0f, 2000.0f, 150.0f, 200.0f) \
    PF(fx_dl_highcut, "Delay High Cut", 1000.0f, 20000.0f, 7000.0f, 5000.0f) \
    PF(fx_dl_mix, "Delay Mix", 0.0f, 1.0f, 0.25f, 0.0f) \
    PB(fx_rn_on, "Resonator On", false) \
    PC(fx_rn_model, "Resonator Model", ringsModels, 0) \
    PC(fx_rn_poly, "Resonator Polyphony", ringsPoly, 0) \
    PF(fx_rn_note, "Resonator Note", 24.0f, 96.0f, 48.0f, 0.0f) \
    PB(fx_rn_follow, "Resonator Follow Keys", true) \
    PF(fx_rn_structure, "Resonator Structure", 0.0f, 1.0f, 0.4f, 0.0f) \
    PF(fx_rn_bright, "Resonator Brightness", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(fx_rn_damping, "Resonator Damping", 0.0f, 1.0f, 0.6f, 0.0f) \
    PF(fx_rn_pos, "Resonator Position", 0.0f, 1.0f, 0.3f, 0.0f) \
    PF(fx_rn_mix, "Resonator Mix", 0.0f, 1.0f, 0.5f, 0.0f) \
    PB(fx_gr_on, "Clouds On", false) \
    PC(fx_gr_mode, "Clouds Mode", cloudsModes, 0) \
    PC(fx_gr_quality, "Clouds Quality", cloudsQuality, 0) \
    PB(fx_gr_freeze, "Clouds Freeze", false) \
    PF(fx_gr_pos, "Clouds Position", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(fx_gr_size, "Clouds Size", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(fx_gr_pitch, "Clouds Pitch", -24.0f, 24.0f, 0.0f, 0.0f) \
    PF(fx_gr_density, "Clouds Density", 0.0f, 1.0f, 0.8f, 0.0f) \
    PF(fx_gr_texture, "Clouds Texture", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(fx_gr_spread, "Clouds Spread", 0.0f, 1.0f, 0.5f, 0.0f) \
    PF(fx_gr_fb, "Clouds Feedback", 0.0f, 1.0f, 0.2f, 0.0f) \
    PF(fx_gr_reverb, "Clouds Reverb", 0.0f, 1.0f, 0.4f, 0.0f) \
    PF(fx_gr_mix, "Clouds Mix", 0.0f, 1.0f, 0.5f, 0.0f) \
    PG_FF(PF, PI, PC, PB, 1) \
    PG_FF(PF, PI, PC, PB, 2) \
    PB(fx_rv_on, "Reverb On", false) \
    PF(fx_rv_size, "Reverb Size", 0.0f, 1.0f, 0.6f, 0.0f) \
    PF(fx_rv_decay, "Reverb Decay", 0.2f, 20.0f, 2.5f, 3.0f) \
    PF(fx_rv_damp, "Reverb Damping", 0.0f, 1.0f, 0.4f, 0.0f) \
    PF(fx_rv_pre, "Reverb Pre-Delay", 0.0f, 200.0f, 10.0f, 0.0f) \
    PF(fx_rv_mod, "Reverb Modulation", 0.0f, 1.0f, 0.3f, 0.0f) \
    PF(fx_rv_width, "Reverb Width", 0.0f, 1.0f, 1.0f, 0.0f) \
    PF(fx_rv_mix, "Reverb Mix", 0.0f, 1.0f, 0.25f, 0.0f) \
    /* ---- MIXBUS: second cascade of mix effects ---- */ \
    PB(mb_tp_on, "MixBus Tape On", false) PF(mb_tp_drive, "MixBus Tape Drive", 0.0f, 1.0f, 0.3f, 0.0f) \
    PF(mb_tp_wow, "MixBus Tape Wow", 0.0f, 1.0f, 0.15f, 0.0f) PF(mb_tp_tone, "MixBus Tape Tone", -1.0f, 1.0f, 0.0f, 0.0f) \
    PF(mb_tp_mix, "MixBus Tape Mix", 0.0f, 1.0f, 1.0f, 0.0f) \
    PB(mb_eq_on, "MixBus EQ On", false) PF(mb_eq_low, "MixBus EQ Low", -12.0f, 12.0f, 0.0f, 0.0f) \
    PF(mb_eq_mid, "MixBus EQ Mid", -12.0f, 12.0f, 0.0f, 0.0f) PF(mb_eq_mfreq, "MixBus EQ Mid Freq", 200.0f, 6000.0f, 1000.0f, 1000.0f) \
    PF(mb_eq_high, "MixBus EQ High", -12.0f, 12.0f, 0.0f, 0.0f) PF(mb_eq_lcut, "MixBus EQ Low Cut", 20.0f, 600.0f, 20.0f, 100.0f) \
    PF(mb_eq_hcut, "MixBus EQ High Cut", 2000.0f, 20000.0f, 20000.0f, 8000.0f) \
    PB(mb_cp_on, "MixBus Comp On", false) PF(mb_cp_thr, "MixBus Comp Threshold", -40.0f, 0.0f, -12.0f, 0.0f) \
    PF(mb_cp_ratio, "MixBus Comp Ratio", 1.0f, 20.0f, 3.0f, 4.0f) PF(mb_cp_att, "MixBus Comp Attack", 0.1f, 100.0f, 10.0f, 10.0f) \
    PF(mb_cp_rel, "MixBus Comp Release", 10.0f, 1000.0f, 150.0f, 150.0f) PF(mb_cp_gain, "MixBus Comp Makeup", 0.0f, 24.0f, 0.0f, 0.0f) \
    PF(mb_cp_mix, "MixBus Comp Mix", 0.0f, 1.0f, 1.0f, 0.0f) \
    PB(mb_ch_on, "MixBus Chorus On", false) PF(mb_ch_rate, "MixBus Chorus Rate", 0.05f, 5.0f, 0.6f, 0.8f) \
    PF(mb_ch_depth, "MixBus Chorus Depth", 0.0f, 1.0f, 0.5f, 0.0f) PF(mb_ch_mix, "MixBus Chorus Mix", 0.0f, 1.0f, 0.4f, 0.0f) \
    PB(mb_im_on, "MixBus Imager On", false) PF(mb_im_width, "MixBus Imager Width", 0.0f, 2.0f, 1.2f, 0.0f) \
    PF(mb_im_bass, "MixBus Imager Mono Bass", 20.0f, 400.0f, 120.0f, 100.0f) \
    PB(mb_tg_on, "MixBus Gate On", false) PC(mb_tg_pat, "MixBus Gate Pattern", gatePatterns, 0) \
    PC(mb_tg_div, "MixBus Gate Division", divisions, 4) PF(mb_tg_depth, "MixBus Gate Depth", 0.0f, 1.0f, 1.0f, 0.0f) \
    PF(mb_tg_smooth, "MixBus Gate Smooth", 0.0f, 1.0f, 0.2f, 0.0f) \
    PB(mb_dj_on, "MixBus DJ Filter On", false) PF(mb_dj_pos, "MixBus DJ Filter", -1.0f, 1.0f, 0.0f, 0.0f) \
    PF(mb_dj_res, "MixBus DJ Filter Reso", 0.0f, 1.0f, 0.3f, 0.0f) \
    PB(mb_bc_on, "MixBus Crusher On", false) PF(mb_bc_bits, "MixBus Crusher Bits", 2.0f, 16.0f, 10.0f, 0.0f) \
    PF(mb_bc_rate, "MixBus Crusher Downsample", 1.0f, 32.0f, 1.0f, 4.0f) PF(mb_bc_mix, "MixBus Crusher Mix", 0.0f, 1.0f, 1.0f, 0.0f) \
    PB(mb_tr_on, "MixBus Transient On", false) PF(mb_tr_att, "MixBus Transient Attack", -1.0f, 1.0f, 0.3f, 0.0f) \
    PF(mb_tr_sus, "MixBus Transient Sustain", -1.0f, 1.0f, 0.0f, 0.0f) \
    PB(mb_lm_on, "MixBus Limiter On", false) PF(mb_lm_in, "MixBus Limiter Input", 0.0f, 18.0f, 0.0f, 0.0f) \
    PF(mb_lm_ceil, "MixBus Limiter Ceiling", -12.0f, 0.0f, -1.0f, 0.0f) PF(mb_lm_rel, "MixBus Limiter Release", 10.0f, 500.0f, 80.0f, 80.0f) \
    PC(mb_slot1, "MixBus Slot 1", mixbusTypes, 1) PC(mb_slot2, "MixBus Slot 2", mixbusTypes, 2) \
    PC(mb_slot3, "MixBus Slot 3", mixbusTypes, 3) PC(mb_slot4, "MixBus Slot 4", mixbusTypes, 4) \
    PC(mb_slot5, "MixBus Slot 5", mixbusTypes, 5) PC(mb_slot6, "MixBus Slot 6", mixbusTypes, 6) \
    PC(mb_slot7, "MixBus Slot 7", mixbusTypes, 7) PC(mb_slot8, "MixBus Slot 8", mixbusTypes, 8) \
    PC(mb_slot9, "MixBus Slot 9", mixbusTypes, 9) PC(mb_slot10, "MixBus Slot 10", mixbusTypes, 10) \
    /* ---- FX rack: the order of the cascade ---- */ \
    PC(fx_slot1, "FX Slot 1", fxTypes, 1) PC(fx_slot2, "FX Slot 2", fxTypes, 2) \
    PC(fx_slot3, "FX Slot 3", fxTypes, 3) PC(fx_slot4, "FX Slot 4", fxTypes, 4) \
    PC(fx_slot5, "FX Slot 5", fxTypes, 5) PC(fx_slot6, "FX Slot 6", fxTypes, 6) \
    PC(fx_slot7, "FX Slot 7", fxTypes, 7) PC(fx_slot8, "FX Slot 8", fxTypes, 8) \
    PC(fx_slot9, "FX Slot 9", fxTypes, 9) PC(fx_slot10, "FX Slot 10", fxTypes, 0)

#define PG_ENUM_F(id, ...) id,
#define PG_ENUM_I(id, ...) id,
#define PG_ENUM_C(id, ...) id,
#define PG_ENUM_B(id, ...) id,
enum PID : int { PG_PARAM_LIST (PG_ENUM_F, PG_ENUM_I, PG_ENUM_C, PG_ENUM_B) kNumParams };
#undef PG_ENUM_F
#undef PG_ENUM_I
#undef PG_ENUM_C
#undef PG_ENUM_B

enum class PType { Float, Int, Choice, Bool };

struct ParamInfo
{
    juce::String id, name;
    PType type;
    float min, max, def;
};

const std::vector<ParamInfo>& paramInfos();
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
// adds the parameters whose id passes 'keep' (used by other Calabi Yau instruments to host this FX rack)
void addParamsTo (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const std::function<bool (const juce::String&)>& keep);
inline const juce::String& idOf (int pid) { return paramInfos()[(size_t) pid].id; }
int indexOf (const juce::String& id);   // -1 if unknown

// Modulation sources / destinations (indices into choices::modSources / modDests)
enum ModSrc { MS_None, MS_Lfo1, MS_Lfo2, MS_Lfo3, MS_Lfo4, MS_Chaos1, MS_Chaos2, MS_Chaos2Y, MS_Chaos2Z,
              MS_Seq1, MS_Seq2, MS_Seq3, MS_Seq4, MS_Seq5, MS_Seq6, MS_Seq7, MS_Seq8,
              MS_ModEnv, MS_FiltEnv, MS_AmpEnv, MS_Velocity, MS_Note, MS_ModWheel,
              MS_Aftertouch, MS_PitchBend, MS_Random, MS_ModEnv2, MS_ModEnv3, MS_ModEnv4, MS_Count };

enum ModDst { MD_None,
              MD_WtAPos, MD_WtAPitch, MD_WtALevel, MD_WtAWarp,
              MD_WtBPos, MD_WtBPitch, MD_WtBLevel, MD_WtBWarp,
              MD_WtCPos, MD_WtCPitch, MD_WtCLevel, MD_WtCWarp,
              MD_SpPos, MD_SpPitch, MD_SpTilt, MD_SpStretch, MD_SpShift, MD_SpLevel, MD_SpWarp,
              MD_GrPos, MD_GrSize, MD_GrDensity, MD_GrPitch, MD_GrSpray, MD_GrLevel,
              MD_FmAmount, MD_SubLevel, MD_NoiseLevel, MD_SmpPitch, MD_SmpLevel,
              MD_PlHarm, MD_PlTimbre, MD_PlMorph, MD_PlLevel,
              MD_BiteDrive, MD_BiteFold, MD_BiteCrush,
              MD_Cutoff, MD_Resonance, MD_Pan, MD_Volume,
              MD_VoiceCount,                      // ---- below: global (FX) destinations ----
              MD_DelayMix = MD_VoiceCount, MD_DelayFb, MD_DelayTime, MD_RnStructure, MD_RnBright, MD_RnPos,
              MD_GrfxPos, MD_GrfxSize, MD_GrfxDensity, MD_GrfxTexture, MD_GrfxMix,
              MD_FlMix, MD_PhMix, MD_Ff1Freq, MD_Ff1Res, MD_Ff1Morph, MD_Ff2Freq, MD_Ff2Res, MD_Ff2Morph,
              MD_RvSize, MD_RvMix, MD_MbDjFilter, MD_MbGateDepth, MD_MbTapeDrive, MD_MbCrushBits, MD_MbWidth, MD_Count };

// FX rack slot types (choices::fxTypes)
enum MbType { MB_Empty, MB_Tape, MB_EQ, MB_Comp, MB_Chorus, MB_Imager, MB_Gate, MB_DJ, MB_Crush, MB_Transient, MB_Limiter, MB_Count };

enum FxType { FX_Empty, FX_Filter1, FX_Filter2, FX_Stereo, FX_Flanger, FX_Phaser, FX_Delay, FX_Rings, FX_Clouds, FX_Reverb, FX_Count };

} // namespace pg
