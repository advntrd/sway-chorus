#include "PluginProcessor.h"
#include "PluginEditor.h"

SwayAudioProcessor::SwayAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "PARAMETERS", createParameters())
#endif
{
    for (int i = 0; i < maxVoices; ++i)
        voices[(size_t) i].phaseOffset = (float) i / (float) maxVoices;
}

SwayAudioProcessor::~SwayAudioProcessor()
{
}

const juce::String SwayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SwayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SwayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SwayAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SwayAudioProcessor::getTailLengthSeconds() const
{
    return 0.05;
}

int SwayAudioProcessor::getNumPrograms() { return 1; }
int SwayAudioProcessor::getCurrentProgram() { return 0; }
void SwayAudioProcessor::setCurrentProgram (int) {}
const juce::String SwayAudioProcessor::getProgramName (int) { return {}; }
void SwayAudioProcessor::changeProgramName (int, const juce::String&) {}

void SwayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 1;

    for (auto& v : voices)
    {
        v.prepare (spec);
        v.reset();
    }

    toneFilterL.reset();
    toneFilterR.reset();
    toneFilterL.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 8000.0f);
    toneFilterR.coefficients = toneFilterL.coefficients;

    smoothedRate.reset (sampleRate, 0.02);
    smoothedDepth.reset (sampleRate, 0.02);
    smoothedFeedback.reset (sampleRate, 0.02);
    smoothedWidth.reset (sampleRate, 0.02);
    smoothedMix.reset (sampleRate, 0.02);
    smoothedTone.reset (sampleRate, 0.02);

    smoothedRate.setCurrentAndTargetValue (apvts.getRawParameterValue ("rate")->load());
    smoothedDepth.setCurrentAndTargetValue (apvts.getRawParameterValue ("depth")->load());
    smoothedFeedback.setCurrentAndTargetValue (apvts.getRawParameterValue ("feedback")->load());
    smoothedWidth.setCurrentAndTargetValue (apvts.getRawParameterValue ("width")->load());
    smoothedMix.setCurrentAndTargetValue (apvts.getRawParameterValue ("mix")->load());
    smoothedTone.setCurrentAndTargetValue (apvts.getRawParameterValue ("tone")->load());

    feedbackStateL = feedbackStateR = 0.0f;
}

void SwayAudioProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SwayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void SwayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();

    auto rateHz      = apvts.getRawParameterValue ("rate")->load();
    auto depthParam  = apvts.getRawParameterValue ("depth")->load();
    auto voicesParam = (int) apvts.getRawParameterValue ("voices")->load();
    auto feedback    = apvts.getRawParameterValue ("feedback")->load();
    auto width       = apvts.getRawParameterValue ("width")->load();
    auto mixParam    = apvts.getRawParameterValue ("mix")->load();
    auto toneHz      = apvts.getRawParameterValue ("tone")->load();
    auto syncOn      = apvts.getRawParameterValue ("sync")->load() > 0.5f;
    auto syncDivision = (int) apvts.getRawParameterValue ("syncDivision")->load();

    if (syncOn)
    {
        double bpm = 120.0;

        if (auto* playHead = getPlayHead())
            if (auto position = playHead->getPosition())
                if (position->getBpm().hasValue())
                    bpm = *position->getBpm();

        rateHz = syncDivisionToHz (syncDivision, bpm);
    }

    smoothedRate.setTargetValue (rateHz);
    smoothedDepth.setTargetValue (depthParam);
    smoothedFeedback.setTargetValue (feedback);
    smoothedWidth.setTargetValue (width);
    smoothedMix.setTargetValue (mixParam);
    smoothedTone.setTargetValue (toneHz);

    voicesParam = juce::jlimit (1, maxVoices, voicesParam);

    const float baseDelayMs = 7.0f;

    float lastToneHzApplied = -1.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float rate = smoothedRate.getNextValue();
        float depth = smoothedDepth.getNextValue();
        float fb = smoothedFeedback.getNextValue();
        float wid = smoothedWidth.getNextValue();
        float mix = smoothedMix.getNextValue();
        float tone = smoothedTone.getNextValue();

        if (std::abs (tone - lastToneHzApplied) > 1.0f)
        {
            auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, tone);
            toneFilterL.coefficients = coeffs;
            toneFilterR.coefficients = coeffs;
            lastToneHzApplied = tone;
        }

        float phaseIncrement = rate / (float) currentSampleRate;

        float wetL = 0.0f, wetR = 0.0f;

        for (int v = 0; v < voicesParam; ++v)
        {
            auto& voice = voices[(size_t) v];

            voice.lfoPhase += phaseIncrement;
            if (voice.lfoPhase >= 1.0f)
                voice.lfoPhase -= 1.0f;

            float effectivePhase = voice.lfoPhase + voice.phaseOffset;
            if (effectivePhase >= 1.0f)
                effectivePhase -= 1.0f;

            float lfoValue = std::sin (effectivePhase * juce::MathConstants<float>::twoPi);
            float delayMs = baseDelayMs + lfoValue * depth * baseDelayMs;
            float delaySamples = juce::jmax (1.0f, delayMs * 0.001f * (float) currentSampleRate);

            voice.delay.setDelay (delaySamples);

            float inL = numChannels > 0 ? buffer.getReadPointer (0)[sample] : 0.0f;
            float inR = numChannels > 1 ? buffer.getReadPointer (1)[sample] : inL;

            float monoIn = 0.5f * (inL + inR);
            float fbSample = 0.5f * (feedbackStateL + feedbackStateR);

            voice.delay.pushSample (0, monoIn + fbSample * fb);
            float delayedMono = voice.delay.popSample (0, delaySamples, true);

            float pan = (voicesParam > 1) ? ((float) v / (float) (voicesParam - 1) - 0.5f) : 0.0f;
            float panAmount = pan * wid;

            wetL += delayedMono * (1.0f - juce::jmax (0.0f, panAmount));
            wetR += delayedMono * (1.0f + juce::jmin (0.0f, panAmount));
        }

        if (voicesParam > 0)
        {
            wetL /= (float) voicesParam;
            wetR /= (float) voicesParam;
        }

        wetL = toneFilterL.processSample (wetL);
        wetR = toneFilterR.processSample (wetR);

        feedbackStateL = wetL;
        feedbackStateR = wetR;

        float dryL = numChannels > 0 ? buffer.getReadPointer (0)[sample] : 0.0f;
        float dryR = numChannels > 1 ? buffer.getReadPointer (1)[sample] : dryL;

        float outL = dryL * (1.0f - mix) + wetL * mix;
        float outR = dryR * (1.0f - mix) + wetR * mix;

        if (numChannels > 0) buffer.getWritePointer (0)[sample] = outL;
        if (numChannels > 1) buffer.getWritePointer (1)[sample] = outR;
    }

    displayLfoPhase.store (voices[0].lfoPhase, std::memory_order_relaxed);
}

bool SwayAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SwayAudioProcessor::createEditor()
{
    return new SwayAudioProcessorEditor (*this);
}

void SwayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void SwayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::StringArray SwayAudioProcessor::getPresetNames()
{
    return { "Subtle", "Classic", "Wide", "Vintage Tape", "Shimmer" };
}

void SwayAudioProcessor::applyPreset (const juce::String& presetName)
{
    struct PresetValues { float rate, depth; int voices; float feedback, width, mix, tone; };

    PresetValues p { 0.5f, 0.3f, 2, 0.0f, 0.5f, 0.3f, 8000.0f };

    if (presetName == "Subtle")       p = { 0.35f, 0.20f, 2, 0.00f, 0.30f, 0.20f, 9000.0f };
    else if (presetName == "Classic") p = { 0.80f, 0.35f, 2, 0.05f, 0.50f, 0.35f, 8000.0f };
    else if (presetName == "Wide")    p = { 0.60f, 0.45f, 4, 0.10f, 0.90f, 0.45f, 7500.0f };
    else if (presetName == "Vintage Tape") p = { 1.20f, 0.55f, 3, 0.25f, 0.40f, 0.40f, 4500.0f };
    else if (presetName == "Shimmer") p = { 2.20f, 0.60f, 4, 0.15f, 0.75f, 0.50f, 10000.0f };

    apvts.getParameter ("rate")->setValueNotifyingHost (apvts.getParameter ("rate")->convertTo0to1 (p.rate));
    apvts.getParameter ("depth")->setValueNotifyingHost (apvts.getParameter ("depth")->convertTo0to1 (p.depth));
    apvts.getParameter ("voices")->setValueNotifyingHost (apvts.getParameter ("voices")->convertTo0to1 ((float) p.voices));
    apvts.getParameter ("feedback")->setValueNotifyingHost (apvts.getParameter ("feedback")->convertTo0to1 (p.feedback));
    apvts.getParameter ("width")->setValueNotifyingHost (apvts.getParameter ("width")->convertTo0to1 (p.width));
    apvts.getParameter ("mix")->setValueNotifyingHost (apvts.getParameter ("mix")->convertTo0to1 (p.mix));
    apvts.getParameter ("tone")->setValueNotifyingHost (apvts.getParameter ("tone")->convertTo0to1 (p.tone));
    apvts.getParameter ("sync")->setValueNotifyingHost (0.0f);
}

juce::StringArray SwayAudioProcessor::getSyncDivisionNames()
{
    return { "1/1", "1/2", "1/2T", "1/4", "1/4T", "1/8", "1/8D", "1/8T", "1/16", "1/16T" };
}

float SwayAudioProcessor::syncDivisionToHz (int divisionIndex, double bpm)
{
    static const double beatsPerCycle[] =
    {
        4.0,
        2.0,
        2.0 * (2.0 / 3.0),
        1.0,
        1.0 * (2.0 / 3.0),
        0.5,
        0.5 * 1.5,
        0.5 * (2.0 / 3.0),
        0.25,
        0.25 * (2.0 / 3.0)
    };

    divisionIndex = juce::jlimit (0, (int) (sizeof (beatsPerCycle) / sizeof (double)) - 1, divisionIndex);

    double secondsPerBeat = 60.0 / juce::jmax (1.0, bpm);
    double secondsPerCycle = beatsPerCycle[divisionIndex] * secondsPerBeat;

    return secondsPerCycle > 0.0001 ? (float) (1.0 / secondsPerCycle) : 1.0f;
}

juce::AudioProcessorValueTreeState::ParameterLayout SwayAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("rate", "Rate",
        juce::NormalisableRange<float> (0.05f, 5.0f, 0.01f, 0.4f), 0.8f, "Hz"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("depth", "Depth",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));

    params.push_back (std::make_unique<juce::AudioParameterInt> ("voices", "Voices", 1, SwayAudioProcessor::maxVoices, 2));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("feedback", "Feedback",
        juce::NormalisableRange<float> (0.0f, 0.9f, 0.001f), 0.05f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("width", "Width",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("mix", "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));

    params.push_back (std::make_unique<juce::AudioParameterFloat> ("tone", "Tone",
        juce::NormalisableRange<float> (500.0f, 12000.0f, 1.0f, 0.4f), 8000.0f, "Hz"));

    params.push_back (std::make_unique<juce::AudioParameterBool> ("sync", "Sync", false));

    params.push_back (std::make_unique<juce::AudioParameterChoice> ("syncDivision", "Sync Division",
        SwayAudioProcessor::getSyncDivisionNames(), 3));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SwayAudioProcessor();
}