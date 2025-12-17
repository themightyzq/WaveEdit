/*
  ==============================================================================

    PluginChain.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include "PluginChainNode.h"
#include "PluginManager.h"
#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <mutex>

/**
 * Manages an ordered chain of effect plugins for processing audio.
 *
 * Thread Safety:
 * - processBlock() is real-time safe (no locks, no allocations)
 * - Chain modification (add/remove/reorder) must be done from message thread
 * - Uses a swap mechanism to update the chain atomically
 *
 * Usage:
 * @code
 * PluginChain chain;
 *
 * // Prepare for playback
 * chain.prepareToPlay(44100.0, 512);
 *
 * // Add plugins from message thread
 * chain.addPlugin(pluginDescription);
 *
 * // In audio callback
 * chain.processBlock(buffer, midi);
 *
 * // Get total latency for delay compensation
 * int latency = chain.getTotalLatency();
 * @endcode
 */
class PluginChain : public juce::ChangeBroadcaster
{
public:
    //==============================================================================
    PluginChain();
    ~PluginChain() override;

    //==============================================================================
    // Audio Processing Setup

    /**
     * Prepare all plugins for playback.
     * Must be called before processBlock().
     * @param sampleRate The sample rate to use
     * @param blockSize Maximum block size
     */
    void prepareToPlay(double sampleRate, int blockSize);

    /** Release resources when playback stops */
    void releaseResources();

    /** Check if the chain is prepared for playback */
    bool isPrepared() const { return m_prepared; }

    //==============================================================================
    // Audio Processing (Real-Time Safe)

    /**
     * Process audio through the entire chain.
     * This is real-time safe - no locks, no allocations.
     * @param buffer Audio buffer to process in-place
     * @param midi MIDI buffer (typically empty for effects)
     */
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    //==============================================================================
    // Chain Management (Message Thread Only)

    /**
     * Add a plugin to the end of the chain.
     * @param description Plugin to add
     * @return Index of added plugin, or -1 on failure
     */
    int addPlugin(const juce::PluginDescription& description);

    /**
     * Insert a plugin at a specific position.
     * @param description Plugin to add
     * @param index Position to insert at (clamped to valid range)
     * @return Index of inserted plugin, or -1 on failure
     */
    int insertPlugin(const juce::PluginDescription& description, int index);

    /**
     * Remove a plugin from the chain.
     * @param index Plugin index to remove
     * @return true if removed successfully
     */
    bool removePlugin(int index);

    /**
     * Move a plugin to a new position.
     * @param fromIndex Current position
     * @param toIndex Target position
     * @return true if moved successfully
     */
    bool movePlugin(int fromIndex, int toIndex);

    /** Remove all plugins from the chain */
    void clear();

    //==============================================================================
    // Chain Access

    /** Get the number of plugins in the chain */
    int getNumPlugins() const;

    /** Get a plugin node by index (nullptr if invalid) */
    PluginChainNode* getPlugin(int index);
    const PluginChainNode* getPlugin(int index) const;

    /** Check if the chain is empty */
    bool isEmpty() const { return getNumPlugins() == 0; }

    //==============================================================================
    // Bypass Control

    /** Bypass all plugins in the chain */
    void setAllBypassed(bool bypassed);

    /** Check if all plugins are bypassed */
    bool areAllBypassed() const;

    //==============================================================================
    // Latency

    /** Get total latency of the chain in samples */
    int getTotalLatency() const;

    //==============================================================================
    // State Serialization

    /**
     * Save the entire chain state to XML.
     * Includes plugin identifiers, bypass states, and plugin states.
     */
    std::unique_ptr<juce::XmlElement> saveToXml() const;

    /**
     * Load chain state from XML.
     * @param xml The XML element to load from
     * @return true if loaded successfully
     */
    bool loadFromXml(const juce::XmlElement& xml);

    /**
     * Save chain to JSON for use in presets.
     */
    juce::var saveToJson() const;

    /**
     * Load chain from JSON preset.
     */
    bool loadFromJson(const juce::var& json);

private:
    //==============================================================================
    using NodeList = std::vector<std::unique_ptr<PluginChainNode>>;

    // The active chain used by audio thread
    NodeList m_nodes;

    // SpinLock for audio-thread-safe synchronization
    // Used in processBlock and all chain modification methods
    juce::SpinLock m_processLock;

    // Mutex for chain modifications from message thread
    mutable std::mutex m_chainMutex;

    // Playback state
    double m_sampleRate = 44100.0;
    int m_blockSize = 512;
    bool m_prepared = false;

    // Create a plugin instance from description
    std::unique_ptr<PluginChainNode> createNode(const juce::PluginDescription& description);

    // Notify listeners that chain changed
    void notifyChainChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChain)
};
