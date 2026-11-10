// Renders the West Coast Modular editor to PNG:  WcUiSnapshot <outDir>
// one image per built-in patch, plus a test that patches a cable through the editor's API.
#include "WcEditor.h"
#include "modules/M259.h"
#include "ui/RackView.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (juce::String (argc > 1 ? argv[1] : "/tmp/wc_ui"));
    out.createDirectory();
    const auto names = WcProcessor::builtinPatchNames();
    for (int k : { 16, 17, 99 })
    {
        WcProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        proc.loadBuiltinPatch (k == 99 ? 9 : k);
        if (k == 99) for (int i = 1; i < wc::kInstOutput; ++i) proc.setPresent (i, true);
        // play a note so the meters and LEDs show something
        for (int b = 0; b < 60; ++b)
        {
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
            proc.processBlock (buf, midi);
        }
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (1400, k == 99 ? 3600 : 1300);
        for (int i = 0; i < 5; ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, k == 99 ? 1.0f : 2.0f);
        if (k == 99)   // every panel on its own, at 2x (for the manual)
        {
            std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
            {
                if (auto* pn = dynamic_cast<wc::ui::ModulePanel*> (&c))
                {
                    for (auto* ch : pn->getChildren()) if (dynamic_cast<juce::TextButton*> (ch) != nullptr) ch->setVisible (false);   // no remove button
                    auto pi = pn->createComponentSnapshot (pn->getLocalBounds(), true, 2.0f);
                    auto pf = out.getChildFile ("panel_" + wc::instName (pn->inst).replace (" ", "") + ".png");
                    pf.deleteFile();
                    juce::FileOutputStream ps (pf);
                    juce::PNGImageFormat().writeImageToStream (pi, ps);
                    return;
                }
                for (auto* ch : c.getChildren()) walk (*ch);
            };
            walk (*ed);
        }
        auto f = out.getChildFile ("patch" + juce::String (k) + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        std::printf ("%d: %d cables\n", k, (int) proc.getCables().size());
    }
    // connect checks: kind mismatch must be refused, a new cable into a used input replaces the old one
    {
        WcProcessor proc;
        const bool bad = proc.connect (wc::kInstMidi, 0, wc::kInst259, wc::M259::In_PoFm);       // banana -> mini
        const int before = (int) proc.getCables().size();
        const bool good = proc.connect (wc::kInstMidi, 3, wc::kInst259, wc::M259::In_PoPitch);  // replaces pitch
        std::printf ("banana->mini refused: %s, replace keeps count: %s\n", bad ? "NO" : "yes", (good && (int) proc.getCables().size() == before) ? "yes" : "NO");
    }
    return 0;
}
