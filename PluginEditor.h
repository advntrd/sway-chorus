#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class SwayAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                   public juce::Timer,
                                   public juce::ComboBox::Listener,
                                   public juce::Button::Listener
{
public:
    SwayAudioProcessorEditor (SwayAudioProcessor&);
    ~SwayAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void comboBoxChanged (juce::ComboBox* box) override;
    void buttonClicked (juce::Button* button) override;

private:
    void drawLfoVisualizer (juce::Graphics& g, juce::Rectangle<float> area);
    void updateSyncEnablement();

    SwayAudioProcessor& audioProcessor;

    juce::ComboBox presetBox;
    juce::Label presetLabel;

    juce::ToggleButton syncButton { "Sync" };
    juce::ComboBox syncDivisionBox;
    juce::Label syncDivisionLabel;

    juce::Slider rateSlider, depthSlider, voicesSlider, feedbackSlider, widthSlider, mixSlider, toneSlider;
    juce::Label rateLabel, depthLabel, voicesLabel, feedbackLabel, widthLabel, mixLabel, toneLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        rateAttachment, depthAttachment, voicesAttachment, feedbackAttachment, widthAttachment, mixAttachment, toneAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncDivisionAttachment;

    juce::Rectangle<float> lfoVisualizerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwayAudioProcessorEditor)
};