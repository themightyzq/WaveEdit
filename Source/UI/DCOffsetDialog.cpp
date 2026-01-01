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
    m_loopToggle.setButtonText("Loop");
    m_loopToggle.setToggleState(true, juce::dontSendNotification);  // Default ON
    addAndMakeVisible(m_loopToggle);

    // Buttons
    m_previewButton.setButtonText("Preview");
    m_previewButton.onClick = [this]() { onPreviewClicked(); };
    addAndMakeVisible(m_previewButton);

    // Bypass button (starts disabled, enabled only during preview)
    m_bypassButton.setButtonText("Bypass");
    m_bypassButton.onClick = [this]() { onBypassClicked(); };
    m_bypassButton.setEnabled(false);  // Disabled until preview starts
    addAndMakeVisible(m_bypassButton);

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
    // Stop any preview playback and reset bypass
    if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
    {
        m_audioEngine->stop();
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
        m_audioEngine->setPreviewBypassed(false);
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
    // Left: Preview + Loop + Bypass | Right: Cancel + Apply
    bounds.removeFromTop(bounds.getHeight() - 40); // Push to bottom
    auto buttonRow = bounds.removeFromTop(40);
    const int buttonWidth = 90;
    const int buttonSpacing = 10;

    // Left side: Preview, Loop toggle, and Bypass
    m_previewButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(buttonSpacing);
    m_loopToggle.setBounds(buttonRow.removeFromLeft(60));  // Loop toggle
    buttonRow.removeFromLeft(buttonSpacing);
    m_bypassButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
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
        m_audioEngine->setDCOffsetPreview(false);  // Disable DC offset processor
        m_audioEngine->setPreviewBypassed(false);  // Reset bypass state
        m_isPreviewPlaying = false;
        m_previewButton.setButtonText("Preview");
        m_previewButton.setColour(juce::TextButton::buttonColourId, getLookAndFeel().findColour(juce::TextButton::buttonColourId));
        // Disable and reset bypass button
        m_bypassButton.setEnabled(false);
        m_bypassButton.setButtonText("Bypass");
        m_bypassButton.setColour(juce::TextButton::buttonColourId, getLookAndFeel().findColour(juce::TextButton::buttonColourId));
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

    // 3. Set preview mode to REALTIME_DSP for instant DC offset removal
    m_audioEngine->setPreviewMode(PreviewMode::REALTIME_DSP);

    // 4. Enable DC offset removal processor
    m_audioEngine->setDCOffsetPreview(true);

    // 5. Set preview selection offset for accurate cursor positioning
    m_audioEngine->setPreviewSelectionOffset(m_selectionStart);

    // 6. Set position and loop points in FILE coordinates
    const double sampleRate = m_bufferManager->getSampleRate();
    double selectionStartSec = m_selectionStart / sampleRate;
    double selectionEndSec = m_selectionEnd / sampleRate;

    m_audioEngine->setPosition(selectionStartSec);

    if (shouldLoop)
    {
        m_audioEngine->setLoopPoints(selectionStartSec, selectionEndSec);
    }

    // 7. Start playback
    m_audioEngine->play();

    // 8. Update button state for toggle
    m_isPreviewPlaying = true;
    m_previewButton.setButtonText("Stop Preview");
    m_previewButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);

    // 9. Enable bypass button during preview
    m_bypassButton.setEnabled(true);
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

void DCOffsetDialog::onBypassClicked()
{
    if (!m_audioEngine)
        return;

    bool bypassed = m_audioEngine->isPreviewBypassed();
    m_audioEngine->setPreviewBypassed(!bypassed);

    // Update button appearance
    if (!bypassed)
    {
        m_bypassButton.setButtonText("Bypassed");
        m_bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff8c00));
    }
    else
    {
        m_bypassButton.setButtonText("Bypass");
        m_bypassButton.setColour(juce::TextButton::buttonColourId,
                                 getLookAndFeel().findColour(juce::TextButton::buttonColourId));
    }
}
