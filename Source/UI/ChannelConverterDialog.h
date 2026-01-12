/*
  ==============================================================================

    ChannelConverterDialog.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>
#include "../Audio/ChannelLayout.h"

/**
 * Modal dialog for converting audio channel count.
 *
 * Provides a preset-based interface for common channel layouts:
 * - Mono (1 channel)
 * - Stereo (2 channels)
 * - 5.1 Surround (6 channels)
 * - 7.1 Surround (8 channels)
 *
 * Displays current and target channel counts with layout names.
 * Shows warning when downmixing (potential quality loss).
 */
class ChannelConverterDialog : public juce::Component
{
public:
    /**
     * Result structure containing conversion parameters.
     */
    struct Result
    {
        int targetChannels;
        waveedit::ChannelLayoutType targetLayout;
    };

    /**
     * Creates a ChannelConverterDialog.
     *
     * @param currentChannels Current number of channels in the audio
     */
    explicit ChannelConverterDialog(int currentChannels);
    ~ChannelConverterDialog() override = default;

    /**
     * Show the dialog modally and return the conversion settings.
     *
     * @param currentChannels Current number of channels
     * @return std::optional<Result> containing target channels if Apply clicked,
     *         std::nullopt if user cancelled
     */
    static std::optional<Result> showDialog(int currentChannels);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void populatePresets();
    void onPresetChanged();
    void onApplyClicked();
    void onCancelClicked();
    void updateInfoLabel();

    // UI Components
    juce::Label m_titleLabel;
    juce::Label m_currentLabel;
    juce::Label m_currentValueLabel;
    juce::Label m_targetLabel;
    juce::ComboBox m_targetCombo;
    juce::Label m_infoLabel;        // Shows warnings/info about conversion
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;

    // State
    int m_currentChannels;
    std::optional<Result> m_result;

    // Preset data
    struct PresetInfo
    {
        juce::String name;
        int channels;
        waveedit::ChannelLayoutType layout;
    };
    std::vector<PresetInfo> m_presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelConverterDialog)
};
