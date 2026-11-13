#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace wc::ui
{
// What a control shows in the value readout
struct ReadoutText
{
    juce::String name, value, note;   // line 1: parameter name, line 2: value with its unit, line 3 (optional): a note
    bool warn = false;                // the note is a warning (gold), not information (orchid)
};

// The controls that have a readout: knobs, faders, levers and panel buttons
struct ReadoutSource
{
    virtual ~ReadoutSource() = default;
    virtual bool readoutText (ReadoutText&) = 0;          // false: nothing to show (a dynamic knob with no parameter)
    virtual juce::Rectangle<float> readoutAnchor() = 0;   // the visible outline of the control, in its own coordinates
};

// Typing a value into a control
struct ValueEdit
{
    juce::String name, text, example;   // header, the text it opens with (selected), and what to suggest on a bad value
    // false: the text is not a value for this control. When the value had to be limited, `limited` says to what.
    std::function<bool (const juce::String& text, juce::String& limited)> apply;
};

// The value of the control under the mouse or being dragged, and the box to type a value into. A child of the
// editor, outside the zoom transform of the rack, so it reads the same at 50 % and at 150 %.
class ValueReadout : public juce::Component, private juce::Timer
{
public:
    ValueReadout();
    ~ValueReadout() override;

    std::function<bool()> suppressed;                    // a cable being dragged, a menu open
    std::function<juce::Rectangle<int>()> visibleArea;   // where the rack can be seen, in the parent's coordinates
    std::function<float (juce::Component&)> markMargin;  // room a control's MIDI ring or dot takes around it (parent px)

    // hover: after 400 ms still; at once if a readout was up less than 800 ms ago
    void hoverEnter (juce::Component&);
    void hoverMove (juce::Component&);
    void hoverExit (juce::Component&);
    // drag: at once, follows every change; on release it stays while the mouse is over, otherwise goes after 600 ms
    void dragStart (juce::Component&);
    void dragEnd (juce::Component&, bool mouseStillOver);
    void refresh();                                      // texts and place; the rack calls it at 30 Hz
    void flash (juce::Component&, const juce::String& note, bool warn, int ms);
    // a fixed text over a control until released (MIDI Learn waiting for a CC): no timeout, hovers elsewhere ignored
    void hold (juce::Component&, const ReadoutText&);
    void release();                                      // only if it is still holding
    bool isHolding() const { return mode == Mode::Hold; }

    void edit (juce::Component&, ValueEdit);
    bool isEditing() const { return mode == Mode::Edit; }
    void cancelEdit();
    void commitEdit (bool fromKey);                      // Enter / Tab (true), or the field left (false)

    bool isUp() const { return isVisible(); }
    juce::Component* targetComponent() const { return target.getComponent(); }
    const ReadoutText& current() const { return shown; }
    bool showsError() const { return invalid; }
    juce::TextEditor field;                              // the typing field (public for the UI test)

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kHoverMs = 400, kWarmMs = 800, kLingerMs = 600, kFadeMs = 120;

private:
    enum class Mode { Hidden, Pending, Hover, Drag, Linger, Flash, Edit, Hold };
    Mode mode = Mode::Hidden;
    juce::Component::SafePointer<juce::Component> target;
    ReadoutText shown;
    ValueEdit editing;
    bool invalid = false;
    juce::String refused;
    juce::uint32 pendingSince = 0, lastVisible = 0, until = 0;
    juce::Point<float> stillAt;
    struct Outside : public juce::MouseListener
    {
        explicit Outside (ValueReadout& r) : owner (r) {}
        void mouseDown (const juce::MouseEvent&) override;
        ValueReadout& owner;
    } outside { *this };
    bool listening = false;
    struct TabKey : public juce::KeyListener   // Tab applies like Enter
    {
        explicit TabKey (ValueReadout& r) : owner (r) {}
        bool keyPressed (const juce::KeyPress&, juce::Component*) override;
        ValueReadout& owner;
    } tabKey { *this };

    void timerCallback() override;
    bool show (Mode m, bool evenIfHidden);
    void hideNow();
    bool layout (bool evenIfHidden);
    juce::Rectangle<float> anchorInParent() const;
    void closeEdit();
};
} // namespace wc::ui
