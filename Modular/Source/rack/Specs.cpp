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

const ModuleSpec& specFor (ModuleType t)
{
    static const ModuleSpec specs[MT_Count] = { makeMidi(), make259(), makeOutput() };
    return specs[t];
}
} // namespace wc
