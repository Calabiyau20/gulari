// Builds the West Coast Modular factory library:  WcPresetGen <out factory.xml> [count] [demoDir]
// Every preset is built from one of twelve archetypes with seeded random choices, rendered, checked for
// silence / clipping and levelled with the output master; failures are rebuilt with the next seed.
#include "WcProcessor.h"
#include "modules/M259.h"
#include "modules/MidiInterface.h"
#include "modules/OutputModule.h"
#include "modules/M281.h"
#include "modules/M292.h"
#include "modules/M266.h"
#include "modules/M245.h"
#include "modules/M242.h"
#include "modules/M257.h"
#include "modules/M258.h"
#include "modules/M264.h"
#include "modules/M230.h"
#include "modules/M285.h"
#include "modules/M277.h"
#include "modules/M207.h"
#include "modules/M296.h"
#include "modules/M248.h"
#include "modules/M218.h"
#include "modules/M250.h"
#include "modules/M252.h"
#include <set>
#include <cstdio>

using namespace wc;
using MI = MidiInterface;
static constexpr double SR = 48000.0;

struct Gen
{
    WcProcessor& p;
    juce::Random r;
    float rr (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); }
    int ri (int lo, int hi) { return lo + r.nextInt (hi - lo + 1); }
    bool chance (float c) { return r.nextFloat() < c; }
    template <typename T> T pick (std::initializer_list<T> l) { auto it = l.begin(); std::advance (it, r.nextInt ((int) l.size())); return *it; }
    void sp (const juce::String& id, float v) { if (auto* x = p.apvts.getParameter (id)) x->setValueNotifyingHost (x->convertTo0to1 (v)); else std::printf ("  !! no param %s\n", id.toRawUTF8()); }
    void on (int inst) { p.setPresent (inst, true); }
    void con (int si, int sj, int di, int dj) { if (! p.connect (si, sj, di, dj)) std::printf ("  !! cable %d.%d -> %d.%d refused\n", si, sj, di, dj); }
    static float hz (float f) { return coarseNorm (f, false); }
    static float hz258 (float f) { return std::log (f / 5.0f) / std::log (4000.0f); }
    // a 259 voice: prefix, base pitch, FM ratio, timbre - with the keyboard on both oscillators
    void voice259 (int inst, float baseHz, float ratio, float idx, float timbre, bool kbd = true)
    {
        on (inst);
        const juce::String a (prefix259 (inst - kInst259));
        sp (a + "po_freq", hz (baseHz)); sp (a + "mo_freq", hz (baseHz * ratio));
        sp (a + "idx", idx); sp (a + "mod_pitch", idx > 0.01f ? 1.0f : 0.0f);
        sp (a + "timbre", timbre); sp (a + "sym", rr (0.35f, 0.65f)); sp (a + "order", chance (0.4f) ? rr (0.0f, 0.6f) : 0.0f);
        sp (a + "mo_wave", (float) ri (0, 2));
        if (chance (0.25f)) sp (a + "mod_timbre", 1.0f);
        if (chance (0.15f)) sp (a + "mod_amp", 1.0f);
        if (kbd) { con (kInstMidi, MI::Out_Pitch, inst, M259::In_PoPitch); con (kInstMidi, MI::Out_Pitch2, inst, M259::In_MoPitch); }
    }
    juce::String p259 (int inst) { return prefix259 (inst - kInst259); }
};

// ---------------------------------------------------------------------------------------------
// archetypes: return the render mode (0 = a held note, 1 = plays by itself, 2 = a held chord)
// ---------------------------------------------------------------------------------------------
static int bass (Gen& g)
{
    const int A = kInst259;
    g.voice259 (A, g.pick ({ 32.7f, 41.2f, 49.0f, 55.0f, 65.4f }), g.pick ({ 0.5f, 1.0f, 1.0f, 2.0f, 3.0f }), g.rr (0.0f, 0.35f), g.rr (0.15f, 0.7f));
    g.sp ("midi_trans", -1.0f);
    const juce::String a = g.p259 (A), f (prefix281 (0)), l (prefix292 (0));
    g.on (kInst281); g.on (kInst292);
    g.con (kInstMidi, MI::Out_Gate, kInst281, M281::inTrig (0));
    g.sp (f + "mode1", 1.0f); g.sp (f + "att1", g.rr (0.0f, 0.15f)); g.sp (f + "dec1", g.rr (0.35f, 0.6f));
    g.con (kInst281, M281::outFunc (0), kInst292, M292::inCtl (0));
    if (g.chance (0.7f)) { g.con (kInst281, M281::outFunc (0), A, M259::In_TimbreCv); g.sp (a + "timbre_cv", g.rr (0.2f, 0.7f)); }
    g.con (A, M259::Out_Final1, kInst292, M292::inSig (0));
    g.con (kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left);
    g.sp (l + "mode1", (float) g.pick ({ 0, 1, 1 })); g.sp (l + "res1", g.rr (0.3f, 0.9f));
    if (g.chance (0.35f)) g.sp ("midi_glide", g.rr (0.1f, 0.4f));
    return 0;
}

static int lead (Gen& g)
{
    const int A = kInst259;
    g.voice259 (A, g.pick ({ 65.4f, 98.0f, 130.8f }), g.pick ({ 1.0f, 2.0f, 1.5f, 3.0f, 4.0f }), g.rr (0.0f, 0.4f), g.rr (0.1f, 0.5f));
    const juce::String a = g.p259 (A), f (prefix281 (0)), l (prefix292 (0));
    g.on (kInst281); g.on (kInst292);
    g.con (kInstMidi, MI::Out_Gate, kInst281, M281::inTrig (0));
    g.sp (f + "mode1", 1.0f); g.sp (f + "att1", g.rr (0.05f, 0.35f)); g.sp (f + "dec1", g.rr (0.45f, 0.7f));
    g.con (kInst281, M281::outFunc (0), kInst292, M292::inCtl (0));
    g.con (kInst281, M281::outFunc (0), A, M259::In_TimbreCv); g.sp (a + "timbre_cv", g.rr (0.3f, 0.8f));
    if (g.chance (0.6f))
    {   // slow vibrato / wobble from a cycling function
        g.sp (f + "mode2", 2.0f); g.sp (f + "att2", g.rr (0.3f, 0.5f)); g.sp (f + "dec2", g.rr (0.3f, 0.5f));
        g.con (kInst281, M281::outFunc (1), A, g.pick ({ M259::In_OrderCv, M259::In_SymCv, M259::In_IndexCv }));
    }
    g.con (A, M259::Out_Final1, kInst292, M292::inSig (0));
    g.sp (l + "mode1", 1.0f);
    if (g.chance (0.5f))
    {
        g.on (kInst277);
        g.con (kInst292, M292::outSig (0), kInst277, M277::In_A);
        g.sp ("dl_time", g.rr (0.3f, 1.0f)); g.sp ("dl_long", g.chance (0.5f) ? 1.0f : 0.0f);
        g.con (kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left);
        g.con (kInst277, g.pick ({ M277::Out_Q3, M277::Out_T }), kInstOutput, OutputModule::In_Right);
    }
    else g.con (kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left);
    g.sp ("midi_glide", g.rr (0.0f, 0.35f));
    return 0;
}

static int pluck (Gen& g)
{
    const int A = kInst259;
    g.voice259 (A, g.pick ({ 65.4f, 98.0f, 130.8f, 196.0f }), g.pick ({ 1.0f, 2.0f, 3.0f, 3.5f, 0.5f, 7.0f }), g.rr (0.05f, 0.5f), g.rr (0.05f, 0.45f));
    const juce::String l (prefix292 (0));
    g.on (kInst292);
    g.con (kInstMidi, MI::Out_Pulse, kInst292, M292::inCtl (0));
    g.con (A, M259::Out_Final1, kInst292, M292::inSig (0));
    g.sp (l + "mode1", (float) g.pick ({ 0, 1, 1, 2 })); g.sp (l + "res1", g.rr (0.2f, 1.0f)); g.sp (l + "off1", g.rr (0.0f, 0.08f));
    if (g.chance (0.4f))
    {   // velocity into the timbre
        g.con (kInstMidi, MI::Out_Vel, A, M259::In_TimbreCv); g.sp (g.p259 (A) + "timbre_cv", g.rr (0.2f, 0.6f));
    }
    if (g.chance (0.45f))
    {
        g.on (kInst277);
        g.con (kInst292, M292::outSig (0), kInst277, M277::In_A);
        g.sp ("dl_time", g.rr (0.2f, 0.9f)); g.sp ("dl_long", 1.0f);
        g.con (kInst277, M277::Out_T, kInst277, M277::In_B); g.sp ("dl_lvlb", g.rr (0.3f, 0.6f));   // feedback echoes
        g.con (kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left);
        g.con (kInst277, M277::Out_Q2, kInstOutput, OutputModule::In_Right);
    }
    else g.con (kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left);
    return 0;
}

static int bell (Gen& g)
{
    const int A = kInst259;
    g.voice259 (A, g.pick ({ 130.8f, 196.0f, 261.6f }), g.pick ({ 1.414f, 2.76f, 3.5f, 1.73f, 5.19f, 2.0f }), g.rr (0.25f, 0.6f), g.rr (0.0f, 0.2f));
    g.sp ("out_lag", g.rr (150.0f, 300.0f));
    g.con (kInstMidi, MI::Out_Gate, kInstOutput, OutputModule::In_LevelCv);
    if (g.chance (0.5f))
    {
        g.on (kInst285);
        g.con (A, M259::Out_Final1, kInst285, M285::In_FsSignal);
        g.sp ("fs_ref", g.rr (0.03f, 0.25f));
        g.con (kInst285, M285::Out_Sum, kInstOutput, OutputModule::In_Left);
        g.con (kInst285, M285::Out_Diff, kInstOutput, OutputModule::In_Right);
    }
    else
    {
        g.on (kInst277);
        g.con (A, M259::Out_Final1, kInst277, M277::In_A);
        g.sp ("dl_time", g.rr (0.4f, 1.0f)); g.sp ("dl_long", 1.0f);
        g.con (A, M259::Out_Final1, kInstOutput, OutputModule::In_Left);
        g.con (kInst277, M277::Out_T, kInstOutput, OutputModule::In_Right);
    }
    g.sp ("midi_trans", g.pick ({ 0.0f, 1.0f }));
    return 0;
}

static int pad (Gen& g)
{
    const int A = kInst259, B = kInst259 + 1;
    const float base = g.pick ({ 65.4f, 98.0f, 130.8f });
    g.voice259 (A, base, g.pick ({ 1.0f, 2.0f }), g.rr (0.0f, 0.2f), g.rr (0.1f, 0.4f));
    g.voice259 (B, base * g.pick ({ 1.0f, 1.5f, 2.0f }), g.pick ({ 1.0f, 2.0f }), g.rr (0.0f, 0.2f), g.rr (0.1f, 0.4f));
    g.sp (g.p259 (B) + "po_fine", g.rr (0.08f, 0.25f));
    const juce::String f (prefix281 (0));
    g.on (kInst281);
    g.sp (f + "mode1", 2.0f); g.sp (f + "att1", g.rr (0.7f, 0.9f)); g.sp (f + "dec1", g.rr (0.7f, 0.9f));
    g.sp (f + "mode2", 2.0f); g.sp (f + "att2", g.rr (0.6f, 0.9f)); g.sp (f + "dec2", g.rr (0.6f, 0.9f));
    g.con (kInst281, M281::outFunc (0), A, M259::In_TimbreCv); g.sp (g.p259 (A) + "timbre_cv", g.rr (0.3f, 0.7f));
    g.con (kInst281, M281::outFunc (1), B, M259::In_TimbreCv); g.sp (g.p259 (B) + "timbre_cv", g.rr (0.3f, 0.7f));
    g.sp ("out_lag", 300.0f);
    g.con (kInstMidi, MI::Out_Gate, kInstOutput, OutputModule::In_LevelCv);
    if (g.chance (0.5f))
    {
        g.on (kInst277);
        g.con (A, M259::Out_Final1, kInst277, M277::In_A); g.con (B, M259::Out_Final1, kInst277, M277::In_B);
        g.sp ("dl_lvlb", 0.8f); g.sp ("dl_time", g.rr (0.5f, 1.0f)); g.sp ("dl_long", 1.0f);
        g.con (kInst281, M281::outFunc (1), kInst277, M277::In_Control); g.sp ("dl_ctl", g.rr (0.05f, 0.2f));
        g.con (kInst277, M277::Out_Q3, kInstOutput, OutputModule::In_Left); g.con (kInst277, M277::Out_T, kInstOutput, OutputModule::In_Right);
    }
    else { g.con (A, M259::Out_Final1, kInstOutput, OutputModule::In_Left); g.con (B, M259::Out_Final1, kInstOutput, OutputModule::In_Right); }
    return 0;
}

static int keys (Gen& g)
{
    const int O = kInst258;
    g.on (O);
    const juce::String o (prefix258 (0)), f (prefix281 (0)), l (prefix292 (0));
    const float base = g.pick ({ 65.4f, 98.0f, 130.8f });
    g.sp (o + "freq1", Gen::hz258 (base)); g.sp (o + "freq2", Gen::hz258 (base * g.pick ({ 1.0f, 2.0f, 1.5f, 3.0f })));
    g.sp (o + "fine2", g.rr (-0.15f, 0.15f));
    g.sp (o + "shape1", g.rr (0.0f, 0.6f)); g.sp (o + "shape2", g.rr (0.0f, 0.5f));
    g.con (kInstMidi, MI::Out_Pitch, O, M258::inOf (0, 0)); g.con (kInstMidi, MI::Out_Pitch2, O, M258::inOf (1, 0));
    g.on (kInst281); g.on (kInst292);
    g.con (kInstMidi, MI::Out_Gate, kInst281, M281::inTrig (0));
    g.sp (f + "mode1", 1.0f); g.sp (f + "att1", g.rr (0.0f, 0.1f)); g.sp (f + "dec1", g.rr (0.45f, 0.65f));
    g.con (kInst281, M281::outFunc (0), kInst292, M292::inCtl (0)); g.con (kInst281, M281::outFunc (0), kInst292, M292::inCtl (1));
    g.con (kInst281, M281::outFunc (0), O, M258::inOf (0, 3));
    g.con (O, M258::Out_1, kInst292, M292::inSig (0)); g.con (O, M258::Out_2, kInst292, M292::inSig (1));
    g.sp (l + "mode1", 1.0f); g.sp (l + "mode2", (float) g.pick ({ 0, 1 }));
    g.con (kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left); g.con (kInst292, M292::outSig (1), kInstOutput, OutputModule::In_Right);
    return 0;
}

static const int kScales[6][7] = { { 0, 3, 5, 7, 10, 12, 15 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 1, 3, 5, 7, 8, 10 },
                                   { 0, 2, 4, 7, 9, 12, 14 }, { 0, 3, 7, 10, 12, 15, 19 }, { 0, 2, 3, 5, 7, 8, 11 } };

static int sequence (Gen& g)
{
    const int A = kInst259;
    g.voice259 (A, g.pick ({ 55.0f, 65.4f, 82.4f, 110.0f }), g.pick ({ 1.0f, 2.0f, 3.0f, 0.5f }), g.rr (0.05f, 0.35f), g.rr (0.05f, 0.4f), false);
    const juce::String a = g.p259 (A), l (prefix292 (0));
    g.on (kInst292);
    const int* sc = kScales[g.ri (0, 5)];
    const int kind = g.ri (0, 2);
    int mode = 1;
    if (kind == 0)
    {   // 245 with its pulser
        g.on (kInst245);
        for (int k = 1; k <= 5; ++k) { g.sp ("sv_a" + juce::String (k), 0.01f * (float) sc[g.ri (0, 6)]); g.sp ("sv_b" + juce::String (k), g.rr (0.0f, 0.8f)); }
        g.sp ("sv_time", g.rr (0.45f, 0.62f)); g.sp ("sv_len", g.rr (0.05f, 0.4f));
        if (g.chance (0.4f)) g.sp ("sv_sw" + juce::String (g.ri (2, 4)), 0.0f);
        g.con (kInst245, M245::Out_A, A, M259::In_PoPitch); g.con (kInst245, M245::Out_A, A, M259::In_MoPitch);
        g.con (kInst245, M245::Out_B, A, M259::In_TimbreCv); g.sp (a + "timbre_cv", g.rr (0.2f, 0.6f));
        g.con (kInst245, M245::Out_Pulse, kInst292, M292::inCtl (0));
    }
    else if (kind == 1)
    {   // 248 MARF started by a key
        g.on (kInst248);
        const int n = g.pick ({ 4, 6, 8, 8, 12, 16 });
        for (int k = 1; k <= n; ++k)
        {
            const juce::String s (k);
            g.sp ("mf_lvl" + s, 0.01f * (float) sc[g.ri (0, 6)]); g.sp ("mf_tim" + s, g.rr (0.3f, 0.7f));
            g.sp ("mf_p1_" + s, g.chance (0.8f) ? 1.0f : 0.0f); g.sp ("mf_trange_" + s, 1.0f);
            if (g.chance (0.15f)) g.sp ("mf_slope_" + s, 1.0f);
        }
        g.sp ("mf_last_" + juce::String (n), 1.0f);
        g.sp ("mf_mult1", g.rr (0.2f, 0.45f));
        g.con (kInstMidi, MI::Out_Gate, kInst248, M248::inJ (0, 2));
        g.con (kInst248, M248::outJ (0, 3), A, M259::In_PoPitch);
        g.con (kInst248, M248::outJ (0, 5), kInst292, M292::inCtl (0));
        g.con (kInst248, M248::outJ (0, 1), A, M259::In_TimbreCv); g.sp (a + "timbre_cv", g.rr (0.1f, 0.4f));
        mode = 0;
    }
    else
    {   // 250 started by a key
        g.on (kInst250);
        for (int k = 1; k <= 16; ++k)
        {
            const juce::String s (k);
            g.sp ("sq_v1_" + s, 0.01f * (float) sc[g.ri (0, 6)]); g.sp ("sq_v2_" + s, g.rr (0.0f, 0.7f));
            g.sp ("sq_t_" + s, g.rr (0.3f, 0.6f)); g.sp ("sq_p1_" + s, g.chance (0.75f) ? 1.0f : 0.0f);
            if (g.chance (0.15f)) g.sp ("sq_vi_" + s, 1.0f);
        }
        if (g.chance (0.5f)) { g.sp ("sq_loopend", (float) g.pick ({ 4, 8 })); g.sp ("sq_loopto", 1.0f); g.sp ("sq_loops", (float) g.ri (1, 3)); }
        g.sp ("sq_mult", g.rr (0.3f, 0.5f));
        g.con (kInstMidi, MI::Out_Pulse, kInst250, M250::In_Start);
        g.con (kInst250, M250::Out_Cv1, A, M259::In_PoPitch);
        g.con (kInst250, M250::Out_Cv2, A, M259::In_TimbreCv); g.sp (a + "timbre_cv", g.rr (0.2f, 0.6f));
        g.con (kInst250, M250::Out_P1, kInst292, M292::inCtl (0));
        mode = 0;
    }
    g.con (A, M259::Out_Final1, kInst292, M292::inSig (0));
    g.sp (l + "mode1", 1.0f); g.sp (l + "res1", g.rr (0.3f, 0.9f));
    if (g.chance (0.5f))
    {
        g.on (kInst277);
        g.con (kInst292, M292::outSig (0), kInst277, M277::In_A);
        g.sp ("dl_time", g.rr (0.3f, 1.0f)); g.sp ("dl_long", 1.0f);
        g.con (kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left);
        g.con (kInst277, M277::Out_T, kInstOutput, OutputModule::In_Right);
    }
    else g.con (kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left);
    return mode;
}

static int rhythm (Gen& g)
{
    const int A = kInst259, B = kInst259 + 1;
    g.voice259 (A, g.pick ({ 41.2f, 49.0f, 55.0f }), g.pick ({ 1.0f, 2.0f }), g.rr (0.1f, 0.5f), g.rr (0.1f, 0.5f), false);
    g.voice259 (B, g.pick ({ 220.0f, 330.0f, 440.0f }), g.pick ({ 1.41f, 2.76f, 3.5f }), g.rr (0.2f, 0.6f), g.rr (0.0f, 0.3f), false);
    g.on (kInst292); g.on (kInst266);
    const juce::String l (prefix292 (0));
    if (g.chance (0.6f))
    {   // 252 rings
        g.on (kInst252);
        auto* m = g.p.rack.m252();
        const int ra = g.pick ({ 10, 10, 9, 5 }), rb = g.pick ({ 2, 4, 6, 8 }), rc = g.pick ({ 0, 1, 3 });
        const int na = M252::kSize[ra], nb = M252::kSize[rb], nc = M252::kSize[rc];
        const int ka = g.ri (3, na - 1), kb = g.ri (1, nb), kc = g.ri (1, nc);
        for (int i = 0; i < na; ++i) m->rgb[M252::base (ra) + i].store (((i * ka) % na) < ka ? 1 : 0);
        for (int i = 0; i < nb; ++i) m->rgb[M252::base (rb) + i].store (((i * kb) % nb) < kb ? 2 : 0);
        for (int i = 0; i < nc; ++i) m->rgb[M252::base (rc) + i].store (((i * kc) % nc) < kc ? 4 : 0);
        g.sp ("rg_clk1", (float) (ra + 1)); g.sp ("rg_clk2", (float) (rb + 1)); g.sp ("rg_clk3", (float) (rc + 1));
        g.sp ("rg_sync", (float) g.pick ({ 1, 1, 2 })); g.sp ("rg_bpm", (float) g.ri (90, 150)); g.sp ("rg_pdur", g.rr (0.02f, 0.2f));
        g.con (kInst252, M252::Out_R, kInst292, M292::inCtl (0)); g.con (kInst252, M252::Out_G, kInst292, M292::inCtl (1));
        g.con (kInst252, M252::Out_B, kInst292, M292::inCtl (2));
    }
    else
    {   // 242 matrix
        g.on (kInst242);
        const int len = g.pick ({ 8, 12, 12, 16 > 12 ? 12 : 12 });
        g.sp ("pp_len", (float) len); g.sp ("pp_period", g.rr (0.42f, 0.5f));
        for (int c = 1; c <= len; ++c)
        {
            if (g.chance (0.4f)) g.sp ("pp_a" + juce::String (c), 1.0f);
            if (g.chance (0.3f)) g.sp ("pp_b" + juce::String (c), 1.0f);
            if (g.chance (0.25f)) g.sp ("pp_c" + juce::String (c), 1.0f);
        }
        g.sp ("pp_a1", 1.0f);
        g.con (kInst242, M242::Out_A, kInst292, M292::inCtl (0)); g.con (kInst242, M242::Out_B, kInst292, M292::inCtl (1));
        g.con (kInst242, M242::Out_C, kInst292, M292::inCtl (2));
    }
    g.con (A, M259::Out_Final1, kInst292, M292::inSig (0)); g.con (B, M259::Out_Final1, kInst292, M292::inSig (1));
    g.con (kInst266, g.pick ({ M266::Out_White, M266::Out_Blue, M266::Out_Pink }), kInst292, M292::inSig (2));
    for (int k = 1; k <= 3; ++k) { g.sp (l + "mode" + juce::String (k), (float) g.pick ({ 0, 1, 2 })); g.sp (l + "res" + juce::String (k), g.rr (0.2f, 0.9f)); }
    g.con (kInst292, M292::outAll, kInstOutput, OutputModule::In_Left);
    return 1;
}

static int generative (Gen& g)
{
    const int A = kInst259;
    g.voice259 (A, g.pick ({ 65.4f, 98.0f, 130.8f }), g.pick ({ 1.0f, 2.0f, 1.5f, 3.0f }), g.rr (0.05f, 0.35f), g.rr (0.05f, 0.4f), false);
    const juce::String a = g.p259 (A), f (prefix281 (0)), l (prefix292 (0));
    g.on (kInst266); g.on (kInst281); g.on (kInst292);
    g.sp (f + "mode1", 2.0f); g.sp (f + "att1", g.rr (0.0f, 0.2f)); g.sp (f + "dec1", g.rr (0.45f, 0.7f));
    g.con (kInst281, M281::outEod (0), kInst266, M266::In_QPulse);
    g.sp ("ua_steps", (float) g.ri (3, 6)); g.sp ("ua_smooth", g.rr (0.0f, 0.3f)); g.sp ("ua_rate", g.rr (0.2f, 0.6f));
    if (g.chance (0.5f))
    {   // through the 264 quantizer (ART output) in a key
        g.on (kInst264);
        g.con (kInst266, M266::Out_Qn1, kInst264, M264::In_Cv1);
        g.con (kInst281, M281::outEod (0), kInst264, M264::In_Pulse1);
        g.sp ("sh_scale", (float) g.ri (0, 2)); g.sp ("sh_key", (float) g.ri (0, 11));
        g.con (kInst264, M264::Out_Art1, A, M259::In_PoPitch);
    }
    else
    {
        g.con (kInst266, M266::Out_Qn1, kInst266, M266::In_IntIn);
        g.con (kInst266, M266::Out_Int, A, M259::In_PoPitch);
    }
    g.con (kInst266, M266::Out_Fluct1, A, M259::In_TimbreCv); g.sp (a + "timbre_cv", g.rr (0.2f, 0.6f));
    g.con (kInst281, M281::outEod (0), kInst292, M292::inCtl (0));
    g.con (A, M259::Out_Final1, kInst292, M292::inSig (0));
    g.sp (l + "mode1", 1.0f);
    g.on (kInst277);
    g.con (kInst292, M292::outSig (0), kInst277, M277::In_A);
    g.sp ("dl_time", g.rr (0.3f, 1.0f)); g.sp ("dl_long", 1.0f);
    g.con (kInst266, M266::Out_Fluct2, kInst277, M277::In_Control); g.sp ("dl_ctl", g.rr (0.0f, 0.15f));
    g.con (kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left);
    g.con (kInst277, M277::Out_T, kInstOutput, OutputModule::In_Right);
    return 1;
}

static int spectral (Gen& g)
{
    const int A = kInst259;
    g.on (kInst296);
    g.voice259 (A, g.pick ({ 55.0f, 65.4f, 82.4f }), g.pick ({ 1.0f, 2.0f, 1.5f }), g.rr (0.1f, 0.4f), g.rr (0.4f, 0.8f));
    g.con (A, M259::Out_Final1, kInst296, M296::In_SigAll);
    for (int b = 0; b < 16; ++b) g.sp ("sp_att" + juce::String (b), g.rr (0.55f, 0.95f));
    g.on (kInst281);
    const juce::String f (prefix281 (0));
    g.sp (f + "mode1", 2.0f); g.sp (f + "att1", g.rr (0.6f, 0.85f)); g.sp (f + "dec1", g.rr (0.6f, 0.85f));
    g.con (kInst281, M281::outFunc (0), kInst296, M296::In_FreqCv);
    g.sp ("sp_freq", g.rr (0.0f, 0.3f)); g.sp ("sp_fcv", g.rr (0.5f, 0.8f)); g.sp ("sp_width", g.rr (0.1f, 0.3f));
    g.sp ("out_lag", 120.0f);
    g.con (kInstMidi, MI::Out_Gate, kInstOutput, OutputModule::In_LevelCv);
    g.con (kInst296, M296::Out_ProgEven, kInstOutput, OutputModule::In_Left);
    g.con (kInst296, M296::Out_ProgOdd, kInstOutput, OutputModule::In_Right);
    return 0;
}

static int fx (Gen& g)
{
    const int A = kInst259;
    g.voice259 (A, g.pick ({ 65.4f, 98.0f, 130.8f }), g.pick ({ 1.0f, 2.0f, 3.5f }), g.rr (0.1f, 0.5f), g.rr (0.1f, 0.5f));
    g.on (kInst285); g.on (kInst266); g.on (kInst277);
    g.sp ("out_lag", 200.0f);
    g.con (kInstMidi, MI::Out_Gate, kInstOutput, OutputModule::In_LevelCv);
    if (g.chance (0.5f))
    {   // ring / variable modulation with noise or a second source
        g.con (A, M259::Out_Final1, kInst285, M285::In_BmSignal);
        g.con (A, g.pick ({ M259::Out_MoSignal, M259::Out_Square }), kInst285, M285::In_BmRef);
        g.sp ("fs_amp", g.rr (0.4f, 1.0f));
        g.con (kInst285, M285::Out_Var, kInst277, M277::In_A);
        g.con (kInst285, M285::Out_Ring, kInstOutput, OutputModule::In_Left);
    }
    else
    {   // shifter with the fluctuating voltages on the shift
        g.con (A, M259::Out_Final1, kInst285, M285::In_FsSignal);
        g.con (kInst266, M266::Out_Fluct1, kInst285, M285::In_FsControl); g.sp ("fs_ctl", g.rr (-0.6f, 0.6f));
        g.sp ("fs_ref", g.rr (0.0f, 0.15f)); g.sp ("ua_rate", g.rr (0.2f, 0.5f));
        g.con (kInst285, M285::Out_Sum, kInst277, M277::In_A);
        g.con (kInst285, M285::Out_Diff, kInstOutput, OutputModule::In_Left);
    }
    g.sp ("dl_time", g.rr (0.0f, 1.0f)); g.sp ("dl_long", g.chance (0.5f) ? 1.0f : 0.0f);
    g.con (kInst266, M266::Out_Fluct2, kInst277, M277::In_Control); g.sp ("dl_ctl", g.rr (0.05f, 0.4f));
    g.con (kInst277, M277::Out_T, kInst277, M277::In_B); g.sp ("dl_lvlb", g.rr (0.3f, 0.7f));
    g.con (kInst277, M277::Out_Q3, kInstOutput, OutputModule::In_Right);
    return 0;
}

static int touch (Gen& g)
{
    const int A = kInst259;
    g.voice259 (A, g.pick ({ 32.7f, 65.4f, 98.0f }), g.pick ({ 1.0f, 2.0f, 3.0f }), g.rr (0.05f, 0.35f), g.rr (0.05f, 0.4f), false);
    g.on (kInst218); g.on (kInst292);
    const juce::String a = g.p259 (A), l (prefix292 (0));
    g.sp ("kb_arp", (float) g.pick ({ 0, 0, 1 })); g.sp ("kb_rate", g.rr (0.45f, 0.7f)); g.sp ("kb_porta", g.rr (0.0f, 0.25f));
    g.sp ("kb_add", (float) g.pick ({ 0, 2 })); g.sp ("kb_sel", (float) g.ri (0, 2));
    g.con (kInst218, M218::Out_Pitch, A, M259::In_PoPitch); g.con (kInst218, M218::Out_Pitch, A, M259::In_MoPitch);
    g.con (kInst218, M218::Out_Pulse, kInst292, M292::inCtl (0));
    g.con (kInst218, M218::Out_Pressure, A, M259::In_TimbreCv); g.sp (a + "timbre_cv", g.rr (0.2f, 0.5f));
    g.con (A, M259::Out_Final1, kInst292, M292::inSig (0));
    g.sp (l + "mode1", 1.0f); g.sp (l + "res1", g.rr (0.3f, 0.9f));
    g.con (kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left);
    if (g.chance (0.5f))
    {
        g.on (kInst277);
        g.con (kInst292, M292::outSig (0), kInst277, M277::In_A);
        g.sp ("dl_time", g.rr (0.2f, 0.8f)); g.sp ("dl_long", 1.0f);
        g.con (kInst277, M277::Out_T, kInstOutput, OutputModule::In_Right);
    }
    return 2;
}

// ---------------------------------------------------------------------------------------------
// names
// ---------------------------------------------------------------------------------------------
static juce::String makeName (juce::Random& r, const juce::String& cat, std::set<juce::String>& used)
{
    static const juce::StringArray adj { "Folded", "Liquid", "Orbital", "Quantum", "Hollow", "Velvet", "Static", "Solar", "Lunar", "Fractal", "Silver",
                                         "Copper", "Glass", "Neon", "Stellar", "Deep", "Distant", "Bright", "Dusty", "Electric", "Patagonian", "Andean",
                                         "Berkeley", "Pacific", "Nordic", "Ancient", "Rubber", "Woven", "Fluid", "Prismatic", "Ionic", "Plasma", "Arcane",
                                         "Tidal", "Magnetic", "Feral", "Crystal", "Shifting", "Spiral", "Infinite", "Calabi", "Hyper", "Nebular", "Molten" };
    static const juce::StringArray noun { "Horizon", "Quasar", "Pulse", "Orbit", "Bloom", "Drift", "Ritual", "Engine", "Lattice", "Nebula", "Garden",
                                          "Signal", "Comet", "Mirror", "Tide", "Bongo", "Bell", "Chime", "Voice", "Swarm", "Circuit", "Temple", "Canyon",
                                          "Aurora", "Vortex", "Meridian", "Prism", "Manifold", "Singularity", "Monolith", "Fjord", "Wind", "Ember", "Echo",
                                          "Cascade", "Helix", "Spectrum", "Machine", "Cathedral", "Geyser", "Satellite", "Cell", "Wave", "Field" };
    for (int tries = 0; tries < 200; ++tries)
    {
        const juce::String n = adj[r.nextInt (adj.size())] + " " + noun[r.nextInt (noun.size())];
        if (used.insert (n).second) return n;
    }
    return "Patch " + juce::String (used.size());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outFile (juce::String (argc > 1 ? argv[1] : "factory.xml"));
    const int total = argc > 2 ? juce::String (argv[2]).getIntValue() : 300;
    const juce::File demoDir (juce::String (argc > 3 ? argv[3] : ""));
    struct Arch { const char* cat; int (*fn) (Gen&); int count; };
    const Arch arch[] = { { "Bass", bass, 30 }, { "Lead", lead, 30 }, { "Pluck & Bongo", pluck, 30 }, { "Bells & Metal", bell, 25 },
                          { "Pads & Drones", pad, 25 }, { "Keys", keys, 20 }, { "Sequences", sequence, 30 }, { "Rhythm", rhythm, 25 },
                          { "Generative", generative, 30 }, { "Spectral", spectral, 15 }, { "FX & Textures", fx, 20 }, { "Touch & Arp", touch, 20 } };
    juce::ValueTree lib ("WCFACTORY");
    std::set<juce::String> used;
    juce::Random names (248);
    int made = 0, seed = 1000, rejected = 0;
    for (auto& a : arch)
    {
        const int want = (int) std::lround ((double) a.count * total / 300.0);
        for (int i = 0; i < want; ++i)
        {
            bool done = false;
            for (int attempt = 0; attempt < 12 && ! done; ++attempt)
            {
                WcProcessor p;
                p.prepareToPlay (SR, 512);
                p.resetAll();
                Gen g { p, juce::Random (++seed) };
                const int mode = a.fn (g);
                // render: a held note (0), by itself (1), a held chord (2)
                auto render = [&] (std::vector<float>* keep)
                {
                    p.prepareToPlay (SR, 512);
                    const int blocks = (int) (SR * 3.0 / 512.0);
                    double sum = 0.0; float peak = 0.0f; long n = 0;
                    for (int b = 0; b < blocks; ++b)
                    {
                        juce::AudioBuffer<float> buf (2, 512); buf.clear();
                        juce::MidiBuffer midi;
                        if (b == 0 && mode != 1)
                        {
                            if (mode == 2) for (int nn : { 48, 51, 55, 58 }) midi.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 100), 0);
                            else midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
                        }
                        if (b == (int) (SR * 1.6 / 512.0) && mode != 1)
                            for (int nn : { 48, 51, 55, 58 }) midi.addEvent (juce::MidiMessage::noteOff (1, nn), 0);
                        p.processBlock (buf, midi);
                        for (int ch = 0; ch < 2; ++ch)
                            for (int s = 0; s < 512; ++s)
                            {
                                const float v = buf.getSample (ch, s);
                                peak = juce::jmax (peak, std::fabs (v));
                                if (b > 8) { sum += (double) v * v; ++n; }
                                if (keep != nullptr && ch == 0) keep->push_back (v), keep->push_back (buf.getSample (1, s));
                            }
                    }
                    return std::make_pair (peak, (float) std::sqrt (sum / (double) juce::jmax (1L, n)));
                };
                auto [peak, rms] = render (nullptr);
                if (peak < 1.0e-4f || rms < 3.0e-4f || ! std::isfinite (peak)) { ++rejected; continue; }
                // level: peak to -4 dBFS with the master (-48 .. +6 dB)
                const float master = p.apvts.getRawParameterValue ("out_master")->load();
                const float target = juce::jlimit (-48.0f, 6.0f, master + (-4.0f - juce::Decibels::gainToDecibels (peak)));
                g.sp ("out_master", target);
                std::vector<float> wav;
                auto [peak2, rms2] = render (demoDir.getFullPathName().isNotEmpty() ? &wav : nullptr);
                if (peak2 > 0.99f || peak2 < 0.05f || rms2 < 0.002f) { ++rejected; continue; }
                const juce::String name = makeName (names, a.cat, used);
                auto pt = p.presetToTree (name, a.cat);
                pt.setProperty ("pk", peak2, nullptr);
                lib.addChild (pt, -1, nullptr);
                ++made; done = true;
                std::printf ("%3d %-15s %-24s peak %.2f rms %5.1f dB master %+5.1f\n", made, a.cat, name.toRawUTF8(), peak2,
                             juce::Decibels::gainToDecibels (rms2), target);
                if (! wav.empty() && i < 2)
                {
                    juce::AudioBuffer<float> b (2, (int) wav.size() / 2);
                    for (int s = 0; s < b.getNumSamples(); ++s) { b.setSample (0, s, wav[(size_t) s * 2]); b.setSample (1, s, wav[(size_t) s * 2 + 1]); }
                    auto f = demoDir.getChildFile (juce::String (made).paddedLeft ('0', 3) + " " + a.cat + " - " + name + ".wav");
                    demoDir.createDirectory(); f.deleteFile();
                    juce::WavAudioFormat fmt;
                    if (auto* os = new juce::FileOutputStream (f); os->openedOk())
                        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (fmt.createWriterFor (os, SR, 2, 24, {}, 0))) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
                }
            }
            if (! done) std::printf ("  !! %s %d could not be made\n", a.cat, i);
        }
    }
    std::printf ("made %d presets, %d attempts rejected\n", made, rejected);
    if (auto xml = lib.createXml()) { outFile.deleteFile(); xml->writeTo (outFile); }
    std::printf ("wrote %s (%lld bytes)\n", outFile.getFullPathName().toRawUTF8(), (long long) outFile.getSize());
    // verify: every preset reloaded from the written file into a fresh plug-in sounds like it did
    int bad = 0;
    if (auto xml = juce::parseXML (outFile))
    {
        auto back = juce::ValueTree::fromXml (*xml);
        for (auto pr : back)
        {
            WcProcessor q;
            q.prepareToPlay (SR, 512);
            q.loadPresetTree (pr);
            q.prepareToPlay (SR, 512);
            float pk = 0.0f;
            for (int b = 0; b < (int) (SR * 3.0 / 512.0); ++b)
            {
                juce::AudioBuffer<float> buf (2, 512); buf.clear();
                juce::MidiBuffer midi;
                if (b == 0) for (int nn : { 48, 51, 55, 58 }) midi.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 100), 0);
                if (b == (int) (SR * 1.6 / 512.0)) for (int nn : { 48, 51, 55, 58 }) midi.addEvent (juce::MidiMessage::noteOff (1, nn), 0);
                q.processBlock (buf, midi);
                pk = juce::jmax (pk, buf.getMagnitude (0, 512));
            }
            const float ref = (float) pr["pk"];
            if (! (pk > 0.03f && pk < 1.2f)) { ++bad; std::printf ("  !! reload %s / %s peak %.3f (was %.3f)\n", pr["cat"].toString().toRawUTF8(), pr["name"].toString().toRawUTF8(), pk, ref); }
        }
        std::printf ("verify: %d presets reloaded, %d bad\n", back.getNumChildren(), bad);
    }
    return made == total && bad == 0 ? 0 : 1;
}
