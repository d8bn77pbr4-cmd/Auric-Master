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
        p.editorBeingDeleted (ed.get());
    }
    return bad ? 1 : 0;
}
