/*
  ==============================================================================

    NewFileDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "NewFileDialog.h"

NewFileDialog::NewFileDialog()
    : m_titleLabel("titleLabel", "New Audio File"),
      m_sampleRateLabel("sampleRateLabel", "Sample Rate:"),
      m_channelsLabel("channelsLabel", "Channels:"),
      m_bitDepthLabel("bitDepthLabel", "Bit Depth:"),
      m_durationLabel("durationLabel", "Duration:"),
      m_durationUnitLabel("durationUnitLabel", "seconds"),
      m_samplesLabel("samplesLabel", "Total Samples:"),
      m_samplesValueLabel("samplesValueLabel", "0"),
      m_createButton("Create"),
      m_cancelButton("Cancel")
{
    // Title label
    m_titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Sample rate combo
    m_sampleRateLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_sampleRateLabel);

    m_sampleRateCombo.addItem("44100 Hz", 1);
    m_sampleRateCombo.addItem("48000 Hz", 2);
    m_sampleRateCombo.addItem("88200 Hz", 3);
    m_sampleRateCombo.addItem("96000 Hz", 4);
    m_sampleRateCombo.addItem("176400 Hz", 5);
    m_sampleRateCombo.addItem("192000 Hz", 6);
    m_sampleRateCombo.setSelectedId(2);  // Default: 48000 Hz
    m_sampleRateCombo.onChange = [this]() { updateSamplesFromDuration(); };
    addAndMakeVisible(m_sampleRateCombo);

    // Channels combo
    m_channelsLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_channelsLabel);

    m_channelsCombo.addItem("Mono", 1);
    m_channelsCombo.addItem("Stereo", 2);
    m_channelsCombo.setSelectedId(2);  // Default: Stereo
    addAndMakeVisible(m_channelsCombo);

    // Bit depth combo
    m_bitDepthLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_bitDepthLabel);

    m_bitDepthCombo.addItem("16-bit", 1);
    m_bitDepthCombo.addItem("24-bit", 2);
    m_bitDepthCombo.addItem("32-bit float", 3);
    m_bitDepthCombo.setSelectedId(2);  // Default: 24-bit
    addAndMakeVisible(m_bitDepthCombo);

    // Duration input
    m_durationLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_durationLabel);

    m_durationInput.setInputRestrictions(0, "0123456789.");
    m_durationInput.setJustification(juce::Justification::centredLeft);
    m_durationInput.setText("10.0");  // Default: 10 seconds
    m_durationInput.setSelectAllWhenFocused(true);
    m_durationInput.onReturnKey = [this]() { onCreateClicked(); };
    m_durationInput.onEscapeKey = [this]() { onCancelClicked(); };
    m_durationInput.onTextChange = [this]() { updateSamplesFromDuration(); };
    addAndMakeVisible(m_durationInput);

    m_durationUnitLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_durationUnitLabel);

    // Samples display
    m_samplesLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_samplesLabel);

    m_samplesValueLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_samplesValueLabel);

    // Create button
    m_createButton.onClick = [this]() { onCreateClicked(); };
    addAndMakeVisible(m_createButton);

    // Cancel button
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    // Initial calculation
    updateSamplesFromDuration();

    setSize(400, 340);
}

std::optional<NewFileSettings> NewFileDialog::showDialog()
{
    NewFileDialog dialog;

    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(&dialog);
    options.dialogTitle = "New Audio File";
    options.dialogBackgroundColour = juce::Colour(0xff2b2b2b);
    options.escapeKeyTriggersCloseButton = false;
    options.useNativeTitleBar = false;
    options.resizable = false;
    options.componentToCentreAround = nullptr;

    #if JUCE_MODAL_LOOPS_PERMITTED
        int result = options.runModal();
        return (result == 1) ? dialog.m_result : std::nullopt;
    #else
        jassertfalse; // Modal loops required for this dialog
        return std::nullopt;
    #endif
}

void NewFileDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
}

void NewFileDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // Title
    m_titleLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(15);

    const int rowHeight = 28;
    const int labelWidth = 100;
    const int spacing = 10;

    // Sample rate row
    auto row = bounds.removeFromTop(rowHeight);
    m_sampleRateLabel.setBounds(row.removeFromLeft(labelWidth));
    row.removeFromLeft(spacing);
    m_sampleRateCombo.setBounds(row.removeFromLeft(150));
    bounds.removeFromTop(spacing);

    // Channels row
    row = bounds.removeFromTop(rowHeight);
    m_channelsLabel.setBounds(row.removeFromLeft(labelWidth));
    row.removeFromLeft(spacing);
    m_channelsCombo.setBounds(row.removeFromLeft(150));
    bounds.removeFromTop(spacing);

    // Bit depth row
    row = bounds.removeFromTop(rowHeight);
    m_bitDepthLabel.setBounds(row.removeFromLeft(labelWidth));
    row.removeFromLeft(spacing);
    m_bitDepthCombo.setBounds(row.removeFromLeft(150));
    bounds.removeFromTop(spacing);

    // Duration row
    row = bounds.removeFromTop(rowHeight);
    m_durationLabel.setBounds(row.removeFromLeft(labelWidth));
    row.removeFromLeft(spacing);
    m_durationInput.setBounds(row.removeFromLeft(100));
    row.removeFromLeft(5);
    m_durationUnitLabel.setBounds(row.removeFromLeft(60));
    bounds.removeFromTop(spacing);

    // Samples row
    row = bounds.removeFromTop(rowHeight);
    m_samplesLabel.setBounds(row.removeFromLeft(labelWidth));
    row.removeFromLeft(spacing);
    m_samplesValueLabel.setBounds(row);
    bounds.removeFromTop(spacing + 10);

    // Buttons row
    row = bounds.removeFromTop(35);
    auto buttonWidth = 100;
    auto totalButtonWidth = buttonWidth * 2 + spacing;
    row.removeFromLeft((row.getWidth() - totalButtonWidth) / 2);
    m_createButton.setBounds(row.removeFromLeft(buttonWidth));
    row.removeFromLeft(spacing);
    m_cancelButton.setBounds(row.removeFromLeft(buttonWidth));
}

void NewFileDialog::onCreateClicked()
{
    // Get sample rate from combo selection
    double sampleRate = 48000.0;
    switch (m_sampleRateCombo.getSelectedId())
    {
        case 1: sampleRate = 44100.0; break;
        case 2: sampleRate = 48000.0; break;
        case 3: sampleRate = 88200.0; break;
        case 4: sampleRate = 96000.0; break;
        case 5: sampleRate = 176400.0; break;
        case 6: sampleRate = 192000.0; break;
    }

    // Get channels from combo selection
    int numChannels = m_channelsCombo.getSelectedId();  // 1 = mono, 2 = stereo

    // Get bit depth from combo selection
    int bitDepth = 24;
    switch (m_bitDepthCombo.getSelectedId())
    {
        case 1: bitDepth = 16; break;
        case 2: bitDepth = 24; break;
        case 3: bitDepth = 32; break;
    }

    // Get duration from text input
    double durationSeconds = m_durationInput.getText().getDoubleValue();
    if (durationSeconds <= 0.0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Invalid Duration",
            "Duration must be greater than 0 seconds.");
        return;
    }

    // Limit duration to reasonable maximum (10 hours)
    const double maxDuration = 36000.0;
    if (durationSeconds > maxDuration)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Duration Too Long",
            "Duration cannot exceed 10 hours.");
        return;
    }

    m_result = NewFileSettings{sampleRate, numChannels, durationSeconds, bitDepth};

    if (auto* dlgWindow = findParentComponentOfClass<juce::DialogWindow>())
    {
        dlgWindow->exitModalState(1);  // 1 = Create clicked
    }
}

void NewFileDialog::onCancelClicked()
{
    if (auto* dlgWindow = findParentComponentOfClass<juce::DialogWindow>())
    {
        dlgWindow->exitModalState(0);  // 0 = Cancel
    }
}

void NewFileDialog::updateSamplesFromDuration()
{
    double sampleRate = 48000.0;
    switch (m_sampleRateCombo.getSelectedId())
    {
        case 1: sampleRate = 44100.0; break;
        case 2: sampleRate = 48000.0; break;
        case 3: sampleRate = 88200.0; break;
        case 4: sampleRate = 96000.0; break;
        case 5: sampleRate = 176400.0; break;
        case 6: sampleRate = 192000.0; break;
    }

    double durationSeconds = m_durationInput.getText().getDoubleValue();
    if (durationSeconds > 0.0)
    {
        int64_t totalSamples = static_cast<int64_t>(durationSeconds * sampleRate);
        m_samplesValueLabel.setText(juce::String(totalSamples), juce::dontSendNotification);
    }
    else
    {
        m_samplesValueLabel.setText("0", juce::dontSendNotification);
    }
}

void NewFileDialog::updateDurationFromSamples()
{
    // Not used in current implementation, but available for future use
}
