// Auric Master 3 - offline test suite. Every check prints PASS/FAIL; the exit code is the number of failures.
//   AM3Tests              run all checks
//   AM3Tests --shots dir  render every editor page to PNG (with music running through the plugin)
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdarg>
#include <cstdio>
#include <functional>

namespace
{
    constexpr int block = 512;
    const double twoPi = juce::MathConstants<double>::twoPi;
    int failures = 0, checks = 0;

    void check (bool ok, const char* name, const char* fmt, ...)
    {
        ++checks;
        if (! ok) ++failures;
        char buf[512];
        va_list args;
        va_start (args, fmt);
        std::vsnprintf (buf, sizeof (buf), fmt, args);
        va_end (args);
        std::printf ("[%s] %-34s %s\n", ok ? "PASS" : "FAIL", name, buf);
    }

    // ---------- meter helpers ----------
    struct Seg { double seconds; float dbfs; };

    std::unique_ptr<LoudnessMeter> measureSine (double fs, std::initializer_list<Seg> segs, double freq = 1000.0, int channels = 2)
    {
        auto mp = std::make_unique<LoudnessMeter>();
        mp->prepare (fs, channels, true);
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
                mp->process (view, channels);
            }
        }
        return mp;
    }

    // ---------- processor helpers ----------
    void setP (AuricMaster3Processor& p, const char* id, float v)
    {
        auto* pr = p.apvts.getParameter (id);
        pr->setValueNotifyingHost (pr->convertTo0to1 (v));
    }
    float val (AuricMaster3Processor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

    using Setup = std::function<void (AuricMaster3Processor&)>;
    std::unique_ptr<AuricMaster3Processor> makeProc (double fs = 48000.0, Setup setup = nullptr)
    {
        auto p = std::make_unique<AuricMaster3Processor>();
        if (setup) setup (*p);
        p->setRateAndBufferSizeDetails (fs, block);
        p->prepareToPlay (fs, block);
        return p;
    }

    void pump (int ms = 2) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

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

    // stereo noise with the long-term spectrum of an assistant genre target (plus an optional extra curve)
    void shapedNoise (std::vector<float>& l, std::vector<float>& r, double fs, float rmsDbfs, int profile,
                      std::function<float (float)> extraDb, float side, unsigned seed, float sideLowCut = 200.0f)
    {
        constexpr int order = 20;
        const int N = 1 << order;
        juce::dsp::FFT fft (order);
        juce::Random rng ((juce::int64) seed);
        auto make = [&] (bool sideSignal)
        {
            std::vector<float> d ((size_t) 2 * (size_t) N, 0.0f);
            for (int k = 1; k < N / 2; ++k)
            {
                const float f = (float) (k * fs / N);
                if (sideSignal && f < sideLowCut) continue;
                const float db = AutoMatcher::targetDb (f, profile) + (extraDb ? extraDb (f) : 0.0f);
                const float mag = std::pow (10.0f, db / 20.0f), ph = rng.nextFloat() * (float) twoPi;
                d[(size_t) (2 * k)] = mag * std::cos (ph);
                d[(size_t) (2 * k + 1)] = mag * std::sin (ph);
            }
            fft.performRealOnlyInverseTransform (d.data());
            d.resize ((size_t) N);
            return d;
        };
        auto c = make (false), s = make (true);
        double e = 0.0;
        for (float v : c) e += (double) v * v;
        const float g = std::pow (10.0f, rmsDbfs / 20.0f) / (float) std::sqrt (e / N);
        l.resize ((size_t) N); r.resize ((size_t) N);
        for (size_t i = 0; i < (size_t) N; ++i) { l[i] = g * (c[i] + side * s[i]); r[i] = g * (c[i] - side * s[i]); }
    }

    // a realistic mix: the synthetic band plus noise with a typical master balance
    void material (std::vector<float>& l, std::vector<float>& r, float noiseDb, float musicGain, unsigned seed, double fs = 48000.0)
    {
        std::vector<float> nl, nr;
        shapedNoise (nl, nr, fs, noiseDb, AutoMatcher::pop, nullptr, 0.5f, seed);
        music (l, r, fs, (double) nl.size() / fs, musicGain, seed + 1);
        for (size_t i = 0; i < l.size(); ++i) { l[i] += nl[i]; r[i] += nr[i]; }
    }

    void run (AuricMaster3Processor& p, std::vector<float>& l, std::vector<float>& r, int blk = block)
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

    // plays the material in a loop until LERNEN has finished (the processor's timer applies the results)
    bool runLearn (AuricMaster3Processor& p, const std::vector<float>& l, const std::vector<float>& r, float firstPass = 20.0f, bool start = true)
    {
        if (start) p.startLoudnessLearn (firstPass);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer m;
        size_t cursor = 0;
        for (int k = 0; k < 40000; ++k)
        {
            if (cursor + block > l.size()) cursor = 0;
            for (int i = 0; i < block; ++i) { buf.setSample (0, i, l[cursor + (size_t) i]); buf.setSample (1, i, r[cursor + (size_t) i]); }
            p.processBlock (buf, m);
            cursor += block;
            if (k % 8 == 0) pump (1);
            if (p.getLearnState() != AuricMaster3Processor::learnListening && k > 10) return p.getLearnState() == AuricMaster3Processor::learnDone;
        }
        return false;
    }

    bool runAssistant (AuricMaster3Processor& p, const std::vector<float>& l, const std::vector<float>& r)
    {
        const int runsBefore = p.getAssistRuns();
        p.startAssistant();
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer m;
        size_t cursor = 0;
        for (int k = 0; k < 6000 && p.isAutoLearning(); ++k)
        {
            if (cursor + block > l.size()) cursor = 0;
            for (int i = 0; i < block; ++i) { buf.setSample (0, i, l[cursor + (size_t) i]); buf.setSample (1, i, r[cursor + (size_t) i]); }
            p.processBlock (buf, m);
            cursor += block;
        }
        for (int k = 0; k < 100 && p.getAssistRuns() == runsBefore; ++k) pump (20);
        if (p.getAssistRuns() == runsBefore) return false;
        return runLearn (p, l, r, 8.0f, false);   // the assistant starts LERNEN itself
    }

    float lufsOf (const std::vector<float>& l, const std::vector<float>& r, size_t a, size_t b, double fs = 48000.0, float* tp = nullptr)
    {
        LoudnessMeter m;
        m.prepare (fs, 2, true);
        juce::AudioBuffer<float> buf (2, block);
        for (size_t s = a; s + block <= b; s += block)
        {
            for (int i = 0; i < block; ++i) { buf.setSample (0, i, l[s + (size_t) i]); buf.setSample (1, i, r[s + (size_t) i]); }
            m.process (buf, 2);
        }
        if (tp != nullptr) *tp = m.getTruePeakMaxDb();
        return m.getIntegrated();
    }

    // fraction of non-harmonic energy relative to the fundamental (aliasing + noise), dB
    float inharmonicDb (AuricMaster3Processor& p, float f0, float amp, double fs = 48000.0)
    {
        const int total = (int) (fs * 3.0);
        std::vector<float> l ((size_t) total), r ((size_t) total);
        for (int i = 0; i < total; ++i) l[(size_t) i] = r[(size_t) i] = amp * (float) std::sin (twoPi * f0 * i / fs);
        run (p, l, r);
        constexpr int order = 16, N = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> d ((size_t) 2 * N, 0.0f);
        const size_t start = (size_t) total - (size_t) N - block;
        for (int i = 0; i < N; ++i)
        {
            const double x = twoPi * i / N;   // 4-term Blackman-Harris
            d[(size_t) i] = l[start + (size_t) i] * (float) (0.35875 - 0.48829 * std::cos (x) + 0.14128 * std::cos (2 * x) - 0.01168 * std::cos (3 * x));
        }
        fft.performFrequencyOnlyForwardTransform (d.data());
        const double binHz = fs / N;
        double fund = 0.0, rest = 0.0;
        for (int k = (int) (20.0 / binHz); k < N / 2; ++k)
        {
            const double f = k * binHz, pw = (double) d[(size_t) k] * d[(size_t) k];
            const double nearest = std::round (f / f0);
            const bool isHarm = nearest >= 1.0 && std::abs (f - nearest * f0) < 10.0 * binHz;
            if (isHarm && nearest == 1.0) fund += pw;
            else if (! isHarm) rest += pw;
        }
        return (float) (10.0 * std::log10 ((rest + 1.0e-30) / (fund + 1.0e-30)));
    }

    // largest second difference after a parameter switch vs. the steady state (clicks), dB
    float switchClickDb (const char* id, float from, float to, Setup setup = nullptr)
    {
        auto p = makeProc (48000.0, [&] (AuricMaster3Processor& q) { if (setup) setup (q); setP (q, id, from); });
        const double fs = 48000.0;
        const int total = (int) (fs * 2.0), sw = (int) (fs * 1.0) / block * block;
        std::vector<float> out ((size_t) total, 0.0f);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer m;
        for (int s = 0; s + block <= total; s += block)
        {
            if (s == sw) setP (*p, id, to);
            for (int i = 0; i < block; ++i)
            {
                const float v = 0.25f * (float) std::sin (twoPi * 997.0 * (s + i) / fs);
                buf.setSample (0, i, v); buf.setSample (1, i, v);
            }
            p->processBlock (buf, m);
            for (int i = 0; i < block; ++i) out[(size_t) (s + i)] = buf.getSample (0, i);
        }
        auto maxD2 = [&] (int a, int b)
        {
            float mx = 0.0f;
            for (int i = juce::jmax (2, a); i < b; ++i) mx = juce::jmax (mx, std::abs (out[(size_t) i] - 2.0f * out[(size_t) i - 1] + out[(size_t) i - 2]));
            return mx;
        };
        const float steady = maxD2 (sw - (int) (0.4 * fs), sw - (int) (0.1 * fs));
        const float around = maxD2 (sw, total / block * block);
        return 20.0f * std::log10 ((around + 1.0e-9f) / (steady + 1.0e-9f));
    }

    void fullChain (AuricMaster3Processor& p)
    {
        for (auto* id : { "res_on", "eq_on", "dyn_on", "sat_on", "st_on", "glue_on", "lim_on" }) setP (p, id, 1.0f);
        setP (p, "eq2_gain", -2.0f); setP (p, "eq4_gain", 2.0f);
        setP (p, "mb_low_thr", -26.0f); setP (p, "mb_mid_thr", -28.0f); setP (p, "mb_high_thr", -32.0f);
        setP (p, "glue_thr", -24.0f); setP (p, "width_high", 115.0f); setP (p, "mono_freq", 100.0f);
        setP (p, "lim_drive", 8.0f); setP (p, "clip", 30.0f);
    }

    int shots (const char* dirName, bool withAssistant, bool dynEq = false)
    {
        const juce::File dir (juce::File::getCurrentWorkingDirectory().getChildFile (dirName));
        dir.createDirectory();
        auto p = makeProc (48000.0, [dynEq] (AuricMaster3Processor& q) {
            if (! dynEq) return;
            setP (q, "eq2_freq", 300.0f); setP (q, "eq2_dyn", 1.0f); setP (q, "eq2_thr", -40.0f); setP (q, "eq2_range", -5.0f);
            setP (q, "eq3_freq", 3000.0f); setP (q, "eq3_gain", 2.0f); setP (q, "eq3_dyn", 1.0f); setP (q, "eq3_thr", -50.0f); setP (q, "eq3_range", -6.0f); });
        std::vector<float> l, r;
        material (l, r, -24.0f, 0.06f, 21);
        size_t cursor = 0;
        auto feed = [&] (double seconds)
        {
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer m;
            for (size_t done = 0; done + block <= (size_t) (seconds * 48000.0); done += block)
            {
                if (cursor + block > l.size()) cursor = 0;
                for (int i = 0; i < block; ++i) { buf.setSample (0, i, l[cursor + (size_t) i]); buf.setSample (1, i, r[cursor + (size_t) i]); }
                p->processBlock (buf, m);
                cursor += block;
            }
        };
        if (withAssistant) runAssistant (*p, l, r);
        feed (20.0);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
        auto* e = dynamic_cast<AuricMaster3Editor*> (ed.get());
        e->setSize (MainView::designWidth, MainView::designHeight);
        const char* names[] = { "simple", "eq", "dynamics", "glue", "stereo", "limiter", "resonance" };
        for (int pg = 0; pg < MainView::numPages; ++pg)
        {
            e->getView().showPage (pg);
            for (int k = 0; k < 90; ++k) { feed (1.0 / 30.0); e->getView().tick (k); pump (1); }
            auto img = e->createComponentSnapshot (e->getLocalBounds(), true, 1.0f);
            juce::FileOutputStream out (dir.getChildFile (juce::String (names[pg]) + ".png"));
            out.setPosition (0); out.truncate();
            juce::PNGImageFormat().writeImageToStream (img, out);
        }
        std::printf ("screenshots written to %s\n", dir.getFullPathName().toRawUTF8());
        if (p->getAutoResult() != nullptr) std::printf ("assistant: %s\n%s\n", p->getAssistSummary().toRawUTF8(), p->getLearnStatus().toRawUTF8());
        return 0;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc >= 3 && juce::String (argv[1]) == "--shots")
        return shots (argv[2], argc >= 4 && juce::String (argv[3]) == "assist", argc >= 4 && juce::String (argv[3]) == "dyneq");
    const double fs = 48000.0;

    // ---------------- loudness meter (ITU-R BS.1770-4 / EBU Tech 3341, 3342) ----------------
    {
        double s[5], h[5];
        LoudnessMeter::kWeightingCoefficients (48000.0, s, h);
        const double refS[5] = { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 };
        const double refH[5] = { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 };
        double err = 0.0;
        for (int i = 0; i < 5; ++i) err = juce::jmax (err, std::abs (s[i] - refS[i]), std::abs (h[i] - refH[i]));
        check (err < 1.0e-6, "K-weighting coefficients", "max deviation %.1e", err);
    }
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        auto c1 = measureSine (rate, { { 20.0, -23.0f } });
        auto c3 = measureSine (rate, { { 10.0, -36.0f }, { 60.0, -23.0f }, { 10.0, -36.0f } });
        const float e1 = std::abs (c1->getIntegrated() + 23.0f), e3 = std::abs (c3->getIntegrated() + 23.0f);
        char name[64]; std::snprintf (name, sizeof (name), "EBU 3341 cases @ %.1f kHz", rate / 1000.0);
        check (e1 < 0.1f && e3 < 0.1f, name, "I %.2f / %.2f LUFS (expected -23, gating)", c1->getIntegrated(), c3->getIntegrated());
    }
    {
        auto l1 = measureSine (48000.0, { { 20.0, -20.0f }, { 20.0, -30.0f } });
        auto tp = measureSine (48000.0, { { 2.0, -6.0f } }, 12000.0);
        check (std::abs (l1->getLra() - 10.0f) < 0.2f, "EBU 3342 loudness range", "%.2f LU (expected 10)", l1->getLra());
        check (tp->getTruePeakMaxDb() > -6.5f && tp->getTruePeakMaxDb() < -5.7f, "true peak 12 kHz sine", "%.2f dBTP (expected about -6)", tp->getTruePeakMaxDb());
    }

    // ---------------- inserted with default settings: nothing changes ----------------
    {
        auto p = makeProc();
        std::vector<float> l, r;
        material (l, r, -26.0f, 0.03f, 31);
        auto lo = l, ro = r;
        run (*p, lo, ro);
        const int lat = p->getLatencySamples();
        double err = 0.0, ref = 0.0;
        for (size_t i = (size_t) lat + 48000; i < l.size(); ++i) { const double d = lo[i] - l[i - (size_t) lat]; err += d * d; ref += (double) l[i - (size_t) lat] * l[i - (size_t) lat]; }
        const float diffDb = (float) (10.0 * std::log10 (err / ref + 1.0e-30));
        check (lat == 0, "defaults: no latency", "%d samples", lat);
        check (diffDb < -60.0f, "defaults: transparent", "difference to the input %.1f dB (magnitude flat, crossover phase only)", diffDb);
        const float inL = lufsOf (l, r, 48000 * 5, l.size()), outL = lufsOf (lo, ro, 48000 * 5, lo.size());
        check (std::abs (outL - inL) < 0.05f, "defaults: loudness unchanged", "%.2f -> %.2f LUFS", inL, outL);
    }

    // ---------------- latency only for modules that need it ----------------
    {
        auto pRes = makeProc (fs, [] (AuricMaster3Processor& q) { setP (q, "res_on", 1.0f); });
        auto pLim = makeProc (fs, [] (AuricMaster3Processor& q) { setP (q, "lim_on", 1.0f); });
        auto pAll = makeProc (fs, fullChain);
        check (pRes->getLatencySamples() > 0 && pLim->getLatencySamples() > 0 && pAll->getLatencySamples() == pRes->getLatencySamples() + pLim->getLatencySamples(),
               "latency per module", "resonance %.1f ms, limiter %.1f ms, all %.1f ms", 1000.0 * pRes->getLatencySamples() / fs,
               1000.0 * pLim->getLatencySamples() / fs, 1000.0 * pAll->getLatencySamples() / fs);
        // switching the limiter on while playing: the host is told the new latency
        auto p = makeProc();
        setP (*p, "lim_on", 1.0f);
        std::vector<float> l, r;
        music (l, r, fs, 1.0, 0.1f, 3);
        run (*p, l, r);
        for (int k = 0; k < 20; ++k) pump (5);
        check (p->getLatencySamples() == pLim->getLatencySamples(), "latency update when switching", "reported %d samples (expected %d)", p->getLatencySamples(), pLim->getLatencySamples());
    }

    // ---------------- crossover ----------------
    {
        auto p = makeProc (fs, [] (AuricMaster3Processor& q) { setP (q, "dyn_on", 1.0f); setP (q, "mb_low_thr", 0.0f); setP (q, "mb_mid_thr", 0.0f); setP (q, "mb_high_thr", 0.0f); });
        float worst = 0.0f;
        for (float f : { 40.0f, 100.0f, 150.0f, 300.0f, 1000.0f, 3000.0f, 6000.0f, 12000.0f })
        {
            p->prepareToPlay (fs, block);
            std::vector<float> l ((size_t) fs), r ((size_t) fs);
            for (size_t i = 0; i < l.size(); ++i) l[i] = r[i] = 0.1f * (float) std::sin (twoPi * f * (double) i / fs);
            run (*p, l, r);
            double e = 0.0;
            for (size_t i = l.size() / 2; i < l.size(); ++i) e += (double) l[i] * l[i];
            const float db = (float) (10.0 * std::log10 (e / (double) (l.size() / 2))) - (20.0f * std::log10 (0.1f) - 3.0103f);
            worst = juce::jmax (worst, std::abs (db));
        }
        check (worst < 0.05f, "crossover sum flat", "worst deviation %.3f dB (40 Hz - 12 kHz)", worst);
    }

    // ---------------- deterministic: same result wherever playback starts ----------------
    {
        std::vector<float> v1, v2, c1, c2;
        material (v1, v2, -28.0f, 0.03f, 41);   // quiet verse
        material (c1, c2, -18.0f, 0.1f, 42);    // chorus, 10 dB louder
        std::vector<float> L (v1), R (v2);
        L.insert (L.end(), c1.begin(), c1.end());
        R.insert (R.end(), c2.begin(), c2.end());
        const size_t chorus = v1.size();
        const float inVerse = lufsOf (L, R, chorus - (size_t) (fs * 10), chorus), inChorus = lufsOf (L, R, L.size() - (size_t) (fs * 10), L.size());

        auto a = makeProc (fs, fullChain);
        auto la = L, ra = R;
        run (*a, la, ra);
        const size_t lat = (size_t) a->getLatencySamples();
        auto b = makeProc (fs, fullChain);
        std::vector<float> lb (L.begin() + (long) chorus, L.end()), rb (R.begin() + (long) chorus, R.end());
        run (*b, lb, rb);
        const float aWin = lufsOf (la, ra, chorus + (size_t) (fs * 2) + lat, chorus + (size_t) (fs * 8) + lat);
        const float bWin = lufsOf (lb, rb, (size_t) (fs * 2) + lat, (size_t) (fs * 8) + lat);
        check (std::abs (aWin - bWin) < 0.1f, "independent of playback start", "chorus from song start vs. from chorus: %+.2f LU", aWin - bWin);
        const float outVerse = lufsOf (la, ra, chorus - (size_t) (fs * 10) + lat, chorus + lat), outChorus = lufsOf (la, ra, la.size() - (size_t) (fs * 10), la.size());
        check (outChorus - outVerse > 5.0f, "verse/chorus contrast kept", "input %.1f LU -> output %.1f LU (full chain, heavy settings)", inChorus - inVerse, outChorus - outVerse);
    }

    // ---------------- LERNEN: targets reached, then held ----------------
    for (int t : { 1, 2, 4, 5 })
    {
        auto p = makeProc (fs, [t] (AuricMaster3Processor& q) { setP (q, "target", (float) t); setP (q, "lim_on", 1.0f); setP (q, "clip", 30.0f); });
        std::vector<float> l, r;
        material (l, r, -24.0f, 0.05f, 50 + (unsigned) t);
        const bool done = runLearn (*p, l, r);
        const float drive = val (*p, "lim_drive");
        // a fresh pass over the whole song with the learned (fixed) drive
        auto q = makeProc (fs, [&] (AuricMaster3Processor& x) { setP (x, "target", (float) t); setP (x, "lim_on", 1.0f); setP (x, "clip", 30.0f); setP (x, "lim_drive", drive); });
        auto lo = l, ro = r;
        run (*q, lo, ro);
        float tp = 0.0f;
        const float out = lufsOf (lo, ro, 0, lo.size(), fs, &tp);
        char name[64]; std::snprintf (name, sizeof (name), "LERNEN %s", Targets::names()[t].toRawUTF8());
        check (done && std::abs (out - Targets::lufs (t)) < 0.5f && tp <= -0.95f, name, "song %.2f LUFS, true peak %.2f dBTP, drive %+.1f dB | %s",
               out, tp, drive, p->getLearnStatus().toRawUTF8());
    }
    {
        // a quiet mix cannot reach a loud target with the drive alone: LERNEN says so
        auto p = makeProc (fs, [] (AuricMaster3Processor& q) { setP (q, "target", 5.0f); setP (q, "lim_on", 1.0f); });
        std::vector<float> l, r;
        material (l, r, -44.0f, 0.004f, 61);
        runLearn (*p, l, r);
        check (p->getLearnState() == AuricMaster3Processor::learnFailed, "LERNEN unreachable target", "%s", p->getLearnStatus().toRawUTF8());
    }

    // ---------------- dynamic EQ ----------------
    {
        // steady sine through a dynamic bell; returns the level change at the output in dB
        auto dynEqChange = [&] (float freq, float amp, Setup extra, bool sideOnly = false)
        {
            auto p = makeProc (fs, [&] (AuricMaster3Processor& q) {
                setP (q, "eq2_freq", 1000.0f); setP (q, "eq2_q", 1.0f); setP (q, "eq2_gain", 0.0f);
                setP (q, "eq2_dyn", 1.0f); setP (q, "eq2_thr", -20.0f); setP (q, "eq2_range", -6.0f);
                if (extra) extra (q); });
            const int total = (int) (fs * 2.0);
            std::vector<float> l ((size_t) total), r ((size_t) total);
            for (int i = 0; i < total; ++i)
            {
                const float v = amp * (float) std::sin (twoPi * freq * i / fs);
                l[(size_t) i] = v; r[(size_t) i] = sideOnly ? -v : v;
            }
            auto lo = l, ro = r;
            run (*p, lo, ro);
            const size_t a0 = (size_t) fs, a1 = (size_t) total / block * block;
            double ei = 0.0, eo = 0.0;
            for (size_t i = a0; i < a1; ++i) { ei += (double) l[i] * l[i]; eo += (double) lo[i] * lo[i]; }
            return (float) (10.0 * std::log10 (eo / ei));
        };
        const float quiet = dynEqChange (1000.0f, 0.03f, nullptr);   // band -33 dB RMS: below threshold
        const float loud = dynEqChange (1000.0f, 0.5f, nullptr);     // band -9 dB RMS: 11 dB above
        const float other = dynEqChange (100.0f, 0.5f, nullptr);     // loud, but outside the band
        check (std::abs (quiet) < 0.1f, "dynamic EQ: inactive below threshold", "%+.2f dB", quiet);
        check (std::abs (loud + 6.0f) < 0.4f, "dynamic EQ: full range when loud", "%+.2f dB (range -6)", loud);
        check (std::abs (other) < 0.3f, "dynamic EQ: frequency selective", "%+.2f dB at 100 Hz (band at 1 kHz)", other);
        const float boost = dynEqChange (1000.0f, 0.5f, [] (AuricMaster3Processor& q) { setP (q, "eq2_gain", 3.0f); });
        check (std::abs (boost + 3.0f) < 0.4f, "dynamic EQ: static + dynamic gain", "%+.2f dB (static +3, range -6)", boost);
        const float midOnly = dynEqChange (1000.0f, 0.5f, [] (AuricMaster3Processor& q) { setP (q, "eq2_mode", 1.0f); }, true);
        check (std::abs (midOnly) < 0.3f, "dynamic EQ: mid band ignores side", "%+.2f dB (side-only signal, band in Mid mode)", midOnly);
        const float off = dynEqChange (1000.0f, 0.5f, [] (AuricMaster3Processor& q) { setP (q, "eq2_dyn", 0.0f); });
        check (std::abs (off) < 0.1f, "dynamic EQ: DYN off = static EQ", "%+.2f dB (static gain 0)", off);
    }

    // ---------------- multiband mid/side mode ----------------
    {
        // loud side-only 1 kHz tone through the mid band: GR depends on the band mode
        auto bandGr = [&] (float mode, bool sideSignal)
        {
            auto p = makeProc (fs, [mode] (AuricMaster3Processor& q) {
                setP (q, "dyn_on", 1.0f); setP (q, "mb_mid_thr", -30.0f); setP (q, "mb_mid_ratio", 4.0f); setP (q, "mb_mid_mode", mode); });
            const int total = (int) (fs * 1.5);
            std::vector<float> l ((size_t) total), r ((size_t) total);
            for (int i = 0; i < total; ++i)
            {
                const float v = 0.3f * (float) std::sin (twoPi * 1000.0 * i / fs);
                l[(size_t) i] = v; r[(size_t) i] = sideSignal ? -v : v;
            }
            auto lo = l, ro = r;
            run (*p, lo, ro);
            double ei = 0.0, eo = 0.0;
            for (size_t i = (size_t) fs; i < (size_t) total / block * block; ++i) { ei += (double) l[i] * l[i]; eo += (double) lo[i] * lo[i]; }
            return (float) (10.0 * std::log10 (eo / ei));
        };
        const float stereoSide = bandGr (0.0f, true), midSide = bandGr (1.0f, true), sideSide = bandGr (2.0f, true), sideMono = bandGr (2.0f, false);
        check (stereoSide < -3.0f && std::abs (midSide) < 0.2f && sideSide < -3.0f && std::abs (sideMono) < 0.2f, "multiband mid/side mode",
               "side tone: stereo %+.1f, mid %+.1f, side %+.1f dB | mono tone in side mode %+.1f dB", stereoSide, midSide, sideSide, sideMono);
    }

    // ---------------- monitoring: MONO / SIDE ----------------
    {
        std::vector<float> l, r;
        material (l, r, -18.0f, 0.1f, 91);
        l.resize ((size_t) fs * 2); r.resize ((size_t) fs * 2);
        auto setup = [] (AuricMaster3Processor& q) { setP (q, "glue_on", 1.0f); setP (q, "width_high", 130.0f); };
        auto pn = makeProc (fs, setup);
        auto pm = makeProc (fs, [&] (AuricMaster3Processor& q) { setup (q); setP (q, "mon_mono", 1.0f); });
        auto ps = makeProc (fs, [&] (AuricMaster3Processor& q) { setup (q); setP (q, "mon_side", 1.0f); });
        auto ln = l, rn = r, lm = l, rm = r, ls = l, rs = r;
        run (*pn, ln, rn); run (*pm, lm, rm); run (*ps, ls, rs);
        float em = 0.0f, es = 0.0f;
        for (size_t i = 4800; i < l.size() / block * block; ++i)
        {
            em = juce::jmax (em, std::abs (lm[i] - 0.5f * (ln[i] + rn[i])), std::abs (rm[i] - lm[i]));
            es = juce::jmax (es, std::abs (ls[i] - 0.5f * (ln[i] - rn[i])), std::abs (rs[i] - ls[i]));
        }
        check (em < 1.0e-5f && es < 1.0e-5f, "monitoring MONO / SIDE", "mono error %.1e, side error %.1e", em, es);
    }

    // ---------------- DELTA listening: normal + delta = what went into the stage ----------------
    {
        std::vector<float> l, r;
        material (l, r, -18.0f, 0.1f, 81);
        l.resize ((size_t) fs * 4); r.resize ((size_t) fs * 4);
        auto runPair = [&] (Setup setup, const char* deltaId, float inputGainDb)
        {
            auto pn = makeProc (fs, setup);
            auto pd = makeProc (fs, [&] (AuricMaster3Processor& q) { setup (q); setP (q, deltaId, 1.0f); });
            auto ln = l, rn = r, ld = l, rd = r;
            run (*pn, ln, rn);
            run (*pd, ld, rd);
            const size_t lat = (size_t) pn->getLatencySamples(), end = l.size() / block * block;
            const float g = std::pow (10.0f, inputGainDb / 20.0f);
            float err = 0.0f, deltaPeak = 0.0f;
            for (size_t i = lat + 4800; i < end; ++i)
            {
                err = juce::jmax (err, std::abs (ln[i] + ld[i] - g * l[i - lat]));
                deltaPeak = juce::jmax (deltaPeak, std::abs (ld[i]));
            }
            return std::make_pair (err, deltaPeak);
        };
        const auto res = runPair ([] (AuricMaster3Processor& q) { setP (q, "res_on", 1.0f); setP (q, "res_depth", 80.0f); setP (q, "eq_on", 0.0f); setP (q, "st_on", 0.0f); },
                                  "res_delta", 0.0f);
        check (res.first < 1.0e-4f && res.second > 1.0e-3f, "DELTA resonance", "normal + delta = input (max error %.1e), delta peak %.3f", res.first, res.second);
        const auto lim = runPair ([] (AuricMaster3Processor& q) { setP (q, "lim_on", 1.0f); setP (q, "lim_drive", 12.0f); setP (q, "clip", 40.0f); setP (q, "eq_on", 0.0f); setP (q, "st_on", 0.0f); },
                                  "lim_delta", 12.0f);
        check (lim.first < 1.0e-4f && lim.second > 1.0e-2f, "DELTA limiter + clipper", "normal + delta = driven input (max error %.1e), delta peak %.3f", lim.first, lim.second);
    }

    // ---------------- compressor ----------------
    {
        Compressor d;
        d.prepare (fs);
        d.setParams (-20.0f, 4.0f, 1.0f, 100.0f, 0.0f, false);
        for (int i = 0; i < (int) fs; ++i) d.process (0.1f);   // power 0.1 = -10 dB: 10 dB over -> 7.5 dB reduction at 4:1
        check (std::abs (d.getGainReductionDb() + 7.5f) < 0.1f, "compressor static curve", "10 dB over threshold at 4:1: %.2f dB (expected -7.5)", d.getGainReductionDb());
    }

    // ---------------- robustness ----------------
    {
        auto p = makeProc (fs, fullChain);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer m;
        buf.clear();
        buf.setSample (0, 10, std::numeric_limits<float>::quiet_NaN());
        buf.setSample (1, 20, std::numeric_limits<float>::infinity());
        p->processBlock (buf, m);
        bool finite = true;
        float peak = 0.0f;
        for (int k = 0; k < 400; ++k)
        {
            for (int i = 0; i < block; ++i) { const float v = 0.1f * (float) std::sin (0.05 * (k * block + i)); buf.setSample (0, i, v); buf.setSample (1, i, v); }
            p->processBlock (buf, m);
            if (k > 300) for (int i = 0; i < block; ++i) { finite = finite && std::isfinite (buf.getSample (0, i)); peak = juce::jmax (peak, std::abs (buf.getSample (0, i))); }
        }
        check (finite && peak > 0.01f, "NaN/Inf from an earlier plugin", "output finite and playing again: %s", finite && peak > 0.01f ? "yes" : "no");
    }
    {
        // host bypass = ORIGINAL: latency-compensated
        auto p = makeProc (fs, fullChain);
        check (p->getBypassParameter() != nullptr, "host bypass parameter", "%s", p->getBypassParameter() != nullptr ? "mapped to ORIGINAL" : "missing");
        p->getBypassParameter()->setValueNotifyingHost (1.0f);
        std::vector<float> l ((size_t) fs, 0.0f), r ((size_t) fs, 0.0f);
        for (size_t i = 0; i < l.size(); ++i) l[i] = r[i] = 0.2f * (float) std::sin (twoPi * 330.0 * (double) i / fs);
        auto lo = l, ro = r;
        run (*p, lo, ro);
        const size_t lat = (size_t) p->getLatencySamples();
        float err = 0.0f;
        const size_t processed = lo.size() / block * block;   // run() only processes whole blocks
        for (size_t i = 24000; i < processed; ++i) err = juce::jmax (err, std::abs (lo[i] - l[i - lat]));
        check (err < 1.0e-5f, "host bypass is latency-compensated", "max difference to the delayed input %.1e", err);
    }
    {
        auto eqSetup = [] (AuricMaster3Processor& p) { setP (p, "eq2_freq", 1000.0f); setP (p, "eq2_gain", 6.0f); };
        float worst = 0.0f;
        const char* worstName = "";
        struct Sw { const char* id; float a, b; Setup s; };
        const Sw sws[] = { { "eq2_mode", 0, 2, eqSetup }, { "eq_on", 1, 0, eqSetup }, { "lim_mode", 0, 2, [] (AuricMaster3Processor& p) { setP (p, "lim_on", 1.0f); } },
                           { "res_on", 0, 1, nullptr }, { "lim_on", 0, 1, nullptr }, { "dyn_on", 0, 1, [] (AuricMaster3Processor& p) { setP (p, "mb_mid_thr", -30.0f); } },
                           { "glue_on", 0, 1, [] (AuricMaster3Processor& p) { setP (p, "glue_thr", -30.0f); } }, { "sat_on", 0, 1, nullptr },
                           { "mono_freq", 39, 120, nullptr },
                           { "eq3_dyn", 0, 1, [] (AuricMaster3Processor& p) { setP (p, "eq3_freq", 1000.0f); setP (p, "eq3_thr", -40.0f); setP (p, "eq3_range", -9.0f); } }, { "lowcut", 20, 80, nullptr }, { "bypass", 0, 1, [] (AuricMaster3Processor& p) { fullChain (p); } } };
        for (auto& s : sws)
        {
            const float c = switchClickDb (s.id, s.a, s.b, s.s);
            if (c > worst) { worst = c; worstName = s.id; }
        }
        check (worst < 6.0f, "click-free switching (12 controls)", "worst %.1f dB above steady state (%s)", worst, worstName);
    }
    {
        auto pEx = makeProc (fs, [] (AuricMaster3Processor& p) { setP (p, "sat_on", 1.0f); setP (p, "sat_high", 100.0f); setP (p, "sat_low", 0.0f); });
        auto pEx44 = makeProc (44100.0, [] (AuricMaster3Processor& p) { setP (p, "sat_on", 1.0f); setP (p, "sat_high", 100.0f); setP (p, "sat_low", 0.0f); });
        auto pCl = makeProc (fs, [] (AuricMaster3Processor& p) { setP (p, "lim_on", 1.0f); setP (p, "clip", 100.0f); setP (p, "lim_drive", 12.0f); });
        const float a = juce::jmax (inharmonicDb (*pEx, 11000.0f, 0.5f), inharmonicDb (*pEx, 7000.0f, 0.5f));
        const float a44 = inharmonicDb (*pEx44, 10000.0f, 0.5f, 44100.0);
        const float c = inharmonicDb (*pCl, 9000.0f, 0.5f);
        check (a < -70.0f && a44 < -70.0f, "exciter aliasing", "worst %.1f dB (48 kHz), %.1f dB (44.1 kHz, 10 kHz tone)", a, a44);
        check (c < -45.0f, "clipper + limiter aliasing", "%.1f dB (9 kHz, +12 dB drive, clipper 100 %%)", c);
    }
    {
        auto q = std::make_unique<AuricMaster3Processor>();
        AuricMaster3Processor::BusesLayout mono;
        mono.inputBuses.add (juce::AudioChannelSet::mono());
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        const bool ok = q->setBusesLayout (mono);
        fullChain (*q);
        q->setRateAndBufferSizeDetails (fs, block);
        q->prepareToPlay (fs, block);
        juce::AudioBuffer<float> buf (1, block);
        juce::MidiBuffer m;
        float peak = 0.0f; bool finite = true;
        for (int k = 0; k < 400; ++k)
        {
            for (int i = 0; i < block; ++i) buf.setSample (0, i, 0.2f * (float) std::sin (twoPi * 220.0 * (k * block + i) / fs));
            q->processBlock (buf, m);
            for (int i = 0; i < block; ++i) { finite = finite && std::isfinite (buf.getSample (0, i)); peak = juce::jmax (peak, std::abs (buf.getSample (0, i))); }
        }
        check (ok && finite && peak > 0.05f, "mono track, full chain", "layout %s, peak %.2f", ok ? "accepted" : "rejected", peak);
    }
    {
        // snapshots and project reload
        auto p = makeProc();
        setP (*p, "glue_thr", -30.0f);
        p->selectSnapshot (1);
        setP (*p, "glue_thr", -10.0f);
        p->selectSnapshot (0);
        const float back = val (*p, "glue_thr");
        juce::MemoryBlock state;
        p->getStateInformation (state);
        auto q = makeProc();
        q->setStateInformation (state.getData(), (int) state.getSize());
        const float reloadedA = val (*q, "glue_thr");
        q->selectSnapshot (1);
        check (std::abs (back + 30.0f) < 0.01f && std::abs (reloadedA + 30.0f) < 0.01f && std::abs (val (*q, "glue_thr") + 10.0f) < 0.01f,
               "snapshots + project reload", "A %.0f, reloaded A %.0f, B %.0f", back, reloadedA, val (*q, "glue_thr"));
    }
    {
        // large host blocks give the same result
        auto a = makeProc (fs, fullChain), b = makeProc (fs, fullChain);
        std::vector<float> l1, r1;
        music (l1, r1, fs, 3.0, 0.2f, 3);
        auto l2 = l1, r2 = r1;
        run (*a, l1, r1, block);
        run (*b, l2, r2, block * 16);
        const size_t full = l1.size() / (size_t) (block * 16) * (size_t) (block * 16);
        float diff = 0.0f;
        for (size_t i = 0; i < full; ++i) diff = juce::jmax (diff, std::abs (l1[i] - l2[i]));
        check (diff < 1.0e-5f, "large host blocks", "8192 vs 512 samples: max difference %.1e", diff);
    }

    // ---------------- undo / redo, presets ----------------
    {
        auto p = makeProc();
        p->beginEdit();
        setP (*p, "glue_thr", -30.0f);
        p->endEdit();
        p->beginEdit();
        setP (*p, "glue_thr", -10.0f);
        p->endEdit();
        p->beginEdit();   // a click without a change must not create an undo step
        p->endEdit();
        p->undo();
        const float u1 = val (*p, "glue_thr");
        p->undo();
        const float u2 = val (*p, "glue_thr");
        p->redo();
        const float r1 = val (*p, "glue_thr");
        check (std::abs (u1 + 30.0f) < 0.01f && std::abs (u2 + 16.0f) < 0.01f && std::abs (r1 + 30.0f) < 0.01f, "undo / redo",
               "after undo %.0f, undo %.0f, redo %.0f (expected -30, -16, -30)", u1, u2, r1);
        // automation (not from the editor) is not recorded as an undo step
        const int depth = p->getUndoDepth();
        setP (*p, "glue_thr", -5.0f);
        check (p->getUndoDepth() == depth, "undo ignores automation", "undo steps %d -> %d", depth, p->getUndoDepth());
    }
    {
        const auto names = AuricMaster3Processor::getFactoryPresetNames();
        bool allOk = names.size() >= 8;
        juce::String bad;
        for (int i = 0; i < names.size(); ++i)
        {
            auto p = makeProc();
            p->loadFactoryPreset (i);
            std::vector<float> l, r;
            material (l, r, -24.0f, 0.05f, 70);
            l.resize ((size_t) fs * 6); r.resize ((size_t) fs * 6);
            run (*p, l, r);
            bool finite = true;
            float peak = 0.0f;
            for (size_t k = 0; k < l.size(); ++k) { finite = finite && std::isfinite (l[k]) && std::isfinite (r[k]); peak = juce::jmax (peak, std::abs (l[k])); }
            if (! finite || peak > 1.0f || p->getPresetName() != names[i]) { allOk = false; bad = names[i]; }
        }
        check (allOk, "factory presets load and play", "%d presets%s%s", names.size(), bad.isEmpty() ? "" : ", problem: ", bad.toRawUTF8());

        // user preset round trip in a temporary folder; monitoring switches are not part of a preset
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("am3-preset-test");
        dir.deleteRecursively();
        auto a = makeProc();
        a->setUserPresetDirectory (dir);
        setP (*a, "glue_on", 1.0f); setP (*a, "glue_thr", -27.0f); setP (*a, "eq3_gain", 2.5f);
        const bool saved = a->saveUserPreset ("Mein Test");
        auto b = makeProc();
        b->setUserPresetDirectory (dir);
        setP (*b, "bypass", 1.0f);
        const auto files = b->getUserPresetFiles();
        const bool loaded = files.size() == 1 && b->loadPresetFile (files[0]);
        check (saved && loaded && std::abs (val (*b, "glue_thr") + 27.0f) < 0.01f && std::abs (val (*b, "eq3_gain") - 2.5f) < 0.01f
               && val (*b, "bypass") > 0.5f && b->getPresetName() == "Mein Test" && ! b->isPresetModified(),
               "user preset save / load", "glue %.0f dB, EQ3 %+.1f dB, bypass kept: %s", val (*b, "glue_thr"), val (*b, "eq3_gain"), val (*b, "bypass") > 0.5f ? "yes" : "no");
        setP (*b, "glue_thr", -20.0f);
        check (b->isPresetModified(), "preset modified flag", "%s", b->isPresetModified() ? "set after a change" : "missing");
        dir.deleteRecursively();
    }

    // ---------------- assistant ----------------
    {
        auto shelfDb = [fs] (bool high, float freq, float gainDb)
        {
            return [=] (float f)
            {
                Biquad b;
                if (high) b.setHighShelf (fs, freq, 0.7, gainDb); else b.setLowShelf (fs, freq, 0.7, gainDb);
                return (float) b.magnitudeDb (fs, f);
            };
        };
        std::vector<float> l, r;
        shapedNoise (l, r, fs, -20.0f, AutoMatcher::pop, nullptr, 0.4f, 11);
        auto np = makeProc();
        const bool ok1 = runAssistant (*np, l, r);
        const float e[4] = { val (*np, "eq1_gain"), val (*np, "eq2_gain"), val (*np, "eq3_gain"), val (*np, "eq4_gain") };
        check (ok1 && std::abs (e[0]) <= 0.6f && std::abs (e[1]) <= 0.6f && std::abs (e[2]) <= 0.6f && std::abs (e[3]) <= 0.6f, "assistant: neutral material",
               "EQ %+.1f/%+.1f/%+.1f/%+.1f dB | %s", e[0], e[1], e[2], e[3], np->getLearnStatus().toRawUTF8());
        check (np->getLearnState() == AuricMaster3Processor::learnDone && val (*np, "lim_on") > 0.5f, "assistant ends with LERNEN", "%s", np->getAssistSummary().toRawUTF8());

        shapedNoise (l, r, fs, -20.0f, AutoMatcher::pop, shelfDb (false, 100.0f, 6.0f), 0.4f, 12);
        auto bp = makeProc();
        runAssistant (*bp, l, r);
        const float low = val (*bp, "eq1_gain");
        check (low < -2.0f && low > -4.01f, "assistant: +6 dB bass", "low shelf %+.1f dB", low);
        setP (*bp, "eq1_gain", 2.0f);
        runAssistant (*bp, l, r);
        const float second = val (*bp, "eq1_gain");
        bp->undoAssistant();
        check (std::abs (second - low) < 0.15f && std::abs (val (*bp, "eq1_gain") - 2.0f) < 0.01f, "assistant: repeatable + undo",
               "2nd run %+.1f dB, after undo %+.1f dB (user value +2.0)", second, val (*bp, "eq1_gain"));

        shapedNoise (l, r, fs, -20.0f, AutoMatcher::pop, shelfDb (true, 8000.0f, -6.0f), 0.4f, 13);
        auto dp = makeProc();
        runAssistant (*dp, l, r);
        check (val (*dp, "eq4_gain") > 2.0f, "assistant: dull mix", "high shelf %+.1f dB", val (*dp, "eq4_gain"));

        shapedNoise (l, r, fs, -20.0f, AutoMatcher::pop, nullptr, 0.85f, 14, 20.0f);
        auto wp = makeProc();
        runAssistant (*wp, l, r);
        check (val (*wp, "width_high") < 100.0f && val (*wp, "mono_freq") >= 140.0f, "assistant: very wide mix", "width %.0f %%, mono bass %.0f Hz",
               val (*wp, "width_high"), val (*wp, "mono_freq"));

        shapedNoise (l, r, fs, -20.0f, AutoMatcher::pop, nullptr, 0.4f, 11);
        for (size_t i = 0; i < l.size(); ++i) { l[i] = std::tanh (5.0f * l[i]) * 0.2f; r[i] = std::tanh (5.0f * r[i]) * 0.2f; }
        auto sp = makeProc();
        runAssistant (*sp, l, r);
        check (val (*sp, "mb_mid_ratio") < val (*np, "mb_mid_ratio"), "assistant: squashed mix gets less", "ratio %.1f vs %.1f (open mix)",
               val (*sp, "mb_mid_ratio"), val (*np, "mb_mid_ratio"));
    }

    if (std::getenv ("AM3_CPU"))
    {
        const char* mods[] = { "none", "res_on", "eq_on", "dyn_on", "sat_on", "st_on", "glue_on", "lim_on" };
        for (double rate : { 48000.0, 192000.0 })
            for (auto* m : mods)
            {
                auto p = makeProc (rate, [m] (AuricMaster3Processor& q) {
                    for (auto* id : { "res_on", "eq_on", "dyn_on", "sat_on", "st_on", "glue_on", "lim_on" }) setP (q, id, 0.0f);
                    if (std::string (m) != "none") setP (q, m, 1.0f);
                    if (std::string (m) == "st_on") setP (q, "width_high", 120.0f); });
                std::vector<float> l, r;
                music (l, r, rate, 6.0, 0.2f, 5);
                const double t0 = juce::Time::getMillisecondCounterHiRes();
                run (*p, l, r);
                std::printf ("  cpu %6.0f Hz %-8s %.2f %%\n", rate, m, 100.0 * (juce::Time::getMillisecondCounterHiRes() - t0) / 6000.0);
            }
        return 0;
    }

    // ---------------- CPU ----------------
    for (double rate : { 48000.0, 192000.0 })
    {
        auto p = makeProc (rate, fullChain);
        std::vector<float> l, r;
        music (l, r, rate, 6.0, 0.2f, 5);
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        run (*p, l, r);
        const double pct = 100.0 * (juce::Time::getMillisecondCounterHiRes() - t0) / 6000.0;
        char name[64]; std::snprintf (name, sizeof (name), "CPU full chain @ %.0f kHz", rate / 1000.0);
        // on shared CI machines and in sanitizer builds timing is not meaningful: report only
       #if defined (__SANITIZE_ADDRESS__)
        const bool timingInfoOnly = true;
       #else
        const bool timingInfoOnly = std::getenv ("CI") != nullptr;
       #endif
        check (timingInfoOnly || pct < (rate > 100000.0 ? 30.0 : 15.0), name, "%.1f %% of one core%s", pct, timingInfoOnly ? " (informational)" : "");
    }

    std::printf ("\n%d of %d checks passed\n", checks - failures, checks);
    juce::ignoreUnused (argc, argv);
    return failures;
}
