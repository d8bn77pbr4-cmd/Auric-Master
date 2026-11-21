#include "Limiter.h"

namespace
{
    double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
        return sum;
    }
}

void TruePeakGuard::prepare (double sampleRate, int numChannels)
{
    numCh = juce::jlimit (1, Biquad::maxChannels, numChannels);
    // same Kaiser-windowed-sinc 4x interpolator as the loudness meter
    const int N = phases * taps;
    const double beta = 8.0, cutoff = 0.9, centre = (N - 1) / 2.0;
    std::array<double, phases> sum {};
    std::vector<double> h ((size_t) N);
    for (int n = 0; n < N; ++n)
    {
        const double t = (n - centre) / phases, x = juce::MathConstants<double>::pi * cutoff * t;
        const double r = (n - centre) / centre;
        h[(size_t) n] = (std::abs (x) < 1.0e-9 ? 1.0 : std::sin (x) / x) * besselI0 (beta * std::sqrt (juce::jmax (0.0, 1.0 - r * r))) / besselI0 (beta);
        sum[(size_t) (n % phases)] += h[(size_t) n];
    }
    for (int n = 0; n < N; ++n) coeff[(size_t) (n % phases)][(size_t) (n / phases)] = (float) (h[(size_t) n] / sum[(size_t) (n % phases)]);

    avgLen = juce::jmax (1, (int) std::round (0.0015 * sampleRate));   // 1.5 ms attack
    window = avgLen + taps;                                           // covers the interpolator's span
    delayLen = avgLen + taps / 2;
    delay.assign ((size_t) numCh, std::vector<float> ((size_t) delayLen + 1, 0.0f));
    win.assign ((size_t) window, 1.0f);
    avgRing.assign ((size_t) avgLen, 1.0f);
    relC = (float) std::exp (-1.0 / (0.05 * sampleRate));
    reset();
}

void TruePeakGuard::reset()
{
    for (auto& hh : hist) hh.fill (0.0f);
    for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
    std::fill (win.begin(), win.end(), 1.0f);
    std::fill (avgRing.begin(), avgRing.end(), 1.0f);
    histPos = wp = winPos = avgPos = 0;
    avgSum = (double) avgLen;
    env = 1.0f;
}

void TruePeakGuard::process (float* const* ch, int channels, int n, float ceiling)
{
    channels = juce::jmin (channels, numCh);
    const int dsize = delayLen + 1;
    float minEnv = 1.0f;
    for (int i = 0; i < n; ++i)
    {
        // true peak around this sample (max over the 4 interpolated phases, all channels)
        float tp = 0.0f;
        for (int c = 0; c < channels; ++c)
        {
            auto& hh = hist[(size_t) c];
            hh[(size_t) histPos] = ch[c][i];
            tp = juce::jmax (tp, std::abs (ch[c][i]));
            for (int p = 0; p < phases; ++p)
            {
                float acc = 0.0f;
                int idx = histPos;
                for (int k = 0; k < taps; ++k) { acc += coeff[(size_t) p][(size_t) k] * hh[(size_t) idx]; if (--idx < 0) idx = taps - 1; }
                tp = juce::jmax (tp, std::abs (acc));
            }
        }
        if (++histPos >= taps) histPos = 0;
        win[(size_t) winPos] = tp > ceiling ? ceiling / tp : 1.0f;
        if (++winPos >= window) winPos = 0;
        float hold = 1.0f;
        for (float v : win) hold = juce::jmin (hold, v);   // short window: a plain scan is cheap

        avgSum += (double) hold - (double) avgRing[(size_t) avgPos];
        avgRing[(size_t) avgPos] = hold;
        if (++avgPos >= avgLen) avgPos = 0;
        const float smoothed = juce::jmin (1.0f, (float) (avgSum / avgLen));
        env = smoothed < env ? smoothed : smoothed + (env - smoothed) * relC;
        minEnv = juce::jmin (minEnv, env);

        int rp = wp - delayLen;
        if (rp < 0) rp += dsize;
        for (int c = 0; c < channels; ++c)
        {
            auto& d = delay[(size_t) c];
            d[(size_t) wp] = ch[c][i];
            ch[c][i] = d[(size_t) rp] * env;
        }
        if (++wp >= dsize) wp = 0;
    }
    grDb.store (20.0f * std::log10 (minEnv + 1.0e-9f));
}

void MasterLimiter::prepare (double rate, int numChannels, int lookaheadSamples)
{
    fs = rate;
    numCh = juce::jlimit (1, Biquad::maxChannels, numChannels);
    L = juce::jmax (4, lookaheadSamples);
    delay.assign ((size_t) numCh, std::vector<float> ((size_t) L, 0.0f));
    dqVal.assign ((size_t) L + 2, 1.0f);
    dqIdx.assign ((size_t) L + 2, 0);
    avgRing.assign ((size_t) L, 1.0f);
    sustainC = (float) std::exp (-1.0 / (0.5 * fs));
    reset();
    setMode (mode);
}

void MasterLimiter::reset()
{
    for (auto& d : delay) std::fill (d.begin(), d.end(), 0.0f);
    dqHead = dqSize = 0;
    t = 0;
    wp = 0;
    env = 1.0f;
    sustain = 0.0f;
    std::fill (avgRing.begin(), avgRing.end(), 1.0f);
    avgPos = 0;
    avgSum = (double) avgLen;
}

void MasterLimiter::setAttackLength (int len)
{
    len = juce::jlimit (1, L, len);
    if (len == avgLen) return;
    avgLen = len;
    // restart the moving average from the current gain so the switch is seamless
    std::fill (avgRing.begin(), avgRing.end(), env);
    avgPos = 0;
    avgSum = (double) env * avgLen;
}

void MasterLimiter::setMode (int m)
{
    mode = juce::jlimit (0, 2, m);
    // attack (fraction of the look-ahead) and base release per character
    const float attackFrac[3] = { 1.0f, 0.6f, 0.3f };
    const float release[3] = { 0.12f, 0.07f, 0.03f };
    setAttackLength ((int) std::round (attackFrac[mode] * (float) L));
    relBase = release[mode];
}

void MasterLimiter::process (float* const* ch, int channels, int n)
{
    channels = juce::jmin (channels, numCh);
    const int cap = L + 2;
    float minEnv = 1.0f;

    for (int i = 0; i < n; ++i)
    {
        float peak = 0.0f;
        for (int c = 0; c < channels; ++c) peak = juce::jmax (peak, std::abs (ch[c][i]));
        const float target = peak > ceiling ? ceiling / peak : 1.0f;

        // sliding minimum over the last L samples
        while (dqSize > 0 && dqVal[(size_t) ((dqHead + dqSize - 1) % cap)] >= target) --dqSize;
        const int tail = (dqHead + dqSize) % cap;
        dqVal[(size_t) tail] = target;
        dqIdx[(size_t) tail] = t;
        ++dqSize;
        while (dqIdx[(size_t) dqHead] <= t - L) { dqHead = (dqHead + 1) % cap; --dqSize; }
        const float hold = dqVal[(size_t) dqHead];

        // attack: moving average reaches 'hold' within avgLen <= L samples
        avgSum += (double) hold - (double) avgRing[(size_t) avgPos];
        avgRing[(size_t) avgPos] = hold;
        if (++avgPos >= avgLen) avgPos = 0;
        const float smoothed = juce::jmin (1.0f, (float) (avgSum / avgLen));

        // release: slower while the limiter has been working continuously (no pumping on dense material)
        sustain = (env < 0.89f ? 1.0f : 0.0f) + (sustain - (env < 0.89f ? 1.0f : 0.0f)) * sustainC;
        const float relTime = relBase * (1.0f + 3.0f * sustain);
        const float relC = std::exp (-1.0f / (relTime * (float) fs));
        env = smoothed < env ? smoothed : smoothed + (env - smoothed) * relC;
        minEnv = juce::jmin (minEnv, env);

        for (int c = 0; c < channels; ++c)
        {
            auto& d = delay[(size_t) c];
            const float delayed = d[(size_t) wp];
            d[(size_t) wp] = ch[c][i];
            ch[c][i] = juce::jlimit (-ceiling, ceiling, delayed * env);   // safety: never above the ceiling
        }
        if (++wp >= L) wp = 0;
        ++t;
    }
    if (t > (juce::int64) 1 << 40) { reset(); }   // keep indices small on very long sessions
    grDb.store (20.0f * std::log10 (minEnv + 1.0e-9f));
}
