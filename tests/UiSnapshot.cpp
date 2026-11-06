// Renders every page of the editor to PNG (run under Xvfb on Linux).
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::String ("."));
    outDir.createDirectory();
    SynthgularityProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    proc.loadFactoryPreset (argc > 2 ? juce::String (argv[2]).getIntValue() : 1);
    if (argc > 3) { juce::String e; proc.loadSample (juce::File (juce::String (argv[3])), e); }
    // play a chord briefly so displays have content
    juce::AudioBuffer<float> b (2, 256);
    for (int i = 0; i < 80; ++i)
    {
        juce::MidiBuffer m;
        if (i == 0) for (int n : { 48, 55, 60 }) m.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
        proc.processBlock (b, m);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (1180, 780);
    auto* tabs = [&]() -> juce::TabbedComponent*
    {
        for (auto* c : ed->getChildren()) if (auto* t = dynamic_cast<juce::TabbedComponent*> (c)) return t;
        return nullptr;
    }();
    for (int t = 0; t < tabs->getNumTabs(); ++t)
    {
        tabs->setCurrentTabIndex (t);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile (juce::String (t) + "_" + tabs->getTabNames()[t].replace (" / ", "_") + ".png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        png.writeImageToStream (img, os);
    }
    ed.reset();
    return 0;
}
