#pragma once
#include "RProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/Widgets.h"
#include "ui/Widgets2.h"
#include "ui/RWidgets.h"

class ResonatorEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit ResonatorEditor (ResonatorProcessor&);
    ~ResonatorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void setSkin (int index);
    void showTab (int index) { tabs.setCurrentTabIndex (index); }
    static constexpr int kHeaderH = 84, kChipsH = 40, kAnalyzerH = 168;

private:
    void timerCallback() override;
    void refreshPresetMenu();
    void choosePreset (int id);
    void savePresetDialog();

    struct AnimatedTabs : public juce::TabbedComponent
    {
        AnimatedTabs() : juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop) {}
        void currentTabChanged (int, const juce::String&) override
        {
            if (auto* c = getCurrentContentComponent()) c->setAlpha (0.0f);   // the editor timer fades it in
        }
    };

    ResonatorProcessor& proc;
    struct LookAndFeel : public pg::ui::SynthLookAndFeel
    {
        int getTabButtonBestWidth (juce::TabBarButton& b, int) override
        { return juce::jmax (110, (int) (pg::ui::fonts::label (11.5f).getStringWidthFloat (b.getButtonText()) * 1.15f) + 40); }
    } lnf;
    juce::TooltipWindow tooltips { this, 600 };
    juce::Image stars, logoGrid, isologo;
    double animT = 0.0;

    juce::ComboBox presetBox, skinBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, initButton { "INIT" },
                     loadButton { "LOAD" }, foldersButton { "FOLDERS" };
    void loadPresetDialog();
    void foldersMenu();
    std::unique_ptr<cyr::ui::ModKnob> masterKnob;
    juce::Label voicesLabel;

    AnimatedTabs tabs;
    juce::OwnedArray<juce::Component> pages;
    juce::OwnedArray<cyr::ui::SourceChip> chips;
    std::unique_ptr<pg::ui::AnalyzerStrip> analyzer;
    std::unique_ptr<juce::MidiKeyboardComponent> keyboard;

    std::vector<std::vector<juce::Component*>> live;   // per tab: repainted by the timer
    std::vector<std::vector<cyr::ui::ModKnob*>> knobs;  // per tab: modulation rings
    std::vector<cyr::ui::SurfaceView*> surfaces;
    std::vector<cyr::ui::ModKnob*> oscKnobs;
    cyr::ui::MatrixView* matrixView = nullptr;
    juce::Array<juce::File> userPresets;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonatorEditor)
};
