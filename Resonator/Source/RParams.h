#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// =============================================================================
// Calabi-Yau Resonator parameters. Declared once in CYR_PARAM_LIST:
//   F(id, name, min, max, default, skewCentre)   I(id, name, min, max, default)
//   C(id, name, choiceList, default)              B(id, name, default)
// The FX rack and MIXBUS parameters are hosted from Synthgularity (pg::addParamsTo).
// =============================================================================
namespace cyr
{
namespace choices
{
    juce::StringArray filterTypes();
    juce::StringArray lfoShapes();
    juce::StringArray divisions();
    juce::StringArray quality();
    juce::StringArray modSources();
    juce::StringArray modDests();     // display names of every modulatable parameter
    double divisionInBeats (int index);
}

#define CYR_OSC(PF, PI, PC, PB, o, O) \
    PB(osc##o##_on, "Osc " #O " On", false) \
    PI(osc##o##_n, "Osc " #O " Dimension n", 2, 7, 5) \
    PI(osc##o##_k1, "Osc " #O " Patch k1", 0, 6, 0) \
    PI(osc##o##_k2, "Osc " #O " Patch k2", 0, 6, 0) \
    PF(osc##o##_alpha, "Osc " #O " Angle Alpha", 0.0f, 6.2831853f, 0.8f, 0.0f) \
    PF(osc##o##_ru, "Osc " #O " Radius u", 0.0f, 0.98f, 0.6f, 0.0f) \
    PF(osc##o##_rv, "Osc " #O " Radius v", 0.0f, 1.0f, 0.4f, 0.0f) \
    PI(osc##o##_ratio, "Osc " #O " Ratio v:u", 1, 7, 1) \
    PF(osc##o##_spin, "Osc " #O " Phase Spin", -8.0f, 8.0f, 0.0f, 0.0f) \
    PF(osc##o##_width, "Osc " #O " Stereo Width", 0.0f, 1.0f, 0.6f, 0.0f) \
    PI(osc##o##_uni, "Osc " #O " Unison", 1, 4, 1) \
    PF(osc##o##_det, "Osc " #O " Detune", 0.0f, 50.0f, 8.0f, 0.0f) \
    PI(osc##o##_oct, "Osc " #O " Octave", -4, 4, 0) \
    PI(osc##o##_semi, "Osc " #O " Semitone", -12, 12, 0) \
    PF(osc##o##_fine, "Osc " #O " Fine", -100.0f, 100.0f, 0.0f, 0.0f) \
    PF(osc##o##_level, "Osc " #O " Level", 0.0f, 1.0f, 0.7f, 0.0f) \
    PF(osc##o##_pan, "Osc " #O " Pan", -1.0f, 1.0f, 0.0f, 0.0f) \
    PB(osc##o##_rphase, "Osc " #O " Random Phase", true)

#define CYR_FMROW(PF, s, S) \
    PF(fm_##s##a, "FM " #S " > A", 0.0f, 1.0f, 0.0f, 0.0f) PF(fm_##s##b, "FM " #S " > B", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(fm_##s##c, "FM " #S " > C", 0.0f, 1.0f, 0.0f, 0.0f) PF(fm_##s##d, "FM " #S " > D", 0.0f, 1.0f, 0.0f, 0.0f)
#define CYR_PDROW(PF, s, S) \
    PF(pd_##s##a, "PD " #S " > A", 0.0f, 1.0f, 0.0f, 0.0f) PF(pd_##s##b, "PD " #S " > B", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(pd_##s##c, "PD " #S " > C", 0.0f, 1.0f, 0.0f, 0.0f) PF(pd_##s##d, "PD " #S " > D", 0.0f, 1.0f, 0.0f, 0.0f)

#define CYR_ENV(PF, n) \
    PF(env##n##_a, "Env " #n " Attack", 0.001f, 10.0f, 0.01f, 0.5f) \
    PF(env##n##_d, "Env " #n " Decay", 0.001f, 10.0f, 0.6f, 0.5f) \
    PF(env##n##_s, "Env " #n " Sustain", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(env##n##_r, "Env " #n " Release", 0.001f, 15.0f, 0.5f, 0.8f)

#define CYR_LFO(PF, PI, PC, PB, n) \
    PC(lfo##n##_shape, "LFO " #n " Shape", lfoShapes, 0) \
    PF(lfo##n##_rate, "LFO " #n " Rate", 0.02f, 30.0f, 1.0f, 2.0f) \
    PB(lfo##n##_sync, "LFO " #n " Sync", false) \
    PC(lfo##n##_div, "LFO " #n " Division", divisions, 10) \
    PF(lfo##n##_phase, "LFO " #n " Phase", 0.0f, 1.0f, 0.0f, 0.0f) \
    PB(lfo##n##_trig, "LFO " #n " Note Retrigger", false) \
    PF(lfo##n##_fade, "LFO " #n " Fade In", 0.0f, 5.0f, 0.0f, 1.0f)

#define CYR_MOD(PF, PI, PC, PB, n) \
    PC(mod##n##_src, "Mod " #n " Source", modSources, 0) \
    PC(mod##n##_dst, "Mod " #n " Destination", modDests, 0) \
    PF(mod##n##_amt, "Mod " #n " Amount", -1.0f, 1.0f, 0.0f, 0.0f)

#define CYR_MOD8(PF, PI, PC, PB, a) \
    CYR_MOD(PF, PI, PC, PB, a##1) CYR_MOD(PF, PI, PC, PB, a##2) CYR_MOD(PF, PI, PC, PB, a##3) CYR_MOD(PF, PI, PC, PB, a##4) \
    CYR_MOD(PF, PI, PC, PB, a##5) CYR_MOD(PF, PI, PC, PB, a##6) CYR_MOD(PF, PI, PC, PB, a##7) CYR_MOD(PF, PI, PC, PB, a##8)

#define CYR_PARAM_LIST(PF, PI, PC, PB) \
    PF(master, "Master", -36.0f, 6.0f, -6.0f, 0.0f) \
    PI(poly, "Polyphony", 1, 16, 8) \
    PF(glide, "Glide", 0.0f, 2.0f, 0.0f, 0.3f) \
    PI(bend, "Bend Range", 0, 24, 2) \
    PF(velsens, "Velocity Sensitivity", 0.0f, 1.0f, 0.6f, 0.0f) \
    PC(quality, "Oversampling", quality, 0) \
    PF(amp_a, "Amp Attack", 0.001f, 10.0f, 0.005f, 0.5f) \
    PF(amp_d, "Amp Decay", 0.001f, 10.0f, 0.4f, 0.5f) \
    PF(amp_s, "Amp Sustain", 0.0f, 1.0f, 0.85f, 0.0f) \
    PF(amp_r, "Amp Release", 0.001f, 15.0f, 0.5f, 0.8f) \
    PB(flt_on, "Filter On", true) \
    PC(flt_type, "Filter Type", filterTypes, 0) \
    PF(flt_cut, "Filter Cutoff", 20.0f, 20000.0f, 9000.0f, 1000.0f) \
    PF(flt_res, "Filter Resonance", 0.0f, 1.0f, 0.15f, 0.0f) \
    PF(flt_drive, "Filter Drive", 0.0f, 1.0f, 0.0f, 0.0f) \
    PF(flt_key, "Filter Key Track", 0.0f, 1.0f, 0.0f, 0.0f) \
    CYR_OSC(PF, PI, PC, PB, a, A) \
    CYR_OSC(PF, PI, PC, PB, b, B) \
    CYR_OSC(PF, PI, PC, PB, c, C) \
    CYR_OSC(PF, PI, PC, PB, d, D) \
    CYR_FMROW(PF, a, A) CYR_FMROW(PF, b, B) CYR_FMROW(PF, c, C) CYR_FMROW(PF, d, D) \
    CYR_PDROW(PF, a, A) CYR_PDROW(PF, b, B) CYR_PDROW(PF, c, C) CYR_PDROW(PF, d, D) \
    CYR_ENV(PF, 1) CYR_ENV(PF, 2) CYR_ENV(PF, 3) CYR_ENV(PF, 4) \
    CYR_LFO(PF, PI, PC, PB, 1) CYR_LFO(PF, PI, PC, PB, 2) CYR_LFO(PF, PI, PC, PB, 3) \
    CYR_LFO(PF, PI, PC, PB, 4) CYR_LFO(PF, PI, PC, PB, 5) CYR_LFO(PF, PI, PC, PB, 6) \
    CYR_MOD8(PF, PI, PC, PB, 0) CYR_MOD8(PF, PI, PC, PB, 1) CYR_MOD8(PF, PI, PC, PB, 2) CYR_MOD8(PF, PI, PC, PB, 3)

#define CYR_ENUM(id, ...) id,
enum RID : int { CYR_PARAM_LIST (CYR_ENUM, CYR_ENUM, CYR_ENUM, CYR_ENUM) kNumR };
#undef CYR_ENUM

constexpr int kOscs = 4, kEnvs = 4, kLfos = 6, kSlots = 32;
constexpr int kOscStride = oscb_on - osca_on;
constexpr int kLfoStride = lfo2_shape - lfo1_shape;
constexpr int kEnvStride = env2_a - env1_a;
constexpr int kModStride = mod02_src - mod01_src;
constexpr int kFirstMod = mod01_src;

enum ModSrc { MS_None, MS_Env1, MS_Env2, MS_Env3, MS_Env4, MS_Lfo1, MS_Lfo2, MS_Lfo3, MS_Lfo4, MS_Lfo5, MS_Lfo6,
              MS_AmpEnv, MS_Velocity, MS_Key, MS_ModWheel, MS_Aftertouch, MS_PitchBend, MS_Random, MS_Count };
inline bool isBipolarSource (int s) { return (s >= MS_Lfo1 && s <= MS_Lfo6) || s == MS_Key || s == MS_PitchBend || s == MS_Random; }

struct RInfo { juce::String id, name; float min, max, def, skew; bool isInt, isChoice, isBool; };
const std::vector<RInfo>& infos();
inline const juce::String& idOf (int i) { return infos()[(size_t) i].id; }
int indexOfR (const juce::String& id);
const std::vector<juce::NormalisableRange<float>>& ranges();   // per parameter, with skew

// Modulation destinations: index 0 = none, then these parameter indices
const std::vector<int>& modDestParams();
int destIndexOfParam (int rid);          // -1 when the parameter cannot be modulated
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
bool isHostedFxParam (const juce::String& id);   // pg FX rack / MIXBUS ids hosted by the Resonator

// a slot's row in the parameter list
inline int slotSrc (int s) { return kFirstMod + s * kModStride; }
inline int slotDst (int s) { return kFirstMod + s * kModStride + 1; }
inline int slotAmt (int s) { return kFirstMod + s * kModStride + 2; }
} // namespace cyr
