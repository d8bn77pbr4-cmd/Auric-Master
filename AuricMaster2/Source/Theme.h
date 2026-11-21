#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Pal
{
    const juce::Colour bg       { 0xfff3f6fb };
    const juce::Colour panel    { 0xffffffff };
    const juce::Colour border   { 0xffdfe5ef };
    const juce::Colour track    { 0xffe4e9f2 };
    const juce::Colour accent   { 0xff2f6bff };
    const juce::Colour deep     { 0xff1a3d9c };
    const juce::Colour light    { 0xffd6e3ff };
    const juce::Colour text     { 0xff1b2a4a };
    const juce::Colour dim      { 0xff8a96ad };
    const juce::Colour good     { 0xff1fa37a };
    const juce::Colour warn     { 0xffe0663d };
    const juce::Colour screenTop    { 0xff13254d };
    const juce::Colour screenBottom { 0xff0a1531 };
    const juce::Colour screenLine   { 0xff8fb4ff };
    const juce::Colour screenText   { 0xffb9cbef };
    const juce::Colour toneLine     { 0xffffc46b };
}

class AuricLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AuricLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.0f, juce::Font::bold); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (14.0f); }
};
