// Renders the West Coast Modular editor to PNG:  WcUiSnapshot <outDir>
// one image per built-in patch, plus a test that patches a cable through the editor's API.
#include "WcEditor.h"
#include "modules/M250.h"
#include "modules/M259.h"
#include "modules/M281.h"
#include "modules/MidiInterface.h"
#include "ui/RackView.h"
#include <set>
#if JUCE_LINUX
 #include <dlfcn.h>
#endif

// Xvfb runs without a window manager, so a popup menu window asks for a window-manager atom that does not exist and
// Xlib's default handler ends the program. A plugin's host (or JUCE, in a standalone app) installs its own handler;
// this console test has to do it itself.
static void quietXErrors()
{
   #if JUCE_LINUX
    using Handler = int (*) (void*, void*);
    using SetHandler = Handler (*) (Handler);
    if (auto* lib = dlopen ("libX11.so.6", RTLD_LAZY))
        if (auto set = (SetHandler) dlsym (lib, "XSetErrorHandler"))
            set ([] (void*, void*) { return 0; });
   #endif
}

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

        // MODE has to reach all five stage modes from the panel, and pulse - which has no lamp of its
        // own on the front - has to light adv and stop together rather than leaving the row dark.
        {
            auto* m = proc.rack.m250();
            auto modeOf = [&proc] { auto* v = proc.apvts.getRawParameterValue ("sq_mode_1"); return v != nullptr ? (int) std::lround (v->load()) : -1; };
            if (auto* p = proc.apvts.getParameter ("sq_mode_1")) p->setValueNotifyingHost (0.0f);
            if (auto* p = proc.apvts.getParameter ("sq_edit")) p->setValueNotifyingHost (0.0f);   // edit stage 1
            juce::String walk;
            bool lamps = true;
            for (int i = 1; i <= 5; ++i)
            {
                press ("mode");
                juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
                const int md = modeOf();
                walk << md << (i < 5 ? " " : "");
                const int lit = (m->led (58) > 0.5f) + (m->led (59) > 0.5f) + (m->led (60) > 0.5f) + (m->led (61) > 0.5f);
                if (md == 4) { if (! (m->led (58) > 0.5f && m->led (61) > 0.5f && lit == 2)) lamps = false; }
                else if (lit != 1) lamps = false;
            }
            const bool wraps = walk == "1 2 3 4 0";
            std::printf ("250 mode: walks %s (want 1 2 3 4 0) %s, lamps %s\n", walk.toRawUTF8(),
                         wraps ? "ok" : "BAD", lamps ? "ok" : "BAD");
            if (! wraps || ! lamps) return 1;
        }
    }
    // ---- knobs, readout, menu, typing a value ----
    {
        WcProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        for (int i : { wc::kInst259, wc::kInst292, wc::kInst248, wc::kInst281, wc::kInst207, wc::kInst252 }) proc.setPresent (i, true);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (1400, 1300);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        auto pump = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
        quietXErrors();

        wc::ui::RackView* rv = nullptr;
        wc::ui::ValueReadout* ro = nullptr;
        juce::Viewport* vp = nullptr;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            if (auto* r = dynamic_cast<wc::ui::RackView*> (&c)) rv = r;
            if (auto* r = dynamic_cast<wc::ui::ValueReadout*> (&c)) ro = r;
            if (auto* v = dynamic_cast<juce::Viewport*> (&c)) if (vp == nullptr) vp = v;
            for (auto* ch : c.getChildren()) walk (*ch);
        };
        walk (*ed);
        if (rv == nullptr || ro == nullptr || vp == nullptr) { std::printf ("knobs: NO rack / readout / viewport\n"); return 1; }
        auto panelOf = [&] (int inst) -> wc::ui::ModulePanel*
        {
            wc::ui::ModulePanel* found = nullptr;
            std::function<void (juce::Component&)> f = [&] (juce::Component& c)
            {
                if (auto* p = dynamic_cast<wc::ui::ModulePanel*> (&c)) if (p->inst == inst) found = p;
                for (auto* ch : c.getChildren()) f (*ch);
            };
            f (*rv);
            return found;
        };
        auto knobOf = [&] (int inst, const juce::String& id) -> wc::ui::WcKnob*
        {
            if (auto* p = panelOf (inst))
                for (auto* ch : p->getChildren())
                    if (auto* k = dynamic_cast<wc::ui::WcKnob*> (ch)) if (k->paramId == id) return k;
            return nullptr;
        };
        // a control found by the parameter name its readout shows
        auto controlNamed = [&] (int inst, const juce::String& name) -> juce::Component*
        {
            if (auto* p = panelOf (inst))
                for (auto* ch : p->getChildren())
                    if (auto* src = dynamic_cast<wc::ui::ReadoutSource*> (ch))
                    {
                        wc::ui::ReadoutText t;
                        if (src->readoutText (t) && t.name == name) return ch;
                    }
            return nullptr;
        };
        auto mouse = [] (juce::Component& c, juce::Point<float> pos, juce::ModifierKeys mods, int clicks = 1)
        {
            const auto now = juce::Time::getCurrentTime();
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos, mods, juce::MouseInputSource::defaultPressure,
                                     0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, pos, now, clicks, false);
        };
        auto norm = [&proc] (const char* id) { return proc.apvts.getParameter (id)->getValue(); };
        auto setNorm = [&proc] (const char* id, float v) { proc.apvts.getParameter (id)->setValueNotifyingHost (v); };
        bool good = true;

        // H4: a right click opens the menu (or nothing) and never moves the control under it
        {
            const auto right = juce::ModifierKeys (juce::ModifierKeys::rightButtonModifier);
            juce::StringArray moved;
            auto rightClick = [&] (juce::Component* c, juce::Point<float> at, const juce::String& what, std::function<juce::String()> state)
            {
                if (c == nullptr) { moved.add (what + " (missing)"); return; }
                const auto before = state();
                c->mouseDown (mouse (*c, at, right));
                const auto held = state();          // a momentary button acts while it is held
                c->mouseUp (mouse (*c, at, right));
                pump (30);
                juce::PopupMenu::dismissAllActiveMenus();
                pump (30);
                if (held != before || state() != before) moved.add (what);
            };
            auto all = [&proc] { juce::String s; for (auto* p : proc.getParameters()) s << p->getValue() << ","; return s; };
            // a 292 lever, low half (would set "Gate"), and high half
            setNorm ("la_mode1", 0.5f);
            auto* lever = controlNamed (wc::kInst292, "292 A 1 Mode");
            rightClick (lever, { 12.0f, lever != nullptr ? (float) lever->getHeight() - 2.0f : 0.0f }, "292 lever", [&] { return juce::String (norm ("la_mode1")); });
            rightClick (lever, { 12.0f, 2.0f }, "292 lever (top)", [&] { return juce::String (norm ("la_mode1")); });
            auto* button = controlNamed (wc::kInst259, "259 A MO Range");
            rightClick (button, { 14.0f, 14.0f }, "259 range button", all);
            juce::Component* prog = nullptr, *push = nullptr;
            if (auto* p = panelOf (wc::kInst248)) for (auto* ch : p->getChildren()) if (dynamic_cast<wc::ui::ProgSwitch*> (ch) != nullptr && prog == nullptr) prog = ch;
            rightClick (prog, { 10.0f, 4.0f }, "248 programming lever", all);
            if (auto* p = panelOf (wc::kInst281)) for (auto* ch : p->getChildren()) if (dynamic_cast<wc::ui::PushButton*> (ch) != nullptr && push == nullptr) push = ch;
            rightClick (push, { 13.0f, 13.0f }, "281 push button", [&] { return juce::String ((int) proc.rack.m281 (0)->manual[0].load()); });
            auto* knob = knobOf (wc::kInst259, "a_timbre");
            setNorm ("a_timbre", 0.37f);
            rightClick (knob, { 20.0f, 20.0f }, "259 knob", [&] { return juce::String (norm ("a_timbre")); });
            if (knob != nullptr)   // and a right drag does not turn it either
            {
                knob->mouseDown (mouse (*knob, { 20.0f, 20.0f }, right));
                knob->mouseDrag (mouse (*knob, { 80.0f, -40.0f }, right));
                knob->mouseUp (mouse (*knob, { 80.0f, -40.0f }, right));
                pump (30); juce::PopupMenu::dismissAllActiveMenus(); pump (30);
                if (std::abs (norm ("a_timbre") - 0.37f) > 1.0e-6f) moved.add ("259 knob (right drag)");
            }
            std::printf ("right click: lever, panel button, programming lever, push button, knob %s\n",
                         moved.isEmpty() ? "unchanged  ok" : ("MOVED: " + moved.joinIntoString (", ")).toRawUTF8());
            if (! moved.isEmpty()) good = false;
            if (rv->menuOpen) { std::printf ("right click: the menu flag stayed on  BAD\n"); good = false; }
        }

        // H1: Alt+click without moving must not send the knob to its default (JUCE's Alt+click shortcut)
        auto* knob = knobOf (wc::kInst259, "a_timbre");
        if (knob == nullptr) { std::printf ("knobs: NO 259 timbre knob\n"); return 1; }
        {
            const auto alt = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier);
            setNorm ("a_timbre", 0.37f);
            const auto c = knob->getLocalBounds().toFloat().getCentre();
            knob->mouseDown (mouse (*knob, c, alt));
            knob->mouseUp (mouse (*knob, c, alt));
            const bool kept = std::abs (norm ("a_timbre") - 0.37f) < 1.0e-6f;
            std::printf ("alt+click without moving: timbre %.3f (was 0.370) %s\n", norm ("a_timbre"), kept ? "ok" : "BAD");
            if (! kept) good = false;
        }

        // 125 screen pixels with Alt are half a turn at any zoom: 0.5, in a 10 % step
        {
            juce::String got;
            bool same = true;
            for (float z : { 0.25f, 1.0f, 2.0f })
            {
                rv->setZoom (z);
                pump (10);
                setNorm ("a_timbre", 0.0f);
                const auto alt = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier);
                const auto c = knob->getLocalBounds().toFloat().getCentre();
                knob->mouseDown (mouse (*knob, c, alt));
                for (int s = 1; s <= 5; ++s) knob->mouseDrag (mouse (*knob, c + juce::Point<float> (125.0f / z * (float) s / 5.0f, 0.0f), alt));
                knob->mouseUp (mouse (*knob, c + juce::Point<float> (125.0f / z, 0.0f), alt));
                got << juce::String (norm ("a_timbre"), 3) << " ";
                if (std::abs (norm ("a_timbre") - 0.5f) > 1.0e-4f) same = false;
            }
            // and without Alt, the same 125 px is still half a turn; with Shift a tenth of that
            rv->setZoom (1.0f);
            pump (10);
            const auto c = knob->getLocalBounds().toFloat().getCentre();
            auto dragBy = [&] (juce::ModifierKeys m, float px)
            {
                setNorm ("a_timbre", 0.0f);
                knob->mouseDown (mouse (*knob, c, m));
                knob->mouseDrag (mouse (*knob, c + juce::Point<float> (px / 2.0f, 0.0f), m));
                knob->mouseDrag (mouse (*knob, c + juce::Point<float> (px / 2.0f, -px / 2.0f), m));   // up counts as right
                knob->mouseUp (mouse (*knob, c + juce::Point<float> (px / 2.0f, -px / 2.0f), m));
                return norm ("a_timbre");
            };
            const float plain = dragBy (juce::ModifierKeys::leftButtonModifier, 125.0f);
            const float fine = dragBy (juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::shiftModifier), 125.0f);
            const bool drags = std::abs (plain - 0.5f) < 1.0e-3f && std::abs (fine - 0.05f) < 1.0e-3f;
            std::printf ("125 px with Alt at zoom 0.25 / 1 / 2: %s%s; plain %.3f, shift %.3f %s\n", got.toRawUTF8(), same ? "ok" : "BAD",
                         plain, fine, drags ? "ok" : "BAD");
            if (! same || ! drags) good = false;
        }

        // a fader: a click in the slot away from the cap does not jump
        {
            auto* f = knobOf (wc::kInst207, "mx_lvl_a");
            setNorm ("mx_lvl_a", 0.8f);
            bool kept = f != nullptr;
            if (f != nullptr)
            {
                const juce::Point<float> slot ((float) f->getWidth() * 0.5f, (float) f->getHeight() - 4.0f);
                f->mouseDown (mouse (*f, slot, juce::ModifierKeys::leftButtonModifier));
                f->mouseUp (mouse (*f, slot, juce::ModifierKeys::leftButtonModifier));
                kept = std::abs (norm ("mx_lvl_a") - 0.8f) < 1.0e-6f;
            }
            std::printf ("fader: click in the slot keeps %.3f (was 0.800) %s\n", norm ("mx_lvl_a"), kept ? "ok" : "BAD");
            if (! kept) good = false;
        }

        // the readout: a hover shows after 400 ms, not before, outside the zoom
        const juce::File shots = out.getChildFile ("captures");
        shots.createDirectory();
        auto save = [&] (const juce::Image& img, const juce::String& name)
        {
            auto file = shots.getChildFile (name);
            file.deleteFile();
            juce::FileOutputStream os (file);
            juce::PNGImageFormat().writeImageToStream (img, os);
        };
        auto bring = [&] (juce::Component& c)   // scroll the viewport so the control is in sight
        {
            const auto r = rv->getLocalArea (&c, c.getLocalBounds());
            vp->setViewPosition (juce::jmax (0, r.getCentreX() - 300), juce::jmax (0, r.getCentreY() - 220));
            pump (20);
        };
        auto crop = [&] (juce::Component& c, juce::Rectangle<int> extra)
        {
            auto area = ed->getLocalArea (&c, c.getLocalBounds()).getUnion (ro->isVisible() ? ro->getBounds() : juce::Rectangle<int>());
            area = area.getUnion (extra).expanded (40).getIntersection (ed->getLocalBounds());
            return ed->createComponentSnapshot (area, true, 2.0f);
        };
        auto* freq = knobOf (wc::kInst259, "a_po_freq");
        if (freq == nullptr) { std::printf ("knobs: NO 259 frequency knob\n"); return 1; }
        juce::String sizes;
        for (float z : { 0.5f, 1.5f })
        {
            rv->setZoom (z);
            pump (20);
            bring (*freq);
            if (auto* t = ro->targetComponent()) ro->hoverExit (*t);   // the mouse leaves whatever it was on
            pump (900);   // cold: nothing up for a while
            freq->mouseEnter (mouse (*freq, freq->getLocalBounds().toFloat().getCentre(), {}));
            pump (200);
            const bool early = ro->isUp();
            pump (500);
            const bool shown = ro->isUp() && ro->targetComponent() == freq && ro->current().value.endsWith ("Hz");
            const auto b = ro->getBounds();
            const auto knobArea = ed->getLocalArea (freq, freq->readoutAnchor().toNearestInt());
            const bool clear = ! b.intersects (knobArea);
            sizes << "zoom " << juce::String (z, 2) << ": " << b.getWidth() << "x" << b.getHeight() << " \"" << ro->current().value << "\"  ";
            std::printf ("readout at zoom %.2f: %s at 200 ms, %s at 700 ms, %dx%d, %s the knob %s\n", z, early ? "UP" : "hidden",
                         shown ? "up" : "NOT up", b.getWidth(), b.getHeight(), clear ? "clear of" : "OVER",
                         (! early && shown && clear && b.getHeight() == 44) ? "ok" : "BAD");
            if (early || ! shown || ! clear || b.getHeight() != 44) good = false;
            save (crop (*freq, {}), "readout_zoom" + juce::String (juce::roundToInt (z * 100)) + ".png");
            freq->mouseExit (mouse (*freq, { -5.0f, -5.0f }, {}));
            if (ro->isUp()) { std::printf ("readout: still up after leaving the knob  BAD\n"); good = false; }
        }
        rv->setZoom (1.0f);
        pump (20);
        bring (*freq);

        // the menu: what it offers, and a capture of it open
        {
            auto m = wc::ui::RackView::buildParamMenu (*proc.apvts.getParameter ("a_po_freq"), true);
            juce::StringArray items;
            for (juce::PopupMenu::MenuItemIterator it (m); it.next();)
            {
                auto& i = it.getItem();
                if (i.isSectionHeader) items.add ("[" + i.text + "]");
                else if (i.itemID != 0) items.add (juce::String (i.itemID) + ":" + i.text + (i.isEnabled ? "" : "(off)"));
            }
            const auto list = items.joinIntoString (" ");
            const bool shape = list == juce::String (juce::CharPointer_UTF8 ("[259 A Principal Frequency] 1:Type a value\xe2\x80\xa6 2:Reset"))
                                       + " to default(off) [Knob position] 10:0 % 11:25 % 12:50 % 13:75 % 14:100 %"
                                       " 200:MIDI Learn 202:Forget MIDI CC(off)";
            std::printf ("menu: %s %s\n", list.toRawUTF8(), shape ? "ok" : "BAD");
            if (! shape) good = false;
            auto sw = wc::ui::RackView::buildParamMenu (*proc.apvts.getParameter ("la_mode1"), false);
            juce::StringArray swItems;
            for (juce::PopupMenu::MenuItemIterator it (sw); it.next();) if (it.getItem().itemID > 0 && it.getItem().itemID < 100) swItems.add (it.getItem().text);
            const bool values = swItems.joinIntoString ("|") == "Reset to default|LoPass|Combo|Gate";
            std::printf ("menu of a lever: %s %s\n", swItems.joinIntoString (" | ").toRawUTF8(), values ? "ok" : "BAD");
            if (! values) good = false;

            setNorm ("a_po_freq", 0.43f);
            freq->mouseDown (mouse (*freq, freq->getLocalBounds().toFloat().getCentre(), juce::ModifierKeys::rightButtonModifier));
            freq->mouseUp (mouse (*freq, freq->getLocalBounds().toFloat().getCentre(), juce::ModifierKeys::rightButtonModifier));
            // no pumping: without a window manager this process is never the foreground one, so the menu closes on its first timer
            juce::Component* menu = nullptr;
            for (int i = 0; i < juce::Desktop::getInstance().getNumComponents(); ++i)
                if (auto* c = juce::Desktop::getInstance().getComponent (i)) if (c->isVisible() && c != ed.get()) menu = c;
            const bool suppressed = rv->menuOpen && ! ro->isUp();
            if (menu != nullptr)
            {
                // the menu is a window of its own: drawn here next to the knob, as it opens under the mouse
                auto menuImg = menu->createComponentSnapshot (menu->getLocalBounds(), true, 2.0f);
                const auto knobArea = ed->getLocalArea (freq, freq->getLocalBounds());
                const juce::Rectangle<int> at (knobArea.getCentreX(), knobArea.getCentreY(), menu->getWidth(), menu->getHeight());
                auto base = crop (*freq, at);
                const auto area = ed->getLocalArea (freq, freq->getLocalBounds()).getUnion (at).expanded (40).getIntersection (ed->getLocalBounds());
                juce::Graphics g (base);
                g.drawImageAt (menuImg, (at.getX() - area.getX()) * 2, (at.getY() - area.getY()) * 2);
                save (base, "menu_open.png");
            }
            std::printf ("menu open: %s, readout %s %s\n", menu != nullptr ? "shown" : "NOT shown", suppressed ? "held back" : "NOT held back",
                         menu != nullptr && suppressed ? "ok" : "BAD");
            if (menu == nullptr || ! suppressed) good = false;
            juce::PopupMenu::dismissAllActiveMenus();
            pump (60);
        }

        // typing a value: a bad text keeps the field open with the error, a good one applies, too much is limited
        {
            setNorm ("a_po_freq", 0.43f);
            const float before = norm ("a_po_freq");
            rv->typeValue (*freq, "a_po_freq");
            const bool opened = ro->isEditing() && ro->field.getText() == proc.apvts.getParameter ("a_po_freq")->getCurrentValueAsText();
            ro->field.setText ("250 ms");
            ro->commitEdit (true);
            pump (30);   // the field's own change notice comes late: the error has to survive it
            const bool refused = ro->isEditing() && ro->showsError() && ro->current().note.startsWith (juce::CharPointer_UTF8 ("Not a value for this control \xe2\x80\x93 try 440 Hz"))
                                 && std::abs (norm ("a_po_freq") - before) < 1.0e-6f && ro->getHeight() == 66;
            save (crop (*freq, {}), "type_value_error.png");
            ro->field.setText ("1,2k");
            ro->commitEdit (true);
            const float hz = wc::coarseHz (proc.apvts.getParameter ("a_po_freq")->convertFrom0to1 (norm ("a_po_freq")), false);
            const bool applied = ! ro->isEditing() && std::abs (hz - 1200.0f) < 0.5f;
            rv->typeValue (*freq, "a_po_freq");
            ro->field.setText ("20 kHz");
            ro->commitEdit (true);
            const bool limited = ! ro->isEditing() && ro->isUp() && ro->current().warn && ro->current().note == "Limited to 7.04 kHz (max)"
                                 && std::abs (norm ("a_po_freq") - 1.0f) < 1.0e-6f;
            pump (20);
            save (crop (*freq, {}), "type_value_limited.png");
            rv->typeValue (*freq, "a_po_freq");
            ro->field.setText ("440");
            ro->cancelEdit();                      // Esc
            const bool cancelled = ! ro->isEditing() && std::abs (norm ("a_po_freq") - 1.0f) < 1.0e-6f;
            // Ctrl / Cmd + click without moving opens it as well
            const auto cmd = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::commandModifier);
            const auto c = freq->getLocalBounds().toFloat().getCentre();
            freq->mouseDown (mouse (*freq, c, cmd));
            freq->mouseDrag (mouse (*freq, c + juce::Point<float> (2.0f, 0.0f), cmd));
            freq->mouseUp (mouse (*freq, c + juce::Point<float> (2.0f, 0.0f), cmd));
            const bool cmdClick = ro->isEditing() && std::abs (norm ("a_po_freq") - 1.0f) < 1.0e-6f;
            ro->cancelEdit();
            std::printf ("type a value: opens %s, \"250 ms\" on a frequency %s, \"1,2k\" -> %.1f Hz %s, \"20 kHz\" %s, Esc %s, Ctrl+click %s %s\n",
                         opened ? "ok" : "BAD", refused ? "refused" : "NOT refused", hz, applied ? "ok" : "BAD", limited ? "limited" : "NOT limited",
                         cancelled ? "cancels" : "BAD", cmdClick ? "opens" : "BAD",
                         (opened && refused && applied && limited && cancelled && cmdClick) ? "ok" : "BAD");
            if (! (opened && refused && applied && limited && cancelled && cmdClick)) good = false;
        }
        // the 252 cell knobs are no host parameters: their own texts, and C.V. takes a typed value too
        {
            wc::ui::WcKnob* cv = nullptr;
            if (auto* p = panelOf (wc::kInst252))
                for (auto* ch : p->getChildren())
                    if (auto* k = dynamic_cast<wc::ui::WcKnob*> (ch))
                    {
                        wc::ui::ReadoutText t;
                        if (k->paramId.isEmpty() && k->readoutText (t) && t.name == "Cell: C.V. value") cv = k;
                    }
            wc::ui::ReadoutText before, after;
            bool ok252 = cv != nullptr && cv->readoutText (before) && before.value == "-- (no change)" && cv->onType != nullptr;
            if (ok252)
            {
                cv->onType();
                ro->field.setText ("6,4 V");
                ro->commitEdit (true);
                ok252 = ! ro->isEditing() && cv->readoutText (after) && after.value == "6.40 V";
            }
            std::printf ("252 cell C.V.: \"%s\", typed 6,4 V -> \"%s\" %s\n", before.value.toRawUTF8(), after.value.toRawUTF8(), ok252 ? "ok" : "BAD");
            if (! ok252) good = false;
        }
        // ---- MIDI Learn: waiting, assigned, the dot, activity, CC 1, a CC moved from another control, cancelling ----
        {
            auto send = [&proc] (std::initializer_list<juce::MidiMessage> msgs)
            {
                juce::AudioBuffer<float> buf (2, 64);
                juce::MidiBuffer midi;
                int at = 0;
                for (auto& m : msgs) midi.addEvent (m, at++);
                proc.processBlock (buf, midi);
            };
            auto cc = [] (int ch, int n, int v) { return juce::MidiMessage::controllerEvent (ch, n, v); };
            auto items = [] (const juce::PopupMenu& m)
            {
                juce::StringArray l;
                for (juce::PopupMenu::MenuItemIterator it (m); it.next();)
                {
                    auto& i = it.getItem();
                    if (i.itemID >= 200) l.add (juce::String (i.itemID) + ":" + i.text + (i.shortcutKeyDescription.isNotEmpty() ? "[" + i.shortcutKeyDescription + "]" : "") + (i.isEnabled ? "" : "(off)"));
                }
                return l.joinIntoString (" ");
            };
            // the dot of a learned knob: on the skirt at 45 degrees up to the right, in the MIDI colour (on the panel itself)
            auto dotAt = [&] (wc::ui::WcKnob& k)
            {
                auto* pn = k.getParentComponent();
                const auto a = pn->getLocalArea (&k, k.readoutAnchor());
                const float d = a.getWidth() * 0.5f * 0.86f;
                const auto at = a.getCentre() + juce::Point<float> (d * 0.70710678f, -d * 0.70710678f);
                auto img = pn->createComponentSnapshot (pn->getLocalBounds(), true, 1.0f);
                return img.getPixelAt (juce::roundToInt (at.x), juce::roundToInt (at.y));
            };
            auto isAccent = [] (juce::Colour c) { const auto a = wc::ui::col::midi; return std::abs (c.getRed() - a.getRed()) < 40 && std::abs (c.getGreen() - a.getGreen()) < 40 && std::abs (c.getBlue() - a.getBlue()) < 40; };
            bool learnOk = true;
            juce::StringArray bad;
            auto check = [&] (bool b, const juce::String& what) { if (! b) { bad.add (what); learnOk = false; } };

            const auto* tp = proc.apvts.getParameter ("a_timbre");
            check (items (wc::ui::RackView::buildParamMenu (*tp, true)) == "200:MIDI Learn 202:Forget MIDI CC(off)", "menu without CC: " + items (wc::ui::RackView::buildParamMenu (*tp, true)));
            check (items (wc::ui::RackView::buildParamMenu (*tp, true, 74)) == "200:Learn another CC[CC 74] 202:Forget CC 74", "menu with CC 74: " + items (wc::ui::RackView::buildParamMenu (*tp, true, 74)));
            check (items (wc::ui::RackView::buildParamMenu (*tp, true, -1, true)) == "201:Cancel MIDI Learn 202:Forget MIDI CC(off)", "menu waiting: " + items (wc::ui::RackView::buildParamMenu (*tp, true, -1, true)));
            check (items (wc::ui::RackView::buildParamMenu (*proc.apvts.getParameter ("la_mode1"), false, 20)) == "200:Learn another CC[CC 20] 202:Forget CC 20", "lever menu");

            auto* timbre = knobOf (wc::kInst259, "a_timbre");
            if (timbre == nullptr) { std::printf ("midi learn: NO timbre knob\n"); return 1; }
            bring (*timbre);
            const auto restDot = dotAt (*timbre);
            // waiting: the readout holds its text; CC 0 and 120 are not taken, CC 74 (any channel) is
            rv->startLearn ("a_timbre");
            pump (40);
            check (rv->learning() && ro->isHolding() && ro->current().value == juce::String (juce::CharPointer_UTF8 ("Move a MIDI control\xe2\x80\xa6")) && ro->current().note == "Right-click or Esc to cancel", "waiting readout");
            if (auto* t = ro->targetComponent()) if (t != timbre) check (false, "waiting readout on another control");
            timbre->mouseExit (mouse (*timbre, { -5.0f, -5.0f }, {}));   // the mouse going away does not take it down
            freq->mouseEnter (mouse (*freq, freq->getLocalBounds().toFloat().getCentre(), {}));
            check (ro->isHolding() && ro->targetComponent() == timbre, "hover elsewhere took the waiting readout");   // not even for a frame
            pump (500);
            check (ro->isHolding() && ro->targetComponent() == timbre, "hover elsewhere took the waiting readout later");
            freq->mouseExit (mouse (*freq, { -5.0f, -5.0f }, {}));
            save (crop (*timbre, {}), "midi_waiting.png");
            send ({ cc (1, 0, 10), cc (1, 120, 0) });
            pump (100);
            check (rv->learning() && proc.midiMapEmpty(), "CC 0 / 120 were taken by a learn");
            send ({ cc (5, 74, 30) });
            pump (100);
            check (! rv->learning() && proc.ccOf ("a_timbre") == 74, "CC 74 not learned");
            float ringAlpha = 0.0f;
            check (rv->learnRing ("a_timbre", ringAlpha) && ringAlpha == 1.0f, "no steady ring after the learn");
            check (ro->isUp() && ro->current().note == "CC 74" && ! ro->current().warn, "readout after the learn: \"" + ro->current().note + "\"");
            save (crop (*timbre, {}), "midi_just_assigned.png");
            pump (wc::ui::RackView::kAssignedRingMs + 100);
            check (! rv->learnRing ("a_timbre", ringAlpha), "the ring stayed");
            pump (wc::ui::RackView::kLearnedMs + 400);
            check (! ro->isUp(), "the readout stayed up");
            const auto shownDot = dotAt (*timbre);
            check (isAccent (shownDot) && ! isAccent (restDot), "no dot (" + shownDot.toDisplayString (false) + ")");
            save (crop (*timbre, {}), "midi_assigned.png");
            // hover adds "CC 74"; each CC moves it and makes the dot bigger for a moment
            timbre->mouseEnter (mouse (*timbre, timbre->getLocalBounds().toFloat().getCentre(), {}));
            pump (500);
            check (ro->isUp() && ro->current().note == "CC 74" && ro->getHeight() == 61, "hover without \"CC 74\"");
            // the box keeps 8 px off the dot and the ring (6 px around the skirt), not off the skirt
            check (! ro->getBounds().toFloat().intersects (ed->getLocalArea (timbre, timbre->readoutAnchor()).expanded (6.0f + 7.5f)), "readout box too close to the MIDI marks");
            save (crop (*timbre, {}), "midi_hover.png");
            timbre->mouseExit (mouse (*timbre, { -5.0f, -5.0f }, {}));
            send ({ cc (2, 74, 127) });
            pump (40);
            check (rv->ccActive (74) && std::abs (norm ("a_timbre") - 1.0f) < 1.0e-6f, "CC 74 did not move it / no activity");
            pump (wc::ui::RackView::kActivityMs + 100);
            check (! rv->ccActive (74), "activity stayed");

            // CC 1 learned: amber warning, and the MOD jack of the KB-225 says so
            if (auto* off = controlNamed (wc::kInst292, "292 A 1 Offset"))
            {
                bring (*off);
                rv->startLearn ("la_off1");
                send ({ cc (1, 1, 64) });
                pump (100);
                check (proc.ccOf ("la_off1") == 1 && ro->isUp() && ro->current().warn && ro->current().note == "CC 1 (mod wheel) no longer reaches the MIDI module",
                       "CC 1 warning: \"" + ro->current().note + "\"");
                save (crop (*off, {}), "midi_cc1_warning.png");
                // one width cap, 288 px: a longer note takes a second line instead of being cut
                ro->flash (*off, "CC 1 (mod wheel) no longer reaches the MIDI module, nor the KB-225 MOD output", true, 300);
                check (ro->getWidth() == 288 && ro->getHeight() == 61 + 13, "long note: " + ro->getBounds().toString());
                save (crop (*off, {}), "readout_two_line_note.png");
                juce::String tip;
                if (auto* pn = panelOf (wc::kInstMidi))
                    for (auto& j : pn->jacks) if (j->output && j->index == wc::MidiInterface::Out_Mod) tip = j->getTooltip();
                check (tip.contains ("CC 1 is assigned to 292 A 1 Offset: this output holds its last value"), "MOD jack tooltip: " + tip.replace ("\n", " / "));
                // "Learn another CC" on 259 Timbre keeps CC 74 while it waits; Esc cancels and it is as it was
                bring (*timbre);
                rv->startLearn ("a_timbre");
                check (items (wc::ui::RackView::buildParamMenu (*tp, true, proc.ccOf ("a_timbre"), rv->learningId() == "a_timbre")) == "201:Cancel MIDI Learn 202:Forget CC 74", "re-learn menu");
                ed->keyPressed (juce::KeyPress (juce::KeyPress::escapeKey));
                check (! rv->learning() && proc.ccOf ("a_timbre") == 74 && ! ro->isHolding(), "Esc did not cancel, or lost CC 74");
                // learning another control cancels the first
                rv->startLearn ("a_timbre");
                rv->startLearn ("a_po_freq");
                check (rv->learningId() == "a_po_freq", "a second learn did not take over");
                rv->cancelLearn();
                // CC 1 was 292's: learned on Timbre it moves, with an amber note naming where from
                rv->startLearn ("a_timbre");
                send ({ cc (1, 1, 0) });
                pump (100);
                check (proc.ccOf ("a_timbre") == 1 && proc.ccOf ("la_off1") < 0 && proc.paramOfCc (74).isEmpty() && ro->current().warn
                       && ro->current().note == "CC 1 moved from 292 A 1 Offset", "moved: \"" + ro->current().note + "\"");
                save (crop (*timbre, {}), "midi_moved.png");
                // a lever: its dot sits on the top right corner of the lever itself, not of its labels (it fell next to B)
                if (auto* lever = controlNamed (wc::kInst292, "292 A 1 Mode"))
                {
                    proc.assignCc (21, "la_mode1");
                    rv->repaintMarks ("la_mode1");
                    bring (*lever);
                    auto* pn = lever->getParentComponent();
                    // the lever is the 24 px on the left of the control, 18 px either side of its middle (3 positions)
                    const juce::Point<int> corner (lever->getX() + 24, lever->getBounds().getCentreY() - 18);
                    const auto px = pn->createComponentSnapshot (pn->getLocalBounds(), true, 1.0f).getPixelAt (corner.x, corner.y);
                    check (isAccent (px), "no dot on the lever (" + px.toDisplayString (false) + ")");
                    save (crop (*lever, {}), "midi_lever_assigned.png");
                }
            }
            else check (false, "NO 292 offset knob");
            // what the menu's MIDI items do. Master BPM has 281 values: 201 is also a value of its list, and
            // "Cancel MIDI Learn" must not set it
            {
                const auto& br = proc.apvts.getParameter ("rg_bpm")->getNormalisableRange();
                const float bpm = norm ("rg_bpm");
                rv->startLearn ("rg_bpm");
                rv->menuChoice ("rg_bpm", 201, true, nullptr);
                check ((br.end - br.start) / br.interval > 181.0f && ! rv->learning() && norm ("rg_bpm") == bpm, "Cancel MIDI Learn set Master BPM to " + juce::String (norm ("rg_bpm")));
                rv->menuChoice ("a_po_freq", 200, false, nullptr);
                check (rv->learningId() == "a_po_freq", "MIDI Learn from the menu");
                rv->menuChoice ("a_po_freq", 0, true, nullptr);   // its menu closed without a choice
                check (! rv->learning(), "closing the waiting control's menu did not cancel");
                proc.assignCc (40, "a_po_freq");
                rv->menuChoice ("a_po_freq", 202, false, nullptr);
                check (proc.ccOf ("a_po_freq") < 0, "Forget CC 40");
            }
            pump (wc::ui::RackView::kMovedMs + 400);
            // captures for the visual review: knob sizes, a fader, a waiting lever and its menu, zoom, the Neon Void skin
            {
                auto learnt = [&] (juce::Component& c, const juce::String& id, int n, const juce::String& file)
                {
                    proc.assignCc (n, id);
                    rv->repaintMarks (id);
                    bring (c);
                    save (crop (c, {}), file);
                };
                learnt (*freq, "a_po_freq", 41, "midi_knob_big.png");
                wc::ui::WcKnob* small = nullptr;   // a 28 px skirt (kr 14)
                if (auto* pn = panelOf (wc::kInst259))
                    for (auto* ch : pn->getChildren())
                        if (auto* k = dynamic_cast<wc::ui::WcKnob*> (ch))
                            if (small == nullptr && k->param != nullptr && k->isRotary() && k->readoutAnchor().getWidth() == 28.0f) small = k;
                check (small != nullptr, "no small knob on the 259");
                if (small != nullptr) learnt (*small, small->paramId, 42, "midi_knob_small.png");
                if (auto* f = knobOf (wc::kInst207, "mx_lvl_a")) learnt (*f, "mx_lvl_a", 43, "midi_fader.png");
                if (auto* lever = controlNamed (wc::kInst292, "292 A 1 Mode"))
                {
                    rv->startLearn ("la_mode1");
                    bring (*lever);
                    pump (300);
                    save (crop (*lever, {}), "midi_lever_waiting.png");
                    rv->cancelLearn();
                    setNorm ("la_mode1", 0.5f);
                    const auto at = lever->getLocalBounds().toFloat().getCentre();
                    lever->mouseDown (mouse (*lever, at, juce::ModifierKeys::rightButtonModifier));
                    juce::Component* menu = nullptr;
                    for (int i = 0; i < juce::Desktop::getInstance().getNumComponents(); ++i)
                        if (auto* c = juce::Desktop::getInstance().getComponent (i)) if (c->isVisible() && c != ed.get()) menu = c;
                    check (menu != nullptr, "no lever menu");
                    if (menu != nullptr)
                    {
                        auto menuImg = menu->createComponentSnapshot (menu->getLocalBounds(), true, 2.0f);
                        const auto la = ed->getLocalArea (lever, lever->getLocalBounds());
                        const juce::Rectangle<int> box (la.getCentreX(), la.getCentreY(), menu->getWidth(), menu->getHeight());
                        auto base = crop (*lever, box);
                        const auto area = la.getUnion (box).expanded (40).getIntersection (ed->getLocalBounds());
                        juce::Graphics g (base);
                        g.drawImageAt (menuImg, (box.getX() - area.getX()) * 2, (box.getY() - area.getY()) * 2);
                        save (base, "menu_lever_ticked.png");
                    }
                    juce::PopupMenu::dismissAllActiveMenus();
                    pump (60);
                }
                rv->startLearn ("a_timbre");   // "Learn another CC": the ring and the dot of CC 1 together
                for (float z : { 0.5f, 1.5f })
                {
                    rv->setZoom (z);
                    pump (20);
                    bring (*timbre);
                    pump (40);
                    save (crop (*timbre, {}), "midi_zoom" + juce::String (juce::roundToInt (z * 100)) + ".png");
                }
                rv->cancelLearn();
                rv->setZoom (1.0f);
                pump (20);
                wc::ui::skin::current = 1;     // Neon Void
                bring (*timbre);
                timbre->mouseEnter (mouse (*timbre, timbre->getLocalBounds().toFloat().getCentre(), {}));
                pump (500);
                save (crop (*timbre, {}), "midi_neon_void.png");
                timbre->mouseExit (mouse (*timbre, { -5.0f, -5.0f }, {}));
                wc::ui::skin::current = 0;
            }
            std::printf ("midi learn: menus, waiting, CC 0 / 120 refused, CC 74 learned, ring, dot, hover, activity, CC 1 note and MOD tooltip, re-learn, Esc, moved  %s\n",
                         learnOk ? "ok" : ("BAD: " + bad.joinIntoString ("; ")).toRawUTF8());
            if (! learnOk) good = false;
        }
        std::printf ("%s\n", sizes.toRawUTF8());
        if (! good) return 1;
    }

    // ---- rows: the strip, moving a module, auto-scroll, old patches where they always were ----
    {
        auto pump = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
        const auto left = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier);
        auto at = [] (juce::Component& c, juce::Point<float> screen, juce::ModifierKeys mods)   // an event at a screen point
        {
            const auto pos = c.getLocalPoint (nullptr, screen);
            const auto now = juce::Time::getCurrentTime();
            return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos, mods, juce::MouseInputSource::defaultPressure,
                                     0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, pos, now, 1, false);
        };
        struct Rig
        {
            WcProcessor proc;
            std::unique_ptr<juce::AudioProcessorEditor> ed;
            wc::ui::RackView* rv = nullptr;
            juce::Viewport* vp = nullptr;
            wc::ui::ValueReadout* ro = nullptr;
            Rig (int w, int h, const std::function<void (WcProcessor&)>& setup)
            {
                proc.license.status = wc::License::Status::Licensed;
                proc.prepareToPlay (48000.0, 512);
                setup (proc);
                ed.reset (proc.createEditor());
                ed->setSize (w, h);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
                std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
                {
                    if (auto* r = dynamic_cast<wc::ui::RackView*> (&c)) rv = r;
                    if (auto* r = dynamic_cast<wc::ui::ValueReadout*> (&c)) ro = r;
                    if (auto* v = dynamic_cast<juce::Viewport*> (&c)) if (vp == nullptr) vp = v;
                    for (auto* ch : c.getChildren()) walk (*ch);
                };
                walk (*ed);
            }
            std::vector<wc::ui::ModulePanel*> panels() const
            {
                std::vector<wc::ui::ModulePanel*> v;
                for (auto* c : rv->content.getChildren()) if (auto* p = dynamic_cast<wc::ui::ModulePanel*> (c)) v.push_back (p);
                return v;
            }
            wc::ui::ModulePanel* panel (int inst) const { for (auto* p : panels()) if (p->inst == inst) return p; return nullptr; }
            juce::Point<float> screen (juce::Point<float> contentPt) const { return rv->content.localPointToGlobal (contentPt); }
            juce::String rowsText() const
            {
                juce::String t;
                for (auto& r : proc.layout()) { t << "["; for (int i : r) t << wc::instKey (i) << " "; t = t.trimEnd() + "] "; }
                return t.trimEnd();
            }
        };
        // grabs a module by its strip and drags it through content points (the last one is where it is let go)
        auto drag = [&] (Rig& r, int inst, std::vector<juce::Point<float>> pts, bool release = true)
        {
            auto* g = r.panel (inst)->grip.get();
            const auto start = g->localPointToGlobal (juce::Point<float> (30.0f, 11.0f));
            g->mouseDown (at (*g, start, left));
            for (auto pt : pts) g->mouseDrag (at (*g, r.screen (pt), left));
            if (release) g->mouseUp (at (*g, r.screen (pts.back()), left));
            pump (5);
        };
        const juce::File shots = out.getChildFile ("captures");
        shots.createDirectory();
        auto shot = [&] (Rig& r, const juce::String& name, juce::Rectangle<int> area = {})
        {
            const auto img = r.ed->createComponentSnapshot (area.isEmpty() ? r.ed->getLocalBounds() : area, true, 1.0f);
            auto f = shots.getChildFile (name);
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        };
        juce::StringArray bad;
        auto check = [&] (bool ok, const juce::String& what) { if (! ok) bad.add (what); };

        // 1. the strip takes no control: it lies under every child of its panel (the "x" on top), and no child of any
        //    panel reaches into it, but on the 296, whose DECAY lever and top jacks start at 15 and 21 units
        {
            Rig r (1400, 900, [] (WcProcessor& p) { for (int i = 1; i < wc::kInstOutput; ++i) p.setPresent (i, true); });
            juce::StringArray hit;
            int over = 0;
            for (auto* pn : r.panels())
            {
                const auto strip = pn->grip->getBounds();
                check (strip == juce::Rectangle<int> (0, 0, pn->getWidth(), 22), "strip of " + wc::instName (pn->inst));
                if (pn->getIndexOfChildComponent (pn->grip.get()) != 0) { ++over; check (false, "the strip of " + wc::instName (pn->inst) + " over a control"); }
                if (wc::typeOfInst (pn->inst) == wc::MT_296) continue;
                for (auto* c : pn->getChildren())
                    if (c != pn->grip.get() && dynamic_cast<juce::TextButton*> (c) == nullptr && c->isVisible() && c->getBounds().intersects (strip))
                        hit.addIfNotAlreadyThere (wc::instName (pn->inst) + " " + c->getBounds().toString());
            }
            check (hit.isEmpty(), "controls in the strip: " + hit.joinIntoString (", "));
            std::printf ("rows, strip: %d panels, under every control %s, free of them but on the 296 %s\n", (int) r.panels().size(),
                         over == 0 ? "ok" : "BAD", hit.isEmpty() ? "ok" : "BAD");
        }

        // 2. patches without LAYOUT (factory library, built-ins, a fresh rack) open where they always did, to the pixel
        {
            Rig r (1400, 900, [] (WcProcessor&) {});
            int checked = 0, wrong = 0, oneRow = 0;
            juce::String firstWrong;
            auto same = [&] (const juce::String& what)
            {
                pump (2);
                int x = 6, y = 6, rows = 1;   // the layout of before, as RackView::rebuild did it
                for (auto* pn : r.panels())
                {
                    const int w = pn->getWidth();
                    if (x + w > 1336 && x > 6) { x = 6; y += 448; ++rows; }
                    if (pn->getPosition() != juce::Point<int> (x, y)) { if (wrong++ == 0) firstWrong = what + " " + wc::instName (pn->inst); }
                    x += w + 6;
                }
                if ((int) r.proc.layout().size() != juce::jmax (2, rows)) { if (wrong++ == 0) firstWrong = what + " rows"; }
                if (rows == 1) ++oneRow;
                ++checked;
            };
            for (int i = 0; i < r.proc.factory.getNumChildren(); ++i) { r.proc.loadFactoryAt (i); same ("factory " + juce::String (i)); }
            for (int i = 0; i < WcProcessor::builtinPatchNames().size(); ++i) { r.proc.loadBuiltinPatch (i); same ("built-in " + juce::String (i)); }
            r.proc.resetAll(); same ("reset");
            check (checked == 324 && wrong == 0, "old layout: " + juce::String (wrong) + " wrong, first " + firstWrong);
            std::printf ("rows, old patches: %d laid out as before (%d of one row, with an empty second), %d wrong %s\n", checked, oneRow, wrong, wrong == 0 && checked == 324 ? "ok" : "BAD");
            r.proc.loadBuiltinPatch (0);
            pump (20);
            shot (r, "rows_old_patch_one_row.png");
        }

        // 3. moving a module keeps the cables and the sound (bit for bit) and makes no panel again
        {
            WcProcessor ref;
            ref.prepareToPlay (48000.0, 512);
            ref.loadBuiltinPatch (1);
            Rig r (1400, 1300, [] (WcProcessor& p) { p.loadBuiltinPatch (1); });
            auto render = [] (WcProcessor& p, int blocks, bool note, std::vector<float>& outv)
            {
                for (int b = 0; b < blocks; ++b)
                {
                    juce::AudioBuffer<float> buf (2, 512);
                    juce::MidiBuffer midi;
                    if (note && b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
                    p.processBlock (buf, midi);
                    for (int c = 0; c < 2; ++c) outv.insert (outv.end(), buf.getReadPointer (c), buf.getReadPointer (c) + 512);
                }
            };
            std::vector<float> a, b;
            render (ref, 30, true, a); render (r.proc, 30, true, b);
            const auto cablesBefore = r.proc.getCables();
            const auto panelsBefore = r.panels();
            std::set<wc::ui::ModulePanel*> before (panelsBefore.begin(), panelsBefore.end());
            // 259 A down to the empty second row
            drag (r, wc::kInst259, { { 300.0f, 120.0f }, { 400.0f, 600.0f }, { 420.0f, 640.0f } }, false);
            const bool hole = r.rv->movingPanel() == r.panel (wc::kInst259);
            shot (r, "rows_drag_into_empty_row.png", { 0, 52, 1400, 1100 });
            auto* g = r.panel (wc::kInst259)->grip.get();
            g->mouseUp (at (*g, r.screen ({ 420.0f, 640.0f }), left));
            pump (20);
            render (ref, 60, false, a); render (r.proc, 60, false, b);
            const auto panelsAfter = r.panels();
            const bool samePanels = std::set<wc::ui::ModulePanel*> (panelsAfter.begin(), panelsAfter.end()) == before;
            bool sameCables = cablesBefore.size() == r.proc.getCables().size();
            for (size_t i = 0; sameCables && i < cablesBefore.size(); ++i)
            {
                const auto &x = cablesBefore[i], &y = r.proc.getCables()[i];
                sameCables = x.srcInst == y.srcInst && x.srcJack == y.srcJack && x.dstInst == y.dstInst && x.dstJack == y.dstJack && x.colour == y.colour;
            }
            const bool sameAudio = a.size() == b.size() && std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0;
            const bool moved = r.proc.layout().size() == 2 && r.proc.layout()[1] == std::vector<int> { wc::kInst259 }
                               && r.panel (wc::kInst259)->getPosition() == juce::Point<int> (6, 454);
            check (hole && moved && samePanels && sameCables && sameAudio, "move: hole " + juce::String ((int) hole) + " moved " + juce::String ((int) moved)
                   + " panels " + juce::String ((int) samePanels) + " cables " + juce::String ((int) sameCables) + " audio " + juce::String ((int) sameAudio));
            std::printf ("rows, move 259 A to row 2: %s, same panels %s, same cables %s, audio bit for bit %s %s\n", r.rowsText().toRawUTF8(),
                         samePanels ? "yes" : "NO", sameCables ? "yes" : "NO", sameAudio ? "yes" : "NO", hole && moved && samePanels && sameCables && sameAudio ? "ok" : "BAD");

            // LAYOUT comes back with the state, and the other way round
            juce::MemoryBlock st;
            r.proc.getStateInformation (st);
            WcProcessor back;
            back.setStateInformation (st.getData(), (int) st.getSize());
            check (back.layout() == r.proc.layout(), "state round trip of the rows");
        }

        // 4. a full row refuses the module (red bar, the readout says why); below the last row there is always room
        {
            Rig r (1400, 1500, [] (WcProcessor& p)
            {
                p.loadBuiltinPatch (0);
                for (int i : { wc::kInst248, wc::kInst218, wc::kInst666, wc::kInst281, wc::kInst292 }) p.setPresent (i, true, 0);
            });
            const auto rows0 = r.rowsText();
            check (! WcProcessor::rowHasRoom (r.proc.layout()[0], WcProcessor::panelWidth (wc::kInst292)), "row 1 is not full: " + rows0);
            // the 292 (row 2) onto row 1, between the MIDI and the 259
            drag (r, wc::kInst292, { { 200.0f, 600.0f }, { 186.0f, 200.0f } }, false);
            const bool refusedShown = r.ro->isUp() && r.ro->current().value == "Row 1 is full" && r.ro->current().warn;
            shot (r, "rows_full_row.png", { 0, 52, 1400, 1100 });
            auto* g = r.panel (wc::kInst292)->grip.get();
            g->mouseUp (at (*g, r.screen ({ 186.0f, 200.0f }), left));
            pump (10);
            const bool refused = r.rowsText() == rows0 && ! r.ro->isUp();
            // under the last row: the provisional one
            drag (r, wc::kInst292, { { 200.0f, 600.0f }, { 300.0f, 1000.0f } }, false);
            shot (r, "rows_provisional_row.png", { 0, 52, 1400, 1448 });
            g = r.panel (wc::kInst292)->grip.get();
            g->mouseUp (at (*g, r.screen ({ 300.0f, 1000.0f }), left));
            pump (10);
            const bool fresh = r.proc.layout().size() == 3 && r.proc.layout()[2] == std::vector<int> { wc::kInst292 }
                               && r.panel (wc::kInst292)->getPosition() == juce::Point<int> (6, 902);
            check (refusedShown && refused && fresh, "full row " + juce::String ((int) refusedShown) + juce::String ((int) refused) + ", new row " + juce::String ((int) fresh) + ": " + r.rowsText());
            std::printf ("rows, full row refused %s, new row below the last %s: %s\n", refusedShown && refused ? "ok" : "BAD", fresh ? "ok" : "BAD", r.rowsText().toRawUTF8());
            // the 281 next to the 292: row 2 is left empty and stays, with its "x" (three rows: it can go); at 50 % too
            drag (r, wc::kInst281, { { 200.0f, 600.0f }, { 600.0f, 1050.0f } });
            const bool kept = r.rowsText() == "[midi a out mf kb s1] [] [la fa]";
            shot (r, "rows_empty_row_x.png", { 0, 52, 1400, 1448 });
            r.rv->setZoom (0.5f); pump (20);
            shot (r, "rows_empty_row_x_zoom50.png", { 0, 52, 1400, 800 });
            r.rv->setZoom (1.0f); pump (20);
            juce::TextButton* rowX = nullptr;
            for (auto* c : r.rv->content.getChildren())
                if (auto* tb = dynamic_cast<juce::TextButton*> (c)) if (tb->isVisible() && tb->isEnabled()) rowX = tb;
            if (rowX != nullptr && rowX->onClick != nullptr) rowX->onClick();
            pump (20);
            const bool gone = r.rowsText() == "[midi a out mf kb s1] [la fa]" && r.panel (wc::kInst292)->getPosition() == juce::Point<int> (6, 454);
            check (kept && gone, "empty row kept " + juce::String ((int) kept) + ", its x " + juce::String ((int) gone) + ": " + r.rowsText());
            std::printf ("rows, an emptied row stays %s, its x takes it away %s\n", kept ? "ok" : "BAD", gone ? "ok" : "BAD");
            // the OUTPUT on its way between the 292 and the 281: the hole opens there and the 281 steps aside
            drag (r, wc::kInstOutput, { { 900.0f, 300.0f }, { 300.0f, 640.0f } }, false);
            shot (r, "rows_drag_hole.png", { 0, 52, 1400, 1000 });
            r.rv->cancelModuleDrag();
            { auto* go = r.panel (wc::kInstOutput)->grip.get(); go->mouseUp (at (*go, r.screen ({ 300.0f, 640.0f }), left)); }
            check (r.rowsText() == "[midi a out mf kb s1] [la fa]", "cancel moved something: " + r.rowsText());
            // the strip under the mouse, Classic and Neon Void
            auto* g259 = r.panel (wc::kInst259)->grip.get();
            g259->mouseEnter (at (*g259, g259->localPointToGlobal (juce::Point<float> (60.0f, 11.0f)), {}));
            const auto near = r.ed->getLocalArea (r.panel (wc::kInst259), juce::Rectangle<int> (-40, -20, 720, 140));
            shot (r, "rows_strip_hover.png", near);
            wc::ui::skin::current = 1;
            r.ed->repaint(); pump (20);
            shot (r, "rows_strip_hover_neon.png", near);
            drag (r, wc::kInst292, { { 200.0f, 600.0f }, { 700.0f, 200.0f } }, false);
            shot (r, "rows_full_row_neon.png", { 0, 52, 1400, 1100 });
            r.rv->cancelModuleDrag();
            wc::ui::skin::current = 0;
            g259->mouseExit (at (*g259, { -5.0f, -5.0f }, {}));
        }

        // 5. a cable dragged to the edge of the view scrolls it (or rows of 4008 could not be patched); a module too
        {
            Rig r (1400, 700, [] (WcProcessor& p) { p.loadBuiltinPatch (0); });
            auto* jack = r.rv->findJack (wc::kInst259, true, wc::M259::Out_Sine);
            const auto vb = r.vp->getScreenBounds().toFloat();
            const auto x0 = r.vp->getViewPosition();
            jack->mouseDown (at (*jack, jack->getScreenBounds().toFloat().getCentre(), left));
            for (int i = 0; i < 10; ++i) jack->mouseDrag (at (*jack, { vb.getRight() - 10.0f, vb.getCentreY() }, left));
            const auto x1 = r.vp->getViewPosition();
            for (int i = 0; i < 10; ++i) jack->mouseDrag (at (*jack, { vb.getCentreX(), vb.getBottom() + 30.0f }, left));
            const auto x2 = r.vp->getViewPosition();
            const int cables = (int) r.proc.getCables().size();
            jack->mouseUp (at (*jack, { vb.getCentreX(), vb.getBottom() + 30.0f }, left));
            const bool cableScroll = x1.x - x0.x == 200 && x2.y > x1.y && (int) r.proc.getCables().size() == cables;
            // a module, the same way; Esc-like cancel puts it back
            const auto rows0 = r.rowsText();
            r.vp->setViewPosition (0, 0);
            auto* g = r.panel (wc::kInst259)->grip.get();
            g->mouseDown (at (*g, g->localPointToGlobal (juce::Point<float> (30.0f, 11.0f)), left));
            for (int i = 0; i < 10; ++i) g->mouseDrag (at (*g, { vb.getRight() - 10.0f, vb.getCentreY() }, left));
            const bool moduleScroll = r.vp->getViewPositionX() == 200;
            r.rv->cancelModuleDrag();
            g->mouseUp (at (*g, { vb.getRight() - 10.0f, vb.getCentreY() }, left));
            const bool back = r.rowsText() == rows0 && r.rv->movingPanel() == nullptr && r.panel (wc::kInst259)->getPosition() == juce::Point<int> (188, 6);
            // "+ MODULE" with the second row in view: the 230 goes at its end, the readout says so
            r.vp->setViewPosition (0, 448);
            r.rv->addModule (wc::kInst230);
            pump (30);
            const bool added = r.rowsText() == "[midi a out] [ef]" && r.ro->isUp() && r.ro->current().value == "Added at the end of row 2";
            check (added, "+ MODULE: " + r.rowsText() + " / " + r.ro->current().value);
            std::printf ("rows, + MODULE with row 2 in view: %s, \"%s\" %s\n", r.rowsText().toRawUTF8(), r.ro->current().value.toRawUTF8(), added ? "ok" : "BAD");
            check (cableScroll && moduleScroll && back, "auto-scroll: cable " + x0.toString() + " -> " + x1.toString() + " -> " + x2.toString()
                   + ", module " + juce::String ((int) moduleScroll) + ", back " + juce::String ((int) back));
            std::printf ("rows, auto-scroll: cable %d px right then down %d px %s, module %s, cancel puts it back %s\n", x1.x - x0.x, x2.y - x1.y,
                         cableScroll ? "ok" : "BAD", moduleScroll ? "ok" : "BAD", back ? "ok" : "BAD");
        }
        if (! bad.isEmpty()) { std::printf ("rows: BAD: %s\n", bad.joinIntoString ("; ").toRawUTF8()); return 1; }
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
