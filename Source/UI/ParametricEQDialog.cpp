/*
  ==============================================================================

    ParametricEQDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "ParametricEQDialog.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioBufferManager.h"
#include <cmath>

//==============================================================================
// BandControl Implementation

ParametricEQDialog::BandControl::BandControl(const juce::String& bandName)
    : m_bandName(bandName),
      m_titleLabel("titleLabel", bandName),
      m_freqLabel("freqLabel", "Frequency:"),
      m_freqValueLabel("freqValueLabel", "1000 Hz"),
      m_gainLabel("gainLabel", "Gain:"),
      m_gainValueLabel("gainValueLabel", "0.0 dB"),
      m_qLabel("qLabel", "Q:"),
      m_qValueLabel("qValueLabel", "0.71")
{
    // Title
    m_titleLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Frequency controls
    m_freqLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_freqLabel);

    m_freqSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_freqSlider.setRange(20.0, 20000.0, 1.0);
    m_freqSlider.setSkewFactorFromMidPoint(1000.0);  // Logarithmic scale
    m_freqSlider.setValue(1000.0);
    m_freqSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_freqSlider.onValueChange = [this]() {
        updateValueLabels();
        if (auto* parent = findParentComponentOfClass<ParametricEQDialog>())
            parent->onParameterChanged();
    };
    addAndMakeVisible(m_freqSlider);

    m_freqValueLabel.setJustificationType(juce::Justification::centredLeft);
    m_freqValueLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    addAndMakeVisible(m_freqValueLabel);

    // Gain controls
    m_gainLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_gainLabel);

    m_gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_gainSlider.setRange(-20.0, 20.0, 0.1);
    m_gainSlider.setValue(0.0);
    m_gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_gainSlider.onValueChange = [this]() {
        updateValueLabels();
        if (auto* parent = findParentComponentOfClass<ParametricEQDialog>())
            parent->onParameterChanged();
    };
    addAndMakeVisible(m_gainSlider);

    m_gainValueLabel.setJustificationType(juce::Justification::centredLeft);
    m_gainValueLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    addAndMakeVisible(m_gainValueLabel);

    // Q controls
    m_qLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_qLabel);

    m_qSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_qSlider.setRange(0.1, 10.0, 0.01);
    m_qSlider.setValue(0.707);
    m_qSlider.setSkewFactorFromMidPoint(0.707);  // Logarithmic scale centered on 0.707
    m_qSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_qSlider.onValueChange = [this]() {
        updateValueLabels();
        if (auto* parent = findParentComponentOfClass<ParametricEQDialog>())
            parent->onParameterChanged();
    };
    addAndMakeVisible(m_qSlider);

    m_qValueLabel.setJustificationType(juce::Justification::centredLeft);
    m_qValueLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    addAndMakeVisible(m_qValueLabel);

    // Initialize value labels
    updateValueLabels();

    setSize(400, 120);
}

void ParametricEQDialog::BandControl::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawRect(getLocalBounds(), 1);
}

void ParametricEQDialog::BandControl::resized()
{
    auto area = getLocalBounds().reduced(10);

    // Title
    m_titleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(5);

    // Frequency row
    auto freqRow = area.removeFromTop(24);
    m_freqLabel.setBounds(freqRow.removeFromLeft(70));
    freqRow.removeFromLeft(5);
    m_freqValueLabel.setBounds(freqRow.removeFromRight(70));
    freqRow.removeFromRight(5);
    m_freqSlider.setBounds(freqRow);

    area.removeFromTop(3);

    // Gain row
    auto gainRow = area.removeFromTop(24);
    m_gainLabel.setBounds(gainRow.removeFromLeft(70));
    gainRow.removeFromLeft(5);
    m_gainValueLabel.setBounds(gainRow.removeFromRight(70));
    gainRow.removeFromRight(5);
    m_gainSlider.setBounds(gainRow);

    area.removeFromTop(3);

    // Q row
    auto qRow = area.removeFromTop(24);
    m_qLabel.setBounds(qRow.removeFromLeft(70));
    qRow.removeFromLeft(5);
    m_qValueLabel.setBounds(qRow.removeFromRight(70));
    qRow.removeFromRight(5);
    m_qSlider.setBounds(qRow);
}

void ParametricEQDialog::BandControl::setParameters(const ParametricEQ::BandParameters& params)
{
    m_freqSlider.setValue(params.frequency, juce::dontSendNotification);
    m_gainSlider.setValue(params.gain, juce::dontSendNotification);
    m_qSlider.setValue(params.q, juce::dontSendNotification);
    updateValueLabels();
}

ParametricEQ::BandParameters ParametricEQDialog::BandControl::getParameters() const
{
    ParametricEQ::BandParameters params;
    params.frequency = static_cast<float>(m_freqSlider.getValue());
    params.gain = static_cast<float>(m_gainSlider.getValue());
    params.q = static_cast<float>(m_qSlider.getValue());
    return params;
}

void ParametricEQDialog::BandControl::resetToNeutral()
{
    m_gainSlider.setValue(0.0, juce::sendNotification);
}

void ParametricEQDialog::BandControl::updateValueLabels()
{
    // Frequency
    {
        float freq = static_cast<float>(m_freqSlider.getValue());
        if (freq >= 1000.0f)
        {
            m_freqValueLabel.setText(juce::String(freq / 1000.0f, 2) + " kHz", juce::dontSendNotification);
        }
        else
        {
            m_freqValueLabel.setText(juce::String(static_cast<int>(freq)) + " Hz", juce::dontSendNotification);
        }
    }

    // Gain
    {
        float gain = static_cast<float>(m_gainSlider.getValue());
        juce::String gainText = juce::String(gain, 1) + " dB";
        if (gain > 0.0f)
            gainText = "+" + gainText;
        m_gainValueLabel.setText(gainText, juce::dontSendNotification);
    }

    // Q
    {
        float q = static_cast<float>(m_qSlider.getValue());
        m_qValueLabel.setText(juce::String(q, 2), juce::dontSendNotification);
    }
}

//==============================================================================
// ParametricEQDialog Implementation

ParametricEQDialog::ParametricEQDialog(AudioEngine* audioEngine,
                                       AudioBufferManager* bufferManager,
                                       int64_t selectionStart,
                                       int64_t selectionEnd)
    : m_titleLabel("titleLabel", "3-Band Parametric EQ"),
      m_lowBand("Low Shelf"),
      m_midBand("Mid Peak"),
      m_highBand("High Shelf"),
      m_previewButton("Preview"),
      m_loopToggle("Loop"),
      m_resetButton("Reset"),
      m_applyButton("Apply"),
      m_cancelButton("Cancel"),
      m_audioEngine(audioEngine),
      m_bufferManager(bufferManager),
      m_selectionStart(selectionStart),
      m_selectionEnd(selectionEnd),
      m_isPreviewPlaying(false)
{
    // Title
    m_titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Band controls
    addAndMakeVisible(m_lowBand);
    addAndMakeVisible(m_midBand);
    addAndMakeVisible(m_highBand);

    // Preview controls - only visible if audio engine available
    if (m_audioEngine && m_bufferManager)
    {
        m_previewButton.onClick = [this]() { onPreviewClicked(); };
        addAndMakeVisible(m_previewButton);

        m_loopToggle.setToggleState(true, juce::dontSendNotification);  // Default ON
        addAndMakeVisible(m_loopToggle);

        // Create EQ processor for preview
        m_parametricEQ = std::make_unique<ParametricEQ>();
    }

    // Buttons
    m_resetButton.onClick = [this]() { onResetClicked(); };
    addAndMakeVisible(m_resetButton);

    m_applyButton.onClick = [this]() { onApplyClicked(); };
    addAndMakeVisible(m_applyButton);

    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    // Dialog size: 3 bands (120px each) + spacing + title + buttons
    setSize(450, 490);
}

ParametricEQDialog::~ParametricEQDialog()
{
    // Stop preview when dialog is destroyed
    if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
    {
        m_audioEngine->stop();
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
    }
}

std::optional<ParametricEQ::Parameters> ParametricEQDialog::showDialog(
    AudioEngine* audioEngine,
    AudioBufferManager* bufferManager,
    int64_t selectionStart,
    int64_t selectionEnd,
    const ParametricEQ::Parameters& currentParams)
{
    ParametricEQDialog dialog(audioEngine, bufferManager, selectionStart, selectionEnd);

    // Initialize with current parameters
    dialog.m_lowBand.setParameters(currentParams.low);
    dialog.m_midBand.setParameters(currentParams.mid);
    dialog.m_highBand.setParameters(currentParams.high);

    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(&dialog);
    options.dialogTitle = "Parametric EQ";
    options.dialogBackgroundColour = juce::Colour(0xff2b2b2b);
    options.escapeKeyTriggersCloseButton = false;
    options.useNativeTitleBar = false;
    options.resizable = false;
    options.componentToCentreAround = nullptr;

    #if JUCE_MODAL_LOOPS_PERMITTED
        int result = options.runModal();
        return (result == 1) ? dialog.m_result : std::nullopt;
    #else
        jassertfalse;  // Modal loops required
        return std::nullopt;
    #endif
}

void ParametricEQDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));

    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawRect(getLocalBounds(), 1);
}

void ParametricEQDialog::resized()
{
    auto area = getLocalBounds().reduced(15);

    // Title
    m_titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    // Low band
    m_lowBand.setBounds(area.removeFromTop(120));
    area.removeFromTop(5);

    // Mid band
    m_midBand.setBounds(area.removeFromTop(120));
    area.removeFromTop(5);

    // High band
    m_highBand.setBounds(area.removeFromTop(120));
    area.removeFromTop(15);

    // Button row - standardized layout across all dialogs
    // Left: Preview + Loop | Center: Reset | Right: Cancel + Apply
    auto buttonRow = area.removeFromTop(30);
    const int buttonWidth = 90;
    const int buttonSpacing = 10;

    // Left side: Preview and Loop toggle (if audio engine available)
    if (m_audioEngine && m_bufferManager)
    {
        m_previewButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
        buttonRow.removeFromLeft(buttonSpacing);
        m_loopToggle.setBounds(buttonRow.removeFromLeft(60));
        buttonRow.removeFromLeft(buttonSpacing);
    }

    // Right side: Cancel and Apply buttons
    m_applyButton.setBounds(buttonRow.removeFromRight(buttonWidth));
    buttonRow.removeFromRight(buttonSpacing);
    m_cancelButton.setBounds(buttonRow.removeFromRight(buttonWidth));
    buttonRow.removeFromRight(buttonSpacing);

    // Center: Reset button (uses remaining space)
    m_resetButton.setBounds(buttonRow);
}

void ParametricEQDialog::visibilityChanged()
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

void ParametricEQDialog::onApplyClicked()
{
    m_result = getCurrentParameters();

    // Disable EQ preview when applying
    if (m_audioEngine)
    {
        m_audioEngine->setParametricEQPreview(ParametricEQ::Parameters::createNeutral(), false);
    }

    if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
    {
        parent->exitModalState(1);
    }
}

void ParametricEQDialog::onCancelClicked()
{
    m_result = std::nullopt;

    // Disable EQ preview when cancelling
    if (m_audioEngine)
    {
        m_audioEngine->setParametricEQPreview(ParametricEQ::Parameters::createNeutral(), false);
    }

    if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
    {
        parent->exitModalState(0);
    }
}

void ParametricEQDialog::onResetClicked()
{
    m_lowBand.resetToNeutral();
    m_midBand.resetToNeutral();
    m_highBand.resetToNeutral();
}

void ParametricEQDialog::onPreviewClicked()
{
    if (!m_audioEngine || !m_bufferManager || !m_parametricEQ)
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

    // Using REALTIME_DSP mode for instant parameter updates

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

    // 3. Set preview mode to REALTIME_DSP for instant parameter changes
    m_audioEngine->setPreviewMode(PreviewMode::REALTIME_DSP);

    // 4. Set initial EQ parameters
    ParametricEQ::Parameters params = getCurrentParameters();
    m_audioEngine->setParametricEQPreview(params, true);

    // 5. Set preview selection offset for accurate cursor positioning
    m_audioEngine->setPreviewSelectionOffset(m_selectionStart);

    // 6. Set position and loop points in FILE coordinates
    double selectionStartSec = m_selectionStart / m_bufferManager->getSampleRate();
    double selectionEndSec = m_selectionEnd / m_bufferManager->getSampleRate();

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
}

void ParametricEQDialog::onParameterChanged()
{
    // Only update preview if it's currently playing
    if (!m_isPreviewPlaying || !m_audioEngine)
        return;

    // Update EQ parameters atomically - no buffer reload needed!
    ParametricEQ::Parameters params = getCurrentParameters();
    m_audioEngine->setParametricEQPreview(params, true);

    // That's it! The audio thread will pick up the new parameters
    // on the next audio block. No artifacts, instant response.
}

ParametricEQ::Parameters ParametricEQDialog::getCurrentParameters() const
{
    ParametricEQ::Parameters params;
    params.low = m_lowBand.getParameters();
    params.mid = m_midBand.getParameters();
    params.high = m_highBand.getParameters();
    return params;
}
