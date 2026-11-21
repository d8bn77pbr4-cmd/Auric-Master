#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // parameter ids, in the order of the P enum below
    const char* const ids[] = {
        "input", "output", "amount", "bypass", "compare",
        "tone_low", "tone_mid", "tone_high", "punch", "width",
        "lowcut",
        "eq1_freq", "eq1_gain", "eq1_q", "eq1_mode", "eq2_freq", "eq2_gain", "eq2_q", "eq2_mode",
        "eq3_freq", "eq3_gain", "eq3_q", "eq3_mode", "eq4_freq", "eq4_gain", "eq4_q", "eq4_mode",
        "xover_low", "xover_high", "mb_low", "mb_mid", "mb_high", "mb_speed", "mb_low_gain", "mb_mid_gain", "mb_high_gain",
        "glue", "glue_speed",
        "sat_low", "sat_high",
        "width_mid", "width_high", "mono_freq",
        "ceiling", "lim_mode", "clip", "target", "drive",
        "res_on", "res_depth", "res_sharp", "res_sens", "res_attack", "res_release",
        "clash1_freq", "clash1_amt", "clash1_width", "clash2_freq", "clash2_amt", "clash2_width",
        "clash3_freq", "clash3_amt", "clash3_width", "clash4_freq", "clash4_amt", "clash4_width",
        "clash5_freq", "clash5_amt", "clash5_width", "clash6_freq", "clash6_amt", "clash6_width",
        "assist_profile"
    };
    enum P
    {
        pInput, pOutput, pAmount, pBypass, pCompare,
        pToneLow, pToneMid, pToneHigh, pPunch, pWidth,
        pLowCut,
        pEq1, // 4 bands x (freq, gain, q, mode)
        pXoverLow = pEq1 + 16, pXoverHigh, pMbLow, pMbMid, pMbHigh, pMbSpeed, pMbLowGain, pMbMidGain, pMbHighGain,
        pGlue, pGlueSpeed,
        pSatLow, pSatHigh,
        pWidthMid, pWidthHigh, pMonoFreq,
        pCeiling, pLimMode, pClip, pTarget, pDrive,
        pResOn, pResDepth, pResSharp, pResSens, pResAttack, pResRelease,
        pClash1, // 6 x (freq, amt, width)
        pAssistProfile = pClash1 + 18,
        pCount
    };
    static_assert (sizeof (ids) / sizeof (ids[0]) == pCount, "parameter id list and enum must match");

    inline float coeff (double sr, double seconds) { return (float) std::exp (-1.0 / (seconds * sr)); }
}

AuricMaster2Processor::AuricMaster2Processor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto* id : ids) ptr.push_back (apvts.getRawParameterValue (id));
    startTimerHz (10);
}

juce::AudioProcessorValueTreeState::ParameterLayout AuricMaster2Processor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto dB = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; });
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });
    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto off = [] (float limit) { return AudioParameterFloatAttributes().withStringFromValueFunction ([limit] (float v, int) { return v <= limit ? String ("Off") : String (roundToInt (v)) + " Hz"; }); };
    auto ms = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ms"; });
    auto add = [&p] (auto param) { p.push_back (std::move (param)); };
    auto pctR = NormalisableRange<float> (0.0f, 100.0f, 0.1f);
    auto freqR = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };

    add (std::make_unique<AudioParameterFloat> (ParameterID { "input", 1 }, "Input", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, dB));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output Trim", NormalisableRange<float> (-12.0f, 0.0f, 0.1f), 0.0f, dB));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount", NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, pct));
    add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Original (Bypass)", false));
    add (std::make_unique<AudioParameterBool> (ParameterID { "compare", 1 }, "Level Match", false));

    add (std::make_unique<AudioParameterFloat> (ParameterID { "tone_low", 1 }, "Tone Low", NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dB));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "tone_mid", 1 }, "Tone Mid", NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dB));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "tone_high", 1 }, "Tone High", NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dB));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "punch", 1 }, "Punch", pctR, 50.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Width", NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, pct));

    add (std::make_unique<AudioParameterFloat> (ParameterID { "lowcut", 1 }, "Low Cut", freqR (20.0f, 150.0f, 50.0f), 20.0f, off (20.5f)));
    const float eqF[4] = { 80.0f, 300.0f, 3000.0f, 12000.0f };
    const char* eqName[4] = { "Low Shelf", "Low Mid", "High Mid", "High Shelf" };
    for (int b = 0; b < 4; ++b)
    {
        const auto n = String (b + 1), nm = String (eqName[b]);
        add (std::make_unique<AudioParameterFloat> (ParameterID { "eq" + n + "_freq", 1 }, nm + " Freq", freqR (20.0f, 20000.0f, 1000.0f), eqF[b], hz));
        add (std::make_unique<AudioParameterFloat> (ParameterID { "eq" + n + "_gain", 1 }, nm + " Gain", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dB));
        add (std::make_unique<AudioParameterFloat> (ParameterID { "eq" + n + "_q", 1 }, nm + " Q", freqR (0.3f, 6.0f, 1.0f), b == 0 || b == 3 ? 0.7f : 1.0f,
                                                    AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2); })));
        add (std::make_unique<AudioParameterChoice> (ParameterID { "eq" + n + "_mode", 1 }, nm + " Mode", StringArray { "Stereo", "Mid", "Side" }, 0));
    }

    add (std::make_unique<AudioParameterFloat> (ParameterID { "xover_low", 1 }, "Crossover Low", freqR (60.0f, 400.0f, 150.0f), 150.0f, hz));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "xover_high", 1 }, "Crossover High", freqR (1000.0f, 8000.0f, 3000.0f), 3000.0f, hz));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "mb_low", 1 }, "Dynamics Low", pctR, 30.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "mb_mid", 1 }, "Dynamics Mid", pctR, 25.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "mb_high", 1 }, "Dynamics High", pctR, 25.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "mb_speed", 1 }, "Dynamics Speed", pctR, 50.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "mb_low_gain", 1 }, "Band Low Gain", NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dB));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "mb_mid_gain", 1 }, "Band Mid Gain", NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dB));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "mb_high_gain", 1 }, "Band High Gain", NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dB));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "glue", 1 }, "Glue", pctR, 30.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "glue_speed", 1 }, "Glue Speed", pctR, 50.0f, pct));

    add (std::make_unique<AudioParameterFloat> (ParameterID { "sat_low", 1 }, "Warmth (Low)", pctR, 20.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "sat_high", 1 }, "Excite (High)", pctR, 15.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "width_mid", 1 }, "Width Mid", NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "width_high", 1 }, "Width High", NormalisableRange<float> (0.0f, 200.0f, 0.1f), 110.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "mono_freq", 1 }, "Mono Bass", freqR (39.0f, 250.0f, 100.0f), 100.0f, off (40.0f)));

    add (std::make_unique<AudioParameterFloat> (ParameterID { "ceiling", 1 }, "Ceiling", NormalisableRange<float> (-3.0f, -0.1f, 0.1f), -1.0f,
                                                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dBTP"; })));
    add (std::make_unique<AudioParameterChoice> (ParameterID { "lim_mode", 1 }, "Limiter Character", StringArray { "Transparent", "Punchy", "Loud" }, 1));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "clip", 1 }, "Clipper", pctR, 30.0f, pct));
    add (std::make_unique<AudioParameterChoice> (ParameterID { "target", 1 }, "Loudness Target", Targets::names(), 1));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "drive", 1 }, "Drive", NormalisableRange<float> (0.0f, 18.0f, 0.1f), 3.0f, dB));

    add (std::make_unique<AudioParameterBool> (ParameterID { "res_on", 1 }, "Resonance On", true));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "res_depth", 1 }, "Resonance Depth", pctR, 25.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "res_sharp", 1 }, "Resonance Sharpness", pctR, 50.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "res_sens", 1 }, "Resonance Selectivity", pctR, 50.0f, pct));
    NormalisableRange<float> attR (0.5f, 100.0f, 0.1f), relR (10.0f, 500.0f, 1.0f);
    attR.setSkewForCentre (8.0f); relR.setSkewForCentre (100.0f);
    add (std::make_unique<AudioParameterFloat> (ParameterID { "res_attack", 1 }, "Resonance Attack", attR, 5.0f, ms));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "res_release", 1 }, "Resonance Release", relR, 80.0f, ms));
    for (int i = 0; i < numClashBands; ++i)
    {
        const auto n = String (i + 1), nm = String ("Clash ") + clashNames[i];
        add (std::make_unique<AudioParameterFloat> (ParameterID { "clash" + n + "_freq", 1 }, nm + " Freq", freqR (20.0f, 20000.0f, 1000.0f), clashDefaultFreqs[i], hz));
        add (std::make_unique<AudioParameterFloat> (ParameterID { "clash" + n + "_amt", 1 }, nm + " Amount", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, pct));
        add (std::make_unique<AudioParameterFloat> (ParameterID { "clash" + n + "_width", 1 }, nm + " Width", NormalisableRange<float> (0.25f, 3.0f, 0.01f), 1.0f));
    }
    add (std::make_unique<AudioParameterChoice> (ParameterID { "assist_profile", 1 }, "Assistant Genre", AutoMatcher::profileNames(), 0));
    return { p.begin(), p.end() };
}

bool AuricMaster2Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()) && out == layouts.getMainInputChannelSet();
}

//==============================================================================
void AuricMaster2Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    numCh = juce::jlimit (1, 2, getMainBusNumOutputChannels());
    blockCap = juce::jmax (64, samplesPerBlock);

    inMeter.prepare (sr, numCh, false);
    outMeter.prepare (sr, numCh, true);
    resonance.prepare (sr, numCh);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numCh, osLog2,
                       juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) blockCap);
    const int osFactor = 1 << osLog2;
    int L = (int) std::ceil (0.002 * sr * osFactor);
    L = (L + osFactor - 1) / osFactor * osFactor;
    limiter.prepare (sr * osFactor, numCh, L);
    tpGuard.prepare (sr, numCh);

    const int latency = resonance.getLatency() + (int) std::round (oversampling->getLatencyInSamples()) + L / osFactor + tpGuard.getLatency();
    setLatencySamples (latency);
    dryDelay.prepare (numCh, latency + 8);
    dryDelay.setDelay (latency);
    dryBuffer.setSize (numCh, blockCap);

    for (auto* b : { &lowCut1, &lowCut2, &toneLowB, &toneMidB, &toneHighB, &apLow, &glueSc, &excPre, &excPost }) b->reset();
    for (auto& b : eqMid) b.reset();
    for (auto& b : eqSide) b.reset();
    split1.reset(); split2.reset(); monoSplit.reset();
    eqFreqLast.fill (-1.0f); eqQLast.fill (-1.0f); eqModeLast.fill (-1);
    eqGainS.fill (1000.0f);   // force first coefficient update
    toneS.fill (1000.0f);
    lowCutLast = x1Last = x2Last = monoLast = -1.0f;
    for (auto& d : band) d = Dyn();
    glue = Dyn();
    glueSc.setHighPass (sr, 100.0, 0.707);
    aLow.setLowPass (sr, 150.0, 0.707);
    aLow.reset();
    aHigh.setHighPass (sr, 300.0, 0.707);
    aHigh.reset();
    excPre.setLowPass (sr, juce::jmin (9000.0, sr * 0.4), 0.707);
    excEnv = 0.0f;
    wMidS = wHighS = 1.0f;

    inGainS.reset (sr, 0.03);   inGainS.setCurrentAndTargetValue (dbToGain (p (pInput)));
    outGainS.reset (sr, 0.03);  outGainS.setCurrentAndTargetValue (dbToGain (p (pOutput)));
    bypassMix.reset (sr, 0.03); bypassMix.setCurrentAndTargetValue (p (pBypass) > 0.5f ? 1.0f : 0.0f);
    matchProcDb = matchOrigDb = 0.0f;
    driveS = p (pTarget) > 0.5f ? autoDrive : p (pDrive);
    corrLR = corrLL = corrRR = 0.0f;
}

void AuricMaster2Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto main = getBusBuffer (buffer, false, 0);
    const int n = main.getNumSamples();
    const int chans = juce::jmin (numCh, main.getNumChannels());
    if (n == 0 || chans == 0) return;
    for (int start = 0; start < n; start += blockCap)   // hosts may send larger blocks than announced
    {
        juce::AudioBuffer<float> view (main.getArrayOfWritePointers(), chans, start, juce::jmin (blockCap, n - start));
        processChunk (view, chans);
    }
}

void AuricMaster2Processor::Dyn::compute (float amount, float knee, float levelC, float makeupC, float trimDb)
{
    const float envDb = powerToDb (env);
    trackLevel (envDb, levelC);
    float gr = 0.0f;
    if (amount > 0.001f)
    {
        // threshold follows the band's own running level: the same setting works for any mix level
        const float thr = levelDb + 6.0f - 12.0f * amount, ratio = 1.0f + 3.0f * amount, slope = 1.0f / ratio - 1.0f;
        const float over = envDb - thr;
        if (2.0f * over > knee) gr = over * slope;
        else if (2.0f * std::abs (over) <= knee) gr = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
    }
    target = dbToGain (gr);
    grDb = gr;
    if (envDb > -60.0f) makeupDb += (-gr - makeupDb) * makeupC;   // auto makeup: keeps the loudness where it was
    outGain = dbToGain (makeupDb + trimDb);
}

void AuricMaster2Processor::updateControl (int numSamples)
{
    // ---- loudness target automation ----
    const int target = (int) p (pTarget);
    const float dt = (float) numSamples / (float) sr;
    if (target != lastTarget) { autoJumped = false; holdoff = 0.0f; lastTarget = target; }
    float driveTarget = p (pDrive);
    if (target > 0)
    {
        const float t = Targets::lufs (target);
        // adapt only while the song is at its usual level (not in a quiet intro or a break)
        if (outMeter.hasRunning() && outMeter.getMomentary() > outMeter.getRunning() - 12.0f)
        {
            if (holdoff > 0.0f) holdoff -= dt;
            else
            {
                const float err = t - outMeter.getRunning();
                if (! autoJumped)
                {
                    // first estimate after 3 s of steady measurement: jump close to the target. The running
                    // measurement is moved by the same amount, so the fine-tuning does not chase its own lag.
                    if (outMeter.getMeasuredSeconds() > 3.0f)
                    {
                        const float jump = juce::jlimit (-12.0f, 12.0f, err);
                        autoDrive += jump;
                        outMeter.offsetRunning (jump);
                        autoJumped = true;
                        holdoff = 1.5f;
                    }
                }
                else
                    autoDrive += juce::jlimit (-6.0f, 6.0f, err) * dt / 4.0f;
            }
            autoDrive = juce::jlimit (-12.0f, 24.0f, autoDrive);
            targetReached.store (std::abs (t - outMeter.getRunning()) < 0.7f);
        }
        driveTarget = autoDrive;
    }
    else
    {
        autoDrive = driveTarget;
        targetReached.store (false);
    }
    driveDb.store (driveTarget);

    // ---- resonance control ----
    const float amountScale = p (pAmount) * 0.01f;
    ResonanceSuppressor::Settings rs;
    rs.enabled = p (pResOn) > 0.5f;
    rs.depth = juce::jlimit (0.0f, 1.5f, p (pResDepth) * 0.01f * amountScale);
    rs.sharpness = p (pResSharp) * 0.01f;
    rs.sensitivity = p (pResSens) * 0.01f;
    rs.attackMs = p (pResAttack);
    rs.releaseMs = p (pResRelease);
    for (size_t i = 0; i < (size_t) numClashBands; ++i)
        rs.nodes[i] = { p (pClash1 + (int) i * 3), p (pClash1 + (int) i * 3 + 1) * 0.01f, p (pClash1 + (int) i * 3 + 2), clashTypes[i] };
    resonance.setSettings (rs);

    // ---- dynamics timing ----
    const float speed = p (pMbSpeed) * 0.01f, gspeed = p (pGlueSpeed) * 0.01f;
    for (int b = 0; b < 3; ++b)
    {
        const float slow = b == 0 ? 1.6f : 1.0f;   // bass needs slower time constants
        band[(size_t) b].attC = coeff (sr, slow * juce::jmap (speed, 0.030f, 0.003f));
        band[(size_t) b].relC = coeff (sr, slow * juce::jmap (speed, 0.400f, 0.060f));
    }
    glue.attC = coeff (sr, juce::jmap (gspeed, 0.030f, 0.005f));
    glue.relC = coeff (sr, juce::jmap (gspeed, 0.500f, 0.100f));

    // ---- crossovers ----
    const float x1 = p (pXoverLow), x2 = juce::jmax (p (pXoverHigh), x1 * 2.0f);
    if (! juce::exactlyEqual (x1, x1Last)) { split1.set (sr, x1); x1Last = x1; }
    if (! juce::exactlyEqual (x2, x2Last))
    {
        split2.set (sr, x2);
        apLow.setAllPass (sr, x2, 0.70710678);   // keeps the low band in phase with mid + high
        excPost.setHighPass (sr, juce::jmax (2500.0f, x2), 0.707);
        x2Last = x2;
    }
    const float mono = p (pMonoFreq);
    if (mono > 40.0f && ! juce::exactlyEqual (mono, monoLast)) { monoSplit.set (sr, mono); monoLast = mono; }
    const float lc = p (pLowCut);
    if (! juce::exactlyEqual (lc, lowCutLast)) { lowCut1.setHighPass (sr, lc, 0.5412); lowCut2.setHighPass (sr, lc, 1.3066); lowCutLast = lc; }

    // ---- limiter ----
    limiter.setMode ((int) p (pLimMode));
}

void AuricMaster2Processor::processChunk (juce::AudioBuffer<float>& main, int chans)
{
    const int n = main.getNumSamples();
    updateControl (n);

    // input gain + input metering (for level-matched compare and the assistant)
    inGainS.setTargetValue (dbToGain (p (pInput)));
    for (int i = 0; i < n; ++i)
    {
        const float g = inGainS.getNextValue();
        for (int ch = 0; ch < chans; ++ch) main.getWritePointer (ch)[i] *= g;
    }
    inMeter.process (main, chans);

    // assistant: collect stereo / low-end statistics of the incoming mix while it listens
    if (assistCollect.load() && resonance.isLearning() && chans > 1)
    {
        const float* l = main.getReadPointer (0);
        const float* r = main.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            // correlation above ~300 Hz: that is the range the width controls act on (the bass is mono anyway)
            const float hl = aHigh.process (l[i], 0), hr = aHigh.process (r[i], 1);
            aLR += (double) hl * hr; aLL += (double) hl * hl; aRR += (double) hr * hr;
            const float lm = aLow.process (0.5f * (l[i] + r[i]), 0), ls = aLow.process (0.5f * (l[i] - r[i]), 1);
            aMlow += (double) lm * lm; aSlow += (double) ls * ls;
            aPeak = juce::jmax (aPeak, std::abs (l[i]), std::abs (r[i]));
        }
    }

    for (int ch = 0; ch < chans; ++ch) dryBuffer.copyFrom (ch, 0, main, ch, 0, n);
    juce::AudioBuffer<float> dry (dryBuffer.getArrayOfWritePointers(), chans, 0, n);
    dryDelay.process (dry, chans);

    // 1) resonance control (corrective, FFT stage)
    resonance.process (main, chans);

    // effective amounts (Amount scales every colouring stage; Punch and Width are macros on top)
    const float amountScale = p (pAmount) * 0.01f, punch = p (pPunch) / 50.0f;
    const float mbAmt[3] = { juce::jlimit (0.0f, 1.5f, p (pMbLow) * 0.01f * punch * amountScale),
                             juce::jlimit (0.0f, 1.5f, p (pMbMid) * 0.01f * punch * amountScale),
                             juce::jlimit (0.0f, 1.5f, p (pMbHigh) * 0.01f * punch * amountScale) };
    const float mbTrim[3] = { p (pMbLowGain), p (pMbMidGain), p (pMbHighGain) };
    const float glueAmt = juce::jlimit (0.0f, 1.5f, p (pGlue) * 0.01f * punch * amountScale);
    const float satLow = juce::jlimit (0.0f, 1.5f, p (pSatLow) * 0.01f * amountScale);
    const float excite = juce::jlimit (0.0f, 1.5f, p (pSatHigh) * 0.01f * amountScale) * 0.35f;
    const float widthMacro = p (pWidth) * 0.01f;
    const float wMidT = juce::jlimit (0.0f, 2.0f, 1.0f + (p (pWidthMid) * 0.01f * widthMacro - 1.0f) * amountScale);
    const float wHighT = juce::jlimit (0.0f, 2.0f, 1.0f + (p (pWidthHigh) * 0.01f * widthMacro - 1.0f) * amountScale);
    const bool monoOn = p (pMonoFreq) > 40.0f, lowCutOn = p (pLowCut) > 20.5f, stereo = chans > 1;

    const float levelC = 1.0f - coeff (sr / ctrl, 3.0), makeupC = 1.0f - coeff (sr / ctrl, 2.0);
    const float gC = 1.0f - coeff (sr, 0.001), smoothC = 1.0f - coeff (sr / ctrl, 0.02);
    const float excAtt = coeff (sr, 0.001), excRel = coeff (sr, 0.05);
    constexpr float tapeBias = 0.2f;
    const float tb = std::tanh (tapeBias);

    float* L = main.getWritePointer (0);
    float* R = stereo ? main.getWritePointer (1) : main.getWritePointer (0);
    float grMax[4] = { 0, 0, 0, 0 };

    for (int start = 0; start < n; start += ctrl)
    {
        const int len = juce::jmin (ctrl, n - start);

        // ---- control rate: EQ coefficients (gains glide), compressor gain computers ----
        for (int b = 0; b < 4; ++b)
        {
            const int base = pEq1 + b * 4;
            const float f = p (base), g = p (base + 1) * amountScale, q = p (base + 2);
            const int mode = (int) p (base + 3);
            auto& gs = eqGainS[(size_t) b];
            const bool modeChanged = mode != eqModeLast[(size_t) b];
            const float cur = (modeChanged || gs > 500.0f) ? g : gs + (g - gs) * smoothC;   // gain glides, mode switches at once
            const bool changed = modeChanged || std::abs (cur - gs) > 0.0005f
                                 || ! juce::exactlyEqual (f, eqFreqLast[(size_t) b]) || ! juce::exactlyEqual (q, eqQLast[(size_t) b]);
            gs = cur;
            eqModeLast[(size_t) b] = mode;
            if (changed)
            {
                const float gm = mode == 2 ? 0.0f : cur, gside = mode == 1 ? 0.0f : cur;
                if (b == 0)      { eqMid[0].setLowShelf (sr, f, q, gm);  eqSide[0].setLowShelf (sr, f, q, gside); }
                else if (b == 3) { eqMid[3].setHighShelf (sr, f, q, gm); eqSide[3].setHighShelf (sr, f, q, gside); }
                else             { eqMid[(size_t) b].setPeak (sr, f, q, gm); eqSide[(size_t) b].setPeak (sr, f, q, gside); }
                eqFreqLast[(size_t) b] = f;
                eqQLast[(size_t) b] = q;
            }
        }
        const float toneT[3] = { p (pToneLow) * amountScale, p (pToneMid) * amountScale, p (pToneHigh) * amountScale };
        for (int k = 0; k < 3; ++k)
        {
            auto& ts = toneS[(size_t) k];
            const float next = ts > 500.0f ? toneT[k] : ts + (toneT[k] - ts) * smoothC;
            if (std::abs (next - ts) > 0.0005f)
            {
                if (k == 0) toneLowB.setLowShelf (sr, 100.0, 0.7, next);
                if (k == 1) toneMidB.setPeak (sr, 1200.0, 0.7, next);
                if (k == 2) toneHighB.setHighShelf (sr, 10000.0, 0.7, next);
            }
            ts = next;
        }
        for (int b = 0; b < 3; ++b) band[(size_t) b].compute (mbAmt[b], 6.0f, levelC, makeupC, mbTrim[b]);
        {
            // glue: gentler threshold/ratio than the bands, wide knee
            const float envDb = powerToDb (glue.env);
            glue.trackLevel (envDb, levelC);
            float gr = 0.0f;
            if (glueAmt > 0.001f)
            {
                const float thr = glue.levelDb + 4.0f - 10.0f * glueAmt, slope = 1.0f / (1.3f + 1.7f * glueAmt) - 1.0f, knee = 8.0f;
                const float over = envDb - thr;
                if (2.0f * over > knee) gr = over * slope;
                else if (2.0f * std::abs (over) <= knee) gr = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
            }
            glue.target = dbToGain (gr);
            glue.grDb = gr;
            if (envDb > -60.0f) glue.makeupDb += (-gr - glue.makeupDb) * makeupC;
            glue.outGain = dbToGain (glue.makeupDb);
        }
        wMidS += (wMidT - wMidS) * smoothC;
        wHighS += (wHighT - wHighS) * smoothC;
        for (int b = 0; b < 3; ++b) grMax[b] = juce::jmin (grMax[b], band[(size_t) b].grDb);
        grMax[3] = juce::jmin (grMax[3], glue.grDb);

        // ---- audio rate ----
        for (int i = start; i < start + len; ++i)
        {
            float l = L[i], r = stereo ? R[i] : L[i];

            // tone + EQ in the mid/side domain
            float m = 0.5f * (l + r), s = 0.5f * (l - r);
            if (lowCutOn) { m = lowCut2.process (lowCut1.process (m, 0), 0); s = lowCut2.process (lowCut1.process (s, 1), 1); }
            for (int b = 0; b < 4; ++b) { m = eqMid[(size_t) b].process (m, 0); s = eqSide[(size_t) b].process (s, 1); }
            m = toneHighB.process (toneMidB.process (toneLowB.process (m, 0), 0), 0);
            s = toneHighB.process (toneMidB.process (toneLowB.process (s, 1), 1), 1);
            l = m + s; r = m - s;

            // three bands (LR4, phase-compensated)
            float lo[2], mi[2], hi[2], h1;
            const float in2[2] = { l, r };
            for (int ch = 0; ch < 2; ++ch)
            {
                split1.process (in2[ch], ch, lo[ch], h1);
                lo[ch] = apLow.process (lo[ch], ch);
                split2.process (h1, ch, mi[ch], hi[ch]);
            }
            band[0].detect (juce::jmax (lo[0] * lo[0], lo[1] * lo[1]));
            band[1].detect (juce::jmax (mi[0] * mi[0], mi[1] * mi[1]));
            band[2].detect (juce::jmax (hi[0] * hi[0], hi[1] * hi[1]));
            for (auto& d : band) d.gain += (d.target - d.gain) * gC;
            const float g0 = band[0].gain * band[0].outGain, g1 = band[1].gain * band[1].outGain, g2 = band[2].gain * band[2].outGain;

            // exciter envelope on the (pre-filtered) high band
            float e[2];
            for (int ch = 0; ch < 2; ++ch) e[ch] = excPre.process (hi[ch], ch);
            const float ea = juce::jmax (std::abs (e[0]), std::abs (e[1]));
            excEnv = ea > excEnv ? ea + (excEnv - ea) * excAtt : ea + (excEnv - ea) * excRel;

            for (int ch = 0; ch < 2; ++ch)
            {
                lo[ch] *= g0; mi[ch] *= g1; hi[ch] *= g2;
                if (satLow > 0.0f)   // tape-style warmth on the bass band (even + odd harmonics)
                {
                    const float k = 1.0f + 3.0f * satLow;
                    const float y = (std::tanh (k * lo[ch] + tapeBias) - tb) / (k * (1.0f - tb * tb));
                    lo[ch] += juce::jmin (1.0f, satLow) * (y - lo[ch]);
                }
                if (excite > 0.0f)   // exciter: level-proportional 2nd/3rd harmonics of the 3-9 kHz content
                {
                    const float en = e[ch] / (excEnv + 1.0e-4f);
                    const float gen = (0.7f * en * en + 0.3f * en * en * en) * excEnv;
                    hi[ch] += excite * excPost.process (gen, ch) * g2;
                }
            }
            // width per band (mid/side), bass stays as it is
            const float mm = 0.5f * (mi[0] + mi[1]), ms = 0.5f * (mi[0] - mi[1]) * wMidS;
            const float hm = 0.5f * (hi[0] + hi[1]), hs = 0.5f * (hi[0] - hi[1]) * wHighS;
            l = lo[0] + mm + ms + hm + hs;
            r = lo[1] + mm - ms + hm - hs;

            // mono bass: remove side content below the mono frequency
            if (monoOn && stereo)
            {
                const float mm2 = 0.5f * (l + r), ss = monoSplit.highOnly (0.5f * (l - r), 0);
                l = mm2 + ss; r = mm2 - ss;
            }

            // glue compressor (side-chain high-passed, so the kick doesn't pump it)
            const float scL = glueSc.process (l, 0), scR = glueSc.process (r, 1);
            glue.detect (juce::jmax (scL * scL, scR * scR));
            glue.gain += (glue.target - glue.gain) * gC;
            const float gg = glue.gain * glue.outGain;
            L[i] = l * gg;
            if (stereo) R[i] = r * gg;
        }
    }
    grLow.store (grMax[0]); grMid.store (grMax[1]); grHigh.store (grMax[2]); grGlue.store (grMax[3]);

    // ---- loudness stage: drive -> (4x oversampled) clipper -> true-peak limiter ----
    const float driveTarget = driveDb.load();
    const float driveC = 1.0f - coeff (sr, 0.05);
    for (int i = 0; i < n; ++i)
    {
        driveS += (driveTarget - driveS) * driveC;
        const float g = dbToGain (driveS);
        for (int ch = 0; ch < chans; ++ch) main.getWritePointer (ch)[i] *= g;
    }
    const float ceilingLin = dbToGain (p (pCeiling));
    const float limCeiling = ceilingLin * dbToGain (-0.3f);   // headroom for the down-sampling filter
    const float clipAmt = p (pClip) * 0.01f;
    clipper.setThreshold (limCeiling * dbToGain (4.0f * (1.0f - clipAmt)), clipAmt > 0.01f);
    limiter.setCeiling (limCeiling);
    {
        juce::dsp::AudioBlock<float> block (main.getArrayOfWritePointers(), (size_t) chans, (size_t) n);
        auto os = oversampling->processSamplesUp (block);
        const int osN = (int) os.getNumSamples();
        float clipIn = 0.0f, clipOut = 0.0f;
        if (clipper.on)
            for (int ch = 0; ch < chans; ++ch)
            {
                float* d = os.getChannelPointer ((size_t) ch);
                for (int i = 0; i < osN; ++i)
                {
                    clipIn = juce::jmax (clipIn, std::abs (d[i]));
                    d[i] = clipper.process (d[i]);
                    clipOut = juce::jmax (clipOut, std::abs (d[i]));
                }
            }
        grClip.store (clipIn > clipOut && clipOut > 0.0f ? 20.0f * std::log10 (clipOut / clipIn) : 0.0f);
        float* chPtr[2] = { os.getChannelPointer (0), chans > 1 ? os.getChannelPointer (1) : nullptr };
        limiter.process (chPtr, chans, osN);
        grLimit.store (limiter.getGainReductionDb());
        if (grLimit.load() < grLimitPeak.load()) grLimitPeak.store (grLimit.load());
        if (grClip.load() < grClipPeak.load()) grClipPeak.store (grClip.load());
        oversampling->processSamplesDown (block);
    }
    tpGuard.process (main.getArrayOfWritePointers(), chans, n, ceilingLin * dbToGain (-0.05f));

    // output trim (never above 0 dB, so the ceiling holds), metering
    outGainS.setTargetValue (dbToGain (p (pOutput)));
    for (int i = 0; i < n; ++i)
    {
        const float g = outGainS.getNextValue();
        for (int ch = 0; ch < chans; ++ch)
        {
            float* d = main.getWritePointer (ch);
            d[i] = juce::jlimit (-ceilingLin, ceilingLin, d[i] * g);
        }
    }
    outMeter.process (main, chans);

    // stereo correlation (+1 mono, 0 wide, -1 out of phase)
    if (stereo)
    {
        const float c = coeff (sr, 0.3);
        const float* l = main.getReadPointer (0);
        const float* r = main.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            corrLR = l[i] * r[i] + (corrLR - l[i] * r[i]) * c;
            corrLL = l[i] * l[i] + (corrLL - l[i] * l[i]) * c;
            corrRR = r[i] * r[i] + (corrRR - r[i] * r[i]) * c;
        }
        const float den = std::sqrt (corrLL * corrRR);
        correlation.store (den > 1.0e-9f ? juce::jlimit (-1.0f, 1.0f, corrLR / den) : 1.0f);

        int sp = scopePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; i += 2)
        {
            scope[(size_t) (2 * sp)].store (l[i], std::memory_order_relaxed);
            scope[(size_t) (2 * sp + 1)].store (r[i], std::memory_order_relaxed);
            if (++sp >= scopeSize) sp = 0;
        }
        scopePos.store (sp, std::memory_order_release);
    }

    // ORIGINAL (bypass) and LEVEL MATCH. With level match on, the louder version is turned DOWN to the
    // loudness of the quieter one, so neither is ever boosted into clipping and the comparison is fair.
    float diff = 0.0f;
    if (inMeter.hasRunning() && outMeter.hasRunning())
        diff = juce::jlimit (-30.0f, 30.0f, outMeter.getRunning() - inMeter.getRunning());
    compareGainDb.store (diff);
    const bool match = p (pCompare) > 0.5f;
    const float procT = match ? juce::jmin (0.0f, -diff) : 0.0f, origT = match ? juce::jmin (0.0f, diff) : 0.0f;
    bypassMix.setTargetValue (p (pBypass) > 0.5f ? 1.0f : 0.0f);
    const float mc = 1.0f - coeff (sr, 0.05);
    for (int i = 0; i < n; ++i)
    {
        matchProcDb += (procT - matchProcDb) * mc;
        matchOrigDb += (origT - matchOrigDb) * mc;
        const float mix = bypassMix.getNextValue();
        const float gp = dbToGain (matchProcDb) * (1.0f - mix), go = dbToGain (matchOrigDb) * mix;
        for (int ch = 0; ch < chans; ++ch)
        {
            float* d = main.getWritePointer (ch);
            d[i] = d[i] * gp + dry.getSample (ch, i) * go;
        }
    }
}

//==============================================================================
void AuricMaster2Processor::startAssistant()
{
    aLR = aLL = aRR = aMlow = aSlow = 0.0;
    aPeak = 0.0f;
    inMeter.requestReset();
    assistCollect.store (true);
    resonance.requestLearn (assistSeconds);
}

void AuricMaster2Processor::timerCallback()
{
    if (resonance.consumeLearnResult())
    {
        assistCollect.store (false);
        learnedFreqs = resonance.getBinFrequencies();
        learnedPower = resonance.getLearnPower();
        learnedActivity = resonance.getLearnActivity();
        applyAssistant();
    }
}

void AuricMaster2Processor::undoAssistant()
{
    if (! preAssistState.isValid()) return;
    apvts.replaceState (preAssistState.createCopy());
    preAssistState = {};
    hasAutoResult = false;
    assistSummary = {};
}

void AuricMaster2Processor::applyAssistant()
{
    if (learnedPower.empty()) return;
    preAssistState = apvts.copyState();
    using juce::String;
    const int profile = (int) p (pAssistProfile);
    std::array<float, numClashBands> defaults;
    std::copy (std::begin (clashDefaultFreqs), std::end (clashDefaultFreqs), defaults.begin());
    autoResult = AutoMatcher::analyse (learnedFreqs, learnedPower, learnedActivity, profile, defaults, clashNames);
    // mastering: resonance bands stay subtle (the broad balance is handled by the EQ below)
    {
        juce::StringArray found;
        for (int i = 0; i < numClashBands; ++i)
        {
            auto& b = autoResult.bands[(size_t) i];
            b.amount = juce::jlimit (-60.0f, 80.0f, b.amount);
            if (std::abs (b.amount) >= 10.0f)
                found.add (String (clashNames[i]) + " " + (b.freq >= 1000.0f ? String (b.freq / 1000.0f, 1) + " kHz" : String (juce::roundToInt (b.freq)) + " Hz")
                           + " " + (b.amount > 0 ? "+" : "") + String (juce::roundToInt (b.amount)) + "%");
        }
        autoResult.depth = juce::jlimit (20.0f, 50.0f, autoResult.depth);
        autoResult.summary = found.isEmpty() ? String ("keine deutlichen Resonanzen gefunden") : found.joinIntoString (String::fromUTF8 ("  \xc2\xb7  "));
    }

    // broad tonal balance vs. the genre reference (1/3 octave, aligned on 100 Hz..10 kHz, ~2/3 octave smoothing)
    constexpr int nb = 31;
    std::array<float, nb> fc {}, dev {};
    const double binHz = learnedFreqs.size() > 1 ? (double) learnedFreqs[1] : 1.0;
    const auto targetPow = AutoMatcher::targetBinPower (learnedFreqs, profile);   // same bins, same window
    for (int i = 0; i < nb; ++i)
    {
        fc[(size_t) i] = 20.0f * std::pow (2.0f, (float) i / 3.0f);
        const int lo = juce::jlimit (1, (int) learnedPower.size() - 1, (int) std::round (fc[(size_t) i] * 0.891f / binHz));
        const int hi = juce::jlimit (lo, (int) learnedPower.size() - 1, (int) std::round (fc[(size_t) i] * 1.122f / binHz));
        double s = 0.0, t = 0.0;
        for (int b = lo; b <= hi; ++b) { s += learnedPower[(size_t) b]; t += targetPow[(size_t) b]; }
        dev[(size_t) i] = (float) (10.0 * std::log10 ((s + 1.0e-20) / (t + 1.0e-20)));
    }
    std::array<float, nb> core {};
    int nc = 0;
    for (int i = 0; i < nb; ++i) if (fc[(size_t) i] >= 100.0f && fc[(size_t) i] <= 10000.0f) core[(size_t) nc++] = dev[(size_t) i];
    std::sort (core.begin(), core.begin() + nc);
    const float offset = core[(size_t) nc / 2];
    for (auto& d : dev) d -= offset;
    auto avgDev = [&] (float f1, float f2)
    {
        float s = 0.0f; int k = 0;
        for (int i = 0; i < nb; ++i) if (fc[(size_t) i] >= f1 && fc[(size_t) i] <= f2) { s += dev[(size_t) i]; ++k; }
        return k > 0 ? s / (float) k : 0.0f;
    };

    auto set = [this] (const char* id, float value)
    {
        if (auto* prm = apvts.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (value));
            prm->endChangeGesture();
        }
    };
    auto round1 = [] (float v) { return std::round (v * 10.0f) / 10.0f; };

    // EQ: correct about 60 % of the broad deviation, at most +-4 dB (taste stays with the mix)
    const float eqLow = round1 (juce::jlimit (-4.0f, 4.0f, -0.6f * avgDev (40.0f, 120.0f)));
    const float eqLowMid = round1 (juce::jlimit (-4.0f, 4.0f, -0.6f * avgDev (200.0f, 500.0f)));
    const float eqHighMid = round1 (juce::jlimit (-4.0f, 4.0f, -0.6f * avgDev (2000.0f, 5000.0f)));
    const float eqHigh = round1 (juce::jlimit (-4.0f, 4.0f, -0.6f * avgDev (9000.0f, 16000.0f)));
    // bands at the positions that were measured: shelf 80 Hz, bells 300 Hz / 3 kHz, shelf 12 kHz
    set ("eq1_freq", 80.0f);    set ("eq1_q", 0.7f); set ("eq1_gain", eqLow);
    set ("eq2_freq", 300.0f);   set ("eq2_q", 1.0f); set ("eq2_gain", eqLowMid);
    set ("eq3_freq", 3000.0f);  set ("eq3_q", 1.0f); set ("eq3_gain", eqHighMid);
    set ("eq4_freq", 12000.0f); set ("eq4_q", 0.7f); set ("eq4_gain", eqHigh);
    for (auto* id : { "eq1_mode", "eq2_mode", "eq3_mode", "eq4_mode" }) set (id, 0.0f);

    // dynamics from the peak-to-loudness ratio: an open mix gets more, an already squashed one little
    const float integratedIn = inMeter.getIntegrated();
    const float plr = juce::jlimit (0.0f, 30.0f, 20.0f * std::log10 (aPeak + 1.0e-9f) - integratedIn);
    const float base = juce::jlimit (0.1f, 0.8f, (plr - 8.0f) / 12.0f);
    set ("mb_low", std::round (60.0f * base)); set ("mb_mid", std::round (45.0f * base)); set ("mb_high", std::round (45.0f * base));
    set ("glue", std::round (50.0f * base + (inMeter.getLra() > 10.0f ? 10.0f : 0.0f)));
    set ("punch", 50.0f);

    // width from the correlation (mono safety first), mono bass from the low-end side content
    const double den = std::sqrt (aLL * aRR);
    const float corr = den > 1.0e-12 ? (float) (aLR / den) : 1.0f;
    float wMid = 100.0f, wHigh = 110.0f;
    if (corr < 0.3f)       { wMid = 85.0f;  wHigh = 95.0f; }
    else if (corr > 0.85f) { wMid = 110.0f; wHigh = 125.0f; }
    set ("width_mid", wMid); set ("width_high", wHigh); set ("width", 100.0f);
    const float sideLow = (float) (aSlow / (aMlow + 1.0e-12));
    const float mono = sideLow > 0.1f ? 150.0f : 100.0f;
    set ("mono_freq", mono);

    // colour and limiter character
    set ("sat_low", plr > 12.0f ? 25.0f : 10.0f);
    set ("sat_high", avgDev (8000.0f, 14000.0f) < -2.0f ? 30.0f : 15.0f);
    set ("lim_mode", plr > 14.0f ? 1.0f : 0.0f);
    if ((int) p (pTarget) == 0) set ("target", 1.0f);

    // resonances (clash bands + depth from the spectral analysis)
    for (int i = 0; i < numClashBands; ++i)
    {
        const auto n = "clash" + String (i + 1);
        const auto& b = autoResult.bands[(size_t) i];
        set ((n + "_freq").toRawUTF8(), b.freq);
        set ((n + "_amt").toRawUTF8(), b.amount);
        set ((n + "_width").toRawUTF8(), b.width);
    }
    set ("res_depth", autoResult.depth);
    set ("res_on", 1.0f);

    auto sgn = [] (float v) { return (v > 0.04f ? "+" : "") + String (v, 1); };
    const auto dot = String::fromUTF8 ("  \xc2\xb7  ");
    assistSummary = "EQ " + sgn (eqLow) + " / " + sgn (eqLowMid) + " / " + sgn (eqHighMid) + " / " + sgn (eqHigh) + " dB" + dot
                  + "Dynamik " + String (juce::roundToInt (100.0f * base)) + " %" + dot
                  + "Breite " + String (juce::roundToInt (wMid)) + "/" + String (juce::roundToInt (wHigh)) + " %" + dot
                  + "Mono-Bass " + String (juce::roundToInt (mono)) + " Hz" + dot
                  + "PLR " + String (plr, 1) + " dB, Korrelation " + String (corr, 2);
    hasAutoResult = true;
    autoResultTime = juce::Time::getMillisecondCounter();
    ++assistRuns;
}

void AuricMaster2Processor::selectSnapshot (int slot)
{
    slot = juce::jlimit (0, 3, slot);
    if (slot == activeSnapshot) return;
    snapshots[(size_t) activeSnapshot] = apvts.copyState();
    if (snapshots[(size_t) slot].isValid())
        apvts.replaceState (snapshots[(size_t) slot].createCopy());
    activeSnapshot = slot;
}

juce::AudioProcessorEditor* AuricMaster2Processor::createEditor()
{
    return new AuricMaster2Editor (*this);
}

void AuricMaster2Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::ValueTree snaps ("Snapshots");
    snaps.setProperty ("active", activeSnapshot, nullptr);
    for (int i = 0; i < 4; ++i)
    {
        juce::ValueTree slot ("Slot");
        slot.setProperty ("index", i, nullptr);
        if (snapshots[(size_t) i].isValid() && i != activeSnapshot) slot.appendChild (snapshots[(size_t) i].createCopy(), nullptr);
        snaps.appendChild (slot, nullptr);
    }
    state.appendChild (snaps, nullptr);
    state.setProperty ("uiWidth", uiWidth.load(), nullptr);
    state.setProperty ("uiPage", uiPage.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AuricMaster2Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    uiWidth.store ((int) state.getProperty ("uiWidth", 0));
    uiPage.store ((int) state.getProperty ("uiPage", 0));
    state.removeProperty ("uiWidth", nullptr);
    state.removeProperty ("uiPage", nullptr);
    auto snaps = state.getChildWithName ("Snapshots");
    for (auto& s : snapshots) s = {};
    if (snaps.isValid())
    {
        activeSnapshot = juce::jlimit (0, 3, (int) snaps.getProperty ("active", 0));
        for (auto slot : snaps)
            if (slot.getNumChildren() > 0)
                snapshots[(size_t) juce::jlimit (0, 3, (int) slot.getProperty ("index", 0))] = slot.getChild (0).createCopy();
        state.removeChild (snaps, nullptr);
    }
    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AuricMaster2Processor();
}
