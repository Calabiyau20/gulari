#include "WcLook.h"
#include "WcData.h"

namespace wc::ui
{
namespace skin
{
int current = 0;
// Neon Void: deep-space panels with electric violet frames and neon green lettering
juce::Colour panelHi (juce::uint32 bg)
{
    if (! neon()) return bg != 0 ? juce::Colour (bg).brighter (0.15f) : juce::Colour (0xffe3e5e8);
    return bg != 0 ? juce::Colour (bg).withMultipliedBrightness (0.35f).withMultipliedSaturation (1.4f) : juce::Colour (0xff221a52);
}
juce::Colour panelLo (juce::uint32 bg)
{
    if (! neon()) return bg != 0 ? juce::Colour (bg).darker (0.25f) : juce::Colour (0xffc7cad0);
    return bg != 0 ? juce::Colour (bg).withMultipliedBrightness (0.18f).withMultipliedSaturation (1.4f) : juce::Colour (0xff0c0a22);
}
juce::Colour ink (juce::uint32 i)
{
    if (! neon()) return i != 0 ? juce::Colour (i) : col::ink;
    return juce::Colour (0xff6ff0b4);
}
juce::Colour knobCap (juce::uint32 cap)
{
    if (! neon()) return juce::Colour (cap);
    auto c = juce::Colour (cap);
    return c.getBrightness() > 0.6f ? juce::Colour (0xff3b2f6e) : c.withMultipliedSaturation (1.3f);
}
juce::Colour engrave() { return neon() ? juce::Colour (0xffd8d2ea) : juce::Colour (0xff8d9097); }
} // namespace skin

juce::Colour cableColour (int index, bool banana)
{
    static const juce::uint32 b[6] = { 0xffd8352a, 0xff1d1e22, 0xff2f6fd0, 0xffe7c234, 0xff2f9d55, 0xffeeeae2 };
    static const juce::uint32 m[6] = { 0xff3a3b40, 0xff1d1e22, 0xff2c3442, 0xff40382a, 0xff2b3a30, 0xff55565c };
    const int i = ((index % 6) + 6) % 6;
    return juce::Colour (banana ? b[i] : m[i]);
}

static juce::Typeface::Ptr face (const void* data, size_t size)
{
    return juce::Typeface::createSystemTypefaceFor (data, size);
}

juce::Font WcLook::label (float h)
{
    static auto tf = face (WcData::MontserratSemiBold_ttf, WcData::MontserratSemiBold_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}
juce::Font WcLook::title (float h)
{
    static auto tf = face (WcData::MontserratBold_ttf, WcData::MontserratBold_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}
juce::Font WcLook::brand (float h)
{
    static auto tf = face (WcData::SyncopateBold_ttf, WcData::SyncopateBold_ttfSize);
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}

WcLook::WcLook()
{
    setColour (juce::PopupMenu::backgroundColourId, col::overlay);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::midi.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, col::text);
    setColour (juce::PopupMenu::headerTextColourId, col::info);
    setColour (juce::ComboBox::textColourId, juce::Colour (0xffe6e6e6));
    setColour (juce::ComboBox::arrowColourId, col::accent);
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe6e6e6));
    setColour (juce::TooltipWindow::textColourId, col::text);
}

// 200t knobs: a brushed silver cap on a coloured (blue) knurled skirt, black index line.
// The printed scale around the knob is drawn by the panel.
void WcLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const int style = (int) s.getProperties().getWithDefault ("style", 1);
    const juce::Colour skirt (skin::knobCap ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("cap", (juce::int64) 0xff2c55b4)));
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = area.getCentre();
    const float kr = style == 0 ? 33.0f : (style == 1 ? 22.0f : 14.0f);       // skirt radius
    const float cr = style == 0 ? kr * 0.5f : kr * 0.72f;                    // cap radius (big knobs: wide blue skirt)
    const float ang = a0 + pos * (a1 - a0);
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.fillEllipse (c.x - kr + 1.5f, c.y - kr + 3.0f, kr * 2.0f, kr * 2.0f);
    // knurled skirt
    {
        juce::Path sk;
        const int teeth = style == 0 ? 48 : 30;
        for (int i = 0; i <= teeth * 2; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) (teeth * 2) + ang;
            const float rr = kr * (i % 2 == 0 ? 1.0f : 0.965f);
            if (i == 0) sk.startNewSubPath (c.x + rr * std::sin (a), c.y - rr * std::cos (a));
            else sk.lineTo (c.x + rr * std::sin (a), c.y - rr * std::cos (a));
        }
        sk.closeSubPath();
        g.setGradientFill (juce::ColourGradient (skirt.brighter (0.35f), c.x - kr, c.y - kr, skirt.darker (0.45f), c.x + kr, c.y + kr, false));
        g.fillPath (sk);
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.strokePath (sk, juce::PathStrokeType (0.7f));
    }
    // brushed silver cap
    juce::ColourGradient grad (juce::Colour (0xfff7f8fa), c.x - cr * 0.7f, c.y - cr * 0.8f, juce::Colour (0xff9da2aa), c.x + cr * 0.7f, c.y + cr * 0.9f, false);
    grad.addColour (0.5, juce::Colour (0xffd9dce1));
    g.setGradientFill (grad);
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
    for (int i = 0; i < 6; ++i)   // concentric brushing
    {
        const float rr = cr * (0.25f + 0.12f * (float) i);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 0.5f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f, 0.8f);
    // index line on the cap (big knobs: across the skirt too)
    g.setColour (juce::Colour (0xff121316));
    const float r1 = style == 0 ? 0.0f : cr * 0.15f, r2 = style == 0 ? kr * 0.95f : cr * 0.98f;
    g.drawLine (c.x + r1 * std::sin (ang), c.y - r1 * std::cos (ang), c.x + r2 * std::sin (ang), c.y - r2 * std::cos (ang), style == 0 ? 2.6f : 2.0f);
    if (s.isMouseOverOrDragging())
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    }
}

// 207t fader: black slot, blue cap
void WcLook::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cx = (float) x + (float) w * 0.5f;
    g.setColour (juce::Colour (0xff111114));
    g.fillRoundedRectangle (cx - 3.0f, (float) y, 6.0f, (float) h, 2.0f);
    const juce::Rectangle<float> cap (cx - 11.0f, pos - 7.0f, 22.0f, 14.0f);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (cap.translated (1.0f, 2.0f), 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4f7be0), cap.getX(), cap.getY(), juce::Colour (0xff1f44a8), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.drawLine (cap.getX() + 3.0f, cap.getCentreY(), cap.getRight() - 3.0f, cap.getCentreY(), 1.0f);
}

void WcLook::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (col::midi.withAlpha (col::frameAlpha));
    g.drawRect (0, 0, w, h, 1);
}

// Menu items: text 13 px from 18 px in (the tick before it); the right column in SemiBold 11 px, not squeezed, dimmed
constexpr int kMenuTextX = 18, kMenuItemH = 20;

void WcLook::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardHeight, int& w, int& h)
{
    LookAndFeel_V4::getIdealPopupMenuItemSize (text, isSeparator, standardHeight, w, h);
    if (! isSeparator) h = juce::jmax (h, kMenuItemH);   // room for 13 px without JUCE shrinking it
}

void WcLook::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                                bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcut,
                                const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (isSeparator || icon != nullptr || hasSubMenu)
    {
        LookAndFeel_V4::drawPopupMenuItem (g, area, isSeparator, isActive, isHighlighted, isTicked, hasSubMenu, text, shortcut, icon, textColour);
        return;
    }
    auto r = area.reduced (1);
    if (isHighlighted && isActive) { g.setColour (findColour (juce::PopupMenu::highlightedBackgroundColourId)); g.fillRect (r); }
    const auto ink = (textColour != nullptr ? *textColour : col::text).withMultipliedAlpha (isActive ? 1.0f : 0.5f);
    g.setColour (ink);
    if (isTicked)
    {
        auto tick = getTickShape (1.0f);
        const auto box = juce::Rectangle<float> ((float) area.getX() + 5.0f, (float) r.getY(), (float) kMenuTextX - 8.0f, (float) r.getHeight()).reduced (0.0f, 5.0f);
        g.fillPath (tick, tick.getTransformToScaleToFit (box, true));
    }
    r.setLeft (area.getX() + kMenuTextX);
    r.removeFromRight (8);
    g.setFont (label (13.0f));
    g.drawFittedText (text, r, juce::Justification::centredLeft, 1, 1.0f);
    if (shortcut.isNotEmpty())
    {
        g.setColour (col::textDim.withMultipliedAlpha (isActive ? 1.0f : 0.5f));
        g.setFont (label (11.0f));
        g.drawText (shortcut, r, juce::Justification::centredRight, true);
    }
}

void WcLook::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setFont (getPopupMenuFont().boldened());
    g.setColour (findColour (juce::PopupMenu::headerTextColourId));
    g.drawFittedText (name, area.getX() + kMenuTextX, area.getY(), area.getWidth() - kMenuTextX - 4, (int) ((float) area.getHeight() * 0.8f),
                      juce::Justification::bottomLeft, 1);
}

void WcLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colour (down ? 0xff2d3036 : (over ? 0xff292b31 : 0xff222328)));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (b.getToggleState() ? col::accent : juce::Colour (0xff45474e));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void WcLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (label (12.5f));
    g.setColour (b.findColour (juce::TextButton::textColourOffId));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void WcLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colour (0xff222328));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? col::accent : juce::Colour (0xff45474e));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 14.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (col::accent);
    g.fillPath (arrow);
}

void WcLook::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (col::overlay.withAlpha (col::overlayAlpha));
    g.setColour (col::midi.withAlpha (col::frameAlpha));
    g.drawRect (0, 0, w, h, 1);
    g.setColour (col::text);
    g.setFont (label (13.0f));
    g.drawFittedText (text, 8, 4, w - 16, h - 8, juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> WcLook::getTooltipBounds (const juce::String& tipText, juce::Point<int> p, juce::Rectangle<int> parent)
{
    const int w = juce::jmin (340, (int) juce::GlyphArrangement::getStringWidth (label (13.0f), tipText) + 24);
    const int lines = 1 + (int) juce::GlyphArrangement::getStringWidth (label (13.0f), tipText) / 320;
    const int h = 10 + 17 * lines;
    return juce::Rectangle<int> (p.x + 14, p.y + 18, w, h).constrainedWithin (parent);
}
} // namespace wc::ui
