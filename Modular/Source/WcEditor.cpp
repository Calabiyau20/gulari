#include "WcEditor.h"
#include "WcData.h"

using namespace wc;
using namespace wc::ui;

static constexpr int kTopH = 52, kKeysH = 64;

WcEditor::WcEditor (WcProcessor& p)
    : AudioProcessorEditor (p), proc (p), rack (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&look);
    logo = juce::ImageCache::getFromMemory (WcData::isologo_png, WcData::isologo_pngSize);   // official Calabi Yau isologo

    addAndMakeVisible (patchBox);
    patchBox.setTooltip ("Patches: built-in starting points and your saved patches");
    patchBox.onChange = [this]
    {
        const int id = patchBox.getSelectedId();
        if (id >= 1 && id < 1000) proc.loadBuiltinPatch (id - 1);
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
        chooser = std::make_unique<juce::FileChooser> ("Open patch", proc.patchFolder(), "*.wcpatch");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        {
            if (fc.getResult().existsAsFile()) proc.loadPatchFile (fc.getResult());
        });
    };
    saveButton.onClick = [this]
    {
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

    addAndMakeVisible (zoomBox);
    zoomBox.addItemList ({ "75 %", "85 %", "100 %", "115 %", "130 %" }, 1);
    zoomBox.setTooltip ("Zoom");
    static const float zooms[] = { 0.75f, 0.85f, 1.0f, 1.15f, 1.3f };
    int zi = 2;
    for (int i = 0; i < 5; ++i) if (std::fabs (zooms[i] - proc.uiZoom) < 0.01f) zi = i;
    zoomBox.setSelectedItemIndex (zi, juce::dontSendNotification);
    zoomBox.onChange = [this]
    {
        proc.uiZoom = zooms[juce::jlimit (0, 4, zoomBox.getSelectedItemIndex())];
        rack.setZoom (proc.uiZoom);
        resized();
    };
    rack.setZoom (proc.uiZoom);

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

    proc.addChangeListener (this);
    refreshPatchMenu();
    setResizable (true, true);
    setResizeLimits (900, 560, 2600, 1800);
    setSize (1360, 640);
}

WcEditor::~WcEditor()
{
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void WcEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    patchBox.setText (proc.patchName, juce::dontSendNotification);
    resized();
}

void WcEditor::refreshPatchMenu()
{
    patchBox.clear (juce::dontSendNotification);
    auto* root = patchBox.getRootMenu();
    root->addSectionHeader ("Built-in");
    const auto names = WcProcessor::builtinPatchNames();
    for (int i = 0; i < names.size(); ++i) patchBox.addItem (names[i], 1 + i);
    auto files = proc.patchFolder().findChildFiles (juce::File::findFiles, false, "*.wcpatch");
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
            if (! proc.isPresent (first + i)) { m.addItem (1 + first + i, title + "  (" + instName (first + i) + ")"); any = true; }
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
    g.drawText ("CALABI YAU  -  v" WC_VERSION_STRING, 58, 29, 320, 14, juce::Justification::left);
}

void WcEditor::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (kTopH).reduced (10, 11);
    top.removeFromLeft (330);
    zoomBox.setBounds (top.removeFromRight (84));
    top.removeFromRight (8);
    cablesButton.setBounds (top.removeFromRight (78));
    top.removeFromRight (8);
    addButton.setBounds (top.removeFromRight (96));
    top.removeFromRight (16);
    saveButton.setBounds (top.removeFromRight (60));
    top.removeFromRight (6);
    loadButton.setBounds (top.removeFromRight (60));
    top.removeFromRight (6);
    patchBox.setBounds (top.removeFromRight (juce::jmin (260, top.getWidth())));
    keyboard.setBounds (r.removeFromBottom (kKeysH));
    viewport.setBounds (r);
    const auto ps = rack.preferredSize();
    rack.setSize (ps.x, ps.y);
}
