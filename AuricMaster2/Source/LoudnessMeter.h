#pragma once

#include "DspUtils.h"
#include <atomic>

// Loudness meter after ITU-R BS.1770 / EBU R 128 (Tech 3341 / 3342):
//  - K-weighting (shelf + high-pass), coefficients derived for any sample rate
//  - momentary (400 ms) and short-term (3 s) loudness from 100 ms sub-blocks
//  - integrated loudness with absolute (-70 LUFS) and relative (-10 LU) gating, 75 % block overlap
//  - loudness range (LRA): 10th..95th percentile of gated short-term values (relative gate -20 LU)
//  - true peak via 4x polyphase interpolation
// Integrated and LRA use histograms (0.05 LU bins), so memory stays constant for any song length.
class LoudnessMeter
{
public:
    static constexpr int historySize = 600;   // 60 s of short-term values at 10 Hz

    void prepare (double sampleRate, int numChannels, bool withTruePeak);
    void requestReset() { resetRequested.store (true); }   // thread-safe: applied on the next audio block
    void process (const juce::AudioBuffer<float>& buffer, int numChannels);

    float getMomentary() const     { return momentary.load(); }
    float getShortTerm() const     { return shortTerm.load(); }
    float getIntegrated() const    { return integrated.load(); }     // -inf until gated blocks exist
    float getLra() const           { return lra.load(); }
    float getTruePeakDb() const    { return truePeak.load(); }       // last 100 ms
    float getTruePeakMaxDb() const { return truePeakMax.load(); }    // since reset
    float getRunning() const       { return running.load(); }        // gated, ~3 s memory (for automation)
    bool hasRunning() const        { return runningValid.load(); }
    float getMeasuredSeconds() const { return measuredSeconds.load(); }
    void copyHistory (std::array<float, historySize>& out, int& newest) const;

    static void kWeightingCoefficients (double fs, double shelf[5], double highPass[5]);   // b0 b1 b2 a1 a2

private:
    void endSubBlock();
    void doReset();

    static constexpr int bins = 1500;          // -70 .. +5 LUFS in 0.05 LU steps
    static constexpr float histLo = -70.0f, histStep = 0.05f;
    static constexpr int tpPhases = 4, tpTaps = 24;

    double sr = 48000.0;
    int numCh = 2, subLen = 4800, subCount = 0, subIdx = 0, subsSinceReset = 0;
    bool truePeakOn = true;
    Biquad kShelf, kHigh;
    double subEnergy = 0.0;
    std::array<double, 30> subRing {};
    std::array<double, bins> blockEnergy {}, stEnergy {};
    std::array<juce::int64, bins> blockCount {}, stCount {};
    double runEnergy = 0.0;
    float tpBlockMax = 0.0f;
    std::array<std::array<float, tpTaps>, tpPhases> tpCoeff {};
    std::array<std::array<float, tpTaps>, Biquad::maxChannels> tpHist {};
    int tpPos = 0;

    std::atomic<bool> resetRequested { false }, runningValid { false };
    std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f }, lra { 0.0f },
                       truePeak { -100.0f }, truePeakMax { -100.0f }, running { -100.0f }, measuredSeconds { 0.0f };
    std::array<std::atomic<float>, historySize> history {};
    std::atomic<int> historyPos { 0 };
};
