#include "PluginEditor.h"
#include "BinaryData.h"
#include <map>

using namespace juce;
using namespace pg;
using namespace pg::ui;

namespace
{
// A page is a plain component whose children are laid out by a lambda.
struct Page : public Component
{
    OwnedArray<Component> items;
    std::function<void (Rectangle<int>)> layout;
    template <typename T> T* add (T* c) { items.add (c); addAndMakeVisible (c); return c; }
    void resized() override { if (layout) layout (getLocalBounds().reduced (6)); }
};

// Splits an area into n equal parts along x (or y)
std::vector<Rectangle<int>> split (Rectangle<int> r, std::initializer_list<float> weights, bool horizontal = true)
{
    float total = 0.0f;
    for (auto w : weights) total += w;
    std::vector<Rectangle<int>> out;
    const int size = horizontal ? r.getWidth() : r.getHeight();
    int used = 0, i = 0;
    for (auto w : weights)
    {
        const int s = (++i == (int) weights.size()) ? size - used : (int) ((float) size * w / total);
        out.push_back (horizontal ? r.removeFromLeft (s) : r.removeFromTop (s));
        used += s;
    }
    return out;
}

StringArray items (std::initializer_list<const char*> list)
{
    StringArray a;
    for (auto* s : list) a.add (s);
    return a;
}

TextButton* makeButton (Page& page, const String& text, std::function<void()> fn, const String& tip = {})
{
    auto* b = page.add (new TextButton (text));
    b->onClick = std::move (fn);
    b->setTooltip (tip);
    return b;
}

// The FX cascade drawn as a signal flow: IN > slot > slot > ... > OUT
struct FxChainView : public Component
{
    FxChainView (APVTS& s, bool mixbus = false) : st (s), mb (mixbus) {}
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
        const float gap = 14.0f;
        const float w = (inner.getWidth() - gap * (float) (n - 1)) / (float) n;
        auto box = inner.withHeight (jmin (46.0f, inner.getHeight())).withY (inner.getCentreY() - 23.0f).withWidth (w);
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
            g.setFont (fonts::label (io ? 11.0f : 9.5f));
            g.drawFittedText (name.toUpperCase() + (io || on ? "" : "\n(off)"), box.reduced (2.0f).toNearestInt(), Justification::centred, 2, 0.6f);
            if (i < n - 1)
            {
                const float ax = box.getRight() + 2.0f, ay = box.getCentreY();
                Path a;
                a.addArrow ({ ax, ay, ax + gap - 4.0f, ay }, 1.5f, 7.0f, 6.0f);
                g.setColour (cosmos::orchid);
                g.fillPath (a);
            }
            box.translate (w + gap, 0.0f);
        }
    }
};

// ROUTING page diagram: sources -> distortion -> filter (or straight) -> amp -> FX buses -> master,
// with dots flowing along the active wires
struct RoutingView : public Component
{
    explicit RoutingView (APVTS& s) : st (s) {}
    APVTS& st;
    float v (const char* id) const { return st.getRawParameterValue (id)->load(); }
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (4.0f);
        auto inner = brand::chamferFrame (g, r, 16.0f, 5.0f, cosmos::voidC.withAlpha (0.9f), cosmos::electric, true).reduced (16.0f, 12.0f);
        g.setColour (colours::textDim);
        g.setFont (fonts::label (9.5f));
        g.drawText ("SIGNAL FLOW  -  ONE VOICE  >  WHOLE SYNTH", inner.removeFromTop (16.0f), Justification::centredLeft);
        const float t = (float) (Time::getMillisecondCounterHiRes() * 0.001);
        struct Src { const char* name; bool on; bool filt; };
        const Src srcs[] = {
            { "WT A", v ("wtA_on") > 0.5f, v ("rt_flt_a") > 0.5f }, { "WT B", v ("wtB_on") > 0.5f, v ("rt_flt_b") > 0.5f },
            { "WT C", v ("wtC_on") > 0.5f, v ("rt_flt_c") > 0.5f }, { "SPECTRAL", v ("sp_on") > 0.5f, v ("rt_flt_sp") > 0.5f },
            { "GRAIN", v ("gr_on") > 0.5f, v ("rt_flt_gr") > 0.5f }, { "PLAITS", v ("pl_on") > 0.5f, v ("rt_flt_pl") > 0.5f },
            { "SUB+NOISE", v ("sub_level") > 0.0f || v ("noise_level") > 0.0f, v ("rt_flt_sub") > 0.5f }, { "ONE-SHOT", v ("smp_on") > 0.5f, v ("rt_flt_smp") > 0.5f } };
        const float colW = inner.getWidth() / 7.0f;
        const float rowH = inner.getHeight() / 8.0f;
        auto box = [&] (Rectangle<float> b, const String& txt, bool on, Colour c)
        {
            g.setColour (on ? cosmos::deepSpace : cosmos::night);
            g.fillRoundedRectangle (b, 5.0f);
            g.setColour (on ? c : cosmos::steel.withAlpha (0.4f));
            g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, on ? 1.6f : 1.0f);
            g.setColour (on ? cosmos::starlight : cosmos::steel);
            g.setFont (fonts::label (jmin (10.5f, b.getHeight() * 0.42f)));
            g.drawFittedText (txt, b.reduced (3.0f).toNearestInt(), Justification::centred, 2, 0.6f);
        };
        auto wire = [&] (Point<float> a, Point<float> b, bool on, Colour c, float phase)
        {
            Path p; p.startNewSubPath (a);
            p.cubicTo ({ (a.x + b.x) * 0.5f, a.y }, { (a.x + b.x) * 0.5f, b.y }, b);
            g.setColour ((on ? c : cosmos::steel).withAlpha (on ? 0.55f : 0.15f));
            g.strokePath (p, PathStrokeType (on ? 1.8f : 1.0f));
            if (! on) return;
            const float len = p.getLength();
            for (int k = 0; k < 3; ++k)
            {
                const float f = std::fmod (t * 0.45f + phase + (float) k / 3.0f, 1.0f);
                const auto pt = p.getPointAlongPath (f * len);
                g.setColour (c.withAlpha (0.35f)); g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (pt));
                g.setColour (cosmos::starlight); g.fillEllipse (Rectangle<float> (3.5f, 3.5f).withCentre (pt));
            }
        };
        const bool post = v ("bite_post") > 0.5f;
        auto col = [&] (int c) { return inner.getX() + colW * (float) c; };
        const float midY = inner.getCentreY();
        auto bDist = Rectangle<float> (col (post ? 3 : 2) + 8, midY - 26 - rowH * 1.5f, colW - 16, 52);
        auto bFilt = Rectangle<float> (col (post ? 2 : 3) + 8, midY - 26 - rowH * 1.5f, colW - 16, 52);
        auto bAmp = Rectangle<float> (col (4) + 8, midY - 26, colW - 16, 52);
        const bool mbFirst = v ("rt_order") > 0.5f;
        auto bRack = Rectangle<float> (col (mbFirst ? 6 : 5) + 8, midY - 26, colW - 16, 52);
        auto bMix = Rectangle<float> (col (mbFirst ? 5 : 6) + 8, midY - 26, colW - 16, 52);
        // sources
        for (int i = 0; i < 8; ++i)
        {
            auto b = Rectangle<float> (col (0) + 4, inner.getY() + rowH * (float) i + 3, colW * 1.2f - 8, rowH - 6);
            box (b, srcs[i].name, srcs[i].on, cosmos::neon);
            const auto from = Point<float> (b.getRight(), b.getCentreY());
            if (srcs[i].filt) wire (from, { (post ? bFilt : bDist).getX(), (post ? bFilt : bDist).getCentreY() }, srcs[i].on, cosmos::neon, (float) i * 0.13f);
            else wire (from, { bAmp.getX(), bAmp.getBottom() - 8.0f }, srcs[i].on, cosmos::gold, (float) i * 0.13f);
        }
        const bool any = true;
        auto first = post ? bFilt : bDist, second = post ? bDist : bFilt;
        wire ({ first.getRight(), first.getCentreY() }, { second.getX(), second.getCentreY() }, any, cosmos::orchid, 0.2f);
        wire ({ second.getRight(), second.getCentreY() }, { bAmp.getX(), bAmp.getY() + 10.0f }, any, cosmos::orchid, 0.4f);
        auto bFirstBus = mbFirst ? bMix : bRack, bSecondBus = mbFirst ? bRack : bMix;
        wire ({ bAmp.getRight(), bAmp.getCentreY() }, { bFirstBus.getX(), bFirstBus.getCentreY() }, any, cosmos::fuchsia, 0.1f);
        wire ({ bFirstBus.getRight(), bFirstBus.getCentreY() }, { bSecondBus.getX(), bSecondBus.getCentreY() }, any, cosmos::fuchsia, 0.3f);
        box (bDist, "MULTI-\nDISTORTION", true, colours::warn);
        box (bFilt, "FILTER", true, cosmos::orchid);
        box (bAmp, "AMP\nENVELOPE", true, cosmos::neon);
        box (bRack, "FX RACK", true, cosmos::fuchsia);
        box (bMix, "MIXBUS", true, cosmos::fuchsia);
        g.setColour (colours::textDim);
        g.setFont (fonts::label (9.0f));
        g.drawText ("> MASTER  dBTP / LUFS", Rectangle<float> (bSecondBus.getX() - 20, bSecondBus.getBottom() + 6, colW + 40, 14), Justification::centred);
        g.drawText ("GOLD WIRES SKIP THE FILTER", Rectangle<float> (col (1), inner.getBottom() - 14, colW * 3, 14), Justification::centredLeft);
    }
};

Label* makeLabel (Page& page, const String& text, float size = 12.0f, Colour c = colours::textDim)
{
    auto* l = page.add (new Label ({}, text));
    l->setFont (FontOptions (size));
    l->setColour (Label::textColourId, c);
    l->setJustificationType (Justification::centredLeft);
    return l;
}
} // namespace

SynthgularityEditor::SynthgularityEditor (SynthgularityProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    skinIndex() = proc.uiSkin;
    setLookAndFeel (&lnf);
    auto& st = proc.apvts;
   #if PG_HAS_LOGO
    logo = ImageCache::getFromMemory (BinaryData::logocalabiyau_png, BinaryData::logocalabiyau_pngSize);
   #endif

    // ---------------- header ----------------
    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this] { choosePreset (presetBox.getSelectedId()); };
    for (auto* b : { &prevButton, &nextButton, &saveButton, &initButton }) addAndMakeVisible (*b);
    prevButton.onClick = [this]
    {
        const int i = presetBox.getSelectedItemIndex();
        if (i > 0) presetBox.setSelectedItemIndex (i - 1);
    };
    nextButton.onClick = [this]
    {
        const int i = presetBox.getSelectedItemIndex();
        if (i < presetBox.getNumItems() - 1) presetBox.setSelectedItemIndex (i + 1);
    };
    saveButton.onClick = [this] { savePresetDialog(); };
    initButton.onClick = [this] { proc.initPreset(); presetBox.setText ("Init", dontSendNotification); };
    master = std::make_unique<MasterSection> (proc, st);
    addAndMakeVisible (*master);
    const pg::ui::ScopeFeed feed { &proc.meter, proc.scopeL.data(), proc.scopeR.data(), &proc.scopePos, SynthgularityProcessor::kScopeRing };
    gonio = std::make_unique<pg::ui::GonioMini> (feed);
    addAndMakeVisible (*gonio);
    // modulation sources: drag a chip onto any knob, on any page
    {
        static const char* shortNames[] = { "-", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "CHAOS", "LORENZ X", "LORENZ Y", "LORENZ Z",
                                            "SEQ 1", "SEQ 2", "SEQ 3", "SEQ 4", "SEQ 5", "SEQ 6", "SEQ 7", "SEQ 8",
                                            "MENV 1", "F.ENV", "A.ENV", "VEL", "NOTE", "WHEEL", "AFTER", "BEND", "RAND", "MENV 2", "MENV 3", "MENV 4" };
        static const int order[] = { MS_Lfo1, MS_Lfo2, MS_Lfo3, MS_Lfo4, MS_Chaos1, MS_Chaos2, MS_Chaos2Y, MS_Chaos2Z,
                                     MS_ModEnv, MS_ModEnv2, MS_ModEnv3, MS_ModEnv4, MS_FiltEnv, MS_AmpEnv,
                                     MS_Seq1, MS_Seq2, MS_Seq3, MS_Seq4, MS_Seq5, MS_Seq6, MS_Seq7, MS_Seq8,
                                     MS_Velocity, MS_Note, MS_ModWheel, MS_Aftertouch, MS_PitchBend, MS_Random };
        for (int src : order)
        {
            const bool bip = src == MS_Lfo1 || src == MS_Lfo2 || src == MS_Lfo3 || src == MS_Lfo4 || (src >= MS_Chaos1 && src <= MS_Chaos2Z)
                             || src == MS_Note || src == MS_PitchBend || src == MS_Random;
            auto* c = chips.add (new pg::ui::SourceChip (src, shortNames[src], colourOf (src),
                                                         [this, src] { return proc.synth.liveSrc[(size_t) src].load (std::memory_order_relaxed); }, bip));
            addAndMakeVisible (c);
        }
    }
    voicesLabel.setColour (Label::textColourId, colours::textDim);
    voicesLabel.setJustificationType (Justification::centred);
    addAndMakeVisible (voicesLabel);
    refreshPresetMenu();

    tabs.setTabBarDepth (36);
    tabs.setOutline (0);
    tabs.setColour (TabbedComponent::backgroundColourId, Colours::transparentBlack);
    tabs.setColour (TabbedComponent::outlineColourId, Colours::transparentBlack);
    addAndMakeVisible (tabs);
    logoGrid = ImageCache::getFromMemory (BinaryData::bglogogrid_jpg, BinaryData::bglogogrid_jpgSize);
    isologo  = ImageCache::getFromMemory (BinaryData::isologo_png, BinaryData::isologo_pngSize);
    addChildComponent (about);

    auto addLive = [this] (std::initializer_list<Component*> cs) { live.emplace_back (cs); };

    // ---------------- OSC: three wavetable oscillators ----------------
    {
        auto* page = new Page(); pages.add (page);
        Panel* wt[3];
        const char* pre[] = { "wtA_", "wtB_", "wtC_" };
        const char* names[] = { "Wavetable A", "Wavetable B", "Wavetable C" };
        const Colour cols[] = { colours::accent, colours::accent2, cosmos::gold };
        for (int o = 0; o < 3; ++o)
        {
            const String x = pre[o];
            StringArray it;
            for (auto sfx : { "on:On", "table:Table", "pos:Position", "rphase:Free",
                              "warp:Warp 1", "wamt:Warp 1 Amt", "warp2:Warp 2", "wamt2:Warp 2 Amt",
                              "oct:Octave", "semi:Semi", "fine:Fine", "phase:Phase",
                              "unison:Unison", "detune:Detune", "spread:Spread", "level:Level", "pan:Pan" })
                it.add (x + sfx);
            wt[o] = page->add (new Panel (st, names[o], it, 4, cols[o]));
            wt[o]->extraHeight = 110;
            auto* view = page->add (new WavetableView (proc, o));
            tableViews[o] = view;
            auto* load = makeButton (*page, "LOAD WAV", [this, o] { loadWavetableDialog (o); },
                                     "Load a WAV/AIFF/FLAC/MP3 as a wavetable (2048-sample frames)");
            wt[o]->layoutExtra = [view, load, panel = wt[o]] (Rectangle<int> r)
            {
                auto pr = r + panel->getPosition();
                load->setBounds (pr.removeFromBottom (26).removeFromRight (96).reduced (2, 0));
                pr.removeFromBottom (2);
                view->setBounds (pr.reduced (0, 2));
            };
            view->toFront (false); load->toFront (false);
        }
        page->layout = [=] (Rectangle<int> r)
        {
            auto c = split (r, { 1.0f, 1.0f, 1.0f });
            for (int o = 0; o < 3; ++o) wt[o]->setBounds (c[(size_t) o]);
        };
        tabs.addTab ("OSC", Colours::transparentBlack, page, false);
        addLive ({ tableViews[0], tableViews[1], tableViews[2] });
    }

    // ---------------- SPECTRAL ----------------
    {
        auto* page = new Page(); pages.add (page);
        auto* view = page->add (new SpectralView (proc));
        auto loader = [this] (const File& f, String& e) { return proc.loadSpectralFile (f, e); };
        view->onFile = [this, loader] (const File& f) { String err; if (! loader (f, err)) showError (err); };
        auto* load = makeButton (*page, "LOAD AUDIO", [this, loader] { loadFileDialog ("Analyse an audio file for the spectral oscillator", loader); },
                                 "Analyse a WAV/AIFF/FLAC/MP3: pitch is detected and 64 spectral frames are captured across the file");
        auto* info = makeLabel (*page, "64 sine partials re-synthesised from a spectral table. POSITION / SCAN move through time, SHIFT moves the formants, "
                                       "STRETCH makes it inharmonic (bells, metal). Drop an audio file on the display to analyse it.", 12.0f);
        auto* panel = page->add (new Panel (st, "Spectral Oscillator", items ({ "sp_on:On", "sp_sound:Sound", "sp_pos:Position", "sp_scan:Scan",
                                           "sp_freeze:Freeze", "sp_partials:Partials", "sp_tilt:Tilt", "sp_stretch:Stretch", "sp_shift:Shift",
                                           "sp_oddeven:Odd/Even", "sp_blur:Blur", "sp_shimmer:Shimmer", "sp_width:Width", "sp_oct:Octave",
                                           "sp_semi:Semi", "sp_fine:Fine", "sp_level:Level", "sp_pan:Pan", "sp_warp:Spectral Warp", "sp_wamt:Warp Amt",
                                           "sp_pdecay:Partial Decay", "sp_jitter:Jitter" }), 11, cosmos::neon));
        page->layout = [=] (Rectangle<int> r)
        {
            auto top = r.removeFromTop (r.getHeight() * 44 / 100);
            auto bar = top.removeFromBottom (30);
            view->setBounds (top.reduced (3));
            load->setBounds (bar.removeFromRight (130).reduced (3, 3));
            info->setBounds (bar.reduced (6, 0));
            panel->setBounds (r);
        };
        tabs.addTab ("SPECTRAL", Colours::transparentBlack, page, false);
        addLive ({ view });
    }

    // ---------------- GRAIN ----------------
    {
        auto* page = new Page(); pages.add (page);
        sampleView = page->add (new SampleView (proc));
        sampleView->onFile = [this] (const File& f)
        {
            String err;
            if (! proc.loadSample (f, err)) showError (err);
        };
        auto* load = makeButton (*page, "LOAD SAMPLE", [this] { loadSampleDialog(); }, "WAV, AIFF, FLAC, MP3 or OGG");
        auto* info = makeLabel (*page, "Cloud: grains at DENSITY per second, pitch follows the keys.   "
                                       "Pitched: one grain per period of the note, so the sample keeps its formants. Gold lines = region.", 12.0f);
        auto* panel = page->add (new Panel (st, "Granular Oscillator", items ({ "gr_on:On", "gr_mode:Mode", "gr_pos:Position", "gr_scan:Scan",
                                           "gr_spray:Spray", "gr_size:Size", "gr_density:Density", "gr_window:Window",
                                           "gr_pitch:Pitch", "gr_prand:Pitch Rnd", "gr_keytrack:Keytrack", "gr_root:Root Note",
                                           "gr_reverse:Reverse", "gr_stereo:Stereo", "gr_level:Level", "gr_pan:Pan", "gr_freeze:Freeze" }), 9));
        auto* texture = page->add (new Panel (st, "Texture", items ({ "gr_start:Region In", "gr_end:Region Out", "gr_jitter:Jitter",
                                             "gr_octrand:Octave Rnd", "gr_formant:Formant", "gr_streams:Streams", "gr_sdetune:Stream Det",
                                             "gr_tilt:Win Tilt" }), 8, cosmos::orchid));
        auto* gm1p = page->add (new Panel (st, "Grain Mod 1  (LFO)", items ({ "gm1_shape:Shape", "gm1_rate:Rate", "gm1_pos:> Position",
                                          "gm1_size:> Size", "gm1_density:> Density" }), 5, cosmos::fuchsia));
        auto* gm2p = page->add (new Panel (st, "Grain Mod 2  (Drift)", items ({ "gm2_rate:Rate", "gm2_smooth:Smooth", "gm2_pitch:> Pitch",
                                          "gm2_spray:> Spray", "gm2_stereo:> Stereo" }), 5, cosmos::gold));
        page->layout = [=] (Rectangle<int> r)
        {
            auto top = r.removeFromTop (r.getHeight() * 32 / 100);
            auto bar = top.removeFromBottom (30);
            sampleView->setBounds (top.reduced (3));
            load->setBounds (bar.removeFromRight (130).reduced (3, 3));
            info->setBounds (bar.reduced (6, 0));
            auto rows = split (r, { 1.55f, 1.0f }, false);
            panel->setBounds (rows[0]);
            auto bottom = split (rows[1], { 1.6f, 1.0f, 1.0f });
            texture->setBounds (bottom[0]); gm1p->setBounds (bottom[1]); gm2p->setBounds (bottom[2]);
        };
        tabs.addTab ("GRAIN", Colours::transparentBlack, page, false);
        addLive ({ sampleView });
    }

    // ---------------- SOURCES: sub, noise, one-shot sampler, Plaits, FM matrix ----------------
    {
        auto* page = new Page(); pages.add (page);
        auto* sub = page->add (new Panel (st, "Sub + Noise", items ({ "sub_wave:Sub Wave", "sub_oct:Sub Oct", "sub_level:Sub Level", "sub_phase:Sub Phase",
                                          "noise_type:Noise", "noise_level:Noise Lvl" }), 3));
        auto* smp = page->add (new Panel (st, "One-Shot Sampler  (clicks / transients / noises)", items ({ "smp_on:On", "smp_sound:Sound", "smp_pitch:Pitch",
                                          "smp_key:Keytrack", "smp_start:Start", "smp_attack:Attack", "smp_decay:Decay", "smp_level:Level", "smp_pan:Pan" }), 9, cosmos::orchid));
        smp->extraHeight = 70;
        auto* shotView = page->add (new ShotView (proc));
        auto shotLoader = [this] (const File& f, String& e) { return proc.loadShot (f, e); };
        shotView->onFile = [this, shotLoader] (const File& f) { String err; if (! shotLoader (f, err)) showError (err); };
        auto* shotLoad = makeButton (*page, "LOAD", [this, shotLoader] { loadFileDialog ("Load a one-shot sample", shotLoader); }, "Load your own click or one-shot (up to 20 s)");
        smp->layoutExtra = [shotView, shotLoad, smp] (Rectangle<int> r)
        {
            r += smp->getPosition();
            shotLoad->setBounds (r.removeFromRight (70).withSizeKeepingCentre (64, 26));
            shotView->setBounds (r.reduced (0, 2));
        };
        shotView->toFront (false); shotLoad->toFront (false);
        auto* pl = page->add (new Panel (st, "Plaits (Mutable Instruments)", items ({ "pl_on:On", "pl_engine:Engine", "pl_oct:Octave", "pl_level:Level",
                                          "pl_harm:Harmonics", "pl_timbre:Timbre", "pl_morph:Morph", "pl_pan:Pan", "pl_lpg:LPG", "pl_decay:Decay", "pl_colour:Colour" }), 4, colours::warn));
        StringArray fmItems; fmItems.add ("fm_amount:Amount");
        const char* fmS[] = { "a", "b", "c", "s", "g", "n" }; const char* fmSN[] = { "A", "B", "C", "Spec", "Grain", "Noise" };
        const char* fmD[] = { "a", "b", "c", "s" }; const char* fmDN[] = { "A", "B", "C", "Spec" };
        for (int si = 0; si < 6; ++si) for (int di = 0; di < 4; ++di)
            fmItems.add (String ("fm_") + fmS[si] + "_" + fmD[di] + ":" + fmSN[si] + ">" + fmDN[di]);
        auto* fm = page->add (new Panel (st, "Mod Matrix  FM / PD / AM  (row > column)", fmItems, 9, colours::accent2));
        {
            ParamControl* types[4];
            const char* tid[] = { "fm_type_a", "fm_type_b", "fm_type_c", "fm_type_s" };
            const char* tname[] = { "Into A", "Into B", "Into C", "Into Spectral" };
            for (int t = 0; t < 4; ++t) types[t] = page->add (new ParamControl (st, tid[t], tname[t]));
            fm->extraHeight = 44;
            fm->layoutExtra = [types, panel = fm] (Rectangle<int> r)
            {
                r += panel->getPosition();
                const int w = r.getWidth() / 4;
                for (int t = 0; t < 4; ++t) types[t]->setBounds (r.removeFromLeft (w));
            };
        }
        page->layout = [=] (Rectangle<int> r)
        {
            auto rows = split (r, { 0.8f, 1.2f }, false);
            auto top = split (rows[0], { 0.75f, 2.0f });
            sub->setBounds (top[0]); smp->setBounds (top[1]);
            auto bottom = split (rows[1], { 0.85f, 1.75f });
            pl->setBounds (bottom[0]); fm->setBounds (bottom[1]);
        };
        tabs.addTab ("SOURCES", Colours::transparentBlack, page, false);
        addLive ({ shotView });
    }
    // ---------------- FILTER / AMP ----------------
    {
        auto* page = new Page(); pages.add (page);
        auto* flt = page->add (new Panel (st, "Filter", items ({ "flt_type:Type", "flt_model:Model", "flt_slope:Slope", "flt_drive:Drive",
                                         "flt_cutoff:Cutoff", "flt_res:Resonance", "flt_env:Env Amt", "flt_key:Keytrack" }), 4));
        auto* bite = page->add (new Panel (st, "MultiDistortion", items ({ "bite_drive:Drive", "bite_asym:Asymmetry", "bite_rect:Rectify",
                                          "bite_fold:Fold", "bite_soft:Saturate", "bite_crush:Crush", "bite_tone:Tone", "bite_mix:Mix",
                                          "bite_post:Post Filter" }), 5, colours::warn));
        Panel* env[2];
        const char* envNames[] = { "Amp Envelope", "Filter Envelope" };
        const char* envPre[] = { "amp_", "fenv_" };
        const Colour envCol[] = { cosmos::neon, cosmos::orchid };
        auto addEnv = [&] (const String& title, const String& pre, Colour c) -> Panel*
        {
            auto* pnl = page->add (new Panel (st, title, StringArray { pre + "a:Attack", pre + "d:Decay", pre + "s:Sustain", pre + "r:Release" }, 4, c));
            pnl->extraHeight = 92;
            auto* view = page->add (new EnvelopeView (st, pre, c));
            envViews.add (view);
            pnl->layoutExtra = [view, pnl] (Rectangle<int> r) { view->setBounds (r + pnl->getPosition()); };
            return pnl;
        };
        for (int e = 0; e < 2; ++e) env[e] = addEnv (envNames[e], envPre[e], envCol[e]);

        // Mod Envelope 1-4: one panel per envelope, tab buttons switch between them
        std::array<Panel*, 4> modEnv;
        const char* modPre[] = { "menv_", "menv2_", "menv3_", "menv4_" };
        for (int e = 0; e < 4; ++e)
            modEnv[(size_t) e] = addEnv ("Mod Envelope", modPre[e], e % 2 ? cosmos::gold : cosmos::fuchsia);
        std::array<TextButton*, 4> modTabs;
        for (int e = 0; e < 4; ++e)
        {
            modTabs[(size_t) e] = makeButton (*page, String (e + 1), {}, "Mod Envelope " + String (e + 1) + " (source: Mod Env " + String (e + 1) + ")");
            modTabs[(size_t) e]->setClickingTogglesState (false);
        }
        auto showModEnv = [modEnv, modTabs, this] (int which)
        {
            for (int e = 0; e < 4; ++e)
            {
                modEnv[(size_t) e]->setVisible (e == which);
                modTabs[(size_t) e]->setToggleState (e == which, dontSendNotification);
                envViews[2 + e]->setVisible (e == which);
            }
        };
        for (int e = 0; e < 4; ++e) modTabs[(size_t) e]->onClick = [showModEnv, e] { showModEnv (e); };
        showModEnv (0);
        auto* voice = page->add (new Panel (st, "Voice", items ({ "voice_mode:Mode", "polyphony:Voices", "glide:Glide", "bend_range:Bend" }), 2));
        page->layout = [=] (Rectangle<int> r)
        {
            auto rows = split (r, { 1.0f, 1.25f }, false);
            auto a = split (rows[0], { 0.9f, 1.15f, 0.55f });
            flt->setBounds (a[0]); bite->setBounds (a[1]); voice->setBounds (a[2]);
            auto b = split (rows[1], { 1.0f, 1.0f, 1.0f });
            for (int e = 0; e < 2; ++e) env[e]->setBounds (b[(size_t) e]);
            for (int e = 0; e < 4; ++e) modEnv[(size_t) e]->setBounds (b[2]);
            auto tabsArea = b[2].reduced (14, 0).withTrimmedTop (8).withHeight (24).removeFromRight (132);
            for (int e = 0; e < 4; ++e)
            {
                modTabs[(size_t) e]->setBounds (tabsArea.removeFromLeft (33).reduced (2, 0));
                modTabs[(size_t) e]->toFront (false);
            }
        };
        tabs.addTab ("FILTER / AMP", Colours::transparentBlack, page, false);
        { std::vector<Component*> v; for (auto* e : envViews) v.push_back (e); live.push_back (v); }
    }

    // ---------------- MOD ----------------
    {
        auto* page = new Page(); pages.add (page);
        Panel* lfo[4];
        for (int i = 0; i < 4; ++i)
        {
            const String n (i + 1);
            StringArray it;
            for (auto sfx : { "_shape:Shape", "_retrig:Retrig", "_sync:Sync", "_rate:Rate", "_div:Division", "_phase:Phase" })
                it.add ("lfo" + n + sfx);
            lfo[i] = page->add (new Panel (st, "LFO " + n, it, 3, i % 2 ? colours::accent2 : colours::accent));
        }
        auto* chaos1 = page->add (new Panel (st, "Chaos 1", items ({ "chaos1_type:Attractor", "chaos1_rate:Rate", "chaos1_smooth:Smooth", "chaos1_amt:Chaos" }), 4, colours::warn));
        auto* chaos2 = page->add (new Panel (st, "Chaos 2  Lorenz  (X / Y / Z)", items ({ "chaos2_rate:Rate", "chaos2_smooth:Smooth", "chaos2_rho:Rho",
                                             "chaos2_sigma:Sigma", "chaos2_beta:Beta", "chaos2_sync:Retrig" }), 6, cosmos::fuchsia));
        auto* tip = makeLabel (*page, "LFO, Chaos and SEQ lanes do nothing until you route them on the MATRIX page (Chaos 2 has three outputs: Lorenz X, Y, Z). "
                                      "Retrig Note = one LFO per voice, restarted on each note; Free = one shared LFO.", 12.0f);
        page->layout = [=] (Rectangle<int> r)
        {
            tip->setBounds (r.removeFromBottom (26).reduced (6, 0));
            auto rows = split (r, { 1.0f, 1.0f, 0.7f }, false);
            auto a = split (rows[0], { 1.0f, 1.0f });
            lfo[0]->setBounds (a[0]); lfo[1]->setBounds (a[1]);
            auto b = split (rows[1], { 1.0f, 1.0f });
            lfo[2]->setBounds (b[0]); lfo[3]->setBounds (b[1]);
            auto c = split (rows[2], { 0.8f, 1.2f });
            chaos1->setBounds (c[0]); chaos2->setBounds (c[1]);
        };
        tabs.addTab ("MOD", Colours::transparentBlack, page, false);
        live.push_back ({});
    }

    // ---------------- MATRIX ----------------
    {
        auto* page = new Page(); pages.add (page);
        auto* hSrc = makeLabel (*page, "SOURCE", 11.5f);
        auto* hPol = makeLabel (*page, "POLARITY", 11.5f);
        auto* hDst = makeLabel (*page, "DESTINATION", 11.5f);
        auto* hAmt = makeLabel (*page, "AMOUNT", 11.5f);
        auto* hVia = makeLabel (*page, "VIA (scales the amount)", 11.5f);
        std::vector<MatrixRow*> rows;
        auto* holder = new Component();
        auto* view = page->add (new Viewport());
        view->setViewedComponent (holder, true);
        view->setScrollBarsShown (true, false);
        for (int s = 0; s < kMtxSlots; ++s) { rows.push_back (new MatrixRow (st, s)); holder->addAndMakeVisible (rows.back()); }
        for (auto* row : rows) owned.add (row);
        auto* tip = makeLabel (*page, "32 slots. Dropping a MOD SOURCES chip on a knob fills the next free one. Voice destinations follow each note; "
                                      "the others (any knob, the FX) are shared by all voices.", 12.0f);
        page->layout = [=] (Rectangle<int> r)
        {
            tip->setBounds (r.removeFromBottom (24).reduced (6, 0));
            {
                auto h = r.removeFromTop (22);
                h.removeFromLeft (30);
                const int w = h.getWidth();
                hSrc->setBounds (h.removeFromLeft (w * 19 / 100));
                h.removeFromLeft (4);
                hPol->setBounds (h.removeFromLeft (w * 11 / 100));
                h.removeFromLeft (4);
                hDst->setBounds (h.removeFromLeft (w * 25 / 100));
                h.removeFromLeft (4);
                hVia->setBounds (h.removeFromRight (w * 16 / 100));
                hAmt->setBounds (h.reduced (6, 0));
            }
            view->setBounds (r);
            const int h = 30, w = r.getWidth() - view->getScrollBarThickness();
            holder->setSize (w, h * (int) rows.size());
            for (int i = 0; i < (int) rows.size(); ++i) rows[(size_t) i]->setBounds (0, i * h, w, h);
        };
        tabs.addTab ("MATRIX", Colours::transparentBlack, page, false);
        live.push_back ({});
    }

    // ---------------- SEQ: 8 lanes, shown four at a time ----------------
    {
        auto* page = new Page(); pages.add (page);
        std::vector<Panel*> ps;
        std::vector<std::vector<TextButton*>> tools;
        for (int l = 0; l < 8; ++l)
        {
            const String n (l + 1);
            StringArray it;
            for (auto sfx : { "_on:On", "_len:Length", "_div:Division", "_dir:Direction", "_mode:Range", "_swing:Swing", "_glide:Glide",
                              "_dst:Route to matrix", "_amt:Amount" })
                it.add ("seq" + n + sfx);
            auto* pnl = page->add (new Panel (st, "Lane " + n, it, 5, l % 2 ? colours::accent2 : colours::accent));
            ps.push_back (pnl);
            auto* lane = lanes.add (new StepLaneEditor (proc, l));
            page->addAndMakeVisible (lane);
            tools.push_back ({ makeButton (*page, "RANDOM", [lane] { lane->randomize (false); }, "New random values (locked steps stay)"),
                               makeButton (*page, "MUTATE", [lane] { lane->randomize (true); }, "Small random changes"),
                               makeButton (*page, "EUCLID", [lane] { lane->euclid(); }, "Spread hits evenly; click again for one more hit"),
                               makeButton (*page, "RAMP", [lane] { lane->ramp(); }),
                               makeButton (*page, "REVERSE", [lane] { lane->transform (StepLaneEditor::Reverse); }, "Play the steps backwards"),
                               makeButton (*page, "MIRROR", [lane] { lane->transform (StepLaneEditor::Mirror); }, "Second half mirrors the first (palindrome)"),
                               makeButton (*page, "INVERT", [lane] { lane->transform (StepLaneEditor::Invert); }, "Flip every value upside down"),
                               makeButton (*page, "ROTATE <", [lane] { lane->transform (StepLaneEditor::RotateLeft); }),
                               makeButton (*page, "ROTATE >", [lane] { lane->transform (StepLaneEditor::RotateRight); }),
                               makeButton (*page, "DOUBLE", [lane] { lane->transform (StepLaneEditor::Double); }, "Pattern twice as fast"),
                               makeButton (*page, "HALF", [lane] { lane->transform (StepLaneEditor::Half); }, "Pattern twice as slow"),
                               makeButton (*page, "CLEAR", [lane] { lane->clear(); }),
                               makeButton (*page, "COPY", [lane] { lane->copyLane(); }, "Copy this lane"),
                               makeButton (*page, "PASTE", [lane] { lane->pasteLane(); }, "Paste the copied lane here") });
        }
        auto bankHolder = std::make_shared<int> (0);
        TextButton* bankBtn[2];
        auto* tip = makeLabel (*page, "Draw with the mouse, right-click locks a step (gold). ROUTE TO MATRIX sends the lane straight to a destination; the MATRIX page still works too.", 12.0f);
        auto* lanesPtr = &lanes;
        for (int b2 = 0; b2 < 2; ++b2)
        {
            bankBtn[b2] = makeButton (*page, b2 == 0 ? "LANES 1-4" : "LANES 5-8", {}, "Show lanes " + String (b2 == 0 ? "1-4" : "5-8"));
            bankBtn[b2]->onClick = [bankHolder, b2, page] { *bankHolder = b2; page->resized(); };
        }
        page->layout = [=] (Rectangle<int> r)
        {
            auto head = r.removeFromTop (30);
            for (int b2 = 0; b2 < 2; ++b2)
            {
                bankBtn[b2]->setBounds (head.removeFromLeft (120).reduced (2, 2));
                bankBtn[b2]->setToggleState (*bankHolder == b2, dontSendNotification);
            }
            tip->setBounds (head.reduced (10, 0));
            const int h = r.getHeight() / 4;
            for (int l = 0; l < 8; ++l)
            {
                const bool vis = (l / 4) == *bankHolder;
                ps[(size_t) l]->setVisible (vis);
                (*lanesPtr)[l]->setVisible (vis);
                for (auto* t : tools[(size_t) l]) t->setVisible (vis);
                if (! vis) continue;
                auto row = r.removeFromTop (h);
                ps[(size_t) l]->setBounds (row.removeFromLeft (430));
                auto tcol = row.removeFromRight (236).reduced (4, 6);
                const int cols = 3, rows = 5;
                const int bw = tcol.getWidth() / cols, bh = tcol.getHeight() / rows;
                for (int k = 0; k < (int) tools[(size_t) l].size(); ++k)
                    tools[(size_t) l][(size_t) k]->setBounds (Rectangle<int> (tcol.getX() + (k % cols) * bw, tcol.getY() + (k / cols) * bh, bw, bh).reduced (2, 2));
                (*lanesPtr)[l]->setBounds (row.reduced (6, 10));
            }
        };
        tabs.addTab ("SEQ", Colours::transparentBlack, page, false);
        { std::vector<Component*> v; for (auto* l : lanes) v.push_back (l); live.push_back (v); }
    }

    // ---------------- ARP ----------------
    {
        auto* page = new Page(); pages.add (page);
        auto* panel = page->add (new Panel (st, "Arpeggiator", items ({ "arp_on:On", "arp_mode:Mode", "arp_oct:Octaves", "arp_div:Division",
                                           "arp_len:Steps", "arp_gate:Gate", "arp_swing:Swing", "arp_latch:Latch", "arp_scale:Scale / Mode", "arp_root:Root" }), 10, colours::warn));
        arpGrid = page->add (new ArpGrid (proc));
        auto* tip = makeLabel (*page, "SCALE / MODE quantises every arp note (after PITCH) to the chosen scale or Greek mode from ROOT. "
                                      "Click ON for rests, drag PITCH and VEL, TIE glides (Legato), RATCHET plays 1-4 notes per step.", 12.0f);
        page->layout = [=] (Rectangle<int> r)
        {
            panel->setBounds (r.removeFromTop (130));
            tip->setBounds (r.removeFromBottom (26).reduced (6, 0));
            arpGrid->setBounds (r.reduced (6, 10));
        };
        tabs.addTab ("ARP", Colours::transparentBlack, page, false);
        addLive ({ arpGrid });
    }

    // ---------------- FX and MIXBUS: two racks of 10 slots in cascade ----------------
    auto buildRack = [this, &st, addLive] (const String& tabName, bool mixbus, std::map<int, Panel*> (*makePanels) (Page&, APVTS&))
    {
        auto* page = new Page(); pages.add (page);
        auto fxPanels = makePanels (*page, st);
        auto* rackTitle = makeLabel (*page, mixbus ? "MIXBUS  -  CASCADE  (top > bottom)" : "FX RACK  -  CASCADE  (top > bottom)", 12.5f, colours::text);
        auto* rackTip = makeLabel (*page, mixbus ? "Mix processors for the whole synth. Set the order of FX RACK and MIXBUS on the ROUTING page."
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
            auto left = r.removeFromLeft (330);
            r.removeFromLeft (8);
            rackTitle->setBounds (left.removeFromTop (24).reduced (6, 0));
            rackTip->setBounds (left.removeFromBottom (44).reduced (6, 2));
            const int h = jmin (48, left.getHeight() / 10);
            for (auto* row : rows) row->setBounds (left.removeFromTop (h));
            auto area = r.removeFromTop (jmin (r.getHeight() - 150, 440));
            for (auto& kv : fxPanels) kv.second->setBounds (area);
            emptyNote->setBounds (area);
            r.removeFromTop (8);
            chain->setBounds (r);
            showSel();
        };
        tabs.addTab (tabName, Colours::transparentBlack, page, false);
        std::vector<Component*> v (rows.begin(), rows.end());
        v.push_back (chain);
        live.push_back (v);
        juce::ignoreUnused (addLive);
    };
    buildRack ("FX", false, [] (Page& page, APVTS& st) -> std::map<int, Panel*>
    {
        std::map<int, Panel*> m;
        m[pg::FX_Filter1] = page.add (new Panel (st, "Filter 1", items ({ "ff1_on:On", "ff1_type:Type", "ff1_freq:Freq", "ff1_res:Reso",
                                        "ff1_morph:Morph / Vowel", "ff1_poles:Poles", "ff1_spread:Spread", "ff1_drive:Drive", "ff1_mix:Mix" }), 5, cosmos::neon));
        m[pg::FX_Filter2] = page.add (new Panel (st, "Filter 2", items ({ "ff2_on:On", "ff2_type:Type", "ff2_freq:Freq", "ff2_res:Reso",
                                        "ff2_morph:Morph / Vowel", "ff2_poles:Poles", "ff2_spread:Spread", "ff2_drive:Drive", "ff2_mix:Mix" }), 5, cosmos::orchid));
        m[pg::FX_Stereo] = page.add (new Panel (st, "Stereo", items ({ "fx_st_on:On", "fx_st_width:Width", "fx_st_bass:Mono Bass", "fx_st_haas:Haas ms" }), 4));
        m[pg::FX_Flanger] = page.add (new Panel (st, "Flanger", items ({ "fx_fl_on:On", "fx_fl_rate:Rate", "fx_fl_depth:Depth", "fx_fl_fb:Feedback", "fx_fl_delay:Delay", "fx_fl_mix:Mix" }), 6));
        m[pg::FX_Phaser] = page.add (new Panel (st, "Phaser", items ({ "fx_ph_on:On", "fx_ph_rate:Rate", "fx_ph_depth:Depth", "fx_ph_fb:Feedback", "fx_ph_center:Center", "fx_ph_stages:Stages", "fx_ph_mix:Mix" }), 7));
        m[pg::FX_Delay] = page.add (new Panel (st, "Delay", items ({ "fx_dl_on:On", "fx_dl_sync:Sync", "fx_dl_pp:Ping-Pong", "fx_dl_div:Division", "fx_dl_time:Time ms",
                                        "fx_dl_fb:Feedback", "fx_dl_lowcut:Low Cut", "fx_dl_highcut:High Cut", "fx_dl_mix:Mix" }), 5));
        m[pg::FX_Rings] = page.add (new Panel (st, "Resonator (Rings)", items ({ "fx_rn_on:On", "fx_rn_model:Model", "fx_rn_poly:Poly", "fx_rn_follow:Follow Keys", "fx_rn_note:Note",
                                        "fx_rn_structure:Structure", "fx_rn_bright:Brightness", "fx_rn_damping:Damping", "fx_rn_pos:Position", "fx_rn_mix:Mix" }), 5, colours::warn));
        m[pg::FX_Clouds] = page.add (new Panel (st, "Granular (Clouds)", items ({ "fx_gr_on:On", "fx_gr_mode:Mode", "fx_gr_quality:Quality", "fx_gr_freeze:Freeze",
                                        "fx_gr_pos:Position", "fx_gr_size:Size", "fx_gr_pitch:Pitch", "fx_gr_density:Density", "fx_gr_texture:Texture",
                                        "fx_gr_spread:Spread", "fx_gr_fb:Feedback", "fx_gr_reverb:Reverb", "fx_gr_mix:Mix" }), 7, colours::warn));
        m[pg::FX_Reverb] = page.add (new Panel (st, "Reverb", items ({ "fx_rv_on:On", "fx_rv_size:Size", "fx_rv_decay:Decay", "fx_rv_damp:Damping",
                                        "fx_rv_pre:Pre-Delay", "fx_rv_mod:Modulation", "fx_rv_width:Width", "fx_rv_mix:Mix" }), 4, cosmos::fuchsia));
        return m;
    });
    buildRack ("MIXBUS", true, [] (Page& page, APVTS& st) -> std::map<int, Panel*>
    {
        std::map<int, Panel*> m;
        m[pg::MB_Tape] = page.add (new Panel (st, "Tape", items ({ "mb_tp_on:On", "mb_tp_drive:Drive", "mb_tp_wow:Wow & Flutter", "mb_tp_tone:Tone", "mb_tp_mix:Mix" }), 5, cosmos::gold));
        m[pg::MB_EQ] = page.add (new Panel (st, "EQ", items ({ "mb_eq_on:On", "mb_eq_lcut:Low Cut", "mb_eq_low:Low", "mb_eq_mid:Mid", "mb_eq_mfreq:Mid Freq", "mb_eq_high:High", "mb_eq_hcut:High Cut" }), 7, cosmos::neon));
        m[pg::MB_Comp] = page.add (new Panel (st, "Compressor", items ({ "mb_cp_on:On", "mb_cp_thr:Threshold", "mb_cp_ratio:Ratio", "mb_cp_att:Attack ms", "mb_cp_rel:Release ms", "mb_cp_gain:Makeup", "mb_cp_mix:Mix" }), 7, cosmos::orchid));
        m[pg::MB_Chorus] = page.add (new Panel (st, "Chorus", items ({ "mb_ch_on:On", "mb_ch_rate:Rate", "mb_ch_depth:Depth", "mb_ch_mix:Mix" }), 4, cosmos::azure));
        m[pg::MB_Imager] = page.add (new Panel (st, "Imager", items ({ "mb_im_on:On", "mb_im_width:Width", "mb_im_bass:Mono Bass" }), 3, cosmos::teal));
        m[pg::MB_Gate] = page.add (new Panel (st, "Trance Gate", items ({ "mb_tg_on:On", "mb_tg_pat:Pattern", "mb_tg_div:Division", "mb_tg_depth:Depth", "mb_tg_smooth:Smooth" }), 5, cosmos::fuchsia));
        m[pg::MB_DJ] = page.add (new Panel (st, "DJ Filter", items ({ "mb_dj_on:On", "mb_dj_pos:LP < > HP", "mb_dj_res:Reso" }), 3, cosmos::neon));
        m[pg::MB_Crush] = page.add (new Panel (st, "Bitcrusher", items ({ "mb_bc_on:On", "mb_bc_bits:Bits", "mb_bc_rate:Downsample", "mb_bc_mix:Mix" }), 4, cosmos::electric));
        m[pg::MB_Transient] = page.add (new Panel (st, "Transient Shaper", items ({ "mb_tr_on:On", "mb_tr_att:Attack", "mb_tr_sus:Sustain" }), 3, cosmos::gold));
        m[pg::MB_Limiter] = page.add (new Panel (st, "Limiter", items ({ "mb_lm_on:On", "mb_lm_in:Input", "mb_lm_ceil:Ceiling", "mb_lm_rel:Release ms" }), 4, cosmos::fuchsia));
        return m;
    });

    // ---------------- ROUTING / MIX ----------------
    {
        auto* page = new Page(); pages.add (page);
        auto* flow = page->add (new RoutingView (st));
        auto* mixer = page->add (new Panel (st, "Source Mixer", items ({ "wtA_level:WT A", "wtB_level:WT B", "wtC_level:WT C", "sp_level:Spectral",
                                            "gr_level:Grain", "pl_level:Plaits", "sub_level:Sub", "noise_level:Noise", "smp_level:One-Shot" }), 9, cosmos::neon));
        auto* routes = page->add (new Panel (st, "Through the filter  (off = goes straight to the amp)", items ({ "rt_flt_a:WT A", "rt_flt_b:WT B", "rt_flt_c:WT C",
                                            "rt_flt_sp:Spectral", "rt_flt_gr:Grain", "rt_flt_pl:Plaits", "rt_flt_sub:Sub + Noise", "rt_flt_smp:One-Shot" }), 4, cosmos::orchid));
        auto* dist = page->add (new Panel (st, "MultiDistortion", items ({ "bite_post:After Filter", "rt_dist_sp:Spectral In", "bite_drive:Drive", "bite_mix:Mix" }), 2, colours::warn));
        auto* bus = page->add (new Panel (st, "Buses & Engine", items ({ "rt_order:FX Bus Order", "oversampling:Oversampling", "polyphony:Voices", "master_gain:Master" }), 2, cosmos::fuchsia));
        auto* skinBox = page->add (new ComboBox());
        skinBox->setTooltip ("Look of the whole interface");
        for (int k = 0; k < (int) skins().size(); ++k) skinBox->addItem (skins()[(size_t) k].name, k + 1);
        skinBox->setSelectedId (skinIndex() + 1, dontSendNotification);
        skinBox->onChange = [this, skinBox] { setSkin (skinBox->getSelectedId() - 1); };
        auto* skinLbl = makeLabel (*page, "SKIN", 11.0f, colours::text);
        page->layout = [=] (Rectangle<int> r)
        {
            auto top = r.removeFromTop (r.getHeight() * 42 / 100);
            flow->setBounds (top);
            r.removeFromTop (6);
            auto rows = split (r, { 0.9f, 1.25f }, false);
            mixer->setBounds (rows[0]);
            auto b = split (rows[1], { 1.4f, 0.75f, 0.85f });
            routes->setBounds (b[0]); dist->setBounds (b[1]);
            auto right = b[2];
            auto sk = right.removeFromBottom (40).reduced (14, 6);
            skinLbl->setBounds (sk.removeFromLeft (50));
            skinBox->setBounds (sk);
            bus->setBounds (right);
        };
        tabs.addTab ("ROUTING", Colours::transparentBlack, page, false);
        live.push_back ({ flow });
    }

    // ---------------- ANALYZER: scope, spectrum, goniometer, loudness ----------------
    {
        auto* page = new Page(); pages.add (page);
        auto* strip = page->add (new pg::ui::AnalyzerStrip (feed));
        page->layout = [strip] (Rectangle<int> r) { strip->setBounds (r.reduced (6)); };
        tabs.addTab ("ANALYZER", Colours::transparentBlack, page, false);
        live.push_back ({});
    }

    // every knob on every page, for the modulation rings and the drag glow
    for (int t = 0; t < tabs.getNumTabs(); ++t)
    {
        std::vector<ParamControl*> list;
        std::function<void (Component*)> walk = [&] (Component* c)
        {
            if (auto* pc = dynamic_cast<ParamControl*> (c)) { if (pc->kind() == ParamControl::Knob) list.push_back (pc); return; }
            for (auto* ch : c->getChildren()) walk (ch);
        };
        walk (tabs.getTabContentComponent (t));
        tabKnobs.push_back (list);
    }

    setResizable (true, true);
    setResizeLimits (1180, 740 + kChipsH, 2200, 1500);
    setSize (1280, 800 + kChipsH);
    startTimerHz (30);
}

SynthgularityEditor::~SynthgularityEditor()
{
    stopTimer();
    tabs.clearTabs();   // the pages are owned here, not by the tab bar
    setLookAndFeel (nullptr);
}

void SynthgularityEditor::paint (Graphics& g)
{
    using namespace pg::ui;
    const int sk = skinIndex();
    if (stars.getWidth() != getWidth() || stars.getHeight() != getHeight())
    {
        const float W = (float) getWidth(), Hh = (float) getHeight();
        if (sk == 0)
        {
            stars = brand::starfield (getWidth(), getHeight(), 7, 0.9f);
            // the Calabi Yau logo grid, sunk into the starfield
            Graphics sg (stars);
            sg.setOpacity (0.22f);
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
                // brushed metal / dark walnut strokes
                for (int k = 0; k < 900; ++k)
                {
                    sg.setColour (Colour (0xffffffff).withAlpha (0.012f + rnd.nextFloat() * 0.02f));
                    const float y = rnd.nextFloat() * Hh;
                    sg.drawHorizontalLine ((int) y, rnd.nextFloat() * W * 0.5f, W);
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

    // header: drop glyph + product name in the display face, then the Calabi Yau signature
    auto header = getLocalBounds().removeFromTop (kHeaderH).reduced (18, 0).toFloat();
    // official isologo file (opens the About card)
    isologoArea = Rectangle<float> (header.getX(), header.getCentreY() - 23.0f, 46.0f, 46.0f);
    {
        // animated aura: two slow rings breathing out of the isologo, a few twinkling sparks
        const float t = (float) animT;
        for (int k = 0; k < 2; ++k)
        {
            const float ph = std::fmod (t * 0.35f + (float) k * 0.5f, 1.0f);
            const float rad = 23.0f + ph * 16.0f;
            g.setColour ((k == 0 ? cosmos::fuchsia : cosmos::neon).withAlpha (0.35f * (1.0f - ph)));
            g.drawEllipse (Rectangle<float> (rad * 2, rad * 2).withCentre (isologoArea.getCentre()), 1.5f);
        }
        Random rnd (21);
        for (int k = 0; k < 9; ++k)
        {
            const Point<float> pt (header.getX() + 40.0f + rnd.nextFloat() * 280.0f, 6.0f + rnd.nextFloat() * ((float) kHeaderH - 12.0f));
            const float a = 0.5f + 0.5f * std::sin (t * (1.2f + rnd.nextFloat() * 2.0f) + (float) k * 1.7f);
            brand::sparkle (g, pt, 2.0f + 5.0f * a, cosmos::starlight.withAlpha (0.25f + 0.6f * a));
        }
    }
    g.drawImage (isologo, isologoArea, RectanglePlacement::centred);
    g.setColour (cosmos::starlight);
    g.setFont (fonts::display (16.5f));
    g.drawText ("SYNTHGULARITY", header.withTrimmedLeft (58.0f).withHeight (header.getHeight() * 0.62f), Justification::bottomLeft);
    auto sig = header.withTrimmedLeft (60.0f).withTrimmedTop (header.getHeight() * 0.64f).withHeight (16.0f);
   #if PG_HAS_LOGO
    if (logo.isValid())
    {
        // official logo file, never redrawn
        const float h = 26.0f, w = h * (float) logo.getWidth() / (float) logo.getHeight();
        g.setColour (cosmos::steel);
        g.setFont (fonts::label (9.5f));
        g.drawText ("BY", sig.withWidth (22.0f), Justification::centredLeft);
        g.drawImage (logo, Rectangle<float> (sig.getX() + 22.0f, sig.getCentreY() - h / 2, w, h), RectanglePlacement::centred);
    }
    else
   #endif
    {
        g.setColour (cosmos::gold);
        g.setFont (fonts::label (10.0f));
        g.drawText ("BY CALABI YAU", sig, Justification::centredLeft);
    }
    if (sk != 1) brand::checker (g, Rectangle<float> (0.0f, (float) kHeaderH, (float) getWidth(), 8.0f), 4.0f);
    g.setColour (colours::textDim);
    g.setFont (fonts::label (8.5f));
    g.drawText ("MOD SOURCES", Rectangle<float> (12.0f, (float) (getHeight() - kChipsH), 86.0f, (float) kChipsH), Justification::centredLeft);
}

// ---------------------------------------------------------------------------
// Drag-and-drop modulation
// ---------------------------------------------------------------------------
Colour SynthgularityEditor::colourOf (int s) const
{
    switch (s)
    {
        case MS_Lfo1: return cosmos::neon;     case MS_Lfo2: return cosmos::teal;
        case MS_Lfo3: return cosmos::azure;    case MS_Lfo4: return cosmos::orchid;
        case MS_Chaos1: return cosmos::fuchsia;
        case MS_Chaos2: case MS_Chaos2Y: case MS_Chaos2Z: return Colour (0xffff5c8a);
        case MS_ModEnv: case MS_ModEnv2: case MS_ModEnv3: case MS_ModEnv4: return cosmos::gold;
        case MS_FiltEnv: return Colour (0xffff8a3d);
        case MS_AmpEnv: return cosmos::starlight;
        case MS_Velocity: return Colour (0xff9fd7ff);
        case MS_Note: return Colour (0xffb7a6ff);
        case MS_ModWheel: return Colour (0xff7ef0d0);
        case MS_Aftertouch: return Colour (0xffffc7f0);
        case MS_PitchBend: return Colour (0xffd6e685);
        case MS_Random: return Colour (0xffc9c9c9);
        default:
            if (s >= MS_Seq1 && s <= MS_Seq8) return cosmos::electric.interpolatedWith (cosmos::neon, (float) (s - MS_Seq1) / 7.0f);
            return cosmos::steel;
    }
}

bool SynthgularityEditor::canModulate (const String& id) const
{
    const int pid = indexOf (id);
    return pid >= 0 && destForParam (pid) > 0;
}

void SynthgularityEditor::dropModulation (int source, const String& id)
{
    const int pid = indexOf (id);
    const bool bip = isBipolarSource (source, proc.paramCache());
    if (proc.addModulation (source, pid, bip ? 0.25f : 0.35f) < 0)
        showError ("All 32 matrix slots are in use. Clear one on the MATRIX page, or right-click a knob to remove a route.");
}

std::vector<ui::ModRing> SynthgularityEditor::ringsFor (const String& id) const
{
    std::vector<ui::ModRing> out;
    const int pid = indexOf (id);
    if (pid < 0) return out;
    auto* prm = proc.apvts.getParameter (id);
    const float base = prm != nullptr ? prm->getValue() : 0.0f;
    const int stride = mtx02_src - mtx01_src;
    for (int s : proc.slotsFor (pid))
    {
        const int src = (int) proc.value (mtx01_src + s * stride), pol = (int) proc.value (mtx01_pol + s * stride);
        const float amt = proc.value (mtx01_amt + s * stride);
        const bool bip = pol == 2 || (pol == 0 && isBipolarSource (src, proc.paramCache()));
        const float lo = bip ? base - std::abs (amt) : jmin (base, base + amt), hi = bip ? base + std::abs (amt) : jmax (base, base + amt);
        out.push_back ({ colourOf (src), lo, hi });
    }
    return out;
}

float SynthgularityEditor::liveValue (const String& id) const
{
    const int pid = indexOf (id);
    if (pid < 0) return -1.0f;
    auto* prm = proc.apvts.getParameter (id);
    if (prm == nullptr) return -1.0f;
    if (destForParam (pid) >= MD_Count)
        return prm->convertTo0to1 (proc.synth.effectiveParams()[pid]);
    // voice destinations: knob plus every route times the source's present value
    float v = prm->getValue();
    const int stride = mtx02_src - mtx01_src;
    for (int s : proc.slotsFor (pid))
    {
        const int src = (int) proc.value (mtx01_src + s * stride), pol = (int) proc.value (mtx01_pol + s * stride);
        v += applyPolarity (proc.synth.liveSrc[(size_t) src].load(), src, pol, proc.paramCache()) * proc.value (mtx01_amt + s * stride);
    }
    return jlimit (0.0f, 1.0f, v);
}

void SynthgularityEditor::showModMenu (const String& id, Component& knob)
{
    const int pid = indexOf (id);
    const auto slots = proc.slotsFor (pid);
    static const char* srcNames[] = { "-", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Chaos 1", "Lorenz X", "Lorenz Y", "Lorenz Z",
                                      "Seq 1", "Seq 2", "Seq 3", "Seq 4", "Seq 5", "Seq 6", "Seq 7", "Seq 8",
                                      "Mod Env 1", "Filter Env", "Amp Env", "Velocity", "Note", "Mod Wheel", "Aftertouch", "Pitch Bend", "Random",
                                      "Mod Env 2", "Mod Env 3", "Mod Env 4" };
    static const float amounts[] = { -1.0f, -0.5f, -0.25f, -0.1f, 0.1f, 0.25f, 0.5f, 0.75f, 1.0f };
    PopupMenu m;
    m.addSectionHeader (proc.apvts.getParameter (id)->getName (40));
    m.addItem (1, "Reset to default");
    if (slots.isEmpty()) m.addItem (2, "Drag a MOD SOURCES chip onto this knob to modulate it", false, false);
    const int stride = mtx02_src - mtx01_src;
    for (int i = 0; i < slots.size(); ++i)
    {
        const int s = slots[i];
        const int src = (int) proc.value (mtx01_src + s * stride);
        const float amt = proc.value (mtx01_amt + s * stride);
        PopupMenu sub;
        for (int k = 0; k < 9; ++k)
            sub.addItem (100 + i * 20 + k, (amounts[k] > 0 ? "+" : "") + String (roundToInt (amounts[k] * 100)) + " %", true, std::abs (amounts[k] - amt) < 0.01f);
        sub.addSeparator();
        sub.addItem (100 + i * 20 + 10, "Invert");
        sub.addItem (100 + i * 20 + 11, "Remove");
        m.addSubMenu (String (srcNames[src]) + "   " + (amt >= 0 ? "+" : "") + String (roundToInt (amt * 100)) + " %   (slot " + String (s + 1) + ")", sub);
    }
    SafePointer<Component> safe (&knob);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&knob), [this, slots, id, safe] (int r)
    {
        if (r == 1) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->getDefaultValue()); }
        else if (r >= 100)
        {
            const int i = (r - 100) / 20, k = (r - 100) % 20;
            if (i >= slots.size()) return;
            const int stride2 = mtx02_src - mtx01_src;
            if (k < 9) proc.setModAmount (slots[i], amounts[k]);
            else if (k == 10) proc.setModAmount (slots[i], -proc.value (mtx01_amt + slots[i] * stride2));
            else if (k == 11) proc.removeModulation (slots[i]);
        }
        if (safe != nullptr) safe->repaint();
    });
}

void SynthgularityEditor::setSkin (int index)
{
    skinIndex() = jlimit (0, (int) skins().size() - 1, index);
    proc.uiSkin = skinIndex();
    stars = Image();
    sendLookAndFeelChange();
    repaint();
}

void SynthgularityEditor::mouseDown (const MouseEvent& e)
{
    if (isologoArea.contains (e.position)) { about.setVisible (true); about.toFront (false); }
}

void SynthgularityEditor::mouseMove (const MouseEvent& e)
{
    setMouseCursor (isologoArea.contains (e.position) ? MouseCursor::PointingHandCursor : MouseCursor::NormalCursor);
}

void SynthgularityEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (kHeaderH).reduced (12, 12);
    r.removeFromTop (10);
    header.removeFromLeft (318);
    master->setBounds (header.removeFromRight (jmin (560, header.getWidth() - 440)).withTrimmedTop (-8).withTrimmedBottom (-8));
    header.removeFromRight (6);
    gonio->setBounds (header.removeFromRight (64).withTrimmedTop (-8).withTrimmedBottom (-8));
    header.removeFromRight (8);
    auto presetArea = header.withSizeKeepingCentre (jmin (480, header.getWidth()), 30).translated (0, -9);
    voicesLabel.setBounds (presetArea.translated (0, 31).withHeight (16));
    prevButton.setBounds (presetArea.removeFromLeft (30));
    presetArea.removeFromLeft (4);
    initButton.setBounds (presetArea.removeFromRight (60));
    presetArea.removeFromRight (4);
    saveButton.setBounds (presetArea.removeFromRight (64));
    presetArea.removeFromRight (4);
    nextButton.setBounds (presetArea.removeFromRight (30));
    presetArea.removeFromRight (4);
    presetBox.setBounds (presetArea);
    {
        auto bar = r.removeFromBottom (kChipsH).reduced (10, 2);
        bar.removeFromLeft (88);   // "MOD SOURCES" label (painted)
        const int w = bar.getWidth() / juce::jmax (1, chips.size());
        for (auto* c : chips) c->setBounds (bar.removeFromLeft (w));
    }
    tabs.setBounds (r.reduced (4, 0));
    about.setBounds (getLocalBounds());
}

void SynthgularityEditor::timerCallback()
{
    animT += 1.0 / 30.0;
    repaint (0, 0, 340, kHeaderH);   // the header aura
    gonio->repaint();
    for (auto* c : chips) c->repaint();
    if (auto* c = tabs.getCurrentContentComponent())
        if (c->getAlpha() < 1.0f) c->setAlpha (jmin (1.0f, c->getAlpha() + 0.16f));
    const int tab = tabs.getCurrentTabIndex();
    if (tab >= 0 && tab < (int) tabKnobs.size())
    {
        const bool dragging = isDragAndDropActive();
        for (auto* k : tabKnobs[(size_t) tab])
            if (dragging || k->isModulated()) k->repaint();
    }
    if (tab >= 0 && tab < (int) live.size())
        for (auto* c : live[(size_t) tab]) if (c != nullptr && c->isVisible()) c->repaint();
    voicesLabel.setText (String (proc.synth.activeVoices.load()) + " voices", dontSendNotification);
}

// ---------------------------------------------------------------------------
// Presets & files
// ---------------------------------------------------------------------------
void SynthgularityEditor::refreshPresetMenu()
{
    presetBox.clear (dontSendNotification);
    auto factory = SynthgularityProcessor::factoryPresetNames();
    presetBox.addSectionHeading ("Factory");
    for (int i = 0; i < factory.size(); ++i) presetBox.addItem (factory[i], 1 + i);
    userPresets = proc.listPresets();
    if (! userPresets.isEmpty())
    {
        // folders become sub-menus: Factory Library > 01 Bass > ...
        std::map<String, std::unique_ptr<PopupMenu>> subs;
        std::vector<String> order;
        PopupMenu userTop;
        for (int i = 0; i < userPresets.size(); ++i)
        {
            const auto rel = userPresets[i].getRelativePathFrom (proc.presetFolder()).replaceCharacter ('\\', '/');
            const auto dir = rel.upToLastOccurrenceOf ("/", false, false);
            const auto name = userPresets[i].getFileNameWithoutExtension();
            if (dir.isEmpty()) { userTop.addItem (1000 + i, name); continue; }
            if (subs.find (dir) == subs.end()) { subs[dir] = std::make_unique<PopupMenu>(); order.push_back (dir); }
            subs[dir]->addItem (1000 + i, name);
        }
        auto* root = presetBox.getRootMenu();
        // group "A/B" folders under "A"
        std::map<String, PopupMenu> parents;
        std::vector<String> parentOrder;
        for (auto& dir : order)
        {
            const auto parent = dir.upToFirstOccurrenceOf ("/", false, false);
            const auto child = dir.fromFirstOccurrenceOf ("/", false, false);
            if (child.isEmpty()) { root->addSubMenu (dir, *subs[dir]); continue; }
            if (parents.find (parent) == parents.end()) parentOrder.push_back (parent);
            parents[parent].addSubMenu (child, *subs[dir]);
        }
        for (auto& p : parentOrder) root->addSubMenu (p, parents[p]);
        if (userTop.getNumItems() > 0) { presetBox.addSectionHeading ("User"); root->addSubMenu ("My presets", userTop); }
    }
    presetBox.setText (proc.currentPresetName, dontSendNotification);
}

void SynthgularityEditor::choosePreset (int id)
{
    if (id <= 0) return;
    if (id < 1000) proc.loadFactoryPreset (id - 1);
    else if (id - 1000 < userPresets.size()) proc.loadPreset (userPresets[id - 1000]);
    for (auto* l : lanes) l->repaint();
    if (arpGrid) arpGrid->repaint();
}

void SynthgularityEditor::savePresetDialog()
{
    auto* w = new AlertWindow ("Save preset", "Stores every knob, the SEQ and ARP steps and the paths of your loaded files.", MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.currentPresetName, "Name");
    w->addButton ("SAVE", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("CANCEL", 0, KeyPress (KeyPress::escapeKey));
    w->enterModalState (true, ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result == 1)
        {
            String err;
            if (! proc.savePreset (w->getTextEditorContents ("name"), err)) showError (err);
            refreshPresetMenu();
        }
    }), true);
}

void SynthgularityEditor::loadSampleDialog()
{
    chooser = std::make_unique<FileChooser> ("Load a sample for the granular oscillator",
                                             File::getSpecialLocation (File::userMusicDirectory), pg::supportedAudioWildcard());
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc)
    {
        auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        String err;
        if (! proc.loadSample (f, err)) showError (err);
    });
}

void SynthgularityEditor::loadWavetableDialog (int osc)
{
    chooser = std::make_unique<FileChooser> ("Load a wavetable", File::getSpecialLocation (File::userMusicDirectory), pg::supportedAudioWildcard());
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this, osc] (const FileChooser& fc)
    {
        auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        String err;
        if (! proc.loadUserWavetable (osc, f, err)) showError (err);
    });
}

void SynthgularityEditor::loadFileDialog (const String& title, std::function<bool (const File&, String&)> loader)
{
    chooser = std::make_unique<FileChooser> (title, File::getSpecialLocation (File::userMusicDirectory), pg::supportedAudioWildcard());
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this, loader] (const FileChooser& fc)
    {
        auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        String err;
        if (! loader (f, err)) showError (err);
    });
}

void SynthgularityEditor::showError (const String& message)
{
    AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Synthgularity", message);
}
