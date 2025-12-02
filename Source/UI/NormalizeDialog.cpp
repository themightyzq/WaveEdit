/*
  ==============================================================================

    NormalizeDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "NormalizeDialog.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioBufferManager.h"

NormalizeDialog::NormalizeDialog(AudioEngine* audioEngine,
                                 AudioBufferManager* bufferManager,
                                 int64_t selectionStart,
                                 int64_t selectionEnd)
    : m_audioEngine(audioEngine)
    , m_bufferManager(bufferManager)
    , m_selectionStart(selectionStart)
    , m_selectionEnd(selectionEnd)
    , m_currentPeakDB(0.0f)
{
    // Title
    m_titleLabel.setText("Normalize", juce::dontSendNotification);
    m_titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Target level slider (-80 to 0 dB, default -0.1 dB for safety margin)
    m_targetLevelSlider.setRange(-80.0, 0.0, 0.1);
    m_targetLevelSlider.setValue(-0.1, juce::dontSendNotification);
    m_targetLevelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_targetLevelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 24);
    m_targetLevelSlider.setTextValueSuffix(" dB");
    m_targetLevelSlider.onValueChange = [this]() { onTargetLevelChanged(); };
    addAndMakeVisible(m_targetLevelSlider);

    m_targetLevelLabel.setText("Target Peak Level:", juce::dontSendNotification);
    m_targetLevelLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(m_targetLevelLabel);

    // Current peak display
    m_currentPeakLabel.setText("Current Peak:", juce::dontSendNotification);
    m_currentPeakLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(m_currentPeakLabel);

    m_currentPeakValue.setText("Analyzing...", juce::dontSendNotification);
    m_currentPeakValue.setJustificationType(juce::Justification::left);
    m_currentPeakValue.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(m_currentPeakValue);

    // Required gain display
    m_requiredGainLabel.setText("Required Gain:", juce::dontSendNotification);
    m_requiredGainLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(m_requiredGainLabel);

    m_requiredGainValue.setText("—", juce::dontSendNotification);
    m_requiredGainValue.setJustificationType(juce::Justification::left);
    m_requiredGainValue.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(m_requiredGainValue);

    // Loop toggle
    m_loopToggle.setButtonText("Loop Preview");
    m_loopToggle.setToggleState(false, juce::dontSendNotification);
    addAndMakeVisible(m_loopToggle);

    // Buttons
    m_previewButton.setButtonText("Preview");
    m_previewButton.onClick = [this]() { onPreviewClicked(); };
    addAndMakeVisible(m_previewButton);

    m_applyButton.setButtonText("Apply");
    m_applyButton.onClick = [this]() { onApplyClicked(); };
    addAndMakeVisible(m_applyButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    setSize(400, 310);  // Increased height for loop toggle
}

NormalizeDialog::~NormalizeDialog()
{
    // Stop any preview playback
    if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
    {
        m_audioEngine->stop();
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
    }
}

void NormalizeDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void NormalizeDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // Title
    m_titleLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(10); // Spacing

    // Target level
    auto targetRow = bounds.removeFromTop(30);
    m_targetLevelLabel.setBounds(targetRow.removeFromLeft(140));
    targetRow.removeFromLeft(10); // Spacing
    m_targetLevelSlider.setBounds(targetRow);
    bounds.removeFromTop(15); // Spacing

    // Current peak
    auto currentRow = bounds.removeFromTop(24);
    m_currentPeakLabel.setBounds(currentRow.removeFromLeft(140));
    currentRow.removeFromLeft(10); // Spacing
    m_currentPeakValue.setBounds(currentRow);
    bounds.removeFromTop(10); // Spacing

    // Required gain
    auto gainRow = bounds.removeFromTop(24);
    m_requiredGainLabel.setBounds(gainRow.removeFromLeft(140));
    gainRow.removeFromLeft(10); // Spacing
    m_requiredGainValue.setBounds(gainRow);
    bounds.removeFromTop(15); // Spacing

    // Buttons (bottom) - standardized layout
    // Left: Preview + Loop | Right: Cancel + Apply
    bounds.removeFromTop(bounds.getHeight() - 40); // Push to bottom
    auto buttonRow = bounds.removeFromTop(40);
    const int buttonWidth = 90;
    const int buttonSpacing = 10;

    // Left side: Preview and Loop toggle
    m_previewButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(buttonSpacing);
    m_loopToggle.setBounds(buttonRow.removeFromLeft(100));  // Wider for "Loop Preview" text
    buttonRow.removeFromLeft(buttonSpacing);

    // Right side: Cancel and Apply buttons
    m_applyButton.setBounds(buttonRow.removeFromRight(buttonWidth));
    buttonRow.removeFromRight(buttonSpacing);
    m_cancelButton.setBounds(buttonRow.removeFromRight(buttonWidth));
}

void NormalizeDialog::visibilityChanged()
{
    if (isVisible())
    {
        // Analyze peak level when dialog becomes visible
        analyzePeakLevel();
    }
    else
    {
        // Stop preview when dialog is hidden
        if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
        {
            m_audioEngine->stop();
            m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
        }
    }
}

void NormalizeDialog::analyzePeakLevel()
{
    if (!m_bufferManager)
        return;

    // Use selection bounds passed in constructor
    int64_t numSamples = m_selectionEnd - m_selectionStart;

    // Extract audio range
    auto workBuffer = m_bufferManager->getAudioRange(m_selectionStart, numSamples);

    // Find peak magnitude across all channels
    float peakMagnitude = 0.0f;
    for (int ch = 0; ch < workBuffer.getNumChannels(); ++ch)
    {
        const float channelPeak = workBuffer.getMagnitude(ch, 0, workBuffer.getNumSamples());
        peakMagnitude = std::max(peakMagnitude, channelPeak);
    }

    // Convert to dB
    if (peakMagnitude > 0.0f)
    {
        m_currentPeakDB = juce::Decibels::gainToDecibels(peakMagnitude);
    }
    else
    {
        m_currentPeakDB = -100.0f; // Silent
    }

    // Update UI
    m_currentPeakValue.setText(juce::String(m_currentPeakDB, 2) + " dB", juce::dontSendNotification);
    updateRequiredGain();
}

void NormalizeDialog::updateRequiredGain()
{
    const float targetDB = static_cast<float>(m_targetLevelSlider.getValue());
    const float requiredGainDB = targetDB - m_currentPeakDB;

    juce::String gainText = juce::String(requiredGainDB, 2) + " dB";
    if (requiredGainDB > 0.0f)
    {
        gainText = "+" + gainText;
    }

    m_requiredGainValue.setText(gainText, juce::dontSendNotification);

    // Warn if gain is excessive
    if (requiredGainDB > 12.0f)
    {
        m_requiredGainValue.setColour(juce::Label::textColourId, juce::Colours::orange);
    }
    else if (requiredGainDB > 24.0f)
    {
        m_requiredGainValue.setColour(juce::Label::textColourId, juce::Colours::red);
    }
    else
    {
        m_requiredGainValue.setColour(juce::Label::textColourId,
            getLookAndFeel().findColour(juce::Label::textColourId));
    }
}

void NormalizeDialog::onPreviewClicked()
{
    if (!m_audioEngine || !m_bufferManager)
        return;

    // Toggle behavior: if preview is playing, stop it
    if (m_isPreviewPlaying && m_audioEngine->isPlaying())
    {
        m_audioEngine->stop();
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
        m_isPreviewPlaying = false;
        m_previewButton.setButtonText("Preview");
        m_previewButton.setColour(juce::TextButton::buttonColourId, getLookAndFeel().findColour(juce::TextButton::buttonColourId));
        return;
    }

    // Following GainDialog pattern (GainDialog.cpp:289-365)

    // 0. Stop any current playback FIRST
    if (m_audioEngine->isPlaying())
    {
        m_audioEngine->stop();
    }

    // 1. Clear stale loop points (CRITICAL for coordinate system)
    m_audioEngine->clearLoopPoints();

    // 2. Configure looping based on loop toggle
    bool shouldLoop = m_loopToggle.getToggleState();
    m_audioEngine->setLooping(shouldLoop);

    // 3. Extract selection using bounds passed in constructor
    int64_t numSamples = m_selectionEnd - m_selectionStart;

    auto workBuffer = m_bufferManager->getAudioRange(m_selectionStart, numSamples);
    const double sampleRate = m_bufferManager->getSampleRate();
    const int numChannels = workBuffer.getNumChannels();

    // 4. Apply normalize to copy (ON MESSAGE THREAD)
    const float targetDB = static_cast<float>(m_targetLevelSlider.getValue());
    const float requiredGainDB = targetDB - m_currentPeakDB;
    const float gainLinear = juce::Decibels::decibelsToGain(requiredGainDB);

    workBuffer.applyGain(gainLinear);

    // 5. Load into preview system (THREAD-SAFE)
    m_audioEngine->loadPreviewBuffer(workBuffer, sampleRate, numChannels);

    // 6. Set preview mode and position
    m_audioEngine->setPreviewMode(PreviewMode::OFFLINE_BUFFER);

    // CRITICAL: Set preview selection offset for accurate cursor positioning
    // This transforms preview buffer coordinates (0-based) to file coordinates
    m_audioEngine->setPreviewSelectionOffset(m_selectionStart);

    m_audioEngine->setPosition(0.0);

    // CRITICAL: Set loop points in PREVIEW BUFFER coordinates (0-based)
    // Preview buffer spans from 0.0s to selection length
    double selectionLengthSec = numSamples / sampleRate;
    if (shouldLoop)
    {
        m_audioEngine->setLoopPoints(0.0, selectionLengthSec);
    }

    // 7. Start playback
    m_audioEngine->play();

    // 8. Update button state for toggle
    m_isPreviewPlaying = true;
    m_previewButton.setButtonText("Stop Preview");
    m_previewButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
}

void NormalizeDialog::onApplyClicked()
{
    const float targetDB = static_cast<float>(m_targetLevelSlider.getValue());

    // Stop any preview playback
    if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
    {
        m_audioEngine->stop();
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
    }

    if (m_applyCallback)
    {
        m_applyCallback(targetDB);
    }
}

void NormalizeDialog::onCancelClicked()
{
    // Stop any preview playback
    if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
    {
        m_audioEngine->stop();
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
    }

    if (m_cancelCallback)
    {
        m_cancelCallback();
    }
}

void NormalizeDialog::onTargetLevelChanged()
{
    updateRequiredGain();
}
