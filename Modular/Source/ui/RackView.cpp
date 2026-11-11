#include "RackView.h"
#include "../modules/M259.h"
#include "../modules/MidiInterface.h"
#include "../modules/OutputModule.h"
#include "../modules/M281.h"
#include "../modules/M292.h"
#include "../modules/M266.h"
#include "../modules/M245.h"
#include "../modules/M242.h"
#include "../modules/M248.h"
#include "../modules/M218.h"
#include "../modules/M250.h"
#include "../modules/M252.h"
#include "../modules/M267.h"
#include "../modules/M291.h"
#include "../modules/M666.h"
#include "WcData.h"

namespace wc::ui
{
static juce::Colour gInk { 0xff2146a8 };   // silkscreen colour of the panel being painted (set by ModulePanel::paint)

static juce::String jackTip (const JackSpec& j)
{
    juce::String t = juce::String (j.label).isEmpty() ? juce::String (j.output ? "output" : "input") : juce::String (j.label);
    if (j.kind == JackKind::Mini) t << (j.output ? "  - audio out" : "  - audio in");
    else if (j.pulse) t << (j.output ? "  - pulse out" : "  - pulse / gate in (above 3 V)");
    else t << (j.output ? "  - control voltage out (0..10 V)" : "  - control voltage in");
    const juce::String l (j.label);
    if (! j.output && l == "pitch") t << ", 1.2 V/oct";
    if (l == "lock") t << ": locks the principal oscillator to this signal (or its nearest subharmonic)";
    if (l == "wave") t << ": program - 0-3.3 V saw, 3.3-6.7 V square, above: triangle (remote / both)";
    t << "\nDrag to patch. Right-click to disconnect.";
    return t;
}

// italic silkscreen: the 200-series panels are lettered in lowercase italics
static void drawItalic (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, juce::Justification j, const juce::Font& f, juce::Colour c)
{
    juce::Graphics::ScopedSaveState ss (g);
    const float shear = -0.2f;
    g.addTransform (juce::AffineTransform::shear (shear, 0.0f).translated (-shear * r.getCentreY(), 0.0f));
    g.setColour (c);
    g.setFont (f);
    g.drawText (text, r, j, false);
}

// =================================================================================================
// JackComp
// =================================================================================================
JackComp::JackComp (RackView& rv, int i, const JackSpec& s, int dsp)
    : inst (i), index (dsp), output (s.output), kind (s.kind), pulse (s.pulse), tint (s.tint), rack (rv)
{
    setTooltip (jackTip (s));
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void JackComp::paint (juce::Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const bool hi = rack.isDragging() && rack.canDropOn (*this);
    if (hi)
    {
        g.setColour (col::accent.withAlpha (0.35f));
        g.fillEllipse (c.x - 13.0f, c.y - 13.0f, 26.0f, 26.0f);
    }
    if (kind == JackKind::Banana)
    {
        // colour code of the 200 series: red = pulses, black = control in, blue = control out
        const juce::Colour ins = tint != 0 ? juce::Colour (tint) : (pulse ? juce::Colour (0xffc62127) : (output ? juce::Colour (0xff2452b8) : juce::Colour (0xff17181b)));
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillEllipse (c.x - 10.0f, c.y - 8.5f, 21.0f, 21.0f);
        g.setGradientFill (juce::ColourGradient (ins.brighter (0.35f), c.x - 5, c.y - 8, ins.darker (0.3f), c.x + 6, c.y + 9, false));
        g.fillEllipse (c.x - 10.0f, c.y - 10.0f, 20.0f, 20.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawEllipse (c.x - 10.0f, c.y - 10.0f, 20.0f, 20.0f, 0.8f);
        // nickel socket
        g.setColour (juce::Colour (0xffd8d8d4));
        g.fillEllipse (c.x - 4.6f, c.y - 4.6f, 9.2f, 9.2f);
        g.setColour (juce::Colour (0xff222326));
        g.fillEllipse (c.x - 2.6f, c.y - 2.6f, 5.2f, 5.2f);
    }
    else
    {
        // nickel hex nut
        juce::Path hex;
        for (int k = 0; k < 6; ++k)
        {
            const float a = juce::MathConstants<float>::pi / 3.0f * (float) k + 0.52f;
            const float x = c.x + 10.0f * std::cos (a), y = c.y + 10.0f * std::sin (a);
            if (k == 0) hex.startNewSubPath (x, y); else hex.lineTo (x, y);
        }
        hex.closeSubPath();
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillPath (hex, juce::AffineTransform::translation (0.8f, 1.5f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeeeeee), c.x - 6, c.y - 8, juce::Colour (0xff8d9097), c.x + 6, c.y + 9, false));
        g.fillPath (hex);
        g.setColour (juce::Colour (0xff5d6066));
        g.strokePath (hex, juce::PathStrokeType (0.8f));
        g.setColour (juce::Colour (0xff121316));
        g.fillEllipse (c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f);
    }
    if (isMouseOver())
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawEllipse (c.x - 11.5f, c.y - 11.5f, 23.0f, 23.0f, 1.5f);
    }
}

void JackComp::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { rack.jackMenu (*this); return; }
    rack.beginDrag (*this, e);
}
void JackComp::mouseDrag (const juce::MouseEvent& e) { if (! e.mods.isPopupMenu()) rack.dragMove (e); }
void JackComp::mouseUp (const juce::MouseEvent& e) { if (! e.mods.isPopupMenu()) rack.endDrag (e); }

// =================================================================================================
// ParamSwitch
// =================================================================================================
ParamSwitch::ParamSwitch (juce::RangedAudioParameter& p, int n, const juce::String& l, juce::Colour t)
    : param (p), positions (n), tip (t), attach (p, [this] (float v) { value = juce::roundToInt (v); repaint(); })
{
    topIsOne = dynamic_cast<juce::AudioParameterBool*> (&p) != nullptr;
    if (l.isNotEmpty()) labels = juce::StringArray::fromTokens (l, "|", "");
    attach.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip (p.getName (64) + (l.isNotEmpty() ? ": " + l.replace ("|", " / ") : juce::String()) + "  (click a position)");
}

// chrome bat-handle toggle, vertical, labels printed to its right
void ParamSwitch::paint (juce::Graphics& g)
{
    const float cx = 12.0f, cy = (float) getHeight() * 0.5f;
    // position 0..positions-1 from the top
    int pos = topIsOne ? (value == 1 ? 0 : 1) : value;
    pos = juce::jlimit (0, positions - 1, pos);
    // hex nut + bushing
    juce::Path hex;
    for (int k = 0; k < 6; ++k)
    {
        const float a = juce::MathConstants<float>::pi / 3.0f * (float) k;
        const float x = cx + 8.0f * std::cos (a), y = cy + 8.0f * std::sin (a);
        if (k == 0) hex.startNewSubPath (x, y); else hex.lineTo (x, y);
    }
    hex.closeSubPath();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0f0ee), cx - 5, cy - 6, juce::Colour (0xff8f9298), cx + 5, cy + 7, false));
    g.fillPath (hex);
    g.setColour (juce::Colour (0xff6c6f75));
    g.strokePath (hex, juce::PathStrokeType (0.7f));
    // lever
    const float span = positions == 3 ? 14.0f : 12.0f;
    const float ty = cy + (positions == 3 ? (float) (pos - 1) * span : (pos == 0 ? -span : span));
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawLine (cx + 1.0f, cy + 1.0f, cx + 1.0f, ty + 1.0f, 4.5f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, cx - 2, ty, juce::Colour (0xff9a9da3), cx + 2, cy, false));
    g.drawLine (cx, cy, cx, ty, 4.0f);
    g.setColour (tip);
    g.fillEllipse (cx - 3.6f, ty - 3.6f, 7.2f, 7.2f);
    g.setColour (juce::Colour (0xff6c6f75));
    g.drawEllipse (cx - 3.6f, ty - 3.6f, 7.2f, 7.2f, 0.7f);
    if (labels.isEmpty()) return;
    if (labels.size() == 1)
    {
        drawItalic (g, labels[0], { 0.0f, 0.0f, 24.0f + 30.0f, 10.0f }, juce::Justification::centredLeft, WcLook::label (9.0f), gInk);
        return;
    }
    // position labels
    for (int i = 0; i < juce::jmin (positions, labels.size()); ++i)
    {
        const float y = positions == 3 ? cy + (float) (i - 1) * span : (i == 0 ? cy - span : cy + span);
        drawItalic (g, labels[i], { 24.0f, y - 6.0f, (float) getWidth() - 24.0f, 12.0f }, juce::Justification::centredLeft,
                    WcLook::label (9.5f), i == pos ? gInk : gInk.withAlpha (0.55f));
    }
}

void ParamSwitch::mouseDown (const juce::MouseEvent& e)
{
    const float rel = (float) e.y / (float) getHeight();
    int pos;
    if (positions == 3) pos = rel < 0.38f ? 0 : (rel > 0.62f ? 2 : 1);
    else pos = rel < 0.5f ? 0 : 1;
    const int v = topIsOne ? (pos == 0 ? 1 : 0) : pos;
    attach.setValueAsCompleteGesture ((float) v);
}

ParamButton::ParamButton (juce::RangedAudioParameter& p)
    : param (p), attach (p, [this] (float v) { value = v; repaint(); })
{
    attach.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip (p.getName (64) + " - click to change");
}

void ParamButton::mouseDown (const juce::MouseEvent&)
{
    down = true;
    const int steps = juce::jmax (2, param.getNumSteps());
    const float hi = param.convertFrom0to1 (1.0f), lo = param.convertFrom0to1 (0.0f);
    float next = value + (hi - lo) / (float) (steps - 1);
    if (next > hi + 1.0e-3f) next = lo;
    attach.setValueAsCompleteGesture (next);
    repaint();
}

void ParamButton::paint (juce::Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillEllipse (c.x - 11.0f, c.y - 9.5f, 23.0f, 23.0f);
    g.setColour (juce::Colour (0xff9c9d9a));
    g.fillEllipse (c.x - 11.5f, c.y - 11.5f, 23.0f, 23.0f);
    const float r = down ? 8.0f : 9.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xffd5d5d1 : 0xffffffff), c.x - 4, c.y - 6, juce::Colour (0xffc4c4bf), c.x + 4, c.y + 6, false));
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
}

// white momentary button, as on the 281t
void PushButton::paint (juce::Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillEllipse (c.x - 10.0f, c.y - 9.0f, 21.0f, 21.0f);
    g.setColour (juce::Colour (0xff9c9d9a));
    g.fillEllipse (c.x - 10.5f, c.y - 10.5f, 21.0f, 21.0f);
    const float r = down ? 7.5f : 8.5f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xffcfcfcb : 0xffffffff), c.x - 4, c.y - 6, juce::Colour (0xffbdbdb8), c.x + 4, c.y + 6, false));
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
}

// =================================================================================================
// ProgSwitch (248)
// =================================================================================================
ProgSwitch::ProgSwitch (WcProcessor& p, const juce::String& pre, const juce::String& f, int v, bool h, juce::Colour t)
    : proc (p), prefix (pre), fn (f), value (v), horiz (h), tip (t)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip (fn == "stage" ? "Stage No.: left / right picks the stage to program"
                : fn == "clear" ? "Clear (up): clears the program of all 16 stages"
                : value >= 0 ? "Up: select for the stage being programmed" : "Up: on / down: off, for the stage being programmed");
}

int ProgSwitch::editStage() const
{
    auto* e = proc.apvts.getRawParameterValue (prefix + "edit");
    return e != nullptr ? (int) std::lround (e->load()) : 1;
}

void ProgSwitch::set (const juce::String& id, float plain)
{
    if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

void ProgSwitch::mouseDown (const juce::MouseEvent& e)
{
    const bool first = horiz ? e.position.x < (float) getWidth() * 0.5f : e.position.y < (float) getHeight() * 0.5f;   // up / left
    dir = first ? 1 : -1;
    repaint();
    if (fn == "stage")
    {
        int s = editStage() + (first ? -1 : 1);
        if (s < 1) s = 16;
        if (s > 16) s = 1;
        set (prefix + "edit", (float) s);
        return;
    }
    if (fn == "clear")
    {
        if (! first) return;
        for (int st = 1; st <= 16; ++st)
            for (int f = 0; f < wc::M248::F_Count; ++f)
                set (prefix + wc::M248::fnName (f) + "_" + juce::String (st), f == wc::M248::F_TRange ? 2.0f : 0.0f);
        return;
    }
    const juce::String id = prefix + fn + "_" + juce::String (editStage());
    if (value >= 0) { if (first) set (id, (float) value); }
    else set (id, first ? 1.0f : 0.0f);
}

void ProgSwitch::paint (juce::Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    g.setColour (juce::Colour (0xff8d8e8a));
    g.fillEllipse (c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f);
    g.setColour (juce::Colour (0xffd9d9d4));
    g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
    juce::Point<float> end = c;
    if (horiz) end.x += dir == 0 ? 0.0f : (dir > 0 ? -12.0f : 12.0f);
    else end.y += dir == 0 ? -3.0f : (dir > 0 ? -13.0f : 13.0f);
    g.setColour (juce::Colour (0xffb8b9b5));
    g.drawLine ({ c, end }, 3.5f);
    g.setColour (tip);
    g.fillEllipse (end.x - 4.5f, end.y - 4.5f, 9.0f, 9.0f);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.fillEllipse (end.x - 2.5f, end.y - 3.0f, 3.0f, 3.0f);
}

// =================================================================================================
// ModulePanel
// =================================================================================================
ModulePanel::ModulePanel (RackView& rv, WcProcessor& p, int i)
    : inst (i), rack (rv), proc (p), spec (specFor (typeOfInst (i)))
{
    prefix = instPrefix (i);
    setSize ((int) spec.width, (int) spec.height);
    for (auto& c : spec.controls)
    {
        if (c.style == CtrlStyle::Prog)
        {
            const juce::String lab (c.label);
            const int v = lab.startsWithChar ('=') ? lab.substring (1).getIntValue() : -1;
            const bool h = juce::String (c.suffix) == "stage";
            auto ps = std::make_unique<ProgSwitch> (proc, prefix, c.suffix, v, h, juce::Colour (c.colour == 0 ? 0xff2f62d0 : c.colour));
            if (h) ps->setBounds ((int) c.x - 18, (int) c.y - 10, 36, 20);
            else ps->setBounds ((int) c.x - 10, (int) c.y - 17, 20, 34);
            addAndMakeVisible (*ps);
            progs.push_back (std::move (ps));
            continue;
        }
        if (c.style == CtrlStyle::Push)
        {
            const juce::String id (c.suffix);
            std::unique_ptr<PushButton> b;
            const auto type = typeOfInst (inst);
            if (type == MT_281)
                b = std::make_unique<PushButton> (&proc.rack.m281 (subIndex (inst))->manual[juce::jlimit (0, 3, id.getTrailingIntValue() - 1)]);
            else if (type == MT_245)
                b = std::make_unique<PushButton> (id == "start" ? &proc.rack.m245()->btnStart : &proc.rack.m245()->btnStop,
                                                  id == "start" ? "Start the pulser" : "Stop the pulser (when stopped: a single pulse)");
            else if (type == MT_242)
            {
                auto* m = proc.rack.m242();
                if (id == "cell")
                {
                    b = std::make_unique<PushButton> (nullptr, "on / off: toggle the matrix point under the cursor (row and column encoders)");
                    b->onPress = [this]
                    {
                        auto get = [this] (const char* s) { auto* v = proc.apvts.getRawParameterValue (prefix + s); return v ? (int) std::lround (v->load()) : 1; };
                        toggleMatrixCell (get ("row") - 1, get ("col") - 1);
                    };
                }
                else
                    b = std::make_unique<PushButton> (id == "start" ? &m->btnStart : (id == "stop" ? &m->btnStop : &m->btnSingle),
                                                      id == "start" ? "Start" : (id == "stop" ? "Stop" : "Single pulse"));
            }
            else if (type == MT_248)
            {
                auto* m = proc.rack.m248();
                const int fgI = id.getTrailingIntValue() - 1;
                if (id.startsWith ("adv")) b = std::make_unique<PushButton> (&m->btnAdvance[juce::jlimit (0, 1, fgI)], "Advance to the next stage");
                else if (id.startsWith ("start")) b = std::make_unique<PushButton> (&m->btnStart[juce::jlimit (0, 1, fgI)], "Start (when stopped)");
                else if (id.startsWith ("reset")) b = std::make_unique<PushButton> (&m->btnReset[juce::jlimit (0, 1, fgI)], "Reset to the first stage");
                else
                {
                    b = std::make_unique<PushButton> (nullptr, id.startsWith ("disp") ? "Display: show this function generator's stage on the LEDs"
                                                                : id == "load" ? "Load, then 1-12" : id == "save" ? "Save, then 1-12"
                                                                : id == "scale" ? "Scale, then 10 (M) / 11 (m) / 12 (ch)" : id == "key" ? "Key, then C .. B"
                                                                : id == "tune" ? "Autotune (the plugin never drifts)" : "1-12: preset / key / scale");
                    b->onPress = [this, id, m]
                    {
                        if (id.startsWith ("disp")) { m->displayFg.store (id.getTrailingIntValue() - 1); return; }
                        if (id == "load") { m->armed.store (m->armed.load() == 1 ? 0 : 1); return; }
                        if (id == "save") { m->armed.store (m->armed.load() == 2 ? 0 : 2); return; }
                        if (id == "scale") { m->armed.store (m->armed.load() == 3 ? 0 : 3); return; }
                        if (id == "key") { m->armed.store (m->armed.load() == 4 ? 0 : 4); return; }
                        if (! id.startsWith ("n")) return;
                        const int n = id.substring (1).getIntValue();
                        auto setP = [this] (const juce::String& pid, float v) { if (auto* p = proc.apvts.getParameter (prefix + pid)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
                        switch (m->armed.load())
                        {
                            case 1: proc.marfLoad (n); break;
                            case 2: proc.marfSave (n); break;
                            case 3: if (n == 10) setP ("scale", 2.0f); else if (n == 11) setP ("scale", 0.0f); else if (n == 12) setP ("scale", 1.0f); break;
                            case 4: setP ("key", (float) (n - 1)); break;
                            default: break;
                        }
                        m->armed.store (0);
                    };
                }
            }
            else if (type == MT_250)
            {
                auto* m = proc.rack.m250();
                if (id == "start") b = std::make_unique<PushButton> (&m->btnStart, "Start");
                else if (id == "stop") b = std::make_unique<PushButton> (&m->btnStop, "Stop");
                else
                {
                    b = std::make_unique<PushButton> (nullptr, id == "set" ? "Jump & loop: set the loop stage, then the jump-to stage, then the count (centre knob)"
                                                                 : id == "vset" ? "Voltage set 1 / 2 (the outer knobs edit the selected set)"
                                                                 : id == "editbtn" ? "Edit mode: the switches below program the stage picked with the centre knob"
                                                                 : id == "addrbtn" ? "Stage addressing: off / strobed / continuous"
                                                                 : id == "remote" ? "Remote enable (preset manager)"
                                                                 : "Edit mode: programs the stage selected with the centre knob");
                    b->onPress = [this, id]
                    {
                        auto raw = [this] (const juce::String& pid) { auto* v = proc.apvts.getRawParameterValue (prefix + pid); return v != nullptr ? v->load() : 0.0f; };
                        auto put = [this] (const juce::String& pid, float v) { if (auto* p = proc.apvts.getParameter (prefix + pid)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
                        const juce::String st = "_" + juce::String ((int) std::lround (raw ("edit")));
                        auto cycle = [&] (const juce::String& pid, int n) { put (pid, (float) (((int) std::lround (raw (pid)) + 1) % n)); };
                        if (id == "editbtn") { put ("editmode", raw ("editmode") > 0.5f ? 0.0f : 1.0f); repaint(); return; }
                        // the switches program a stage only in edit mode (the voltage and time knobs always work)
                        if (raw ("editmode") < 0.5f && id != "vset" && id != "remote" && id != "addrbtn") return;
                        if (id == "tq" || id == "ti" || id == "vq" || id == "vi" || id == "p1" || id == "p2" || id == "text" || id == "vext" || id == "fix")
                            put (id + st, raw (id + st) > 0.5f ? 0.0f : 1.0f);
                        else if (id == "trng") cycle ("trng" + st, 4);
                        else if (id == "vrng") cycle ("vrng" + st, 6);
                        else if (id == "mode") cycle ("mode" + st, 4);
                        else if (id == "vset") put ("set", raw ("set") > 0.5f ? 0.0f : 1.0f);
                        else if (id == "addrbtn") cycle ("addr", 3);
                        else if (id == "remote") put ("remote", raw ("remote") > 0.5f ? 0.0f : 1.0f);
                        else if (id == "set")
                        {
                            const int step = (int) std::lround (raw ("setstep")), e = (int) std::lround (raw ("edit"));
                            if (step == 0) { put ("loopend", (float) e); put ("setstep", 1.0f); }
                            else if (step == 1) { put ("loopto", (float) e); put ("setstep", 2.0f); }
                            else { put ("loops", e >= 16 ? 0.0f : (float) e); put ("setstep", 0.0f); }
                        }
                        repaint();
                    };
                }
            }
            else if (type == MT_252)
            {
                auto* m = proc.rack.m252();
                if (id == "reset") b = std::make_unique<PushButton> (&m->btnReset, "Reset: all rings back to cell 1");
                else
                {
                    b = std::make_unique<PushButton> (nullptr, id == "red" || id == "green" || id == "blue" ? "Adds / removes this colour's pulse in the cursor cell"
                                                                 : id == "rhythm" ? "Rhythm select: user defined / Euclidean (pattern # sets the pulses of the edited ring)"
                                                                 : id.startsWith ("assign") ? "Assign / delta-t: in free sync, this clock runs at its own rate (host parameter Delta-t Scale)"
                                                                 : id == "display" ? "Display layer: pulses / cv 1-3 / cv 4-6 + subdiv / all" : "");
                    b->onPress = [this, id] { action252 (id); };
                }
            }
            else if (type == MT_291)
            {
                b = std::make_unique<PushButton> (nullptr, id == "node" ? "Node: which filter (A / B / C) the encoders edit"
                                                             : id == "play" ? "Stop / play: morph through the eight stages"
                                                             : id == "editsel" ? "Edit select: step to the next stage to program"
                                                             : id == "modtype" ? "Modulation destination: a.m. / b.w. / f.m."
                                                             : id == "solo" ? "Solo: only the selected node reaches the 'all' outputs"
                                                             : "Remote enable (preset manager)");
                b->onPress = [this, id]
                {
                    auto raw = [this] (const char* pid) { auto* v = proc.apvts.getRawParameterValue (prefix + pid); return v != nullptr ? v->load() : 0.0f; };
                    auto put = [this] (const char* pid, float v) { if (auto* p = proc.apvts.getParameter (prefix + pid)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
                    if (id == "node") put ("node", (float) (((int) std::lround (raw ("node")) + 1) % 3));
                    else if (id == "play") put ("play", raw ("play") > 0.5f ? 0.0f : 1.0f);
                    else if (id == "editsel") put ("edit", (float) ((int) std::lround (raw ("edit")) % 8 + 1));
                    else if (id == "modtype") put ("modtype", (float) (((int) std::lround (raw ("modtype")) + 1) % 3));
                    else if (id == "solo") put ("solo", raw ("solo") > 0.5f ? 0.0f : 1.0f);
                    else if (id == "remote") put ("remote", raw ("remote") > 0.5f ? 0.0f : 1.0f);
                    rebindDynamic();
                    repaint();
                };
            }
            else if (type == MT_267)
            {
                b = std::make_unique<PushButton> (nullptr, id == "remote" ? "Remote enable (preset manager)" : "Mod type: amplitude / frequency / bandwidth");
                b->onPress = [this, id]
                {
                    auto raw = [this] (const juce::String& pid) { auto* v = proc.apvts.getRawParameterValue (prefix + pid); return v != nullptr ? v->load() : 0.0f; };
                    auto put = [this] (const juce::String& pid, float v) { if (auto* p = proc.apvts.getParameter (prefix + pid)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
                    if (id == "remote") put ("remote", raw ("remote") > 0.5f ? 0.0f : 1.0f);
                    else put (id, (float) (((int) std::lround (raw (id)) + 1) % 3));
                    repaint();
                };
            }
            else if (type == MT_264)
                b = std::make_unique<PushButton> (nullptr, "press to auto tune / hold to initial tune (the plugin's oscillators never drift)");
            if (b == nullptr) continue;
            b->setBounds ((int) c.x - 13, (int) c.y - 13, 26, 26);
            addAndMakeVisible (*b);
            pushes.push_back (std::move (b));
            continue;
        }
        if (c.suffix[0] == '@')
        {
            auto fk = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
            const int size = c.style == CtrlStyle::BigKnob ? 74 : (c.style == CtrlStyle::Knob ? 54 : 38);
            fk->getProperties().set ("style", c.style == CtrlStyle::BigKnob ? 0 : (c.style == CtrlStyle::Knob ? 1 : 2));
            fk->getProperties().set ("cap", (juce::int64) c.colour);
            fk->setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
            fk->setScrollWheelEnabled (false);
            fk->setRange (0.0, 1.0);
            fk->setValue (juce::String (c.suffix) == "@phase" ? 0.5 : 0.0, juce::dontSendNotification);
            fk->setBounds ((int) c.x - size / 2, (int) c.y - size / 2, size, size);
            const juce::String nm (c.suffix);
            fk->setTooltip (nm == "@cv" ? "C.V. value of the selected output for the cursor cell (fully left = --, no change)"
                            : nm == "@offset" ? "Behind / ahead: moves the cursor cell earlier (left) or later (right)"
                            : nm == "@subc" ? "Cell subdivide: spacing of the subdivided pulses (0 even .. 1 bunched late)"
                            : "Phase: turn to rotate the cells of the edited ring (active display layer)");
            fk->onValueChange = [this, nm] { free252 (nm, freeKnobs[nm]->getValue()); };
            addAndMakeVisible (*fk);
            freeKnobs[nm] = std::move (fk);
            continue;
        }
        auto* param = proc.apvts.getParameter (resolveId (c.suffix));
        if (param != nullptr && c.style == CtrlStyle::Button)
        {
            auto b = std::make_unique<ParamButton> (*param);
            b->setBounds ((int) c.x - 14, (int) c.y - 14, 28, 28);
            addAndMakeVisible (*b);
            buttons.push_back (std::move (b));
            continue;
        }
        jassert (param != nullptr);
        if (param == nullptr) continue;
        if (c.style == CtrlStyle::Toggle || c.style == CtrlStyle::Switch3)
        {
            auto sw = std::make_unique<ParamSwitch> (*param, c.style == CtrlStyle::Switch3 ? 3 : 2, c.label,
                                                     c.colour == 0xff2c55b4 ? juce::Colour (0xff2f62d0)
                                                         : (c.colour == 0 || c.colour == 0xffe6e6e1 ? juce::Colour (0xfff7f7f5) : juce::Colour (c.colour)));
            const bool single = juce::String (c.label).isNotEmpty() && ! juce::String (c.label).containsChar ('|');
            const int h = c.style == CtrlStyle::Switch3 ? 46 : (single ? 50 : 38);
            sw->setBounds ((int) c.x - 12, (int) c.y - h / 2, single ? 56 : 74, h);
            addAndMakeVisible (*sw);
            switches.push_back (std::move (sw));
            continue;
        }
        if (c.style == CtrlStyle::Fader)
        {
            auto f = std::make_unique<juce::Slider> (juce::Slider::LinearVertical, juce::Slider::NoTextBox);
            const int len = c.scale2 != nullptr ? juce::String (c.scale2).getIntValue() : 120;
            f->setBounds ((int) c.x - 13, (int) c.y - len / 2, 26, len);
            f->setScrollWheelEnabled (false);
            f->setPopupDisplayEnabled (true, true, nullptr);
            f->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
            f->setTooltip (param->getName (64));
            addAndMakeVisible (*f);
            attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, resolveId (c.suffix), *f));
            sliders.push_back (std::move (f));
            sliderIds.push_back (c.suffix);
            boundIds.push_back (resolveId (c.suffix));
            continue;
        }
        auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        const int size = c.style == CtrlStyle::BigKnob ? 74 : (c.style == CtrlStyle::Knob ? 54 : 38);
        s->getProperties().set ("style", c.style == CtrlStyle::BigKnob ? 0 : (c.style == CtrlStyle::Knob ? 1 : 2));
        s->getProperties().set ("cap", (juce::int64) c.colour);
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        s->setBounds ((int) c.x - size / 2, (int) c.y - size / 2, size, size);
        s->setScrollWheelEnabled (false);
        s->setPopupDisplayEnabled (true, true, nullptr);
        s->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        s->setTooltip (param->getName (64));
        addAndMakeVisible (*s);
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, resolveId (c.suffix), *s));
        sliders.push_back (std::move (s));
        sliderIds.push_back (c.suffix);
        boundIds.push_back (resolveId (c.suffix));
    }
    int nIn = 0, nOut = 0;
    for (auto& j : spec.jacks)
    {
        auto jc = std::make_unique<JackComp> (rack, inst, j, j.output ? nOut++ : nIn++);
        jc->setBounds ((int) j.x - 13, (int) j.y - 13, 26, 26);
        addAndMakeVisible (*jc);
        jacks.push_back (std::move (jc));
    }
    if (isRemovable (i))
    {
        removeButton.setTooltip ("Remove this module from the rack");
        removeButton.setBounds ((int) spec.width - 26, 7, 18, 18);
        removeButton.onClick = [this] { proc.setPresent (inst, false); };
        addAndMakeVisible (removeButton);
    }
}

// printed scale around a knob: numbers (or dots for "*") on one or two rings
static void drawScale (juce::Graphics& g, const ControlSpec& c)
{
    const float r = c.style == CtrlStyle::BigKnob ? 34.0f : (c.style == CtrlStyle::Knob ? 24.0f : 16.0f);
    const float a0 = juce::MathConstants<float>::pi * 1.2f, a1 = juce::MathConstants<float>::pi * 2.8f;
    auto ring = [&] (const char* sc, float tr, float size)
    {
        if (sc == nullptr) return;
        const auto marks = juce::StringArray::fromTokens (sc, "|", "");
        for (int i = 0; i < marks.size(); ++i)
        {
            const float t = marks.size() == 1 ? 0.5f : (float) i / (float) (marks.size() - 1);
            const float a = a0 + (a1 - a0) * t;
            const float x = c.x + tr * std::sin (a), y = c.y - tr * std::cos (a);
            if (marks[i] == "*") { g.setColour (gInk); g.fillEllipse (x - 1.8f, y - 1.8f, 3.6f, 3.6f); }
            else drawItalic (g, marks[i], { x - 18.0f, y - 6.0f, 36.0f, 12.0f }, juce::Justification::centred, WcLook::label (size), gInk);
        }
    };
    if (c.scale2 != nullptr) { ring (c.scale, r + 26.0f, 8.5f); ring (c.scale2, r + 13.0f, 7.5f); }
    else ring (c.scale, r + (c.style == CtrlStyle::BigKnob ? 13.0f : 11.0f), c.style == CtrlStyle::BigKnob ? 9.0f : 8.0f);
}

static void drawArrowLine (juce::Graphics& g, const LineSpec& l)
{
    juce::Line<float> line (l.x1, l.y1, l.x2, l.y2);
    g.setColour (gInk);
    if (l.dashed)
    {
        const float dashes[] = { 4.0f, 3.0f };
        g.drawDashedLine (line, dashes, 2, l.width);
    }
    else g.drawLine (line, l.width);
    if (l.arrow)
    {
        juce::Path head;
        head.addArrow (juce::Line<float> (line.getPointAlongLineProportionally (0.8f), line.getEnd()), 0.0f, 7.0f, 7.0f);
        g.fillPath (head);
    }
}

// Controls written with a placeholder follow the panel's edit selectors:
//   {e} = stage being edited, {n} = node a/b/c (291), {s} = voltage set 1/2 (250)
juce::String ModulePanel::resolveId (const juce::String& suffix) const
{
    juce::String id = suffix;
    auto raw = [this] (const char* pid) { auto* v = proc.apvts.getRawParameterValue (prefix + pid); return v != nullptr ? v->load() : 0.0f; };
    if (id.contains ("{e}")) id = id.replace ("{e}", juce::String ((int) std::lround (raw ("edit"))));
    if (id.contains ("{n}")) id = id.replace ("{n}", juce::String::charToString ((juce::juce_wchar) ('a' + juce::jlimit (0, 2, (int) std::lround (raw ("node"))))));
    if (id.contains ("{s}")) id = id.replace ("{s}", juce::String (1 + juce::jlimit (0, 1, (int) std::lround (raw ("set")))));
    return prefix + id;
}

void ModulePanel::rebindDynamic()
{
    for (size_t i = 0; i < sliders.size(); ++i)
    {
        if (! sliderIds[i].containsChar ('{')) continue;
        const auto id = resolveId (sliderIds[i]);
        if (id == boundIds[i]) continue;
        boundIds[i] = id;
        attachments[i].reset();
        if (proc.apvts.getParameter (id) == nullptr) continue;
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *sliders[i]);
        sliders[i]->setTooltip (proc.apvts.getParameter (id)->getName (64));
    }
}

int ModulePanel::cursor252() const
{
    const int ring = juce::jlimit (0, wc::M252::kRings - 1, (int) std::lround (proc.apvts.getRawParameterValue (prefix + "ring")->load()));
    const int cell = juce::jlimit (0, wc::M252::kSize[ring] - 1, (int) std::lround (proc.apvts.getRawParameterValue (prefix + "cell")->load()));
    return wc::M252::base (ring) + cell;
}

void ModulePanel::sync252Knobs()
{
    auto* m = proc.rack.m252();
    const int c = cursor252();
    const int sel = (int) std::lround (proc.apvts.getRawParameterValue (prefix + "cvsel")->load());
    const float v = m->cv[c][juce::jlimit (0, 5, sel)].load();
    freeKnobs["@cv"]->setValue (v < 0.0f ? 0.0 : 0.02 + 0.98 * v / 10.0, juce::dontSendNotification);
    freeKnobs["@offset"]->setValue (0.5 + 0.5 * m->offset[c].load(), juce::dontSendNotification);
    freeKnobs["@subc"]->setValue (m->subCurve[c].load(), juce::dontSendNotification);
}

void ModulePanel::free252 (const juce::String& name, double v)
{
    auto* m = proc.rack.m252();
    const int c = cursor252();
    if (name == "@cv")
    {
        const int sel = juce::jlimit (0, 5, (int) std::lround (proc.apvts.getRawParameterValue (prefix + "cvsel")->load()));
        m->cv[c][sel].store (v < 0.02 ? -1.0f : (float) ((v - 0.02) / 0.98 * 10.0));
    }
    else if (name == "@offset") m->offset[c].store ((float) (v * 2.0 - 1.0));
    else if (name == "@subc") m->subCurve[c].store ((float) v);
    else if (name == "@phase")
    {
        const int step = (int) std::lround (v * 16.0);
        int d = step - phaseStep252;
        phaseStep252 = step;
        const int ring = (int) std::lround (proc.apvts.getRawParameterValue (prefix + "ring")->load());
        const int n = wc::M252::kSize[ring], b0 = wc::M252::base (ring);
        const int layer = (int) std::lround (proc.apvts.getRawParameterValue (prefix + "layer")->load());
        while (d != 0)
        {
            const int dir = d > 0 ? 1 : -1;
            // rotate the active layer of the ring by one cell
            auto rot = [&] (auto get, auto set)
            {
                std::vector<float> tmp ((size_t) n);
                for (int i = 0; i < n; ++i) tmp[(size_t) i] = get (b0 + i);
                for (int i = 0; i < n; ++i) set (b0 + ((i + dir) % n + n) % n, tmp[(size_t) i]);
            };
            if (layer == 0 || layer == 3) { rot ([m] (int i) { return (float) m->rgb[i].load(); }, [m] (int i, float x) { m->rgb[i].store ((int) x); });
                                            rot ([m] (int i) { return m->offset[i].load(); }, [m] (int i, float x) { m->offset[i].store (x); }); }
            if (layer == 1 || layer == 3) for (int o = 0; o < 3; ++o) rot ([m, o] (int i) { return m->cv[i][o].load(); }, [m, o] (int i, float x) { m->cv[i][o].store (x); });
            if (layer == 2 || layer == 3)
            {
                for (int o = 3; o < 6; ++o) rot ([m, o] (int i) { return m->cv[i][o].load(); }, [m, o] (int i, float x) { m->cv[i][o].store (x); });
                rot ([m] (int i) { return (float) m->sub[i].load(); }, [m] (int i, float x) { m->sub[i].store ((int) x); });
                rot ([m] (int i) { return m->subCurve[i].load(); }, [m] (int i, float x) { m->subCurve[i].store (x); });
            }
            d -= dir;
        }
    }
    repaint();
}

void ModulePanel::action252 (const juce::String& id)
{
    auto* m = proc.rack.m252();
    const int c = cursor252();
    auto raw = [this] (const char* pid) { return proc.apvts.getRawParameterValue (prefix + pid)->load(); };
    auto put = [this] (const char* pid, float v) { if (auto* p = proc.apvts.getParameter (prefix + pid)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    if (id == "red" || id == "green" || id == "blue") m->rgb[c].store (m->rgb[c].load() ^ (id == "red" ? 1 : (id == "green" ? 2 : 4)));
    else if (id == "play") put ("play", raw ("play") > 0.5f ? 0.0f : 1.0f);
    else if (id == "display") put ("layer", (float) (((int) std::lround (raw ("layer")) + 1) % 4));
    else if (id == "cvselect") { put ("cvsel", (float) (((int) std::lround (raw ("cvsel")) + 1) % 6)); }
    else if (id == "editbtn") put ("edit", raw ("edit") > 0.5f ? 0.0f : 1.0f);
    else if (id == "clear") m->clearCell (c, (int) std::lround (raw ("layer")));
    else if (id == "subcount") m->sub[c].store ((m->sub[c].load() + 1) % 8);
    else if (id == "copy")
    {
        clip252.rgb = m->rgb[c].load(); clip252.sub = m->sub[c].load(); clip252.subc = m->subCurve[c].load(); clip252.off = m->offset[c].load();
        for (int o = 0; o < 6; ++o) clip252.cv[o] = m->cv[c][o].load();
        clip252.valid = true;
    }
    else if (id == "paste" && clip252.valid)
    {
        const int layer = (int) std::lround (raw ("layer"));
        if (layer == 0 || layer == 3) { m->rgb[c].store (clip252.rgb); m->offset[c].store (clip252.off); }
        if (layer == 1 || layer == 3) for (int o = 0; o < 3; ++o) m->cv[c][o].store (clip252.cv[o]);
        if (layer == 2 || layer == 3) { for (int o = 3; o < 6; ++o) m->cv[c][o].store (clip252.cv[o]); m->sub[c].store (clip252.sub); m->subCurve[c].store (clip252.subc); }
    }
    else if (id == "rhythm") { put ("euclid", raw ("euclid") > 0.5f ? 0.0f : 1.0f); patt252 = -1; }
    else if (id == "sync") put ("sync", (float) (((int) std::lround (raw ("sync")) + 1) % 3));
    else if (id == "remote") put ("remote", raw ("remote") > 0.5f ? 0.0f : 1.0f);
    else if (id.startsWith ("assign"))
    {
        const juce::String pid = "dt" + juce::String (id.getTrailingIntValue());
        if (auto* p = proc.apvts.getParameter (prefix + pid)) p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
    }
    sync252Knobs();
    repaint();
}

void ModulePanel::paint252 (juce::Graphics& g)
{
    auto* m = proc.rack.m252();
    const auto& d = spec.sections[0];
    const auto ctr = juce::Point<float> (d.x + d.w * 0.5f, d.y + d.h * 0.5f);
    const float rOut = d.w * 0.5f * 0.94f, rIn = rOut * 0.16f;
    const int layer = (int) std::lround (proc.apvts.getRawParameterValue (prefix + "layer")->load());
    const int editRing = (int) std::lround (proc.apvts.getRawParameterValue (prefix + "ring")->load());
    const int cur = cursor252();
    const bool edit = proc.apvts.getRawParameterValue (prefix + "edit")->load() > 0.5f;
    const bool blink = (juce::Time::getMillisecondCounter() / 250) % 2 == 0;
    for (int r = 0; r < wc::M252::kRings; ++r)
    {
        const int n = wc::M252::kSize[r];
        const float r0 = rIn + (rOut - rIn) * (float) r / (float) wc::M252::kRings, r1 = rIn + (rOut - rIn) * (float) (r + 1) / (float) wc::M252::kRings;
        bool playing[3] {}; int pc[3];
        for (int k = 0; k < 3; ++k) { pc[k] = m->playCell[k].load(); playing[k] = m->playRing[k].load() == r; }
        for (int i = 0; i < n; ++i)
        {
            const int ci = wc::M252::base (r) + i;
            const float a0 = juce::MathConstants<float>::twoPi * (float) i / (float) n + 0.02f, a1 = juce::MathConstants<float>::twoPi * (float) (i + 1) / (float) n - 0.02f;
            juce::Path seg;
            seg.addPieSegment (juce::Rectangle<float> (ctr.x - r1 + 1.0f, ctr.y - r1 + 1.0f, 2.0f * r1 - 2.0f, 2.0f * r1 - 2.0f), a0, a1, (r0 + 1.0f) / (r1 - 1.0f));
            const int bits = m->rgb[ci].load();
            float rr = 0, gg = 0, bb = 0;
            auto has = [&] (int o) { return m->cv[ci][o].load() >= 0.0f; };
            if (layer == 0) { rr = (bits & 1) ? 1.0f : 0; gg = (bits & 2) ? 0.85f : 0; bb = (bits & 4) ? 1.0f : 0; }
            else if (layer == 1) { rr = has (0) ? 1.0f : 0; gg = has (1) ? 0.85f : 0; bb = has (2) ? 1.0f : 0; }
            else if (layer == 2) { rr = (has (3) || m->sub[ci].load() > 0) ? 1.0f : 0; gg = has (4) ? 0.85f : 0; bb = has (5) ? 1.0f : 0; }
            else { const bool any = bits != 0 || m->sub[ci].load() > 0 || has (0) || has (1) || has (2) || has (3) || has (4) || has (5); rr = gg = bb = any ? 0.85f : 0.0f; }
            const bool lit = rr + gg + bb > 0.0f;
            bool head = false;
            for (int k = 0; k < 3; ++k) head = head || (playing[k] && pc[k] == i);
            juce::Colour c = lit ? juce::Colour::fromFloatRGBA (juce::jmax (rr, 0.12f), juce::jmax (gg, 0.12f), juce::jmax (bb, 0.12f), 1.0f) : juce::Colour (0xff1c1d22);
            if (head) c = lit ? c.brighter (0.6f) : juce::Colour (0xff5a5c66);
            g.setColour (c);
            g.fillPath (seg);
            if (edit && ci == cur && blink) { g.setColour (juce::Colours::white); g.strokePath (seg, juce::PathStrokeType (2.0f)); }
            else if (r == editRing) { g.setColour (juce::Colour (0xff6a7aa8)); g.strokePath (seg, juce::PathStrokeType (0.8f)); }
        }
    }
    // the seven-segment windows
    auto seg7 = [&] (const wc::SectionSpec& sc, const juce::String& txt)
    {
        auto rr = juce::Rectangle<float> (sc.x, sc.y, sc.w, sc.h);
        g.setColour (juce::Colour (0xff062a12)); g.fillRoundedRectangle (rr, 2.0f);
        g.setColour (juce::Colour (0xff35e06a)); g.setFont (juce::FontOptions (rr.getHeight() * 0.62f, juce::Font::bold));
        g.drawText (txt, rr, juce::Justification::centred);
    };
    const int sel = juce::jlimit (0, 5, (int) std::lround (proc.apvts.getRawParameterValue (prefix + "cvsel")->load()));
    const float cvv = m->cv[cur][sel].load();
    seg7 (spec.sections[1], juce::String (sel + 1) + (cvv < 0.0f ? juce::String (":--") : ":" + juce::String (cvv, 1)));
    seg7 (spec.sections[2], juce::String (m->sub[cur].load()));
    seg7 (spec.sections[3], juce::String (wc::M252::kSize[juce::jlimit (0, 10, editRing)]));
    seg7 (spec.sections[4], juce::String ((int) std::lround (proc.apvts.getRawParameterValue (prefix + "pattern")->load())));
    seg7 (spec.sections[5], juce::String ((int) std::lround (proc.apvts.getRawParameterValue (prefix + "bpm")->load())));
}

// The maker's mark engraved in the aluminium at the foot of every panel:
// "CALABI YAU  (isologo)  <model>", cut into the metal rather than printed.
static void drawEngravedFooter (juce::Graphics& g, const ModuleSpec& spec, float panelW)
{
    static juce::Image logo = juce::ImageCache::getFromMemory (WcData::isologo_png, WcData::isologo_pngSize);
    const float y = spec.footY > 0.0f ? spec.footY : spec.height - 9.0f;
    const float h = juce::jlimit (9.0f, 15.0f, panelW * 0.016f + 8.0f);
    const auto f = WcLook::title (h * 0.82f);
    const juce::String left ("CALABI YAU"), right (spec.engr);
    const float lw = juce::GlyphArrangement::getStringWidth (f, left), rw = juce::GlyphArrangement::getStringWidth (f, right);
    const float gapW = h * 0.75f, markW = h * 1.25f;
    const float total = lw + gapW + markW + gapW + rw;
    float x = (panelW - total) * 0.5f;
    auto cut = [&] (const juce::String& t, float tx, float tw)
    {
        // engraving: a dark bite above, a bright highlight below, then the metal-coloured letter
        g.setColour (juce::Colours::black.withAlpha (0.38f));
        g.setFont (f);
        g.drawText (t, juce::Rectangle<float> (tx, y - h * 0.5f - 0.7f, tw, h), juce::Justification::centred, false);
        g.setColour (juce::Colours::white.withAlpha (skin::neon() ? 0.20f : 0.55f));
        g.drawText (t, juce::Rectangle<float> (tx, y - h * 0.5f + 0.9f, tw, h), juce::Justification::centred, false);
        g.setColour (skin::engrave());
        g.drawText (t, juce::Rectangle<float> (tx, y - h * 0.5f, tw, h), juce::Justification::centred, false);
    };
    cut (left, x, lw);
    x += lw + gapW;
    const auto mark = juce::Rectangle<float> (x, y - markW * 0.5f, markW, markW);
    if (logo.isValid())
    {
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.drawEllipse (mark.translated (0.0f, -0.7f).reduced (0.4f), 0.9f);
        g.setOpacity (skin::neon() ? 0.85f : 0.62f);
        g.drawImage (logo, mark, juce::RectanglePlacement::centred);
        g.setOpacity (1.0f);
    }
    else
    {
        g.setColour (skin::engrave());
        g.drawEllipse (mark.reduced (1.0f), 1.0f);
    }
    x += markW + gapW;
    cut (right, x, rw);
}

// --------------------------------------------------------------------------------------------
// SC-666: the cathode ray tube. The graticule is 10 x 8 divisions; the trace is drawn with a soft
// outer glow and a bright core, the way phosphor looks, and the beam intensity follows the Z axis.
// --------------------------------------------------------------------------------------------
void ModulePanel::paint666 (juce::Graphics& g)
{
    const auto& sc = spec.sections[0];
    const auto face = juce::Rectangle<float> (sc.x, sc.y, sc.w, sc.h);
    auto raw = [this] (const char* pid) { auto* v = proc.apvts.getRawParameterValue (prefix + pid); return v != nullptr ? v->load() : 0.0f; };
    const bool on = raw ("power") > 0.5f;
    const float intensity = raw ("intensity"), focus = raw ("focus");
    // bezel and glass
    g.setColour (juce::Colour (0xff17181c));
    g.fillEllipse (face.expanded (face.getWidth() * 0.055f));
    g.setColour (juce::Colour (0xff3a3d44));
    g.drawEllipse (face.expanded (face.getWidth() * 0.035f), 2.5f);
    juce::ColourGradient glass (juce::Colour (on ? 0xff0c2a16 : 0xff121416), face.getCentreX(), face.getCentreY(),
                                juce::Colour (on ? 0xff041006 : 0xff0a0b0d), face.getX(), face.getBottom(), true);
    g.setGradientFill (glass);
    g.fillEllipse (face);
    juce::Graphics::ScopedSaveState ss (g);
    juce::Path clip; clip.addEllipse (face);
    g.reduceClipRegion (clip);
    // graticule
    const auto grid = face.reduced (face.getWidth() * 0.085f);
    g.setColour (juce::Colour (0xff2f7a4a).withAlpha (0.32f));
    for (int i = 0; i <= 10; ++i)
    {
        const float x = grid.getX() + grid.getWidth() * (float) i / 10.0f;
        g.drawLine (x, grid.getY(), x, grid.getBottom(), i == 5 ? 1.1f : 0.6f);
    }
    for (int i = 0; i <= 8; ++i)
    {
        const float y = grid.getY() + grid.getHeight() * (float) i / 8.0f;
        g.drawLine (grid.getX(), y, grid.getRight(), y, i == 4 ? 1.1f : 0.6f);
    }
    if (! on || frame666 == nullptr || frame666->n <= 1) return;
    const auto& fr = *frame666;
    const float va = wc::M666::voltsPerDiv (raw ("va")), vb = wc::M666::voltsPerDiv (raw ("vb")), xg = wc::M666::voltsPerDiv (raw ("xgain"));
    auto trace = [&] (bool second)
    {
        juce::Path p;
        bool started = false;
        for (int i = 0; i < fr.n; ++i)
        {
            const float sig = second ? fr.b[i] : fr.a[i];
            const float div = second ? vb : va;
            float x;
            if (fr.xy && ! fr.dual) x = grid.getCentreX() + grid.getWidth() * dsp::clampf (fr.x[i] / (xg * 10.0f), -0.5f, 0.5f);
            else x = grid.getX() + grid.getWidth() * dsp::clampf (fr.x[i], -0.05f, 1.05f);
            const float y = grid.getCentreY() - grid.getHeight() * dsp::clampf (sig / (div * 8.0f), -0.6f, 0.6f);
            if (! started) { p.startNewSubPath (x, y); started = true; }
            else p.lineTo (x, y);
        }
        if (! started) return;
        const juce::Colour core = second ? juce::Colour (0xffb8ff6a) : juce::Colour (0xff6fff9a);
        const float glow = 0.35f + 0.5f * intensity;
        const float w = 1.0f + 2.6f * (1.0f - focus);
        g.setColour (core.withAlpha (0.10f * glow));
        g.strokePath (p, juce::PathStrokeType (w * 4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (core.withAlpha (0.28f * glow));
        g.strokePath (p, juce::PathStrokeType (w * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (core.withAlpha (juce::jlimit (0.3f, 1.0f, 0.55f + 0.45f * intensity)));
        g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    trace (false);
    if (fr.dual) trace (true);
}

// --------------------------------------------------------------------------------------------
// 291e: the alphanumeric display - stage, node and the exact values being edited
// --------------------------------------------------------------------------------------------
void ModulePanel::paint291 (juce::Graphics& g)
{
    const auto& sc = spec.sections[0];
    auto rr = juce::Rectangle<float> (sc.x, sc.y, sc.w, sc.h);
    g.setColour (juce::Colour (0xff6f8c76)); g.fillRoundedRectangle (rr, 3.0f);
    g.setColour (juce::Colour (0xff2b3a2f)); g.drawRoundedRectangle (rr, 3.0f, 1.2f);
    auto raw = [this] (const juce::String& pid) { auto* v = proc.apvts.getRawParameterValue (prefix + pid); return v != nullptr ? v->load() : 0.0f; };
    const int e = juce::jlimit (1, 8, (int) std::lround (raw ("edit")));
    const int nd = juce::jlimit (0, 2, (int) std::lround (raw ("node")));
    const juce::String c = juce::String::charToString ((juce::juce_wchar) ('a' + nd));
    const juce::String sf (e);
    const float hz = wc::M291::freqHz (raw ("f_" + sf + "_" + c));
    const float bw = raw ("bw_" + sf + "_" + c) * 100.0f, am = raw ("amp_" + sf + "_" + c) * 100.0f;
    const float tt = wc::M291::stageTime (raw ("time_" + sf));
    g.setColour (juce::Colour (0xff17231a));
    g.setFont (WcLook::title (rr.getHeight() * 0.33f));
    g.drawText ("ST" + sf + " " + c.toUpperCase() + "   " + (hz < 1000.0f ? juce::String (hz, 0) + "Hz" : juce::String (hz / 1000.0f, 2) + "kHz"),
                rr.reduced (6.0f, 2.0f).removeFromTop (rr.getHeight() * 0.5f), juce::Justification::centredLeft);
    g.setFont (WcLook::label (rr.getHeight() * 0.28f));
    g.drawText ("BW" + juce::String (bw, 0) + "  AM" + juce::String (am, 0) + "  T" + (tt < 1.0f ? juce::String (tt * 1000.0f, 0) + "ms" : juce::String (tt, 2) + "s"),
                rr.reduced (6.0f, 2.0f).removeFromBottom (rr.getHeight() * 0.5f), juce::Justification::centredLeft);
}

void ModulePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const bool silk = ! spec.lines.empty();        // modules drawn from a photo layout
    gInk = skin::ink (spec.ink);
    {
        const auto hi = spec.bg != 0 || silk ? skin::panelHi (spec.bg) : (skin::neon() ? juce::Colour (0xff221a52) : col::panelHi);
        const auto lo = spec.bg != 0 || silk ? skin::panelLo (spec.bg) : (skin::neon() ? juce::Colour (0xff0c0a22) : col::panelLo);
        g.setGradientFill (juce::ColourGradient (hi, 0, 0, lo, 0, r.getHeight(), false));
    }
    g.fillRect (r);
    if (typeOfInst (inst) == MT_252)
    {
        const auto& d = spec.sections[0];
        g.setColour (juce::Colour (0xff1f56c8)); g.fillEllipse (d.x - 6.0f, d.y - 6.0f, d.w + 12.0f, d.h + 12.0f);
        g.setColour (juce::Colour (0xff050507)); g.fillEllipse (d.x, d.y, d.w, d.h);
    }
    if (typeOfInst (inst) == MT_218)
    {
        // the red band with the strip and the preset pads, and the 29 touch plates
        const auto& strip = spec.sections[0];
        g.setColour (juce::Colour (0xffc8302a));
        g.fillRect (juce::Rectangle<float> (6.0f, strip.y - 4.0f, r.getWidth() - 12.0f, strip.h + 8.0f));
        g.setColour (juce::Colour (0xfff4f4f0));
        g.fillRoundedRectangle (strip.x, strip.y, strip.w, strip.h, 3.0f);
        g.setColour (juce::Colour (0xffd8a0a0));
        for (int i = 0; i < 4; ++i)
        {
            const float x0 = strip.x + 10.0f + (float) i * strip.w / 4.0f;
            juce::Path chev; chev.startNewSubPath (x0, strip.y + 3.0f); chev.lineTo (x0 + strip.w / 4.0f - 14.0f, strip.y + 0.5f * strip.h); chev.lineTo (x0, strip.y + strip.h - 3.0f);
            g.strokePath (chev, juce::PathStrokeType (1.6f));
        }
        for (size_t i = 1; i < 5; ++i)
        {
            const auto& pd = spec.sections[i];
            g.setColour (juce::Colour (0xfff4f4f0));
            g.fillRoundedRectangle (pd.x, pd.y, pd.w, pd.h, 2.0f);
        }
        const auto& kb = spec.sections[5];
        const int n = wc::M218::kKeys;
        const float kw = kb.w / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const int pc = i % 12;
            const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            const float x = kb.x + kw * (float) i;
            auto pr = juce::Rectangle<float> (x + (black ? kw * 0.22f : kw * 0.08f), kb.y, black ? kw * 0.56f : kw * 0.84f, kb.h);
            g.setColour (black ? juce::Colour (0xffd23a2a) : juce::Colour (0xfff6f6f2));
            g.fillRoundedRectangle (pr, 3.0f);
            g.setColour (black ? juce::Colour (0xff8e1c16) : juce::Colour (0xffc9cfdc));
            g.drawRoundedRectangle (pr, 3.0f, 1.0f);
            if (! black)
                for (float y = pr.getY() + 8.0f; y < pr.getBottom() - 4.0f; y += 6.0f)
                    g.drawHorizontalLine ((int) y, pr.getX() + 3.0f, pr.getRight() - 3.0f);
        }
    }
    juce::Random rnd (inst * 977 + 3);
    const float brush = skin::neon() ? 0.35f : 1.0f;
    for (int y = 0; y < getHeight(); y += 2)
    {
        g.setColour (juce::Colours::white.withAlpha (brush * (0.03f + 0.04f * rnd.nextFloat())));
        g.drawHorizontalLine (y, 0.0f, r.getWidth());
    }
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRect (r, 1.0f);
    // mounting slots
    for (auto p : { juce::Point<float> (14, 7), juce::Point<float> (r.getWidth() - 40, r.getHeight() - 7), juce::Point<float> (14, r.getHeight() - 7) })
    {
        g.setColour (juce::Colour (0xfff4f4f2));
        g.fillRoundedRectangle (p.x - 7.0f, p.y - 3.0f, 14.0f, 6.0f, 2.0f);
        g.setColour (juce::Colour (0xff9a9ca0));
        g.drawRoundedRectangle (p.x - 7.0f, p.y - 3.0f, 14.0f, 6.0f, 2.0f, 0.7f);
    }

    if (! silk)
    {
        // our own modules (MIDI, Output): title, footer and rounded section frames
        drawItalic (g, spec.name, { 20.0f, 6.0f, r.getWidth() - 50.0f, 22.0f }, juce::Justification::centred, WcLook::title (15.0f), gInk);
        for (auto& sct : spec.sections)
        {
            auto sr = juce::Rectangle<float> (sct.x, sct.y, sct.w, sct.h);
            g.setColour (gInk);
            g.drawRoundedRectangle (sr, 5.0f, 1.4f);
            if (juce::String (sct.title).isNotEmpty())
            {
                const auto f = WcLook::label (10.0f);
                const float tw = juce::GlyphArrangement::getStringWidth (f, sct.title) + 12.0f;
                auto tr = juce::Rectangle<float> (tw, 13.0f).withCentre ({ sr.getCentreX(), sr.getY() });
                g.setGradientFill (juce::ColourGradient (skin::neon() ? juce::Colour (0xff221a52) : col::panelHi, 0, 0, skin::neon() ? juce::Colour (0xff0c0a22) : col::panelLo, 0, r.getHeight(), false));
                g.fillRect (tr);
                drawItalic (g, sct.title, tr, juce::Justification::centred, f, gInk);
            }
        }
    }

    // silkscreen: rules, arrows, texts
    for (auto& l : spec.lines) drawArrowLine (g, l);
    for (auto& t : spec.texts)
    {
        juce::String txt (t.text);
        if (t.style == 4 && isRemovable (inst) && (typeOfInst (inst) == MT_281 || typeOfInst (inst) == MT_258 || typeOfInst (inst) == MT_259 || typeOfInst (inst) == MT_292)) txt << "  " << juce::String::charToString ((juce::juce_wchar) ('A' + subIndex (inst)));
        const auto f = t.style == 0 ? WcLook::label (t.size) : WcLook::title (t.size);
        const float w = juce::GlyphArrangement::getStringWidth (f, txt) + 10.0f;
        juce::Rectangle<float> box;
        juce::Justification j = juce::Justification::centred;
        if (t.just == 1) { box = { t.x, t.y - t.size, w, t.size * 2.0f }; j = juce::Justification::centredLeft; }
        else if (t.just == 2) { box = { t.x - w, t.y - t.size, w, t.size * 2.0f }; j = juce::Justification::centredRight; }
        else box = { t.x - w * 0.5f, t.y - t.size, w, t.size * 2.0f };
        if (t.style == 3)
        {
            g.setColour (gInk);
            g.drawEllipse (t.x - 8.0f, t.y - 8.0f, 16.0f, 16.0f, 1.2f);
        }
        if (t.style == 4)   // title strip: a light band with the panel's lettering on it
        {
            g.setColour (skin::neon() ? juce::Colour (0xff2b1f66) : juce::Colour (0xfff2f3f5));
            g.fillRect (juce::Rectangle<float> (4.0f, t.y - 8.0f, r.getWidth() - 8.0f, 16.0f));
        }
        drawItalic (g, txt, box, j, f, gInk);
    }

    // printed scales and knob captions
    for (auto& c : spec.controls)
    {
        if (c.style == CtrlStyle::BigKnob || c.style == CtrlStyle::Knob || c.style == CtrlStyle::SmallKnob)
        {
            drawScale (g, c);
            if (juce::String (c.label).isNotEmpty())
            {
                const float half = c.style == CtrlStyle::BigKnob ? 37.0f : (c.style == CtrlStyle::Knob ? 28.0f : 20.0f);
                const float extra = c.scale != nullptr && c.style != CtrlStyle::SmallKnob ? 6.0f : 0.0f;
                drawItalic (g, c.label, { c.x - 46.0f, c.y + half + extra - 2.0f, 92.0f, 13.0f }, juce::Justification::centred,
                            WcLook::label (c.style == CtrlStyle::SmallKnob ? 9.0f : 10.5f), gInk);
            }
        }
    }
    for (auto& j : spec.jacks)
        if (juce::String (j.label).isNotEmpty())
            drawItalic (g, j.label, { j.x - 30.0f, j.y + 11.0f, 60.0f, 12.0f }, juce::Justification::centred, WcLook::label (9.0f), gInk);

    drawEngravedFooter (g, spec, r.getWidth());
}

void ModulePanel::paintOverChildren (juce::Graphics& g)
{
    auto led = [&] (float x, float y, juce::Colour c, float a)
    {
        g.setColour (juce::Colour (0xff4a2222));
        g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f, 0.7f);
        if (a > 0.02f)
        {
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
            g.fillEllipse (x - 3.6f, y - 3.6f, 7.2f, 7.2f);
            g.setColour (c.withAlpha (0.25f * juce::jlimit (0.0f, 1.0f, a)));
            g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);
        }
    };
    for (size_t i = 0; i < spec.leds.size(); ++i)
        led (spec.leds[i].x, spec.leds[i].y, juce::Colour (spec.leds[i].colour), i < ledCache.size() ? ledCache[i] : 0.0f);
    if (typeOfInst (inst) == MT_252) paint252 (g);
    if (typeOfInst (inst) == MT_666 && ! spec.sections.empty()) paint666 (g);
    if (typeOfInst (inst) == MT_291 && ! spec.sections.empty()) paint291 (g);
    if (typeOfInst (inst) == MT_250 && spec.sections.size() >= 2)
    {
        auto raw = [this] (const juce::String& pid) { auto* v = proc.apvts.getRawParameterValue (prefix + pid); return v != nullptr ? v->load() : 0.0f; };
        auto ival = [&] (const juce::String& pid) { return (int) std::lround (raw (pid)); };
        const int le = ival ("loopend"), lt = ival ("loopto"), lc = ival ("loops"), step = ival ("setstep");
        const bool editing = raw ("editmode") > 0.5f;
        const int st = juce::jlimit (1, 16, ival ("edit"));
        const juce::String sf ("_" + juce::String (st));
        for (int k = 0; k < 2; ++k)
        {
            const auto& sc = spec.sections[(size_t) k];
            auto rr = juce::Rectangle<float> (sc.x, sc.y, sc.w, sc.h);
            g.setColour (juce::Colour (editing ? 0xffc9e4a8 : 0xff9fc4e8)); g.fillRoundedRectangle (rr, 2.0f);
            g.setColour (juce::Colour (0xff3a5a80)); g.drawRoundedRectangle (rr, 2.0f, 1.0f);
            juce::String txt;
            if (editing)
            {
                if (k == 0) txt = "S" + juce::String (st);
                else
                {
                    const int rng = juce::jlimit (0, 3, ival ("trng" + sf));
                    if (rng == 3)
                    {
                        static const char* notes[7] = { "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/4" };
                        txt = notes[juce::jlimit (0, 6, (int) std::lround (raw ("t" + sf) * 6.0f))];
                    }
                    else
                    {
                        float lo, hi;
                        wc::M250::timeRange (rng, lo, hi);
                        float secs = lo * std::pow (hi / lo, raw ("t" + sf));
                        if (raw ("tq" + sf) > 0.5f) secs = 0.01f * std::exp2 ((float) juce::jlimit (0, 9, (int) std::lround (std::log2 (secs / 0.01f))));
                        txt = secs < 1.0f ? juce::String (secs * 1000.0f, 0) + "m" : juce::String (secs, 2) + "s";
                    }
                }
            }
            else
            {
                txt = k == 0 ? (le > 0 ? juce::String (le) + ">" + juce::String (lt) : juce::String ("--"))
                             : (le > 0 ? (lc == 0 ? juce::String ("oo") : juce::String (lc) + "x") : juce::String ("--"));
                if (step == 1 && k == 0) txt = "to?";
                if (step == 2 && k == 1) txt = "n?";
            }
            g.setColour (juce::Colour (0xff0d1f3a));
            g.setFont (WcLook::title (txt.length() > 4 ? 9.5f : 12.0f));
            g.drawText (txt, rr, juce::Justification::centred);
        }
    }
    const auto type = typeOfInst (inst);
    if (type == MT_Midi)
        led (150.0f, 56.0f, col::ledRed, ledOn ? 1.0f : 0.0f);
    else if (type == MT_Output)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const float v = ch == 0 ? ledA : ledB;
            auto bar = juce::Rectangle<float> (134.0f + 6.0f * (float) ch, 56.0f, 4.0f, 200.0f);
            g.setColour (juce::Colour (0xff1b1c20));
            g.fillRect (bar);
            const float db = juce::Decibels::gainToDecibels (v, -60.0f);
            const float h = bar.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
            g.setColour (db > -1.0f ? col::ledRed : (db > -9.0f ? juce::Colour (0xffe7c234) : col::ledGreen));
            g.fillRect (bar.withTop (bar.getBottom() - h));
        }
    }
}

void ModulePanel::updateLeds()
{
    if (typeOfInst (inst) == MT_252)
    {
        const int c = cursor252();
        if (c != cur252) { cur252 = c; sync252Knobs(); }
        const bool eu = proc.apvts.getRawParameterValue (prefix + "euclid")->load() > 0.5f;
        const int pt = (int) std::lround (proc.apvts.getRawParameterValue (prefix + "pattern")->load());
        if (eu && (pt != patt252 || ! euclid252))
        {
            auto* m = proc.rack.m252();
            const int ring = (int) std::lround (proc.apvts.getRawParameterValue (prefix + "ring")->load());
            const int n = wc::M252::kSize[ring], k = juce::jlimit (0, n, pt);
            for (int i = 0; i < n; ++i) m->rgb[wc::M252::base (ring) + i].store (((i * k) % n) < k ? 1 : 0);
        }
        euclid252 = eu; patt252 = pt;
        for (auto& sc : spec.sections) repaint (juce::Rectangle<float> (sc.x, sc.y, sc.w, sc.h).expanded (3.0f).toNearestInt());
    }
    rebindDynamic();
    if (typeOfInst (inst) == MT_250)
        for (auto& sc : spec.sections) repaint (juce::Rectangle<float> (sc.x, sc.y, sc.w, sc.h).expanded (2.0f).toNearestInt());
    if (typeOfInst (inst) == MT_291 && ! spec.sections.empty())
    {
        const auto& sc = spec.sections[0];
        repaint (juce::Rectangle<float> (sc.x, sc.y, sc.w, sc.h).expanded (2.0f).toNearestInt());
    }
    if (typeOfInst (inst) == MT_666 && ! spec.sections.empty())
    {
        auto* m = proc.rack.m666 (subIndex (inst));
        if (frame666 == nullptr) frame666 = std::make_unique<wc::M666::Frame>();
        if (m->latest (*frame666, seq666))
        {
            const auto& sc = spec.sections[0];
            repaint (juce::Rectangle<float> (sc.x, sc.y, sc.w, sc.h).expanded (4.0f).toNearestInt());
        }
    }
    const auto type = typeOfInst (inst);
    if (! spec.leds.empty())
    {
        auto* m = proc.rack.module (inst);
        ledCache.resize (spec.leds.size(), 0.0f);
        for (size_t i = 0; i < spec.leds.size(); ++i)
        {
            const float v = m->led (spec.leds[i].id);
            if (std::fabs (v - ledCache[i]) > 0.02f)
            {
                ledCache[i] = v;
                repaint (juce::Rectangle<int> ((int) spec.leds[i].x - 9, (int) spec.leds[i].y - 9, 18, 18));
            }
        }
    }
    if (type == MT_Midi)
    {
        const bool on = proc.rack.midi().gateLed.load (std::memory_order_relaxed);
        if (on != ledOn) { ledOn = on; repaint (136, 42, 30, 30); }
    }
    else if (type == MT_Output)
    {
        ledA = proc.rack.output().peakL.load (std::memory_order_relaxed);
        ledB = proc.rack.output().peakR.load (std::memory_order_relaxed);
        repaint (130, 52, 16, 210);
    }
}

void ModulePanel::toggleMatrixCell (int row, int col)
{
    if (row < 0 || row > 3 || col < 0 || col > 11) return;
    if (row == 0)
    {
        if (auto* p = proc.apvts.getParameter (prefix + "len")) p->setValueNotifyingHost (p->convertTo0to1 ((float) (col + 1)));
    }
    else if (auto* p = proc.apvts.getParameter (prefix + juce::String::charToString ((juce::juce_wchar) ('a' + row - 1)) + juce::String (col + 1)))
        p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
    // the cursor follows the click, as if the encoders had been turned
    if (auto* p = proc.apvts.getParameter (prefix + "row")) p->setValueNotifyingHost (p->convertTo0to1 ((float) (row + 1)));
    if (auto* p = proc.apvts.getParameter (prefix + "col")) p->setValueNotifyingHost (p->convertTo0to1 ((float) (col + 1)));
}

void ModulePanel::touch218 (juce::Point<float> p, bool down)
{
    auto* m = proc.rack.m218();
    if (! down) { m->touchKey.store (-1); m->stripTouch.store (false); m->padTouch.store (-1); return; }
    const auto& st = spec.sections[0];
    if (juce::Rectangle<float> (st.x, st.y, st.w, st.h).expanded (2.0f).contains (p))
    {
        m->stripPos.store (juce::jlimit (0.0f, 1.0f, (p.x - st.x) / st.w));
        m->stripTouch.store (true);
        return;
    }
    for (int i = 0; i < 4; ++i)
    {
        const auto& pd = spec.sections[(size_t) (1 + i)];
        if (juce::Rectangle<float> (pd.x, pd.y, pd.w, pd.h).expanded (2.0f).contains (p))
        {
            m->padTouch.store (i);
            if (auto* sp = proc.apvts.getParameter (prefix + "sel")) sp->setValueNotifyingHost (sp->convertTo0to1 ((float) i));
            return;
        }
    }
    const auto& kb = spec.sections[5];
    if (juce::Rectangle<float> (kb.x, kb.y, kb.w, kb.h).contains (p))
    {
        const int k = juce::jlimit (0, wc::M218::kKeys - 1, (int) ((p.x - kb.x) / kb.w * (float) wc::M218::kKeys));
        m->touchPressure.store (juce::jlimit (0.2f, 1.0f, 0.2f + 0.8f * (p.y - kb.y) / kb.h));   // further down the plate = more pressure
        m->touchKey.store (k);
    }
    else m->touchKey.store (-1);
}

void ModulePanel::mouseDrag (const juce::MouseEvent& e) { if (typeOfInst (inst) == MT_218 && ! e.mods.isPopupMenu()) touch218 (e.position, true); }
void ModulePanel::mouseUp (const juce::MouseEvent& e) { if (typeOfInst (inst) == MT_218) touch218 (e.position, false); }

void ModulePanel::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() && typeOfInst (inst) == MT_218) { touch218 (e.position, true); return; }
    if (! e.mods.isPopupMenu() && typeOfInst (inst) == MT_252)
    {
        const auto& d = spec.sections[0];
        const auto ctr = juce::Point<float> (d.x + d.w * 0.5f, d.y + d.h * 0.5f);
        const float rOut = d.w * 0.5f * 0.94f, rIn = rOut * 0.16f, dist = e.position.getDistanceFrom (ctr);
        if (dist > rIn && dist < rOut)
        {
            const int ring = juce::jlimit (0, 10, (int) ((dist - rIn) / (rOut - rIn) * 11.0f));
            float a = std::atan2 (e.position.x - ctr.x, ctr.y - e.position.y);
            if (a < 0.0f) a += juce::MathConstants<float>::twoPi;
            const int n = wc::M252::kSize[ring];
            const int cell = juce::jlimit (0, n - 1, (int) (a / juce::MathConstants<float>::twoPi * (float) n));
            auto put = [this] (const char* pid, float v) { if (auto* p = proc.apvts.getParameter (prefix + pid)) p->setValueNotifyingHost (p->convertTo0to1 (v)); };
            put ("ring", (float) ring); put ("cell", (float) cell);
            repaint();
        }
        return;
    }
    if (! e.mods.isPopupMenu() && typeOfInst (inst) == MT_666 && ! spec.sections.empty())
    {
        const auto& sc = spec.sections[0];
        if (juce::Rectangle<float> (sc.x, sc.y, sc.w, sc.h).contains (e.position))
        {
            proc.rack.m666 (subIndex (inst))->rearm.store (true);
            return;
        }
    }
    if (! e.mods.isPopupMenu() && typeOfInst (inst) == MT_242)
    {
        for (auto& d : spec.leds)
            if (d.id >= 100 && e.position.getDistanceFrom ({ d.x, d.y }) < 10.0f)
            {
                toggleMatrixCell ((d.id - 100) / 12, (d.id - 100) % 12);
                return;
            }
        return;
    }
    if (! e.mods.isPopupMenu() || ! isRemovable (inst)) return;
    juce::PopupMenu m;
    m.addItem (1, "Remove " + instName (inst));
    m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r) { if (r == 1) proc.setPresent (inst, false); });
}

// =================================================================================================
// CableLayer
// =================================================================================================
static void drawCable (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour c, bool banana, float alpha, float floorY)
{
    const float dist = a.getDistanceFrom (b);
    // gravity sag, limited so the cord never hangs below the rack floor (bezier low point ~ max y + 0.75 sag)
    const float sag = juce::jmax (4.0f, juce::jmin (18.0f + dist * 0.22f, (floorY - juce::jmax (a.y, b.y)) / 0.75f));
    juce::Path p;
    p.startNewSubPath (a);
    p.cubicTo (a.translated (0.0f, sag), b.translated (0.0f, sag), b);
    const float w = banana ? 3.4f : 4.4f;
    g.setColour (juce::Colours::black.withAlpha (0.28f * alpha));
    g.strokePath (p, juce::PathStrokeType (w + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (2.0f, 4.0f));
    g.setColour (c.withAlpha (alpha));
    g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c.brighter (0.6f).withAlpha (0.35f * alpha));
    g.strokePath (p, juce::PathStrokeType (w * 0.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (-0.6f, -0.8f));
    // plugs
    for (auto pt : { a, b })
    {
        if (banana)
        {
            g.setColour (c.darker (0.2f).withAlpha (alpha));
            g.fillEllipse (pt.x - 6.5f, pt.y - 6.5f, 13.0f, 13.0f);
            g.setColour (c.brighter (0.5f).withAlpha (0.6f * alpha));
            g.drawEllipse (pt.x - 6.5f, pt.y - 6.5f, 13.0f, 13.0f, 1.0f);
        }
        else
        {
            g.setColour (juce::Colour (0xff17181b).withAlpha (alpha));
            g.fillEllipse (pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f);
            g.setColour (juce::Colour (0xff5a5c62).withAlpha (alpha));
            g.drawEllipse (pt.x - 7.0f, pt.y - 7.0f, 14.0f, 14.0f, 1.2f);
        }
    }
}

void CableLayer::paint (juce::Graphics& g)
{
    for (auto& c : rack.proc.getCables())
    {
        auto* a = rack.findJack (c.srcInst, true, c.srcJack);
        auto* b = rack.findJack (c.dstInst, false, c.dstJack);
        if (a == nullptr || b == nullptr) continue;
        drawCable (g, rack.jackCentre (*a), rack.jackCentre (*b), cableColour (c.colour, a->kind == JackKind::Banana), a->kind == JackKind::Banana, rack.cableAlpha, (float) getHeight() - 4.0f);
    }
    if (rack.dragFrom != nullptr)
        drawCable (g, rack.jackCentre (*rack.dragFrom), rack.dragPos, cableColour (rack.dragColour, rack.dragFrom->kind == JackKind::Banana),
                   rack.dragFrom->kind == JackKind::Banana, 1.0f, (float) getHeight() - 4.0f);
}

// =================================================================================================
// RackView
// =================================================================================================
RackView::RackView (WcProcessor& p) : proc (p)
{
    addAndMakeVisible (content);
    content.addAndMakeVisible (cables);
    proc.addChangeListener (this);
    rebuild();
    startTimerHz (30);
}

RackView::~RackView() { proc.removeChangeListener (this); }

void RackView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // rebuild when the set of modules changed, otherwise just redraw the cables
    std::vector<int> want;
    for (int i = 0; i < kNumInst; ++i) if (proc.isPresent (i)) want.push_back (i);
    std::vector<int> have;
    for (auto& pnl : panels) have.push_back (pnl->inst);
    if (want != have) rebuild();
    cables.repaint();
    for (auto& pnl : panels) for (auto& j : pnl->jacks) juce::ignoreUnused (j);
}

void RackView::rebuild()
{
    panels.clear();
    // order on the rails: MIDI, the 259s, OUTPUT; rows wrap at ~1330 units
    std::vector<int> order;
    order.push_back (kInstMidi);
    for (int i = 1; i < kInstOutput; ++i) if (proc.isPresent (i)) order.push_back (i);   // 281s, 259s, 292s
    order.push_back (kInstOutput);
    const int gap = 6, maxW = 1336, rowH = 430;
    int x = gap, y = gap, rowW = 0;
    for (int inst : order)
    {
        auto p = std::make_unique<ModulePanel> (*this, proc, inst);
        if (x + p->getWidth() > maxW && x > gap) { x = gap; y += rowH + 18; }
        p->setTopLeftPosition (x, y);
        x += p->getWidth() + gap;
        rowW = juce::jmax (rowW, x);
        content.addAndMakeVisible (*p);
        panels.push_back (std::move (p));
    }
    contentSize = { juce::jmax (rowW, 800), y + rowH + 64 };   // room under the last row for the cords to hang
    content.setSize (contentSize.x, contentSize.y);
    cables.setBounds (content.getLocalBounds());
    cables.toFront (false);
    setZoom (zoom);
    if (auto* parent = getParentComponent()) parent->resized();
}

void RackView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (onZoomGesture != nullptr && (e.mods.isCtrlDown() || e.mods.isCommandDown()) && w.deltaY != 0.0f)
    {
        onZoomGesture (w.isReversed ? -w.deltaY : w.deltaY, e.position);
        return;                                   // zoomed: the viewport must not scroll as well
    }
    Component::mouseWheelMove (e, w);             // otherwise it travels up to the viewport
}

void RackView::setZoom (float z)
{
    zoom = z;
    content.setTransform (juce::AffineTransform::scale (zoom));
    setSize (juce::roundToInt ((float) contentSize.x * zoom), juce::roundToInt ((float) contentSize.y * zoom));
}

juce::Point<int> RackView::preferredSize() const
{
    return { juce::roundToInt ((float) contentSize.x * zoom), juce::roundToInt ((float) contentSize.y * zoom) };
}

void RackView::resized() {}

void RackView::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    // rack rails behind each row
    g.setColour (col::rail);
    for (int y = 0; y < (int) ((float) contentSize.y * zoom); y += (int) (448.0f * zoom))
    {
        g.fillRect (0, y, getWidth(), (int) (6.0f * zoom));
        g.fillRect (0, y + (int) (436.0f * zoom), getWidth(), (int) (8.0f * zoom));
    }
}

void RackView::timerCallback()
{
    for (auto& p : panels) p->updateLeds();
}

JackComp* RackView::findJack (int inst, bool output, int index) const
{
    for (auto& p : panels)
        if (p->inst == inst)
            for (auto& j : p->jacks)
                if (j->output == output && j->index == index) return j.get();
    return nullptr;
}

juce::Point<float> RackView::jackCentre (const JackComp& j) const
{
    return content.getLocalPoint (&j, j.getLocalBounds().toFloat().getCentre());
}

int RackView::cableCountAt (const JackComp& j) const
{
    int n = 0;
    for (auto& c : proc.getCables())
        if (j.output ? (c.srcInst == j.inst && c.srcJack == j.index) : (c.dstInst == j.inst && c.dstJack == j.index)) ++n;
    return n;
}

bool RackView::canDropOn (const JackComp& j) const
{
    if (dragFrom == nullptr || &j == dragFrom) return false;
    return j.output != dragFrom->output && j.kind == dragFrom->kind;
}

void RackView::beginDrag (JackComp& j, const juce::MouseEvent& e)
{
    dragPos = content.getLocalPoint (nullptr, e.getScreenPosition()).toFloat();
    if (! j.output)
    {
        // picking up a plugged input: the cable stays attached to its source and follows the mouse
        for (auto& c : proc.getCables())
            if (c.dstInst == j.inst && c.dstJack == j.index)
            {
                dragFrom = findJack (c.srcInst, true, c.srcJack);
                dragColour = c.colour;
                proc.disconnectInput (j.inst, j.index);
                break;
            }
    }
    if (dragFrom == nullptr)
    {
        dragFrom = &j;
        dragColour = proc.nextColour (j.kind == JackKind::Banana);
    }
    for (auto& p : panels) for (auto& jc : p->jacks) jc->repaint();
    cables.repaint();
}

void RackView::dragMove (const juce::MouseEvent& e)
{
    if (dragFrom == nullptr) return;
    dragPos = content.getLocalPoint (nullptr, e.getScreenPosition()).toFloat();
    cables.repaint();
}

void RackView::endDrag (const juce::MouseEvent& e)
{
    if (dragFrom == nullptr) return;
    auto* from = dragFrom;
    dragFrom = nullptr;
    // which jack is under the mouse?
    JackComp* target = nullptr;
    const auto screen = e.getScreenPosition();
    for (auto& p : panels)
        for (auto& j : p->jacks)
            if (j->getScreenBounds().expanded (4).contains (screen)) target = j.get();
    if (target != nullptr && target != from && target->output != from->output && target->kind == from->kind)
    {
        auto* out = from->output ? from : target;
        auto* in = from->output ? target : from;
        proc.connect (out->inst, out->index, in->inst, in->index, dragColour);
    }
    for (auto& p : panels) for (auto& jc : p->jacks) jc->repaint();
    cables.repaint();
}

void RackView::jackMenu (JackComp& j)
{
    juce::PopupMenu m;
    const int n = cableCountAt (j);
    m.addItem (1, n > 1 ? "Disconnect " + juce::String (n) + " cables" : "Disconnect", n > 0);
    m.showMenuAsync (juce::PopupMenu::Options(), [this, &j] (int r)
    {
        if (r != 1) return;
        if (j.output) proc.disconnectOutput (j.inst, j.index); else proc.disconnectInput (j.inst, j.index);
    });
}
} // namespace wc::ui
