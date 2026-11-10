#include "RackView.h"
#include "../modules/M259.h"
#include "../modules/MidiInterface.h"
#include "../modules/OutputModule.h"
#include "../modules/M281.h"
#include "../modules/M292.h"
#include "../modules/M266.h"

namespace wc::ui
{
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
    : inst (i), index (dsp), output (s.output), kind (s.kind), pulse (s.pulse), rack (rv)
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
        const juce::Colour ins = pulse ? juce::Colour (0xffc62127) : (output ? juce::Colour (0xff2452b8) : juce::Colour (0xff17181b));
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
ParamSwitch::ParamSwitch (juce::RangedAudioParameter& p, int n, const juce::String& l)
    : param (p), positions (n), attach (p, [this] (float v) { value = juce::roundToInt (v); repaint(); })
{
    topIsOne = dynamic_cast<juce::AudioParameterBool*> (&p) != nullptr;
    labels = juce::StringArray::fromTokens (l, "|", "");
    attach.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip (p.getName (64) + ": " + l.replace ("|", " / ") + "  (click a position)");
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
    g.setColour (juce::Colour (0xfff7f7f5));
    g.fillEllipse (cx - 3.6f, ty - 3.6f, 7.2f, 7.2f);
    g.setColour (juce::Colour (0xff6c6f75));
    g.drawEllipse (cx - 3.6f, ty - 3.6f, 7.2f, 7.2f, 0.7f);
    if (labels.size() == 1)
    {
        drawItalic (g, labels[0], { 0.0f, 0.0f, 24.0f + 30.0f, 10.0f }, juce::Justification::centredLeft, WcLook::label (9.0f), col::ink);
        return;
    }
    // position labels
    for (int i = 0; i < juce::jmin (positions, labels.size()); ++i)
    {
        const float y = positions == 3 ? cy + (float) (i - 1) * span : (i == 0 ? cy - span : cy + span);
        drawItalic (g, labels[i], { 24.0f, y - 6.0f, (float) getWidth() - 24.0f, 12.0f }, juce::Justification::centredLeft,
                    WcLook::label (9.5f), i == pos ? col::ink : col::ink.withAlpha (0.55f));
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
// ModulePanel
// =================================================================================================
ModulePanel::ModulePanel (RackView& rv, WcProcessor& p, int i)
    : inst (i), rack (rv), proc (p), spec (specFor (typeOfInst (i)))
{
    prefix = instPrefix (i);
    setSize ((int) spec.width, (int) spec.height);
    for (auto& c : spec.controls)
    {
        if (c.style == CtrlStyle::Push)
        {
            const int n = juce::String (c.suffix).getTrailingIntValue() - 1;
            auto b = std::make_unique<PushButton> (proc.rack.m281 (subIndex (inst))->manual[juce::jlimit (0, 3, n)]);
            b->setBounds ((int) c.x - 13, (int) c.y - 13, 26, 26);
            addAndMakeVisible (*b);
            pushes.push_back (std::move (b));
            continue;
        }
        auto* param = proc.apvts.getParameter (prefix + c.suffix);
        jassert (param != nullptr);
        if (param == nullptr) continue;
        if (c.style == CtrlStyle::Toggle || c.style == CtrlStyle::Switch3)
        {
            auto sw = std::make_unique<ParamSwitch> (*param, c.style == CtrlStyle::Switch3 ? 3 : 2, c.label);
            const bool single = ! juce::String (c.label).containsChar ('|');
            const int h = c.style == CtrlStyle::Switch3 ? 46 : (single ? 50 : 38);
            sw->setBounds ((int) c.x - 12, (int) c.y - h / 2, single ? 56 : 74, h);
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
    if (isRemovable (i))
    {
        removeButton.setTooltip ("Remove this module from the rack");
        removeButton.setBounds ((int) spec.width - 26, 7, 18, 18);
        removeButton.onClick = [this] { proc.setPresent (inst, false); };
        addAndMakeVisible (removeButton);
    }
}

// printed scale around a knob (blue ring + numbers), as on the 200-series panels
static void drawScale (juce::Graphics& g, const ControlSpec& c)
{
    const float r = c.style == CtrlStyle::BigKnob ? 33.0f : (c.style == CtrlStyle::Knob ? 24.0f : 16.0f);
    const float a0 = juce::MathConstants<float>::pi * 1.2f, a1 = juce::MathConstants<float>::pi * 2.8f;
    g.setColour (col::ink);
    if (c.style != CtrlStyle::SmallKnob)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r + 3.0f, r + 3.0f, 0.0f, a0, a1, true);
        g.strokePath (arc, juce::PathStrokeType (c.style == CtrlStyle::BigKnob ? 2.6f : 2.0f));
    }
    if (c.scale == nullptr) return;
    const auto marks = juce::StringArray::fromTokens (c.scale, "|", "");
    const float tr = r + (c.style == CtrlStyle::BigKnob ? 13.0f : 11.0f);
    for (int i = 0; i < marks.size(); ++i)
    {
        const float t = marks.size() == 1 ? 0.5f : (float) i / (float) (marks.size() - 1);
        const float a = a0 + (a1 - a0) * t;
        const float x = c.x + tr * std::sin (a), y = c.y - tr * std::cos (a);
        drawItalic (g, marks[i], { x - 16.0f, y - 6.0f, 32.0f, 12.0f }, juce::Justification::centred, WcLook::label (c.style == CtrlStyle::BigKnob ? 9.0f : 8.0f), col::ink);
    }
}

void ModulePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    // anodised light grey aluminium
    g.setGradientFill (juce::ColourGradient (col::panelHi, 0, 0, col::panelLo, 0, r.getHeight(), false));
    g.fillRect (r);
    juce::Random rnd (inst * 977 + 3);
    for (int y = 0; y < getHeight(); y += 2)
    {
        g.setColour (juce::Colours::white.withAlpha (0.03f + 0.04f * rnd.nextFloat()));
        g.drawHorizontalLine (y, 0.0f, r.getWidth());
    }
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRect (r, 1.0f);
    // mounting screws
    for (auto p : { juce::Point<float> (9, 9), juce::Point<float> (r.getWidth() - 9, r.getHeight() - 9), juce::Point<float> (9, r.getHeight() - 9) })
    {
        g.setColour (juce::Colour (0xffa3a4a6));
        g.fillEllipse (p.x - 3.5f, p.y - 3.5f, 7.0f, 7.0f);
        g.setColour (juce::Colour (0xff6a6c70));
        g.drawLine (p.x - 2.5f, p.y, p.x + 2.5f, p.y, 1.0f);
    }

    // title (upper case, bold italic, blue) and footer
    {
        juce::String t = spec.name;
        if (isRemovable (inst) && typeOfInst (inst) != MT_266) t << "  " << juce::String::charToString ((juce::juce_wchar) ('A' + subIndex (inst)));
        drawItalic (g, t, { 20.0f, 6.0f, r.getWidth() - 50.0f, 22.0f }, juce::Justification::centred, WcLook::title (15.0f), col::ink);
        juce::String foot ("CALABI YAU   ");
        foot << spec.model;
        if (juce::String (spec.after).isNotEmpty()) foot << "   -   " << spec.after;
        drawItalic (g, foot, { 20.0f, r.getHeight() - 20.0f, r.getWidth() - 40.0f, 14.0f }, juce::Justification::centred, WcLook::title (9.5f), col::ink);
    }

    // section frames: blue rules with the title on the frame
    for (auto& sct : spec.sections)
    {
        auto sr = juce::Rectangle<float> (sct.x, sct.y, sct.w, sct.h);
        g.setColour (col::ink);
        g.drawRoundedRectangle (sr, 5.0f, 1.4f);
        if (juce::String (sct.title).isNotEmpty())
        {
            const auto f = WcLook::label (10.0f);
            const float tw = juce::GlyphArrangement::getStringWidth (f, sct.title) + 12.0f;
            auto tr = juce::Rectangle<float> (tw, 13.0f).withCentre ({ sr.getCentreX(), sr.getY() });
            g.setGradientFill (juce::ColourGradient (col::panelHi, 0, 0, col::panelLo, 0, r.getHeight(), false));
            g.fillRect (tr);
            drawItalic (g, sct.title, tr, juce::Justification::centred, f, col::ink);
        }
    }

    // printed scales and knob labels
    for (auto& c : spec.controls)
    {
        if (c.style == CtrlStyle::BigKnob || c.style == CtrlStyle::Knob || c.style == CtrlStyle::SmallKnob)
        {
            drawScale (g, c);
            const float half = c.style == CtrlStyle::BigKnob ? 37.0f : (c.style == CtrlStyle::Knob ? 28.0f : 20.0f);
            const float extra = c.scale != nullptr && c.style != CtrlStyle::SmallKnob ? 6.0f : 0.0f;
            drawItalic (g, c.label, { c.x - 46.0f, c.y + half + extra - 2.0f, 92.0f, 13.0f }, juce::Justification::centred,
                        WcLook::label (c.style == CtrlStyle::SmallKnob ? 9.0f : 10.5f), col::ink);
        }
    }
    // jack labels (below the jack, lowercase italics)
    for (auto& j : spec.jacks)
        if (juce::String (j.label).isNotEmpty())
            drawItalic (g, j.label, { j.x - 26.0f, j.y + 11.0f, 52.0f, 12.0f }, juce::Justification::centred, WcLook::label (9.0f), col::ink);

    // module-specific printing
    const auto type = typeOfInst (inst);
    if (type == MT_281)
    {
        const float rowY[4] = { 78, 162, 246, 330 };
        const char* L[4] = { "A", "B", "C", "D" };
        for (int n = 0; n < 4; ++n)
        {
            const float y = rowY[n];
            g.setColour (col::ink);
            g.drawEllipse (199.0f, y + 20.0f, 16.0f, 16.0f, 1.2f);                 // channel letter
            drawItalic (g, L[n], { 199.0f, y + 20.0f, 16.0f, 16.0f }, juce::Justification::centred, WcLook::title (10.0f), col::ink);
            g.drawLine (322.0f, y - 12.0f, 330.0f, y - 12.0f, 1.2f);               // pulse symbol by the EOD jack
            g.drawLine (326.0f, y - 22.0f, 326.0f, y - 12.0f, 1.2f);
            juce::Path fn;                                                           // function symbol by the FUNC jack
            fn.startNewSubPath (320.0f, y + 22.0f); fn.lineTo (326.0f, y + 12.0f); fn.lineTo (332.0f, y + 22.0f);
            g.strokePath (fn, juce::PathStrokeType (1.2f));
            drawItalic (g, "outs", { 340.0f, y - 6.0f, 40.0f, 12.0f }, juce::Justification::centredRight, WcLook::label (8.0f), col::ink);
        }
        for (int p = 0; p < 2; ++p)
        {
            const float ky = p == 0 ? 88.0f : 238.0f, jy = p == 0 ? 152.0f : 302.0f;
            juce::Path a1, a2;
            a1.startNewSubPath (420.0f, ky + 38.0f); a1.lineTo (412.0f, jy - 14.0f);
            a2.startNewSubPath (444.0f, ky + 38.0f); a2.lineTo (452.0f, jy - 14.0f);
            g.setColour (col::ink);
            g.strokePath (a1, juce::PathStrokeType (1.2f));
            g.strokePath (a2, juce::PathStrokeType (1.2f));
            drawItalic (g, p == 0 ? "A>B" : "C>D", { 392.0f, jy + 11.0f, 36.0f, 12.0f }, juce::Justification::centred, WcLook::label (8.5f), col::ink);
            drawItalic (g, p == 0 ? "B>A" : "D>C", { 436.0f, jy + 11.0f, 36.0f, 12.0f }, juce::Justification::centred, WcLook::label (8.5f), col::ink);
        }
        drawItalic (g, "A - B", { 386.0f, 338.0f, 40.0f, 12.0f }, juce::Justification::centred, WcLook::label (8.5f), col::ink);
        drawItalic (g, "C - D", { 426.0f, 338.0f, 40.0f, 12.0f }, juce::Justification::centred, WcLook::label (8.5f), col::ink);
    }
    else if (type == MT_292)
    {
        const float cx[4] = { 53, 147, 241, 335 };
        const char* L[4] = { "A", "B", "C", "D" };
        for (int n = 0; n < 4; ++n)
        {
            g.setColour (col::ink);
            g.drawEllipse (cx[n] - 8.0f, 44.0f, 16.0f, 16.0f, 1.2f);
            drawItalic (g, L[n], { cx[n] - 8.0f, 44.0f, 16.0f, 16.0f }, juce::Justification::centred, WcLook::title (10.0f), col::ink);
            g.drawLine (cx[n], 302.0f, cx[n], 324.0f, 1.0f);       // signal flow: input -> c.v. -> output
        }
        drawItalic (g, "mix", { 386.0f, 340.0f, 50.0f, 12.0f }, juce::Justification::centred, WcLook::label (9.0f), col::ink);
    }
    else if (type == MT_259)
    {
        drawItalic (g, "program inputs", { 182.0f, 300.0f, 150.0f, 12.0f }, juce::Justification::centred, WcLook::label (9.0f), col::ink);
        g.setColour (col::ink);
        g.drawLine (192.0f, 313.0f, 322.0f, 313.0f, 1.0f);
    }
}

void ModulePanel::paintOverChildren (juce::Graphics& g)
{
    auto led = [&] (float x, float y, juce::Colour c, float a)
    {
        g.setColour (juce::Colour (0xff3a2222));
        g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
        if (a > 0.02f)
        {
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
            g.fillEllipse (x - 3.2f, y - 3.2f, 6.4f, 6.4f);
            g.setColour (c.withAlpha (0.25f * juce::jlimit (0.0f, 1.0f, a)));
            g.fillEllipse (x - 7.5f, y - 7.5f, 15.0f, 15.0f);
        }
    };
    const auto type = typeOfInst (inst);
    if (type == MT_259)
    {
        led (156.0f, 54.0f, ledA >= 0.0f ? col::ledGreen : col::ledRed, std::fabs (ledA));
        led (229.0f, 378.0f, col::ledGreen, ledOn ? 1.0f : 0.0f);
    }
    else if (type == MT_Midi)
        led (150.0f, 56.0f, col::ledRed, ledOn ? 1.0f : 0.0f);
    else if (type == MT_281)
    {
        const float rowY[4] = { 78, 162, 246, 330 };
        for (int n = 0; n < 4; ++n)
        {
            led (366.0f, rowY[n] - 17.0f, juce::Colour (0xffffd23a), leds4[n] > 0.999f ? 1.0f : 0.0f);   // peak / pulse
            led (366.0f, rowY[n] + 17.0f, col::ledRed, leds4[n]);                                        // function level
        }
    }
    else if (type == MT_292)
    {
        const float cx[4] = { 53, 147, 241, 335 };
        for (int n = 0; n < 4; ++n) led (cx[n] + 32.0f, 200.0f, juce::Colour (0xffffb14a), leds4[n]);
    }
    else if (type == MT_266)
        led (171.0f, 150.0f, col::ledRed, ledA);
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
    const auto type = typeOfInst (inst);
    if (type == MT_259)
    {
        auto* m = proc.rack.m259 (subIndex (inst));
        const float a = m->moLed.load (std::memory_order_relaxed);
        const bool l = m->lockLed.load (std::memory_order_relaxed);
        if (std::fabs (a - ledA) > 0.02f || l != ledOn) { ledA = a; ledOn = l; repaint (140, 40, 32, 30); repaint (215, 364, 30, 30); }
    }
    else if (type == MT_281 || type == MT_292)
    {
        bool changed = false;
        for (int n = 0; n < 4; ++n)
        {
            float v = type == MT_281 ? proc.rack.m281 (subIndex (inst))->level[n].load (std::memory_order_relaxed)
                                     : std::sqrt (proc.rack.m292 (subIndex (inst))->led[n].load (std::memory_order_relaxed));
            if (std::fabs (v - leds4[n]) > 0.01f) { leds4[n] = v; changed = true; }
        }
        if (changed) { if (type == MT_281) repaint (354, 30, 26, 340); else repaint (0, 186, getWidth(), 30); }
    }
    else if (type == MT_266)
    {
        const float v = proc.rack.m266()->fluctLed.load (std::memory_order_relaxed);
        if (std::fabs (v - ledA) > 0.01f) { ledA = v; repaint (160, 139, 22, 22); }
    }
    else if (type == MT_Midi)
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

void ModulePanel::mouseDown (const juce::MouseEvent& e)
{
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
