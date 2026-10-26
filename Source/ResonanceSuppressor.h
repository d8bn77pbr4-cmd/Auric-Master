#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

// One adjustable "frequency clash" band: scales how hard resonances are suppressed in its area
struct ClashNode
{
    enum Type { bell = 0, lowShelf = 1, highShelf = 2 };
    float freq = 1000.0f;   // Hz
    float amount = 0.0f;    // -1 (ignore area) .. +1 (double sensitivity)
    float widthOct = 1.0f;  // bell width / shelf slope in octaves
    int type = bell;
};

static constexpr int numClashBands = 6;
using ClashNodes = std::array<ClashNode, numClashBands>;

// Sensitivity weighting across frequency (shared by DSP and display so both match exactly)
inline float clashWeight (float freq, const ClashNodes& nodes)
{
    float w = 1.0f;
    for (const auto& n : nodes)
    {
        const float o = std::log2 (freq / n.freq);
        switch (n.type)
        {
            case ClashNode::lowShelf:  w += n.amount / (1.0f + std::exp (o / (0.25f * n.widthOct))); break;
            case ClashNode::highShelf: w += n.amount / (1.0f + std::exp (-o / (0.25f * n.widthOct))); break;
            default:
            {
                const float d = o / (0.5f * n.widthOct);
                w += n.amount * std::exp (-0.5f * d * d);
            }
        }
    }
    return juce::jlimit (0.0f, 3.0f, w);
}

// Dynamic spectral resonance suppressor (STFT, stereo-linked).
// Detects bins that stick out of the smoothed spectrum and turns them down dynamically.
class ResonanceSuppressor
{
public:
    static constexpr int numDisplayPoints = 400;
    static constexpr float maxReductionDb = 24.0f;

    struct Settings
    {
        bool enabled = true, delta = false, hard = false;
        float depth = 0.3f, sharpness = 0.5f, sensitivity = 0.5f;
        float attackMs = 5.0f, releaseMs = 80.0f, mix = 1.0f, trimDb = 0.0f;
        ClashNodes nodes;
    };

    void prepare (double sampleRate, int numChannels);
    void reset();
    void setSettings (const Settings& s) { settings = s; }
    int getLatency() const { return fftSize; }
    void process (juce::AudioBuffer<float>& buffer, int numChannels);

    // Editor side: copy the latest live spectrum (dB, tilt-compensated) and reduction (dB, positive) curves
    void copyDisplay (std::array<float, numDisplayPoints>& spectrum, std::array<float, numDisplayPoints>& reduction);
    static float displayPointToFreq (float i) { return 20.0f * std::pow (1000.0f, i / (numDisplayPoints - 1)); }

    // --- Auto-match learning (thread-safe handshake between message and audio thread) ---
    void requestLearn (float seconds) { learnSeconds = seconds; learnRequested.store (true); learnFinished.store (false); }
    void cancelLearn() { learnRequested.store (false); learning.store (false); }
    bool isLearning() const { return learning.load() || learnRequested.load(); }
    float getLearnProgress() const { return learnProgress.load(); }
    bool consumeLearnResult() { return learnFinished.exchange (false); }
    // valid after consumeLearnResult() returned true (audio thread no longer writes them)
    const std::vector<double>& getLearnPower() const { return learnPower; }
    const std::vector<double>& getLearnActivity() const { return learnActivity; }
    const std::vector<float>& getBinFrequencies() const { return binFreq; }

private:
    void processFrame();
    void updateWeights();
    static void boxSmooth (const std::vector<float>& in, std::vector<float>& out, std::vector<double>& prefix, float halfWidthFactor);

    double sr = 44100.0;
    int fftOrder = 11, fftSize = 2048, hop = 256, numCh = 2, pos = 0, hopCounter = 0;
    float olaNorm = 1.0f, ampNorm = 1.0f;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;
    std::vector<std::vector<float>> inRing, outRing, fftData;
    std::vector<float> magDb, ref1, ref2, grDb, grSmooth, grTmp, weights, binFreq;
    std::vector<double> prefix;
    Settings settings;
    ClashNodes cachedNodes;
    bool weightsValid = false;
    juce::SmoothedValue<float> mixS, trimS;

    std::atomic<bool> learnRequested { false }, learning { false }, learnFinished { false };
    std::atomic<float> learnProgress { 0.0f };
    float learnSeconds = 10.0f;
    int learnFrames = 0, learnTargetFrames = 1;
    std::vector<double> learnPower, learnActivity;

    juce::SpinLock displayLock;
    std::array<float, numDisplayPoints> dispSpectrum {}, dispReduction {};
    std::array<float, numDisplayPoints> dispBinLo {}, dispBinHi {};
};
