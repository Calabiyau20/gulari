#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace wc::ui
{
namespace col
{
    const juce::Colour bg       { 0xff15161a };   // window / rack rails
    const juce::Colour rail     { 0xff2a2b30 };
    const juce::Colour panelHi  { 0xffdedfdc };   // 200-series light grey aluminium
    const juce::Colour panelLo  { 0xffc9cac6 };
    const juce::Colour ink      { 0xff2146a8 };   // blue silkscreen
    const juce::Colour inkSoft  { 0xff5671b8 };
    const juce::Colour accent   { 0xff27c7d8 };   // Calabi Yau cyan (top bar only)
    const juce::Colour ledRed   { 0xffff3b30 };
    const juce::Colour ledGreen { 0xff35e06a };
}

// patch cord colours: banana cords (red, black, blue, yellow, green, white), mini cords use darker variants
juce::Colour cableColour (int index, bool banana);

class WcLook : public juce::LookAndFeel_V4
{
public:
    WcLook();
    static juce::Font label (float h);        // panel silkscreen
    static juce::Font title (float h);        // module titles
    static juce::Font brand (float h);        // top bar
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return label (13.0f); }
    juce::Font getPopupMenuFont() override { return label (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return label (12.5f); }
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
};
} // namespace wc::ui
