#include "ValueReadout.h"
#include "WcLook.h"

namespace wc::ui
{
namespace
{
constexpr int kPadX = 8, kPadY = 4, kGap = 4, kLine1 = 13, kLine2 = 19, kLine3 = 13, kField = 24;
constexpr int kMinW = 72, kMaxW = 288, kMinEditW = 120, kAway = 8, kEdge = 4;

juce::uint32 now() { return juce::Time::getMillisecondCounter(); }
float width (const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidth (f, s); }
// a name or note wider than the widest box takes a second line instead of being cut
int rows (const juce::String& s) { return width (WcLook::label (11.0f), s) > (float) (kMaxW - 2 * kPadX) ? 2 : 1; }
} // namespace

ValueReadout::ValueReadout()
{
    setInterceptsMouseClicks (false, false);
    setAlwaysOnTop (true);
    addChildComponent (field);
    field.setFont (WcLook::label (15.0f));
    field.setJustification (juce::Justification::centredLeft);
    field.setIndents (0, 3);   // the text starts 8 px in, as the other lines
    field.setColour (juce::TextEditor::backgroundColourId, col::bg);
    field.setColour (juce::TextEditor::textColourId, col::text);
    field.setColour (juce::TextEditor::highlightColourId, col::midi.withAlpha (0.35f));
    field.setColour (juce::TextEditor::highlightedTextColourId, col::text);
    field.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    field.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    field.setColour (juce::CaretComponent::caretColourId, col::text);
    field.setSelectAllWhenFocused (false);
    field.onReturnKey = [this] { commitEdit (true); };
    field.onEscapeKey = [this] { cancelEdit(); };
    field.onFocusLost = [this] { commitEdit (false); };
    // the error goes as soon as the text is changed (the change arrives asynchronously, so compare with what was refused)
    field.onTextChange = [this] { if (invalid && field.getText() != refused) { invalid = false; shown.note.clear(); layout (true); repaint(); } };
    field.addKeyListener (&tabKey);
}

ValueReadout::~ValueReadout()
{
    mode = Mode::Hidden;   // the editor closing with a value half typed: nothing is applied on the way out
    if (listening) juce::Desktop::getInstance().removeGlobalMouseListener (&outside);
    field.removeKeyListener (&tabKey);
}

// ---- hover ----
void ValueReadout::hoverEnter (juce::Component& c)
{
    if (mode == Mode::Edit || mode == Mode::Drag || mode == Mode::Hold) return;
    target = &c;
    stillAt = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
    pendingSince = now();
    // warm: the readout was up a moment ago, so the next one comes at once
    if (isVisible() || now() - lastVisible < (juce::uint32) kWarmMs)
        if (show (Mode::Hover, false)) return;
    hideNow();
    target = &c;
    mode = Mode::Pending;
    startTimerHz (60);
}

void ValueReadout::hoverMove (juce::Component& c)
{
    if (mode == Mode::Edit || mode == Mode::Drag || mode == Mode::Hold) return;
    if (target.getComponent() != &c || mode == Mode::Hidden) { hoverEnter (c); return; }
    if (mode != Mode::Pending) return;
    const auto p = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
    if (p.getDistanceFrom (stillAt) >= 3.0f) { stillAt = p; pendingSince = now(); }   // 400 ms *still*
}

void ValueReadout::hoverExit (juce::Component& c)
{
    if (target.getComponent() != &c) return;
    if (mode == Mode::Pending || mode == Mode::Hover) hideNow();
}

// ---- drag ----
void ValueReadout::dragStart (juce::Component& c)
{
    if (mode == Mode::Edit) commitEdit (false);   // grabbing another control is a click outside the field
    target = &c;
    show (Mode::Drag, true);
}

void ValueReadout::dragEnd (juce::Component& c, bool over)
{
    if (target.getComponent() != &c || mode != Mode::Drag) return;
    if (over) { mode = Mode::Hover; layout (false); return; }
    mode = Mode::Linger;
    until = now() + (juce::uint32) kLingerMs;
    startTimerHz (60);
}

void ValueReadout::flash (juce::Component& c, const juce::String& note, bool warn, int ms)
{
    if (mode == Mode::Edit) return;
    target = &c;
    if (! show (Mode::Flash, true)) return;
    shown.note = note;
    shown.warn = warn;
    layout (true);
    until = now() + (juce::uint32) ms;
}

void ValueReadout::hold (juce::Component& c, const ReadoutText& t)
{
    if (mode == Mode::Edit) cancelEdit();
    target = &c;
    shown = t;
    show (Mode::Hold, true);
}

void ValueReadout::release()
{
    if (mode == Mode::Hold) hideNow();
}

void ValueReadout::refresh()
{
    if (mode == Mode::Hidden || mode == Mode::Pending) return;
    if (mode != Mode::Edit && suppressed != nullptr && suppressed()) { hideNow(); return; }
    if (! layout (mode != Mode::Hover)) hideNow();
}

bool ValueReadout::show (Mode m, bool evenIfHidden)
{
    if (suppressed != nullptr && suppressed()) { hideNow(); return false; }
    mode = m;
    setAlpha (1.0f);
    if (! layout (evenIfHidden)) { hideNow(); return false; }
    setVisible (true);
    toFront (false);
    lastVisible = now();
    startTimerHz (60);
    return true;
}

void ValueReadout::hideNow()
{
    if (isVisible()) lastVisible = now();
    mode = Mode::Hidden;
    setVisible (false);
    setAlpha (1.0f);
    stopTimer();
}

void ValueReadout::timerCallback()
{
    const auto t = now();
    if (target == nullptr && mode != Mode::Hidden) { if (mode == Mode::Edit) closeEdit(); hideNow(); return; }
    switch (mode)
    {
        case Mode::Hidden: stopTimer(); return;
        case Mode::Pending:
            if (suppressed != nullptr && suppressed()) { pendingSince = t; return; }
            if (t - pendingSince >= (juce::uint32) kHoverMs && ! show (Mode::Hover, false))
            {
                mode = Mode::Pending;   // not enough of the control in sight yet: keep waiting while the mouse is on it
                pendingSince = t;
                startTimerHz (60);
            }
            return;
        case Mode::Flash:
            if (t < until) break;
            if (target != nullptr && target->isMouseOver (true)) { mode = Mode::Hover; shown.note.clear(); layout (false); break; }
            mode = Mode::Linger;
            until = t;
            [[fallthrough]];
        case Mode::Linger:
        {
            if (t < until) break;
            const float a = 1.0f - (float) (t - until) / (float) kFadeMs;
            if (a <= 0.0f) { hideNow(); return; }
            setAlpha (a);
            break;
        }
        case Mode::Hover:
        case Mode::Drag:
        case Mode::Edit:
        case Mode::Hold:
            break;
    }
    if (isVisible()) lastVisible = t;
}

// ---- place and size ----
juce::Rectangle<float> ValueReadout::anchorInParent() const
{
    auto* c = target.getComponent();
    auto* parent = getParentComponent();
    auto* src = dynamic_cast<ReadoutSource*> (c);
    if (c == nullptr || parent == nullptr || src == nullptr) return {};
    const auto a = parent->getLocalArea (c, src->readoutAnchor());
    return markMargin != nullptr ? a.expanded (markMargin (*c)) : a;   // 8 px off the MIDI ring or dot, not off the control
}

bool ValueReadout::layout (bool evenIfHidden)
{
    auto* src = dynamic_cast<ReadoutSource*> (target.getComponent());
    auto* parent = getParentComponent();
    if (src == nullptr || parent == nullptr) return false;
    if (mode != Mode::Edit && mode != Mode::Hold)   // these two show the text they were given
    {
        const auto keepNote = mode == Mode::Flash ? shown.note : juce::String();
        const bool keepWarn = shown.warn;
        ReadoutText t;
        if (! src->readoutText (t)) return false;
        if (keepNote.isNotEmpty()) { t.note = keepNote; t.warn = keepWarn; }
        shown = t;
    }
    const auto anchor = anchorInParent();
    if (anchor.isEmpty()) return false;
    // less than a third of the control in sight: no hover (a drag always shows)
    if (! evenIfHidden && visibleArea != nullptr)
    {
        const auto seen = anchor.getIntersection (visibleArea().toFloat());
        if (seen.getWidth() * seen.getHeight() < anchor.getWidth() * anchor.getHeight() / 3.0f) return false;
    }
    const bool three = shown.note.isNotEmpty();
    const bool ed = mode == Mode::Edit;
    float textW = juce::jmax (width (WcLook::label (11.0f), shown.name), width (WcLook::label (15.0f), ed ? field.getText() : shown.value),
                              three ? width (WcLook::label (11.0f), shown.note) : 0.0f);
    const int w = juce::jlimit (ed ? kMinEditW : kMinW, kMaxW, (int) std::ceil (textW) + 2 * kPadX + (ed ? 12 : 0));
    const int h = kPadY + kLine1 * rows (shown.name) + kGap + (ed ? kField : kLine2) + (three ? kGap + kLine3 * rows (shown.note) : 0) + kPadY;   // 44 / 61, editing 49 / 66

    // above, centred, 8 px away; else below; else to the right; else to the left. Never over the control.
    const auto area = parent->getLocalBounds().toFloat();
    const float fw = (float) w, fh = (float) h;
    juce::Rectangle<float> r;
    auto clampX = [&] (float x) { return juce::jlimit (area.getX() + kEdge, juce::jmax (area.getX() + kEdge, area.getRight() - kEdge - fw), x); };
    auto clampY = [&] (float y) { return juce::jlimit (area.getY() + kEdge, juce::jmax (area.getY() + kEdge, area.getBottom() - kEdge - fh), y); };
    if (anchor.getY() - kAway - fh >= area.getY())
        r = { clampX (anchor.getCentreX() - fw * 0.5f), anchor.getY() - kAway - fh, fw, fh };
    else if (anchor.getBottom() + kAway + fh <= area.getBottom())
        r = { clampX (anchor.getCentreX() - fw * 0.5f), anchor.getBottom() + kAway, fw, fh };
    else if (anchor.getRight() + kAway + fw <= area.getRight())
        r = { anchor.getRight() + kAway, clampY (anchor.getCentreY() - fh * 0.5f), fw, fh };
    else
        r = { anchor.getX() - kAway - fw, clampY (anchor.getCentreY() - fh * 0.5f), fw, fh };
    setBounds (r.toNearestInt());
    repaint();
    return true;
}

void ValueReadout::resized()
{
    field.setBounds (kPadX, kPadY + kLine1 * rows (shown.name) + kGap, getWidth() - 2 * kPadX, kField);
}

void ValueReadout::paint (juce::Graphics& g)
{
    const bool ed = mode == Mode::Edit;
    g.fillAll (col::overlay.withAlpha (col::overlayAlpha));
    g.setColour (ed ? (invalid ? col::error : col::midi) : col::midi.withAlpha (col::frameAlpha));
    g.drawRect (getLocalBounds(), 1);
    const int x = kPadX, w = getWidth() - 2 * kPadX;
    int y = kPadY;
    g.setColour (col::textDim);
    g.setFont (WcLook::label (11.0f));
    g.drawFittedText (shown.name, x, y, w, kLine1 * rows (shown.name), juce::Justification::centredLeft, rows (shown.name), 1.0f);
    y += kLine1 * rows (shown.name) + kGap;
    if (! ed)
    {
        g.setColour (col::text);
        g.setFont (WcLook::label (15.0f));
        g.drawText (shown.value, x, y, w, kLine2, juce::Justification::centredLeft, true);
    }
    y += (ed ? kField : kLine2) + kGap;
    if (shown.note.isNotEmpty())
    {
        g.setColour (ed && invalid ? col::error : (shown.warn ? col::warn : col::info));
        g.setFont (WcLook::label (11.0f));
        g.drawFittedText (shown.note, x, y, w, kLine3 * rows (shown.note), juce::Justification::centredLeft, rows (shown.note), 1.0f);
    }
}

// ---- typing a value ----
void ValueReadout::edit (juce::Component& c, ValueEdit e)
{
    if (mode == Mode::Edit) cancelEdit();
    target = &c;
    editing = std::move (e);
    invalid = false;
    shown = { editing.name, editing.text, {}, false };
    mode = Mode::Edit;
    setAlpha (1.0f);
    field.setText (editing.text, false);
    if (! layout (true)) { mode = Mode::Hidden; return; }
    setInterceptsMouseClicks (false, true);
    field.setVisible (true);
    setVisible (true);
    toFront (false);
    field.grabKeyboardFocus();
    field.selectAll();
    if (! listening) { juce::Desktop::getInstance().addGlobalMouseListener (&outside); listening = true; }
    startTimerHz (30);
}

void ValueReadout::commitEdit (bool fromKey)
{
    if (mode != Mode::Edit) return;
    const auto text = field.getText().trim();
    if (text.isEmpty()) { cancelEdit(); return; }   // empty + Enter = cancel
    juce::String limited;
    const bool ok = editing.apply != nullptr && editing.apply (text, limited);
    if (ok)
    {
        juce::Component::SafePointer<juce::Component> c = target;
        closeEdit();
        if (c != nullptr && limited.isNotEmpty()) flash (*c, limited, true, 2000);   // "Limited to 20.0 kHz (max)"
        return;
    }
    if (! fromKey) { closeEdit(); return; }          // left with a bad value: dropped, no fuss
    invalid = true;
    refused = field.getText();
    shown.note = juce::String (juce::CharPointer_UTF8 ("Not a value for this control \xe2\x80\x93 try ")) + editing.example;
    layout (true);
    repaint();
    field.grabKeyboardFocus();
}

void ValueReadout::cancelEdit()
{
    if (mode != Mode::Edit) return;
    closeEdit();
}

void ValueReadout::closeEdit()
{
    mode = Mode::Hidden;   // first, so the focus leaving the field does not commit again
    invalid = false;
    if (listening) { juce::Desktop::getInstance().removeGlobalMouseListener (&outside); listening = false; }
    setInterceptsMouseClicks (false, false);
    field.setVisible (false);
    hideNow();
}

bool ValueReadout::TabKey::keyPressed (const juce::KeyPress& k, juce::Component*)
{
    if (owner.mode == Mode::Edit && k.getKeyCode() == juce::KeyPress::tabKey) { owner.commitEdit (true); return true; }
    return false;
}

void ValueReadout::Outside::mouseDown (const juce::MouseEvent& e)
{
    if (owner.mode != Mode::Edit) return;
    if (owner.getScreenBounds().contains (e.getScreenPosition())) return;
    owner.commitEdit (false);   // a click anywhere else: a good value is applied, a bad one dropped
}
} // namespace wc::ui
