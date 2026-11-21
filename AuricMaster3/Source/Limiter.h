#pragma once

#include "DspUtils.h"
#include <atomic>

// Soft clipper (runs oversampled, before the limiter): shaves the fastest peaks so the limiter
// has to work less. Linear below the knee, then a smooth curve that approaches the threshold.
struct SoftClipper
{
    void setThreshold (float lin, bool enabled) { thr = lin; on = enabled; knee = 0.7f * lin; }
    float process (float x) const
    {
        if (! on) return x;
        const float a = std::abs (x);
        if (a <= knee) return x;
        const float range = thr - knee;
        const float y = knee + range * std::tanh ((a - knee) / range);
        return x < 0.0f ? -y : y;
    }
    float thr = 1.0f, knee = 0.7f;
    bool on = false;
};

// Final true-peak guard at the output sample rate. Limiting and clipping in the oversampled domain create
// content above the output Nyquist; removing it on the way down can push inter-sample peaks over the
// ceiling. The guard estimates the true peak of every sample with a 4x interpolation filter (as the meter
// does) and pulls the gain down with a short look-ahead, so the delivered file stays under the ceiling.
class TruePeakGuard
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();
    int getLatency() const { return delayLen; }
    void process (float* const* channels, int numChannels, int numSamples, float ceiling);
    float getGainReductionDb() const { return grDb.load(); }

private:
    static constexpr int phases = 4, taps = 24;
    std::array<std::array<float, taps>, phases> coeff {};
    std::array<std::array<float, taps>, Biquad::maxChannels> hist {};
    int histPos = 0, numCh = 2, delayLen = 1, window = 1, avgLen = 1, wp = 0, avgPos = 0;
    std::vector<std::vector<float>> delay;
    std::vector<float> avgRing;
    // sliding minimum over the window (monotonic deque): constant cost per sample at any sample rate
    std::vector<float> dqVal;
    std::vector<juce::int64> dqIdx;
    int dqHead = 0, dqSize = 0;
    juce::int64 t = 0;
    double avgSum = 1.0;
    float env = 1.0f, relC = 0.999f;
    std::atomic<float> grDb { 0.0f };
};

// Look-ahead true-peak limiter for the oversampled signal.
// Gain = exact sliding minimum of the required gain over the look-ahead window, smoothed by a moving
// average (attack, reaches the minimum before the peak leaves the delay) and a program-dependent release.
class MasterLimiter
{
public:
    enum Mode { transparent = 0, punchy, loud };

    void prepare (double rate, int numChannels, int lookaheadSamples);
    void reset();
    void setCeiling (float lin) { ceiling = lin; }
    void setMode (int m);
    void process (float* const* channels, int numChannels, int numSamples);
    float getGainReductionDb() const { return grDb.load(); }

private:
    void setAttackLength (int len);

    double fs = 192000.0;
    int numCh = 2, L = 384, wp = 0, mode = punchy;
    float ceiling = 1.0f, env = 1.0f, sustain = 0.0f, relBase = 0.07f, sustainC = 0.0f;
    std::vector<std::vector<float>> delay;
    // monotonic deque (ring) for the sliding minimum
    std::vector<float> dqVal;
    std::vector<juce::int64> dqIdx;
    int dqHead = 0, dqSize = 0;
    juce::int64 t = 0;
    // moving average (attack)
    std::vector<float> avgRing;
    int avgLen = 1, avgPos = 0;
    double avgSum = 1.0;
    std::atomic<float> grDb { 0.0f };
};
