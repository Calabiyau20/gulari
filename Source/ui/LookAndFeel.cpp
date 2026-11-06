#include "LookAndFeel.h"
#include "BinaryData.h"

namespace pg::ui
{
using namespace juce;

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

void SynthLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float start, float end, Slider& s)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const float track = jmax (3.0f, r * 0.15f);
    auto col = s.findColour (Slider::rotarySliderFillColourId);
    if (col == Colour() || col.isTransparent()) col = colours::accent;

    Path bg;
    bg.addCentredArc (c.x, c.y, r - track, r - track, 0.0f, start, end, true);
    g.setColour (colours::outline);
    g.strokePath (bg, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? (start + end) * 0.5f : start;
    Path arc;
    arc.addCentredArc (c.x, c.y, r - track, r - track, 0.0f, jmin (from, angle), jmax (from, angle), true);
    g.setColour (s.isEnabled() ? col : colours::textDim);
    g.strokePath (arc, PathStrokeType (track, PathStrokeType::curved, PathStrokeType::rounded));

    // cap: a void bolt with a fine steel ring
    const auto cap = Rectangle<float> (r * 1.14f, r * 1.14f).withCentre (c);
    g.setColour (cosmos::voidC);
    g.fillEllipse (cap);
    g.setColour (cosmos::steel.withAlpha (0.6f));
    g.drawEllipse (cap, 1.0f);
    Path needle;
    needle.addRoundedRectangle (-1.4f, -(r * 0.56f), 2.8f, r * 0.36f, 1.2f);
    g.setColour (cosmos::starlight);
    g.fillPath (needle, AffineTransform::rotation (angle).translated (c));
    // node at the end of the value arc
    const Point<float> tip (c.x + (r - track) * std::sin (angle), c.y - (r - track) * std::cos (angle));
    g.setColour (cosmos::starlight);
    g.fillEllipse (Rectangle<float> (track * 0.75f, track * 0.75f).withCentre (tip));
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
    g.setColour (on ? cosmos::violet.withAlpha (0.55f) : cosmos::voidC.brighter (highlighted ? 0.06f : 0.0f));
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? cosmos::neon : cosmos::electric);
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
    if (down) fill = cosmos::violet;
    g.setColour (fill);
    g.fillRoundedRectangle (r, rad);
    g.setColour (highlighted ? cosmos::orchid : cosmos::electric);
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
    g.setColour (box.hasKeyboardFocus (false) ? cosmos::neon : cosmos::electric);
    g.drawRoundedRectangle (r, 5.0f, 1.5f);
    Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.5f, ay - 2.5f, ax + 4.5f, ay - 2.5f, ax, ay + 3.5f);   // inverted triangle, as in the iso blocks
    g.setColour (cosmos::fuchsia);
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
        g.setColour (cosmos::fuchsia);
        g.fillRect (Rectangle<float> (r.getX() - 3.0f, r.getCentreY() - 2.0f, r.getWidth() + 6.0f, 4.0f));
        g.setColour (cosmos::voidC);
        g.fillRoundedRectangle (r.reduced (6.0f, 0.0f), rad);
        g.setColour (cosmos::electric);
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
    g.setColour (cosmos::electric);
    g.drawRoundedRectangle (body, 5.0f, 1.5f);
}
} // namespace pg::ui
