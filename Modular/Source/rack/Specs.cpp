// Panel layouts, after the Series 200 / 200t front panels:
// light grey aluminium, blue silkscreen in lowercase italics, printed scales around the knobs,
// banana jacks colour-coded by signal (red = pulses, black = control in, blue = control out),
// 3.5 mm jacks for audio.
#include "Module.h"

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

// ---------------------------------------------------------------------------------------------
// CW-259 Complex Waveform Generator
// ---------------------------------------------------------------------------------------------
static ModuleSpec make259()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_259;
    s.name = "COMPLEX WAVEFORM GENERATOR";
    s.model = "CW-259";
    s.after = "after Buchla 259";
    s.width = 640.0f;
    s.height = 430.0f;
    s.sections = {
        { "modulation oscillator", 8, 34, 168, 362 },
        { "modulation", 182, 34, 150, 362 },
        { "principal oscillator", 338, 34, 150, 362 },
        { "timbre", 494, 34, 138, 362 },
    };
    s.jacks = {
        J ("pitch", K::Banana, false, 30, 336),       // In_MoPitch
        J ("c.v. in", K::Banana, false, 72, 336),     // In_MoCv
        J ("fm", K::Mini, false, 114, 336),           // In_MoFm
        J ("index", K::Banana, false, 202, 378),      // In_IndexCv
        J ("wave", K::Banana, false, 200, 336),       // In_ProgWave
        J ("pitch", K::Banana, false, 238, 336, true),   // In_ProgPitch  (program gates: red)
        J ("timbre", K::Banana, false, 276, 336, true),  // In_ProgTimbre
        J ("amp", K::Banana, false, 314, 336, true),     // In_ProgAmp
        J ("lock", K::Mini, false, 312, 378),         // In_Lock
        J ("pitch", K::Banana, false, 362, 336),      // In_PoPitch
        J ("c.v. in", K::Banana, false, 413, 336),    // In_PoCv
        J ("fm", K::Mini, false, 464, 336),           // In_PoFm
        J ("timbre", K::Banana, false, 516, 336),     // In_TimbreCv
        J ("sym", K::Banana, false, 563, 336),        // In_SymCv
        J ("order", K::Banana, false, 610, 336),      // In_OrderCv
        J ("signal", K::Mini, true, 114, 378),        // Out_MoSignal
        J ("c.v. out", K::Banana, true, 156, 378),    // Out_MoCv
        J ("sine", K::Mini, true, 413, 378),          // Out_Sine
        J ("square", K::Mini, true, 464, 378),        // Out_Square
        J ("final", K::Mini, true, 563, 378),         // Out_Final1
        J ("final", K::Mini, true, 610, 378),         // Out_Final2
    };
    s.controls = {
        C ("mo_freq", "frequency", CtrlStyle::BigKnob, 92, 104, kGreyKnob, "28|55|110|220|440|880|1.7k|3.5k|7k"),
        C ("mo_fine", "fine", CtrlStyle::Knob, 46, 196, kGreyKnob, "-|0|+"),
        C ("mo_wave", "saw|sqr|tri", CtrlStyle::Switch3, 124, 196),
        C ("mo_range", "high|low", CtrlStyle::Toggle, 40, 266),
        C ("mo_cv", "c.v.", CtrlStyle::SmallKnob, 100, 268, kBlack, "-|0|+"),
        C ("mo_fm", "fm", CtrlStyle::SmallKnob, 146, 268, kBlack),

        C ("idx", "mod index", CtrlStyle::BigKnob, 236, 104, kGreyKnob, "0|2|4|6|8|10"),
        C ("idx_cv", "c.v.", CtrlStyle::SmallKnob, 306, 84, kBlack),
        C ("mod_amp", "amp", CtrlStyle::Toggle, 212, 204),
        C ("mod_pitch", "pitch", CtrlStyle::Toggle, 257, 204),
        C ("mod_timbre", "timbre", CtrlStyle::Toggle, 302, 204),
        C ("prog", "local|remote|both", CtrlStyle::Switch3, 226, 268),
        C ("lock", "lock", CtrlStyle::SmallKnob, 262, 378, kBlack),

        C ("po_freq", "frequency", CtrlStyle::BigKnob, 413, 104, kGreyKnob, "28|55|110|220|440|880|1.7k|3.5k|7k"),
        C ("po_fine", "fine", CtrlStyle::Knob, 372, 196, kGreyKnob, "-|0|+"),
        C ("po_cv", "c.v.", CtrlStyle::SmallKnob, 456, 196, kBlack, "-|0|+"),
        C ("po_fm", "fm", CtrlStyle::SmallKnob, 456, 262, kBlack),

        C ("timbre", "timbre", CtrlStyle::BigKnob, 540, 104, kGreyKnob, "0|2|4|6|8|10"),
        C ("timbre_cv", "c.v.", CtrlStyle::SmallKnob, 608, 84, kBlack, "-|0|+"),
        C ("sym", "symmetry", CtrlStyle::Knob, 526, 222, kGreyKnob, "0|5|10"),
        C ("order", "order", CtrlStyle::Knob, 602, 222, kGreyKnob, "0|5|10"),
    };
    return s;
}

// ---------------------------------------------------------------------------------------------
// QF-281 Quad Function Generator - layout of the 281t: four rows A..D, quadrature column on the right
// ---------------------------------------------------------------------------------------------
static ModuleSpec make281()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_281;
    s.name = "QUAD FUNCTION GENERATOR";
    s.model = "QF-281";
    s.after = "after Buchla 281";
    s.width = 486.0f;
    s.height = 430.0f;
    const float rowY[4] = { 78, 162, 246, 330 };
    for (int n = 0; n < 4; ++n) s.sections.push_back ({ "", 8, rowY[n] - 40, 372, 80 });
    s.sections.push_back ({ "quadrature", 386, 38, 92, 372 });
    for (int n = 0; n < 4; ++n)          // inputs per row: trig, cycle, c.v. in (attack), c.v. in (decay)
    {
        const float y = rowY[n];
        s.jacks.push_back (J ("trig", K::Banana, false, 24, y - 16, true));
        s.jacks.push_back (J ("cycle", K::Banana, false, 62, y - 16, true));
        s.jacks.push_back (J ("c.v. in", K::Banana, false, 106, y - 16));
        s.jacks.push_back (J ("c.v. in", K::Banana, false, 306, y - 16));
    }
    for (int n = 0; n < 4; ++n)          // outputs per row: function (blue), end of decay (red)
    {
        const float y = rowY[n];
        s.jacks.push_back (J ("", K::Banana, true, 346, y + 17));
        s.jacks.push_back (J ("", K::Banana, true, 346, y - 17, true));
    }
    s.jacks.push_back (J ("", K::Banana, true, 410, 152));    // quadrature mixes
    s.jacks.push_back (J ("", K::Banana, true, 454, 152));
    s.jacks.push_back (J ("", K::Banana, true, 410, 302));
    s.jacks.push_back (J ("", K::Banana, true, 454, 302));

    static const char* att[4] = { "att1", "att2", "att3", "att4" };
    static const char* dec[4] = { "dec1", "dec2", "dec3", "dec4" };
    static const char* mode[4] = { "mode1", "mode2", "mode3", "mode4" };
    static const char* trg[4] = { "trig1", "trig2", "trig3", "trig4" };
    for (int n = 0; n < 4; ++n)
    {
        const float y = rowY[n];
        s.controls.push_back (C (trg[n], "", CtrlStyle::Push, 24, y + 22));
        s.controls.push_back (C (mode[n], "trans|sust|cycle", CtrlStyle::Switch3, 62, y + 22));
        s.controls.push_back (C (att[n], "attack", CtrlStyle::Knob, 162, y - 2, kGreyKnob, kTimeScale));
        s.controls.push_back (C (dec[n], "decay", CtrlStyle::Knob, 252, y - 2, kGreyKnob, kTimeScale));
    }
    s.controls.push_back (C ("mix12", "", CtrlStyle::BigKnob, 432, 88, kGreyKnob, "A|B"));
    s.controls.push_back (C ("mix34", "", CtrlStyle::BigKnob, 432, 238, kGreyKnob, "C|D"));
    s.controls.push_back (C ("quad12", "on|off", CtrlStyle::Toggle, 404, 368));
    s.controls.push_back (C ("quad34", "on|off", CtrlStyle::Toggle, 444, 368));
    return s;
}

// ---------------------------------------------------------------------------------------------
// LG-292 Quad Lowpass Gate - four columns with blue offset knobs and blue mode switches, "all" mix out
// ---------------------------------------------------------------------------------------------
static ModuleSpec make292()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_292;
    s.name = "QUAD LOPASS GATE";
    s.model = "LG-292";
    s.after = "after Buchla 292";
    s.width = 444.0f;
    s.height = 430.0f;
    const float cx[4] = { 53, 147, 241, 335 };
    for (int n = 0; n < 4; ++n) s.sections.push_back ({ "", cx[n] - 45, 36, 90, 372 });
    s.sections.push_back ({ "", 386, 36, 50, 372 });
    for (int n = 0; n < 4; ++n)
    {
        s.jacks.push_back (J ("input", K::Mini, false, cx[n], 290));
        s.jacks.push_back (J ("c.v. in", K::Banana, false, cx[n], 336));
    }
    for (int n = 0; n < 4; ++n) s.jacks.push_back (J ("output", K::Mini, true, cx[n], 382));
    s.jacks.push_back (J ("all", K::Mini, true, 411, 382));
    static const char* off[4] = { "off1", "off2", "off3", "off4" };
    static const char* cv[4] = { "cv1", "cv2", "cv3", "cv4" };
    static const char* res[4] = { "res1", "res2", "res3", "res4" };
    static const char* mode[4] = { "mode1", "mode2", "mode3", "mode4" };
    for (int n = 0; n < 4; ++n)
    {
        s.controls.push_back (C (off[n], "offset", CtrlStyle::Knob, cx[n], 88, kBlueKnob, "0|5|10"));
        s.controls.push_back (C (cv[n], "c.v. level", CtrlStyle::SmallKnob, cx[n] - 20, 160, kBlack));
        s.controls.push_back (C (res[n], "res", CtrlStyle::SmallKnob, cx[n] + 22, 160, kBlack));
        s.controls.push_back (C (mode[n], "lopass|combo|gate", CtrlStyle::Switch3, cx[n] - 26, 224, kBlueKnob));
    }
    return s;
}

// ---------------------------------------------------------------------------------------------
// SU-266 Source of Uncertainty
// ---------------------------------------------------------------------------------------------
static ModuleSpec make266()
{
    using K = JackKind;
    ModuleSpec s;
    s.type = MT_266;
    s.name = "SOURCE OF UNCERTAINTY";
    s.model = "SU-266";
    s.after = "after Buchla 266";
    s.width = 524.0f;
    s.height = 430.0f;
    s.sections = {
        { "noise", 8, 36, 82, 176 },
        { "fluctuating random voltages", 96, 36, 150, 176 },
        { "sample & hold", 252, 36, 264, 176 },
        { "integrator", 8, 220, 166, 188 },
        { "quantized random voltages", 180, 220, 166, 188 },
        { "stored random voltages", 352, 220, 164, 188 },
    };
    // inputs (DSP order = M266::In)
    s.jacks = {
        J ("c.v. in", K::Banana, false, 122, 176),            // In_FluctCv
        J ("pulse in", K::Banana, false, 278, 90, true),      // In_ShPulse
        J ("c.v. in", K::Banana, false, 278, 166),            // In_ShCv
        J ("input", K::Banana, false, 34, 278),               // In_IntIn
        J ("c.v. in", K::Banana, false, 34, 370),             // In_IntCv
        J ("pulse in", K::Banana, false, 206, 278, true),     // In_QPulse
        J ("c.v. in", K::Banana, false, 206, 370),            // In_QCv
        J ("pulse in", K::Banana, false, 378, 278, true),     // In_SPulse
        J ("c.v. in", K::Banana, false, 378, 370),            // In_SCv
        // outputs (DSP order = M266::Out)
        J ("pink", K::Mini, true, 49, 82),                    // Out_Pink
        J ("white", K::Mini, true, 49, 130),                  // Out_White
        J ("blue", K::Mini, true, 49, 178),                   // Out_Blue
        J ("c.v. out", K::Banana, true, 220, 176),            // Out_Fluct
        J ("c.v. out", K::Banana, true, 344, 128),            // Out_Sh
        J ("alt", K::Banana, true, 438, 90),                  // Out_ShAltA
        J ("alt", K::Banana, true, 488, 90),                  // Out_ShAltB
        J ("alt", K::Banana, true, 438, 166, true),           // Out_ShPulseA
        J ("alt", K::Banana, true, 488, 166, true),           // Out_ShPulseB
        J ("c.v. out", K::Banana, true, 148, 370),            // Out_Int
        J ("c.v. out", K::Banana, true, 320, 370),            // Out_Quant
        J ("c.v. out", K::Banana, true, 452, 370),            // Out_StoredUni
        J ("c.v. out", K::Banana, true, 494, 370),            // Out_StoredDist
    };
    s.controls = {
        C ("rate", "rate", CtrlStyle::Knob, 171, 98, kGreyKnob, ".05|.5|5|50"),
        C ("smooth", "smoothing", CtrlStyle::Knob, 112, 290, kGreyKnob, ".01|.1|1|10"),
        C ("steps", "n", CtrlStyle::Knob, 272, 290, kGreyKnob, "1|2|3|4|5|6"),
        C ("qmode", "n+1|2^n", CtrlStyle::Toggle, 312, 286),
        C ("dist", "distribution", CtrlStyle::Knob, 452, 290, kGreyKnob, "lo|bell|hi"),
    };
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
    static const ModuleSpec specs[MT_Count] = { makeMidi(), make259(), makeOutput(), make281(), make292(), make266() };
    return specs[t];
}
} // namespace wc
