#include "AutoMatcher.h"

namespace AutoMatcher
{
namespace
{
    struct ProfileShape { float slope, lowBoost, lowFreq, highShelf, highFreq; };

    // slope in dB/oct around 1 kHz, plus soft low and high shelves
    constexpr ProfileShape shapes[] = {
        { -4.5f,  1.5f, 100.0f, -1.5f, 12000.0f },   // Pop
        { -5.0f,  4.0f,  90.0f, -1.0f, 12000.0f },   // Hip-Hop / Trap
        { -4.5f,  3.0f, 100.0f,  0.5f, 10000.0f },   // EDM
        { -4.0f,  0.0f, 100.0f, -1.5f, 10000.0f },   // Rock
        { -3.5f, -2.0f, 120.0f, -2.0f, 12000.0f },   // Acoustic
    };

    // search range per band (LOW, MUD, BOX, HARSH, SIBILANCE, HIGH)
    constexpr float rangeLo[numClashBands] = { 25.0f, 120.0f, 400.0f, 1500.0f, 5000.0f, 9000.0f };
    constexpr float rangeHi[numClashBands] = { 120.0f, 400.0f, 1200.0f, 5000.0f, 10000.0f, 18000.0f };

    constexpr int gridPoints = 241;   // 1/24 octave from 20 Hz to 20 kHz
    float gridFreq (int i) { return 20.0f * std::pow (1000.0f, (float) i / (gridPoints - 1)); }
    int freqToGrid (float f) { return juce::jlimit (0, gridPoints - 1, (int) std::round (std::log (f / 20.0f) / std::log (1000.0f) * (gridPoints - 1))); }

    // average of 'values' over bins within +-halfOct around f (at least the nearest bin)
    double bandAverage (const std::vector<float>& binFreq, const std::vector<double>& values, float f, float halfOct)
    {
        const double binHz = binFreq.size() > 1 ? (double) binFreq[1] : 1.0;
        const int last = (int) binFreq.size() - 1;
        int lo = (int) std::floor (f * std::pow (2.0f, -halfOct) / binHz);
        int hi = (int) std::ceil (f * std::pow (2.0f, halfOct) / binHz);
        lo = juce::jlimit (1, last, lo);
        hi = juce::jlimit (lo, last, hi);
        double sum = 0.0;
        for (int b = lo; b <= hi; ++b) sum += values[(size_t) b];
        return sum / (hi - lo + 1);
    }

    std::vector<float> smoothGrid (const std::vector<float>& v, int radius)
    {
        std::vector<float> out (v.size());
        for (int i = 0; i < (int) v.size(); ++i)
        {
            double s = 0.0; int n = 0;
            for (int k = juce::jmax (0, i - radius); k <= juce::jmin ((int) v.size() - 1, i + radius); ++k) { s += v[(size_t) k]; ++n; }
            out[(size_t) i] = (float) (s / n);
        }
        return out;
    }

    juce::String freqText (float f)
    {
        return f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";
    }
}

float targetDb (float freq, int profile)
{
    const auto& p = shapes[juce::jlimit (0, 4, profile)];
    const float o = std::log2 (freq / 1000.0f);
    float db = p.slope * o;
    db += p.lowBoost / (1.0f + std::pow (freq / p.lowFreq, 2.0f));
    db += p.highShelf / (1.0f + std::pow (p.highFreq / freq, 2.0f));
    return db;
}

Result analyse (const std::vector<float>& binFreq, const std::vector<double>& power,
                const std::vector<double>& activity, int profile,
                const std::array<float, numClashBands>& defaultFreqs, const char* const* bandNames)
{
    Result r;

    // 1) learned spectrum + resonance activity on a 1/24-octave grid
    std::vector<float> mixDb ((size_t) gridPoints), act ((size_t) gridPoints), dev ((size_t) gridPoints);
    for (int i = 0; i < gridPoints; ++i)
    {
        const float f = gridFreq (i);
        mixDb[(size_t) i] = (float) (10.0 * std::log10 (bandAverage (binFreq, power, f, 1.0f / 12.0f) + 1.0e-20));
        act[(size_t) i] = (float) bandAverage (binFreq, activity, f, 1.0f / 12.0f);
    }
    mixDb = smoothGrid (mixDb, 3);   // ~1/4 octave smoothing: we want tonal build-ups, not single bins

    // 2) align with the target over the core range (median of the difference)
    std::vector<float> diffs;
    for (int i = freqToGrid (100.0f); i <= freqToGrid (10000.0f); ++i)
        diffs.push_back (mixDb[(size_t) i] - targetDb (gridFreq (i), profile));
    std::sort (diffs.begin(), diffs.end());
    const float offset = diffs[diffs.size() / 2];
    for (int i = 0; i < gridPoints; ++i)
        dev[(size_t) i] = mixDb[(size_t) i] - targetDb (gridFreq (i), profile) - offset;

    // activity: how much more "peaky" than average a region is
    float actMean = 0.0f;
    for (auto a : act) actMean += a;
    actMean /= (float) act.size();

    // 3) score: local build-ups (clashes) count fully, the broad tonal deviation from the
    //    reference only gently (overall balance is an EQ job), plus extra resonance activity
    const auto broad = smoothGrid (dev, 18);   // ~1.5 octave trend
    std::vector<float> score ((size_t) gridPoints);
    for (int i = 0; i < gridPoints; ++i)
    {
        const float local = dev[(size_t) i] - broad[(size_t) i];
        score[(size_t) i] = local + juce::jlimit (-2.5f, 2.5f, 0.35f * broad[(size_t) i]) + 0.7f * (act[(size_t) i] - actMean);
    }
    score = smoothGrid (score, 2);

    // 4) per band: find the worst spot within its range
    juce::StringArray found;
    float positiveSum = 0.0f;
    std::vector<bool> claimed ((size_t) gridPoints, false);   // grid points already handled by a band
    for (int bnd = 0; bnd < numClashBands; ++bnd)
    {
        const int lo = freqToGrid (rangeLo[bnd]), hi = freqToGrid (rangeHi[bnd]);
        const bool shelf = bnd == 0 || bnd == numClashBands - 1;

        float peakScore, freq, width = 1.0f;
        if (shelf)
        {
            double s = 0.0;
            for (int i = lo; i <= hi; ++i) s += score[(size_t) i];
            peakScore = (float) (s / (hi - lo + 1));
            freq = defaultFreqs[(size_t) bnd];
            if (peakScore >= 1.0f)
                for (int i = lo; i <= hi; ++i) claimed[(size_t) i] = true;
        }
        else
        {
            // clashes (build-ups) have priority; only if there is none, look for a thin spot to protect
            int best = -1;
            for (int i = lo; i <= hi; ++i)
                if (! claimed[(size_t) i] && (best < 0 || score[(size_t) i] > score[(size_t) best])) best = i;
            if (best < 0) best = lo;
            // a maximum at the edge of the range that belongs to the neighbour's bump is not a new clash
            // a maximum at the edge of the range whose bump continues into a neighbour is not this band's clash
            const bool lowEdge  = best <= lo + 2 && lo > 0 && (claimed[(size_t) lo - 1] || score[(size_t) lo - 1] > score[(size_t) best]);
            const bool highEdge = best >= hi - 2 && hi < gridPoints - 1 && score[(size_t) hi + 1] > score[(size_t) best];
            if (score[(size_t) best] < 1.0f || lowEdge || highEdge)
            {
                for (int i = lo; i <= hi; ++i)
                    if (! claimed[(size_t) i] && score[(size_t) i] < score[(size_t) best]) best = i;
                if (score[(size_t) best] > -1.0f) best = juce::jlimit (lo, hi, freqToGrid (defaultFreqs[(size_t) bnd]));
            }
            peakScore = score[(size_t) best];
            freq = gridFreq (best);

            // width: span where the score stays within 3 dB of the peak
            int a = best, b = best;
            const float limit = std::abs (peakScore) - 3.0f;
            while (a > 0 && std::abs (score[(size_t) a - 1]) > limit && (peakScore > 0) == (score[(size_t) a - 1] > 0)) --a;
            while (b < gridPoints - 1 && std::abs (score[(size_t) b + 1]) > limit && (peakScore > 0) == (score[(size_t) b + 1] > 0)) ++b;
            width = juce::jlimit (0.4f, 2.5f, (float) (b - a + 1) / 24.0f);
            if (std::abs (peakScore) >= 1.0f)
                for (int i = a; i <= b; ++i) claimed[(size_t) i] = true;
        }

        // +3 dB over target -> +40 %, below target -> protect (negative)
        float amount = std::abs (peakScore) < 1.0f ? 0.0f : juce::jlimit (-50.0f, 100.0f, peakScore * (peakScore > 0.0f ? 13.3f : 8.0f));
        if (juce::exactlyEqual (amount, 0.0f)) { width = 1.0f; freq = defaultFreqs[(size_t) bnd]; }
        r.bands[(size_t) bnd] = { freq, amount, width };
        if (peakScore > 0.0f) positiveSum += peakScore;

        if (std::abs (amount) >= 10.0f)
            found.add (juce::String (bandNames[bnd]) + " " + freqText (freq) + " " + (amount > 0 ? "+" : "") + juce::String (juce::roundToInt (amount)) + "%");
    }

    r.depth = juce::jlimit (20.0f, 60.0f, 25.0f + 2.0f * positiveSum + 3.0f * actMean);

    for (int i = 0; i < ResonanceSuppressor::numDisplayPoints; ++i)
        r.deviation[(size_t) i] = score[(size_t) freqToGrid (ResonanceSuppressor::displayPointToFreq ((float) i))];

    r.summary = found.isEmpty() ? juce::String ("keine deutlichen Clashes gefunden") : found.joinIntoString (juce::String::fromUTF8 ("  \xc2\xb7  "));
    return r;
}
}
