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
    // what floats over the rack: the value readout, tooltips and menus
    const juce::Colour overlay  { 0xff202126 };   // at overlayAlpha
    constexpr float overlayAlpha = 0.98f;         // at 94 % the info text over aluminium was 4.2:1; at 98 % it is 4.6:1
    constexpr float frameAlpha = 0.5f;            // the 1 px frame, in `midi`
    const juce::Colour text     { 0xffeeeeee };   // light text on the overlay
    const juce::Colour textDim  { 0xffb9bbc1 };
    const juce::Colour midi     { 0xffd613d0 };   // Cosmos fuchsia: MIDI ring and dot, overlay frame
    const juce::Colour info     { 0xffc560ef };   // Cosmos orchid: info text on the overlay (fuchsia is 3.2:1 there)
    const juce::Colour warn     { 0xfff5be3e };   // Cosmos gold
    const juce::Colour error    { 0xffff6b6e };
}

// the two panel finishes: 0 = Classic (200-series aluminium, blue silkscreen), 1 = Neon Void (Calabi Yau cosmos)
namespace skin
{
    extern int current;                 // set by the editor, read while painting
    inline bool neon() { return current == 1; }
    juce::Colour panelHi (juce::uint32 specBg);
    juce::Colour panelLo (juce::uint32 specBg);
    juce::Colour ink (juce::uint32 specInk);
    juce::Colour knobCap (juce::uint32 cap);
    juce::Colour engrave();             // the engraved footer lettering
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
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return label (13.0f); }
    juce::Font getPopupMenuFont() override { return label (13.0f); }
    void getIdealPopupMenuItemSize (const juce::String&, bool isSeparator, int standardHeight, int& w, int& h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>&, bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                            bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText, const juce::Drawable*, const juce::Colour*) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return label (12.5f); }
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
};
} // namespace wc::ui
