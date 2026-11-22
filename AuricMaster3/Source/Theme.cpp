#include "Theme.h"

using namespace juce;

//==============================================================================
Colour Hardware::capColourFor (const String& id)
{
    if (id.startsWith ("eq") || id == "lowcut" || id == "highcut" || id.startsWith ("tone")) return Pal::capBlue;
    if (id.startsWith ("mb_") || id.startsWith ("xover"))                                      return Pal::capGreen;
    if (id.startsWith ("glue"))                                                                 return Pal::capRed;
    if (id.startsWith ("sat"))                                                                  return Pal::capOrange;
    if (id.startsWith ("width") || id == "mono_freq")                                           return Pal::capYellow;
    if (id.startsWith ("res") || id.startsWith ("clash"))                                       return Pal::capPurple;
    if (id.startsWith ("lim") || id == "ceiling" || id == "clip" || id == "output")             return Pal::capSilver;
    return Pal::capBlack;   // input, mix
}

void Hardware::brushed (Graphics& g, Rectangle<float> r, Colour base, float radius)
{
    g.setGradientFill (ColourGradient (base.brighter (0.10f), 0, r.getY(), base.darker (0.18f), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    // fine horizontal brushing (deterministic, so the texture never flickers)
    Graphics::ScopedSaveState s (g);
    Path clip;
    clip.addRoundedRectangle (r, radius);
    g.reduceClipRegion (clip);
    uint32 seed = 0x9e3779b9u;
    for (float y = r.getY() + 1.0f; y < r.getBottom(); y += 1.5f)
    {
        seed = seed * 1664525u + 1013904223u;
        const float a = 0.012f + 0.03f * (float) ((seed >> 24) & 0xff) / 255.0f;
        g.setColour (((seed >> 8) & 1u) ? Colours::white.withAlpha (a) : Colours::black.withAlpha (a * 1.4f));
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
    }
}

void Hardware::screw (Graphics& g, Point<float> c, float rad, float angle)
{
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - rad - 0.5f, c.y - rad + 0.8f, 2.0f * rad + 1.0f, 2.0f * rad + 1.0f);
    g.setGradientFill (ColourGradient (Colour (0xffd2d5d9), c.x - rad * 0.6f, c.y - rad * 0.7f, Colour (0xff6e737a), c.x + rad, c.y + rad, true));
    g.fillEllipse (c.x - rad, c.y - rad, 2.0f * rad, 2.0f * rad);
    g.setColour (Colour (0xff2b2e33));
    const float dx = std::cos (angle) * rad * 0.75f, dy = std::sin (angle) * rad * 0.75f;
    g.drawLine (c.x - dx, c.y - dy, c.x + dx, c.y + dy, jmax (1.0f, rad * 0.32f));
}

void Hardware::engrave (Graphics& g, const String& text, Rectangle<float> r, Justification j, float size, Colour colour)
{
    g.setFont (FontOptions (size, Font::bold).withKerningFactor (0.06f));
    g.setColour (Colours::black.withAlpha (0.55f));
    g.drawText (text, r.translated (0.0f, 1.0f), j, false);
    g.setColour (colour);
    g.drawText (text, r, j, false);
}

void Hardware::plate (Graphics& g, Rectangle<float> r, const String& title, bool screws)
{
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 6.0f);
    brushed (g, r, Pal::panel, 6.0f);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawLine (r.getX() + 6.0f, r.getY() + 1.0f, r.getRight() - 6.0f, r.getY() + 1.0f, 1.0f);
    g.setColour (Pal::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    if (screws && r.getWidth() > 90.0f && r.getHeight() > 70.0f)
        for (auto p : { r.getTopLeft() + Point<float> (9.0f, 9.0f), r.getTopRight() + Point<float> (-9.0f, 9.0f),
                        r.getBottomLeft() + Point<float> (9.0f, -9.0f), r.getBottomRight() + Point<float> (-9.0f, -9.0f) })
            screw (g, p, 3.4f, 0.4f + p.x * 0.013f);
    if (title.isNotEmpty())
    {
        const float x0 = r.getX() + (screws ? 20.0f : 12.0f);
        engrave (g, title, { x0, r.getY() + 5.0f, r.getWidth() - 40.0f, 14.0f }, Justification::centredLeft, 10.5f, Pal::dim);
        const float tw = GlyphArrangement::getStringWidth (FontOptions (10.5f, Font::bold).withKerningFactor (0.06f), title);
        const float lx = x0 + tw + 8.0f, rx = r.getRight() - (screws ? 20.0f : 12.0f);
        if (rx > lx + 10.0f)
        {
            g.setColour (Colours::black.withAlpha (0.45f));
            g.drawHorizontalLine ((int) (r.getY() + 12.0f), lx, rx);
            g.setColour (Colours::white.withAlpha (0.07f));
            g.drawHorizontalLine ((int) (r.getY() + 13.0f), lx, rx);
        }
    }
}

//==============================================================================
AuricLookAndFeel::AuricLookAndFeel()
{
    const Colour lcdText (0xfff3c47c);
    setColour (Slider::textBoxTextColourId, Pal::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxHighlightColourId, Pal::accent.withAlpha (0.4f));
    setColour (TextEditor::textColourId, Pal::text);
    setColour (TextEditor::backgroundColourId, Pal::screenBottom);
    setColour (TextEditor::highlightedTextColourId, Colours::black);
    setColour (TextEditor::highlightColourId, Pal::accent);
    setColour (TextEditor::outlineColourId, Pal::border);
    setColour (TextEditor::focusedOutlineColourId, Pal::accent);
    setColour (CaretComponent::caretColourId, Pal::accent);
    setColour (Label::textColourId, Pal::text);
    setColour (Label::textWhenEditingColourId, Pal::text);
    setColour (ComboBox::textColourId, lcdText);
    setColour (ComboBox::arrowColourId, Pal::accent);
    setColour (ComboBox::backgroundColourId, Pal::screenBottom);
    setColour (PopupMenu::backgroundColourId, Colour (0xff1d2023));
    setColour (PopupMenu::textColourId, Pal::text);
    setColour (PopupMenu::headerTextColourId, Pal::dim);
    setColour (PopupMenu::highlightedBackgroundColourId, Pal::accent);
    setColour (PopupMenu::highlightedTextColourId, Colours::black);
    setColour (AlertWindow::backgroundColourId, Pal::panel);
    setColour (AlertWindow::textColourId, Pal::text);
    setColour (AlertWindow::outlineColourId, Pal::border);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff1d2023));
    setColour (TooltipWindow::textColourId, Pal::text);
    setColour (TooltipWindow::outlineColourId, Pal::border);
    setColour (TextButton::textColourOffId, Pal::text);
    setColour (TextButton::textColourOnId, Pal::text);
    setColour (ResizableWindow::backgroundColourId, Pal::bg);
}

void AuricLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, Slider& s)
{
    const bool big = s.getProperties().getWithDefault ("big", false);
    const Colour cap = s.getProperties().contains ("cap") ? Colour ((uint32) (int64) s.getProperties()["cap"]) : Pal::capBlack;
    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float r = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // printed scale
    const int ticks = big ? 21 : 11;
    for (int i = 0; i < ticks; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (ticks - 1);
        const bool major = big ? (i % 5 == 0) : (i % 5 == 0);
        const float r0 = r * (major ? 0.83f : 0.88f), r1 = r * 0.98f;
        const float sx = std::sin (a), sy = -std::cos (a);
        g.setColour (Pal::text.withAlpha (major ? 0.75f : 0.40f));
        g.drawLine (c.x + sx * r0, c.y + sy * r0, c.x + sx * r1, c.y + sy * r1, major ? 1.4f : 1.0f);
    }
    // value lamp: thin arc in the cap colour between scale and knob
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? startAngle + 0.5f * (endAngle - startAngle) : startAngle;
    {
        Path trackArc, valueArc;
        const float ar = r * 0.80f;
        trackArc.addCentredArc (c.x, c.y, ar, ar, 0.0f, startAngle, endAngle, true);
        g.setColour (Colours::black.withAlpha (0.45f));
        g.strokePath (trackArc, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        valueArc.addCentredArc (c.x, c.y, ar, ar, 0.0f, jmin (from, angle), jmax (from, angle), true);
        g.setColour (cap.brighter (0.35f));
        g.strokePath (valueArc, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // knurled metal skirt
    const float sr = r * (big ? 0.72f : 0.70f);
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - sr, c.y - sr + 2.5f, 2.0f * sr, 2.0f * sr);
    g.setGradientFill (ColourGradient (Colour (0xff5a5f67), c.x - sr * 0.5f, c.y - sr, Colour (0xff1f2226), c.x + sr * 0.4f, c.y + sr, false));
    g.fillEllipse (c.x - sr, c.y - sr, 2.0f * sr, 2.0f * sr);
    const int knurls = big ? 48 : 32;
    for (int i = 0; i < knurls; ++i)
    {
        const float a = angle + MathConstants<float>::twoPi * (float) i / (float) knurls;
        const float sx = std::sin (a), sy = -std::cos (a);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawLine (c.x + sx * sr * 0.86f, c.y + sy * sr * 0.86f, c.x + sx * sr * 0.99f, c.y + sy * sr * 0.99f, 1.0f);
    }
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawEllipse (c.x - sr + 0.5f, c.y - sr + 0.5f, 2.0f * sr - 1.0f, 2.0f * sr - 1.0f, 1.0f);

    // coloured cap
    const float cr = sr * 0.74f;
    g.setGradientFill (ColourGradient (cap.brighter (0.45f), c.x - cr * 0.6f, c.y - cr * 0.8f, cap.darker (0.55f), c.x + cr * 0.6f, c.y + cr, true));
    g.fillEllipse (c.x - cr, c.y - cr, 2.0f * cr, 2.0f * cr);
    g.setColour (Colours::white.withAlpha (0.20f));
    g.fillEllipse (c.x - cr * 0.62f, c.y - cr * 0.78f, cr * 1.0f, cr * 0.55f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - cr, c.y - cr, 2.0f * cr, 2.0f * cr, 1.0f);

    // pointer line across skirt and cap
    const float sx = std::sin (angle), sy = -std::cos (angle);
    const float p0 = big ? cr * 0.55f : cr * 0.20f, p1 = sr * 0.92f;
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawLine (c.x + sx * p0 + 0.7f, c.y + sy * p0 + 0.9f, c.x + sx * p1 + 0.7f, c.y + sy * p1 + 0.9f, big ? 3.2f : 2.4f);
    g.setColour (Colour (0xfff7f4ea));
    g.drawLine (c.x + sx * p0, c.y + sy * p0, c.x + sx * p1, c.y + sy * p1, big ? 2.6f : 2.0f);

    if (big)
    {
        // engraved value in the middle of the cap
        Hardware::engrave (g, String (roundToInt (s.getValue())), Rectangle<float> (c.x - cr, c.y - cr * 0.28f, 2.0f * cr, cr * 0.56f),
                           Justification::centred, jmax (9.0f, cr * 0.36f), Colour (0xfff7f4ea));
    }
    if (s.getProperties().getWithDefault ("auto", false))
        Hardware::engrave (g, "AUTO", Rectangle<float> (c.x - cr, c.y + cr * 0.15f, 2.0f * cr, cr * 0.5f), Justification::centred, jmax (8.0f, cr * 0.32f), Pal::accent);
}

static void drawKey (Graphics& g, Rectangle<float> r, bool down, bool highlighted, bool lit, Colour lamp)
{
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 4.0f);
    if (lit)
        g.setGradientFill (ColourGradient (lamp.brighter (0.25f), 0, r.getY(), lamp.darker (0.35f), 0, r.getBottom(), false));
    else
        g.setGradientFill (ColourGradient (Colour (down ? 0xff2c3035 : 0xff4a4f57), 0, r.getY(), Colour (down ? 0xff24272b : 0xff2b2f34), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::white.withAlpha (lit ? 0.35f : (highlighted ? 0.22f : 0.12f)));
    g.drawLine (r.getX() + 3.0f, r.getY() + 1.0f, r.getRight() - 3.0f, r.getY() + 1.0f, 1.0f);
    g.setColour (Pal::border);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void AuricLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f, 1.5f);
    const bool on = b.getToggleState() && b.isEnabled();
    if (! b.isEnabled()) highlighted = false;
    const bool amber = b.getProperties().getWithDefault ("lamp", "") == var ("amber");
    const Colour lampColour = amber ? Pal::accent : Pal::good;
    drawKey (g, r, down, highlighted, false, lampColour);

    // LED: dot on the left for wide keys, lamp strip at the top for small ones
    const bool small = r.getWidth() < 64.0f;
    Rectangle<float> textArea = r;
    if (small)
    {
        auto strip = Rectangle<float> (r.getX() + 6.0f, r.getY() + 3.0f, r.getWidth() - 12.0f, 3.0f);
        g.setColour (on ? lampColour : Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (strip, 1.5f);
        if (on) { g.setColour (lampColour.withAlpha (0.35f)); g.fillRoundedRectangle (strip.expanded (2.0f, 1.5f), 3.0f); }
        textArea = r.withTrimmedTop (4.0f);
    }
    else
    {
        const float d = 7.0f;
        auto led = Rectangle<float> (r.getX() + 9.0f, r.getCentreY() - d * 0.5f, d, d);
        if (on)
        {
            g.setColour (lampColour.withAlpha (0.30f));
            g.fillEllipse (led.expanded (3.0f));
        }
        g.setGradientFill (ColourGradient (on ? lampColour.brighter (0.5f) : Colour (0xff3a3d42), led.getX(), led.getY(),
                                           on ? lampColour.darker (0.2f) : Colour (0xff17191c), led.getRight(), led.getBottom(), true));
        g.fillEllipse (led);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawEllipse (led, 0.8f);
        textArea = r.withTrimmedLeft (14.0f);
    }
    Hardware::engrave (g, b.getButtonText(), textArea, Justification::centred, small ? 10.5f : 11.0f,
                       b.isEnabled() ? (on ? Pal::deep : Pal::text) : Pal::dim.withAlpha (0.5f));
}

void AuricLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    const bool primary = b.getProperties().getWithDefault ("primary", false);
    drawKey (g, b.getLocalBounds().toFloat().reduced (1.0f, 1.5f), down, highlighted && b.isEnabled(), primary || b.getToggleState(), Pal::accent);
}

Font AuricLookAndFeel::getTextButtonFont (TextButton&, int buttonHeight)
{
    return FontOptions (jmin (13.5f, (float) buttonHeight * 0.42f), Font::bold).withKerningFactor (0.05f);
}

void AuricLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    const bool lit = (bool) b.getProperties().getWithDefault ("primary", false) || b.getToggleState();
    auto r = b.getLocalBounds().toFloat().reduced (4.0f, 2.0f);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    if (! lit)
    {
        g.setColour (Colours::black.withAlpha (0.55f));
        g.drawFittedText (b.getButtonText(), r.translated (0.0f, 1.0f).toNearestInt(), Justification::centred, 1);
    }
    g.setColour (lit ? Colour (0xff1c1a16) : (b.isEnabled() ? Pal::text : Pal::dim.withAlpha (0.5f)));
    g.drawFittedText (b.getButtonText(), r.toNearestInt(), Justification::centred, 1);
}

void AuricLookAndFeel::drawComboBox (Graphics& g, int width, int height, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) width, (float) height).reduced (1.0f);
    // recessed display window
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (r.translated (0.0f, 1.0f), 4.0f, 1.0f);
    g.setGradientFill (ColourGradient (Pal::screenTop, 0, r.getY(), Pal::screenBottom, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (r, 4.0f, 1.2f);
    g.setColour (Colours::white.withAlpha (0.04f));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f).reduced (2.0f, 1.0f), 3.0f);
    Path arrow;
    const float ax = r.getRight() - 13.0f, ay = r.getCentreY();
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (box.isMouseOver (true) ? Pal::accent.brighter (0.3f) : Pal::accent);
    g.fillPath (arrow);
}

void AuricLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void AuricLookAndFeel::drawLabel (Graphics& g, Label& label)
{
    if (label.isBeingEdited()) return;
    const auto font = getLabelFont (label);
    const auto area = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds()).toFloat();
    const auto colour = label.findColour (Label::textColourId).withMultipliedAlpha (label.isEnabled() ? 1.0f : 0.5f);
    g.setFont (font);
    g.setColour (Colours::black.withAlpha (0.5f * colour.getFloatAlpha()));
    g.drawFittedText (label.getText(), area.translated (0.0f, 1.0f).toNearestInt(), label.getJustificationType(),
                      jmax (1, (int) (area.getHeight() / font.getHeight())), label.getMinimumHorizontalScale());
    g.setColour (colour);
    g.drawFittedText (label.getText(), area.toNearestInt(), label.getJustificationType(),
                      jmax (1, (int) (area.getHeight() / font.getHeight())), label.getMinimumHorizontalScale());
}

void AuricLookAndFeel::drawTooltip (Graphics& g, const String& text, int width, int height)
{
    auto r = Rectangle<float> (0, 0, (float) width, (float) height);
    g.setColour (Colour (0xff1d2023));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Pal::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    g.setColour (Pal::text);
    g.setFont (FontOptions (13.0f));
    g.drawFittedText (text, r.reduced (8.0f, 4.0f).toNearestInt(), Justification::centredLeft, 6);
}
