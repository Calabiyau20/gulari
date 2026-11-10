#include "RackView.h"
#include "../modules/M259.h"
#include "../modules/MidiInterface.h"
#include "../modules/OutputModule.h"

namespace wc::ui
{
static juce::String jackTip (int inst, const JackSpec& j)
{
    juce::String t = j.label;
    t << (j.output ? "  output" : "  input") << (j.kind == JackKind::Banana ? "  (banana, control)" : "  (mini, audio)");
    const juce::String l (j.label);
    if (! j.output && l == "PITCH") t << " - 1.2 V/oct";
    if (l == "LOCK") t << " - locks the principal to this signal (or its nearest subharmonic)";
    if (l == "WAVE") t << " - program: 0-3.3 V saw, 3.3-6.7 V square, above triangle (Remote / Both)";
    if (! j.output && (l == "AMP" || (l == "PITCH" && inst > 0 && j.x > 180 && j.x < 340) || l == "TIMBRE") && j.x > 180 && j.x < 340)
        t << " - program gate: above 3 V switches this modulation on (Remote / Both)";
    if (l == "CV OUT") t << " - modulation oscillator as 0..10 V control";
    if (l == "LEVEL CV") t << " - 0..10 V opens the output (through the LAG)";
    t << "\nDrag to patch. Right-click to disconnect.";
    return t;
}

// =================================================================================================
// JackComp
// =================================================================================================
JackComp::JackComp (RackView& rv, int i, const JackSpec& s, int dsp)
    : inst (i), index (dsp), output (s.output), kind (s.kind), rack (rv)
{
    setTooltip (jackTip (i, s));
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
        // coloured insulator: outputs orange, inputs black
        const juce::Colour ins = output ? juce::Colour (0xffe0782a) : juce::Colour (0xff1e1f23);
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillEllipse (c.x - 9.0f, c.y - 8.0f, 19.0f, 19.0f);
        g.setColour (ins);
        g.fillEllipse (c.x - 9.5f, c.y - 9.5f, 19.0f, 19.0f);
        g.setColour (ins.brighter (0.4f));
        g.drawEllipse (c.x - 9.5f, c.y - 9.5f, 19.0f, 19.0f, 1.0f);
        // brass socket
        g.setColour (juce::Colour (0xffc9a75a));
        g.fillEllipse (c.x - 4.2f, c.y - 4.2f, 8.4f, 8.4f);
        g.setColour (juce::Colour (0xff3a2d12));
        g.fillEllipse (c.x - 2.4f, c.y - 2.4f, 4.8f, 4.8f);
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
        if (output)
        {
            g.setColour (col::ink);
            g.drawEllipse (c.x - 7.5f, c.y - 7.5f, 15.0f, 15.0f, 1.4f);
        }
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
ParamSwitch::ParamSwitch (juce::RangedAudioParameter& p, int n)
    : param (p), positions (n), attach (p, [this] (float v) { value = juce::roundToInt (v); repaint(); })
{
    attach.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip (p.getName (64) + " - click to change");
}

void ParamSwitch::paint (juce::Graphics& g)
{
    // a chrome bat-handle toggle in a black bushing; positions left .. right
    auto r = getLocalBounds().toFloat();
    const auto c = r.getCentre();
    const float w = positions == 3 ? 46.0f : 30.0f;
    auto slot = juce::Rectangle<float> (w, 14.0f).withCentre (c);
    g.setColour (juce::Colour (0xff1b1c20));
    g.fillRoundedRectangle (slot, 7.0f);
    g.setColour (juce::Colour (0xff4b4e55));
    g.drawRoundedRectangle (slot, 7.0f, 1.0f);
    const float t = positions == 3 ? (float) value / 2.0f : (float) value;
    const float x = slot.getX() + 7.0f + t * (slot.getWidth() - 14.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (x - 6.0f, c.y - 5.0f, 13.0f, 13.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, x - 4.0f, c.y - 5.0f, juce::Colour (0xff8c8f96), x + 4.0f, c.y + 5.0f, false));
    g.fillEllipse (x - 6.0f, c.y - 6.0f, 12.0f, 12.0f);
    // position LEDs for on / off toggles
    if (positions == 2)
    {
        g.setColour (value == 1 ? col::ledRed : juce::Colour (0xff3a1a18));
        g.fillEllipse (c.x - 3.0f, slot.getY() - 9.0f, 6.0f, 6.0f);
    }
}

void ParamSwitch::mouseDown (const juce::MouseEvent& e)
{
    int v;
    if (positions == 3)
    {
        const float rel = (float) e.x / (float) getWidth();
        v = rel < 0.38f ? 0 : (rel > 0.62f ? 2 : 1);
    }
    else v = 1 - value;
    attach.setValueAsCompleteGesture ((float) v);
}

// =================================================================================================
// ModulePanel
// =================================================================================================
ModulePanel::ModulePanel (RackView& rv, WcProcessor& p, int i)
    : inst (i), rack (rv), proc (p), spec (specFor (typeOfInst (i)))
{
    prefix = typeOfInst (i) == MT_259 ? juce::String (prefix259 (i - kInst259)) : juce::String();
    setSize ((int) spec.width, (int) spec.height);
    for (auto& c : spec.controls)
    {
        auto* param = proc.apvts.getParameter (prefix + c.suffix);
        jassert (param != nullptr);
        if (param == nullptr) continue;
        if (c.style == CtrlStyle::Toggle || c.style == CtrlStyle::Switch3)
        {
            auto sw = std::make_unique<ParamSwitch> (*param, c.style == CtrlStyle::Switch3 ? 3 : 2);
            const int w = c.style == CtrlStyle::Switch3 ? 52 : 36;
            sw->setBounds ((int) c.x - w / 2, (int) c.y - 12, w, 28);
            addAndMakeVisible (*sw);
            switches.push_back (std::move (sw));
            continue;
        }
        auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        const int size = c.style == CtrlStyle::BigKnob ? 74 : (c.style == CtrlStyle::Knob ? 54 : 38);
        s->getProperties().set ("style", c.style == CtrlStyle::BigKnob ? 0 : (c.style == CtrlStyle::Knob ? 1 : 2));
        s->getProperties().set ("cap", (juce::int64) c.colour);
        s->setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        s->setBounds ((int) c.x - size / 2, (int) c.y - size / 2, size, size);
        s->setPopupDisplayEnabled (true, true, nullptr);
        s->setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        s->setTooltip (param->getName (64));
        addAndMakeVisible (*s);
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, prefix + c.suffix, *s));
        sliders.push_back (std::move (s));
    }
    int nIn = 0, nOut = 0;
    for (auto& j : spec.jacks)
    {
        auto jc = std::make_unique<JackComp> (rack, inst, j, j.output ? nOut++ : nIn++);
        jc->setBounds ((int) j.x - 13, (int) j.y - 13, 26, 26);
        addAndMakeVisible (*jc);
        jacks.push_back (std::move (jc));
    }
    if (typeOfInst (i) == MT_259)
    {
        removeButton.setTooltip ("Remove this module from the rack");
        removeButton.setBounds ((int) spec.width - 26, 7, 18, 18);
        removeButton.onClick = [this] { proc.setPresent (inst, false); };
        addAndMakeVisible (removeButton);
    }
}

void ModulePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // brushed aluminium
    g.setGradientFill (juce::ColourGradient (col::panelHi, 0, 0, col::panelLo, 0, r.getHeight(), false));
    g.fillRect (r);
    juce::Random rnd (inst * 977 + 3);
    for (int y = 0; y < getHeight(); y += 2)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f + 0.06f * rnd.nextFloat()));
        g.drawHorizontalLine (y, 0.0f, r.getWidth());
    }
    // rack screws
    for (auto p : { juce::Point<float> (10, 10), juce::Point<float> (r.getWidth() - 10, 10), juce::Point<float> (10, r.getHeight() - 10), juce::Point<float> (r.getWidth() - 10, r.getHeight() - 10) })
    {
        if (typeOfInst (inst) == MT_259 && p.x > r.getWidth() - 20 && p.y < 20) continue;
        g.setColour (juce::Colour (0xff8e9096));
        g.fillEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f);
        g.setColour (juce::Colour (0xff55585e));
        g.drawLine (p.x - 3.0f, p.y, p.x + 3.0f, p.y, 1.2f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRect (r, 1.0f);

    // title strip
    g.setColour (col::ink);
    g.setFont (WcLook::title (15.0f));
    juce::String t = spec.name;
    if (typeOfInst (inst) == MT_259) t << "   " << juce::String::charToString ((juce::juce_wchar) ('A' + inst - kInst259));
    g.drawText (t, 24, 6, (int) r.getWidth() - 48, 20, juce::Justification::centred);
    g.setFont (WcLook::label (10.0f));
    g.setColour (col::inkSoft);
    g.drawText (juce::String ("MODEL ") + spec.model, 20, 22, 160, 14, juce::Justification::left);
    if (typeOfInst (inst) == MT_259)
        g.drawText ("after Buchla 259", (int) r.getWidth() - 190, 22, 160, 14, juce::Justification::right);

    // sections: thin ruled frames with a title
    for (auto& s : spec.sections)
    {
        auto sr = juce::Rectangle<float> (s.x, s.y, s.w, s.h);
        g.setColour (col::ink.withAlpha (0.55f));
        g.drawRoundedRectangle (sr, 3.0f, 1.0f);
        g.setFont (WcLook::label (9.5f));
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), s.title) + 10.0f;
        auto tr = juce::Rectangle<float> (tw, 12.0f).withCentre ({ sr.getCentreX(), sr.getY() });
        g.setGradientFill (juce::ColourGradient (col::panelHi, 0, 0, col::panelLo, 0, r.getHeight(), false));
        g.fillRect (tr);
        g.setColour (col::ink);
        g.drawText (s.title, tr, juce::Justification::centred);
    }

    // control labels
    g.setColour (col::ink);
    for (auto& c : spec.controls)
    {
        const float half = c.style == CtrlStyle::BigKnob ? 37.0f : (c.style == CtrlStyle::Knob ? 27.0f : (c.style == CtrlStyle::SmallKnob ? 19.0f : 12.0f));
        g.setFont (WcLook::label (c.style == CtrlStyle::SmallKnob ? 8.5f : 9.5f));
        const float y = c.style == CtrlStyle::Toggle || c.style == CtrlStyle::Switch3 ? c.y + 15.0f : c.y + half - 1.0f;
        g.drawText (c.label, juce::Rectangle<float> (c.x - 45.0f, y, 90.0f, 12.0f), juce::Justification::centred);
    }
    // jack labels (above the jack)
    for (auto& j : spec.jacks)
    {
        g.setFont (WcLook::label (8.0f));
        g.setColour (j.output ? juce::Colour (0xffa2461a) : col::ink);
        g.drawText (j.label, juce::Rectangle<float> (j.x - 20.0f, j.y - 25.0f, 40.0f, 11.0f), juce::Justification::centred);
    }
    // 259: "INPUTS" / "OUTPUTS" hints and the program row label
    if (typeOfInst (inst) == MT_259)
    {
        g.setFont (WcLook::label (8.0f));
        g.setColour (col::inkSoft);
        g.drawText ("PROGRAM INPUTS", juce::Rectangle<float> (182.0f, 300.0f, 150.0f, 10.0f), juce::Justification::centred);
        g.drawLine (192.0f, 313.0f, 322.0f, 313.0f, 0.7f);
    }
}

void ModulePanel::paintOverChildren (juce::Graphics& g)
{
    // LEDs
    auto led = [&] (float x, float y, juce::Colour c, float a)
    {
        g.setColour (juce::Colour (0xff201010));
        g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
        if (a > 0.02f)
        {
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
            g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
            g.setColour (c.withAlpha (0.25f * a));
            g.fillEllipse (x - 8.0f, y - 8.0f, 16.0f, 16.0f);
        }
    };
    const auto type = typeOfInst (inst);
    if (type == MT_259)
    {
        led (154.0f, 62.0f, ledA >= 0.0f ? col::ledGreen : col::ledRed, std::fabs (ledA));
        led (229.0f, 400.0f, col::ledGreen, ledOn ? 1.0f : 0.0f);
    }
    else if (type == MT_Midi)
        led (150.0f, 62.0f, col::ledRed, ledOn ? 1.0f : 0.0f);
    else if (type == MT_Output)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const float v = ch == 0 ? ledA : ledB;
            auto bar = juce::Rectangle<float> (134.0f + 6.0f * (float) ch, 60.0f, 4.0f, 200.0f);
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
    const auto type = typeOfInst (inst);
    if (type == MT_259)
    {
        auto* m = proc.rack.m259 (inst - kInst259);
        const float a = m->moLed.load (std::memory_order_relaxed);
        const bool l = m->lockLed.load (std::memory_order_relaxed);
        // only the low range is slow enough to see the LED move; at audio rate it shows a steady glow
        if (std::fabs (a - ledA) > 0.02f || l != ledOn) { ledA = a; ledOn = l; repaint (130, 40, 50, 44); repaint (213, 384, 32, 32); }
    }
    else if (type == MT_Midi)
    {
        const bool on = proc.rack.midi().gateLed.load (std::memory_order_relaxed);
        if (on != ledOn) { ledOn = on; repaint (136, 48, 30, 30); }
    }
    else if (type == MT_Output)
    {
        ledA = proc.rack.output().peakL.load (std::memory_order_relaxed);
        ledB = proc.rack.output().peakR.load (std::memory_order_relaxed);
        repaint (130, 56, 16, 210);
    }
}

void ModulePanel::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() || typeOfInst (inst) != MT_259) return;
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
    for (int i = 0; i < kMax259; ++i) if (proc.isPresent (kInst259 + i)) order.push_back (kInst259 + i);
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
