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

struct Toggle
{
    juce::ToggleButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
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

// Spectrum + EQ curve (4 bands, filters, tone). Interactive: drag nodes (freq/gain), wheel = Q, double-click = reset.
class EqCurveView : public juce::Component
{
public:
    EqCurveView (AuricMaster3Processor&, bool interactive);
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
    AuricMaster3Processor& proc;
    const bool interactive;
    std::array<Node, 4> nodes;
    std::array<float, ResonanceSuppressor::numDisplayPoints> spectrum {}, reduction {};
    std::array<float, points> curveMid {}, curveSide {}, curveStatic {};
    std::array<bool, 4> dynOn {};
    std::array<float, 4> dynNow {}, dynRange {};
    bool sideDiffers = false, moduleOn = true, anyDyn = false;
    int hovered = -1, dragging = -1;
};

// Three bands over frequency with live gain reduction and thresholds; crossover lines can be dragged
class DynamicsView : public juce::Component
{
public:
    explicit DynamicsView (AuricMaster3Processor&);
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
    AuricMaster3Processor& proc;
    std::unique_ptr<juce::ParameterAttachment> lowAtt, highAtt;
    float xLow = 150.0f, xHigh = 3000.0f;
    std::array<float, 3> gr {}, grHold {};
    std::array<float, ResonanceSuppressor::numDisplayPoints> spectrum {}, reduction {};
    int dragging = -1, hovered = -1;
};

// Glue compressor: transfer curve with the current level, and gain-reduction history
class GlueView : public juce::Component
{
public:
    explicit GlueView (AuricMaster3Processor& p) : proc (p) { hist.fill (0.0f); }
    void refresh();
    void paint (juce::Graphics&) override;

private:
    static constexpr int len = 300;   // 10 s at 30 Hz
    AuricMaster3Processor& proc;
    std::array<float, len> hist {};
    int pos = 0;
    float levelS = -100.0f;
};

// Vectorscope (mid up, side sideways) with correlation bar
class ScopeView : public juce::Component
{
public:
    explicit ScopeView (AuricMaster3Processor& p) : proc (p) {}
    void refresh() { corrS = corrS * 0.85f + proc.correlation.load() * 0.15f; repaint(); }
    void paint (juce::Graphics&) override;

private:
    AuricMaster3Processor& proc;
    float corrS = 1.0f;
};

// Loudness and gain-reduction history (UI-side, 30 Hz)
class LimiterView : public juce::Component
{
public:
    explicit LimiterView (AuricMaster3Processor& p) : proc (p) { st.fill (-100.0f); }
    void refresh();
    void paint (juce::Graphics&) override;

private:
    static constexpr int len = 600;   // 20 s at 30 Hz
    AuricMaster3Processor& proc;
    std::array<float, len> st {}, lim {}, clip {};
    int pos = 0;
};

// Right-hand panel: integrated loudness, M/S/LRA/TP, history, peaks, correlation, gain reduction
class LoudnessPanel : public juce::Component
{
public:
    explicit LoudnessPanel (AuricMaster3Processor&);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AuricMaster3Processor& proc;
    juce::TextButton resetButton { "RESET" };
    std::array<float, LoudnessMeter::historySize> histOut {}, histIn {};
    std::array<float, 6> grS {};
    std::array<float, 4> peakS {}, peakHold {};
    std::array<float, 90> tpRing {};   // true peak of the last 3 s (for PSR)
    int tpPos = 0;
    float corrS = 1.0f;
};

class TabButton : public juce::Button
{
public:
    TabButton (const juce::String& name, std::atomic<float>* moduleOn) : juce::Button (name), on (moduleOn) { setClickingTogglesState (false); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    std::atomic<float>* on = nullptr;   // module power (LED), nullptr for pages without a module
    bool shownOn = false;
};

// curved arrow icon (undo / redo)
class ArrowIconButton : public juce::Button
{
public:
    ArrowIconButton (const juce::String& name, bool redoDirection) : juce::Button (name), redo (redoDirection) {}
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
private:
    bool redo;
};

// preset list that refreshes itself just before it opens (user presets may change on disk)
class PresetBox : public juce::ComboBox
{
public:
    std::function<void()> beforePopup;
    void showPopup() override { if (beforePopup) beforePopup(); juce::ComboBox::showPopup(); }
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
    enum PageId { simple = 0, eq, dyn, glue, stereo, limit, res, numPages };

    explicit MainView (AuricMaster3Processor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick (int frame);   // 30 Hz
    void showPage (int index);
    bool keyPressed (const juce::KeyPress&) override;

private:
    void setupKnob (Knob&, juce::Component& parent, const char* id, const juce::String& name, const juce::String& tip, bool big = false);
    void setupCombo (Combo&, juce::Component& parent, const char* id, const juce::String& name, const juce::String& tip);
    void setupToggle (Toggle&, juce::Component& parent, const char* id, const juce::String& text, const juce::String& tip);
    static void placeKnob (Knob&, juce::Rectangle<int>);
    static void placeCombo (Combo&, juce::Rectangle<int>);
    void layoutSimple();
    void layoutEq();
    void layoutDyn();
    void layoutGlue();
    void layoutStereo();
    void layoutLimit();
    void layoutRes();
    void learnClicked();
    void rebuildPresetList();
    void presetChosen (int id);
    void stepPreset (int delta);
    void showPresetMenu();
    void savePresetDialog();

    AuricMaster3Processor& proc;
    int currentPage = 0;

    // header
    ArrowIconButton undoArrow { "undo", false }, redoArrow { "redo", true };
    PresetBox presetBox;
    juce::ArrowButton presetPrev { "prev", 0.5f, Pal::accent }, presetNext { "next", 0.0f, Pal::accent };
    juce::TextButton presetMenu { juce::String::fromUTF8 ("\xc2\xb7\xc2\xb7\xc2\xb7") };
    juce::Array<juce::File> userPresets;
    juce::String shownPreset;
    std::array<std::unique_ptr<SnapButton>, 4> snaps;
    Toggle matchButton, originalButton, monoButton, sideButton;
    juce::TextButton assistButton { "ASSISTENT" };
    Combo genre;
    std::array<std::unique_ptr<TabButton>, numPages> tabs;
    std::array<Page, numPages> pages;

    // footer
    juce::String status;
    juce::TextButton undoButton { juce::String::fromUTF8 ("R\xc3\x9c" "CKG\xc3\x84NGIG") };
    juce::uint32 learnStatusTime = 0;
    int lastLearnState = 0;

    LoudnessPanel loudness;

    // EINFACH
    std::array<Toggle, 7> chain;   // module switches in signal order
    EqCurveView simpleCurve;
    Knob toneLow, toneMid, toneHigh, glueThrS, widthS, input, output, mix, driveS, ceilingS, clipS;
    Combo targetS, limModeS;
    juce::TextButton learnS { "LERNEN" };
    juce::Label learnInfoS, learnInfoL;

    // EQ
    Toggle eqOn;
    EqCurveView eqCurve;
    Knob lowCut, highCut, eqDynAtt, eqDynRel;
    std::array<Knob, 4> eqFreq, eqGain, eqQ, eqThr, eqRange;
    std::array<Combo, 4> eqMode;
    std::array<Toggle, 4> eqDyn;

    // DYNAMIK
    Toggle dynOn;
    DynamicsView dynView;
    std::array<Knob, 3> mbThr, mbRatio, mbAtt, mbRel, mbGain;
    std::array<Toggle, 3> mbSolo, mbByp;
    std::array<Combo, 3> mbMode;
    Knob xLow, xHigh;

    // GLUE
    Toggle glueOn, glueAuto;
    GlueView glueView;
    Knob glueThr, glueRatio, glueAtt, glueRel, glueMakeup, glueMix;
    Combo glueHpf;

    // STEREO & FARBE
    Toggle stOn, satOn;
    ScopeView scope;
    Knob widthMid, widthHigh, monoFreq, satLow, satHigh;

    // LIMITER
    Toggle limOn;
    LimiterView limView;
    Combo targetL, limModeL;
    Knob driveL, ceilingL, clip, outputL;
    juce::TextButton learnL { "LERNEN" };

    // RESONANZ
    Toggle resOn, resDelta, limDelta;
    ClashDisplay clash;
    Knob resDepth, resSharp, resSens, resAttack, resRelease;
};

class AuricMaster3Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AuricMaster3Editor (AuricMaster3Processor&);
    ~AuricMaster3Editor() override;
    void resized() override;
    MainView& getView() { return view; }

private:
    void timerCallback() override;

    AuricLookAndFeel lnf;
    MainView view;
    juce::TooltipWindow tooltips { this, 600 };
    int frame = 0;
    bool sizeRestored = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuricMaster3Editor)
};
