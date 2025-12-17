/*
  ==============================================================================

    OfflinePluginDialog.h
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
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>

// Forward declarations
class AudioEngine;
class AudioBufferManager;
class PluginManager;

/**
 * Dialog for applying a single VST3/AU plugin offline.
 *
 * This dialog allows the user to:
 * - Select a plugin from a searchable browser list
 * - View and modify the plugin's parameters via its native editor
 * - Preview the effect on the selection with real-time processing
 * - Apply the effect permanently
 *
 * Workflow:
 * 1. User opens dialog (Plugins → Offline Plugin...)
 * 2. User types in search box to filter plugins, then selects one
 * 3. Plugin editor loads showing plugin UI
 * 4. User adjusts parameters
 * 5. User clicks Preview to hear the result with real-time plugin processing
 * 6. User clicks Apply to process the audio or Cancel to abort
 *
 * Thread Safety: UI thread only. Must be shown from message thread.
 * Preview uses REALTIME_DSP mode for instant response and plugin visualization.
 */
class OfflinePluginDialog : public juce::Component,
                            public juce::ChangeListener,
                            public juce::TableListBoxModel
{
public:
    /**
     * Render options for plugin processing.
     */
    struct RenderOptions
    {
        bool convertToStereo = false;    ///< Convert mono to stereo before processing
        bool includeTail = false;        ///< Include effect tail (for reverb/delay)
        double tailLengthSeconds = 2.0;  ///< Tail length in seconds
    };

    /**
     * Result structure containing the selected plugin and its state.
     */
    struct Result
    {
        bool applied = false;                          ///< true if user clicked Apply
        juce::PluginDescription pluginDescription;     ///< The selected plugin
        juce::MemoryBlock pluginState;                 ///< Plugin state to apply
        RenderOptions renderOptions;                   ///< Render options selected by user
    };

    /**
     * Creates an OfflinePluginDialog.
     *
     * @param audioEngine AudioEngine for preview functionality
     * @param bufferManager AudioBufferManager for extracting preview selection
     * @param selectionStart Start sample of selection (0 if no selection)
     * @param selectionEnd End sample of selection (total length if no selection)
     */
    explicit OfflinePluginDialog(AudioEngine* audioEngine,
                                 AudioBufferManager* bufferManager,
                                 int64_t selectionStart = 0,
                                 int64_t selectionEnd = 0);

    ~OfflinePluginDialog() override;

    /**
     * Show the dialog modally and return the result.
     *
     * @param audioEngine AudioEngine for preview functionality
     * @param bufferManager AudioBufferManager for extracting preview selection
     * @param selectionStart Start sample of selection
     * @param selectionEnd End sample of selection
     * @return std::optional<Result> containing plugin and state if user clicked Apply,
     *         std::nullopt if user clicked Cancel or closed the dialog
     */
    static std::optional<Result> showDialog(AudioEngine* audioEngine,
                                            AudioBufferManager* bufferManager,
                                            int64_t selectionStart = 0,
                                            int64_t selectionEnd = 0);

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // ChangeListener override (for plugin scanner)
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // TableListBoxModel overrides (for plugin browser)
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height,
                           bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId,
                   int width, int height, bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void selectedRowsChanged(int lastRowSelected) override;

private:
    //==============================================================================
    // Private methods
    void refreshPluginList();
    void updateFilteredPlugins();
    void onSearchTextChanged();
    void onPluginSelected(int pluginIndex);
    void loadSelectedPlugin();
    void unloadCurrentPlugin();
    void createPluginEditor();
    void resizeToFitEditor();
    void onPreviewClicked();
    void onApplyClicked();
    void onCancelClicked();
    void disablePreview();
    void enableRealtimePreview();

    //==============================================================================
    // Column IDs for plugin browser table
    enum ColumnId
    {
        NameColumn = 1,
        ManufacturerColumn = 2,
        FormatColumn = 3
    };

    // Struct for filtered plugin list
    struct FilteredPlugin
    {
        int originalIndex;
        const juce::PluginDescription* description;
    };

    //==============================================================================
    // UI Components - Plugin Browser (replaces ComboBox with searchable table)
    juce::Label m_titleLabel;
    juce::TextEditor m_searchBox;
    juce::TableListBox m_pluginTable;
    juce::TextButton m_rescanButton;

    // UI Components - Plugin Editor Container
    juce::Viewport m_editorViewport;
    std::unique_ptr<juce::Component> m_editorContainer;
    std::unique_ptr<juce::AudioProcessorEditor> m_pluginEditor;
    juce::Label m_noPluginLabel;

    // UI Components - Render Options
    juce::GroupComponent m_renderOptionsGroup;
    juce::ToggleButton m_convertToStereoCheckbox;
    juce::ToggleButton m_includeTailCheckbox;
    juce::Label m_tailLengthLabel;
    juce::Slider m_tailLengthSlider;

    // UI Components - Buttons
    juce::ToggleButton m_loopCheckbox;
    juce::TextButton m_previewButton;
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;

    //==============================================================================
    // Plugin data
    juce::Array<juce::PluginDescription> m_availablePlugins;
    juce::Array<FilteredPlugin> m_filteredPlugins;
    juce::String m_filterText;
    std::unique_ptr<juce::AudioPluginInstance> m_pluginInstance;
    juce::PluginDescription m_selectedPluginDescription;
    int m_selectedPluginIndex = -1;

    //==============================================================================
    // Audio preview state
    AudioEngine* m_audioEngine;
    AudioBufferManager* m_bufferManager;
    int64_t m_selectionStart;
    int64_t m_selectionEnd;
    bool m_isPreviewActive;
    bool m_isPreviewPlaying;
    bool m_isSourceMono;   ///< True if source buffer is mono

    //==============================================================================
    // Dialog result
    std::optional<Result> m_result;

    //==============================================================================
    // Layout constants - Side-by-side layout (editor on left, browser on right)
    static const int kMinEditorWidth = 400;
    static const int kMinEditorHeight = 300;
    static const int kBrowserWidth = 300;       // Width for plugin browser panel on right
    static const int kSearchRowHeight = 30;     // Height for search box row
    static const int kButtonRowHeight = 40;     // Height for Preview/Cancel/Apply buttons
    static const int kRenderOptionsHeight = 90; // Height for render options section (increased for 2 checkboxes + slider)
    static const int kPadding = 15;             // Edge padding
    static const int kSpacing = 10;             // Inter-element spacing
    static const int kBrowserRowHeight = 22;    // Row height in plugin table
    static const int kDividerWidth = 8;         // Divider between editor and browser

    // Visual settings (match PluginChainWindow)
    const juce::Colour m_backgroundColour { 0xff2b2b2b };
    const juce::Colour m_alternateRowColour { 0xff252525 };
    const juce::Colour m_selectedRowColour { 0xff3a3a3a };
    const juce::Colour m_textColour { 0xffe0e0e0 };
    const juce::Colour m_accentColour { 0xff4a90d9 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OfflinePluginDialog)
};
