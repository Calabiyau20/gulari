#include "PluginEditor.h"
#include "BinaryData.h"
#include <map>

using namespace juce;
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
    explicit FxChainView (APVTS& s) : st (s) {}
    APVTS& st;
    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (4.0f);
        auto inner = brand::chamferFrame (g, r, 14.0f, 5.0f, cosmos::voidC.withAlpha (0.85f), cosmos::nebula, false).reduced (12.0f, 8.0f);
        g.setColour (colours::textDim);
        g.setFont (fonts::label (9.5f));
        g.drawText ("SIGNAL FLOW", inner.removeFromTop (16.0f), Justification::centredLeft);
        std::vector<std::pair<String, bool>> nodes;
        bool used[pg::FX_Count] {};
        for (int i = 0; i < 10; ++i)
        {
            const int t = (int) st.getRawParameterValue ("fx_slot" + String (i + 1))->load();
            if (t <= 0 || t >= pg::FX_Count || used[t]) continue;
            used[t] = true;
            const auto id = FxSlotRow::onParamFor (t);
            const bool on = id.isNotEmpty() && st.getRawParameterValue (id)->load() > 0.5f;
            nodes.push_back ({ pg::choices::fxTypes()[t], on });
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
                                           "sp_semi:Semi", "sp_fine:Fine", "sp_level:Level", "sp_pan:Pan" }), 9, cosmos::neon));
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
        auto* fm = page->add (new Panel (st, "Mod Matrix  FM / PD / AM  (row > column)", items ({ "fm_amount:Amount", "fm_a_a:A>A",
                                          "fm_a_b:A>B", "fm_a_c:A>C", "fm_b_a:B>A", "fm_b_b:B>B", "fm_b_c:B>C", "fm_c_a:C>A", "fm_c_b:C>B",
                                          "fm_c_c:C>C", "fm_g_a:Grain>A", "fm_g_b:Grain>B", "fm_g_c:Grain>C", "fm_n_a:Noise>A", "fm_n_b:Noise>B", "fm_n_c:Noise>C" }), 6, colours::accent2));
        {
            ParamControl* types[3];
            const char* tid[] = { "fm_type_a", "fm_type_b", "fm_type_c" };
            const char* tname[] = { "Type into A", "Type into B", "Type into C" };
            for (int t = 0; t < 3; ++t) types[t] = page->add (new ParamControl (st, tid[t], tname[t]));
            fm->extraHeight = 44;
            fm->layoutExtra = [types, panel = fm] (Rectangle<int> r)
            {
                r += panel->getPosition();
                const int w = r.getWidth() / 3;
                for (int t = 0; t < 3; ++t) types[t]->setBounds (r.removeFromLeft (w));
            };
        }
        page->layout = [=] (Rectangle<int> r)
        {
            auto rows = split (r, { 0.85f, 1.15f }, false);
            auto top = split (rows[0], { 0.75f, 2.0f });
            sub->setBounds (top[0]); smp->setBounds (top[1]);
            auto bottom = split (rows[1], { 1.0f, 1.45f });
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
        for (int s = 0; s < 16; ++s) rows.push_back (page->add (new MatrixRow (st, s)));
        auto* tip = makeLabel (*page, "Per-voice sources (envelopes, velocity, note, random) only reach voice destinations. FX destinations use the shared sources.", 12.0f);
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
            const int h = r.getHeight() / 16;
            for (auto* row : rows) row->setBounds (r.removeFromTop (h));
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
            for (auto sfx : { "_on:On", "_len:Length", "_div:Division", "_dir:Direction", "_mode:Range", "_swing:Swing", "_glide:Glide" })
                it.add ("seq" + n + sfx);
            ps.push_back (page->add (new Panel (st, "Lane " + n, it, 4, l % 2 ? colours::accent2 : colours::accent)));
            auto* lane = lanes.add (new StepLaneEditor (proc, l));
            page->addAndMakeVisible (lane);
            tools.push_back ({ makeButton (*page, "RANDOM", [lane] { lane->randomize (false); }, "New random values (locked steps stay)"),
                               makeButton (*page, "MUTATE", [lane] { lane->randomize (true); }, "Small random changes"),
                               makeButton (*page, "EUCLID", [lane] { lane->euclid(); }, "Spread hits evenly; click again for one more hit"),
                               makeButton (*page, "RAMP", [lane] { lane->ramp(); }),
                               makeButton (*page, "CLEAR", [lane] { lane->clear(); }) });
        }
        auto bankHolder = std::make_shared<int> (0);
        TextButton* bankBtn[2];
        auto* tip = makeLabel (*page, "Draw with the mouse. Right-click a step to lock it (gold). DIRECTION: forward, backward, ping-pong, random, even or odd steps. "
                                      "Route a lane on the MATRIX page (Seq 1-8).", 12.0f);
        auto* lanesPtr = &lanes;
        for (int b = 0; b < 2; ++b)
        {
            bankBtn[b] = makeButton (*page, b == 0 ? "LANES 1-4" : "LANES 5-8", {}, "Show lanes " + String (b == 0 ? "1-4" : "5-8"));
            bankBtn[b]->onClick = [bankHolder, b, page] { *bankHolder = b; page->resized(); };
        }
        page->layout = [=] (Rectangle<int> r)
        {
            auto head = r.removeFromTop (30);
            for (int b = 0; b < 2; ++b)
            {
                bankBtn[b]->setBounds (head.removeFromLeft (120).reduced (2, 2));
                bankBtn[b]->setToggleState (*bankHolder == b, dontSendNotification);
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
                ps[(size_t) l]->setBounds (row.removeFromLeft (360));
                auto tcol = row.removeFromRight (90).reduced (4, 8);
                const int bh = tcol.getHeight() / 5;
                for (auto* t : tools[(size_t) l]) t->setBounds (tcol.removeFromTop (bh).reduced (0, 2));
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

    // ---------------- FX: a rack of 10 slots in cascade ----------------
    {
        auto* page = new Page(); pages.add (page);
        std::map<int, Panel*> fxPanels;
        fxPanels[pg::FX_Filter1] = page->add (new Panel (st, "Filter 1", items ({ "ff1_on:On", "ff1_type:Type", "ff1_freq:Freq", "ff1_res:Reso",
                                        "ff1_morph:Morph / Vowel", "ff1_poles:Poles", "ff1_spread:Spread", "ff1_drive:Drive", "ff1_mix:Mix" }), 5, cosmos::neon));
        fxPanels[pg::FX_Filter2] = page->add (new Panel (st, "Filter 2", items ({ "ff2_on:On", "ff2_type:Type", "ff2_freq:Freq", "ff2_res:Reso",
                                        "ff2_morph:Morph / Vowel", "ff2_poles:Poles", "ff2_spread:Spread", "ff2_drive:Drive", "ff2_mix:Mix" }), 5, cosmos::orchid));
        fxPanels[pg::FX_Stereo] = page->add (new Panel (st, "Stereo", items ({ "fx_st_on:On", "fx_st_width:Width", "fx_st_bass:Mono Bass", "fx_st_haas:Haas ms" }), 4));
        fxPanels[pg::FX_Flanger] = page->add (new Panel (st, "Flanger", items ({ "fx_fl_on:On", "fx_fl_rate:Rate", "fx_fl_depth:Depth", "fx_fl_fb:Feedback", "fx_fl_delay:Delay", "fx_fl_mix:Mix" }), 6));
        fxPanels[pg::FX_Phaser] = page->add (new Panel (st, "Phaser", items ({ "fx_ph_on:On", "fx_ph_rate:Rate", "fx_ph_depth:Depth", "fx_ph_fb:Feedback", "fx_ph_center:Center", "fx_ph_stages:Stages", "fx_ph_mix:Mix" }), 7));
        fxPanels[pg::FX_Delay] = page->add (new Panel (st, "Delay", items ({ "fx_dl_on:On", "fx_dl_sync:Sync", "fx_dl_pp:Ping-Pong", "fx_dl_div:Division", "fx_dl_time:Time ms",
                                        "fx_dl_fb:Feedback", "fx_dl_lowcut:Low Cut", "fx_dl_highcut:High Cut", "fx_dl_mix:Mix" }), 5));
        fxPanels[pg::FX_Rings] = page->add (new Panel (st, "Resonator (Rings)", items ({ "fx_rn_on:On", "fx_rn_model:Model", "fx_rn_poly:Poly", "fx_rn_follow:Follow Keys", "fx_rn_note:Note",
                                        "fx_rn_structure:Structure", "fx_rn_bright:Brightness", "fx_rn_damping:Damping", "fx_rn_pos:Position", "fx_rn_mix:Mix" }), 5, colours::warn));
        fxPanels[pg::FX_Clouds] = page->add (new Panel (st, "Granular (Clouds)", items ({ "fx_gr_on:On", "fx_gr_mode:Mode", "fx_gr_quality:Quality", "fx_gr_freeze:Freeze",
                                        "fx_gr_pos:Position", "fx_gr_size:Size", "fx_gr_pitch:Pitch", "fx_gr_density:Density", "fx_gr_texture:Texture",
                                        "fx_gr_spread:Spread", "fx_gr_fb:Feedback", "fx_gr_reverb:Reverb", "fx_gr_mix:Mix" }), 7, colours::warn));
        fxPanels[pg::FX_Reverb] = page->add (new Panel (st, "Reverb", items ({ "fx_rv_on:On", "fx_rv_size:Size", "fx_rv_decay:Decay", "fx_rv_damp:Damping",
                                        "fx_rv_pre:Pre-Delay", "fx_rv_mod:Modulation", "fx_rv_width:Width", "fx_rv_mix:Mix" }), 4, cosmos::fuchsia));
        auto* rackTitle = makeLabel (*page, "FX RACK  -  CASCADE  (top > bottom)", 12.5f, colours::text);
        auto* rackTip = makeLabel (*page, "Pick an effect in each slot, the dot switches it on. Arrows move it in the chain. Click a slot to edit it.", 11.0f);
        rackTip->setJustificationType (Justification::topLeft);
        std::vector<FxSlotRow*> rows;
        for (int s2 = 0; s2 < 10; ++s2) rows.push_back (page->add (new FxSlotRow (st, s2)));
        auto selected = std::make_shared<int> (0);
        auto* emptyNote = makeLabel (*page, "Empty slot: choose an effect from the menu.", 13.0f);
        emptyNote->setJustificationType (Justification::centred);
        auto showSel = [rows, fxPanels, selected, emptyNote] ()
        {
            emptyNote->setVisible (rows[(size_t) *selected]->type() == pg::FX_Empty);
            for (int i = 0; i < (int) rows.size(); ++i) { rows[(size_t) i]->selected = i == *selected; rows[(size_t) i]->repaint(); }
            const int type = rows[(size_t) *selected]->type();
            for (auto& kv : fxPanels) kv.second->setVisible (kv.first == type);
        };
        for (auto* row : rows)
        {
            row->onSelect = [selected, showSel] (int sl) { *selected = sl; showSel(); };
            row->onMove = [this, selected, showSel] (int sl, int dir)
            {
                const int other = sl + dir;
                if (other < 0 || other > 9) return;
                auto* a = proc.apvts.getParameter ("fx_slot" + String (sl + 1));
                auto* b = proc.apvts.getParameter ("fx_slot" + String (other + 1));
                const float va = a->getValue(), vb = b->getValue();
                a->setValueNotifyingHost (vb);
                b->setValueNotifyingHost (va);
                *selected = other;
                showSel();
            };
        }
        auto* chain = page->add (new FxChainView (st));
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
        tabs.addTab ("FX", Colours::transparentBlack, page, false);
        std::vector<Component*> v (rows.begin(), rows.end());
        v.push_back (chain);
        live.push_back (v);
    }

    setResizable (true, true);
    setResizeLimits (1040, 700, 2000, 1400);
    setSize (1180, 780);
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
    if (stars.getWidth() != getWidth() || stars.getHeight() != getHeight())
    {
        stars = brand::starfield (getWidth(), getHeight(), 7, 0.9f);
        // the Calabi Yau logo grid, sunk into the starfield
        Graphics sg (stars);
        sg.setOpacity (0.22f);
        sg.drawImage (logoGrid, stars.getBounds().toFloat(), RectanglePlacement::centred | RectanglePlacement::fillDestination);
        sg.setOpacity (1.0f);
        sg.setGradientFill (ColourGradient (cosmos::voidC, 0.0f, 0.0f, cosmos::voidC.withAlpha (0.0f), 0.0f, (float) getHeight() * 0.55f, false));
        sg.fillAll();
    }
    g.drawImageAt (stars, 0, 0);

    // header: drop glyph + product name in the display face, then the Calabi Yau signature
    auto header = getLocalBounds().removeFromTop (kHeaderH).reduced (18, 0).toFloat();
    // official isologo file (opens the About card)
    isologoArea = Rectangle<float> (header.getX(), header.getCentreY() - 23.0f, 46.0f, 46.0f);
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
    brand::checker (g, Rectangle<float> (0.0f, (float) kHeaderH, (float) getWidth(), 8.0f), 4.0f);
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
    master->setBounds (header.removeFromRight (452).withTrimmedTop (-8).withTrimmedBottom (-8));
    header.removeFromRight (10);
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
    tabs.setBounds (r.reduced (4, 0));
    about.setBounds (getLocalBounds());
}

void SynthgularityEditor::timerCallback()
{
    const int tab = tabs.getCurrentTabIndex();
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
