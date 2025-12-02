/*
  ==============================================================================

    DCOffsetDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "DCOffsetDialog.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioBufferManager.h"
#include "../Audio/AudioProcessor.h"

DCOffsetDialog::DCOffsetDialog(AudioEngine* audioEngine,
                               AudioBufferManager* bufferManager,
                               int64_t selectionStart,
                               int64_t selectionEnd)
    : m_audioEngine(audioEngine)
    , m_bufferManager(bufferManager)
    , m_selectionStart(selectionStart)
    , m_selectionEnd(selectionEnd)
{
    // Title
    m_titleLabel.setText("Remove DC Offset", juce::dontSendNotification);
    m_titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Instruction
    m_instructionLabel.setText("Remove DC offset (average signal level) from the selection.\n"
                              "This centers the waveform around zero.",
                              juce::dontSendNotification);
    m_instructionLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_instructionLabel);

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

    setSize(400, 230);
}

DCOffsetDialog::~DCOffsetDialog()
{
    // Stop any preview playback
    if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
    {
        m_audioEngine->stop();
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
    }
}

void DCOffsetDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void DCOffsetDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // Title
    m_titleLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(10); // Spacing

    // Instruction
    m_instructionLabel.setBounds(bounds.removeFromTop(60));
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

void DCOffsetDialog::visibilityChanged()
{
    if (!isVisible())
    {
        // Stop preview when dialog is hidden
        if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
        {
            m_audioEngine->stop();
            m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
        }
    }
}

void DCOffsetDialog::onPreviewClicked()
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

    // Following GainDialog/NormalizeDialog/FadeInDialog/FadeOutDialog pattern

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

    // 4. Remove DC offset from copy (ON MESSAGE THREAD)
    AudioProcessor::removeDCOffset(workBuffer);

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

void DCOffsetDialog::onApplyClicked()
{
    // Stop any preview playback
    if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
    {
        m_audioEngine->stop();
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
    }

    if (m_applyCallback)
    {
        m_applyCallback();
    }
}

void DCOffsetDialog::onCancelClicked()
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
