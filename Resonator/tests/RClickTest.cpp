// Moves every Resonator parameter mid-note and reports instant jumps.  Usage: ResonatorClickTest [threshold] [filter]
#include "RProcessor.h"
#include "ClickHarness.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double threshold = argc > 1 ? juce::String (argv[1]).getDoubleValue() : 0.35;
    const juce::String only = argc > 2 ? juce::String (argv[2]) : juce::String();
    auto make = []
    {
        auto proc = std::make_unique<ResonatorProcessor>();
        proc->setPlayConfigDetails (0, 2, 48000.0, 128);
        auto set = [&] (const char* id, float v) { if (auto* p = proc->apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
        for (auto* o : { "osca", "oscb", "oscc", "oscd" })
        {
            set ((juce::String (o) + "_on").toRawUTF8(), 1);
            set ((juce::String (o) + "_rphase").toRawUTF8(), 0);
            set ((juce::String (o) + "_level").toRawUTF8(), 0.5f);
        }
        set ("oscb_oct", -1); set ("oscc_semi", 7); set ("oscd_n", 3);
        set ("fm_ab", 0.2f); set ("pd_ca", 0.2f);
        if (std::getenv ("CYR_SCANBASE") != nullptr)
            for (auto* o : { "osca", "oscb", "oscc", "oscd" })
            {
                set ((juce::String (o) + "_mode").toRawUTF8(), 1);
                set ((juce::String (o) + "_cover").toRawUTF8(), 0.3f);
                set ((juce::String (o) + "_pos").toRawUTF8(), 0.4f);
            }
        if (std::getenv ("CYR_SIMPLE") != nullptr)
        {
            for (auto* o : { "oscb", "oscc", "oscd" }) set ((juce::String (o) + "_on").toRawUTF8(), 0);
            set ("fm_ab", 0); set ("pd_ca", 0);
        }
        return proc;
    };
    auto skip = [&] (const juce::String& id)
    {
        if (only.isNotEmpty() && ! id.startsWith (only)) return true;
        for (auto* s : { "mod", "lfo", "env", "quality", "fx_slot", "mb_slot", "rt_order", "poly" })
            if (id.startsWith (s)) return true;
        return false;
    };
    const int bad = clicktest::report (clicktest::run<ResonatorProcessor> (make, skip, { 48, 55, 60 }), threshold, "resonator");
    std::printf ("TOTAL %d\n", bad);
    return 0;
}
