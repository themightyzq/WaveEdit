/*
  ==============================================================================

    PluginChainPanel.h
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

/**
 * Panel for managing the plugin effect chain.
 *
 * Features:
 * - Drag-reorderable list of plugins
 * - Per-plugin Bypass button (clearly labeled, toggles effect on/off)
 * - Per-plugin Edit button (opens plugin editor)
 * - Per-plugin remove button
 * - Total latency display
 * - Add Plugin button (opens PluginManagerDialog)
 * - Bypass All toggle
 *
 * Threading: All UI operations on message thread.
 * Uses ChangeListener to track chain changes.
 */
class PluginChainPanel : public juce::Component,
                          public juce::ListBoxModel,
                          public juce::ChangeListener,
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
     * Listener interface for plugin chain panel events.
     */
    class Listener
    {
    public:
        virtual ~Listener() = default;

        /** Called when user clicks Add Plugin button */
        virtual void pluginChainPanelAddPlugin() = 0;

        /** Called when user double-clicks a plugin (edit) */
        virtual void pluginChainPanelEditPlugin(int index) = 0;

        /** Called when user removes a plugin */
        virtual void pluginChainPanelRemovePlugin(int index) = 0;

        /** Called when plugin order changes via drag */
        virtual void pluginChainPanelMovePlugin(int fromIndex, int toIndex) = 0;

        /** Called when plugin bypass state changes */
        virtual void pluginChainPanelBypassPlugin(int index, bool bypassed) = 0;

        /** Called when bypass all state changes */
        virtual void pluginChainPanelBypassAll(bool bypassed) = 0;

        /** Called when user clicks Apply to Selection button */
        virtual void pluginChainPanelApplyToSelection() = 0;
    };

    /**
     * Constructor.
     * @param chain The plugin chain to display and manage
     */
    explicit PluginChainPanel(PluginChain* chain);

    /**
     * Destructor.
     */
    ~PluginChainPanel() override;

    /**
     * Sets the listener for panel events.
     */
    void setListener(Listener* listener) { m_listener = listener; }

    /**
     * Refreshes the list to reflect current chain state.
     */
    void refresh();

    /**
     * Gets the current render options selected in the panel.
     * @return Current render options
     */
    RenderOptions getRenderOptions() const;

    /**
     * Sets whether the source audio is mono (to enable/disable stereo conversion).
     * @param isMono true if source is mono
     */
    void setSourceIsMono(bool isMono);

    /**
     * Shows this panel in a window.
     * @param modal If true, shows as a modal window
     * @return The created window (caller owns this)
     */
    juce::DocumentWindow* showInWindow(bool modal = false);

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    //==============================================================================
    // ListBoxModel overrides
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                          bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent& e) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent& e) override;
    void deleteKeyPressed(int lastRowSelected) override;
    void returnKeyPressed(int lastRowSelected) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& rowsToDescribe) override;

    //==============================================================================
    // ChangeListener override (for PluginChain changes)
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    /**
     * Custom component for plugin row with buttons.
     */
    class PluginRowComponent : public juce::Component
    {
    public:
        PluginRowComponent(PluginChainPanel& owner);
        void update(int index, PluginChainNode* node);
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;

    private:
        void updateBypassButtonAppearance(bool isBypassed);
        PluginChainPanel& m_owner;
        int m_index = -1;
        PluginChainNode* m_node = nullptr;

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
        DraggableListBox(PluginChainPanel& owner);

        bool isInterestedInDragSource(const SourceDetails& details) override;
        void itemDragEnter(const SourceDetails& details) override;
        void itemDragMove(const SourceDetails& details) override;
        void itemDragExit(const SourceDetails& details) override;
        void itemDropped(const SourceDetails& details) override;
        void paint(juce::Graphics& g) override;

    private:
        PluginChainPanel& m_owner;
        int m_dropInsertIndex = -1;

        int getInsertIndexForPosition(int y) const;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DraggableListBox)
    };

    //==============================================================================
    // Private methods
    void updateLatencyDisplay();
    void onAddPluginClicked();
    void onBypassAllClicked();
    void onApplyToSelectionClicked();
    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected,
                                             juce::Component* existingComponentToUpdate) override;

    //==============================================================================
    // Member variables
    PluginChain* m_chain;
    Listener* m_listener = nullptr;

    // UI Components
    juce::Label m_titleLabel;
    DraggableListBox m_listBox;
    juce::Label m_latencyLabel;
    juce::Label m_emptyLabel;
    juce::TextButton m_addPluginButton;
    juce::TextButton m_applyToSelectionButton;  // Apply chain to selection (Cmd+P)
    juce::ToggleButton m_bypassAllButton;

    // UI Components - Render Options
    juce::GroupComponent m_renderOptionsGroup;
    juce::ToggleButton m_convertToStereoCheckbox;
    juce::ToggleButton m_includeTailCheckbox;
    juce::Label m_tailLengthLabel;
    juce::Slider m_tailLengthSlider;

    // State
    bool m_isSourceMono = false;  ///< True if source buffer is mono

    // Visual settings
    const int m_rowHeight = 48;
    const juce::Colour m_backgroundColour { 0xff1e1e1e };
    const juce::Colour m_alternateRowColour { 0xff252525 };
    const juce::Colour m_selectedRowColour { 0xff3a3a3a };
    const juce::Colour m_textColour { 0xffe0e0e0 };
    const juce::Colour m_accentColour { 0xff4a90d9 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChainPanel)
};
