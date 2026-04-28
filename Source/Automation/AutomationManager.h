/*
  ==============================================================================

    AutomationManager.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Manages all automation lanes for one document.
    Provides lock-free parameter application during audio playback.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "AutomationData.h"
#include <vector>

class PluginChain;

/**
 * Manages all automation lanes for one document.
 * Owned by Document (one per open file).
 *
 * Thread safety:
 * - Lane management (add/remove/edit): message thread only
 * - applyAutomation(): called from audio thread, reads lock-free
 *   from each lane's AutomationCurve
 */
class AutomationManager
{
public:
    AutomationManager();
    ~AutomationManager();

    //==========================================================================
    // Lane management (message thread)

    /** Add a new automation lane for a plugin parameter. */
    AutomationLane& addLane(int pluginIndex, int parameterIndex,
                            const juce::String& pluginName,
                            const juce::String& parameterName,
                            const juce::String& parameterID);

    /** Remove a lane by index. */
    void removeLane(int laneIndex);

    /** Get lane by index (nullptr if out of range). */
    AutomationLane* getLane(int index);

    /** Find lane for a specific plugin parameter. */
    AutomationLane* findLane(int pluginIndex, int parameterIndex);

    /** Get all lanes (read-only). */
    const std::vector<AutomationLane>& getLanes() const;

    /** Get number of lanes. */
    int getNumLanes() const;

    /** Clear all automation lanes. */
    void clearAll();

    /** Remove all lanes for a specific plugin (when plugin is removed from chain). */
    void removePluginLanes(int pluginIndex);

    /** Shift plugin indices when a plugin is inserted or removed from the chain. */
    void shiftPluginIndices(int fromIndex, int delta);

    //==========================================================================
    // Playback (called from audio thread context)

    /**
     * Apply all enabled automation at the given time position.
     * Called from the audio thread BEFORE plugin processing.
     *
     * Lock-free: reads from each AutomationCurve's atomic point list.
     */
    void applyAutomation(PluginChain& chain, double timeInSeconds);

    //==========================================================================
    // Global state

    bool isAnyLaneRecording() const;
    void stopAllRecording();

    //==========================================================================
    // Serialization

    juce::var toVar() const;
    void fromVar(const juce::var& data);

    //==========================================================================
    // Listener for UI updates

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void automationDataChanged() = 0;
    };

    void addListener(Listener* l);
    void removeListener(Listener* l);

private:
    std::vector<AutomationLane> m_lanes;
    juce::ListenerList<Listener> m_listeners;

    void notifyListeners();
};
