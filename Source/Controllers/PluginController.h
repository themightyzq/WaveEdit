/*
  ==============================================================================

    PluginController.h - Plugin browser/chain window controller
    Part of WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Per CLAUDE.md §8.1: plugin chain window management belongs out of
    Main.cpp. PluginController owns the plugin manager dialog flow and
    the plugin-chain window lifecycle (including its ChainWindowListener
    tracking).

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class Document;
class ChainWindowListener;
class DSPController;

class PluginController
{
public:
    PluginController() = default;
    ~PluginController() = default;

    /**
     * Modal Plugin Manager dialog. On Add, appends the chosen plugin to the
     * current document's plugin chain.
     */
    void showPluginManagerDialog(Document* currentDoc);

    /**
     * Show the unified Plugin Chain window for the given document. Tracks
     * the ChainWindowListener in an internal OwnedArray; the caller's
     * DSPController is invoked when the user requests "apply chain".
     */
    void showPluginChainPanel(Document* currentDoc,
                              juce::ApplicationCommandManager& commandManager,
                              DSPController& dspController,
                              std::function<Document*()> currentDocAccessor);

    /**
     * Notify all tracked chain-window listeners that the document has gone
     * away. Call from the document close path before the AudioEngine is
     * destroyed to break the listener's reference.
     */
    void notifyDocumentClosed(Document* doc);

    /** Drop all tracked listeners (e.g. on application shutdown). */
    void clearListeners();

private:
    juce::OwnedArray<ChainWindowListener> m_listeners;
};
