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

// Live resonance display: spectrum, reduction, peak markers, waterfall and 6 draggable clash bands
class ClashDisplay : public juce::Component
{
public:
    explicit ClashDisplay (AuricMasterProcessor&);
    void refresh();
    void setLarge (bool shouldBeLarge) { large = shouldBeLarge; repaint(); }

    void paint (juce::Graphics&) override;
    void resized() override;
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
    juce::Rectangle<float> waterfallArea() const;
    float freqToX (float f) const;
    float xToFreq (float x) const;
    float amountToY (float a) const;
    float yToAmount (float y) const;
    juce::Point<float> nodePos (int i) const;
    int hitNode (juce::Point<float> p) const;
    ClashNodes currentNodes() const;
    void pushWaterfallRow();
    static juce::String freqText (float f);

    AuricMasterProcessor& proc;
    std::array<Node, numClashBands> nodes;
    std::array<float, ResonanceSuppressor::numDisplayPoints> spectrum {}, reduction {}, peakHold {};
    juce::Image waterfall;
    bool large = false;
    int hovered = -1, dragging = -1, selected = -1;
    float mouseX = -1.0f;
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
    void setPage (int page);

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
    int page = 0; // 0 = master, 1 = resonance

    ClashDisplay display;
    Knob resDepth, resSharp, resSens, resAttack, resRelease, resMix, resTrim;
    Knob punch, bass, air, warmth, width, input, ceiling, output, mix;

    juce::ToggleButton bypass { "BYPASS" }, resOn { "ON" }, resDelta { "DELTA" }, resHard { "HARD" };
    juce::ToggleButton tabMaster { "MASTER" }, tabResonance { "RESONANCE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAtt, resOnAtt, resDeltaAtt, resHardAtt;

    LevelMeter inL, inR, outL, outR, compGr { true }, limGr { true };
    juce::Rectangle<int> inMeterArea, outMeterArea, grMeterArea;
    juce::Rectangle<int> resPanel, mainPanel, bottomPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuricMasterEditor)
};
