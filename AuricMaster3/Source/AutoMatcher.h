#pragma once

#include "ResonanceSuppressor.h"

// Derives resonance-band settings from a learned long-term spectrum by comparing it
// with a reference tonal-balance curve (approximation of typical commercial masters).
namespace AutoMatcher
{
    enum Profile { pop = 0, hipHop, edm, rock, acoustic };
    inline juce::StringArray profileNames() { return { "Pop", "Hip-Hop / Trap", "EDM", "Rock", "Acoustic" }; }

    struct Band { float freq, amount, width; };   // amount in % (-100..100), width in octaves

    struct Result
    {
        std::array<Band, numClashBands> bands;
        float depth = 30.0f;                       // %
        std::array<float, ResonanceSuppressor::numDisplayPoints> deviation {}; // clash score vs. reference, dB
        juce::String summary;
    };

    // Target level in dB relative to 1 kHz (power density per FFT bin; masters roll off below ~22 Hz)
    float targetDb (float freq, int profile);

    // The target as the analysis FFT (Hann window) would measure it in each bin: the window's leakage
    // mixes neighbouring frequencies, which matters at the coarse low bins. Compare like with like.
    std::vector<double> targetBinPower (const std::vector<float>& binFreq, int profile);

    Result analyse (const std::vector<float>& binFreq, const std::vector<double>& power,
                    const std::vector<double>& activity, int profile,
                    const std::array<float, numClashBands>& defaultFreqs, const char* const* bandNames);
}
