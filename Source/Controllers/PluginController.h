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
class AutomationLanesPanel;

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

    //==========================================================================
    // Plugin Parameter Automation — Phase 5

    /**
     * Open the Automation Lanes panel for the document. If already open,
     * brings it to front. Optionally scrolls to a specific lane after
     * opening (pass -1 / -1 to skip the scroll).
     */
    void showAutomationLanesPanel(Document* currentDoc,
                                  juce::ApplicationCommandManager& commandManager,
                                  int scrollToPluginIndex = -1,
                                  int scrollToParameterIndex = -1);

private:
    juce::OwnedArray<ChainWindowListener> m_listeners;

    // Tracks the currently-open Automation Lanes window (if any). When the
    // user closes the window it deletes itself, so this auto-clears.
    juce::Component::SafePointer<juce::DocumentWindow> m_automationLanesWindow;
    juce::Component::SafePointer<AutomationLanesPanel> m_automationLanesPanel;
};
