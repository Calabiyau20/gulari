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
    for (int k : { 21, 22, 99 })
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
        ed->setSize (1400, k == 99 ? 6200 : 1300);
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
    // zoom: Ctrl + wheel over the rack steps progressively and stops at 50 % and 150 %
    {
        WcProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (1400, 1300);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        wc::ui::RackView* rv = nullptr;
        std::function<void (juce::Component&)> find = [&] (juce::Component& c)
        {
            if (rv != nullptr) return;
            if (auto* r = dynamic_cast<wc::ui::RackView*> (&c)) { rv = r; return; }
            for (auto* ch : c.getChildren()) find (*ch);
        };
        find (*ed);
        if (rv == nullptr || rv->onZoomGesture == nullptr) { std::printf ("zoom: NO gesture hook\n"); return 1; }
        const float start = proc.uiZoom;
        rv->onZoomGesture (0.1f, { 400.0f, 300.0f });
        const float up = proc.uiZoom;
        for (int i = 0; i < 40; ++i) rv->onZoomGesture (0.1f, { 400.0f, 300.0f });
        const float top = proc.uiZoom;
        for (int i = 0; i < 80; ++i) rv->onZoomGesture (-0.1f, { 400.0f, 300.0f });
        const float bottom = proc.uiZoom;
        std::printf ("zoom: %.2f -> %.2f on one notch, ceiling %.2f, floor %.2f  %s\n", start, up, top, bottom,
                     (up > start && std::fabs (top - 1.5f) < 0.001f && std::fabs (bottom - 0.5f) < 0.001f) ? "ok" : "BAD");
    }
    // AF-250: every push button on the panel has to move something. The edit lamp must not gate the rest:
    // for a while it did, and thirteen of the switches went dead unless EDIT happened to be lit.
    {
        WcProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        proc.setPresent (wc::kInst250, true);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (1400, 1300);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);

        wc::ui::ModulePanel* panel = nullptr;
        std::function<void (juce::Component&)> find = [&] (juce::Component& c)
        {
            if (panel != nullptr) return;
            if (auto* p = dynamic_cast<wc::ui::ModulePanel*> (&c)) { if (p->inst == wc::kInst250) { panel = p; return; } }
            for (auto* ch : c.getChildren()) find (*ch);
        };
        find (*ed);
        if (panel == nullptr) { std::printf ("250 buttons: NO panel\n"); return 1; }

        // the whole apvts state, so a press that changes nothing anywhere is caught
        auto snapshot = [&proc]
        {
            juce::String s;
            for (auto* p : proc.getParameters()) s << p->getValue() << ",";
            return s;
        };
        auto press = [&] (const juce::String& id) -> bool
        {
            for (auto* ch : panel->getChildren())
                if (auto* b = dynamic_cast<wc::ui::PushButton*> (ch))
                    if (b->getComponentID() == id && b->onPress != nullptr) { b->onPress(); return true; }
            return false;
        };

        // "set" walks a three-press sequence, so it gets three; the rest must bite on the first press
        const char* ids[] = { "editbtn", "vset", "remote", "addrbtn", "tq", "ti", "vq", "vi",
                              "p1", "p2", "text", "vext", "fix", "trng", "vrng", "mode", "set" };
        juce::StringArray dead;
        for (auto* id : ids)
        {
            const auto before = snapshot();
            if (! press (id)) { dead.add (juce::String (id) + " (missing)"); continue; }
            if (snapshot() == before) dead.add (id);
        }
        // and they have to keep working with the edit lamp off, which is where the regression lived
        press ("editbtn");
        for (auto* id : { "tq", "p1", "trng", "mode", "vrng" })
        {
            const auto before = snapshot();
            press (id);
            if (snapshot() == before) dead.add (juce::String (id) + " (lamp off)");
        }
        std::printf ("250 buttons: %d tested, %s\n", (int) (sizeof (ids) / sizeof (ids[0])),
                     dead.isEmpty() ? "all respond  ok" : ("DEAD: " + dead.joinIntoString (", ")).toRawUTF8());
        if (! dead.isEmpty()) return 1;
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
