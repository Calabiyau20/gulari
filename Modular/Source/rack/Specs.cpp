#include "Module.h"
#include "../modules/M259.h"
#include "../modules/MidiInterface.h"
#include "../modules/OutputModule.h"

namespace wc
{
// knob cap colours (Series 200 panels: aluminium with coloured knob caps)
static constexpr juce::uint32 kBlue = 0xff2f6db3, kRed = 0xffc8402e, kOrange = 0xffe08a2c, kYellow = 0xffe4c33a,
                              kGreen = 0xff3c9a5f, kGrey = 0xff7d8288, kWhite = 0xffe9e6df;

static ModuleSpec make259()
{
    using J = JackKind;
    ModuleSpec s;
    s.type = MT_259;
    s.name = "COMPLEX WAVEFORM GENERATOR";
    s.model = "CW-259";
    s.after = "after Buchla 259";
    s.width = 640.0f;
    s.height = 430.0f;
    s.sections = {
        { "MODULATION OSCILLATOR", 8, 40, 168, 382 },
        { "MODULATION", 182, 40, 150, 382 },
        { "PRINCIPAL OSCILLATOR", 338, 40, 150, 382 },
        { "WAVESHAPE", 494, 40, 138, 382 },
    };
    // inputs (DSP order = M259::In)
    s.jacks = {
        { "PITCH", J::Banana, false, 30, 352 },     // In_MoPitch
        { "CV", J::Banana, false, 72, 352 },        // In_MoCv
        { "FM", J::Mini, false, 114, 352 },         // In_MoFm
        { "INDEX", J::Banana, false, 202, 400 },    // In_IndexCv
        { "WAVE", J::Banana, false, 200, 352 },     // In_ProgWave
        { "PITCH", J::Banana, false, 238, 352 },    // In_ProgPitch
        { "TIMBRE", J::Banana, false, 276, 352 },   // In_ProgTimbre
        { "AMP", J::Banana, false, 314, 352 },      // In_ProgAmp
        { "LOCK", J::Mini, false, 312, 400 },       // In_Lock
        { "PITCH", J::Banana, false, 362, 352 },    // In_PoPitch
        { "CV", J::Banana, false, 413, 352 },       // In_PoCv
        { "FM", J::Mini, false, 464, 352 },         // In_PoFm
        { "TIMBRE", J::Banana, false, 516, 352 },   // In_TimbreCv
        { "SYM", J::Banana, false, 563, 352 },      // In_SymCv
        { "ORDER", J::Banana, false, 610, 352 },    // In_OrderCv
        // outputs (DSP order = M259::Out)
        { "SIGNAL", J::Mini, true, 114, 400 },      // Out_MoSignal
        { "CV OUT", J::Banana, true, 156, 400 },    // Out_MoCv
        { "SINE", J::Mini, true, 413, 400 },        // Out_Sine
        { "SQUARE", J::Mini, true, 464, 400 },      // Out_Square
        { "FINAL", J::Mini, true, 563, 400 },       // Out_Final1
        { "FINAL", J::Mini, true, 610, 400 },       // Out_Final2
    };
    s.controls = {
        { "mo_freq", "FREQUENCY", CtrlStyle::BigKnob, 92, 108, kBlue },
        { "mo_fine", "FINE", CtrlStyle::Knob, 46, 196, kBlue },
        { "mo_wave", "SAW  SQR  TRI", CtrlStyle::Switch3, 132, 196, kGrey },
        { "mo_range", "HI / LO", CtrlStyle::Toggle, 46, 262, kGrey },
        { "mo_cv", "CV", CtrlStyle::SmallKnob, 100, 270, kWhite },
        { "mo_fm", "FM", CtrlStyle::SmallKnob, 146, 270, kWhite },

        { "idx", "MOD INDEX", CtrlStyle::BigKnob, 236, 108, kYellow },
        { "idx_cv", "CV", CtrlStyle::SmallKnob, 304, 92, kWhite },
        { "mod_amp", "AMP", CtrlStyle::Toggle, 206, 196, kGrey },
        { "mod_pitch", "PITCH", CtrlStyle::Toggle, 257, 196, kGrey },
        { "mod_timbre", "TIMBRE", CtrlStyle::Toggle, 308, 196, kGrey },
        { "prog", "LOCAL  REM  BOTH", CtrlStyle::Switch3, 236, 266, kGrey },
        { "lock", "LOCK", CtrlStyle::SmallKnob, 262, 400, kWhite },

        { "po_freq", "FREQUENCY", CtrlStyle::BigKnob, 413, 108, kRed },
        { "po_fine", "FINE", CtrlStyle::Knob, 372, 196, kRed },
        { "po_cv", "CV", CtrlStyle::SmallKnob, 454, 196, kWhite },
        { "po_fm", "FM", CtrlStyle::SmallKnob, 454, 262, kWhite },

        { "timbre", "TIMBRE", CtrlStyle::BigKnob, 538, 108, kGreen },
        { "timbre_cv", "CV", CtrlStyle::SmallKnob, 606, 92, kWhite },
        { "sym", "SYMMETRY", CtrlStyle::Knob, 524, 222, kOrange },
        { "order", "ORDER", CtrlStyle::Knob, 600, 222, kOrange },
    };
    return s;
}

static ModuleSpec makeMidi()
{
    using J = JackKind;
    ModuleSpec s;
    s.type = MT_Midi;
    s.name = "MIDI INTERFACE";
    s.model = "KB-225";
    s.after = "";
    s.width = 176.0f;
    s.height = 430.0f;
    s.sections = { { "KEYBOARD", 8, 40, 160, 190 }, { "OUTPUTS", 8, 236, 160, 186 } };
    s.jacks = {
        { "PITCH", J::Banana, true, 46, 288 },
        { "GATE", J::Banana, true, 88, 288 },
        { "PULSE", J::Banana, true, 130, 288 },
        { "VEL", J::Banana, true, 46, 340 },
        { "PRESS", J::Banana, true, 88, 340 },
        { "MOD", J::Banana, true, 130, 340 },
        { "BEND", J::Banana, true, 46, 392 },
        { "PITCH", J::Banana, true, 88, 392 },      // second pitch output (stacking convenience)
    };
    s.controls = {
        { "midi_glide", "GLIDE", CtrlStyle::Knob, 88, 100, kGrey },
        { "midi_bend", "BEND", CtrlStyle::SmallKnob, 50, 180, kWhite },
        { "midi_trans", "OCTAVE", CtrlStyle::SmallKnob, 126, 180, kWhite },
    };
    return s;
}

static ModuleSpec makeOutput()
{
    using J = JackKind;
    ModuleSpec s;
    s.type = MT_Output;
    s.name = "OUTPUT";
    s.model = "OUT-2";
    s.after = "";
    s.width = 156.0f;
    s.height = 430.0f;
    s.sections = { { "LEVEL", 8, 40, 140, 246 }, { "INPUTS", 8, 292, 140, 130 } };
    s.jacks = {
        { "LEFT", J::Mini, false, 46, 344 },
        { "RIGHT", J::Mini, false, 110, 344 },
        { "LEVEL CV", J::Banana, false, 78, 398 },
    };
    s.controls = {
        { "out_level", "LEVEL", CtrlStyle::Knob, 78, 96, kGrey },
        { "out_cv", "CV", CtrlStyle::SmallKnob, 44, 176, kWhite },
        { "out_lag", "LAG", CtrlStyle::SmallKnob, 112, 176, kWhite },
        { "out_master", "MASTER", CtrlStyle::Knob, 78, 244, kGrey },
    };
    return s;
}

// QF-281 Quad Function Generator: two pairs (A-B, C-D), each pair with a QUAD switch
static ModuleSpec make281()
{
    using J = JackKind;
    ModuleSpec s;
    s.type = MT_281;
    s.name = "QUAD FUNCTION GENERATOR";
    s.model = "QF-281";
    s.after = "after Buchla 281";
    s.width = 424.0f;
    s.height = 430.0f;
    s.sections = { { "FUNCTIONS  A - B", 8, 40, 200, 382 }, { "FUNCTIONS  C - D", 216, 40, 200, 382 } };
    const float cx[4] = { 58, 158, 266, 366 };
    const char* names[4] = { "A", "B", "C", "D" };
    juce::ignoreUnused (names);
    for (int n = 0; n < 4; ++n)          // inputs: TRIG, ATT, DEC per channel
    {
        s.jacks.push_back ({ "TRIG", J::Banana, false, cx[n] - 26, 352 });
        s.jacks.push_back ({ "ATT", J::Banana, false, cx[n] - 26, 400 });
        s.jacks.push_back ({ "DEC", J::Banana, false, cx[n], 400 });
    }
    for (int n = 0; n < 4; ++n)          // outputs: FUNCTION, END OF DECAY
    {
        s.jacks.push_back ({ "OUT", J::Banana, true, cx[n] + 26, 352 });
        s.jacks.push_back ({ "EOD", J::Banana, true, cx[n] + 26, 400 });
    }
    static const char* att[4] = { "att1", "att2", "att3", "att4" };
    static const char* dec[4] = { "dec1", "dec2", "dec3", "dec4" };
    static const char* mode[4] = { "mode1", "mode2", "mode3", "mode4" };
    static const char* trg[4] = { "trig1", "trig2", "trig3", "trig4" };
    for (int n = 0; n < 4; ++n)
    {
        s.controls.push_back ({ att[n], "ATTACK", CtrlStyle::Knob, cx[n], 112, kOrange });
        s.controls.push_back ({ dec[n], "DECAY", CtrlStyle::Knob, cx[n], 194, kBlue });
        s.controls.push_back ({ mode[n], "TRAN  SUS  CYC", CtrlStyle::Switch3, cx[n], 262, kGrey });
        s.controls.push_back ({ trg[n], "TRIGGER", CtrlStyle::Push, cx[n], 304, kGrey });
    }
    s.controls.push_back ({ "quad12", "QUAD", CtrlStyle::Toggle, 108, 66, kGrey });
    s.controls.push_back ({ "quad34", "QUAD", CtrlStyle::Toggle, 316, 66, kGrey });
    return s;
}

// LG-292 Quad Lowpass Gate: four vactrol channels
static ModuleSpec make292()
{
    using J = JackKind;
    ModuleSpec s;
    s.type = MT_292;
    s.name = "QUAD LOWPASS GATE";
    s.model = "LG-292";
    s.after = "after Buchla 292";
    s.width = 392.0f;
    s.height = 430.0f;
    s.sections = { { "1", 8, 40, 90, 382 }, { "2", 102, 40, 90, 382 }, { "3", 196, 40, 90, 382 }, { "4", 290, 40, 90, 382 } };
    const float cx[4] = { 53, 147, 241, 335 };
    for (int n = 0; n < 4; ++n)
    {
        s.jacks.push_back ({ "SIGNAL", J::Mini, false, cx[n] - 20, 352 });
        s.jacks.push_back ({ "CONTROL", J::Banana, false, cx[n] + 20, 352 });
    }
    for (int n = 0; n < 4; ++n) s.jacks.push_back ({ "OUT", J::Mini, true, cx[n], 400 });
    static const char* off[4] = { "off1", "off2", "off3", "off4" };
    static const char* cv[4] = { "cv1", "cv2", "cv3", "cv4" };
    static const char* res[4] = { "res1", "res2", "res3", "res4" };
    static const char* mode[4] = { "mode1", "mode2", "mode3", "mode4" };
    for (int n = 0; n < 4; ++n)
    {
        s.controls.push_back ({ off[n], "OFFSET", CtrlStyle::Knob, cx[n], 104, kGrey });
        s.controls.push_back ({ cv[n], "CV LEVEL", CtrlStyle::SmallKnob, cx[n], 180, kWhite });
        s.controls.push_back ({ res[n], "RES", CtrlStyle::SmallKnob, cx[n], 238, kWhite });
        s.controls.push_back ({ mode[n], "LP  BOTH  VCA", CtrlStyle::Switch3, cx[n], 288, kGrey });
    }
    return s;
}

const ModuleSpec& specFor (ModuleType t)
{
    static const ModuleSpec specs[MT_Count] = { makeMidi(), make259(), makeOutput(), make281(), make292() };
    return specs[t];
}
} // namespace wc
