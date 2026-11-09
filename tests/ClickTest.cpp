// Moves every Synthgularity parameter mid-note and reports instant jumps.  Usage: SynthgularityClickTest [threshold] [filter]
#include "PluginProcessor.h"
#include "ClickHarness.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double threshold = argc > 1 ? juce::String (argv[1]).getDoubleValue() : 0.35;
    const juce::String only = argc > 2 ? juce::String (argv[2]) : juce::String();
    auto skipBase = [&] (const juce::String& id)
    {
        if (only.isNotEmpty() && ! id.startsWith (only)) return true;
        for (auto* s : { "mtx", "seq", "arp_", "gr_", "gm", "smp_", "oversampling", "lfo", "chaos", "menv", "fx_slot", "mb_slot", "rt_order", "poly" })
            if (id.startsWith (s)) return true;
        return false;
    };
    auto maker = [] (bool fxOn)
    {
        return [fxOn]
        {
            auto proc = std::make_unique<SynthgularityProcessor>();
            proc->setPlayConfigDetails (0, 2, 48000.0, 128);
            auto set = [&] (const char* id, float v) { if (auto* p = proc->apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
            proc->initPreset();
            set ("gr_on", 0); set ("wtA_on", 1);
            if (! fxOn)
            {
                set ("wtB_on", 1); set ("wtC_on", 1); set ("wtB_oct", -1); set ("wtC_semi", 7);
                set ("sp_on", 1); set ("pl_on", 1); set ("sub_level", 0.3f); set ("bite_drive", 0.2f);
            }
            else
                for (auto* id : { "fx_st_on", "fx_fl_on", "fx_ph_on", "fx_dl_on", "fx_rn_on", "fx_gr_on", "fx_rv_on", "ff1_on", "ff2_on",
                                  "mb_tp_on", "mb_eq_on", "mb_cp_on", "mb_ch_on", "mb_im_on", "mb_dj_on", "mb_bc_on", "mb_tr_on", "mb_lm_on" })
                    set (id, 1);
            return proc;
        };
    };
    int bad = 0;
    bad += clicktest::report (clicktest::run<SynthgularityProcessor> (maker (false),
             [&] (const juce::String& id) { return skipBase (id) || id.startsWith ("fx_") || id.startsWith ("ff") || id.startsWith ("mb_"); },
             { 48, 55, 60 }), threshold, "voice sources");
    bad += clicktest::report (clicktest::run<SynthgularityProcessor> (maker (true),
             [&] (const juce::String& id) { return skipBase (id) || ! (id.startsWith ("fx_") || id.startsWith ("ff") || id.startsWith ("mb_") || id == "master_gain"); },
             { 48, 55, 60 }), threshold, "effects");
    std::printf ("TOTAL %d\n", bad);
    return 0;
}
