#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DspUtils.h"
#include "Dynamics.h"
#include "LoudnessMeter.h"
#include "Limiter.h"
#include "ResonanceSuppressor.h"
#include "AutoMatcher.h"

namespace Targets
{
    // loudness targets (integrated LUFS) for LERNEN; index 0 = none
    inline juce::StringArray names() { return { "Kein Ziel", "Streaming -14 LUFS", "Apple Music -16 LUFS", "Laut -11 LUFS", "Sehr laut -9 LUFS", "Club -7 LUFS" }; }
    inline float lufs (int index) { const float v[] = { 0.0f, -14.0f, -16.0f, -11.0f, -9.0f, -7.0f }; return v[juce::jlimit (0, 5, index)]; }
}

// Auric Master 3: modular mastering suite. Fixed chain, every module can be switched off:
//   Input -> RESONANZ (FFT) -> EQ + KLANG -> [crossover] DYNAMIK (multiband) / FARBE / STEREO -> GLUE
//   -> MIX (parallel, dry/wet) -> LIMITER (drive, clipper, true-peak limiter) -> Output
// Processing is deterministic: no gain riding, no level-following thresholds. Loudness targets are reached
// with LERNEN, which measures once and then holds a fixed drive.
class AuricMaster3Processor : public juce::AudioProcessor, private juce::Timer, private juce::AudioProcessorParameter::Listener
{
public:
    AuricMaster3Processor();
    ~AuricMaster3Processor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter ("bypass"); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // ---- undo / redo of user actions (editor gestures, presets, assistant, LERNEN); automation is not recorded ----
    void beginEdit();
    void endEdit();
    void undo();
    void redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    int getUndoDepth() const { return (int) undoStack.size(); }

    // ---- presets (message thread) ----
    static juce::StringArray getFactoryPresetNames();
    void loadFactoryPreset (int index);
    void setUserPresetDirectory (const juce::File& dir) { userPresetDir = dir; }
    juce::File getUserPresetDirectory() const { return userPresetDir; }
    juce::Array<juce::File> getUserPresetFiles() const;
    bool saveUserPreset (const juce::String& name);
    bool loadPresetFile (const juce::File& file);
    bool deleteUserPreset (const juce::File& file);
    juce::String getPresetName() const { return presetName; }
    int getFactoryPresetIndex() const { return factoryIndex; }   // -1 = user preset
    bool isPresetModified() const;

    // ---- snapshots A-D (message thread) ----
    void selectSnapshot (int slot);
    int getActiveSnapshot() const { return activeSnapshot; }
    bool snapshotUsed (int slot) const { return snapshots[(size_t) slot].isValid(); }

    // ---- LERNEN: measure the output loudness while the song plays, then set a fixed drive ----
    enum LearnState { learnIdle = 0, learnListening, learnDone, learnFailed };
    void startLoudnessLearn (float firstPassSeconds = 20.0f);
    void finishLoudnessLearn();                      // use what was measured so far
    int getLearnState() const { return learnState.load(); }
    float getLearnProgress() const;
    juce::String getLearnStatus() const { return learnStatus; }

    // ---- Auto-Master assistant: listens 15 s, sets all modules, then runs LERNEN ----
    static constexpr float assistSeconds = 15.0f;
    void startAssistant();
    bool isAutoLearning() const { return resonance.isLearning(); }
    float getAutoProgress() const { return resonance.getLearnProgress(); }
    const AutoMatcher::Result* getAutoResult() const { return hasAutoResult ? &autoResult : nullptr; }
    juce::uint32 getAutoResultTime() const { return autoResultTime; }
    juce::String getAssistSummary() const { return assistSummary; }
    bool canUndoAssistant() const { return preAssistState.isValid(); }
    int getAssistRuns() const { return assistRuns; }
    void undoAssistant();

    // ---- metering / display (read by the editor) ----
    LoudnessMeter inMeter, outMeter;
    ResonanceSuppressor resonance;
    std::atomic<float> grLow { 0.0f }, grMid { 0.0f }, grHigh { 0.0f }, grGlue { 0.0f }, grClip { 0.0f }, grLimit { 0.0f };
    std::atomic<float> grLimitPeak { 0.0f }, grClipPeak { 0.0f }, grGluePeak { 0.0f };   // peak since the editor last read them (exchange)
    std::atomic<float> glueInDb { -100.0f };                         // glue detector level (for the transfer curve)
    std::array<std::atomic<float>, 2> inPeak {}, outPeak {};         // sample peak since last read (exchange)
    std::array<std::atomic<float>, 4> eqDynGainDb {};                 // current dynamic EQ gain per band (display)
    std::atomic<float> correlation { 1.0f }, compareGainDb { 0.0f };
    std::atomic<int> latencySamplesNow { 0 };
    std::atomic<bool> analyserWanted { false };   // editor open: run the spectrum analysis even with RESONANZ off

    static constexpr int scopeSize = 1024;
    std::array<std::atomic<float>, scopeSize * 2> scope {};
    std::atomic<int> scopePos { 0 };

    std::atomic<int> uiWidth { 0 }, uiPage { 0 };

    static constexpr const char* clashNames[numClashBands] = { "LOW", "MUD", "BOX", "HARSH", "SIBILANCE", "HIGH" };
    static constexpr float clashDefaultFreqs[numClashBands] = { 80.0f, 250.0f, 600.0f, 3000.0f, 7500.0f, 12000.0f };
    static constexpr int clashTypes[numClashBands] = { ClashNode::lowShelf, ClashNode::bell, ClashNode::bell,
                                                       ClashNode::bell, ClashNode::bell, ClashNode::highShelf };
    static constexpr const char* bandIds[3] = { "low", "mid", "high" };

private:
    enum ConfigBits { cfgRes = 1, cfgLim = 2, cfgHq = 4 };
    int wantedConfig() const;

    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int index, bool starting) override;
    std::vector<float> captureValues() const;
    void restoreValues (const std::vector<float>& values);
    bool differs (const std::vector<float>& a, const std::vector<float>& b) const;
    void applyPresetValues (const std::function<float (const juce::String& id, float defaultValue)>& valueFor);
    std::vector<bool> excluded;   // monitoring / context parameters: not part of presets or undo
    std::vector<std::vector<float>> undoStack, redoStack;
    std::vector<float> pendingEdit, presetValues;
    int editDepth = 0, activeGestures = 0, factoryIndex = 0;
    bool restoring = false;
    juce::File userPresetDir;
    juce::String presetName { "Neutral (alles aus)" };

    void timerCallback() override;
    void processChunk (juce::AudioBuffer<float>& main, int chans);
    void updateControl (int numSamples);
    void applyConfig (int config);
    int latencyFor (int config) const;
    void resetStates();
    void applyAssistant();
    float p (int id) const { return ptr[(size_t) id]->load(); }
    void setParam (const char* id, float value);
    std::vector<std::atomic<float>*> ptr;

    double sr = 44100.0;
    int numCh = 2, blockCap = 512;
    static constexpr int ctrl = 32;

    // configuration of latency-affecting modules (RESONANZ, LIMITER): switched with a short fade
    int activeConfig = 0;
    float configFade = 1.0f, configFadeStep = 0.001f;
    int configHold = 0;   // samples of silence after a switch, until newly enabled stages deliver valid audio
    bool configSwitching = false;
    std::atomic<bool> latencyDirty { false };

    // module ramps (click-free on/off); the crossover only runs while a band module needs it,
    // so the plugin is bit-transparent when nothing is active
    Ramp eqRamp, dynRamp, satRamp, stRamp, glueRamp, bandRamp;
    bool bandRunning = false;

    // EQ (mid/side domain: channel 0 = mid, 1 = side)
    Biquad lowCut1, lowCut2, highCut1, highCut2;
    std::array<Biquad, 4> eqMid, eqSide;
    std::array<float, 4> eqGainS {};
    std::array<int, 4> eqModeLast { -1, -1, -1, -1 };
    std::array<float, 4> eqFreqLast { -1, -1, -1, -1 }, eqQLast { -1, -1, -1, -1 };
    Biquad toneLowB, toneMidB, toneHighB;
    std::array<float, 3> toneS {};
    float lowCutLast = -1.0f, highCutLast = -1.0f;
    // dynamic EQ
    std::array<Biquad, 4> eqDet;
    std::array<float, 4> eqDetLevel {}, eqDynDb {}, eqDetFreqLast {}, eqDetQLast {}, eqTotalLast {};
    std::array<double, 4> eqDetPow {};
    int lastCtrlLen = ctrl;

    // crossover + multiband
    LR4Split split1, split2;
    Biquad apLow;
    float x1Last = -1.0f, x2Last = -1.0f;
    std::array<Compressor, 3> bandComp;
    std::array<float, 3> bandGainS { 1.0f, 1.0f, 1.0f };
    std::array<float, 3> soloS { 1.0f, 1.0f, 1.0f };
    // colour
    Biquad excSq1, excSq2, excCu1, excCu2, excPost;
    float excEnv = 0.0f;
    // stereo
    LR4Split monoSplit;
    float monoLast = -1.0f, wMidS = 1.0f, wHighS = 1.0f;
    // glue
    Compressor glueComp;
    Biquad glueSc;
    int glueHpfLast = -1;
    float glueMakeupS = 1.0f, glueMixS = 1.0f;

    // limiter stage
    // two prepared limiter paths: normal and HQ (twice the oversampling); switching never allocates
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling, oversamplingHq;
    int osLog2 = 2, lookahead = 0, osLog2Hq = 3, lookaheadHq = 0;
    MasterLimiter limiterHq;
    SoftClipper clipper;
    MasterLimiter limiter;
    TruePeakGuard tpGuard;
    float driveS = 0.0f;

    juce::SmoothedValue<float> inGainS, outGainS, mixS, bypassMix;
    float matchProcDb = 0.0f, matchOrigDb = 0.0f;
    SimpleDelay origDelay, mixDelay, resDeltaDelay, limInDelay;
    juce::AudioBuffer<float> origBuffer, mixBuffer, resDeltaBuffer, limDeltaBuffer;
    float corrLR = 0.0f, corrLL = 0.0f, corrRR = 0.0f;

    // LERNEN
    LoudnessMeter learnMeter;
    std::atomic<int> learnState { learnIdle };
    std::atomic<bool> learnResetRequest { false }, learnFinishRequest { false }, learnResultReady { false };
    std::atomic<float> learnMeasured { -100.0f }, learnSignalSeconds { 0.0f };
    float learnPassSeconds = 20.0f, learnSkip = 0.0f;
    int learnPass = 0;
    juce::String learnStatus;

    // assistant
    bool hasAutoResult = false;
    AutoMatcher::Result autoResult;
    juce::uint32 autoResultTime = 0;
    juce::String assistSummary;
    juce::ValueTree preAssistState;
    int assistRuns = 0;
    bool assistLearnPending = false;
    std::vector<float> learnedFreqs;
    std::vector<double> learnedPower, learnedActivity;
    std::atomic<bool> assistCollect { false };
    double aLR = 0.0, aLL = 0.0, aRR = 0.0, aMlow = 0.0, aSlow = 0.0;
    float aPeak = 0.0f;
    Biquad aLow, aHigh, aGlueHp;
    static constexpr int histBins = 160;   // band level histograms, 0.5 dB steps from -80 dBFS
    std::array<std::array<int, histBins>, 4> levelHist {};

    std::array<juce::ValueTree, 4> snapshots;
    int activeSnapshot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuricMaster3Processor)
};
