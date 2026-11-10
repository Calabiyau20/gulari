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
    s.after = "after Buchla 259t";
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
    s.after = "after Buchla 281t";
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
    s.after = "after Buchla 292t";
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
    s.after = "after Buchla 266t";
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
    s.after = "after Buchla 245t";
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
    s.after = "after Buchla 242t";
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
    s.type = MT_257; s.name = "DUAL VOLTAGE PROCESSOR"; s.model = "VP-257"; s.after = "after Buchla 257t";
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
    s.type = MT_258; s.name = "DUAL OSCILLATOR"; s.model = "DO-258"; s.after = "after Buchla 258t";
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
    s.type = MT_264; s.name = "QUAD SAMPLE AND HOLD"; s.model = "QS-264"; s.after = "after Buchla 264t";
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
        T ("CALABI YAU  -  after Buchla 264t   POLYPHONIC ADAPTOR", 417, 1142, 9.0f, 1),
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
    s.type = MT_230; s.name = "TRIPLE ENVELOPE FOLLOWER"; s.model = "EF-230"; s.after = "after Buchla 230t";
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
    s.type = MT_285; s.name = "FREQUENCY SHIFTER"; s.model = "FS-285"; s.after = "after Buchla 285t";
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
    s.type = MT_277; s.name = "SIGNAL DELAY UNIT"; s.model = "SD-277"; s.after = "after Buchla 277t";
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
// MX-207 Mixer / Preamplifier - layout of the 207t front (photo, flattened to 825 x 905 px)
// ---------------------------------------------------------------------------------------------
static ModuleSpec make207()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_207; s.name = "MIXER / PREAMPLIFIER"; s.model = "MX-207"; s.after = "after Buchla 207t";
    s.width = 825.0f; s.height = 905.0f;
    const float cx[6] = { 40, 135, 230, 350, 455, 560 };
    s.jacks.push_back (J ("", K::Banana, false, 34, 165));      // program c.v.
    s.jacks.push_back (J ("", K::Banana, false, 552, 140));     // monitor c.v.
    for (int c = 0; c < 6; ++c) s.jacks.push_back (J ("", K::Mini, false, cx[c], 768));
    s.jacks.push_back (J ("", K::Mini, false, 655, 428)); s.jacks.push_back (J ("", K::Mini, false, 775, 428));   // expansion in
    s.jacks.push_back (J ("", K::Mini, false, 778, 765));                                                          // preamp input
    s.jacks.push_back (J ("", K::Mini, true, 655, 125)); s.jacks.push_back (J ("", K::Mini, true, 775, 125));     // L - R
    s.jacks.push_back (J ("", K::Mini, true, 655, 300)); s.jacks.push_back (J ("", K::Mini, true, 775, 300));     // monitor
    s.jacks.push_back (J ("", K::Mini, true, 655, 775));                                                           // preamp output
    static const char* lv[6] = { "lvl_a", "lvl_b", "lvl_c", "lvl_d", "lvl_e", "lvl_f" };
    static const char* pg[6] = { "prog_a", "prog_b", "prog_c", "prog_d", "prog_e", "prog_f" };
    static const char* mn[6] = { "mon_a", "mon_b", "mon_c", "mon_d", "mon_e", "mon_f" };
    static const char* pn[4] = { "pan1", "pan2", "pan3", "pan4" };
    const float px[4] = { 112, 222, 332, 442 };
    for (int k = 0; k < 4; ++k) s.controls.push_back (C (pn[k], "", CtrlStyle::SmallKnob, px[k], 160, kSkirt));
    const char* abc[6] = { "A", "B", "C", "D", "E", "F" };
    for (int c = 0; c < 6; ++c)
    {
        s.controls.push_back (C (lv[c], "", CtrlStyle::Fader, cx[c], 382));
        s.controls.push_back (C (pg[c], "", CtrlStyle::Toggle, cx[c], 568, kSkirt));
        s.controls.push_back (C (mn[c], "", CtrlStyle::Toggle, cx[c], 672, 0xffe8c22a));
        s.texts.push_back (T (abc[c], cx[c] - 30, 742, 9.5f, 1));
    }
    s.controls.push_back (C ("gain", "", CtrlStyle::Knob, 752, 588, kSkirt, "*|*|*|*|*|*"));
    s.controls.push_back (C ("range", "", CtrlStyle::Toggle, 645, 640, kSkirt));
    s.leds = { D (20, 222, M207::Led_ProgA), D (62, 222, M207::Led_ProgB), D (540, 190, M207::Led_MonA), D (595, 190, M207::Led_MonB) };
    const char* marks[6] = { "10", "8", "6", "4", "2", "0" };
    for (int i = 0; i < 6; ++i)
    {
        const float y = 270.0f + (float) i * 46.0f;
        s.texts.push_back (T (marks[i], 290, y + 4, 9.0f, 1));
        s.lines.push_back (L (52, y, 272, y, 0.8f)); s.lines.push_back (L (308, y, 548, y, 0.8f));
    }
    s.lines.push_back (L (10, 245, 50, 245, 1.2f, false, true)); s.lines.push_back (L (600, 212, 560, 212, 1.2f, false, true));
    for (float y : { 40.0f, 240.0f, 530.0f, 625.0f, 728.0f, 830.0f }) s.lines.push_back (L (8, y, 615, y));
    s.lines.push_back (L (615, 20, 615, 885)); s.lines.push_back (L (615, 40, 817, 40));
    for (float y : { 225.0f, 355.0f, 485.0f, 830.0f }) s.lines.push_back (L (615, y, 817, y));
    s.lines.push_back (L (680, 125, 750, 125, 1.4f)); s.lines.push_back (L (680, 300, 750, 300, 1.4f));
    s.texts.push_back (T ("MIXER / PREAMPLIFIER  MX-207", 412, 20, 10.5f, 4));
    s.texts.push_back (T ("CHANNEL  ASSIGNMENT", 290, 225, 9.5f, 1));
    s.texts.push_back (T ("LEVEL  ADJUSTMENTS", 290, 518, 9.5f, 1));
    s.texts.push_back (T ("PROG", 292, 550, 9.0f, 1)); s.texts.push_back (T ("MON", 292, 655, 9.0f, 1));
    s.texts.push_back (T ("INPUTS", 292, 815, 9.5f, 1));
    s.texts.push_back (T ("L - R", 715, 128, 10.0f, 1)); s.texts.push_back (T ("SIGNAL OUTPUTS", 715, 205, 9.5f, 1));
    s.texts.push_back (T ("monitor", 715, 255, 9.5f)); s.texts.push_back (T ("EXPANSION IN", 715, 378, 9.5f, 1));
    s.texts.push_back (T ("PREAMP", 715, 508, 10.0f, 1)); s.texts.push_back (T ("GAIN", 655, 575, 9.0f, 1));
    s.texts.push_back (T ("75", 690, 628, 8.5f, 1, 1)); s.texts.push_back (T ("55", 690, 655, 8.5f, 1, 1));
    s.texts.push_back (T ("INPUT", 760, 700, 9.5f, 1)); s.texts.push_back (T ("OUTPUT", 657, 728, 9.5f, 1));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 207t", 412, 862, 9.0f, 1));
    scaleSpec (s, 430.0f / 905.0f);
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
    s.after = "";
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
    s.after = "";
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

const ModuleSpec& specFor (ModuleType t)
{
    static const ModuleSpec specs[MT_Count] = { makeMidi(), make259(), makeOutput(), make281(), make292(), make266(), make245(), make242(), make257(), make258(), make264(), make230(), make285(), make207(), make277() };
    return specs[t];
}
} // namespace wc
