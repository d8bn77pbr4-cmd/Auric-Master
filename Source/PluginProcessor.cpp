#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// LookaheadLimiter
void LookaheadLimiter::prepare (double sampleRate, int numChannels, int lookaheadSamples)
{
    lookahead = juce::jmax (1, lookaheadSamples);
    delayBuf.assign ((size_t) numChannels, std::vector<float> ((size_t) lookahead, 0.0f));
    avgBuf.assign ((size_t) lookahead, 1.0f);
    releaseCoeff = (float) std::exp (-1.0 / (0.080 * sampleRate)); // 80 ms release
    reset();
}

void LookaheadLimiter::reset()
{
    for (auto& d : delayBuf) std::fill (d.begin(), d.end(), 0.0f);
    std::fill (avgBuf.begin(), avgBuf.end(), 1.0f);
    avgSum = (double) lookahead;
    writePos = avgPos = holdCounter = 0;
    hold = env = minGain = 1.0f;
}

void LookaheadLimiter::process (float* const* ch, int numChannels, int numSamples)
{
    numChannels = juce::jmin (numChannels, (int) delayBuf.size());
    minGain = 1.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float peak = 0.0f;
        for (int c = 0; c < numChannels; ++c)
            peak = juce::jmax (peak, std::abs (ch[c][i]));

        const float target = peak > ceiling ? ceiling / peak : 1.0f;

        // peak-hold of the target gain over the lookahead window
        if (target <= hold || holdCounter <= 0)
        {
            hold = target;
            holdCounter = lookahead;
        }
        else
            --holdCounter;

        // moving average over lookahead -> gain reaches 'hold' exactly when the peak leaves the delay
        avgSum += (double) hold - (double) avgBuf[(size_t) avgPos];
        avgBuf[(size_t) avgPos] = hold;
        if (++avgPos >= lookahead) avgPos = 0;
        const float smoothed = (float) (avgSum / lookahead);

        env = smoothed < env ? smoothed : smoothed + (env - smoothed) * releaseCoeff;
        minGain = juce::jmin (minGain, env);

        for (int c = 0; c < numChannels; ++c)
        {
            auto& d = delayBuf[(size_t) c];
            const float delayed = d[(size_t) writePos];
            d[(size_t) writePos] = ch[c][i];
            ch[c][i] = juce::jlimit (-ceiling, ceiling, delayed * env); // safety clip
        }
        if (++writePos >= lookahead) writePos = 0;
    }
}

//==============================================================================
AuricMasterProcessor::AuricMasterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pInput   = apvts.getRawParameterValue ("input");
    pBass    = apvts.getRawParameterValue ("bass");
    pAir     = apvts.getRawParameterValue ("air");
    pPunch   = apvts.getRawParameterValue ("punch");
    pWarmth  = apvts.getRawParameterValue ("warmth");
    pWidth   = apvts.getRawParameterValue ("width");
    pCeiling = apvts.getRawParameterValue ("ceiling");
    pOutput  = apvts.getRawParameterValue ("output");
    pMix     = apvts.getRawParameterValue ("mix");
    pBypass  = apvts.getRawParameterValue ("bypass");
    pResOn    = apvts.getRawParameterValue ("res_on");
    pResDelta = apvts.getRawParameterValue ("res_delta");
    pResDepth = apvts.getRawParameterValue ("res_depth");
    pResSharp = apvts.getRawParameterValue ("res_sharp");
    pResSens  = apvts.getRawParameterValue ("res_sens");
    startTimerHz (10);
    pResAttack  = apvts.getRawParameterValue ("res_attack");
    pResRelease = apvts.getRawParameterValue ("res_release");
    pResMix     = apvts.getRawParameterValue ("res_mix");
    pResTrim    = apvts.getRawParameterValue ("res_trim");
    pResHard    = apvts.getRawParameterValue ("res_hard");
    for (int i = 0; i < numClashBands; ++i)
    {
        const auto n = juce::String (i + 1);
        pClashFreq[(size_t) i]  = apvts.getRawParameterValue ("clash" + n + "_freq");
        pClashAmt[(size_t) i]   = apvts.getRawParameterValue ("clash" + n + "_amt");
        pClashWidth[(size_t) i] = apvts.getRawParameterValue ("clash" + n + "_width");
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout AuricMasterProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto dB = [] (float v, int) { return String (v, 1) + " dB"; };
    auto pct = [] (float v, int) { return String (roundToInt (v)) + " %"; };
    auto attrDb = AudioParameterFloatAttributes().withStringFromValueFunction (dB);
    auto attrPct = AudioParameterFloatAttributes().withStringFromValueFunction (pct);

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "input", 1 },   "Input",   NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, attrDb));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "bass", 1 },    "Bass",    NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, attrDb));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "air", 1 },     "Air",     NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, attrDb));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "punch", 1 },   "Punch",   NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, attrPct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "warmth", 1 },  "Warmth",  NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f, attrPct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 },   "Width",   NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, attrPct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ceiling", 1 }, "Ceiling", NormalisableRange<float> (-3.0f, 0.0f, 0.1f), -0.3f, attrDb));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 },  "Output",  NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, attrDb));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 },     "Mix",     NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, attrPct));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "bypass", 1 },  "Bypass",  false));

    // Resonance suppressor
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "res_on", 1 },    "Resonance On",          true));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "res_delta", 1 }, "Resonance Delta",       false));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "res_depth", 1 }, "Resonance Depth",       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, attrPct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "res_sharp", 1 }, "Resonance Sharpness",   NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, attrPct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "res_sens", 1 },  "Resonance Selectivity", NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, attrPct));
    auto ms = [] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ms"; };
    auto attrMs = AudioParameterFloatAttributes().withStringFromValueFunction (ms);
    NormalisableRange<float> attRange (0.5f, 100.0f, 0.1f), relRange (10.0f, 500.0f, 1.0f);
    attRange.setSkewForCentre (8.0f);
    relRange.setSkewForCentre (100.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "res_attack", 1 },  "Resonance Attack",  attRange, 5.0f, attrMs));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "res_release", 1 }, "Resonance Release", relRange, 80.0f, attrMs));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "res_mix", 1 },     "Resonance Mix",     NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, attrPct));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "res_trim", 1 },    "Resonance Trim",    NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, attrDb));
    p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { "res_hard", 1 },    "Resonance Hard Mode", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "auto_profile", 1 }, "Auto Match Profile", AutoMatcher::profileNames(), 0));

    auto hz = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto oct = [] (float v, int) { return String (v, 2) + " oct"; };
    auto bipct = [] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v)) + " %"; };
    NormalisableRange<float> freqRange (20.0f, 20000.0f);
    freqRange.setSkewForCentre (1000.0f);
    for (int i = 0; i < numClashBands; ++i)
    {
        const auto n = String (i + 1);
        const auto name = String ("Clash ") + clashNames[i];
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "clash" + n + "_freq", 1 }, name + " Freq", freqRange, clashDefaultFreqs[i],
                                                            AudioParameterFloatAttributes().withStringFromValueFunction (hz)));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "clash" + n + "_amt", 1 }, name + " Amount", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction (bipct)));
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "clash" + n + "_width", 1 }, name + " Width", NormalisableRange<float> (0.25f, 3.0f, 0.01f), 1.0f,
                                                            AudioParameterFloatAttributes().withStringFromValueFunction (oct)));
    }

    return { p.begin(), p.end() };
}

bool AuricMasterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void AuricMasterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const auto numCh = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, numCh };
    lowShelf.prepare (spec);
    highShelf.prepare (spec);
    lowCut.prepare (spec);
    *lowCut.state = *juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 20.0f, 0.707f);
    lastBass = lastAir = 999.0f;
    updateFilters();

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        numCh, osFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    const int osFactor = 1 << osFactorLog2;
    const double osRate = sampleRate * osFactor;
    // 1.5 ms lookahead, rounded to a multiple of the OS factor so latency is an integer at base rate
    int la = (int) std::ceil (0.0015 * osRate);
    la = ((la + osFactor - 1) / osFactor) * osFactor;
    limiter.prepare (osRate, (int) numCh, la);

    resonance.prepare (sampleRate, (int) numCh);
    if (pendingAutoLearn)
    {
        // freshly inserted on a mixer track: analyse the first seconds of audio automatically
        pendingAutoLearn = false;
        resonance.requestLearn (autoLearnSeconds);
    }

    const int latency = resonance.getLatency() + (int) std::round (oversampling->getLatencyInSamples()) + la / osFactor;
    setLatencySamples (latency);

    dryDelay.prepare ((int) numCh, latency + 8);
    dryDelay.setDelay (latency);
    dryBuffer.setSize ((int) numCh, samplesPerBlock);

    compEnv = 0.0f;
    compGain = 1.0f;

    for (auto* s : { &inGainS, &outGainS, &widthS, &warmthS, &mixS })
        s->reset (sampleRate, 0.03);
    inGainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInput->load()));
    outGainS.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    widthS.setCurrentAndTargetValue (pWidth->load() * 0.01f);
    warmthS.setCurrentAndTargetValue (pWarmth->load() * 0.01f);
    mixS.setCurrentAndTargetValue (pMix->load() * 0.01f);
}

void AuricMasterProcessor::updateFilters()
{
    const float bass = pBass->load(), air = pAir->load();
    if (! juce::exactlyEqual (bass, lastBass))
    {
        *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 100.0f, 0.6f, juce::Decibels::decibelsToGain (bass));
        lastBass = bass;
    }
    if (! juce::exactlyEqual (air, lastAir))
    {
        const float f = (float) juce::jmin (12000.0, sr * 0.45);
        *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, f, 0.6f, juce::Decibels::decibelsToGain (air));
        lastAir = air;
    }
}

static float peakOf (const juce::AudioBuffer<float>& b, int ch)
{
    return ch < b.getNumChannels() ? b.getMagnitude (ch, 0, b.getNumSamples()) : 0.0f;
}

void AuricMasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), getTotalNumOutputChannels());
    if (numSamples == 0 || numCh == 0) return;

    for (int ch = getTotalNumInputChannels(); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    inPeakL = juce::jmax (inPeakL.load(), peakOf (buffer, 0));
    inPeakR = juce::jmax (inPeakR.load(), peakOf (buffer, numCh > 1 ? 1 : 0));

    // keep a latency-aligned copy of the dry signal (for Mix and Bypass)
    dryBuffer.setSize (numCh, numSamples, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    dryDelay.process (dryBuffer);

    if (pBypass->load() > 0.5f)
    {
        for (int ch = 0; ch < numCh; ++ch)
            buffer.copyFrom (ch, 0, dryBuffer, ch, 0, numSamples);
        compGainDb = 0.0f;
        limGainDb = 0.0f;
        outPeakL = juce::jmax (outPeakL.load(), peakOf (buffer, 0));
        outPeakR = juce::jmax (outPeakR.load(), peakOf (buffer, numCh > 1 ? 1 : 0));
        return;
    }

    inGainS.setTargetValue (juce::Decibels::decibelsToGain (pInput->load()));
    outGainS.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    widthS.setTargetValue (pWidth->load() * 0.01f);
    warmthS.setTargetValue (pWarmth->load() * 0.01f);
    mixS.setTargetValue (pMix->load() * 0.01f);

    // 1) Input gain
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = inGainS.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }

    // 2) Tone: DC/sub cleanup, bass shelf, air shelf
    updateFilters();
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        lowCut.process (ctx);
        lowShelf.process (ctx);
        highShelf.process (ctx);
    }

    // 3) Resonance suppression (frequency clashes)
    const bool deltaListen = pResDelta->load() > 0.5f && pResOn->load() > 0.5f;
    {
        ResonanceSuppressor::Settings rs;
        rs.enabled = pResOn->load() > 0.5f;
        rs.delta = pResDelta->load() > 0.5f;
        rs.depth = pResDepth->load() * 0.01f;
        rs.sharpness = pResSharp->load() * 0.01f;
        rs.sensitivity = pResSens->load() * 0.01f;
        rs.hard = pResHard->load() > 0.5f;
        rs.attackMs = pResAttack->load();
        rs.releaseMs = pResRelease->load();
        rs.mix = pResMix->load() * 0.01f;
        rs.trimDb = pResTrim->load();
        for (size_t i = 0; i < (size_t) numClashBands; ++i)
            rs.nodes[i] = { pClashFreq[i]->load(), pClashAmt[i]->load() * 0.01f, pClashWidth[i]->load(), clashTypes[i] };
        resonance.setSettings (rs);
        resonance.process (buffer, numCh);
    }

    // 4) Punch: stereo-linked glue compressor, soft knee, auto makeup
    if (! deltaListen)
    {
        const float punch = pPunch->load() * 0.01f;
        const float threshold = -2.0f - 22.0f * punch;   // dB
        const float ratio = 1.5f + 2.5f * punch;
        const float knee = 6.0f;
        const float makeup = -threshold * (1.0f - 1.0f / ratio) * 0.7f;
        const float att = std::exp (-1.0f / (0.012f * (float) sr));   // 12 ms
        const float rel = std::exp (-1.0f / (0.150f * (float) sr));   // 150 ms
        const float gSmooth = std::exp (-1.0f / (0.003f * (float) sr));
        float minG = 1.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float level = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                level = juce::jmax (level, std::abs (buffer.getSample (ch, i)));

            compEnv = level > compEnv ? level + (compEnv - level) * att : level + (compEnv - level) * rel;

            float gr = 0.0f;
            if (punch > 0.0f)
            {
                const float x = juce::Decibels::gainToDecibels (compEnv, -120.0f);
                const float over = x - threshold;
                if (2.0f * over > knee)
                    gr = over * (1.0f / ratio - 1.0f);
                else if (2.0f * std::abs (over) <= knee)
                    gr = (1.0f / ratio - 1.0f) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
            }
            const float target = juce::Decibels::decibelsToGain (gr);
            compGain = target + (compGain - target) * gSmooth;
            minG = juce::jmin (minG, compGain);

            const float g = compGain * juce::Decibels::decibelsToGain (punch > 0.0f ? makeup : 0.0f);
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
        compGainDb = juce::Decibels::gainToDecibels (minG);
    }
    else
    {
        compGainDb = 0.0f;
    }

    // 5) Stereo width (M/S)
    if (numCh > 1 && ! deltaListen)
    {
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float w = widthS.getNextValue();
            const float m = 0.5f * (l[i] + r[i]);
            const float s = 0.5f * (l[i] - r[i]) * w;
            l[i] = m + s;
            r[i] = m - s;
        }
    }
    else
        widthS.skip (numSamples);

    // 6) Oversampled: warmth saturation + lookahead limiter
    {
        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) numSamples);
        auto os = oversampling->processSamplesUp (block);
        const int osN = (int) os.getNumSamples();
        const int factor = 1 << osFactorLog2;

        constexpr float bias = 0.15f;
        const float tb = std::tanh (bias);
        const float slope0 = 1.0f - tb * tb;

        for (int i = 0; i < osN; ++i)
        {
            if (i % factor == 0) warmthS.getNextValue();
            const float w = warmthS.getCurrentValue();
            if (w <= 0.0001f || deltaListen) continue;
            const float k = 1.0f + 5.0f * w;
            const float norm = 1.0f / (k * slope0);
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = os.getChannelPointer ((size_t) ch);
                const float x = d[i];
                const float sat = (std::tanh (k * x + bias) - tb) * norm; // asymmetric -> even harmonics
                d[i] = x + w * (sat - x);
            }
        }

        const float ceilingGain = juce::Decibels::decibelsToGain (pCeiling->load());
        limiter.setCeiling (deltaListen ? 1.0f : ceilingGain * 0.955f); // 0.4 dB headroom for the downsampling filter
        float* chans[2] = { os.getChannelPointer (0), numCh > 1 ? os.getChannelPointer (1) : nullptr };
        limiter.process (chans, numCh, osN);
        limGainDb = juce::Decibels::gainToDecibels (limiter.getGainReduction());

        oversampling->processSamplesDown (block);

        // final sample-peak safety at base rate
        for (int ch = 0; ch < numCh; ++ch)
            juce::FloatVectorOperations::clip (buffer.getWritePointer (ch), buffer.getReadPointer (ch), -ceilingGain, ceilingGain, numSamples);
    }

    // 7) Output gain + dry/wet
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = outGainS.getNextValue();
        const float mix = deltaListen ? 1.0f : mixS.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            d[i] = (d[i] * mix + dryBuffer.getSample (ch, i) * (1.0f - mix)) * g;
        }
    }

    outPeakL = juce::jmax (outPeakL.load(), peakOf (buffer, 0));
    outPeakR = juce::jmax (outPeakR.load(), peakOf (buffer, numCh > 1 ? 1 : 0));
}

juce::AudioProcessorEditor* AuricMasterProcessor::createEditor()
{
    return new AuricMasterEditor (*this);
}

void AuricMasterProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AuricMasterProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            // restored project: keep the user's settings, no automatic re-analysis
            if (apvts.state.getProperty ("autoLearned", false))
            {
                pendingAutoLearn = false;
                resonance.cancelLearn();
            }
        }
}

void AuricMasterProcessor::startAutoMatch()
{
    resonance.requestLearn (autoLearnSeconds);
}

void AuricMasterProcessor::timerCallback()
{
    if (resonance.consumeLearnResult())
    {
        learnedFreqs = resonance.getBinFrequencies();
        learnedPower = resonance.getLearnPower();
        learnedActivity = resonance.getLearnActivity();
        applyAutoMatch();
    }
    else if (hasAutoResult && (int) apvts.getRawParameterValue ("auto_profile")->load() != lastAutoProfile)
    {
        applyAutoMatch();   // profile switched: re-fit instantly from the learned data
    }
}

void AuricMasterProcessor::applyAutoMatch()
{
    if (learnedPower.empty()) return;

    lastAutoProfile = (int) apvts.getRawParameterValue ("auto_profile")->load();
    std::array<float, numClashBands> defaults;
    std::copy (std::begin (clashDefaultFreqs), std::end (clashDefaultFreqs), defaults.begin());
    autoResult = AutoMatcher::analyse (learnedFreqs, learnedPower, learnedActivity, lastAutoProfile, defaults, clashNames);

    auto set = [this] (const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    };
    for (int i = 0; i < numClashBands; ++i)
    {
        const auto n = "clash" + juce::String (i + 1);
        const auto& b = autoResult.bands[(size_t) i];
        set (n + "_freq", b.freq);
        set (n + "_amt", b.amount);
        set (n + "_width", b.width);
    }
    set ("res_depth", autoResult.depth);

    apvts.state.setProperty ("autoLearned", true, nullptr);
    hasAutoResult = true;
    autoResultTime = juce::Time::getMillisecondCounter();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AuricMasterProcessor();
}
