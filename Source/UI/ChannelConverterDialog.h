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
 * Modal dialog for converting audio channel count (downmix/upmix).
 *
 * Features:
 * - Preset-based layout selection (Mono, Stereo, 5.1, 7.1, etc.)
 * - Downmix presets: ITU Standard, Professional, Film Fold-Down
 * - LFE handling options: Exclude, Include at -3dB, Include at -6dB
 * - Upmix strategies: Front Only, Phantom Center, Full Surround, Duplicate
 * - Plain language preview showing how channels will be mixed
 *
 * Uses ITU-R BS.775 standard coefficients for professional downmixing.
 *
 * For extracting channels to separate files, use ChannelExtractorDialog.
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
        waveedit::DownmixPreset downmixPreset;
        waveedit::LFEHandling lfeHandling;
        waveedit::UpmixStrategy upmixStrategy;
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
    void onDownmixPresetChanged();
    void onLFEHandlingChanged();
    void onUpmixStrategyChanged();
    void onApplyClicked();
    void onCancelClicked();
    void updateInfoLabel();
    void updateFormulaPreview();

    // UI Components - Header
    juce::Label m_titleLabel;
    juce::Label m_currentLabel;
    juce::Label m_currentValueLabel;

    // Target layout selector
    juce::Label m_targetLabel;
    juce::ComboBox m_targetCombo;

    // Downmix options (visible when downmixing)
    juce::GroupComponent m_downmixGroup;
    juce::Label m_downmixPresetLabel;
    juce::ToggleButton m_ituPresetButton;
    juce::ToggleButton m_professionalPresetButton;
    juce::ToggleButton m_customPresetButton;

    juce::Label m_lfeLabel;
    juce::ToggleButton m_lfeExcludeButton;
    juce::ToggleButton m_lfeMinus3dBButton;
    juce::ToggleButton m_lfeMinus6dBButton;

    // Upmix options (visible when upmixing)
    juce::GroupComponent m_upmixGroup;
    juce::Label m_upmixStrategyLabel;
    juce::ToggleButton m_frontOnlyButton;
    juce::ToggleButton m_phantomCenterButton;
    juce::ToggleButton m_fullSurroundButton;
    juce::ToggleButton m_duplicateButton;

    // Mix preview
    juce::Label m_formulaLabel;
    juce::TextEditor m_formulaPreview;

    // Info and buttons
    juce::Label m_infoLabel;
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
