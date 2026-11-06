#include "Widgets.h"
#include "../PluginProcessor.h"

namespace pg::ui
{
using namespace juce;

// =============================================================================
// ParamControl
// =============================================================================
ParamControl::ParamControl (APVTS& state, const String& id, const String& label)
{
    auto* prm = state.getParameter (id);
    jassert (prm != nullptr);
    caption.setText (label, dontSendNotification);
    caption.setJustificationType (Justification::centred);
    caption.setColour (Label::textColourId, colours::textDim);
    caption.setFont (fonts::sans (11.0f));
    caption.setInterceptsMouseClicks (false, false);

    if (auto* c = dynamic_cast<AudioParameterChoice*> (prm))
    {
        k = Menu;
        combo.addItemList (c->choices, 1);
        addAndMakeVisible (combo);
        addAndMakeVisible (caption);
        ca = std::make_unique<APVTS::ComboBoxAttachment> (state, id, combo);
    }
    else if (dynamic_cast<AudioParameterBool*> (prm) != nullptr)
    {
        k = Switch;
        toggle.setButtonText (label);
        addAndMakeVisible (toggle);
        ba = std::make_unique<APVTS::ButtonAttachment> (state, id, toggle);
    }
    else
    {
        k = Knob;
        slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        slider.setTooltip (prm->getName (64));
        addAndMakeVisible (slider);
        addAndMakeVisible (caption);
        sa = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }
}

void ParamControl::resized()
{
    auto r = getLocalBounds();
    switch (k)
    {
        case Knob:
            caption.setBounds (r.removeFromBottom (16));
            slider.setBounds (r.withSizeKeepingCentre (jmin (r.getWidth(), r.getHeight()), jmin (r.getWidth(), r.getHeight())));
            break;
        case Menu:
        {
            auto c = r.withSizeKeepingCentre (r.getWidth() - 6, 40);
            caption.setBounds (c.removeFromTop (16));
            combo.setBounds (c.removeFromTop (24));
            break;
        }
        case Switch:
            toggle.setBounds (r.withSizeKeepingCentre (r.getWidth() - 6, 24));
            break;
    }
}

// =============================================================================
// Panel
// =============================================================================
Panel::Panel (APVTS& state, const String& t, const StringArray& items, int columns, Colour accent)
    : title (t), cols (columns), accentColour (accent)
{
    for (auto& it : items)
    {
        auto* c = controls.add (new ParamControl (state, it.upToFirstOccurrenceOf (":", false, false),
                                                   it.fromFirstOccurrenceOf (":", false, false)));
        c->slider.setColour (Slider::rotarySliderFillColourId, accentColour);
        addAndMakeVisible (c);
    }
}

void Panel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    brand::chamferFrame (g, r, 14.0f, 5.0f, cosmos::deepSpace, accentColour.withAlpha (0.85f), true);
    brand::sparkle (g, { r.getX() + 22.0f, r.getY() + 21.0f }, 6.0f, accentColour);
    g.setColour (colours::text);
    g.setFont (fonts::label (11.5f));
    g.drawText (title.toUpperCase(), Rectangle<float> (r.getX() + 34.0f, r.getY() + 10.0f, r.getWidth() - 48.0f, 22.0f), Justification::centredLeft);
}

void Panel::resized()
{
    auto r = getLocalBounds().reduced (14, 10);
    r.removeFromTop (24);
    if (extraHeight > 0)
    {
        extraArea = r.removeFromTop (extraHeight);
        if (layoutExtra) layoutExtra (extraArea);
        r.removeFromTop (4);
    }
    if (controls.isEmpty()) return;
    const int rows = (controls.size() + cols - 1) / cols;
    const int cellW = r.getWidth() / cols;
    const int cellH = jmin (84, r.getHeight() / jmax (1, rows));
    for (int i = 0; i < controls.size(); ++i)
    {
        const int row = i / cols, col = i % cols;
        controls[i]->setBounds (r.getX() + col * cellW, r.getY() + row * cellH, cellW, cellH);
    }
}

// =============================================================================
// SampleView
// =============================================================================
SampleView::SampleView (SynthgularityProcessor& p) : proc (p) {}

void SampleView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto inner = brand::chamferFrame (g, r, 16.0f, 6.0f, cosmos::voidC, dragOver ? cosmos::gold : cosmos::neon, true);
    r = inner.reduced (6.0f, 4.0f);
    brand::perspectiveGrid (g, r.withTrimmedTop (r.getHeight() * 0.62f), cosmos::electric, 0.35f, 14, 6);

    auto s = proc.getSample();
    if (s == nullptr) return;
    const float* cache = proc.paramCache();
    const float pos = cache[pg::gr_pos], spray = cache[pg::gr_spray];

    // spray region
    const float w = r.getWidth(), x0 = r.getX();
    g.setColour (cosmos::neon.withAlpha (0.10f));
    const float sx = (pos - spray * 0.5f) * w, ex = (pos + spray * 0.5f) * w;
    g.fillRect (Rectangle<float> (x0 + sx, r.getY(), ex - sx, r.getHeight()));
    if (sx < 0.0f) g.fillRect (Rectangle<float> (x0 + w + sx, r.getY(), -sx, r.getHeight()));
    if (ex > w) g.fillRect (Rectangle<float> (x0, r.getY(), ex - w, r.getHeight()));

    // waveform
    const float mid = r.getCentreY(), half = r.getHeight() * 0.42f;
    Path wave;
    const int n = (int) s->peaks.size();
    for (int i = 0; i < n; ++i)
    {
        const float x = x0 + (float) i / (float) (n - 1) * w;
        const float h = s->peaks[(size_t) i] * half;
        wave.addRectangle (x, mid - h, jmax (1.0f, w / (float) n), h * 2.0f + 0.5f);
    }
    g.setGradientFill (ColourGradient (cosmos::orchid, 0.0f, mid - half, cosmos::electric, 0.0f, mid, true));
    g.fillPath (wave);

    // position: neon line with a node on top
    g.setColour (cosmos::neon);
    g.fillRect (Rectangle<float> (x0 + pos * w - 1.0f, r.getY(), 2.0f, r.getHeight()));
    brand::bolt (g, { x0 + pos * w, r.getY() + 6.0f }, 6.0f, cosmos::neon);

    // live grains
    auto& viz = proc.synth.viz;
    for (size_t i = 0; i < viz.pos.size(); ++i)
    {
        const float gp = viz.pos[i].load();
        if (gp < 0.0f) continue;
        const float a = viz.amp[i].load();
        const float y = mid + half * 0.8f * std::sin ((float) i * 2.39f);
        brand::sparkle (g, { x0 + gp * w, y }, 3.0f + 6.0f * a, cosmos::starlight.withAlpha (0.35f + 0.65f * a));
    }

    // caption
    g.setColour (colours::text);
    g.setFont (fonts::label (11.5f));
    const double secs = (double) s->length / s->sampleRate;
    g.drawText (s->name + "   " + String (secs, 2) + " s   " + String ((int) s->sampleRate) + " Hz",
                r.reduced (10.0f, 6.0f).withTrimmedLeft (8.0f), Justification::topLeft);
    g.setColour (colours::textDim);
    g.setFont (fonts::sans (11.0f));
    g.drawText ("Drop WAV, AIFF, FLAC, MP3 or OGG here  -  click or drag to set the position",
                r.reduced (10.0f, 6.0f).withTrimmedLeft (8.0f), Justification::bottomLeft);
}

void SampleView::setPosFromMouse (float x)
{
    if (auto* prm = proc.apvts.getParameter ("gr_pos"))
        prm->setValueNotifyingHost (jlimit (0.0f, 1.0f, (x - 12.0f) / ((float) getWidth() - 24.0f)));
}

void SampleView::mouseDown (const MouseEvent& e)
{
    if (auto* prm = proc.apvts.getParameter ("gr_pos")) prm->beginChangeGesture();
    dragging = true;
    setPosFromMouse ((float) e.x);
}
void SampleView::mouseDrag (const MouseEvent& e) { setPosFromMouse ((float) e.x); }
void SampleView::mouseUp (const MouseEvent&)
{
    if (dragging) if (auto* prm = proc.apvts.getParameter ("gr_pos")) prm->endChangeGesture();
    dragging = false;
}

bool SampleView::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;wave;aif;aiff;flac;mp3;ogg")) return true;
    return false;
}

void SampleView::filesDropped (const StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;wave;aif;aiff;flac;mp3;ogg"))
        {
            if (onFile) onFile (File (f));
            break;
        }
    repaint();
}

// =============================================================================
// WavetableView
// =============================================================================
void WavetableView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (cosmos::voidC);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (cosmos::nebula);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    auto t = proc.getActiveTable (oscIndex);
    if (t == nullptr) return;
    const float* cache = proc.paramCache();
    const float pos = cache[oscIndex == 0 ? pg::wtA_pos : pg::wtB_pos];
    const bool on = cache[oscIndex == 0 ? pg::wtA_on : pg::wtB_on] > 0.5f;
    const int frames = t->numFrames;
    const int len = pg::Wavetable::levelLength (0);

    auto drawFrame = [&] (int f, Colour c, float thick, float yOff)
    {
        const float* d = t->level (f, 0);
        Path p;
        const int step = 16;
        for (int i = 0; i <= len; i += step)
        {
            const float x = r.getX() + 8.0f + (float) i / (float) len * (r.getWidth() - 16.0f);
            const float y = r.getCentreY() + yOff - d[jmin (i, len)] * r.getHeight() * 0.36f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (p, PathStrokeType (thick));
    };
    for (int k = 0; k < 6 && frames > 1; ++k)
    {
        const int f = (int) ((float) k / 5.0f * (float) (frames - 1));
        drawFrame (f, cosmos::nebula, 1.0f, 0.0f);
    }
    drawFrame (jlimit (0, frames - 1, (int) std::round (pos * (float) (frames - 1))), on ? cosmos::neon : cosmos::steel, 2.0f, 0.0f);
    g.setColour (colours::textDim);
    g.setFont (fonts::sans (10.5f));
    g.drawText (t->name + "  (" + String (frames) + " frames)", r.reduced (8.0f, 4.0f), Justification::topLeft);
}

// =============================================================================
// StepLaneEditor
// =============================================================================
int StepLaneEditor::length() const
{
    return jlimit (1, 32, (int) proc.paramCache()[pg::seq1_len + laneIndex * (pg::seq2_on - pg::seq1_on)]);
}

void StepLaneEditor::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (cosmos::voidC);
    g.fillRoundedRectangle (r, 6.0f);
    const int len = length();
    const int play = proc.synth.steps.seqPlayStep[(size_t) laneIndex].load();
    const float cw = r.getWidth() / 32.0f;
    for (int s = 0; s < 32; ++s)
    {
        const float v = proc.synth.steps.seq[(size_t) (laneIndex * 32 + s)].load();
        const bool locked = proc.synth.steps.seqLock[(size_t) (laneIndex * 32 + s)].load();
        auto cell = Rectangle<float> (r.getX() + (float) s * cw, r.getY(), cw, r.getHeight()).reduced (1.5f, 3.0f);
        const bool active = s < len;
        g.setColour (active ? cosmos::deepSpace : cosmos::night);
        g.fillRoundedRectangle (cell, 2.0f);
        auto bar = cell.withTrimmedTop (cell.getHeight() * (1.0f - v));
        const auto barCol = s == play ? cosmos::starlight : (s % 4 == 0 ? cosmos::orchid : cosmos::electric);
        g.setColour (barCol.withAlpha (active ? 1.0f : 0.25f));
        g.fillRoundedRectangle (bar, 2.0f);
        if (locked)
        {
            g.setColour (cosmos::gold);
            g.fillRect (cell.removeFromTop (3.0f));
        }
    }
}

void StepLaneEditor::setFromMouse (Point<float> p)
{
    const int s = jlimit (0, 31, (int) (p.x / (float) getWidth() * 32.0f));
    const float v = jlimit (0.0f, 1.0f, 1.0f - p.y / (float) getHeight());
    proc.synth.steps.seq[(size_t) (laneIndex * 32 + s)].store (v);
    repaint();
}

void StepLaneEditor::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        const int s = jlimit (0, 31, (int) ((float) e.x / (float) getWidth() * 32.0f));
        auto& l = proc.synth.steps.seqLock[(size_t) (laneIndex * 32 + s)];
        l.store (! l.load());
        repaint();
        return;
    }
    setFromMouse (e.position);
}
void StepLaneEditor::mouseDrag (const MouseEvent& e) { if (! e.mods.isPopupMenu()) setFromMouse (e.position); }

void StepLaneEditor::randomize (bool mutate)
{
    Random rnd;
    for (int s = 0; s < 32; ++s)
    {
        const size_t i = (size_t) (laneIndex * 32 + s);
        if (proc.synth.steps.seqLock[i].load()) continue;
        if (mutate)
        {
            if (rnd.nextFloat() < 0.3f)
                proc.synth.steps.seq[i].store (jlimit (0.0f, 1.0f, proc.synth.steps.seq[i].load() + (rnd.nextFloat() - 0.5f) * 0.4f));
        }
        else
            proc.synth.steps.seq[i].store (rnd.nextFloat());
    }
    repaint();
}

void StepLaneEditor::euclid()
{
    const int len = length();
    euclidHits = euclidHits % len + 1;   // each click adds a hit
    for (int s = 0; s < len; ++s)
    {
        const size_t i = (size_t) (laneIndex * 32 + s);
        if (proc.synth.steps.seqLock[i].load()) continue;
        const bool hit = ((s * euclidHits) % len) < euclidHits;
        proc.synth.steps.seq[i].store (hit ? 1.0f : 0.0f);
    }
    repaint();
}

void StepLaneEditor::ramp()
{
    const int len = length();
    for (int s = 0; s < len; ++s)
        if (! proc.synth.steps.seqLock[(size_t) (laneIndex * 32 + s)].load())
            proc.synth.steps.seq[(size_t) (laneIndex * 32 + s)].store (len == 1 ? 1.0f : (float) s / (float) (len - 1));
    repaint();
}

void StepLaneEditor::clear()
{
    for (int s = 0; s < 32; ++s)
        if (! proc.synth.steps.seqLock[(size_t) (laneIndex * 32 + s)].load())
            proc.synth.steps.seq[(size_t) (laneIndex * 32 + s)].store (0.5f);
    repaint();
}

// =============================================================================
// ArpGrid
// =============================================================================
static const char* arpRowNames[] = { "ON", "PITCH", "VEL", "TIE", "RATCHET" };

Rectangle<float> ArpGrid::row (int r) const
{
    auto b = getLocalBounds().toFloat().withTrimmedLeft (70.0f);
    const float heights[] = { 0.12f, 0.36f, 0.28f, 0.12f, 0.12f };
    float y = b.getY();
    for (int i = 0; i < r; ++i) y += heights[i] * b.getHeight();
    return { b.getX(), y, b.getWidth(), heights[r] * b.getHeight() };
}

void ArpGrid::paint (Graphics& g)
{
    auto& st = proc.synth.steps;
    const int len = jlimit (1, 32, (int) proc.paramCache()[pg::arp_len]);
    const int play = st.arpPlayStep.load();
    for (int r = 0; r < 5; ++r)
    {
        auto rr = row (r);
        g.setColour (colours::textDim);
        g.setFont (fonts::label (10.5f));
        g.drawText (arpRowNames[r], Rectangle<float> (0.0f, rr.getY(), 66.0f, rr.getHeight()), Justification::centredRight);
        const float cw = rr.getWidth() / 32.0f;
        for (int s = 0; s < 32; ++s)
        {
            auto cell = Rectangle<float> (rr.getX() + (float) s * cw, rr.getY(), cw, rr.getHeight()).reduced (1.5f, 2.0f);
            const bool active = s < len;
            g.setColour (active ? cosmos::deepSpace : cosmos::night);
            g.fillRoundedRectangle (cell, 2.0f);
            const Colour rowCol[] = { cosmos::neon, cosmos::orchid, cosmos::electric, cosmos::fuchsia, cosmos::gold };
            const auto col = (s == play ? cosmos::starlight : rowCol[r]).withAlpha (active ? 1.0f : 0.25f);
            g.setColour (col);
            switch (r)
            {
                case 0: if (st.arpOn[(size_t) s].load()) g.fillRoundedRectangle (cell.reduced (2.0f), 2.0f); break;
                case 1:
                {
                    const float t = (float) st.arpTranspose[(size_t) s].load() / 24.0f;
                    const float midY = cell.getCentreY();
                    const float y = midY - t * cell.getHeight() * 0.5f;
                    g.fillRect (Rectangle<float> (cell.getX(), jmin (midY, y), cell.getWidth(), jmax (1.5f, std::abs (y - midY))));
                    g.setFont (fonts::sans (9.5f));
                    g.setColour (colours::text.withAlpha (0.7f));
                    if (st.arpTranspose[(size_t) s].load() != 0)
                        g.drawText (String (st.arpTranspose[(size_t) s].load()), cell, Justification::centredBottom);
                    break;
                }
                case 2: g.fillRoundedRectangle (cell.withTrimmedTop (cell.getHeight() * (1.0f - st.arpVel[(size_t) s].load())), 2.0f); break;
                case 3: if (st.arpTie[(size_t) s].load()) g.fillRoundedRectangle (cell.reduced (2.0f, 4.0f), 2.0f); break;
                case 4:
                {
                    const int n = st.arpRatchet[(size_t) s].load();
                    for (int k = 0; k < n; ++k)
                        g.fillEllipse (Rectangle<float> (4.0f, 4.0f).withCentre ({ cell.getCentreX(), cell.getY() + cell.getHeight() * (float) (k + 1) / (float) (n + 1) }));
                    break;
                }
            }
        }
    }
}

void ArpGrid::edit (Point<float> p, bool click)
{
    auto& st = proc.synth.steps;
    if (click)
    {
        dragRow = -1;
        for (int r = 0; r < 5; ++r) if (row (r).contains (p)) dragRow = r;
    }
    if (dragRow < 0) return;
    auto rr = row (dragRow);
    const int s = jlimit (0, 31, (int) ((p.x - rr.getX()) / rr.getWidth() * 32.0f));
    const float v = jlimit (0.0f, 1.0f, 1.0f - (p.y - rr.getY()) / rr.getHeight());
    switch (dragRow)
    {
        case 0: if (click) st.arpOn[(size_t) s].store (! st.arpOn[(size_t) s].load()); break;
        case 1: st.arpTranspose[(size_t) s].store (jlimit (-24, 24, (int) std::round ((v * 2.0f - 1.0f) * 24.0f))); break;
        case 2: st.arpVel[(size_t) s].store (jmax (0.05f, v)); break;
        case 3: if (click) st.arpTie[(size_t) s].store (! st.arpTie[(size_t) s].load()); break;
        case 4: if (click) st.arpRatchet[(size_t) s].store (st.arpRatchet[(size_t) s].load() % 4 + 1); break;
        default: break;
    }
    repaint();
}

void ArpGrid::mouseDown (const MouseEvent& e) { edit (e.position, true); }
void ArpGrid::mouseDrag (const MouseEvent& e) { edit (e.position, false); }

// =============================================================================
// OrbPad
// =============================================================================
Rectangle<float> OrbPad::padArea() const { return getLocalBounds().toFloat().reduced (48.0f); }

void OrbPad::paint (Graphics& g)
{
    auto full = getLocalBounds().toFloat();
    auto inner = brand::chamferFrame (g, full.reduced (2.0f), 22.0f, 7.0f, cosmos::voidC, cosmos::neon, false);
    auto r = padArea();
    const bool on = proc.paramCache()[pg::orb_on] > 0.5f;
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (inner.reduced (4.0f).toNearestInt());
        Random rnd (11);
        for (int i = 0; i < 160; ++i)
        {
            g.setColour (cosmos::starlight.withAlpha (0.2f + rnd.nextFloat() * 0.5f));
            g.fillRect (inner.getX() + rnd.nextFloat() * inner.getWidth(), inner.getY() + rnd.nextFloat() * inner.getHeight() * 0.55f, 1.2f, 1.2f);
        }
        brand::perspectiveGrid (g, inner.withTrimmedTop (inner.getHeight() * 0.5f), cosmos::electric, 0.55f, 14, 9);
    }
    g.setColour (cosmos::nebula.withAlpha (0.8f));
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 4.0f), r.getY(), r.getBottom());
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 4.0f), r.getX(), r.getRight());
    }

    const char* names[] = { "A", "B", "C", "D" };
    const Point<float> corners[] = { r.getTopLeft(), r.getTopRight(), r.getBottomLeft(), r.getBottomRight() };
    for (int c = 0; c < 4; ++c)
    {
        const bool filled = proc.cornerFilled (c);
        auto b = Rectangle<float> (42.0f, 42.0f).withCentre (corners[c]);
        g.setColour (filled ? cosmos::fuchsia : cosmos::night);
        g.fillEllipse (b);
        g.setColour (filled ? cosmos::starlight : cosmos::electric);
        g.drawEllipse (b, 2.0f);
        g.setFont (fonts::display (14.0f));
        g.drawText (names[c], b.translated (2.0f, 0.0f), Justification::centred);
    }
    const float x = proc.apvts.getRawParameterValue ("orb_x")->load();
    const float y = proc.apvts.getRawParameterValue ("orb_y")->load();
    const Point<float> pt { r.getX() + x * r.getWidth(), r.getBottom() - y * r.getHeight() };
    const auto pc = on ? cosmos::neon : cosmos::steel;
    for (int k = 3; k >= 1; --k)
    {
        g.setColour (pc.withAlpha (0.08f * (float) k));
        g.fillEllipse (Rectangle<float> (18.0f + 14.0f * (float) k, 18.0f + 14.0f * (float) k).withCentre (pt));
    }
    brand::bolt (g, pt, 10.0f, pc);
    brand::sparkle (g, pt.translated (16.0f, -16.0f), 7.0f, cosmos::starlight);
    if (! on)
    {
        g.setColour (cosmos::cream);
        g.setFont (fonts::sans (13.0f));
        g.drawText ("ORB is off: turn it on to morph between the corners", r.reduced (20.0f), Justification::centredBottom);
    }
}

void OrbPad::setFromMouse (Point<float> p)
{
    auto r = padArea();
    auto* px = proc.apvts.getParameter ("orb_x");
    auto* py = proc.apvts.getParameter ("orb_y");
    px->setValueNotifyingHost (jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth()));
    py->setValueNotifyingHost (jlimit (0.0f, 1.0f, (r.getBottom() - p.y) / r.getHeight()));
    repaint();
}

void OrbPad::mouseDown (const MouseEvent& e)
{
    proc.apvts.getParameter ("orb_x")->beginChangeGesture();
    proc.apvts.getParameter ("orb_y")->beginChangeGesture();
    setFromMouse (e.position);
}
void OrbPad::mouseDrag (const MouseEvent& e) { setFromMouse (e.position); }
void OrbPad::mouseUp (const MouseEvent&)
{
    proc.apvts.getParameter ("orb_x")->endChangeGesture();
    proc.apvts.getParameter ("orb_y")->endChangeGesture();
}

// =============================================================================
// MatrixRow
// =============================================================================
MatrixRow::MatrixRow (APVTS& state, int s) : slot (s)
{
    const String n = String (s + 1).paddedLeft ('0', 2);
    for (auto* c : { &src, &dst, &via }) addAndMakeVisible (*c);
    src.addItemList (pg::choices::modSources(), 1);
    via.addItemList (pg::choices::modSources(), 1);
    dst.addItemList (pg::choices::modDests(), 1);
    amt.setSliderStyle (Slider::LinearHorizontal);
    amt.setTextBoxStyle (Slider::TextBoxRight, false, 48, 20);
    addAndMakeVisible (amt);
    srcA = std::make_unique<APVTS::ComboBoxAttachment> (state, "mtx" + n + "_src", src);
    dstA = std::make_unique<APVTS::ComboBoxAttachment> (state, "mtx" + n + "_dst", dst);
    viaA = std::make_unique<APVTS::ComboBoxAttachment> (state, "mtx" + n + "_via", via);
    amtA = std::make_unique<APVTS::SliderAttachment> (state, "mtx" + n + "_amt", amt);
    amt.textFromValueFunction = [] (double v) { return String (v, 2); };
    amt.updateText();
}

void MatrixRow::paint (Graphics& g)
{
    g.setColour (src.getSelectedItemIndex() > 0 && dst.getSelectedItemIndex() > 0 ? cosmos::neon : colours::textDim);
    g.setFont (fonts::display (11.0f));
    g.drawText (String (slot + 1), getLocalBounds().removeFromLeft (28), Justification::centred);
}

void MatrixRow::resized()
{
    auto r = getLocalBounds().reduced (0, 2);
    r.removeFromLeft (30);
    const int w = r.getWidth();
    src.setBounds (r.removeFromLeft (w * 22 / 100).reduced (3, 0));
    r.removeFromLeft (6);
    dst.setBounds (r.removeFromLeft (w * 28 / 100).reduced (3, 0));
    r.removeFromLeft (6);
    via.setBounds (r.removeFromRight (w * 18 / 100).reduced (3, 0));
    amt.setBounds (r.reduced (6, 0));
}
} // namespace pg::ui
