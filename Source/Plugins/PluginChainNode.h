/*
  ==============================================================================

    PluginChainNode.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>

/**
 * Represents a single plugin slot in the effect chain.
 *
 * Thread Safety:
 * - m_bypassed is atomic for lock-free access from audio thread
 * - Plugin instance operations (including state) must be done from the message
 *   thread. State is NEVER applied on the audio thread: setStateInformation()
 *   on a third-party plugin can allocate/parse/lock. Restore state before a
 *   node is audio-visible (PluginChain::addPluginConfigured, C4/L-H1).
 */
class PluginChainNode
{
public:
    //==============================================================================
    /**
     * Create a plugin chain node with an instance.
     * @param instance The plugin instance (takes ownership)
     * @param description The plugin description for identification
     */
    PluginChainNode(std::unique_ptr<juce::AudioPluginInstance> instance,
                    const juce::PluginDescription& description);

    /** Destructor - releases plugin instance */
    ~PluginChainNode();

    //==============================================================================
    // Plugin Access

    /** Get the plugin instance (may be nullptr if failed to load) */
    juce::AudioPluginInstance* getPlugin() { return m_instance.get(); }
    const juce::AudioPluginInstance* getPlugin() const { return m_instance.get(); }

    /** Get the plugin description */
    const juce::PluginDescription& getDescription() const { return m_description; }

    /** Get the plugin name */
    juce::String getName() const { return m_description.name; }

    /** Get unique identifier string */
    juce::String getIdentifier() const { return m_description.createIdentifierString(); }

    //==============================================================================
    // Bypass Control

    /** Check if this node is bypassed (lock-free) */
    bool isBypassed() const { return m_bypassed.load(std::memory_order_acquire); }

    /** Set bypass state (lock-free) */
    void setBypassed(bool shouldBypass) { m_bypassed.store(shouldBypass, std::memory_order_release); }

    /** Toggle bypass state */
    void toggleBypass() { m_bypassed.store(!m_bypassed.load(std::memory_order_acquire), std::memory_order_release); }

    //==============================================================================
    // Audio Processing Setup

    /**
     * Prepare the plugin for playback.
     * Must be called from message thread before processing.
     */
    void prepareToPlay(double sampleRate, int blockSize);

    /** Release resources when playback stops */
    void releaseResources();

    /** Get the plugin's latency in samples */
    int getLatencySamples() const;

    //==============================================================================
    // Audio Processing (Real-Time Safe)

    /**
     * Process audio through this node.
     * Handles bypass and pending state updates.
     * @param buffer Audio buffer to process in-place
     * @param midi MIDI buffer (typically empty for effects)
     */
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi);

    //==============================================================================
    // State Management (Message Thread Only)

    /**
     * Get current plugin state.
     * Must be called from message thread.
     */
    juce::MemoryBlock getState() const;

    /**
     * Set plugin state immediately.
     * Must be called from message thread when audio is not running.
     */
    void setState(const juce::MemoryBlock& state);

    //==============================================================================
    // Editor Support

    /** Check if plugin has a custom editor */
    bool hasEditor() const;

    /** Create the plugin editor (must be called from message thread) */
    std::unique_ptr<juce::AudioProcessorEditor> createEditor();

private:
    //==============================================================================
    std::unique_ptr<juce::AudioPluginInstance> m_instance;
    juce::PluginDescription m_description;

    // Atomic bypass flag for lock-free audio thread access
    std::atomic<bool> m_bypassed{false};

    // Playback state
    double m_sampleRate = 44100.0;
    int m_blockSize = 512;
    bool m_prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChainNode)
};
