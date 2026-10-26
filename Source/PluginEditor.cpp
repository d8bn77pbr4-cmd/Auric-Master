#include "PluginEditor.h"

namespace Pal
{
    const juce::Colour bg       { 0xfff3f6fb };
    const juce::Colour panel    { 0xffffffff };
    const juce::Colour border   { 0xffdfe5ef };
    const juce::Colour track    { 0xffe4e9f2 };
    const juce::Colour accent   { 0xff2f6bff };  // blue
    const juce::Colour deep     { 0xff1a3d9c };  // navy (main knob, reduction)
    const juce::Colour light    { 0xffd6e3ff };
    const juce::Colour text     { 0xff1b2a4a };
    const juce::Colour dim      { 0xff8a96ad };

    // resonance display (dark blue screen)
    const juce::Colour screenTop    { 0xff13254d };
    const juce::Colour screenBottom { 0xff0a1531 };
    const juce::Colour screenLine   { 0xff8fb4ff };
    const juce::Colour screenText   { 0xffb9cbef };
}

//==============================================================================
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

void AuricLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    using namespace juce;
    const bool big = s.getProperties().getWithDefault ("big", false);
    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (big ? 8.0f : 4.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float arcW = big ? 6.0f : 3.5f;
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

    // body with soft shadow
    const float bodyR = radius - arcW * 2.4f;
    g.setColour (Colour (0x141b2a4a));
    g.fillEllipse (c.x - bodyR, c.y - bodyR + 2.0f, bodyR * 2, bodyR * 2);
    g.setGradientFill (ColourGradient (Colours::white, c.x, c.y - bodyR, Colour (0xffeef2f8), c.x, c.y + bodyR, false));
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
    g.setColour (Pal::border);
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);

    Path ptr;
    const float pl = bodyR * 0.78f;
    const float pw = big ? 4.0f : 2.6f;
    ptr.addRoundedRectangle (-pw * 0.5f, -pl, pw, pl * 0.42f, pw * 0.5f);
    g.setColour (acc);
    g.fillPath (ptr, AffineTransform::rotation (angle).translated (c.x, c.y));
}

void AuricLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? Pal::accent : Pal::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on || highlighted ? Pal::accent : Pal::border);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    g.setColour (on ? juce::Colours::white : Pal::dim);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

//==============================================================================
void LevelMeter::setLevel (float v)
{
    float norm;
    if (gr)
        norm = juce::jlimit (0.0f, 1.0f, -v / 12.0f);
    else
        norm = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (v, -60.0f) + 60.0f) / 60.0f);

    value = norm > value ? norm : value * 0.85f + norm * 0.15f;
    if (value >= peakHold) { peakHold = value; holdFrames = 45; }
    else if (--holdFrames < 0) peakHold = juce::jmax (value, peakHold - 0.01f);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Pal::track);
    g.fillRoundedRectangle (r, 3.0f);

    if (gr)
    {
        g.setColour (Pal::deep);
        g.fillRoundedRectangle (r.withHeight (r.getHeight() * value), 3.0f);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (Pal::deep, 0, r.getY(), Pal::accent.withAlpha (0.55f), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r.withTop (r.getBottom() - r.getHeight() * value), 3.0f);
        g.setColour (Pal::text);
        g.fillRect (r.getX(), r.getBottom() - r.getHeight() * peakHold, r.getWidth(), 2.0f);
    }
}

//==============================================================================
ClashDisplay::ClashDisplay (AuricMasterProcessor& p) : proc (p)
{
    for (int i = 0; i < numClashBands; ++i)
    {
        auto& n = nodes[(size_t) i];
        const auto id = "clash" + juce::String (i + 1);
        auto attach = [this] (const juce::String& pid, float& target)
        {
            auto a = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (pid),
                                                                  [this, &target] (float v) { target = v; repaint(); });
            a->sendInitialUpdate();
            return a;
        };
        n.freqAtt = attach (id + "_freq", n.freq);
        n.amtAtt = attach (id + "_amt", n.amount);
        n.widthAtt = attach (id + "_width", n.width);
    }
    spectrum.fill (-120.0f);
    peakHold.fill (-120.0f);
    waterfall = juce::Image (juce::Image::ARGB, ResonanceSuppressor::numDisplayPoints, 90, true);
}

void ClashDisplay::resized() { repaint(); }

juce::String ClashDisplay::freqText (float f)
{
    return f >= 1000.0f ? juce::String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";
}

juce::Rectangle<float> ClashDisplay::plotArea() const
{
    auto r = getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (16.0f);
    if (large) r.removeFromBottom (82.0f);
    return r;
}

juce::Rectangle<float> ClashDisplay::waterfallArea() const
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    return r.removeFromBottom (78.0f).reduced (0.0f, 2.0f);
}

float ClashDisplay::freqToX (float f) const
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * std::log (f / 20.0f) / std::log (1000.0f);
}

float ClashDisplay::xToFreq (float x) const
{
    const auto a = plotArea();
    return juce::jlimit (20.0f, 20000.0f, 20.0f * std::pow (1000.0f, (x - a.getX()) / a.getWidth()));
}

float ClashDisplay::amountToY (float a) const
{
    const auto r = plotArea();
    return r.getCentreY() + r.getHeight() * 0.08f - a * 0.01f * r.getHeight() * 0.34f;
}

float ClashDisplay::yToAmount (float y) const
{
    const auto r = plotArea();
    return juce::jlimit (-100.0f, 100.0f, (r.getCentreY() + r.getHeight() * 0.08f - y) / (r.getHeight() * 0.34f) * 100.0f);
}

juce::Point<float> ClashDisplay::nodePos (int i) const
{
    const auto& n = nodes[(size_t) i];
    return { freqToX (n.freq), amountToY (n.amount) };
}

int ClashDisplay::hitNode (juce::Point<float> p) const
{
    for (int i = 0; i < numClashBands; ++i)
        if (nodePos (i).getDistanceFrom (p) < 12.0f)
            return i;
    return -1;
}

ClashNodes ClashDisplay::currentNodes() const
{
    ClashNodes c;
    for (size_t i = 0; i < (size_t) numClashBands; ++i)
        c[i] = { nodes[i].freq, nodes[i].amount * 0.01f, nodes[i].width, AuricMasterProcessor::clashTypes[i] };
    return c;
}

void ClashDisplay::pushWaterfallRow()
{
    const int w = waterfall.getWidth(), h = waterfall.getHeight();
    waterfall.moveImageSection (0, 1, 0, 0, w, h - 1);
    for (int x = 0; x < w; ++x)
    {
        const float t = juce::jlimit (0.0f, 1.0f, std::sqrt (reduction[(size_t) x] / 8.0f));
        const auto c = t < 0.5f ? juce::Colours::transparentBlack.interpolatedWith (Pal::accent, t * 2.0f)
                                : Pal::accent.interpolatedWith (juce::Colour (0xffe6f0ff), (t - 0.5f) * 2.0f);
        waterfall.setPixelAt (x, 0, c);
    }
}

void ClashDisplay::refresh()
{
    proc.resonance.copyDisplay (spectrum, reduction);
    for (size_t i = 0; i < peakHold.size(); ++i)
        peakHold[i] = juce::jmax (spectrum[i], peakHold[i] - 0.35f);
    pushWaterfallRow();
    repaint();
}

void ClashDisplay::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto full = getLocalBounds().toFloat();
    const auto a = plotArea();
    constexpr int N = ResonanceSuppressor::numDisplayPoints;
    auto xAt = [&] (int i) { return a.getX() + a.getWidth() * (float) i / (N - 1); };

    g.setGradientFill (ColourGradient (Pal::screenTop, 0, full.getY(), Pal::screenBottom, 0, full.getBottom(), false));
    g.fillRoundedRectangle (full, 8.0f);

    // grid
    g.setFont (FontOptions (10.0f));
    for (float f : { 30.0f, 50.0f, 100.0f, 200.0f, 300.0f, 500.0f, 1000.0f, 2000.0f, 3000.0f, 5000.0f, 10000.0f, 15000.0f })
    {
        const float x = freqToX (f);
        const int fi = (int) f;
        const bool major = fi == 100 || fi == 1000 || fi == 10000;
        g.setColour (Colours::white.withAlpha (major ? 0.12f : 0.06f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        g.setColour (Pal::screenText.withAlpha (0.7f));
        g.drawText (f >= 1000.0f ? String ((int) (f / 1000)) + "k" : String ((int) f), (int) x - 20, (int) a.getBottom() + 2, 40, 12, Justification::centred);
    }

    // spectrum (tilted, -84..+6 dB)
    auto specY = [&] (float db) { return jmap (jlimit (-84.0f, 6.0f, db), -84.0f, 6.0f, a.getBottom(), a.getY() + a.getHeight() * 0.1f); };
    Path spec, hold;
    spec.startNewSubPath (a.getX(), a.getBottom());
    for (int i = 0; i < N; ++i)
    {
        spec.lineTo (xAt (i), specY (spectrum[(size_t) i]));
        if (i == 0) hold.startNewSubPath (xAt (i), specY (peakHold[(size_t) i]));
        else hold.lineTo (xAt (i), specY (peakHold[(size_t) i]));
    }
    spec.lineTo (a.getRight(), a.getBottom());
    spec.closeSubPath();
    g.setGradientFill (ColourGradient (Pal::screenLine.withAlpha (0.30f), 0, a.getY(), Pal::screenLine.withAlpha (0.03f), 0, a.getBottom(), false));
    g.fillPath (spec);
    g.setColour (Pal::screenLine.withAlpha (0.55f));
    g.strokePath (spec, PathStrokeType (1.0f));
    g.setColour (Colours::white.withAlpha (0.18f));
    g.strokePath (hold, PathStrokeType (1.0f));

    // live reduction, hanging from the top
    const float redMax = 12.0f, redH = a.getHeight() * 0.42f;
    auto redY = [&] (float db) { return a.getY() + jlimit (0.0f, 1.0f, db / redMax) * redH; };
    Path red;
    red.startNewSubPath (a.getX(), a.getY());
    for (int i = 0; i < N; ++i)
        red.lineTo (xAt (i), redY (reduction[(size_t) i]));
    red.lineTo (a.getRight(), a.getY());
    red.closeSubPath();
    g.setGradientFill (ColourGradient (Colour (0xffe6f0ff).withAlpha (0.55f), 0, a.getY(), Pal::accent.withAlpha (0.25f), 0, a.getY() + redH, false));
    g.fillPath (red);
    g.setColour (Colour (0xffe6f0ff));
    g.strokePath (red, PathStrokeType (1.5f));

    // reduction scale
    g.setFont (FontOptions (9.5f));
    for (int db : { 0, 6, 12 })
    {
        g.setColour (Pal::screenText.withAlpha (0.55f));
        g.drawText ("-" + String (db), (int) a.getRight() - 28, (int) redY ((float) db) - (db == 0 ? 0 : 6), 24, 12, Justification::centredRight);
    }

    // markers for the strongest active resonances
    {
        std::vector<int> peaks;
        for (int i = 2; i < N - 2; ++i)
        {
            const float r = reduction[(size_t) i];
            if (r > 1.0f && r >= reduction[(size_t) i - 1] && r > reduction[(size_t) i + 1]
                && r >= reduction[(size_t) i - 2] && r > reduction[(size_t) i + 2])
                peaks.push_back (i);
        }
        std::sort (peaks.begin(), peaks.end(), [this] (int x, int y) { return reduction[(size_t) x] > reduction[(size_t) y]; });
        std::vector<int> shown;
        for (int i : peaks)
        {
            bool farEnough = true;
            for (int s : shown) farEnough = farEnough && std::abs (s - i) > N / 14;
            if (farEnough) shown.push_back (i);
            if ((int) shown.size() >= (large ? 4 : 3)) break;
        }
        g.setFont (FontOptions (10.0f, Font::bold));
        for (int i : shown)
        {
            const float x = xAt (i), y = redY (reduction[(size_t) i]);
            const String t = freqText (ResonanceSuppressor::displayPointToFreq ((float) i)) + "  -" + String (reduction[(size_t) i], 1) + " dB";
            auto box = Rectangle<float> (x - 50, y + 6, 100, 16).constrainedWithin (a.reduced (2.0f));
            g.setColour (Pal::screenBottom.withAlpha (0.85f));
            g.fillRoundedRectangle (box, 3.0f);
            g.setColour (Colours::white);
            g.drawText (t, box, Justification::centred);
        }
    }

    // auto-match: mix vs. reference curve (dashed), shown after an analysis
    if (auto* res = proc.getAutoResult())
    {
        Path devPath;
        for (int i = 0; i < N; ++i)
        {
            const float y = amountToY (jlimit (-100.0f, 100.0f, res->deviation[(size_t) i] * 13.3f));
            if (i == 0) devPath.startNewSubPath (xAt (i), y); else devPath.lineTo (xAt (i), y);
        }
        Path dashed;
        const float dashes[] = { 4.0f, 4.0f };
        PathStrokeType (1.2f).createDashedStroke (dashed, devPath, dashes, 2);
        g.setColour (Colour (0xffffc46b).withAlpha (0.75f));
        g.fillPath (dashed);
    }

    // band sensitivity curve
    const auto cn = currentNodes();
    Path curve;
    for (int px = 0; px <= (int) a.getWidth(); px += 2)
    {
        const float x = a.getX() + (float) px;
        const float y = amountToY ((clashWeight (xToFreq (x), cn) - 1.0f) * 100.0f);
        if (px == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    g.setColour (Colour (0xff5b8cff).withAlpha (0.35f));
    g.strokePath (curve, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (Colour (0xff6f9bff));
    g.strokePath (curve, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // nodes
    for (int i = 0; i < numClashBands; ++i)
    {
        const auto p = nodePos (i);
        const bool active = i == hovered || i == dragging || i == selected;
        const float r = active ? 8.0f : 6.0f;
        g.setColour (i == selected ? Pal::accent : Pal::screenBottom);
        g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
        g.setColour (Colours::white);
        g.drawEllipse (p.x - r, p.y - r, r * 2, r * 2, 2.0f);
        g.setFont (FontOptions (9.5f, Font::bold));
        g.setColour (Colours::white.withAlpha (active ? 1.0f : 0.7f));
        g.drawText (AuricMasterProcessor::clashNames[i], (int) p.x - 40, (int) p.y + 10, 80, 12, Justification::centred);
    }

    // info for the hovered / selected band
    const int info = dragging >= 0 ? dragging : (hovered >= 0 ? hovered : selected);
    if (info >= 0)
    {
        const auto& n = nodes[(size_t) info];
        const int type = AuricMasterProcessor::clashTypes[info];
        const String s = String (AuricMasterProcessor::clashNames[info]) + "   "
                       + (type == ClashNode::bell ? "Bell" : (type == ClashNode::lowShelf ? "Low Shelf" : "High Shelf")) + "   "
                       + freqText (n.freq) + "   " + (n.amount > 0 ? "+" : "") + String (roundToInt (n.amount)) + " %   "
                       + String (n.width, 2) + (type == ClashNode::bell ? " oct" : " slope");
        auto box = Rectangle<float> (a.getX() + 8, a.getBottom() - 24, 300, 18);
        g.setColour (Pal::accent.withAlpha (0.9f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (Colours::white);
        g.setFont (FontOptions (10.5f, Font::bold));
        g.drawText (s, box, Justification::centred);
    }

    // auto-match status
    {
        String line1, line2;
        float progress = -1.0f;
        if (proc.isAutoLearning())
        {
            progress = jlimit (0.0f, 1.0f, proc.getAutoProgress());
            line1 = String::fromUTF8 ("AUTO-MATCH  \xc2\xb7  analysiere Mix ") + String (roundToInt (progress * 100.0f)) + " %";
            line2 = progress <= 0.0f ? String::fromUTF8 ("Spiele den Song ab \xe2\x80\x93 Stille wird ignoriert") : String ("Frequenz-Clashes werden erkannt ...");
        }
        else if (auto* res = proc.getAutoResult())
        {
            if (Time::getMillisecondCounter() - proc.getAutoResultTime() < 12000)
            {
                line1 = String::fromUTF8 ("AUTO-MATCH angewendet  \xc2\xb7  Profil: ") + proc.apvts.getParameter ("auto_profile")->getCurrentValueAsText();
                line2 = res->summary;
            }
        }
        if (line1.isNotEmpty())
        {
            g.setFont (FontOptions (10.5f, Font::bold));
            const float w = jmin (a.getWidth() - 20.0f, jmax (GlyphArrangement::getStringWidth (g.getCurrentFont(), line1),
                                                              GlyphArrangement::getStringWidth (g.getCurrentFont(), line2)) + 30.0f);
            auto box = Rectangle<float> (a.getCentreX() - w * 0.5f, a.getY() + a.getHeight() * 0.5f - 26.0f, w, progress >= 0.0f ? 52.0f : 42.0f);
            g.setColour (Pal::screenBottom.withAlpha (0.92f));
            g.fillRoundedRectangle (box, 6.0f);
            g.setColour (Colour (0xffffc46b));
            g.drawRoundedRectangle (box, 6.0f, 1.0f);
            auto inner = box.reduced (12.0f, 6.0f);
            g.drawText (line1, inner.removeFromTop (15.0f), Justification::centred);
            g.setColour (Colours::white.withAlpha (0.85f));
            g.setFont (FontOptions (10.0f));
            g.drawText (line2, inner.removeFromTop (15.0f), Justification::centred, true);
            if (progress >= 0.0f)
            {
                auto bar = inner.removeFromTop (8.0f).reduced (0.0f, 1.0f);
                g.setColour (Colours::white.withAlpha (0.12f));
                g.fillRoundedRectangle (bar, 3.0f);
                g.setColour (Colour (0xffffc46b));
                g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress), 3.0f);
            }
        }
    }

    // crosshair frequency readout
    if (mouseX >= a.getX() && mouseX <= a.getRight() && dragging < 0)
    {
        g.setColour (Colours::white.withAlpha (0.25f));
        g.drawVerticalLine ((int) mouseX, a.getY(), a.getBottom());
        g.setColour (Colours::white.withAlpha (0.85f));
        g.setFont (FontOptions (10.0f));
        g.drawText (freqText (xToFreq (mouseX)), (int) mouseX + 4, (int) a.getBottom() - 40, 70, 12, Justification::centredLeft);
    }

    if (large)
    {
        // waterfall: reduction history (newest on top)
        const auto wf = waterfallArea();
        g.setColour (Colours::black.withAlpha (0.2f));
        g.fillRect (wf);
        g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
        g.drawImage (waterfall, wf, RectanglePlacement::stretchToFit);
        g.setColour (Pal::screenText.withAlpha (0.6f));
        g.setFont (FontOptions (9.5f, Font::bold));
        g.drawText ("REDUCTION HISTORY", wf.reduced (6.0f, 3.0f), Justification::topLeft);

        g.setFont (FontOptions (10.0f));
        g.drawText (String::fromUTF8 ("Band ziehen: Frequenz / St\xc3\xa4rke  \xc2\xb7  Mausrad: Breite  \xc2\xb7  Doppelklick: Reset"),
                    a.reduced (8.0f, 4.0f), Justification::bottomRight);
    }

    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (full.reduced (0.5f), 8.0f, 1.0f);
}

void ClashDisplay::mouseMove (const juce::MouseEvent& e)
{
    mouseX = e.position.x;
    hovered = hitNode (e.position);
    setMouseCursor (hovered >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
    repaint();
}

void ClashDisplay::mouseExit (const juce::MouseEvent&) { hovered = -1; mouseX = -1.0f; repaint(); }

void ClashDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitNode (e.position);
    selected = dragging;
    if (dragging >= 0)
    {
        nodes[(size_t) dragging].freqAtt->beginGesture();
        nodes[(size_t) dragging].amtAtt->beginGesture();
    }
    repaint();
}

void ClashDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    auto& n = nodes[(size_t) dragging];
    n.freqAtt->setValueAsPartOfGesture (xToFreq (e.position.x));
    n.amtAtt->setValueAsPartOfGesture (yToAmount (e.position.y));
}

void ClashDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
    {
        nodes[(size_t) dragging].freqAtt->endGesture();
        nodes[(size_t) dragging].amtAtt->endGesture();
    }
    dragging = -1;
    repaint();
}

void ClashDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h < 0) return;
    auto& n = nodes[(size_t) h];
    n.amtAtt->setValueAsCompleteGesture (0.0f);
    n.widthAtt->setValueAsCompleteGesture (1.0f);
    n.freqAtt->setValueAsCompleteGesture (AuricMasterProcessor::clashDefaultFreqs[h]);
}

void ClashDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int h = hitNode (e.position);
    if (h < 0) h = selected;
    if (h < 0) return;
    auto& n = nodes[(size_t) h];
    n.widthAtt->setValueAsCompleteGesture (juce::jlimit (0.25f, 3.0f, n.width * (wheel.deltaY > 0 ? 1.12f : 1.0f / 1.12f)));
}

//==============================================================================
AuricMasterEditor::AuricMasterEditor (AuricMasterProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (display);
    setupKnob (resDepth, "res_depth", "DEPTH");
    setupKnob (resSharp, "res_sharp", "SHARPNESS");
    setupKnob (resSens, "res_sens", "SELECTIVITY");
    setupKnob (resAttack, "res_attack", "ATTACK");
    setupKnob (resRelease, "res_release", "RELEASE");
    setupKnob (resMix, "res_mix", "MIX");
    setupKnob (resTrim, "res_trim", "TRIM");

    setupKnob (punch, "punch", "PUNCH");
    punch.slider.getProperties().set ("big", true);
    setupKnob (bass, "bass", "BASS");
    setupKnob (warmth, "warmth", "WARMTH");
    setupKnob (air, "air", "AIR");
    setupKnob (width, "width", "WIDTH");
    setupKnob (input, "input", "INPUT");
    setupKnob (ceiling, "ceiling", "CEILING");
    setupKnob (output, "output", "OUTPUT");
    setupKnob (mix, "mix", "MIX");

    setupToggle (bypass, bypassAtt, "bypass");
    setupToggle (resOn, resOnAtt, "res_on");
    setupToggle (resDelta, resDeltaAtt, "res_delta");
    setupToggle (resHard, resHardAtt, "res_hard");

    for (auto* t : { &tabMaster, &tabResonance })
    {
        t->setRadioGroupId (1);
        t->setClickingTogglesState (true);
        addAndMakeVisible (t);
    }
    addAndMakeVisible (autoButton);
    autoButton.setClickingTogglesState (false);
    autoButton.setTooltip ("Analysiert den Mix und stellt die Resonanz-Baender automatisch ein");
    autoButton.onClick = [this] { proc.startAutoMatch(); };
    profileBox.addItemList (AutoMatcher::profileNames(), 1);
    addAndMakeVisible (profileBox);
    profileAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "auto_profile", profileBox);

    tabMaster.onClick = [this] { setPage (0); };
    tabResonance.onClick = [this] { setPage (1); };

    for (auto* m : { &inL, &inR, &outL, &outR, &compGr, &limGr })
        addAndMakeVisible (m);

    setSize (1000, 700);
    setPage (0);
    display.refresh();
    startTimerHz (60);
}

AuricMasterEditor::~AuricMasterEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AuricMasterEditor::setPage (int newPage)
{
    page = newPage;
    tabMaster.setToggleState (page == 0, juce::dontSendNotification);
    tabResonance.setToggleState (page == 1, juce::dontSendNotification);

    const bool master = page == 0;
    for (auto* k : { &punch, &bass, &air, &warmth, &width, &input, &ceiling, &output, &mix })
    {
        k->slider.setVisible (master);
        k->label.setVisible (master);
    }
    for (auto* k : { &resAttack, &resRelease, &resTrim })
    {
        k->slider.setVisible (! master);
        k->label.setVisible (! master);
    }
    resHard.setVisible (! master);
    for (auto* m : { &inL, &inR, &outL, &outR, &compGr, &limGr })
        m->setVisible (master);

    display.setLarge (! master);
    resized();
    repaint();
}

void AuricMasterEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxTextColourId, Pal::text);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void AuricMasterEditor::setupToggle (juce::ToggleButton& b, std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att,
                                     const juce::String& id)
{
    addAndMakeVisible (b);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b);
}

void AuricMasterEditor::placeKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (16));
    k.slider.setBounds (area);
}

void AuricMasterEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (Pal::bg);

    // header
    g.setColour (Pal::text);
    g.setFont (FontOptions (24.0f, Font::bold));
    g.drawText ("AURIC", 28, 14, 100, 32, Justification::centredLeft);
    g.setColour (Pal::accent);
    g.setFont (FontOptions (24.0f));
    g.drawText ("MASTER", 108, 14, 140, 32, Justification::centredLeft);

    auto panelAt = [&g] (Rectangle<int> r)
    {
        g.setColour (Colour (0x0c1b2a4a));
        g.fillRoundedRectangle (r.toFloat().translated (0.0f, 2.0f), 10.0f);
        g.setColour (Pal::panel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (Pal::border);
        g.drawRoundedRectangle (r.toFloat(), 10.0f, 1.0f);
    };

    g.setFont (FontOptions (11.0f, Font::bold));
    if (page == 0)
    {
        panelAt ({ 20, 64, 84, getHeight() - 84 });
        panelAt ({ getWidth() - 144, 64, 124, getHeight() - 84 });
        panelAt (resPanel);
        panelAt (mainPanel);
        panelAt (bottomPanel);

        g.setColour (Pal::accent);
        g.drawText ("RESONANCE CONTROL", resPanel.getX() + 16, resPanel.getY() + 8, 200, 16, Justification::centredLeft);
        g.drawText ("TONE & DYNAMICS", mainPanel.getX() + 16, mainPanel.getY() + 8, 200, 16, Justification::centredLeft);

        g.setColour (Pal::dim);
        g.drawText ("IN", inMeterArea.withY (inMeterArea.getBottom() + 4).withHeight (14), Justification::centred);
        g.drawText ("OUT", outMeterArea.withY (outMeterArea.getBottom() + 4).withHeight (14), Justification::centred);
        g.drawText ("GR", grMeterArea.withY (grMeterArea.getBottom() + 4).withHeight (14), Justification::centred);
        g.setFont (FontOptions (9.5f));
        g.drawText ("C    L", grMeterArea.withY (grMeterArea.getY() - 14).withHeight (12), Justification::centred);
    }
    else
    {
        panelAt (resPanel);
    }
}

void AuricMasterEditor::resized()
{
    bypass.setBounds (getWidth() - 116, 16, 96, 28);
    tabMaster.setBounds (300, 16, 110, 28);
    tabResonance.setBounds (416, 16, 110, 28);
    profileBox.setBounds (getWidth() - 360, 16, 150, 28);
    autoButton.setBounds (getWidth() - 204, 16, 80, 28);

    if (page == 1)
    {
        // full-size resonance window
        display.setBounds (20, 64, getWidth() - 40, 446);
        resPanel = { 20, 522, getWidth() - 40, getHeight() - 522 - 20 };
        auto row = resPanel.reduced (16, 12);
        auto buttons = row.removeFromRight (120);
        resOn.setBounds (buttons.getX() + 10, buttons.getY() + 10, 100, 30);
        resDelta.setBounds (buttons.getX() + 10, buttons.getY() + 50, 100, 30);
        resHard.setBounds (buttons.getX() + 10, buttons.getY() + 90, 100, 30);
        const int w = row.getWidth() / 7;
        for (auto* k : { &resDepth, &resSharp, &resSens, &resAttack, &resRelease, &resMix, &resTrim })
            placeKnob (*k, row.removeFromLeft (w).reduced (6, 4));
        return;
    }

    const int meterTop = 80, meterH = getHeight() - 80 - 42;
    inMeterArea = { 36, meterTop, 52, meterH };
    inL.setBounds (inMeterArea.getX(), meterTop, 22, meterH);
    inR.setBounds (inMeterArea.getX() + 30, meterTop, 22, meterH);

    outMeterArea = { getWidth() - 132, meterTop, 52, meterH };
    outL.setBounds (outMeterArea.getX(), meterTop, 22, meterH);
    outR.setBounds (outMeterArea.getX() + 30, meterTop, 22, meterH);

    grMeterArea = { getWidth() - 70, meterTop + 14, 40, meterH - 14 };
    compGr.setBounds (grMeterArea.getX(), grMeterArea.getY(), 16, grMeterArea.getHeight());
    limGr.setBounds (grMeterArea.getX() + 24, grMeterArea.getY(), 16, grMeterArea.getHeight());

    const int cx0 = 116, cw = getWidth() - 116 - 156;

    // resonance section (compact)
    resPanel = { cx0, 64, cw, 318 };
    display.setBounds (resPanel.getX() + 14, resPanel.getY() + 30, cw - 28, 186);
    {
        auto row = juce::Rectangle<int> (resPanel.getX() + 14, resPanel.getY() + 222, cw - 28, 90);
        auto buttons = row.removeFromRight (110);
        resOn.setBounds (buttons.getX() + 10, buttons.getY() + 18, 90, 26);
        resDelta.setBounds (buttons.getX() + 10, buttons.getY() + 52, 90, 26);
        const int w = row.getWidth() / 4;
        for (auto* k : { &resDepth, &resSharp, &resSens, &resMix })
            placeKnob (*k, row.removeFromLeft (w).reduced (8, 0));
    }

    // tone & dynamics
    mainPanel = { cx0, 394, cw, 180 };
    {
        auto row = mainPanel.reduced (14, 0).withTrimmedTop (22).withTrimmedBottom (6);
        const int side = (row.getWidth() - 200) / 4;
        auto small = [] (juce::Rectangle<int> r) { return r.reduced (14, 0).withTrimmedTop (14).withTrimmedBottom (8); };
        placeKnob (bass, small (row.removeFromLeft (side)));
        placeKnob (warmth, small (row.removeFromLeft (side)));
        placeKnob (punch, row.removeFromLeft (200));
        placeKnob (air, small (row.removeFromLeft (side)));
        placeKnob (width, small (row));
    }

    // output section
    bottomPanel = { cx0, 586, cw, getHeight() - 586 - 20 };
    {
        auto row = bottomPanel.reduced (14, 6);
        const int w = row.getWidth() / 4;
        for (auto* k : { &input, &ceiling, &output, &mix })
            placeKnob (*k, row.removeFromLeft (w));
    }
}

void AuricMasterEditor::timerCallback()
{
    inL.setLevel (proc.inPeakL.exchange (0.0f));
    inR.setLevel (proc.inPeakR.exchange (0.0f));
    outL.setLevel (proc.outPeakL.exchange (0.0f));
    outR.setLevel (proc.outPeakR.exchange (0.0f));
    compGr.setLevel (proc.compGainDb.load());
    limGr.setLevel (proc.limGainDb.load());
    display.refresh();
    autoButton.setToggleState (proc.isAutoLearning(), juce::dontSendNotification);
}
