#pragma once
#include <map>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../WcProcessor.h"
#include "../modules/M666.h"
#include "WcLook.h"
#include "ValueReadout.h"

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
    juce::String getTooltip() override;      // the KB-225 MOD output says when CC 1 is learned by a control
    const int inst, index;
    const bool output;
    const JackKind kind;
    const bool pulse;
    const juce::uint32 tint;
private:
    RackView& rack;
};

// Knob or fader with its own drag: 250 px of screen for the whole turn at any zoom (a fader: its own length),
// Shift fine (x0.1), Alt in 10 % steps, Shift+Alt in 1 %. A click without moving changes nothing, a click in a
// fader's slot does not jump. Right click: menu. Ctrl+click (Cmd on a Mac): type a value. Double click: default.
class WcKnob : public juce::Slider, public ReadoutSource
{
public:
    explicit WcKnob (SliderStyle);
    juce::RangedAudioParameter* param = nullptr;   // the bound parameter; none for the 252 cell knobs or a dynamic knob without one
    juce::String paramId;
    std::function<bool (ReadoutText&)> describe;    // a knob that is not a parameter (252 cells) says what it shows
    std::function<void()> onMenu, onType;
    bool readoutText (ReadoutText&) override;
    juce::Rectangle<float> readoutAnchor() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void valueChanged() override;
    static constexpr double kTurnPx = 250.0;   // screen pixels for the whole turn of a knob
private:
    double raw = 0.0;                          // where the drag has the knob, 0..1, before any Alt steps
    juce::Point<float> last, down;             // screen positions
    bool menuClick = false, typing = false, gesture = false;
    juce::String stepsNote;
    juce::Point<float> screen (const juce::MouseEvent&) const;
    int numValues() const;
};

// 2- or 3-position lever switch bound to a parameter
class ParamSwitch : public juce::Component, public juce::SettableTooltipClient, public ReadoutSource
{
public:
    ParamSwitch (juce::RangedAudioParameter& p, int positions, const juce::String& labels, juce::Colour tip = juce::Colour (0xfff7f7f5));
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool readoutText (ReadoutText&) override;
    juce::Rectangle<float> readoutAnchor() override;
    juce::String paramId() const { return param.getParameterID(); }
private:
    juce::RangedAudioParameter& param;
    int positions, value = 0;
    bool topIsOne = false;          // on/off switches: up = on
    juce::Colour tip;
    juce::StringArray labels;       // one per position, top to bottom
    juce::ParameterAttachment attach;
};

// white panel button that steps a parameter (259t RANGE, 1V/OCT)
class ParamButton : public juce::Component, public juce::SettableTooltipClient, public ReadoutSource
{
public:
    explicit ParamButton (juce::RangedAudioParameter& p);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { down = false; repaint(); }
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool readoutText (ReadoutText&) override;
    juce::Rectangle<float> readoutAnchor() override { return getLocalBounds().toFloat(); }
    juce::String paramId() const { return param.getParameterID(); }
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
    // the right button does nothing: no menu, no press
    void mouseDown (const juce::MouseEvent& e) override { menuClick = e.mods.isPopupMenu(); if (menuClick) return; if (target) target->store (true); if (onPress) onPress(); down = true; repaint(); }
    void mouseUp (const juce::MouseEvent&) override { if (menuClick) { menuClick = false; return; } if (target) target->store (false); down = false; repaint(); }
    std::function<void()> onPress;
private:
    std::atomic<bool>* target = nullptr;
    bool down = false, menuClick = false;
};

// 248 programming lever: momentary up / down (or left / right), acting on the stage being edited
class ProgSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    ProgSwitch (WcProcessor& p, const juce::String& prefix, const juce::String& fn, int selectValue, bool horizontal, juce::Colour tip);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dir = 0; repaint(); }
private:
    WcProcessor& proc;
    juce::String prefix, fn;
    int value, dir = 0;
    bool horiz;
    juce::Colour tip;
    void set (const juce::String& id, float plain);
    int editStage() const;
};

class ModulePanel;

// The top strip of a panel (22 units, as wide as the panel): drag it to move the module. A child of the panel, so it
// gets the mouse before the 218, 242, 252 and 666 keep every click of their own. It paints nothing at rest (the panel
// images stay as they are). A right click is the panel's own menu.
class ModuleGrip : public juce::Component, public juce::SettableTooltipClient, public ReadoutSource
{
public:
    ModuleGrip (RackView& rv, ModulePanel& p);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool readoutText (ReadoutText&) override;
    juce::Rectangle<float> readoutAnchor() override { return getLocalBounds().toFloat(); }
    ReadoutText message;              // what the readout says over the strip ("Added at the end of row 2") ...
    juce::uint32 messageUntil = 0;    // ... until then
    static constexpr int kHeight = 22;
private:
    RackView& rack;
    ModulePanel& panel;
    juce::Point<float> downAt;        // screen
    bool hover = false, pressed = false, moving = false;
};

class ModulePanel : public juce::Component
{
public:
    ModulePanel (RackView& rv, WcProcessor& p, int inst);
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void updateLeds();
    void touch218 (juce::Point<float> p, bool down);
    void toggleMatrixCell (int row, int col);   // 242: row 0 = stages, rows 1..3 = A B C
    juce::Component* controlFor (const juce::String& paramId) const;   // the knob, fader, lever or button showing it
    const int inst;
    std::vector<std::unique_ptr<JackComp>> jacks;
    std::unique_ptr<ModuleGrip> grip;
private:
    RackView& rack;
    WcProcessor& proc;
    const ModuleSpec& spec;
    juce::String prefix;
    std::vector<std::unique_ptr<WcKnob>> sliders;
    std::vector<juce::String> sliderIds;
    std::vector<juce::String> boundIds;      // resolved parameter id of each slider ({e} / {n} / {s} substituted)
    void rebindDynamic();
    juce::String resolveId (const juce::String& suffix) const;
    // 252: knobs that edit the cursor cell (not host parameters), copy buffer, cached cursor
    std::map<juce::String, std::unique_ptr<WcKnob>> freeKnobs;
    void bindKnob (size_t i);                // parameter, double click and menu of slider i, after its attachment
    bool describe252 (const juce::String& name, ReadoutText&) const;
    void menu252 (const juce::String& name);
    void type252 (const juce::String& name);
    struct Cell252 { int rgb = 0, sub = 0; float subc = 0, off = 0, cv[6] { -1, -1, -1, -1, -1, -1 }; bool valid = false; } clip252;
    int cur252 = -1, phaseStep252 = 8, patt252 = -1;
    bool euclid252 = false;
    int cursor252() const;
    void sync252Knobs();
    void free252 (const juce::String& name, double v);
    void action252 (const juce::String& id);
    void paint252 (juce::Graphics&);
    void paint666 (juce::Graphics&);
    void paint291 (juce::Graphics&);
    void paintMidi (juce::Graphics&);        // MIDI Learn: the ring of a control waiting for a CC, the dot of a learned one
    int seq666 = 0;
    std::unique_ptr<wc::M666::Frame> frame666;
    std::vector<juce::Point<float>> trail666;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::vector<std::unique_ptr<ParamSwitch>> switches;
    std::vector<std::unique_ptr<PushButton>> pushes;
    std::vector<std::unique_ptr<ParamButton>> buttons;
    std::vector<std::unique_ptr<ProgSwitch>> progs;
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
    // Ctrl (or Cmd) + wheel over the rack zooms; a plain wheel is left to the viewport, which scrolls.
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    std::function<void (float amount, juce::Point<float> posInRack)> onZoomGesture;
    juce::Point<int> preferredSize() const;   // at the current zoom

    // dragging cables
    void beginDrag (JackComp& j, const juce::MouseEvent& e);
    void dragMove (const juce::MouseEvent& e);
    void endDrag (const juce::MouseEvent& e);
    bool isDragging() const { return dragFrom != nullptr; }
    void autoScroll (const juce::MouseEvent& e);   // a drag near or past the edge of the view scrolls it

    // moving a module by its top strip. Until the drop nothing changes but what is on screen; the drop changes only
    // the order of the rows (WcProcessor::moveModule): no rebuild, no setPatch, the audio is not touched.
    void beginModuleDrag (ModulePanel& p, juce::Point<float> grabInPanel);
    void moduleDragMove (const juce::MouseEvent& e);
    void endModuleDrag();
    void cancelModuleDrag();                       // Esc, a rebuild: back where it was
    const ModulePanel* movingPanel() const { return moving; }
    void addModule (int inst);                     // "+ MODULE": at the end of the row in the middle of the view
    bool canDropOn (const JackComp& j) const;
    void jackMenu (JackComp& j);

    // the value readout (owned by the editor), the right-click menu of a parameter control, typing a value.
    // The menu works on the parameter id: if rebuild() takes the control away meanwhile, it still applies.
    ValueReadout* readout = nullptr;
    bool menuOpen = false;
    bool readoutSuppressed() const { return isDragging() || menuOpen; }
    void paramMenu (juce::Component& control, const juce::String& paramId, bool knob);
    // knob: Type and positions. cc: the parameter's learned CC (-1 none); waiting: it is the one waiting for a CC
    static juce::PopupMenu buildParamMenu (const juce::RangedAudioParameter&, bool knob, int cc = -1, bool waiting = false);
    void menuChoice (const juce::String& paramId, int result, bool waiting, juce::Component* control);   // what the menu does
    void typeValue (juce::Component& control, const juce::String& paramId);
    static ValueReadout* readoutFor (juce::Component& control);
    static void ccNote (juce::Component& control, const juce::String& paramId, ReadoutText&);   // "CC 74" on a learned control

    // MIDI Learn. Waiting for a CC is the editor's state, by parameter id (a rebuild meanwhile does not lose it): the
    // next learnable CC that comes in is assigned. A menu, Esc, a click in the plugin, another learn or the editor
    // closing cancel it; there is no timeout. "Learn another CC" waits with the old CC still in place.
    void startLearn (const juce::String& paramId);
    void cancelLearn();
    bool learning() const { return learnId.isNotEmpty(); }
    const juce::String& learningId() const { return learnId; }
    bool learnRing (const juce::String& paramId, float& alpha) const;   // waiting (pulsing) or just assigned (600 ms)
    float midiMarkMargin (juce::Component& control) const;   // room its ring or dot takes around it, in screen px (0: none)
    bool ccActive (int cc) const;                                       // the CC moved its parameter in the last 150 ms
    juce::Component* controlFor (const juce::String& paramId) const;
    void repaintMarks (const juce::String& paramId);
    static constexpr int kAssignedRingMs = 600, kActivityMs = 150, kLearnedMs = 1500, kMovedMs = 2500, kCc1Ms = 3000;

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
    std::vector<std::unique_ptr<juce::TextButton>> rowRemove;   // the "x" of each row, shown on the empty ones
    CableLayer cables { *this };
    ModulePanel* moving = nullptr;             // the module being moved
    juce::Point<float> grab;                   // where its strip was taken, in the panel
    int dropRow = -1, dropIndex = 0;           // where it would land (-1: outside the view); index in the row without it
    bool dropOk = false;                       // false: the row is full, or outside
    int heldMsg = 0, revealInst = -1;
    juce::Rectangle<float> hole;               // where it lands (or goes back to), content units
    void layoutPanels();                       // places the panels from the rows (and the drag)
    ModulePanel* panelOf (int inst) const;
    void reveal (int inst);
    int rowAtViewCentre() const;
    float zoom = 1.0f;
    juce::Point<int> contentSize { 800, 450 };
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    juce::String learnId, justId;              // waiting for a CC; just assigned (its ring stays a moment)
    juce::uint32 learnFrom = 0, learnSince = 0, justUntil = 0;
    std::array<juce::uint32, 128> movesSeen {}, activeUntil {};
    void learnTick();
    void finishLearn (int cc);
    void holdLearnReadout();
    void endLearn();
    // a click anywhere in the plugin cancels a learn (a right click on the waiting control opens its menu instead)
    struct LearnClicks : public juce::MouseListener
    {
        explicit LearnClicks (RackView& r) : owner (r) {}
        void mouseDown (const juce::MouseEvent&) override;
        RackView& owner;
    } learnClicks { *this };
    bool listeningClicks = false;
};
} // namespace wc::ui
