#pragma once
#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/Widgets.h"

class SynthgularityEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SynthgularityEditor (SynthgularityProcessor&);
    ~SynthgularityEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshPresetMenu();
    void choosePreset (int id);
    void savePresetDialog();
    void loadSampleDialog();
    void loadWavetableDialog (int osc);
    void showError (const juce::String& message);

    SynthgularityProcessor& proc;
    static constexpr int kHeaderH = 72;
    juce::Image stars, logo, logoGrid, isologo;
    juce::Rectangle<float> isologoArea;
    pg::ui::AboutOverlay about;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    pg::ui::SynthLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // header
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, initButton { "INIT" };
    std::unique_ptr<pg::ui::ParamControl> masterKnob;
    juce::Label voicesLabel;

    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::OwnedArray<juce::Component> pages;

    // widgets that need repaint from the timer
    pg::ui::SampleView* sampleView = nullptr;
    pg::ui::WavetableView* tableViews[2] {};
    juce::OwnedArray<pg::ui::StepLaneEditor> lanes;
    juce::Array<pg::ui::EnvelopeView*> envViews;
    pg::ui::ArpGrid* arpGrid = nullptr;
    pg::ui::OrbPad* orbPad = nullptr;
    juce::OwnedArray<juce::Component> owned;   // extra buttons etc.
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::File> userPresets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthgularityEditor)
};
