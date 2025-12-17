/*
  ==============================================================================

    PluginEditorWindow.h
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
#include "../Plugins/PluginChainNode.h"

/**
 * Window for hosting a native VST3/AU plugin editor.
 *
 * Features:
 * - Hosts the plugin's native UI (or generic fallback)
 * - Bypass toggle button in title bar
 * - Window position persistence (optional)
 * - Closes cleanly when chain is modified
 * - Routes keyboard shortcuts to main application
 *
 * Threading: All operations on message thread.
 */
class PluginEditorWindow : public juce::DocumentWindow,
                           public juce::ApplicationCommandTarget
{
public:
    /**
     * Create a plugin editor window.
     * @param node The plugin chain node to edit
     * @param commandManager The application command manager for shortcut routing
     */
    explicit PluginEditorWindow(PluginChainNode* node,
                                juce::ApplicationCommandManager* commandManager = nullptr);

    /**
     * Destructor.
     */
    ~PluginEditorWindow() override;

    /**
     * Get the plugin node being edited.
     */
    PluginChainNode* getNode() const { return m_node; }

    /**
     * Check if this window is for a specific node.
     */
    bool isForNode(PluginChainNode* node) const { return m_node == node; }

    /**
     * Update bypass button state to match node.
     */
    void updateBypassState();

    //==============================================================================
    // DocumentWindow overrides
    void closeButtonPressed() override;

    //==============================================================================
    // ApplicationCommandTarget overrides - route shortcuts to main window
    juce::ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands(juce::Array<juce::CommandID>& commands) override;
    void getCommandInfo(juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;
    bool perform(const InvocationInfo& info) override;

    /**
     * Static method to show a plugin editor window.
     * If a window for this node already exists, brings it to front.
     * @param node The plugin chain node to edit
     * @param commandManager The application command manager for shortcut routing
     * @return The editor window (may be existing or new)
     */
    static PluginEditorWindow* showForNode(PluginChainNode* node,
                                           juce::ApplicationCommandManager* commandManager = nullptr);

    /**
     * Close all open plugin editor windows.
     */
    static void closeAll();

    /**
     * Close the editor window for a specific node.
     * @param node The node whose editor to close
     */
    static void closeForNode(PluginChainNode* node);

private:
    //==============================================================================
    /**
     * Content component that hosts the editor and bypass button.
     */
    class ContentComponent : public juce::Component
    {
    public:
        ContentComponent(PluginEditorWindow& owner, std::unique_ptr<juce::AudioProcessorEditor> editor);
        ~ContentComponent() override;

        void paint(juce::Graphics& g) override;
        void resized() override;

        void updateBypassState();

    private:
        PluginEditorWindow& m_owner;
        std::unique_ptr<juce::AudioProcessorEditor> m_editor;
        juce::ToggleButton m_bypassButton;
        juce::Label m_latencyLabel;

        static const int kToolbarHeight = 28;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ContentComponent)
    };

    //==============================================================================
    PluginChainNode* m_node;
    ContentComponent* m_content = nullptr;

    // Command routing
    juce::ApplicationCommandManager* m_commandManager = nullptr;
    juce::ApplicationCommandTarget* m_mainCommandTarget = nullptr;

    // Static registry of open windows
    static juce::Array<PluginEditorWindow*> s_openWindows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorWindow)
};
