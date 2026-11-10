#include "WcLook.h"
#include "WcData.h"

namespace wc::ui
{
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
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff202126));
    setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe6e6e6));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.35f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, col::accent);
    setColour (juce::ComboBox::textColourId, juce::Colour (0xffe6e6e6));
    setColour (juce::ComboBox::arrowColourId, col::accent);
    setColour (juce::TextButton::textColourOffId, juce::Colour (0xffe6e6e6));
    setColour (juce::TooltipWindow::textColourId, juce::Colour (0xffeeeeee));
}

// 200-series knobs: a light grey (or blue / black) skirted knob with a black (or white) index line.
// The blue ring and numbers around it are printed on the panel (ModulePanel::paint).
void WcLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const int style = (int) s.getProperties().getWithDefault ("style", 1);
    const juce::Colour body ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("cap", (juce::int64) 0xffe6e6e1));
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const auto c = area.getCentre();
    const float kr = style == 0 ? 30.0f : (style == 1 ? 21.0f : 13.5f);
    const float ang = a0 + pos * (a1 - a0);
    // shadow
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.fillEllipse (c.x - kr + 1.5f, c.y - kr + 3.0f, kr * 2.0f, kr * 2.0f);
    // skirt
    g.setGradientFill (juce::ColourGradient (body.brighter (0.15f), c.x - kr, c.y - kr, body.darker (0.35f), c.x + kr, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 0.8f);
    // cap
    const float cr = kr * 0.74f;
    g.setGradientFill (juce::ColourGradient (body.brighter (0.35f), c.x - cr * 0.6f, c.y - cr * 0.8f, body.darker (0.12f), c.x + cr * 0.6f, c.y + cr, true));
    g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.drawEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f, 0.7f);
    // index line
    const bool dark = body.getPerceivedBrightness() < 0.5f;
    g.setColour (dark ? juce::Colour (0xfff4f4f2) : juce::Colour (0xff141416));
    g.drawLine (c.x + kr * 0.12f * std::sin (ang), c.y - kr * 0.12f * std::cos (ang), c.x + kr * 0.96f * std::sin (ang), c.y - kr * 0.96f * std::cos (ang), style == 0 ? 3.0f : 2.2f);
    if (s.isMouseOverOrDragging())
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    }
}

void WcLook::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (col::accent.withAlpha (0.4f));
    g.drawRect (0, 0, w, h, 1);
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
    g.fillAll (juce::Colour (0xf0202126));
    g.setColour (col::accent.withAlpha (0.5f));
    g.drawRect (0, 0, w, h, 1);
    g.setColour (juce::Colour (0xffeeeeee));
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
