#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>

// RBJ biquad with per-channel state; coefficients can change at control rate without allocation
struct Biquad
{
    static constexpr int maxChannels = 2;

    void reset() { for (auto& s : state) s = { 0.0f, 0.0f }; }

    void setPeak (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = w0 (fs, f);
        const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }
    void setLowShelf (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = w0 (fs, f);
        const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w), sA = 2.0 * std::sqrt (A) * alpha;
        set (A * ((A + 1) - (A - 1) * c + sA), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sA),
             (A + 1) + (A - 1) * c + sA, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sA);
    }
    void setHighShelf (double fs, double f, double q, double gainDb)
    {
        const double A = std::pow (10.0, gainDb / 40.0), w = w0 (fs, f);
        const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w), sA = 2.0 * std::sqrt (A) * alpha;
        set (A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
             (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
    }
    void setLowPass (double fs, double f, double q)
    {
        const double w = w0 (fs, f), alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set ((1 - c) * 0.5, 1 - c, (1 - c) * 0.5, 1 + alpha, -2 * c, 1 - alpha);
    }
    void setHighPass (double fs, double f, double q)
    {
        const double w = w0 (fs, f), alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + alpha, -2 * c, 1 - alpha);
    }
    void setAllPass (double fs, double f, double q)
    {
        const double w = w0 (fs, f), alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        set (1 - alpha, -2 * c, 1 + alpha, 1 + alpha, -2 * c, 1 - alpha);
    }
    void setRaw (double nb0, double nb1, double nb2, double na1, double na2) { set (nb0, nb1, nb2, 1.0, na1, na2); }

    // magnitude response in dB at frequency f (for displays)
    double magnitudeDb (double fs, double f) const
    {
        const double w = juce::MathConstants<double>::twoPi * f / fs, c1 = std::cos (w), c2 = std::cos (2 * w), s1 = std::sin (w), s2 = std::sin (2 * w);
        const double nr = b0 + b1 * c1 + b2 * c2, ni = -(b1 * s1 + b2 * s2);
        const double dr = 1.0 + a1 * c1 + a2 * c2, di = -(a1 * s1 + a2 * s2);
        return 10.0 * std::log10 ((nr * nr + ni * ni) / (dr * dr + di * di) + 1.0e-30);
    }

    float process (float x, int ch)
    {
        auto& s = state[(size_t) ch];
        const float y = b0 * x + s[0];
        s[0] = b1 * x - a1 * y + s[1];
        s[1] = b2 * x - a2 * y;
        return y;
    }

private:
    static double w0 (double fs, double f) { return juce::MathConstants<double>::twoPi * juce::jlimit (1.0, fs * 0.49, f) / fs; }
    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    std::array<std::array<float, 2>, maxChannels> state {};
};

// 4th-order Linkwitz-Riley split (two cascaded Butterworth sections per side).
// low + high = 2nd-order all-pass at the crossover frequency: flat magnitude when the bands are summed.
struct LR4Split
{
    void set (double fs, double f)
    {
        for (auto* b : { &lp1, &lp2 }) b->setLowPass (fs, f, 0.70710678);
        for (auto* b : { &hp1, &hp2 }) b->setHighPass (fs, f, 0.70710678);
    }
    void reset() { for (auto* b : { &lp1, &lp2, &hp1, &hp2 }) b->reset(); }
    void process (float x, int ch, float& low, float& high)
    {
        low = lp2.process (lp1.process (x, ch), ch);
        high = hp2.process (hp1.process (x, ch), ch);
    }
    float highOnly (float x, int ch) { return hp2.process (hp1.process (x, ch), ch); }

private:
    Biquad lp1, lp2, hp1, hp2;
};

// Integer-delay circular buffer (latency compensation for the dry / compare path)
class SimpleDelay
{
public:
    void prepare (int numChannels, int maxDelay)
    {
        buffer.setSize (numChannels, maxDelay + 1);
        buffer.clear();
        writePos = 0;
    }
    void setDelay (int d) { delay = juce::jlimit (0, buffer.getNumSamples() - 1, d); }

    void process (juce::AudioBuffer<float>& io, int numChannels)
    {
        const int size = buffer.getNumSamples();
        const int chans = juce::jmin (numChannels, buffer.getNumChannels());
        int wp = writePos;
        for (int i = 0; i < io.getNumSamples(); ++i)
        {
            int rp = wp - delay;
            if (rp < 0) rp += size;
            for (int ch = 0; ch < chans; ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                d[wp] = io.getSample (ch, i);
                io.setSample (ch, i, d[rp]);
            }
            if (++wp >= size) wp = 0;
        }
        writePos = wp;
    }

private:
    juce::AudioBuffer<float> buffer;
    int writePos = 0, delay = 0;
};

inline float dbToGain (float db) { return std::exp (db * 0.11512925f); }
inline float powerToDb (float p) { return 10.0f * std::log10 (p + 1.0e-20f); }
