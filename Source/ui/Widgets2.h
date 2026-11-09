#pragma once
#include "Widgets.h"
#include <juce_dsp/juce_dsp.h>

namespace pg::ui
{
// Header master section: FFT spectrum, the master knob, true-peak bars and LUFS readouts.
class MasterSection : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    MasterSection (SynthgularityProcessor& p, APVTS& state);
    ~MasterSection() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void refreshNow() { timerCallback(); }
private:
    void timerCallback() override;
    SynthgularityProcessor& proc;
    std::unique_ptr<ParamControl> knob;
    juce::Rectangle<int> specArea, meterArea;
    static constexpr int kOrder = 11, kSize = 1 << kOrder, kBands = 120;
    juce::dsp::FFT fft { kOrder };
    std::array<float, kSize * 2> fftData {};
    std::array<float, kSize> window {};
    std::array<float, kBands> bands {};
    float tpL = -120, tpR = -120, tpMax = -120, lm = -120, ls = -120, li = -120;
};

// Spectral oscillator display: the whole table as a heat map (time across, partials up)
// and the frame being played as bars. Drop an audio file to analyse it.
class SpectralView : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit SpectralView (SynthgularityProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
    std::function<void (const juce::File&)> onFile;
private:
    SynthgularityProcessor& proc;
    const void* cachedFor = nullptr;
    juce::Image heat;
};

// One-shot sampler: waveform with the attack / decay envelope over it
class ShotView : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit ShotView (SynthgularityProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int, int) override;
    std::function<void (const juce::File&)> onFile;
private:
    SynthgularityProcessor& proc;
};

// One slot of the FX rack: effect menu, power LED, move up / down
class FxSlotRow : public juce::Component
{
public:
    FxSlotRow (APVTS& state, int slot);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    int type() const;
    bool selected = false;
    std::function<void (int slot)> onSelect;
    std::function<void (int slot, int dir)> onMove;
    static juce::String onParamFor (int type);
private:
    juce::Rectangle<float> ledArea() const;
    APVTS& st;
    int slot;
    juce::ComboBox combo;
    juce::TextButton up { juce::CharPointer_UTF8 ("\xe2\x96\xb2") }, down { juce::CharPointer_UTF8 ("\xe2\x96\xbc") };
    std::unique_ptr<APVTS::ComboBoxAttachment> att;
};
} // namespace pg::ui
