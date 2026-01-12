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
    constexpr int DIALOG_WIDTH = 400;
    constexpr int DIALOG_HEIGHT = 280;
    constexpr int MARGIN = 20;
    constexpr int ROW_HEIGHT = 30;
    constexpr int LABEL_WIDTH = 120;
    constexpr int BUTTON_WIDTH = 90;
    constexpr int BUTTON_HEIGHT = 30;
}

ChannelConverterDialog::ChannelConverterDialog(int currentChannels)
    : m_currentChannels(currentChannels)
{
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
    m_currentValueLabel.setText(juce::String(currentChannels) + " (" + currentLayout.getLayoutName() + ")",
                                 juce::dontSendNotification);
    m_currentValueLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(m_currentValueLabel);

    // Target preset selector
    m_targetLabel.setText("Convert to:", juce::dontSendNotification);
    m_targetLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_targetLabel);

    m_targetCombo.setTooltip("Select target channel layout");
    m_targetCombo.onChange = [this]() { onPresetChanged(); };
    addAndMakeVisible(m_targetCombo);

    // Info label for warnings
    m_infoLabel.setFont(juce::Font(12.0f));
    m_infoLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_infoLabel);

    // Buttons
    m_applyButton.setButtonText("Apply");
    m_applyButton.onClick = [this]() { onApplyClicked(); };
    addAndMakeVisible(m_applyButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    // Populate presets
    populatePresets();

    // Select a default that's different from current
    if (m_targetCombo.getNumItems() > 0)
    {
        // Try to select stereo by default if not already stereo
        for (int i = 0; i < m_targetCombo.getNumItems(); ++i)
        {
            if (m_presets[static_cast<size_t>(i)].channels != m_currentChannels)
            {
                m_targetCombo.setSelectedItemIndex(i);
                break;
            }
        }
    }

    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);
}

void ChannelConverterDialog::populatePresets()
{
    m_presets.clear();
    m_targetCombo.clear();

    // Standard presets
    m_presets.push_back({ "Mono (1 channel)", 1, waveedit::ChannelLayoutType::Mono });
    m_presets.push_back({ "Stereo (2 channels)", 2, waveedit::ChannelLayoutType::Stereo });
    m_presets.push_back({ "LCR (3 channels)", 3, waveedit::ChannelLayoutType::LCR });
    m_presets.push_back({ "Quad (4 channels)", 4, waveedit::ChannelLayoutType::Quad });
    m_presets.push_back({ "5.0 Surround (5 channels)", 5, waveedit::ChannelLayoutType::Surround_5_0 });
    m_presets.push_back({ "5.1 Surround (6 channels)", 6, waveedit::ChannelLayoutType::Surround_5_1 });
    m_presets.push_back({ "6.1 Surround (7 channels)", 7, waveedit::ChannelLayoutType::Surround_6_1 });
    m_presets.push_back({ "7.1 Surround (8 channels)", 8, waveedit::ChannelLayoutType::Surround_7_1 });

    int itemId = 1;
    for (const auto& preset : m_presets)
    {
        m_targetCombo.addItem(preset.name, itemId++);
    }
}

void ChannelConverterDialog::onPresetChanged()
{
    updateInfoLabel();
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
        m_infoLabel.setText("No conversion needed (already at target)", juce::dontSendNotification);
        m_infoLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        m_applyButton.setEnabled(false);
    }
    else if (targetChannels < m_currentChannels)
    {
        // Downmix warning
        m_infoLabel.setText("Downmixing will combine channels (irreversible)", juce::dontSendNotification);
        m_infoLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        m_applyButton.setEnabled(true);
    }
    else
    {
        // Upmix info
        m_infoLabel.setText("Upmixing will derive channels from existing audio", juce::dontSendNotification);
        m_infoLabel.setColour(juce::Label::textColourId, juce::Colour(waveedit::ui::kTextSecondary));
        m_applyButton.setEnabled(true);
    }
}

void ChannelConverterDialog::onApplyClicked()
{
    int selectedIndex = m_targetCombo.getSelectedItemIndex();
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(m_presets.size()))
    {
        const auto& preset = m_presets[static_cast<size_t>(selectedIndex)];
        m_result = Result{ preset.channels, preset.layout };
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
    m_titleLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(10);

    // Current channels row
    auto currentRow = bounds.removeFromTop(ROW_HEIGHT);
    m_currentLabel.setBounds(currentRow.removeFromLeft(LABEL_WIDTH));
    currentRow.removeFromLeft(10);
    m_currentValueLabel.setBounds(currentRow);
    bounds.removeFromTop(10);

    // Target row
    auto targetRow = bounds.removeFromTop(ROW_HEIGHT);
    m_targetLabel.setBounds(targetRow.removeFromLeft(LABEL_WIDTH));
    targetRow.removeFromLeft(10);
    m_targetCombo.setBounds(targetRow);
    bounds.removeFromTop(20);

    // Info label
    m_infoLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(10);

    // Buttons at bottom
    auto buttonRow = bounds.removeFromBottom(BUTTON_HEIGHT);
    m_applyButton.setBounds(buttonRow.removeFromRight(BUTTON_WIDTH));
    buttonRow.removeFromRight(10);
    m_cancelButton.setBounds(buttonRow.removeFromRight(BUTTON_WIDTH));
}

std::optional<ChannelConverterDialog::Result> ChannelConverterDialog::showDialog(int currentChannels)
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Channel Converter";
    options.dialogBackgroundColour = juce::Colour(waveedit::ui::kBackgroundPrimary);

    auto* dialog = new ChannelConverterDialog(currentChannels);
    options.content.setOwned(dialog);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    std::unique_ptr<juce::DialogWindow> dlg(options.create());
    dlg->centreWithSize(DIALOG_WIDTH, DIALOG_HEIGHT);

#if JUCE_MODAL_LOOPS_PERMITTED
    int result = dlg->runModalLoop();
    if (result == 1)
        return dialog->m_result;
#endif

    return std::nullopt;
}
