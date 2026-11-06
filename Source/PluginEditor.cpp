#include "PluginEditor.h"
#include "BinaryData.h"

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
    masterKnob = std::make_unique<ParamControl> (st, "master_gain", "MASTER");
    addAndMakeVisible (*masterKnob);
    voicesLabel.setColour (Label::textColourId, colours::textDim);
    voicesLabel.setJustificationType (Justification::centredRight);
    addAndMakeVisible (voicesLabel);
    refreshPresetMenu();

    tabs.setTabBarDepth (36);
    tabs.setOutline (0);
    tabs.setColour (TabbedComponent::backgroundColourId, Colours::transparentBlack);
    tabs.setColour (TabbedComponent::outlineColourId, Colours::transparentBlack);
    addAndMakeVisible (tabs);

    // ---------------- OSC ----------------
    {
        auto* page = new Page(); pages.add (page);
        Panel* wt[2];
        for (int o = 0; o < 2; ++o)
        {
            const String x = o == 0 ? "wtA_" : "wtB_";
            StringArray it;
            for (auto s : { "on:On", "table:Table", "pos:Position", "oct:Octave", "semi:Semi", "fine:Fine",
                            "unison:Unison", "detune:Detune", "spread:Spread", "level:Level", "pan:Pan", "rphase:Rnd Phase" })
                it.add (x + s);
            wt[o] = page->add (new Panel (st, o == 0 ? "Wavetable A" : "Wavetable B", it, 6, o == 0 ? colours::accent : colours::accent2));
            wt[o]->extraHeight = 78;
            auto* view = page->add (new WavetableView (proc, o));
            tableViews[o] = view;
            auto* load = makeButton (*page, "LOAD WAV", [this, o] { loadWavetableDialog (o); },
                                     "Load a WAV/AIFF/FLAC/MP3 as a wavetable (2048-sample frames)");
            wt[o]->layoutExtra = [view, load, panel = wt[o]] (Rectangle<int> r)
            {
                auto pr = r + panel->getPosition();
                load->setBounds (pr.removeFromRight (92).withSizeKeepingCentre (86, 26));
                view->setBounds (pr.reduced (0, 2));
            };
            view->toFront (false); load->toFront (false);
        }
        auto* mo = page->add (new Panel (st, "Modulator", items ({ "mo_wave:Wave", "mo_oct:Octave", "mo_semi:Semi", "mo_fine:Fine", "mo_level:Level" }), 3));
        auto* sub = page->add (new Panel (st, "Sub + Noise", items ({ "sub_wave:Sub Wave", "sub_oct:Sub Oct", "sub_level:Sub Level", "noise_type:Noise", "noise_level:Noise Lvl" }), 3));
        auto* pl = page->add (new Panel (st, "Plaits (Mutable Instruments)", items ({ "pl_on:On", "pl_engine:Engine", "pl_oct:Octave", "pl_level:Level",
                                          "pl_harm:Harmonics", "pl_timbre:Timbre", "pl_morph:Morph", "pl_pan:Pan", "pl_lpg:LPG", "pl_decay:Decay", "pl_colour:Colour" }), 4, colours::warn));
        auto* fm = page->add (new Panel (st, "FM Matrix  (row > column)", items ({ "fm_mode:Mode", "fm_amount:Amount", "fm_a_a:A>A",
                                          "fm_a_b:A>B", "fm_a_m:A>Mod", "fm_b_a:B>A", "fm_b_b:B>B", "fm_b_m:B>Mod", "fm_m_a:Mod>A", "fm_m_b:Mod>B",
                                          "fm_m_m:Mod>Mod", "fm_g_a:Grain>A", "fm_g_b:Grain>B", "fm_g_m:Grain>Mod", "fm_n_a:Noise>A", "fm_n_b:Noise>B", "fm_n_m:Noise>Mod" }), 6, colours::accent2));
        page->layout = [=] (Rectangle<int> r)
        {
            auto rows = split (r, { 1.0f, 1.25f }, false);
            auto top = split (rows[0], { 1.0f, 1.0f });
            wt[0]->setBounds (top[0]); wt[1]->setBounds (top[1]);
            auto bottom = split (rows[1], { 1.0f, 1.05f, 1.4f, 1.65f });
            mo->setBounds (bottom[0]);
            sub->setBounds (bottom[1]);
            pl->setBounds (bottom[2]);
            fm->setBounds (bottom[3]);
        };
        tabs.addTab ("OSC", Colours::transparentBlack, page, false);
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
                                       "Pitched: one grain per period of the note, so the sample keeps its formants.", 12.0f);
        auto* panel = page->add (new Panel (st, "Granular Oscillator", items ({ "gr_on:On", "gr_mode:Mode", "gr_pos:Position", "gr_scan:Scan",
                                           "gr_spray:Spray", "gr_size:Size", "gr_density:Density", "gr_window:Window",
                                           "gr_pitch:Pitch", "gr_prand:Pitch Rnd", "gr_keytrack:Keytrack", "gr_root:Root Note",
                                           "gr_reverse:Reverse", "gr_stereo:Stereo", "gr_level:Level", "gr_pan:Pan" }), 8));
        page->layout = [=] (Rectangle<int> r)
        {
            auto top = r.removeFromTop (r.getHeight() * 46 / 100);
            auto bar = top.removeFromBottom (34);
            sampleView->setBounds (top.reduced (3));
            load->setBounds (bar.removeFromRight (130).reduced (3, 4));
            info->setBounds (bar.reduced (6, 0));
            panel->setBounds (r);
        };
        tabs.addTab ("GRAIN", Colours::transparentBlack, page, false);
    }

    // ---------------- FILTER / AMP ----------------
    {
        auto* page = new Page(); pages.add (page);
        auto* flt = page->add (new Panel (st, "Filter", items ({ "flt_type:Type", "flt_model:Model", "flt_slope:Slope", "flt_drive:Drive",
                                         "flt_cutoff:Cutoff", "flt_res:Resonance", "flt_env:Env Amt", "flt_key:Keytrack" }), 4));
        auto* bite = page->add (new Panel (st, "Bite", items ({ "bite_drive:Drive", "bite_fold:Fold", "bite_crush:Crush", "bite_soft:Soft Clip",
                                          "bite_asym:Asymmetry", "bite_mix:Mix", "bite_post:After Filter" }), 4, colours::warn));
        auto* ampE = page->add (new Panel (st, "Amp Envelope", items ({ "amp_a:Attack", "amp_d:Decay", "amp_s:Sustain", "amp_r:Release" }), 4));
        auto* fltE = page->add (new Panel (st, "Filter Envelope", items ({ "fenv_a:Attack", "fenv_d:Decay", "fenv_s:Sustain", "fenv_r:Release" }), 4, colours::accent2));
        auto* modE = page->add (new Panel (st, "Mod Envelope", items ({ "menv_a:Attack", "menv_d:Decay", "menv_s:Sustain", "menv_r:Release" }), 4, colours::accent2));
        auto* voice = page->add (new Panel (st, "Voice", items ({ "voice_mode:Mode", "polyphony:Voices", "glide:Glide", "bend_range:Bend" }), 4));
        page->layout = [=] (Rectangle<int> r)
        {
            auto rows = split (r, { 1.6f, 1.0f, 1.0f }, false);
            auto a = split (rows[0], { 1.0f, 1.0f });
            flt->setBounds (a[0]); bite->setBounds (a[1]);
            auto b = split (rows[1], { 1.0f, 1.0f });
            ampE->setBounds (b[0]); fltE->setBounds (b[1]);
            auto c = split (rows[2], { 1.0f, 1.0f });
            modE->setBounds (c[0]); voice->setBounds (c[1]);
        };
        tabs.addTab ("FILTER / AMP", Colours::transparentBlack, page, false);
    }

    // ---------------- MOD ----------------
    {
        auto* page = new Page(); pages.add (page);
        Panel* lfo[4];
        for (int i = 0; i < 4; ++i)
        {
            const String n (i + 1);
            StringArray it;
            for (auto s : { "_shape:Shape", "_retrig:Retrig", "_sync:Sync", "_rate:Rate", "_div:Division", "_phase:Phase" })
                it.add ("lfo" + n + s);
            lfo[i] = page->add (new Panel (st, "LFO " + n, it, 3, i % 2 ? colours::accent2 : colours::accent));
        }
        auto* chaos = page->add (new Panel (st, "Chaos (Lorenz)", items ({ "chaos1_rate:Rate 1", "chaos1_smooth:Smooth 1", "chaos2_rate:Rate 2", "chaos2_smooth:Smooth 2" }), 4, colours::warn));
        auto* macros = page->add (new Panel (st, "Macros", items ({ "macro1:Macro 1", "macro2:Macro 2", "macro3:Macro 3", "macro4:Macro 4" }), 4));
        auto* tip = makeLabel (*page, "LFO, Chaos, Macro and SEQ lanes do nothing until you route them on the MATRIX page. "
                                      "Retrig Note = one LFO per voice, restarted on each note; Free = one shared LFO.", 12.0f);
        page->layout = [=] (Rectangle<int> r)
        {
            tip->setBounds (r.removeFromBottom (26).reduced (6, 0));
            auto rows = split (r, { 1.0f, 1.0f, 0.7f }, false);
            auto a = split (rows[0], { 1.0f, 1.0f });
            lfo[0]->setBounds (a[0]); lfo[1]->setBounds (a[1]);
            auto b = split (rows[1], { 1.0f, 1.0f });
            lfo[2]->setBounds (b[0]); lfo[3]->setBounds (b[1]);
            auto c = split (rows[2], { 1.0f, 1.0f });
            chaos->setBounds (c[0]); macros->setBounds (c[1]);
        };
        tabs.addTab ("MOD", Colours::transparentBlack, page, false);
    }

    // ---------------- MATRIX ----------------
    {
        auto* page = new Page(); pages.add (page);
        auto* hSrc = makeLabel (*page, "SOURCE", 11.5f);
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
                hSrc->setBounds (h.removeFromLeft (w * 22 / 100));
                h.removeFromLeft (6);
                hDst->setBounds (h.removeFromLeft (w * 28 / 100));
                h.removeFromLeft (6);
                hVia->setBounds (h.removeFromRight (w * 18 / 100));
                hAmt->setBounds (h.reduced (6, 0));
            }
            const int h = r.getHeight() / 16;
            for (auto* row : rows) row->setBounds (r.removeFromTop (h));
        };
        tabs.addTab ("MATRIX", Colours::transparentBlack, page, false);
    }

    // ---------------- SEQ ----------------
    {
        auto* page = new Page(); pages.add (page);
        std::vector<Panel*> ps;
        std::vector<std::vector<TextButton*>> tools;
        for (int l = 0; l < 4; ++l)
        {
            const String n (l + 1);
            StringArray it;
            for (auto s : { "_on:On", "_len:Length", "_div:Division", "_mode:Range", "_swing:Swing", "_glide:Glide" })
                it.add ("seq" + n + s);
            ps.push_back (page->add (new Panel (st, "Lane " + n, it, 3, l % 2 ? colours::accent2 : colours::accent)));
            auto* lane = lanes.add (new StepLaneEditor (proc, l));
            page->addAndMakeVisible (lane);
            tools.push_back ({ makeButton (*page, "RANDOM", [lane] { lane->randomize (false); }, "New random values (locked steps stay)"),
                               makeButton (*page, "MUTATE", [lane] { lane->randomize (true); }, "Small random changes"),
                               makeButton (*page, "EUCLID", [lane] { lane->euclid(); }, "Spread hits evenly; click again for one more hit"),
                               makeButton (*page, "RAMP", [lane] { lane->ramp(); }),
                               makeButton (*page, "CLEAR", [lane] { lane->clear(); }) });
        }
        auto* tip = makeLabel (*page, "Draw with the mouse. Right-click a step to lock it (gold). Route a lane on the MATRIX page (Seq 1-4) to hear it.", 12.0f);
        auto* lanesPtr = &lanes;
        page->layout = [=] (Rectangle<int> r)
        {
            tip->setBounds (r.removeFromBottom (24).reduced (6, 0));
            const int h = r.getHeight() / 4;
            for (int l = 0; l < 4; ++l)
            {
                auto row = r.removeFromTop (h);
                ps[(size_t) l]->setBounds (row.removeFromLeft (300));
                auto tcol = row.removeFromRight (90).reduced (4, 8);
                const int bh = tcol.getHeight() / 5;
                for (auto* b : tools[(size_t) l]) b->setBounds (tcol.removeFromTop (bh).reduced (0, 2));
                (*lanesPtr)[l]->setBounds (row.reduced (6, 10));
            }
        };
        tabs.addTab ("SEQ", Colours::transparentBlack, page, false);
    }

    // ---------------- ARP ----------------
    {
        auto* page = new Page(); pages.add (page);
        auto* panel = page->add (new Panel (st, "Arpeggiator", items ({ "arp_on:On", "arp_mode:Mode", "arp_oct:Octaves", "arp_div:Division",
                                           "arp_len:Steps", "arp_gate:Gate", "arp_swing:Swing", "arp_latch:Latch" }), 8, colours::warn));
        arpGrid = page->add (new ArpGrid (proc));
        auto* tip = makeLabel (*page, "Click ON to make rests. Drag PITCH (+/-24 semitones) and VEL. TIE glides into the next step (use Legato voice mode). "
                                      "RATCHET plays 1-4 notes inside the step. Synced to the host when it plays.", 12.0f);
        page->layout = [=] (Rectangle<int> r)
        {
            panel->setBounds (r.removeFromTop (130));
            tip->setBounds (r.removeFromBottom (26).reduced (6, 0));
            arpGrid->setBounds (r.reduced (6, 10));
        };
        tabs.addTab ("ARP", Colours::transparentBlack, page, false);
    }

    // ---------------- FX ----------------
    {
        auto* page = new Page(); pages.add (page);
        auto* stp = page->add (new Panel (st, "1 Stereo", items ({ "fx_st_on:On", "fx_st_width:Width", "fx_st_bass:Mono Bass", "fx_st_haas:Haas ms" }), 4));
        auto* fl = page->add (new Panel (st, "2 Flanger", items ({ "fx_fl_on:On", "fx_fl_rate:Rate", "fx_fl_depth:Depth", "fx_fl_fb:Feedback", "fx_fl_delay:Delay", "fx_fl_mix:Mix" }), 6));
        auto* ph = page->add (new Panel (st, "3 Phaser", items ({ "fx_ph_on:On", "fx_ph_rate:Rate", "fx_ph_depth:Depth", "fx_ph_fb:Feedback", "fx_ph_center:Center", "fx_ph_stages:Stages", "fx_ph_mix:Mix" }), 7));
        auto* dl = page->add (new Panel (st, "4 Delay", items ({ "fx_dl_on:On", "fx_dl_sync:Sync", "fx_dl_pp:Ping-Pong", "fx_dl_div:Division", "fx_dl_time:Time ms",
                                        "fx_dl_fb:Feedback", "fx_dl_lowcut:Low Cut", "fx_dl_highcut:High Cut", "fx_dl_mix:Mix" }), 5));
        auto* rn = page->add (new Panel (st, "5 Resonator (Rings)", items ({ "fx_rn_on:On", "fx_rn_model:Model", "fx_rn_poly:Poly", "fx_rn_follow:Follow Keys", "fx_rn_note:Note",
                                        "fx_rn_structure:Structure", "fx_rn_bright:Brightness", "fx_rn_damping:Damping", "fx_rn_pos:Position", "fx_rn_mix:Mix" }), 5, colours::warn));
        auto* cl = page->add (new Panel (st, "6 Granular (Clouds)", items ({ "fx_gr_on:On", "fx_gr_mode:Mode", "fx_gr_quality:Quality", "fx_gr_freeze:Freeze",
                                        "fx_gr_pos:Position", "fx_gr_size:Size", "fx_gr_pitch:Pitch", "fx_gr_density:Density", "fx_gr_texture:Texture",
                                        "fx_gr_spread:Spread", "fx_gr_fb:Feedback", "fx_gr_reverb:Reverb", "fx_gr_mix:Mix" }), 7, colours::warn));
        page->layout = [=] (Rectangle<int> r)
        {
            auto rows = split (r, { 0.8f, 1.5f, 1.5f }, false);
            auto a = split (rows[0], { 0.8f, 1.2f, 1.4f });
            stp->setBounds (a[0]); fl->setBounds (a[1]); ph->setBounds (a[2]);
            auto b = split (rows[1], { 1.0f, 1.0f });
            dl->setBounds (b[0]); rn->setBounds (b[1]);
            cl->setBounds (rows[2]);
        };
        tabs.addTab ("FX", Colours::transparentBlack, page, false);
    }

    // ---------------- ORB ----------------
    {
        auto* page = new Page(); pages.add (page);
        orbPad = page->add (new OrbPad (proc));
        auto* panel = page->add (new Panel (st, "ORB", items ({ "orb_on:ORB On", "orb_x:X", "orb_y:Y" }), 3, colours::accent2));
        const char* names[] = { "A", "B", "C", "D" };
        std::vector<std::array<TextButton*, 3>> corner;
        for (int c = 0; c < 4; ++c)
        {
            corner.push_back ({ makeButton (*page, String ("CAPTURE ") + names[c], [this, c] { proc.captureCorner (c); orbPad->repaint(); },
                                            "Store the current sound in this corner"),
                                makeButton (*page, "LOAD", [this, c]
                                {
                                    chooser = std::make_unique<FileChooser> ("Load a preset into this corner", proc.presetFolder(), "*.sgpreset");
                                    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this, c] (const FileChooser& fc)
                                    {
                                        if (fc.getResult().existsAsFile()) proc.loadPresetIntoCorner (c, fc.getResult());
                                        orbPad->repaint();
                                    });
                                }, "Load a saved preset into this corner"),
                                makeButton (*page, "CLEAR", [this, c] { proc.clearCorner (c); orbPad->repaint(); }, "Empty corner: plays the current knobs") });
        }
        auto* tip = makeLabel (*page, "CAPTURE a sound (or LOAD a preset) into each corner, turn ORB on and move the point. "
                                      "Knobs blend between the corners; menus and switches follow the closest corner. "
                                      "X and Y are automatable and can be MIDI-learned in your DAW.", 12.5f, colours::text);
        tip->setJustificationType (Justification::topLeft);
        page->layout = [=] (Rectangle<int> r)
        {
            auto left = r.removeFromLeft (r.getHeight());
            orbPad->setBounds (left);
            r.removeFromLeft (12);
            panel->setBounds (r.removeFromTop (130));
            r.removeFromTop (10);
            for (int c = 0; c < 4; ++c)
            {
                auto row = r.removeFromTop (40).reduced (6, 4);
                corner[(size_t) c][0]->setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 0));
                corner[(size_t) c][1]->setBounds (row.removeFromLeft (row.getWidth() / 2).reduced (2, 0));
                corner[(size_t) c][2]->setBounds (row.reduced (2, 0));
            }
            tip->setBounds (r.reduced (8, 16));
        };
        tabs.addTab ("ORB", Colours::transparentBlack, page, false);
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
        stars = brand::starfield (getWidth(), getHeight(), 7, 0.9f);
    g.drawImageAt (stars, 0, 0);

    // header: drop glyph + product name in the display face, then the Calabi Yau signature
    auto header = getLocalBounds().removeFromTop (kHeaderH).reduced (18, 0).toFloat();
    brand::drop (g, { header.getX() + 12.0f, header.getCentreY() - 2.0f }, 30.0f);
    g.setColour (cosmos::starlight);
    g.setFont (fonts::display (19.0f));
    g.drawText ("SYNTHGULARITY", header.withTrimmedLeft (34.0f).withHeight (header.getHeight() * 0.62f), Justification::bottomLeft);
    auto sig = header.withTrimmedLeft (36.0f).withTrimmedTop (header.getHeight() * 0.64f).withHeight (16.0f);
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

void SynthgularityEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (kHeaderH).reduced (12, 12);
    r.removeFromTop (10);
    header.removeFromLeft (360);
    masterKnob->setBounds (header.removeFromRight (60).withTrimmedTop (-6));
    voicesLabel.setBounds (header.removeFromRight (110));
    auto presetArea = header.withSizeKeepingCentre (jmin (520, header.getWidth()), 30);
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
}

void SynthgularityEditor::timerCallback()
{
    const int tab = tabs.getCurrentTabIndex();
    if (tab == 0) { for (auto* v : tableViews) if (v) v->repaint(); }
    if (tab == 1 && sampleView) sampleView->repaint();
    if (tab == 5) for (auto* l : lanes) l->repaint();
    if (tab == 6 && arpGrid) arpGrid->repaint();
    if (tab == 8 && orbPad) orbPad->repaint();
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
        presetBox.addSectionHeading ("User");
        for (int i = 0; i < userPresets.size(); ++i)
            presetBox.addItem (userPresets[i].getRelativePathFrom (proc.presetFolder()).upToLastOccurrenceOf (".", false, false), 1000 + i);
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
    auto* w = new AlertWindow ("Save preset", "Stores every knob, the SEQ and ARP steps, the ORB corners and the sample path.", MessageBoxIconType::NoIcon, this);
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

void SynthgularityEditor::showError (const String& message)
{
    AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Synthgularity", message);
}
