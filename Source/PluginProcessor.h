#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "dsp/Synth.h"
#include "dsp/Fx.h"
#include "dsp/Sample.h"
#include "dsp/Wavetable.h"
#include <array>
#include <atomic>
#include <mutex>

class SynthgularityProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    SynthgularityProcessor();
    ~SynthgularityProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override { return PG_PRODUCT_NAME; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return currentPresetName; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- sound sources ----
    bool loadSample (const juce::File& file, juce::String& error);
    pg::SamplePtr getSample() const { return std::atomic_load (&sample); }
    bool loadUserWavetable (int osc, const juce::File& file, juce::String& error);
    pg::WavetablePtr getActiveTable (int osc) const;

    // ---- ORB (4-corner morph) ----
    void captureCorner (int corner);
    void clearCorner (int corner);
    bool cornerFilled (int corner) const { return orbFilled[(size_t) corner].load(); }
    bool loadPresetIntoCorner (int corner, const juce::File& file);

    // ---- presets ----
    juce::File presetFolder() const;
    juce::Array<juce::File> listPresets() const;
    bool savePreset (const juce::String& name, juce::String& error);
    bool loadPreset (const juce::File& file);
    void loadFactoryPreset (int index);
    static juce::StringArray factoryPresetNames();
    void initPreset();
    juce::String currentPresetName { "Init" };

    juce::AudioProcessorValueTreeState apvts;
    pg::Synth synth;
    const float* paramCache() const { return cache.data(); }

private:
    void timerCallback() override;
    void refreshCache();
    juce::ValueTree buildState();
    void applyState (const juce::ValueTree& state, bool includeOrb);

    std::array<std::atomic<float>*, pg::kNumParams> raw {};
    std::array<juce::RangedAudioParameter*, pg::kNumParams> params {};
    std::array<float, pg::kNumParams> cache {};

    std::array<std::array<std::atomic<float>, pg::kNumParams>, 4> orbCorners;
    std::array<std::atomic<bool>, 4> orbFilled {};

    pg::SamplePtr sample;
    pg::WavetablePtr userTable[2];
    std::vector<std::shared_ptr<const void>> graveyard;   // released on the message thread
    std::mutex graveyardLock;

    pg::FxChain fx;
    int lastStrumCount = 0;
    juce::AudioBuffer<float> work;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthgularityProcessor)
};
