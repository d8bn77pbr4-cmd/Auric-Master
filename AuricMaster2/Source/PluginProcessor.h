#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DspUtils.h"
#include "LoudnessMeter.h"
#include "Limiter.h"
#include "ResonanceSuppressor.h"
#include "AutoMatcher.h"

namespace Targets
{
    // loudness targets (integrated LUFS); index 0 = off (manual drive)
    inline juce::StringArray names() { return { "Aus (manuell)", "Streaming -14 LUFS", "Apple Music -16 LUFS", "Laut -11 LUFS", "Sehr laut -9 LUFS", "Club -7 LUFS" }; }
    inline float lufs (int index) { const float v[] = { 0.0f, -14.0f, -16.0f, -11.0f, -9.0f, -7.0f }; return v[juce::jlimit (0, 5, index)]; }
}

class AuricMaster2Processor : public juce::AudioProcessor
{
public:
    AuricMaster2Processor();
    ~AuricMaster2Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // ---- snapshots A-D (message thread) ----
    void selectSnapshot (int slot);
    int getActiveSnapshot() const { return activeSnapshot; }
    bool snapshotUsed (int slot) const { return snapshots[(size_t) slot].isValid(); }

    // ---- metering / display (read by the editor) ----
    LoudnessMeter inMeter, outMeter;
    ResonanceSuppressor resonance;
    std::atomic<float> grLow { 0.0f }, grMid { 0.0f }, grHigh { 0.0f }, grGlue { 0.0f }, grClip { 0.0f }, grLimit { 0.0f };
    std::atomic<float> driveDb { 0.0f }, correlation { 1.0f }, compareGainDb { 0.0f };
    std::atomic<bool> targetReached { false };

    static constexpr const char* clashNames[numClashBands] = { "LOW", "MUD", "BOX", "HARSH", "SIBILANCE", "HIGH" };
    static constexpr float clashDefaultFreqs[numClashBands] = { 80.0f, 250.0f, 600.0f, 3000.0f, 7500.0f, 12000.0f };
    static constexpr int clashTypes[numClashBands] = { ClashNode::lowShelf, ClashNode::bell, ClashNode::bell,
                                                       ClashNode::bell, ClashNode::bell, ClashNode::highShelf };

private:
    void processChunk (juce::AudioBuffer<float>& main, int chans);
    void updateControl (int numSamples);
    float p (int id) const { return ptr[(size_t) id]->load(); }
    std::vector<std::atomic<float>*> ptr;   // parameter values, indexed by the enum in the .cpp

    struct Dyn   // one compressor band: running-level relative threshold, so it works at any input level
    {
        float env = 0.0f, levelDb = -40.0f, gain = 1.0f, target = 1.0f, makeupDb = 0.0f, grDb = 0.0f;
        float attC = 0.0f, relC = 0.0f, outGain = 1.0f;
        void detect (float p) { env = p > env ? p + (env - p) * attC : p + (env - p) * relC; }
        void compute (float amount, float knee, float levelC, float makeupC, float trimDb);
    };

    double sr = 44100.0;
    int numCh = 2, blockCap = 512;
    static constexpr int osLog2 = 2, ctrl = 32;

    // EQ (mid/side domain: channel 0 = mid, 1 = side)
    Biquad lowCut1, lowCut2;
    std::array<Biquad, 4> eqMid, eqSide;
    std::array<float, 4> eqGainS {};
    std::array<int, 4> eqModeLast { -1, -1, -1, -1 };
    Biquad toneLowB, toneMidB, toneHighB;
    std::array<float, 3> toneS {};
    std::array<float, 4> eqFreqLast { -1, -1, -1, -1 }, eqQLast { -1, -1, -1, -1 };
    float lowCutLast = -1.0f;

    // multiband
    LR4Split split1, split2;
    Biquad apLow;
    float x1Last = -1.0f, x2Last = -1.0f;
    std::array<Dyn, 3> band;
    Dyn glue;
    Biquad glueSc;
    // saturation
    Biquad excPre, excPost;
    float excEnv = 0.0f;
    // stereo
    LR4Split monoSplit;
    float monoLast = -1.0f;
    float wMidS = 1.0f, wHighS = 1.0f;

    // loudness stage
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    SoftClipper clipper;
    MasterLimiter limiter;
    TruePeakGuard tpGuard;
    float driveS = 0.0f, autoDrive = 0.0f, holdoff = 0.0f;
    int lastTarget = -1;
    bool autoJumped = false;

    juce::SmoothedValue<float> inGainS, outGainS, bypassMix;
    float matchProcDb = 0.0f, matchOrigDb = 0.0f;
    SimpleDelay dryDelay;
    juce::AudioBuffer<float> dryBuffer;
    float corrLR = 0.0f, corrLL = 0.0f, corrRR = 0.0f;

    std::array<juce::ValueTree, 4> snapshots;
    int activeSnapshot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuricMaster2Processor)
};
