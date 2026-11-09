#pragma once
#include "Widgets.h"
#include <juce_dsp/juce_dsp.h>
#include "../dsp/Meter.h"
#include <atomic>
#include <functional>

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
    juce::Rectangle<int> specArea, meterArea, scopeArea;
    static constexpr int kScope = 160;
    std::array<std::array<float, kScope>, 4> traces {};   // newest first: phosphor persistence
    int traceHead = 0;
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
    FxSlotRow (APVTS& state, int slot, bool mixbus = false);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    int type() const;
    bool selected = false;
    std::function<void (int slot)> onSelect;
    std::function<void (int slot, int dir)> onMove;
    static juce::String onParamFor (int type, bool mixbus = false);
private:
    juce::Rectangle<float> ledArea() const;
    APVTS& st;
    int slot;
    bool mb = false;
    juce::ComboBox combo;
    juce::TextButton up { juce::CharPointer_UTF8 ("\xe2\x96\xb2") }, down { juce::CharPointer_UTF8 ("\xe2\x96\xbc") };
    std::unique_ptr<APVTS::ComboBoxAttachment> att;
};

// Where the analyzers read the output: meters plus a stereo ring buffer (size a power of two)
struct ScopeFeed
{
    MasterMeter* meter = nullptr;
    const float* L = nullptr;
    const float* R = nullptr;
    const std::atomic<int>* pos = nullptr;
    int size = 0;
};

// Scope, FFT spectrum, goniometer and true-peak / LUFS meters of the output
class AnalyzerStrip : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit AnalyzerStrip (ScopeFeed feed);
    ~AnalyzerStrip() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    void timerCallback() override;
    ScopeFeed feed;
    juce::Rectangle<int> scopeA, fftA, gonioA, meterA;
    static constexpr int kOrder = 11, kSize = 1 << kOrder, kBands = 140, kScope = 220, kGonio = 1024;
    juce::dsp::FFT fft { kOrder };
    std::array<float, kSize * 2> fftData {};
    std::array<float, kSize> window {};
    std::array<float, kBands> bands {}, peaks {};
    std::array<std::array<float, kScope>, 3> traces {};
    int traceHead = 0;
    std::array<float, kGonio> gl {}, gr {};
    float correlation = 0.0f;
    float tpL = -120, tpR = -120, tpMax = -120, lm = -120, ls = -120, li = -120;
};

// Small goniometer for the header
class GonioMini : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit GonioMini (ScopeFeed feed);
    void paint (juce::Graphics&) override;
private:
    ScopeFeed feed;
    float corr = 0.0f;
};

// A modulation source chip: drag it onto any knob
class SourceChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    SourceChip (int source, const juce::String& name, juce::Colour colour, std::function<float()> value, bool bipolar);
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
private:
    int src;
    juce::String name;
    juce::Colour col;
    std::function<float()> value;
    bool bipolar;
};
} // namespace pg::ui
