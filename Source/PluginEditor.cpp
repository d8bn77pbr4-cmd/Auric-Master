#include "PluginEditor.h"

namespace Pal
{
    const juce::Colour bgTop    { 0xff10141c };
    const juce::Colour bgBottom { 0xff07090d };
    const juce::Colour panel    { 0xff161c26 };
    const juce::Colour accent   { 0xff35e0c8 };  // teal
    const juce::Colour accent2  { 0xffffb347 };  // amber
    const juce::Colour text     { 0xffd8e2ee };
    const juce::Colour dim      { 0xff5d6b7d };
}

//==============================================================================
AuricLookAndFeel::AuricLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Pal::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Pal::dim);
}

void AuricLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    using namespace juce;
    const bool big = s.getProperties().getWithDefault ("big", false);
    auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (big ? 10.0f : 6.0f);
    const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float arcW = big ? 7.0f : 4.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const Colour acc = big ? Pal::accent2 : Pal::accent;

    // track
    Path track;
    track.addCentredArc (c.x, c.y, radius - arcW, radius - arcW, 0.0f, startAngle, endAngle, true);
    g.setColour (Colour (0xff222b38));
    g.strokePath (track, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

    // value arc (bipolar params start from the centre)
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float from = bipolar ? startAngle + 0.5f * (endAngle - startAngle) : startAngle;
    Path arc;
    arc.addCentredArc (c.x, c.y, radius - arcW, radius - arcW, 0.0f, jmin (from, angle), jmax (from, angle), true);
    g.setColour (acc.withAlpha (0.25f));
    g.strokePath (arc, PathStrokeType (arcW * 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (acc);
    g.strokePath (arc, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

    // knob body
    const float bodyR = radius - arcW * 2.6f;
    ColourGradient grad (Colour (0xff2c3644), c.x, c.y - bodyR, Colour (0xff0d1117), c.x, c.y + bodyR, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2);
    g.setColour (Colour (0xff3a4656));
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2, bodyR * 2, 1.0f);

    if (big)
    {
        // subtle concentric rings
        g.setColour (Pal::accent2.withAlpha (0.08f));
        for (float r = bodyR * 0.35f; r < bodyR * 0.95f; r += bodyR * 0.15f)
            g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 1.0f);
    }

    // pointer
    Path ptr;
    const float pl = bodyR * 0.75f;
    ptr.addRoundedRectangle (-1.5f * (big ? 1.6f : 1.0f), -pl, 3.0f * (big ? 1.6f : 1.0f), pl * 0.45f, 1.5f);
    g.setColour (acc);
    g.fillPath (ptr, AffineTransform::rotation (angle).translated (c.x, c.y));
}

void AuricLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? Pal::accent2.withAlpha (0.25f) : Pal::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? Pal::accent2 : (highlighted ? Pal::text : Pal::dim));
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

//==============================================================================
void LevelMeter::setLevel (float v)
{
    float norm;
    if (gr)
        norm = juce::jlimit (0.0f, 1.0f, -v / 12.0f);                       // 0..-12 dB
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
    g.setColour (juce::Colour (0xff0b0f15));
    g.fillRoundedRectangle (r, 3.0f);

    if (gr)
    {
        auto bar = r.withHeight (r.getHeight() * value);
        g.setColour (Pal::accent2);
        g.fillRoundedRectangle (bar, 3.0f);
    }
    else
    {
        juce::ColourGradient grad (juce::Colour (0xffff4d5e), 0, r.getY(), Pal::accent, 0, r.getBottom(), false);
        grad.addColour (0.12, Pal::accent2);
        g.setGradientFill (grad);
        auto bar = r.withTop (r.getBottom() - r.getHeight() * value);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (Pal::text.withAlpha (0.8f));
        const float py = r.getBottom() - r.getHeight() * peakHold;
        g.fillRect (r.getX(), py, r.getWidth(), 2.0f);
    }
}

//==============================================================================
AuricMasterEditor::AuricMasterEditor (AuricMasterProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

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

    addAndMakeVisible (bypass);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypass);

    for (auto* m : { &inL, &inR, &outL, &outR, &compGr, &limGr })
        addAndMakeVisible (m);

    setSize (900, 540);
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
    k.label.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
}

void AuricMasterEditor::placeKnob (Knob& k, juce::Rectangle<int> area)
{
    k.label.setBounds (area.removeFromTop (18));
    k.slider.setBounds (area);
}

void AuricMasterEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.setGradientFill (ColourGradient (Pal::bgTop, 0, 0, Pal::bgBottom, 0, (float) getHeight(), false));
    g.fillAll();

    // header
    g.setColour (Pal::text);
    g.setFont (FontOptions (26.0f, Font::bold));
    g.drawText ("AURIC", 28, 14, 120, 34, Justification::centredLeft);
    g.setColour (Pal::accent);
    g.setFont (FontOptions (26.0f));
    g.drawText ("MASTER", 116, 14, 160, 34, Justification::centredLeft);
    g.setColour (Pal::dim);
    g.setFont (FontOptions (11.0f));
    g.drawText ("MIX BUS  /  MASTERING PROCESSOR", 280, 14, 300, 34, Justification::centredLeft);

    g.setColour (Colour (0xff1f2733));
    g.drawHorizontalLine (60, 20.0f, (float) getWidth() - 20.0f);

    // panels
    auto panelAt = [&g] (Rectangle<int> r)
    {
        g.setColour (Pal::panel);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (Colour (0xff222b38));
        g.drawRoundedRectangle (r.toFloat(), 10.0f, 1.0f);
    };
    panelAt ({ 20, 76, 90, 440 });
    panelAt ({ getWidth() - 160, 76, 140, 440 });
    panelAt ({ 126, 404, getWidth() - 302, 112 });

    // glow behind the main knob
    auto c = punch.slider.getBounds().toFloat().getCentre();
    ColourGradient glow (Pal::accent2.withAlpha (0.12f), c.x, c.y, Pal::accent2.withAlpha (0.0f), c.x + 170, c.y, true);
    g.setGradientFill (glow);
    g.fillEllipse (c.x - 170, c.y - 170, 340, 340);

    // meter captions
    g.setColour (Pal::dim);
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText ("IN", inMeterArea.withY (inMeterArea.getBottom() + 4).withHeight (14), Justification::centred);
    g.drawText ("OUT", outMeterArea.withY (outMeterArea.getBottom() + 4).withHeight (14), Justification::centred);
    g.drawText ("GR", grMeterArea.withY (grMeterArea.getBottom() + 4).withHeight (14), Justification::centred);
    g.setFont (FontOptions (9.5f));
    g.drawText ("C   L", grMeterArea.withY (grMeterArea.getY() - 14).withHeight (12), Justification::centred);
}

void AuricMasterEditor::resized()
{
    bypass.setBounds (getWidth() - 120, 18, 96, 28);

    // meters
    inMeterArea = { 38, 92, 54, 390 };
    inL.setBounds (inMeterArea.getX(), inMeterArea.getY(), 24, inMeterArea.getHeight());
    inR.setBounds (inMeterArea.getX() + 30, inMeterArea.getY(), 24, inMeterArea.getHeight());

    outMeterArea = { getWidth() - 146, 92, 54, 390 };
    outL.setBounds (outMeterArea.getX(), outMeterArea.getY(), 24, outMeterArea.getHeight());
    outR.setBounds (outMeterArea.getX() + 30, outMeterArea.getY(), 24, outMeterArea.getHeight());

    grMeterArea = { getWidth() - 80, 106, 46, 376 };
    compGr.setBounds (grMeterArea.getX(), grMeterArea.getY(), 18, grMeterArea.getHeight());
    limGr.setBounds (grMeterArea.getX() + 26, grMeterArea.getY(), 18, grMeterArea.getHeight());

    // centre section
    const int cx = 126 + (getWidth() - 302) / 2;
    placeKnob (punch, { cx - 115, 80, 230, 250 });
    placeKnob (bass,   { cx - 285, 90, 120, 140 });
    placeKnob (warmth, { cx - 285, 250, 120, 140 });
    placeKnob (air,    { cx + 165, 90, 120, 140 });
    placeKnob (width,  { cx + 165, 250, 120, 140 });

    // bottom row
    auto row = juce::Rectangle<int> (126, 410, getWidth() - 302, 102).reduced (10, 0);
    const int w = row.getWidth() / 4;
    for (auto* k : { &input, &ceiling, &output, &mix })
        placeKnob (*k, row.removeFromLeft (w));
}

void AuricMasterEditor::timerCallback()
{
    inL.setLevel (proc.inPeakL.exchange (0.0f));
    inR.setLevel (proc.inPeakR.exchange (0.0f));
    outL.setLevel (proc.outPeakL.exchange (0.0f));
    outR.setLevel (proc.outPeakR.exchange (0.0f));
    compGr.setLevel (proc.compGainDb.load());
    limGr.setLevel (proc.limGainDb.load());
}
