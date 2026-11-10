#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "WcParams.h"
#include "rack/Rack.h"

class WcProcessor : public juce::AudioProcessor, public juce::ChangeBroadcaster
{
public:
    WcProcessor();
    ~WcProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Calabi Yau West Coast Modular"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return patchName; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- patching (message thread) ----
    const std::vector<wc::Cable>& getCables() const { return cables; }
    bool isPresent (int inst) const { return present[(size_t) inst]; }
    // connects an output to an input; replaces whatever was in that input. false if the jacks don't match
    bool connect (int srcInst, int srcJack, int dstInst, int dstJack, int colour = -1);
    void disconnectInput (int inst, int inJack);
    void disconnectOutput (int inst, int outJack);
    void setPresent (int inst, bool on);
    int nextColour (bool banana);

    // ---- patches ----
    static juce::StringArray builtinPatchNames();
    void loadBuiltinPatch (int index);
    juce::ValueTree patchToTree() const;
    void patchFromTree (const juce::ValueTree& t);
    bool savePatchFile (const juce::File& f);
    bool loadPatchFile (const juce::File& f);
    juce::File patchFolder() const;
    // ---- compact presets (only the non-default parameters) and the factory library ----
    juce::ValueTree presetToTree (const juce::String& name, const juce::String& category) const;
    void loadPresetTree (const juce::ValueTree& preset);
    void setFactoryData (const void* data, int size);         // the embedded factory.xml (parsed once)
    juce::ValueTree factory { "WCFACTORY" };
    void resetAll();                                           // all parameters to default, no cables, empty 252

    // ---- 248 presets (12 memories, kept with the plugin state) ----
    void marfSave (int slot);
    void marfLoad (int slot);
    juce::ValueTree marfPresets { "MARFPRESETS" };
    juce::String patchName { "Init Voice" };
    float uiZoom = 1.0f;

    juce::AudioProcessorValueTreeState apvts;
    wc::Rack rack;
    juce::MidiKeyboardState keyboardState;

private:
    std::vector<wc::Cable> cables;
    std::array<bool, wc::kNumInst> present {};
    int colourCounter = 0;
    void commit();
    void resetParams();
    void setParam (const juce::String& id, float plain);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WcProcessor)
};
