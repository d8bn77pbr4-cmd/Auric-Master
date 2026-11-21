#include "Theme.h"

AuricLookAndFeel::AuricLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Pal::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Pal::light);
    setColour (juce::TextEditor::textColourId, Pal::text);
    setColour (juce::TextEditor::highlightedTextColourId, Pal::text);
    setColour (juce::CaretComponent::caretColourId, Pal::accent);
    setColour (juce::Label::textColourId, Pal::dim);
    setColour (juce::Label::textWhenEditingColourId, Pal::text);
    setColour (juce::ComboBox::textColourId, Pal::text);
    setColour (juce::ComboBox::arrowColourId, Pal::accent);
    setColour (juce::PopupMenu::backgroundColourId, Pal::panel);
    setColour (juce::PopupMenu::textColourId, Pal::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Pal::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void AuricLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider& s)
{
    using namespace juce;
    const bool big = s.getProperties().getWithDefault ("big", false);
    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (big ? 8.0f : 4.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float arcW = big ? 7.0f : 3.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const Colour acc = big ? Pal::deep : Pal::accent;

    Path track;
    track.addCentredArc (c.x, c.y, radius - arcW, radius - arcW, 0.0f, startAngle, endAngle, true);
    g.setColour (Pal::track);
    g.strokePath (track, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? startAngle + 0.5f * (endAngle - startAngle) : startAngle;
    Path arc;
    arc.addCentredArc (c.x, c.y, radius - arcW, radius - arcW, 0.0f, jmin (from, angle), jmax (from, angle), true);
    g.setColour (acc);
    g.strokePath (arc, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

    const float bodyR = radius - arcW * 2.4f;
    g.setColour (Colour (0x141b2a4a));
    g.fillEllipse (c.x - bodyR, c.y - bodyR + 2.0f, bodyR * 2, bodyR * 2);
    g.setGradientFill (ColourGradient (Colours::white, c.x, c.y - bodyR, Colour (0xffeef2f8), c.x, c.y + bodyR, false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
    g.setColour (Pal::border);
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);

    if (s.getProperties().getWithDefault ("auto", false))
    {
        g.setColour (Pal::accent);
        g.setFont (FontOptions (jmax (8.0f, bodyR * 0.38f), Font::bold));
        g.drawText ("AUTO", Rectangle<float> (c.x - bodyR, c.y - bodyR * 0.3f, bodyR * 2, bodyR * 0.6f), Justification::centred);
    }

    if (big)
    {
        g.setColour (Pal::deep);
        g.setFont (FontOptions (bodyR * 0.32f, Font::bold));
        g.drawText (String (roundToInt (s.getValue())), Rectangle<float> (c.x - bodyR, c.y - bodyR * 0.25f, bodyR * 2, bodyR * 0.5f), Justification::centred);
    }

    Path ptr;
    const float pl = bodyR * 0.82f;
    const float pw = big ? 4.0f : 2.6f;
    ptr.addRoundedRectangle (-pw * 0.5f, -pl, pw, pl * (big ? 0.25f : 0.42f), pw * 0.5f);
    g.setColour (acc);
    g.fillPath (ptr, AffineTransform::rotation (angle).translated (c.x, c.y));
}

void AuricLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState() && b.isEnabled();
    if (! b.isEnabled()) highlighted = false;
    g.setColour (on ? Pal::accent : Pal::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on || highlighted ? Pal::accent : Pal::border);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    g.setColour (on ? juce::Colours::white : Pal::dim.withAlpha (b.isEnabled() ? 1.0f : 0.45f));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void AuricLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (1.0f);
    g.setColour (Pal::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.isMouseOver (true) ? Pal::accent : Pal::border);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    juce::Path arrow;
    const float ax = r.getRight() - 16.0f, ay = r.getCentreY();
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (Pal::accent);
    g.fillPath (arrow);
}

