#include "WcEditor.h"
#include "WcData.h"

using namespace wc;
using namespace wc::ui;

static constexpr int kTopH = 52, kKeysH = 64;
static const float kZooms[] = { 0.5f, 0.65f, 0.75f, 0.85f, 1.0f, 1.15f, 1.3f, 1.5f };
static constexpr int kNumZooms = 8;

WcEditor::WcEditor (WcProcessor& p)
    : AudioProcessorEditor (p), proc (p), rack (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&look);
    logo = juce::ImageCache::getFromMemory (WcData::isologo_png, WcData::isologo_pngSize);   // official Calabi Yau isologo
    proc.setFactoryData (WcData::factory_xml, WcData::factory_xmlSize);

    addAndMakeVisible (patchBox);
    patchBox.setTooltip ("Patches: built-in starting points and your saved patches");
    patchBox.onChange = [this]
    {
        const int id = patchBox.getSelectedId();
        if (id >= 1 && id < 1000) proc.loadBuiltinPatch (id - 1);
        else if (id >= 5000) proc.loadFactoryAt (id - 5000);
        else if (id >= 1000)
        {
            auto files = proc.patchFolder().findChildFiles (juce::File::findFiles, false, "*.wcpatch");
            files.sort();
            if (id - 1000 < files.size()) proc.loadPatchFile (files[id - 1000]);
        }
    };
    for (auto* b : { &loadButton, &saveButton, &addButton, &cablesButton }) addAndMakeVisible (*b);
    loadButton.setTooltip ("Open a patch file (.wcpatch)");
    saveButton.setTooltip ("Save the rack (modules, cables and every knob) as a patch file");
    addButton.setTooltip ("Add a module to the rack");
    cablesButton.setTooltip ("Cable opacity: solid / see-through / hidden while you tweak knobs");
    loadButton.onClick = [this]
    {
        if (proc.demo()) { chooseLicenseFile(); return; }
        chooser = std::make_unique<juce::FileChooser> ("Open patch", proc.patchFolder(), "*.wcpatch");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile()) proc.loadPatchFile (fc.getResult());
        });
    };
    saveButton.onClick = [this]
    {
        if (proc.demo()) { chooseLicenseFile(); return; }
        chooser = std::make_unique<juce::FileChooser> ("Save patch", proc.patchFolder().getChildFile (proc.patchName + ".wcpatch"), "*.wcpatch");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
        {
            auto f = fc.getResult();
            if (f == juce::File()) return;
            proc.savePatchFile (f.withFileExtension ("wcpatch"));
            refreshPatchMenu();
        });
    };
    addButton.onClick = [this] { addModuleMenu(); };
    cablesButton.onClick = [this]
    {
        rack.cableAlpha = rack.cableAlpha > 0.8f ? 0.35f : (rack.cableAlpha > 0.2f ? 0.0f : 0.9f);
        rack.content.repaint();
    };

    addAndMakeVisible (zoomButton);
    zoomButton.setTooltip ("Zoom 50 - 150 %: hold Ctrl (Cmd on a Mac) and turn the wheel over the rack."
                           "\nThe wheel on its own scrolls the rack, and never turns a knob."
                           "\nClick here to go back to 100 %.");
    zoomButton.onClick = [this] { setZoom (1.0f); };
    rack.onZoomGesture = [this] (float amount, juce::Point<float> pos) { setZoom (proc.uiZoom * std::exp (amount * 1.6f), pos, true); };
    setZoom (proc.uiZoom);

    addAndMakeVisible (skinBox);
    skinBox.addItemList ({ "CLASSIC", "NEON VOID" }, 1);
    skinBox.setTooltip ("Panel finish: Classic aluminium or Neon Void");
    skinBox.setSelectedItemIndex (juce::jlimit (0, 1, proc.uiSkin), juce::dontSendNotification);
    skinBox.onChange = [this]
    {
        proc.uiSkin = juce::jlimit (0, 1, skinBox.getSelectedItemIndex());
        applySkin();
    };
    applySkin();

    for (auto* b : { &prevButton, &nextButton })
    {
        addAndMakeVisible (*b);
        b->setConnectedEdges (b == &prevButton ? juce::Button::ConnectedOnRight : juce::Button::ConnectedOnLeft);
    }
    prevButton.setTooltip ("Previous preset in the factory library");
    nextButton.setTooltip ("Next preset in the factory library");
    prevButton.onClick = [this] { proc.stepFactory (-1); refreshPatchMenu(); };
    nextButton.onClick = [this] { proc.stepFactory (1); refreshPatchMenu(); };

    addAndMakeVisible (licenseButton);
    licenseButton.onClick = [this] { chooseLicenseFile(); };
    updateLicenseUi();

    viewport.setViewedComponent (&rack, false);
    viewport.setScrollBarsShown (true, true);
    viewport.setScrollBarThickness (10);
    addAndMakeVisible (viewport);

    keyboard.setAvailableRange (24, 108);
    keyboard.setLowestVisibleKey (36);
    keyboard.setKeyWidth (18.0f);
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, col::accent.withAlpha (0.6f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, col::accent.withAlpha (0.25f));
    addAndMakeVisible (keyboard);

    // the value readout: a child of the editor, so it does not scale with the rack
    addChildComponent (readout);
    readout.suppressed = [this] { return rack.readoutSuppressed(); };
    readout.visibleArea = [this] { return viewport.getBounds(); };
    rack.readout = &readout;

    proc.addChangeListener (this);
    refreshPatchMenu();
    setResizable (true, true);
    setResizeLimits (900, 560, 2600, 1800);
    setSize (1360, 640);
}

WcEditor::~WcEditor()
{
    rack.readout = nullptr;
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void WcEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    patchBox.setText (proc.patchName, juce::dontSendNotification);
    updateLicenseUi();
    resized();
}

void WcEditor::setZoom (float z, juce::Point<float> anchorInRack, bool keepAnchor)
{
    const float old = proc.uiZoom;
    const float nz = juce::jlimit (0.5f, 1.5f, z);
    if (std::fabs (nz - old) < 1.0e-4f && zoomButton.getButtonText().isNotEmpty()) { rack.setZoom (nz); return; }
    // the rack is the content scaled by the zoom, so this is where the cursor sits on the unscaled rack
    const auto contentPt = anchorInRack / juce::jmax (0.01f, old);
    const auto inViewport = anchorInRack - viewport.getViewPosition().toFloat();
    proc.uiZoom = nz;
    rack.setZoom (nz);
    resized();
    if (keepAnchor)
    {
        const auto want = contentPt * nz - inViewport;
        viewport.setViewPosition (juce::roundToInt (want.x), juce::roundToInt (want.y));
    }
    zoomButton.setButtonText (juce::String (juce::roundToInt (nz * 100.0f)) + " %");
}

void WcEditor::applySkin()
{
    wc::ui::skin::current = juce::jlimit (0, 1, proc.uiSkin);
    rack.repaint();
    for (auto* c : rack.getChildren()) c->repaint();
    repaint();
}

void WcEditor::updateLicenseUi()
{
    const bool demo = proc.demo();
    licenseButton.setButtonText (demo ? "DEMO" : "LICENSED");
    licenseButton.setTooltip (demo ? "Demo mode: two oscillators, presets disabled.\nClick to pick your " + wc::License::keyFileName()
                                   : proc.license.summary());
    licenseButton.setColour (juce::TextButton::textColourOffId, demo ? juce::Colour (0xffe8a33a) : wc::ui::col::ledGreen);
    loadButton.setEnabled (! demo);
    saveButton.setEnabled (! demo);
    prevButton.setEnabled (! demo);
    nextButton.setEnabled (! demo);
}

void WcEditor::chooseLicenseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Open your West Coast Modular license key",
                                                   juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.key");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        proc.recheckLicense (f);
        if (proc.license.licensed())
        {
            // keep it where the plugin will find it next time
            auto dest = proc.patchFolder().getParentDirectory().getChildFile (wc::License::keyFileName());
            dest.getParentDirectory().createDirectory();
            if (! dest.existsAsFile()) f.copyFileTo (dest);
        }
        updateLicenseUi();
        refreshPatchMenu();
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "West Coast Modular",
                                                     proc.license.licensed() ? proc.license.summary() + "\nThank you!"
                                                                             : "That key file could not be validated.\nThe plugin stays in demo mode.");
    });
}

void WcEditor::refreshPatchMenu()
{
    patchBox.clear (juce::dontSendNotification);
    auto* root = patchBox.getRootMenu();
    root->addSectionHeader ("Built-in");
    const auto names = WcProcessor::builtinPatchNames();
    for (int i = 0; i < names.size(); ++i) patchBox.addItem (names[i], 1 + i);
    if (proc.factory.getNumChildren() > 0 && ! proc.demo())
    {
        root->addSectionHeader ("Factory library (" + juce::String (proc.factory.getNumChildren()) + ")");
        juce::StringArray cats;
        for (auto pr : proc.factory) cats.addIfNotAlreadyThere (pr["cat"].toString());
        for (auto& c : cats)
        {
            juce::PopupMenu sub;
            int i = 0;
            for (auto pr : proc.factory)
            {
                if (pr["cat"].toString() == c) sub.addItem (5000 + i, pr["name"].toString());
                ++i;
            }
            root->addSubMenu (c, sub);
        }
    }
    auto files = proc.demo() ? juce::Array<juce::File>() : proc.patchFolder().findChildFiles (juce::File::findFiles, false, "*.wcpatch");
    files.sort();
    if (! files.isEmpty())
    {
        root->addSectionHeader ("My patches");
        for (int i = 0; i < files.size(); ++i) patchBox.addItem (files[i].getFileNameWithoutExtension(), 1000 + i);
    }
    patchBox.setText (proc.patchName, juce::dontSendNotification);
}

void WcEditor::addModuleMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Add to the rack");
    auto addType = [&] (int first, int count, const juce::String& title)
    {
        bool any = false;
        for (int i = 0; i < count; ++i)
            if (! proc.isPresent (first + i))
            {
                const bool ok = proc.canAddOscillator (first + i);
                m.addItem (1 + first + i, title + "  (" + instName (first + i) + ")" + (ok ? "" : "  - demo: 2 oscillators"), ok);
                any = true;
            }
        if (! any) m.addItem (999, title + "  - all slots in use", false);
    };
    addType (kInst218, kMax218, "KB-218 Touch Activated Voltage Source");
    addType (kInst242, kMax242, "PP-242 Programmable Pulser");
    addType (kInst245, kMax245, "SV-245 Sequential Voltage Source");
    addType (kInst248, kMax248, "MF-248 Multiple Arbitrary Function Generator");
    addType (kInst250, kMax250, "AF-250 Dual Arbitrary Function Generator");
    addType (kInst252, kMax252, "RG-252 Polyphonic Rhythm Generator");
    addType (kInst266, kMax266, "SU-266 Source of Uncertainty");
    addType (kInst230, kMax230, "EF-230 Triple Envelope Follower");
    addType (kInst264, kMax264, "QS-264 Quad Sample & Hold");
    addType (kInst257, kMax257, "VP-257 Dual Voltage Processor");
    addType (kInst281, kMax281, "QF-281 Quad Function Generator");
    addType (kInst258, kMax258, "DO-258 Dual Oscillator");
    addType (kInst259, kMax259, "CW-259 Complex Waveform Generator");
    addType (kInst285, kMax285, "FS-285 Frequency Shifter / Balanced Modulator");
    addType (kInst296, kMax296, "PS-296 Programmable Spectral Processor");
    addType (kInst277, kMax277, "SD-277 Signal Delay Unit");
    addType (kInst292, kMax292, "LG-292 Quad Lowpass Gate");
    addType (kInst207, kMax207, "MX-207 Mixer / Preamplifier");
    addType (kInst267, kMax267, "US-267 Uncertainty Source / Dual Filter");
    addType (kInst275, kMax275, "RV-275 Dual Equalizer / Reverb");
    addType (kInst284, kMax284, "QE-284 Quad Envelope Generator");
    addType (kInst291, kMax291, "TM-291 Triple Morphing Filter");
    addType (kInst666, kMax666, "SC-666 Oscilloscope");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addButton), [this] (int r)
    {
        if (r >= 1 && r <= kNumInst && isRemovable (r - 1)) proc.setPresent (r - 1, true);
    });
}

void WcEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    auto top = getLocalBounds().removeFromTop (kTopH);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1d1e23), 0, 0, juce::Colour (0xff131418), 0, (float) kTopH, false));
    g.fillRect (top);
    g.setColour (col::accent.withAlpha (0.5f));
    g.drawHorizontalLine (kTopH - 1, 0.0f, (float) getWidth());
    if (logo.isValid())
        g.drawImageWithin (logo, 12, 8, 36, 36, juce::RectanglePlacement::centred);
    g.setColour (juce::Colour (0xffeeeeee));
    g.setFont (WcLook::brand (16.0f));
    g.drawText ("WEST COAST MODULAR", 58, 9, 320, 20, juce::Justification::left);
    g.setColour (col::accent);
    g.setFont (WcLook::label (10.5f));
    g.drawText (juce::String ("CALABI YAU  -  v" WC_VERSION_STRING) + (proc.demo() ? "  -  DEMO" : ""), 58, 29, 320, 14, juce::Justification::left);
}

void WcEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (kTopH).reduced (10, 11);
    top.removeFromLeft (300);
    zoomButton.setBounds (top.removeFromRight (62));
    top.removeFromRight (6);
    skinBox.setBounds (top.removeFromRight (104));
    top.removeFromRight (6);
    cablesButton.setBounds (top.removeFromRight (74));
    top.removeFromRight (6);
    addButton.setBounds (top.removeFromRight (92));
    top.removeFromRight (12);
    licenseButton.setBounds (top.removeFromRight (86));
    top.removeFromRight (12);
    saveButton.setBounds (top.removeFromRight (56));
    top.removeFromRight (5);
    loadButton.setBounds (top.removeFromRight (56));
    top.removeFromRight (5);
    nextButton.setBounds (top.removeFromRight (26));
    patchBox.setBounds (top.removeFromRight (juce::jmin (230, juce::jmax (60, top.getWidth() - 26))));
    prevButton.setBounds (top.removeFromRight (26));
    keyboard.setBounds (r.removeFromBottom (kKeysH));
    viewport.setBounds (r);
    const auto ps = rack.preferredSize();
    rack.setSize (ps.x, ps.y);
}
