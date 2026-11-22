#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Auric Master 3 - "console" look: anthracite brushed-metal faceplates, engraved legends, machined knobs with
// coloured caps, lit push buttons and dark-glass displays. Original design in the style of studio hardware.
namespace Pal
{
    const juce::Colour bg       { 0xff23262a };   // gaps between plates
    const juce::Colour panel    { 0xff363a40 };   // faceplate metal
    const juce::Colour panelHi  { 0xff42474e };   // faceplate highlight
    const juce::Colour border   { 0xff141619 };
    const juce::Colour track    { 0xff1b1d21 };   // recessed tracks / bars
    const juce::Colour accent   { 0xffeaa23e };   // amber lamp
    const juce::Colour deep     { 0xfff3efe4 };   // brightest legend (big numbers, selected)
    const juce::Colour light    { 0xff4b5058 };
    const juce::Colour text     { 0xffe6e2d8 };   // engraved white
    const juce::Colour dim      { 0xff9ea3aa };
    const juce::Colour good     { 0xff59d06a };   // green LED
    const juce::Colour warn     { 0xffff5b47 };   // red LED
    const juce::Colour screenTop    { 0xff15181b };   // dark glass displays
    const juce::Colour screenBottom { 0xff07080a };
    const juce::Colour screenLine   { 0xff6cc7b8 };   // teal phosphor trace
    const juce::Colour screenText   { 0xffa9b8b4 };
    const juce::Colour toneLine     { 0xfff0b452 };   // amber trace

    // knob caps per section
    const juce::Colour capBlue   { 0xff3a6fc0 };
    const juce::Colour capGreen  { 0xff3e9152 };
    const juce::Colour capRed    { 0xffb8443a };
    const juce::Colour capOrange { 0xffcf8429 };
    const juce::Colour capYellow { 0xffcdb544 };
    const juce::Colour capPurple { 0xff7c58b8 };
    const juce::Colour capSilver { 0xffb9bdc3 };
    const juce::Colour capBlack  { 0xff2c2f34 };
}

namespace Hardware
{
    juce::Colour capColourFor (const juce::String& paramId);
    void brushed (juce::Graphics&, juce::Rectangle<float> r, juce::Colour base, float radius);
    void screw (juce::Graphics&, juce::Point<float> centre, float radius, float angle = 0.6f);
    void plate (juce::Graphics&, juce::Rectangle<float> r, const juce::String& title, bool screws = true);
    void engrave (juce::Graphics&, const juce::String& text, juce::Rectangle<float> r, juce::Justification, float size, juce::Colour colour = Pal::text);
}

class AuricLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AuricLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.0f, juce::Font::bold); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
};
