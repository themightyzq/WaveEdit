/*
  ==============================================================================

    ChainWindowListener.h
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
#include "../Plugins/PluginChain.h"
#include "../Audio/AudioEngine.h"
#include "../UI/PluginChainWindow.h"
#include "../UI/PluginEditorWindow.h"
#include "../UI/ErrorDialog.h"
#include <functional>

/**
 * Listener for plugin chain window events.
 * Decouples the plugin chain window from MainComponent by using callbacks
 * instead of direct ownership coupling.
 */
class ChainWindowListener : public PluginChainWindow::Listener
{
public:
    /**
     * Create a new chain window listener.
     *
     * @param commandManager Reference to the application command manager
     * @param chain Pointer to the PluginChain (may become null after document close)
     * @param audioEngine Pointer to the AudioEngine (may become null after document close)
     * @param window Pointer to the DocumentWindow (will be deleted in documentClosed)
     * @param applyCallback Function to call when applying plugin chain to selection
     *                       Parameters: (convertToStereo, includeTail, tailLengthSeconds)
     */
    ChainWindowListener(
        juce::ApplicationCommandManager& commandManager,
        PluginChain* chain,
        AudioEngine* audioEngine,
        juce::DocumentWindow* window,
        std::function<void(bool, bool, double)> applyCallback)
        : m_commandManager(commandManager),
          m_chain(chain),
          m_audioEngine(audioEngine),
          m_window(window),
          m_applyCallback(applyCallback)
    {
    }

    void pluginChainWindowEditPlugin(int index) override
    {
        if (m_chain == nullptr) return;
        auto* node = m_chain->getPlugin(index);
        if (node != nullptr)
        {
            PluginEditorWindow::showForNode(node, &m_commandManager);
        }
    }

    void pluginChainWindowApplyToSelection(const PluginChainWindow::RenderOptions& options) override
    {
        if (m_chain == nullptr || m_audioEngine == nullptr) return;
        // Apply with the render options from the window
        if (m_applyCallback)
        {
            m_applyCallback(options.convertToStereo, options.includeTail, options.tailLengthSeconds);
        }
    }

    void pluginChainWindowPluginAdded(const juce::PluginDescription& description) override
    {
        if (m_chain == nullptr || m_audioEngine == nullptr) return;
        int index = m_chain->addPlugin(description);
        if (index >= 0)
        {
            m_audioEngine->setPluginChainEnabled(true);
            DBG("Added plugin: " + description.name);
        }
        else
        {
            ErrorDialog::show("Plugin Error", "Failed to load plugin: " + description.name);
        }
    }

    void pluginChainWindowPluginRemoved(int index) override
    {
        if (m_chain == nullptr) return;
        m_chain->removePlugin(index);
    }

    void pluginChainWindowPluginMoved(int fromIndex, int toIndex) override
    {
        if (m_chain == nullptr) return;
        m_chain->movePlugin(fromIndex, toIndex);
    }

    void pluginChainWindowPluginBypassed(int index, bool bypassed) override
    {
        if (m_chain == nullptr) return;
        auto* node = m_chain->getPlugin(index);
        if (node != nullptr)
        {
            node->setBypassed(bypassed);
        }
    }

    void pluginChainWindowBypassAll(bool bypassed) override
    {
        if (m_chain == nullptr) return;
        m_chain->setAllBypassed(bypassed);
    }

    /** Called when the associated document is closed - invalidates pointers and closes window */
    void documentClosed()
    {
        m_chain = nullptr;
        m_audioEngine = nullptr;
        if (m_window != nullptr)
        {
            m_window->setVisible(false);
            delete m_window;
            m_window = nullptr;
        }
    }

    /** Check if this listener is for the given chain */
    bool isForChain(PluginChain* chain) const { return m_chain == chain; }

private:
    juce::ApplicationCommandManager& m_commandManager;
    PluginChain* m_chain;
    AudioEngine* m_audioEngine;
    juce::DocumentWindow* m_window;
    std::function<void(bool, bool, double)> m_applyCallback;
};
