#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "ResonanceSuppressor.h"

// Simple integer-delay circular buffer (used for dry-path latency compensation)
class SimpleDelay
{
public:
    void prepare (int numChannels, int maxDelay)
    {
        buffer.setSize (numChannels, maxDelay + 1);
        buffer.clear();
        writePos = 0;
    }
    void setDelay (int d) { delay = juce::jlimit (0, buffer.getNumSamples() - 1, d); }
    void reset() { buffer.clear(); writePos = 0; }

    void process (juce::AudioBuffer<float>& io)
    {
        const int size = buffer.getNumSamples();
        const int chans = juce::jmin (io.getNumChannels(), buffer.getNumChannels());
        int wp = writePos;
        for (int i = 0; i < io.getNumSamples(); ++i)
        {
            int rp = wp - delay;
            if (rp < 0) rp += size;
            for (int ch = 0; ch < chans; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                d[wp] = io.getSample (ch, i);
                io.setSample (ch, i, d[rp]);
            }
            if (++wp >= size) wp = 0;
        }
        writePos = wp;
    }

private:
    juce::AudioBuffer<float> buffer;
    int writePos = 0, delay = 0;
};

// Lookahead brickwall limiter (runs inside the oversampled domain)
class LookaheadLimiter
{
public:
    void prepare (double sampleRate, int numChannels, int lookaheadSamples);
    void reset();
    void setCeiling (float linear) { ceiling = linear; }
    void process (float* const* channels, int numChannels, int numSamples);
    float getGainReduction() const { return minGain; }   // lowest gain of last block
    int getLookahead() const { return lookahead; }

private:
    std::vector<std::vector<float>> delayBuf;
    std::vector<float> avgBuf;
    int lookahead = 1, writePos = 0, avgPos = 0, holdCounter = 0;
    double avgSum = 0.0;
    float hold = 1.0f, env = 1.0f, releaseCoeff = 0.999f, ceiling = 1.0f, minGain = 1.0f;
};

class AuricMasterProcessor : public juce::AudioProcessor
{
public:
    AuricMasterProcessor();
    ~AuricMasterProcessor() override = default;

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

    // Metering (read by the editor)
    std::atomic<float> inPeakL { 0.0f }, inPeakR { 0.0f }, outPeakL { 0.0f }, outPeakR { 0.0f };
    std::atomic<float> compGainDb { 0.0f }, limGainDb { 0.0f };
    ResonanceSuppressor resonance;

    static constexpr const char* clashNames[numClashBands] = { "LOW", "MUD", "BOX", "HARSH", "SIBILANCE", "HIGH" };
    static constexpr float clashDefaultFreqs[numClashBands] = { 80.0f, 250.0f, 600.0f, 3000.0f, 7500.0f, 12000.0f };
    static constexpr int clashTypes[numClashBands] = { ClashNode::lowShelf, ClashNode::bell, ClashNode::bell,
                                                       ClashNode::bell, ClashNode::bell, ClashNode::highShelf };

private:
    void updateFilters();

    static constexpr int osFactorLog2 = 2; // 4x oversampling
    using Filter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;

    Filter lowShelf, highShelf, lowCut;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    LookaheadLimiter limiter;
    SimpleDelay dryDelay;
    juce::AudioBuffer<float> dryBuffer;

    double sr = 44100.0;
    float compEnv = 0.0f, compGain = 1.0f;
    float lastBass = 999.0f, lastAir = 999.0f;

    juce::SmoothedValue<float> inGainS, outGainS, widthS, warmthS, mixS;

    std::atomic<float>* pInput;
    std::atomic<float>* pBass;
    std::atomic<float>* pAir;
    std::atomic<float>* pPunch;
    std::atomic<float>* pWarmth;
    std::atomic<float>* pWidth;
    std::atomic<float>* pCeiling;
    std::atomic<float>* pOutput;
    std::atomic<float>* pMix;
    std::atomic<float>* pBypass;
    std::atomic<float>* pResOn;
    std::atomic<float>* pResDelta;
    std::atomic<float>* pResDepth;
    std::atomic<float>* pResSharp;
    std::atomic<float>* pResSens;
    std::atomic<float>* pResAttack;
    std::atomic<float>* pResRelease;
    std::atomic<float>* pResMix;
    std::atomic<float>* pResTrim;
    std::atomic<float>* pResHard;
    std::array<std::atomic<float>*, numClashBands> pClashFreq {}, pClashAmt {}, pClashWidth {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuricMasterProcessor)
};
