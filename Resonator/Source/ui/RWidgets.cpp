#include "RWidgets.h"
#include "../RProcessor.h"
#include "../dsp/CYMath.h"
#include <algorithm>

namespace cyr::ui
{
using namespace juce;
using namespace pg::ui;

Colour sourceColour (int s)
{
    switch (s)
    {
        case MS_Env1: return cosmos::gold;
        case MS_Env2: return Colour (0xffff8a3d);
        case MS_Env3: return cosmos::cream;
        case MS_Env4: return Colour (0xffff5c8a);
        case MS_Lfo1: return cosmos::neon;
        case MS_Lfo2: return cosmos::teal;
        case MS_Lfo3: return cosmos::azure;
        case MS_Lfo4: return cosmos::orchid;
        case MS_Lfo5: return cosmos::fuchsia;
        case MS_Lfo6: return cosmos::jade;
        case MS_AmpEnv: return cosmos::starlight;
        case MS_Velocity: return Colour (0xff9fd7ff);
        case MS_Key: return Colour (0xffb7a6ff);
        case MS_ModWheel: return Colour (0xff7ef0d0);
        case MS_Aftertouch: return Colour (0xffffc7f0);
        case MS_PitchBend: return Colour (0xffd6e685);
        case MS_Random: return Colour (0xffc9c9c9);
        default: return cosmos::steel;
    }
}

String sourceShort (int s)
{
    static const char* n[] = { "-", "ENV 1", "ENV 2", "ENV 3", "ENV 4", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6",
                               "AMP", "VEL", "KEY", "WHEEL", "AFTER", "BEND", "RAND" };
    return s >= 0 && s < MS_Count ? n[s] : "-";
}

// =============================================================================
// ModKnob
// =============================================================================
ModKnob::ModKnob (ResonatorProcessor& p, const String& paramId, const String& label, Colour acc)
    : proc (p), id (paramId), caption (label), accent (acc)
{
    rid = indexOfR (paramId);
    modulatable = rid >= 0 && destIndexOfParam (rid) > 0;
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.setColour (Slider::rotarySliderFillColourId, acc);
    if (auto* prm = proc.apvts.getParameter (paramId))
    {
        slider.setTooltip (prm->getName (64) + (modulatable ? "  -  drop a modulation source here, right-click to edit" : ""));
        att = std::make_unique<APVTS::SliderAttachment> (proc.apvts, paramId, slider);
        slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }
    slider.onMenu = [this] { showMenu(); };
    addAndMakeVisible (slider);
}

void ModKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (15);
    const int s = jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
}

void ModKnob::paint (Graphics& g)
{
    auto r = getLocalBounds();
    const bool hot = slider.isMouseOverOrDragging();
    g.setFont (fonts::sans (10.5f));
    g.setColour (hot ? colours::text : colours::textDim);
    const auto text = hot ? slider.getTextFromValue (slider.getValue()) : caption;
    g.drawFittedText (text, r.removeFromBottom (15), Justification::centred, 1, 0.7f);
}

void ModKnob::paintOverChildren (Graphics& g)
{
    const auto b = slider.getBounds().toFloat().reduced (1.0f);
    const auto c = b.getCentre();
    const float rad = jmin (b.getWidth(), b.getHeight()) * 0.5f + 1.5f;
    const auto rp = slider.getRotaryParameters();
    const float a0 = rp.startAngleRadians, a1 = rp.endAngleRadians;
    auto angleAt = [&] (float n) { return a0 + jlimit (0.0f, 1.0f, n) * (a1 - a0); };

    // drag in progress: knobs that accept the source glow
    auto* dc = modulatable ? DragAndDropContainer::findParentDragContainerFor (this) : nullptr;
    if (dc != nullptr && dc->isDragAndDropActive())
        {
            const auto desc = dc->getCurrentDragDescription().toString();
            const int s = desc.fromFirstOccurrenceOf ("mod:", false, false).getIntValue();
            const auto col = sourceColour (s);
            g.setColour (col.withAlpha (dropHover ? 0.55f : 0.18f));
            g.drawEllipse (Rectangle<float> (rad * 2 + 6, rad * 2 + 6).withCentre (c), dropHover ? 3.0f : 1.5f);
        }
    if (! modulatable) return;
    const auto slots = proc.slotsFor (rid);
    if (slots.isEmpty()) return;
    auto* prm = proc.apvts.getParameter (id);
    const float base = prm != nullptr ? prm->getValue() : 0.0f;
    float ringR = rad + 3.0f;
    for (int s : slots)
    {
        const int src = (int) proc.value (slotSrc (s));
        const float amt = proc.value (slotAmt (s));
        const bool bip = isBipolarSource (src);
        const float lo = bip ? base - std::abs (amt) : jmin (base, base + amt);
        const float hi = bip ? base + std::abs (amt) : jmax (base, base + amt);
        Path arc;
        arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, angleAt (lo), angleAt (hi), true);
        g.setColour (sourceColour (src).withAlpha (0.9f));
        g.strokePath (arc, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        ringR += 3.2f;
    }
    // live value of the newest voice
    const int d = destIndexOfParam (rid);
    const float live = d > 0 && d < (int) proc.synth.liveNorm.size() ? proc.synth.liveNorm[(size_t) d].load() : -1.0f;
    if (live >= 0.0f)
    {
        const float a = angleAt (live);
        const Point<float> pt (c.x + std::sin (a) * (rad + 3.0f), c.y - std::cos (a) * (rad + 3.0f));
        g.setColour (cosmos::starlight);
        g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (pt));
        g.setColour (sourceColour ((int) proc.value (slotSrc (slots[0]))).withAlpha (0.35f));
        g.fillEllipse (Rectangle<float> (11.0f, 11.0f).withCentre (pt));
    }
}

bool ModKnob::isInterestedInDragSource (const SourceDetails& d)
{
    return modulatable && d.description.toString().startsWith ("mod:");
}

void ModKnob::itemDropped (const SourceDetails& d)
{
    dropHover = false;
    const int src = d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue();
    proc.addModulation (src, rid, isBipolarSource (src) ? 0.25f : 0.35f);
    repaint();
}

void ModKnob::showMenu()
{
    PopupMenu m;
    m.addSectionHeader (caption);
    m.addItem (1, "Reset to default");
    const auto slots = proc.slotsFor (rid);
    if (modulatable)
    {
        if (slots.isEmpty()) m.addItem (2, "Drag a source chip (ENV, LFO...) onto this knob to modulate it", false, false);
        for (int i = 0; i < slots.size(); ++i)
        {
            const int s = slots[i];
            const int src = (int) proc.value (slotSrc (s));
            const float amt = proc.value (slotAmt (s));
            PopupMenu sub;
            for (int k = 0; k < 9; ++k)
            {
                static const float v[] = { -1.0f, -0.5f, -0.25f, -0.1f, 0.1f, 0.25f, 0.5f, 0.75f, 1.0f };
                sub.addItem (100 + i * 20 + k, (v[k] > 0 ? "+" : "") + String (roundToInt (v[k] * 100)) + " %", true, std::abs (v[k] - amt) < 0.01f);
            }
            sub.addSeparator();
            sub.addItem (100 + i * 20 + 10, "Invert");
            sub.addItem (100 + i * 20 + 11, "Remove");
            m.addSubMenu (sourceShort (src) + "   " + (amt >= 0 ? "+" : "") + String (roundToInt (amt * 100)) + " %", sub);
        }
    }
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this, slots] (int r)
    {
        if (r == 1) { if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (p->getDefaultValue()); }
        else if (r >= 100)
        {
            const int i = (r - 100) / 20, k = (r - 100) % 20;
            if (i >= slots.size()) return;
            static const float v[] = { -1.0f, -0.5f, -0.25f, -0.1f, 0.1f, 0.25f, 0.5f, 0.75f, 1.0f };
            if (k < 9) proc.setModAmount (slots[i], v[k]);
            else if (k == 10) proc.setModAmount (slots[i], -proc.value (slotAmt (slots[i])));
            else if (k == 11) proc.removeModulation (slots[i]);
        }
        repaint();
    });
}

// =============================================================================
// Switch
// =============================================================================
Switch::Switch (APVTS& st, const String& paramId, const String& label)
{
    button.setButtonText (label);
    addAndMakeVisible (button);
    att = std::make_unique<APVTS::ButtonAttachment> (st, paramId, button);
}

// =============================================================================
// SourceChip
// =============================================================================
SourceChip::SourceChip (ResonatorProcessor& p, int source) : proc (p), src (source)
{
    setTooltip ("Drag " + sourceShort (source) + " onto any oscillator, matrix, filter or LFO knob to modulate it");
    setMouseCursor (MouseCursor::DraggingHandCursor);
}

void SourceChip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f, 3.0f);
    const auto col = sourceColour (src);
    const float v = proc.synth.liveSrc[(size_t) src].load();
    const float act = jlimit (0.0f, 1.0f, std::abs (v));
    g.setColour (cosmos::night.withAlpha (0.92f));
    g.fillRoundedRectangle (r, 6.0f);
    // live level fill
    g.setColour (col.withAlpha (0.16f + 0.3f * act));
    auto fill = r.reduced (1.5f);
    g.fillRoundedRectangle (isBipolarSource (src) ? fill.withWidth (fill.getWidth() * (0.5f + 0.5f * jlimit (-1.0f, 1.0f, v))) : fill.withWidth (fill.getWidth() * act), 5.0f);
    g.setColour (col.withAlpha (isMouseOver() ? 1.0f : 0.7f));
    g.drawRoundedRectangle (r, 6.0f, isMouseOver() ? 1.8f : 1.1f);
    // grip dots
    for (int k = 0; k < 3; ++k)
        g.fillEllipse (r.getX() + 6.0f, r.getCentreY() - 5.0f + (float) k * 4.0f, 2.2f, 2.2f);
    g.setColour (cosmos::starlight);
    g.setFont (fonts::label (9.5f));
    g.drawFittedText (sourceShort (src), r.withTrimmedLeft (10.0f).toNearestInt(), Justification::centred, 1, 0.7f);
}

void SourceChip::mouseDrag (const MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 4) return;
    if (auto* dc = DragAndDropContainer::findParentDragContainerFor (this))
        if (! dc->isDragAndDropActive())
        {
            Image img (Image::ARGB, getWidth(), getHeight(), true);
            Graphics g (img);
            paint (g);
            dc->startDragging ("mod:" + String (src), this, ScaledImage (img), true);
        }
}

// =============================================================================
// SurfaceView
// =============================================================================
static Colour patchColour (int k1, int k2, int n)
{
    static const Colour stops[] = { cosmos::azure, cosmos::electric, cosmos::fuchsia, cosmos::orchid, cosmos::teal, cosmos::neon };
    const float t = ((float) k1 + (float) k2 / (float) n) / (float) n * 5.0f;
    const int i = jmin (4, (int) t);
    return stops[i].interpolatedWith (stops[i + 1], t - (float) i);
}

void SurfaceView::rebuild (int n)
{
    builtN = n;
    // keep the quad count moderate so four views stay light
    grid = jlimit (3, 10, (int) std::floor (std::sqrt (620.0 / (double) (n * n))));
    const int S = grid + 1;
    verts.assign ((size_t) (n * n * S * S * 4), 0.0f);
    size_t o = 0;
    for (int k1 = 0; k1 < n; ++k1)
        for (int k2 = 0; k2 < n; ++k2)
            for (int j = 0; j <= grid; ++j)
                for (int i = 0; i <= grid; ++i)
                {
                    const double u = (double) i / grid * 1.5707963, v = ((double) j / grid - 0.5) * 3.14159265;
                    // real and imaginary parts separately, so alpha can change without rebuilding
                    float a[3], b[3];
                    surfacePoint (u, v, k1, k2, n, 0.0, a);                   // Re z1, Re z2, Im z1
                    surfacePoint (u, v, k1, k2, n, 1.5707963267948966, b);    // ..., Im z2
                    verts[o++] = a[0]; verts[o++] = a[1]; verts[o++] = a[2]; verts[o++] = b[2];
                }
}

void SurfaceView::tick (double t)
{
    time = t;
    if (++frameSkip >= 2) { frameSkip = 0; repaint(); }   // ~15 fps
}

void SurfaceView::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const int b = o * kOscStride;
    const bool on = proc.value (osca_on + b) > 0.5f;
    const int n = jlimit (2, 7, (int) std::lround (proc.value (osca_n + b)));
    if (n != builtN) rebuild (n);
    const int k1 = (int) std::lround (proc.value (osca_k1 + b)) % n, k2 = (int) std::lround (proc.value (osca_k2 + b)) % n;
    float alpha = proc.value (osca_alpha + b);
    const int dA = destIndexOfParam (osca_alpha + b);
    const float liveA = proc.synth.liveNorm[(size_t) dA].load();
    if (liveA >= 0.0f) alpha = ranges()[(size_t) (osca_alpha + b)].convertFrom0to1 (liveA);
    const float ca = std::cos (alpha), sa = std::sin (alpha);

    g.setColour (cosmos::voidC.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds, 8.0f);
    const float W = bounds.getWidth(), H = bounds.getHeight();
    const float yaw = (float) time * 0.22f + (float) o * 1.3f, pitch = 0.38f;
    const float cy = std::cos (yaw), sy = std::sin (yaw), cp = std::cos (pitch), sp = std::sin (pitch);
    const float scale = jmin (W, H) * 0.24f;
    auto project = [&] (float x, float y, float z, float& px, float& py, float& depth)
    {
        const float X = x * cy - y * sy, Y = x * sy + y * cy;
        const float Yc = Y * cp - z * sp, Zc = Y * sp + z * cp;
        const float d = 7.0f, s = scale * d / (d + Yc);
        px = bounds.getCentreX() + X * s;
        py = bounds.getCentreY() - Zc * s;
        depth = Yc;
    };
    const int S = grid + 1, NP = n * n, NV = NP * S * S;
    std::vector<float> P ((size_t) NV * 3);
    for (int i = 0; i < NV; ++i)
    {
        const float* v = &verts[(size_t) i * 4];
        project (v[0], v[1], v[2] * ca + v[3] * sa, P[(size_t) i * 3], P[(size_t) i * 3 + 1], P[(size_t) i * 3 + 2]);
    }
    struct Q { float depth; int idx; };
    std::vector<Q> quads;
    quads.reserve ((size_t) (NP * grid * grid));
    for (int p = 0; p < NP; ++p)
        for (int j = 0; j < grid; ++j)
            for (int i = 0; i < grid; ++i)
            {
                const int a = p * S * S + j * S + i;
                quads.push_back ({ P[(size_t) a * 3 + 2] + P[(size_t) (a + S + 1) * 3 + 2], p * grid * grid + j * grid + i });
            }
    std::sort (quads.begin(), quads.end(), [] (const Q& x, const Q& y) { return x.depth > y.depth; });
    std::vector<Colour> cols;
    for (int a1 = 0; a1 < n; ++a1) for (int a2 = 0; a2 < n; ++a2) cols.push_back (patchColour (a1, a2, n));
    const int active = k1 * n + k2;
    const float ref = std::pow (scale * 1.6f / (float) grid, 2.0f);
    for (auto& q : quads)
    {
        const int p = q.idx / (grid * grid), rem = q.idx % (grid * grid), j = rem / grid, i = rem % grid;
        const int a = p * S * S + j * S + i, bb = a + 1, c = a + S + 1, d = a + S;
        auto pt = [&] (int k) { return Point<float> (P[(size_t) k * 3], P[(size_t) k * 3 + 1]); };
        const auto A = pt (a), B = pt (bb), C = pt (c), D = pt (d);
        const float cross = (C.x - A.x) * (D.y - B.y) - (C.y - A.y) * (D.x - B.x);
        const float lit = 0.4f + 0.6f * jmin (1.0f, std::abs (cross) * 0.5f / ref);
        const bool hot = p == active;
        Path quad;
        quad.startNewSubPath (A); quad.lineTo (B); quad.lineTo (C); quad.lineTo (D); quad.closeSubPath();
        auto col = cols[(size_t) p].withMultipliedBrightness (lit * (hot ? 1.15f : 0.55f));
        g.setColour (col.withAlpha (hot ? 0.95f : 0.8f));
        g.fillPath (quad);
        g.setColour (hot ? cosmos::gold.withAlpha (0.35f) : cosmos::voidC.withAlpha (0.4f));
        g.strokePath (quad, PathStrokeType (0.5f));
    }
    // orbit of the note on the active patch
    const float ru = proc.value (osca_ru + b), rv = proc.value (osca_rv + b);
    const int ratio = jlimit (1, 7, (int) std::lround (proc.value (osca_ratio + b)));
    Path orbit;
    float xyz[3], px, py, dep;
    for (int s = 0; s <= 120; ++s)
    {
        const double ph = (double) s / 120.0;
        const double u = 0.78539816 + ru * 0.78539816 * std::cos (6.283185307 * ph);
        const double v = rv * 1.5707963 * std::sin (ratio * 6.283185307 * ph);
        surfacePoint (u, v, k1, k2, n, alpha, xyz);
        project (xyz[0], xyz[1], xyz[2], px, py, dep);
        if (s == 0) orbit.startNewSubPath (px, py); else orbit.lineTo (px, py);
    }
    g.setColour (cosmos::gold.withAlpha (0.25f));
    g.strokePath (orbit, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (cosmos::gold);
    g.strokePath (orbit, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    // travelling point (slowed down so the eye can follow it); brighter while a voice sounds
    {
        const double ph = std::fmod (time * 0.35, 1.0);
        const double u = 0.78539816 + ru * 0.78539816 * std::cos (6.283185307 * ph);
        const double v = rv * 1.5707963 * std::sin (ratio * 6.283185307 * ph);
        surfacePoint (u, v, k1, k2, n, alpha, xyz);
        project (xyz[0], xyz[1], xyz[2], px, py, dep);
        const bool sounding = proc.synth.activeVoices.load() > 0;
        g.setColour (cosmos::neon.withAlpha (sounding ? 0.45f : 0.2f));
        g.fillEllipse (Rectangle<float> (16.0f, 16.0f).withCentre ({ px, py }));
        g.setColour (cosmos::starlight);
        g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre ({ px, py }));
    }
    g.setColour (colours::textDim);
    g.setFont (fonts::sans (10.0f));
    g.drawText ("n=" + String (n) + "  patch " + String (k1) + "," + String (k2), bounds.reduced (8.0f, 5.0f), Justification::bottomLeft);
    if (! on)
    {
        g.setColour (cosmos::voidC.withAlpha (0.6f));
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (colours::textDim);
        g.setFont (fonts::label (12.0f));
        g.drawText ("OFF", bounds, Justification::centred);
    }
}

// =============================================================================
// MatrixView
// =============================================================================
void MatrixView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (10.0f);
    auto inner = brand::chamferFrame (g, r, 14.0f, 4.0f, cosmos::voidC.withAlpha (0.85f), cosmos::nebula, false).reduced (14.0f);
    g.setColour (colours::textDim);
    g.setFont (fonts::label (9.5f));
    g.drawText ("SIGNAL MAP", inner.removeFromTop (14.0f), Justification::centredLeft);
    auto legend = inner.removeFromBottom (16.0f);
    g.setColour (cosmos::gold); g.drawText ("FM  phase modulation", legend.removeFromLeft (legend.getWidth() / 2), Justification::centredLeft);
    g.setColour (cosmos::orchid); g.drawText ("PD  phase distortion", legend, Justification::centredRight);

    const float R = jmin (inner.getWidth(), inner.getHeight()) * 0.34f;
    const auto c = inner.getCentre();
    Point<float> node[4];
    for (int i = 0; i < 4; ++i)
    {
        const float a = -2.356f + (float) i * 1.5708f;   // A top-left, clockwise
        node[i] = { c.x + std::cos (a) * R * 1.25f, c.y + std::sin (a) * R };
    }
    const float t = (float) time;
    for (int pass = 0; pass < 2; ++pass)
        for (int s = 0; s < 4; ++s)
            for (int d = 0; d < 4; ++d)
            {
                const float amt = proc.value ((pass == 0 ? fm_aa : pd_aa) + s * 4 + d);
                if (amt < 0.005f) continue;
                const auto col = pass == 0 ? cosmos::gold : cosmos::orchid;
                Path p;
                if (s == d)
                {
                    // feedback loop
                    const auto dir = (node[s] - c) / jmax (1.0f, node[s].getDistanceFrom (c));
                    const auto o2 = node[s] + dir * (34.0f + (float) pass * 10.0f);
                    p.addEllipse (Rectangle<float> (22.0f + (float) pass * 8.0f, 22.0f + (float) pass * 8.0f).withCentre (o2));
                }
                else
                {
                    const auto a = node[s], b = node[d];
                    const auto mid = (a + b) * 0.5f;
                    const auto nrm = Point<float> (-(b.y - a.y), b.x - a.x) / jmax (1.0f, a.getDistanceFrom (b));
                    const auto ctl = mid + nrm * (pass == 0 ? 22.0f : -22.0f);
                    p.startNewSubPath (a);
                    p.quadraticTo (ctl, b);
                }
                g.setColour (col.withAlpha (0.25f + 0.55f * amt));
                g.strokePath (p, PathStrokeType (1.0f + amt * 3.5f, PathStrokeType::curved, PathStrokeType::rounded));
                // flowing dots, faster with more amount
                const float len = p.getLength();
                for (int k = 0; k < 3; ++k)
                {
                    float f = std::fmod (t * (0.25f + amt) + (float) k / 3.0f, 1.0f);
                    const auto pt = p.getPointAlongPath (f * len);
                    g.setColour (col);
                    g.fillEllipse (Rectangle<float> (4.0f + amt * 3.0f, 4.0f + amt * 3.0f).withCentre (pt));
                }
            }
    static const char* names[] = { "A", "B", "C", "D" };
    for (int i = 0; i < 4; ++i)
    {
        const bool on = proc.value (osca_on + i * kOscStride) > 0.5f;
        auto box = Rectangle<float> (46.0f, 46.0f).withCentre (node[i]);
        const float pulse = 0.5f + 0.5f * std::sin (t * 2.0f + (float) i);
        if (on)
        {
            g.setColour (cosmos::neon.withAlpha (0.12f + 0.12f * pulse));
            g.fillEllipse (box.expanded (8.0f));
        }
        g.setColour (on ? cosmos::deepSpace : cosmos::night);
        g.fillEllipse (box);
        g.setColour (on ? cosmos::neon : cosmos::steel.withAlpha (0.5f));
        g.drawEllipse (box, 2.0f);
        g.setColour (on ? cosmos::starlight : cosmos::steel);
        g.setFont (fonts::display (15.0f));
        g.drawText (names[i], box, Justification::centred);
    }
}

// =============================================================================
// LfoView
// =============================================================================
void LfoView::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (cosmos::voidC.withAlpha (0.8f));
    g.fillRoundedRectangle (r, 6.0f);
    r = r.reduced (6.0f, 8.0f);
    const int shape = (int) proc.value (lfo1_shape + l * kLfoStride);
    const auto col = sourceColour (MS_Lfo1 + l);
    pg::Lfo shapeLfo;
    shapeLfo.prevRand = -0.6f; shapeLfo.nextRand = 0.7f; shapeLfo.held = 0.4f;
    Path p;
    for (int i = 0; i <= 100; ++i)
    {
        const float ph = (float) i / 100.0f;
        float v = shapeLfo.shapeAt (shape, ph);
        if (shape == 5) v = ph < 0.25f ? 0.4f : (ph < 0.5f ? -0.7f : (ph < 0.75f ? 0.9f : -0.2f));
        const float x = r.getX() + ph * r.getWidth(), y = r.getCentreY() - v * r.getHeight() * 0.45f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (colours::outline);
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
    g.setColour (col.withAlpha (0.85f));
    g.strokePath (p, PathStrokeType (1.6f));
    const float v = proc.synth.liveSrc[(size_t) (MS_Lfo1 + l)].load();
    const float y = r.getCentreY() - jlimit (-1.0f, 1.0f, v) * r.getHeight() * 0.45f;
    g.setColour (col.withAlpha (0.35f));
    g.fillEllipse (Rectangle<float> (14.0f, 14.0f).withCentre ({ r.getRight() - 6.0f, y }));
    g.setColour (cosmos::starlight);
    g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre ({ r.getRight() - 6.0f, y }));
}

// =============================================================================
// AnalyzerStrip
// =============================================================================
AnalyzerStrip::AnalyzerStrip (ResonatorProcessor& p) : proc (p)
{
    for (int i = 0; i < kSize; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (pg::kTwoPi * (float) i / (float) (kSize - 1));
    bands.fill (-100.0f);
    peaks.fill (-100.0f);
    setTooltip ("Oscilloscope, FFT spectrum, goniometer (stereo image, correlation) and true peak / LUFS. Click the meters to reset.");
    startTimerHz (30);
}

void AnalyzerStrip::resized()
{
    auto r = getLocalBounds().reduced (6, 4);
    meterA = r.removeFromRight (170);
    r.removeFromRight (6);
    gonioA = r.removeFromRight (r.getHeight() + 10);
    r.removeFromRight (6);
    scopeA = r.removeFromLeft (r.getWidth() * 2 / 5);
    r.removeFromLeft (6);
    fftA = r;
}

void AnalyzerStrip::mouseDown (const MouseEvent& e)
{
    if (meterA.contains (e.getPosition())) proc.meter.requestReset();
}

void AnalyzerStrip::timerCallback()
{
    auto& m = proc.meter;
    tpL = m.tpL.load(); tpR = m.tpR.load(); tpMax = m.tpMax.load();
    lm = m.lufsM.load(); ls = m.lufsS.load(); li = m.lufsI.load();
    const int N = ResonatorProcessor::kScopeRing, mask = N - 1;
    const int wp = proc.scopePos.load (std::memory_order_acquire);
    auto mono = [&] (int k) { return 0.5f * (proc.scopeL[(size_t) (k & mask)] + proc.scopeR[(size_t) (k & mask)]); };
    // scope: rising zero crossing, ~25 ms
    {
        const int span = (int) (m.sampleRate() * 0.025);
        int start = wp - span - 4;
        for (int k = wp - span - 4; k > wp - 4000; --k)
            if (mono (k - 1) <= 0.0f && mono (k) > 0.0f) { start = k; break; }
        traceHead = (traceHead + 2) % 3;
        auto& t = traces[(size_t) traceHead];
        for (int i = 0; i < kScope; ++i) t[(size_t) i] = mono (start + i * span / kScope);
    }
    // FFT
    for (int i = 0; i < kSize; ++i) fftData[(size_t) i] = mono (wp - kSize + i) * window[(size_t) i];
    std::fill (fftData.begin() + kSize, fftData.end(), 0.0f);
    fft.performFrequencyOnlyForwardTransform (fftData.data());
    const float sr = (float) m.sampleRate();
    for (int b = 0; b < kBands; ++b)
    {
        const float f0 = 20.0f * std::pow (1000.0f, (float) b / kBands), f1 = 20.0f * std::pow (1000.0f, (float) (b + 1) / kBands);
        const int i0 = jlimit (1, kSize / 2 - 1, (int) (f0 / sr * kSize)), i1 = jlimit (i0, kSize / 2 - 1, (int) (f1 / sr * kSize));
        float mx = 0.0f;
        for (int i = i0; i <= i1; ++i) mx = jmax (mx, fftData[(size_t) i]);
        const float db = Decibels::gainToDecibels (mx * 2.0f / (float) kSize * 2.0f, -100.0f) + 4.5f * std::log2 (f0 / 1000.0f) * 0.5f;
        bands[(size_t) b] = jmax (db, bands[(size_t) b] - 2.5f);
        peaks[(size_t) b] = jmax (db, peaks[(size_t) b] - 0.35f);
    }
    // goniometer: last 1024 samples, correlation
    double sLR = 0, sLL = 0, sRR = 0;
    for (int i = 0; i < kGonio; ++i)
    {
        const int k = (wp - kGonio + i) & mask;
        gl[(size_t) i] = proc.scopeL[(size_t) k];
        gr[(size_t) i] = proc.scopeR[(size_t) k];
        sLR += gl[(size_t) i] * gr[(size_t) i]; sLL += gl[(size_t) i] * gl[(size_t) i]; sRR += gr[(size_t) i] * gr[(size_t) i];
    }
    const double den = std::sqrt (sLL * sRR);
    const float c = den > 1e-9 ? (float) (sLR / den) : 0.0f;
    correlation += (c - correlation) * 0.2f;
    repaint();
}

static void panelBg (Graphics& g, Rectangle<int> a, const String& title)
{
    g.setColour (cosmos::voidC.withAlpha (0.85f));
    g.fillRoundedRectangle (a.toFloat(), 7.0f);
    g.setColour (cosmos::nebula);
    g.drawRoundedRectangle (a.toFloat().reduced (0.5f), 7.0f, 1.0f);
    g.setColour (colours::textDim);
    g.setFont (fonts::label (8.5f));
    g.drawText (title, a.reduced (8, 4), Justification::topLeft);
}

void AnalyzerStrip::paint (Graphics& g)
{
    // ---- scope with phosphor persistence ----
    panelBg (g, scopeA, "SCOPE");
    {
        auto r = scopeA.toFloat().reduced (8.0f, 14.0f);
        g.setColour (colours::outline.withAlpha (0.5f));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
        for (int age = 2; age >= 0; --age)
        {
            const auto& t = traces[(size_t) ((traceHead + age) % 3)];
            Path p;
            for (int i = 0; i < kScope; ++i)
            {
                const float x = r.getX() + r.getWidth() * (float) i / (kScope - 1), y = r.getCentreY() - jlimit (-1.2f, 1.2f, t[(size_t) i]) * r.getHeight() * 0.45f;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            g.setColour (cosmos::neon.withAlpha (age == 0 ? 0.95f : 0.25f / (float) age));
            g.strokePath (p, PathStrokeType (age == 0 ? 1.6f : 1.0f));
        }
    }
    // ---- spectrum ----
    panelBg (g, fftA, "SPECTRUM");
    {
        auto r = fftA.toFloat().reduced (8.0f, 14.0f);
        g.setFont (fonts::sans (8.5f));
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = r.getX() + r.getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
            g.setColour (colours::outline.withAlpha (0.45f));
            g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
            g.setColour (colours::textDim);
            g.drawText (f >= 1000.0f ? String ((int) (f / 1000.0f)) + "k" : String ((int) f), (int) x + 2, (int) r.getBottom() - 10, 30, 10, Justification::left);
        }
        auto yOf = [&] (float db) { return r.getBottom() - jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f) * r.getHeight(); };
        Path fill, line, pk;
        fill.startNewSubPath (r.getX(), r.getBottom());
        for (int b = 0; b < kBands; ++b)
        {
            const float x = r.getX() + r.getWidth() * (float) b / (kBands - 1);
            fill.lineTo (x, yOf (bands[(size_t) b]));
            if (b == 0) { line.startNewSubPath (x, yOf (bands[0])); pk.startNewSubPath (x, yOf (peaks[0])); }
            else { line.lineTo (x, yOf (bands[(size_t) b])); pk.lineTo (x, yOf (peaks[(size_t) b])); }
        }
        fill.lineTo (r.getRight(), r.getBottom());
        fill.closeSubPath();
        g.setGradientFill (ColourGradient (cosmos::fuchsia.withAlpha (0.55f), r.getX(), r.getY(), cosmos::electric.withAlpha (0.1f), r.getX(), r.getBottom(), false));
        g.fillPath (fill);
        g.setColour (cosmos::orchid);
        g.strokePath (line, PathStrokeType (1.3f));
        g.setColour (cosmos::gold.withAlpha (0.5f));
        g.strokePath (pk, PathStrokeType (0.8f));
    }
    // ---- goniometer (M/S rotated 45 degrees) ----
    panelBg (g, gonioA, "GONIOMETER");
    {
        auto r = gonioA.toFloat().reduced (8.0f, 14.0f);
        auto sq = r.withSizeKeepingCentre (jmin (r.getWidth(), r.getHeight() - 12.0f), jmin (r.getWidth(), r.getHeight() - 12.0f)).translated (0.0f, -6.0f);
        const auto c = sq.getCentre();
        const float h = sq.getWidth() * 0.5f;
        g.setColour (colours::outline.withAlpha (0.5f));
        g.drawLine (c.x, sq.getY(), c.x, sq.getBottom(), 0.8f);
        g.drawLine (sq.getX(), c.y, sq.getRight(), c.y, 0.8f);
        g.drawLine (c.x - h * 0.7f, c.y - h * 0.7f, c.x + h * 0.7f, c.y + h * 0.7f, 0.6f);
        g.drawLine (c.x + h * 0.7f, c.y - h * 0.7f, c.x - h * 0.7f, c.y + h * 0.7f, 0.6f);
        g.setFont (fonts::sans (8.0f));
        g.setColour (colours::textDim);
        g.drawText ("L", (int) (c.x - h * 0.75f) - 8, (int) (c.y - h * 0.75f) - 8, 10, 10, Justification::centred);
        g.drawText ("R", (int) (c.x + h * 0.75f), (int) (c.y - h * 0.75f) - 8, 10, 10, Justification::centred);
        for (int i = 0; i < kGonio; ++i)
        {
            const float L = gl[(size_t) i], R = gr[(size_t) i];
            const float x = (R - L) * 0.7071f, y = (L + R) * 0.7071f;
            const float px = c.x + jlimit (-1.0f, 1.0f, x) * h, py = c.y - jlimit (-1.0f, 1.0f, y) * h;
            g.setColour (cosmos::neon.withAlpha (0.08f + 0.5f * (float) i / kGonio));
            g.fillRect (px, py, 1.5f, 1.5f);
        }
        // correlation bar
        auto bar = Rectangle<float> (r.getX() + 6.0f, r.getBottom() - 6.0f, r.getWidth() - 12.0f, 4.0f);
        g.setColour (colours::outline);
        g.fillRoundedRectangle (bar, 2.0f);
        const float cx = bar.getCentreX() + correlation * bar.getWidth() * 0.5f;
        g.setColour (correlation < 0.0f ? Colour (0xffff5c5c) : cosmos::neon);
        g.fillEllipse (Rectangle<float> (7.0f, 7.0f).withCentre ({ cx, bar.getCentreY() }));
    }
    // ---- true peak + LUFS ----
    panelBg (g, meterA, "OUTPUT");
    {
        auto r = meterA.toFloat().reduced (8.0f, 16.0f);
        auto bars = r.removeFromLeft (34.0f);
        auto yOf = [&] (float db) { return bars.getBottom() - jlimit (0.0f, 1.0f, (db + 60.0f) / 63.0f) * bars.getHeight(); };
        for (int ch = 0; ch < 2; ++ch)
        {
            auto b = bars.withWidth (12.0f).translated ((float) ch * 16.0f, 0.0f);
            g.setColour (colours::outline);
            g.fillRect (b);
            const float v = ch == 0 ? tpL : tpR;
            const float top = yOf (v);
            g.setGradientFill (ColourGradient (Colour (0xffff5c5c), 0, yOf (0.0f), cosmos::neon, 0, yOf (-24.0f), false));
            g.fillRect (b.withTop (top));
        }
        g.setColour (cosmos::gold);
        g.drawHorizontalLine ((int) yOf (-1.0f), bars.getX(), bars.getRight());
        r.removeFromLeft (8.0f);
        g.setFont (fonts::sans (10.5f));
        auto line = [&] (const String& k, float v, Colour c)
        {
            auto row = r.removeFromTop (r.getHeight() / 4.0f);
            g.setColour (colours::textDim);
            g.drawText (k, row.removeFromLeft (30.0f), Justification::centredLeft);
            g.setColour (c);
            g.drawText (v <= -99.0f ? String ("-inf") : String (v, 1), row, Justification::centredRight);
        };
        line ("TP", tpMax, tpMax > -1.0f ? Colour (0xffff5c5c) : colours::text);
        line ("M", lm, colours::text);
        line ("S", ls, colours::text);
        line ("INT", li, cosmos::neon);
    }
}

// =============================================================================
// ModSlotRow
// =============================================================================
ModSlotRow::ModSlotRow (ResonatorProcessor& p, int s) : proc (p), slot (s)
{
    src.addItemList (choices::modSources(), 1);
    dst.addItemList (choices::modDests(), 1);
    amt.setSliderStyle (Slider::LinearBar);
    amt.setTextValueSuffix ("");
    amt.setColour (Slider::trackColourId, cosmos::electric.withAlpha (0.6f));
    amt.textFromValueFunction = [] (double v) { return (v >= 0 ? "+" : "") + String (roundToInt (v * 100)) + " %"; };
    for (auto* c : std::initializer_list<Component*> { &src, &dst, &amt, &clear }) addAndMakeVisible (c);
    srcA = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, idOf (slotSrc (s)), src);
    dstA = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, idOf (slotDst (s)), dst);
    amtA = std::make_unique<APVTS::SliderAttachment> (proc.apvts, idOf (slotAmt (s)), amt);
    amt.textFromValueFunction = [] (double v) { return (v >= 0 ? "+" : "") + String (roundToInt (v * 100)) + " %"; };
    amt.updateText();
    clear.onClick = [this] { proc.removeModulation (slot); };
    clear.setTooltip ("Remove this modulation");
}

void ModSlotRow::paint (Graphics& g)
{
    const int s = (int) proc.value (slotSrc (slot));
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (s > 0 ? sourceColour (s).withAlpha (0.12f) : cosmos::night.withAlpha (0.5f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (s > 0 ? sourceColour (s) : colours::outline);
    g.fillRoundedRectangle (r.withWidth (4.0f), 2.0f);
    g.setColour (colours::textDim);
    g.setFont (fonts::sans (10.0f));
    g.drawText (String (slot + 1), r.withTrimmedLeft (8.0f).withWidth (22.0f), Justification::centredLeft);
}

void ModSlotRow::resized()
{
    auto r = getLocalBounds().reduced (2);
    r.removeFromLeft (30);
    clear.setBounds (r.removeFromRight (28));
    r.removeFromRight (4);
    src.setBounds (r.removeFromLeft (r.getWidth() * 28 / 100));
    r.removeFromLeft (4);
    dst.setBounds (r.removeFromLeft (r.getWidth() * 62 / 100));
    r.removeFromLeft (4);
    amt.setBounds (r);
}
} // namespace cyr::ui
