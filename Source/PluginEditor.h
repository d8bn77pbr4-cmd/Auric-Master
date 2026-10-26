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
    void placeKnob (Knob&, juce::Rectangle<int> area);

    AuricMasterProcessor& proc;
    AuricLookAndFeel lnf;

    Knob punch, bass, air, warmth, width, input, ceiling, output, mix;
    juce::ToggleButton bypass { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    LevelMeter inL, inR, outL, outR, compGr { true }, limGr { true };
    juce::Rectangle<int> inMeterArea, outMeterArea, grMeterArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuricMasterEditor)
};
