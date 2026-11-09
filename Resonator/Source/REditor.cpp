#include "REditor.h"
#include "BinaryData.h"
#include <map>

using namespace juce;
using namespace pg::ui;
using namespace cyr;
using namespace cyr::ui;

namespace
{
struct Page : public Component
{
    OwnedArray<Component> items;
    std::function<void (Rectangle<int>)> layout;
    template <typename T> T* add (T* c) { items.add (c); addAndMakeVisible (c); return c; }
    void resized() override { if (layout) layout (getLocalBounds().reduced (6)); }
};

// Titled chamfer frame holding a grid of knobs / switches, with an optional display on top or at the left
struct Section : public Component
{
    Section (const String& t, Colour a) : title (t), accent (a) {}
    String title;
    Colour accent;
    std::vector<Component*> cells;
    Component* display = nullptr;
    Component* titleExtra = nullptr;
    int cols = 6, displayH = 0, displayW = 0;   // displayW > 0: display at the left
    OwnedArray<Component> owned;
    template <typename T> T* add (T* c) { owned.add (c); addAndMakeVisible (c); cells.push_back (c); return c; }
    template <typename T> T* own (T* c) { owned.add (c); addAndMakeVisible (c); return c; }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const auto& sk = skin();
        const auto acc = sk.tintFrames ? sk.frameTint : accent;
        brand::chamferFrame (g, r, 14.0f, 5.0f, sk.panelFill, acc.withAlpha (0.85f), sk.knob != 1);
        brand::sparkle (g, { r.getX() + 22.0f, r.getY() + 21.0f }, 6.0f, acc);
        g.setColour (sk.text);
        g.setFont (fonts::label (11.5f));
        g.drawText (title.toUpperCase(), Rectangle<float> (r.getX() + 34.0f, r.getY() + 10.0f, r.getWidth() - 48.0f, 22.0f), Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 10);
        auto top = r.removeFromTop (24);
        if (titleExtra != nullptr) titleExtra->setBounds (top.removeFromRight (jmin (150, top.getWidth() / 2)).reduced (0, 1));
        if (display != nullptr)
        {
            if (displayW > 0) { display->setBounds (r.removeFromLeft (displayW).reduced (0, 2)); r.removeFromLeft (8); }
            else { display->setBounds (r.removeFromTop (displayH)); r.removeFromTop (4); }
        }
        // switches share one row at the bottom
        std::vector<Component*> sw, kn;
        for (auto* c : cells) (dynamic_cast<cyr::ui::Switch*> (c) != nullptr ? sw : kn).push_back (c);
        if (! sw.empty())
        {
            auto row = r.removeFromBottom (28);
            const int w = jmin (150, row.getWidth() / (int) sw.size());
            for (auto* c : sw) c->setBounds (row.removeFromLeft (w).reduced (3, 1));
        }
        if (kn.empty()) return;
        const int rows = ((int) kn.size() + cols - 1) / cols;
        const int cw = r.getWidth() / cols, ch = jmin (r.getHeight() / jmax (1, rows), 86);
        for (size_t i = 0; i < kn.size(); ++i)
        {
            const int cx = (int) i % cols, cy = (int) i / cols;
            kn[i]->setBounds (Rectangle<int> (r.getX() + cx * cw, r.getY() + cy * ch, cw, ch).reduced (2));
        }
    }
};

std::vector<Rectangle<int>> split (Rectangle<int> r, std::initializer_list<float> weights, bool horizontal = true, int gap = 6)
{
    float total = 0.0f;
    for (auto w : weights) total += w;
    std::vector<Rectangle<int>> out;
    const int size = (horizontal ? r.getWidth() : r.getHeight()) - gap * ((int) weights.size() - 1);
    int i = 0, used = 0;
    for (auto w : weights)
    {
        const int s = (++i == (int) weights.size()) ? size - used : (int) ((float) size * w / total);
        out.push_back (horizontal ? r.removeFromLeft (s) : r.removeFromTop (s));
        if (horizontal) r.removeFromLeft (gap); else r.removeFromTop (gap);
        used += s;
    }
    return out;
}

Label* makeLabel (Page& page, const String& text, float size = 12.0f, Colour c = colours::textDim)
{
    auto* l = page.add (new Label ({}, text));
    l->setFont (fonts::sans (size));
    l->setColour (Label::textColourId, c);
    l->setJustificationType (Justification::centredLeft);
    return l;
}

// FX cascade drawn as a signal flow (same as Synthgularity's FX page)
struct FxChainView : public Component
{
    FxChainView (APVTS& s, bool mixbus) : st (s), mb (mixbus) {}
    APVTS& st;
    bool mb;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (4.0f);
        auto inner = brand::chamferFrame (g, r, 14.0f, 5.0f, cosmos::voidC.withAlpha (0.85f), cosmos::nebula, false).reduced (12.0f, 8.0f);
        g.setColour (colours::textDim);
        g.setFont (fonts::label (9.5f));
        g.drawText ("SIGNAL FLOW", inner.removeFromTop (16.0f), Justification::centredLeft);
        std::vector<std::pair<String, bool>> nodes;
        bool used[16] {};
        const int count = mb ? pg::MB_Count : pg::FX_Count;
        for (int i = 0; i < 10; ++i)
        {
            const int t = (int) st.getRawParameterValue ((mb ? "mb_slot" : "fx_slot") + String (i + 1))->load();
            if (t <= 0 || t >= count || used[t]) continue;
            used[t] = true;
            const auto id = FxSlotRow::onParamFor (t, mb);
            const bool on = id.isNotEmpty() && st.getRawParameterValue (id)->load() > 0.5f;
            nodes.push_back ({ (mb ? pg::choices::mixbusTypes() : pg::choices::fxTypes())[t], on });
        }
        const int n = (int) nodes.size() + 2;
        const float gap = 12.0f, w = (inner.getWidth() - gap * (float) (n - 1)) / (float) n;
        auto box = inner.withHeight (jmin (44.0f, inner.getHeight())).withY (inner.getCentreY() - 22.0f).withWidth (w);
        const float t = (float) Time::getMillisecondCounterHiRes() * 0.001f;
        for (int i = 0; i < n; ++i)
        {
            const bool io = i == 0 || i == n - 1;
            const String name = i == 0 ? "IN" : (i == n - 1 ? "OUT" : nodes[(size_t) i - 1].first);
            const bool on = io || nodes[(size_t) i - 1].second;
            g.setColour (io ? cosmos::deepSpace : (on ? cosmos::nebula : cosmos::night));
            g.fillRoundedRectangle (box, 5.0f);
            g.setColour (io ? cosmos::gold : (on ? cosmos::neon : cosmos::steel.withAlpha (0.5f)));
            g.drawRoundedRectangle (box.reduced (0.5f), 5.0f, on ? 1.6f : 1.0f);
            g.setColour (on ? cosmos::starlight : cosmos::steel);
            g.setFont (fonts::label (io ? 11.0f : 9.0f));
            g.drawFittedText (name.toUpperCase() + (io || on ? "" : "\n(off)"), box.reduced (2.0f).toNearestInt(), Justification::centred, 2, 0.6f);
            if (i < n - 1)
            {
                const float ax = box.getRight() + 1.0f, ay = box.getCentreY();
                Path a;
                a.addArrow ({ ax, ay, ax + gap - 2.0f, ay }, 1.5f, 6.0f, 5.0f);
                g.setColour (cosmos::orchid);
                g.fillPath (a);
                // signal dot
                const float f = std::fmod (t * 1.4f + (float) i * 0.13f, 1.0f);
                g.setColour (cosmos::neon.withAlpha (0.8f));
                g.fillEllipse (ax + f * (gap - 2.0f) - 1.5f, ay - 1.5f, 3.0f, 3.0f);
            }
            box.translate (w + gap, 0.0f);
        }
    }
};
} // namespace

// =============================================================================
ResonatorEditor::ResonatorEditor (ResonatorProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    skinIndex() = jlimit (0, (int) skins().size() - 1, proc.uiSkin);
    setLookAndFeel (&lnf);
    logoGrid = ImageCache::getFromMemory (BinaryData::bglogogrid_jpg, BinaryData::bglogogrid_jpgSize);
    isologo  = ImageCache::getFromMemory (BinaryData::isologo_png, BinaryData::isologo_pngSize);
    auto& st = proc.apvts;

    // ---------------- header ----------------
    for (auto* c : std::initializer_list<Component*> { &presetBox, &prevButton, &nextButton, &saveButton, &initButton, &loadButton, &foldersButton, &skinBox, &voicesLabel })
        addAndMakeVisible (c);
    refreshPresetMenu();
    presetBox.onChange = [this] { choosePreset (presetBox.getSelectedId()); };
    prevButton.onClick = [this] { const int i = presetBox.getSelectedItemIndex(); if (i > 0) presetBox.setSelectedItemIndex (i - 1); };
    nextButton.onClick = [this] { const int i = presetBox.getSelectedItemIndex(); if (i + 1 < presetBox.getNumItems()) presetBox.setSelectedItemIndex (i + 1); };
    saveButton.onClick = [this] { savePresetDialog(); };
    loadButton.onClick = [this] { loadPresetDialog(); };
    foldersButton.onClick = [this] { foldersMenu(); };
    loadButton.setTooltip ("Open a preset file (.cyrpreset) from any folder");
    foldersButton.setTooltip ("Choose the folders where presets are opened from and saved to");
    initButton.onClick = [this] { proc.initPreset(); presetBox.setText ("Init", dontSendNotification); };
    for (int i = 0; i < (int) skins().size(); ++i) skinBox.addItem (skins()[(size_t) i].name, i + 1);
    skinBox.setSelectedId (skinIndex() + 1, dontSendNotification);
    skinBox.onChange = [this] { setSkin (skinBox.getSelectedId() - 1); };
    skinBox.setTooltip ("Interface skin");
    masterKnob = std::make_unique<ModKnob> (proc, "master", "MASTER", cosmos::gold);
    addAndMakeVisible (*masterKnob);
    voicesLabel.setFont (fonts::sans (11.0f));
    voicesLabel.setColour (Label::textColourId, colours::textDim);
    voicesLabel.setJustificationType (Justification::centred);

    addAndMakeVisible (tabs);
    tabs.setTabBarDepth (30);
    tabs.setOutline (0);

    auto addTab = [this] (const String& name, Page* page, std::vector<Component*> lv, std::vector<ModKnob*> kn)
    {
        pages.add (page);
        tabs.addTab (name, Colours::transparentBlack, page, false);
        live.push_back (std::move (lv));
        knobs.push_back (std::move (kn));
    };
    auto knob = [this] (Section& s, std::vector<ModKnob*>& list, const String& id, const String& label, Colour c)
    {
        auto* k = s.add (new ModKnob (proc, id, label, c));
        list.push_back (k);
        return k;
    };

    // ---------------- OSCILLATORS ----------------
    {
        auto* page = new Page();
        std::vector<ModKnob*> kn;
        std::vector<Component*> lv;
        static const char* letters[] = { "a", "b", "c", "d" };
        static const Colour accents[] = { cosmos::neon, cosmos::orchid, cosmos::azure, cosmos::gold };
        std::vector<Section*> secs;
        for (int o = 0; o < 4; ++o)
        {
            const String pre = String ("osc") + letters[o] + "_";
            auto* s = page->add (new Section ("Oscillator " + String (letters[o]).toUpperCase() + "  -  Calabi-Yau manifold", accents[o]));
            s->cols = 7;
            auto* sv = s->own (new SurfaceView (proc, o));
            s->display = sv;
            s->displayW = 190;
            surfaces.push_back (sv);
            s->titleExtra = s->own (new cyr::ui::Switch (st, pre + "on", "ON"));
            const std::pair<const char*, const char*> ks[] = {
                { "mode", "MODE" }, { "n", "DIMENSION n" }, { "k1", "PATCH k1" }, { "k2", "PATCH k2" }, { "alpha", "ALPHA" }, { "cover", "COVERAGE" }, { "pos", "POSITION" },
                { "ru", "RADIUS u" }, { "rv", "RADIUS v" }, { "ratio", "RATIO v:u" }, { "spin", "SPIN Hz" }, { "width", "WIDTH" }, { "uni", "UNISON" }, { "det", "DETUNE" },
                { "oct", "OCTAVE" }, { "semi", "SEMI" }, { "fine", "FINE" }, { "level", "LEVEL" }, { "pan", "PAN" } };
            for (auto& k : ks) knob (*s, kn, pre + k.first, k.second, accents[o]);
            s->add (new cyr::ui::Switch (st, pre + "rphase", "RANDOM PHASE"));
            secs.push_back (s);
            lv.push_back (sv);
        }
        page->layout = [secs] (Rectangle<int> r)
        {
            auto rows = split (r, { 1.0f, 1.0f }, false);
            for (int i = 0; i < 2; ++i)
            {
                auto c = split (rows[(size_t) i], { 1.0f, 1.0f });
                secs[(size_t) i * 2]->setBounds (c[0]);
                secs[(size_t) i * 2 + 1]->setBounds (c[1]);
            }
        };
        addTab ("OSCILLATORS", page, lv, kn);
        oscKnobs = kn;
    }

    // ---------------- FM / PD MATRIX ----------------
    {
        auto* page = new Page();
        std::vector<ModKnob*> kn;
        auto makeGrid = [&] (const String& title, const String& prefix, Colour c)
        {
            auto* s = page->add (new Section (title, c));
            s->cols = 5;
            static const char* L[] = { "a", "b", "c", "d" };
            // header row: blank + targets
            for (int col = 0; col < 5; ++col)
            {
                auto* lab = s->add (new Label ({}, col == 0 ? String ("FROM \\ TO") : String (L[col - 1]).toUpperCase()));
                lab->setJustificationType (Justification::centred);
                lab->setFont (col == 0 ? fonts::label (9.0f) : fonts::display (13.0f));
                lab->setColour (Label::textColourId, col == 0 ? colours::textDim : c);
            }
            for (int src = 0; src < 4; ++src)
            {
                auto* lab = s->add (new Label ({}, String (L[src]).toUpperCase()));
                lab->setJustificationType (Justification::centred);
                lab->setFont (fonts::display (13.0f));
                lab->setColour (Label::textColourId, c);
                for (int dst = 0; dst < 4; ++dst)
                    knob (*s, kn, prefix + L[src] + L[dst], String (L[src]).toUpperCase() + " > " + String (L[dst]).toUpperCase() + (src == dst ? " FB" : ""), c);
            }
            return s;
        };
        auto* fmS = makeGrid ("FM matrix  -  phase modulation", "fm_", cosmos::gold);
        auto* pdS = makeGrid ("PD matrix  -  phase distortion", "pd_", cosmos::orchid);
        matrixView = page->add (new MatrixView (proc));
        auto* tip = makeLabel (*page, "Rows modulate columns. FM bends the orbit phase of the target by the source's output; PD moves the target's "
                                      "half-cycle point with the source (Casio-style phase distortion, at audio rate). The diagonal is self-feedback. "
                                      "Every cell is a knob you can modulate: drag an ENV or LFO onto it.", 11.5f);
        tip->setJustificationType (Justification::topLeft);
        page->layout = [=] (Rectangle<int> r)
        {
            tip->setBounds (r.removeFromBottom (40).reduced (8, 2));
            auto c = split (r, { 1.0f, 0.9f, 1.0f });
            fmS->setBounds (c[0]);
            matrixView->setBounds (c[1]);
            pdS->setBounds (c[2]);
        };
        addTab ("FM / PD MATRIX", page, { matrixView }, kn);
    }

    // ---------------- MODULATION: envelopes and LFOs ----------------
    {
        auto* page = new Page();
        std::vector<ModKnob*> kn;
        std::vector<Component*> lv;
        std::vector<Section*> envS, lfoS;
        for (int e = 0; e < kEnvs; ++e)
        {
            const String pre = "env" + String (e + 1) + "_";
            auto* s = page->add (new Section ("Envelope " + String (e + 1), sourceColour (MS_Env1 + e)));
            s->cols = 4;
            s->display = s->own (new EnvelopeView (st, pre, sourceColour (MS_Env1 + e)));
            s->displayH = 74;
            for (auto& k : { std::pair<const char*, const char*> { "a", "ATTACK" }, { "d", "DECAY" }, { "s", "SUSTAIN" }, { "r", "RELEASE" } })
                knob (*s, kn, pre + k.first, k.second, sourceColour (MS_Env1 + e));
            envS.push_back (s);
        }
        for (int l = 0; l < kLfos; ++l)
        {
            const String pre = "lfo" + String (l + 1) + "_";
            auto* s = page->add (new Section ("LFO " + String (l + 1), sourceColour (MS_Lfo1 + l)));
            s->cols = 4;
            auto* view = s->own (new LfoView (proc, l));
            s->display = view;
            s->displayH = 46;
            lv.push_back (view);
            const auto c = sourceColour (MS_Lfo1 + l);
            knob (*s, kn, pre + "shape", "SHAPE", c);
            knob (*s, kn, pre + "rate", "RATE", c);
            knob (*s, kn, pre + "div", "DIVISION", c);
            knob (*s, kn, pre + "phase", "PHASE", c);
            knob (*s, kn, pre + "fade", "FADE IN", c);
            s->add (new cyr::ui::Switch (st, pre + "sync", "SYNC"));
            s->add (new cyr::ui::Switch (st, pre + "trig", "RETRIG"));
            lfoS.push_back (s);
        }
        auto* tip = makeLabel (*page, "Drag a chip from the MOD SOURCES bar (bottom) onto any knob. Right-click a modulated knob to change the amount, invert or remove it. "
                                      "LFO RETRIG restarts the LFO on every note (per voice); off = one free-running LFO for all voices.", 11.5f);
        tip->setJustificationType (Justification::topLeft);
        page->layout = [=] (Rectangle<int> r)
        {
            tip->setBounds (r.removeFromBottom (34).reduced (8, 2));
            auto rows = split (r, { 0.92f, 1.0f }, false);
            auto e = split (rows[0], { 1, 1, 1, 1 });
            for (int i = 0; i < 4; ++i) envS[(size_t) i]->setBounds (e[(size_t) i]);
            auto l = split (rows[1], { 1, 1, 1, 1, 1, 1 });
            for (int i = 0; i < 6; ++i) lfoS[(size_t) i]->setBounds (l[(size_t) i]);
        };
        addTab ("ENV / LFO", page, lv, kn);
    }

    // ---------------- MOD LIST ----------------
    {
        auto* page = new Page();
        std::vector<Component*> rows;
        for (int s = 0; s < kSlots; ++s) rows.push_back (page->add (new ModSlotRow (proc, s)));
        auto* title = makeLabel (*page, "MODULATION SLOTS  -  source, destination, amount (32 slots; drag and drop fills the next free one)", 12.0f, colours::text);
        page->layout = [=] (Rectangle<int> r)
        {
            title->setBounds (r.removeFromTop (26).reduced (6, 0));
            auto cols = split (r, { 1, 1 }, true, 14);
            for (int c = 0; c < 2; ++c)
            {
                auto col = cols[(size_t) c];
                const int h = jmin (34, col.getHeight() / 16);
                for (int i = 0; i < 16; ++i) rows[(size_t) (c * 16 + i)]->setBounds (col.removeFromTop (h));
            }
        };
        addTab ("MOD LIST", page, rows, {});
    }

    // ---------------- VOICE: amp, filter, global, keyboard ----------------
    {
        auto* page = new Page();
        std::vector<ModKnob*> kn;
        auto* ampS = page->add (new Section ("Amplifier", cosmos::starlight));
        ampS->cols = 4;
        ampS->display = ampS->own (new EnvelopeView (st, "amp_", cosmos::neon));
        ampS->displayH = 90;
        for (auto& k : { std::pair<const char*, const char*> { "amp_a", "ATTACK" }, { "amp_d", "DECAY" }, { "amp_s", "SUSTAIN" }, { "amp_r", "RELEASE" } })
            knob (*ampS, kn, k.first, k.second, cosmos::neon);
        auto* fltS = page->add (new Section ("Filter", cosmos::fuchsia));
        fltS->cols = 3;
        fltS->titleExtra = fltS->own (new cyr::ui::Switch (st, "flt_on", "ON"));
        knob (*fltS, kn, "flt_type", "TYPE", cosmos::fuchsia);
        knob (*fltS, kn, "flt_cut", "CUTOFF", cosmos::fuchsia);
        knob (*fltS, kn, "flt_res", "RESONANCE", cosmos::fuchsia);
        knob (*fltS, kn, "flt_drive", "DRIVE", cosmos::fuchsia);
        knob (*fltS, kn, "flt_key", "KEY TRACK", cosmos::fuchsia);
        auto* glS = page->add (new Section ("Voice", cosmos::gold));
        glS->cols = 3;
        knob (*glS, kn, "poly", "VOICES", cosmos::gold);
        knob (*glS, kn, "glide", "GLIDE", cosmos::gold);
        knob (*glS, kn, "bend", "BEND RANGE", cosmos::gold);
        knob (*glS, kn, "velsens", "VELOCITY", cosmos::gold);
        knob (*glS, kn, "quality", "OVERSAMPLE", cosmos::gold);
        knob (*glS, kn, "master", "MASTER", cosmos::gold);
        keyboard = std::make_unique<MidiKeyboardComponent> (proc.keyboardState, MidiKeyboardComponent::horizontalKeyboard);
        keyboard->setAvailableRange (24, 108);
        keyboard->setLowestVisibleKey (36);
        keyboard->setColour (MidiKeyboardComponent::keyDownOverlayColourId, cosmos::neon.withAlpha (0.7f));
        keyboard->setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, cosmos::orchid.withAlpha (0.35f));
        keyboard->setWantsKeyboardFocus (true);
        page->addAndMakeVisible (*keyboard);
        auto* kbTip = makeLabel (*page, "Click the keyboard, then play with the computer keys (A W S E D F T G Y H U J K). Z / X change octave.", 11.0f);
        page->layout = [=] (Rectangle<int> r)
        {
            auto kb = r.removeFromBottom (170);
            kbTip->setBounds (kb.removeFromTop (20).reduced (8, 0));
            keyboard->setBounds (kb.reduced (6, 2));
            keyboard->setKeyWidth ((float) keyboard->getWidth() / 50.0f);
            keyboard->setLowestVisibleKey (24);
            r.removeFromBottom (6);
            auto c = split (r, { 1.2f, 1.0f, 1.0f });
            ampS->setBounds (c[0]); fltS->setBounds (c[1]); glS->setBounds (c[2]);
        };
        addTab ("VOICE", page, {}, kn);
    }

    // ---------------- FX and MIXBUS: Synthgularity's two racks of 10 slots ----------------
    auto buildRack = [this, &st] (const String& tabName, bool mixbus, std::map<int, Panel*> (*makePanels) (Page&, APVTS&))
    {
        auto* page = new Page();
        auto fxPanels = makePanels (*page, st);
        auto* rackTitle = makeLabel (*page, mixbus ? "MIXBUS  -  CASCADE  (top > bottom)" : "FX RACK  -  CASCADE  (top > bottom)", 12.5f, colours::text);
        auto* rackTip = makeLabel (*page, mixbus ? "Mix processors for the whole synth, after the FX RACK."
                                                 : "Pick an effect in each slot, the dot switches it on. Arrows move it in the chain. Click a slot to edit it.", 11.0f);
        rackTip->setJustificationType (Justification::topLeft);
        std::vector<FxSlotRow*> rows;
        for (int s2 = 0; s2 < 10; ++s2) rows.push_back (page->add (new FxSlotRow (st, s2, mixbus)));
        auto selected = std::make_shared<int> (0);
        auto* emptyNote = makeLabel (*page, "Empty slot: choose an effect from the menu.", 13.0f);
        emptyNote->setJustificationType (Justification::centred);
        auto showSel = [rows, fxPanels, selected, emptyNote] ()
        {
            emptyNote->setVisible (rows[(size_t) *selected]->type() == 0);
            for (int i = 0; i < (int) rows.size(); ++i) { rows[(size_t) i]->selected = i == *selected; rows[(size_t) i]->repaint(); }
            const int type = rows[(size_t) *selected]->type();
            for (auto& kv : fxPanels) kv.second->setVisible (kv.first == type);
        };
        const String prefix = mixbus ? "mb_slot" : "fx_slot";
        for (auto* row : rows)
        {
            row->onSelect = [selected, showSel] (int sl) { *selected = sl; showSel(); };
            row->onMove = [this, selected, showSel, prefix] (int sl, int dir)
            {
                const int other = sl + dir;
                if (other < 0 || other > 9) return;
                auto* a = proc.apvts.getParameter (prefix + String (sl + 1));
                auto* b = proc.apvts.getParameter (prefix + String (other + 1));
                const float va = a->getValue(), vb = b->getValue();
                a->setValueNotifyingHost (vb);
                b->setValueNotifyingHost (va);
                *selected = other;
                showSel();
            };
        }
        auto* chain = page->add (new FxChainView (st, mixbus));
        page->layout = [=] (Rectangle<int> r)
        {
            auto left = r.removeFromLeft (320);
            r.removeFromLeft (8);
            rackTitle->setBounds (left.removeFromTop (24).reduced (6, 0));
            rackTip->setBounds (left.removeFromBottom (40).reduced (6, 2));
            const int h = jmin (46, left.getHeight() / 10);
            for (auto* row : rows) row->setBounds (left.removeFromTop (h));
            auto area = r.removeFromTop (jmax (200, r.getHeight() - 110));
            for (auto& kv : fxPanels) kv.second->setBounds (area);
            emptyNote->setBounds (area);
            r.removeFromTop (8);
            chain->setBounds (r);
            showSel();
        };
        std::vector<Component*> v (rows.begin(), rows.end());
        v.push_back (chain);
        pages.add (page);
        tabs.addTab (tabName, Colours::transparentBlack, page, false);
        live.push_back (v);
        knobs.push_back ({});
    };
    auto items = [] (std::initializer_list<const char*> l) { StringArray a; for (auto* s : l) a.add (s); return a; };
    juce::ignoreUnused (items);
    buildRack ("FX", false, [] (Page& page, APVTS& s) -> std::map<int, Panel*>
    {
        auto it = [] (std::initializer_list<const char*> l) { StringArray a; for (auto* x : l) a.add (x); return a; };
        std::map<int, Panel*> m;
        m[pg::FX_Filter1] = page.add (new Panel (s, "Filter 1", it ({ "ff1_on:On", "ff1_type:Type", "ff1_freq:Freq", "ff1_res:Reso",
                                        "ff1_morph:Morph / Vowel", "ff1_poles:Poles", "ff1_spread:Spread", "ff1_drive:Drive", "ff1_mix:Mix" }), 5, cosmos::neon));
        m[pg::FX_Filter2] = page.add (new Panel (s, "Filter 2", it ({ "ff2_on:On", "ff2_type:Type", "ff2_freq:Freq", "ff2_res:Reso",
                                        "ff2_morph:Morph / Vowel", "ff2_poles:Poles", "ff2_spread:Spread", "ff2_drive:Drive", "ff2_mix:Mix" }), 5, cosmos::orchid));
        m[pg::FX_Stereo] = page.add (new Panel (s, "Stereo", it ({ "fx_st_on:On", "fx_st_width:Width", "fx_st_bass:Mono Bass", "fx_st_haas:Haas ms" }), 4));
        m[pg::FX_Flanger] = page.add (new Panel (s, "Flanger", it ({ "fx_fl_on:On", "fx_fl_rate:Rate", "fx_fl_depth:Depth", "fx_fl_fb:Feedback", "fx_fl_delay:Delay", "fx_fl_mix:Mix" }), 6));
        m[pg::FX_Phaser] = page.add (new Panel (s, "Phaser", it ({ "fx_ph_on:On", "fx_ph_rate:Rate", "fx_ph_depth:Depth", "fx_ph_fb:Feedback", "fx_ph_center:Center", "fx_ph_stages:Stages", "fx_ph_mix:Mix" }), 7));
        m[pg::FX_Delay] = page.add (new Panel (s, "Delay", it ({ "fx_dl_on:On", "fx_dl_sync:Sync", "fx_dl_pp:Ping-Pong", "fx_dl_div:Division", "fx_dl_time:Time ms",
                                        "fx_dl_fb:Feedback", "fx_dl_lowcut:Low Cut", "fx_dl_highcut:High Cut", "fx_dl_mix:Mix" }), 5));
        m[pg::FX_Rings] = page.add (new Panel (s, "Resonator (Rings)", it ({ "fx_rn_on:On", "fx_rn_model:Model", "fx_rn_poly:Poly", "fx_rn_follow:Follow Keys", "fx_rn_note:Note",
                                        "fx_rn_structure:Structure", "fx_rn_bright:Brightness", "fx_rn_damping:Damping", "fx_rn_pos:Position", "fx_rn_mix:Mix" }), 5, colours::warn));
        m[pg::FX_Clouds] = page.add (new Panel (s, "Granular (Clouds)", it ({ "fx_gr_on:On", "fx_gr_mode:Mode", "fx_gr_quality:Quality", "fx_gr_freeze:Freeze",
                                        "fx_gr_pos:Position", "fx_gr_size:Size", "fx_gr_pitch:Pitch", "fx_gr_density:Density", "fx_gr_texture:Texture",
                                        "fx_gr_spread:Spread", "fx_gr_fb:Feedback", "fx_gr_reverb:Reverb", "fx_gr_mix:Mix" }), 7, colours::warn));
        m[pg::FX_Reverb] = page.add (new Panel (s, "Reverb", it ({ "fx_rv_on:On", "fx_rv_size:Size", "fx_rv_decay:Decay", "fx_rv_damp:Damping",
                                        "fx_rv_pre:Pre-Delay", "fx_rv_mod:Modulation", "fx_rv_width:Width", "fx_rv_mix:Mix" }), 4, cosmos::fuchsia));
        return m;
    });
    buildRack ("MIXBUS", true, [] (Page& page, APVTS& s) -> std::map<int, Panel*>
    {
        auto it = [] (std::initializer_list<const char*> l) { StringArray a; for (auto* x : l) a.add (x); return a; };
        std::map<int, Panel*> m;
        m[pg::MB_Tape] = page.add (new Panel (s, "Tape", it ({ "mb_tp_on:On", "mb_tp_drive:Drive", "mb_tp_wow:Wow & Flutter", "mb_tp_tone:Tone", "mb_tp_mix:Mix" }), 5, cosmos::gold));
        m[pg::MB_EQ] = page.add (new Panel (s, "EQ", it ({ "mb_eq_on:On", "mb_eq_lcut:Low Cut", "mb_eq_low:Low", "mb_eq_mid:Mid", "mb_eq_mfreq:Mid Freq", "mb_eq_high:High", "mb_eq_hcut:High Cut" }), 7, cosmos::neon));
        m[pg::MB_Comp] = page.add (new Panel (s, "Compressor", it ({ "mb_cp_on:On", "mb_cp_thr:Threshold", "mb_cp_ratio:Ratio", "mb_cp_att:Attack ms", "mb_cp_rel:Release ms", "mb_cp_gain:Makeup", "mb_cp_mix:Mix" }), 7, cosmos::orchid));
        m[pg::MB_Chorus] = page.add (new Panel (s, "Chorus", it ({ "mb_ch_on:On", "mb_ch_rate:Rate", "mb_ch_depth:Depth", "mb_ch_mix:Mix" }), 4, cosmos::azure));
        m[pg::MB_Imager] = page.add (new Panel (s, "Imager", it ({ "mb_im_on:On", "mb_im_width:Width", "mb_im_bass:Mono Bass" }), 3, cosmos::teal));
        m[pg::MB_Gate] = page.add (new Panel (s, "Trance Gate", it ({ "mb_tg_on:On", "mb_tg_pat:Pattern", "mb_tg_div:Division", "mb_tg_depth:Depth", "mb_tg_smooth:Smooth" }), 5, cosmos::fuchsia));
        m[pg::MB_DJ] = page.add (new Panel (s, "DJ Filter", it ({ "mb_dj_on:On", "mb_dj_pos:LP < > HP", "mb_dj_res:Reso" }), 3, cosmos::neon));
        m[pg::MB_Crush] = page.add (new Panel (s, "Bitcrusher", it ({ "mb_bc_on:On", "mb_bc_bits:Bits", "mb_bc_rate:Downsample", "mb_bc_mix:Mix" }), 4, cosmos::electric));
        m[pg::MB_Transient] = page.add (new Panel (s, "Transient Shaper", it ({ "mb_tr_on:On", "mb_tr_att:Attack", "mb_tr_sus:Sustain" }), 3, cosmos::gold));
        m[pg::MB_Limiter] = page.add (new Panel (s, "Limiter", it ({ "mb_lm_on:On", "mb_lm_in:Input", "mb_lm_ceil:Ceiling", "mb_lm_rel:Release ms" }), 4, cosmos::fuchsia));
        return m;
    });

    // ---------------- persistent bottom: modulation sources + analyzers ----------------
    for (int s = 1; s < MS_Count; ++s) addAndMakeVisible (chips.add (new cyr::ui::SourceChip (proc, s)));
    analyzer = std::make_unique<pg::ui::AnalyzerStrip> (pg::ui::ScopeFeed { &proc.meter, proc.scopeL.data(), proc.scopeR.data(), &proc.scopePos, ResonatorProcessor::kScopeRing });
    addAndMakeVisible (*analyzer);

    setResizable (true, true);
    setResizeLimits (1240, 860, 2200, 1500);
    setSize (1320, 900);
    startTimerHz (30);
}

ResonatorEditor::~ResonatorEditor()
{
    stopTimer();
    tabs.clearTabs();
    setLookAndFeel (nullptr);
}

void ResonatorEditor::setSkin (int index)
{
    skinIndex() = jlimit (0, (int) skins().size() - 1, index);
    proc.uiSkin = skinIndex();
    stars = Image();
    sendLookAndFeelChange();
    repaint();
}

void ResonatorEditor::paint (Graphics& g)
{
    const int sk = skinIndex();
    if (stars.getWidth() != getWidth() || stars.getHeight() != getHeight())
    {
        const float W = (float) getWidth(), Hh = (float) getHeight();
        if (sk == 0)
        {
            stars = brand::starfield (getWidth(), getHeight(), 11, 0.9f);
            Graphics sg (stars);
            sg.setOpacity (0.2f);
            sg.drawImage (logoGrid, stars.getBounds().toFloat(), RectanglePlacement::centred | RectanglePlacement::fillDestination);
            sg.setOpacity (1.0f);
            sg.setGradientFill (ColourGradient (cosmos::voidC, 0.0f, 0.0f, cosmos::voidC.withAlpha (0.0f), 0.0f, Hh * 0.55f, false));
            sg.fillAll();
        }
        else
        {
            stars = Image (Image::RGB, getWidth(), getHeight(), true);
            Graphics sg (stars);
            sg.setGradientFill (ColourGradient (skin().bgTop, 0, 0, skin().bgBottom, 0, Hh, false));
            sg.fillAll();
            Random rnd (3);
            if (sk == 1)
            {
                for (int k = 0; k < 900; ++k)
                {
                    sg.setColour (Colour (0xffffffff).withAlpha (0.012f + rnd.nextFloat() * 0.02f));
                    sg.drawHorizontalLine ((int) (rnd.nextFloat() * Hh), rnd.nextFloat() * W * 0.5f, W);
                }
                sg.setColour (Colour (0xffb08d57).withAlpha (0.5f));
                sg.fillRect (0.0f, (float) kHeaderH - 2.0f, W, 2.0f);
            }
            else
            {
                brand::perspectiveGrid (sg, { 0.0f, Hh * 0.55f, W, Hh * 0.45f }, cosmos::neon, 0.18f, 22, 12);
                for (int k = 0; k < 160; ++k)
                {
                    sg.setColour (cosmos::starlight.withAlpha (0.15f + rnd.nextFloat() * 0.4f));
                    sg.fillRect (rnd.nextFloat() * W, rnd.nextFloat() * Hh * 0.5f, 1.2f, 1.2f);
                }
            }
        }
    }
    g.drawImageAt (stars, 0, 0);

    // header: official isologo with a breathing aura, product name in the display face
    auto header = getLocalBounds().removeFromTop (kHeaderH).reduced (18, 0).toFloat();
    const auto iso = Rectangle<float> (header.getX(), header.getCentreY() - 23.0f, 46.0f, 46.0f);
    const float t = (float) animT;
    for (int k = 0; k < 3; ++k)
    {
        // three orbits turning around the isologo, like the oscillators around the surface
        const float ph = std::fmod (t * 0.3f + (float) k / 3.0f, 1.0f);
        const float rad = 24.0f + ph * 18.0f;
        g.setColour ((k == 0 ? cosmos::fuchsia : (k == 1 ? cosmos::neon : cosmos::gold)).withAlpha (0.32f * (1.0f - ph)));
        g.drawEllipse (Rectangle<float> (rad * 2.0f, rad * 1.3f).withCentre (iso.getCentre()), 1.4f);
        const float a = t * (0.9f + 0.4f * (float) k) + (float) k * 2.1f;
        g.setColour (cosmos::starlight.withAlpha (0.8f * (1.0f - ph)));
        g.fillEllipse (Rectangle<float> (3.0f, 3.0f).withCentre ({ iso.getCentreX() + std::cos (a) * rad, iso.getCentreY() + std::sin (a) * rad * 0.65f }));
    }
    g.setOpacity (1.0f);   // drawImage uses the current colour's alpha
    g.drawImage (isologo, iso, RectanglePlacement::centred);
    g.setColour (cosmos::starlight);
    g.setFont (fonts::display (16.0f));
    g.drawText ("CALABI-YAU RESONATOR", header.withTrimmedLeft (60.0f).withHeight (header.getHeight() * 0.6f), Justification::bottomLeft);
    g.setColour (cosmos::gold);
    g.setFont (fonts::label (9.5f));
    g.drawText ("BY CALABI YAU  -  z1^n + z2^n = 1", header.withTrimmedLeft (62.0f).withTrimmedTop (header.getHeight() * 0.64f).withHeight (16.0f), Justification::centredLeft);
    if (sk != 1) brand::checker (g, Rectangle<float> (0.0f, (float) kHeaderH, (float) getWidth(), 8.0f), 4.0f);

    // modulation source bar label
    auto bar = getLocalBounds().withTrimmedBottom (kAnalyzerH).removeFromBottom (kChipsH).reduced (14, 0);
    g.setColour (colours::textDim);
    g.setFont (fonts::label (9.0f));
    g.drawText ("MOD SOURCES", bar.removeFromLeft (110).toFloat(), Justification::centredLeft);
}

void ResonatorEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (kHeaderH).reduced (12, 12);
    header.removeFromLeft (330);
    masterKnob->setBounds (header.removeFromRight (70).withTrimmedTop (-6).withTrimmedBottom (-8));
    header.removeFromRight (8);
    auto skinArea = header.removeFromRight (170).withSizeKeepingCentre (170, 28).translated (0, -9);
    skinBox.setBounds (skinArea);
    header.removeFromRight (12);
    auto presetArea = header.withSizeKeepingCentre (jmin (620, header.getWidth()), 30).translated (0, -9);
    voicesLabel.setBounds (presetArea.translated (0, 31).withHeight (16));
    prevButton.setBounds (presetArea.removeFromLeft (30));
    presetArea.removeFromLeft (4);
    foldersButton.setBounds (presetArea.removeFromRight (82));
    presetArea.removeFromRight (4);
    initButton.setBounds (presetArea.removeFromRight (52));
    presetArea.removeFromRight (4);
    saveButton.setBounds (presetArea.removeFromRight (56));
    presetArea.removeFromRight (4);
    loadButton.setBounds (presetArea.removeFromRight (56));
    presetArea.removeFromRight (4);
    nextButton.setBounds (presetArea.removeFromRight (30));
    presetArea.removeFromRight (4);
    presetBox.setBounds (presetArea);

    analyzer->setBounds (r.removeFromBottom (kAnalyzerH).reduced (8, 2));
    auto bar = r.removeFromBottom (kChipsH).reduced (14, 2);
    bar.removeFromLeft (114);
    const int w = bar.getWidth() / chips.size();
    for (auto* c : chips) c->setBounds (bar.removeFromLeft (w));
    r.removeFromTop (10);
    tabs.setBounds (r.reduced (4, 0));
}

void ResonatorEditor::timerCallback()
{
    animT += 1.0 / 30.0;
    repaint (0, 0, 340, kHeaderH);
    if (auto* c = tabs.getCurrentContentComponent())
        if (c->getAlpha() < 1.0f) c->setAlpha (jmin (1.0f, c->getAlpha() + 0.16f));
    const int tab = tabs.getCurrentTabIndex();
    if (tab == 0)
    {
        for (auto* s : surfaces) s->tick (animT);
        // knobs that do nothing in the current mode fade back
        for (auto* k : oscKnobs)
        {
            const int rid = k->paramIndex();
            const int o = (rid - osca_on) / kOscStride, local = rid - o * kOscStride;
            const bool scan = (int) proc.value (osca_mode + o * kOscStride) == 1;
            const bool orbitOnly = local == osca_ru || local == osca_rv || local == osca_ratio || local == osca_k1 || local == osca_k2;
            const bool scanOnly = local == osca_cover || local == osca_pos;
            const float want = (scan && orbitOnly) || (! scan && scanOnly) ? 0.38f : 1.0f;
            if (std::abs (k->getAlpha() - want) > 0.01f) k->setAlpha (k->getAlpha() + (want - k->getAlpha()) * 0.3f);
        }
    }
    if (matrixView != nullptr) matrixView->time = animT;
    if (tab >= 0 && tab < (int) live.size())
        for (auto* c : live[(size_t) tab]) if (c != nullptr && c->isShowing() && dynamic_cast<SurfaceView*> (c) == nullptr) c->repaint();
    // knobs: modulation rings (live dot), and the glow while a source is dragged
    const bool dragging = isDragAndDropActive();
    if (tab >= 0 && tab < (int) knobs.size())
        for (auto* k : knobs[(size_t) tab])
            if (dragging || ! proc.slotsFor (k->paramIndex()).isEmpty()) k->repaint();
    masterKnob->repaint();
    for (auto* c : chips) c->repaint();
    voicesLabel.setText (String (proc.synth.activeVoices.load()) + " voices  -  " + proc.currentPresetName, dontSendNotification);
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------
void ResonatorEditor::refreshPresetMenu()
{
    presetBox.clear (dontSendNotification);
    auto factory = ResonatorProcessor::factoryPresetNames();
    // factory presets grouped by category ("Bass - Name" -> Bass submenu); the list is sorted, so prev / next walk it in order
    auto* root = presetBox.getRootMenu();
    root->addSectionHeader ("Factory (" + String (factory.size()) + ")");
    for (int i = 0; i < factory.size();)
    {
        const auto cat = factory[i].upToFirstOccurrenceOf (" - ", false, false);
        PopupMenu sub;
        int count = 0;
        for (; i < factory.size() && factory[i].upToFirstOccurrenceOf (" - ", false, false) == cat; ++i, ++count)
            sub.addItem (1 + i, factory[i].fromFirstOccurrenceOf (" - ", false, false));
        root->addSubMenu (cat + "  (" + String (count) + ")", sub);
    }
    userPresets = proc.listPresets();
    if (! userPresets.isEmpty())
    {
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < userPresets.size(); ++i) presetBox.addItem (userPresets[i].getFileNameWithoutExtension(), 1000 + i);
    }
    presetBox.setText (proc.currentPresetName, dontSendNotification);
}

void ResonatorEditor::choosePreset (int id)
{
    if (id <= 0) return;
    if (id < 1000) proc.loadFactoryPreset (id - 1);
    else if (id - 1000 < userPresets.size()) proc.loadPreset (userPresets[id - 1000]);
}

void ResonatorEditor::savePresetDialog()
{
    const auto dir = proc.folders().saveFolder();
    chooser = std::make_unique<FileChooser> ("Save preset", dir.getChildFile (proc.currentPresetName + ".cyrpreset"), "*.cyrpreset");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                          [this] (const FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f == File()) return;
        // saving somewhere else makes that the new save folder
        if (f.getParentDirectory() != proc.folders().saveFolder()) proc.folders().setSaveFolder (f.getParentDirectory());
        proc.savePreset (f.getFileNameWithoutExtension());
        refreshPresetMenu();
    });
}

void ResonatorEditor::loadPresetDialog()
{
    chooser = std::make_unique<FileChooser> ("Open a preset", proc.folders().openFolder(), "*.cyrpreset");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        if (proc.loadPreset (f)) presetBox.setText (proc.currentPresetName, dontSendNotification);
    });
}

void ResonatorEditor::foldersMenu()
{
    auto& pf = proc.folders();
    PopupMenu m;
    m.addSectionHeader ("Open presets from:  " + pf.openFolder().getFullPathName());
    m.addItem (1, "Choose the folder to open presets from...");
    m.addItem (3, "Show this folder");
    m.addSectionHeader ("Save presets to:  " + pf.saveFolder().getFullPathName());
    m.addItem (2, "Choose the folder to save presets to...");
    m.addItem (4, "Show this folder");
    m.addSeparator();
    m.addItem (5, "Use the default folders (Documents / Calabi Yau / Calabi-Yau Resonator / Presets)");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&foldersButton), [this] (int r)
    {
        auto& f = proc.folders();
        if (r == 1 || r == 2)
        {
            chooser = std::make_unique<FileChooser> (r == 1 ? "Folder to open presets from" : "Folder to save presets to",
                                                     r == 1 ? f.openFolder() : f.saveFolder());
            chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this, r] (const FileChooser& fc)
            {
                const auto dir = fc.getResult();
                if (! dir.isDirectory()) return;
                if (r == 1) proc.folders().setOpenFolder (dir); else proc.folders().setSaveFolder (dir);
                refreshPresetMenu();
            });
        }
        else if (r == 3) f.openFolder().revealToUser();
        else if (r == 4) f.saveFolder().revealToUser();
        else if (r == 5) { f.useDefaults(); refreshPresetMenu(); }
    });
}
