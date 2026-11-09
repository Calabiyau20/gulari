// Renders every tab of the Resonator editor to PNG: RUiSnapshot <outDir> [presetIndex] [skin]
#include "REditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (juce::String (argc > 1 ? argv[1] : "/tmp/cyr_ui"));
    out.createDirectory();
    const int preset = argc > 2 ? juce::String (argv[2]).getIntValue() : 0;
    const int skin = argc > 3 ? juce::String (argv[3]).getIntValue() : 0;
    ResonatorProcessor proc;
    proc.uiSkin = skin;
    proc.prepareToPlay (48000.0, 512);
    proc.loadFactoryPreset (preset);
    // a drag-and-drop route made like a user would, so the rings show
    proc.addModulation (cyr::MS_Lfo3, cyr::oscb_ru, 0.4f);
    // play a chord so the scopes and live dots have something to show
    for (int b = 0; b < 160; ++b)
    {
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        if (b == 0) for (int nn : { 48, 55, 60, 64 }) midi.addEvent (juce::MidiMessage::noteOn (1, nn, (juce::uint8) 100), 0);
        proc.processBlock (buf, midi);
    }
    std::unique_ptr<ResonatorEditor> ed (dynamic_cast<ResonatorEditor*> (proc.createEditor()));
    ed->setSize (1320, 900);
    for (int tab = 0; tab < 7; ++tab)
    {
        ed->showTab (tab);
        for (int k = 0; k < 6; ++k)
        {
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            proc.processBlock (buf, midi);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
        }
        // the page fades in: force it fully visible
        for (int i = 0; i < ed->getNumChildComponents(); ++i)
            if (auto* t = dynamic_cast<juce::TabbedComponent*> (ed->getChildComponent (i)))
                if (auto* c = t->getCurrentContentComponent()) c->setAlpha (1.0f);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = out.getChildFile ("tab" + juce::String (tab) + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
    }
    ed.reset();
    return 0;
}
