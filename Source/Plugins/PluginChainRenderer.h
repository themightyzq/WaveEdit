/*
  ==============================================================================

    PluginChainRenderer.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginChain.h"
#include "PluginManager.h"
#include "../Automation/AutomationData.h"
#include "../Utils/ProgressCallback.h"

class AutomationManager;

/**
 * Offline renderer for processing audio through a plugin chain.
 *
 * This class creates independent plugin instances for offline rendering,
 * allowing the main plugin chain to continue real-time processing without
 * interference.
 *
 * Thread Safety:
 * - renderSelection() is designed to run on background threads
 * - Creates internal copies of plugins to avoid conflicts with real-time audio
 * - Progress callback is invoked from background thread; must be thread-safe
 *
 * Latency Compensation:
 * - Calculates total chain latency from all non-bypassed plugins
 * - Prepends silence to input, processes, then discards initial samples
 * - Result buffer has same length as input, properly aligned
 *
 * Usage:
 * @code
 * PluginChainRenderer renderer;
 * auto result = renderer.renderSelection(
 *     sourceBuffer,
 *     pluginChain,
 *     sampleRate,
 *     startSample,
 *     numSamples,
 *     [](float progress, const juce::String& status) {
 *         return !userCancelled;
 *     }
 * );
 * if (result.success)
 *     bufferManager.replaceRange(startSample, numSamples, result.processedBuffer);
 * @endcode
 */
class PluginChainRenderer
{
public:
    //==============================================================================
    /**
     * Result structure for rendering operations.
     */
    struct RenderResult
    {
        bool success = false;                      ///< true if rendering completed successfully
        bool cancelled = false;                    ///< true if user cancelled operation
        juce::String errorMessage;                 ///< Error description if !success && !cancelled
        juce::AudioBuffer<float> processedBuffer;  ///< Processed audio (valid only if success)
        int latencySamples = 0;                    ///< Total chain latency for reference
    };

    //==============================================================================
    PluginChainRenderer() = default;
    ~PluginChainRenderer() = default;

    //==============================================================================
    /**
     * Renders a selection through the plugin chain.
     *
     * Creates independent plugin instances to avoid real-time thread conflicts.
     * Processes audio in chunks with progress reporting.
     *
     * @param sourceBuffer Source audio buffer (read-only)
     * @param chain Plugin chain to render through (state is copied, not modified)
     * @param sampleRate Sample rate for processing
     * @param startSample Selection start in source buffer
     * @param numSamples Number of samples to process
     * @param progress Progress callback (returns false to cancel)
     * @return RenderResult with processed audio or error information
     *
     * Thread Safety: Safe to call from background thread.
     */
    RenderResult renderSelection(
        const juce::AudioBuffer<float>& sourceBuffer,
        PluginChain& chain,
        double sampleRate,
        int64_t startSample,
        int64_t numSamples,
        const ProgressCallback& progress);

    //==============================================================================
    /**
     * Renders an entire buffer through the plugin chain.
     * Convenience wrapper around renderSelection for full-file processing.
     *
     * @param sourceBuffer Source audio buffer
     * @param chain Plugin chain to render through
     * @param sampleRate Sample rate for processing
     * @param progress Progress callback
     * @return RenderResult with processed audio
     */
    RenderResult renderEntireBuffer(
        const juce::AudioBuffer<float>& sourceBuffer,
        PluginChain& chain,
        double sampleRate,
        const ProgressCallback& progress);

    //==============================================================================
    // Configuration

    /**
     * Sets the processing block size (default: 8192 samples).
     * Larger blocks = fewer callbacks, smaller blocks = better progress granularity.
     */
    void setBlockSize(int blockSize) { m_blockSize = juce::jmax(64, blockSize); }

    /**
     * Gets the current processing block size.
     */
    int getBlockSize() const { return m_blockSize; }

    //==============================================================================
    /**
     * Builds a human-readable description of the plugin chain.
     * Used for undo action names.
     *
     * @param chain The plugin chain to describe
     * @return String like "Plugin1, Plugin2, Plugin3"
     */
    static juce::String buildChainDescription(PluginChain& chain);

//==============================================================================
    /**
     * Structure holding offline plugin instances.
     * Made public so caller can create chain on message thread and pass to background.
     */
    /**
     * One automation lane frozen for an offline render: the curve's points
     * are copied on the message thread, so the render thread never touches
     * the live AutomationManager.
     */
    struct OfflineAutomationLane
    {
        int instanceIndex = -1;                  ///< Index into OfflineChain::instances
        int parameterIndex = -1;                 ///< Index into that instance's getParameters()
        std::vector<AutomationPoint> points;     ///< Sorted, non-empty
    };

    struct OfflineChain
    {
        std::vector<std::unique_ptr<juce::AudioPluginInstance>> instances;
        std::vector<bool> bypassed;  ///< Bypass state per plugin
        std::vector<int> chainIndices;  ///< Source chain slot of each instance (a failed
                                        ///< instantiation is skipped, so slot != index)
        int totalLatency = 0;

        /** Recorded parameter automation to render (see captureAutomation()). */
        std::vector<OfflineAutomationLane> automation;

        /** File position (in samples) of index 0 of the source buffer passed to
            renderWithOfflineChain(). Automation is evaluated at file time, so a
            caller rendering a copied-out selection sets this to the selection
            start. */
        int64_t automationFileOffset = 0;

        bool isValid() const { return !instances.empty(); }
    };

    /**
     * Snapshot the enabled, non-empty lanes of @p automation that target a
     * plugin present in @p offlineChain (matched through chainIndices) into
     * offlineChain.automation. Message thread only.
     */
    static void captureAutomation(OfflineChain& offlineChain,
                                  const AutomationManager& automation);

    /** Block size used while automation is rendered: the realtime path
        applies automation once per device block, so render in blocks of
        about that size rather than the 8192-sample default. */
    static constexpr int kAutomationBlockSize = 512;

    /**
     * Creates offline plugin instances from chain descriptions.
     * Each plugin is prepared independently for offline rendering.
     *
     * IMPORTANT: Must be called from message thread (plugin instantiation requirement).
     *
     * @param chain Source chain to copy from
     * @param sampleRate Sample rate for processing
     * @param blockSize Block size for processing
     * @return OfflineChain with instances ready for processing, or empty on failure
     */
    static OfflineChain createOfflineChain(
        PluginChain& chain,
        double sampleRate,
        int blockSize);

    /**
     * Renders using a pre-created offline chain.
     * Call this from background thread after creating chain on message thread.
     *
     * @param sourceBuffer Source audio buffer
     * @param offlineChain Pre-created offline chain (from createOfflineChain)
     * @param sampleRate Sample rate
     * @param startSample Start position
     * @param numSamples Number of samples
     * @param progress Progress callback
     * @param outputChannels Number of output channels (0 = match source, 2 = force stereo)
     * @param tailSamples Number of additional samples to capture effect tail (default: 0)
     * @return RenderResult (processedBuffer will be numSamples + tailSamples long)
     */
    RenderResult renderWithOfflineChain(
        const juce::AudioBuffer<float>& sourceBuffer,
        OfflineChain& offlineChain,
        double sampleRate,
        int64_t startSample,
        int64_t numSamples,
        const ProgressCallback& progress,
        int outputChannels = 0,
        int64_t tailSamples = 0);

private:
    //==============================================================================
    int m_blockSize = 8192;  ///< Processing block size

    // Per-instance block counter used only to throttle render logging. Was a
    // function-local `static int`, which is a write-write race if an offline
    // render and a realtime preview run concurrently on separate renderer
    // instances (L13). A plain member is unshared and race-free.
    int m_blockCounter = 0;

    /**
     * Process a single block through the offline chain.
     * Handles bypass and error recovery.
     *
     * @param offlineChain The offline plugin chain
     * @param buffer Buffer to process in-place
     * @param midi Empty MIDI buffer
     * @return true if successful, false if a plugin crashed
     */
    bool processBlock(
        OfflineChain& offlineChain,
        juce::AudioBuffer<float>& buffer,
        juce::MidiBuffer& midi);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginChainRenderer)
};
