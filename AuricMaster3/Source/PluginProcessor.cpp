#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // parameter ids, in the order of the P enum below
    const char* const ids[] = {
        "input", "output", "mix", "bypass", "compare",
        "res_on", "eq_on", "dyn_on", "sat_on", "st_on", "glue_on", "lim_on",
        "res_depth", "res_sharp", "res_sens", "res_attack", "res_release",
        "clash1_freq", "clash1_amt", "clash1_width", "clash2_freq", "clash2_amt", "clash2_width",
        "clash3_freq", "clash3_amt", "clash3_width", "clash4_freq", "clash4_amt", "clash4_width",
        "clash5_freq", "clash5_amt", "clash5_width", "clash6_freq", "clash6_amt", "clash6_width",
        "lowcut", "highcut",
        "eq1_freq", "eq1_gain", "eq1_q", "eq1_mode", "eq2_freq", "eq2_gain", "eq2_q", "eq2_mode",
        "eq3_freq", "eq3_gain", "eq3_q", "eq3_mode", "eq4_freq", "eq4_gain", "eq4_q", "eq4_mode",
        "tone_low", "tone_mid", "tone_high",
        "xover_low", "xover_high",
        "mb_low_thr", "mb_low_ratio", "mb_low_att", "mb_low_rel", "mb_low_gain", "mb_low_solo", "mb_low_byp",
        "mb_mid_thr", "mb_mid_ratio", "mb_mid_att", "mb_mid_rel", "mb_mid_gain", "mb_mid_solo", "mb_mid_byp",
        "mb_high_thr", "mb_high_ratio", "mb_high_att", "mb_high_rel", "mb_high_gain", "mb_high_solo", "mb_high_byp",
        "sat_low", "sat_high",
        "width_mid", "width_high", "mono_freq",
        "glue_thr", "glue_ratio", "glue_att", "glue_rel", "glue_auto", "glue_makeup", "glue_hpf", "glue_mix",
        "lim_drive", "ceiling", "lim_mode", "clip", "target",
        "assist_profile"
    };
    enum P
    {
        pInput, pOutput, pMix, pBypass, pCompare,
        pResOn, pEqOn, pDynOn, pSatOn, pStOn, pGlueOn, pLimOn,
        pResDepth, pResSharp, pResSens, pResAttack, pResRelease,
        pClash1,                       // 6 x (freq, amt, width)
        pLowCut = pClash1 + 18, pHighCut,
        pEq1,                          // 4 x (freq, gain, q, mode)
        pToneLow = pEq1 + 16, pToneMid, pToneHigh,
        pXoverLow, pXoverHigh,
        pMb,                           // 3 x (thr, ratio, att, rel, gain, solo, byp)
        pSatLow = pMb + 21, pSatHigh,
        pWidthMid, pWidthHigh, pMonoFreq,
        pGlueThr, pGlueRatio, pGlueAtt, pGlueRel, pGlueAuto, pGlueMakeup, pGlueHpf, pGlueMix,
        pLimDrive, pCeiling, pLimMode, pClip, pTarget,
        pAssistProfile,
        pCount
    };
    static_assert (sizeof (ids) / sizeof (ids[0]) == pCount, "parameter id list and enum must match");
    constexpr int mbStride = 7;

    inline float coeff (double sr, double seconds) { return (float) std::exp (-1.0 / (seconds * sr)); }
    constexpr float glueHpfFreqs[] = { 0.0f, 60.0f, 90.0f, 150.0f };
}

AuricMaster3Processor::AuricMaster3Processor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (auto* id : ids) ptr.push_back (apvts.getRawParameterValue (id));
    startTimerHz (20);
}

juce::AudioProcessorValueTreeState::ParameterLayout AuricMaster3Processor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    auto dB = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " dB"; });
    auto dBfs = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; });
    auto hz = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; });
    auto offBelow = [] (float limit) { return AudioParameterFloatAttributes().withStringFromValueFunction ([limit] (float v, int) { return v <= limit ? String ("Off") : String (roundToInt (v)) + " Hz"; }); };
    auto offAbove = [] (float limit) { return AudioParameterFloatAttributes().withStringFromValueFunction ([limit] (float v, int)
        { return v >= limit ? String ("Off") : (v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz"); }); };
    auto ms = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ms"; });
    auto ratioTxt = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + ":1"; });
    auto twoDec = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2); });
    auto add = [&p] (auto param) { p.push_back (std::move (param)); };
    auto pctR = NormalisableRange<float> (0.0f, 100.0f, 0.1f);
    auto skewR = [] (float lo, float hi, float centre, float step = 0.0f) { NormalisableRange<float> r (lo, hi, step); r.setSkewForCentre (centre); return r; };
    auto boolP = [&add] (const char* id, const char* name, bool def) { add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    // global
    add (std::make_unique<AudioParameterFloat> (ParameterID { "input", 1 }, "Input", NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f, dB));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, dB));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix", pctR, 100.0f, pct));
    boolP ("bypass", "Bypass (Original)", false);
    boolP ("compare", "Level Match (monitoring)", false);

    // module switches: everything that colours the sound starts off, so inserting the plugin changes nothing
    boolP ("res_on", "Resonance On", false);
    boolP ("eq_on", "EQ On", true);
    boolP ("dyn_on", "Multiband On", false);
    boolP ("sat_on", "Colour On", false);
    boolP ("st_on", "Stereo On", true);
    boolP ("glue_on", "Glue On", false);
    boolP ("lim_on", "Limiter On", false);

    // resonance
    add (std::make_unique<AudioParameterFloat> (ParameterID { "res_depth", 1 }, "Resonance Depth", pctR, 25.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "res_sharp", 1 }, "Resonance Sharpness", pctR, 50.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "res_sens", 1 }, "Resonance Selectivity", pctR, 50.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "res_attack", 1 }, "Resonance Attack", skewR (0.5f, 100.0f, 8.0f, 0.1f), 5.0f, ms));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "res_release", 1 }, "Resonance Release", skewR (10.0f, 500.0f, 100.0f, 1.0f), 80.0f, ms));
    for (int i = 0; i < numClashBands; ++i)
    {
        const auto n = String (i + 1), nm = String ("Resonance ") + clashNames[i];
        add (std::make_unique<AudioParameterFloat> (ParameterID { "clash" + n + "_freq", 1 }, nm + " Freq", skewR (20.0f, 20000.0f, 1000.0f), clashDefaultFreqs[i], hz));
        add (std::make_unique<AudioParameterFloat> (ParameterID { "clash" + n + "_amt", 1 }, nm + " Amount", NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, pct));
        add (std::make_unique<AudioParameterFloat> (ParameterID { "clash" + n + "_width", 1 }, nm + " Width", NormalisableRange<float> (0.25f, 3.0f, 0.01f), 1.0f, twoDec));
    }

    // EQ
    add (std::make_unique<AudioParameterFloat> (ParameterID { "lowcut", 1 }, "Low Cut", skewR (20.0f, 300.0f, 60.0f), 20.0f, offBelow (20.5f)));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "highcut", 1 }, "High Cut", skewR (2000.0f, 20000.0f, 8000.0f), 20000.0f, offAbove (19990.0f)));
    const float eqF[4] = { 80.0f, 300.0f, 3000.0f, 12000.0f };
    const char* eqName[4] = { "EQ Low Shelf", "EQ Bell 2", "EQ Bell 3", "EQ High Shelf" };
    for (int b = 0; b < 4; ++b)
    {
        const auto n = String (b + 1);
        const String nm (eqName[b]);
        add (std::make_unique<AudioParameterFloat> (ParameterID { "eq" + n + "_freq", 1 }, nm + " Freq", skewR (20.0f, 20000.0f, 1000.0f), eqF[b], hz));
        add (std::make_unique<AudioParameterFloat> (ParameterID { "eq" + n + "_gain", 1 }, nm + " Gain", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dB));
        add (std::make_unique<AudioParameterFloat> (ParameterID { "eq" + n + "_q", 1 }, nm + " Q", skewR (0.3f, 6.0f, 1.0f), b == 0 || b == 3 ? 0.7f : 1.0f, twoDec));
        add (std::make_unique<AudioParameterChoice> (ParameterID { "eq" + n + "_mode", 1 }, nm + " Mode", StringArray { "Stereo", "Mid", "Side" }, 0));
    }
    add (std::make_unique<AudioParameterFloat> (ParameterID { "tone_low", 1 }, "Tone Low", NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dB));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "tone_mid", 1 }, "Tone Mid", NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dB));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "tone_high", 1 }, "Tone High", NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f, dB));

    // multiband
    add (std::make_unique<AudioParameterFloat> (ParameterID { "xover_low", 1 }, "Crossover Low", skewR (60.0f, 400.0f, 150.0f), 150.0f, hz));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "xover_high", 1 }, "Crossover High", skewR (1000.0f, 8000.0f, 3000.0f), 3000.0f, hz));
    const float thrDef[3] = { -18.0f, -18.0f, -20.0f }, attDef[3] = { 30.0f, 15.0f, 5.0f }, relDef[3] = { 250.0f, 150.0f, 80.0f };
    const char* bandName[3] = { "Low", "Mid", "High" };
    for (int b = 0; b < 3; ++b)
    {
        const auto base = String ("mb_") + bandIds[b] + "_", nm = String ("Band ") + bandName[b];
        add (std::make_unique<AudioParameterFloat> (ParameterID { base + "thr", 1 }, nm + " Threshold", NormalisableRange<float> (-50.0f, 0.0f, 0.1f), thrDef[b], dBfs));
        add (std::make_unique<AudioParameterFloat> (ParameterID { base + "ratio", 1 }, nm + " Ratio", skewR (1.0f, 10.0f, 2.5f, 0.01f), 2.0f, ratioTxt));
        add (std::make_unique<AudioParameterFloat> (ParameterID { base + "att", 1 }, nm + " Attack", skewR (0.1f, 100.0f, 10.0f, 0.01f), attDef[b], ms));
        add (std::make_unique<AudioParameterFloat> (ParameterID { base + "rel", 1 }, nm + " Release", skewR (10.0f, 1000.0f, 150.0f, 1.0f), relDef[b], ms));
        add (std::make_unique<AudioParameterFloat> (ParameterID { base + "gain", 1 }, nm + " Gain", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, dB));
        add (std::make_unique<AudioParameterBool> (ParameterID { base + "solo", 1 }, nm + " Solo", false));
        add (std::make_unique<AudioParameterBool> (ParameterID { base + "byp", 1 }, nm + " Bypass", false));
    }

    // colour, stereo
    add (std::make_unique<AudioParameterFloat> (ParameterID { "sat_low", 1 }, "Warmth", pctR, 20.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "sat_high", 1 }, "Shine", pctR, 15.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "width_mid", 1 }, "Width Mid", NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "width_high", 1 }, "Width High", NormalisableRange<float> (0.0f, 200.0f, 0.1f), 100.0f, pct));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "mono_freq", 1 }, "Mono Bass", skewR (39.0f, 250.0f, 100.0f), 39.0f, offBelow (40.0f)));

    // glue (classic bus compressor)
    add (std::make_unique<AudioParameterFloat> (ParameterID { "glue_thr", 1 }, "Glue Threshold", NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -16.0f, dBfs));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "glue_ratio", 1 }, "Glue Ratio", skewR (1.0f, 10.0f, 2.5f, 0.01f), 2.0f, ratioTxt));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "glue_att", 1 }, "Glue Attack", skewR (0.1f, 30.0f, 5.0f, 0.01f), 10.0f, ms));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "glue_rel", 1 }, "Glue Release", skewR (20.0f, 1200.0f, 200.0f, 1.0f), 300.0f, ms));
    boolP ("glue_auto", "Glue Auto Release", true);
    add (std::make_unique<AudioParameterFloat> (ParameterID { "glue_makeup", 1 }, "Glue Makeup", NormalisableRange<float> (0.0f, 12.0f, 0.1f), 0.0f, dB));
    add (std::make_unique<AudioParameterChoice> (ParameterID { "glue_hpf", 1 }, "Glue Sidechain HPF", StringArray { "Off", "60 Hz", "90 Hz", "150 Hz" }, 2));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "glue_mix", 1 }, "Glue Mix", pctR, 100.0f, pct));

    // limiter
    add (std::make_unique<AudioParameterFloat> (ParameterID { "lim_drive", 1 }, "Limiter Drive", NormalisableRange<float> (-12.0f, 24.0f, 0.01f), 0.0f, dB));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "ceiling", 1 }, "Ceiling", NormalisableRange<float> (-3.0f, -0.1f, 0.1f), -1.0f,
                                                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dBTP"; })));
    add (std::make_unique<AudioParameterChoice> (ParameterID { "lim_mode", 1 }, "Limiter Character", StringArray { "Transparent", "Punchy", "Loud" }, 1));
    add (std::make_unique<AudioParameterFloat> (ParameterID { "clip", 1 }, "Clipper", pctR, 0.0f, pct));
    add (std::make_unique<AudioParameterChoice> (ParameterID { "target", 1 }, "Loudness Target", Targets::names(), 1));

    add (std::make_unique<AudioParameterChoice> (ParameterID { "assist_profile", 1 }, "Assistant Genre", AutoMatcher::profileNames(), 0));
    return { p.begin(), p.end() };
}

bool AuricMaster3Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono()) && out == layouts.getMainInputChannelSet();
}

void AuricMaster3Processor::setParam (const char* id, float value)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        prm->endChangeGesture();
    }
}

//==============================================================================
int AuricMaster3Processor::latencyFor (int config) const
{
    int l = 0;
    if (config & cfgRes) l += resonance.getLatency();
    if (config & cfgLim) l += (int) std::round (oversampling->getLatencyInSamples()) + lookahead / (1 << osLog2) + tpGuard.getLatency();
    return l;
}

void AuricMaster3Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    numCh = juce::jlimit (1, 2, getMainBusNumOutputChannels());
    blockCap = juce::jmax (64, samplesPerBlock);

    inMeter.prepare (sr, numCh, false);
    outMeter.prepare (sr, numCh, true);
    learnMeter.prepare (sr, numCh, false);
    resonance.prepare (sr, numCh);

    // oversampling for clipper + limiter, by sample rate: 4x up to 48 kHz, 2x up to 96 kHz, none above
    osLog2 = sr <= 50000.0 ? 2 : (sr <= 100000.0 ? 1 : 0);
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> ((size_t) numCh, (size_t) osLog2,
                       juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) blockCap);
    const int osFactor = 1 << osLog2;
    lookahead = (int) std::ceil (0.002 * sr * osFactor);
    lookahead = (lookahead + osFactor - 1) / osFactor * osFactor;
    limiter.prepare (sr * osFactor, numCh, lookahead);
    tpGuard.prepare (sr, numCh);

    const int maxLatency = latencyFor (cfgRes | cfgLim);
    origDelay.prepare (numCh, maxLatency + 8);
    mixDelay.prepare (numCh, maxLatency + 8);
    origBuffer.setSize (numCh, blockCap);
    mixBuffer.setSize (numCh, blockCap);

    for (auto* r : { &eqRamp, &dynRamp, &satRamp, &stRamp, &glueRamp, &bandRamp }) r->prepare (sr, 0.02f);
    updateControl (0);
    for (auto* r : { &eqRamp, &dynRamp, &satRamp, &stRamp, &glueRamp, &bandRamp }) r->snap();
    for (auto& c : bandComp) c.prepare (sr);
    glueComp.prepare (sr);
    // exciter pre-filters: the squared part only sees content up to ~8 kHz, the cubed part up to ~6 kHz,
    // so the generated harmonics stay below the Nyquist frequency (no aliasing at 44.1 kHz and up)
    excSq1.setLowPass (sr, 8000.0, 0.5412); excSq2.setLowPass (sr, 8000.0, 1.3066);
    excCu1.setLowPass (sr, 6000.0, 0.5412); excCu2.setLowPass (sr, 6000.0, 1.3066);
    aGlueHp.setHighPass (sr, 90.0, 0.707);   // the assistant measures the glue level as the glue detector sees it

    resetStates();

    inGainS.reset (sr, 0.03);   inGainS.setCurrentAndTargetValue (dbToGain (p (pInput)));
    outGainS.reset (sr, 0.03);  outGainS.setCurrentAndTargetValue (dbToGain (p (pOutput)));
    mixS.reset (sr, 0.03);      mixS.setCurrentAndTargetValue (p (pMix) * 0.01f);
    bypassMix.reset (sr, 0.03); bypassMix.setCurrentAndTargetValue (p (pBypass) > 0.5f ? 1.0f : 0.0f);
    matchProcDb = matchOrigDb = 0.0f;
    driveS = p (pLimDrive);

    configFadeStep = 1.0f / (float) (0.01 * sr);
    configSwitching = false;
    configFade = 1.0f;
    applyConfig ((p (pResOn) > 0.5f ? cfgRes : 0) | (p (pLimOn) > 0.5f ? cfgLim : 0));
    setLatencySamples (latencySamplesNow.load());
    latencyDirty.store (false);
}

void AuricMaster3Processor::resetStates()
{
    for (auto* b : { &lowCut1, &lowCut2, &highCut1, &highCut2, &toneLowB, &toneMidB, &toneHighB, &apLow, &glueSc,
                     &excSq1, &excSq2, &excCu1, &excCu2, &excPost, &aLow, &aHigh, &aGlueHp })
        b->reset();
    for (auto& b : eqMid) b.reset();
    for (auto& b : eqSide) b.reset();
    eqGainS.fill (1000.0f);        // > 500: jump to the target on the next control block
    eqModeLast.fill (-1);
    eqFreqLast.fill (-1.0f);
    eqQLast.fill (-1.0f);
    toneS.fill (1000.0f);
    lowCutLast = highCutLast = x1Last = x2Last = monoLast = -1.0f;
    glueHpfLast = -1;
    split1.reset(); split2.reset(); monoSplit.reset();
    for (auto& c : bandComp) c.reset();
    glueComp.reset();
    excEnv = 0.0f;
    wMidS = p (pWidthMid) * 0.01f;
    wHighS = p (pWidthHigh) * 0.01f;
    for (int b = 0; b < 3; ++b) { bandGainS[(size_t) b] = dbToGain (p (pMb + b * mbStride + 4)); soloS[(size_t) b] = 1.0f; }
    glueMakeupS = dbToGain (p (pGlueMakeup));
    glueMixS = p (pGlueMix) * 0.01f;
    resonance.reset();
    if (oversampling != nullptr) oversampling->reset();
    limiter.reset();
    tpGuard.reset();
    corrLR = corrLL = corrRR = 0.0f;
}

void AuricMaster3Processor::applyConfig (int config)
{
    // stages that are switched on start from a clean state; the output stays silent until they deliver audio
    configHold = 0;
    if ((config & cfgRes) && ! (activeConfig & cfgRes)) { resonance.reset(); configHold += resonance.getLatency(); }
    if ((config & cfgLim) && ! (activeConfig & cfgLim))
    {
        oversampling->reset(); limiter.reset(); tpGuard.reset();
        configHold += latencyFor (cfgLim) + 64;
    }
    activeConfig = config;
    origDelay.setDelay (latencyFor (config));
    mixDelay.setDelay ((config & cfgRes) ? resonance.getLatency() : 0);
    latencySamplesNow.store (latencyFor (config));
    latencyDirty.store (true);
    configSwitching = false;
}

void AuricMaster3Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto main = getBusBuffer (buffer, false, 0);
    const int n = main.getNumSamples();
    const int chans = juce::jmin (numCh, main.getNumChannels());
    for (int ch = chans; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, n);
    if (n == 0 || chans == 0) return;
    for (int start = 0; start < n; start += blockCap)   // hosts may send larger blocks than announced
    {
        juce::AudioBuffer<float> view (main.getArrayOfWritePointers(), chans, start, juce::jmin (blockCap, n - start));
        processChunk (view, chans);
    }
}

void AuricMaster3Processor::updateControl (int /*numSamples*/)
{
    eqRamp.setTarget (p (pEqOn) > 0.5f);
    dynRamp.setTarget (p (pDynOn) > 0.5f);
    satRamp.setTarget (p (pSatOn) > 0.5f);
    stRamp.setTarget (p (pStOn) > 0.5f);
    glueRamp.setTarget (p (pGlueOn) > 0.5f);
    const bool stActive = p (pStOn) > 0.5f && (std::abs (p (pWidthMid) - 100.0f) > 0.05f || std::abs (p (pWidthHigh) - 100.0f) > 0.05f || p (pMonoFreq) > 40.0f);
    bandRamp.setTarget (p (pDynOn) > 0.5f || p (pSatOn) > 0.5f || stActive);

    // resonance
    ResonanceSuppressor::Settings rs;
    rs.enabled = true;
    rs.depth = juce::jlimit (0.0f, 1.0f, p (pResDepth) * 0.01f);
    rs.sharpness = p (pResSharp) * 0.01f;
    rs.sensitivity = p (pResSens) * 0.01f;
    rs.attackMs = p (pResAttack);
    rs.releaseMs = p (pResRelease);
    for (size_t i = 0; i < (size_t) numClashBands; ++i)
        rs.nodes[i] = { p (pClash1 + (int) i * 3), p (pClash1 + (int) i * 3 + 1) * 0.01f, p (pClash1 + (int) i * 3 + 2), clashTypes[i] };
    resonance.setSettings (rs);

    // filters that change rarely
    const float x1 = p (pXoverLow), x2 = p (pXoverHigh);
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
    const float hc = p (pHighCut);
    if (! juce::exactlyEqual (hc, highCutLast))
    {
        const double f = juce::jmin ((double) hc, sr * 0.45);
        highCut1.setLowPass (sr, f, 0.5412); highCut2.setLowPass (sr, f, 1.3066); highCutLast = hc;
    }

    // compressors
    for (int b = 0; b < 3; ++b)
    {
        const int base = pMb + b * mbStride;
        bandComp[(size_t) b].setParams (p (base), p (base + 1), p (base + 2), p (base + 3), 6.0f, false);
    }
    glueComp.setParams (p (pGlueThr), p (pGlueRatio), p (pGlueAtt), p (pGlueRel), 6.0f, p (pGlueAuto) > 0.5f);
    const int hpf = (int) p (pGlueHpf);
    if (hpf != glueHpfLast && hpf > 0) glueSc.setHighPass (sr, glueHpfFreqs[juce::jlimit (0, 3, hpf)], 0.707);
    glueHpfLast = hpf;

    // limiter
    limiter.setMode ((int) p (pLimMode));
}

void AuricMaster3Processor::processChunk (juce::AudioBuffer<float>& main, int chans)
{
    const int n = main.getNumSamples();
    const bool stereo = chans > 1;

    // latency-affecting modules switch after a short fade-out (then the host is told the new latency)
    const int wanted = (p (pResOn) > 0.5f ? cfgRes : 0) | (p (pLimOn) > 0.5f ? cfgLim : 0);
    if (configSwitching && configFade <= 0.0f) applyConfig (wanted);
    else if (! configSwitching && wanted != activeConfig) configSwitching = true;

    updateControl (n);

    // the untouched input, delayed by the plugin latency (ORIGINAL / level match / host bypass)
    for (int ch = 0; ch < chans; ++ch) origBuffer.copyFrom (ch, 0, main, ch, 0, n);
    juce::AudioBuffer<float> orig (origBuffer.getArrayOfWritePointers(), chans, 0, n);
    origDelay.process (orig, chans);

    // input gain; broken values from earlier plugins (NaN / Inf) are replaced by silence
    inGainS.setTargetValue (dbToGain (p (pInput)));
    float inPk[2] = { 0.0f, 0.0f };
    for (int i = 0; i < n; ++i)
    {
        const float g = inGainS.getNextValue();
        for (int ch = 0; ch < chans; ++ch)
        {
            float* d = main.getWritePointer (ch);
            float v = d[i] * g;
            if (! std::isfinite (v)) v = 0.0f;
            d[i] = v;
            inPk[ch] = juce::jmax (inPk[ch], std::abs (v));
        }
    }
    for (int ch = 0; ch < chans; ++ch) if (inPk[ch] > inPeak[(size_t) ch].load()) inPeak[(size_t) ch].store (inPk[ch]);
    inMeter.process (main, chans);

    // assistant: stereo / low-end statistics of the incoming mix while it listens
    const bool collecting = assistCollect.load() && resonance.isLearning();
    if (collecting && stereo)
    {
        const float* l = main.getReadPointer (0);
        const float* r = main.getReadPointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float hl = aHigh.process (l[i], 0), hr = aHigh.process (r[i], 1);
            aLR += (double) hl * hr; aLL += (double) hl * hl; aRR += (double) hr * hr;
            const float lm = aLow.process (0.5f * (l[i] + r[i]), 0), ls = aLow.process (0.5f * (l[i] - r[i]), 1);
            aMlow += (double) lm * lm; aSlow += (double) ls * ls;
            aPeak = juce::jmax (aPeak, std::abs (l[i]), std::abs (r[i]));
        }
    }

    // dry signal for MIX (parallel processing), aligned with the signal after the resonance stage
    for (int ch = 0; ch < chans; ++ch) mixBuffer.copyFrom (ch, 0, main, ch, 0, n);
    juce::AudioBuffer<float> dry (mixBuffer.getArrayOfWritePointers(), chans, 0, n);
    mixDelay.process (dry, chans);

    // 1) RESONANZ (when off, only the analysis runs for the displays and the assistant)
    if (activeConfig & cfgRes) resonance.process (main, chans);
    else if (analyserWanted.load() || resonance.isLearning()) resonance.analyse (main, chans);

    // 2) EQ + KLANG, 3) crossover with DYNAMIK / FARBE / STEREO, 4) GLUE
    const float smoothC = 1.0f - coeff (sr / ctrl, 0.02);
    const float excAtt = coeff (sr, 0.001), excRel = coeff (sr, 0.05);
    constexpr float tapeBias = 0.2f;
    const float tb = std::tanh (tapeBias);
    const bool lowCutOn = p (pLowCut) > 20.5f, highCutOn = p (pHighCut) < 19990.0f, monoOn = p (pMonoFreq) > 40.0f;
    const float satLow = p (pSatLow) * 0.01f, excite = p (pSatHigh) * 0.01f * 0.35f;
    const bool glueHpfOn = (int) p (pGlueHpf) > 0;
    bool anySolo = false;
    std::array<bool, 3> bandSolo {}, bandByp {};
    for (int b = 0; b < 3; ++b)
    {
        bandSolo[(size_t) b] = p (pMb + b * mbStride + 5) > 0.5f;
        bandByp[(size_t) b] = p (pMb + b * mbStride + 6) > 0.5f;
        anySolo = anySolo || bandSolo[(size_t) b];
    }
    const bool dynOn = p (pDynOn) > 0.5f;
    // modules that were completely off start from a clean state
    if (dynRamp.value <= 0.0f && dynRamp.target > 0.0f) for (auto& c : bandComp) c.reset();
    if (glueRamp.value <= 0.0f && glueRamp.target > 0.0f) glueComp.reset();
    // the crossover runs while a band module needs it (or the assistant measures band levels)
    const bool runBands = bandRamp.active() || collecting;
    if (! runBands && bandRunning)
    {
        split1.reset(); split2.reset(); apLow.reset(); monoSplit.reset();
        for (auto& c : bandComp) c.reset();
        excSq1.reset(); excSq2.reset(); excCu1.reset(); excCu2.reset(); excPost.reset();
        excEnv = 0.0f;
    }
    bandRunning = runBands;

    float* L = main.getWritePointer (0);
    float* R = stereo ? main.getWritePointer (1) : main.getWritePointer (0);
    float grMin[4] = { 0, 0, 0, 0 };

    for (int start = 0; start < n; start += ctrl)
    {
        const int len = juce::jmin (ctrl, n - start);

        // ---- control rate: EQ coefficients (gains glide, mode switches at once), smoothed gains ----
        for (int b = 0; b < 4; ++b)
        {
            const int base = pEq1 + b * 4;
            const float f = p (base), g = p (base + 1), q = p (base + 2);
            const int mode = (int) p (base + 3);
            auto& gs = eqGainS[(size_t) b];
            const bool modeChanged = mode != eqModeLast[(size_t) b];
            const float cur = gs > 500.0f ? g : gs + (g - gs) * smoothC;
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
        const float toneT[3] = { p (pToneLow), p (pToneMid), p (pToneHigh) };
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
        for (int b = 0; b < 3; ++b)
        {
            const float gT = bandByp[(size_t) b] ? 1.0f : dbToGain (p (pMb + b * mbStride + 4));
            bandGainS[(size_t) b] += (gT - bandGainS[(size_t) b]) * smoothC;
            const float sT = (dynOn && anySolo && ! bandSolo[(size_t) b]) ? 0.0f : 1.0f;
            soloS[(size_t) b] += (sT - soloS[(size_t) b]) * smoothC;
        }
        wMidS += (p (pWidthMid) * 0.01f - wMidS) * smoothC;
        wHighS += (p (pWidthHigh) * 0.01f - wHighS) * smoothC;
        glueMakeupS += (dbToGain (p (pGlueMakeup)) - glueMakeupS) * smoothC;
        glueMixS += (p (pGlueMix) * 0.01f - glueMixS) * smoothC;
        double blockPow[4] = { 0, 0, 0, 0 };

        // ---- audio rate ----
        for (int i = start; i < start + len; ++i)
        {
            float l = L[i], r = stereo ? R[i] : L[i];

            // EQ + KLANG in the mid/side domain
            const float re = eqRamp.next();
            if (re > 0.0f)
            {
                float m = 0.5f * (l + r), s = 0.5f * (l - r);
                if (lowCutOn)  { m = lowCut2.process (lowCut1.process (m, 0), 0);   s = lowCut2.process (lowCut1.process (s, 1), 1); }
                if (highCutOn) { m = highCut2.process (highCut1.process (m, 0), 0); s = highCut2.process (highCut1.process (s, 1), 1); }
                for (int b = 0; b < 4; ++b) { m = eqMid[(size_t) b].process (m, 0); s = eqSide[(size_t) b].process (s, 1); }
                m = toneHighB.process (toneMidB.process (toneLowB.process (m, 0), 0), 0);
                s = toneHighB.process (toneMidB.process (toneLowB.process (s, 1), 1), 1);
                l += re * ((m + s) - l);
                r += re * ((m - s) - r);
            }

            const float rb = bandRamp.next();
            if (! runBands)
            {
                // GLUE only (no band module active)
                const float rg = glueRamp.next();
                if (collecting)
                {
                    const float hl = aGlueHp.process (l, 0), hr = aGlueHp.process (r, 1);
                    blockPow[3] += juce::jmax (hl * hl, hr * hr);
                }
                if (rg > 0.0f)
                {
                    const float scL = glueHpfOn ? glueSc.process (l, 0) : l, scR = glueHpfOn ? glueSc.process (r, 1) : r;
                    const float det = juce::jmax (scL * scL, scR * scR);
                    const float gg = glueComp.process (det) * glueMakeupS;
                    const float k = 1.0f + rg * glueMixS * (gg - 1.0f);
                    l *= k; r *= k;
                    grMin[3] = juce::jmin (grMin[3], glueComp.getGainReductionDb());
                }
                dynRamp.next(); satRamp.next(); stRamp.next();
                L[i] = l;
                if (stereo) R[i] = r;
                continue;
            }
            const float inL = l, inR = r;

            // three bands (LR4, phase-compensated; magnitude of the sum is flat)
            float lo[2], mi[2], hi[2], h1;
            const float in2[2] = { l, r };
            for (int ch = 0; ch < 2; ++ch)
            {
                split1.process (in2[ch], ch, lo[ch], h1);
                lo[ch] = apLow.process (lo[ch], ch);
                split2.process (h1, ch, mi[ch], hi[ch]);
            }
            const float pw[3] = { juce::jmax (lo[0] * lo[0], lo[1] * lo[1]), juce::jmax (mi[0] * mi[0], mi[1] * mi[1]), juce::jmax (hi[0] * hi[0], hi[1] * hi[1]) };
            blockPow[0] += pw[0]; blockPow[1] += pw[1]; blockPow[2] += pw[2];

            // DYNAMIK: per-band compressors
            const float rd = dynRamp.next();
            float g[3] = { 1.0f, 1.0f, 1.0f };
            if (rd > 0.0f)
                for (int b = 0; b < 3; ++b)
                {
                    const float gc = bandByp[(size_t) b] ? 1.0f : bandComp[(size_t) b].process (pw[b]);
                    g[b] = (1.0f + rd * (gc * bandGainS[(size_t) b] - 1.0f)) * soloS[(size_t) b];
                    grMin[b] = juce::jmin (grMin[b], bandByp[(size_t) b] ? 0.0f : bandComp[(size_t) b].getGainReductionDb());
                }

            // FARBE: exciter source from the high band before the band gains
            const float rs = satRamp.next();
            float exGen[2] = { 0.0f, 0.0f };
            if (rs > 0.0f && excite > 0.0f)
            {
                float e2[2], e3[2];
                for (int ch = 0; ch < 2; ++ch)
                {
                    e2[ch] = excSq2.process (excSq1.process (hi[ch], ch), ch);
                    e3[ch] = excCu2.process (excCu1.process (hi[ch], ch), ch);
                }
                const float ea = juce::jmax (std::abs (e2[0]), std::abs (e2[1]));
                excEnv = ea > excEnv ? ea + (excEnv - ea) * excAtt : ea + (excEnv - ea) * excRel;
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float n2 = e2[ch] / (excEnv + 1.0e-4f), n3 = juce::jlimit (-1.5f, 1.5f, e3[ch] / (excEnv + 1.0e-4f));
                    exGen[ch] = excPost.process ((0.7f * n2 * n2 + 0.3f * n3 * n3 * n3) * excEnv, ch);
                }
            }
            for (int ch = 0; ch < 2; ++ch)
            {
                lo[ch] *= g[0]; mi[ch] *= g[1]; hi[ch] *= g[2];
                if (rs > 0.0f)
                {
                    if (satLow > 0.0f)   // tape-style warmth on the bass band (even + odd harmonics)
                    {
                        const float k = 1.0f + 3.0f * satLow;
                        const float y = (std::tanh (k * lo[ch] + tapeBias) - tb) / (k * (1.0f - tb * tb));
                        lo[ch] += rs * juce::jmin (1.0f, satLow) * (y - lo[ch]);
                    }
                    hi[ch] += rs * excite * exGen[ch] * g[2];
                }
            }

            // STEREO: width per band, mono bass
            const float rst = stRamp.next();
            const float wm = 1.0f + rst * (wMidS - 1.0f), wh = 1.0f + rst * (wHighS - 1.0f);
            const float mm = 0.5f * (mi[0] + mi[1]), ms = 0.5f * (mi[0] - mi[1]) * wm;
            const float hm = 0.5f * (hi[0] + hi[1]), hs = 0.5f * (hi[0] - hi[1]) * wh;
            l = lo[0] + mm + ms + hm + hs;
            r = lo[1] + mm - ms + hm - hs;
            if (monoOn && stereo && rst > 0.0f)
            {
                const float m2 = 0.5f * (l + r), s2 = 0.5f * (l - r);
                const float s3 = s2 + rst * (monoSplit.highOnly (s2, 0) - s2);
                l = m2 + s3; r = m2 - s3;
            }
            // crossfade between the direct signal and the band path when the band section switches
            if (rb < 1.0f) { l = inL + rb * (l - inL); r = inR + rb * (r - inR); }

            // GLUE: bus compressor with side-chain high-pass, makeup and parallel mix
            const float rg = glueRamp.next();
            if (collecting)
            {
                const float hl = aGlueHp.process (l, 0), hr = aGlueHp.process (r, 1);
                blockPow[3] += juce::jmax (hl * hl, hr * hr);
            }
            if (rg > 0.0f)
            {
                const float scL = glueHpfOn ? glueSc.process (l, 0) : l, scR = glueHpfOn ? glueSc.process (r, 1) : r;
                const float det = juce::jmax (scL * scL, scR * scR);
                const float gg = glueComp.process (det) * glueMakeupS;
                const float k = 1.0f + rg * glueMixS * (gg - 1.0f);
                l *= k; r *= k;
                grMin[3] = juce::jmin (grMin[3], glueComp.getGainReductionDb());
            }

            L[i] = l;
            if (stereo) R[i] = r;
        }

        // assistant: level statistics per band and of the whole signal (for thresholds)
        if (collecting)
            for (int k = 0; k < 4; ++k)
            {
                const float db = powerToDb ((float) (blockPow[k] / len));
                if (db > -80.0f) ++levelHist[(size_t) k][(size_t) juce::jlimit (0, histBins - 1, (int) ((db + 80.0f) * 2.0f))];
            }
    }
    grLow.store (grMin[0]); grMid.store (grMin[1]); grHigh.store (grMin[2]); grGlue.store (grMin[3]);
    if (grMin[3] < grGluePeak.load()) grGluePeak.store (grMin[3]);
    glueInDb.store (glueComp.getDetectorDb());

    // 5) MIX: parallel blend with the dry signal (before the limiter, so the limiter sees the sum)
    mixS.setTargetValue (p (pMix) * 0.01f);
    if (mixS.isSmoothing() || mixS.getTargetValue() < 0.9999f)
        for (int i = 0; i < n; ++i)
        {
            const float m = mixS.getNextValue();
            for (int ch = 0; ch < chans; ++ch)
            {
                float* d = main.getWritePointer (ch);
                d[i] = dry.getSample (ch, i) + m * (d[i] - dry.getSample (ch, i));
            }
        }

    // 6) LIMITER: drive -> (oversampled) clipper -> true-peak limiter -> true-peak guard
    const bool limOn = (activeConfig & cfgLim) != 0;
    const float ceilingLin = dbToGain (p (pCeiling));
    if (limOn)
    {
        const float driveT = p (pLimDrive);
        const float driveC = 1.0f - coeff (sr, 0.05);
        for (int i = 0; i < n; ++i)
        {
            driveS += (driveT - driveS) * driveC;
            const float g = dbToGain (driveS);
            for (int ch = 0; ch < chans; ++ch) main.getWritePointer (ch)[i] *= g;
        }
        const float limCeiling = ceilingLin * dbToGain (-0.3f);   // headroom for the down-sampling filter
        const float clipAmt = p (pClip) * 0.01f;
        clipper.setThreshold (limCeiling * dbToGain (4.0f * (1.0f - clipAmt)), clipAmt > 0.01f);
        limiter.setCeiling (limCeiling);
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
        oversampling->processSamplesDown (block);
        tpGuard.process (main.getArrayOfWritePointers(), chans, n, ceilingLin * dbToGain (-0.05f));
    }
    else
    {
        driveS = p (pLimDrive);
        grClip.store (0.0f);
        grLimit.store (0.0f);
    }
    if (grLimit.load() < grLimitPeak.load()) grLimitPeak.store (grLimit.load());
    if (grClip.load() < grClipPeak.load()) grClipPeak.store (grClip.load());

    // 7) output gain (with the limiter on it can only lower the level, so the ceiling holds)
    outGainS.setTargetValue (dbToGain (limOn ? juce::jmin (0.0f, p (pOutput)) : p (pOutput)));
    bool broken = false;
    float outPk[2] = { 0.0f, 0.0f };
    for (int i = 0; i < n; ++i)
    {
        const float g = outGainS.getNextValue();
        for (int ch = 0; ch < chans; ++ch)
        {
            float* d = main.getWritePointer (ch);
            float v = d[i] * g;
            if (limOn) v = juce::jlimit (-ceilingLin, ceilingLin, v);
            if (! std::isfinite (v)) broken = true;
            d[i] = v;
            outPk[ch] = juce::jmax (outPk[ch], std::abs (v));
        }
    }
    if (broken)   // should never happen; if it does, recover instead of staying silent
    {
        main.clear();
        resetStates();
        outPk[0] = outPk[1] = 0.0f;
    }
    for (int ch = 0; ch < chans; ++ch) if (outPk[ch] > outPeak[(size_t) ch].load()) outPeak[(size_t) ch].store (outPk[ch]);
    outMeter.process (main, chans);

    // LERNEN: measure the delivered loudness
    if (learnState.load() == learnListening)
    {
        if (learnResetRequest.exchange (false))
        {
            learnMeter.requestReset();
            learnSkip = 0.5f;
            learnSignalSeconds.store (0.0f);
        }
        const float dt = (float) n / (float) sr;
        if (learnSkip > 0.0f) learnSkip -= dt;
        else if (! learnResultReady.load())
        {
            learnMeter.process (main, chans);
            if (learnMeter.getMomentary() > -50.0f) learnSignalSeconds.store (learnSignalSeconds.load() + dt);
            const bool finishNow = learnFinishRequest.exchange (false) && learnSignalSeconds.load() >= 3.0f;
            if (learnSignalSeconds.load() >= learnPassSeconds || finishNow)
            {
                learnMeasured.store (learnMeter.getIntegrated());
                learnResultReady.store (true);
            }
        }
    }

    // stereo correlation (+1 mono, 0 wide, -1 out of phase) and vectorscope
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

    // ORIGINAL (also the host's bypass) and level match. Level match is a listening aid only: it turns the
    // louder version down to the quieter one and is ignored while the host renders offline.
    float diff = 0.0f;
    if (inMeter.hasRunning() && outMeter.hasRunning())
        diff = juce::jlimit (-30.0f, 30.0f, outMeter.getRunning() - (inMeter.getRunning() - p (pInput)));
    compareGainDb.store (diff);
    const bool match = p (pCompare) > 0.5f && ! isNonRealtime();
    const float procT = match ? juce::jmin (0.0f, -diff) : 0.0f, origT = match ? juce::jmin (0.0f, diff) : 0.0f;
    bypassMix.setTargetValue (p (pBypass) > 0.5f ? 1.0f : 0.0f);
    const float mc = 1.0f - coeff (sr, 0.05);
    for (int i = 0; i < n; ++i)
    {
        matchProcDb += (procT - matchProcDb) * mc;
        matchOrigDb += (origT - matchOrigDb) * mc;
        const float bm = bypassMix.getNextValue();
        const float gp = dbToGain (matchProcDb) * (1.0f - bm), go = dbToGain (matchOrigDb) * bm;
        if (configSwitching) configFade = juce::jmax (0.0f, configFade - configFadeStep);
        else if (configHold > 0) --configHold;
        else if (configFade < 1.0f) configFade = juce::jmin (1.0f, configFade + configFadeStep);
        for (int ch = 0; ch < chans; ++ch)
        {
            float* d = main.getWritePointer (ch);
            d[i] = (d[i] * gp + orig.getSample (ch, i) * go) * configFade;
        }
    }
}

//==============================================================================
float AuricMaster3Processor::getLearnProgress() const
{
    return learnState.load() == learnListening ? juce::jlimit (0.0f, 1.0f, learnSignalSeconds.load() / juce::jmax (1.0f, learnPassSeconds)) : 0.0f;
}

void AuricMaster3Processor::startLoudnessLearn (float firstPassSeconds)
{
    if ((int) p (pTarget) == 0) setParam ("target", 1.0f);
    if (p (pLimOn) < 0.5f) setParam ("lim_on", 1.0f);
    learnPass = 0;
    learnPassSeconds = firstPassSeconds;
    learnResultReady.store (false);
    learnFinishRequest.store (false);
    learnResetRequest.store (true);
    learnSignalSeconds.store (0.0f);
    learnState.store (learnListening);
    learnStatus = juce::String::fromUTF8 ("Lautheit wird gemessen \xe2\x80\x93 Song abspielen (lauter Teil)");
}

void AuricMaster3Processor::finishLoudnessLearn()
{
    if (learnState.load() == learnListening) learnFinishRequest.store (true);
}

void AuricMaster3Processor::timerCallback()
{
    if (latencyDirty.exchange (false))
        setLatencySamples (latencySamplesNow.load());

    if (resonance.consumeLearnResult())
    {
        assistCollect.store (false);
        learnedFreqs = resonance.getBinFrequencies();
        learnedPower = resonance.getLearnPower();
        learnedActivity = resonance.getLearnActivity();
        applyAssistant();
    }

    if (learnResultReady.load())
    {
        using juce::String;
        const float target = Targets::lufs ((int) p (pTarget));
        const float measured = learnMeasured.load();
        const float drive = p (pLimDrive);
        const auto lufsText = [] (float v) { return String (v, 1).replace ("-", String::fromUTF8 ("\xe2\x88\x92")); };
        const auto dbText = [] (float v) { return (v > 0.04f ? "+" : "") + String (v, 1).replace ("-", String::fromUTF8 ("\xe2\x88\x92")); };
        if (measured < -60.0f)
        {
            learnState.store (learnFailed);
            learnStatus = String::fromUTF8 ("LERNEN: kein Signal gemessen \xe2\x80\x93 bitte w\xc3\xa4hrend der Wiedergabe starten");
        }
        else
        {
            const float delta = target - measured;
            if (learnPass > 0 && std::abs (delta) <= 0.3f)
            {
                learnState.store (learnDone);
                learnStatus = "Ziel " + lufsText (target) + " LUFS erreicht (gemessen " + lufsText (measured) + "), Drive " + dbText (drive) + " dB fest";
                outMeter.requestReset();   // the integrated value now starts with the final settings
                inMeter.requestReset();
            }
            else
            {
                const float newDrive = juce::jlimit (-12.0f, 24.0f, drive + delta);
                setParam ("lim_drive", newDrive);
                if (delta > 0.3f && newDrive >= 23.99f && learnPass > 0)
                {
                    learnState.store (learnFailed);
                    learnStatus = String::fromUTF8 ("Ziel nicht erreichbar: Drive am Maximum (+24 dB) \xe2\x80\x93 INPUT anheben oder leiseres Ziel w\xc3\xa4hlen");
                }
                else if (learnPass >= 3)
                {
                    learnState.store (learnDone);
                    learnStatus = "Ziel " + lufsText (target) + " LUFS (gemessen " + lufsText (measured) + "), Drive " + dbText (newDrive) + " dB fest";
                    outMeter.requestReset();
                    inMeter.requestReset();
                }
                else
                {
                    ++learnPass;
                    learnPassSeconds = 6.0f;
                    learnResetRequest.store (true);
                    learnStatus = String::fromUTF8 ("Lautheit wird gepr\xc3\xbc" "ft (Durchgang ") + String (learnPass + 1) + ")";
                }
            }
        }
        learnResultReady.store (false);
    }
}

//==============================================================================
void AuricMaster3Processor::startAssistant()
{
    aLR = aLL = aRR = aMlow = aSlow = 0.0;
    aPeak = 0.0f;
    for (auto& h : levelHist) h.fill (0);
    inMeter.requestReset();
    assistCollect.store (true);
    resonance.requestLearn (assistSeconds);
}

void AuricMaster3Processor::undoAssistant()
{
    if (! preAssistState.isValid()) return;
    if (learnState.load() == learnListening) learnState.store (learnIdle);
    apvts.replaceState (preAssistState.createCopy());
    preAssistState = {};
    hasAutoResult = false;
    assistSummary = {};
}

void AuricMaster3Processor::applyAssistant()
{
    if (learnedPower.empty()) return;
    using juce::String;
    preAssistState = apvts.copyState();
    const int profile = (int) p (pAssistProfile);
    std::array<float, numClashBands> defaults;
    std::copy (std::begin (clashDefaultFreqs), std::end (clashDefaultFreqs), defaults.begin());
    autoResult = AutoMatcher::analyse (learnedFreqs, learnedPower, learnedActivity, profile, defaults, clashNames);
    {
        // mastering: resonance bands stay subtle (the broad balance is handled by the EQ below)
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

    // broad tonal balance vs. the genre reference (1/3 octave, same bins and window as the analysis)
    constexpr int nb = 31;
    std::array<float, nb> fc {}, dev {};
    const double binHz = learnedFreqs.size() > 1 ? (double) learnedFreqs[1] : 1.0;
    const auto targetPow = AutoMatcher::targetBinPower (learnedFreqs, profile);
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
    auto round1 = [] (float v) { return std::round (v * 10.0f) / 10.0f; };
    auto percentile = [this] (int k, float pc)
    {
        const auto& h = levelHist[(size_t) k];
        long total = 0;
        for (int c : h) total += c;
        if (total == 0) return -30.0f;
        long acc = 0;
        for (int i = 0; i < histBins; ++i) { acc += h[(size_t) i]; if ((float) acc >= pc * (float) total) return -80.0f + 0.5f * (float) i + 0.25f; }
        return 0.0f;
    };

    // EQ: about 60 % of the broad deviation, at most +-4 dB (the character of the mix stays)
    const float eqG[4] = { round1 (juce::jlimit (-4.0f, 4.0f, -0.6f * avgDev (40.0f, 120.0f))),
                           round1 (juce::jlimit (-4.0f, 4.0f, -0.6f * avgDev (200.0f, 500.0f))),
                           round1 (juce::jlimit (-4.0f, 4.0f, -0.6f * avgDev (2000.0f, 5000.0f))),
                           round1 (juce::jlimit (-4.0f, 4.0f, -0.6f * avgDev (9000.0f, 16000.0f))) };
    const float eqOld[4] = { p (pEq1 + 1), p (pEq1 + 5), p (pEq1 + 9), p (pEq1 + 13) };
    const bool eqWasOn = p (pEqOn) > 0.5f;
    setParam ("eq_on", 1.0f);
    const float eqFreq[4] = { 80.0f, 300.0f, 3000.0f, 12000.0f }, eqQ[4] = { 0.7f, 1.0f, 1.0f, 0.7f };
    for (int b = 0; b < 4; ++b)
    {
        const auto n = "eq" + String (b + 1);
        setParam ((n + "_freq").toRawUTF8(), eqFreq[b]);
        setParam ((n + "_q").toRawUTF8(), eqQ[b]);
        setParam ((n + "_gain").toRawUTF8(), eqG[b]);
        setParam ((n + "_mode").toRawUTF8(), 0.0f);
    }

    // dynamics: thresholds from the measured band levels (90th percentile), shifted by the EQ change;
    // an open mix (high peak-to-loudness ratio) gets more compression than one that is already dense
    const float integratedIn = inMeter.getIntegrated();
    const float plr = juce::jlimit (0.0f, 30.0f, 20.0f * std::log10 (aPeak + 1.0e-9f) - integratedIn);
    const float base = juce::jlimit (0.1f, 0.8f, (plr - 8.0f) / 12.0f);
    const float eqShift[3] = { eqWasOn ? eqG[0] - eqOld[0] : eqG[0],
                               eqWasOn ? 0.5f * (eqG[1] + eqG[2] - eqOld[1] - eqOld[2]) : 0.5f * (eqG[1] + eqG[2]),
                               eqWasOn ? eqG[3] - eqOld[3] : eqG[3] };
    setParam ("dyn_on", 1.0f);
    for (int b = 0; b < 3; ++b)
    {
        const auto pre = String ("mb_") + bandIds[b] + "_";
        const float thr = juce::jlimit (-50.0f, 0.0f, std::round (percentile (b, 0.9f) + eqShift[b] - (1.0f + 4.0f * base)));
        setParam ((pre + "thr").toRawUTF8(), thr);
        setParam ((pre + "ratio").toRawUTF8(), std::round ((1.5f + base) * 10.0f) / 10.0f);
        setParam ((pre + "gain").toRawUTF8(), 0.0f);
        setParam ((pre + "solo").toRawUTF8(), 0.0f);
        setParam ((pre + "byp").toRawUTF8(), 0.0f);
    }
    // glue: about 1-2 dB gain reduction in the loud parts (typical bus "glue")
    const float glueThr = juce::jlimit (-40.0f, 0.0f, std::round (percentile (3, 0.9f) - (3.0f + 3.0f * base)));
    setParam ("glue_on", 1.0f);
    setParam ("glue_thr", glueThr);
    setParam ("glue_ratio", 2.0f);
    setParam ("glue_att", 30.0f);
    setParam ("glue_auto", 1.0f);
    setParam ("glue_makeup", 0.0f);
    setParam ("glue_hpf", 2.0f);
    setParam ("glue_mix", 100.0f);

    // width from the correlation above 300 Hz, mono bass from the side content in the low end
    const double den = std::sqrt (aLL * aRR);
    const float corr = den > 1.0e-12 ? (float) (aLR / den) : 1.0f;
    float wMid = 100.0f, wHigh = 100.0f;
    if (corr < 0.3f)       { wMid = 85.0f;  wHigh = 90.0f; }
    else if (corr > 0.85f) { wMid = 110.0f; wHigh = 120.0f; }
    const float sideLow = (float) (aSlow / (aMlow + 1.0e-12));
    const float mono = sideLow > 0.1f ? 150.0f : 100.0f;
    setParam ("st_on", 1.0f);
    setParam ("width_mid", wMid); setParam ("width_high", wHigh); setParam ("mono_freq", mono);

    // colour, limiter
    setParam ("sat_on", 1.0f);
    setParam ("sat_low", plr > 12.0f ? 25.0f : 10.0f);
    setParam ("sat_high", avgDev (8000.0f, 14000.0f) < -2.0f ? 30.0f : 15.0f);
    setParam ("lim_mode", plr > 14.0f ? 1.0f : 0.0f);
    setParam ("clip", std::round (15.0f + 20.0f * base));

    // resonances
    for (int i = 0; i < numClashBands; ++i)
    {
        const auto n = "clash" + String (i + 1);
        const auto& b = autoResult.bands[(size_t) i];
        setParam ((n + "_freq").toRawUTF8(), b.freq);
        setParam ((n + "_amt").toRawUTF8(), b.amount);
        setParam ((n + "_width").toRawUTF8(), b.width);
    }
    setParam ("res_depth", autoResult.depth);
    setParam ("res_on", 1.0f);

    auto sgn = [] (float v) { return (v > 0.04f ? "+" : "") + String (v, 1); };
    const auto dot = String::fromUTF8 ("  \xc2\xb7  ");
    assistSummary = "EQ " + sgn (eqG[0]) + " / " + sgn (eqG[1]) + " / " + sgn (eqG[2]) + " / " + sgn (eqG[3]) + " dB" + dot
                  + "Multiband " + String (p (pMb), 0) + " / " + String (p (pMb + mbStride), 0) + " / " + String (p (pMb + 2 * mbStride), 0) + " dB" + dot
                  + "Glue " + String (glueThr, 0) + " dB" + dot
                  + "Breite " + String (juce::roundToInt (wMid)) + "/" + String (juce::roundToInt (wHigh)) + " %" + dot
                  + "Mono-Bass " + String (juce::roundToInt (mono)) + " Hz" + dot
                  + "PLR " + String (plr, 1) + " dB";
    hasAutoResult = true;
    autoResultTime = juce::Time::getMillisecondCounter();
    ++assistRuns;

    // finally: measure the loudness with the new settings and set the drive for the target
    startLoudnessLearn (8.0f);
}

void AuricMaster3Processor::selectSnapshot (int slot)
{
    slot = juce::jlimit (0, 3, slot);
    if (slot == activeSnapshot) return;
    snapshots[(size_t) activeSnapshot] = apvts.copyState();
    if (snapshots[(size_t) slot].isValid())
        apvts.replaceState (snapshots[(size_t) slot].createCopy());
    activeSnapshot = slot;
}

juce::AudioProcessorEditor* AuricMaster3Processor::createEditor()
{
    return new AuricMaster3Editor (*this);
}

void AuricMaster3Processor::getStateInformation (juce::MemoryBlock& destData)
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

void AuricMaster3Processor::setStateInformation (const void* data, int sizeInBytes)
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
    return new AuricMaster3Processor();
}
