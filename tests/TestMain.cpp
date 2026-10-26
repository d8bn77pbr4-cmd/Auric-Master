#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>
#include <cstdlib>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
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

    if (argc > 1)
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File f (argv[1]); f.deleteFile();
        juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        p.editorBeingDeleted (ed.get());
    }
    return bad ? 1 : 0;
}
