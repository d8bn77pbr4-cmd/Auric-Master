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
    for (int i = 0; i < 4; ++i)
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
}

juce::Rectangle<float> ClashDisplay::plotArea() const { return getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (16.0f); }

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
    return r.getCentreY() - a * 0.01f * r.getHeight() * 0.38f;
}

float ClashDisplay::yToAmount (float y) const
{
    const auto r = plotArea();
    return juce::jlimit (-100.0f, 100.0f, (r.getCentreY() - y) / (r.getHeight() * 0.38f) * 100.0f);
}

juce::Point<float> ClashDisplay::nodePos (int i) const
{
    const auto& n = nodes[(size_t) i];
    return { freqToX (n.freq), amountToY (n.amount) };
}

int ClashDisplay::hitNode (juce::Point<float> p) const
{
    for (int i = 0; i < 4; ++i)
        if (nodePos (i).getDistanceFrom (p) < 12.0f)
            return i;
    return -1;
}

ClashNodes ClashDisplay::currentNodes() const
{
    ClashNodes c;
    for (size_t i = 0; i < 4; ++i)
        c[i] = { nodes[i].freq, nodes[i].amount * 0.01f, nodes[i].width };
    return c;
}

void ClashDisplay::refresh()
{
    proc.resonance.copyDisplay (spectrum, reduction);
    repaint();
}

void ClashDisplay::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto a = plotArea();
    g.setColour (Colour (0xfffafbfe));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);

    // grid
    g.setFont (FontOptions (10.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = freqToX (f);
        g.setColour (Pal::border.withAlpha (0.7f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        g.setColour (Pal::dim);
        g.drawText (f >= 1000.0f ? String ((int) (f / 1000)) + "k" : String ((int) f), (int) x - 20, (int) a.getBottom() + 2, 40, 12, Justification::centred);
    }
    g.setColour (Pal::border);
    g.drawHorizontalLine ((int) a.getCentreY(), a.getX(), a.getRight());

    constexpr int N = ResonanceSuppressor::numDisplayPoints;
    auto xAt = [&] (int i) { return a.getX() + a.getWidth() * (float) i / (N - 1); };

    // spectrum (-90..0 dBFS)
    Path spec;
    spec.startNewSubPath (a.getX(), a.getBottom());
    for (int i = 0; i < N; ++i)
    {
        const float db = jlimit (-90.0f, 0.0f, spectrum[(size_t) i]);
        spec.lineTo (xAt (i), jmap (db, -90.0f, 0.0f, a.getBottom(), a.getY() + 6.0f));
    }
    spec.lineTo (a.getRight(), a.getBottom());
    spec.closeSubPath();
    g.setGradientFill (ColourGradient (Pal::accent.withAlpha (0.22f), 0, a.getY(), Pal::accent.withAlpha (0.04f), 0, a.getBottom(), false));
    g.fillPath (spec);
    g.setColour (Pal::accent.withAlpha (0.45f));
    g.strokePath (spec, PathStrokeType (1.0f));

    // resonance reduction, hanging from the top (0..12 dB shown)
    Path red;
    red.startNewSubPath (a.getX(), a.getY());
    for (int i = 0; i < N; ++i)
        red.lineTo (xAt (i), a.getY() + jlimit (0.0f, 1.0f, reduction[(size_t) i] / 12.0f) * a.getHeight() * 0.5f);
    red.lineTo (a.getRight(), a.getY());
    red.closeSubPath();
    g.setColour (Pal::deep.withAlpha (0.25f));
    g.fillPath (red);
    g.setColour (Pal::deep);
    g.strokePath (red, PathStrokeType (1.4f));

    // sensitivity curve from the clash nodes
    const auto cn = currentNodes();
    Path curve;
    for (int px = 0; px <= (int) a.getWidth(); px += 2)
    {
        const float x = a.getX() + (float) px;
        const float y = amountToY ((clashWeight (xToFreq (x), cn) - 1.0f) * 100.0f);
        if (px == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
    }
    g.setColour (Pal::accent);
    g.strokePath (curve, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // nodes
    for (int i = 0; i < 4; ++i)
    {
        const auto p = nodePos (i);
        const bool active = i == hovered || i == dragging;
        const float r = active ? 8.0f : 6.5f;
        g.setColour (Colours::white);
        g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
        g.setColour (Pal::accent);
        g.drawEllipse (p.x - r, p.y - r, r * 2, r * 2, 2.0f);
        g.setFont (FontOptions (10.0f, Font::bold));
        g.setColour (Pal::text);
        g.drawText (AuricMasterProcessor::clashNames[i], (int) p.x - 40, (int) p.y - 24, 80, 12, Justification::centred);

        if (active)
        {
            const auto& n = nodes[(size_t) i];
            const String info = (n.freq >= 1000.0f ? String (n.freq / 1000.0f, 2) + " kHz" : String (roundToInt (n.freq)) + " Hz")
                              + "   " + (n.amount > 0 ? "+" : "") + String (roundToInt (n.amount)) + " %"
                              + "   " + String (n.width, 2) + " oct";
            auto box = Rectangle<float> (p.x - 85, p.y + 12, 170, 18).constrainedWithin (a);
            g.setColour (Pal::text.withAlpha (0.9f));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (Colours::white);
            g.drawText (info, box, Justification::centred);
        }
    }

    g.setColour (Pal::dim);
    g.setFont (FontOptions (10.0f));
    g.drawText (String::fromUTF8 ("Punkt ziehen: Frequenz / St\xc3\xa4rke  \xc2\xb7  Mausrad: Breite  \xc2\xb7  Doppelklick: Reset"),
                a.reduced (8.0f, 4.0f), Justification::bottomRight);

    g.setColour (Pal::border);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);
}

void ClashDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h != hovered) { hovered = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void ClashDisplay::mouseExit (const juce::MouseEvent&) { hovered = -1; repaint(); }

void ClashDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitNode (e.position);
    if (dragging >= 0)
    {
        nodes[(size_t) dragging].freqAtt->beginGesture();
        nodes[(size_t) dragging].amtAtt->beginGesture();
    }
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
    const int h = hitNode (e.position);
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
    setupKnob (resSens, "res_sens", "SENSITIVITY");
    setupKnob (resSpeed, "res_speed", "SPEED");

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

    for (auto* m : { &inL, &inR, &outL, &outR, &compGr, &limGr })
        addAndMakeVisible (m);

    setSize (1000, 700);
    display.refresh();
    startTimerHz (30);
}

AuricMasterEditor::~AuricMasterEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
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
    g.setColour (Pal::dim);
    g.setFont (FontOptions (11.0f));
    g.drawText ("RESONANCE  /  TONE  /  DYNAMICS  /  LIMITER", 250, 14, 340, 32, Justification::centredLeft);

    auto panelAt = [&g] (Rectangle<int> r)
    {
        g.setColour (Colour (0x0c1b2a4a));
        g.fillRoundedRectangle (r.toFloat().translated (0.0f, 2.0f), 10.0f);
        g.setColour (Pal::panel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (Pal::border);
        g.drawRoundedRectangle (r.toFloat(), 10.0f, 1.0f);
    };
    panelAt ({ 20, 64, 84, getHeight() - 84 });
    panelAt ({ getWidth() - 144, 64, 124, getHeight() - 84 });
    panelAt (resPanel);
    panelAt (mainPanel);
    panelAt (bottomPanel);

    g.setFont (FontOptions (11.0f, Font::bold));
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

void AuricMasterEditor::resized()
{
    bypass.setBounds (getWidth() - 116, 16, 96, 28);

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

    // resonance section
    resPanel = { cx0, 64, cw, 318 };
    display.setBounds (resPanel.getX() + 14, resPanel.getY() + 30, cw - 28, 186);
    {
        auto row = juce::Rectangle<int> (resPanel.getX() + 14, resPanel.getY() + 222, cw - 28, 90);
        auto buttons = row.removeFromRight (110);
        resOn.setBounds (buttons.getX() + 10, buttons.getY() + 18, 90, 26);
        resDelta.setBounds (buttons.getX() + 10, buttons.getY() + 52, 90, 26);
        const int w = row.getWidth() / 4;
        for (auto* k : { &resDepth, &resSharp, &resSens, &resSpeed })
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
}
