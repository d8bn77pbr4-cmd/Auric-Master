#pragma once

#include "PluginProcessor.h"

// Live resonance display: spectrum, reduction, peak markers, waterfall and 6 draggable clash bands
class ClashDisplay : public juce::Component
{
public:
    explicit ClashDisplay (AuricMaster3Processor&);
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

    AuricMaster3Processor& proc;
    std::array<Node, numClashBands> nodes;
    std::array<float, ResonanceSuppressor::numDisplayPoints> spectrum {}, reduction {}, peakHold {};
    juce::Image waterfall;
    bool large = false;
    int hovered = -1, dragging = -1, selected = -1;
    float mouseX = -1.0f;
};
