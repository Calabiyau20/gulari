#pragma once
#include <map>
#include <juce_gui_basics/juce_gui_basics.h>

// Calabi Yau brand system (manual "calabi-yau-brand.js" v1.2.0) translated to JUCE.
namespace pg::ui
{
// ---- Cosmos palette (the world of The Singularity) ----
namespace cosmos
{
    const juce::Colour voidC     { 0xff07070d };
    const juce::Colour night     { 0xff0d0d15 };
    const juce::Colour deepSpace { 0xff19134d };
    const juce::Colour nebula    { 0xff43296e };
    const juce::Colour violet    { 0xff5f18b5 };
    const juce::Colour electric  { 0xff902bea };
    const juce::Colour orchid    { 0xffc560ef };
    const juce::Colour fuchsia   { 0xffd613d0 };
    const juce::Colour neon      { 0xff11e98a };
    const juce::Colour jade      { 0xff61d693 };
    const juce::Colour cobalt    { 0xff0f3e96 };
    const juce::Colour azure     { 0xff345fc2 };
    const juce::Colour teal      { 0xff149ba7 };
    const juce::Colour gold      { 0xfff5be3e };
    const juce::Colour cream     { 0xffedd9c2 };
    const juce::Colour starlight { 0xfffaf1e0 };
    const juce::Colour steel     { 0xff808d9d };
}
// ---- Character palette (accents only) ----
namespace character
{
    const juce::Colour mint    { 0xff66d196 };
    const juce::Colour magenta { 0xffe601c0 };
}

// Roles used by the widgets
namespace colours
{
    const juce::Colour bg      = cosmos::voidC;      // window, displays
    const juce::Colour panel   = cosmos::deepSpace;  // inside of chamfer frames
    const juce::Colour panelHi = cosmos::night;      // menus, buttons, step cells
    const juce::Colour frame   = cosmos::violet;     // chamfer frame border
    const juce::Colour outline = cosmos::nebula;     // knob tracks, quiet borders
    const juce::Colour text    = cosmos::cream;
    const juce::Colour textDim = cosmos::steel;
    const juce::Colour accent  = cosmos::neon;       // main accent: nodes, active values
    const juce::Colour accent2 = cosmos::orchid;     // secondary accent
    const juce::Colour warn    = cosmos::gold;       // Mutable Instruments modules, locks
}

// ---- Type: Syncopate for display (never the logotype), Montserrat for everything else ----
namespace fonts
{
    juce::Font display (float height);                 // Syncopate Bold, wide tracking
    juce::Font sans (float height, bool bold = false); // Montserrat SemiBold / Bold
    juce::Font label (float height);                   // Montserrat SemiBold, versal tracking 0.2 em
}

// ---- Brand drawing primitives ----
namespace brand
{
    // Octagonal chassis with bolts and inner neon filet. Returns the inner area.
    juce::Rectangle<float> chamferFrame (juce::Graphics&, juce::Rectangle<float> r, float cut, float border,
                                         juce::Colour fill = cosmos::deepSpace, juce::Colour accent = cosmos::neon,
                                         bool bolts = true);
    void sparkle (juce::Graphics&, juce::Point<float> c, float size, juce::Colour colour = cosmos::starlight);
    void drop (juce::Graphics&, juce::Point<float> c, float size);
    void checker (juce::Graphics&, juce::Rectangle<float> r, float cell);
    void perspectiveGrid (juce::Graphics&, juce::Rectangle<float> r, juce::Colour colour, float alpha, int cols, int rows);
    juce::Image starfield (int w, int h, uint32_t seed, float density);
    void bolt (juce::Graphics&, juce::Point<float> c, float r, juce::Colour colour = cosmos::jade);
}

class SynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SynthLookAndFeel();
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return fonts::sans (12.5f); }
    juce::Font getPopupMenuFont() override { return fonts::sans (13.0f); }
    juce::Font getLabelFont (juce::Label&) override { return fonts::sans (11.5f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return fonts::label (11.5f); }
    void drawTabButton (juce::TabBarButton&, juce::Graphics&, bool over, bool down) override;
    int getTabButtonBestWidth (juce::TabBarButton&, int) override { return 112; }
    void drawTabAreaBehindFrontButton (juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}
    void drawBubble (juce::Graphics&, juce::BubbleComponent&, const juce::Point<float>& tip, const juce::Rectangle<float>& body) override;

private:
    const juce::Image& knobFace (int px);   // official isologo, pre-scaled per size
    juce::Image isologo;
    std::map<int, juce::Image> faceCache;
};
} // namespace pg::ui
