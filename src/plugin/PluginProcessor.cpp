#include "PluginProcessor.h"
#include "../editor/PluginEditor.h"

using namespace juce;

AudioProcessorValueTreeState::ParameterLayout ApurrrProcessor::createLayout()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    const int v = 1; // parameter version
    auto choice = [&] (const char* id, const char* name, StringArray items, int def)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, v }, name, items, def));
    };
    auto toggle = [&] (const char* id, const char* name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, v }, name, def));
    };

    // Threshold in dBu as printed on the knob (0 dBu is -22 dBFS). The knob turns the other way round: right is lower.
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::thr, v }, "Threshold",
        NormalisableRange<float> (-20.0f, 10.0f, 0.01f), 4.0f, AudioParameterFloatAttributes().withLabel ("dBu")));
    choice (ids::ratio, "Ratio", { "1.5:1", "2:1", "3:1", "4:1", "6:1", "10:1", "Inf:1" }, 1);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::gain, v }, "Make-up",
        NormalisableRange<float> (0.0f, 24.0f, 0.01f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    choice (ids::attack, "Attack", { "0.03 ms", "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }, 5);
    choice (ids::release, "Release", { "50 ms", "100 ms", "200 ms", "500 ms", "1 s", "2 s", "Var" }, 2);
    {
        const float lo = 50.0f, ratio = 3000.0f / 50.0f;
        NormalisableRange<float> r (lo, 3000.0f,
            [lo, ratio] (float, float, float n) { return lo * std::pow (ratio, n); },
            [lo, ratio] (float, float, float x) { return std::log (x / lo) / std::log (ratio); },
            [lo] (float, float, float x) { return jlimit (lo, 3000.0f, x); });
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::relVar, v }, "Release Var", r, 150.0f, AudioParameterFloatAttributes().withLabel ("ms")));
    }
    choice (ids::knee, "Knee", { "Soft", "Med", "Hard" }, 1);
    choice (ids::tilt, "Tilt Filter", { "Norm", "Med", "Loud" }, 0);
    choice (ids::type, "Type", { "Old", "New" }, 1);
    toggle (ids::extSc, "Ext SC", false);
    choice (ids::link, "Link", { "IND", "50%", "60%", "70%", "80%", "90%", "100%" }, 6);
    choice (ids::shape, "Link Shape", { "Fast", "Slow", "Both" }, 0);
    choice (ids::muMode, "Make-up Mode", { "Auto", "Manual" }, 0);
    choice (ids::blend, "Blend", { "100%", "X-fade", "Par" }, 0);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, v }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));
    toggle (ids::inSw, "IN", true);
    toggle (ids::bypass, "Bypass", false);
    toggle (ids::levelMatch, "Level Match", false);
    choice (ids::meter, "Meter Shows", { "GR", "Out", "In" }, 0);

    return { p.begin(), p.end() };
}

ApurrrProcessor::ApurrrProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "APURRR2500", createLayout())
{
    pThr = apvts.getRawParameterValue (ids::thr);          pRatio = apvts.getRawParameterValue (ids::ratio);
    pGain = apvts.getRawParameterValue (ids::gain);        pAttack = apvts.getRawParameterValue (ids::attack);
    pRelease = apvts.getRawParameterValue (ids::release);  pRelVar = apvts.getRawParameterValue (ids::relVar);
    pKnee = apvts.getRawParameterValue (ids::knee);        pTilt = apvts.getRawParameterValue (ids::tilt);
    pType = apvts.getRawParameterValue (ids::type);        pExtSc = apvts.getRawParameterValue (ids::extSc);
    pLink = apvts.getRawParameterValue (ids::link);        pShape = apvts.getRawParameterValue (ids::shape);
    pMuMode = apvts.getRawParameterValue (ids::muMode);    pBlend = apvts.getRawParameterValue (ids::blend);
    pMix = apvts.getRawParameterValue (ids::mix);          pInSw = apvts.getRawParameterValue (ids::inSw);
    pBypass = apvts.getRawParameterValue (ids::bypass);    pLevelMatch = apvts.getRawParameterValue (ids::levelMatch);
}

apurr::Params ApurrrProcessor::readParams() const
{
    apurr::Params q;
    q.thrDbu = pThr->load();
    q.ratio = jlimit (0, 6, (int) std::lround (pRatio->load()));
    q.makeupDb = pGain->load();
    q.manualMakeup = (int) std::lround (pMuMode->load()) == 1;
    q.attack = jlimit (0, 6, (int) std::lround (pAttack->load()));
    q.release = jlimit (0, 6, (int) std::lround (pRelease->load()));
    q.relVarMs = pRelVar->load();
    q.knee = jlimit (0, 2, (int) std::lround (pKnee->load()));
    q.tilt = jlimit (0, 2, (int) std::lround (pTilt->load()));
    q.oldType = (int) std::lround (pType->load()) == 0;
    q.extSc = pExtSc->load() > 0.5f;
    q.link = jlimit (0, 6, (int) std::lround (pLink->load()));
    q.shape = jlimit (0, 2, (int) std::lround (pShape->load()));
    q.blend = jlimit (0, 2, (int) std::lround (pBlend->load()));
    q.mix = pMix->load() * 0.01;
    q.inSw = pInSw->load() > 0.5f;
    q.levelMatch = pLevelMatch->load() > 0.5f;
    return q;
}

bool ApurrrProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (! ((out == AudioChannelSet::mono() || out == AudioChannelSet::stereo()) && out == l.getMainInputChannelSet())) return false;
    const auto sc = l.getChannelSet (true, 1);
    return sc.isDisabled() || sc == AudioChannelSet::mono() || sc == AudioChannelSet::stereo();
}

var ApurrrProcessor::getSetting (const char* key, const var& fallback) const
{
    return apvts.state.getProperty (key, fallback);
}

void ApurrrProcessor::setSetting (const char* key, const var& v)
{
    apvts.state.setProperty (key, v, nullptr);
    if (String (key) == setting::needle) needleRt.store (jlimit (20, 2000, (int) v));
}

void ApurrrProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateRt.store (sampleRate);
    core.prepare (sampleRate);
    needleRt.store (jlimit (20, 2000, (int) getSetting (setting::needle, 300)));
    needleApplied = needleRt.load();
    core.setAvgMs ((double) needleApplied);
    core.setParams (readParams(), true);
    core.snap();
    maxChunk = jmax (64, samplesPerBlock);
    dryCopy.setSize (2, maxChunk, false, true, false);
    bypassMix = pBypass->load() > 0.5f ? 1.0f : 0.0f;
    lastLevelMatch = pLevelMatch->load() > 0.5f;
    setLatencySamples (0); // no look-ahead and no oversampling: the compressor adds no delay
}

void ApurrrProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    auto main = getBusBuffer (buffer, true, 0);
    const int numCh = jmin (2, main.getNumChannels());
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());
    if (numCh == 0) return;

    const bool scOn = getBusCount (true) > 1 && getBus (true, 1) != nullptr && getBus (true, 1)->isEnabled();
    AudioBuffer<float> sc;
    if (scOn) sc = getBusBuffer (buffer, true, 1);

    apurr::Params prm = readParams();
    if (prm.levelMatch != lastLevelMatch) { core.resetLevelMatch(); lastLevelMatch = prm.levelMatch; }
    core.setParams (prm);
    const int nd = needleRt.load();
    if (nd != needleApplied) { core.setAvgMs ((double) nd); needleApplied = nd; }

    const float bypassTarget = pBypass->load() > 0.5f ? 1.0f : 0.0f;
    const float rampStep = 1.0f / (0.010f * (float) jmax (8000.0, getSampleRate())); // about 10 ms
    float inPk = 0.0f, outPk = 0.0f, gpl = 0.0f, gpr = 0.0f;

    // The compressor always runs (warm state, and the GR meter stays active in bypass); bypass crossfades to the dry copy.
    for (int start = 0; start < main.getNumSamples(); start += maxChunk)
    {
        const int n = jmin (maxChunk, main.getNumSamples() - start);
        float* l = main.getWritePointer (0) + start;
        float* r = numCh > 1 ? main.getWritePointer (1) + start : nullptr;
        for (int ch = 0; ch < numCh; ++ch)
            FloatVectorOperations::copy (dryCopy.getWritePointer (ch), main.getReadPointer (ch) + start, n);
        for (int i = 0; i < n; ++i)
        {
            inPk = jmax (inPk, std::abs (l[i]));
            if (r) inPk = jmax (inPk, std::abs (r[i]));
        }

        const float* sl = nullptr; const float* sr = nullptr;
        if (scOn && sc.getNumChannels() > 0)
        {
            sl = sc.getReadPointer (0) + start;
            sr = sc.getNumChannels() > 1 ? sc.getReadPointer (1) + start : sl;
        }
        core.process (l, r, n, sl, sr);
        gpl = jmax (gpl, (float) core.grLeftPeak());
        gpr = jmax (gpr, (float) core.grRightPeak());

        for (int i = 0; i < n; ++i)
        {
            if (bypassMix < bypassTarget) bypassMix = jmin (bypassTarget, bypassMix + rampStep);
            else if (bypassMix > bypassTarget) bypassMix = jmax (bypassTarget, bypassMix - rampStep);
            if (bypassMix >= 1.0f)
            {
                l[i] = dryCopy.getReadPointer (0)[i];       // bit exact when bypassed
                if (r) r[i] = dryCopy.getReadPointer (1)[i];
            }
            else if (bypassMix > 0.0f)
            {
                l[i] += (dryCopy.getReadPointer (0)[i] - l[i]) * bypassMix;
                if (r) r[i] += (dryCopy.getReadPointer (1)[i] - r[i]) * bypassMix;
            }
            outPk = jmax (outPk, std::abs (l[i]));
            if (r) outPk = jmax (outPk, std::abs (r[i]));
        }
    }

    grL.store ((float) core.grLeft()); grR.store ((float) core.grRight());
    raiseTo (grPeakL, gpl); raiseTo (grPeakR, gpr); raiseTo (grPeak, jmax (gpl, gpr));
    raiseTo (inPeak, inPk);
    raiseTo (outPeak, outPk);
    for (int c = 0; c < 2; ++c) { inMs[c].store ((float) core.inMeanSquare (c)); outMs[c].store ((float) core.outMeanSquare (c)); }
    matchDb.store ((float) core.levelMatchDb());
}

AudioProcessorEditor* ApurrrProcessor::createEditor()
{
    return new ApurrrEditor (*this);
}

void ApurrrProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("stateVersion", 1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ApurrrProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml)); // unknown keys are ignored by APVTS
            needleRt.store (jlimit (20, 2000, (int) getSetting (setting::needle, 300)));
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ApurrrProcessor(); }
