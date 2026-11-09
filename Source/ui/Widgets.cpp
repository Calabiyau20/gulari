#include "Widgets.h"
#include "BinaryData.h"
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
    const auto& sk = skin();
    const auto acc = sk.tintFrames ? sk.frameTint : accentColour;
    brand::chamferFrame (g, r, 14.0f, 5.0f, sk.panelFill, acc.withAlpha (0.85f), sk.knob != 1);
    brand::sparkle (g, { r.getX() + 22.0f, r.getY() + 21.0f }, 6.0f, acc);
    g.setColour (sk.text);
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
    const float regA = jmin (cache[pg::gr_start], cache[pg::gr_end]), regB = jmax (cache[pg::gr_start], cache[pg::gr_end]);
    const float pos = regA + cache[pg::gr_pos] * jmax (0.002f, regB - regA), spray = cache[pg::gr_spray] * jmax (0.002f, regB - regA);

    // spray region
    const float w = r.getWidth(), x0 = r.getX();
    {
        // area outside the grain region is dimmed
        const float a0 = jmin (cache[pg::gr_start], cache[pg::gr_end]), a1 = jmax (cache[pg::gr_start], cache[pg::gr_end]);
        g.setColour (cosmos::voidC.withAlpha (0.65f));
        g.fillRect (Rectangle<float> (x0, r.getY(), a0 * w, r.getHeight()));
        g.fillRect (Rectangle<float> (x0 + a1 * w, r.getY(), (1.0f - a1) * w, r.getHeight()));
        g.setColour (cosmos::gold);
        g.fillRect (Rectangle<float> (x0 + a0 * w - 1.0f, r.getY(), 2.0f, r.getHeight()));
        g.fillRect (Rectangle<float> (x0 + a1 * w - 1.0f, r.getY(), 2.0f, r.getHeight()));
    }
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
    const int base = oscIndex == 0 ? pg::wtA_on : (oscIndex == 1 ? pg::wtB_on : pg::wtC_on);
    auto q = [&] (int id) { return cache[base + (id - pg::wtA_on)]; };
    const float pos = q (pg::wtA_pos);
    const bool on = q (pg::wtA_on) > 0.5f;
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
    {
        // the current cycle exactly as the oscillator plays it, warp included
        const int warp = (int) q (pg::wtA_warp), warp2 = (int) q (pg::wtA_warp2);
        const float amt = q (pg::wtA_wamt), amt2 = q (pg::wtA_wamt2);
        float cyc[256];
        pg::WavetableOsc::renderCycle (*t, pos, warp, amt, cyc, 256, warp2, amt2);
        float peak = 0.01f;
        for (float v : cyc) peak = jmax (peak, std::abs (v));
        Path p;
        for (int i = 0; i < 256; ++i)
        {
            const float x = r.getX() + 8.0f + (float) i / 255.0f * (r.getWidth() - 16.0f);
            const float y = r.getCentreY() - cyc[i] / peak * r.getHeight() * 0.36f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (on ? cosmos::neon : cosmos::steel);
        g.strokePath (p, PathStrokeType (2.0f));
        String w;
        if (warp > 0 && amt > 0.0f) w = pg::choices::warpModes()[warp].toUpperCase();
        if (warp2 > 0 && amt2 > 0.0f) w += (w.isEmpty() ? "" : " > ") + pg::choices::warpModes()[warp2].toUpperCase();
        if (w.isNotEmpty())
        {
            g.setColour (cosmos::gold);
            g.setFont (fonts::label (9.5f));
            g.drawText ("WARP " + w, r.reduced (8.0f, 4.0f), Justification::topRight);
        }
    }
    g.setColour (colours::textDim);
    g.setFont (fonts::sans (10.5f));
    g.drawText (t->name + "  (" + String (frames) + " frames)", r.reduced (8.0f, 4.0f), Justification::topLeft);
}

// =============================================================================
// EnvelopeView
// =============================================================================
EnvelopeView::EnvelopeView (APVTS& state, const String& prefix, Colour colour) : st (state), pre (prefix), col (colour)
{
    setRepaintsOnMouseActivity (true);
}

RangedAudioParameter* EnvelopeView::param (const char* suffix) const { return st.getParameter (pre + suffix); }
float EnvelopeView::val (const char* suffix) const { return st.getRawParameterValue (pre + suffix)->load(); }
void EnvelopeView::set (const char* suffix, float v)
{
    if (auto* p = param (suffix)) p->setValueNotifyingHost (p->convertTo0to1 (p->getNormalisableRange().snapToLegalValue (v)));
}

// times are drawn on a compressed scale so short and long settings both stay readable
static float timeToWidth (float t) { return std::log1p (t * 20.0f) / std::log1p (15.0f * 20.0f); }
static float widthToTime (float w) { return std::expm1 (jlimit (0.0f, 1.0f, w) * std::log1p (15.0f * 20.0f)) / 20.0f; }

EnvelopeView::Geo EnvelopeView::geometry() const
{
    auto r = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    const float seg = r.getWidth() / 3.6f;   // each time segment can use up to ~a quarter of the width
    const float top = r.getY(), bottom = r.getBottom();
    Geo g;
    g.start = { r.getX(), bottom };
    g.peak = { g.start.x + seg * timeToWidth (val ("a")), top };
    const float susY = bottom - val ("s") * (bottom - top);
    g.sus = { g.peak.x + seg * timeToWidth (val ("d")), susY };
    g.rel = { jmax (g.sus.x + 12.0f, r.getRight() - seg * 1.05f), susY };
    g.end = { g.rel.x + seg * timeToWidth (val ("r")), bottom };
    return g;
}

void EnvelopeView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (cosmos::voidC);
    g.fillRoundedRectangle (r, 6.0f);
    brand::perspectiveGrid (g, r.reduced (2.0f).withTrimmedTop (r.getHeight() * 0.55f), cosmos::nebula, 0.6f, 10, 4);
    const auto geo = geometry();

    Path curve;
    curve.startNewSubPath (geo.start);
    // analogue-style curves: convex attack, exponential decay and release
    curve.quadraticTo ({ geo.start.x + (geo.peak.x - geo.start.x) * 0.25f, geo.peak.y + (geo.start.y - geo.peak.y) * 0.15f }, geo.peak);
    curve.quadraticTo ({ geo.peak.x + (geo.sus.x - geo.peak.x) * 0.15f, geo.sus.y }, geo.sus);
    curve.lineTo (geo.rel);
    curve.quadraticTo ({ geo.rel.x + (geo.end.x - geo.rel.x) * 0.15f, geo.end.y }, geo.end);

    Path fill (curve);
    fill.lineTo (geo.start);
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (col.withAlpha (0.45f), 0.0f, r.getY(), col.withAlpha (0.02f), 0.0f, r.getBottom(), false));
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (curve, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    // dashed sustain hold
    g.setColour (cosmos::steel.withAlpha (0.5f));
    float dashes[] = { 3.0f, 3.0f };
    g.drawDashedLine (Line<float> (geo.sus, geo.rel), dashes, 2, 1.0f);

    const Point<float> nodes[] = { geo.peak, geo.sus, geo.end };
    for (int i = 0; i < 3; ++i)
        brand::bolt (g, nodes[i], (dragNode == i ? 7.0f : 5.5f), dragNode == i ? cosmos::starlight : col);

    g.setColour (cosmos::steel);
    g.setFont (fonts::sans (9.5f));
    auto ms = [] (float t) { return t < 1.0f ? String (roundToInt (t * 1000.0f)) + " ms" : String (t, 2) + " s"; };
    g.drawText ("A " + ms (val ("a")) + "   D " + ms (val ("d")) + "   S " + String (roundToInt (val ("s") * 100.0f)) + "%   R " + ms (val ("r")),
                r.reduced (10.0f, 4.0f), Justification::topRight);
}

void EnvelopeView::mouseDown (const MouseEvent& e)
{
    const auto geo = geometry();
    const Point<float> nodes[] = { geo.peak, geo.sus, geo.end };
    dragNode = -1;
    float best = 18.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float d = nodes[i].getDistanceFrom (e.position);
        if (d < best) { best = d; dragNode = i; }
    }
    if (dragNode >= 0)
        for (auto* sfx : { "a", "d", "s", "r" }) if (auto* p = param (sfx)) p->beginChangeGesture();
    repaint();
}

void EnvelopeView::mouseDrag (const MouseEvent& e)
{
    if (dragNode < 0) return;
    auto r = getLocalBounds().toFloat().reduced (10.0f, 8.0f);
    const float seg = r.getWidth() / 3.6f;
    const auto geo = geometry();
    switch (dragNode)
    {
        case 0: set ("a", jmax (0.001f, widthToTime ((e.position.x - geo.start.x) / seg))); break;
        case 1:
            set ("d", jmax (0.001f, widthToTime ((e.position.x - geo.peak.x) / seg)));
            set ("s", jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight()));
            break;
        case 2: set ("r", jmax (0.001f, widthToTime ((e.position.x - geo.rel.x) / seg))); break;
        default: break;
    }
    repaint();
}

void EnvelopeView::mouseUp (const MouseEvent&)
{
    if (dragNode >= 0)
        for (auto* sfx : { "a", "d", "s", "r" }) if (auto* p = param (sfx)) p->endChangeGesture();
    dragNode = -1;
    repaint();
}

void EnvelopeView::mouseDoubleClick (const MouseEvent&)
{
    for (auto* sfx : { "a", "d", "s", "r" })
        if (auto* p = param (sfx)) p->setValueNotifyingHost (p->getDefaultValue());
    repaint();
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

void StepLaneEditor::transform (int op)
{
    auto& st = proc.synth.steps;
    const int len = length();
    float v[32]; bool lk[32];
    for (int s2 = 0; s2 < 32; ++s2) { v[s2] = st.seq[(size_t) (laneIndex * 32 + s2)].load(); lk[s2] = st.seqLock[(size_t) (laneIndex * 32 + s2)].load(); }
    float o[32]; bool ol[32];
    for (int s2 = 0; s2 < 32; ++s2) { o[s2] = v[s2]; ol[s2] = lk[s2]; }
    for (int s2 = 0; s2 < len; ++s2)
    {
        int src = s2;
        switch (op)
        {
            case Reverse:     src = len - 1 - s2; break;
            case Mirror:      src = s2 < (len + 1) / 2 ? s2 : len - 1 - s2; break;     // palindrome of the first half
            case RotateLeft:  src = (s2 + 1) % len; break;
            case RotateRight: src = (s2 + len - 1) % len; break;
            case Double:      src = (s2 * 2) % len; break;                             // twice as fast
            case Half:        src = s2 / 2; break;                                     // twice as slow
            default: break;
        }
        o[s2] = op == Invert ? 1.0f - v[s2] : v[src];
        ol[s2] = op == Invert ? lk[s2] : lk[src];
    }
    for (int s2 = 0; s2 < len; ++s2) { st.seq[(size_t) (laneIndex * 32 + s2)].store (o[s2]); st.seqLock[(size_t) (laneIndex * 32 + s2)].store (ol[s2]); }
    repaint();
}

namespace { float laneClip[32]; bool laneClipLock[32]; bool laneClipFull = false; }

void StepLaneEditor::copyLane()
{
    for (int s2 = 0; s2 < 32; ++s2)
    {
        laneClip[s2] = proc.synth.steps.seq[(size_t) (laneIndex * 32 + s2)].load();
        laneClipLock[s2] = proc.synth.steps.seqLock[(size_t) (laneIndex * 32 + s2)].load();
    }
    laneClipFull = true;
}

bool StepLaneEditor::canPaste() { return laneClipFull; }

void StepLaneEditor::pasteLane()
{
    if (! laneClipFull) return;
    for (int s2 = 0; s2 < 32; ++s2)
    {
        proc.synth.steps.seq[(size_t) (laneIndex * 32 + s2)].store (laneClip[s2]);
        proc.synth.steps.seqLock[(size_t) (laneIndex * 32 + s2)].store (laneClipLock[s2]);
    }
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
static const char* arpRowNames[] = { "ON", "PITCH", "VEL", "PROB", "GATE", "TIE", "RATCHET" };

Rectangle<float> ArpGrid::row (int r) const
{
    auto b = getLocalBounds().toFloat().withTrimmedLeft (70.0f);
    const float heights[] = { 0.09f, 0.27f, 0.18f, 0.15f, 0.15f, 0.08f, 0.08f };
    float y = b.getY();
    for (int i = 0; i < r; ++i) y += heights[i] * b.getHeight();
    return { b.getX(), y, b.getWidth(), heights[r] * b.getHeight() };
}

void ArpGrid::paint (Graphics& g)
{
    auto& st = proc.synth.steps;
    const int len = jlimit (1, 32, (int) proc.paramCache()[pg::arp_len]);
    const int play = st.arpPlayStep.load();
    for (int r = 0; r < 7; ++r)
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
            const Colour rowCol[] = { cosmos::neon, cosmos::orchid, cosmos::electric, cosmos::teal, cosmos::azure, cosmos::fuchsia, cosmos::gold };
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
                case 3:
                {
                    const float pv = st.arpProb[(size_t) s].load();
                    g.fillRoundedRectangle (cell.withTrimmedTop (cell.getHeight() * (1.0f - pv)), 2.0f);
                    if (pv < 0.995f && cell.getWidth() > 18.0f)
                    {
                        g.setColour (colours::text.withAlpha (0.8f)); g.setFont (fonts::sans (8.5f));
                        g.drawText (String (roundToInt (pv * 100.0f)), cell, Justification::centredTop);
                    }
                    break;
                }
                case 4:
                {
                    // gate length drawn as a bar growing to the right inside the step
                    const float gv = st.arpGate[(size_t) s].load();
                    g.fillRoundedRectangle (cell.reduced (0.0f, cell.getHeight() * 0.3f).withWidth (cell.getWidth() * gv), 2.0f);
                    break;
                }
                case 5: if (st.arpTie[(size_t) s].load()) g.fillRoundedRectangle (cell.reduced (2.0f, 4.0f), 2.0f); break;
                case 6:
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
        for (int r = 0; r < 7; ++r) if (row (r).contains (p)) dragRow = r;
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
        case 3: st.arpProb[(size_t) s].store (jlimit (0.0f, 1.0f, std::round (v * 20.0f) / 20.0f)); break;
        case 4:
        {
            const float cw = rr.getWidth() / 32.0f;
            const float within = (p.x - (rr.getX() + (float) s * cw)) / cw;
            st.arpGate[(size_t) s].store (jlimit (0.05f, 1.0f, click ? within : jmax (v, within)));
            break;
        }
        case 5: if (click) st.arpTie[(size_t) s].store (! st.arpTie[(size_t) s].load()); break;
        case 6: if (click) st.arpRatchet[(size_t) s].store (st.arpRatchet[(size_t) s].load() % 4 + 1); break;
        default: break;
    }
    repaint();
}

void ArpGrid::mouseDown (const MouseEvent& e) { edit (e.position, true); }
void ArpGrid::mouseDrag (const MouseEvent& e) { edit (e.position, false); }

// =============================================================================
// OrbPad
// =============================================================================

// =============================================================================
// MatrixRow
// =============================================================================
MatrixRow::MatrixRow (APVTS& state, int s) : slot (s)
{
    const String n = String (s + 1).paddedLeft ('0', 2);
    for (auto* c : { &src, &dst, &via, &pol }) addAndMakeVisible (*c);
    pol.addItemList (pg::choices::modPolarity(), 1);
    pol.setTooltip ("Native keeps the source as it is; Unipolar = 0..1, Bipolar = -1..1");
    src.addItemList (pg::choices::modSources(), 1);
    via.addItemList (pg::choices::modSources(), 1);
    dst.addItemList (pg::choices::modDests(), 1);
    amt.setSliderStyle (Slider::LinearHorizontal);
    amt.setTextBoxStyle (Slider::TextBoxRight, false, 48, 20);
    addAndMakeVisible (amt);
    srcA = std::make_unique<APVTS::ComboBoxAttachment> (state, "mtx" + n + "_src", src);
    dstA = std::make_unique<APVTS::ComboBoxAttachment> (state, "mtx" + n + "_dst", dst);
    viaA = std::make_unique<APVTS::ComboBoxAttachment> (state, "mtx" + n + "_via", via);
    polA = std::make_unique<APVTS::ComboBoxAttachment> (state, "mtx" + n + "_pol", pol);
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
    src.setBounds (r.removeFromLeft (w * 19 / 100).reduced (3, 0));
    r.removeFromLeft (4);
    pol.setBounds (r.removeFromLeft (w * 11 / 100).reduced (3, 0));
    r.removeFromLeft (4);
    dst.setBounds (r.removeFromLeft (w * 25 / 100).reduced (3, 0));
    r.removeFromLeft (4);
    via.setBounds (r.removeFromRight (w * 16 / 100).reduced (3, 0));
    amt.setBounds (r.reduced (6, 0));
}
} // namespace pg::ui

namespace pg::ui
{
using namespace juce;

AboutOverlay::AboutOverlay()
{
    banner  = ImageCache::getFromMemory (BinaryData::bannerarch_jpg, BinaryData::bannerarch_jpgSize);
    machine = ImageCache::getFromMemory (BinaryData::aboutmachine_jpg, BinaryData::aboutmachine_jpgSize);
    serpent = ImageCache::getFromMemory (BinaryData::artserpent_jpg, BinaryData::artserpent_jpgSize);
    isologo = ImageCache::getFromMemory (BinaryData::isologo_png, BinaryData::isologo_pngSize);
}

void AboutOverlay::paint (Graphics& g)
{
    g.fillAll (cosmos::voidC.withAlpha (0.86f));
    auto card = getLocalBounds().toFloat().withSizeKeepingCentre (jmin (880.0f, (float) getWidth() - 40.0f),
                                                                  jmin (600.0f, (float) getHeight() - 40.0f));
    auto cover = [&g] (const Image& img, Rectangle<float> r)
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r.toNearestInt());
        g.drawImage (img, r, RectanglePlacement::centred | RectanglePlacement::fillDestination);
    };
    auto inner = brand::chamferFrame (g, card, 26.0f, 7.0f, cosmos::voidC, cosmos::orchid, true).reduced (14.0f);

    auto top = inner.removeFromTop (inner.getHeight() * 0.36f);
    cover (banner, top);
    brand::checker (g, inner.removeFromTop (8.0f), 4.0f);
    inner.removeFromTop (12.0f);

    const float side = jmax (120.0f, (inner.getWidth() - 300.0f) * 0.5f);
    auto left = inner.removeFromLeft (side);
    cover (machine, left);
    auto right = inner.removeFromRight (side).withTrimmedTop (0.0f);
    cover (serpent, right);

    auto mid = inner.reduced (18.0f, 0.0f);
    auto logoR = mid.removeFromTop (74.0f).withSizeKeepingCentre (70.0f, 70.0f);
    g.drawImage (isologo, logoR, RectanglePlacement::centred);   // official isologo file
    mid.removeFromTop (8.0f);
    g.setColour (cosmos::starlight);
    g.setFont (fonts::display (17.0f));
    g.drawText ("SYNTHGULARITY", mid.removeFromTop (26.0f), Justification::centred);
    g.setColour (cosmos::gold);
    g.setFont (fonts::label (10.5f));
   #ifdef JucePlugin_VersionString
    const String ver ("v" JucePlugin_VersionString);
   #else
    const String ver ("dev");
   #endif
    g.drawText ("BY CALABI YAU  -  " + ver, mid.removeFromTop (18.0f), Justification::centred);
    mid.removeFromTop (12.0f);
    g.setColour (cosmos::cream);
    g.setFont (fonts::sans (11.0f));
    g.drawFittedText ("Granular, wavetable and Mutable Instruments engines in one voice.\n\n"
                      "Plaits, Rings and Clouds DSP by Emilie Gillet (MIT).\n"
                      "JUCE, CLAP extensions, Syncopate and Montserrat fonts.\n\n"
                      "Click anywhere to close.",
                      mid.toNearestInt(), Justification::centredTop, 10);
}
} // namespace pg::ui
