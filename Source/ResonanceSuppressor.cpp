#include "ResonanceSuppressor.h"

void ResonanceSuppressor::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    numCh = juce::jmax (1, numChannels);
    fftOrder = sampleRate <= 50000.0 ? 11 : (sampleRate <= 100000.0 ? 12 : 13);
    fftSize = 1 << fftOrder;
    hop = fftSize / 8;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    window.resize ((size_t) fftSize);
    double sumW = 0.0, sumW2 = 0.0;
    for (int i = 0; i < fftSize; ++i)
    {
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);
        sumW += window[(size_t) i];
        sumW2 += window[(size_t) i] * window[(size_t) i];
    }
    olaNorm = (float) (hop / sumW2);   // analysis + synthesis window overlap-add gain
    ampNorm = (float) (2.0 / sumW);    // full-scale sine -> 0 dBFS

    inRing.assign ((size_t) numCh, std::vector<float> ((size_t) fftSize, 0.0f));
    outRing.assign ((size_t) numCh, std::vector<float> ((size_t) fftSize, 0.0f));
    fftData.assign ((size_t) numCh, std::vector<float> ((size_t) fftSize * 2, 0.0f));

    const size_t bins = (size_t) fftSize / 2 + 1;
    for (auto* v : { &magDb, &ref1, &ref2, &grDb, &grSmooth, &weights, &binFreq })
        v->assign (bins, 0.0f);
    prefix.assign (bins + 1, 0.0);
    for (size_t b = 0; b < bins; ++b)
        binFreq[b] = (float) ((double) b * sr / fftSize);

    for (int i = 0; i < numDisplayPoints; ++i)
        dispBin[(size_t) i] = juce::jlimit (1, (int) bins - 1, (int) std::round (displayPointToFreq (i) * fftSize / sr));

    weightsValid = false;
    reset();
}

void ResonanceSuppressor::reset()
{
    for (auto& v : inRing) std::fill (v.begin(), v.end(), 0.0f);
    for (auto& v : outRing) std::fill (v.begin(), v.end(), 0.0f);
    std::fill (grDb.begin(), grDb.end(), 0.0f);
    pos = hopCounter = 0;
    dispSpectrum.fill (-120.0f);
    dispReduction.fill (0.0f);
}

void ResonanceSuppressor::process (juce::AudioBuffer<float>& buffer, int channels)
{
    channels = juce::jmin (channels, numCh);
    const int n = buffer.getNumSamples();
    const bool delta = settings.delta;

    for (int i = 0; i < n; ++i)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            auto& in = inRing[(size_t) ch];
            auto& out = outRing[(size_t) ch];
            const float dry = in[(size_t) pos];   // input from exactly fftSize samples ago
            const float wet = out[(size_t) pos];
            out[(size_t) pos] = 0.0f;
            in[(size_t) pos] = buffer.getSample (ch, i);
            buffer.setSample (ch, i, delta ? dry - wet : wet);
        }
        if (++pos >= fftSize) pos = 0;
        if (++hopCounter >= hop)
        {
            hopCounter = 0;
            processFrame();
        }
    }
}

void ResonanceSuppressor::updateWeights()
{
    for (size_t b = 0; b < weights.size(); ++b)
        weights[b] = b == 0 ? 0.0f : clashWeight (binFreq[b], settings.nodes);
    cachedNodes = settings.nodes;
    weightsValid = true;
}

void ResonanceSuppressor::boxSmooth (const std::vector<float>& in, std::vector<float>& out,
                                     std::vector<double>& pre, float halfWidthFactor)
{
    const int bins = (int) in.size();
    pre[0] = 0.0;
    for (int b = 0; b < bins; ++b)
        pre[(size_t) b + 1] = pre[(size_t) b] + in[(size_t) b];

    for (int b = 1; b < bins; ++b)
    {
        const int h = juce::jmax (1, (int) std::round ((float) b * halfWidthFactor));
        const int lo = juce::jmax (1, b - h), hi = juce::jmin (bins - 1, b + h);
        out[(size_t) b] = (float) ((pre[(size_t) hi + 1] - pre[(size_t) lo]) / (hi - lo + 1));
    }
    out[0] = out[1];
}

void ResonanceSuppressor::processFrame()
{
    const int bins = fftSize / 2 + 1;
    const int mask = fftSize - 1;

    bool nodesChanged = ! weightsValid;
    for (size_t i = 0; i < cachedNodes.size() && ! nodesChanged; ++i)
        nodesChanged = ! juce::exactlyEqual (cachedNodes[i].freq, settings.nodes[i].freq)
                    || ! juce::exactlyEqual (cachedNodes[i].amount, settings.nodes[i].amount)
                    || ! juce::exactlyEqual (cachedNodes[i].widthOct, settings.nodes[i].widthOct);
    if (nodesChanged)
        updateWeights();

    // analysis
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& d = fftData[(size_t) ch];
        const auto& in = inRing[(size_t) ch];
        for (int k = 0; k < fftSize; ++k)
            d[(size_t) k] = in[(size_t) ((pos + k) & mask)] * window[(size_t) k];
        std::fill (d.begin() + fftSize, d.end(), 0.0f);
        fft->performRealOnlyForwardTransform (d.data(), true);
    }

    float frameMax = -200.0f;
    for (int b = 0; b < bins; ++b)
    {
        float m = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const auto& d = fftData[(size_t) ch];
            m = juce::jmax (m, std::hypot (d[(size_t) (2 * b)], d[(size_t) (2 * b + 1)]));
        }
        magDb[(size_t) b] = 20.0f * std::log10 (m * ampNorm + 1.0e-9f);
        if (b > 0) frameMax = juce::jmax (frameMax, magDb[(size_t) b]);
    }

    // reference spectrum: constant-Q smoothing; narrower window = only sharp resonances stick out
    const float octaves = juce::jmap (settings.sharpness, 2.0f, 0.15f);
    const float hw = std::pow (2.0f, octaves * 0.5f) - 1.0f;
    boxSmooth (magDb, ref1, prefix, hw);
    boxSmooth (ref1, ref2, prefix, hw);

    const float threshold = juce::jmap (settings.sensitivity, 12.0f, 1.0f);
    const float depth = settings.enabled ? settings.depth : 0.0f;
    const float frameSec = (float) hop / (float) sr;
    const float attTime = juce::jmap (settings.speed, 0.030f, 0.002f);
    const float relTime = juce::jmap (settings.speed, 0.300f, 0.030f);
    const float attC = std::exp (-frameSec / attTime);
    const float relC = std::exp (-frameSec / relTime);

    for (int b = 1; b < bins; ++b)
    {
        float target = 0.0f;
        const float m = magDb[(size_t) b];
        if (m > -100.0f && m > frameMax - 70.0f)
        {
            const float excess = m - ref2[(size_t) b] - threshold;
            if (excess > 0.0f)
                target = juce::jmin (maxReductionDb, excess * depth * weights[(size_t) b]);
        }
        auto& g = grDb[(size_t) b];
        g = target > g ? target + (g - target) * attC : target + (g - target) * relC;
    }
    grDb[0] = 0.0f;

    for (int b = 1; b < bins - 1; ++b)
        grSmooth[(size_t) b] = 0.25f * grDb[(size_t) b - 1] + 0.5f * grDb[(size_t) b] + 0.25f * grDb[(size_t) b + 1];
    grSmooth[0] = 0.0f;
    grSmooth[(size_t) bins - 1] = grDb[(size_t) bins - 1];

    // apply gains + synthesis
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& d = fftData[(size_t) ch];
        for (int b = 0; b < bins; ++b)
        {
            const float gain = juce::Decibels::decibelsToGain (-grSmooth[(size_t) b]);
            d[(size_t) (2 * b)] *= gain;
            d[(size_t) (2 * b + 1)] *= gain;
        }
        fft->performRealOnlyInverseTransform (d.data());

        auto& out = outRing[(size_t) ch];
        for (int k = 0; k < fftSize; ++k)
            out[(size_t) ((pos + k) & mask)] += d[(size_t) k] * window[(size_t) k] * olaNorm;
    }

    // display data (never blocks the audio thread)
    const juce::SpinLock::ScopedTryLockType lock (displayLock);
    if (lock.isLocked())
    {
        for (int i = 0; i < numDisplayPoints; ++i)
        {
            const int b = dispBin[(size_t) i];
            auto& s = dispSpectrum[(size_t) i];
            s = 0.75f * s + 0.25f * juce::jmax (-120.0f, magDb[(size_t) b]);
            dispReduction[(size_t) i] = grSmooth[(size_t) b];
        }
    }
}

void ResonanceSuppressor::copyDisplay (std::array<float, numDisplayPoints>& spectrum,
                                       std::array<float, numDisplayPoints>& reduction)
{
    const juce::SpinLock::ScopedLockType lock (displayLock);
    spectrum = dispSpectrum;
    reduction = dispReduction;
}
