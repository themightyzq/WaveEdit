/*
  ==============================================================================

    PluginPathsPanel.h
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
#include "PluginManager.h"

/**
 * Panel for configuring VST3 plugin search paths.
 *
 * Shows:
 * - Default system paths (read-only)
 * - Custom user-defined paths (editable)
 * - Add/Remove buttons for custom paths
 *
 * This can be embedded in a Preferences dialog or shown standalone.
 */
class PluginPathsPanel : public juce::Component,
                          public juce::ListBoxModel
{
public:
    PluginPathsPanel();
    ~PluginPathsPanel() override;

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // ListBoxModel overrides
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g,
                          int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override;

    //==============================================================================
    /** Refresh the paths display from PluginManager */
    void refresh();

    /** Apply changes (saves custom paths) */
    void applyChanges();

    /**
     * Show this panel in a dialog window.
     */
    static void showDialog();

private:
    struct PathEntry
    {
        juce::String path;
        bool isDefault;  // true = system path (read-only), false = custom (editable)
    };

    void onAddPathClicked();
    void onRemovePathClicked();
    void onBrowseClicked();
    void updateRemoveButtonState();

    juce::Label m_titleLabel;
    juce::Label m_descriptionLabel;

    juce::Label m_defaultPathsLabel;
    juce::Label m_customPathsLabel;

    juce::ListBox m_pathsList;
    juce::Array<PathEntry> m_paths;

    juce::TextButton m_addButton;
    juce::TextButton m_removeButton;
    juce::TextButton m_browseButton;

    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;

    bool m_hasChanges = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginPathsPanel)
};
