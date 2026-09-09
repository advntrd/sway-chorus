#pragma once

#include <JuceHeader.h>
#include <array>

// One modulated delay line "voice". Several of these, each with its own
// LFO phase offset, are summed together to create the chorus effect.
struct ChorusVoice
{
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay { 44100 };
    float lfoPhase = 0.0f;      // 0..1, this voice's own position in the LFO cycle
    float phaseOffset = 0.0f;   // 0..1, how far this voice's LFO is offset from voice 0

    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        delay.prepare (spec);
        delay.setMaximumDelayInSamples ((int) (spec.sampleRate * 0.05)); // 50ms max, generous for chorus
    }

    void reset()
    {
        delay.reset();
        lfoPhase = phaseOffset;
    }
};

class SwayAudioProcessor  : public juce::AudioProcessor
{
public:
    SwayAudioProcessor();
    ~SwayAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // UI-safe read of the master LFO phase (0..1), for drawing an LFO visualizer.
    // Published once per block from the audio thread - same pattern as SideChainz's
    // currentPhase fix: never let the UI read a value the audio thread is mid-write on.
    float getDisplayLfoPhase() const { return displayLfoPhase.load (std::memory_order_relaxed); }

    // Applies a named preset by directly setting APVTS parameters. Safe to call from the UI thread.
    void applyPreset (const juce::String& presetName);
    static juce::StringArray getPresetNames();

    // Note division choices for tempo sync, in the same order as the "syncDivision" parameter.
    static juce::StringArray getSyncDivisionNames();

    // Converts a division index + host BPM into an LFO rate in Hz.
    static float syncDivisionToHz (int divisionIndex, double bpm);

    static constexpr int maxVoices = 4;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    std::array<ChorusVoice, maxVoices> voices;
    juce::dsp::IIR::Filter<float> toneFilterL, toneFilterR;

    double currentSampleRate = 44100.0;
    std::atomic<float> displayLfoPhase { 0.0f };

    // Smoothed so turning knobs doesn't cause zipper noise / clicks
    juce::SmoothedValue<float> smoothedRate, smoothedDepth, smoothedFeedback, smoothedWidth, smoothedMix, smoothedTone;

    float feedbackStateL = 0.0f, feedbackStateR = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwayAudioProcessor)
};