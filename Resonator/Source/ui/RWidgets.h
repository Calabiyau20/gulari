#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "ui/LookAndFeel.h"
#include "../RParams.h"

class ResonatorProcessor;

namespace cyr::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

juce::Colour sourceColour (int source);
juce::String sourceShort (int source);

// Slider that hands right-clicks to its owner instead of dragging
struct KnobSlider : public juce::Slider
{
    std::function<void()> onMenu;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) { if (onMenu) onMenu(); return; } Slider::mouseDown (e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) Slider::mouseUp (e); }
};

// A parameter as a knob (choices too). Drop a modulation source on it to route that source here;
// its modulation shows as coloured arcs around the knob with a live dot. Right-click to edit them.
class ModKnob : public juce::Component, public juce::DragAndDropTarget
{
public:
    ModKnob (ResonatorProcessor& p, const juce::String& paramId, const juce::String& label, juce::Colour accent = pg::ui::cosmos::neon);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;
    int paramIndex() const { return rid; }
    KnobSlider slider;
private:
    void showMenu();
    ResonatorProcessor& proc;
    juce::String id, caption;
    int rid = -1;
    bool modulatable = false, dropHover = false;
    juce::Colour accent;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// On / off as a small lit switch
class Switch : public juce::Component
{
public:
    Switch (APVTS& st, const juce::String& paramId, const juce::String& label);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::ToggleButton button;
private:
    std::unique_ptr<APVTS::ButtonAttachment> att;
};

// A modulation source you drag onto any knob
class SourceChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    SourceChip (ResonatorProcessor& p, int source);
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
private:
    ResonatorProcessor& proc;
    int src;
    bool dragging = false;
};

// The oscillator's Calabi-Yau surface, turning slowly, with the patch it plays and the orbit traced in gold
class SurfaceView : public juce::Component
{
public:
    SurfaceView (ResonatorProcessor& p, int osc) : proc (p), o (osc) {}
    void paint (juce::Graphics&) override;
    void tick (double t);   // called by the editor timer
private:
    void rebuild (int n);
    ResonatorProcessor& proc;
    int o;
    double time = 0.0;
    int builtN = -1, grid = 6;
    std::vector<float> verts;    // per vertex: Re z1, Re z2, Im z1, Im z2
    juce::Image frame;
    int frameSkip = 0;
};

// FM (gold) and PD (orchid) connections between the four oscillators, with dots flowing along them
class MatrixView : public juce::Component
{
public:
    explicit MatrixView (ResonatorProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    double time = 0.0;
private:
    ResonatorProcessor& proc;
};

// LFO shape with a moving dot at its current value
class LfoView : public juce::Component
{
public:
    LfoView (ResonatorProcessor& p, int lfo) : proc (p), l (lfo) {}
    void paint (juce::Graphics&) override;
private:
    ResonatorProcessor& proc;
    int l;
};

// One row of the modulation list
class ModSlotRow : public juce::Component
{
public:
    ModSlotRow (ResonatorProcessor& p, int slot);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    ResonatorProcessor& proc;
    int slot;
    juce::ComboBox src, dst;
    juce::Slider amt;
    juce::TextButton clear { "X" };
    std::unique_ptr<APVTS::ComboBoxAttachment> srcA, dstA;
    std::unique_ptr<APVTS::SliderAttachment> amtA;
};
} // namespace cyr::ui
