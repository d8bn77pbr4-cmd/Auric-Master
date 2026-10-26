#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>
#include <cstdlib>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    // --- ResonanceSuppressor unit tests ---
    {
        ResonanceSuppressor rs;
        rs.prepare (48000.0, 2);
        ResonanceSuppressor::Settings st; st.depth = 0.0f;
        rs.setSettings (st);
        const int L = rs.getLatency(), total = 48000;
        juce::AudioBuffer<float> in (2, total), out;
        juce::Random r (7);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < total; ++i) in.setSample (c, i, r.nextFloat() - 0.5f);
        out.makeCopyOf (in);
        for (int s = 0; s < total; s += 480) { juce::AudioBuffer<float> blk (out.getArrayOfWritePointers(), 2, s, 480); rs.process (blk, 2); }
        float err = 0;
        for (int i = L + 4096; i < total; ++i) err = juce::jmax (err, std::abs (out.getSample (0, i) - in.getSample (0, i - L)));
        std::printf ("[resonance] depth0 identity max error = %.2e (latency %d)\n", err, L);

        // resonance test: pink-ish noise + loud 3 kHz tone
        auto run = [&] (float depth, float harshAmt)
        {
            ResonanceSuppressor rr; rr.prepare (48000.0, 1);
            ResonanceSuppressor::Settings s2; s2.depth = depth; s2.sensitivity = 0.5f;
            s2.nodes[2] = { 3000.0f, harshAmt, 1.0f };
            rr.setSettings (s2);
            juce::AudioBuffer<float> b (1, total);
            juce::Random rn (3);
            for (int i = 0; i < total; ++i)
                b.setSample (0, i, 0.1f * (rn.nextFloat() - 0.5f) + 0.3f * (float) std::sin (2 * juce::MathConstants<double>::pi * 3000.0 * i / 48000.0));
            rr.process (b, 1);
            // measure 3 kHz component via correlation on the second half
            double re = 0, im = 0;
            for (int i = total / 2; i < total; ++i)
            {
                const double ph = 2 * juce::MathConstants<double>::pi * 3000.0 * (i - L) / 48000.0;
                re += b.getSample (0, i) * std::sin (ph); im += b.getSample (0, i) * std::cos (ph);
            }
            return 20.0 * std::log10 (2.0 * std::sqrt (re * re + im * im) / (total / 2) / 0.3);
        };
        std::printf ("[resonance] 3 kHz tone level: depth 0 = %.1f dB, depth 50%% = %.1f dB, depth 50%% + HARSH zone +100%% = %.1f dB, HARSH -100%% = %.1f dB\n",
                     run (0.0f, 0.0f), run (0.5f, 0.0f), run (0.5f, 1.0f), run (0.5f, -1.0f));
    }
    // --- Auto-match test: pink-ish noise with mud build-up at 300 Hz and a resonance at 3.2 kHz ---
    {
        AuricMasterProcessor ap;
        ap.setPlayConfigDetails (2, 2, 48000.0, 512);
        ap.prepareToPlay (48000.0, 512);   // fresh instance -> learning starts automatically
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer m;
        juce::Random rn (11);
        // simple coloured noise: pink-ish (1-pole lowpass mix) + resonant bandpasses
        auto bp = [] (double f, double q) { return juce::dsp::IIR::Coefficients<float>::makeBandPass (48000.0, (float) f, (float) q); };
        juce::dsp::IIR::Filter<float> pinkLp, mud, harsh;
        pinkLp.coefficients = juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (48000.0, 400.0f);
        mud.coefficients = bp (300.0, 1.2);
        harsh.coefficients = bp (3200.0, 8.0);
        const int blocks = (int) (14.0 * 48000.0 / 512);
        for (int k = 0; k < blocks; ++k)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float w = rn.nextFloat() * 2.0f - 1.0f;
                const float x = 0.3f * pinkLp.processSample (w) + 0.03f * w + 0.5f * mud.processSample (w) + 0.25f * harsh.processSample (w);
                b.setSample (0, i, x); b.setSample (1, i, x);
            }
            ap.processBlock (b, m);
            if (k % 20 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        if (auto* r = ap.getAutoResult())
        {
            std::printf ("[auto] depth=%.0f%%  summary: %s\n", r->depth, r->summary.toRawUTF8());
            for (int i = 0; i < numClashBands; ++i)
                std::printf ("[auto]   %-9s %7.0f Hz  %+5.0f %%  %.2f oct\n", AuricMasterProcessor::clashNames[i], r->bands[(size_t) i].freq, r->bands[(size_t) i].amount, r->bands[(size_t) i].width);
            std::printf ("[auto] param clash4_freq now = %.0f Hz\n", ap.apvts.getRawParameterValue ("clash4_freq")->load());
        }
        else
            std::printf ("[auto] NO RESULT (learning=%d progress=%.2f)\n", (int) ap.isAutoLearning(), ap.getAutoProgress());
    }

    AuricMasterProcessor p;
    p.setPlayConfigDetails (2, 2, 48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    *p.apvts.getRawParameterValue ("punch") = std::getenv ("P") ? std::atof (std::getenv ("P")) : 80.0f;
    *p.apvts.getRawParameterValue ("warmth") = 70.0f;
    *p.apvts.getRawParameterValue ("input") = std::getenv ("I") ? std::atof (std::getenv ("I")) : 6.0f;
    *p.apvts.getRawParameterValue ("bass") = 6.0f;
    *p.apvts.getRawParameterValue ("ceiling") = -1.0f;

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    juce::Random rng (1);
    float maxOut = 0.0f; bool bad = false; double phase = 0;
    for (int b = 0; b < 400; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            float s = 0.7f * (float) std::sin (phase) + 0.3f * (rng.nextFloat() * 2 - 1);
            phase += 2 * juce::MathConstants<double>::pi * 80.0 / 48000.0;
            buf.setSample (0, i, s); buf.setSample (1, i, s * 0.8f);
        }
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
            {
                float v = buf.getSample (c, i);
                if (! std::isfinite (v)) bad = true;
                if (b > 10) maxOut = juce::jmax (maxOut, std::abs (v));
            }
    }
    std::printf ("latency=%d  maxOut=%.2f dBFS (ceiling -1.0)  compGR=%.1f dB  limGR=%.1f dB  nan=%d\n",
                 p.getLatencySamples(), juce::Decibels::gainToDecibels (maxOut), p.compGainDb.load(), p.limGainDb.load(), (int) bad);

    if (argc > 2)
    {
        auto setP = [&] (const char* id, float v) { auto* pr = p.apvts.getParameter (id); pr->setValueNotifyingHost (pr->convertTo0to1 (v)); };
        setP ("clash4_amt", 60.0f);
        setP ("clash2_amt", -40.0f);
        setP ("clash6_amt", -30.0f);
        setP ("res_depth", 60.0f);
        setP ("input", 0.0f);

        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ClashDisplay* disp = nullptr;
        for (auto* c : ed->getChildren())
            if (auto* d = dynamic_cast<ClashDisplay*> (c)) disp = d;

        double ph2 = 0.0;
        for (int b = 0; b < 300; ++b)
        {
            const double resF = 1500.0 + 2500.0 * std::sin (b * 0.03); // moving resonance
            for (int i = 0; i < 512; ++i)
            {
                float s = 0.08f * (rng.nextFloat() * 2 - 1) + 0.3f * (float) std::sin (phase)
                        + ((b / 40) % 2 == 0 ? 0.15f : 0.03f) * (float) std::sin (ph2);
                phase += 2 * juce::MathConstants<double>::pi * 80.0 / 48000.0;
                ph2 += 2 * juce::MathConstants<double>::pi * resF / 48000.0;
                buf.setSample (0, i, s); buf.setSample (1, i, s * 0.9f);
            }
            p.processBlock (buf, midi);
            if (disp) disp->refresh();
        }

        auto snap = [&] (const char* path)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File f (path); f.deleteFile();
            juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        };
        snap (argv[1]);
        for (auto* c : ed->getChildren())
            if (auto* t = dynamic_cast<juce::ToggleButton*> (c))
                if (t->getButtonText() == "RESONANCE") { t->setToggleState (true, juce::dontSendNotification); t->onClick(); }
        snap (argv[2]);
        if (argc > 3)
        {
            // keep playing until the auto-match is done, then show the result
            juce::dsp::IIR::Filter<float> mudF, harshF;
            mudF.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (48000.0, 300.0f, 1.2f);
            harshF.coefficients = juce::dsp::IIR::Coefficients<float>::makeBandPass (48000.0, 3200.0f, 8.0f);
            for (int b = 0; b < 1200 && p.getAutoResult() == nullptr; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float w = rng.nextFloat() * 2 - 1;
                    float s = 0.05f * w + 0.3f * (float) std::sin (phase) + 0.5f * mudF.processSample (w) + 0.3f * harshF.processSample (w);
                    phase += 2 * juce::MathConstants<double>::pi * 80.0 / 48000.0;
                    buf.setSample (0, i, s); buf.setSample (1, i, s * 0.9f);
                }
                p.processBlock (buf, midi);
                if (disp) disp->refresh();
                if (b % 10 == 0) juce::MessageManager::getInstance()->runDispatchLoopUntil (1);
            }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            if (disp) disp->refresh();
            snap (argv[3]);
        }
        p.editorBeingDeleted (ed.get());
    }
    return bad ? 1 : 0;
}
