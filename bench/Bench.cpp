#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <chrono>
#include <sys/resource.h>

static double run (double sr, int block, bool resOn, int warmth, float seconds)
{
    AuricMasterProcessor p;
    p.setPlayConfigDetails (2, 2, sr, block);
    p.prepareToPlay (sr, block);
    p.resonance.cancelLearn();
    auto setP = [&] (const char* id, float v) { *p.apvts.getRawParameterValue (id) = v; };
    setP ("res_on", resOn ? 1.0f : 0.0f);
    setP ("warmth", (float) warmth);
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer m;
    juce::Random r (1);
    const int blocks = (int) (seconds * sr / block);
    double t = 0.0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int c = 0; c < 2; ++c) for (int i = 0; i < block; ++i) buf.setSample (c, i, 0.3f * (r.nextFloat() - 0.5f));
        auto t0 = std::chrono::high_resolution_clock::now();
        p.processBlock (buf, m);
        t += std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
    }
    return 100.0 * t / (blocks * block / sr); // % of one core
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    std::printf ("CPU-Last eines Kerns (%% der Echtzeit):\n");
    std::printf ("  44.1 kHz, 512 Samples, alles an      : %.2f %%\n", run (44100, 512, true, 25, 20));
    std::printf ("  48 kHz,   256 Samples, alles an      : %.2f %%\n", run (48000, 256, true, 25, 20));
    std::printf ("  48 kHz,   512 Samples, Resonance aus : %.2f %%\n", run (48000, 512, false, 25, 20));
    std::printf ("  96 kHz,   512 Samples, alles an      : %.2f %%\n", run (96000, 512, true, 25, 20));
    std::printf ("  48 kHz,   512 Samples, Warmth 0      : %.2f %%\n", run (48000, 512, true, 0, 20));
    struct rusage ru; getrusage (RUSAGE_SELF, &ru);
    std::printf ("Max. Speicher des Testprozesses: %.1f MB\n", ru.ru_maxrss / 1024.0);
    return 0;
}
