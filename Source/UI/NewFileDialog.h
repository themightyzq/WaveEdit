/*
  ==============================================================================

    NewFileDialog.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

/**
 * Result structure for NewFileDialog.
 * Contains all parameters needed to create a new audio document.
 */
struct NewFileSettings
{
    double sampleRate;      // Sample rate in Hz (e.g., 44100, 48000, 96000)
    int numChannels;        // Number of channels (1 = mono, 2 = stereo)
    double durationSeconds; // Duration in seconds
    int bitDepth;           // Bit depth (16, 24, 32)
};

/**
 * Modal dialog for creating a new audio file with custom settings.
 *
 * Allows user to specify:
 * - Sample rate (44100, 48000, 88200, 96000, 176400, 192000 Hz)
 * - Channels (Mono, Stereo)
 * - Duration (in seconds, or samples)
 * - Bit depth (16, 24, 32-bit float)
 *
 * Thread Safety: UI thread only. Must be shown from message thread.
 * The showDialog() method blocks until the user dismisses the dialog.
 */
class NewFileDialog : public juce::Component
{
public:
    NewFileDialog();
    ~NewFileDialog() override = default;

    /**
     * Show the dialog modally and return the user's settings.
     *
     * @return std::optional<NewFileSettings> containing settings if user clicked Create,
     *         std::nullopt if user clicked Cancel or closed the dialog
     */
    static std::optional<NewFileSettings> showDialog();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void onCreateClicked();
    void onCancelClicked();
    void updateDurationFromSamples();
    void updateSamplesFromDuration();

    // UI Components
    juce::Label m_titleLabel;

    juce::Label m_sampleRateLabel;
    juce::ComboBox m_sampleRateCombo;

    juce::Label m_channelsLabel;
    juce::ComboBox m_channelsCombo;

    juce::Label m_bitDepthLabel;
    juce::ComboBox m_bitDepthCombo;

    juce::Label m_durationLabel;
    juce::TextEditor m_durationInput;
    juce::Label m_durationUnitLabel;

    juce::Label m_samplesLabel;
    juce::Label m_samplesValueLabel;

    juce::TextButton m_createButton;
    juce::TextButton m_cancelButton;

    // State
    std::optional<NewFileSettings> m_result;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NewFileDialog)
};
