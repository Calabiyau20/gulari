// Builds the "All Modules" preset pack:  WcPackGen <out dir> [count]
//
// The 300-preset factory library grew around the modules that were there when it was written, and it
// shows: eight slots never appear in it at all (the 230, the 257, the 207, the second 281, the second
// 258, the third and fourth 259, the second 292), and the five modules that arrived with 1.0 - 267,
// 275, 284, 291 and the SC-666 - could not appear, because they did not exist yet.
//
// This pack is built the other way round: each archetype is BUILT ON one of those modules, so the
// module is not merely present but actually doing the work. Every preset is rendered, checked for
// silence and clipping, levelled with the output master, then written as its own .wcpatch file.
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
#include "modules/M267.h"
#include "modules/M275.h"
#include "modules/M284.h"
#include "modules/M291.h"
#include "modules/M666.h"
#include "modules/M252.h"
#include "modules/M264.h"
#include <map>
#include <set>
#include <cstdio>
#include <tuple>
#include <vector>

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
    bool conTry (int si, int sj, int di, int dj) { return p.connect (si, sj, di, dj); }
    void con (int si, int sj, int di, int dj) { if (! p.connect (si, sj, di, dj)) std::printf ("  !! cable %d.%d -> %d.%d refused\n", si, sj, di, dj); }
    static float hz (float f) { return coarseNorm (f, false); }
    static float hz258 (float f) { return std::log (f / 5.0f) / std::log (4000.0f); }

    // a 259 voice, straight out of the factory generator: it is the sound this rack is known for
    void voice259 (int inst, float baseHz, float ratio, float idx, float timbre, bool kbd = true)
    {
        on (inst);
        const juce::String a (prefix259 (inst - kInst259));
        sp (a + "po_freq", hz (baseHz)); sp (a + "mo_freq", hz (baseHz * ratio));
        sp (a + "idx", idx); sp (a + "mod_pitch", idx > 0.01f ? 1.0f : 0.0f);
        sp (a + "timbre", timbre); sp (a + "sym", rr (0.35f, 0.65f));
        sp (a + "mo_wave", (float) ri (0, 2));
        if (kbd) { con (kInstMidi, MI::Out_Pitch, inst, M259::In_PoPitch); con (kInstMidi, MI::Out_Pitch2, inst, M259::In_MoPitch); }
    }
    juce::String p259 (int inst) { return prefix259 (inst - kInst259); }

    // a 292 lowpass gate opened by a 281 function generator, the usual way a voice gets its shape
    void gate (int lpg, int lane, int fg, int fgLane, int src259)
    {
        on (lpg); on (fg);
        const juce::String l (prefix292 (lpg - kInst292)), f (prefix281 (fg - kInst281));
        const juce::String ln = juce::String::charToString ((juce::juce_wchar) ('1' + lane));
        const juce::String fn = juce::String::charToString ((juce::juce_wchar) ('1' + fgLane));
        sp (l + "mode" + ln, (float) pick ({ 0, 1, 2 }));
        sp (l + "off" + ln, rr (0.0f, 0.15f));
        sp (f + "att" + fn, rr (0.005f, 0.2f)); sp (f + "dec" + fn, rr (0.15f, 0.8f));
        con (kInstMidi, MI::Out_Gate, fg, M281::inTrig (fgLane));
        con (fg, M281::outFunc (fgLane), lpg, M292::inCtl (lane));
        con (src259, M259::Out_Final1, lpg, M292::inSig (lane));
        con (lpg, M292::outSig (lane), kInstOutput, OutputModule::In_Left);
    }

    // the scope: it listens, it never changes the sound. Cheap to add and nice to see on a new patch.
    void scope (int which, int srcInst, int srcJack)
    {
        const int inst = kInst666 + which;
        on (inst);
        const juce::String s (prefix666 (which));
        sp (s + "power", 1.0f); sp (s + "level", rr (0.35f, 0.75f));
        sp (s + "time", rr (0.25f, 0.6f)); sp (s + "intensity", rr (0.5f, 0.85f));
        sp (s + "focus", rr (0.4f, 0.8f)); sp (s + "src", 0.0f);
        if (! conTry (srcInst, srcJack, inst, M666::In_A))
            conTry (srcInst, srcJack, inst, M666::In_VertPos);
    }
};

// =================================================================================================
// The archetypes. Each one is named after the module it is built on, and must use it for real.
// Return value is the render mode: 0 = a held note, 1 = it plays by itself, 2 = a held chord.
// =================================================================================================

// 230 - the envelope follower. A voice drives it, and what it hears opens a gate on a second voice.
static int follower (Gen& g)
{
    const int v1 = kInst259, v2 = kInst259 + 1;
    g.voice259 (v1, g.rr (70.0f, 180.0f), g.pick ({ 1.0f, 1.5f, 2.0f }), g.rr (0.1f, 0.5f), g.rr (0.3f, 0.7f));
    g.voice259 (v2, g.rr (300.0f, 900.0f), g.pick ({ 2.0f, 3.0f, 4.01f }), g.rr (0.2f, 0.7f), g.rr (0.4f, 0.9f));
    g.on (kInst230);
    g.sp ("ef_sens1", g.rr (0.45f, 0.8f)); g.sp ("ef_decay1", g.rr (0.15f, 0.5f));
    g.sp ("ef_mode1", g.chance (0.5f) ? 1.0f : 0.0f);
    g.sp ("ef_sens2", g.rr (0.4f, 0.75f)); g.sp ("ef_decay2", g.rr (0.3f, 0.7f));
    g.con (v1, M259::Out_Final1, kInst230, M230::In_Audio1);
    // what the follower hears becomes the gate of the second voice, and bends its timbre
    g.on (kInst292); g.on (kInst281);
    g.sp ("la_mode1", 0.0f); g.sp ("la_off1", 0.0f);
    g.con (kInst230, M230::Out_Env1, kInst292, M292::inCtl (0));
    g.con (v2, M259::Out_Final1, kInst292, M292::inSig (0));
    g.con (kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left);
    g.con (kInst230, M230::Out_EnvB1, v2, M259::In_TimbreCv);
    // the first voice goes out through its own gate so you hear both
    g.gate (kInst292, 1, kInst281, 0, v1);
    if (g.chance (0.6f)) g.scope (0, kInst230, M230::Out_Env1);
    return 0;
}

// 257 - the voltage processor. It attenuverts, offsets and crossfades c.v. before it reaches a voice.
static int processor (Gen& g)
{
    const int v = kInst259;
    g.voice259 (v, g.rr (90.0f, 320.0f), g.pick ({ 1.0f, 2.0f, 3.0f }), g.rr (0.1f, 0.6f), g.rr (0.3f, 0.8f));
    g.on (kInst257); g.on (kInst266);
    g.sp ("vp_k1", g.rr (-0.9f, 0.9f));              // attenuvert: the inversion is the point
    g.sp ("vp_off1", g.rr (0.1f, 0.6f));
    g.sp ("vp_m1", g.rr (0.0f, 1.0f));               // crossfade between the two inputs
    g.sp ("vp_k2", g.rr (-0.8f, 0.8f)); g.sp ("vp_off2", g.rr (0.0f, 0.4f));
    g.sp ("ua_rate", g.rr (0.2f, 0.7f));
    g.con (kInst266, M266::Out_Fluct1, kInst257, M257::In_Va1);
    g.con (kInst266, M266::Out_Fluct2, kInst257, M257::In_Vb1);
    g.con (kInst257, M257::Out_1, v, M259::In_TimbreCv);
    g.con (kInst266, M266::Out_Sh, kInst257, M257::In_Va2);
    g.con (kInst257, M257::Out_2, v, M259::In_IndexCv);
    g.gate (kInst292, 0, kInst281, 0, v);
    if (g.chance (0.5f)) g.scope (0, kInst257, M257::Out_1);
    return 0;
}

// 207 - the mixer. Several voices arrive at it, and it, not the output, does the balancing.
static int mixer (Gen& g)
{
    const int v1 = kInst259, v2 = kInst259 + 1, v3 = kInst259 + 2;
    g.voice259 (v1, g.rr (60.0f, 130.0f), 1.0f, g.rr (0.0f, 0.3f), g.rr (0.2f, 0.5f));
    g.voice259 (v2, g.rr (200.0f, 500.0f), g.pick ({ 1.5f, 2.0f }), g.rr (0.2f, 0.6f), g.rr (0.4f, 0.8f));
    g.voice259 (v3, g.rr (700.0f, 1600.0f), g.pick ({ 3.0f, 4.01f, 5.0f }), g.rr (0.3f, 0.8f), g.rr (0.5f, 0.95f));
    g.on (kInst207);
    const char* ch[] = { "a", "b", "c" };
    const int src[] = { v1, v2, v3 };
    for (int i = 0; i < 3; ++i)
    {
        g.sp (juce::String ("mx_lvl_") + ch[i], g.rr (0.45f, 0.9f));
        g.sp (juce::String ("mx_prog_") + ch[i], 1.0f);
        g.con (src[i], M259::Out_Final1, kInst207, M207::In_A + i);
    }
    g.sp ("mx_pan1", g.rr (0.1f, 0.4f)); g.sp ("mx_pan2", g.rr (0.35f, 0.65f)); g.sp ("mx_pan3", g.rr (0.6f, 0.9f));
    g.con (kInst207, M207::Out_L, kInstOutput, OutputModule::In_Left);
    g.con (kInst207, M207::Out_R, kInstOutput, OutputModule::In_Right);
    // one 281 lane per voice, so the chord has movement instead of sitting still
    g.on (kInst281);
    for (int i = 0; i < 3; ++i)
    {
        const juce::String n = juce::String::charToString ((juce::juce_wchar) ('1' + i));
        g.sp ("fa_att" + n, g.rr (0.01f, 0.3f)); g.sp ("fa_dec" + n, g.rr (0.3f, 1.0f));
        g.con (kInstMidi, MI::Out_Gate, kInst281, M281::inTrig (i));
        g.con (kInst281, M281::outFunc (i), src[i], M259::In_IndexCv);
    }
    if (g.chance (0.6f)) g.scope (0, kInst207, M207::Out_L);
    return 2;
}

// 281 A + B - eight function generators at once, the complex envelope patch
static int eightLanes (Gen& g)
{
    const int v1 = kInst259, v2 = kInst259 + 1;
    g.voice259 (v1, g.rr (80.0f, 260.0f), g.pick ({ 1.0f, 2.0f, 3.0f }), g.rr (0.1f, 0.5f), g.rr (0.3f, 0.7f));
    g.voice259 (v2, g.rr (260.0f, 800.0f), g.pick ({ 2.0f, 4.01f }), g.rr (0.2f, 0.7f), g.rr (0.4f, 0.9f));
    g.on (kInst281); g.on (kInst281 + 1);
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String::charToString ((juce::juce_wchar) ('1' + i));
        g.sp ("fa_att" + n, g.rr (0.005f, 0.25f)); g.sp ("fa_dec" + n, g.rr (0.1f, 0.9f));
        g.sp ("fb_att" + n, g.rr (0.05f, 0.6f));  g.sp ("fb_dec" + n, g.rr (0.3f, 1.0f));
        if (g.chance (0.4f)) g.sp ("fb_mode" + n, 2.0f);      // B free-running: it keeps moving
    }
    g.con (kInstMidi, MI::Out_Gate, kInst281, M281::inTrig (0));
    g.con (kInstMidi, MI::Out_Gate, kInst281, M281::inTrig (1));
    g.con (kInst281 + 1, M281::outFunc (0), v1, M259::In_TimbreCv);
    g.con (kInst281 + 1, M281::outFunc (1), v2, M259::In_IndexCv);
    g.con (kInst281 + 1, M281::outFunc (2), v1, M259::In_IndexCv);
    g.gate (kInst292, 0, kInst281, 2, v1);
    g.gate (kInst292, 1, kInst281, 3, v2);
    if (g.chance (0.5f)) g.scope (0, kInst281 + 1, M281::outFunc (0));
    return 0;
}

// 258 A + B - the two simple oscillators, detuned against each other
static int twoSimple (Gen& g)
{
    g.on (kInst258); g.on (kInst258 + 1);
    const float base = g.rr (70.0f, 240.0f);
    const float det = g.rr (0.004f, 0.02f);
    const juce::String a (prefix258 (0)), b (prefix258 (1));
    g.sp (a + "freq1", Gen::hz258 (base));              g.sp (a + "shape1", g.rr (0.0f, 1.0f));
    g.sp (a + "freq2", Gen::hz258 (base * (1 + det)));  g.sp (a + "shape2", g.rr (0.0f, 1.0f));
    g.sp (b + "freq1", Gen::hz258 (base * 2.0f * (1 - det))); g.sp (b + "shape1", g.rr (0.0f, 1.0f));
    g.sp (b + "freq2", Gen::hz258 (base * g.pick ({ 1.5f, 3.0f, 4.0f }))); g.sp (b + "shape2", g.rr (0.0f, 1.0f));
    for (int o = 0; o < 2; ++o)
    {
        const int inst = kInst258 + o;
        g.con (kInstMidi, MI::Out_Pitch, inst, M258::In_Kbd1);
        g.con (kInstMidi, MI::Out_Pitch2, inst, M258::In_Kbd2);
    }
    g.on (kInst292); g.on (kInst281);
    for (int lane = 0; lane < 4; ++lane)
    {
        const juce::String ln = juce::String::charToString ((juce::juce_wchar) ('1' + lane));
        g.sp ("la_mode" + ln, (float) g.pick ({ 0, 1 })); g.sp ("la_off" + ln, 0.0f);
    }
    g.sp ("fa_att1", g.rr (0.01f, 0.2f)); g.sp ("fa_dec1", g.rr (0.3f, 0.9f));
    g.con (kInstMidi, MI::Out_Gate, kInst281, M281::inTrig (0));
    for (int lane = 0; lane < 4; ++lane)
    {
        const int o = kInst258 + lane / 2;
        g.con (o, lane % 2 == 0 ? M258::Out_1 : M258::Out_2, kInst292, M292::inSig (lane));
        g.con (kInst281, M281::outFunc (0), kInst292, M292::inCtl (lane));
        g.con (kInst292, M292::outSig (lane), kInstOutput, lane % 2 == 0 ? OutputModule::In_Left : OutputModule::In_Right);
    }
    if (g.chance (0.5f)) g.scope (0, kInst258, M258::Out_1);
    return 0;
}

// 259 A B C D - the full four-voice stack
static int fourVoices (Gen& g)
{
    const float root = g.rr (55.0f, 150.0f);
    const float iv[4] = { 1.0f, g.pick ({ 1.5f, 1.498f }), 2.0f, g.pick ({ 2.5f, 3.0f }) };
    for (int i = 0; i < 4; ++i)
        g.voice259 (kInst259 + i, root * iv[i], g.pick ({ 1.0f, 2.0f, 3.0f, 4.01f }), g.rr (0.05f, 0.6f), g.rr (0.25f, 0.9f));
    g.on (kInst292); g.on (kInst292 + 1); g.on (kInst281); g.on (kInst281 + 1);
    for (int i = 0; i < 4; ++i)
    {
        const int lpg = kInst292 + i / 2, lane = i % 2;
        const int fg = kInst281 + i / 2, fgl = i % 2;
        const juce::String l (prefix292 (i / 2)), f (prefix281 (i / 2));
        const juce::String ln = juce::String::charToString ((juce::juce_wchar) ('1' + lane));
        const juce::String fn = juce::String::charToString ((juce::juce_wchar) ('1' + fgl));
        g.sp (l + "mode" + ln, (float) g.pick ({ 0, 1, 2 })); g.sp (l + "off" + ln, 0.0f);
        g.sp (f + "att" + fn, g.rr (0.02f, 0.5f)); g.sp (f + "dec" + fn, g.rr (0.3f, 1.0f));
        g.con (kInstMidi, MI::Out_Gate, fg, M281::inTrig (fgl));
        g.con (fg, M281::outFunc (fgl), lpg, M292::inCtl (lane));
        g.con (kInst259 + i, M259::Out_Final1, lpg, M292::inSig (lane));
        g.con (lpg, M292::outSig (lane), kInstOutput, i % 2 == 0 ? OutputModule::In_Left : OutputModule::In_Right);
    }
    if (g.chance (0.6f)) g.scope (0, kInst259, M259::Out_Final1);
    return 2;
}

// 267 - the new uncertainty source, feeding its own filters and the voice
static int uncertainty (Gen& g)
{
    const int v = kInst259;
    g.voice259 (v, g.rr (90.0f, 400.0f), g.pick ({ 1.0f, 2.0f, 3.0f }), g.rr (0.1f, 0.6f), g.rr (0.3f, 0.8f));
    g.on (kInst267);
    g.sp ("us_ratea", g.rr (0.25f, 0.8f)); g.sp ("us_randa", g.rr (0.3f, 0.95f));
    g.sp ("us_freqa", g.rr (0.2f, 0.8f));  g.sp ("us_bwa", g.rr (0.15f, 0.7f));
    g.sp ("us_idxa", g.rr (0.3f, 0.9f));   g.sp ("us_moda", g.rr (0.2f, 0.8f));
    g.sp ("us_rateb", g.rr (0.1f, 0.6f));  g.sp ("us_randb", g.rr (0.2f, 0.9f));
    g.sp ("us_freqb", g.rr (0.3f, 0.85f)); g.sp ("us_bwb", g.rr (0.2f, 0.75f));
    // its noise through its own filter, and its fluctuating voltage onto the voice
    g.con (kInst267, M267::Out_Flat1, kInst267, M267::In_SigA);
    g.con (kInst267, M267::Out_SigA1, kInst292, M292::inSig (1));
    g.con (kInst267, M267::Out_ShA, v, M259::In_TimbreCv);
    g.con (kInst267, M267::Out_MixB, v, M259::In_IndexCv);
    g.on (kInst292); g.on (kInst281);
    g.sp ("la_mode2", 0.0f); g.sp ("la_off2", g.rr (0.0f, 0.1f));
    g.sp ("fa_att2", g.rr (0.05f, 0.4f)); g.sp ("fa_dec2", g.rr (0.3f, 0.9f));
    g.con (kInstMidi, MI::Out_Gate, kInst281, M281::inTrig (1));
    g.con (kInst281, M281::outFunc (1), kInst292, M292::inCtl (1));
    g.con (kInst292, M292::outSig (1), kInstOutput, OutputModule::In_Right);
    g.gate (kInst292, 0, kInst281, 0, v);
    if (g.chance (0.6f)) g.scope (0, kInst267, M267::Out_SigA1);
    return 0;
}

// 291 - the new triple morphing filter, with the voice running through it
static int morph (Gen& g)
{
    const int v = kInst259;
    g.voice259 (v, g.rr (70.0f, 260.0f), g.pick ({ 1.0f, 2.0f }), g.rr (0.15f, 0.7f), g.rr (0.4f, 0.95f));
    g.on (kInst291);
    g.sp ("tm_play", 1.0f);
    g.sp ("tm_index", g.rr (0.15f, 0.85f));
    g.sp ("tm_modtype", (float) g.ri (0, 1));
    for (int s = 1; s <= 4; ++s)
    {
        const juce::String sn (s);
        g.sp ("tm_time_" + sn, g.rr (0.2f, 0.7f));
        for (const char* c : { "a", "b", "c" })
        {
            g.sp ("tm_f_" + sn + "_" + c, g.rr (0.1f, 0.9f));
            g.sp ("tm_bw_" + sn + "_" + c, g.rr (0.1f, 0.6f));
            g.sp ("tm_amp_" + sn + "_" + c, g.rr (0.4f, 1.0f));
        }
    }
    g.con (v, M259::Out_Final1, kInst291, M291::In_SigAll);
    g.con (kInst291, M291::Out_All1, kInst292, M292::inSig (0));
    g.on (kInst292); g.on (kInst281);
    g.sp ("la_mode1", (float) g.pick ({ 0, 1 })); g.sp ("la_off1", 0.0f);
    g.sp ("fa_att1", g.rr (0.01f, 0.25f)); g.sp ("fa_dec1", g.rr (0.3f, 1.0f));
    g.con (kInstMidi, MI::Out_Gate, kInst281, M281::inTrig (0));
    g.con (kInst281, M281::outFunc (0), kInst292, M292::inCtl (0));
    g.con (kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left);
    if (g.chance (0.5f)) g.con (kInst281, M281::outFunc (0), kInst291, M291::In_FreqA);
    if (g.chance (0.6f)) g.scope (0, kInst291, M291::Out_All1);
    return 0;
}

// 284 + 275 - the new quad envelope driving the voices, into the new equaliser and reverb
static int roomTone (Gen& g)
{
    const int v1 = kInst259, v2 = kInst259 + 1;
    g.voice259 (v1, g.rr (80.0f, 300.0f), g.pick ({ 1.0f, 2.0f, 3.0f }), g.rr (0.1f, 0.5f), g.rr (0.3f, 0.8f));
    g.voice259 (v2, g.rr (300.0f, 900.0f), g.pick ({ 2.0f, 4.01f }), g.rr (0.2f, 0.7f), g.rr (0.4f, 0.9f));
    g.on (kInst284);
    for (int c = 1; c <= 4; ++c)
    {
        const juce::String k (c);
        g.sp ("ev_att" + k, g.rr (0.01f, 0.35f));
        g.sp ("ev_dec" + k, g.rr (0.15f, 0.7f));
        g.sp ("ev_sus" + k, g.rr (0.25f, 0.85f));
        g.sp ("ev_dur" + k, g.rr (0.2f, 0.7f));
        g.con (kInstMidi, MI::Out_Gate, kInst284, M284::inTrig (c - 1));
    }
    g.on (kInst292);
    g.sp ("la_mode1", 0.0f); g.sp ("la_off1", 0.0f);
    g.sp ("la_mode2", 0.0f); g.sp ("la_off2", 0.0f);
    g.con (kInst284, M284::outEnv (0, 0), kInst292, M292::inCtl (0));
    g.con (kInst284, M284::outEnv (1, 0), kInst292, M292::inCtl (1));
    g.con (v1, M259::Out_Final1, kInst292, M292::inSig (0));
    g.con (v2, M259::Out_Final1, kInst292, M292::inSig (1));
    g.con (kInst284, M284::outEnv (2, 0), v2, M259::In_TimbreCv);
    // into the 275: equaliser, then the room
    g.on (kInst275);
    g.sp ("rv_treb1", g.rr (0.35f, 0.75f)); g.sp ("rv_bass1", g.rr (0.3f, 0.8f));
    g.sp ("rv_mix1", g.rr (0.25f, 0.7f));   g.sp ("rv_rec1", g.rr (0.35f, 0.85f));
    g.sp ("rv_treb2", g.rr (0.3f, 0.7f));   g.sp ("rv_bass2", g.rr (0.35f, 0.75f));
    g.sp ("rv_mix2", g.rr (0.2f, 0.6f));    g.sp ("rv_rec2", g.rr (0.4f, 0.9f));
    g.con (kInst292, M292::outSig (0), kInst275, M275::In_Audio1);
    g.con (kInst292, M292::outSig (1), kInst275, M275::In_Audio2);
    g.con (kInst275, M275::Out_Mix1a, kInstOutput, OutputModule::In_Left);
    g.con (kInst275, M275::Out_Mix2a, kInstOutput, OutputModule::In_Right);
    if (g.chance (0.6f)) g.scope (0, kInst275, M275::Out_Mix1a);
    return 0;
}

// Four SC-666 at once, each watching a different corner of a patch that plays by itself
static int fourScopes (Gen& g)
{
    const int v1 = kInst259, v2 = kInst259 + 1;
    g.voice259 (v1, g.rr (60.0f, 200.0f), g.pick ({ 1.0f, 2.0f }), g.rr (0.1f, 0.6f), g.rr (0.3f, 0.8f), false);
    g.voice259 (v2, g.rr (200.0f, 700.0f), g.pick ({ 1.5f, 3.0f }), g.rr (0.2f, 0.7f), g.rr (0.4f, 0.9f), false);
    g.on (kInst266); g.on (kInst281);
    g.sp ("ua_rate", g.rr (0.3f, 0.8f)); g.sp ("ua_rate2", g.rr (0.15f, 0.6f));
    g.sp ("fa_att1", g.rr (0.02f, 0.3f)); g.sp ("fa_dec1", g.rr (0.2f, 0.8f)); g.sp ("fa_mode1", 2.0f);
    g.sp ("fa_att2", g.rr (0.05f, 0.5f)); g.sp ("fa_dec2", g.rr (0.3f, 1.0f)); g.sp ("fa_mode2", 2.0f);
    g.con (kInst266, M266::Out_Fluct1, v1, M259::In_TimbreCv);
    g.con (kInst266, M266::Out_Fluct2, v2, M259::In_IndexCv);
    g.on (kInst292);
    g.sp ("la_mode1", 0.0f); g.sp ("la_off1", 0.0f);
    g.sp ("la_mode2", 0.0f); g.sp ("la_off2", 0.0f);
    g.con (kInst281, M281::outFunc (0), kInst292, M292::inCtl (0));
    g.con (kInst281, M281::outFunc (1), kInst292, M292::inCtl (1));
    g.con (v1, M259::Out_Final1, kInst292, M292::inSig (0));
    g.con (v2, M259::Out_Final1, kInst292, M292::inSig (1));
    g.con (kInst292, M292::outSig (0), kInstOutput, OutputModule::In_Left);
    g.con (kInst292, M292::outSig (1), kInstOutput, OutputModule::In_Right);
    g.scope (0, kInst292, M292::outSig (0));
    g.scope (1, v2, M259::Out_Final1);
    g.scope (2, kInst281, M281::outFunc (0));
    g.scope (3, kInst266, M266::Out_Fluct1);
    return 1;
}

// The whole rack at once: every slot in the machine, patched into one piece that plays by itself
static int everything (Gen& g)
{
    // four voices
    const float root = g.rr (55.0f, 120.0f);
    for (int i = 0; i < 4; ++i)
        g.voice259 (kInst259 + i, root * g.pick ({ 1.0f, 1.5f, 2.0f, 3.0f }), g.pick ({ 1.0f, 2.0f, 3.0f }),
                    g.rr (0.05f, 0.5f), g.rr (0.25f, 0.85f), false);
    // both simple oscillators
    g.on (kInst258); g.on (kInst258 + 1);
    g.sp (juce::String (prefix258 (0)) + "freq1", Gen::hz258 (root * 2.0f));
    g.sp (juce::String (prefix258 (1)) + "freq1", Gen::hz258 (root * 3.01f));
    // the control sources
    g.on (kInst218); g.on (kInst242); g.on (kInst245); g.on (kInst248); g.on (kInst250); g.on (kInst252);
    g.on (kInst266); g.on (kInst264); g.on (kInst230); g.on (kInst257);
    g.sp ("ua_rate", g.rr (0.3f, 0.8f)); g.sp ("ua_rate2", g.rr (0.2f, 0.7f));
    g.sp ("pp_a1", g.rr (0.3f, 0.75f)); g.sp ("pp_b1", g.rr (0.2f, 0.6f));
    g.sp ("sv_time", g.rr (0.3f, 0.7f)); g.sp ("sv_len", g.rr (0.4f, 1.0f));
    g.sp ("vp_k1", g.rr (-0.8f, 0.8f)); g.sp ("vp_off1", g.rr (0.1f, 0.5f));
    g.sp ("ef_sens1", g.rr (0.4f, 0.8f)); g.sp ("ef_decay1", g.rr (0.2f, 0.6f));
    // both function generators, free running so it moves on its own
    g.on (kInst281); g.on (kInst281 + 1); g.on (kInst284);
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String::charToString ((juce::juce_wchar) ('1' + i));
        g.sp ("fa_att" + n, g.rr (0.01f, 0.3f)); g.sp ("fa_dec" + n, g.rr (0.2f, 0.9f)); g.sp ("fa_mode" + n, 2.0f);
        g.sp ("fb_att" + n, g.rr (0.05f, 0.5f)); g.sp ("fb_dec" + n, g.rr (0.3f, 1.0f)); g.sp ("fb_mode" + n, 2.0f);
        g.sp ("ev_att" + n, g.rr (0.02f, 0.3f)); g.sp ("ev_dec" + n, g.rr (0.2f, 0.7f)); g.sp ("ev_sus" + n, g.rr (0.3f, 0.8f));
    }
    // the processors
    g.on (kInst285); g.on (kInst296); g.on (kInst277); g.on (kInst267); g.on (kInst291); g.on (kInst275);
    g.sp ("fs_ctl", g.rr (0.42f, 0.58f)); g.sp ("fs_amp", g.rr (0.3f, 0.8f));
    g.sp ("dl_time", g.rr (0.2f, 0.65f)); g.sp ("dl_mod", g.rr (0.1f, 0.5f)); g.sp ("dl_lvla", g.rr (0.3f, 0.7f));
    g.sp ("us_ratea", g.rr (0.3f, 0.8f)); g.sp ("us_randa", g.rr (0.3f, 0.9f));
    g.sp ("tm_play", 1.0f); g.sp ("tm_index", g.rr (0.2f, 0.8f));
    g.sp ("rv_mix1", g.rr (0.2f, 0.55f)); g.sp ("rv_rec1", g.rr (0.4f, 0.8f));
    g.sp ("rv_mix2", g.rr (0.2f, 0.55f)); g.sp ("rv_rec2", g.rr (0.4f, 0.8f));
    // both gate banks and the mixer
    g.on (kInst292); g.on (kInst292 + 1); g.on (kInst207);
    for (int i = 0; i < 4; ++i)
    {
        const int lpg = kInst292 + i / 2, lane = i % 2;
        const juce::String l (prefix292 (i / 2));
        const juce::String ln = juce::String::charToString ((juce::juce_wchar) ('1' + lane));
        g.sp (l + "mode" + ln, (float) g.pick ({ 0, 1, 2 })); g.sp (l + "off" + ln, 0.0f);
        g.con (kInst259 + i, M259::Out_Final1, lpg, M292::inSig (lane));
        g.con (kInst281 + i / 2, M281::outFunc (lane), lpg, M292::inCtl (lane));
    }
    const char* ch[] = { "a", "b", "c", "d" };
    for (int i = 0; i < 4; ++i)
    {
        g.sp (juce::String ("mx_lvl_") + ch[i], g.rr (0.4f, 0.8f));
        g.sp (juce::String ("mx_prog_") + ch[i], 1.0f);
        g.con (kInst292 + i / 2, M292::outSig (i % 2), kInst207, M207::In_A + i);
    }
    g.sp ("mx_pan1", 0.15f); g.sp ("mx_pan2", 0.4f); g.sp ("mx_pan3", 0.6f); g.sp ("mx_pan4", 0.85f);
    // the chain into the room, and out
    g.con (kInst207, M207::Out_L, kInst275, M275::In_Audio1);
    g.con (kInst207, M207::Out_R, kInst275, M275::In_Audio2);
    g.con (kInst275, M275::Out_Mix1a, kInstOutput, OutputModule::In_Left);
    g.con (kInst275, M275::Out_Mix2a, kInstOutput, OutputModule::In_Right);
    // the modulation web
    g.con (kInst266, M266::Out_Fluct1, kInst257, M257::In_Va1);
    g.con (kInst257, M257::Out_1, kInst259, M259::In_TimbreCv);
    g.con (kInst266, M266::Out_Sh, kInst259 + 1, M259::In_IndexCv);
    g.con (kInst292 + 1, M292::outSig (1), kInst230, M230::In_Audio2);
    g.con (kInst230, M230::Out_Env1, kInst259 + 2, M259::In_TimbreCv);
    g.con (kInst281 + 1, M281::outFunc (0), kInst267, M267::In_FreqA);
    g.con (kInst258 + 1, M258::Out_2, kInst291, M291::In_SigAll);
    g.con (kInst291, M291::Out_All1, kInst207, M207::In_E);
    g.sp ("mx_lvl_e", g.rr (0.3f, 0.6f)); g.sp ("mx_prog_e", 1.0f);
    // all four scopes watching
    g.scope (0, kInst207, M207::Out_L);
    g.scope (1, kInst259, M259::Out_Final1);
    g.scope (2, kInst281, M281::outFunc (0));
    g.scope (3, kInst267, M267::Out_SigA1);
    return 1;
}


// =================================================================================================
// POLYRHYTHM / POLYMETER
// Cycles of different lengths running against each other, and nobody playing a note: the rack keeps
// time by itself. Polyrhythm is several clocks dividing the same beat differently; polymeter is
// sequences of different LENGTHS over the same pulse. Both are built into this rack and almost
// nothing in the factory library uses them.
// =================================================================================================

// A handful of voices waiting to be driven. Returns nothing: the archetype wires the gates itself.
struct Voices
{
    int n = 0;
    int v[4] = { kInst259, kInst259 + 1, kInst259 + 2, kInst259 + 3 };
};

static Voices polyVoices (Gen& g, int count)
{
    Voices vs; vs.n = count;
    const float root = g.rr (55.0f, 130.0f);
    static const float iv[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
    for (int i = 0; i < count; ++i)
        g.voice259 (vs.v[i], root * iv[i] * g.rr (0.98f, 1.02f), g.pick ({ 1.0f, 2.0f, 3.0f }),
                    g.rr (0.05f, 0.55f), g.rr (0.25f, 0.9f), false);   // sin teclado: toca solo
    return vs;
}

// Un pulso abre una compuerta: ese es el latido que se escucha.
static void polyGate (Gen& g, int voice, int lane, int srcInst, int srcJack, float attMax = 0.05f)
{
    // Los cuatro golpes salen SIEMPRE de la primera unidad de generadores. La segunda queda entera
    // para modular: si una envolvente de golpe comparte carril con una que gira en ciclo, la voz
    // deja de tener silencios y el patron se convierte en un zumbido.
    const int lpg = kInst292 + lane / 2, lpgLane = lane % 2;
    const int fg = kInst281, fgLane = lane;
    g.on (lpg); g.on (fg);
    const juce::String l (prefix292 (lane / 2)), f (prefix281 (0));
    const juce::String ln = juce::String::charToString ((juce::juce_wchar) ('1' + lpgLane));
    const juce::String fn = juce::String::charToString ((juce::juce_wchar) ('1' + fgLane));
    g.sp (l + "mode" + ln, (float) g.pick ({ 0, 1, 2 })); g.sp (l + "off" + ln, 0.0f);
    g.sp (f + "att" + fn, g.rr (0.003f, attMax)); g.sp (f + "dec" + fn, g.rr (0.04f, 0.28f));
    g.sp (f + "mode" + fn, 0.0f);                       // transitorio: dispara y cae
    g.con (srcInst, srcJack, fg, M281::inTrig (fgLane));
    g.con (fg, M281::outFunc (fgLane), lpg, M292::inCtl (lpgLane));
    g.con (voice, M259::Out_Final1, lpg, M292::inSig (lpgLane));
    // Al mixer, no a la salida. Un cable nuevo a la misma entrada reemplaza al anterior, asi que
    // mandar cuatro voces a izquierda y derecha deja oir solo las dos ultimas.
    g.on (kInst207);
    static const char* ch[] = { "a", "b", "c", "d", "e", "f" };
    g.sp (juce::String ("mx_lvl_") + ch[lane], g.rr (0.45f, 0.85f));
    g.sp (juce::String ("mx_prog_") + ch[lane], 1.0f);
    g.sp ("mx_pan" + juce::String (lane + 1), g.rr (0.12f, 0.88f));
    g.con (lpg, M292::outSig (lpgLane), kInst207, M207::In_A + lane);
    g.con (kInst207, M207::Out_L, kInstOutput, OutputModule::In_Left);
    g.con (kInst207, M207::Out_R, kInstOutput, OutputModule::In_Right);
}

// 252 - tres relojes dividiendo el mismo pulso de maneras que no coinciden
static int threeClocks (Gen& g)
{
    auto vs = polyVoices (g, 3);
    g.on (kInst252);
    g.sp ("rg_play", 1.0f);
    g.sp ("rg_bpm", (float) g.ri (72, 160));
    g.sp ("rg_pdur", g.rr (0.05f, 0.2f));
    g.sp ("rg_sync", g.chance (0.5f) ? 1.0f : 0.0f);   // 1: polirritmia en el mismo ciclo; 0: polimetria
    // anillos que no se cruzan: 3 contra 4 contra 5, o 5 contra 7, etc.
    static const int trio[5][3] = { { 3, 4, 5 }, { 4, 6, 9 }, { 3, 5, 8 }, { 5, 7, 11 }, { 2, 3, 7 } };
    const int* r = trio[g.ri (0, 4)];
    for (int k = 0; k < 3; ++k)
    {
        const juce::String kk (k + 1);
        g.sp ("rg_clk" + kk, (float) r[k]);
        g.sp ("rg_dts" + kk, g.rr (0.8f, 1.6f));
        if (g.chance (0.35f)) g.sp ("rg_dt" + kk, 1.0f);      // delta-t: el reloj respira
    }
    if (g.chance (0.5f)) g.sp ("rg_euclid", 1.0f);
    g.sp ("rg_pattern", (float) g.ri (0, 16));
    // Las celdas del anillo no son parametros: viven adentro del modulo. Si no se programan, los
    // tres relojes giran sobre celdas vacias y no sale un solo pulso. Cada reloj pinta su anillo
    // de un color, asi que cada uno sale por su propia salida.
    if (auto* rg = g.p.rack.m252())
    {
        rg->clearAll();
        for (int k = 0; k < 3; ++k)
        {
            const int ring = juce::jlimit (0, M252::kRings - 1, r[k] - 1);
            const int n = M252::kSize[ring], b = M252::base (ring);
            const int bit = 1 << k;                       // 1 rojo, 2 verde, 4 azul
            const int hits = juce::jlimit (1, n, (int) std::lround (n * g.rr (0.35f, 0.8f)));
            // reparto euclidiano: los golpes quedan lo mas separados que se puede
            for (int i = 0; i < n; ++i)
                if (((i * hits) % n) < hits) rg->rgb[b + i].store (rg->rgb[b + i].load() | bit);
            // y una tension por celda para que la melodia se mueva con el ritmo
            for (int i = 0; i < n; ++i)
                for (int o = 0; o < 3; ++o)
                    rg->cv[b + i][o].store (g.chance (0.55f) ? g.rr (0.0f, 8.0f) : -1.0f);
            if (g.chance (0.3f)) rg->sub[b + g.ri (0, n - 1)].store (g.ri (1, 3));   // subdivisiones
        }
    }
    const int outs[3] = { M252::Out_R, M252::Out_G, M252::Out_B };
    for (int i = 0; i < 3; ++i) polyGate (g, vs.v[i], i, kInst252, outs[i]);
    // y las tensiones del 252 le mueven el timbre a cada voz
    g.con (kInst252, M252::Out_Cv1, vs.v[0], M259::In_TimbreCv);
    g.con (kInst252, M252::Out_Cv2, vs.v[1], M259::In_IndexCv);
    g.con (kInst252, M252::Out_Cv3, vs.v[2], M259::In_TimbreCv);
    if (g.chance (0.6f)) g.scope (0, kInst292, M292::outSig (0));
    return 1;
}

// 242 contra 250 - doce pasos de largo programable contra un ciclo de dieciseis etapas
static int twoCycles (Gen& g)
{
    auto vs = polyVoices (g, 3);
    // el pulsador: largo primo para que no caiga nunca en el mismo lugar
    g.on (kInst242);
    g.sp ("pp_len", (float) g.pick ({ 5, 7, 9, 11 }));
    g.sp ("pp_period", g.rr (0.18f, 0.42f));
    for (int s = 1; s <= 12; ++s)
    {
        const juce::String ss (s);
        g.sp ("pp_a" + ss, g.chance (0.45f) ? 1.0f : 0.0f);
        g.sp ("pp_b" + ss, g.chance (0.3f) ? 1.0f : 0.0f);
        g.sp ("pp_c" + ss, g.chance (0.22f) ? 1.0f : 0.0f);
    }
    // la secuencia de dieciseis, con tiempos distintos por etapa: su vuelta dura otra cosa
    g.on (kInst250);
    for (int s = 1; s <= 16; ++s)
    {
        const juce::String ss (s);
        g.sp ("sq_t_" + ss, g.rr (0.15f, 0.75f));
        g.sp ("sq_v1_" + ss, g.rr (0.0f, 1.0f));
        g.sp ("sq_v2_" + ss, g.rr (0.0f, 1.0f));
        g.sp ("sq_p1_" + ss, g.chance (0.4f) ? 1.0f : 0.0f);
        g.sp ("sq_p2_" + ss, g.chance (0.25f) ? 1.0f : 0.0f);
        g.sp ("sq_mode_" + ss, 0.0f);
        g.sp ("sq_trng_" + ss, (float) g.pick ({ 0, 1 }));
    }
    polyGate (g, vs.v[0], 0, kInst242, M242::Out_A);
    polyGate (g, vs.v[1], 1, kInst242, M242::Out_B);
    polyGate (g, vs.v[2], 2, kInst250, M250::Out_P1);
    g.con (kInst250, M250::Out_Cv1, vs.v[0], M259::In_TimbreCv);
    g.con (kInst250, M250::Out_Cv2, vs.v[2], M259::In_IndexCv);
    g.con (kInst242, M242::Out_Ramp, vs.v[1], M259::In_TimbreCv);
    if (g.chance (0.4f)) g.con (kInst242, M242::Out_C, kInst250, M250::In_AddrPulse);
    if (g.chance (0.6f)) g.scope (0, kInst292, M292::outSig (0));
    return 1;
}

// 245 + 264 - el secuenciador de cinco etapas, y las retenciones cuantizadas a una escala
static int fiveAndHold (Gen& g)
{
    auto vs = polyVoices (g, 3);
    g.on (kInst245); g.on (kInst264); g.on (kInst242);
    g.sp ("sv_time", g.rr (0.25f, 0.6f)); g.sp ("sv_len", g.rr (0.3f, 0.8f));
    for (int s = 1; s <= 5; ++s)
    {
        const juce::String ss (s);
        g.sp ("sv_a" + ss, g.rr (0.0f, 1.0f)); g.sp ("sv_b" + ss, g.rr (0.0f, 1.0f));
        g.sp ("sv_c" + ss, g.rr (0.0f, 1.0f)); g.sp ("sv_d" + ss, g.rr (0.0f, 1.0f));
    }
    g.sp ("pp_len", (float) g.pick ({ 5, 7, 8, 11 }));
    g.sp ("pp_period", g.rr (0.14f, 0.35f));
    for (int s = 1; s <= 12; ++s)
    {
        const juce::String ss (s);
        g.sp ("pp_a" + ss, g.chance (0.38f) ? 1.0f : 0.0f);
        g.sp ("pp_b" + ss, g.chance (0.26f) ? 1.0f : 0.0f);
        g.sp ("pp_c" + ss, g.chance (0.2f) ? 1.0f : 0.0f);
    }
    // cinco etapas avanzando con un pulso, doce pasos con otro: dos vueltas de largo distinto
    g.con (kInst242, M242::Out_A, kInst245, M245::In_Advance);
    g.sp ("sh_voices", 3.0f); g.sp ("sh_scale", (float) g.pick ({ 0, 2 }));
    g.sp ("sh_key", (float) g.ri (0, 11)); g.sp ("sh_offset", g.rr (-0.4f, 0.6f));
    const int svOut[3] = { M245::Out_A, M245::Out_B, M245::Out_C };
    const int pulses[3] = { M242::Out_A, M242::Out_B, M245::Out_Pulse };
    const int pulseSrc[3] = { kInst242, kInst242, kInst245 };
    for (int i = 0; i < 3; ++i)
    {
        g.con (kInst245, svOut[i], kInst264, M264::In_Cv1 + i);
        g.con (pulseSrc[i], pulses[i], kInst264, M264::In_Pulse1 + i);
        g.con (kInst264, M264::Out_Art1 + i, vs.v[i], M259::In_PoPitch);   // ya cuantizado a la escala
        polyGate (g, vs.v[i], i, pulseSrc[i], pulses[i]);
    }
    if (g.chance (0.6f)) g.scope (0, kInst292, M292::outSig (0));
    return 1;
}

// 248 - las dos secciones del generador multiple, cada una con su propio largo de bucle
static int twoSections (Gen& g)
{
    auto vs = polyVoices (g, 4);
    g.on (kInst248);
    for (int s = 1; s <= 16; ++s)
    {
        const juce::String ss (s);
        g.sp ("mf_lvl" + ss, g.rr (0.0f, 1.0f));
        g.sp ("mf_tim" + ss, g.rr (0.12f, 0.6f));
        g.sp ("mf_p1_" + ss, g.chance (0.4f) ? 1.0f : 0.0f);
        g.sp ("mf_p2_" + ss, g.chance (0.3f) ? 1.0f : 0.0f);
        g.sp ("mf_first_" + ss, 0.0f); g.sp ("mf_last_" + ss, 0.0f);
        g.sp ("mf_trange_" + ss, (float) g.pick ({ 0, 1 }));
    }
    // dos bucles de largo distinto sobre las mismas dieciseis etapas: 5 contra 7, 3 contra 8...
    const int lenA = g.pick ({ 3, 5, 7 }), lenB = g.pick ({ 4, 8, 11 });
    g.sp ("mf_first_1", 1.0f);              g.sp ("mf_last_" + juce::String (lenA), 1.0f);
    g.sp ("mf_first_" + juce::String (lenA + 1), 1.0f);
    g.sp ("mf_last_" + juce::String (juce::jmin (16, lenA + lenB)), 1.0f);
    // El 248 arranca detenido, y al llegar a su ultima etapa se para. Un pulsador le manda start a
    // cada seccion: el primero la pone en marcha y los siguientes la relanzan cuando termino.
    // Como cada seccion tiene otro largo, las dos vueltas se separan enseguida.
    g.on (kInst242);
    g.sp ("pp_len", (float) g.pick ({ 5, 7, 11 }));
    g.sp ("pp_period", g.rr (0.3f, 0.7f));
    for (int s = 1; s <= 12; ++s)
    {
        const juce::String ss (s);
        g.sp ("pp_a" + ss, g.chance (0.3f) ? 1.0f : 0.0f);
        g.sp ("pp_b" + ss, g.chance (0.25f) ? 1.0f : 0.0f);
    }
    g.con (kInst242, M242::Out_A, kInst248, M248::In_Start1);
    g.con (kInst242, M242::Out_B, kInst248, M248::In_Start2);
    polyGate (g, vs.v[0], 0, kInst248, M248::Out_P1_1);
    polyGate (g, vs.v[1], 1, kInst248, M248::Out_P2_1);
    polyGate (g, vs.v[2], 2, kInst248, M248::Out_P1_2);
    polyGate (g, vs.v[3], 3, kInst248, M248::Out_P2_2);
    g.con (kInst248, M248::Out_Volt1, vs.v[0], M259::In_TimbreCv);
    g.con (kInst248, M248::Out_Volt2, vs.v[2], M259::In_TimbreCv);
    g.con (kInst248, M248::Out_All1, vs.v[1], M259::In_IndexCv);
    g.con (kInst248, M248::Out_All2, vs.v[3], M259::In_IndexCv);
    if (g.chance (0.6f)) g.scope (0, kInst248, M248::Out_Volt1);
    return 1;
}

// 250 + 242 marcando, y los cuatro generadores de la segunda unidad girando por encima
// Un 281 en ciclo no tiene silencio entre golpe y golpe: es un tremolo, no un ritmo. Asi que el
// pulso lo ponen la secuencia y el pulsador, cada uno con su vuelta, y los que giran libres se
// ocupan de mover el timbre a cuatro velocidades que no coinciden con nada.
static int cyclesOverPulse (Gen& g)
{
    auto vs = polyVoices (g, 3);
    g.on (kInst242); g.on (kInst250);
    g.sp ("pp_len", (float) g.pick ({ 5, 7, 9, 11 }));
    g.sp ("pp_period", g.rr (0.15f, 0.38f));
    for (int s = 1; s <= 12; ++s)
    {
        const juce::String ss (s);
        g.sp ("pp_a" + ss, g.chance (0.42f) ? 1.0f : 0.0f);
        g.sp ("pp_b" + ss, g.chance (0.3f) ? 1.0f : 0.0f);
    }
    for (int s = 1; s <= 16; ++s)
    {
        const juce::String ss (s);
        g.sp ("sq_t_" + ss, g.rr (0.12f, 0.6f));
        g.sp ("sq_v1_" + ss, g.rr (0.0f, 1.0f));
        g.sp ("sq_p1_" + ss, g.chance (0.42f) ? 1.0f : 0.0f);
        g.sp ("sq_p2_" + ss, g.chance (0.3f) ? 1.0f : 0.0f);
        g.sp ("sq_mode_" + ss, 0.0f);
    }
    polyGate (g, vs.v[0], 0, kInst242, M242::Out_A);
    polyGate (g, vs.v[1], 1, kInst242, M242::Out_B);
    polyGate (g, vs.v[2], 2, kInst250, M250::Out_P1);
    // la segunda unidad, los cuatro en ciclo, a proporciones que no vuelven a cruzarse
    g.on (kInst281 + 1);
    static const float ratio[4] = { 1.0f, 1.618f, 2.5f, 4.2f };
    const float base = g.rr (0.12f, 0.4f);
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n = juce::String::charToString ((juce::juce_wchar) ('1' + i));
        const float tt = juce::jlimit (0.03f, 0.95f, base * ratio[i]);
        g.sp ("fb_att" + n, tt * g.rr (0.3f, 0.7f));
        g.sp ("fb_dec" + n, tt);
        g.sp ("fb_mode" + n, 2.0f);                     // ciclo: gira sola, para siempre
    }
    // Diez voltios enteros sobre el timbre o el indice dejan la voz en un extremo y casi no sale
    // señal. Dos de los ciclos pasan por el procesador de voltaje, que los atenua; los otros dos
    // van a la afinacion del modulador, que aguanta el rango completo sin apagarse.
    g.on (kInst257);
    g.sp ("vp_k1", g.rr (0.12f, 0.35f)); g.sp ("vp_off1", g.rr (0.0f, 0.2f));
    g.sp ("vp_k2", g.rr (0.12f, 0.35f)); g.sp ("vp_off2", g.rr (0.0f, 0.2f));
    g.con (kInst281 + 1, M281::outFunc (0), kInst257, M257::In_Va1);
    g.con (kInst257, M257::Out_1, vs.v[0], M259::In_TimbreCv);
    g.con (kInst281 + 1, M281::outFunc (1), kInst257, M257::In_Va2);
    g.con (kInst257, M257::Out_2, vs.v[1], M259::In_IndexCv);
    g.con (kInst281 + 1, M281::outFunc (2), vs.v[2], M259::In_MoPitch);
    g.con (kInst250, M250::Out_Cv1, vs.v[0], M259::In_MoPitch);
    if (g.chance (0.4f)) g.con (kInst242, M242::Out_C, kInst250, M250::In_AddrPulse);
    if (g.chance (0.6f)) g.scope (0, kInst292, M292::outSig (0));
    return 1;
}

// =================================================================================================

static juce::String makeName (juce::Random& r, const juce::String& cat, std::set<juce::String>& used)
{
    static const char* a[] = { "Deep", "Lost", "Quiet", "Bright", "Hollow", "Slow", "Wild", "Soft", "Hard", "Far",
                               "Iron", "Glass", "Velvet", "Paper", "Copper", "Amber", "Cobalt", "Ash", "Salt", "Dust",
                               "Early", "Late", "First", "Last", "Low", "High", "Near", "Open", "Close", "Still" };
    static const char* b[] = { "Mirror", "Engine", "Garden", "Signal", "Current", "Lantern", "Harbour", "Circuit", "Prism", "Thread",
                               "Canyon", "Beacon", "Orbit", "Tide", "Ember", "Spiral", "Window", "Anchor", "Meadow", "Pulse",
                               "Shadow", "Cradle", "Furnace", "Needle", "Ribbon", "Column", "Vessel", "Lattice", "Echo", "Drift" };
    for (int t = 0; t < 400; ++t)
    {
        juce::String n = juce::String (a[r.nextInt (30)]) + " " + b[r.nextInt (30)];
        if (used.insert (n).second) return n;
    }
    return cat + " " + juce::String (used.size());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc > 1 && juce::String (argv[1]) == "--params")   // ground truth for whoever writes archetypes
    {
        WcProcessor p;
        for (auto* par : p.getParameters())
            if (auto* w = dynamic_cast<juce::AudioProcessorParameterWithID*> (par)) std::printf ("%s\n", w->paramID.toRawUTF8());
        return 0;
    }
    const juce::File outDir (juce::File::getCurrentWorkingDirectory()
                                 .getChildFile (juce::String (argc > 1 ? argv[1] : "AllModules")));
    const int total = argc > 2 ? juce::String (argv[2]).getIntValue() : 100;
    const juce::String privPath { juce::CharPointer_UTF8 (argc > 3 ? argv[3] : "") };
    outDir.createDirectory();

    // The generator has to run licensed. In demo mode setPresent() silently refuses the third and
    // fourth oscillator, so a pack built without a licence would quietly be a two-voice pack - which
    // is exactly how the 300-preset library ended up never using the 259 C and D.
    juce::TemporaryFile keyHolder (".key");
    const juce::File keyFile = keyHolder.getFile().getParentDirectory().getChildFile (wc::License::keyFileName());
    if (privPath.isNotEmpty())
    {
        const juce::File priv { privPath };
        juce::String key;
        for (auto& line : juce::StringArray::fromLines (priv.loadFileAsString()))
            if (line.trim().contains (",") && line.trim().length() > 100) { key = line.trim(); break; }
        if (key.isEmpty()) { std::printf ("no private key in %s\n", privPath.toRawUTF8()); return 1; }
        keyFile.replaceWithText (wc::License::makeKeyFile ("Preset Pack Builder", "build@calabiyau", "PACK", key));
    }
    {
        WcProcessor probe;
        probe.recheckLicense (keyFile);
        if (probe.demo())
        {
            std::printf ("FAILED: the generator is in demo mode, so the rack caps at %d oscillators.\n"
                         "Pass the private signing key file as the third argument.\n",
                         wc::License::kDemoMaxOscillators);
            return 1;
        }
    }

    struct Arch { const char* cat; int (*fn) (Gen&); int count; };
    const bool poly = argc > 4 && juce::String (argv[4]) == "poly";
    const Arch polyArch[] = {
        { "252 Three Clocks", threeClocks, 20 },
        { "242 vs 250",      twoCycles,   20 },
        { "245 + 264 Hold",  fiveAndHold, 20 },
        { "248 Two Loops",   twoSections, 20 },
        { "250 + 242 + Ciclos", cyclesOverPulse, 20 },
    };
    const Arch arch[] = {
        { "230 Follower",   follower,   10 },
        { "257 Processor",  processor,  10 },
        { "207 Mixer",      mixer,      10 },
        { "281 Eight Lanes", eightLanes, 10 },
        { "258 Two Oscillators", twoSimple, 10 },
        { "259 Four Voices", fourVoices, 10 },
        { "267 Uncertainty", uncertainty, 10 },
        { "291 Morphing",   morph,      10 },
        { "284 + 275 Room", roomTone,   10 },
        { "666 Four Scopes", fourScopes, 5 },
        { "The Whole Rack", everything, 5 },
    };

    std::set<juce::String> used;
    std::map<juce::String, int> coverage;
    juce::Random names (777);
    int made = 0, seed = 7000, rejected = 0;

    const Arch* table = poly ? polyArch : arch;
    const int tableLen = poly ? (int) (sizeof (polyArch) / sizeof (polyArch[0]))
                              : (int) (sizeof (arch) / sizeof (arch[0]));
    for (int ai = 0; ai < tableLen; ++ai)
    {
        const Arch& a = table[ai];
        const int want = (int) std::lround ((double) a.count * total / 100.0);
        for (int i = 0; i < want; ++i)
        {
            bool done = false;
            int why[3] = { 0, 0, 0 };
            for (int attempt = 0; attempt < 45 && ! done; ++attempt)
            {
                WcProcessor p;
                p.recheckLicense (keyFile);
                p.prepareToPlay (SR, 512);
                p.resetAll();
                Gen g { p, juce::Random (++seed) };
                const int mode = a.fn (g);

                auto render = [&]
                {
                    p.prepareToPlay (SR, 512);
                    const int blocks = (int) (SR * (poly ? 8.0 : 3.0) / 512.0);
                    double sum = 0.0; float peak = 0.0f; long n = 0;
                    std::vector<float> blockRms;
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
                        double bsum = 0.0;
                        for (int ch = 0; ch < 2; ++ch)
                            for (int s = 0; s < 512; ++s)
                            {
                                const float v = buf.getSample (ch, s);
                                peak = juce::jmax (peak, std::fabs (v));
                                bsum += (double) v * v;
                                if (b > 8) { sum += (double) v * v; ++n; }
                            }
                        if (b > 8) blockRms.push_back ((float) std::sqrt (bsum / 1024.0));
                    }
                    // cuanto se mueve: la desviacion en dB del nivel bloque a bloque. Un drone da
                    // casi cero; un patron ritmico da varios dB. Sin esto entran piezas que suenan
                    // pero no tocan nada.
                    float movement = 0.0f;
                    if (blockRms.size() > 8)
                    {
                        double m = 0.0; int c = 0;
                        for (float v : blockRms) { m += juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, v)); ++c; }
                        m /= juce::jmax (1, c);
                        double s2 = 0.0;
                        for (float v : blockRms)
                        { const double d = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, v)) - m; s2 += d * d; }
                        movement = (float) std::sqrt (s2 / juce::jmax (1, c));
                    }
                    return std::make_tuple (peak, (float) std::sqrt (sum / (double) juce::jmax (1L, n)), movement);
                };

                auto [peak, rms, mov] = render();
                if (peak < 1.0e-4f || rms < 3.0e-4f || ! std::isfinite (peak))
                { ++rejected; why[0]++; continue; }
                const float master = p.apvts.getRawParameterValue ("out_master")->load();
                const float target = juce::jlimit (-48.0f, 6.0f, master + (-4.0f - juce::Decibels::gainToDecibels (peak)));
                g.sp ("out_master", target);
                auto [peak2, rms2, mov2] = render();
                // -42 dB over three seconds with a healthy peak means a click and then nothing: a dud,
                // not a pluck. The factory library used -54 dB and let a few of those through.
                if (peak2 > 0.99f || peak2 < 0.05f || rms2 < 0.008f) { ++rejected; why[1]++; continue; }
                // una pieza generativa que no se mueve es un drone, no un patron: afuera
                if (poly && mov2 < 5.0f) { ++rejected; why[2]++; continue; }

                const juce::String name = makeName (names, a.cat, used);
                const juce::File f = outDir.getChildFile (juce::String::formatted ("%03d ", made + 1)
                                                          + name + ".wcpatch");
                p.patchName = f.getFileNameWithoutExtension();
                if (! p.savePatchFile (f)) { std::printf ("  !! could not write %s\n", f.getFullPathName().toRawUTF8()); return 1; }

                // which modules did this one actually use?
                for (int inst = 0; inst < kNumInst; ++inst)
                    if (p.isPresent (inst)) coverage[instKey (inst)]++;

                ++made; done = true;
                std::printf ("%3d  %-22s %-26s peak %.2f  rms %5.1f dB  mov %4.1f dB\n", made, a.cat,
                             name.toRawUTF8(), peak2, juce::Decibels::gainToDecibels (rms2), mov2);
            }
            if (! done) std::printf ("  !! gave up on one %s preset (mudo %d, nivel %d, sin movimiento %d)\n",
                                     a.cat, why[0], why[1], why[2]);
        }
    }

    std::printf ("\n%d presets written to %s (%d rejected and rebuilt)\n",
                 made, outDir.getFullPathName().toRawUTF8(), rejected);

    // every slot in the rack has to appear in at least one preset, or the pack has not done its job
    std::printf ("\nmodule coverage:\n");
    int missing = 0;
    for (int inst = 0; inst < kNumInst; ++inst)
    {
        const juce::String k = instKey (inst);
        const int c = coverage[k];
        if (c == 0) ++missing;
        std::printf ("   %-6s %3d%s\n", k.toRawUTF8(), c, c == 0 ? "   <<<< MISSING" : "");
    }
    if (missing > 0 && ! poly) { std::printf ("\n%d module(s) never used. FAILED\n", missing); return 1; }

    // and every file has to load back into a fresh plugin
    int bad = 0;
    for (auto& f : outDir.findChildFiles (juce::File::findFiles, false, "*.wcpatch"))
    {
        WcProcessor q;
        q.recheckLicense (keyFile);
        q.prepareToPlay (SR, 512);
        if (! q.loadPatchFile (f)) { std::printf ("  !! will not load: %s\n", f.getFileName().toRawUTF8()); ++bad; }
    }
    std::printf ("\nreloaded %d file(s), %d bad\n", made, bad);
    std::printf (bad == 0 && (missing == 0 || poly) ? "ALL OK\n" : "FAILED\n");
    return bad == 0 ? 0 : 1;
}
