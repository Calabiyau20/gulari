#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../WcProcessor.h"
#include "WcLook.h"

namespace wc::ui
{
class RackView;

// a jack on a panel: banana (control) or mini (audio)
class JackComp : public juce::Component, public juce::SettableTooltipClient
{
public:
    JackComp (RackView& rv, int inst, const JackSpec& spec, int dspIndex);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    const int inst, index;
    const bool output;
    const JackKind kind;
    const bool pulse;
    const juce::uint32 tint;
private:
    RackView& rack;
};

// 2- or 3-position lever switch bound to a parameter
class ParamSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    ParamSwitch (juce::RangedAudioParameter& p, int positions, const juce::String& labels, juce::Colour tip = juce::Colour (0xfff7f7f5));
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    juce::RangedAudioParameter& param;
    int positions, value = 0;
    bool topIsOne = false;          // on/off switches: up = on
    juce::Colour tip;
    juce::StringArray labels;       // one per position, top to bottom
    juce::ParameterAttachment attach;
};

// white panel button that steps a parameter (259t RANGE, 1V/OCT)
class ParamButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ParamButton (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { down = false; repaint(); }
private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attach;
    float value = 0.0f;
    bool down = false;
};

// momentary panel button (281 manual triggers)
class PushButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PushButton (std::atomic<bool>* t, const juce::String& tip = "Trigger (hold for a sustained function)")
        : target (t) { setMouseCursor (juce::MouseCursor::PointingHandCursor); setTooltip (tip); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { if (target) target->store (true); if (onPress) onPress(); down = true; repaint(); }
    void mouseUp (const juce::MouseEvent&) override { if (target) target->store (false); down = false; repaint(); }
    std::function<void()> onPress;
private:
    std::atomic<bool>* target = nullptr;
    bool down = false;
};

class ModulePanel : public juce::Component
{
public:
    ModulePanel (RackView& rv, WcProcessor& p, int inst);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;
    void updateLeds();
    void toggleMatrixCell (int row, int col);   // 242: row 0 = stages, rows 1..3 = A B C
    const int inst;
    std::vector<std::unique_ptr<JackComp>> jacks;
private:
    RackView& rack;
    WcProcessor& proc;
    const ModuleSpec& spec;
    juce::String prefix;
    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::vector<std::unique_ptr<ParamSwitch>> switches;
    std::vector<std::unique_ptr<PushButton>> pushes;
    std::vector<std::unique_ptr<ParamButton>> buttons;
    std::vector<float> ledCache;
    float leds4[4] {};
    juce::TextButton removeButton { "x" };
    float ledA = 0.0f, ledB = 0.0f;
    bool ledOn = false;
    juce::Rectangle<int> ledArea;
};

// the cables, drawn on top of the panels
class CableLayer : public juce::Component
{
public:
    explicit CableLayer (RackView& rv) : rack (rv) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
private:
    RackView& rack;
};

class RackView : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    explicit RackView (WcProcessor& p);
    ~RackView() override;
    void rebuild();                          // modules added / removed
    void resized() override;
    void paint (juce::Graphics&) override;
    void setZoom (float z);
    float getZoom() const { return zoom; }
    juce::Point<int> preferredSize() const;   // at the current zoom

    // dragging cables
    void beginDrag (JackComp& j, const juce::MouseEvent& e);
    void dragMove (const juce::MouseEvent& e);
    void endDrag (const juce::MouseEvent& e);
    bool isDragging() const { return dragFrom != nullptr; }
    bool canDropOn (const JackComp& j) const;
    void jackMenu (JackComp& j);

    JackComp* findJack (int inst, bool output, int index) const;
    juce::Point<float> jackCentre (const JackComp& j) const;
    int cableCountAt (const JackComp& j) const;

    WcProcessor& proc;
    juce::Component content;                   // the rack, at 1:1, scaled by the zoom transform

    // drag state (content coordinates)
    JackComp* dragFrom = nullptr;              // fixed end of the cable being dragged
    juce::Point<float> dragPos;
    int dragColour = 0;
    float cableAlpha = 0.9f;

private:
    std::vector<std::unique_ptr<ModulePanel>> panels;
    CableLayer cables { *this };
    float zoom = 1.0f;
    juce::Point<int> contentSize { 800, 450 };
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
};
} // namespace wc::ui
