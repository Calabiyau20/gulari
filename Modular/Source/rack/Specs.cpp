// Panel layouts, after the Series 200 / 200t front panels:
// light grey aluminium, blue silkscreen in lowercase italics, printed scales around the knobs,
// banana jacks colour-coded by signal (red = pulses, black = control in, blue = control out),
// 3.5 mm jacks for audio.
#include "Module.h"
#include "../modules/M259.h"
#include "../modules/M266.h"
#include "../modules/M245.h"
#include "../modules/M242.h"
#include "../modules/M257.h"
#include "../modules/M264.h"
#include "../modules/M230.h"
#include "../modules/M277.h"
#include "../modules/M207.h"
#include "../modules/M296.h"
#include "../modules/M248.h"
#include "../modules/M218.h"
#include "../modules/M250.h"

namespace wc
{
static constexpr juce::uint32 kGreyKnob = 0xffe6e6e1, kBlueKnob = 0xff3a60c0, kBlack = 0xff1b1c20, kWhite = 0xfff2f2ee;
static const char* kTimeScale = ".001|.01|.03|.1|.3|1|3|10";

static JackSpec J (const char* l, JackKind k, bool out, float x, float y, bool pulse = false) { JackSpec j { l, k, out, x, y }; j.pulse = pulse; return j; }
static ControlSpec C (const char* id, const char* l, CtrlStyle st, float x, float y, juce::uint32 col = kGreyKnob, const char* scale = nullptr)
{
    ControlSpec c { id, l, st, x, y, col };
    c.scale = scale;
    return c;
}

// silkscreen helpers -------------------------------------------------------------------------------
static constexpr juce::uint32 kSkirt = 0xff2c55b4;           // blue knob skirt (200t knobs)
static constexpr juce::uint32 kTintGrey = 0xffa6a8ac, kTintOrange = 0xffe8892b, kTintPurple = 0xffb79ade, kTintRed = 0xffc8202a;
static TextSpec T (const char* t, float x, float y, float size = 9.5f, int style = 0, int just = 0) { return { t, x, y, size, style, just }; }
static LineSpec L (float x1, float y1, float x2, float y2, float w = 2.0f, bool dashed = false, bool arrow = false) { return { x1, y1, x2, y2, w, dashed, arrow }; }
static LedSpec D (float x, float y, int id, juce::uint32 col = 0xffe8241c) { return { x, y, id, col }; }
static JackSpec JT (const char* l, JackKind k, bool out, float x, float y, juce::uint32 tint, bool pulse = false) { auto j = J (l, k, out, x, y, pulse); j.tint = tint; return j; }
// small waveform icons drawn with silkscreen lines
static void iconSaw (ModuleSpec& s, float x, float y) { s.lines.push_back (L (x - 8, y + 4, x, y - 4, 1.2f)); s.lines.push_back (L (x, y - 4, x, y + 4, 1.2f)); s.lines.push_back (L (x, y + 4, x + 8, y - 4, 1.2f)); s.lines.push_back (L (x + 8, y - 4, x + 8, y + 4, 1.2f)); }
static void iconSqr (ModuleSpec& s, float x, float y) { s.lines.push_back (L (x - 8, y + 4, x - 8, y - 4, 1.2f)); s.lines.push_back (L (x - 8, y - 4, x, y - 4, 1.2f)); s.lines.push_back (L (x, y - 4, x, y + 4, 1.2f)); s.lines.push_back (L (x, y + 4, x + 8, y + 4, 1.2f)); s.lines.push_back (L (x + 8, y + 4, x + 8, y - 4, 1.2f)); }
static void iconTri (ModuleSpec& s, float x, float y) { s.lines.push_back (L (x - 9, y + 4, x - 4.5f, y - 4, 1.2f)); s.lines.push_back (L (x - 4.5f, y - 4, x, y + 4, 1.2f)); s.lines.push_back (L (x, y + 4, x + 4.5f, y - 4, 1.2f)); s.lines.push_back (L (x + 4.5f, y - 4, x + 9, y + 4, 1.2f)); }
static void iconSine (ModuleSpec& s, float x, float y)
{
    float px = x - 9, py = y;
    for (int i = 1; i <= 12; ++i) { const float t = (float) i / 12.0f; const float nx = x - 9 + 18 * t, ny = y - 4.0f * std::sin (6.2832f * t); s.lines.push_back (L (px, py, nx, ny, 1.2f)); px = nx; py = ny; }
}
// scale a layout drawn in photo pixels to panel units (knob / jack sizes and font sizes are not scaled)
static void scaleSpec (ModuleSpec& s, float k)
{
    s.width *= k; s.height *= k;
    for (auto& j : s.jacks) { j.x *= k; j.y *= k; }
    for (auto& c : s.controls) { c.x *= k; c.y *= k; }
    for (auto& t : s.texts) { t.x *= k; t.y *= k; }
    for (auto& l : s.lines) { l.x1 *= k; l.y1 *= k; l.x2 *= k; l.y2 *= k; }
    for (auto& d : s.leds) { d.x *= k; d.y *= k; }
    for (auto& sc : s.sections) { sc.x *= k; sc.y *= k; sc.w *= k; sc.h *= k; }
    s.footY *= k;
}

// ---------------------------------------------------------------------------------------------
// CW-259 Complex Waveform Generator - layout of the 259t front (photo, 560 x 422 px), plus a narrow
// "program" strip with the programming inputs of the original 259.
// ---------------------------------------------------------------------------------------------
static ModuleSpec make259()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_259;
    s.name = "PROGRAMMABLE COMPLEX WAVEFORM GENERATOR";
    s.model = "CW-259";
    s.engr = "259t";
    s.width = 622.0f;
    s.height = 422.0f;
    // ---- jacks: inputs in DSP order (M259::In) ----
    s.jacks = {
        JT ("kbd input", K::Banana, false, 105, 357, kTintGrey),   // In_MoPitch
        J ("c.v. in", K::Banana, false, 168, 357),                 // In_MoCv
        J ("f.m. in", K::Mini, false, 48, 357),                    // In_MoFm
        J ("c.v. in", K::Banana, false, 235, 357),                 // In_IndexCv
        J ("wave", K::Banana, false, 591, 124),                    // In_ProgWave
        J ("pitch", K::Banana, false, 591, 230, true),             // In_ProgPitch
        J ("timbre", K::Banana, false, 591, 280, true),            // In_ProgTimbre
        J ("ampl.", K::Banana, false, 591, 178, true),             // In_ProgAmp
        J ("", K::Mini, false, 357, 124),                          // In_Lock (phase lock input)
        JT ("kbd input", K::Banana, false, 365, 357, kTintGrey),   // In_PoPitch
        J ("c.v. in", K::Banana, false, 428, 357),                 // In_PoCv
        J ("f.m. in", K::Mini, false, 305, 357),                   // In_PoFm
        J ("c.v. in", K::Banana, false, 505, 357),                 // In_TimbreCv
        J ("", K::Banana, false, 538, 56),                         // In_SymCv
        J ("", K::Banana, false, 538, 140),                        // In_OrderCv
        // outputs (M259::Out)
        J ("", K::Mini, true, 125, 56),                            // Out_MoSignal
        J ("", K::Banana, true, 75, 56),                           // Out_MoCv (blue)
        J ("", K::Mini, true, 295, 56),                            // Out_Sine
        J ("", K::Mini, true, 340, 56),                            // Out_Square
        J ("", K::Mini, true, 385, 56),                            // Out_Final1
        J ("final", K::Mini, true, 591, 340),                      // Out_Final2
        J ("", K::Mini, true, 30, 56),                             // Out_MoTri
        J ("", K::Banana, true, 172, 56, true),                    // Out_MoPulse  (GATE OUT)
        J ("", K::Banana, true, 425, 56, true),                    // Out_PoPulse  (GATE OUT)
    };
    // ---- controls ----
    s.controls = {
        C ("mo_freq", "", CtrlStyle::BigKnob, 85, 214, kSkirt, "27.5|55|110|220|440|880|1760|3520|7040"),
        C ("mo_fine", "", CtrlStyle::SmallKnob, 166, 178, kSkirt),
        C ("mo_range", "high|low", CtrlStyle::Button, 82, 114),
        C ("mo_wave", "", CtrlStyle::Switch3, 176, 116),
        C ("mo_fm", "", CtrlStyle::SmallKnob, 46, 302, kSkirt),
        C ("mo_cv", "", CtrlStyle::SmallKnob, 166, 302, kSkirt),

        C ("lock_on", "", CtrlStyle::Toggle, 256, 52, kSkirt),
        C ("mod_amp", "", CtrlStyle::Toggle, 256, 90, kSkirt),
        C ("mod_pitch", "", CtrlStyle::Toggle, 256, 128, kSkirt),
        C ("mod_timbre", "", CtrlStyle::Toggle, 256, 166, kSkirt),
        C ("idx", "", CtrlStyle::Knob, 235, 236, kSkirt, "*|*|*|*|*|*|*|*|*|*|*"),
        C ("idx_cv", "", CtrlStyle::SmallKnob, 235, 302, kSkirt),

        C ("volt1", "", CtrlStyle::Button, 290, 114),
        C ("lock", "", CtrlStyle::SmallKnob, 423, 124, kSkirt),
        C ("po_freq", "", CtrlStyle::BigKnob, 345, 214, kSkirt, "27.5|55|110|220|440|880|1760|3520|7040"),
        C ("po_fine", "", CtrlStyle::SmallKnob, 426, 178, kSkirt),
        C ("po_fm", "", CtrlStyle::SmallKnob, 297, 302, kSkirt),
        C ("po_cv", "", CtrlStyle::SmallKnob, 426, 302, kSkirt),

        C ("sym", "", CtrlStyle::Knob, 492, 96, kSkirt),
        C ("order", "", CtrlStyle::Knob, 492, 176, kSkirt),
        C ("timbre", "", CtrlStyle::Knob, 505, 244, kSkirt, "*|*|*|*|*|*|*|*|*|*|*"),
        C ("timbre_cv", "", CtrlStyle::SmallKnob, 505, 310, kSkirt),

        C ("prog", "local|rem|both", CtrlStyle::Switch3, 576, 66),
    };
    s.controls[0].scale2 = ".25|.5|1|2|4|8|16|32|64";
    s.controls[14].scale2 = "A0|A1|A2|A3|A4|A5|A6|A7|A8";
    // ---- silkscreen ----
    s.lines = {
        L (4, 22, 618, 22), L (4, 392, 618, 392),
        L (150, 22, 150, 92), L (200, 22, 200, 392), L (270, 22, 270, 392), L (400, 22, 400, 92), L (455, 22, 455, 392), L (560, 22, 560, 392),
        L (4, 92, 200, 92), L (270, 92, 455, 92), L (4, 150, 200, 150), L (270, 150, 455, 150),
        L (105, 92, 105, 150), L (335, 92, 335, 150), L (455, 250, 560, 250), L (200, 196, 270, 196),
        L (255, 34, 215, 34, 1.4f, false, true),                    // the arrow in the modulation column
        L (370, 124, 405, 124, 1.2f),                                // phase lock input -> gain
    };
    iconTri (s, 30, 78); iconSine (s, 295, 78); iconSqr (s, 340, 78);
    iconSaw (s, 142, 100); iconSqr (s, 142, 116); iconTri (s, 142, 132);
    s.texts = {
        T ("PROGRAMMABLE COMPLEX WAVEFORM GENERATOR  MODEL CW-259", 290, 13, 10.5f, 4),
        T ("MODULATION   OSC. OUTPUTS", 12, 33, 9.5f, 1, 1),
        T ("c.v.", 75, 78), T ("signal", 125, 78),
        T ("GATE", 175, 33, 9.5f, 1), T ("OUT", 165, 80, 8.5f, 1),
        T ("PRINCIPAL   OSC. OUTPUTS", 278, 33, 9.5f, 1, 1), T ("final", 385, 78),
        T ("GATE", 428, 33, 9.5f, 1), T ("OUT", 418, 80, 8.5f, 1),
        T ("HARMONICS", 462, 33, 9.5f, 1, 1),
        T ("low", 32, 101, 9.0f, 0, 1), T ("high", 32, 117, 9.0f, 0, 1),
        T ("RANGE", 82, 138, 8.5f, 1), T ("WAVESHAPE", 152, 141, 8.5f, 1),
        T ("FREQUENCY (Hz)", 85, 272, 9.5f, 1),
        T ("phase", 217, 47, 8.5f, 0, 1), T ("lock", 217, 58, 8.5f, 0, 1),
        T ("ampl.", 217, 85, 8.5f, 0, 1), T ("mod.", 217, 96, 8.5f, 0, 1),
        T ("pitch", 217, 123, 8.5f, 0, 1), T ("mod.", 217, 134, 8.5f, 0, 1),
        T ("timbre", 217, 161, 8.5f, 0, 1), T ("mod.", 217, 172, 8.5f, 0, 1),
        T ("MOD. INDEX", 235, 272, 9.0f, 1),
        T ("1V/OCT", 290, 98, 8.5f, 1), T ("1v/oct", 315, 132, 7.5f),
        T ("input", 357, 102), T ("gain", 423, 98), T ("PHASE LOCK", 395, 143, 8.5f, 1),
        T ("PITCH (Hz)", 345, 272, 9.5f, 1),
        T ("Symmetry", 500, 54, 9.5f, 1), T ("even", 463, 130, 8.5f), T ("odd", 524, 130, 8.5f),
        T ("Order", 496, 140, 9.5f, 1), T ("low", 463, 210, 8.5f), T ("high", 524, 210, 8.5f),
        T ("TIMBRE", 505, 280, 9.5f, 1),
        T ("-", 148, 279, 11.0f, 1), T ("+", 184, 279, 11.0f, 1), T ("-", 217, 279, 11.0f, 1), T ("+", 253, 279, 11.0f, 1),
        T ("-", 408, 279, 11.0f, 1), T ("+", 444, 279, 11.0f, 1), T ("-", 484, 296, 11.0f, 1), T ("+", 526, 296, 11.0f, 1),
        T ("PROGRAM", 591, 33, 8.5f, 1),
        T ("CALABI YAU   -   after Buchla 259t", 290, 407, 9.0f, 1),
    };
    s.leds = {
        D (22, 101, M259::Led_High), D (22, 117, M259::Led_Low),
        D (122, 100, M259::Led_Saw), D (122, 116, M259::Led_Sqr), D (122, 132, M259::Led_Tri),
        D (188, 80, M259::Led_MoGate), D (441, 80, M259::Led_PoGate),
        D (209, 52, M259::Led_Lock), D (209, 90, M259::Led_Amp), D (209, 128, M259::Led_Pitch), D (209, 166, M259::Led_Timbre),
        D (315, 116, M259::Led_1V), D (468, 52, M259::Led_Sym), D (468, 140, M259::Led_Order),
    };
    scaleSpec (s, 430.0f / 422.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// QF-281 Quad Function Generator - layout of the 281t front (photo, 413 x 370 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make281()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_281;
    s.name = "QUAD FUNCTION GENERATOR";
    s.model = "QF-281";
    s.engr = "281t";
    s.width = 413.0f;
    s.height = 370.0f;
    const float yc[4] = { 67, 145, 223, 301 };
    for (int n = 0; n < 4; ++n)
    {
        const float y = yc[n];
        s.jacks.push_back (J ("trig", K::Banana, false, 22, y - 20, true));
        s.jacks.push_back (J ("cycle", K::Banana, false, 60, y - 20, true));
        s.jacks.push_back (J ("c.v. in", K::Banana, false, 97, y - 20));
        s.jacks.push_back (J ("c.v. in", K::Banana, false, 263, y - 20));
    }
    for (int n = 0; n < 4; ++n)
    {
        const float y = yc[n];
        s.jacks.push_back (J ("", K::Banana, true, 301, y + 14));          // FUNC (blue)
        s.jacks.push_back (J ("", K::Banana, true, 301, y - 20, true));    // EOD (red)
    }
    s.jacks.push_back (J ("", K::Banana, true, 357, 116));
    s.jacks.push_back (J ("", K::Banana, true, 395, 116));
    s.jacks.push_back (J ("", K::Banana, true, 357, 256));
    s.jacks.push_back (J ("", K::Banana, true, 395, 256));
    static const char* att[4] = { "att1", "att2", "att3", "att4" };
    static const char* dec[4] = { "dec1", "dec2", "dec3", "dec4" };
    static const char* mode[4] = { "mode1", "mode2", "mode3", "mode4" };
    static const char* trg[4] = { "trig1", "trig2", "trig3", "trig4" };
    for (int n = 0; n < 4; ++n)
    {
        const float y = yc[n];
        s.controls.push_back (C (trg[n], "", CtrlStyle::Push, 22, y + 20));
        s.controls.push_back (C (mode[n], "", CtrlStyle::Switch3, 60, y + 20, 0xffd8261f));     // red-tipped, as on the 281t
        s.controls.push_back (C (att[n], "attack", CtrlStyle::Knob, 146, y - 10, kSkirt, kTimeScale));
        s.controls.push_back (C (dec[n], "decay", CtrlStyle::Knob, 216, y - 10, kSkirt, kTimeScale));
    }
    s.controls.push_back (C ("mix12", "", CtrlStyle::BigKnob, 376, 64, kSkirt));
    s.controls.push_back (C ("mix34", "", CtrlStyle::BigKnob, 376, 204, kSkirt));
    s.controls.push_back (C ("quad12", "", CtrlStyle::Toggle, 357, 320, 0xffe8c22a));   // yellow-tipped
    s.controls.push_back (C ("quad34", "", CtrlStyle::Toggle, 397, 320, 0xffe8c22a));
    // silkscreen
    s.lines = { L (4, 22, 409, 22), L (4, 345, 409, 345), L (338, 22, 338, 345) };
    for (int n = 1; n < 4; ++n) s.lines.push_back (L (4, yc[n] - 39, 338, yc[n] - 39));
    const char* letters[4] = { "A", "B", "C", "D" };
    for (int n = 0; n < 4; ++n)
    {
        const float y = yc[n];
        iconSaw (s, 74, y + 12); iconTri (s, 74, y + 28);                    // cycle switch icons
        s.texts.push_back (T (letters[n], 254, y + 24, 10.0f, 3));            // circled letter
        s.texts.push_back (T ("outs", 330, y - 3, 8.0f, 0, 1));
        s.lines.push_back (L (314, y - 28, 314, y - 20, 1.2f)); s.lines.push_back (L (310, y - 20, 318, y - 20, 1.2f));  // pulse symbol
        s.lines.push_back (L (310, y + 22, 314, y + 12, 1.2f)); s.lines.push_back (L (314, y + 12, 318, y + 22, 1.2f));  // function symbol
        s.leds.push_back (D (323, y - 20, n * 2, 0xffffd23a));                // EOD / peak
        s.leds.push_back (D (323, y + 14, n * 2 + 1));                         // function level
    }
    s.texts.push_back (T ("A", 349, 92, 9.0f, 1)); s.texts.push_back (T ("B", 403, 92, 9.0f, 1));
    s.texts.push_back (T ("C", 349, 232, 9.0f, 1)); s.texts.push_back (T ("D", 403, 232, 9.0f, 1));
    s.lines.push_back (L (360, 90, 357, 102, 1.2f, false, true)); s.lines.push_back (L (392, 90, 395, 102, 1.2f, false, true));
    s.lines.push_back (L (360, 230, 357, 242, 1.2f, false, true)); s.lines.push_back (L (392, 230, 395, 242, 1.2f, false, true));
    s.texts.push_back (T ("A - B", 357, 298, 8.5f, 1)); s.texts.push_back (T ("C - D", 397, 298, 8.5f, 1));
    s.texts.push_back (T ("on", 377, 314, 7.5f, 1)); s.texts.push_back (T ("off", 377, 327, 7.5f, 1));
    s.texts.push_back (T ("QUADRATURE", 375, 340, 8.5f, 1));
    s.texts.push_back (T ("QUAD FUNCTION GENERATOR  QF-281", 206, 12, 10.5f, 4));
    s.texts.push_back (T ("CALABI YAU   -   after Buchla 281t", 206, 358, 9.0f, 1));
    scaleSpec (s, 430.0f / 370.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// LG-292 Quad Lowpass Gate - layout of the 292t front (photo, 400 x 630 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make292()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_292;
    s.name = "QUAD LOPASS GATE";
    s.model = "LG-292";
    s.engr = "292t";
    s.width = 400.0f;
    s.height = 630.0f;
    const float cx[4] = { 130, 200, 268, 340 };
    const float cy[4] = { 165, 245, 320, 395 };
    for (int n = 0; n < 4; ++n)
    {
        s.jacks.push_back (J ("", K::Mini, false, cx[n], 540));       // signal input
        s.jacks.push_back (J ("", K::Banana, false, 45, cy[n]));      // c.v. in
    }
    for (int n = 0; n < 4; ++n) s.jacks.push_back (J ("", K::Mini, true, cx[n], 80));
    s.jacks.push_back (J ("", K::Mini, true, 38, 76));                // all
    static const char* off[4] = { "off1", "off2", "off3", "off4" };
    static const char* mode[4] = { "mode1", "mode2", "mode3", "mode4" };
    const char* letters[4] = { "A", "B", "C", "D" };
    s.lines = { L (4, 30, 396, 30), L (4, 115, 396, 115), L (4, 450, 396, 450), L (4, 505, 396, 505), L (4, 585, 396, 585) };
    for (int n = 0; n < 4; ++n)
    {
        s.controls.push_back (C (off[n], "", CtrlStyle::Knob, cx[n], cy[n], kSkirt));
        s.controls.push_back (C (mode[n], "", CtrlStyle::Switch3, cx[n] - 4, 477, kSkirt));
        s.lines.push_back (L (cx[n], 96, cx[n], 445, 1.6f));
        s.lines.push_back (L (cx[n], 505, cx[n], 520, 1.6f));
        s.lines.push_back (L (62, cy[n], cx[n] - 30, cy[n], 1.6f));
        s.leds.push_back (D (cx[n] + 26, 463, n * 2));
        s.leds.push_back (D (cx[n] + 26, 491, n * 2 + 1));
        s.texts.push_back (T (letters[n], cx[n], 50, 10.0f, 1));
        s.texts.push_back (T (letters[n], 78, cy[n] - 18, 10.0f, 1));
        s.texts.push_back (T (letters[n], cx[n], 516, 10.0f, 1));
    }
    s.texts.push_back (T ("QUAD LOPASS GATE  LG-292", 200, 16, 10.5f, 4));
    s.texts.push_back (T ("OUTPUTS", 14, 54, 10.0f, 1, 1));
    s.texts.push_back (T ("all", 72, 100, 10.0f, 0));
    s.texts.push_back (T ("cv in", 45, 428, 9.5f, 0));
    s.texts.push_back (T ("min", 306, 438, 9.0f)); s.texts.push_back (T ("max", 370, 438, 9.0f));
    s.texts.push_back (T ("M", 18, 460, 9.0f, 1)); s.texts.push_back (T ("O", 18, 472, 9.0f, 1));
    s.texts.push_back (T ("D", 18, 484, 9.0f, 1)); s.texts.push_back (T ("E", 18, 496, 9.0f, 1));
    s.texts.push_back (T ("lopass", 34, 463, 9.0f, 0, 1)); s.texts.push_back (T ("combo", 34, 477, 9.0f, 0, 1)); s.texts.push_back (T ("gate", 34, 491, 9.0f, 0, 1));
    s.texts.push_back (T ("SIGNAL", 14, 528, 10.0f, 1, 1)); s.texts.push_back (T ("INPUTS", 14, 546, 10.0f, 1, 1));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 292t", 200, 604, 9.0f, 1));
    scaleSpec (s, 430.0f / 630.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// SU-266 Source of Uncertainty - layout of the 266t front (photo, 590 x 610 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make266()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_266;
    s.name = "SOURCE OF UNCERTAINTY";
    s.model = "SU-266";
    s.engr = "266t";
    s.width = 590.0f;
    s.height = 610.0f;
    s.jacks = {
        // inputs (M266::In)
        J ("input", K::Banana, false, 175, 80),                       // In_Fluct1Cv
        J ("input", K::Banana, false, 175, 190),                      // In_Fluct2Cv
        JT ("", K::Banana, false, 445, 145, kTintOrange, true),       // In_ShPulse
        J ("", K::Banana, false, 445, 205),                           // In_ShCv
        J ("", K::Banana, false, 40, 310),                            // In_QCv
        JT ("inputs", K::Banana, false, 40, 375, kTintOrange, true),  // In_QPulse
        J ("", K::Banana, false, 40, 470),                            // In_SCv
        JT ("inputs", K::Banana, false, 40, 535, kTintOrange, true),  // In_SPulse
        J ("inputs", K::Banana, false, 445, 395),                     // In_IntIn
        J ("", K::Banana, false, 445, 480),                           // In_IntCv
        // outputs (M266::Out)
        J ("", K::Mini, true, 40, 70),                                // Out_Pink   (-3 dB/oct)
        J ("", K::Mini, true, 40, 155),                               // Out_White  (flat)
        J ("", K::Mini, true, 40, 240),                               // Out_Blue   (+3 dB/oct)
        JT ("", K::Banana, true, 375, 80, kTintPurple),               // Out_Fluct1
        JT ("", K::Banana, true, 375, 190, kTintPurple),              // Out_Fluct2
        JT ("", K::Banana, true, 545, 90, kTintRed, true),            // Out_ShPulseA
        JT ("alt", K::Banana, true, 545, 145, kTintRed, true),        // Out_ShPulseB
        J ("", K::Banana, true, 445, 270),                            // Out_Sh
        J ("", K::Banana, true, 545, 245),                            // Out_ShAltA
        J ("alt", K::Banana, true, 545, 300),                         // Out_ShAltB
        J ("", K::Banana, true, 300, 330),                            // Out_Q2n
        J ("", K::Banana, true, 385, 330),                            // Out_Qn1
        JT ("", K::Banana, true, 300, 492, kTintPurple),              // Out_StoredUni
        JT ("", K::Banana, true, 385, 492, kTintPurple),              // Out_StoredDist
        JT ("out", K::Banana, true, 545, 395, kTintPurple),           // Out_Int
    };
    s.controls = {
        C ("rate", "", CtrlStyle::Knob, 290, 84, kSkirt, ".05|.5|5|50"),
        C ("rate2", "", CtrlStyle::Knob, 290, 194, kSkirt, ".05|.5|5|50"),
        C ("steps", "quantization", CtrlStyle::Knob, 140, 322, kSkirt, "1|2|3|4|5|6"),
        C ("dist", "probability", CtrlStyle::Knob, 140, 482, kSkirt, "low|*|*|*|high"),
        C ("smooth", "", CtrlStyle::Knob, 510, 500, kSkirt, ".01|.1|1|10"),
    };
    s.lines = {
        L (4, 28, 586, 28), L (4, 592, 586, 592),
        L (145, 28, 145, 275), L (410, 28, 410, 592), L (4, 275, 410, 275), L (4, 440, 410, 440), L (410, 340, 586, 340),
        // noise source rules
        L (58, 70, 138, 70, 1.4f), L (58, 155, 138, 155, 1.4f), L (58, 240, 138, 240, 1.4f),
        // fluctuating: input -> knob
        L (190, 80, 262, 84, 1.4f), L (190, 190, 262, 194, 1.4f),
        // sample and hold: dashed alternating arrows
        L (462, 138, 528, 96, 1.4f, true, true), L (462, 150, 528, 145, 1.4f, true, true),
        L (462, 262, 528, 248, 1.4f, true, true), L (462, 276, 528, 298, 1.4f, true, true),
        // quantized / stored: c.v. in -> knob
        L (56, 310, 116, 318, 1.4f), L (56, 470, 116, 478, 1.4f),
        // integrator: input -> out, c.v. -> knob
        L (462, 395, 526, 395, 1.4f, false, true), L (462, 480, 484, 492, 1.4f),
    };
    s.texts = {
        T ("SOURCE OF UNCERTAINTY  SU-266", 295, 15, 10.5f, 4),
        T ("-3db/oct", 98, 60, 10.0f), T ("flat", 98, 145, 10.0f), T ("+3db/oct", 98, 230, 10.0f),
        T ("NOISE SOURCE", 75, 264, 10.0f, 1),
        T ("out", 375, 56, 10.0f), T ("out", 375, 166, 10.0f),
        T ("FLUCTUATING RANDOM VOLTAGES", 278, 158, 10.0f, 1), T ("FLUCTUATING RANDOM VOLTAGES", 278, 264, 10.0f, 1),
        T ("pulse in", 445, 98, 10.0f), T ("c.v. in", 492, 205, 10.0f, 0, 1), T ("c.v. out", 492, 270, 10.0f, 0, 1),
        T ("SAMPLE AND HOLD", 495, 328, 10.0f, 1),
        T ("2^n", 240, 340, 10.0f), T ("n + 1", 345, 340, 10.0f), T ("out", 300, 306, 10.0f), T ("out", 385, 306, 10.0f),
        T ("distribution", 340, 368, 10.0f),
        T ("QUANTIZED RANDOM VOLTAGES", 210, 428, 10.0f, 1),
        T ("out", 300, 468, 10.0f), T ("out", 385, 468, 10.0f), T ("uniform", 240, 500, 9.0f), T ("shaped", 345, 500, 9.0f),
        T ("distribution", 340, 528, 10.0f),
        T ("STORED RANDOM VOLTAGES", 210, 580, 10.0f, 1),
        T ("c.v. in", 445, 510, 9.0f),
        T ("INTEGRATOR", 495, 580, 10.0f, 1),
        T ("CALABI YAU  -  after Buchla 266t", 295, 602, 9.0f, 1),
    };
    s.leds = {
        D (375, 124, M266::Led_Fluct1), D (375, 234, M266::Led_Fluct2),
        D (240, 306, M266::Led_Q2n), D (345, 306, M266::Led_Qn1),
        D (240, 466, M266::Led_StoredUni), D (345, 466, M266::Led_StoredDist),
    };
    scaleSpec (s, 430.0f / 610.0f);
    return s;
}


// ---------------------------------------------------------------------------------------------
// SV-245 Sequential Voltage Source - layout of the 245t front (photo, 656 x 554 px):
// the PULSER on the left, five stage columns of four voltage knobs (rows A-D), stage LEDs on top,
// stage switches, gate outputs and stage select inputs below, stored voltage outputs on the right.
// ---------------------------------------------------------------------------------------------
static ModuleSpec make245()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_245;
    s.name = "SEQUENTIAL VOLTAGE SOURCE";
    s.model = "SV-245";
    s.engr = "245t";
    s.width = 656.0f;
    s.height = 554.0f;
    const float sx[5] = { 222, 305, 388, 471, 554 };
    const float ry[4] = { 112, 184, 256, 328 };
    // inputs (M245::In)
    s.jacks.push_back (JT ("advance", K::Banana, false, 622, 432, kTintOrange, true));   // In_Advance
    for (int n = 0; n < 5; ++n) s.jacks.push_back (JT ("", K::Banana, false, sx[n], 492, kTintOrange, true));   // In_Sel1..5
    s.jacks.push_back (J ("analog", K::Banana, false, 622, 492));                         // In_Analog
    s.jacks.push_back (JT ("stop", K::Banana, false, 48, 370, kTintOrange, true));        // In_Stop
    s.jacks.push_back (JT ("start", K::Banana, false, 128, 370, kTintOrange, true));      // In_Start
    s.jacks.push_back (J ("", K::Banana, false, 128, 432));                               // In_LenCv
    s.jacks.push_back (J ("", K::Banana, false, 48, 432));                                // In_TimeCv
    // outputs (M245::Out)
    for (int n = 0; n < 5; ++n) s.jacks.push_back (J ("", K::Banana, true, sx[n], 432, true));      // Out_Gate1..5
    for (int r = 0; r < 4; ++r) s.jacks.push_back (J ("", K::Banana, true, 622, ry[r]));              // Out_A..D
    s.jacks.push_back (JT ("ref", K::Banana, true, 48, 496, 0xfff4f4f0));                // Out_Ref (white)
    s.jacks.push_back (J ("pulse", K::Banana, true, 128, 496, true));                    // Out_Pulse
    // controls
    static const char* ids[4][5] = { { "a1", "a2", "a3", "a4", "a5" }, { "b1", "b2", "b3", "b4", "b5" },
                                     { "c1", "c2", "c3", "c4", "c5" }, { "d1", "d2", "d3", "d4", "d5" } };
    for (int r = 0; r < 4; ++r)
        for (int n = 0; n < 5; ++n) s.controls.push_back (C (ids[r][n], "", CtrlStyle::Knob, sx[n], ry[r], kSkirt));
    static const char* sw[4] = { "sw1", "sw2", "sw3", "sw4" };
    for (int k = 0; k < 4; ++k) s.controls.push_back (C (sw[k], "", CtrlStyle::Toggle, (sx[k] + sx[k + 1]) * 0.5f, 380, kSkirt));
    s.controls.push_back (C ("stop", "", CtrlStyle::Push, 48, 82));
    s.controls.push_back (C ("start", "", CtrlStyle::Push, 128, 82));
    s.controls.push_back (C ("time", "", CtrlStyle::BigKnob, 88, 190, kSkirt, ".002|.01|.1|1|10"));
    s.controls.push_back (C ("tcv", "", CtrlStyle::SmallKnob, 48, 290, kSkirt, "-|*|0|*|+"));
    s.controls.push_back (C ("len", "", CtrlStyle::SmallKnob, 128, 290, kSkirt, "*|*|*|*|*"));
    // silkscreen
    s.lines = { L (4, 24, 652, 24), L (4, 528, 652, 528), L (176, 24, 176, 528), L (590, 24, 590, 528),
                L (176, 362, 590, 362), L (176, 462, 652, 462), L (4, 330, 176, 330), L (590, 362, 652, 362) };
    for (int n = 0; n < 5; ++n) s.lines.push_back (L (sx[n], 140, sx[n], 156, 1.0f));
    for (int k = 0; k < 4; ++k)        // the stage switch arrows: stage n -> n+1
        s.lines.push_back (L (sx[k] + 14, 405, sx[k + 1] - 14, 405, 1.2f, false, true));
    for (int r = 0; r < 4; ++r) s.lines.push_back (L (580, ry[r], 604, ry[r], 1.2f, false, true));
    s.lines.push_back (L (88, 440, 88, 486, 1.2f, true));                  // pulser -> advance (normalled)
    const char* letters[4] = { "A", "B", "C", "D" };
    const char* nums[5] = { "1", "2", "3", "4", "5" };
    for (int r = 0; r < 4; ++r) s.texts.push_back (T (letters[r], 186, ry[r] + 4, 10.0f, 3));
    for (int n = 0; n < 5; ++n)
    {
        s.texts.push_back (T (nums[n], sx[n], 48, 11.0f, 1));
        s.leds.push_back (D (sx[n], 68, M245::Led_Stage1 + n));
    }
    s.leds.push_back (D (48, 50, M245::Led_Stop, 0xffffd23a));
    s.leds.push_back (D (128, 50, M245::Led_Start, 0xff3ad05a));
    s.texts.push_back (T ("SEQUENTIAL VOLTAGE SOURCE  SV-245", 328, 14, 10.5f, 4));
    s.texts.push_back (T ("PULSER", 88, 40, 10.0f, 1));
    s.texts.push_back (T ("stop", 48, 108, 9.0f)); s.texts.push_back (T ("start", 128, 108, 9.0f));
    s.texts.push_back (T ("time", 88, 248, 9.5f));
    s.texts.push_back (T ("c.v.", 48, 318, 9.0f)); s.texts.push_back (T ("length", 128, 318, 9.0f));
    s.texts.push_back (T ("c.v. in", 48, 456, 9.0f)); s.texts.push_back (T ("length c.v.", 128, 456, 8.0f));
    s.texts.push_back (T ("OUT", 88, 520, 9.0f, 1));
    s.texts.push_back (T ("STAGE GATE OUTS", 383, 456, 9.0f, 1));
    s.texts.push_back (T ("STAGE SELECT INPUTS", 383, 520, 9.0f, 1));
    s.texts.push_back (T ("stored", 622, 52, 9.0f)); s.texts.push_back (T ("voltage", 622, 64, 9.0f)); s.texts.push_back (T ("OUTS", 622, 78, 9.0f, 1));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 245t", 328, 544, 9.0f, 1));
    scaleSpec (s, 430.0f / 554.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// PP-242 Programmable Pulser - layout of the 242t front (photo, 413 x 615 px):
// the 4 x 12 LED matrix (row n = number of stages, rows A / B / C = pulse patterns), the row / column
// encoders with the on/off button, stop - single - start, the period controls and the jacks.
// The matrix LEDs can also be clicked directly.
// ---------------------------------------------------------------------------------------------
static ModuleSpec make242()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_242;
    s.name = "PROGRAMMABLE PULSER";
    s.model = "PP-242";
    s.engr = "242t";
    s.width = 413.0f;
    s.height = 615.0f;
    // inputs (M242::In)
    s.jacks.push_back (JT ("start", K::Banana, false, 42, 470, kTintOrange, true));
    s.jacks.push_back (JT ("stop", K::Banana, false, 112, 470, kTintOrange, true));
    s.jacks.push_back (J ("period", K::Banana, false, 182, 470));
    s.jacks.push_back (JT ("clk in", K::Banana, false, 300, 470, kTintOrange, true));
    s.jacks.push_back (JT ("rst in", K::Banana, false, 370, 470, kTintOrange, true));
    // outputs (M242::Out): ALTER (two pulses + ramp), MATRIX A B C, ALL
    s.jacks.push_back (J ("", K::Banana, true, 42, 560, true));
    s.jacks.push_back (J ("", K::Banana, true, 92, 560, true));
    s.jacks.push_back (JT ("", K::Banana, true, 142, 560, kTintGrey));
    s.jacks.push_back (J ("A", K::Banana, true, 220, 560, true));
    s.jacks.push_back (J ("B", K::Banana, true, 270, 560, true));
    s.jacks.push_back (J ("C", K::Banana, true, 320, 560, true));
    s.jacks.push_back (J ("all", K::Banana, true, 378, 560, true));
    // the matrix
    const float mx0 = 82, mdx = 28, my[4] = { 70, 108, 146, 184 };
    const char* rows[4] = { "n", "A", "B", "C" };
    const char* nums[12] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12" };
    for (int r = 0; r < 4; ++r)
    {
        s.texts.push_back (T (rows[r], 40, my[r] + 4, 11.0f, r == 0 ? 1 : 3));
        for (int c = 0; c < 12; ++c)
            s.leds.push_back (D (mx0 + c * mdx, my[r], M242::Led_Matrix + r * M242::kSteps + c, r == 0 ? 0xffffd23a : 0xffe8241c));
    }
    for (int c = 0; c < 12; ++c) s.texts.push_back (T (nums[c], mx0 + c * mdx, 46, 8.5f));
    s.controls.push_back (C ("row", "", CtrlStyle::Knob, 82, 262, kSkirt));
    s.controls.push_back (C ("col", "", CtrlStyle::Knob, 206, 262, kSkirt));
    s.controls.push_back (C ("cell", "", CtrlStyle::Push, 326, 262));
    s.controls.push_back (C ("stop", "", CtrlStyle::Push, 82, 360));
    s.controls.push_back (C ("single", "", CtrlStyle::Push, 152, 360));
    s.controls.push_back (C ("start", "", CtrlStyle::Push, 222, 360));
    s.controls.push_back (C ("cv", "", CtrlStyle::SmallKnob, 182, 412, kSkirt, "-|*|0|*|+"));
    s.controls.push_back (C ("period", "", CtrlStyle::Knob, 330, 370, kSkirt, ".002|.02|.2|2|10|off"));
    s.leds.push_back (D (82, 332, M242::Led_Stop, 0xffffd23a));
    s.leds.push_back (D (222, 332, M242::Led_Start, 0xff3ad05a));
    s.lines = { L (4, 24, 409, 24), L (4, 218, 409, 218), L (4, 308, 409, 308), L (4, 438, 409, 438), L (4, 512, 409, 512), L (4, 598, 409, 598),
                L (260, 308, 260, 438), L (240, 438, 240, 512), L (175, 512, 175, 598), L (350, 512, 350, 598),
                L (182, 430, 182, 452, 1.2f, false, true) };
    // ALTER icons: two pulses with a dashed alternating arrow, ramp
    s.lines.push_back (L (52, 540, 82, 540, 1.2f, true, true));
    s.lines.push_back (L (132, 548, 152, 532, 1.2f)); s.lines.push_back (L (152, 532, 152, 548, 1.2f));
    s.texts.push_back (T ("PROGRAMMABLE PULSER  PP-242", 206, 14, 10.5f, 4));
    s.texts.push_back (T ("STAGES", 260, 210, 8.5f, 1, 1));
    s.texts.push_back (T ("row", 82, 296, 9.0f)); s.texts.push_back (T ("column", 206, 296, 9.0f)); s.texts.push_back (T ("on / off", 326, 290, 9.0f));
    s.texts.push_back (T ("stop", 82, 386, 9.0f)); s.texts.push_back (T ("single", 152, 386, 9.0f)); s.texts.push_back (T ("start", 222, 386, 9.0f));
    s.texts.push_back (T ("PERIOD", 330, 330, 9.5f, 1)); s.texts.push_back (T ("c.v.", 210, 412, 8.5f, 0, 1));
    s.texts.push_back (T ("INPUTS", 120, 506, 9.0f, 1)); s.texts.push_back (T ("CLOCK", 335, 506, 9.0f, 1));
    s.texts.push_back (T ("ALTER", 92, 590, 9.0f, 1)); s.texts.push_back (T ("MATRIX", 270, 590, 9.0f, 1)); s.texts.push_back (T ("ALL", 378, 590, 9.0f, 1));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 242t", 206, 608, 9.0f, 1));
    scaleSpec (s, 430.0f / 615.0f);
    return s;
}


// ---------------------------------------------------------------------------------------------
// VP-257 Dual Voltage Processor - layout of the 257t front (photo, 760 x 1362 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make257()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_257; s.name = "DUAL VOLTAGE PROCESSOR"; s.model = "VP-257"; s.engr = "257t";
    s.width = 760.0f; s.height = 1362.0f;
    const float yo[2] = { 0, 600 };
    const float jx[4] = { 100, 240, 380, 520 };
    for (int n = 0; n < 2; ++n) for (int k = 0; k < 4; ++k) s.jacks.push_back (J ("", K::Banana, false, jx[k], yo[n] + 597));
    for (int n = 0; n < 2; ++n) s.jacks.push_back (J ("", K::Banana, true, 655, yo[n] + 597));
    static const char* kk[2] = { "k1", "k2" }; static const char* mm[2] = { "m1", "m2" }; static const char* pp[2] = { "p51", "p52" }; static const char* oo[2] = { "off1", "off2" };
    for (int n = 0; n < 2; ++n)
    {
        const float y = yo[n];
        s.controls.push_back (C (kk[n], "", CtrlStyle::Knob, 120, y + 227, kSkirt, "*|*|*|*|*|*|*|*|*"));
        s.controls.push_back (C (mm[n], "", CtrlStyle::Knob, 380, y + 227, kSkirt, "*|*|*|*|*|*|*|*|*"));
        s.controls.push_back (C (oo[n], "", CtrlStyle::Knob, 635, y + 227, kSkirt, "*|*|*|*|*|*|*|*|*"));
        s.controls.push_back (C (pp[n], "", CtrlStyle::Button, 170, y + 467));
        s.texts.push_back (T ("-", 40, y + 125, 10.0f, 3)); s.texts.push_back (T ("+", 200, y + 125, 10.0f, 3));
        s.lines.push_back (L (95, y + 118, 70, y + 118, 1.2f, false, true)); s.lines.push_back (L (145, y + 118, 170, y + 118, 1.2f, false, true));
        s.texts.push_back (T ("-1", 55, y + 344, 10.0f, 1)); s.texts.push_back (T ("+1", 185, y + 344, 10.0f, 1));
        s.texts.push_back (T (".5", 380, y + 82, 10.0f, 1)); s.texts.push_back (T ("0", 318, y + 344, 10.0f, 1)); s.texts.push_back (T ("1", 445, y + 344, 10.0f, 1));
        s.texts.push_back (T (".5", 635, y + 82, 10.0f, 1)); s.texts.push_back (T ("0", 575, y + 344, 10.0f, 1)); s.texts.push_back (T ("1", 700, y + 344, 10.0f, 1));
        s.lines.push_back (L (120, y + 290, 120, y + 378, 1.6f, false, true));
        s.lines.push_back (L (380, y + 290, 330, y + 378, 1.6f, false, true)); s.lines.push_back (L (380, y + 290, 430, y + 378, 1.6f, false, true));
        s.lines.push_back (L (635, y + 290, 635, y + 378, 1.6f, false, true));
        s.texts.push_back (T ("Va K", 120, y + 412, 12.0f, 1)); s.texts.push_back (T ("+", 215, y + 412, 11.0f, 1));
        s.texts.push_back (T ("Vb M", 325, y + 412, 12.0f, 1)); s.lines.push_back (L (343, y + 390, 368, y + 390, 1.4f));
        s.texts.push_back (T ("+", 410, y + 412, 11.0f, 1)); s.texts.push_back (T ("M Vc", 480, y + 412, 12.0f, 1));
        s.texts.push_back (T ("+", 560, y + 412, 11.0f, 1)); s.texts.push_back (T ("Voffset", 650, y + 412, 12.0f, 1));
        s.texts.push_back (T ("= Vout", 660, y + 467, 12.0f, 1));
        s.texts.push_back (T ("< +.5", 270, y + 467, 10.0f, 1));
        s.lines.push_back (L (170, y + 500, 210, y + 575, 1.4f));
        for (int k = 0; k < 4; ++k)
            if (k == 2) { s.lines.push_back (L (380, y + 545, 345, y + 508, 1.4f, false, true)); s.lines.push_back (L (380, y + 545, 415, y + 508, 1.4f, false, true)); }
            else s.lines.push_back (L (jx[k], y + 550, jx[k], y + 505, 1.4f, false, true));
        s.leds.push_back (D (592, y + 512, M257::Led_1 + n));
    }
    s.lines.push_back (L (8, 79, 752, 79)); s.lines.push_back (L (8, 677, 752, 677)); s.lines.push_back (L (8, 1287, 752, 1287));
    s.texts.push_back (T ("DUAL VOLTAGE PROCESSOR  VP-257", 380, 38, 10.5f, 4));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 257t", 380, 1325, 9.0f, 1));
    scaleSpec (s, 430.0f / 1362.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// DO-258 Dual Oscillator - layout of the 258t front (photo, flattened to 675 x 1090 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make258()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_258; s.name = "DUAL OSCILLATOR"; s.model = "DO-258"; s.engr = "258t";
    s.width = 675.0f; s.height = 1090.0f;
    const float yb[2] = { 60, 560 };
    for (int o = 0; o < 2; ++o)
    {
        const float y = yb[o] + 70;
        s.jacks.push_back (JT ("", K::Banana, false, 375, y, kTintGrey));   // kbd (1V/OCT)
        s.jacks.push_back (J ("", K::Banana, false, 85, y));                // processing
        s.jacks.push_back (J ("", K::Mini, false, 185, y));                 // f.m.
        s.jacks.push_back (J ("", K::Banana, false, 280, y));               // waveshape c.v.
    }
    s.jacks.push_back (J ("", K::Mini, true, 490, yb[0] + 70));
    s.jacks.push_back (J ("", K::Mini, true, 490, yb[1] + 70));
    s.jacks.push_back (J ("", K::Mini, true, 590, yb[0] + 70));
    s.jacks.push_back (J ("", K::Mini, true, 590, yb[1] + 70));
    static const char* fr[2] = { "freq1", "freq2" }; static const char* sh[2] = { "shape1", "shape2" };
    static const char* pr[2] = { "proc1", "proc2" }; static const char* fm[2] = { "fm1", "fm2" }; static const char* fi[2] = { "fine1", "fine2" };
    for (int o = 0; o < 2; ++o)
    {
        const float y = yb[o];
        s.controls.push_back (C (fm[o], "", CtrlStyle::Knob, 185, y + 205, kSkirt));
        s.controls.push_back (C (fi[o], "", CtrlStyle::Knob, 375, y + 215, kSkirt));
        s.controls.push_back (C (pr[o], "", CtrlStyle::Knob, 85, y + 345, kSkirt));
        s.controls.push_back (C (sh[o], "", CtrlStyle::Knob, 280, y + 365, kSkirt));
        s.controls.push_back (C (fr[o], "", CtrlStyle::BigKnob, 552, y + 345, kSkirt, "5cps|*|*|*|*|*|20kc"));
        s.texts.push_back (T ("INPUTS", 185, y + 25, 10.0f, 1)); s.texts.push_back (T ("1V/OCT", 375, y + 25, 10.0f, 1)); s.texts.push_back (T ("OUTPUTS", 540, y + 25, 10.0f, 1));
        s.lines.push_back (L (512, y + 70, 568, y + 70, 1.4f));
        s.lines.push_back (L (8, y + 120, 667, y + 120));
        s.lines.push_back (L (85, y + 95, 85, y + 280, 1.4f)); s.lines.push_back (L (185, y + 95, 185, y + 140, 1.4f)); s.lines.push_back (L (280, y + 95, 280, y + 300, 1.4f));
        s.texts.push_back (T ("FM", 245, y + 250, 10.0f, 1)); s.texts.push_back (T ("FINE", 440, y + 262, 10.0f, 1)); s.texts.push_back (T ("FREQ", 552, y + 205, 11.0f, 1));
        s.texts.push_back (T ("+", 150, y + 410, 10.0f, 3));
        s.lines.push_back (L (390, y + 285, 345, y + 395, 1.4f, false, true));
        if (o == 0) iconSaw (s, 360, y + 420); else iconSqr (s, 360, y + 420);
        s.lines.push_back (L (8, y + 470, 667, y + 470));
    }
    s.texts.push_back (T ("DUAL OSCILLATOR  DO-258", 337, 24, 10.5f, 4));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 258t", 337, 1062, 9.0f, 1));
    scaleSpec (s, 430.0f / 1090.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// QS-264 Quad Sample & Hold / Polyphonic Adaptor - layout of the 264t front (photo, 835 x 1172 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make264()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_264; s.name = "QUAD SAMPLE AND HOLD"; s.model = "QS-264"; s.engr = "264t";
    s.width = 835.0f; s.height = 1172.0f;
    const float cx[4] = { 100, 234, 372, 507 };
    for (int c = 0; c < 4; ++c) s.jacks.push_back (JT ("", K::Banana, false, cx[c], 709, kTintRed, true));   // pulse inputs (red on the 264t)
    for (int c = 0; c < 4; ++c) s.jacks.push_back (J ("", K::Banana, false, cx[c], 422));                    // c.v. inputs
    s.jacks.push_back (J ("", K::Banana, false, 232, 1020));                                                 // polyphonic c.v.
    s.jacks.push_back (JT ("", K::Banana, false, 505, 1020, kTintRed, true));                                // polyphonic pulse
    s.jacks.push_back (J ("", K::Mini, false, 717, 1025));                                                   // modulation
    for (int c = 0; c < 4; ++c) s.jacks.push_back (J ("", K::Banana, true, cx[c], 846, true));              // pulse outputs
    for (int c = 0; c < 4; ++c) s.jacks.push_back (JT ("", K::Banana, true, cx[c], 287, kBlack));            // c.v. outputs (black)
    for (int c = 0; c < 4; ++c) s.jacks.push_back (JT ("", K::Banana, true, cx[c], 148, kBlack));            // ART outputs
    static const char* tr[4] = { "track1", "track2", "track3", "track4" };
    for (int c = 0; c < 4; ++c)
    {
        s.controls.push_back (C (tr[c], "", CtrlStyle::Toggle, cx[c], 556, kSkirt));
        s.leds.push_back (D (cx[c] + 66, 900, M264::Led_1 + c));
        if (c < 3) s.lines.push_back (L (cx[c] + 34, 422, cx[c + 1] - 34, 422, 1.4f));
    }
    s.controls.push_back (C ("common", "", CtrlStyle::Toggle, 122, 1018, kWhite));
    s.controls.push_back (C ("voices", "", CtrlStyle::SmallKnob, 370, 1030, kSkirt, "1|2|3|4"));
    s.controls.push_back (C ("tune", "", CtrlStyle::Push, 717, 137));
    s.controls.push_back (C ("scale", "", CtrlStyle::Knob, 717, 300, kSkirt, "m|ch|M"));
    s.controls.push_back (C ("key", "", CtrlStyle::Knob, 717, 485, kSkirt, "c|c#|d|d#|e|f|f#|g|g#|a|a#|b"));
    s.controls.push_back (C ("offset", "", CtrlStyle::Knob, 717, 668, kSkirt));
    s.controls.push_back (C ("mod", "", CtrlStyle::Knob, 717, 855, kSkirt));
    s.lines = { L (8, 70, 827, 70), L (8, 1110, 827, 1110), L (600, 70, 600, 1110), L (600, 207, 827, 207) };
    for (float y : { 207.0f, 345.0f, 482.0f, 627.0f, 768.0f, 925.0f }) s.lines.push_back (L (8, y, 600, y));
    s.lines.push_back (L (25, 440, 25, 1018, 1.4f)); s.lines.push_back (L (25, 440, 64, 422, 1.4f)); s.lines.push_back (L (25, 1018, 85, 1018, 1.4f));
    s.lines.push_back (L (268, 1020, 330, 1020, 1.4f)); s.lines.push_back (L (410, 1020, 470, 1020, 1.4f));
    s.lines.push_back (L (717, 910, 717, 995, 1.4f));
    s.texts = {
        T ("QUAD SAMPLE AND HOLD  QS-264", 417, 36, 10.5f, 4),
        T ("ART OUTPUTS", 304, 92, 10.0f, 1), T ("CONTROL VOLTAGE OUTPUTS", 304, 230, 10.0f, 1), T ("CONTROL VOLTAGE INPUTS", 304, 366, 10.0f, 1),
        T ("track and hold (tracks while pulse is high)", 304, 503, 8.5f), T ("sample and hold (samples on rising edge of pulse)", 304, 612, 8.5f),
        T ("PULSE  INPUTS", 304, 652, 10.0f, 1), T ("PULSE  OUTPUTS", 304, 790, 10.0f, 1), T ("POLYPHONIC  INPUTS", 304, 950, 10.0f, 1),
        T ("separate  common", 120, 1078, 8.5f), T ("NUMBER OF VOICES", 400, 1080, 8.5f, 1),
        T ("press to auto tune", 717, 88, 8.5f), T ("hold to initial tune", 717, 190, 8.5f),
        T ("OFFSET", 717, 600, 10.0f, 1), T ("-", 662, 740, 10.0f, 3), T ("+", 772, 740, 10.0f, 3),
        T ("MODULATION", 717, 785, 9.5f, 1), T ("zero", 662, 935, 9.0f), T ("max", 772, 935, 9.0f),
        T ("CALABI YAU  -  after Buchla 264t", 417, 1142, 9.0f, 1), T ("POLYPHONIC ADAPTOR", 417, 1105, 9.0f, 1),
    };
    scaleSpec (s, 430.0f / 1172.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// EF-230 Triple Envelope Follower - layout of the 230t front (photo, flattened to 600 x 1140 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make230()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_230; s.name = "TRIPLE ENVELOPE FOLLOWER"; s.model = "EF-230"; s.engr = "230t";
    s.width = 600.0f; s.height = 1140.0f;
    const float cx[3] = { 105, 300, 495 };
    for (int c = 0; c < 3; ++c) s.jacks.push_back (J ("", K::Mini, false, cx[c], 440));                   // signal inputs
    for (int c = 0; c < 3; ++c) s.jacks.push_back (J ("", K::Banana, true, cx[c] - 45, 135));              // c.v. out (first of the pair)
    for (int c = 0; c < 3; ++c) s.jacks.push_back (J ("", K::Banana, true, cx[c] - 30, 295, true));        // pulse out
    for (int c = 0; c < 3; ++c) s.jacks.push_back (J ("", K::Banana, true, cx[c] + 45, 135));              // c.v. out (second)
    static const char* se[3] = { "sens1", "sens2", "sens3" }; static const char* de[3] = { "decay1", "decay2", "decay3" };
    static const char* mo[3] = { "mode1", "mode2", "mode3" };
    const char* abc[3] = { "A", "B", "C" };
    for (int c = 0; c < 3; ++c)
    {
        const float x = cx[c];
        s.controls.push_back (C (mo[c], "", CtrlStyle::Toggle, x, 590, 0xffd8261f));
        s.controls.push_back (C (de[c], "", CtrlStyle::Knob, x, 778, kSkirt, "*|*|*|*|*|5"));
        s.controls.push_back (C (se[c], "", CtrlStyle::Knob, x, 985, kSkirt, "*|*|*|*|*|max"));
        s.lines.push_back (L (x - 20, 135, x + 20, 135, 1.4f));
        s.texts.push_back (T (abc[c], x, 190, 10.0f, 1));
        s.lines.push_back (L (x, 295, x + 30, 295, 1.4f));
        s.leds.push_back (D (x + 45, 295, M230::Led_Pulse1 + c));
    }
    for (float y : { 52.0f, 215.0f, 355.0f, 500.0f, 660.0f, 870.0f, 1080.0f }) s.lines.push_back (L (8, y, 592, y));
    s.lines.push_back (L (120, 548, 60, 548, 1.2f, false, true)); s.lines.push_back (L (480, 548, 540, 548, 1.2f, false, true));
    s.texts.push_back (T ("TRIPLE ENVELOPE FOLLOWER  EF-230", 300, 26, 10.5f, 4));
    s.texts.push_back (T ("CONTROL VOLTAGE OUTPUTS", 300, 80, 10.0f, 1));
    s.texts.push_back (T ("PULSE OUTPUTS", 300, 240, 10.0f, 1));
    s.texts.push_back (T ("SIGNAL INPUTS", 300, 382, 10.0f, 1));
    s.texts.push_back (T ("PULSE RESPONSE", 300, 528, 10.0f, 1));
    s.texts.push_back (T ("sustained", 100, 530, 9.0f)); s.texts.push_back (T ("transient", 500, 528, 9.0f));
    s.texts.push_back (T ("DECAY TIME", 300, 686, 10.0f, 1));
    s.texts.push_back (T ("SENSITIVITY", 300, 892, 10.0f, 1));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 230t", 300, 1108, 9.0f, 1));
    scaleSpec (s, 430.0f / 1140.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// FS-285 Frequency Shifter / Balanced Modulator - layout of the 285t front (photo, flattened to 740 x 1030 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make285()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_285; s.name = "FREQUENCY SHIFTER"; s.model = "FS-285"; s.engr = "285t";
    s.width = 740.0f; s.height = 1030.0f;
    s.jacks = {
        J ("", K::Banana, false, 110, 272), J ("", K::Mini, false, 240, 272), J ("", K::Mini, false, 505, 272), J ("", K::Mini, false, 640, 272),
        J ("", K::Banana, false, 110, 742), J ("", K::Mini, false, 505, 742), J ("", K::Mini, false, 640, 742),
        J ("", K::Mini, true, 75, 140), J ("", K::Mini, true, 165, 140), J ("", K::Mini, true, 575, 140), J ("", K::Mini, true, 665, 140),
        J ("", K::Mini, true, 75, 610), J ("", K::Mini, true, 165, 610), J ("", K::Mini, true, 575, 610), J ("", K::Mini, true, 665, 610),
    };
    s.controls = {
        C ("ctl", "", CtrlStyle::Knob, 110, 425, kSkirt),
        C ("int", "", CtrlStyle::Toggle, 240, 385, kSkirt),
        C ("ref", "", CtrlStyle::Knob, 385, 425, kSkirt, "0|*|*|*|*|1kc"),
        C ("fm", "", CtrlStyle::Knob, 640, 425, kSkirt, "0|*|*|*|*|max"),
        C ("bmctl", "", CtrlStyle::Knob, 110, 895, kSkirt),
        C ("amp", "", CtrlStyle::Knob, 640, 895, kSkirt, "*|*|*|*|*|ring"),
    };
    for (float y : { 62.0f, 190.0f, 322.0f, 492.0f, 532.0f, 662.0f, 792.0f, 985.0f }) s.lines.push_back (L (8, y, 732, y));
    s.lines.push_back (L (100, 140, 140, 140, 1.4f)); s.lines.push_back (L (600, 140, 640, 140, 1.4f));
    s.lines.push_back (L (100, 610, 140, 610, 1.4f)); s.lines.push_back (L (600, 610, 640, 610, 1.4f));
    s.lines.push_back (L (110, 296, 110, 370, 1.4f)); s.lines.push_back (L (240, 296, 240, 355, 1.4f)); s.lines.push_back (L (640, 296, 640, 370, 1.4f));
    s.lines.push_back (L (110, 766, 110, 840, 1.4f));
    s.texts = {
        T ("FREQUENCY SHIFTER  FS-285", 370, 30, 10.5f, 4),
        T ("difference", 120, 92, 10.0f), T ("OUT", 370, 145, 10.0f, 1), T ("sum", 620, 92, 10.0f),
        T ("control", 110, 222, 9.5f), T ("ref.", 240, 222, 9.5f), T ("IN", 370, 278, 10.0f, 1), T ("signal", 505, 222, 9.5f), T ("mod.", 640, 222, 9.5f),
        T ("INT.", 285, 348, 10.0f, 1), T ("REFERENCE", 420, 348, 10.0f, 1), T ("F.M.", 640, 348, 10.0f, 1),
        T ("-", 50, 478, 9.0f, 3), T ("+", 170, 478, 9.0f, 3),
        T ("BALANCED MODULATOR", 370, 514, 9.5f, 1),
        T ("ring", 120, 562, 10.0f), T ("OUT", 370, 615, 10.0f, 1), T ("variable", 620, 562, 10.0f),
        T ("control", 110, 692, 9.5f), T ("IN", 370, 748, 10.0f, 1), T ("signal", 505, 692, 9.5f), T ("ref.", 640, 692, 9.5f),
        T ("MODULATION", 370, 818, 10.0f, 1), T ("amplitude", 640, 818, 9.5f), T ("+", 170, 948, 9.0f, 3),
        T ("CALABI YAU  -  after Buchla 285t", 370, 1008, 9.0f, 1),
    };
    scaleSpec (s, 430.0f / 1030.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// SD-277 Signal Delay Unit - layout of the 277t front (photo, 787 x 1247 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make277()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_277; s.name = "SIGNAL DELAY UNIT"; s.model = "SD-277"; s.engr = "277t";
    s.width = 787.0f; s.height = 1247.0f;
    s.jacks = {
        J ("", K::Mini, false, 100, 320), J ("", K::Mini, false, 294, 320), J ("", K::Mini, false, 490, 320),
        J ("", K::Banana, false, 92, 742), J ("", K::Mini, false, 92, 1067),
        J ("", K::Mini, true, 100, 160), J ("", K::Mini, true, 294, 160), J ("", K::Mini, true, 490, 160), J ("", K::Mini, true, 682, 160),
    };
    s.controls = {
        C ("lvla", "", CtrlStyle::Knob, 132, 504, kSkirt, "*|*|*|*|*|*|*"),
        C ("lvlb", "", CtrlStyle::Knob, 387, 504, kSkirt, "*|*|*|*|*|*|*"),
        C ("lvlc", "", CtrlStyle::Knob, 637, 504, kSkirt, "*|*|*|*|*|*|*"),
        C ("long", "", CtrlStyle::Button, 717, 737),
        C ("ctl", "", CtrlStyle::Knob, 277, 777, kSkirt),
        C ("mod", "", CtrlStyle::Knob, 272, 992, kSkirt, "*|*|*|*|*|*|*"),
        C ("time", "", CtrlStyle::BigKnob, 572, 962, kSkirt, ".05|*|*|.10|*|*|.15|*|*|.20"),
    };
    s.leds = { D (684, 320, M277::Led_Overload) };
    for (float y : { 77.0f, 227.0f, 380.0f, 622.0f, 1167.0f }) s.lines.push_back (L (8, y, 779, y));
    s.lines.push_back (L (100, 344, 132, 450, 1.4f)); s.lines.push_back (L (318, 320, 387, 450, 1.4f)); s.lines.push_back (L (514, 320, 637, 450, 1.4f));
    s.lines.push_back (L (92, 712, 92, 662, 1.4f)); s.lines.push_back (L (92, 662, 277, 662, 1.4f)); s.lines.push_back (L (277, 662, 277, 720, 1.4f));
    s.lines.push_back (L (240, 690, 315, 690, 1.2f, false, true)); s.lines.push_back (L (315, 690, 240, 690, 1.2f, false, true));
    s.lines.push_back (L (92, 1092, 150, 1125, 1.4f)); s.lines.push_back (L (150, 1125, 250, 1045, 1.4f));
    s.texts = {
        T ("SIGNAL DELAY UNIT  SD-277", 393, 38, 10.5f, 4),
        T ("DELAYED  SIGNALS  OUT", 393, 100, 10.0f, 1),
        T ("1/4 t", 187, 165, 10.5f, 1), T ("1/2 t", 390, 165, 10.5f, 1), T ("3/4 t", 582, 165, 10.5f, 1), T ("t", 760, 165, 11.0f, 1),
        T ("SIGNAL  INPUTS", 294, 252, 10.0f, 1), T ("overload", 690, 252, 9.5f),
        T ("A", 32, 325, 11.0f, 1), T ("B", 222, 325, 11.0f, 1), T ("C", 418, 325, 11.0f, 1),
        T ("INPUT   MIXER", 392, 402, 10.0f, 1), T ("zero", 67, 600, 9.0f), T ("max", 217, 600, 9.0f),
        T ("DELAY TIME", 575, 650, 11.0f, 1), T ("(t in seconds)", 575, 682, 9.0f),
        T ("long", 722, 685, 9.5f), T ("original", 722, 795, 9.5f),
        T ("control", 92, 808, 9.0f), T ("input", 92, 838, 9.0f), T ("-", 215, 840, 9.0f, 3), T ("+", 342, 840, 9.0f, 3),
        T ("modu-", 92, 965, 9.0f), T ("lation", 92, 995, 9.0f),
        T ("CALABI YAU  -  after Buchla 277t", 393, 1205, 9.0f, 1),
    };
    scaleSpec (s, 430.0f / 1247.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// MX-207 Mixer / Preamplifier - layout of the 207t front (photo, 888 x 793 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make207()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_207; s.name = "MIXER / PREAMPLIFIER"; s.model = "MX-207"; s.engr = "207t";
    s.width = 888.0f; s.height = 793.0f;
    const float cx[6] = { 56, 168, 278, 410, 519, 632 };
    s.jacks.push_back (J ("", K::Banana, false, 58, 101));      // A assignment c.v.
    s.jacks.push_back (J ("", K::Banana, false, 633, 101));     // F assignment c.v.
    for (int c = 0; c < 6; ++c) s.jacks.push_back (J ("", K::Mini, false, cx[c], 695));
    s.jacks.push_back (J ("", K::Mini, false, 726, 378)); s.jacks.push_back (J ("", K::Mini, false, 846, 378));   // expansion in
    s.jacks.push_back (J ("", K::Mini, false, 831, 671));                                                          // preamp input
    s.jacks.push_back (J ("", K::Mini, true, 726, 101)); s.jacks.push_back (J ("", K::Mini, true, 846, 101));     // L - R
    s.jacks.push_back (J ("", K::Mini, true, 726, 257)); s.jacks.push_back (J ("", K::Mini, true, 846, 257));     // monitor
    s.jacks.push_back (J ("", K::Mini, true, 725, 695));                                                           // preamp output
    static const char* lv[6] = { "lvl_a", "lvl_b", "lvl_c", "lvl_d", "lvl_e", "lvl_f" };
    static const char* pg[6] = { "prog_a", "prog_b", "prog_c", "prog_d", "prog_e", "prog_f" };
    static const char* mn[6] = { "mon_a", "mon_b", "mon_c", "mon_d", "mon_e", "mon_f" };
    static const char* pn[4] = { "pan1", "pan2", "pan3", "pan4" };
    const float px[4] = { 156, 281, 406, 531 };
    for (int k = 0; k < 4; ++k) s.controls.push_back (C (pn[k], "", CtrlStyle::Knob, px[k], 101, kSkirt));
    const char* abc[6] = { "A", "B", "C", "D", "E", "F" };
    const float lx[6] = { 20, 132, 245, 369, 481, 593 };
    for (int c = 0; c < 6; ++c)
    {
        s.controls.push_back (C (lv[c], "", CtrlStyle::Fader, cx[c], 315));
        s.controls.push_back (C (pg[c], "", CtrlStyle::Toggle, cx[c], 496, kSkirt));
        s.controls.push_back (C (mn[c], "", CtrlStyle::Toggle, cx[c], 595, 0xffe8c22a));
        s.texts.push_back (T (abc[c], lx[c], 668, 10.0f, 1));
    }
    s.controls.push_back (C ("gain", "", CtrlStyle::Knob, 831, 518, kSkirt, "*|*|*|*|*|*"));
    s.controls.push_back (C ("range", "", CtrlStyle::Toggle, 726, 553, kSkirt));
    s.leds = { D (33, 151, M207::Led_AL), D (83, 151, M207::Led_AR), D (608, 151, M207::Led_FL), D (658, 151, M207::Led_FR) };
    const char* marks[6] = { "10", "8", "6", "4", "2", "0" };
    for (int i = 0; i < 11; ++i)
    {
        const float y = 218.0f + (float) i * 19.5f;
        s.lines.push_back (L (66, y, 318, y, 0.8f)); s.lines.push_back (L (372, y, 642, y, 0.8f));
    }
    for (int i = 0; i < 6; ++i) s.texts.push_back (T (marks[i], 344, 223.0f + (float) i * 39.0f, 9.5f, 1));
    s.lines.push_back (L (18, 171, 93, 171, 1.2f, false, true)); s.lines.push_back (L (668, 171, 596, 171, 1.2f, false, true));
    for (float y : { 45.0f, 186.0f, 461.0f, 555.0f, 646.0f, 748.0f }) s.lines.push_back (L (4, y, 683, y));
    s.lines.push_back (L (683, 4, 683, 789));
    for (float y : { 45.0f, 186.0f, 305.0f, 425.0f, 748.0f }) s.lines.push_back (L (683, y, 884, y));
    s.lines.push_back (L (750, 257, 822, 257, 1.4f));
    s.lines.push_back (L (750, 518, 800, 518, 1.2f));
    s.texts.push_back (T ("MIXER/PREAMPLIFIER  MX-207", 343, 22, 10.5f, 4));
    s.texts.push_back (T ("CHANNEL   ASSIGNMENT", 343, 171, 9.5f, 1));
    s.texts.push_back (T ("LEVEL   ADJUSTMENTS", 343, 449, 9.5f, 1));
    s.texts.push_back (T ("PROG", 344, 500, 9.0f, 1)); s.texts.push_back (T ("MON", 344, 599, 9.0f, 1));
    s.texts.push_back (T ("INPUTS", 344, 737, 9.0f, 1));
    s.texts.push_back (T ("L - R", 786, 105, 10.0f, 1)); s.texts.push_back (T ("SIGNAL OUTPUTS", 784, 172, 9.0f, 1));
    s.texts.push_back (T ("monitor", 784, 216, 9.5f)); s.texts.push_back (T ("EXPANSION IN", 784, 328, 9.0f, 1));
    s.texts.push_back (T ("PREAMP", 784, 448, 9.5f, 1)); s.texts.push_back (T ("GAIN", 725, 510, 9.0f, 1));
    s.texts.push_back (T ("70", 752, 540, 9.0f, 1, 1)); s.texts.push_back (T ("55", 752, 570, 9.0f, 1, 1));
    s.texts.push_back (T ("INPUT", 831, 610, 9.0f, 1)); s.texts.push_back (T ("OUTPUT", 725, 649, 9.0f, 1));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 207t", 343, 772, 9.0f, 1));
    scaleSpec (s, 430.0f / 793.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// PS-296 Programmable Spectral Processor - layout of the 296 front (buchla.com photo, 853 x 707 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make296()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_296; s.name = "PROGRAMMABLE SPECTRAL PROCESSOR"; s.model = "PS-296"; s.engr = "296";
    s.width = 853.0f; s.height = 707.0f;
    auto bx = [] (int b) { return 30.0f + 53.0f * (float) b; };
    static const char* hex[16] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F" };
    static const char* hz[16] = { "<100", "150", "250", "350", "500", "630", "800", "1k", "1.3k", "1.6k", "2k", "2.6k", "3.5k", "5k", "8k", ">10k" };
    static const char* att[16] = { "att0", "att1", "att2", "att3", "att4", "att5", "att6", "att7", "att8", "att9", "att10", "att11", "att12", "att13", "att14", "att15" };
    // inputs: 16 local program inputs, even / all / odd signal, frequency and width c.v.
    for (int b = 0; b < 16; ++b) s.jacks.push_back (J ("", K::Banana, false, bx (b), 652));
    s.jacks.push_back (J ("", K::Mini, false, 422, 550)); s.jacks.push_back (J ("", K::Mini, false, 477, 550)); s.jacks.push_back (J ("", K::Mini, false, 532, 550));
    s.jacks.push_back (J ("", K::Banana, false, 222, 567)); s.jacks.push_back (J ("", K::Banana, false, 276, 567));
    // outputs: 16 envelope followers (blue / violet), 16 attenuated channels, comb even / odd, attenuator e / a / o, programmed e / a / o
    for (int b = 0; b < 16; ++b) s.jacks.push_back (JT ("", K::Banana, true, bx (b), 147, (b & 1) ? kTintPurple : 0xff3fb6e0));
    for (int b = 0; b < 16; ++b) s.jacks.push_back (J ("", K::Mini, true, bx (b), 209));
    s.jacks.push_back (J ("", K::Mini, true, 265, 57)); s.jacks.push_back (J ("", K::Mini, true, 320, 57));
    s.jacks.push_back (J ("", K::Mini, true, 447, 57)); s.jacks.push_back (J ("", K::Mini, true, 503, 57)); s.jacks.push_back (J ("", K::Mini, true, 559, 57));
    s.jacks.push_back (J ("", K::Mini, true, 705, 57)); s.jacks.push_back (J ("", K::Mini, true, 762, 57)); s.jacks.push_back (J ("", K::Mini, true, 817, 57));
    for (int b = 0; b < 16; ++b)
    {
        auto c = C (att[b], "", CtrlStyle::Fader, bx (b), 377);
        c.scale2 = "128";
        s.controls.push_back (c);
        if (b >= 5) s.texts.push_back (T (hex[b], bx (b), 116, 9.0f, 1));
        s.texts.push_back (T (hz[b], bx (b), 254, 9.0f, 1));
        if (b < 6 || b > 9) s.texts.push_back (T (hex[b], bx (b), 614, 9.0f, 1));
        s.leds.push_back (D (bx (b) + 16, 488, M296::Led_Band0 + b, 0xff3ad05a));
        if (b < 15) for (int k = 0; k < 11; ++k) s.lines.push_back (L (bx (b) + 6, 284.0f + 18.5f * (float) k, bx (b + 1) - 6, 284.0f + 18.5f * (float) k, 0.7f));
    }
    s.controls.push_back (C ("decay", "", CtrlStyle::Switch3, 100, 64, kSkirt));
    s.controls.push_back (C ("freq", "", CtrlStyle::SmallKnob, 50, 552, kSkirt, "*|*|*|*|*|*|*"));
    s.controls.push_back (C ("fcv", "", CtrlStyle::SmallKnob, 145, 552, kSkirt));
    s.controls.push_back (C ("width", "", CtrlStyle::SmallKnob, 348, 552, kSkirt));
    s.controls.push_back (C ("pre_even", "", CtrlStyle::SmallKnob, 613, 552, kSkirt));
    s.controls.push_back (C ("pre_odd", "", CtrlStyle::SmallKnob, 817, 552, kSkirt));
    s.controls.push_back (C ("tr_even", "", CtrlStyle::Toggle, 683, 562, kSkirt));
    s.controls.push_back (C ("tr_odd", "", CtrlStyle::Toggle, 740, 562, kSkirt));
    s.leds.push_back (D (449, 572, M296::Led_Even)); s.leds.push_back (D (506, 572, M296::Led_Odd));
    const char* db[6] = { "+6", "0db", "-6", "-12", "-18", "-oo" };
    const float dy[6] = { 294, 329, 361, 392, 422, 455 };
    for (int i = 0; i < 6; ++i) s.texts.push_back (T (db[i], 427, dy[i], 9.0f, 1));
    for (float y : { 22.0f, 100.0f, 238.0f, 492.0f, 602.0f, 677.0f }) s.lines.push_back (L (4, y, 849, y));
    for (float x : { 170.0f, 350.0f, 587.0f }) s.lines.push_back (L (x, 22, x, 100));
    for (float x : { 390.0f, 565.0f }) s.lines.push_back (L (x, 492, x, 602));
    s.lines.push_back (L (222, 590, 276, 590, 1.2f)); s.lines.push_back (L (160, 535, 330, 535, 1.2f));
    s.lines.push_back (L (680, 528, 655, 528, 1.2f, false, true)); s.lines.push_back (L (740, 538, 765, 538, 1.2f, false, true));
    s.texts.push_back (T ("PROGRAMMABLE  SPECTRAL  PROCESSOR  MODEL PS-296", 425, 12, 9.5f, 4));
    s.texts.push_back (T ("ENVELOPE", 38, 46, 9.0f, 1, 1)); s.texts.push_back (T ("DECAY", 38, 64, 9.0f, 1, 1)); s.texts.push_back (T ("TIME", 38, 82, 9.0f, 1, 1));
    s.texts.push_back (T ("short", 120, 48, 8.5f, 0, 1)); s.texts.push_back (T ("combo", 120, 64, 8.5f, 0, 1)); s.texts.push_back (T ("long", 120, 80, 8.5f, 0, 1));
    s.texts.push_back (T ("COMB", 180, 46, 9.0f, 1, 1)); s.texts.push_back (T ("FILTER", 180, 64, 9.0f, 1, 1)); s.texts.push_back (T ("OUTPUTS", 180, 82, 9.0f, 1, 1));
    s.texts.push_back (T ("even", 265, 90, 8.5f)); s.texts.push_back (T ("odd", 320, 90, 8.5f));
    s.texts.push_back (T ("ATTEN-", 360, 46, 9.0f, 1, 1)); s.texts.push_back (T ("UATOR", 360, 64, 9.0f, 1, 1)); s.texts.push_back (T ("OUTPUTS", 360, 82, 9.0f, 1, 1));
    s.texts.push_back (T ("even", 447, 90, 8.5f)); s.texts.push_back (T ("all", 503, 90, 8.5f)); s.texts.push_back (T ("odd", 559, 90, 8.5f));
    s.texts.push_back (T ("PRO-", 598, 46, 9.0f, 1, 1)); s.texts.push_back (T ("GRAMMED", 598, 64, 9.0f, 1, 1)); s.texts.push_back (T ("OUTPUTS", 598, 82, 9.0f, 1, 1));
    s.texts.push_back (T ("even", 705, 90, 8.5f)); s.texts.push_back (T ("all", 762, 90, 8.5f)); s.texts.push_back (T ("odd", 817, 90, 8.5f));
    s.texts.push_back (T ("ENVELOPE  FOLLOWER  OUTPUTS", 130, 116, 9.0f, 1));
    s.texts.push_back (T ("630", 18, 512, 8.0f)); s.texts.push_back (T ("2k", 85, 512, 8.0f)); s.texts.push_back (T ("100", 18, 590, 8.0f)); s.texts.push_back (T ("10k", 85, 590, 8.0f));
    s.texts.push_back (T ("-", 118, 512, 8.0f, 3)); s.texts.push_back (T ("+", 172, 512, 8.0f, 3));
    s.texts.push_back (T ("PROGRAM CONTROL", 268, 510, 9.0f, 1)); s.texts.push_back (T ("frequency", 210, 532, 8.5f)); s.texts.push_back (T ("width", 290, 532, 8.5f));
    s.texts.push_back (T ("max", 365, 590, 8.0f));
    s.texts.push_back (T ("SIGNAL   INPUTS", 477, 510, 9.0f, 1));
    s.texts.push_back (T ("even", 422, 590, 8.5f)); s.texts.push_back (T ("all", 477, 590, 8.5f)); s.texts.push_back (T ("odd", 532, 590, 8.5f));
    s.texts.push_back (T ("PROGRAMMED SPECTRUM TRANSFER", 708, 510, 9.0f, 1));
    s.texts.push_back (T ("even", 712, 528, 8.0f)); s.texts.push_back (T ("odd", 712, 540, 8.0f));
    s.texts.push_back (T ("even pre-emph.", 625, 590, 8.0f)); s.texts.push_back (T ("odd pre-emph.", 800, 590, 8.0f));
    s.texts.push_back (T ("LOCAL  PROGRAM  INPUTS", 450, 614, 9.0f, 1));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 296", 425, 692, 9.0f, 1));
    scaleSpec (s, 430.0f / 707.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// MF-248 Multiple Arbitrary Function Generator (MARF) - layout of the 248t front, joined from two photos
// (left part 0..905 px as photographed; the slider part scaled x1.113 and placed from x 873), 1830 x 648 px
// ---------------------------------------------------------------------------------------------
static ModuleSpec make248()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_248; s.name = "MULTIPLE ARBITRARY FUNCTION GENERATOR"; s.model = "MF-248"; s.engr = "248t";
    s.width = 1830.0f; s.height = 648.0f;
    auto X = [] (float x) { return 873.0f + (x - 45.0f) * 1.113f; };      // slider photo -> panel
    auto Y = [] (float y) { return (y - 803.0f) * 1.113f; };
    constexpr juce::uint32 kRedLed = 0xffe8241c, kYel = 0xffffd23a, kGrn = 0xff3ad05a;
    // ---- inputs: external A..D, then per FG: mult c.v., stop, start, strobe, address c.v. ----
    s.jacks.push_back (J ("", K::Banana, false, 873, 443)); s.jacks.push_back (J ("", K::Banana, false, 873, 338));
    s.jacks.push_back (J ("", K::Banana, false, 873, 243)); s.jacks.push_back (J ("", K::Banana, false, 873, 146));
    for (int f = 0; f < 2; ++f)
    {
        const float dx = 264.0f * (float) f;
        s.jacks.push_back (J ("", K::Banana, false, 90 + dx, 206));
        s.jacks.push_back (JT ("", K::Banana, false, 88 + dx, 443, kTintRed, true));
        s.jacks.push_back (JT ("", K::Banana, false, 88 + dx, 571, kTintRed, true));
        s.jacks.push_back (JT ("", K::Banana, false, 175 + dx, 571, kTintRed, true));
        s.jacks.push_back (J ("", K::Banana, false, 250 + dx, 571));
    }
    // ---- outputs per FG: ART, ref, time, voltage, all, pulse 1, pulse 2 ----
    for (int f = 0; f < 2; ++f)
    {
        const float dx = 264.0f * (float) f;
        s.jacks.push_back (JT ("", K::Banana, true, 45 + dx, 146, kBlack));
        s.jacks.push_back (JT ("", K::Banana, true, 102 + dx, 83, kTintPurple));
        s.jacks.push_back (J ("", K::Banana, true, 170 + dx, 83));
        s.jacks.push_back (J ("", K::Banana, true, 235 + dx, 83));
        s.jacks.push_back (J ("", K::Banana, true, 130 + dx, 146, true));
        s.jacks.push_back (J ("", K::Banana, true, 188 + dx, 146, true));
        s.jacks.push_back (J ("", K::Banana, true, 255 + dx, 146, true));
    }
    // ---- function generator controls ----
    static const char* art[2] = { "art1", "art2" }; static const char* mcv[2] = { "mcv1", "mcv2" }; static const char* mult[2] = { "mult1", "mult2" };
    static const char* adv[2] = { "adv1", "adv2" }; static const char* sta[2] = { "start1", "start2" }; static const char* rst[2] = { "reset1", "reset2" };
    static const char* dsp[2] = { "disp1", "disp2" }; static const char* adr[2] = { "addr1", "addr2" }; static const char* cnt[2] = { "cont1", "cont2" }; static const char* ext[2] = { "ext1", "ext2" };
    for (int f = 0; f < 2; ++f)
    {
        const float dx = 264.0f * (float) f;
        s.controls.push_back (C (art[f], "1|2|all", CtrlStyle::Switch3, 30 + dx, 83, kWhite));
        s.controls.push_back (C (mcv[f], "", CtrlStyle::SmallKnob, 90 + dx, 288, kSkirt));
        s.controls.push_back (C (mult[f], "", CtrlStyle::BigKnob, 213 + dx, 258, kSkirt, ".5|*|1|*|2|*|4"));
        s.controls.push_back (C (adv[f], "", CtrlStyle::Push, 55 + dx, 378));
        s.controls.push_back (C (sta[f], "", CtrlStyle::Push, 88 + dx, 508));
        s.controls.push_back (C (rst[f], "", CtrlStyle::Push, 175 + dx, 363));
        s.controls.push_back (C (dsp[f], "", CtrlStyle::Push, 255 + dx, 363));
        s.controls.push_back (C (adr[f], "", CtrlStyle::Knob, 213 + dx, 435, kSkirt, "*|*|*|*|*|*|*|*|*|*|*|*|*|*|*|*"));
        s.controls.push_back (C (cnt[f], "", CtrlStyle::Toggle, 175 + dx, 508, kSkirt));
        s.controls.push_back (C (ext[f], "", CtrlStyle::Toggle, 250 + dx, 508, kSkirt));
        s.leds.push_back (D (278 + dx, 71, 100 + f * 10 + 3, kRedLed));
        s.leds.push_back (D (278 + dx, 96, 100 + f * 10 + 4, kRedLed));
        s.leds.push_back (D (123 + dx, 356, 100 + f * 10 + 2, kRedLed));
        s.leds.push_back (D (123 + dx, 383, 100 + f * 10 + 1, kYel));
        s.leds.push_back (D (123 + dx, 408, 100 + f * 10 + 0, kGrn));
        s.leds.push_back (D (213 + dx, 356, 100 + f * 10 + 5, kRedLed));
        s.texts.push_back (T ("PROGRAMMED  OUTPUTS", 165 + dx, 50, 9.0f, 1));
        s.texts.push_back (T ("ART", 38 + dx, 116, 8.0f, 1)); s.texts.push_back (T ("ref.", 102 + dx, 112, 8.0f)); s.texts.push_back (T ("time", 170 + dx, 112, 8.0f));
        s.texts.push_back (T ("voltage", 235 + dx, 112, 8.0f)); s.texts.push_back (T ("all", 130 + dx, 175, 8.0f)); s.texts.push_back (T ("1", 188 + dx, 175, 8.0f)); s.texts.push_back (T ("2", 255 + dx, 175, 8.0f));
        s.texts.push_back (T ("TIME MULTIPLIER", 215 + dx, 190, 8.5f, 1));
        s.texts.push_back (T ("-", 55 + dx, 250, 8.0f, 3)); s.texts.push_back (T ("+", 125 + dx, 250, 8.0f, 3));
        s.texts.push_back (T ("MODE", 88 + dx, 338, 8.5f, 1)); s.texts.push_back (T ("advance", 57 + dx, 405, 8.0f));
        s.texts.push_back (T ("stop", 88 + dx, 478, 8.0f)); s.texts.push_back (T ("start", 88 + dx, 535, 8.0f));
        s.texts.push_back (T ("STAGE ADDRESS", 213 + dx, 338, 8.5f, 1)); s.texts.push_back (T ("reset", 175 + dx, 388, 8.0f)); s.texts.push_back (T ("display", 255 + dx, 388, 8.0f));
        s.texts.push_back (T ("cont.", 175 + dx, 478, 8.0f)); s.texts.push_back (T ("strobe", 175 + dx, 538, 8.0f));
        s.texts.push_back (T ("int.", 250 + dx, 478, 8.0f)); s.texts.push_back (T ("ext.", 250 + dx, 538, 8.0f));
        s.lines.push_back (L (10 + dx, 175, 285 + dx, 175)); s.lines.push_back (L (10 + dx, 328, 285 + dx, 328));
        s.lines.push_back (L (143 + dx, 328, 143 + dx, 640));
    }
    for (float x : { 287.0f, 552.0f, 835.0f }) s.lines.push_back (L (x, 30, x, 640));
    // ---- programming section: LEDs (edit stage) and levers ----
    auto prog = [&] (const char* fn, const char* lab, float x, float y, juce::uint32 tip = 0) { auto c = C (fn, lab, CtrlStyle::Prog, x, y); c.colour = tip; s.controls.push_back (c); };
    auto pled = [&] (int fnI, int v, float x, float y) { s.leds.push_back (D (x, y, 200 + fnI * 8 + v, kRedLed)); };
    pled (M248::F_P1, 0, 575, 83); pled (M248::F_P2, 0, 633, 83);
    prog ("p1", "", 575, 118); prog ("p2", "", 633, 118);
    prog ("clear", "", 715, 88, 0xfff2f2ee);
    prog ("stage", "", 800, 88, 0xff2a2b30);
    const float px[5] = { 577, 633, 688, 743, 797 };
    pled (M248::F_Quant, 0, px[0], 163); pled (M248::F_Slope, 0, px[1], 163); pled (M248::F_Range, 0, px[2], 163); pled (M248::F_Range, 1, px[3], 163); pled (M248::F_VExt, 0, px[4], 163);
    prog ("quant", "", px[0], 215); prog ("slope", "", px[1], 215); prog ("range", "=0", px[2], 215); prog ("range", "=1", px[3], 215); prog ("vext", "", px[4], 215);
    static const char* lim[5] = { "=2", "=3", "=4", "=5", "=6" };
    for (int i = 0; i < 5; ++i) { pled (M248::F_Range, 2 + i, px[i], 275); prog ("range", lim[i], px[i], 335); }
    pled (M248::F_Stop, 0, px[0], 398); pled (M248::F_Sust, 0, px[1], 398); pled (M248::F_Enable, 0, px[2], 398); pled (M248::F_First, 0, px[3], 398); pled (M248::F_Last, 0, px[4], 398);
    prog ("stop", "", px[0], 448); prog ("sust", "", px[1], 448, 0xffe8c22a); prog ("enable", "", px[2], 448, 0xffe8c22a); prog ("first", "", px[3], 448); prog ("last", "", px[4], 448);
    static const char* tr[4] = { "=0", "=1", "=2", "=3" };
    for (int i = 0; i < 4; ++i) { pled (M248::F_TRange, i, px[i], 518); prog ("trange", tr[i], px[i], 570); }
    pled (M248::F_TExt, 0, px[4], 518); prog ("text", "", px[4], 570);
    s.texts.push_back (T ("OUTPUT  PULSES", 605, 50, 8.5f, 1)); s.texts.push_back (T ("1", 590, 120, 8.0f)); s.texts.push_back (T ("2", 648, 120, 8.0f));
    s.texts.push_back (T ("CLEAR", 715, 50, 8.5f, 1)); s.texts.push_back (T ("STAGE  NO", 800, 50, 8.5f, 1));
    s.texts.push_back (T ("<", 776, 90, 9.0f, 1)); s.texts.push_back (T (">", 824, 90, 9.0f, 1));
    s.texts.push_back (T ("OUTPUT VOLTAGE", 612, 143, 8.5f, 1)); s.texts.push_back (T ("- Range -", 715, 143, 8.0f)); s.texts.push_back (T ("- Source -", 797, 143, 8.0f));
    const char* l1[5] = { "quantize", "sloped", "full", "half", "external" };
    const char* l2[5] = { "cont.", "stepped", "", "", "internal" };
    const char* l3[5] = { "+0", "+2", "+4", "+6", "+8" };
    const char* l4[5] = { "stop", "sust.", "enable", "first", "last" };
    const char* l5[5] = { ".03", ".3", "3", "30", "external" };
    const char* l6[5] = { ".002", ".02", ".2", "2", "internal" };
    for (int i = 0; i < 5; ++i)
    {
        s.texts.push_back (T (l1[i], px[i], 184, 7.5f)); s.texts.push_back (T (l2[i], px[i], 246, 7.5f));
        s.texts.push_back (T (l3[i], px[i], 296, 7.5f)); s.texts.push_back (T (l4[i], px[i], 419, 7.5f));
        s.texts.push_back (T (l5[i], px[i], 540, 7.5f)); s.texts.push_back (T (l6[i], px[i], 600, 7.5f));
    }
    s.texts.push_back (T ("limited", 715, 246, 7.5f));
    s.texts.push_back (T ("OPERATING MODE", 618, 372, 8.5f, 1)); s.texts.push_back (T ("- Cycle -", 770, 372, 8.0f));
    s.texts.push_back (T ("(if start pulse high)", 660, 476, 7.5f)); s.texts.push_back (T ("stage", px[3], 476, 7.5f)); s.texts.push_back (T ("stage", px[4], 476, 7.5f));
    s.texts.push_back (T ("TIME RANGE (seconds)", 625, 498, 8.5f, 1)); s.texts.push_back (T ("- Source -", 797, 498, 8.0f));
    for (float y : { 133.0f, 357.0f, 488.0f }) s.lines.push_back (L (552, y, 835, y));
    s.lines.push_back (L (668, 30, 668, 133)); s.lines.push_back (L (760, 30, 760, 133));
    // ---- external inputs, load / save ----
    s.texts.push_back (T ("EXTERNAL", 873, 46, 8.5f, 1)); s.texts.push_back (T ("INPUTS", 873, 60, 8.5f, 1));
    s.texts.push_back (T ("D", 873, 105, 8.5f)); s.texts.push_back (T ("C", 873, 202, 8.5f)); s.texts.push_back (T ("B", 873, 300, 8.5f)); s.texts.push_back (T ("A", 873, 402, 8.5f));
    s.controls.push_back (C ("load", "", CtrlStyle::Push, 888, 518)); s.controls.push_back (C ("save", "", CtrlStyle::Push, 888, 572));
    s.leds.push_back (D (852, 518, 320, kRedLed)); s.leds.push_back (D (852, 572, 321, kRedLed));
    s.texts.push_back (T ("load", 852, 545, 7.5f)); s.texts.push_back (T ("save", 852, 600, 7.5f));
    s.lines.push_back (L (835, 488, 912, 488)); s.lines.push_back (L (912, 30, 912, 640));
    // ---- sliders ----
    static const char* lv[16] = { "lvl1", "lvl2", "lvl3", "lvl4", "lvl5", "lvl6", "lvl7", "lvl8", "lvl9", "lvl10", "lvl11", "lvl12", "lvl13", "lvl14", "lvl15", "lvl16" };
    static const char* tm[16] = { "tim1", "tim2", "tim3", "tim4", "tim5", "tim6", "tim7", "tim8", "tim9", "tim10", "tim11", "tim12", "tim13", "tim14", "tim15", "tim16" };
    static const char* num[16] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
    const float sx[16] = { 112, 162, 212, 262, 311, 361, 410, 460, 510, 560, 610, 660, 709, 759, 809, 858 };
    for (int i = 0; i < 16; ++i)
    {
        auto a = C (lv[i], "", CtrlStyle::Fader, X (sx[i]), Y (950)); a.scale2 = "118"; s.controls.push_back (a);
        auto b = C (tm[i], "", CtrlStyle::Fader, X (sx[i]), Y (1180)); b.scale2 = "118"; s.controls.push_back (b);
        s.texts.push_back (T (num[i], X (sx[i]), Y (1048), 8.0f));
        s.leds.push_back (D (X (sx[i]), Y (1066), i, kRedLed));
        if (i < 15)
            for (int k = 0; k < 9; ++k)
            {
                s.lines.push_back (L (X (sx[i]) + 7, Y (875 + 18.5f * (float) k), X (sx[i + 1]) - 7, Y (875 + 18.5f * (float) k), 0.7f));
                s.lines.push_back (L (X (sx[i]) + 7, Y (1105 + 18.5f * (float) k), X (sx[i + 1]) - 7, Y (1105 + 18.5f * (float) k), 0.7f));
            }
    }
    const char* vs[5] = { "2.0", "1.5", "1.0", ".5", "0" };
    const char* ts[6] = { "30", "20", "10", "5", "3", "2" };
    for (float x : { 335.0f, 634.0f })
    {
        for (int i = 0; i < 5; ++i) s.texts.push_back (T (vs[i], X (x), Y (880 + 35.0f * (float) i), 7.5f, 1));
        for (int i = 0; i < 6; ++i) s.texts.push_back (T (ts[i], X (x), Y (1110 + 28.0f * (float) i), 7.5f, 1));
    }
    const char* dcba[4] = { "D", "C", "B", "A" };
    for (float x : { 88.0f, 884.0f })
        for (int i = 0; i < 4; ++i) { s.texts.push_back (T (dcba[i], X (x), Y (878 + 47.0f * (float) i), 7.5f)); s.texts.push_back (T (dcba[i], X (x), Y (1108 + 47.0f * (float) i), 7.5f)); }
    s.texts.push_back (T ("OUTPUT VOLTAGE LEVEL", X (485), Y (848), 8.5f, 1));
    s.texts.push_back (T ("INTERVAL TIME", X (485), Y (1083), 8.5f, 1));
    // ---- number / key / scale buttons ----
    static const char* nb[12] = { "n1", "n2", "n3", "n4", "n5", "n6", "n7", "n8", "n9", "n10", "n11", "n12" };
    const char* nl[12] = { "1/C", "2/C#", "3/D", "4/D#", "5/E", "6/F", "7/F#", "8/G", "9/G#", "10/A", "11/A#", "12/B" };
    for (int i = 0; i < 12; ++i)
    {
        s.controls.push_back (C (nb[i], "", CtrlStyle::Push, X (sx[i]), Y (1312)));
        s.leds.push_back (D (X (sx[i]) - 25, Y (1290), 300 + i, kRedLed));
        s.texts.push_back (T (nl[i], X (sx[i]), Y (1340), 7.0f));
    }
    s.texts.push_back (T ("M", X (560), Y (1284), 7.5f, 1)); s.texts.push_back (T ("m", X (610), Y (1284), 7.5f, 1)); s.texts.push_back (T ("ch", X (660), Y (1284), 7.5f, 1));
    s.controls.push_back (C ("scale", "", CtrlStyle::Push, X (759), Y (1312))); s.leds.push_back (D (X (735), Y (1290), 322, kRedLed));
    s.controls.push_back (C ("key", "", CtrlStyle::Push, X (809), Y (1312))); s.leds.push_back (D (X (785), Y (1290), 323, kRedLed));
    s.controls.push_back (C ("tune", "", CtrlStyle::Push, X (858), Y (1312)));
    s.texts.push_back (T ("Scale", X (759), Y (1340), 7.5f)); s.texts.push_back (T ("Key", X (809), Y (1340), 7.5f));
    s.texts.push_back (T ("press autotune", X (868), Y (1284), 7.0f)); s.texts.push_back (T ("hold initialtune", X (868), Y (1340), 7.0f));
    s.lines.push_back (L (912, Y (1275), 1826, Y (1275))); s.lines.push_back (L (X (710), Y (1275), X (710), 640)); s.lines.push_back (L (X (833), Y (1275), X (833), 640));
    s.lines.push_back (L (4, 30, 1826, 30)); s.lines.push_back (L (4, 640, 1826, 640)); s.lines.push_back (L (912, Y (838), 1826, Y (838)));
    s.texts.push_back (T ("MULTIPLE ARBITRARY FUNCTION GENERATOR  MODEL MF-248", 915, 15, 10.0f, 4));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 248t", 1300, Y (1372), 8.5f, 1));
    scaleSpec (s, 430.0f / 648.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// KB-218 Touch Activated Voltage Source - layout of the 218e front (photo, 714 x 307 px): navy panel, red knobs,
// strip and preset pads on the red band, 29 touch plates
// ---------------------------------------------------------------------------------------------
static ModuleSpec make218()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_218; s.name = "TOUCH ACTIVATED VOLTAGE SOURCE"; s.model = "KB-218"; s.engr = "218e";
    s.width = 714.0f; s.height = 307.0f;
    s.bg = 0xff1f2c6c; s.ink = 0xfff2f2f2;
    constexpr juce::uint32 kRed = 0xffd23a2a;
    s.jacks = {
        J ("", K::Banana, false, 187, 42),                               // portamento c.v.
        JT ("", K::Banana, false, 272, 42, kTintOrange, true),           // arpeggiator pulse input
        J ("", K::Banana, true, 28, 36, true),                           // pulse
        JT ("", K::Banana, true, 62, 36, kTintPurple),                   // pressure
        J ("", K::Banana, true, 96, 36),                                 // pitch
        J ("", K::Banana, true, 62, 62),                                 // strip
        J ("", K::Banana, true, 28, 62, true),                           // strip pulse
        J ("", K::Banana, true, 432, 55),                                // preset output
        J ("", K::Banana, true, 432, 30, true),                          // preset pulse
    };
    s.controls = {
        C ("porta", "", CtrlStyle::Knob, 145, 42, kRed, "0|*|2|*|4|*|6|*|8|*|10"),
        C ("rate", "", CtrlStyle::Knob, 232, 42, kRed, "0|*|2|*|4|*|6|*|8|*|10"),
        C ("arp", "ascending|random|none", CtrlStyle::Switch3, 300, 42, kWhite),
        C ("add", "octaves|preset|none", CtrlStyle::Switch3, 372, 42, kWhite),
        C ("preset1", "", CtrlStyle::Knob, 478, 42, kRed, "0|*|2|*|4|*|6|*|8|*|10"),
        C ("preset2", "", CtrlStyle::Knob, 537, 42, kRed, "0|*|2|*|4|*|6|*|8|*|10"),
        C ("preset3", "", CtrlStyle::Knob, 597, 42, kRed, "0|*|2|*|4|*|6|*|8|*|10"),
        C ("preset4", "", CtrlStyle::Knob, 657, 42, kRed, "0|*|2|*|4|*|6|*|8|*|10"),
    };
    // touch surfaces (hit areas, drawn by the panel): strip, four preset pads, the plates
    s.sections = { { "strip", 26, 83, 200, 17 }, { "pad", 470, 84, 36, 15 }, { "pad", 530, 84, 36, 15 }, { "pad", 590, 84, 36, 15 }, { "pad", 650, 84, 36, 15 },
                   { "keys", 22, 108, 672, 186 } };
    s.leds.push_back (D (46, 24, M218::Led_Pulse));
    for (int i = 0; i < 4; ++i) s.leds.push_back (D (461 + 60.0f * (float) i, 92, M218::Led_Pad0 + i, 0xfff6e25a));
    for (int i = 0; i < 10; ++i) s.leds.push_back (D (40 + 19.5f * (float) i, 104, M218::Led_Strip0 + i, 0xff4aa0ff));
    for (int i = 0; i < M218::kKeys; ++i) s.leds.push_back (D (22.0f + 672.0f * ((float) i + 0.5f) / (float) M218::kKeys, 298, M218::Led_Key0 + i, 0xff4aa0ff));
    s.texts = {
        T ("TOUCH ACTIVATED VOLTAGE SOURCE  MODEL KB-218", 430, 9, 9.0f, 1),
        T ("pulse", 28, 52, 7.5f), T ("pressure", 62, 52, 7.5f), T ("pitch", 96, 52, 7.5f), T ("strip", 84, 70, 7.5f, 0, 1),
        T ("input", 187, 58, 7.5f), T ("input", 272, 58, 7.5f), T ("output", 432, 72, 7.5f),
        T ("PORTAMENTO SLOPE", 165, 74, 8.0f, 1), T ("ARPEGGIATION RATE / PATTERN", 262, 74, 8.0f, 1),
        T ("ADD TO PITCH", 392, 74, 8.0f, 1), T ("PRESET VOLTAGE SOURCE", 568, 74, 8.0f, 1),
        T ("CALABI YAU", 330, 92, 12.0f, 1),
    };
    s.lines = { L (4, 18, 710, 18), L (452, 70, 452, 80, 1.0f), L (355, 42, 345, 42, 1.2f, false, true) };
    scaleSpec (s, 430.0f / 307.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// AF-250 Dual Arbitrary Function Generator - layout of the 250e front (buchla.com photo, 1097 x 902 px):
// sixteen stages on an elliptical ring, voltage knobs outside, time knobs inside, the edit centre
// ---------------------------------------------------------------------------------------------
static ModuleSpec make250()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_250; s.name = "DUAL ARBITRARY FUNCTION GENERATOR"; s.model = "AF-250"; s.engr = "250e";
    s.width = 1097.0f; s.height = 902.0f;
    constexpr float cx = 555, cy = 447;
    constexpr juce::uint32 kOr = 0xffff9a2a, kRedL = 0xffe8241c;
    auto ang = [] (int n) { return juce::MathConstants<float>::twoPi * (float) n / 16.0f; };
    s.jacks = {
        J ("", K::Banana, false, 93, 825), J ("", K::Banana, false, 173, 825), J ("", K::Banana, false, 253, 825), J ("", K::Banana, false, 330, 825),
        J ("", K::Banana, false, 50, 180),                                       // time mult c.v.
        JT ("", K::Banana, false, 45, 365, kTintOrange, true),                   // start / adv
        JT ("", K::Banana, false, 45, 580, kTintOrange, true),                   // stop
        JT ("", K::Banana, false, 825, 825, kTintOrange, true),                  // address pulse
        J ("", K::Banana, false, 895, 825),                                      // address c.v.
        J ("", K::Banana, true, 60, 95),                                         // cv out (time)
        J ("", K::Banana, true, 980, 98), J ("", K::Banana, true, 1053, 98),     // cv outputs 1, 2
        J ("", K::Banana, true, 1050, 285, true), J ("", K::Banana, true, 1050, 358, true), J ("", K::Banana, true, 1050, 525, true),
    };
    static const char* vk[16] = { "v{s}_1", "v{s}_2", "v{s}_3", "v{s}_4", "v{s}_5", "v{s}_6", "v{s}_7", "v{s}_8", "v{s}_9", "v{s}_10", "v{s}_11", "v{s}_12", "v{s}_13", "v{s}_14", "v{s}_15", "v{s}_16" };
    static const char* tk[16] = { "t_1", "t_2", "t_3", "t_4", "t_5", "t_6", "t_7", "t_8", "t_9", "t_10", "t_11", "t_12", "t_13", "t_14", "t_15", "t_16" };
    static const char* num[16] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
    // Every sector carries, from the rim inwards and all on the sector's own radial line:
    // the big voltage knob, the stage LED with its number, the small time knob, and the pulse LED.
    // All the rings share one aspect ratio (RY / RX) so a parametric angle means the same direction on each.
    constexpr float RX = 455.0f, RY = 375.0f;
    auto ring = [] (float k, float t, float& x, float& y) { x = RX * k * std::sin (t); y = -RY * k * std::cos (t); };
    for (int i = 0; i < 16; ++i)
    {
        const float a = ang (i);
        float x, y;
        ring (0.846f, a, x, y);  s.controls.push_back (C (vk[i], "", CtrlStyle::Knob, cx + x, cy + y, 0xff2a2c33));
        ring (0.690f, a, x, y);  s.leds.push_back (D (cx + x, cy + y, i, 0xfff4f4f0));
        ring (0.690f, a - 0.095f, x, y);
        s.texts.push_back (T (num[i], cx + x, cy + y, 8.5f, 1));
        ring (0.576f, a, x, y);  s.controls.push_back (C (tk[i], "", CtrlStyle::SmallKnob, cx + x, cy + y, 0xff2a2c33));
        ring (0.460f, a, x, y);  s.leds.push_back (D (cx + x, cy + y, 20 + i, kOr));
        // the spoke that closes the sector, from the inner ellipse out to the rim
        const float sa = a + juce::MathConstants<float>::pi / 16.0f;
        float x0, y0, x1, y1;
        ring (0.505f, sa, x0, y0); ring (1.0f, sa, x1, y1);
        s.lines.push_back (L (cx + x0, cy + y0, cx + x1, cy + y1, 0.9f));
    }
    for (int k = 0; k < 64; ++k)   // the two ellipses
    {
        const float a0 = juce::MathConstants<float>::twoPi * (float) k / 64.0f, a1 = juce::MathConstants<float>::twoPi * (float) (k + 1) / 64.0f;
        s.lines.push_back (L (cx + 230 * std::sin (a0), cy - 190 * std::cos (a0), cx + 230 * std::sin (a1), cy - 190 * std::cos (a1), 1.0f));
        s.lines.push_back (L (cx + 455 * std::sin (a0), cy - 375 * std::cos (a0), cx + 455 * std::sin (a1), cy - 375 * std::cos (a1), 1.0f));
        s.lines.push_back (L (cx + 190 * std::sin (a0), cy - 157 * std::cos (a0), cx + 190 * std::sin (a1), cy - 157 * std::cos (a1), 1.0f));
    }
    s.lines.push_back (L (cx - 190, cy + 5, cx + 190, cy + 5, 1.0f)); s.lines.push_back (L (cx, cy + 5, cx, cy + 125, 1.0f));
    // centre: edit knob, set, remote enable, edit; displays "jump to" / "& loop" are drawn by the panel
    s.controls.push_back (C ("edit", "", CtrlStyle::Knob, 555, 440, 0xff2a2c33));
    s.controls.push_back (C ("set", "", CtrlStyle::Push, 458, 415));
    s.controls.push_back (C ("remote", "", CtrlStyle::Push, 497, 515));
    s.controls.push_back (C ("editbtn", "", CtrlStyle::Push, 612, 515));
    s.sections = { { "jump", 528, 352, 58, 44 }, { "loop", 618, 388, 64, 48 } };
    s.leds.push_back (D (450, 490, 67, kRedL)); s.leds.push_back (D (657, 490, 68, kRedL));
    // left: time
    s.controls.push_back (C ("tq", "", CtrlStyle::Push, 133, 108)); s.controls.push_back (C ("ti", "", CtrlStyle::Push, 205, 108));
    s.controls.push_back (C ("trng", "", CtrlStyle::Push, 345, 75));
    s.controls.push_back (C ("mcv", "", CtrlStyle::SmallKnob, 145, 185, 0xff2a2c33));
    s.controls.push_back (C ("mult", "", CtrlStyle::Knob, 70, 265, 0xff2a2c33, ".25|.5|1|2|4"));
    s.leds.push_back (D (110, 80, 41, kOr)); s.leds.push_back (D (180, 80, 42, kOr));
    s.leds.push_back (D (300, 52, 43, kOr)); s.leds.push_back (D (300, 72, 44, kOr)); s.leds.push_back (D (300, 92, 45, kOr)); s.leds.push_back (D (300, 112, 69, kOr));
    // left: start / stop / mode / sources
    s.controls.push_back (C ("start", "", CtrlStyle::Push, 50, 445, 0xff35b48a)); s.controls.push_back (C ("stop", "", CtrlStyle::Push, 50, 505, 0xffe05a5a));
    s.controls.push_back (C ("mode", "", CtrlStyle::Push, 127, 676));
    s.controls.push_back (C ("text", "", CtrlStyle::Push, 50, 767)); s.controls.push_back (C ("vext", "", CtrlStyle::Push, 130, 767)); s.controls.push_back (C ("fix", "", CtrlStyle::Push, 212, 767));
    s.leds.push_back (D (95, 355, 40, 0xff3ad05a));
    s.leds.push_back (D (25, 637, 58, kOr)); s.leds.push_back (D (25, 662, 59, kOr)); s.leds.push_back (D (25, 688, 60, kOr)); s.leds.push_back (D (95, 637, 61, kRedL));
    s.leds.push_back (D (25, 727, 62, kOr)); s.leds.push_back (D (105, 727, 63, kOr)); s.leds.push_back (D (185, 727, 64, kOr));
    // right: voltage
    s.controls.push_back (C ("vset", "", CtrlStyle::Push, 757, 75));
    s.controls.push_back (C ("vq", "", CtrlStyle::Push, 835, 108)); s.controls.push_back (C ("vi", "", CtrlStyle::Push, 905, 108));
    s.controls.push_back (C ("vrng", "", CtrlStyle::Push, 1050, 200));
    s.leds.push_back (D (680, 58, 46, kOr)); s.leds.push_back (D (708, 58, 47, kOr));
    s.leds.push_back (D (815, 80, 48, kOr)); s.leds.push_back (D (880, 80, 49, kOr));
    s.leds.push_back (D (880, 153, 50, kOr)); s.leds.push_back (D (907, 173, 51, kOr)); s.leds.push_back (D (935, 195, 52, kOr));
    s.leds.push_back (D (960, 220, 53, kOr)); s.leds.push_back (D (975, 153, 54, kOr)); s.leds.push_back (D (975, 173, 55, kOr));
    // right: pulses, addressing
    s.controls.push_back (C ("p1", "", CtrlStyle::Push, 1050, 447)); s.controls.push_back (C ("p2", "", CtrlStyle::Push, 1050, 608));
    s.leds.push_back (D (1072, 408, 56, kRedL)); s.leds.push_back (D (1072, 568, 57, kRedL));
    s.controls.push_back (C ("addrbtn", "", CtrlStyle::Push, 1050, 705));
    s.leds.push_back (D (1015, 690, 65, kOr)); s.leds.push_back (D (1015, 717, 66, kOr));
    s.controls.push_back (C ("offset", "", CtrlStyle::Knob, 1010, 800, 0xff2a2c33, "1|4|7|10|13|16"));
    for (auto t : { T ("DUAL ARBITRARY FUNCTION GENERATOR  MODEL AF-250", 548, 18, 10.0f, 4),
                    T ("cv out", 60, 55, 9.0f), T ("quant", 133, 55, 9.0f), T ("interp", 205, 55, 9.0f),
                    T (".01-.1", 268, 52, 8.5f, 0, 2), T (".08-.8", 268, 72, 8.5f, 0, 2), T (".6-6", 268, 92, 8.5f, 0, 2), T ("sync", 268, 112, 8.5f, 0, 2),
                    T ("time", 385, 62, 9.0f, 0, 1), T ("range", 385, 76, 9.0f, 0, 1), T ("edit mode", 612, 540, 8.5f), T ("TIME", 495, 55, 10.0f, 1), T ("VOLTAGE", 612, 55, 10.0f, 1),
                    T ("1", 688, 76, 8.5f), T ("2", 716, 76, 8.5f), T ("quant", 835, 55, 9.0f), T ("interp", 905, 55, 9.0f),
                    T ("1", 968, 72, 8.5f), T ("outputs", 1030, 55, 8.5f), T ("2", 1066, 72, 8.5f),
                    T ("0-2", 895, 153, 8.0f, 0, 1), T ("2-4", 922, 173, 8.0f, 0, 1), T ("4-6", 950, 195, 8.0f, 0, 1), T ("6-8", 975, 220, 8.0f, 0, 1),
                    T ("8-10", 990, 153, 8.0f, 0, 1), T ("all", 990, 173, 8.0f, 0, 1), T ("voltge", 1050, 155, 8.5f), T ("range", 1050, 168, 8.5f),
                    T ("time", 190, 155, 8.5f, 0, 1), T ("mult", 190, 168, 8.5f, 0, 1),                     T ("start/adv", 50, 410, 8.5f), T ("stop", 45, 542, 8.5f), T ("M", 98, 595, 8.5f, 1), T ("O", 104, 610, 8.5f, 1), T ("D", 110, 625, 8.5f, 1), T ("E", 116, 640, 8.5f, 1),
                    T ("adv", 40, 637, 8.5f, 0, 1), T ("sust", 40, 662, 8.5f, 0, 1), T ("enble", 40, 688, 8.5f, 0, 1), T ("stop", 110, 637, 8.5f, 0, 1),
                    T ("time", 40, 727, 8.5f, 0, 1), T ("vltg", 120, 727, 8.5f, 0, 1), T ("fix", 200, 727, 8.5f, 0, 1),
                    T ("A", 68, 840, 8.5f), T ("B", 150, 840, 8.5f), T ("C", 230, 840, 8.5f), T ("D", 305, 840, 8.5f),
                    T ("EXTERNAL INPUTS", 455, 845, 9.5f, 1), T ("STAGE ADDRESSING", 680, 845, 9.5f, 1),
                    T ("jump to", 483, 372, 9.0f), T ("& loop", 650, 375, 9.0f), T ("set", 425, 420, 9.0f),
                    T ("remote", 460, 458, 8.5f), T ("enable", 460, 472, 8.5f), T ("edit", 612, 472, 8.5f),
                    T ("all", 1050, 255, 9.0f), T ("1", 1050, 408, 8.5f), T ("2", 1050, 568, 8.5f), T ("PULSE", 1030, 640, 9.0f, 1), T ("OUTPUTS", 1035, 655, 9.0f, 1),
                    T ("strobed", 955, 690, 8.5f), T ("continuous", 955, 717, 8.5f), T ("offset", 945, 745, 8.5f),
                    T ("pulse", 825, 783, 8.5f), T ("cv in", 895, 783, 8.5f),
                    T ("CALABI YAU  -  after Buchla 250e", 548, 885, 9.0f, 1) })
        s.texts.push_back (t);
    for (auto l : { L (4, 36, 1093, 36), L (4, 868, 1093, 868), L (4, 340, 110, 340), L (4, 705, 270, 705), L (990, 665, 1093, 665), L (990, 245, 1093, 245),
                    L (50, 150, 50, 140), L (50, 140, 145, 140), L (145, 140, 145, 160) })
        s.lines.push_back (l);
    scaleSpec (s, 430.0f / 902.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// RG-252 Polyphonic Rhythm Generator - layout of the 252e front (buchla.com photo, 1128 x 921 px): the round display
// of eleven rings in the middle (drawn and clicked by the panel), cell edit on the left, clocks on the right
// ---------------------------------------------------------------------------------------------
static ModuleSpec make252()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_252; s.name = "POLYPHONIC RHYTHM GENERATOR"; s.model = "RG-252"; s.engr = "252e";
    s.width = 1128.0f; s.height = 921.0f;
    constexpr juce::uint32 kOr = 0xffff9a2a, kRedL = 0xffe8241c, kGrnL = 0xff3ad05a, kBluL = 0xff3a8ae8, kKnob = 0xff23252b;
    const float cy3[3] = { 390, 568, 748 };
    s.jacks = {
        J ("", K::Banana, false, 1085, 383), JT ("", K::Banana, false, 1085, 465, kTintOrange, true),
        J ("", K::Banana, false, 1085, 568), JT ("", K::Banana, false, 1085, 655, kTintOrange, true),
        J ("", K::Banana, false, 1085, 745), JT ("", K::Banana, false, 1085, 830, kTintOrange, true),
        J ("", K::Banana, true, 62, 100, true), J ("", K::Banana, true, 150, 100, true), J ("", K::Banana, true, 235, 100, true),
        JT ("", K::Banana, true, 312, 100, 0xfff4f4f0), JT ("", K::Banana, true, 390, 100, 0xfff4f4f0),
        JT ("", K::Banana, true, 495, 100, 0xff3fb6e0), JT ("", K::Banana, true, 585, 100, 0xff3fb6e0), JT ("", K::Banana, true, 683, 100, 0xff3fb6e0),
        JT ("", K::Banana, true, 775, 100, kTintPurple), JT ("", K::Banana, true, 870, 100, kTintPurple), JT ("", K::Banana, true, 962, 100, kTintPurple),
    };
    s.leds = { D (20, 78, 0, kRedL), D (108, 78, 1, kGrnL), D (190, 78, 2, kBluL), D (275, 60, 3, 0xfff6e25a), D (350, 60, 4, 0xfff6e25a) };
    const float cvx[6] = { 495, 585, 683, 775, 870, 962 };
    for (int o = 0; o < 6; ++o) s.leds.push_back (D (cvx[o] - 38, 78, 80 + o, o % 3 == 0 ? kRedL : (o % 3 == 1 ? kGrnL : kBluL)));
    s.controls = {
        C ("remote", "", CtrlStyle::Push, 1055, 100),
        C ("red", "", CtrlStyle::Push, 50, 215), C ("green", "", CtrlStyle::Push, 120, 215), C ("blue", "", CtrlStyle::Push, 185, 215),
        C ("play", "", CtrlStyle::Push, 268, 220), C ("reset", "", CtrlStyle::Push, 862, 765),
        C ("display", "", CtrlStyle::Push, 905, 290),
        C ("cvselect", "", CtrlStyle::Push, 1088, 205),
        C ("@cv", "", CtrlStyle::Knob, 1043, 278, kKnob),
        C ("pdur", "", CtrlStyle::Knob, 78, 312, kKnob),
        C ("@phase", "", CtrlStyle::Knob, 212, 312, kKnob),
        C ("@offset", "", CtrlStyle::Knob, 78, 452, kKnob),
        C ("editbtn", "", CtrlStyle::Push, 212, 430),
        C ("clear", "", CtrlStyle::Push, 212, 512),
        C ("@subc", "", CtrlStyle::Knob, 78, 592, kKnob),
        C ("subcount", "", CtrlStyle::Push, 212, 592),
        C ("copy", "", CtrlStyle::Push, 52, 678), C ("paste", "", CtrlStyle::Push, 212, 678),
        C ("rhythm", "", CtrlStyle::Push, 300, 760),
        C ("cell", "", CtrlStyle::Knob, 72, 830, kKnob),
        C ("ring", "", CtrlStyle::Knob, 285, 830, kKnob),
        C ("pattern", "", CtrlStyle::Knob, 495, 830, kKnob),
        C ("bpm", "", CtrlStyle::Knob, 770, 830, kKnob),
        C ("sync", "", CtrlStyle::Push, 700, 760),
    };
    static const char* ck[3] = { "clk1", "clk2", "clk3" }; static const char* ab[3] = { "assign1", "assign2", "assign3" };
    const char* ringLab[11] = { "3", "", "5", "", "7", "", "9", "", "11", "", "16" };
    for (int k = 0; k < 3; ++k)
    {
        s.controls.push_back (C (ck[k], "", CtrlStyle::Knob, 1003, cy3[k], kKnob));
        s.controls.push_back (C (ab[k], "", CtrlStyle::Push, 1003, cy3[k] + 77));
        s.leds.push_back (D (965, cy3[k] + 50, 50 + k, kGrnL));
        for (int r = 0; r < 11; ++r)
        {
            const float y = cy3[k] - 12 + 115.0f * (1.0f - (float) r / 10.0f);
            const float x = (r % 2 == 0) ? 920.0f : 945.0f;
            s.leds.push_back (D (x, y, 10 + k * 11 + r, 0xfff6e25a));
            if (ringLab[r][0] != 0) s.texts.push_back (T (ringLab[r], 900, y + 4, 8.0f, 0, 2));
        }
        s.lines.push_back (L (1035, cy3[k], 1060, cy3[k], 1.2f)); s.lines.push_back (L (1035, cy3[k] + 77, 1060, cy3[k] + 77, 1.2f));
    }
    s.leds.push_back (D (905, 162, 60, kOr)); s.leds.push_back (D (905, 190, 61, kOr)); s.leds.push_back (D (905, 217, 62, kOr)); s.leds.push_back (D (905, 243, 63, kOr));
    s.leds.push_back (D (748, 745, 64, kOr)); s.leds.push_back (D (748, 772, 65, kOr));
    s.leds.push_back (D (345, 745, 66, kOr)); s.leds.push_back (D (345, 772, 67, kOr));
    s.leds.push_back (D (245, 398, 68, kOr)); s.leds.push_back (D (303, 190, 69, 0xfff4f4f0)); s.leds.push_back (D (1100, 105, 70, kRedL));
    // display (circle) and the seven-segment windows
    s.sections = { { "display", 288, 148, 556, 556 }, { "cvedit", 962, 188, 80, 40 }, { "subdiv", 140, 572, 40, 40 },
                   { "beats", 152, 812, 66, 40 }, { "patt", 365, 812, 80, 40 }, { "bpm", 592, 812, 105, 40 } };
    s.texts = {
        T ("MODEL RG-252 POLYPHONIC RHYTHM GENERATOR", 564, 20, 10.0f, 4),
        T ("PULSE OUTPUTS", 105, 55, 9.5f, 1), T ("sub-div", 312, 55, 8.5f), T ("cell 1", 390, 55, 8.5f),
        T ("CV OUTPUTS", 525, 55, 9.5f, 1), T ("1", 455, 105, 8.5f), T ("2", 548, 105, 8.5f), T ("3", 645, 105, 8.5f), T ("4", 738, 105, 8.5f), T ("5", 832, 105, 8.5f), T ("6", 925, 105, 8.5f),
        T ("remote", 1055, 58, 8.5f), T ("enable", 1055, 72, 8.5f),
        T ("CELL EDIT", 80, 162, 9.5f, 1), T ("red", 50, 184, 8.5f), T ("green", 120, 184, 8.5f), T ("blue", 185, 184, 8.5f),
        T ("play / stop", 268, 162, 8.5f), T ("reset", 862, 800, 8.5f), T ("DISPLAY", 762, 162, 9.5f, 1),
        T ("pulses", 862, 162, 8.5f, 0, 2), T ("cv 1-3", 862, 190, 8.5f, 0, 2), T ("cv 4-6", 862, 217, 8.5f, 0, 2), T ("subdiv", 862, 243, 8.5f, 0, 2),
        T ("CV EDIT", 1000, 160, 9.5f, 1), T ("select", 1088, 170, 8.5f),
        T ("pulse duration", 78, 262, 8.5f), T ("phase", 212, 262, 8.5f), T ("trans", 24, 352, 8.0f), T ("1 cell", 134, 352, 8.0f),
        T ("behind / ahead", 78, 400, 8.5f), T ("early", 24, 492, 8.0f), T ("late", 134, 492, 8.0f),
        T ("edit", 212, 398, 8.5f), T ("clear", 212, 480, 8.5f),
        T ("cell subdivide", 78, 542, 8.5f), T ("even", 24, 632, 8.0f), T ("late", 134, 632, 8.0f), T ("count", 212, 560, 8.5f),
        T ("copy", 52, 646, 8.5f), T ("paste", 212, 646, 8.5f), T ("copies the edited cell", 132, 704, 7.5f),
        T ("RHYTHM SELECT", 120, 760, 9.5f, 1), T ("user defined", 358, 745, 8.5f, 0, 1), T ("Euclidean", 358, 772, 8.5f, 0, 1),
        T ("cell", 72, 792, 8.5f), T ("beats / cycl", 185, 792, 8.5f), T ("pattern #", 405, 792, 8.5f), T ("MASTER BPM", 645, 792, 9.5f, 1),
        T ("on 1", 760, 745, 8.5f, 0, 1), T ("on cycl", 760, 772, 8.5f, 0, 1), T ("sync", 672, 760, 8.5f, 0, 2),
        T ("master", 952, 360, 8.5f, 0, 2), T ("CLOCKS", 1085, 348, 9.5f, 1), T ("assign / dt", 1003, 502, 8.0f),
        T ("clock 2", 952, 538, 8.5f, 0, 2), T ("clock 3", 952, 710, 8.5f, 0, 2),
        T ("CALABI YAU  -  after Buchla 252e", 564, 902, 9.0f, 1),
    };
    s.lines = { L (4, 36, 1124, 36), L (4, 140, 1124, 140), L (4, 880, 1124, 880), L (435, 36, 435, 140), L (1010, 36, 1010, 140),
                L (225, 140, 225, 250), L (225, 250, 345, 250), L (345, 140, 345, 250), L (950, 140, 950, 330), L (950, 330, 1124, 330),
                L (4, 722, 1124, 722), L (268, 722, 268, 880), L (565, 780, 565, 880), L (900, 722, 900, 880) };
    scaleSpec (s, 430.0f / 921.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// QE-284 Quad Voltage Controlled Envelope Generator - layout of the 284 front (photo, panel 1145 x 940 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make284()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_284; s.name = "QUAD VOLTAGE CONTROLLED ENVELOPE GENERATOR"; s.model = "QE-284"; s.engr = "284";
    s.width = 1145.0f; s.height = 940.0f;
    auto X = [] (float x) { return x - 35.0f; };
    auto Y = [] (float y) { return y - 20.0f; };
    for (int k = 0; k < 4; ++k)
    {
        const float dy = 200.0f * (float) k;
        s.jacks.push_back (J ("", K::Banana, false, X (82), Y (160 + dy)));
        s.jacks.push_back (J ("", K::Banana, false, X (82), Y (237 + dy), true));
    }
    for (int k = 0; k < 4; ++k)
    {
        const float dy = 200.0f * (float) k;
        s.jacks.push_back (J ("", K::Banana, true, X (1060), Y (157 + dy)));
        s.jacks.push_back (J ("", K::Banana, true, X (1133), Y (157 + dy)));
        s.jacks.push_back (J ("", K::Banana, true, X (1135), Y (240 + dy), true));
    }
    static const char* ctl[4] = { "ctl1", "ctl2", "ctl3", "ctl4" }; static const char* del[4] = { "del1", "del2", "del3", "del4" };
    static const char* att[4] = { "att1", "att2", "att3", "att4" }; static const char* dur[4] = { "dur1", "dur2", "dur3", "dur4" };
    static const char* dec[4] = { "dec1", "dec2", "dec3", "dec4" }; static const char* sus[4] = { "sus1", "sus2", "sus3", "sus4" };
    const char* ts = ".002|.025|.5|10";
    for (int k = 0; k < 4; ++k)
    {
        const float cy = 197.0f + 200.0f * (float) k;
        s.controls.push_back (C (ctl[k], "", CtrlStyle::Knob, X (207), Y (cy), kBlack));
        s.controls.push_back (C (del[k], "", CtrlStyle::Knob, X (368), Y (cy), kBlack, ts));
        s.controls.push_back (C (att[k], "", CtrlStyle::Knob, X (530), Y (cy), kBlack, ts));
        s.controls.push_back (C (dur[k], "", CtrlStyle::Knob, X (775), Y (cy), kBlack, ts));
        s.controls.push_back (C (dec[k], "", CtrlStyle::Knob, X (937), Y (cy), kBlack, ts));
        s.controls.push_back (C (sus[k], "", CtrlStyle::Toggle, X (660), Y (cy - 17), 0xffe8241c));
        s.leds.push_back (D (X (1063), Y (cy + 43), k));
        s.texts.push_back (T ("-", X (152), Y (cy + 68), 9.0f, 3)); s.texts.push_back (T ("+", X (258), Y (cy + 68), 9.0f, 3));
        s.texts.push_back (T ("sustain", X (660), Y (cy + 28), 9.5f, 1));
        // the printed loops joining each channel's input to its controls
        s.lines.push_back (L (X (82), Y (cy - 62), X (82), Y (cy - 92), 1.3f)); s.lines.push_back (L (X (82), Y (cy - 92), X (207), Y (cy - 92), 1.3f));
        s.lines.push_back (L (X (207), Y (cy - 92), X (207), Y (cy - 62), 1.3f));
        s.lines.push_back (L (X (82), Y (cy + 62), X (82), Y (cy + 88), 1.3f)); s.lines.push_back (L (X (82), Y (cy + 88), X (660), Y (cy + 88), 1.3f));
        s.lines.push_back (L (X (660), Y (cy + 88), X (660), Y (cy + 42), 1.3f));
        s.lines.push_back (L (X (660), Y (cy - 40), X (660), Y (cy - 92), 1.3f)); s.lines.push_back (L (X (660), Y (cy - 92), X (775), Y (cy - 92), 1.3f));
        s.lines.push_back (L (X (775), Y (cy - 92), X (775), Y (cy - 62), 1.3f));
        s.lines.push_back (L (X (1085), Y (cy - 40), X (1108), Y (cy - 40), 1.3f));
    }
    for (float x : { 127.0f, 288.0f, 450.0f, 612.0f, 857.0f, 1018.0f }) s.lines.push_back (L (X (x), Y (60), X (x), Y (918), 1.6f));
    s.lines.push_back (L (X (40), Y (58), X (1175), Y (58), 1.8f)); s.lines.push_back (L (X (40), Y (918), X (1175), Y (918), 1.8f));
    for (auto t : { T ("QUAD VOLTAGE CONTROLLED ENVELOPE GENERATOR   MODEL 284", 572, 22, 10.5f, 4),
                    T ("INPUT", X (88), Y (80), 11.0f, 1), T ("CONTROL", X (207), Y (80), 11.0f, 1), T ("DELAY", X (368), Y (80), 11.0f, 1),
                    T ("ATTACK", X (530), Y (80), 11.0f, 1), T ("DURATION", X (735), Y (80), 11.0f, 1), T ("DECAY", X (937), Y (80), 11.0f, 1),
                    T ("OUTPUTS", X (1100), Y (80), 11.0f, 1) })
        s.texts.push_back (t);
    s.footY = Y (940);
    scaleSpec (s, 430.0f / 940.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// RV-275 Dual Equalizer / Reverb - layout of the 275 front (photo, panel 1105 x 1800 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make275()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_275; s.name = "DUAL EQUALIZER / REVERB"; s.model = "RV-275"; s.engr = "275";
    s.width = 1105.0f; s.height = 1800.0f;
    auto X = [] (float x) { return x - 50.0f; };
    auto Y = [] (float y) { return y - 30.0f; };
    s.jacks = {
        J ("", K::Banana, false, X (155), Y (1665)), J ("", K::Mini, false, X (325), Y (1665)),
        J ("", K::Banana, false, X (690), Y (1665)), J ("", K::Mini, false, X (865), Y (1665)),
        J ("", K::Mini, true, X (180), Y (215)), J ("", K::Mini, true, X (325), Y (215)), J ("", K::Mini, true, X (870), Y (215)), J ("", K::Mini, true, X (1015), Y (215)),
        J ("", K::Mini, true, X (180), Y (380)), J ("", K::Mini, true, X (350), Y (380)), J ("", K::Mini, true, X (840), Y (380)), J ("", K::Mini, true, X (1010), Y (380)),
    };
    const char* ticks = "*|*|*|*|*|*|*|*|*|*|*";
    s.controls = {
        C ("treb1", "", CtrlStyle::Knob, X (280), Y (670), kBlack, ticks), C ("treb2", "", CtrlStyle::Knob, X (930), Y (670), kBlack, ticks),
        C ("bass1", "", CtrlStyle::Knob, X (280), Y (1010), kBlack, "-10|*|*|*|*|*|*|*|*|*|+10"), C ("bass2", "", CtrlStyle::Knob, X (930), Y (1010), kBlack, "-10|*|*|*|*|*|*|*|*|*|+10"),
        C ("mix1", "", CtrlStyle::Knob, X (280), Y (1395), kBlack, "*|*|*|*|*|*|*|*|*|*|max"), C ("mix2", "", CtrlStyle::Knob, X (810), Y (1395), kBlack, "*|*|*|*|*|*|*|*|*|*|max"),
        C ("rec1", "", CtrlStyle::Toggle, X (535), Y (1425), kBlack), C ("rec2", "", CtrlStyle::Toggle, X (1060), Y (1425), kBlack),
        C ("hot1", "", CtrlStyle::Toggle, X (490), Y (1660), 0xfff2d21c), C ("hot2", "", CtrlStyle::Toggle, X (1035), Y (1660), 0xfff2d21c),
    };
    for (float y : { 105.0f, 500.0f, 1188.0f, 1580.0f, 1790.0f }) s.lines.push_back (L (X (55), Y (y), X (1150), Y (y), 1.6f));
    for (auto l : { L (X (215), Y (215), X (290), Y (215), 1.4f), L (X (905), Y (215), X (980), Y (215), 1.4f),
                    L (X (350), Y (352), X (400), Y (293), 1.3f), L (X (400), Y (293), X (440), Y (293), 1.3f),
                    L (X (850), Y (293), X (950), Y (293), 1.3f), L (X (950), Y (293), X (1010), Y (352), 1.3f),
                    L (X (180), Y (408), X (230), Y (470), 1.3f), L (X (230), Y (470), X (280), Y (470), 1.3f),
                    L (X (610), Y (470), X (790), Y (470), 1.3f), L (X (790), Y (470), X (840), Y (408), 1.3f),
                    L (X (245), Y (1500), X (160), Y (1638), 1.3f), L (X (775), Y (1500), X (695), Y (1638), 1.3f) })
        s.lines.push_back (l);
    for (auto t : { T ("DUAL EQUALIZER / REVERB  275", X (602), Y (75), 9.5f, 4),
                    T ("voltage con-", X (600), Y (165), 9.0f), T ("trolled mix", X (600), Y (215), 9.0f),
                    T ("reverberated  only", X (645), Y (293), 9.0f), T ("OUTPUTS", X (600), Y (383), 10.0f, 1), T ("equalized  only", X (445), Y (470), 9.0f),
                    T ("TREBLE", X (600), Y (555), 10.0f, 1), T ("db at", X (600), Y (650), 9.0f), T ("10 kc", X (600), Y (705), 9.0f),
                    T ("BASS", X (600), Y (895), 10.0f, 1), T ("db at", X (600), Y (985), 9.0f), T ("50 hz", X (600), Y (1040), 9.0f),
                    T ("REVERBERATION", X (600), Y (1225), 10.0f, 1),
                    T ("recede", X (535), Y (1325), 9.0f), T ("near", X (535), Y (1525), 9.0f), T ("recede", X (1060), Y (1325), 9.0f), T ("near", X (1060), Y (1525), 9.0f),
                    T ("hot", X (490), Y (1585), 8.5f), T ("hot", X (1035), Y (1585), 8.5f), T ("INPUTS", X (600), Y (1745), 10.0f, 1) })
        s.texts.push_back (t);
    s.footY = Y (1812);
    scaleSpec (s, 430.0f / 1800.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// TM-291 Triple Morphing Filter - layout of the 291e front (photo, panel 1095 x 1790 px)
// Controls written "x_{e}_{n}" follow the stage being edited ({e}) and the selected node ({n}).
// ---------------------------------------------------------------------------------------------
static ModuleSpec make291()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_291; s.name = "TRIPLE MORPHING FILTER"; s.model = "TM-291"; s.engr = "291e";
    s.width = 1095.0f; s.height = 1790.0f;
    auto X = [] (float x) { return x - 55.0f; };
    auto Y = [] (float y) { return y - 10.0f; };
    constexpr juce::uint32 kOr = 0xffff9a2a, kBlueCap = 0xff3f78c8, kGrn = 0xff3ad05a, kWhiteL = 0xfff4f4f0;
    s.jacks = {
        J ("", K::Mini, false, X (160), Y (1425)), J ("", K::Mini, false, X (325), Y (1425)), J ("", K::Mini, false, X (490), Y (1425)), J ("", K::Mini, false, X (675), Y (1425)),
        J ("", K::Mini, false, X (160), Y (1630)), J ("", K::Mini, false, X (325), Y (1630)), J ("", K::Mini, false, X (490), Y (1630)), J ("", K::Mini, false, X (675), Y (1630)),
        J ("", K::Mini, false, X (245), Y (165)),
        J ("", K::Banana, false, X (160), Y (750)), J ("", K::Banana, false, X (325), Y (750)), J ("", K::Banana, false, X (490), Y (750)),
        J ("", K::Banana, false, X (1035), Y (570)),
        J ("", K::Banana, false, X (665), Y (750)), J ("", K::Banana, false, X (830), Y (750)), J ("", K::Banana, false, X (1000), Y (750)),
        J ("", K::Banana, false, X (160), Y (1210)), J ("", K::Banana, false, X (325), Y (1210)), J ("", K::Banana, false, X (490), Y (1210)),
        J ("", K::Banana, false, X (1035), Y (1035)),
        JT ("", K::Banana, false, X (1035), Y (1200), 0xffff6a2a, true),
        J ("", K::Mini, true, X (430), Y (165)), J ("", K::Mini, true, X (555), Y (165)),
        J ("", K::Mini, true, X (720), Y (165)), J ("", K::Mini, true, X (870), Y (165)), J ("", K::Mini, true, X (1035), Y (165)),
    };
    s.controls = {
        C ("amp_{e}_{n}", "", CtrlStyle::SmallKnob, X (418), Y (570), kBlueCap),
        C ("bw_{e}_{n}", "", CtrlStyle::SmallKnob, X (768), Y (570), kBlueCap),
        C ("f_{e}_{n}", "", CtrlStyle::Knob, X (285), Y (1000), kBlueCap),
        C ("time_{e}", "", CtrlStyle::SmallKnob, X (1035), Y (320), kBlueCap),
        C ("index", "", CtrlStyle::SmallKnob, X (935), Y (1425), kBlueCap),
        C ("remote", "", CtrlStyle::Push, X (240), Y (320), 0xff8a8c90),
        C ("solo", "", CtrlStyle::Push, X (180), Y (575), 0xff4aa0e8),
        C ("node", "", CtrlStyle::Push, X (680), Y (1240), 0xff3a78d8),
        C ("play", "", CtrlStyle::Push, X (870), Y (1035), 0xff3ab878),
        C ("editsel", "", CtrlStyle::Push, X (870), Y (1200), 0xffe8483a),
        C ("modtype", "", CtrlStyle::Push, X (1045), Y (1620), 0xff3ab878),
    };
    s.sections = { { "lcd", X (400), Y (280), 360, 105 } };
    s.leds = { D (X (130), Y (295), 24), D (X (120), Y (540), 23, kWhiteL),
               D (X (360), Y (655), 30, kOr), D (X (318), Y (565), 31, kOr), D (X (360), Y (475), 32, kOr), D (X (478), Y (475), 33, kOr), D (X (520), Y (565), 34, kOr), D (X (478), Y (655), 35, kOr),
               D (X (710), Y (655), 40, kOr), D (X (668), Y (565), 41, kOr), D (X (710), Y (475), 42, kOr), D (X (828), Y (475), 43, kOr), D (X (870), Y (565), 44, kOr), D (X (828), Y (655), 45, kOr),
               D (X (210), Y (1120), 50, kOr), D (X (155), Y (1045), 51, kOr), D (X (170), Y (955), 52, kOr), D (X (235), Y (890), 53, kOr),
               D (X (335), Y (890), 54, kOr), D (X (400), Y (955), 55, kOr), D (X (415), Y (1045), 56, kOr), D (X (365), Y (1120), 57, kOr),
               D (X (1065), Y (895), 8, kGrn),
               D (X (650), Y (1045), 10, kWhiteL), D (X (675), Y (1090), 11, kWhiteL), D (X (700), Y (1140), 12, kWhiteL),
               D (X (890), Y (1515), 60, kOr), D (X (830), Y (1425), 61, kOr), D (X (890), Y (1340), 62, kOr), D (X (990), Y (1340), 63, kOr), D (X (1045), Y (1425), 64, kOr), D (X (990), Y (1515), 65, kOr),
               D (X (950), Y (1575), 20, kGrn), D (X (940), Y (1620), 21, kGrn), D (X (950), Y (1665), 22, kGrn) };
    for (int i = 0; i < 8; ++i) s.leds.push_back (D (X (645.0f + 50.0f * (float) i), Y (895), i, kWhiteL));
    static const char* num[8] = { "1", "2", "3", "4", "5", "6", "7", "8" };
    for (int i = 0; i < 8; ++i) s.texts.push_back (T (num[i], X (645.0f + 50.0f * (float) i), Y (935), 8.5f));
    for (float y : { 60.0f, 225.0f, 415.0f, 825.0f, 1310.0f, 1525.0f, 1740.0f }) s.lines.push_back (L (X (58), Y (y), X (1148), Y (y), 1.4f));
    for (auto l : { L (X (330), Y (225), X (330), Y (415), 1.4f), L (X (600), Y (415), X (600), Y (1310), 1.4f),
                    L (X (600), Y (975), X (770), Y (975), 1.2f), L (X (770), Y (975), X (770), Y (1310), 1.2f),
                    L (X (770), Y (1525), X (770), Y (1740), 1.2f), L (X (290), Y (165), X (385), Y (165), 1.2f, false, true) })
        s.lines.push_back (l);
    for (auto t : { T ("TRIPLE MORPHING FILTER  291e", X (600), Y (35), 9.0f, 4),
                    T ("ex-", X (95), Y (125), 8.5f), T ("pand", X (95), Y (150), 8.5f), T ("input", X (95), Y (175), 8.5f),
                    T ("all", X (490), Y (212), 8.5f), T ("A", X (720), Y (212), 8.5f, 1), T ("B", X (870), Y (212), 8.5f, 1), T ("C", X (1035), Y (212), 8.5f, 1),
                    T ("remote", X (125), Y (350), 8.0f), T ("enable", X (150), Y (380), 8.0f),
                    T ("TIME", X (925), Y (335), 8.5f, 1), T ("EDIT", X (940), Y (372), 8.5f, 1),
                    T ("AMPLITUDE", X (185), Y (450), 8.5f, 1), T ("(affects \"all\")", X (185), Y (482), 7.5f),
                    T ("0", X (325), Y (655), 7.5f), T ("20", X (275), Y (565), 7.5f), T ("40", X (325), Y (470), 7.5f), T ("60", X (515), Y (470), 7.5f), T ("80", X (560), Y (565), 7.5f), T ("100", X (520), Y (655), 7.5f),
                    T ("solo", X (180), Y (640), 8.0f),
                    T ("A", X (110), Y (795), 8.5f, 1), T ("B", X (275), Y (795), 8.5f, 1), T ("C", X (440), Y (795), 8.5f, 1),
                    T ("BAND-", X (1010), Y (450), 8.5f, 1), T ("WIDTH", X (1020), Y (485), 8.5f, 1), T ("all", X (985), Y (640), 8.0f),
                    T ("0", X (675), Y (655), 7.5f), T ("20", X (625), Y (565), 7.5f), T ("40", X (675), Y (470), 7.5f), T ("60", X (865), Y (470), 7.5f), T ("80", X (908), Y (565), 7.5f), T ("100", X (870), Y (655), 7.5f),
                    T ("A", X (615), Y (795), 8.5f, 1), T ("B", X (785), Y (795), 8.5f, 1), T ("C", X (950), Y (795), 8.5f, 1),
                    T ("FREQ", X (520), Y (850), 8.5f, 1),
                    T ("31", X (175), Y (1135), 7.5f), T ("62", X (115), Y (1045), 7.5f), T ("125", X (125), Y (950), 7.5f), T ("250", X (205), Y (865), 7.5f),
                    T ("500", X (370), Y (865), 7.5f), T ("1 kHz", X (455), Y (950), 7.5f), T ("2 kHz", X (470), Y (1045), 7.5f), T ("4 kHz", X (420), Y (1135), 7.5f),
                    T ("A", X (110), Y (1255), 8.5f, 1), T ("B", X (275), Y (1255), 8.5f, 1), T ("C", X (440), Y (1255), 8.5f, 1),
                    T ("STAGE NUMBER", X (790), Y (850), 8.5f, 1), T ("play", X (1065), Y (935), 8.0f),
                    T ("NODE", X (685), Y (998), 8.5f, 1), T ("A", X (625), Y (1045), 8.0f, 1), T ("B", X (650), Y (1090), 8.0f, 1), T ("C", X (675), Y (1140), 8.0f, 1),
                    T ("stop / play", X (870), Y (1092), 8.0f), T ("c.v. in", X (1035), Y (1092), 8.0f), T ("edit select", X (870), Y (1262), 8.0f), T ("adv.", X (1035), Y (1262), 8.0f),
                    T ("MODULATION INPUTS & INDEX", X (400), Y (1345), 8.5f, 1),
                    T ("0", X (855), Y (1515), 7.5f), T ("20", X (795), Y (1425), 7.5f), T ("40", X (855), Y (1340), 7.5f), T ("60", X (1025), Y (1340), 7.5f), T ("80", X (1083), Y (1425), 7.5f), T ("100", X (1035), Y (1515), 7.5f),
                    T ("A", X (110), Y (1470), 8.5f, 1), T ("B", X (275), Y (1470), 8.5f, 1), T ("C", X (440), Y (1470), 8.5f, 1), T ("all", X (622), Y (1470), 8.5f),
                    T ("SIGNAL INPUTS", X (255), Y (1557), 8.5f, 1),
                    T ("A", X (110), Y (1675), 8.5f, 1), T ("B", X (275), Y (1675), 8.5f, 1), T ("C", X (440), Y (1675), 8.5f, 1), T ("all", X (622), Y (1675), 8.5f),
                    T ("a.m.", X (900), Y (1575), 7.5f), T ("b.w.", X (890), Y (1620), 7.5f), T ("f.m.", X (900), Y (1665), 7.5f) })
        s.texts.push_back (t);
    s.footY = Y (1768);
    scaleSpec (s, 430.0f / 1790.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// US-267 Uncertainty Source / Dual Bandpass Filter - layout of the 267e front (photo, panel 375 x 597 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make267()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_267; s.name = "UNCERTAINTY SOURCE"; s.model = "US-267"; s.engr = "267e";
    s.width = 375.0f; s.height = 597.0f;
    auto X = [] (float x) { return x - 3.0f; };
    auto Y = [] (float y) { return y - 3.0f; };
    constexpr juce::uint32 kBlueCap = 0xff3f78c8, kOrJ = 0xffe8892b, kBlueJ = 0xff2a6ad8, kWhiteL = 0xfff4f4f0;
    s.jacks = {
        J ("", K::Banana, false, X (28), Y (110)), JT ("", K::Banana, false, X (85), Y (140), kOrJ, true), J ("", K::Banana, false, X (28), Y (160)),
        J ("", K::Banana, false, X (28), Y (212)), JT ("", K::Banana, false, X (85), Y (240), kOrJ, true), J ("", K::Banana, false, X (28), Y (262)),
        J ("", K::Mini, false, X (28), Y (345)), J ("", K::Banana, false, X (95), Y (348)), J ("", K::Banana, false, X (60), Y (390)), J ("", K::Banana, false, X (28), Y (428)), J ("", K::Mini, false, X (290), Y (395)),
        J ("", K::Mini, false, X (28), Y (475)), J ("", K::Banana, false, X (95), Y (475)), J ("", K::Banana, false, X (60), Y (512)), J ("", K::Banana, false, X (28), Y (550)), J ("", K::Mini, false, X (290), Y (522)),
        J ("", K::Mini, true, X (55), Y (62)), J ("", K::Mini, true, X (130), Y (62)), J ("", K::Mini, true, X (178), Y (62)), J ("", K::Mini, true, X (250), Y (62)),
        JT ("", K::Banana, true, X (340), Y (110), kBlueJ), JT ("", K::Banana, true, X (340), Y (158), kBlueJ),
        JT ("", K::Banana, true, X (340), Y (212), kBlueJ), JT ("", K::Banana, true, X (340), Y (260), kBlueJ),
        J ("", K::Mini, true, X (340), Y (345)), J ("", K::Mini, true, X (340), Y (395)), J ("", K::Mini, true, X (340), Y (475)), J ("", K::Mini, true, X (340), Y (522)),
    };
    const char* rs = ".05|.2|1|5|20|50";
    const char* fsc = "31|62|125|250|500|1k|2k|4k";
    s.controls = {
        C ("ratea", "", CtrlStyle::Knob, X (148), Y (135), kBlueCap, rs), C ("randa", "", CtrlStyle::Knob, X (238), Y (135), kBlueCap),
        C ("rateb", "", CtrlStyle::Knob, X (148), Y (237), kBlueCap, rs), C ("randb", "", CtrlStyle::Knob, X (238), Y (237), kBlueCap),
        C ("freqa", "", CtrlStyle::Knob, X (178), Y (400), kBlueCap, fsc), C ("bwa", "", CtrlStyle::SmallKnob, X (112), Y (428), kBlueCap, "0|1"),
        C ("idxa", "", CtrlStyle::SmallKnob, X (262), Y (350), kBlueCap, "0|1"), C ("moda", "", CtrlStyle::Push, X (328), Y (436), 0xff52c69a),
        C ("freqb", "", CtrlStyle::Knob, X (178), Y (522), kBlueCap, fsc), C ("bwb", "", CtrlStyle::SmallKnob, X (112), Y (550), kBlueCap, "0|1"),
        C ("idxb", "", CtrlStyle::SmallKnob, X (262), Y (477), kBlueCap, "0|1"), C ("modb", "", CtrlStyle::Push, X (328), Y (557), 0xff52c69a),
        C ("remote", "", CtrlStyle::Push, X (345), Y (60), 0xff2a2b30),
    };
    s.leds = { D (X (310), Y (102), 0, kWhiteL), D (X (310), Y (150), 1, kWhiteL), D (X (310), Y (204), 2, kWhiteL), D (X (310), Y (252), 3, kWhiteL),
               D (X (282), Y (412), 10, 0xffd8c060), D (X (282), Y (427), 11, 0xffd8c060), D (X (282), Y (442), 12, 0xffd8c060),
               D (X (282), Y (535), 13, 0xffd8c060), D (X (282), Y (550), 14, 0xffd8c060), D (X (282), Y (565), 15, 0xffd8c060),
               D (X (318), Y (35), 20, 0xffd8c060) };
    for (float y : { 22.0f, 88.0f, 190.0f, 293.0f, 318.0f, 455.0f, 580.0f }) s.lines.push_back (L (X (6), Y (y), X (376), Y (y), 1.3f));
    for (auto l : { L (X (290), Y (22), X (290), Y (88), 1.2f), L (X (155), Y (62), X (178), Y (62), 1.2f),
                    L (X (40), Y (102), X (110), Y (102), 1.0f, false, true), L (X (45), Y (165), X (190), Y (165), 1.0f, false, true),
                    L (X (40), Y (204), X (110), Y (204), 1.0f, false, true), L (X (45), Y (267), X (190), Y (267), 1.0f, false, true),
                    L (X (110), Y (338), X (235), Y (338), 1.0f, false, true), L (X (80), Y (390), X (145), Y (390), 1.0f, false, true),
                    L (X (110), Y (468), X (235), Y (468), 1.0f, false, true), L (X (80), Y (512), X (145), Y (512), 1.0f, false, true),
                    L (X (40), Y (360), X (40), Y (458), 0.9f, true) })
        s.lines.push_back (l);
    for (auto t : { T ("UNCERTAINTY SOURCE  267e", X (165), Y (12), 8.5f, 4),
                    T ("NOISE SOURCE OUTPUTS", X (165), Y (32), 8.0f, 1),
                    T ("low", X (20), Y (48), 7.5f, 1), T ("-3db/oct", X (55), Y (82), 7.0f), T ("flat", X (155), Y (40), 7.5f, 1), T ("0db/oct", X (155), Y (82), 7.0f),
                    T ("high", X (250), Y (40), 7.5f, 1), T ("+3db/oct", X (250), Y (82), 7.0f),
                    T ("rem", X (318), Y (22), 7.0f), T ("enable", X (310), Y (82), 7.0f),
                    T ("RANDOM A", X (238), Y (98), 8.0f, 1), T ("prob chnge rate", X (95), Y (95), 7.0f), T ("smpl", X (85), Y (118), 7.0f),
                    T ("external c.v.", X (95), Y (180), 7.0f), T ("random", X (238), Y (176), 7.0f), T ("mix out", X (325), Y (128), 6.5f), T ("s&h out", X (325), Y (178), 6.5f),
                    T ("RANDOM B", X (238), Y (200), 8.0f, 1), T ("prob chnge rate", X (95), Y (197), 7.0f), T ("sample", X (85), Y (220), 7.0f),
                    T ("external c.v.", X (95), Y (282), 7.0f), T ("random", X (238), Y (278), 7.0f), T ("mix out", X (325), Y (230), 6.5f), T ("s&h out", X (325), Y (280), 6.5f),
                    T ("DUAL BANDPASS FILTER", X (190), Y (306), 8.0f, 1),
                    T ("modulation index", X (175), Y (330), 7.0f), T ("freq", X (112), Y (380), 7.0f), T ("b.w.", X (60), Y (440), 7.0f), T ("sig in", X (28), Y (368), 7.0f),
                    T ("mod in", X (290), Y (372), 7.0f), T ("sig out", X (310), Y (330), 7.0f),
                    T ("amp", X (268), Y (412), 6.5f, 0, 2), T ("freqncy", X (268), Y (427), 6.5f, 0, 2), T ("bandwidth", X (268), Y (442), 6.5f, 0, 2), T ("mod type", X (330), Y (455), 6.5f),
                    T ("FILTER A", X (200), Y (446), 7.5f, 1),
                    T ("modulation index", X (175), Y (462), 7.0f), T ("freq", X (112), Y (502), 7.0f), T ("b.w.", X (60), Y (565), 7.0f), T ("sig in", X (28), Y (498), 7.0f),
                    T ("mod in", X (290), Y (500), 7.0f),
                    T ("amp", X (268), Y (535), 6.5f, 0, 2), T ("freqncy", X (268), Y (550), 6.5f, 0, 2), T ("bandwidth", X (268), Y (565), 6.5f, 0, 2), T ("mod type", X (330), Y (576), 6.5f),
                    T ("FILTER B", X (200), Y (572), 7.5f, 1), T ("N", X (40), Y (318), 6.5f, 3) })
        s.texts.push_back (t);
    s.footY = Y (590);
    scaleSpec (s, 430.0f / 597.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// SC-666 Oscilloscope - a Calabi Yau original (prototype render, 2048 x 1118 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make666()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_666; s.name = "OSCILLOSCOPE"; s.model = "SC-666"; s.engr = "SC-666";
    s.width = 2048.0f; s.height = 1118.0f;
    s.bg = 0xffe9e6dd; s.ink = 0xff26272b;
    s.jacks = {
        J ("", K::Mini, false, 1915, 180), J ("", K::Mini, false, 1915, 360), J ("", K::Banana, false, 1300, 575, true),
        J ("", K::Banana, false, 1265, 810), J ("", K::Banana, false, 1435, 810), J ("", K::Banana, false, 1600, 810), J ("", K::Banana, false, 1770, 810), J ("", K::Banana, false, 1940, 810),
        J ("", K::Banana, true, 1265, 975), J ("", K::Banana, true, 1435, 975, true), J ("", K::Banana, true, 1600, 975, true),
        J ("", K::Mini, true, 1770, 975), J ("", K::Mini, true, 1940, 975),
    };
    s.controls = {
        C ("va", "", CtrlStyle::Knob, 1020, 195, 0xffd8352b), C ("vb", "", CtrlStyle::Knob, 1020, 440, 0xfff0c020),
        C ("hpos", "", CtrlStyle::Knob, 1020, 690, 0xff3f78c8), C ("xgain", "", CtrlStyle::Knob, 1020, 930, 0xff3f78c8),
        C ("src", "", CtrlStyle::Knob, 1300, 205, kBlack, "INT|EXT|FREE"), C ("level", "", CtrlStyle::Knob, 1300, 415, kBlack, "-|*|*|0|*|*|+"),
        C ("time", "", CtrlStyle::BigKnob, 1575, 255, kBlack, ".1ms|1ms|10ms|.1s|1s"), C ("mode", "", CtrlStyle::Knob, 1575, 515, kBlack, "AUTO|NORM|SNGL|X-Y"),
        C ("coupling", "", CtrlStyle::Switch3, 1885, 580, 0xff2a2b30), C ("power", "", CtrlStyle::Toggle, 165, 960, 0xff2a2b30),
        C ("intensity", "", CtrlStyle::Knob, 365, 960, kBlack), C ("focus", "", CtrlStyle::Knob, 670, 960, kBlack),
    };
    s.sections = { { "crt", 510 - 255, 540 - 255, 510, 510 } };
    s.leds = { D (1385, 125, 0, 0xff3ad05a), D (85, 960, 1, 0xffe8241c) };
    auto box = [&] (float x0, float y0, float x1, float y1)
    {
        s.lines.push_back (L (x0, y0, x1, y0, 2.4f)); s.lines.push_back (L (x1, y0, x1, y1, 2.4f));
        s.lines.push_back (L (x1, y1, x0, y1, 2.4f)); s.lines.push_back (L (x0, y1, x0, y0, 2.4f));
    };
    box (885, 22, 1150, 1045); box (1175, 22, 1425, 680); box (1450, 22, 1700, 680); box (1755, 22, 2010, 680); box (1175, 700, 2010, 1045);
    s.lines.push_back (L (80, 75, 870, 75, 2.0f)); s.lines.push_back (L (1175, 865, 2010, 865, 1.2f));
    for (auto t : { T ("CALABI YAU  -  WEST COAST MODULAR", 470, 45, 13.0f, 1), T ("SC-666", 500, 112, 14.0f, 1),
                    T ("VERTICAL A", 1020, 85, 10.5f, 1), T ("VERTICAL B", 1020, 330, 10.5f, 1), T ("HORIZONTAL", 1020, 585, 10.5f, 1), T ("X-Y GAIN", 1020, 830, 10.5f, 1),
                    T ("TRIGGER", 1300, 50, 11.0f, 1), T ("SOURCE", 1300, 105, 9.5f), T ("LEVEL", 1300, 330, 9.5f), T ("EXT TRIG", 1300, 650, 9.5f),
                    T ("SWEEP", 1575, 50, 11.0f, 1), T ("TIME / DIV", 1575, 105, 9.5f), T ("MODE", 1575, 415, 9.5f),
                    T ("INPUT", 1880, 50, 11.0f, 1), T ("A", 1820, 185, 12.0f, 1), T ("B", 1820, 365, 12.0f, 1), T ("COUPLING", 1880, 470, 9.5f),
                    T ("AC", 1930, 548, 8.5f, 0, 1), T ("DC", 1930, 580, 8.5f, 0, 1), T ("GND", 1930, 612, 8.5f, 0, 1),
                    T ("VERT POS", 1265, 735, 8.5f, 1), T ("HORIZ POS", 1435, 735, 8.5f, 1), T ("SWEEP IN", 1600, 735, 8.5f, 1), T ("Z-AXIS IN", 1770, 735, 8.5f, 1), T ("TIME CV", 1940, 735, 8.5f, 1),
                    T ("SWEEP OUT", 1265, 900, 8.5f, 1), T ("GATE OUT", 1435, 900, 8.5f, 1), T ("TRIG OUT", 1600, 900, 8.5f, 1), T ("A THRU", 1770, 900, 8.5f, 1), T ("B THRU", 1940, 900, 8.5f, 1),
                    T ("POWER", 165, 860, 10.0f, 1), T ("ON", 215, 915, 8.5f, 0, 1), T ("OFF", 215, 1008, 8.5f, 0, 1), T ("INTENSITY", 365, 860, 10.0f, 1), T ("FOCUS", 670, 860, 10.0f, 1) })
        s.texts.push_back (t);
    s.footY = 1088.0f;
    scaleSpec (s, 430.0f / 1118.0f);
    return s;
}

// ---------------------------------------------------------------------------------------------
// KB-225 MIDI Interface and OUT-2 Output (in the same visual language)
// ---------------------------------------------------------------------------------------------
static ModuleSpec makeMidi()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_Midi;
    s.name = "MIDI INTERFACE";
    s.model = "KB-225";
    s.engr = "";
    s.width = 176.0f;
    s.height = 430.0f;
    s.sections = { { "keyboard", 8, 36, 160, 190 }, { "outputs", 8, 232, 160, 176 } };
    s.jacks = {
        J ("pitch", K::Banana, true, 46, 282),
        J ("gate", K::Banana, true, 88, 282, true),
        J ("pulse", K::Banana, true, 130, 282, true),
        J ("velocity", K::Banana, true, 46, 334),
        J ("pressure", K::Banana, true, 88, 334),
        J ("mod", K::Banana, true, 130, 334),
        J ("bend", K::Banana, true, 46, 386),
        J ("pitch", K::Banana, true, 88, 386),
    };
    s.controls = {
        C ("midi_glide", "glide", CtrlStyle::Knob, 88, 100, kGreyKnob, "0|1|2"),
        C ("midi_bend", "bend", CtrlStyle::SmallKnob, 50, 182, kBlack),
        C ("midi_trans", "octave", CtrlStyle::SmallKnob, 126, 182, kBlack, "-3|0|+3"),
    };
    return s;
}

static ModuleSpec makeOutput()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_Output;
    s.name = "OUTPUT";
    s.model = "OUT-2";
    s.engr = "";
    s.width = 156.0f;
    s.height = 430.0f;
    s.sections = { { "level", 8, 36, 140, 246 }, { "inputs", 8, 288, 140, 120 } };
    s.jacks = {
        J ("left", K::Mini, false, 46, 336),
        J ("right", K::Mini, false, 110, 336),
        J ("c.v. in", K::Banana, false, 78, 386),
    };
    s.controls = {
        C ("out_level", "level", CtrlStyle::Knob, 74, 92, kGreyKnob, "0|5|10"),
        C ("out_cv", "c.v.", CtrlStyle::SmallKnob, 42, 176, kBlack),
        C ("out_lag", "lag", CtrlStyle::SmallKnob, 106, 176, kBlack),
        C ("out_master", "master", CtrlStyle::Knob, 74, 240, kGreyKnob),
    };
    return s;
}

// the engraved footer replaces the old "CALABI YAU - after ..." line of each layout: keep its height, drop the text
static ModuleSpec finish (ModuleSpec s)
{
    for (auto it = s.texts.begin(); it != s.texts.end();)
    {
        const juce::String t (it->text);
        if (t.contains ("after Buchla") || t.startsWith ("CALABI YAU  -  after"))
        {
            if (s.footY <= 0.0f) s.footY = it->y;
            it = s.texts.erase (it);
        }
        else ++it;
    }
    if (s.footY <= 0.0f) s.footY = s.height - 9.0f;
    if (juce::String (s.engr).isEmpty()) s.engr = s.model;
    return s;
}

const ModuleSpec& specFor (ModuleType t)
{
    static const ModuleSpec specs[MT_Count] = { finish (makeMidi()), finish (make259()), finish (makeOutput()), finish (make281()), finish (make292()), finish (make266()), finish (make245()),
                                                finish (make242()), finish (make257()), finish (make258()), finish (make264()), finish (make230()), finish (make285()), finish (make207()),
                                                finish (make277()), finish (make296()), finish (make248()), finish (make218()), finish (make250()), finish (make252()),
                                                finish (make267()), finish (make275()), finish (make284()), finish (make291()), finish (make666()) };
    return specs[t];
}
} // namespace wc
