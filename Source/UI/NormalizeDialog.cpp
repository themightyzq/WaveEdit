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
#include "../Audio/AudioProcessor.h"
#include "../Utils/Settings.h"

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

    // Mode selector - restore saved preference
    int savedMode = static_cast<int>(Settings::getInstance().getSetting("dsp.normalizeMode", 0));  // 0 = Peak default
    m_mode = (savedMode == 1) ? NormalizeMode::RMS : NormalizeMode::PEAK;

    m_modeLabel.setText("Mode:", juce::dontSendNotification);
    m_modeLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(m_modeLabel);

    m_modeSelector.addItem("Peak Level", 1);
    m_modeSelector.addItem("RMS Level", 2);
    m_modeSelector.setSelectedId(m_mode == NormalizeMode::PEAK ? 1 : 2, juce::dontSendNotification);
    m_modeSelector.onChange = [this]() { onModeChanged(); };
    addAndMakeVisible(m_modeSelector);

    // Target level slider (-80 to 0 dB, default -0.1 dB for safety margin)
    m_targetLevelSlider.setRange(-80.0, 0.0, 0.1);
    m_targetLevelSlider.setValue(-0.1, juce::dontSendNotification);
    m_targetLevelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_targetLevelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 24);
    m_targetLevelSlider.setTextValueSuffix(" dB");
    m_targetLevelSlider.onValueChange = [this]() { onTargetLevelChanged(); };
    addAndMakeVisible(m_targetLevelSlider);

    m_targetLevelLabel.setText("Target Level:", juce::dontSendNotification);  // Changed from "Target Peak Level:"
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

    // Current RMS display
    m_currentRMSLabel.setText("Current RMS:", juce::dontSendNotification);
    m_currentRMSLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(m_currentRMSLabel);

    m_currentRMSValue.setText("Analyzing...", juce::dontSendNotification);
    m_currentRMSValue.setJustificationType(juce::Justification::left);
    m_currentRMSValue.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(m_currentRMSValue);

    // Required gain display
    m_requiredGainLabel.setText("Required Gain:", juce::dontSendNotification);
    m_requiredGainLabel.setJustificationType(juce::Justification::right);
    addAndMakeVisible(m_requiredGainLabel);

    m_requiredGainValue.setText("—", juce::dontSendNotification);
    m_requiredGainValue.setJustificationType(juce::Justification::left);
    m_requiredGainValue.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(m_requiredGainValue);

    // Loop toggle
    m_loopToggle.setButtonText("Loop");
    m_loopToggle.setToggleState(true, juce::dontSendNotification);  // Default ON
    addAndMakeVisible(m_loopToggle);

    // Buttons
    m_previewButton.setButtonText("Preview");
    m_previewButton.onClick = [this]() { onPreviewClicked(); };
    addAndMakeVisible(m_previewButton);

    // Bypass button for A/B comparison (only enabled when preview is active)
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

    setSize(400, 380);  // Increased height for mode selector and RMS display
}

NormalizeDialog::~NormalizeDialog()
{
    // Stop any preview playback and reset bypass state
    if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
    {
        m_audioEngine->stop();
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
        m_audioEngine->setPreviewBypassed(false);
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

    // Mode selector row
    auto modeRow = bounds.removeFromTop(30);
    m_modeLabel.setBounds(modeRow.removeFromLeft(140));
    modeRow.removeFromLeft(10); // Spacing
    m_modeSelector.setBounds(modeRow.removeFromLeft(150));
    bounds.removeFromTop(15); // Spacing

    // Target level
    auto targetRow = bounds.removeFromTop(30);
    m_targetLevelLabel.setBounds(targetRow.removeFromLeft(140));
    targetRow.removeFromLeft(10); // Spacing
    m_targetLevelSlider.setBounds(targetRow);
    bounds.removeFromTop(15); // Spacing

    // Current peak
    auto peakRow = bounds.removeFromTop(24);
    m_currentPeakLabel.setBounds(peakRow.removeFromLeft(140));
    peakRow.removeFromLeft(10); // Spacing
    m_currentPeakValue.setBounds(peakRow);
    bounds.removeFromTop(10); // Spacing

    // Current RMS
    auto rmsRow = bounds.removeFromTop(24);
    m_currentRMSLabel.setBounds(rmsRow.removeFromLeft(140));
    rmsRow.removeFromLeft(10); // Spacing
    m_currentRMSValue.setBounds(rmsRow);
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

    // Left side: Preview, Bypass, and Loop toggle
    m_previewButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(buttonSpacing);
    m_bypassButton.setBounds(buttonRow.removeFromLeft(70));  // Slightly narrower for bypass
    buttonRow.removeFromLeft(buttonSpacing);
    m_loopToggle.setBounds(buttonRow.removeFromLeft(60));
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
        // Analyze both peak and RMS levels when dialog becomes visible
        updateCurrentLevels();
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

void NormalizeDialog::updateCurrentLevels()
{
    if (!m_bufferManager)
        return;

    // Extract audio for analysis
    int64_t numSamples = m_selectionEnd - m_selectionStart;
    if (numSamples <= 0)
        return;

    auto buffer = m_bufferManager->getAudioRange(m_selectionStart, numSamples);

    // Calculate peak level
    m_currentPeakDB = AudioProcessor::getPeakLevelDB(buffer);
    m_currentPeakValue.setText(
        juce::String(m_currentPeakDB, 2) + " dB",
        juce::dontSendNotification
    );

    // Calculate RMS level
    m_currentRMSDB = AudioProcessor::getRMSLevelDB(buffer);
    m_currentRMSValue.setText(
        juce::String(m_currentRMSDB, 2) + " dB",
        juce::dontSendNotification
    );

    // Update required gain based on mode
    updateRequiredGain();
}

void NormalizeDialog::updateRequiredGain()
{
    const float targetDB = static_cast<float>(m_targetLevelSlider.getValue());

    // Use current level based on selected mode
    float currentLevel = (m_mode == NormalizeMode::RMS) ? m_currentRMSDB : m_currentPeakDB;

    const float requiredGainDB = targetDB - currentLevel;

    juce::String gainText = juce::String(requiredGainDB, 2) + " dB";
    if (requiredGainDB > 0.0f)
    {
        gainText = "+" + gainText;
    }

    m_requiredGainValue.setText(gainText, juce::dontSendNotification);

    // Warn if gain is excessive
    if (requiredGainDB > 24.0f)
    {
        m_requiredGainValue.setColour(juce::Label::textColourId, juce::Colours::red);
    }
    else if (requiredGainDB > 12.0f)
    {
        m_requiredGainValue.setColour(juce::Label::textColourId, juce::Colours::orange);
    }
    else
    {
        m_requiredGainValue.setColour(juce::Label::textColourId,
            getLookAndFeel().findColour(juce::Label::textColourId));
    }

    // Update preview in real-time if currently playing
    if (m_isPreviewPlaying && m_audioEngine)
    {
        // Update normalize parameters atomically - instant response!
        m_audioEngine->setNormalizePreview(requiredGainDB, true);
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
        m_audioEngine->setNormalizePreview(0.0f, false);  // Disable normalize processor
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

    // 3. Calculate required gain for normalization
    const float targetDB = static_cast<float>(m_targetLevelSlider.getValue());
    float currentLevel = (m_mode == NormalizeMode::RMS) ? m_currentRMSDB : m_currentPeakDB;
    const float requiredGainDB = targetDB - currentLevel;

    // 4. Set preview mode to REALTIME_DSP for instant parameter changes
    m_audioEngine->setPreviewMode(PreviewMode::REALTIME_DSP);

    // 5. Set normalize parameters
    m_audioEngine->setNormalizePreview(requiredGainDB, true);

    // 6. Set preview selection offset for accurate cursor positioning
    m_audioEngine->setPreviewSelectionOffset(m_selectionStart);

    // 7. Set position and loop points in FILE coordinates
    const double sampleRate = m_bufferManager->getSampleRate();
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

    // 10. Enable bypass button now that preview is active
    m_bypassButton.setEnabled(true);
}

void NormalizeDialog::onApplyClicked()
{
    const float targetDB = static_cast<float>(m_targetLevelSlider.getValue());

    // Save the selected mode preference
    Settings::getInstance().setSetting("dsp.normalizeMode", static_cast<int>(m_mode));

    // Stop any preview playback
    if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
    {
        m_audioEngine->stop();
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
    }

    if (m_applyCallback)
    {
        // Note: The callback still receives targetDB, but the parent should
        // calculate the required gain based on the mode
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

void NormalizeDialog::onModeChanged()
{
    int selectedId = m_modeSelector.getSelectedId();
    m_mode = (selectedId == 2) ? NormalizeMode::RMS : NormalizeMode::PEAK;

    // Update target label to match mode
    if (m_mode == NormalizeMode::RMS)
    {
        m_targetLevelLabel.setText("Target RMS Level:", juce::dontSendNotification);
    }
    else
    {
        m_targetLevelLabel.setText("Target Peak Level:", juce::dontSendNotification);
    }

    // Recalculate required gain based on new mode
    updateRequiredGain();

    // Force layout update
    resized();
}

void NormalizeDialog::onBypassClicked()
{
    if (!m_audioEngine || !m_isPreviewPlaying)
    {
        return;  // Bypass only works when preview is active
    }

    // Toggle bypass state
    bool newBypassState = !m_audioEngine->isPreviewBypassed();
    m_audioEngine->setPreviewBypassed(newBypassState);

    // Update button appearance for visual feedback
    if (newBypassState)
    {
        m_bypassButton.setButtonText("Bypassed");
        m_bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff8c00));  // Orange
    }
    else
    {
        m_bypassButton.setButtonText("Bypass");
        m_bypassButton.setColour(juce::TextButton::buttonColourId, getLookAndFeel().findColour(juce::TextButton::buttonColourId));
    }
}
