#pragma once

#include "PluginProcessor.h"

class AuricLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AuricLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (bool isGainReduction = false) : gr (isGainReduction) {}
    void setLevel (float newValue); // linear peak, or dB (<= 0) for GR meters
    void paint (juce::Graphics&) override;

private:
    bool gr;
    float value = 0.0f, peakHold = 0.0f;
    int holdFrames = 0;
};

// Spectrum + resonance reduction display with 4 draggable "frequency clash" nodes
class ClashDisplay : public juce::Component
{
public:
    explicit ClashDisplay (AuricMasterProcessor&);
    void refresh();

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct Node
    {
        float freq = 1000.0f, amount = 0.0f, width = 1.0f;
        std::unique_ptr<juce::ParameterAttachment> freqAtt, amtAtt, widthAtt;
    };

    juce::Rectangle<float> plotArea() const;
    float freqToX (float f) const;
    float xToFreq (float x) const;
    float amountToY (float a) const;
    float yToAmount (float y) const;
    juce::Point<float> nodePos (int i) const;
    int hitNode (juce::Point<float> p) const;
    ClashNodes currentNodes() const;

    AuricMasterProcessor& proc;
    std::array<Node, 4> nodes;
    std::array<float, ResonanceSuppressor::numDisplayPoints> spectrum {}, reduction {};
    int hovered = -1, dragging = -1;
};

class AuricMasterEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AuricMasterEditor (AuricMasterProcessor&);
    ~AuricMasterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const juce::String& paramId, const juce::String& name);
    void setupToggle (juce::ToggleButton&, std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>&, const juce::String& paramId);
    void placeKnob (Knob&, juce::Rectangle<int> area);

    AuricMasterProcessor& proc;
    AuricLookAndFeel lnf;

    ClashDisplay display;
    Knob resDepth, resSharp, resSens, resSpeed;
    Knob punch, bass, air, warmth, width, input, ceiling, output, mix;

    juce::ToggleButton bypass { "BYPASS" }, resOn { "ON" }, resDelta { "DELTA" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt, resOnAtt, resDeltaAtt;

    LevelMeter inL, inR, outL, outR, compGr { true }, limGr { true };
    juce::Rectangle<int> inMeterArea, outMeterArea, grMeterArea;
    juce::Rectangle<int> resPanel, mainPanel, bottomPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuricMasterEditor)
};
