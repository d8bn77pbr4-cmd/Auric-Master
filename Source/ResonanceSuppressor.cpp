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
    for (auto* v : { &magDb, &ref1, &ref2, &grDb, &grSmooth, &grTmp, &weights, &binFreq })
        v->assign (bins, 0.0f);
    prefix.assign (bins + 1, 0.0);
    for (size_t b = 0; b < bins; ++b)
        binFreq[b] = (float) ((double) b * sr / fftSize);

    // each display point covers the bins between its neighbours' geometric midpoints
    for (int i = 0; i < numDisplayPoints; ++i)
    {
        const float lo = displayPointToFreq ((float) i - 0.5f), hi = displayPointToFreq ((float) i + 0.5f);
        dispBinLo[(size_t) i] = juce::jlimit (1.0f, (float) bins - 1.0f, (float) (lo * fftSize / sr));
        dispBinHi[(size_t) i] = juce::jlimit (1.0f, (float) bins - 1.0f, (float) (hi * fftSize / sr));
    }

    mixS.reset (sampleRate, 0.03);
    trimS.reset (sampleRate, 0.03);
    mixS.setCurrentAndTargetValue (settings.mix);
    trimS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (settings.trimDb));

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
    mixS.setTargetValue (settings.mix);
    trimS.setTargetValue (juce::Decibels::decibelsToGain (settings.trimDb));

    for (int i = 0; i < n; ++i)
    {
        const float mix = mixS.getNextValue();
        const float trim = trimS.getNextValue();
        for (int ch = 0; ch < channels; ++ch)
        {
            auto& in = inRing[(size_t) ch];
            auto& out = outRing[(size_t) ch];
            const float dry = in[(size_t) pos];   // input from exactly fftSize samples ago
            const float wet = out[(size_t) pos];
            out[(size_t) pos] = 0.0f;
            in[(size_t) pos] = buffer.getSample (ch, i);
            buffer.setSample (ch, i, delta ? dry - wet : (dry + (wet - dry) * mix) * trim);
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
                    || ! juce::exactlyEqual (cachedNodes[i].widthOct, settings.nodes[i].widthOct)
                    || cachedNodes[i].type != settings.nodes[i].type;
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
    const float attC = std::exp (-frameSec / juce::jmax (0.0005f, settings.attackMs * 0.001f));
    const float relC = std::exp (-frameSec / juce::jmax (0.001f, settings.releaseMs * 0.001f));

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

    // frequency smoothing of the gain curve: hard = 1 pass (precise), soft = 3 passes (gentler)
    grSmooth = grDb;
    for (int pass = 0; pass < (settings.hard ? 1 : 3); ++pass)
    {
        grTmp = grSmooth;
        for (int b = 1; b < bins - 1; ++b)
            grSmooth[(size_t) b] = 0.25f * grTmp[(size_t) b - 1] + 0.5f * grTmp[(size_t) b] + 0.25f * grTmp[(size_t) b + 1];
    }
    grSmooth[0] = 0.0f;

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

    // live display data (never blocks the audio thread)
    const juce::SpinLock::ScopedTryLockType lock (displayLock);
    if (lock.isLocked())
    {
        const float specFall = std::exp (-frameSec / 0.25f);
        const float redFall = std::exp (-frameSec / 0.12f);
        for (int i = 0; i < numDisplayPoints; ++i)
        {
            const float lo = dispBinLo[(size_t) i], hi = dispBinHi[(size_t) i];
            float spec, red;
            if (hi - lo < 1.0f)
            {
                // fewer than one bin per point: interpolate
                const float c = 0.5f * (lo + hi);
                const int b0 = juce::jmin ((int) c, bins - 2);
                const float f = c - (float) b0;
                spec = magDb[(size_t) b0] + f * (magDb[(size_t) b0 + 1] - magDb[(size_t) b0]);
                red = grSmooth[(size_t) b0] + f * (grSmooth[(size_t) b0 + 1] - grSmooth[(size_t) b0]);
            }
            else
            {
                spec = -200.0f; red = 0.0f;
                for (int b = (int) lo; b <= (int) hi; ++b)
                {
                    spec = juce::jmax (spec, magDb[(size_t) b]);
                    red = juce::jmax (red, grSmooth[(size_t) b]);
                }
            }
            // +4.5 dB/oct tilt so typical mixes look roughly flat
            spec += 4.5f * std::log2 (displayPointToFreq ((float) i) / 1000.0f);

            auto& s = dispSpectrum[(size_t) i];
            s = spec > s ? spec : spec + (s - spec) * specFall;
            auto& r = dispReduction[(size_t) i];
            r = red > r ? red : red + (r - red) * redFall;
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
