/*
  ==============================================================================

    SettingsPanel.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "ShortcutEditorPanel.h"

/**
 * Settings/Preferences panel for WaveEdit.
 *
 * Provides GUI for configuring:
 * - Audio settings (device, buffer size, sample rate)
 * - Display settings (waveform colors, theme)
 * - Auto-save settings (enable/disable, interval)
 * - General settings (startup behavior)
 *
 * Accessed via Cmd+, (macOS) or Ctrl+, (Windows/Linux)
 */
class SettingsPanel : public juce::Component,
                      public juce::Button::Listener,
                      public juce::ComboBox::Listener
{
public:
    /**
     * Constructor.
     *
     * @param deviceManager Reference to the application's AudioDeviceManager
     * @param commandManager Reference to the application's ApplicationCommandManager
     * @param keymapManager Reference to the application's KeymapManager (Phase 3)
     */
    explicit SettingsPanel(juce::AudioDeviceManager& deviceManager,
                          juce::ApplicationCommandManager& commandManager,
                          class KeymapManager& keymapManager);
    ~SettingsPanel() override;

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // Button::Listener overrides

    void buttonClicked(juce::Button* button) override;

    //==============================================================================
    // ComboBox::Listener overrides

    void comboBoxChanged(juce::ComboBox* comboBox) override;

    //==============================================================================
    // Settings management

    /**
     * Loads settings from Settings singleton and updates UI.
     */
    void loadSettings();

    /**
     * Saves current UI values to Settings singleton.
     */
    void saveSettings();

    /**
     * Shows the settings dialog as a modal window.
     *
     * @param parentComponent Parent component to center the dialog over
     * @param deviceManager Reference to the application's AudioDeviceManager
     * @param commandManager Reference to the application's ApplicationCommandManager
     * @param keymapManager Reference to the application's KeymapManager (Phase 3)
     */
    static void showDialog(juce::Component* parentComponent,
                          juce::AudioDeviceManager& deviceManager,
                          juce::ApplicationCommandManager& commandManager,
                          KeymapManager& keymapManager);

private:
    //==============================================================================
    // Private members

    juce::AudioDeviceManager& m_deviceManager;
    juce::ApplicationCommandManager& m_commandManager;
    KeymapManager& m_keymapManager;

    // Tab component for organizing settings
    juce::TabbedComponent m_tabbedComponent {juce::TabbedButtonBar::TabsAtTop};

    // Audio settings panel
    std::unique_ptr<juce::AudioDeviceSelectorComponent> m_audioSettings;

    // Keyboard shortcuts editor
    std::unique_ptr<ShortcutEditorPanel> m_shortcutEditor;

    // Keyboard template selection (Phase 3)
    juce::Label m_templateLabel;
    juce::ComboBox m_templateSelector;
    juce::TextButton m_importTemplateButton;
    juce::TextButton m_exportTemplateButton;

    // Display settings
    juce::Label m_waveformColorLabel;
    juce::ColourSelector m_waveformColorSelector;

    juce::Label m_selectionColorLabel;
    juce::ColourSelector m_selectionColorSelector;

    // Auto-save settings
    juce::Label m_autoSaveLabel;
    juce::ToggleButton m_autoSaveEnabled;

    juce::Label m_autoSaveIntervalLabel;
    juce::ComboBox m_autoSaveInterval;

    // Buttons
    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;
    juce::TextButton m_applyButton;

    //==============================================================================
    // Private methods

    /**
     * Creates the audio settings tab.
     */
    juce::Component* createAudioSettingsTab();

    /**
     * Creates the display settings tab.
     */
    juce::Component* createDisplaySettingsTab();

    /**
     * Creates the auto-save settings tab.
     */
    juce::Component* createAutoSaveSettingsTab();

    /**
     * Creates the keyboard shortcuts tab.
     */
    juce::Component* createKeyboardShortcutsTab();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
