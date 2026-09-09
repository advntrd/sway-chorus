#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::Colour bgColour        (0xff12081f);
    const juce::Colour panelColour     (0xff1e0f33);
    const juce::Colour accentColour    (0xffb066ff);
    const juce::Colour accentFillColour(0x33b066ff);
    const juce::Colour gridColour      (0xff3a2255);
    const juce::Colour textColour      (0xffe0b3ff);
}

SwayAudioProcessorEditor::SwayAudioProcessorEditor (SwayAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    presetBox.addItem ("-- Select Preset --", 1000);
    int presetId = 1;
    for (auto& name : SwayAudioProcessor::getPresetNames())
        presetBox.addItem (name, presetId++);
    presetBox.setSelectedId (1000, juce::dontSendNotification);
    presetBox.setColour (juce::ComboBox::backgroundColourId, panelColour);
    presetBox.setColour (juce::ComboBox::textColourId, textColour);
    presetBox.setColour (juce::ComboBox::outlineColourId, gridColour);
    presetBox.addListener (this);
    addAndMakeVisible (presetBox);

    presetLabel.setText ("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centred);
    presetLabel.setColour (juce::Label::textColourId, textColour);
    presetLabel.attachToComponent (&presetBox, false);
    addAndMakeVisible (presetLabel);

    auto setupKnob = [this] (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, gridColour);
        slider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
        slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider.setColour (juce::Slider::textBoxOutlineColourId, gridColour);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, panelColour);
        addAndMakeVisible (slider);
    };

    setupKnob (rateSlider);
    setupKnob (depthSlider);
    setupKnob (voicesSlider);
    setupKnob (feedbackSlider);
    setupKnob (widthSlider);
    setupKnob (mixSlider);
    setupKnob (toneSlider);

    voicesSlider.setNumDecimalPlacesToDisplay (0);

    auto setupLabel = [this] (juce::Label& label, juce::Component& attachTo, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, textColour);
        label.attachToComponent (&attachTo, false);
        addAndMakeVisible (label);
    };

    setupLabel (rateLabel, rateSlider, "Rate");
    setupLabel (depthLabel, depthSlider, "Depth");
    setupLabel (voicesLabel, voicesSlider, "Voices");
    setupLabel (feedbackLabel, feedbackSlider, "Feedback");
    setupLabel (widthLabel, widthSlider, "Width");
    setupLabel (mixLabel, mixSlider, "Mix");
    setupLabel (toneLabel, toneSlider, "Tone");

    rateAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "rate", rateSlider);
    depthAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "depth", depthSlider);
    voicesAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "voices", voicesSlider);
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "feedback", feedbackSlider);
    widthAttachment    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "width", widthSlider);
    mixAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "mix", mixSlider);
    toneAttachment     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "tone", toneSlider);

    syncButton.setColour (juce::ToggleButton::textColourId, textColour);
    syncButton.setColour (juce::ToggleButton::tickColourId, accentColour);
    syncButton.addListener (this);
    addAndMakeVisible (syncButton);

    int divisionId = 1;
    for (auto& name : SwayAudioProcessor::getSyncDivisionNames())
        syncDivisionBox.addItem (name, divisionId++);
    syncDivisionBox.setColour (juce::ComboBox::backgroundColourId, panelColour);
    syncDivisionBox.setColour (juce::ComboBox::textColourId, textColour);
    syncDivisionBox.setColour (juce::ComboBox::outlineColourId, gridColour);
    addAndMakeVisible (syncDivisionBox);

    syncDivisionLabel.setText ("Division", juce::dontSendNotification);
    syncDivisionLabel.setJustificationType (juce::Justification::centred);
    syncDivisionLabel.setColour (juce::Label::textColourId, textColour);
    syncDivisionLabel.attachToComponent (&syncDivisionBox, false);
    addAndMakeVisible (syncDivisionLabel);

    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "sync", syncButton);
    syncDivisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.apvts, "syncDivision", syncDivisionBox);

    updateSyncEnablement();

    setSize (600, 460);
    startTimerHz (30);
}

SwayAudioProcessorEditor::~SwayAudioProcessorEditor()
{
    presetBox.removeListener (this);
    syncButton.removeListener (this);
}

void SwayAudioProcessorEditor::comboBoxChanged (juce::ComboBox* box)
{
    if (box == &presetBox)
    {
        auto id = presetBox.getSelectedId();
        if (id == 1000) return;

        auto name = presetBox.getItemText (presetBox.getSelectedItemIndex());
        audioProcessor.applyPreset (name);
        updateSyncEnablement();
    }
}

void SwayAudioProcessorEditor::buttonClicked (juce::Button* button)
{
    if (button == &syncButton)
        updateSyncEnablement();
}

void SwayAudioProcessorEditor::updateSyncEnablement()
{
    bool synced = syncButton.getToggleState();
    rateSlider.setEnabled (! synced);
    rateSlider.setAlpha (synced ? 0.4f : 1.0f);
    syncDivisionBox.setVisible (synced);
    syncDivisionLabel.setVisible (synced);
}

void SwayAudioProcessorEditor::timerCallback()
{
    repaint (lfoVisualizerArea.getSmallestIntegerContainer());
}

void SwayAudioProcessorEditor::drawLfoVisualizer (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (panelColour);
    g.fillRoundedRectangle (area, 6.0f);

    g.setColour (gridColour);
    g.drawHorizontalLine ((int) (area.getY() + area.getHeight() * 0.5f), area.getX(), area.getRight());

    float phase = audioProcessor.getDisplayLfoPhase();

    juce::Path wavePath;
    const int numPoints = 100;
    for (int i = 0; i <= numPoints; ++i)
    {
        float t = i / (float) numPoints;
        float y = std::sin ((t + phase) * juce::MathConstants<float>::twoPi);
        float x = area.getX() + area.getWidth() * t;
        float screenY = area.getCentreY() - y * area.getHeight() * 0.4f;

        if (i == 0) wavePath.startNewSubPath (x, screenY);
        else wavePath.lineTo (x, screenY);
    }

    g.setColour (accentColour);
    g.strokePath (wavePath, juce::PathStrokeType (2.0f));

    float markerX = area.getX() + area.getWidth() * 0.5f;
    float markerY = area.getCentreY() - std::sin (phase * juce::MathConstants<float>::twoPi) * area.getHeight() * 0.4f;
    g.setColour (juce::Colours::white);
    g.fillEllipse (markerX - 4.0f, markerY - 4.0f, 8.0f, 8.0f);
}

void SwayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgColour);
    drawLfoVisualizer (g, lfoVisualizerArea);
}

void SwayAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);

    auto topRow = area.removeFromTop (40);
    presetBox.setBounds (topRow.removeFromLeft (220));

    topRow.removeFromLeft (20);
    syncButton.setBounds (topRow.removeFromLeft (70));

    topRow.removeFromLeft (10);
    syncDivisionBox.setBounds (topRow.removeFromLeft (100));

    area.removeFromTop (20);

    lfoVisualizerArea = area.removeFromTop (80).toFloat();

    area.removeFromTop (30);

    auto knobRow1 = area.removeFromTop (110);
    int quarterWidth = knobRow1.getWidth() / 4;
    rateSlider.setBounds (knobRow1.removeFromLeft (quarterWidth).reduced (8, 0));
    depthSlider.setBounds (knobRow1.removeFromLeft (quarterWidth).reduced (8, 0));
    voicesSlider.setBounds (knobRow1.removeFromLeft (quarterWidth).reduced (8, 0));
    feedbackSlider.setBounds (knobRow1.reduced (8, 0));

    area.removeFromTop (20);

    auto knobRow2 = area.removeFromTop (110);
    int thirdWidth = knobRow2.getWidth() / 3;
    widthSlider.setBounds (knobRow2.removeFromLeft (thirdWidth).reduced (8, 0));
    mixSlider.setBounds (knobRow2.removeFromLeft (thirdWidth).reduced (8, 0));
    toneSlider.setBounds (knobRow2.reduced (8, 0));
}