/*
  ==============================================================================

    ChannelConverterDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "ChannelConverterDialog.h"
#include "UIConstants.h"

namespace
{
    constexpr int DIALOG_WIDTH = 520;
    constexpr int DIALOG_HEIGHT = 580;  // Increased to accommodate upmix group fully
    constexpr int MARGIN = 20;
    constexpr int ROW_HEIGHT = 30;
    constexpr int LABEL_WIDTH = 100;
    constexpr int BUTTON_WIDTH = 90;
    constexpr int BUTTON_HEIGHT = 30;
    constexpr int FORMULA_HEIGHT = 150;
}

ChannelConverterDialog::ChannelConverterDialog(int currentChannels)
    : m_currentChannels(currentChannels)
{
    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);

    // Title
    m_titleLabel.setText("Channel Converter", juce::dontSendNotification);
    m_titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Current channels display
    m_currentLabel.setText("Current:", juce::dontSendNotification);
    m_currentLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_currentLabel);

    waveedit::ChannelLayout currentLayout = waveedit::ChannelLayout::fromChannelCount(currentChannels);
    m_currentValueLabel.setText(juce::String(currentChannels) + " channels (" +
                                 currentLayout.getLayoutName() + ")", juce::dontSendNotification);
    m_currentValueLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    addAndMakeVisible(m_currentValueLabel);

    // Target layout selector
    m_targetLabel.setText("Convert To:", juce::dontSendNotification);
    m_targetLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_targetLabel);

    m_targetCombo.onChange = [this]() { onPresetChanged(); };
    addAndMakeVisible(m_targetCombo);

    // Populate presets
    populatePresets();

    // Downmix options group
    m_downmixGroup.setText("Downmix Options");
    addAndMakeVisible(m_downmixGroup);
    m_downmixGroup.setVisible(false);

    m_downmixPresetLabel.setText("Preset:", juce::dontSendNotification);
    m_downmixPresetLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_downmixPresetLabel);
    m_downmixPresetLabel.setVisible(false);

    m_ituPresetButton.setButtonText("ITU Standard");
    m_ituPresetButton.setRadioGroupId(1);
    m_ituPresetButton.setToggleState(true, juce::dontSendNotification);
    m_ituPresetButton.onClick = [this]() { onDownmixPresetChanged(); };
    addAndMakeVisible(m_ituPresetButton);
    m_ituPresetButton.setVisible(false);

    m_professionalPresetButton.setButtonText("Professional");
    m_professionalPresetButton.setRadioGroupId(1);
    m_professionalPresetButton.onClick = [this]() { onDownmixPresetChanged(); };
    addAndMakeVisible(m_professionalPresetButton);
    m_professionalPresetButton.setVisible(false);

    m_customPresetButton.setButtonText("Film Fold-Down");
    m_customPresetButton.setRadioGroupId(1);
    m_customPresetButton.onClick = [this]() { onDownmixPresetChanged(); };
    addAndMakeVisible(m_customPresetButton);
    m_customPresetButton.setVisible(false);

    m_lfeLabel.setText("LFE:", juce::dontSendNotification);
    m_lfeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_lfeLabel);
    m_lfeLabel.setVisible(false);

    m_lfeExcludeButton.setButtonText("Exclude");
    m_lfeExcludeButton.setRadioGroupId(2);
    m_lfeExcludeButton.setToggleState(true, juce::dontSendNotification);
    m_lfeExcludeButton.onClick = [this]() { onLFEHandlingChanged(); };
    addAndMakeVisible(m_lfeExcludeButton);
    m_lfeExcludeButton.setVisible(false);

    m_lfeMinus3dBButton.setButtonText("-3dB");
    m_lfeMinus3dBButton.setRadioGroupId(2);
    m_lfeMinus3dBButton.onClick = [this]() { onLFEHandlingChanged(); };
    addAndMakeVisible(m_lfeMinus3dBButton);
    m_lfeMinus3dBButton.setVisible(false);

    m_lfeMinus6dBButton.setButtonText("-6dB");
    m_lfeMinus6dBButton.setRadioGroupId(2);
    m_lfeMinus6dBButton.onClick = [this]() { onLFEHandlingChanged(); };
    addAndMakeVisible(m_lfeMinus6dBButton);
    m_lfeMinus6dBButton.setVisible(false);

    // Upmix options group
    m_upmixGroup.setText("Upmix Strategy");
    addAndMakeVisible(m_upmixGroup);
    m_upmixGroup.setVisible(false);

    m_upmixStrategyLabel.setText("How to fill additional channels:", juce::dontSendNotification);
    m_upmixStrategyLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(m_upmixStrategyLabel);
    m_upmixStrategyLabel.setVisible(false);

    m_frontOnlyButton.setButtonText("Front Only (Recommended)");
    m_frontOnlyButton.setRadioGroupId(3);
    m_frontOnlyButton.setToggleState(true, juce::dontSendNotification);
    m_frontOnlyButton.onClick = [this]() { onUpmixStrategyChanged(); };
    addAndMakeVisible(m_frontOnlyButton);
    m_frontOnlyButton.setVisible(false);

    m_phantomCenterButton.setButtonText("Phantom Center");
    m_phantomCenterButton.setRadioGroupId(3);
    m_phantomCenterButton.onClick = [this]() { onUpmixStrategyChanged(); };
    addAndMakeVisible(m_phantomCenterButton);
    m_phantomCenterButton.setVisible(false);

    m_fullSurroundButton.setButtonText("Full Surround Derive");
    m_fullSurroundButton.setRadioGroupId(3);
    m_fullSurroundButton.onClick = [this]() { onUpmixStrategyChanged(); };
    addAndMakeVisible(m_fullSurroundButton);
    m_fullSurroundButton.setVisible(false);

    m_duplicateButton.setButtonText("Duplicate to All");
    m_duplicateButton.setRadioGroupId(3);
    m_duplicateButton.onClick = [this]() { onUpmixStrategyChanged(); };
    addAndMakeVisible(m_duplicateButton);
    m_duplicateButton.setVisible(false);

    // Mix preview
    m_formulaLabel.setText("How channels will be mixed:", juce::dontSendNotification);
    m_formulaLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(m_formulaLabel);

    m_formulaPreview.setMultiLine(true, true);
    m_formulaPreview.setReadOnly(true);
    m_formulaPreview.setScrollbarsShown(true);
    m_formulaPreview.setFont(juce::FontOptions(12.0f));
    m_formulaPreview.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xFF1E1E1E));
    m_formulaPreview.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    addAndMakeVisible(m_formulaPreview);

    // Info label
    m_infoLabel.setJustificationType(juce::Justification::centred);
    m_infoLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(m_infoLabel);

    // Buttons
    m_applyButton.setButtonText("Apply");
    m_applyButton.onClick = [this]() { onApplyClicked(); };
    addAndMakeVisible(m_applyButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    // Select first preset and trigger update
    if (m_targetCombo.getNumItems() > 0)
    {
        m_targetCombo.setSelectedId(1, juce::sendNotification);
    }
}

void ChannelConverterDialog::populatePresets()
{
    m_presets.clear();

    // Add standard presets
    m_presets.push_back({"Mono", 1, waveedit::ChannelLayoutType::Mono});
    m_presets.push_back({"Stereo", 2, waveedit::ChannelLayoutType::Stereo});
    m_presets.push_back({"3.0 (L-C-R)", 3, waveedit::ChannelLayoutType::LCR});
    m_presets.push_back({"4.0 Quad", 4, waveedit::ChannelLayoutType::Quad});
    m_presets.push_back({"5.0 Surround", 5, waveedit::ChannelLayoutType::Surround_5_0});
    m_presets.push_back({"5.1 Surround", 6, waveedit::ChannelLayoutType::Surround_5_1});
    m_presets.push_back({"6.1 Surround", 7, waveedit::ChannelLayoutType::Surround_6_1});
    m_presets.push_back({"7.1 Surround", 8, waveedit::ChannelLayoutType::Surround_7_1});

    // Populate combo box
    m_targetCombo.clear();
    int id = 1;
    for (const auto& preset : m_presets)
    {
        juce::String label = preset.name;
        if (preset.channels == m_currentChannels)
            label += " (Current)";
        m_targetCombo.addItem(label, id++);
    }
}

void ChannelConverterDialog::onPresetChanged()
{
    int selectedId = m_targetCombo.getSelectedId();
    if (selectedId <= 0 || selectedId > static_cast<int>(m_presets.size()))
        return;

    int targetChannels = m_presets[static_cast<size_t>(selectedId - 1)].channels;
    bool isUpmix = targetChannels > m_currentChannels;
    bool isDownmix = targetChannels < m_currentChannels;

    // Show/hide upmix vs downmix groups
    m_upmixGroup.setVisible(isUpmix);
    m_upmixStrategyLabel.setVisible(isUpmix);
    m_frontOnlyButton.setVisible(isUpmix);
    m_phantomCenterButton.setVisible(isUpmix);
    m_fullSurroundButton.setVisible(isUpmix);
    m_duplicateButton.setVisible(isUpmix);

    m_downmixGroup.setVisible(isDownmix);
    m_downmixPresetLabel.setVisible(isDownmix);
    m_ituPresetButton.setVisible(isDownmix);
    m_professionalPresetButton.setVisible(isDownmix);
    m_customPresetButton.setVisible(isDownmix);
    m_lfeLabel.setVisible(isDownmix);
    m_lfeExcludeButton.setVisible(isDownmix);
    m_lfeMinus3dBButton.setVisible(isDownmix);
    m_lfeMinus6dBButton.setVisible(isDownmix);

    updateInfoLabel();
    updateFormulaPreview();
    resized();
}

void ChannelConverterDialog::onDownmixPresetChanged()
{
    updateFormulaPreview();
}

void ChannelConverterDialog::onLFEHandlingChanged()
{
    updateFormulaPreview();
}

void ChannelConverterDialog::onUpmixStrategyChanged()
{
    updateFormulaPreview();
}

void ChannelConverterDialog::updateInfoLabel()
{
    int selectedIndex = m_targetCombo.getSelectedItemIndex();
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(m_presets.size()))
    {
        m_infoLabel.setText("", juce::dontSendNotification);
        return;
    }

    int targetChannels = m_presets[static_cast<size_t>(selectedIndex)].channels;

    if (targetChannels == m_currentChannels)
    {
        m_infoLabel.setText("No conversion needed - same channel count", juce::dontSendNotification);
    }
    else if (targetChannels < m_currentChannels)
    {
        m_infoLabel.setText("Downmix: " + juce::String(m_currentChannels) + " → " +
                           juce::String(targetChannels) + " channels", juce::dontSendNotification);
    }
    else
    {
        m_infoLabel.setText("Upmix: " + juce::String(m_currentChannels) + " → " +
                           juce::String(targetChannels) + " channels", juce::dontSendNotification);
    }
}

void ChannelConverterDialog::updateFormulaPreview()
{
    int selectedIndex = m_targetCombo.getSelectedItemIndex();
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(m_presets.size()))
    {
        m_formulaPreview.setText("", juce::dontSendNotification);
        return;
    }

    int targetChannels = m_presets[static_cast<size_t>(selectedIndex)].channels;

    // Bullet point for list items
    const juce::String bullet = juce::String::charToString(0x2022);

    // Handle upmixing
    if (targetChannels > m_currentChannels)
    {
        juce::String description;

        if (m_frontOnlyButton.getToggleState())
        {
            description = "FRONT ONLY STRATEGY:\n\n";
            description += "  " + bullet + " Left channel copied to Front Left\n";
            description += "  " + bullet + " Right channel copied to Front Right\n";
            description += "  " + bullet + " All other channels: Silent\n";
            description += "\nBest for: Preserving original stereo image";
        }
        else if (m_phantomCenterButton.getToggleState())
        {
            description = "PHANTOM CENTER STRATEGY:\n\n";
            description += "  " + bullet + " Left channel to Front Left at full volume\n";
            description += "  " + bullet + " Right channel to Front Right at full volume\n";
            description += "  " + bullet + " Center derived from (L+R) at -3dB\n";
            description += "  " + bullet + " LFE: Silent\n";
            description += "  " + bullet + " Surrounds: Silent\n";
            description += "\nBest for: Wider stereo image with dialog enhancement";
        }
        else if (m_fullSurroundButton.getToggleState())
        {
            description = "FULL SURROUND DERIVE STRATEGY:\n\n";
            description += "  " + bullet + " Left channel to Front Left at full volume\n";
            description += "  " + bullet + " Right channel to Front Right at full volume\n";
            description += "  " + bullet + " Center derived from (L+R) at -3dB\n";
            description += "  " + bullet + " LFE: Silent\n";
            description += "  " + bullet + " Left Surround derived from Left at -6dB\n";
            description += "  " + bullet + " Right Surround derived from Right at -6dB\n";
            description += "\nBest for: Immersive surround sound effect";
        }
        else if (m_duplicateButton.getToggleState())
        {
            description = "DUPLICATE STRATEGY:\n\n";
            description += "  " + bullet + " Left signal sent to all left-side speakers\n";
            description += "  " + bullet + " Right signal sent to all right-side speakers\n";
            description += "  " + bullet + " Center and LFE derived from (L+R)\n";
            description += "\nNote: May sound overwhelming in surround setups";
        }

        m_formulaPreview.setText(description, juce::dontSendNotification);
        return;
    }

    // No conversion needed
    if (targetChannels == m_currentChannels)
    {
        m_formulaPreview.setText("No conversion needed - channel counts match.", juce::dontSendNotification);
        return;
    }

    // Downmix case
    juce::String surroundLevel = m_professionalPresetButton.getToggleState() ? "-6dB" : "-3dB";
    juce::String lfeDesc = "Excluded";

    if (m_lfeMinus3dBButton.getToggleState())
        lfeDesc = "Included at -3dB";
    else if (m_lfeMinus6dBButton.getToggleState())
        lfeDesc = "Included at -6dB";
    else if (m_customPresetButton.getToggleState())
        lfeDesc = "Included at -6dB";

    waveedit::ChannelLayout srcLayout = waveedit::ChannelLayout::fromChannelCount(m_currentChannels);
    juce::String description;

    if (targetChannels == 2)
    {
        juce::String leftDesc = "LEFT OUTPUT:\n";
        juce::String rightDesc = "RIGHT OUTPUT:\n";

        for (int ch = 0; ch < m_currentChannels; ++ch)
        {
            const auto& info = srcLayout.getChannelInfo(ch);
            juce::String channelName = info.fullName.isEmpty() ? info.shortLabel : info.fullName;

            if (info.speakerPosition == waveedit::SpeakerPosition::FRONT_LEFT)
            {
                leftDesc += "  " + bullet + " " + channelName + " at full volume\n";
            }
            else if (info.speakerPosition == waveedit::SpeakerPosition::FRONT_RIGHT)
            {
                rightDesc += "  " + bullet + " " + channelName + " at full volume\n";
            }
            else if (info.speakerPosition == waveedit::SpeakerPosition::FRONT_CENTER)
            {
                leftDesc += "  " + bullet + " " + channelName + " at -3dB\n";
                rightDesc += "  " + bullet + " " + channelName + " at -3dB\n";
            }
            else if (info.speakerPosition == waveedit::SpeakerPosition::BACK_LEFT ||
                     info.speakerPosition == waveedit::SpeakerPosition::SIDE_LEFT)
            {
                leftDesc += "  " + bullet + " " + channelName + " at " + surroundLevel + "\n";
            }
            else if (info.speakerPosition == waveedit::SpeakerPosition::BACK_RIGHT ||
                     info.speakerPosition == waveedit::SpeakerPosition::SIDE_RIGHT)
            {
                rightDesc += "  " + bullet + " " + channelName + " at " + surroundLevel + "\n";
            }
            else if (info.speakerPosition == waveedit::SpeakerPosition::LOW_FREQUENCY)
            {
                if (!m_lfeExcludeButton.getToggleState())
                {
                    juce::String lfeLevel = m_lfeMinus3dBButton.getToggleState() ? "-3dB" : "-6dB";
                    leftDesc += "  " + bullet + " " + channelName + " at " + lfeLevel + "\n";
                    rightDesc += "  " + bullet + " " + channelName + " at " + lfeLevel + "\n";
                }
            }
        }

        description = leftDesc + "\n" + rightDesc;
        description += "\nLFE: " + lfeDesc;
    }
    else if (targetChannels == 1)
    {
        description = "MONO OUTPUT:\n";

        for (int ch = 0; ch < m_currentChannels; ++ch)
        {
            const auto& info = srcLayout.getChannelInfo(ch);
            juce::String channelName = info.fullName.isEmpty() ? info.shortLabel : info.fullName;

            if (info.speakerPosition == waveedit::SpeakerPosition::FRONT_LEFT ||
                info.speakerPosition == waveedit::SpeakerPosition::FRONT_RIGHT)
            {
                description += "  " + bullet + " " + channelName + " at -3dB\n";
            }
            else if (info.speakerPosition == waveedit::SpeakerPosition::FRONT_CENTER)
            {
                description += "  " + bullet + " " + channelName + " at full volume\n";
            }
            else if (info.speakerPosition == waveedit::SpeakerPosition::BACK_LEFT ||
                     info.speakerPosition == waveedit::SpeakerPosition::BACK_RIGHT ||
                     info.speakerPosition == waveedit::SpeakerPosition::SIDE_LEFT ||
                     info.speakerPosition == waveedit::SpeakerPosition::SIDE_RIGHT)
            {
                description += "  " + bullet + " " + channelName + " at -6dB\n";
            }
            else if (info.speakerPosition == waveedit::SpeakerPosition::LOW_FREQUENCY)
            {
                if (!m_lfeExcludeButton.getToggleState())
                {
                    juce::String lfeLevel = m_lfeMinus3dBButton.getToggleState() ? "-3dB" : "-6dB";
                    description += "  " + bullet + " " + channelName + " at " + lfeLevel + "\n";
                }
            }
        }

        description += "\nLFE: " + lfeDesc;
    }

    m_formulaPreview.setText(description, juce::dontSendNotification);
}

void ChannelConverterDialog::onApplyClicked()
{
    int selectedIndex = m_targetCombo.getSelectedItemIndex();
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(m_presets.size()))
    {
        const auto& preset = m_presets[static_cast<size_t>(selectedIndex)];

        // Determine downmix preset
        waveedit::DownmixPreset downmixPreset = waveedit::DownmixPreset::ITU_Standard;
        if (m_professionalPresetButton.getToggleState())
            downmixPreset = waveedit::DownmixPreset::Professional;
        else if (m_customPresetButton.getToggleState())
            downmixPreset = waveedit::DownmixPreset::FilmFoldDown;

        // Determine LFE handling
        waveedit::LFEHandling lfeHandling = waveedit::LFEHandling::Exclude;
        if (m_lfeMinus3dBButton.getToggleState())
            lfeHandling = waveedit::LFEHandling::IncludeMinus3dB;
        else if (m_lfeMinus6dBButton.getToggleState())
            lfeHandling = waveedit::LFEHandling::IncludeMinus6dB;

        // Determine upmix strategy
        waveedit::UpmixStrategy upmixStrategy = waveedit::UpmixStrategy::FrontOnly;
        if (m_phantomCenterButton.getToggleState())
            upmixStrategy = waveedit::UpmixStrategy::PhantomCenter;
        else if (m_fullSurroundButton.getToggleState())
            upmixStrategy = waveedit::UpmixStrategy::FullSurround;
        else if (m_duplicateButton.getToggleState())
            upmixStrategy = waveedit::UpmixStrategy::Duplicate;

        m_result = Result{
            preset.channels,
            preset.layout,
            downmixPreset,
            lfeHandling,
            upmixStrategy
        };
    }

    if (auto* dlg = findParentComponentOfClass<juce::DialogWindow>())
        dlg->exitModalState(1);
}

void ChannelConverterDialog::onCancelClicked()
{
    m_result = std::nullopt;

    if (auto* dlg = findParentComponentOfClass<juce::DialogWindow>())
        dlg->exitModalState(0);
}

void ChannelConverterDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(waveedit::ui::kBackgroundPrimary));
}

void ChannelConverterDialog::resized()
{
    auto bounds = getLocalBounds().reduced(MARGIN);

    // Title
    m_titleLabel.setBounds(bounds.removeFromTop(35));
    bounds.removeFromTop(5);

    // Current channels row
    auto currentRow = bounds.removeFromTop(ROW_HEIGHT);
    m_currentLabel.setBounds(currentRow.removeFromLeft(LABEL_WIDTH));
    currentRow.removeFromLeft(10);
    m_currentValueLabel.setBounds(currentRow);
    bounds.removeFromTop(8);

    // Target row
    auto targetRow = bounds.removeFromTop(ROW_HEIGHT);
    m_targetLabel.setBounds(targetRow.removeFromLeft(LABEL_WIDTH));
    targetRow.removeFromLeft(10);
    m_targetCombo.setBounds(targetRow);
    bounds.removeFromTop(15);

    // Downmix options group (only visible when downmixing)
    if (m_downmixGroup.isVisible())
    {
        int groupHeight = ROW_HEIGHT + 5 + ROW_HEIGHT + 15 + 22 + FORMULA_HEIGHT + 20;
        auto groupBounds = bounds.removeFromTop(groupHeight);
        m_downmixGroup.setBounds(groupBounds);

        auto groupContent = groupBounds.reduced(15, 25);

        // Preset row
        auto presetRow = groupContent.removeFromTop(ROW_HEIGHT);
        m_downmixPresetLabel.setBounds(presetRow.removeFromLeft(60));
        presetRow.removeFromLeft(10);
        m_ituPresetButton.setBounds(presetRow.removeFromLeft(110));
        presetRow.removeFromLeft(5);
        m_professionalPresetButton.setBounds(presetRow.removeFromLeft(110));
        presetRow.removeFromLeft(5);
        m_customPresetButton.setBounds(presetRow.removeFromLeft(120));
        groupContent.removeFromTop(8);

        // LFE row
        auto lfeRow = groupContent.removeFromTop(ROW_HEIGHT);
        m_lfeLabel.setBounds(lfeRow.removeFromLeft(60));
        lfeRow.removeFromLeft(10);
        m_lfeExcludeButton.setBounds(lfeRow.removeFromLeft(90));
        lfeRow.removeFromLeft(5);
        m_lfeMinus3dBButton.setBounds(lfeRow.removeFromLeft(70));
        lfeRow.removeFromLeft(5);
        m_lfeMinus6dBButton.setBounds(lfeRow.removeFromLeft(70));
        groupContent.removeFromTop(15);

        // Mix description preview
        m_formulaLabel.setBounds(groupContent.removeFromTop(22));
        m_formulaPreview.setBounds(groupContent.removeFromTop(FORMULA_HEIGHT));

        bounds.removeFromTop(10);
    }

    // Upmix options group (only visible when upmixing)
    if (m_upmixGroup.isVisible())
    {
        int groupHeight = 25 + 4 * 25 + 15 + 22 + FORMULA_HEIGHT + 20;
        auto groupBounds = bounds.removeFromTop(groupHeight);
        m_upmixGroup.setBounds(groupBounds);

        auto groupContent = groupBounds.reduced(15, 25);

        // Strategy label
        m_upmixStrategyLabel.setBounds(groupContent.removeFromTop(22));
        groupContent.removeFromTop(5);

        // Strategy buttons (stacked vertically)
        m_frontOnlyButton.setBounds(groupContent.removeFromTop(25));
        m_phantomCenterButton.setBounds(groupContent.removeFromTop(25));
        m_fullSurroundButton.setBounds(groupContent.removeFromTop(25));
        m_duplicateButton.setBounds(groupContent.removeFromTop(25));
        groupContent.removeFromTop(10);

        // Mix description preview
        m_formulaLabel.setBounds(groupContent.removeFromTop(22));
        m_formulaPreview.setBounds(groupContent.removeFromTop(FORMULA_HEIGHT));

        bounds.removeFromTop(10);
    }

    // If neither group is visible (same channel count), show formula preview standalone
    if (!m_downmixGroup.isVisible() && !m_upmixGroup.isVisible())
    {
        m_formulaLabel.setBounds(bounds.removeFromTop(22));
        bounds.removeFromTop(5);
        m_formulaPreview.setBounds(bounds.removeFromTop(FORMULA_HEIGHT));
        bounds.removeFromTop(10);
    }

    // Info label
    m_infoLabel.setBounds(bounds.removeFromTop(35));

    // Buttons at bottom
    auto buttonRow = bounds.removeFromBottom(BUTTON_HEIGHT);
    m_applyButton.setBounds(buttonRow.removeFromRight(BUTTON_WIDTH));
    buttonRow.removeFromRight(10);
    m_cancelButton.setBounds(buttonRow.removeFromRight(BUTTON_WIDTH));
}

std::optional<ChannelConverterDialog::Result> ChannelConverterDialog::showDialog(int currentChannels)
{
    // Use stack allocation to avoid use-after-free bug
    // setOwned() would delete the dialog when runModal() returns,
    // making dialog->m_result access freed memory
    ChannelConverterDialog dialog(currentChannels);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Channel Converter";
    options.dialogBackgroundColour = juce::Colour(waveedit::ui::kBackgroundPrimary);
    options.content.setNonOwned(&dialog);  // Use setNonOwned for stack object
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

#if JUCE_MODAL_LOOPS_PERMITTED
    options.runModal();
    return dialog.m_result;
#else
    jassertfalse; // Modal loops required for this dialog
    return std::nullopt;
#endif
}
