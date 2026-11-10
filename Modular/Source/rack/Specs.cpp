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
// VP-257 Dual Voltage Processor (after the 257t): two sections, attenuverter K on Va, crossfader M between Vb and Vc
// (+.5 adds 5 V to Vb), offset, Vout. Layout in panel units (height 430).
// ---------------------------------------------------------------------------------------------
static ModuleSpec make257()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_257; s.name = "DUAL VOLTAGE PROCESSOR"; s.model = "VP-257"; s.after = "after Buchla 257t";
    s.width = 300.0f; s.height = 430.0f;
    const float y0[2] = { 28, 220 };
    static const char* kk[2] = { "k1", "k2" }; static const char* mm[2] = { "m1", "m2" }; static const char* pp[2] = { "p51", "p52" }; static const char* oo[2] = { "off1", "off2" };
    for (int n = 0; n < 2; ++n)
    {
        const float y = y0[n];
        s.jacks.push_back (J ("Va", K::Banana, false, 32, y + 158));
        s.jacks.push_back (J ("Vb", K::Banana, false, 88, y + 158));
        s.jacks.push_back (J ("c.v.", K::Banana, false, 150, y + 158));
        s.jacks.push_back (J ("Vc", K::Banana, false, 210, y + 158));
    }
    s.jacks.push_back (J ("Vout", K::Banana, true, 268, y0[0] + 158));
    s.jacks.push_back (J ("Vout", K::Banana, true, 268, y0[1] + 158));
    for (int n = 0; n < 2; ++n)
    {
        const float y = y0[n];
        s.controls.push_back (C (kk[n], "", CtrlStyle::Knob, 52, y + 62, kSkirt, "-|*|*|*|0|*|*|*|+"));
        s.controls.push_back (C (mm[n], "", CtrlStyle::Knob, 150, y + 62, kSkirt, "b|*|*|*|*|*|*|*|c"));
        s.controls.push_back (C (oo[n], "", CtrlStyle::Knob, 248, y + 62, kSkirt, "0|*|*|*|5|*|*|*|10"));
        s.controls.push_back (C (pp[n], "", CtrlStyle::Toggle, 88, y + 116, kSkirt));
        s.texts.push_back (T ("K", 52, y + 108, 11.0f, 3));
        s.texts.push_back (T ("M", 150, y + 108, 11.0f, 3));
        s.texts.push_back (T ("offset", 248, y + 104, 9.5f));
        s.texts.push_back (T ("+.5", 72, y + 120, 9.5f, 1, 2));
        s.lines.push_back (L (32, y + 140, 52, y + 92, 1.2f));                    // Va -> K
        s.lines.push_back (L (100, y + 150, 128, y + 92, 1.2f, true));             // Vb -> M
        s.lines.push_back (L (198, y + 150, 172, y + 92, 1.2f, true));             // Vc -> M
        s.lines.push_back (L (232, y + 158, 250, y + 158, 1.2f, false, true));     // -> out
        s.leds.push_back (D (268, y + 128, M257::Led_1 + n, 0xff3a7ae8));
        s.texts.push_back (T (n == 0 ? "1" : "2", 14, y + 16, 11.0f, 1, 1));
    }
    s.lines.push_back (L (4, 22, 296, 22)); s.lines.push_back (L (4, 214, 296, 214)); s.lines.push_back (L (4, 408, 296, 408));
    s.texts.push_back (T ("DUAL VOLTAGE PROCESSOR  VP-257", 150, 12, 10.5f, 4));
    s.texts.push_back (T ("Va K + Vb (1 - M) + Vc M + offset", 150, 38, 8.5f));
    s.texts.push_back (T ("Va K + Vb (1 - M) + Vc M + offset", 150, 230, 8.5f));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 257t", 150, 420, 9.0f, 1));
    return s;
}

// ---------------------------------------------------------------------------------------------
// DO-258 Dual Oscillator (after the 258t): two columns, frequency 5 Hz - 20 kHz, waveshape (1: sine-saw, 2: sine-square),
// processing attenuverter, f.m. depth; kbd / processing / f.m. / waveshape inputs, audio output.
// ---------------------------------------------------------------------------------------------
static ModuleSpec make258()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_258; s.name = "DUAL OSCILLATOR"; s.model = "DO-258"; s.after = "after Buchla 258t";
    s.width = 360.0f; s.height = 430.0f;
    const float cx[2] = { 92, 268 };
    for (int o = 0; o < 2; ++o)
    {
        const float x = cx[o];
        s.jacks.push_back (JT ("kbd", K::Banana, false, x - 63, 362, kTintGrey));
        s.jacks.push_back (J ("proc.", K::Banana, false, x - 21, 362));
        s.jacks.push_back (J ("f.m.", K::Mini, false, x + 21, 362));
        s.jacks.push_back (J ("shape", K::Banana, false, x + 63, 362));
    }
    s.jacks.push_back (J ("", K::Mini, true, cx[0], 50));
    s.jacks.push_back (J ("", K::Mini, true, cx[1], 50));
    static const char* fr[2] = { "freq1", "freq2" }; static const char* sh[2] = { "shape1", "shape2" };
    static const char* pr[2] = { "proc1", "proc2" }; static const char* fm[2] = { "fm1", "fm2" };
    for (int o = 0; o < 2; ++o)
    {
        const float x = cx[o];
        s.controls.push_back (C (fr[o], "", CtrlStyle::BigKnob, x, 128, kSkirt, "5|20|80|320|1.3k|5k|20k"));
        s.controls.push_back (C (sh[o], "", CtrlStyle::Knob, x, 226, kSkirt, "*|*|*|*|*|*|*|*|*"));
        s.controls.push_back (C (pr[o], "", CtrlStyle::SmallKnob, x - 42, 298, kSkirt, "-|*|0|*|+"));
        s.controls.push_back (C (fm[o], "", CtrlStyle::SmallKnob, x + 42, 298, kSkirt, "*|*|*|*|*"));
        s.texts.push_back (T ("frequency (Hz)", x, 180, 9.0f));
        s.texts.push_back (T ("processing", x - 42, 326, 8.5f)); s.texts.push_back (T ("f.m.", x + 42, 326, 8.5f));
        s.texts.push_back (T ("waveshape", x, 266, 9.0f));
        s.texts.push_back (T ("output", x + 36, 54, 9.0f, 0, 1));
        s.texts.push_back (T (o == 0 ? "1" : "2", x - 50, 54, 12.0f, 3));
    }
    iconSine (s, cx[0] - 36, 226); iconSaw (s, cx[0] + 38, 226);
    iconSine (s, cx[1] - 36, 226); iconSqr (s, cx[1] + 38, 226);
    s.controls.push_back (C ("volt1", "", CtrlStyle::Button, 180, 226));
    s.texts.push_back (T ("1V/oct", 180, 250, 8.5f));
    s.lines = { L (4, 22, 356, 22), L (180, 22, 180, 200), L (180, 262, 180, 408), L (4, 80, 356, 80), L (4, 338, 356, 338), L (4, 408, 356, 408) };
    s.texts.push_back (T ("DUAL OSCILLATOR  DO-258", 180, 12, 10.5f, 4));
    s.texts.push_back (T ("INPUTS", 180, 392, 9.0f, 1));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 258t", 180, 420, 9.0f, 1));
    return s;
}

// ---------------------------------------------------------------------------------------------
// QS-264 Quad Sample & Hold / Polyphonic Adaptor (after the 264t): four channels with sample / track switches,
// the polyphonic adaptor (poly input, number of voices, common c.v.), offset and modulation.
// ---------------------------------------------------------------------------------------------
static ModuleSpec make264()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_264; s.name = "QUAD SAMPLE & HOLD"; s.model = "QS-264"; s.after = "after Buchla 264t";
    s.width = 420.0f; s.height = 430.0f;
    const float cx[4] = { 48, 118, 188, 258 };
    for (int c = 0; c < 4; ++c) s.jacks.push_back (JT ("", K::Banana, false, cx[c], 150, kTintOrange, true));   // pulse in
    for (int c = 0; c < 4; ++c) s.jacks.push_back (J ("", K::Banana, false, cx[c], 206));                       // c.v. in
    s.jacks.push_back (JT ("poly in", K::Banana, false, 48, 360, kTintOrange, true));
    s.jacks.push_back (J ("mod. in", K::Mini, false, 140, 360));
    for (int c = 0; c < 4; ++c) s.jacks.push_back (J ("", K::Banana, true, cx[c], 262, true));                  // pulse out
    for (int c = 0; c < 4; ++c) s.jacks.push_back (J ("", K::Banana, true, cx[c], 318));                        // c.v. out
    static const char* tr[4] = { "track1", "track2", "track3", "track4" };
    const char* nums[4] = { "1", "2", "3", "4" };
    for (int c = 0; c < 4; ++c)
    {
        s.controls.push_back (C (tr[c], "", CtrlStyle::Toggle, cx[c], 70, kSkirt));
        s.texts.push_back (T (nums[c], cx[c], 40, 11.0f, 1));
        s.leds.push_back (D (cx[c], 110, M264::Led_1 + c));
    }
    s.controls.push_back (C ("common", "", CtrlStyle::Toggle, 362, 70, kSkirt));
    s.controls.push_back (C ("voices", "", CtrlStyle::Knob, 362, 160, kSkirt, "1|2|3|4"));
    s.controls.push_back (C ("offset", "", CtrlStyle::Knob, 362, 252, kSkirt, "-2|*|0|*|+2"));
    s.controls.push_back (C ("mod", "", CtrlStyle::Knob, 260, 362, kSkirt, "*|*|*|*|*"));
    s.lines = { L (4, 22, 416, 22), L (296, 22, 296, 340), L (4, 340, 416, 340), L (4, 408, 416, 408), L (4, 236, 296, 236), L (4, 124, 296, 124) };
    s.lines.push_back (L (162, 360, 228, 362, 1.2f, false, true));
    s.texts.push_back (T ("track", 296 - 4, 60, 8.5f, 0, 2)); s.texts.push_back (T ("sample", 296 - 4, 82, 8.5f, 0, 2));
    s.texts.push_back (T ("PULSE IN", 150, 176, 8.5f, 1)); s.texts.push_back (T ("C.V. IN", 150, 230, 8.5f, 1));
    s.texts.push_back (T ("PULSE OUT", 150, 288, 8.5f, 1)); s.texts.push_back (T ("C.V. OUT", 150, 336, 8.5f, 1));
    s.texts.push_back (T ("common", 362, 40, 9.0f)); s.texts.push_back (T ("on / off", 362, 100, 8.0f));
    s.texts.push_back (T ("voices", 362, 200, 9.0f)); s.texts.push_back (T ("offset", 362, 292, 9.0f));
    s.texts.push_back (T ("modulation", 260, 398, 8.5f)); s.texts.push_back (T ("POLYPHONIC ADAPTOR", 362, 330, 8.0f, 1));
    s.texts.push_back (T ("QUAD SAMPLE & HOLD  QS-264", 210, 12, 10.5f, 4));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 264t", 210, 420, 9.0f, 1));
    return s;
}

// ---------------------------------------------------------------------------------------------
// EF-230 Triple Envelope Follower (after the 230t): three channels, audio in, sensitivity, decay, pulse threshold,
// transient / sustained pulse mode, envelope (c.v.) and pulse outputs.
// ---------------------------------------------------------------------------------------------
static ModuleSpec make230()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_230; s.name = "TRIPLE ENVELOPE FOLLOWER"; s.model = "EF-230"; s.after = "after Buchla 230t";
    s.width = 330.0f; s.height = 430.0f;
    const float cx[3] = { 58, 165, 272 };
    for (int c = 0; c < 3; ++c) s.jacks.push_back (J ("", K::Mini, false, cx[c], 60));
    for (int c = 0; c < 3; ++c) s.jacks.push_back (J ("", K::Banana, true, cx[c] - 24, 368));
    for (int c = 0; c < 3; ++c) s.jacks.push_back (J ("", K::Banana, true, cx[c] + 24, 368, true));
    static const char* se[3] = { "sens1", "sens2", "sens3" }; static const char* de[3] = { "decay1", "decay2", "decay3" };
    static const char* th[3] = { "thr1", "thr2", "thr3" }; static const char* mo[3] = { "mode1", "mode2", "mode3" };
    const char* nums[3] = { "1", "2", "3" };
    for (int c = 0; c < 3; ++c)
    {
        const float x = cx[c];
        s.controls.push_back (C (se[c], "", CtrlStyle::Knob, x, 132, kSkirt, "*|*|*|*|*|*|*|*|*"));
        s.controls.push_back (C (de[c], "", CtrlStyle::Knob, x, 214, kSkirt, ".01|.1|1|3"));
        s.controls.push_back (C (th[c], "", CtrlStyle::SmallKnob, x - 22, 290, kSkirt));
        s.controls.push_back (C (mo[c], "", CtrlStyle::Toggle, x + 26, 290, 0xffd8261f));
        s.texts.push_back (T (nums[c], x - 34, 64, 11.0f, 3));
        s.texts.push_back (T ("sensitivity", x, 172, 8.5f)); s.texts.push_back (T ("decay", x, 254, 8.5f));
        s.texts.push_back (T ("thresh.", x - 22, 318, 7.5f));
        s.texts.push_back (T ("sust.", x + 26, 266, 7.5f)); s.texts.push_back (T ("trans.", x + 26, 316, 7.5f));
        s.texts.push_back (T ("c.v.", x - 24, 392, 8.5f)); s.texts.push_back (T ("pulse", x + 24, 392, 8.5f));
        s.leds.push_back (D (x - 24, 340, M230::Led_Env1 + c, 0xff3a7ae8));
        s.leds.push_back (D (x + 24, 340, M230::Led_Pulse1 + c));
        s.lines.push_back (L (x, 78, x, 100, 1.2f, false, true));
    }
    s.lines.push_back (L (4, 22, 326, 22)); s.lines.push_back (L (4, 408, 326, 408));
    s.lines.push_back (L (111, 22, 111, 408)); s.lines.push_back (L (219, 22, 219, 408));
    s.texts.push_back (T ("TRIPLE ENVELOPE FOLLOWER  EF-230", 165, 12, 10.5f, 4));
    s.texts.push_back (T ("CALABI YAU  -  after Buchla 230t", 165, 420, 9.0f, 1));
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
    static const ModuleSpec specs[MT_Count] = { makeMidi(), make259(), makeOutput(), make281(), make292(), make266(), make245(), make242(), make257(), make258(), make264(), make230() };
    return specs[t];
}
} // namespace wc
