#pragma once

#include "PluginProcessor.h"
#include "Theme.h"
#include "ClashDisplay.h"

struct Knob
{
    juce::Slider slider;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

struct Combo
{
    juce::ComboBox box;
    juce::Label label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
};

// Drive: manual knob while no loudness target is set, otherwise a read-only dial showing the automatic drive
struct DriveDial
{
    Knob manual;
    juce::Slider autoDial;
    juce::Label autoLabel;
};

// White panel with a small title, used to group controls on the pages
class Page : public juce::Component
{
public:
    struct Group { juce::Rectangle<int> area; juce::String title; };
    std::vector<Group> groups;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> notes;   // explanatory text
    void paint (juce::Graphics&) override;
};

// Spectrum + EQ curve (4 bands, low cut, tone macros). Interactive: drag nodes (freq/gain), wheel = Q,
// double-click = reset band.
class EqCurveView : public juce::Component
{
public:
    EqCurveView (AuricMaster2Processor&, bool interactive);
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
    static constexpr int points = 240;
    juce::Rectangle<float> plot() const;
    float freqToX (float f) const;
    float xToFreq (float x) const;
    float gainToY (float g) const;
    float yToGain (float y) const;
    juce::Point<float> nodePos (int i) const;
    int hitNode (juce::Point<float>) const;
    float param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    struct Node
    {
        float freq = 1000.0f, gain = 0.0f, q = 1.0f;
        std::unique_ptr<juce::ParameterAttachment> fAtt, gAtt, qAtt;
    };
    AuricMaster2Processor& proc;
    const bool interactive;
    std::array<Node, 4> nodes;
    std::array<float, ResonanceSuppressor::numDisplayPoints> spectrum {}, reduction {};
    std::array<float, points> curveMid {}, curveSide {};
    bool sideDiffers = false;
    int hovered = -1, dragging = -1;
};

// Three bands over frequency with live gain reduction; crossover lines can be dragged
class DynamicsView : public juce::Component
{
public:
    explicit DynamicsView (AuricMaster2Processor&);
    void refresh();
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plot() const;
    float freqToX (float f) const;
    float xToFreq (float x) const;
    int hitLine (float x) const;
    AuricMaster2Processor& proc;
    std::unique_ptr<juce::ParameterAttachment> lowAtt, highAtt;
    float xLow = 150.0f, xHigh = 3000.0f;
    std::array<float, 4> gr {}, grHold {};
    std::array<float, ResonanceSuppressor::numDisplayPoints> spectrum {}, reduction {};
    int dragging = -1, hovered = -1;
};

// Vectorscope (mid up, side sideways) with correlation bar
class ScopeView : public juce::Component
{
public:
    explicit ScopeView (AuricMaster2Processor& p) : proc (p) {}
    void refresh() { corrS = corrS * 0.85f + proc.correlation.load() * 0.15f; repaint(); }
    void paint (juce::Graphics&) override;

private:
    AuricMaster2Processor& proc;
    float corrS = 1.0f;
};

// Loudness and gain-reduction history (UI-side, 30 Hz)
class LimiterView : public juce::Component
{
public:
    explicit LimiterView (AuricMaster2Processor& p) : proc (p) { st.fill (-100.0f); }
    void refresh();
    void paint (juce::Graphics&) override;

private:
    static constexpr int len = 600;   // 20 s at 30 Hz
    AuricMaster2Processor& proc;
    std::array<float, len> st {}, lim {}, clip {}, tp {};
    int pos = 0;
};

// Right-hand panel: integrated loudness, M/S/LRA/TP, history, correlation, gain reduction
class LoudnessPanel : public juce::Component
{
public:
    explicit LoudnessPanel (AuricMaster2Processor&);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AuricMaster2Processor& proc;
    juce::TextButton resetButton { "RESET" };
    std::array<float, LoudnessMeter::historySize> histOut {}, histIn {};
    std::array<float, 6> grS {};
    float corrS = 1.0f;
};

class TabButton : public juce::Button
{
public:
    explicit TabButton (const juce::String& name) : juce::Button (name) { setClickingTogglesState (false); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

class SnapButton : public juce::Button
{
public:
    explicit SnapButton (const juce::String& name) : juce::Button (name) {}
    bool used = false;
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// All controls at a fixed design size; the editor scales this view, so the window can be resized freely
class MainView : public juce::Component
{
public:
    static constexpr int designWidth = 1160, designHeight = 740;
    enum PageId { simple = 0, eq, dyn, stereo, limit, res, numPages };

    explicit MainView (AuricMaster2Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (int frame);   // 30 Hz
    void showPage (int index);

private:
    void setupKnob (Knob&, juce::Component& parent, const char* id, const juce::String& name, const juce::String& tip, bool big = false);
    void setupCombo (Combo&, juce::Component& parent, const char* id, const juce::String& name, const juce::String& tip);
    void setupDrive (DriveDial&, juce::Component& parent);
    static void placeKnob (Knob&, juce::Rectangle<int>);
    static void placeCombo (Combo&, juce::Rectangle<int>);
    static void placeDrive (DriveDial&, juce::Rectangle<int>);
    void updateDrive (DriveDial&);
    void layoutSimple();
    void layoutEq();
    void layoutDyn();
    void layoutStereo();
    void layoutLimit();
    void layoutRes();

    AuricMaster2Processor& proc;
    int currentPage = 0;

    // header
    std::array<std::unique_ptr<SnapButton>, 4> snaps;
    juce::ToggleButton matchButton { juce::String::fromUTF8 ("GLEICH LAUT") }, originalButton { "ORIGINAL" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> matchAtt, originalAtt;
    juce::TextButton assistButton { "ASSISTENT" };
    Combo genre;
    std::array<std::unique_ptr<TabButton>, numPages> tabs;
    std::array<Page, numPages> pages;

    // footer
    juce::String status;
    juce::TextButton undoButton { juce::String::fromUTF8 ("R\xc3\x9c" "CKG\xc3\x84NGIG") };

    LoudnessPanel loudness;

    // EINFACH
    EqCurveView simpleCurve;
    Knob amount, toneLow, toneMid, toneHigh, punch, width, input, output, ceilingS;
    Combo targetS, limModeS;
    DriveDial driveS;

    // EQ
    EqCurveView eqCurve;
    Knob lowCut;
    std::array<Knob, 4> eqFreq, eqGain, eqQ;
    std::array<Combo, 4> eqMode;

    // DYNAMIK
    DynamicsView dynView;
    Knob mbLow, mbMid, mbHigh, mbLowGain, mbMidGain, mbHighGain, xLow, xHigh, mbSpeed, glue, glueSpeed, punchD;

    // STEREO & FARBE
    ScopeView scope;
    Knob widthD, widthMid, widthHigh, monoFreq, satLow, satHigh;

    // LIMITER
    LimiterView limView;
    Combo targetL, limModeL;
    Knob ceilingL, clip, outputL;
    DriveDial driveL;

    // RESONANZ
    ClashDisplay clash;
    juce::ToggleButton resOn { "AKTIV" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> resOnAtt;
    Knob resDepth, resSharp, resSens, resAttack, resRelease;
};

class AuricMaster2Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AuricMaster2Editor (AuricMaster2Processor&);
    ~AuricMaster2Editor() override;
    void resized() override;
    MainView& getView() { return view; }

private:
    void timerCallback() override;

    AuricLookAndFeel lnf;
    MainView view;
    juce::TooltipWindow tooltips { this, 600 };
    int frame = 0;
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuricMaster2Editor)
};
