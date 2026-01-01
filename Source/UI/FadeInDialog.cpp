/*
  ==============================================================================

    FadeInDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "FadeInDialog.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioBufferManager.h"
#include "../Audio/AudioProcessor.h"
#include "../Utils/Settings.h"

FadeInDialog::FadeInDialog(AudioEngine* audioEngine,
                           AudioBufferManager* bufferManager,
                           int64_t selectionStart,
                           int64_t selectionEnd)
    : m_audioEngine(audioEngine)
    , m_bufferManager(bufferManager)
    , m_selectionStart(selectionStart)
    , m_selectionEnd(selectionEnd)
{
    // Title
    m_titleLabel.setText("Fade In", juce::dontSendNotification);
    m_titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Instruction
    m_instructionLabel.setText("Apply fade from 0% to 100% amplitude over the selection.",
                              juce::dontSendNotification);
    m_instructionLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_instructionLabel);

    // Curve type selector
    m_curveTypeLabel.setText("Curve Type:", juce::dontSendNotification);
    m_curveTypeLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_curveTypeLabel);

    m_curveTypeBox.addItem("Linear", 1);
    m_curveTypeBox.addItem("Exponential", 2);
    m_curveTypeBox.addItem("Logarithmic", 3);
    m_curveTypeBox.addItem("S-Curve", 4);

    // Load last-used curve from settings
    int lastCurve = Settings::getInstance().getSetting("dsp.lastFadeInCurve", 0);
    m_curveTypeBox.setSelectedId(lastCurve + 1, juce::dontSendNotification);

    m_curveTypeBox.onChange = [this]() { onCurveTypeChanged(); };
    addAndMakeVisible(m_curveTypeBox);

    // Curve preview - initialize with the selected curve type
    m_curvePreview.setCurveType(static_cast<FadeCurveType>(lastCurve));
    addAndMakeVisible(m_curvePreview);

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

    setSize(520, 270);  // Increased width to accommodate curve preview
}

FadeInDialog::~FadeInDialog()
{
    // Stop any preview playback and reset bypass
    if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
    {
        m_audioEngine->stop();
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
        m_audioEngine->setPreviewBypassed(false);
    }
}

void FadeInDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void FadeInDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // Title
    m_titleLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(10); // Spacing

    // Instruction
    m_instructionLabel.setBounds(bounds.removeFromTop(50));
    bounds.removeFromTop(15); // Spacing

    // Curve type selector with preview
    auto curveRow = bounds.removeFromTop(60);  // Increased height for preview

    // Left side: label and combo box
    auto controlsArea = curveRow.removeFromLeft(280);
    auto labelRow = controlsArea.removeFromTop(30);
    m_curveTypeLabel.setBounds(labelRow.removeFromLeft(90));
    labelRow.removeFromLeft(10); // Spacing
    m_curveTypeBox.setBounds(labelRow.removeFromLeft(180));

    // Right side: curve preview
    curveRow.removeFromLeft(20);  // Spacing between controls and preview
    m_curvePreview.setBounds(curveRow.removeFromLeft(100).removeFromTop(60));

    bounds.removeFromTop(15); // Spacing

    // Buttons (bottom) - standardized layout
    // Left: Preview + Loop | Right: Cancel + Apply
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

void FadeInDialog::visibilityChanged()
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

void FadeInDialog::onPreviewClicked()
{
    if (!m_audioEngine || !m_bufferManager)
        return;

    // Toggle behavior: if preview is playing, stop it
    if (m_isPreviewPlaying && m_audioEngine->isPlaying())
    {
        m_audioEngine->stop();
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
        m_audioEngine->setFadePreview(true, 0, 0.0f, false);  // Disable fade processor
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

    // Following GainDialog/NormalizeDialog pattern

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

    // 3. Get fade parameters
    int64_t numSamples = m_selectionEnd - m_selectionStart;
    const double sampleRate = m_bufferManager->getSampleRate();
    float durationMs = static_cast<float>((numSamples / sampleRate) * 1000.0);
    int curveIndex = m_curveTypeBox.getSelectedId() - 1;  // Convert to 0-based index

    // 4. Set preview mode to REALTIME_DSP for instant parameter changes
    m_audioEngine->setPreviewMode(PreviewMode::REALTIME_DSP);

    // 5. Set fade parameters (true = fade in)
    m_audioEngine->setFadePreview(true, curveIndex, durationMs, true);

    // 6. Set preview selection offset for accurate cursor positioning
    m_audioEngine->setPreviewSelectionOffset(m_selectionStart);

    // 7. Set position and loop points in FILE coordinates
    double selectionStartSec = m_selectionStart / sampleRate;
    double selectionEndSec = m_selectionEnd / sampleRate;

    m_audioEngine->setPosition(selectionStartSec);

    if (shouldLoop)
    {
        m_audioEngine->setLoopPoints(selectionStartSec, selectionEndSec);
    }

    // 8. Start playback
    m_audioEngine->play();

    // 9. Update button state for toggle
    m_isPreviewPlaying = true;
    m_previewButton.setButtonText("Stop Preview");
    m_previewButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);

    // 10. Enable bypass button during preview
    m_bypassButton.setEnabled(true);
}

void FadeInDialog::onApplyClicked()
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

void FadeInDialog::onCancelClicked()
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

void FadeInDialog::onCurveTypeChanged()
{
    // Save preference
    int curveIndex = m_curveTypeBox.getSelectedId() - 1;
    Settings::getInstance().setSetting("dsp.lastFadeInCurve", curveIndex);

    // Update the curve preview
    m_curvePreview.setCurveType(static_cast<FadeCurveType>(curveIndex));

    // If preview is active, update parameters in real-time
    if (m_isPreviewPlaying && m_audioEngine)
    {
        // Calculate fade duration
        int64_t numSamples = m_selectionEnd - m_selectionStart;
        const double sampleRate = m_bufferManager->getSampleRate();
        float durationMs = static_cast<float>((numSamples / sampleRate) * 1000.0);

        // Update fade parameters atomically - instant response!
        m_audioEngine->setFadePreview(true, curveIndex, durationMs, true);
    }
}

void FadeInDialog::onBypassClicked()
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
