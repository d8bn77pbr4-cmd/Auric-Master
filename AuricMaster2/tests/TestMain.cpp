#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>
#include <functional>

namespace
{
    constexpr int block = 512;
    const double twoPi = juce::MathConstants<double>::twoPi;

    // ---------- meter helpers ----------
    struct Seg { double seconds; float dbfs; };   // 1 kHz sine segments (EBU Tech 3341 style)

    std::unique_ptr<LoudnessMeter> measureSine (double fs, std::initializer_list<Seg> segs, double freq = 1000.0, int channels = 2)
    {
        auto mp = std::make_unique<LoudnessMeter>();
        auto& m = *mp;
        m.prepare (fs, channels, true);
        juce::AudioBuffer<float> buf (channels, block);
        double ph = 0.0;
        for (auto s : segs)
        {
            const float amp = std::pow (10.0f, s.dbfs / 20.0f);
            const int total = (int) (s.seconds * fs);
            for (int done = 0; done < total; done += block)
            {
                const int n = juce::jmin (block, total - done);
                juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), channels, 0, n);
                for (int i = 0; i < n; ++i)
                {
                    const float v = amp * (float) std::sin (ph);
                    ph += twoPi * freq / fs;
                    for (int c = 0; c < channels; ++c) view.setSample (c, i, v);
                }
                m.process (view, channels);
            }
        }
        return mp;
    }

    // ---------- processor helpers ----------
    void setP (AuricMaster2Processor& p, const char* id, float v)
    {
        auto* pr = p.apvts.getParameter (id);
        pr->setValueNotifyingHost (pr->convertTo0to1 (v));
    }

    std::unique_ptr<AuricMaster2Processor> makeProc (double fs = 48000.0)
    {
        auto p = std::make_unique<AuricMaster2Processor>();
        p->setRateAndBufferSizeDetails (fs, block);
        p->prepareToPlay (fs, block);
        return p;
    }

    void neutral (AuricMaster2Processor& p)
    {
        setP (p, "amount", 0.0f); setP (p, "res_on", 0.0f); setP (p, "mono_freq", 39.0f);
        setP (p, "target", 0.0f); setP (p, "drive", 0.0f); setP (p, "clip", 0.0f); setP (p, "ceiling", -0.1f);
    }

    // music-like stereo test material: kick, bass, chords, hats, a bit of room noise
    void music (std::vector<float>& l, std::vector<float>& r, double fs, double seconds, float gain, unsigned seed)
    {
        const int n = (int) (seconds * fs);
        l.assign ((size_t) n, 0.0f); r.assign ((size_t) n, 0.0f);
        juce::Random rng ((juce::int64) seed);
        juce::dsp::IIR::Filter<float> hatL, hatR;
        hatL.coefficients = hatR.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (fs, 7000.0f);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / fs, beat = std::fmod (t, 0.5), off = std::fmod (t + 0.25, 0.5);
            double kick = 0.9 * std::exp (-beat * 18.0) * std::sin (twoPi * (50.0 + 80.0 * std::exp (-beat * 30.0)) * beat);
            double bass = 0.25 * std::sin (twoPi * 55.0 * t) * (0.6 + 0.4 * std::sin (twoPi * 0.5 * t));
            double chordL = 0.0, chordR = 0.0;
            const double notes[4] = { 220.0, 277.18, 329.63, 440.0 };
            for (int k = 0; k < 4; ++k)
            {
                chordL += 0.06 * std::sin (twoPi * notes[k] * t + k);
                chordR += 0.06 * std::sin (twoPi * notes[k] * 1.003 * t + 2 * k);
            }
            const float hat = (float) (0.35 * std::exp (-off * 60.0));
            const float hl = hatL.processSample (hat * (rng.nextFloat() * 2 - 1)), hr = hatR.processSample (hat * (rng.nextFloat() * 2 - 1));
            const float noise = 0.002f * (rng.nextFloat() * 2 - 1);
            l[(size_t) i] = gain * (float) (kick + bass + chordL) + gain * hl + noise;
            r[(size_t) i] = gain * (float) (kick + bass + chordR) + gain * hr + noise;
        }
    }

    void run (AuricMaster2Processor& p, std::vector<float>& l, std::vector<float>& r, int blk = block)
    {
        juce::AudioBuffer<float> buf (2, blk);
        juce::MidiBuffer m;
        for (size_t s = 0; s + (size_t) blk <= l.size(); s += (size_t) blk)
        {
            for (int i = 0; i < blk; ++i) { buf.setSample (0, i, l[s + (size_t) i]); buf.setSample (1, i, r[s + (size_t) i]); }
            p.processBlock (buf, m);
            for (int i = 0; i < blk; ++i) { l[s + (size_t) i] = buf.getSample (0, i); r[s + (size_t) i] = buf.getSample (1, i); }
        }
    }

    double rmsDb (const std::vector<float>& x, size_t a, size_t b)
    {
        double s = 0.0;
        for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
        return 10.0 * std::log10 (s / (double) (b - a) + 1.0e-20);
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    // 1) K-weighting at 48 kHz vs. the coefficients published in ITU-R BS.1770
    {
        double s[5], h[5];
        LoudnessMeter::kWeightingCoefficients (48000.0, s, h);
        const double refS[5] = { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 };
        const double refH[5] = { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 };
        double err = 0.0;
        for (int i = 0; i < 5; ++i) err = juce::jmax (err, std::abs (s[i] - refS[i]), std::abs (h[i] - refH[i]));
        std::printf ("[K-weighting 48k] max coefficient deviation from BS.1770 table: %.2e\n", err);
    }

    // 2) EBU Tech 3341 style cases (expected values in brackets, tolerance +-0.1 LU)
    for (double fs : { 48000.0, 44100.0, 96000.0 })
    {
        auto c1 = measureSine (fs, { { 20.0, -23.0f } });
        auto c2 = measureSine (fs, { { 20.0, -33.0f } });
        auto c3 = measureSine (fs, { { 10.0, -36.0f }, { 60.0, -23.0f }, { 10.0, -36.0f } });
        auto c4 = measureSine (fs, { { 10.0, -72.0f }, { 10.0, -36.0f }, { 60.0, -23.0f }, { 10.0, -36.0f }, { 10.0, -72.0f } });
        std::printf ("[EBU %5.1f kHz] sine -23 dBFS: M %.2f S %.2f I %.2f [-23] | -33 dBFS: I %.2f [-33] | gating: I %.2f, %.2f [-23, -23]\n",
                     fs / 1000.0, c1->getMomentary(), c1->getShortTerm(), c1->getIntegrated(), c2->getIntegrated(), c3->getIntegrated(), c4->getIntegrated());
    }

    // 3) loudness range (Tech 3342 style): two levels 10 / 5 LU apart
    {
        auto a = measureSine (48000.0, { { 20.0, -20.0f }, { 20.0, -30.0f } });
        auto b = measureSine (48000.0, { { 20.0, -20.0f }, { 20.0, -15.0f } });
        std::printf ("[LRA] -20/-30 dBFS: %.1f LU [10] | -20/-15 dBFS: %.1f LU [5]\n", a->getLra(), b->getLra());
    }

    // 4) true peak: sine at fs/4 sampled 45 degrees off its peaks -> samples 3 dB below the real peak
    {
        LoudnessMeter m;
        m.prepare (48000.0, 1, true);
        juce::AudioBuffer<float> buf (1, block);
        float samplePeak = 0.0f;
        for (int b = 0; b < 200; ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                const int k = b * block + i;
                const float v = 0.5f * (float) std::sin (juce::MathConstants<double>::halfPi * k + juce::MathConstants<double>::pi / 4.0);
                buf.setSample (0, i, v);
                samplePeak = juce::jmax (samplePeak, std::abs (v));
            }
            m.process (buf, 1);
        }
        std::printf ("[true peak] sample peak %.2f dBFS, true peak %.2f dBTP [-6.02]\n", 20.0 * std::log10 (samplePeak), m.getTruePeakMaxDb());
    }

    // 5) 3-band LR4 crossover with phase compensation: bands sum back flat
    {
        const double fs = 48000.0;
        LR4Split s1, s2;
        Biquad ap;
        s1.set (fs, 150.0); s2.set (fs, 3000.0); ap.setAllPass (fs, 3000.0, 0.70710678);
        double worst = 0.0;
        for (double f : { 40.0, 100.0, 150.0, 300.0, 1000.0, 3000.0, 6000.0, 12000.0 })
        {
            s1.reset(); s2.reset(); ap.reset();
            double inP = 0.0, outP = 0.0;
            for (int i = 0; i < (int) fs; ++i)
            {
                const float x = (float) std::sin (twoPi * f * i / fs);
                float lo, h1, mi, hi;
                s1.process (x, 0, lo, h1);
                lo = ap.process (lo, 0);
                s2.process (h1, 0, mi, hi);
                if (i > (int) fs / 2) { inP += (double) x * x; outP += (double) (lo + mi + hi) * (lo + mi + hi); }
            }
            worst = juce::jmax (worst, std::abs (10.0 * std::log10 (outP / inP)));
        }
        std::printf ("[crossover] 3-band sum, worst magnitude deviation 40 Hz..12 kHz: %.3f dB\n", worst);
    }

    // 6) neutral settings: level and spectrum unchanged (only the crossover all-pass phase)
    {
        auto p = makeProc();
        neutral (*p);
        std::vector<float> l, r;
        music (l, r, 48000.0, 8.0, 0.3f, 1);
        const auto inL = l;
        run (*p, l, r);
        const size_t L = (size_t) p->getLatencySamples();
        std::printf ("[neutral] latency %d samples (%.1f ms) | level change %+.3f dB\n", (int) L, 1000.0 * (double) L / 48000.0,
                     rmsDb (l, 4 * 48000 + L, 7 * 48000 + L) - rmsDb (inL, 4 * 48000, 7 * 48000));
    }

    // 7) loudness targets: the plugin finds the LUFS number by itself, true peak stays under the ceiling
    for (auto [target, mixGain] : { std::pair<int, float> { 1, 0.3f }, { 2, 0.3f }, { 4, 0.3f }, { 5, 0.5f }, { 4, 0.05f } })
    {
        auto p = makeProc();
        setP (*p, "target", (float) target);
        std::vector<float> l, r;
        music (l, r, 48000.0, 40.0, mixGain, 7);
        const float inLufs = [&] { LoudnessMeter mi; mi.prepare (48000.0, 2, false); juce::AudioBuffer<float> b (2, block);
                                   for (size_t s = 0; s + block <= l.size(); s += block) { for (int i = 0; i < block; ++i) { b.setSample (0, i, l[s + (size_t) i]); b.setSample (1, i, r[s + (size_t) i]); } mi.process (b, 2); }
                                   return mi.getIntegrated(); }();
        run (*p, l, r);
        // measure the last 15 s with a fresh meter
        LoudnessMeter m;
        m.prepare (48000.0, 2, true);
        juce::AudioBuffer<float> buf (2, block);
        for (size_t s = 25 * 48000; s + block <= l.size(); s += block)
        {
            for (int i = 0; i < block; ++i) { buf.setSample (0, i, l[s + (size_t) i]); buf.setSample (1, i, r[s + (size_t) i]); }
            m.process (buf, 2);
        }
        std::printf ("[target %-22s] mix %.1f LUFS -> %.2f LUFS, true peak max %.2f dBTP (ceiling -1.0), drive %+.1f dB\n",
                     Targets::names()[target].toRawUTF8(), inLufs, m.getIntegrated(), m.getTruePeakMaxDb(), p->driveDb.load());
    }

    // 8) level match: master turned down to the original's loudness; ORIGINAL switches to the untouched input
    {
        auto p = makeProc();
        setP (*p, "target", 4.0f);   // -9 LUFS: far louder than the mix
        std::vector<float> l, r;
        music (l, r, 48000.0, 36.0, 0.3f, 9);
        auto measure = [&] (std::vector<float>& a, std::vector<float>& b, size_t from)
        {
            LoudnessMeter m; m.prepare (48000.0, 2, true);
            juce::AudioBuffer<float> buf (2, block);
            for (size_t s = from; s + block <= a.size(); s += block)
            { for (int i = 0; i < block; ++i) { buf.setSample (0, i, a[s + (size_t) i]); buf.setSample (1, i, b[s + (size_t) i]); } m.process (buf, 2); }
            return std::make_pair (m.getIntegrated(), m.getTruePeakMaxDb());
        };
        std::vector<float> l1 (l.begin(), l.begin() + 20 * 48000), r1 (r.begin(), r.begin() + 20 * 48000);
        run (*p, l1, r1);
        const auto master = measure (l1, r1, 12 * 48000);
        setP (*p, "compare", 1.0f);
        std::vector<float> l2 (l.begin() + 20 * 48000, l.begin() + 28 * 48000), r2 (r.begin() + 20 * 48000, r.begin() + 28 * 48000);
        run (*p, l2, r2);
        const auto matched = measure (l2, r2, 2 * 48000);
        setP (*p, "bypass", 1.0f);
        std::vector<float> l3 (l.begin() + 28 * 48000, l.end()), r3 (r.begin() + 28 * 48000, r.end());
        run (*p, l3, r3);
        const auto original = measure (l3, r3, 1 * 48000);
        std::printf ("[level match] master %.2f LUFS | matched master %.2f LUFS | original %.2f LUFS (peak %.1f dBTP, untouched)\n",
                     master.first, matched.first, original.first, original.second);
    }

    // 9) mono bass: side content below 100 Hz goes, side content at 1 kHz stays
    {
        auto p = makeProc();
        neutral (*p);
        setP (*p, "mono_freq", 100.0f);
        const int n = 48000 * 4;
        std::vector<float> l ((size_t) n), r ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            const float s60 = 0.2f * (float) std::sin (twoPi * 50.0 * i / 48000.0), s1k = 0.1f * (float) std::sin (twoPi * 1000.0 * i / 48000.0);
            l[(size_t) i] = s60 + s1k; r[(size_t) i] = -s60 - s1k;   // pure side
        }
        run (*p, l, r);
        juce::dsp::IIR::Filter<float> lp, hp;
        lp.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (48000.0, 200.0f);
        hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (48000.0, 500.0f);
        std::vector<float> sideLow ((size_t) n), sideHigh ((size_t) n);
        for (int i = 0; i < n; ++i) { const float s = 0.5f * (l[(size_t) i] - r[(size_t) i]); sideLow[(size_t) i] = lp.processSample (s); sideHigh[(size_t) i] = hp.processSample (s); }
        std::printf ("[mono bass 100 Hz] side at 50 Hz: %.1f dB | side at 1 kHz: %.1f dB (input -17.0 / -23.0)\n",
                     rmsDb (sideLow, 48000 * 2, (size_t) n), rmsDb (sideHigh, 48000 * 2, (size_t) n));
    }

    // 10) snapshots A-D and project save/load
    {
        auto p = makeProc();
        setP (*p, "punch", 80.0f);
        p->selectSnapshot (1);
        setP (*p, "punch", 20.0f);
        p->selectSnapshot (0);
        const float back = p->apvts.getRawParameterValue ("punch")->load();
        juce::MemoryBlock state;
        p->getStateInformation (state);
        auto q = makeProc();
        q->setStateInformation (state.getData(), (int) state.getSize());
        q->selectSnapshot (1);
        std::printf ("[snapshots] A=%.0f after B was set to 20 | reloaded project: A=%.0f, B=%.0f\n", back,
                     p->apvts.getRawParameterValue ("punch")->load(), q->apvts.getRawParameterValue ("punch")->load());
    }

    // 11) large host blocks give the same result
    {
        auto a = makeProc(), b = makeProc();
        std::vector<float> l1, r1;
        music (l1, r1, 48000.0, 3.0, 0.2f, 3);
        auto l2 = l1, r2 = r1;
        run (*a, l1, r1, block);
        run (*b, l2, r2, block * 16);
        const size_t full = l1.size() / (size_t) (block * 16) * (size_t) (block * 16);
        float diff = 0.0f;
        for (size_t i = 0; i < full; ++i) diff = juce::jmax (diff, std::abs (l1[i] - l2[i]));
        std::printf ("[big blocks] 8192 vs 512 samples: max difference %.2e\n", diff);
    }

    // 12) CPU with everything on
    {
        auto p = makeProc();
        setP (*p, "sat_low", 60.0f); setP (*p, "sat_high", 50.0f); setP (*p, "res_depth", 60.0f);
        std::vector<float> l, r;
        music (l, r, 48000.0, 20.0, 0.2f, 5);
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        run (*p, l, r);
        std::printf ("[cpu] %.1f %% of one core (48 kHz, all modules on)\n", 100.0 * (juce::Time::getMillisecondCounterHiRes() - t0) / 20000.0);
    }

    juce::ignoreUnused (argc, argv);
    return 0;
}
