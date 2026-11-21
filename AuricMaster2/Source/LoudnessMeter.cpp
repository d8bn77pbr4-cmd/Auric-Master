#include "LoudnessMeter.h"

namespace
{
    inline float energyToLufs (double z) { return z > 0.0 ? (float) (-0.691 + 10.0 * std::log10 (z)) : -200.0f; }
    inline double lufsToEnergy (double l) { return std::pow (10.0, (l + 0.691) / 10.0); }

    double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
        }
        return sum;
    }
}

void LoudnessMeter::kWeightingCoefficients (double fs, double shelf[5], double highPass[5])
{
    // Analog prototypes of the BS.1770 K-weighting stages; at 48 kHz this yields the coefficients
    // published in the recommendation, and at other rates the same response.
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        shelf[0] = (Vh + Vb * K / Q + K * K) / a0;
        shelf[1] = 2.0 * (K * K - Vh) / a0;
        shelf[2] = (Vh - Vb * K / Q + K * K) / a0;
        shelf[3] = 2.0 * (K * K - 1.0) / a0;
        shelf[4] = (1.0 - K / Q + K * K) / a0;
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double a0 = 1.0 + K / Q + K * K;
        highPass[0] = 1.0; highPass[1] = -2.0; highPass[2] = 1.0;
        highPass[3] = 2.0 * (K * K - 1.0) / a0;
        highPass[4] = (1.0 - K / Q + K * K) / a0;
    }
}

void LoudnessMeter::prepare (double sampleRate, int numChannels, bool withTruePeak)
{
    sr = sampleRate;
    numCh = juce::jlimit (1, Biquad::maxChannels, numChannels);
    truePeakOn = withTruePeak;
    subLen = juce::jmax (1, (int) std::round (0.1 * sr));

    double s[5], h[5];
    kWeightingCoefficients (sr, s, h);
    kShelf.setRaw (s[0], s[1], s[2], s[3], s[4]);
    kHigh.setRaw (h[0], h[1], h[2], h[3], h[4]);

    // 4x interpolation filter for true peak: Kaiser-windowed sinc, passband to 0.45 fs
    const int N = tpPhases * tpTaps;
    const double beta = 8.0, cutoff = 0.9, centre = (N - 1) / 2.0;
    std::array<double, tpPhases> phaseSum {};
    std::vector<double> hcoef ((size_t) N);
    for (int n = 0; n < N; ++n)
    {
        const double t = (n - centre) / tpPhases;
        const double x = juce::MathConstants<double>::pi * cutoff * t;
        const double sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (x) / x;
        const double r = (n - centre) / centre;
        const double w = besselI0 (beta * std::sqrt (juce::jmax (0.0, 1.0 - r * r))) / besselI0 (beta);
        hcoef[(size_t) n] = sinc * w;
        phaseSum[(size_t) (n % tpPhases)] += hcoef[(size_t) n];
    }
    for (int n = 0; n < N; ++n)
        tpCoeff[(size_t) (n % tpPhases)][(size_t) (n / tpPhases)] = (float) (hcoef[(size_t) n] / phaseSum[(size_t) (n % tpPhases)]);

    kShelf.reset();
    kHigh.reset();
    subRing.fill (0.0);
    subIdx = subCount = 0;
    subEnergy = 0.0;
    for (auto& hh : tpHist) hh.fill (0.0f);
    tpPos = 0;
    doReset();
}

void LoudnessMeter::doReset()
{
    blockEnergy.fill (0.0); stEnergy.fill (0.0);
    blockCount.fill (0); stCount.fill (0);
    subsSinceReset = 0;
    runEnergy = 0.0;
    runningValid.store (false);
    integrated.store (-100.0f);
    lra.store (0.0f);
    truePeakMax.store (-100.0f);
    measuredSeconds.store (0.0f);
    for (auto& v : history) v.store (-100.0f);
}

void LoudnessMeter::process (const juce::AudioBuffer<float>& buffer, int channels)
{
    if (resetRequested.exchange (false)) doReset();
    channels = juce::jmin (channels, numCh, buffer.getNumChannels());
    const int n = buffer.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        double z = 0.0;
        for (int ch = 0; ch < channels; ++ch)
        {
            const float x = buffer.getSample (ch, i);
            const float y = kHigh.process (kShelf.process (x, ch), ch);
            z += (double) y * y;

            if (truePeakOn)
            {
                auto& hist = tpHist[(size_t) ch];
                hist[(size_t) tpPos] = x;
                for (int p = 0; p < tpPhases; ++p)
                {
                    float acc = 0.0f;
                    const auto& c = tpCoeff[(size_t) p];
                    int idx = tpPos;
                    for (int k = 0; k < tpTaps; ++k)
                    {
                        acc += c[(size_t) k] * hist[(size_t) idx];
                        if (--idx < 0) idx = tpTaps - 1;
                    }
                    tpBlockMax = juce::jmax (tpBlockMax, std::abs (acc));
                }
            }
            else
                tpBlockMax = juce::jmax (tpBlockMax, std::abs (x));
        }
        if (truePeakOn && ++tpPos >= tpTaps) tpPos = 0;
        subEnergy += z;
        if (++subCount >= subLen) endSubBlock();
    }
}

void LoudnessMeter::endSubBlock()
{
    subRing[(size_t) subIdx] = subEnergy / subLen;   // mean square, summed over channels
    subIdx = (subIdx + 1) % (int) subRing.size();
    subEnergy = 0.0;
    subCount = 0;
    ++subsSinceReset;

    auto meanOfLast = [this] (int count)
    {
        double s = 0.0;
        for (int k = 1; k <= count; ++k) s += subRing[(size_t) ((subIdx - k + 300) % (int) subRing.size())];
        return s / count;
    };
    const double zM = meanOfLast (4), zS = meanOfLast (30);
    const float lM = energyToLufs (zM), lS = energyToLufs (zS);
    momentary.store (juce::jmax (-100.0f, lM));
    shortTerm.store (juce::jmax (-100.0f, lS));

    // true peak of this 100 ms
    const float tpDb = 20.0f * std::log10 (tpBlockMax + 1.0e-9f);
    truePeak.store (tpDb);
    if (tpDb > truePeakMax.load()) truePeakMax.store (tpDb);
    tpBlockMax = 0.0f;

    // integrated: 400 ms blocks every 100 ms (75 % overlap), absolute gate -70 LUFS
    if (subsSinceReset >= 4)
    {
        measuredSeconds.store (measuredSeconds.load() + 0.1f);
        if (lM > histLo)
        {
            const int b = juce::jlimit (0, bins - 1, (int) ((lM - histLo) / histStep));
            blockEnergy[(size_t) b] += zM;
            ++blockCount[(size_t) b];
        }
        double sumE = 0.0; juce::int64 cnt = 0;
        for (int b = 0; b < bins; ++b) { sumE += blockEnergy[(size_t) b]; cnt += blockCount[(size_t) b]; }
        if (cnt > 0)
        {
            const float relGate = energyToLufs (sumE / (double) cnt) - 10.0f;
            double sumG = 0.0; juce::int64 cntG = 0;
            for (int b = 0; b < bins; ++b)
                if (histLo + (b + 0.5f) * histStep > relGate) { sumG += blockEnergy[(size_t) b]; cntG += blockCount[(size_t) b]; }
            if (cntG > 0) integrated.store (energyToLufs (sumG / (double) cntG));
        }

        // running (automation / level-matched compare): gated exponential average, ~3 s memory
        const bool valid = runningValid.load();
        if (lM > histLo && (! valid || lM > energyToLufs (runEnergy) - 10.0f))
        {
            const double c = std::exp (-0.1 / 3.0);
            runEnergy = valid ? c * runEnergy + (1.0 - c) * zM : zM;
            running.store (energyToLufs (runEnergy));
            runningValid.store (true);
        }
    }

    // loudness range from short-term values (needs 3 s)
    if (subsSinceReset >= 30)
    {
        if (lS > histLo)
        {
            const int b = juce::jlimit (0, bins - 1, (int) ((lS - histLo) / histStep));
            stEnergy[(size_t) b] += zS;
            ++stCount[(size_t) b];
        }
        double sumE = 0.0; juce::int64 cnt = 0;
        for (int b = 0; b < bins; ++b) { sumE += stEnergy[(size_t) b]; cnt += stCount[(size_t) b]; }
        if (cnt > 0)
        {
            const float relGate = energyToLufs (sumE / (double) cnt) - 20.0f;
            juce::int64 total = 0;
            for (int b = 0; b < bins; ++b) if (histLo + (b + 0.5f) * histStep > relGate) total += stCount[(size_t) b];
            if (total > 1)
            {
                const double p10 = 0.10 * (double) (total - 1), p95 = 0.95 * (double) (total - 1);
                float l10 = 0.0f, l95 = 0.0f;
                juce::int64 run = 0;
                bool got10 = false;
                for (int b = 0; b < bins; ++b)
                {
                    if (histLo + (b + 0.5f) * histStep <= relGate) continue;
                    run += stCount[(size_t) b];
                    const float centre = histLo + (b + 0.5f) * histStep;
                    if (! got10 && (double) run > p10) { l10 = centre; got10 = true; }
                    if ((double) run > p95) { l95 = centre; break; }
                }
                lra.store (juce::jmax (0.0f, l95 - l10));
            }
        }
    }

    const int hp = (historyPos.load() + 1) % historySize;
    history[(size_t) hp].store (juce::jmax (-100.0f, lS));
    historyPos.store (hp);
}

void LoudnessMeter::copyHistory (std::array<float, historySize>& out, int& newest) const
{
    newest = historyPos.load();
    for (int i = 0; i < historySize; ++i) out[(size_t) i] = history[(size_t) i].load();
}
