/*
  ==============================================================================

    PluginManagerDialog.h
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
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Plugins/PluginManager.h"

/**
 * Dialog for browsing and selecting VST3/AU plugins.
 *
 * Features:
 * - TableListBox with sortable columns (Name, Manufacturer, Category, Format, Latency)
 * - Search/filter text box
 * - Category/manufacturer dropdown filters
 * - Plugin info panel showing details of selected plugin
 * - Rescan button to trigger background plugin scan
 * - Add to Chain / Cancel buttons
 *
 * Threading: All UI operations on message thread.
 * Plugin scanning happens on background thread via PluginManager.
 */
class PluginManagerDialog : public juce::Component,
                             public juce::TableListBoxModel,
                             public juce::ChangeListener,
                             public juce::Timer
{
public:
    /**
     * Listener interface for plugin manager events.
     */
    class Listener
    {
    public:
        virtual ~Listener() = default;

        /**
         * Called when the user selects a plugin to add to the chain.
         *
         * @param description Description of the selected plugin
         */
        virtual void pluginManagerDialogAddPlugin(const juce::PluginDescription& description) = 0;

        /**
         * Called when the dialog is cancelled.
         */
        virtual void pluginManagerDialogCancelled() = 0;
    };

    /**
     * Constructor.
     */
    PluginManagerDialog();

    /**
     * Destructor.
     */
    ~PluginManagerDialog() override;

    /**
     * Sets the listener for plugin manager events.
     */
    void setListener(Listener* listener) { m_listener = listener; }

    /**
     * Gets the currently selected plugin description.
     * @return Selected plugin description, or nullptr if none selected
     */
    const juce::PluginDescription* getSelectedPlugin() const;

    /**
     * Shows this dialog in a window.
     *
     * @param modal If true, shows as a modal window
     * @return The created window (caller owns this)
     */
    juce::DocumentWindow* showInWindow(bool modal = false);

    /**
     * Refreshes the plugin list from PluginManager.
     */
    void refresh();

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    //==============================================================================
    // TableListBoxModel overrides
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height,
                           bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId,
                   int width, int height, bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void sortOrderChanged(int newSortColumnId, bool isForwards) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void returnKeyPressed(int lastRowSelected) override;

    //==============================================================================
    // ChangeListener override (for PluginManager scan progress)
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    //==============================================================================
    // Timer override (for scan progress updates)
    void timerCallback() override;

private:
    /**
     * Column IDs for the table.
     */
    enum ColumnId
    {
        NameColumn = 1,
        ManufacturerColumn = 2,
        CategoryColumn = 3,
        FormatColumn = 4,
        LatencyColumn = 5
    };

    /**
     * Struct to hold filtered plugin data.
     */
    struct FilteredPlugin
    {
        int originalIndex;
        const juce::PluginDescription* description;
    };

    //==============================================================================
    // Private methods
    void updateFilteredPlugins();
    void applyFilter();
    void sortPlugins();
    void updatePluginInfo();
    void onSearchTextChanged();
    void onCategoryFilterChanged();
    void onManufacturerFilterChanged();
    void onRescanClicked();
    void onAddClicked();
    void onCancelClicked();

    //==============================================================================
    // Member variables
    Listener* m_listener = nullptr;

    // Flag set when user clicks Add to Chain button or double-clicks
    bool m_wasAddClicked = false;

public:
    /** Returns true if the Add button was clicked (vs just closing the dialog) */
    bool wasAddClicked() const { return m_wasAddClicked; }

private:

    // All plugins from PluginManager
    juce::Array<juce::PluginDescription> m_allPlugins;

    // Filtered and sorted plugins
    juce::Array<FilteredPlugin> m_filteredPlugins;

    // Filter state
    juce::String m_filterText;
    juce::String m_categoryFilter;
    juce::String m_manufacturerFilter;

    // Sorting state
    int m_sortColumnId = NameColumn;
    bool m_sortForwards = true;

    // UI Components
    juce::Label m_titleLabel;
    juce::Label m_searchLabel;
    juce::TextEditor m_searchBox;
    juce::Label m_categoryLabel;
    juce::ComboBox m_categoryComboBox;
    juce::Label m_manufacturerLabel;
    juce::ComboBox m_manufacturerComboBox;
    juce::TableListBox m_table;

    // Plugin info panel
    juce::Label m_infoTitleLabel;
    juce::Label m_infoNameLabel;
    juce::Label m_infoManufacturerLabel;
    juce::Label m_infoCategoryLabel;
    juce::Label m_infoVersionLabel;
    juce::Label m_infoFormatLabel;
    juce::Label m_infoLatencyLabel;
    juce::Label m_infoFileLabel;

    // Scan progress
    juce::Label m_scanStatusLabel;
    juce::ProgressBar m_scanProgressBar;
    double m_scanProgress = 0.0;

    // Buttons
    juce::TextButton m_rescanButton;
    juce::TextButton m_addButton;
    juce::TextButton m_cancelButton;

    // Visual settings
    const int m_rowHeight = 24;
    const juce::Colour m_backgroundColour { 0xff1e1e1e };
    const juce::Colour m_alternateRowColour { 0xff252525 };
    const juce::Colour m_selectedRowColour { 0xff3a3a3a };
    const juce::Colour m_textColour { 0xffe0e0e0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginManagerDialog)
};
