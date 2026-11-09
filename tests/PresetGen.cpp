// Builds the Synthgularity Factory Library: ~300 original presets from recipes, each one
// rendered, loudness-matched (EBU R128 integrated) and checked before it is saved.
// Usage: SynthgularityPresetGen <outDir> [count]
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <set>

using namespace pg;

namespace
{
// ---- enums of the choice lists (indices into Params.cpp) ----
enum Tbl { T_Basic, T_PWM, T_Harm, T_Formant, T_Sync, T_FM, T_Fold, T_Digital };
enum Wrp { W_Off, W_BendP, W_BendM, W_BendPM, W_BloatP, W_BloatM, W_Sync, W_SyncWin, W_Pulse, W_Fm, W_FmWin, W_Flip, W_Mirror, W_Breed, W_Brew, W_Sinc, W_Crush, W_Smooth, W_Window };
enum Div { D64, D32T, D32, D16T, D16, D16D, D8T, D8, D8D, D4T, D4, D4D, D2, D1, D2B, D4B };
enum Lfo { L_Sine, L_Tri, L_SawUp, L_SawDn, L_Square, L_SH, L_Smooth };
enum Arp { A_Up, A_Down, A_UpDown, A_DownUp, A_Played, A_Random, A_Chord };
enum FfT { FF_Bank, FF_CombP, FF_CombN, FF_MorphBP, FF_MorphNotch, FF_Formant, FF_Allpass };

struct Gen
{
    SynthgularityProcessor& pr;
    juce::Random rnd;
    int slot = 1;

    void s (const juce::String& id, float v)
    {
        auto* p = pr.apvts.getParameter (id);
        if (p == nullptr) { std::cerr << "unknown parameter " << id << "\n"; std::exit (2); }
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    }
    float u (float a, float b) { return a + (b - a) * rnd.nextFloat(); }
    int i (int a, int b) { return a + rnd.nextInt (b - a + 1); }
    bool chance (float p) { return rnd.nextFloat() < p; }
    template <typename T> T pick (std::initializer_list<T> l) { auto it = l.begin(); std::advance (it, rnd.nextInt ((int) l.size())); return *it; }

    void route (int src, int dst, float amt, int pol = 0, int via = 0)
    {
        if (slot > 16) return;
        const auto n = juce::String (slot++).paddedLeft ('0', 2);
        s ("mtx" + n + "_src", (float) src); s ("mtx" + n + "_dst", (float) dst);
        s ("mtx" + n + "_amt", amt); s ("mtx" + n + "_pol", (float) pol); s ("mtx" + n + "_via", (float) via);
    }
    // ---- building blocks ----
    void wt (char o, int table, float pos, int oct, float level = 0.7f, int uni = 1, float det = 0.2f)
    {
        const juce::String p = juce::String ("wt") + o + "_";
        s (p + "on", 1); s (p + "table", (float) table); s (p + "pos", pos); s (p + "oct", (float) oct); s (p + "level", level);
        s (p + "unison", (float) uni); s (p + "detune", det); s (p + "spread", uni > 1 ? u (0.5f, 1.0f) : 0.0f);
    }
    void warp (char o, int m1, float a1, int m2 = 0, float a2 = 0.0f)
    {
        const juce::String p = juce::String ("wt") + o + "_";
        s (p + "warp", (float) m1); s (p + "wamt", a1); s (p + "warp2", (float) m2); s (p + "wamt2", a2);
    }
    void semi (char o, int st, float fine = 0.0f) { const juce::String p = juce::String ("wt") + o + "_"; s (p + "semi", (float) st); s (p + "fine", fine); }
    void amp (float a, float d, float su, float r) { s ("amp_a", a); s ("amp_d", d); s ("amp_s", su); s ("amp_r", r); }
    void fenv (float a, float d, float su, float r) { s ("fenv_a", a); s ("fenv_d", d); s ("fenv_s", su); s ("fenv_r", r); }
    void menv (int n, float a, float d, float su, float r)
    {
        const juce::String p = n == 1 ? "menv_" : "menv" + juce::String (n) + "_";
        s (p + "a", a); s (p + "d", d); s (p + "s", su); s (p + "r", r);
    }
    void filter (int type, int model, int slope, float cut, float res, float env = 0.0f, float key = 0.0f, float drive = 0.0f)
    {
        s ("flt_type", (float) type); s ("flt_model", (float) model); s ("flt_slope", (float) slope); s ("flt_cutoff", cut);
        s ("flt_res", res); s ("flt_env", env); s ("flt_key", key); s ("flt_drive", drive);
    }
    void lfo (int n, int shape, float rate, bool retrig = false, int syncDiv = -1, float phase = 0.0f)
    {
        const juce::String p = "lfo" + juce::String (n) + "_";
        s (p + "shape", (float) shape); s (p + "rate", rate); s (p + "retrig", retrig ? 1.0f : 0.0f);
        s (p + "sync", syncDiv >= 0 ? 1.0f : 0.0f); if (syncDiv >= 0) s (p + "div", (float) syncDiv); s (p + "phase", phase);
    }
    void reverb (float mix, float decay, float size = 0.6f, float damp = 0.4f) { s ("fx_rv_on", 1); s ("fx_rv_mix", mix); s ("fx_rv_decay", decay); s ("fx_rv_size", size); s ("fx_rv_damp", damp); s ("fx_rv_pre", u (5, 40)); s ("fx_rv_mod", u (0.2f, 0.6f)); }
    void delay (int div, float fb, float mix, bool pp = true) { s ("fx_dl_on", 1); s ("fx_dl_sync", 1); s ("fx_dl_div", (float) div); s ("fx_dl_fb", fb); s ("fx_dl_mix", mix); s ("fx_dl_pp", pp ? 1.0f : 0.0f); s ("fx_dl_lowcut", u (120, 400)); s ("fx_dl_highcut", u (3500, 9000)); }
    void ff (int n, int type, float freq, float res, float morph, int poles, float mix, float drive = 0.0f)
    {
        const juce::String p = "ff" + juce::String (n) + "_";
        s (p + "on", 1); s (p + "type", (float) type); s (p + "freq", freq); s (p + "res", res); s (p + "morph", morph); s (p + "poles", (float) poles); s (p + "mix", mix); s (p + "drive", drive); s (p + "spread", u (0.0f, 0.5f));
    }
    void stereo (float width) { s ("fx_st_on", 1); s ("fx_st_width", width); s ("fx_st_bass", u (100, 200)); }
    void phaser (float mix) { s ("fx_ph_on", 1); s ("fx_ph_mix", mix); s ("fx_ph_rate", u (0.05f, 0.6f)); s ("fx_ph_depth", u (0.4f, 0.9f)); s ("fx_ph_fb", u (0.2f, 0.7f)); s ("fx_ph_stages", (float) i (4, 10)); }
    void flanger (float mix) { s ("fx_fl_on", 1); s ("fx_fl_mix", mix); s ("fx_fl_rate", u (0.05f, 0.4f)); s ("fx_fl_depth", u (0.3f, 0.8f)); s ("fx_fl_fb", u (0.2f, 0.8f)); }
    void clouds (float mix, int mode = 0) { s ("fx_gr_on", 1); s ("fx_gr_mix", mix); s ("fx_gr_mode", (float) mode); s ("fx_gr_density", u (0.6f, 0.9f)); s ("fx_gr_texture", u (0.3f, 0.8f)); s ("fx_gr_size", u (0.3f, 0.8f)); s ("fx_gr_reverb", u (0.3f, 0.7f)); }
    void rings (float mix, int model = 0) { s ("fx_rn_on", 1); s ("fx_rn_mix", mix); s ("fx_rn_model", (float) model); s ("fx_rn_structure", u (0.2f, 0.7f)); s ("fx_rn_bright", u (0.3f, 0.7f)); s ("fx_rn_damping", u (0.4f, 0.8f)); }
    void spectral (int snd, float pos, float scan, float blur = 0.2f)
    {
        s ("sp_on", 1); s ("sp_sound", (float) snd); s ("sp_pos", pos); s ("sp_scan", scan); s ("sp_blur", blur); s ("sp_width", u (0.3f, 0.9f));
    }
    void plaits (int engine, float harm, float timbre, float morph, float level = 0.7f)
    {
        s ("pl_on", 1); s ("pl_engine", (float) engine); s ("pl_harm", harm); s ("pl_timbre", timbre); s ("pl_morph", morph); s ("pl_level", level);
    }
    void grain (float pos, float size, float density, float spray, int mode = 0)
    {
        s ("gr_on", 1); s ("gr_pos", pos); s ("gr_size", size); s ("gr_density", density); s ("gr_spray", spray); s ("gr_mode", (float) mode);
        s ("gr_stereo", u (0.4f, 1.0f)); s ("gr_window", (float) i (0, 3));
    }
    void arp (int mode, int div, int oct, float gate, int scale = 0, int root = 0, bool latch = false)
    {
        s ("arp_on", 1); s ("arp_mode", (float) mode); s ("arp_div", (float) div); s ("arp_oct", (float) oct); s ("arp_gate", gate);
        s ("arp_scale", (float) scale); s ("arp_root", (float) root); s ("arp_latch", latch ? 1.0f : 0.0f);
    }
    void arpSteps (int len, std::function<void (int step, bool& on, int& tr, float& vel, int& ratchet, bool& tie)> fn)
    {
        s ("arp_len", (float) len);
        auto& st = pr.synth.steps;
        for (int k = 0; k < 32; ++k)
        {
            bool on = true, tie = false; int tr = 0, rat = 1; float vel = 0.8f;
            fn (k, on, tr, vel, rat, tie);
            st.arpOn[(size_t) k].store (on); st.arpTranspose[(size_t) k].store (tr); st.arpVel[(size_t) k].store (vel);
            st.arpRatchet[(size_t) k].store (rat); st.arpTie[(size_t) k].store (tie);
        }
    }
    void seqLane (int lane, int len, int div, int dir, bool bipolar, std::function<float (int)> fn, float glide = 0.0f)
    {
        const juce::String p = "seq" + juce::String (lane) + "_";
        s (p + "on", 1); s (p + "len", (float) len); s (p + "div", (float) div); s (p + "dir", (float) dir); s (p + "mode", bipolar ? 1.0f : 0.0f); s (p + "glide", glide);
        for (int k = 0; k < 32; ++k) pr.synth.steps.seq[(size_t) ((lane - 1) * 32 + k)].store (juce::jlimit (0.0f, 1.0f, fn (k)));
    }
    void voice (int mode, int poly = 8, float glide = 0.0f) { s ("voice_mode", (float) mode); s ("polyphony", (float) poly); s ("glide", glide); }
    void bite (float drive, float soft = 0.0f, float fold = 0.0f, float crush = 0.0f, float mix = 1.0f)
    { s ("bite_drive", drive); s ("bite_soft", soft); s ("bite_fold", fold); s ("bite_crush", crush); s ("bite_mix", mix); }
    // mixbus finishing touches (tape warmth, glue, width), plus per-step arp probability / gate variation
    void finish (const juce::String& prefix)
    {
        if (chance (0.45f)) { s ("mb_tp_on", 1); s ("mb_tp_drive", u (0.1f, 0.45f)); s ("mb_tp_wow", u (0.0f, prefix == "PD" || prefix == "DR" ? 0.4f : 0.1f)); s ("mb_tp_tone", u (-0.3f, 0.2f)); }
        if (prefix == "BA" || prefix == "BL") { if (chance (0.6f)) { s ("mb_cp_on", 1); s ("mb_cp_thr", u (-18, -8)); s ("mb_cp_ratio", u (2, 5)); s ("mb_cp_att", u (5, 30)); s ("mb_cp_gain", u (1, 4)); } }
        else if (chance (0.35f)) { s ("mb_im_on", 1); s ("mb_im_width", u (1.1f, 1.5f)); }
        if ((prefix == "PD" || prefix == "KY") && chance (0.4f)) { s ("mb_ch_on", 1); s ("mb_ch_mix", u (0.2f, 0.45f)); s ("mb_ch_rate", u (0.2f, 1.0f)); }
        if (prefix == "GN" || prefix == "AR")
        {
            auto& st = pr.synth.steps;
            for (int k = 0; k < 32; ++k)
            {
                st.arpProb[(size_t) k].store (chance (prefix == "GN" ? 0.5f : 0.2f) ? u (0.35f, 0.9f) : 1.0f);
                st.arpGate[(size_t) k].store (chance (0.3f) ? u (0.3f, 1.0f) : 1.0f);
            }
        }
        if (prefix == "FX" && chance (0.4f)) { s ("mb_tg_on", 1); s ("mb_tg_pat", (float) i (0, 7)); s ("mb_tg_depth", u (0.5f, 1.0f)); }
    }
    void rack (std::initializer_list<int> order)   // FX slot order (FxType values)
    {
        int k = 1;
        for (int t : order) s ("fx_slot" + juce::String (k++), (float) t);
        for (; k <= 10; ++k) s ("fx_slot" + juce::String (k), 0.0f);
    }
};

// ---- names ----
const char* kWordsA[] = { "Astral", "Fractal", "Mycelium", "Quantum", "Lucid", "Shaman", "Kaleido", "Nebula", "Prism", "Mandala", "Serpent", "Third Eye",
    "Hyperspace", "Tesseract", "Calabi", "Orbit", "Lotus", "Chakra", "Plasma", "Neon", "Liquid", "Vortex", "Spiral", "Cosmic", "Hologram", "Wormhole",
    "Pineal", "Ether", "Sacred", "Solar", "Lunar", "Zenith", "Aurora", "Mirage", "Oracle", "Temple", "Jungle", "Desert", "Ocean", "Crystal", "Quasar",
    "Pulsar", "Event Horizon", "Singular", "Infinite", "Hidden", "Electric", "Psionic", "Telepathic", "Glitch", "Alien", "Galactic", "Ancient", "Mystic",
    "Dream", "Shadow", "Golden", "Violet", "Emerald", "Ultra", "Hyper", "Inner", "Outer", "Deep", "Twin", "Parallel", "Dimensional", "Ritual", "Totem",
    "Sigil", "Rune", "Lysergic", "Tryptamine", "Spore", "Fungal", "Bio", "Cyber", "Retro", "Morphic", "Lotus", "Andromeda", "Orion", "Sirius", "Pleiades" };
const char* kWordsB[] = { "Roller", "Drift", "Bloom", "Pulse", "Ritual", "Engine", "Signal", "Garden", "Tunnel", "Gate", "Bridge", "Machine", "Voyage",
    "Ascent", "Descent", "Dream", "Rain", "Storm", "Wave", "Spiral", "Bloom", "Mantra", "Chant", "Orbit", "Rift", "Echo", "Vision", "Glow", "Grid",
    "Circuit", "Flux", "Rush", "Trip", "Path", "Journey", "Portal", "Prayer", "Swarm", "Nest", "Weaver", "Code", "Language", "Choir", "Bell", "Lights",
    "Engine", "Reactor", "Pilot", "Rover", "Spirit", "Ghost", "Nomad", "Hunter", "Dancer", "Oracle", "Waterfall", "Glacier", "Comet", "Meteor", "Seed",
    "Root", "Bloom", "Shift", "Fold", "Knot", "Loop", "Lattice", "Matrix", "Particle", "Field", "Cascade", "Fountain", "Horizon", "Mirror" };

juce::String makeName (juce::Random& r, std::set<juce::String>& used, const juce::String& prefix)
{
    for (int tries = 0; tries < 1000; ++tries)
    {
        juce::String n = juce::String (kWordsA[r.nextInt ((int) std::size (kWordsA))]) + " " + kWordsB[r.nextInt ((int) std::size (kWordsB))];
        if (used.insert (n).second) return prefix + " " + n;
    }
    return prefix + " " + juce::String (r.nextInt (100000));
}

// ---- recipes ----
using Recipe = std::function<void (Gen&)>;

void psyBass (Gen& g)
{
    // rolling psytrance bass: the arp plays the off-beat 16ths of the held note
    g.voice (1);
    const int tbl = g.pick ({ T_Basic, T_PWM, T_Sync, T_Digital, T_Fold });
    g.wt ('A', tbl, g.u (0.0f, 0.8f), -1, 1.0f);
    if (g.chance (0.6f)) g.warp ('A', g.pick ({ W_BendP, W_Sync, W_Pulse, W_Mirror, W_BloatP }), g.u (0.1f, 0.5f), g.chance (0.4f) ? g.pick ({ W_Crush, W_Fm, W_Flip }) : 0, g.u (0.05f, 0.3f));
    g.s ("sub_level", g.u (0.3f, 0.7f)); g.s ("sub_oct", -1);
    g.amp (0.001f, g.u (0.09f, 0.2f), 0.0f, g.u (0.03f, 0.08f));
    g.filter (0, g.pick ({ 0, 1 }), 1, g.u (180, 600), g.u (0.15f, 0.5f), g.u (0.4f, 0.8f), 0.3f, g.u (0.0f, 0.4f));
    g.fenv (0.001f, g.u (0.05f, 0.14f), 0.0f, 0.05f);
    g.bite (g.u (0.0f, 0.4f), g.u (0.2f, 0.7f));
    g.arp (A_Played, D16, 1, g.u (0.45f, 0.7f));
    const int pattern = g.i (0, 3);
    g.arpSteps (16, [&] (int k, bool& on, int& tr, float& vel, int&, bool&)
    {
        on = (k % 4) != 0;                             // the kick takes the first 16th
        vel = (k % 4 == 1) ? 1.0f : 0.75f;
        if (pattern == 1 && k == 15) tr = 12;
        if (pattern == 2 && (k == 7 || k == 15)) tr = 7;
        if (pattern == 3 && k >= 12) tr = (k == 13 ? 3 : 0);
    });
    if (g.chance (0.5f)) { g.menv (1, 0.001f, g.u (0.05f, 0.2f), 0, 0.05f); g.route (MS_ModEnv, MD_WtAWarp, g.u (0.2f, 0.5f)); }
    if (g.chance (0.4f)) { g.lfo (1, L_Sine, 1, false, g.pick ({ D1, D2, D2B })); g.route (MS_Lfo1, MD_Cutoff, g.u (0.1f, 0.25f)); }
}

void growlBass (Gen& g)
{
    g.voice (g.pick ({ 1, 2 }), 1, g.u (0.0f, 0.08f));
    g.wt ('A', g.pick ({ T_FM, T_Fold, T_Formant, T_Digital }), g.u (0.2f, 0.9f), -1, 0.8f, g.i (1, 3), g.u (0.05f, 0.2f));
    g.warp ('A', g.pick ({ W_Fm, W_FmWin, W_Brew, W_Breed, W_BendPM }), g.u (0.3f, 0.8f), g.pick ({ W_Off, W_Crush, W_Mirror, W_Sync }), g.u (0.1f, 0.5f));
    g.wt ('C', T_Basic, 0.0f, -2, g.u (0.3f, 0.6f));
    g.s ("fm_type_a", (float) g.pick ({ 0, 1, 2 })); g.s ("fm_c_a", g.u (0.2f, 0.6f));
    g.amp (0.002f, 0.5f, 0.8f, 0.12f);
    g.filter (0, 1, 1, g.u (300, 1500), g.u (0.2f, 0.6f), g.u (0.1f, 0.4f), 0.4f, g.u (0.2f, 0.6f));
    g.bite (g.u (0.2f, 0.6f), g.u (0.2f, 0.6f), g.u (0.0f, 0.4f));
    g.lfo (1, g.pick ({ L_Sine, L_Tri, L_Square, L_SH }), 1, true, g.pick ({ D8, D8T, D16, D4 }));
    g.route (MS_Lfo1, MD_WtAWarp, g.u (0.3f, 0.6f)); g.route (MS_Lfo1, MD_Cutoff, g.u (0.2f, 0.5f));
    if (g.chance (0.5f)) { g.ff (1, g.pick ({ FF_Formant, FF_CombP, FF_MorphBP }), g.u (400, 1200), g.u (0.3f, 0.7f), g.u (0, 1), g.i (1, 3), g.u (0.3f, 0.6f)); g.route (MS_Lfo1, MD_Ff1Morph, 0.5f); }
}

void reeseBass (Gen& g)
{
    g.voice (2, 1, g.u (0.02f, 0.1f));
    g.wt ('A', T_Basic, g.u (0.6f, 0.8f), -1, 0.7f, g.i (3, 7), g.u (0.2f, 0.5f));
    g.wt ('B', T_Basic, g.u (0.6f, 0.8f), -1, 0.6f, g.i (3, 5), g.u (0.3f, 0.6f)); g.semi ('B', 0, g.u (5, 15));
    g.s ("sub_level", g.u (0.3f, 0.6f));
    g.amp (0.005f, 0.4f, 0.9f, 0.15f);
    g.filter (0, 1, 1, g.u (500, 2000), g.u (0.1f, 0.35f), 0.0f, 0.3f, g.u (0.2f, 0.5f));
    g.phaser (g.u (0.2f, 0.4f));
    g.lfo (2, L_Sine, g.u (0.1f, 0.4f)); g.route (MS_Lfo2, MD_Cutoff, g.u (0.1f, 0.3f));
}

void fmBass (Gen& g)
{
    g.voice (1);
    g.wt ('A', T_Basic, 0.0f, -1, 0.8f);
    g.wt ('B', T_Basic, 0.0f, g.pick ({ -1, 0, 1 }), 0.0f);
    g.s ("fm_type_a", (float) g.pick ({ 0, 1 })); g.s ("fm_b_a", g.u (0.3f, 0.8f)); g.s ("fm_a_a", g.u (0.0f, 0.3f));
    g.amp (0.001f, g.u (0.15f, 0.4f), g.u (0.0f, 0.4f), 0.08f);
    g.menv (1, 0.001f, g.u (0.06f, 0.2f), 0.0f, 0.05f); g.route (MS_ModEnv, MD_FmAmount, g.u (0.4f, 0.9f));
    g.filter (0, 0, 1, g.u (800, 4000), 0.2f, 0.2f, 0.3f);
    if (g.chance (0.5f)) { g.s ("smp_on", 1); g.s ("smp_sound", (float) g.pick ({ 0, 1, 2 })); g.s ("smp_level", g.u (0.3f, 0.6f)); g.s ("smp_decay", g.u (0.03f, 0.1f)); }
}

void bassline (Gen& g)
{
    // sequenced basslines: different rhythms, octave jumps, ratchets
    g.voice (1);
    g.wt ('A', g.pick ({ T_Basic, T_PWM, T_Sync, T_Harm }), g.u (0.2f, 0.9f), -1, 0.8f);
    if (g.chance (0.7f)) g.warp ('A', g.pick ({ W_BendP, W_Sync, W_Pulse, W_Fm }), g.u (0.1f, 0.5f));
    g.s ("sub_level", g.u (0.2f, 0.6f));
    g.amp (0.001f, g.u (0.1f, 0.25f), g.u (0.0f, 0.2f), 0.05f);
    g.filter (0, g.pick ({ 0, 1 }), 1, g.u (200, 900), g.u (0.3f, 0.75f), g.u (0.4f, 0.8f), 0.3f, g.u (0, 0.5f));
    g.fenv (0.001f, g.u (0.06f, 0.2f), 0.0f, 0.05f);
    g.arp (g.pick ({ A_Played, A_Up, A_UpDown }), g.pick ({ D16, D16, D16T, D8 }), g.i (1, 2), g.u (0.4f, 0.8f), g.pick ({ 0, 3, 6, 10, 2 }), 0);
    const int style = g.i (0, 4);
    juce::Random& r = g.rnd;
    g.arpSteps (16, [&] (int k, bool& on, int& tr, float& vel, int& rat, bool& tie)
    {
        switch (style)
        {
            case 0: on = k % 4 != 0; tr = (k % 8 == 6) ? 12 : 0; break;                         // rolling with octave lift
            case 1: on = k % 2 == 1 || k % 8 == 4; tr = (k % 16 == 14) ? 7 : 0; break;            // galloping
            case 2: on = r.nextFloat() > 0.3f; tr = r.nextFloat() > 0.8f ? 12 : (r.nextFloat() > 0.85f ? -12 : 0); break; // acid
            case 3: on = k % 4 != 0; rat = (k == 7 || k == 15) ? 2 : 1; break;                    // ratchet fills
            default: on = true; tr = (k / 4) % 2 ? 3 : 0; tie = (k % 4 == 3) && r.nextFloat() > 0.5f; break;   // walking
        }
        vel = (k % 4 == 1) ? 1.0f : 0.7f + 0.2f * r.nextFloat();
    });
    if (style == 2) { g.s ("flt_res", g.u (0.6f, 0.85f)); g.lfo (1, L_Sine, 1, false, D2B); g.route (MS_Lfo1, MD_Cutoff, 0.3f); }
}

void lead (Gen& g)
{
    const int type = g.i (0, 5);
    g.voice (g.pick ({ 1, 2 }), 1, g.u (0.0f, 0.12f));
    g.amp (g.u (0.002f, 0.03f), 0.4f, g.u (0.6f, 0.9f), g.u (0.15f, 0.5f));
    switch (type)
    {
        case 0:   // supersaw psy lead
            g.wt ('A', T_Basic, g.u (0.6f, 0.75f), 0, 0.7f, g.i (5, 7), g.u (0.2f, 0.45f));
            g.wt ('B', T_Basic, g.u (0.6f, 0.75f), 1, 0.4f, g.i (3, 5), g.u (0.2f, 0.4f));
            g.filter (0, 0, 0, g.u (3000, 9000), 0.15f, 0.2f, 0.4f); break;
        case 1:   // acid sync lead
            g.wt ('A', T_Sync, g.u (0.2f, 1.0f), 0, 0.8f);
            g.warp ('A', W_Sync, g.u (0.3f, 0.7f));
            g.menv (1, 0.001f, g.u (0.2f, 0.6f), 0.1f, 0.2f); g.route (MS_ModEnv, MD_WtAWarp, g.u (0.3f, 0.6f));
            g.filter (0, 1, 1, g.u (800, 3000), g.u (0.5f, 0.8f), 0.5f, 0.4f, 0.3f); g.fenv (0.001f, g.u (0.15f, 0.4f), 0.1f, 0.2f); break;
        case 2:   // FM squeal
            g.wt ('A', T_Basic, 0.0f, 0, 0.8f); g.wt ('B', T_Basic, 0.0f, g.pick ({ 1, 2 }), 0.0f); g.semi ('B', g.pick ({ 0, 7, 5 }));
            g.s ("fm_b_a", g.u (0.3f, 0.7f)); g.s ("fm_type_a", (float) g.pick ({ 0, 1, 4 }));
            g.filter (0, 0, 0, 8000, 0.1f); break;
        case 3:   // spectral lead
            g.spectral (g.pick ({ 0, 7, 12, 10, 19 }), g.u (0, 1), g.u (-0.3f, 0.3f), 0.1f);
            g.s ("sp_level", 0.8f); g.s ("sp_partials", (float) g.i (24, 64)); g.s ("sp_tilt", g.u (-0.2f, 0.4f));
            g.filter (0, 0, 0, 9000, 0.1f); break;
        case 4:   // Plaits lead
            g.plaits (g.pick ({ 0, 1, 8, 9, 10, 11 }), g.u (0.2f, 0.8f), g.u (0.3f, 0.8f), g.u (0.2f, 0.8f), 0.8f);
            g.filter (0, 0, 0, 10000, 0.1f); break;
        default:  // folded square
            g.wt ('A', T_PWM, g.u (0.2f, 0.8f), 0, 0.7f, 2, 0.15f); g.warp ('A', W_Mirror, g.u (0.2f, 0.6f), W_Pulse, g.u (0.1f, 0.5f));
            g.bite (g.u (0.1f, 0.3f), 0.3f, g.u (0.2f, 0.6f));
            g.filter (0, 1, 0, g.u (2000, 6000), 0.3f, 0.2f, 0.3f); break;
    }
    g.lfo (1, L_Sine, g.u (4.5f, 6.5f), true); g.s ("lfo1_phase", 0.0f);
    g.route (MS_Lfo1, type == 3 ? MD_SpPitch : (type == 4 ? MD_PlTimbre : MD_WtAPitch), type == 4 ? 0.1f : 0.006f, 0, MS_ModWheel);
    g.delay (g.pick ({ D8D, D8, D4T, D8T }), g.u (0.3f, 0.55f), g.u (0.15f, 0.3f));
    g.reverb (g.u (0.1f, 0.25f), g.u (1.5f, 3.5f));
    if (g.chance (0.3f)) g.phaser (0.25f);
}

void pad (Gen& g)
{
    const int type = g.i (0, 4);
    g.voice (0, 8);
    g.amp (g.u (0.6f, 2.5f), 1.0f, g.u (0.7f, 1.0f), g.u (1.5f, 5.0f));
    switch (type)
    {
        case 0:   // granular pad (built-in texture)
            g.grain (g.u (0.1f, 0.9f), g.u (120, 500), g.u (15, 60), g.u (0.1f, 0.5f), g.pick ({ 0, 1 }));
            g.s ("gr_scan", g.u (-0.1f, 0.1f)); g.s ("gr_level", 0.8f); break;
        case 1:   // spectral pad
            g.spectral (g.pick ({ 3, 4, 11, 13, 16, 17, 19, 8 }), g.u (0, 0.6f), g.u (0.05f, 0.25f), g.u (0.3f, 0.7f));
            g.s ("sp_shimmer", g.u (0.0f, 0.4f)); break;
        case 2:   // wide wavetable
            g.wt ('A', g.pick ({ T_Harm, T_Formant, T_Basic, T_FM }), g.u (0, 1), 0, 0.6f, g.i (4, 7), g.u (0.15f, 0.35f));
            g.wt ('B', g.pick ({ T_Harm, T_Formant }), g.u (0, 1), 1, 0.4f, g.i (3, 5), g.u (0.2f, 0.4f));
            g.lfo (2, L_Smooth, g.u (0.05f, 0.2f)); g.route (MS_Lfo2, MD_WtAPos, g.u (0.2f, 0.4f)); g.route (MS_Lfo2, MD_WtBPos, -g.u (0.2f, 0.4f)); break;
        case 3:   // Plaits chords / string machine
            g.plaits (g.pick ({ 6, 14, 12, 13 }), g.u (0.2f, 0.8f), g.u (0.3f, 0.7f), g.u (0.2f, 0.8f), 0.7f); break;
        default:  // hybrid spectral + grain
            g.spectral (g.pick ({ 5, 6, 14, 15 }), g.u (0, 1), g.u (0.05f, 0.15f), 0.6f); g.s ("sp_level", 0.5f);
            g.grain (g.u (0, 1), g.u (200, 600), g.u (10, 30), 0.3f); g.s ("gr_level", 0.5f); break;
    }
    g.filter (0, 0, 0, g.u (1500, 7000), g.u (0.1f, 0.3f), g.u (0.0f, 0.3f), 0.2f);
    g.fenv (g.u (0.5f, 2.0f), 2.0f, 0.5f, 3.0f);
    g.lfo (1, L_Sine, g.u (0.05f, 0.3f)); g.route (MS_Lfo1, MD_Cutoff, g.u (0.1f, 0.3f));
    if (g.chance (0.5f)) { g.s ("chaos2_rate", g.u (0.1f, 0.6f)); g.route (MS_Chaos2, type == 1 ? MD_SpPos : MD_Pan, g.u (0.2f, 0.4f)); }
    g.reverb (g.u (0.3f, 0.55f), g.u (3.0f, 9.0f), g.u (0.6f, 0.95f));
    if (g.chance (0.4f)) g.clouds (g.u (0.2f, 0.4f));
    if (g.chance (0.4f)) g.ff (1, g.pick ({ FF_Formant, FF_Allpass, FF_CombP, FF_Bank }), g.u (500, 2000), g.u (0.2f, 0.6f), g.u (0, 1), g.i (2, 6), g.u (0.2f, 0.45f));
    if (g.chance (0.3f)) g.stereo (g.u (1.2f, 1.6f));
}

void arpPreset (Gen& g)
{
    g.voice (0, 8);
    const int src = g.i (0, 3);
    switch (src)
    {
        case 0: g.wt ('A', g.pick ({ T_Basic, T_PWM, T_Harm, T_Digital }), g.u (0, 1), 0, 0.7f, g.i (1, 3), 0.15f);
                if (g.chance (0.5f)) g.warp ('A', g.pick ({ W_Sync, W_BendP, W_Fm, W_Crush }), g.u (0.1f, 0.5f)); break;
        case 1: g.plaits (g.pick ({ 7, 8, 0, 10, 2, 13 }), g.u (0.2f, 0.8f), g.u (0.2f, 0.8f), g.u (0.2f, 0.8f), 0.7f); break;
        case 2: g.spectral (g.pick ({ 5, 6, 9, 14, 15 }), g.u (0, 1), 0.0f, 0.1f); break;
        default: g.wt ('A', T_Basic, 0, 0, 0.7f); g.wt ('B', T_Basic, 0, g.pick ({ 1, 2 }), 0.0f); g.s ("fm_b_a", g.u (0.2f, 0.6f)); break;
    }
    g.amp (0.001f, g.u (0.1f, 0.35f), g.u (0.0f, 0.3f), g.u (0.1f, 0.4f));
    g.filter (0, g.pick ({ 0, 1 }), g.pick ({ 0, 1 }), g.u (900, 5000), g.u (0.2f, 0.6f), g.u (0.2f, 0.6f), 0.3f);
    g.fenv (0.001f, g.u (0.08f, 0.3f), 0.1f, 0.2f);
    const int scale = g.pick ({ 0, 2, 3, 4, 5, 6, 8, 10, 13, 14, 11, 17 });
    g.arp (g.pick ({ A_Up, A_UpDown, A_Down, A_DownUp, A_Played, A_Random }), g.pick ({ D16, D16, D16T, D8, D32 }), g.i (1, 3), g.u (0.3f, 0.8f), scale, g.i (0, 11));
    juce::Random& r = g.rnd;
    const int len = g.pick ({ 8, 12, 16, 16, 7, 5 });
    g.arpSteps (len, [&] (int k, bool& on, int& tr, float& vel, int& rat, bool&)
    {
        on = r.nextFloat() > 0.15f || k == 0;
        tr = r.nextFloat() > 0.8f ? (r.nextBool() ? 12 : 7) : 0;
        vel = 0.55f + 0.45f * r.nextFloat();
        rat = r.nextFloat() > 0.9f ? 2 : 1;
    });
    g.delay (g.pick ({ D8D, D8T, D4T, D16D }), g.u (0.3f, 0.6f), g.u (0.2f, 0.35f));
    g.reverb (g.u (0.15f, 0.3f), g.u (2.0f, 4.0f));
    g.lfo (2, L_Tri, 1, false, g.pick ({ D1, D2B, D4B })); g.route (MS_Lfo2, MD_Cutoff, g.u (0.2f, 0.4f));
}

void pluck (Gen& g)
{
    g.voice (0, 8);
    const int t = g.i (0, 3);
    if (t == 0) { g.wt ('A', g.pick ({ T_Basic, T_Harm, T_PWM }), g.u (0, 1), 0, 0.7f, g.i (1, 3)); }
    else if (t == 1) { g.plaits (g.pick ({ 19, 20, 7, 9 }), g.u (0.2f, 0.8f), g.u (0.2f, 0.8f), g.u (0.2f, 0.8f), 0.8f); }
    else if (t == 2) { g.spectral (9, g.u (0.0f, 0.3f), g.u (0.5f, 1.5f), 0.0f); }
    else { g.wt ('A', T_Basic, 0, 0, 0.7f); g.wt ('B', T_Basic, 0, 2, 0.0f); g.s ("fm_b_a", g.u (0.3f, 0.6f)); g.menv (1, 0.001f, 0.15f, 0, 0.1f); g.route (MS_ModEnv, MD_FmAmount, 0.6f); }
    g.amp (0.001f, g.u (0.15f, 0.5f), 0.0f, g.u (0.2f, 0.6f));
    g.filter (0, 0, 1, g.u (600, 2500), g.u (0.1f, 0.4f), g.u (0.4f, 0.8f), 0.4f);
    g.fenv (0.001f, g.u (0.08f, 0.3f), 0.0f, 0.2f);
    if (g.chance (0.4f)) g.rings (g.u (0.2f, 0.4f), g.pick ({ 0, 2, 4 }));
    g.delay (g.pick ({ D8D, D8, D4T }), g.u (0.25f, 0.5f), g.u (0.15f, 0.3f));
    g.reverb (g.u (0.15f, 0.35f), g.u (1.5f, 4.0f));
}

void keysBells (Gen& g)
{
    g.voice (0, 10);
    const int t = g.i (0, 2);
    if (t == 0) { g.spectral (g.pick ({ 5, 6, 18, 14 }), 0.0f, g.u (0.3f, 1.0f), 0.0f); g.s ("sp_stretch", g.u (0.05f, 0.4f)); }
    else if (t == 1) { g.wt ('A', T_Basic, 0, 0, 0.6f); g.wt ('B', T_Basic, 0, g.pick ({ 1, 2 }), 0.0f); g.semi ('B', g.pick ({ 0, 7, 4 }), g.u (0, 8));
                       g.s ("fm_b_a", g.u (0.2f, 0.5f)); g.menv (1, 0.001f, g.u (0.4f, 1.2f), 0.1f, 0.6f); g.route (MS_ModEnv, MD_FmAmount, 0.5f); }
    else { g.plaits (g.pick ({ 20, 12, 2, 3, 4 }), g.u (0.2f, 0.8f), g.u (0.2f, 0.8f), g.u (0.2f, 0.8f), 0.8f); }
    g.amp (0.001f, g.u (0.8f, 2.5f), g.u (0.0f, 0.3f), g.u (0.6f, 2.0f));
    g.filter (0, 0, 0, g.u (4000, 12000), 0.1f, 0.1f, 0.3f);
    if (g.chance (0.5f)) g.rings (g.u (0.15f, 0.35f), g.pick ({ 0, 1, 5 }));
    g.reverb (g.u (0.2f, 0.4f), g.u (2.5f, 6.0f));
    if (g.chance (0.4f)) g.delay (D4T, 0.35f, 0.15f);
}

void generative (Gen& g)
{
    // hold a chord: random arp in a scale, chaos and sequencers move everything, nothing repeats
    g.voice (0, 10);
    const int t = g.i (0, 3);
    if (t == 0) { g.wt ('A', g.pick ({ T_Harm, T_Formant, T_Basic, T_FM }), g.u (0, 1), 0, 0.7f, g.i (1, 3)); g.warp ('A', g.pick ({ W_BendP, W_Fm, W_Sync, W_Mirror }), g.u (0.1f, 0.4f)); }
    else if (t == 1) { g.spectral (g.i (0, 19), g.u (0, 1), g.u (-0.4f, 0.4f), 0.2f); }
    else if (t == 2) { g.plaits (g.pick ({ 8, 13, 19, 20, 2, 11 }), g.u (0.2f, 0.8f), g.u (0.2f, 0.8f), g.u (0.2f, 0.8f), 0.8f); }
    else { g.wt ('A', T_Basic, 0, 0, 0.7f); g.wt ('B', T_Basic, 0, 1, 0.0f); g.s ("fm_b_a", g.u (0.2f, 0.5f)); }
    g.amp (0.002f, g.u (0.2f, 0.8f), g.u (0.0f, 0.4f), g.u (0.3f, 1.2f));
    g.filter (0, g.pick ({ 0, 1 }), 0, g.u (1200, 5000), g.u (0.2f, 0.5f), g.u (0.1f, 0.4f), 0.3f);
    const int scale = g.pick ({ 2, 3, 4, 5, 6, 8, 10, 11, 12, 13, 14, 17, 18 });
    g.arp (A_Random, g.pick ({ D16, D8, D8T, D16T }), g.i (2, 4), g.u (0.3f, 0.9f), scale, g.i (0, 11), true);
    juce::Random& r = g.rnd;
    g.arpSteps (g.pick ({ 7, 9, 11, 13, 16 }), [&] (int, bool& on, int& tr, float& vel, int& rat, bool&)
    {
        on = r.nextFloat() > 0.3f;
        tr = r.nextFloat() > 0.7f ? (r.nextBool() ? 12 : -12) : 0;
        vel = 0.4f + 0.6f * r.nextFloat();
        rat = r.nextFloat() > 0.85f ? r.nextInt ({ 2, 4 }) : 1;
    });
    // Lorenz and Chaos 1 drive timbre; two sequencer lanes in odd directions
    g.s ("chaos2_rate", g.u (0.3f, 3.0f)); g.s ("chaos2_rho", g.u (0.3f, 0.8f));
    g.s ("chaos1_type", (float) g.i (0, 3)); g.s ("chaos1_rate", g.u (0.2f, 4.0f));
    g.route (MS_Chaos2, MD_Cutoff, g.u (0.2f, 0.4f));
    g.route (MS_Chaos2Z, t == 1 ? MD_SpPos : (t == 2 ? MD_PlTimbre : MD_WtAPos), g.u (0.3f, 0.5f));
    g.route (MS_Chaos1, MD_Pan, g.u (0.3f, 0.6f));
    g.seqLane (1, g.pick ({ 5, 7, 11, 13 }), g.pick ({ D16, D8, D8T }), g.pick ({ 3, 4, 5, 6, 2 }), false, [&] (int) { return r.nextFloat(); }, g.u (0, 0.4f));
    g.seqLane (2, g.pick ({ 3, 6, 9 }), g.pick ({ D8, D4T, D4 }), g.pick ({ 1, 2, 3 }), true, [&] (int) { return r.nextFloat(); }, g.u (0.2f, 0.8f));
    g.route (MS_Seq1, MD_Resonance, g.u (0.1f, 0.3f));
    g.route (MS_Seq2, MD_DelayTime, g.u (0.05f, 0.15f));
    g.delay (g.pick ({ D8D, D4T, D8T }), g.u (0.4f, 0.65f), g.u (0.25f, 0.4f));
    g.reverb (g.u (0.25f, 0.45f), g.u (3.0f, 8.0f), 0.8f);
    if (g.chance (0.5f)) g.clouds (g.u (0.15f, 0.35f), g.pick ({ 0, 2 }));
    if (g.chance (0.4f)) g.ff (1, g.pick ({ FF_CombP, FF_CombN, FF_Formant, FF_Bank }), g.u (300, 1500), g.u (0.3f, 0.7f), g.u (0, 1), g.i (1, 4), g.u (0.2f, 0.4f));
    if (g.chance (0.5f)) g.route (MS_Lfo3, MD_Ff1Morph, 0.4f), g.lfo (3, L_Smooth, g.u (0.1f, 0.5f));
}

void fxTexture (Gen& g)
{
    g.voice (0, 6);
    const int t = g.i (0, 4);
    switch (t)
    {
        case 0:   // riser: noise + pitch-rising wavetable over 8 s
            g.s ("noise_level", g.u (0.3f, 0.6f)); g.s ("noise_type", (float) g.i (0, 1));
            g.wt ('A', T_Basic, 0.7f, 0, 0.5f, 7, 0.4f);
            g.menv (1, g.u (4, 8), 0.1f, 1.0f, 1.0f); g.route (MS_ModEnv, MD_WtAPitch, 0.25f); g.route (MS_ModEnv, MD_Cutoff, 0.6f);
            g.filter (2, 0, 0, 200, 0.4f); g.amp (g.u (3, 6), 1, 1, 2); break;
        case 1:   // crackle / sampler texture
            g.s ("smp_on", 1); g.s ("smp_sound", (float) g.pick ({ 10, 11, 9, 7 })); g.s ("smp_decay", g.u (0.5f, 3.0f)); g.s ("smp_level", 0.8f);
            g.s ("noise_type", 2); g.s ("noise_level", g.u (0.2f, 0.5f)); g.amp (0.01f, 1, 1, 2);
            g.filter (1, 0, 0, g.u (800, 3000), 0.5f); g.lfo (1, L_SH, g.u (2, 8)); g.route (MS_Lfo1, MD_Cutoff, 0.5f); break;
        case 2:   // granular glitch rain
            g.grain (g.u (0, 1), g.u (10, 50), g.u (60, 180), g.u (0.6f, 1.0f)); g.s ("gr_prand", g.u (3, 12)); g.s ("gr_reverse", g.u (0.2f, 0.6f));
            g.s ("gr_octrand", g.u (0.2f, 0.6f)); g.amp (0.2f, 1, 1, 2); g.bite (0, 0, 0, g.u (0.1f, 0.4f)); break;
        case 3:   // zapper / laser
            g.wt ('A', T_Basic, 0, 1, 0.7f); g.menv (1, 0.001f, g.u (0.1f, 0.6f), 0, 0.2f); g.route (MS_ModEnv, MD_WtAPitch, g.u (0.4f, 0.9f));
            g.amp (0.001f, g.u (0.2f, 0.6f), 0, 0.3f); g.filter (0, 0, 0, 12000, 0.1f); g.lfo (1, L_SawDn, g.u (4, 14), true); g.route (MS_Lfo1, MD_WtAPitch, 0.1f); break;
        default:  // metallic texture through comb + rings
            g.spectral (18, g.u (0, 1), g.u (0.2f, 0.8f), 0.3f); g.s ("sp_stretch", g.u (0.2f, 0.8f)); g.s ("sp_shimmer", 0.6f);
            g.amp (0.5f, 1, 1, 3); g.ff (1, FF_CombN, g.u (100, 600), 0.7f, 0.5f, g.i (2, 6), 0.5f); g.rings (0.3f, 1); break;
    }
    g.reverb (g.u (0.3f, 0.6f), g.u (4.0f, 12.0f), 0.85f);
    g.delay (g.pick ({ D8D, D4T }), g.u (0.4f, 0.7f), g.u (0.2f, 0.4f));
}

void drone (Gen& g)
{
    g.voice (0, 6);
    g.spectral (g.pick ({ 13, 16, 17, 19, 11, 3 }), g.u (0, 1), g.u (0.02f, 0.1f), g.u (0.4f, 0.9f)); g.s ("sp_shimmer", g.u (0.2f, 0.6f));
    g.wt ('A', g.pick ({ T_Harm, T_Formant }), g.u (0, 1), -1, 0.4f, 5, 0.25f);
    g.s ("sub_level", g.u (0.1f, 0.3f));
    g.amp (g.u (1.5f, 3.5f), 1, 1, g.u (6, 15));
    g.filter (0, 0, 0, g.u (800, 3000), 0.3f);
    g.s ("chaos2_rate", g.u (0.05f, 0.3f)); g.route (MS_Chaos2, MD_Cutoff, 0.4f); g.route (MS_Chaos2Y, MD_SpShift, 0.2f); g.route (MS_Chaos2Z, MD_WtAPos, 0.5f);
    g.reverb (g.u (0.45f, 0.7f), g.u (8.0f, 20.0f), 0.95f);
    g.clouds (g.u (0.2f, 0.4f), g.pick ({ 0, 2 }));
    if (g.chance (0.5f)) g.ff (1, FF_Formant, g.u (500, 1500), 0.5f, 0.5f, 2, 0.4f), g.lfo (3, L_Sine, 0.03f), g.route (MS_Lfo3, MD_Ff1Morph, 0.5f);
}

struct Category { const char* folder; const char* prefix; int count; Recipe recipe; float targetLufs; int test; };   // test: 0 bass note, 1 lead line, 2 chord, 3 held note
} // namespace

static void play (SynthgularityProcessor& pr, int test, float seconds, juce::AudioBuffer<float>* keep = nullptr, float* peakOut = nullptr)
{
    const double sr = 48000.0; const int block = 256;
    pr.prepareToPlay (sr, block);
    pr.meter.requestReset();
    juce::AudioBuffer<float> buf (2, block);
    const int total = (int) (sr * seconds);
    float peak = 0.0f;
    const int melody[] = { 60, 63, 65, 67, 70, 67, 65, 63 };
    for (int pos = 0; pos < total; pos += block)
    {
        juce::MidiBuffer m;
        auto at = [&] (double t) { const int s = (int) (t * sr); return s >= pos && s < pos + block ? s - pos : -1; };
        const double release = seconds - 1.2;
        if (test == 0 || test == 3)
        {
            const int note = test == 0 ? 36 : 48;
            if (int o = at (0.05); o >= 0) m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), o);
            if (int o = at (release); o >= 0) m.addEvent (juce::MidiMessage::noteOff (1, note), o);
        }
        else if (test == 1)
        {
            for (int k = 0; k < 8; ++k)
            {
                const double t0 = 0.05 + k * (release / 8.0);
                if (int o = at (t0); o >= 0) m.addEvent (juce::MidiMessage::noteOn (1, melody[k], (juce::uint8) 105), o);
                if (int o = at (t0 + release / 8.0 * 0.9); o >= 0) m.addEvent (juce::MidiMessage::noteOff (1, melody[k]), o);
            }
        }
        else
        {
            for (int n : { 48, 55, 60, 63 })
            {
                if (int o = at (0.05); o >= 0) m.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), o);
                if (int o = at (release); o >= 0) m.addEvent (juce::MidiMessage::noteOff (1, n), o);
            }
        }
        buf.clear();
        pr.processBlock (buf, m);
        peak = std::max (peak, buf.getMagnitude (0, block));
        if (keep != nullptr) for (int ch = 0; ch < 2; ++ch) keep->copyFrom (ch, pos, buf, ch, 0, std::min (block, keep->getNumSamples() - pos));
    }
    if (peakOut) *peakOut = peak;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 3 && juce::String (argv[1]) == "--demo")
    {
        // a short audition: two presets of every folder, 4.5 s each
        SynthgularityProcessor pr;
        pr.setPlayConfigDetails (0, 2, 48000.0, 256);
        juce::AudioBuffer<float> all (2, 0);
        juce::StringArray names;
        for (auto& dir : juce::File (juce::String (argv[2])).findChildFiles (juce::File::findDirectories, false))
        {
            auto files = dir.findChildFiles (juce::File::findFiles, false, "*.sgpreset");
            files.sort();
            for (int k = 0; k < 2 && k < files.size(); ++k)
            {
                const auto& f = files[k * (files.size() / 2)];
                pr.loadPreset (f);
                const auto folder = dir.getFileName();
                const int test = folder.startsWith ("01") || folder.startsWith ("02") ? 0 : folder.startsWith ("03") || folder.startsWith ("06") ? 1 : folder.startsWith ("09") ? 3 : 2;
                juce::AudioBuffer<float> b (2, (int) (48000 * 4.5));
                b.clear();
                play (pr, test, 4.5f, &b);
                const int old = all.getNumSamples();
                all.setSize (2, old + b.getNumSamples(), true, true);
                for (int ch = 0; ch < 2; ++ch) all.copyFrom (ch, old, b, ch, 0, b.getNumSamples());
                names.add (juce::String (old / 48000.0, 1) + " s  " + f.getFileNameWithoutExtension());
            }
        }
        const juce::File o { juce::String (argv[3]) };
        o.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> os (o.createOutputStream().release());
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 48000.0, 2, 24, {}, 0));
        os.release();
        w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
        std::cout << names.joinIntoString ("\n") << "\n";
        return 0;
    }
    if (argc > 2 && juce::String (argv[1]) == "--verify")
    {
        // reload every saved file through the normal preset loader and play it
        SynthgularityProcessor pr;
        pr.setPlayConfigDetails (0, 2, 48000.0, 256);
        int bad = 0, n = 0;
        for (auto& f : juce::File (juce::String (argv[2])).findChildFiles (juce::File::findFiles, true, "*.sgpreset"))
        {
            ++n;
            if (! pr.loadPreset (f)) { std::cout << "LOAD FAIL " << f.getFileName() << "\n"; ++bad; continue; }
            const auto folder = f.getParentDirectory().getFileName();
            const int test = folder.startsWith ("01") || folder.startsWith ("02") ? 0 : folder.startsWith ("03") || folder.startsWith ("06") ? 1 : folder.startsWith ("09") ? 3 : 2;
            float peak = 0;
            play (pr, test, test == 3 ? 7.0f : 5.0f, nullptr, &peak);
            const float l = pr.meter.lufsI.load();
            if (! std::isfinite (l) || l < -22.0f || pr.currentPresetName != f.getFileNameWithoutExtension()) { std::cout << "BAD " << f.getFileName() << " " << l << "\n"; ++bad; }
        }
        std::cout << "verified " << n << ", bad " << bad << "\n";
        return bad == 0 ? 0 : 1;
    }
    const juce::File out (argc > 1 ? juce::String (argv[1]) : juce::String ("FactoryLibrary"));
    out.deleteRecursively();
    out.createDirectory();
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() / 300.0f : 1.0f;

    std::vector<Category> cats = {
        { "01 Bass", "BA", 22, psyBass, -14.5f, 0 },
        { "01 Bass", "BA", 8, growlBass, -13.5f, 0 },
        { "01 Bass", "BA", 6, reeseBass, -13.5f, 0 },
        { "01 Bass", "BA", 6, fmBass, -13.5f, 0 },
        { "02 Basslines", "BL", 26, bassline, -14.5f, 0 },
        { "03 Leads", "LD", 40, lead, -14.5f, 1 },
        { "04 Pads", "PD", 40, pad, -17.0f, 2 },
        { "05 Arps", "AR", 40, arpPreset, -15.5f, 2 },
        { "06 Plucks", "PL", 25, pluck, -15.5f, 1 },
        { "07 Keys & Bells", "KY", 20, keysBells, -16.0f, 2 },
        { "08 Generative", "GN", 30, generative, -16.5f, 2 },
        { "09 FX & Textures", "FX", 22, fxTexture, -17.0f, 3 },
        { "10 Drones & Atmos", "DR", 15, drone, -18.0f, 2 },
    };
    std::set<juce::String> usedNames;
    juce::Random nameRnd (777);
    int saved = 0, failed = 0, index = 0;
    SynthgularityProcessor pr;
    pr.setPlayConfigDetails (0, 2, 48000.0, 256);
    for (auto& c : cats)
    {
        const int count = std::max (1, (int) std::round (c.count * scale));
        for (int k = 0; k < count; ++k)
        {
            ++index;
            bool ok = false;
            for (int attempt = 0; attempt < 12 && ! ok; ++attempt)
            {
                pr.resetToDefaults();
                Gen g { pr, juce::Random ((juce::int64) (index * 7919 + attempt * 104729 + 12345)) };
                g.s ("gr_on", 0);
                g.rack ({ FX_Filter1, FX_Filter2, FX_Stereo, FX_Flanger, FX_Phaser, FX_Delay, FX_Rings, FX_Clouds, FX_Reverb });
                c.recipe (g);
                g.finish (c.prefix);
                g.s ("master_gain", -6.0f);
                const float seconds = c.test == 3 ? 7.0f : 5.0f;
                float peak = 0.0f;
                play (pr, c.test, seconds, nullptr, &peak);
                const float lufs = pr.meter.lufsI.load();
                if (! (lufs > -60.0f) || ! std::isfinite (lufs)) { std::cout << "  retry (silent " << lufs << ")\n"; continue; }
                const float gain = juce::jlimit (-30.0f, 6.0f, -6.0f + (c.targetLufs - lufs));
                g.s ("master_gain", gain);
                play (pr, c.test, seconds, nullptr, &peak);
                const float lufs2 = pr.meter.lufsI.load(), tp = pr.meter.tpMax.load();
                ok = std::isfinite (lufs2) && lufs2 > c.targetLufs - 4.0f && lufs2 < c.targetLufs + 2.5f && tp < 0.5f;
                if (! ok) { std::cout << "  retry (lufs " << lufs2 << " tp " << tp << ")\n"; continue; }
                const juce::String name = makeName (nameRnd, usedNames, c.prefix);
                const auto file = out.getChildFile (c.folder).getChildFile (juce::File::createLegalFileName (name) + ".sgpreset");
                if (! pr.savePresetTo (file, name)) { std::cout << "write failed\n"; return 1; }
                ++saved;
                std::cout << juce::String (index).paddedLeft (' ', 3) << " " << name.paddedRight (' ', 34) << " " << juce::String (lufs2, 1) << " LUFS  "
                          << juce::String (tp, 1) << " dBTP  gain " << juce::String (gain, 1) << "\n";
            }
            if (! ok) { ++failed; std::cout << "FAILED " << c.prefix << " #" << k << "\n"; }
        }
    }
    std::cout << "saved " << saved << ", failed " << failed << "\n";
    return failed == 0 ? 0 : 1;
}
