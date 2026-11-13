#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "WcParams.h"
#include "rack/Rack.h"
#include "WcLicense.h"

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
    int uiSkin = 0;                       // 0 Classic, 1 Neon Void

    // ---- licensing ----
    wc::License license;
    bool demo() const noexcept { return ! license.licensed(); }
    int oscillatorsInRack() const;        // 259 + 258 slots currently in the rack
    bool canAddOscillator (int inst) const;
    void recheckLicense (const juce::File& extra = {});
    // factory library navigation (prev / next in the header)
    int factoryIndex = -1;
    void loadFactoryAt (int index);
    void stepFactory (int delta);

    // ---- MIDI Learn: a CC moves one parameter, from any channel ----
    // 128 atomic slots, CC -> parameter index (-1 free): the message thread writes them, processBlock reads them.
    // A mapped CC moves its parameter and goes no further (not to the MIDI module, not to the 218). While the
    // parameter's module is out of the rack the mapping sleeps: kept, and the CC goes on as if it were not there.
    // Waiting for a CC to learn is the editor's business; the processor only says which one came in last.
    static bool isLearnableCc (int cc) noexcept { return cc >= 1 && cc <= 119 && cc != 6 && cc != 32 && cc != 38 && (cc < 98 || cc > 101); }
    // one to one: the parameter lets go of its old CC, and the CC of its old parameter (named in movedFrom).
    // false (and nothing changes) for a CC that cannot be learned or a parameter that does not exist.
    bool assignCc (int cc, const juce::String& paramId, juce::String* movedFrom = nullptr);
    void forgetCc (const juce::String& paramId);
    int ccOf (const juce::String& paramId) const;            // -1: none
    juce::String paramOfCc (int cc) const;                   // empty: free
    bool ccIsLive (int cc) const;                            // mapped, and its module is in the rack
    bool midiMapEmpty() const;
    juce::ValueTree midiMapToTree() const;                   // <MIDIMAP><MAP cc="74" param="sv_time"/></MIDIMAP>
    void midiMapFromTree (const juce::ValueTree&);           // replaces the map (no MIDIMAP: an empty map)
    std::atomic<juce::uint32> lastLearnableCc { 0 };         // (count << 8) | cc of the last learnable CC in; 0 none yet
    std::array<std::atomic<juce::uint32>, 128> ccMoves {};   // how many times each mapped CC has moved its parameter

    juce::AudioProcessorValueTreeState apvts;
    wc::Rack rack;
    juce::MidiKeyboardState keyboardState;

private:
    std::vector<wc::Cable> cables;
    std::array<bool, wc::kNumInst> present {};
    int colourCounter = 0;
    void commit();
    void resetParams();
    std::array<std::atomic<int>, 128> midiMap;               // see MIDI Learn above
    std::vector<juce::RangedAudioParameter*> mapParam;        // every parameter, by index (made once)
    std::vector<int> mapInst;                                 // the module slot of each, -1: always in the rack
    std::vector<bool> mapStepped;
    bool applyMidiMap (const juce::MidiMessage&) noexcept;    // audio thread; true: the CC was taken
    void adoptMidiMap (const juce::ValueTree& patchRoot);     // a patch brings its map only into an empty one
    bool setCc (int cc, const juce::String& paramId, juce::String* movedFrom = nullptr);   // assignCc without telling the host
    void midiMapChanged();                                    // a person changed the map: the host has something to save
    void setParam (const juce::String& id, float plain);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WcProcessor)
};
