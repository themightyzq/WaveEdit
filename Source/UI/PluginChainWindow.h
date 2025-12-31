/*
  ==============================================================================

    PluginChainWindow.h
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
#include "../Plugins/PluginChain.h"
#include "../Plugins/PluginManager.h"

/**
 * Unified window for managing the plugin effect chain with integrated browser.
 *
 * Layout:
 * +----------------------------------+------------------------+
 * |  Plugin Chain (left ~50%)        |  Plugin Browser (right)|
 * |  - Drag-reorderable list         |  - Search box          |
 * |  - Bypass/Edit/Remove buttons    |  - Filter dropdowns    |
 * |  - Bypass All toggle             |  - Plugin list table   |
 * |  - Apply to Selection button     |  - Double-click adds   |
 * |  - Latency display               |  - Rescan button       |
 * +----------------------------------+------------------------+
 *
 * Features:
 * - Single window workflow for plugin management
 * - Double-click in browser instantly adds plugin to chain
 * - Apply button processes selection through chain
 * - Drag-and-drop reordering within chain
 */
class PluginChainWindow : public juce::Component,
                           public juce::ListBoxModel,
                           public juce::ChangeListener,
                           public juce::Timer,
                           public juce::DragAndDropContainer
{
public:
    /**
     * Render options for plugin chain processing.
     */
    struct RenderOptions
    {
        bool convertToStereo = false;    ///< Convert mono to stereo before processing
        bool includeTail = false;        ///< Include effect tail (for reverb/delay)
        double tailLengthSeconds = 2.0;  ///< Tail length in seconds
    };

    /**
     * Listener interface for plugin chain window events.
     */
    class Listener
    {
    public:
        virtual ~Listener() = default;

        /** Called when user double-clicks a plugin in chain (edit) */
        virtual void pluginChainWindowEditPlugin(int index) = 0;

        /** Called when user clicks Apply to Selection button with render options */
        virtual void pluginChainWindowApplyToSelection(const RenderOptions& options) = 0;

        /** Called when a plugin is added to the chain */
        virtual void pluginChainWindowPluginAdded(const juce::PluginDescription& description) = 0;

        /** Called when a plugin is removed from the chain */
        virtual void pluginChainWindowPluginRemoved(int index) = 0;

        /** Called when plugin order changes via drag */
        virtual void pluginChainWindowPluginMoved(int fromIndex, int toIndex) = 0;

        /** Called when plugin bypass state changes */
        virtual void pluginChainWindowPluginBypassed(int index, bool bypassed) = 0;

        /** Called when bypass all state changes */
        virtual void pluginChainWindowBypassAll(bool bypassed) = 0;
    };

    /**
     * Constructor.
     * @param chain The plugin chain to display and manage
     */
    explicit PluginChainWindow(PluginChain* chain);

    /**
     * Destructor.
     */
    ~PluginChainWindow() override;

    /**
     * Sets the listener for window events.
     */
    void setListener(Listener* listener) { m_listener = listener; }

    /**
     * Refreshes both the chain list and browser list.
     */
    void refresh();

    /**
     * Gets the current render options selected in the window.
     * @return Current render options
     */
    RenderOptions getRenderOptions() const;

    /**
     * Sets whether the source audio is mono (to enable/disable stereo conversion).
     * @param isMono true if source is mono
     */
    void setSourceIsMono(bool isMono);

    /**
     * Shows this window with keyboard shortcut support.
     * @param commandManager The application command manager for routing shortcuts
     * @return The created window (caller owns this)
     */
    juce::DocumentWindow* showInWindow(juce::ApplicationCommandManager* commandManager);

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    //==============================================================================
    // ListBoxModel overrides (for plugin chain)
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                          bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent& e) override;
    void deleteKeyPressed(int lastRowSelected) override;
    void returnKeyPressed(int lastRowSelected) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& rowsToDescribe) override;


    //==============================================================================
    // ChangeListener override (for PluginChain and PluginManager changes)
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    //==============================================================================
    // Timer override (for scan progress)
    void timerCallback() override;

private:
    //==============================================================================
    // Inner classes

    /**
     * Custom component for plugin chain row with buttons.
     */
    class PluginRowComponent : public juce::Component
    {
    public:
        PluginRowComponent(PluginChainWindow& owner);
        void update(int index, PluginChainNode* node, int totalCount);
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;

    private:
        void updateBypassButtonAppearance(bool isBypassed);
        void updateMoveButtonStates(int index, int totalCount);
        PluginChainWindow& m_owner;
        int m_index = -1;
        PluginChainNode* m_node = nullptr;

        juce::TextButton m_moveUpButton;
        juce::TextButton m_moveDownButton;
        juce::TextButton m_bypassButton;
        juce::TextButton m_editButton;
        juce::TextButton m_removeButton;
        juce::Label m_nameLabel;
        juce::Label m_latencyLabel;

        bool m_dragStarted = false;
        juce::Point<int> m_dragStartPos;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginRowComponent)
    };

    /**
     * Custom ListBox that supports drag-and-drop reordering.
     */
    class DraggableListBox : public juce::ListBox,
                              public juce::DragAndDropTarget
    {
    public:
        DraggableListBox(PluginChainWindow& owner);

        bool isInterestedInDragSource(const SourceDetails& details) override;
        void itemDragEnter(const SourceDetails& details) override;
        void itemDragMove(const SourceDetails& details) override;
        void itemDragExit(const SourceDetails& details) override;
        void itemDropped(const SourceDetails& details) override;
        void paint(juce::Graphics& g) override;
        void paintOverChildren(juce::Graphics& g) override;

    private:
        PluginChainWindow& m_owner;
        int m_dropInsertIndex = -1;

        int getInsertIndexForPosition(int y) const;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DraggableListBox)
    };

    /**
     * Table model for the plugin browser.
     * Separate class to avoid conflict with ListBoxModel::getNumRows()
     */
    class BrowserTableModel : public juce::TableListBoxModel
    {
    public:
        BrowserTableModel(PluginChainWindow& owner) : m_owner(owner) {}

        int getNumRows() override;
        void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height,
                               bool rowIsSelected) override;
        void paintCell(juce::Graphics& g, int rowNumber, int columnId,
                       int width, int height, bool rowIsSelected) override;
        void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
        void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
        void sortOrderChanged(int newSortColumnId, bool isForwards) override;
        void selectedRowsChanged(int lastRowSelected) override;

    private:
        PluginChainWindow& m_owner;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BrowserTableModel)
    };

    /**
     * Column IDs for the browser table.
     */
    enum BrowserColumnId
    {
        NameColumn = 1,
        ManufacturerColumn = 2,
        CategoryColumn = 3,
        FormatColumn = 4
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
    // Private methods - Chain panel
    void updateLatencyDisplay();
    void onBypassAllClicked();
    void onApplyToSelectionClicked();
    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected,
                                             juce::Component* existingComponentToUpdate) override;

    // Private methods - Browser panel
    void refreshBrowser();
    void updateFilteredPlugins();
    void applyFilter();
    void sortPlugins();
    void onSearchTextChanged();
    void onCategoryFilterChanged();
    void onManufacturerFilterChanged();
    void onRescanClicked();
    void addSelectedPluginToChain();

    //==============================================================================
    // Member variables
    PluginChain* m_chain;
    Listener* m_listener = nullptr;

    // Chain panel UI
    juce::Label m_chainTitleLabel;
    DraggableListBox m_chainListBox;
    juce::Label m_latencyLabel;
    juce::Label m_emptyChainLabel;
    juce::TextButton m_applyToSelectionButton;
    juce::ToggleButton m_bypassAllButton;

    // Render Options UI
    juce::GroupComponent m_renderOptionsGroup;
    juce::ToggleButton m_convertToStereoCheckbox;
    juce::ToggleButton m_includeTailCheckbox;
    juce::Label m_tailLengthLabel;
    juce::Slider m_tailLengthSlider;
    bool m_isSourceMono = false;

    // Browser panel UI
    juce::Label m_browserTitleLabel;
    juce::Label m_searchLabel;
    juce::TextEditor m_searchBox;
    juce::Label m_categoryLabel;
    juce::ComboBox m_categoryComboBox;
    juce::Label m_manufacturerLabel;
    juce::ComboBox m_manufacturerComboBox;
    BrowserTableModel m_browserTableModel;
    juce::TableListBox m_browserTable;
    juce::Label m_emptySearchLabel;  // Shown when filter returns 0 results
    juce::TextButton m_rescanButton;
    juce::Label m_scanStatusLabel;
    juce::ProgressBar m_scanProgressBar;
    double m_scanProgress = 0.0;

    // Browser data
    juce::Array<juce::PluginDescription> m_allPlugins;
    juce::Array<FilteredPlugin> m_filteredPlugins;
    juce::String m_filterText;
    juce::String m_categoryFilter;
    juce::String m_manufacturerFilter;
    int m_sortColumnId = NameColumn;
    bool m_sortForwards = true;

    // Layout
    const int m_chainRowHeight = 48;
    const int m_browserRowHeight = 24;
    const int m_dividerX = 450;  // Split position

    // Visual settings
    const juce::Colour m_backgroundColour { 0xff1e1e1e };
    const juce::Colour m_alternateRowColour { 0xff252525 };
    const juce::Colour m_selectedRowColour { 0xff3a3a3a };
    const juce::Colour m_textColour { 0xffe0e0e0 };
    const juce::Colour m_accentColour { 0xff4a90d9 };
    const juce::Colour m_dividerColour { 0xff333333 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChainWindow)
};
