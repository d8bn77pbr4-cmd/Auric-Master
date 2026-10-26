#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>

// One adjustable "frequency clash" zone: scales how hard resonances are suppressed around 'freq'
struct ClashNode
{
    float freq = 1000.0f;   // Hz
    float amount = 0.0f;    // -1 (ignore zone) .. +1 (double sensitivity)
    float widthOct = 1.0f;  // zone width in octaves
};

using ClashNodes = std::array<ClashNode, 4>;

// Sensitivity weighting across frequency (shared by DSP and display so both match exactly)
inline float clashWeight (float freq, const ClashNodes& nodes)
{
    float w = 1.0f;
    for (const auto& n : nodes)
    {
        const float d = std::log2 (freq / n.freq) / (0.5f * n.widthOct);
        w += n.amount * std::exp (-0.5f * d * d);
    }
    return juce::jlimit (0.0f, 3.0f, w);
}

// Dynamic spectral resonance suppressor (STFT, stereo-linked).
// Detects bins that stick out of the smoothed spectrum and turns them down dynamically.
class ResonanceSuppressor
{
public:
    static constexpr int numDisplayPoints = 256;
    static constexpr float maxReductionDb = 24.0f;

    struct Settings
    {
        bool enabled = true, delta = false;
        float depth = 0.3f, sharpness = 0.5f, sensitivity = 0.5f, speed = 0.5f;
        ClashNodes nodes;
    };

    void prepare (double sampleRate, int numChannels);
    void reset();
    void setSettings (const Settings& s) { settings = s; }
    int getLatency() const { return fftSize; }
    void process (juce::AudioBuffer<float>& buffer, int numChannels);

    // Editor side: copy the latest spectrum (dBFS) and reduction (dB, positive) curves
    void copyDisplay (std::array<float, numDisplayPoints>& spectrum, std::array<float, numDisplayPoints>& reduction);
    static float displayPointToFreq (int i) { return 20.0f * std::pow (1000.0f, (float) i / (numDisplayPoints - 1)); }

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
    std::vector<float> magDb, ref1, ref2, grDb, grSmooth, weights, binFreq;
    std::vector<double> prefix;
    Settings settings;
    ClashNodes cachedNodes;
    bool weightsValid = false;

    juce::SpinLock displayLock;
    std::array<float, numDisplayPoints> dispSpectrum {}, dispReduction {};
    std::array<int, numDisplayPoints> dispBin {};
};
