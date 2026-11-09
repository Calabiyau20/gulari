#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "RParams.h"
#include "dsp/RSynth.h"
#include "Params.h"
#include "PresetFolders.h"
#include "dsp/Fx.h"
#include "dsp/Meter.h"
#include <array>
#include <atomic>

class ResonatorProcessor : public juce::AudioProcessor
{
public:
    ResonatorProcessor();
    ~ResonatorProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Calabi-Yau Resonator"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return currentPresetName; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- modulation (drag and drop) ----
    int addModulation (int source, int paramIndex, float amount = 0.35f);   // returns slot or -1
    void removeModulation (int slot);
    void setModAmount (int slot, float amount);
    juce::Array<int> slotsFor (int paramIndex) const;   // slots that modulate this parameter

    // ---- presets ----
    static juce::StringArray factoryPresetNames();
    void loadFactoryPreset (int index);
    void initPreset();
    juce::File presetFolder() const;          // default folder
    pg::PresetFolders& folders() { return presetFolders; }
    juce::Array<juce::File> listPresets() const;
    bool savePreset (const juce::String& name);
    bool loadPreset (const juce::File& file);
    juce::String currentPresetName { "Init" };
    int uiSkin = 0;

    // ---- analysis (post FX) ----
    pg::MasterMeter meter;
    static constexpr int kScopeRing = 8192;
    std::array<float, kScopeRing> scopeL {}, scopeR {};
    std::atomic<int> scopePos { 0 };

    juce::MidiKeyboardState keyboardState;   // on-screen keyboard
    juce::AudioProcessorValueTreeState apvts;
    cyr::RSynth synth;
    const float* cache() const { return rc.data(); }
    float value (int rid) const { return raw[(size_t) rid]->load(); }

private:
    void refresh();
    void applyQuality (int mode);
    void setParam (const juce::String& id, float plainValue);

    std::array<std::atomic<float>*, cyr::kNumR> raw {};
    std::array<float, cyr::kNumR> rc {};
    std::vector<std::pair<int, std::atomic<float>*>> hosted;   // pg index -> raw value
    std::array<float, pg::kNumParams> pgCache {};

    pg::PresetFolders presetFolders { "Calabi-Yau Resonator", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Calabi Yau").getChildFile ("Calabi-Yau Resonator").getChildFile ("Presets") };
    pg::FxChain fx;
    juce::SmoothedValue<float> masterGain { 0.5f };
    std::vector<float> gainRamp;
    bool fxSmoothReady = false;
    float fxSmoothK = 0.2f;
    pg::GlobalMod fxGlobal;
    int lastStrum = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> os[2];
    juce::MidiBuffer osMidi;
    double baseRate = 48000.0;
    int maxBlock = 512, osMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResonatorProcessor)
};
