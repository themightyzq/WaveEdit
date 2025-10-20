/*
  ==============================================================================

    StripSilenceDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "StripSilenceDialog.h"

StripSilenceDialog::StripSilenceDialog(RegionManager& regionManager,
                                       const juce::AudioBuffer<float>& audioBuffer,
                                       double sampleRate)
    : m_regionManager(regionManager)
    , m_audioBuffer(audioBuffer)
    , m_sampleRate(sampleRate)
    , m_isPreviewMode(false)
{
    // Threshold slider (dB) - default -40dB, range -80dB to 0dB
    m_thresholdLabel.setText("Threshold (dB):", juce::dontSendNotification);
    m_thresholdLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_thresholdLabel);

    m_thresholdSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_thresholdSlider.setRange(-80.0, 0.0, 0.1);
    m_thresholdSlider.setValue(-40.0);
    m_thresholdSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_thresholdSlider.onValueChange = [this]()
    {
        updateValueLabel(m_thresholdSlider, m_thresholdValueLabel, " dB");
    };
    addAndMakeVisible(m_thresholdSlider);

    m_thresholdValueLabel.setText("-40.0 dB", juce::dontSendNotification);
    m_thresholdValueLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_thresholdValueLabel);

    // Min Region Length slider (ms) - default 100ms, range 10ms to 5000ms
    m_minRegionLabel.setText("Min Region Length (ms):", juce::dontSendNotification);
    m_minRegionLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_minRegionLabel);

    m_minRegionSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_minRegionSlider.setRange(10.0, 5000.0, 1.0);
    m_minRegionSlider.setValue(100.0);
    m_minRegionSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_minRegionSlider.onValueChange = [this]()
    {
        updateValueLabel(m_minRegionSlider, m_minRegionValueLabel, " ms");
    };
    addAndMakeVisible(m_minRegionSlider);

    m_minRegionValueLabel.setText("100.0 ms", juce::dontSendNotification);
    m_minRegionValueLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_minRegionValueLabel);

    // Min Silence Length slider (ms) - default 500ms, range 10ms to 5000ms
    m_minSilenceLabel.setText("Min Silence Length (ms):", juce::dontSendNotification);
    m_minSilenceLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_minSilenceLabel);

    m_minSilenceSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_minSilenceSlider.setRange(10.0, 5000.0, 1.0);
    m_minSilenceSlider.setValue(500.0);
    m_minSilenceSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_minSilenceSlider.onValueChange = [this]()
    {
        updateValueLabel(m_minSilenceSlider, m_minSilenceValueLabel, " ms");
    };
    addAndMakeVisible(m_minSilenceSlider);

    m_minSilenceValueLabel.setText("500.0 ms", juce::dontSendNotification);
    m_minSilenceValueLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_minSilenceValueLabel);

    // Pre-Roll slider (ms) - default 10ms, range 0ms to 500ms
    m_preRollLabel.setText("Pre-Roll (ms):", juce::dontSendNotification);
    m_preRollLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_preRollLabel);

    m_preRollSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_preRollSlider.setRange(0.0, 500.0, 1.0);
    m_preRollSlider.setValue(10.0);
    m_preRollSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_preRollSlider.onValueChange = [this]()
    {
        updateValueLabel(m_preRollSlider, m_preRollValueLabel, " ms");
    };
    addAndMakeVisible(m_preRollSlider);

    m_preRollValueLabel.setText("10.0 ms", juce::dontSendNotification);
    m_preRollValueLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_preRollValueLabel);

    // Post-Roll slider (ms) - default 10ms, range 0ms to 500ms
    m_postRollLabel.setText("Post-Roll (ms):", juce::dontSendNotification);
    m_postRollLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_postRollLabel);

    m_postRollSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_postRollSlider.setRange(0.0, 500.0, 1.0);
    m_postRollSlider.setValue(10.0);
    m_postRollSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_postRollSlider.onValueChange = [this]()
    {
        updateValueLabel(m_postRollSlider, m_postRollValueLabel, " ms");
    };
    addAndMakeVisible(m_postRollSlider);

    m_postRollValueLabel.setText("10.0 ms", juce::dontSendNotification);
    m_postRollValueLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_postRollValueLabel);

    // Preview button
    m_previewButton.setButtonText("Preview");
    m_previewButton.onClick = [this]()
    {
        if (!validateParameters())
            return;

        applyStripSilence(true);  // Populate m_previewRegions
        m_isPreviewMode = true;

        // Update preview display with region list
        updatePreviewDisplay();
    };
    addAndMakeVisible(m_previewButton);

    // Apply button
    m_applyButton.setButtonText("Apply");
    m_applyButton.onClick = [this]()
    {
        if (!validateParameters())
            return;

        int numRegions = applyStripSilence(false);

        if (onApply)
            onApply(numRegions);

        // Close the dialog window after applying
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(1);
    };
    addAndMakeVisible(m_applyButton);

    // Cancel button
    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]()
    {
        if (onCancel)
            onCancel();

        // Close the dialog window
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(m_cancelButton);

    // Preview area
    m_previewLabel.setText("Waveform Preview (click Preview to see regions):", juce::dontSendNotification);
    m_previewLabel.setJustificationType(juce::Justification::centredLeft);
    m_previewLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    addAndMakeVisible(m_previewLabel);

    m_waveformPreview = std::make_unique<WaveformPreview>(m_audioBuffer);
    addAndMakeVisible(m_waveformPreview.get());

    m_regionCountLabel.setText("", juce::dontSendNotification);
    m_regionCountLabel.setJustificationType(juce::Justification::centredLeft);
    m_regionCountLabel.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(m_regionCountLabel);

    setSize(650, 580);  // Increased size for waveform preview
}

StripSilenceDialog::~StripSilenceDialog()
{
}

//==============================================================================
// Component overrides

void StripSilenceDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.drawText("Auto Region - Auto-Create Regions", getLocalBounds().removeFromTop(40),
               juce::Justification::centred, true);
}

void StripSilenceDialog::resized()
{
    auto area = getLocalBounds().reduced(20);

    // Title space
    area.removeFromTop(40);

    const int labelWidth = 180;
    const int sliderWidth = 300;
    const int valueWidth = 80;
    const int rowHeight = 40;
    const int buttonHeight = 30;
    const int buttonWidth = 100;

    // Threshold row
    auto thresholdRow = area.removeFromTop(rowHeight);
    m_thresholdLabel.setBounds(thresholdRow.removeFromLeft(labelWidth));
    m_thresholdSlider.setBounds(thresholdRow.removeFromLeft(sliderWidth));
    m_thresholdValueLabel.setBounds(thresholdRow.removeFromLeft(valueWidth));

    // Min Region Length row
    auto minRegionRow = area.removeFromTop(rowHeight);
    m_minRegionLabel.setBounds(minRegionRow.removeFromLeft(labelWidth));
    m_minRegionSlider.setBounds(minRegionRow.removeFromLeft(sliderWidth));
    m_minRegionValueLabel.setBounds(minRegionRow.removeFromLeft(valueWidth));

    // Min Silence Length row
    auto minSilenceRow = area.removeFromTop(rowHeight);
    m_minSilenceLabel.setBounds(minSilenceRow.removeFromLeft(labelWidth));
    m_minSilenceSlider.setBounds(minSilenceRow.removeFromLeft(sliderWidth));
    m_minSilenceValueLabel.setBounds(minSilenceRow.removeFromLeft(valueWidth));

    // Pre-Roll row
    auto preRollRow = area.removeFromTop(rowHeight);
    m_preRollLabel.setBounds(preRollRow.removeFromLeft(labelWidth));
    m_preRollSlider.setBounds(preRollRow.removeFromLeft(sliderWidth));
    m_preRollValueLabel.setBounds(preRollRow.removeFromLeft(valueWidth));

    // Post-Roll row
    auto postRollRow = area.removeFromTop(rowHeight);
    m_postRollLabel.setBounds(postRollRow.removeFromLeft(labelWidth));
    m_postRollSlider.setBounds(postRollRow.removeFromLeft(sliderWidth));
    m_postRollValueLabel.setBounds(postRollRow.removeFromLeft(valueWidth));

    // Preview area
    area.removeFromTop(10); // Spacing
    auto previewLabelArea = area.removeFromTop(25);
    m_previewLabel.setBounds(previewLabelArea);

    auto previewWaveformArea = area.removeFromTop(180);  // Taller for waveform
    m_waveformPreview->setBounds(previewWaveformArea);

    auto regionCountArea = area.removeFromTop(25);
    m_regionCountLabel.setBounds(regionCountArea);

    // Buttons at bottom
    area.removeFromTop(20); // Spacing
    auto buttonRow = area.removeFromTop(buttonHeight);

    // Center buttons horizontally
    int totalButtonWidth = buttonWidth * 3 + 20; // 3 buttons + 2 gaps
    int startX = (buttonRow.getWidth() - totalButtonWidth) / 2;

    m_previewButton.setBounds(startX, buttonRow.getY(), buttonWidth, buttonHeight);
    m_applyButton.setBounds(startX + buttonWidth + 10, buttonRow.getY(), buttonWidth, buttonHeight);
    m_cancelButton.setBounds(startX + (buttonWidth + 10) * 2, buttonRow.getY(), buttonWidth, buttonHeight);
}

//==============================================================================
// Helper methods

void StripSilenceDialog::updateValueLabel(juce::Slider& slider, juce::Label& label, const juce::String& suffix)
{
    double value = slider.getValue();
    juce::String text = juce::String(value, 1) + suffix;
    label.setText(text, juce::dontSendNotification);
}

int StripSilenceDialog::applyStripSilence(bool previewOnly)
{
    // Get parameter values from sliders
    float thresholdDB = static_cast<float>(m_thresholdSlider.getValue());
    float minRegionLengthMs = static_cast<float>(m_minRegionSlider.getValue());
    float minSilenceLengthMs = static_cast<float>(m_minSilenceSlider.getValue());
    float preRollMs = static_cast<float>(m_preRollSlider.getValue());
    float postRollMs = static_cast<float>(m_postRollSlider.getValue());

    if (previewOnly)
    {
        // Preview mode: Save current regions and create temporary regions
        m_previewRegions.clear();

        // Create a temporary RegionManager to test the algorithm
        RegionManager tempManager;
        tempManager.autoCreateRegions(m_audioBuffer, m_sampleRate,
                                       thresholdDB, minRegionLengthMs, minSilenceLengthMs,
                                       preRollMs, postRollMs);

        // Store preview regions
        for (int i = 0; i < tempManager.getNumRegions(); ++i)
        {
            const Region* region = tempManager.getRegion(i);
            if (region)
                m_previewRegions.add(*region);
        }

        return m_previewRegions.size();
    }
    else
    {
        // Apply mode: Actually create regions in the RegionManager
        // Note: autoCreateRegions() clears all existing regions first, so we just return the count after
        m_regionManager.autoCreateRegions(m_audioBuffer, m_sampleRate,
                                          thresholdDB, minRegionLengthMs, minSilenceLengthMs,
                                          preRollMs, postRollMs);

        int numRegionsCreated = m_regionManager.getNumRegions();

        juce::Logger::writeToLog("Auto Region: Created " + juce::String(numRegionsCreated) + " regions");

        return numRegionsCreated;
    }
}

bool StripSilenceDialog::validateParameters()
{
    // Validate audio buffer
    if (m_audioBuffer.getNumSamples() == 0 || m_audioBuffer.getNumChannels() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Auto Region Error",
            "No audio data to analyze. Please load an audio file first.");
        return false;
    }

    // Validate sample rate
    if (m_sampleRate <= 0.0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Auto Region Error",
            "Invalid sample rate. Cannot perform analysis.");
        return false;
    }

    // Check if min region length is reasonable
    double minRegionLengthMs = m_minRegionSlider.getValue();
    double minSilenceLengthMs = m_minSilenceSlider.getValue();

    if (minRegionLengthMs > minSilenceLengthMs)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Auto Region Error",
            "Min Region Length cannot be greater than Min Silence Length.\n\n"
            "This would make it impossible to detect separate regions.");
        return false;
    }

    // Warn if parameters might create too many regions
    double totalDurationMs = (m_audioBuffer.getNumSamples() / m_sampleRate) * 1000.0;
    if (minSilenceLengthMs < 50.0 && totalDurationMs > 60000.0)
    {
        // For now, just log a warning instead of blocking
        // In a full implementation, this would use an async dialog
        juce::Logger::writeToLog("Warning: Min Silence Length is very short (" +
                                  juce::String(minSilenceLengthMs, 1) +
                                  " ms). This may create many regions.");
    }

    return true;
}

void StripSilenceDialog::updatePreviewDisplay()
{
    int numRegions = m_previewRegions.size();

    // Update waveform preview with regions
    m_waveformPreview->setPreviewRegions(m_previewRegions);

    // Update region count label
    juce::String countText;
    if (numRegions == 0)
    {
        countText = "No regions will be created. Try adjusting parameters.";
    }
    else
    {
        countText = "Will create " + juce::String(numRegions) + " region";
        if (numRegions != 1)
            countText += "s";
    }

    m_regionCountLabel.setText(countText, juce::dontSendNotification);
}
