#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "WcProcessor.h"
#include "ui/WcLook.h"
#include "ui/RackView.h"

class WcEditor : public juce::AudioProcessorEditor, private juce::ChangeListener
{
public:
    explicit WcEditor (WcProcessor&);
    ~WcEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WcProcessor& proc;
    wc::ui::WcLook look;
    juce::TooltipWindow tooltips { this, 600 };
    juce::Image logo;
    juce::ComboBox patchBox, zoomBox;
    juce::TextButton loadButton { "LOAD" }, saveButton { "SAVE" }, addButton { "+ MODULE" }, cablesButton { "CABLES" };
    juce::Viewport viewport;
    wc::ui::RackView rack;
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;

    void refreshPatchMenu();
    void addModuleMenu();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
};
