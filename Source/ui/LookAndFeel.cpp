#include "LookAndFeel.h"
#include "BinaryData.h"

namespace pg::ui
{
using namespace juce;

// =============================================================================
// Skins
// =============================================================================
const std::vector<Skin>& skins()
{
    static const std::vector<Skin> list = {
        { "Calabi Cosmos", cosmos::deepSpace, cosmos::violet, cosmos::cream, cosmos::voidC, cosmos::voidC, 0, false },
        { "Analog Studio", Colour (0xff26221e), Colour (0xffb08d57), Colour (0xffeee2c8), Colour (0xff1d1a17), Colour (0xff0c0b0a), 1, true },
        { "Neon Void",     Colour (0xff0a0a10), cosmos::neon, cosmos::starlight, Colour (0xff000000), Colour (0xff05020c), 2, false },
    };
    return list;
}

int& skinIndex() { static int s = 0; return s; }

// control colours that follow the skin (buttons, menus, tabs)
static Colour edgeCol() { return skinIndex() == 1 ? Colour (0xff8a6d42) : cosmos::electric; }
static Colour markCol() { return skinIndex() == 1 ? Colour (0xffe8a33a) : cosmos::fuchsia; }
static Colour fillCol() { return skinIndex() == 1 ? Colour (0xff4a3a24) : cosmos::violet; }

// =============================================================================
// Fonts
// =============================================================================
namespace
{
    struct Typefaces
    {
        Typeface::Ptr display, semi, bold;
        Typefaces()
        {
            display = Typeface::createSystemTypefaceFor (BinaryData::SyncopateBold_ttf, BinaryData::SyncopateBold_ttfSize);
            semi    = Typeface::createSystemTypefaceFor (BinaryData::MontserratSemiBold_ttf, BinaryData::MontserratSemiBold_ttfSize);
            bold    = Typeface::createSystemTypefaceFor (BinaryData::MontserratBold_ttf, BinaryData::MontserratBold_ttfSize);
        }
        static Typefaces& get() { static Typefaces t; return t; }
    };
}

namespace fonts
{
    Font display (float h) { return Font (FontOptions (Typefaces::get().display).withHeight (h).withKerningFactor (0.22f)); }
    Font sans (float h, bool bold) { return Font (FontOptions (bold ? Typefaces::get().bold : Typefaces::get().semi).withHeight (h)); }
    Font label (float h) { return Font (FontOptions (Typefaces::get().semi).withHeight (h).withKerningFactor (0.2f)); }
}

// =============================================================================
// Brand primitives
// =============================================================================
namespace brand
{
    static Path octagon (Rectangle<float> r, float c)
    {
        Path p;
        p.startNewSubPath (r.getX() + c, r.getY());
        p.lineTo (r.getRight() - c, r.getY());
        p.lineTo (r.getRight(), r.getY() + c);
        p.lineTo (r.getRight(), r.getBottom() - c);
        p.lineTo (r.getRight() - c, r.getBottom());
        p.lineTo (r.getX() + c, r.getBottom());
        p.lineTo (r.getX(), r.getBottom() - c);
        p.lineTo (r.getX(), r.getY() + c);
        p.closeSubPath();
        return p;
    }

    void bolt (Graphics& g, Point<float> c, float r, Colour colour)
    {
        g.setColour (colour);
        g.fillEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c));
        g.setColour (cosmos::voidC);
        g.drawEllipse (Rectangle<float> (r * 2, r * 2).withCentre (c), jmax (1.0f, r * 0.2f));
        g.drawLine (c.x - r * 0.55f, c.y + r * 0.55f, c.x + r * 0.55f, c.y - r * 0.55f, jmax (1.0f, r * 0.22f));
    }

    Rectangle<float> chamferFrame (Graphics& g, Rectangle<float> r, float cut, float border, Colour fill, Colour accent, bool bolts)
    {
        g.setColour (colours::frame);
        g.fillPath (octagon (r, cut));
        auto inner = r.reduced (border);
        const float c2 = jmax (0.0f, cut - border * 0.4f);
        auto in = octagon (inner, c2);
        g.setColour (fill);
        g.fillPath (in);
        g.setColour (accent);
        g.strokePath (in, PathStrokeType (jmax (1.2f, border * 0.22f)));
        if (bolts)
        {
            const float br = border * 0.55f, k = cut * 0.5f;
            for (auto p : { Point<float> (r.getX() + k, r.getY() + k), Point<float> (r.getRight() - k, r.getY() + k),
                            Point<float> (r.getRight() - k, r.getBottom() - k), Point<float> (r.getX() + k, r.getBottom() - k) })
            {
                g.setColour (cosmos::neon);
                g.fillEllipse (Rectangle<float> (br * 2, br * 2).withCentre (p));
                g.setColour (cosmos::voidC);
                g.drawEllipse (Rectangle<float> (br * 2, br * 2).withCentre (p), jmax (1.0f, br * 0.25f));
                g.fillEllipse (Rectangle<float> (br * 0.7f, br * 0.7f).withCentre (p));
            }
        }
        return inner;
    }

    void sparkle (Graphics& g, Point<float> c, float s, Colour colour)
    {
        const float k = s * 0.16f;
        Path p;
        p.startNewSubPath (c.x, c.y - s);
        p.quadraticTo (c.x + k, c.y - k, c.x + s, c.y);
        p.quadraticTo (c.x + k, c.y + k, c.x, c.y + s);
        p.quadraticTo (c.x - k, c.y + k, c.x - s, c.y);
        p.quadraticTo (c.x - k, c.y - k, c.x, c.y - s);
        p.closeSubPath();
        g.setColour (colour);
        g.fillPath (p);
    }

    void drop (Graphics& g, Point<float> c, float size)
    {
        const float w = size * 0.56f;
        Path p;
        p.startNewSubPath (c.x, c.y - size / 2);
        p.cubicTo (c.x + w * 0.25f, c.y - size * 0.2f, c.x + w * 0.55f, c.y + size * 0.05f, c.x + w * 0.5f, c.y + size * 0.22f);
        p.addCentredArc (c.x, c.y + size * 0.22f, w * 0.5f, w * 0.5f, 0.0f, MathConstants<float>::halfPi, MathConstants<float>::halfPi + MathConstants<float>::pi);
        p.cubicTo (c.x - w * 0.55f, c.y + size * 0.05f, c.x - w * 0.25f, c.y - size * 0.2f, c.x, c.y - size / 2);
        p.closeSubPath();
        g.setColour (character::mint);
        g.fillPath (p);
        g.setColour (character::magenta);
        g.strokePath (p, PathStrokeType (size * 0.07f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    void checker (Graphics& g, Rectangle<float> r, float cell)
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (r.toNearestInt());
        for (int i = 0; (float) i * cell < r.getWidth(); ++i)
            for (int j = 0; (float) j * cell < r.getHeight(); ++j)
            {
                g.setColour ((i + j) % 2 ? cosmos::voidC : cosmos::starlight);
                g.fillRect (r.getX() + (float) i * cell, r.getY() + (float) j * cell, cell, cell);
            }
    }

    void perspectiveGrid (Graphics& g, Rectangle<float> r, Colour colour, float alpha, int cols, int rows)
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (r.toNearestInt());
        g.setColour (colour.withAlpha (alpha));
        const float vx = r.getCentreX(), spread = 2.2f;
        for (int c = 0; c <= cols; ++c)
        {
            const float t = (float) c / (float) cols - 0.5f;
            g.drawLine (vx + t * r.getWidth() * 0.35f, r.getY(), vx + t * r.getWidth() * spread, r.getBottom(), 1.0f);
        }
        for (int i = 1; i <= rows; ++i)
        {
            const float p = std::pow ((float) i / (float) rows, 1.8f);
            g.drawHorizontalLine ((int) (r.getY() + p * r.getHeight()), r.getX(), r.getRight());
        }
    }

    Image starfield (int w, int h, uint32_t seed, float density)
    {
        Image img (Image::RGB, jmax (1, w), jmax (1, h), true);
        Graphics g (img);
        g.fillAll (cosmos::voidC);
        Random rnd ((int64) seed);
        const int dots = (int) ((float) (w * h) / 2600.0f * density);
        for (int i = 0; i < dots; ++i)
        {
            g.setColour (cosmos::starlight.withAlpha (0.25f + rnd.nextFloat() * 0.6f));
            const float s = rnd.nextFloat() < 0.92f ? 0.8f : 1.6f;
            g.fillRect (rnd.nextFloat() * (float) w, rnd.nextFloat() * (float) h, s, s);
        }
        const int sparks = (int) ((float) (w * h) / 60000.0f * density) + 2;
        for (int j = 0; j < sparks; ++j)
            sparkle (g, { rnd.nextFloat() * (float) w, rnd.nextFloat() * (float) h }, 3.0f + rnd.nextFloat() * 6.0f);
        return img;
    }
}

// =============================================================================
// LookAndFeel
// =============================================================================
SynthLookAndFeel::SynthLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, colours::bg);
    setColour (Label::textColourId, colours::textDim);
    setColour (Slider::textBoxTextColourId, colours::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (ComboBox::backgroundColourId, cosmos::voidC);
    setColour (ComboBox::textColourId, colours::text);
    setColour (ComboBox::outlineColourId, cosmos::electric);
    setColour (ComboBox::arrowColourId, colours::accent);
    setColour (PopupMenu::backgroundColourId, cosmos::night);
    setColour (PopupMenu::textColourId, colours::text);
    setColour (PopupMenu::headerTextColourId, cosmos::gold);
    setColour (PopupMenu::highlightedBackgroundColourId, cosmos::violet);
    setColour (PopupMenu::highlightedTextColourId, cosmos::starlight);
    setColour (TextButton::buttonColourId, cosmos::voidC);
    setColour (TextButton::textColourOffId, colours::text);
    setColour (TextButton::textColourOnId, cosmos::voidC);
    setColour (TextEditor::backgroundColourId, cosmos::voidC);
    setColour (TextEditor::textColourId, colours::text);
    setColour (TextEditor::outlineColourId, cosmos::electric);
    setColour (TextEditor::focusedOutlineColourId, cosmos::neon);
    setColour (AlertWindow::backgroundColourId, cosmos::deepSpace);
    setColour (AlertWindow::textColourId, colours::text);
    setColour (AlertWindow::outlineColourId, cosmos::electric);
    setColour (TooltipWindow::backgroundColourId, cosmos::night);
    setColour (TooltipWindow::textColourId, colours::text);
    setColour (TooltipWindow::outlineColourId, cosmos::electric);
    setColour (BubbleComponent::backgroundColourId, cosmos::night);
    setColour (BubbleComponent::outlineColourId, cosmos::electric);
}

Typeface::Ptr SynthLookAndFeel::getTypefaceForFont (const Font& f)
{
    // every font that asks for the default typeface gets Montserrat
    if (f.getTypefaceName() == Font::getDefaultSansSerifFontName() || f.getTypefaceName().isEmpty())
        return f.isBold() ? fonts::sans (12.0f, true).getTypefacePtr() : fonts::sans (12.0f).getTypefacePtr();
    return LookAndFeel_V4::getTypefaceForFont (f);
}

const Image& SynthLookAndFeel::knobFace (int px)
{
    px = jlimit (8, 512, px);
    auto it = faceCache.find (px);
    if (it != faceCache.end()) return it->second;
    if (isologo.isNull())
        isologo = ImageCache::getFromMemory (BinaryData::isologo_png, BinaryData::isologo_pngSize);
    // step-down resample so small knobs stay crisp
    Image src = isologo;
    while (src.getWidth() / 2 >= px)
        src = src.rescaled (src.getWidth() / 2, src.getHeight() / 2, Graphics::highResamplingQuality);
    return faceCache[px] = src.rescaled (px, px, Graphics::highResamplingQuality);
}

// Isologo knob: the official Calabi Yau isologo as a fixed face, a cyan-to-magenta value
// ring around it and a glowing pointer that sweeps over the face (after the brand filmstrip).
void SynthLookAndFeel::drawAnalogKnob (Graphics& g, Rectangle<float> b, float angle, float start, float end, bool hot, bool on)
{
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const Colour ink (0xffeee2c8), amber (0xffe8a33a);
    // scale ticks
    for (int t = 0; t <= 10; ++t)
    {
        const float a = start + (end - start) * (float) t / 10.0f;
        const float r0 = r * (t % 5 == 0 ? 0.80f : 0.86f), r1 = r * 0.97f;
        g.setColour (ink.withAlpha (on ? 0.75f : 0.3f));
        g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a), c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), t % 5 == 0 ? 1.6f : 1.0f);
    }
    // skirt with knurling
    const float rs = r * 0.76f;
    g.setGradientFill (ColourGradient (Colour (0xff4a4744), c.x - rs, c.y - rs, Colour (0xff0d0c0b), c.x + rs, c.y + rs, false));
    g.fillEllipse (Rectangle<float> (rs * 2, rs * 2).withCentre (c));
    g.setColour (Colour (0xff000000).withAlpha (0.6f));
    for (int k = 0; k < 36; ++k)
    {
        const float a = angle + (float) k * MathConstants<float>::twoPi / 36.0f;
        g.drawLine (c.x + rs * 0.86f * std::sin (a), c.y - rs * 0.86f * std::cos (a), c.x + rs * std::sin (a), c.y - rs * std::cos (a), 1.0f);
    }
    // brushed aluminium cap
    const float rc = rs * 0.62f;
    ColourGradient cap (Colour (0xffe9e7e2), c.x - rc * 0.6f, c.y - rc * 0.7f, Colour (0xff6d6a66), c.x + rc * 0.7f, c.y + rc * 0.8f, true);
    g.setGradientFill (cap);
    g.fillEllipse (Rectangle<float> (rc * 2, rc * 2).withCentre (c));
    g.setColour (Colour (0x30000000));
    for (float rr = rc * 0.2f; rr < rc; rr += 2.0f) g.drawEllipse (Rectangle<float> (rr * 2, rr * 2).withCentre (c), 0.5f);
    // pointer line across skirt and cap
    g.setColour (on ? Colours::white : Colour (0xff9a9a9a));
    g.drawLine (c.x + rc * 0.25f * std::sin (angle), c.y - rc * 0.25f * std::cos (angle), c.x + rs * 0.95f * std::sin (angle), c.y - rs * 0.95f * std::cos (angle), jmax (2.0f, r * 0.07f));
    // little amber value lamp
    if (hot)
    {
        g.setColour (amber.withAlpha (0.35f));
        g.drawEllipse (Rectangle<float> (rs * 2 + 6, rs * 2 + 6).withCentre (c), 2.5f);
    }
}

void SynthLookAndFeel::drawDropKnob (Graphics& g, Rectangle<float> b, float pos, float angle, float start, float end, bool bipolar, bool hot, bool on, Colour col)
{
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ringR = r * 0.88f, w = jmax (2.5f, r * 0.1f);
    Path track; track.addCentredArc (c.x, c.y, ringR, ringR, 0, start, end, true);
    g.setColour (Colour (0xff1a1626)); g.strokePath (track, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.001f)
    {
        Path arc; arc.addCentredArc (c.x, c.y, ringR, ringR, 0, jmin (from, angle), jmax (from, angle), true);
        g.setColour ((on ? col : colours::textDim).withAlpha (0.25f));
        g.strokePath (arc, PathStrokeType (w * 3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (on ? col : colours::textDim);
        g.strokePath (arc, PathStrokeType (w, PathStrokeType::curved, PathStrokeType::rounded));
    }
    g.setColour (Colour (0xff07070b)); g.fillEllipse (Rectangle<float> (r * 1.3f, r * 1.3f).withCentre (c));
    g.setColour (col.withAlpha (hot ? 0.9f : 0.35f)); g.drawEllipse (Rectangle<float> (r * 1.3f, r * 1.3f).withCentre (c), 1.2f);
    // the brand drop, rotated to point at the value
    Graphics::ScopedSaveState ss (g);
    g.addTransform (AffineTransform::rotation (angle, c.x, c.y));
    const float ds = r * 0.62f, dw = ds * 0.56f, dy = c.y - r * 0.08f;
    Path d;
    d.startNewSubPath (c.x, dy - ds / 2);
    d.cubicTo (c.x + dw * 0.25f, dy - ds * 0.2f, c.x + dw * 0.55f, dy + ds * 0.05f, c.x + dw * 0.5f, dy + ds * 0.22f);
    d.addCentredArc (c.x, dy + ds * 0.22f, dw * 0.5f, dw * 0.5f, 0, MathConstants<float>::pi * 0.5f, MathConstants<float>::pi * 1.5f, false);
    d.cubicTo (c.x - dw * 0.55f, dy + ds * 0.05f, c.x - dw * 0.25f, dy - ds * 0.2f, c.x, dy - ds / 2);
    d.closeSubPath();
    if (on) { g.setColour (col.withAlpha (0.25f + 0.25f * pos)); g.strokePath (d, PathStrokeType (5.0f)); }
    g.setColour (on ? col : colours::textDim);
    g.fillPath (d);
}

void SynthLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const bool hot = s.isMouseOverOrDragging() && s.isEnabled();
    if (skin().knob == 1)
    {
        drawAnalogKnob (g, Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f), start + pos * (end - start), start, end, hot, s.isEnabled());
        return;
    }
    if (skin().knob == 2)
    {
        auto col = s.findColour (Slider::rotarySliderFillColourId);
        if (col == Colour() || col.isTransparent()) col = cosmos::neon;
        drawDropKnob (g, Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f), pos, start + pos * (end - start), start, end,
                      s.getMinimum() < 0.0 && s.getMaximum() > 0.0, hot, s.isEnabled(), col);
        return;
    }
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const float ring = jmax (2.5f, r * 0.13f);
    const float ringR = r - ring * 0.5f;
    const bool on = s.isEnabled();
    const float scale = (float) g.getInternalContext().getPhysicalPixelScaleFactor();

    // track
    Path bg;
    bg.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, start, end, true);
    g.setColour (colours::outline);
    g.strokePath (bg, PathStrokeType (ring, PathStrokeType::curved, PathStrokeType::rounded));

    // value ring, teal -> fuchsia gradient (bipolar params grow from 12 o'clock)
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.001f)
    {
        Path arc;
        arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, jmin (from, angle), jmax (from, angle), true);
        if (on)
        {
            ColourGradient grad (cosmos::teal.brighter (0.4f), c.x - r, c.y + r, cosmos::fuchsia, c.x + r, c.y + r, false);
            g.setGradientFill (grad);
        }
        else g.setColour (colours::textDim);
        g.strokePath (arc, PathStrokeType (ring, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // face: the official isologo PNG, never redrawn
    const float faceD = (r - ring) * 2.0f - 1.0f;
    const auto faceR = Rectangle<float> (faceD, faceD).withCentre (c);
    const auto& face = knobFace (roundToInt (faceD * scale));
    g.setOpacity (on ? 1.0f : 0.45f);
    g.drawImage (face, faceR, RectanglePlacement::stretchToFit);
    g.setOpacity (1.0f);

    if (hot)
    {
        // hover halo: soft fuchsia glow just outside the value ring
        g.setColour (cosmos::fuchsia.withAlpha (0.28f));
        g.drawEllipse (bounds.withSizeKeepingCentre (r * 2.0f + 2.0f, r * 2.0f + 2.0f), 3.0f);
    }
    // pointer: a glowing teal line from the face towards the rim
    const auto pt = [&] (float rad) { return Point<float> (c.x + rad * std::sin (angle), c.y - rad * std::cos (angle)); };
    const Line<float> needle (pt (r * 0.40f), pt (r - ring * 0.6f));
    const float nw = jmax (2.0f, r * 0.12f);
    g.setColour (cosmos::voidC.withAlpha (0.55f));
    g.drawLine (needle, nw + 2.0f);
    g.setColour ((on ? cosmos::teal : colours::textDim).withAlpha (0.45f));
    g.drawLine (needle, nw + 1.0f);
    g.setColour (on ? Colour (0xff8ff6ff) : cosmos::steel);
    g.drawLine (needle, nw * 0.5f);
}

void SynthLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                         Slider::SliderStyle style, Slider& s)
{
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    if (style == Slider::LinearHorizontal)
    {
        auto t = b.withSizeKeepingCentre (b.getWidth(), 4.0f);
        g.setColour (colours::outline);
        g.fillRoundedRectangle (t, 2.0f);
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float mid = bipolar ? b.getCentreX() : b.getX();
        g.setColour (cosmos::fuchsia);
        g.fillRoundedRectangle (Rectangle<float> (jmin (mid, pos), t.getY(), std::abs (pos - mid), 4.0f), 2.0f);
        brand::bolt (g, { pos, b.getCentreY() }, 6.0f);
    }
    else
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
}

void SynthLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();
    const float rad = r.getHeight() * 0.5f;
    g.setColour (on ? fillCol().withAlpha (0.55f) : cosmos::voidC.brighter (highlighted ? 0.06f : 0.0f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? cosmos::neon : edgeCol());
    g.drawRoundedRectangle (r, rad, 1.5f);
    auto dotArea = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.28f);
    g.setColour (on ? cosmos::neon : cosmos::nebula);
    g.fillEllipse (dotArea);
    g.setColour (cosmos::voidC);
    g.drawEllipse (dotArea, 1.0f);
    g.setColour (on ? colours::text : colours::textDim);
    g.setFont (fonts::label (10.5f));
    g.drawFittedText (b.getButtonText().toUpperCase(), r.toNearestInt().withTrimmedRight (6), Justification::centredLeft, 1);
}

void SynthLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    // track plate: capsule, void fill, electric border
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const float rad = jmin (8.0f, r.getHeight() * 0.3f);
    const bool on = b.getToggleState();
    Colour fill = on ? cosmos::neon : cosmos::voidC;
    if (highlighted && ! on) fill = cosmos::night.brighter (0.15f);
    if (down) fill = fillCol();
    g.setColour (fill);
    g.fillRoundedRectangle (r, rad);
    g.setColour (highlighted ? cosmos::orchid : edgeCol());
    g.drawRoundedRectangle (r, rad, 2.0f);
}

void SynthLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setFont (fonts::label (11.0f));
    g.setColour (b.getToggleState() ? cosmos::voidC : colours::text);
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds().reduced (4, 0), Justification::centred, 1);
}

void SynthLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.75f);
    g.setColour (cosmos::voidC);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.hasKeyboardFocus (false) ? cosmos::neon : edgeCol());
    g.drawRoundedRectangle (r, 5.0f, 1.5f);
    Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);   // inverted triangle, as in the iso blocks
    g.setColour (markCol());
    g.fillPath (arrow);
}

void SynthLookAndFeel::drawTabButton (TabBarButton& b, Graphics& g, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (4.0f, 5.0f);
    const bool front = b.isFrontTab();
    const float rad = r.getHeight() * 0.3f;
    if (front)
    {
        // track plate with fuchsia pipe and jade end nodes
        g.setColour (markCol());
        g.fillRect (Rectangle<float> (r.getX() - 3.0f, r.getCentreY() - 2.0f, r.getWidth() + 6.0f, 4.0f));
        g.setColour (cosmos::voidC);
        g.fillRoundedRectangle (r.reduced (6.0f, 0.0f), rad);
        g.setColour (edgeCol());
        g.drawRoundedRectangle (r.reduced (6.0f, 0.0f), rad, 2.0f);
        brand::bolt (g, { r.getX() + 4.0f, r.getCentreY() }, 4.0f);
        brand::bolt (g, { r.getRight() - 4.0f, r.getCentreY() }, 4.0f);
    }
    g.setColour (front ? cosmos::cream : (over ? cosmos::cream.withAlpha (0.85f) : cosmos::steel));
    g.setFont (fonts::label (11.5f));
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

void SynthLookAndFeel::drawBubble (Graphics& g, BubbleComponent&, const Point<float>&, const Rectangle<float>& body)
{
    g.setColour (cosmos::night);
    g.fillRoundedRectangle (body, 5.0f);
    g.setColour (edgeCol());
    g.drawRoundedRectangle (body, 5.0f, 1.5f);
}
} // namespace pg::ui
