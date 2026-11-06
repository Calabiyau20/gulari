#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"

class SynthgularityProcessor;

namespace pg::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// One parameter as a knob, menu or switch depending on its type.
class ParamControl : public juce::Component
{
public:
    ParamControl (APVTS& state, const juce::String& paramId, const juce::String& label);
    void resized() override;
    enum Kind { Knob, Menu, Switch };
    Kind kind() const { return k; }
    juce::Slider slider;
    juce::ComboBox combo;

private:
    Kind k = Knob;
    juce::Label caption;
    juce::ToggleButton toggle;
    std::unique_ptr<APVTS::SliderAttachment> sa;
    std::unique_ptr<APVTS::ComboBoxAttachment> ca;
    std::unique_ptr<APVTS::ButtonAttachment> ba;
};

// A titled group of controls laid out on a grid. Items are "paramId:Label".
class Panel : public juce::Component
{
public:
    Panel (APVTS& state, const juce::String& title, const juce::StringArray& items, int columns, juce::Colour accent = colours::accent);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Rectangle<int> extraArea;               // free area above the grid (for displays)
    int extraHeight = 0;
    std::function<void (juce::Rectangle<int>)> layoutExtra;
    juce::OwnedArray<ParamControl> controls;

private:
    juce::String title;
    int cols;
    juce::Colour accentColour;
};

// Waveform of the loaded sample with live grains; drop audio files here.
class SampleView : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    explicit SampleView (SynthgularityProcessor& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;
    std::function<void (const juce::File&)> onFile;

private:
    void setPosFromMouse (float x);
    SynthgularityProcessor& proc;
    bool dragOver = false, dragging = false;
};

// Current frame of a wavetable oscillator (plus a few neighbouring frames)
class WavetableView : public juce::Component
{
public:
    WavetableView (SynthgularityProcessor& p, int osc) : proc (p), oscIndex (osc) {}
    void paint (juce::Graphics&) override;
private:
    SynthgularityProcessor& proc;
    int oscIndex;
};

// Bar editor for one modulation lane (32 steps). Right-click locks a step.
class StepLaneEditor : public juce::Component
{
public:
    StepLaneEditor (SynthgularityProcessor& p, int lane) : proc (p), laneIndex (lane) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void randomize (bool mutate);
    void euclid();
    void ramp();
    void clear();
private:
    void setFromMouse (juce::Point<float> p);
    int length() const;
    SynthgularityProcessor& proc;
    int laneIndex;
    int euclidHits = 3;
};

// Arpeggiator step grid: on / transpose / velocity / tie / ratchet
class ArpGrid : public juce::Component
{
public:
    explicit ArpGrid (SynthgularityProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    void edit (juce::Point<float> p, bool click);
    juce::Rectangle<float> row (int r) const;
    SynthgularityProcessor& proc;
    int dragRow = -1;
};

// ORB: 4-corner morph pad
class OrbPad : public juce::Component
{
public:
    explicit OrbPad (SynthgularityProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    void setFromMouse (juce::Point<float> p);
    juce::Rectangle<float> padArea() const;
    SynthgularityProcessor& proc;
};

// One row of the modulation matrix
class MatrixRow : public juce::Component
{
public:
    MatrixRow (APVTS& state, int slot);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    int slot;
    juce::ComboBox src, dst, via;
    juce::Slider amt;
    std::unique_ptr<APVTS::ComboBoxAttachment> srcA, dstA, viaA;
    std::unique_ptr<APVTS::SliderAttachment> amtA;
};
} // namespace pg::ui
