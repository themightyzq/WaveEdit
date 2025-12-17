/*
  ==============================================================================

    CustomizableToolbar.h
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
#include "../Utils/ToolbarConfig.h"
#include "../Utils/ToolbarManager.h"
#include "ToolbarButton.h"
#include "CompactTransport.h"

class AudioEngine;
class WaveformDisplay;
class Document;

/**
 * Customizable toolbar component that can be configured via JSON templates.
 *
 * Features:
 * - Hosts CompactTransport widget for playback controls
 * - Configurable button layout from ToolbarLayout
 * - Drag-and-drop button reordering
 * - Right-click context menu for customization
 * - Responds to ToolbarManager layout changes
 *
 * Layout Types Supported:
 * - COMMAND: Executes ApplicationCommand
 * - PLUGIN: Opens specific plugin
 * - TRANSPORT: Embedded CompactTransport widget
 * - SEPARATOR: Visual divider
 * - SPACER: Flexible space
 */
class CustomizableToolbar : public juce::Component,
                            public juce::DragAndDropContainer,
                            public juce::DragAndDropTarget,
                            public ToolbarManager::Listener
{
public:
    //==============================================================================

    /**
     * Constructor.
     *
     * @param commandManager Application command manager for button commands
     * @param toolbarManager Toolbar manager for layout management
     */
    CustomizableToolbar(juce::ApplicationCommandManager& commandManager,
                        ToolbarManager& toolbarManager);

    ~CustomizableToolbar() override;

    //==============================================================================
    // Component Overrides

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

    //==============================================================================
    // DragAndDropTarget Overrides

    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

    //==============================================================================
    // ToolbarManager::Listener

    void toolbarLayoutChanged(const ToolbarLayout& newLayout) override;

    //==============================================================================
    // Layout Management

    /**
     * Load a toolbar layout configuration.
     *
     * @param layout Layout to apply
     */
    void loadLayout(const ToolbarLayout& layout);

    /**
     * Get the current toolbar layout.
     */
    const ToolbarLayout& getCurrentLayout() const { return m_currentLayout; }

    /**
     * Get preferred height based on current layout.
     */
    int getPreferredHeight() const { return m_currentLayout.height; }

    //==============================================================================
    // Document Context

    /**
     * Set the current document for transport context.
     * Updates the CompactTransport to control this document's playback.
     *
     * @param doc Document to control, or nullptr when no document is open
     */
    void setDocument(Document* doc);

    /**
     * Get the compact transport component.
     */
    CompactTransport* getCompactTransport() { return m_compactTransport.get(); }

    //==============================================================================
    // Plugin Callback

    /**
     * Set callback for when plugin buttons are clicked.
     */
    std::function<void(const juce::String& pluginId)> onPluginClick;

private:
    //==============================================================================
    // Private Members

    juce::ApplicationCommandManager& m_commandManager;
    ToolbarManager& m_toolbarManager;
    ToolbarLayout m_currentLayout;

    std::unique_ptr<CompactTransport> m_compactTransport;
    juce::OwnedArray<juce::Component> m_buttonComponents;

    Document* m_currentDocument = nullptr;

    // Drag-drop state
    int m_dragInsertIndex = -1;
    bool m_isDragging = false;

    //==============================================================================
    // Private Methods

    /**
     * Rebuild all toolbar components from current layout.
     */
    void rebuildToolbar();

    /**
     * Create component for a button configuration.
     */
    juce::Component* createButtonComponent(const ToolbarButtonConfig& config);

    /**
     * Calculate button positions and layout.
     */
    void layoutButtons();

    /**
     * Get insertion index for drag-drop at given position.
     */
    int getInsertIndexForPosition(int x) const;

    /**
     * Show context menu for toolbar customization.
     * @param screenPosition Screen position to show menu at
     */
    void showContextMenu(juce::Point<int> screenPosition);

    /**
     * Handle plugin button click.
     */
    void handlePluginButtonClick(const juce::String& pluginId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CustomizableToolbar)
};
