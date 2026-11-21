#pragma once

#include "DspUtils.h"
#include <atomic>

// Classic feed-forward compressor (log domain, soft knee). Deterministic: the output depends only on the
// settings and the recent signal (attack/release), never on how long the song has been playing.
//  - detector: short RMS (2 ms) of the linked (max of channels) power
//  - static curve: threshold, ratio, knee
//  - ballistics in the gain-reduction domain: attack / release, optional program-dependent auto release
class Compressor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        rmsC = std::exp (-1.0f / (0.002f * (float) fs));
        sustainC = std::exp (-1.0f / (0.4f * (float) fs));
        reset();
        setParams (thrDb, ratio, attMs, relMs, kneeDb, autoRelease);
    }

    void reset() { rms = 0.0f; grDb = 0.0f; sustain = 0.0f; }

    void setParams (float thresholdDb, float newRatio, float attackMs, float releaseMs, float knee, bool autoRel)
    {
        thrDb = thresholdDb;
        ratio = juce::jmax (1.0f, newRatio);
        slope = 1.0f / ratio - 1.0f;
        kneeDb = juce::jmax (0.0f, knee);
        autoRelease = autoRel;
        if (! juce::exactlyEqual (attackMs, attMs) || ! juce::exactlyEqual (releaseMs, relMs) || attC <= 0.0f)
        {
            attMs = attackMs; relMs = releaseMs;
            attC = std::exp (-1.0f / (juce::jmax (0.01f, attMs) * 0.001f * (float) fs));
            relC = std::exp (-1.0f / (juce::jmax (1.0f, relMs) * 0.001f * (float) fs));
            relFastC = std::exp (-1.0f / (juce::jmax (1.0f, relMs * 0.25f) * 0.001f * (float) fs));
            relSlowC = std::exp (-1.0f / (juce::jmax (1.0f, relMs * 2.5f) * 0.001f * (float) fs));
        }
    }

    // static curve: gain change in dB for an input level in dB (<= 0)
    float curve (float levelDb) const
    {
        const float over = levelDb - thrDb;
        if (2.0f * over < -kneeDb) return 0.0f;
        if (kneeDb > 0.0f && 2.0f * std::abs (over) <= kneeDb)
            return slope * (over + 0.5f * kneeDb) * (over + 0.5f * kneeDb) / (2.0f * kneeDb);
        return slope * over;
    }

    // detPower: linked power of this sample (e.g. max(L*L, R*R)); returns the linear gain to apply
    float process (float detPower)
    {
        rms = detPower + (rms - detPower) * rmsC;
        const float target = curve (powerToDb (rms));
        if (target < grDb)
            grDb = target + (grDb - target) * attC;
        else
        {
            float c = relC;
            if (autoRelease)
            {
                // fast release after short peaks, slow release while the compressor works continuously
                const float busy = grDb < -1.0f ? 1.0f : 0.0f;
                sustain = busy + (sustain - busy) * sustainC;
                c = relFastC + (relSlowC - relFastC) * sustain;
            }
            grDb = target + (grDb - target) * c;
        }
        return dbToGain (grDb);
    }

    float getGainReductionDb() const { return grDb; }
    float getDetectorDb() const { return powerToDb (rms); }

private:
    double fs = 48000.0;
    float thrDb = -12.0f, ratio = 2.0f, slope = -0.5f, kneeDb = 6.0f, attMs = 10.0f, relMs = 200.0f;
    float attC = 0.0f, relC = 0.0f, relFastC = 0.0f, relSlowC = 0.0f, rmsC = 0.0f, sustainC = 0.0f;
    float rms = 0.0f, grDb = 0.0f, sustain = 0.0f;
    bool autoRelease = false;
};

// 0 -> 1 ramp used to switch modules on and off without clicks
struct Ramp
{
    void prepare (double fs, float seconds) { step = 1.0f / juce::jmax (1.0f, seconds * (float) fs); }
    void setTarget (bool on) { target = on ? 1.0f : 0.0f; }
    void snap() { value = target; }
    float next()
    {
        if (value < target) value = juce::jmin (target, value + step);
        else if (value > target) value = juce::jmax (target, value - step);
        return value;
    }
    bool active() const { return value > 0.0f || target > 0.0f; }
    bool settled() const { return juce::exactlyEqual (value, target); }
    float value = 0.0f, target = 0.0f, step = 0.001f;
};
