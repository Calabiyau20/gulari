#pragma once
#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/Widgets.h"
#include "ui/Widgets2.h"

class SynthgularityEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, public pg::ui::ModHost, private juce::Timer
{
public:
    explicit SynthgularityEditor (SynthgularityProcessor&);
    ~SynthgularityEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int kChipsH = 34;

    // ModHost: drag a source chip onto any knob
    bool canModulate (const juce::String& paramId) const override;
    void dropModulation (int source, const juce::String& paramId) override;
    std::vector<pg::ui::ModRing> ringsFor (const juce::String& paramId) const override;
    float liveValue (const juce::String& paramId) const override;
    void showModMenu (const juce::String& paramId, juce::Component& knob) override;
    juce::Colour colourOf (int source) const override;

private:
    void timerCallback() override;
    void refreshPresetMenu();
    void choosePreset (int id);
    void savePresetDialog();
    void loadSampleDialog();
    void loadWavetableDialog (int osc);
    void loadFileDialog (const juce::String& title, std::function<bool (const juce::File&, juce::String&)> loader);
    void showError (const juce::String& message);
    void setSkin (int index);

    // tabs that fade their page in when you switch
    struct AnimatedTabs : public juce::TabbedComponent
    {
        AnimatedTabs() : juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop) {}
        void currentTabChanged (int, const juce::String&) override
        {
            // the editor timer fades the page in (animating bounds here would capture stale sizes)
            if (auto* c = getCurrentContentComponent()) c->setAlpha (0.0f);
        }
    };

    SynthgularityProcessor& proc;
    static constexpr int kHeaderH = 84;
    juce::Image stars, logo, logoGrid, isologo;
    juce::Rectangle<float> isologoArea;
    double animT = 0.0;
    pg::ui::AboutOverlay about;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    pg::ui::SynthLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    // header
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, saveButton { "SAVE" }, initButton { "INIT" };
    std::unique_ptr<pg::ui::MasterSection> master;
    std::unique_ptr<pg::ui::GonioMini> gonio;
    juce::OwnedArray<pg::ui::SourceChip> chips;
    std::vector<std::vector<pg::ui::ParamControl*>> tabKnobs;   // per tab, for rings and drag glow
    juce::Label voicesLabel;

    AnimatedTabs tabs;
    juce::OwnedArray<juce::Component> pages;

    // widgets that need repaint from the timer
    pg::ui::SampleView* sampleView = nullptr;
    pg::ui::WavetableView* tableViews[3] {};
    std::vector<std::vector<juce::Component*>> live;   // per tab: views repainted by the timer
    juce::OwnedArray<pg::ui::StepLaneEditor> lanes;
    juce::Array<pg::ui::EnvelopeView*> envViews;
    pg::ui::ArpGrid* arpGrid = nullptr;
    juce::OwnedArray<juce::Component> owned;   // extra buttons etc.
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::File> userPresets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthgularityEditor)
};
