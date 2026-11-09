#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "LookAndFeel.h"

class SynthgularityProcessor;

namespace pg::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// One parameter as a knob, menu or switch depending on its type.
void fillDestinationMenu (juce::ComboBox&);   // matrix / lane destinations, grouped by module

// Implemented by an editor that offers drag-and-drop modulation onto its knobs.
struct ModRing { juce::Colour colour; float lo, hi; };
struct ModHost
{
    virtual ~ModHost() = default;
    virtual bool canModulate (const juce::String& paramId) const = 0;
    virtual void dropModulation (int source, const juce::String& paramId) = 0;
    virtual std::vector<ModRing> ringsFor (const juce::String& paramId) const = 0;
    virtual float liveValue (const juce::String& paramId) const = 0;     // 0..1, or -1 when not modulated
    virtual void showModMenu (const juce::String& paramId, juce::Component& knob) = 0;
    virtual juce::Colour colourOf (int source) const = 0;
};

// Slider that hands right-clicks to its owner instead of dragging
struct MenuSlider : public juce::Slider
{
    std::function<bool()> onMenu;   // returns true when it handled the click
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu() && onMenu && onMenu()) { menuOpen = true; return; } menuOpen = false; Slider::mouseDown (e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (! menuOpen) Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override { if (! menuOpen) Slider::mouseUp (e); menuOpen = false; }
    bool menuOpen = false;
};

class ParamControl : public juce::Component, public juce::DragAndDropTarget
{
public:
    ParamControl (APVTS& state, const juce::String& paramId, const juce::String& label);
    void resized() override;
    void paintOverChildren (juce::Graphics&) override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
    enum Kind { Knob, Menu, Switch };
    Kind kind() const { return k; }
    const juce::String& paramId() const { return id; }
    bool isModulated() const;
    MenuSlider slider;
    juce::ComboBox combo;

private:
    ModHost* host() const { return findParentComponentOfClass<ModHost>(); }
    juce::String id;
    bool dropHover = false;
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

// ADSR display; drag the nodes to edit attack, decay/sustain and release
class EnvelopeView : public juce::Component
{
public:
    EnvelopeView (APVTS& state, const juce::String& prefix, juce::Colour colour);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    struct Geo { juce::Point<float> start, peak, sus, rel, end; };
    Geo geometry() const;
    float val (const char* suffix) const;
    void set (const char* suffix, float v);
    juce::RangedAudioParameter* param (const char* suffix) const;
    APVTS& st;
    juce::String pre;
    juce::Colour col;
    int dragNode = -1;
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
    enum Transform { Reverse, Mirror, Invert, RotateLeft, RotateRight, Double, Half };
    void transform (int op);
    void copyLane();
    void pasteLane();
    static bool canPaste();
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

// Brand "about" card: Calabi Yau artwork, version and credits. Click anywhere to close.
class AboutOverlay : public juce::Component
{
public:
    AboutOverlay();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
private:
    juce::Image banner, machine, serpent, isologo;
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
    juce::ComboBox src, dst, via, pol;
    juce::Slider amt;
    std::unique_ptr<APVTS::ComboBoxAttachment> srcA, dstA, viaA, polA;
    std::unique_ptr<APVTS::SliderAttachment> amtA;
};
} // namespace pg::ui
