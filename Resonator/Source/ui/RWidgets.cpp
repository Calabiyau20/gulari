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
    // the value each knob really has right now (modulation included)
    auto live = [&] (int rid)
    {
        const int d = destIndexOfParam (rid);
        const float ln = d > 0 ? proc.synth.liveNorm[(size_t) d].load() : -1.0f;
        return ln >= 0.0f ? ranges()[(size_t) rid].convertFrom0to1 (ln) : proc.value (rid);
    };
    const bool scanMode = (int) proc.value (osca_mode + b) == 1;
    const auto sc = makeScan (n, live (osca_cover + b), live (osca_pos + b));
    std::vector<char> hotPatch ((size_t) (n * n), 0);
    if (scanMode)
        for (int i = 0; i < sc.count; ++i) { int pp; float vv; scanLine (sc, sc.start + (float) i, pp, vv); hotPatch[(size_t) pp] = 1; }
    else hotPatch[(size_t) (k1 * n + k2)] = 1;
    const float ref = std::pow (scale * 1.6f / (float) grid, 2.0f);
    for (auto& q : quads)
    {
        const int p = q.idx / (grid * grid), rem = q.idx % (grid * grid), j = rem / grid, i = rem % grid;
        const int a = p * S * S + j * S + i, bb = a + 1, c = a + S + 1, d = a + S;
        auto pt = [&] (int k) { return Point<float> (P[(size_t) k * 3], P[(size_t) k * 3 + 1]); };
        const auto A = pt (a), B = pt (bb), C = pt (c), D = pt (d);
        const float cross = (C.x - A.x) * (D.y - B.y) - (C.y - A.y) * (D.x - B.x);
        const float lit = 0.4f + 0.6f * jmin (1.0f, std::abs (cross) * 0.5f / ref);
        const bool hot = hotPatch[(size_t) p] != 0;
        Path quad;
        quad.startNewSubPath (A); quad.lineTo (B); quad.lineTo (C); quad.lineTo (D); quad.closeSubPath();
        auto col = cols[(size_t) p].withMultipliedBrightness (lit * (hot ? 1.15f : 0.55f));
        g.setColour (col.withAlpha (hot ? 0.95f : 0.8f));
        g.fillPath (quad);
        g.setColour (hot ? cosmos::gold.withAlpha (0.35f) : cosmos::voidC.withAlpha (0.4f));
        g.strokePath (quad, PathStrokeType (0.5f));
    }
    float xyz[3], px, py, dep;
    const float ru = live (osca_ru + b), rv = live (osca_rv + b);
    const int ratio = jlimit (1, 7, (int) std::lround (proc.value (osca_ratio + b)));
    auto pointAt = [&] (double ph, float& x, float& y)
    {
        if (scanMode)
        {
            const auto sp2 = scanPoint (sc, (float) ph);
            surfacePoint (sp2.uA, sp2.vA, sp2.patchA / n, sp2.patchA % n, n, alpha, xyz);
        }
        else
        {
            const double u = 0.78539816 + ru * 0.78539816 * std::cos (6.283185307 * ph);
            const double v = rv * 1.5707963 * std::sin (ratio * 6.283185307 * ph);
            surfacePoint (u, v, k1, k2, n, alpha, xyz);
        }
        project (xyz[0], xyz[1], xyz[2], x, y, dep);
    };
    Path orbit;
    if (scanMode)
    {
        // every line the wave walks, drawn in gold: at full coverage the whole manifold lights up
        for (int i = 0; i < sc.count; ++i)
        {
            int pp; float vv;
            scanLine (sc, sc.start + (float) i, pp, vv);
            for (int k = 0; k <= 8; ++k)
            {
                const float u = kScanUMin + (kScanUMax - kScanUMin) * (float) k / 8.0f;
                surfacePoint (u, vv, pp / n, pp % n, n, alpha, xyz);
                project (xyz[0], xyz[1], xyz[2], px, py, dep);
                if (k == 0) orbit.startNewSubPath (px, py); else orbit.lineTo (px, py);
            }
        }
    }
    else
        for (int s2 = 0; s2 <= 120; ++s2)
        {
            pointAt ((double) s2 / 120.0, px, py);
            if (s2 == 0) orbit.startNewSubPath (px, py); else orbit.lineTo (px, py);
        }
    const float thin = scanMode ? jlimit (0.6f, 1.6f, 40.0f / (float) sc.count) : 1.6f;
    g.setColour (cosmos::gold.withAlpha (0.22f));
    g.strokePath (orbit, PathStrokeType (thin * 3.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (cosmos::gold);
    g.strokePath (orbit, PathStrokeType (thin, PathStrokeType::curved, PathStrokeType::rounded));
    // travelling point (slowed down so the eye can follow it; a scan of many lines moves proportionally faster)
    {
        const double speed = scanMode ? 0.35 * jlimit (1.0, 6.0, std::sqrt ((double) sc.count)) / 2.0 : 0.35;
        pointAt (std::fmod (time * speed, 1.0), px, py);
        const bool sounding = proc.synth.activeVoices.load() > 0;
        g.setColour (cosmos::neon.withAlpha (sounding ? 0.45f : 0.2f));
        g.fillEllipse (Rectangle<float> (16.0f, 16.0f).withCentre ({ px, py }));
        g.setColour (cosmos::starlight);
        g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre ({ px, py }));
    }
    g.setColour (colours::textDim);
    g.setFont (fonts::sans (10.0f));
    g.drawText (scanMode ? "n=" + String (n) + "  scan " + String (sc.count) + " / " + String (sc.total) + " lines"
                         : "n=" + String (n) + "  patch " + String (k1) + "," + String (k2), bounds.reduced (8.0f, 5.0f), Justification::bottomLeft);
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
