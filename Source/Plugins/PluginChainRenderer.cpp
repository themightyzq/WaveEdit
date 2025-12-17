/*
  ==============================================================================

    PluginChainRenderer.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "PluginChainRenderer.h"
#include <iostream>

//==============================================================================
PluginChainRenderer::RenderResult PluginChainRenderer::renderSelection(
    const juce::AudioBuffer<float>& sourceBuffer,
    PluginChain& chain,
    double sampleRate,
    int64_t startSample,
    int64_t numSamples,
    const ProgressCallback& progress)
{
    RenderResult result;

    // Validate chain first
    if (chain.isEmpty())
    {
        result.errorMessage = "Plugin chain is empty";
        return result;
    }

    // Report initial status
    if (progress && !progress(0.0f, "Initializing plugin chain..."))
    {
        result.cancelled = true;
        return result;
    }

    // Create offline plugin instances (NOTE: this should be called from message thread!)
    auto offlineChain = createOfflineChain(chain, sampleRate, m_blockSize);
    if (!offlineChain.isValid())
    {
        result.errorMessage = "Failed to create offline plugin instances";
        return result;
    }

    // Delegate to the version that takes a pre-created chain
    return renderWithOfflineChain(sourceBuffer, offlineChain, sampleRate, startSample, numSamples, progress);
}

//==============================================================================
PluginChainRenderer::RenderResult PluginChainRenderer::renderWithOfflineChain(
    const juce::AudioBuffer<float>& sourceBuffer,
    OfflineChain& offlineChain,
    double sampleRate,
    int64_t startSample,
    int64_t numSamples,
    const ProgressCallback& progress,
    int outputChannels,
    int64_t tailSamples)
{
    (void)sampleRate;  // Sample rate was used during chain creation
    RenderResult result;

    // Validate inputs
    if (numSamples <= 0)
    {
        result.errorMessage = "Invalid selection: numSamples must be positive";
        return result;
    }

    if (startSample < 0 || startSample >= sourceBuffer.getNumSamples())
    {
        result.errorMessage = "Invalid selection: startSample out of range";
        return result;
    }

    if (startSample + numSamples > sourceBuffer.getNumSamples())
    {
        result.errorMessage = "Invalid selection: extends beyond buffer end";
        return result;
    }

    if (!offlineChain.isValid())
    {
        result.errorMessage = "Invalid offline chain";
        return result;
    }

    // Ensure tailSamples is non-negative and reasonable (max 30 seconds of tail at 192kHz = ~5.7M samples)
    const int64_t maxTailSamples = 30 * 192000;  // 30 seconds at 192kHz
    tailSamples = juce::jmax(int64_t(0), juce::jmin(tailSamples, maxTailSamples));

    result.latencySamples = offlineChain.totalLatency;

    // Calculate buffer sizes with latency compensation
    const int sourceChannels = sourceBuffer.getNumChannels();
    // IMPORTANT: Most plugins (especially FabFilter) expect stereo input.
    // Always process with at least 2 channels to avoid crashes with stereo-only plugins.
    const int processChannels = juce::jmax(2, sourceChannels);
    // Include tail samples in total processing length
    const int64_t totalInputSamples = numSamples + offlineChain.totalLatency + tailSamples;

    // Safety check: ensure total processing size is reasonable (max ~500MB at stereo float)
    // This prevents memory allocation failures and potential int overflow issues
    const int64_t maxProcessingSamples = 128 * 1024 * 1024;  // 128M samples (~500MB for stereo float)
    if (totalInputSamples > maxProcessingSamples)
    {
        result.errorMessage = "Processing size too large. Try a smaller selection or shorter tail.";
        std::cerr << "[RENDERER] ERROR: totalInputSamples=" << totalInputSamples
                  << " exceeds max=" << maxProcessingSamples << std::endl;
        return result;
    }

    std::cerr << "[RENDERER] renderWithOfflineChain: sourceChannels=" << sourceChannels
              << ", processChannels=" << processChannels
              << ", totalInputSamples=" << totalInputSamples
              << ", tailSamples=" << tailSamples << std::endl;
    std::cerr.flush();

    // Create input buffer with prepended silence for latency compensation
    // and appended silence for tail capture
    // Use processChannels (at least 2) to ensure plugin compatibility
    juce::AudioBuffer<float> inputBuffer(processChannels, static_cast<int>(totalInputSamples));
    inputBuffer.clear();

    // Copy source audio after latency offset
    // For mono sources, copy to both channels (dual-mono for stereo plugins)
    for (int ch = 0; ch < processChannels; ++ch)
    {
        int srcCh = juce::jmin(ch, sourceChannels - 1);  // Use last channel for extra channels
        inputBuffer.copyFrom(
            ch,
            offlineChain.totalLatency,  // Destination offset
            sourceBuffer,
            srcCh,
            static_cast<int>(startSample),
            static_cast<int>(numSamples)
        );
    }
    // Note: tailSamples area after numSamples remains as silence (zeros)

    // Create output buffer (same size as input for processing)
    juce::AudioBuffer<float> outputBuffer(processChannels, static_cast<int>(totalInputSamples));
    outputBuffer.clear();

    // Process in chunks
    juce::MidiBuffer emptyMidi;
    int64_t samplesProcessed = 0;
    const int64_t totalToProcess = totalInputSamples;

    // Build status string - simple since we don't have chain access here
    juce::String statusMessage = "Processing plugin chain...";

    // Pre-allocate chunk buffer with full block size - ensures consistent memory layout
    // Use processChannels (at least 2) for stereo plugin compatibility
    juce::AudioBuffer<float> chunk(processChannels, m_blockSize);

    while (samplesProcessed < totalToProcess)
    {
        // Calculate chunk size
        const int64_t remaining = totalToProcess - samplesProcessed;
        const int chunkSize = static_cast<int>(juce::jmin(static_cast<int64_t>(m_blockSize), remaining));

        // Clear the chunk buffer and copy input data
        // This ensures any padding beyond chunkSize is zeroed (important for SIMD plugins)
        chunk.clear();
        for (int ch = 0; ch < processChannels; ++ch)
        {
            chunk.copyFrom(ch, 0, inputBuffer, ch, static_cast<int>(samplesProcessed), chunkSize);
        }

        // Process chunk through plugin chain
        if (!processBlock(offlineChain, chunk, emptyMidi))
        {
            result.errorMessage = "Plugin crashed during processing";
            return result;
        }

        // Copy processed chunk to output (only the valid samples, not padding)
        for (int ch = 0; ch < processChannels; ++ch)
        {
            outputBuffer.copyFrom(ch, static_cast<int>(samplesProcessed), chunk, ch, 0, chunkSize);
        }

        samplesProcessed += chunkSize;

        // Report progress
        float progressValue = static_cast<float>(samplesProcessed) / static_cast<float>(totalToProcess);
        if (progress && !progress(progressValue, statusMessage))
        {
            result.cancelled = true;
            return result;
        }
    }

    // Extract final result, discarding latency samples from the beginning
    // Determine output channel count: use outputChannels parameter if specified,
    // otherwise match source channel count (mono stays mono after processing)
    const int finalChannels = (outputChannels > 0) ? outputChannels : sourceChannels;
    // Output includes original samples + tail samples
    const int64_t outputLength = numSamples + tailSamples;
    result.processedBuffer.setSize(finalChannels, static_cast<int>(outputLength));
    for (int ch = 0; ch < finalChannels; ++ch)
    {
        // For stereo output from mono source, use both channels from the stereo processing
        int sourceCh = juce::jmin(ch, processChannels - 1);
        result.processedBuffer.copyFrom(
            ch,
            0,
            outputBuffer,
            sourceCh,
            offlineChain.totalLatency,
            static_cast<int>(outputLength)
        );
    }

    // Release plugin resources
    for (auto& instance : offlineChain.instances)
    {
        if (instance != nullptr)
        {
            instance->releaseResources();
        }
    }

    result.success = true;

    if (progress)
    {
        progress(1.0f, "Complete");
    }

    DBG("PluginChainRenderer: Rendered " + juce::String(numSamples) + " samples through " +
        juce::String(offlineChain.instances.size()) + " plugins (latency: " +
        juce::String(offlineChain.totalLatency) + " samples)");

    return result;
}

//==============================================================================
PluginChainRenderer::RenderResult PluginChainRenderer::renderEntireBuffer(
    const juce::AudioBuffer<float>& sourceBuffer,
    PluginChain& chain,
    double sampleRate,
    const ProgressCallback& progress)
{
    return renderSelection(sourceBuffer, chain, sampleRate, 0,
                          sourceBuffer.getNumSamples(), progress);
}

//==============================================================================
juce::String PluginChainRenderer::buildChainDescription(PluginChain& chain)
{
    juce::StringArray names;
    const int numPlugins = chain.getNumPlugins();

    for (int i = 0; i < numPlugins; ++i)
    {
        if (auto* node = chain.getPlugin(i))
        {
            if (!node->isBypassed())
            {
                names.add(node->getName());
            }
        }
    }

    if (names.isEmpty())
        return "Plugin Chain";

    return names.joinIntoString(", ");
}

//==============================================================================
PluginChainRenderer::OfflineChain PluginChainRenderer::createOfflineChain(
    PluginChain& chain,
    double sampleRate,
    int blockSize)
{
    std::cerr << "[RENDERER] createOfflineChain: Starting, sampleRate=" << sampleRate
              << ", blockSize=" << blockSize << std::endl;
    std::cerr.flush();

    OfflineChain offlineChain;

    auto& pluginManager = PluginManager::getInstance();
    const int numPlugins = chain.getNumPlugins();

    std::cerr << "[RENDERER] createOfflineChain: Processing " << numPlugins << " plugins" << std::endl;
    std::cerr.flush();

    for (int i = 0; i < numPlugins; ++i)
    {
        auto* sourceNode = chain.getPlugin(i);
        if (sourceNode == nullptr)
            continue;

        // Get plugin description and state from source
        const auto& description = sourceNode->getDescription();
        const bool bypassed = sourceNode->isBypassed();

        std::cerr << "[RENDERER] createOfflineChain: Plugin " << i << " - "
                  << description.name.toStdString() << " (bypassed=" << bypassed << ")" << std::endl;
        std::cerr.flush();

        std::cerr << "[RENDERER] createOfflineChain: Getting state..." << std::endl;
        std::cerr.flush();
        juce::MemoryBlock state = sourceNode->getState();
        std::cerr << "[RENDERER] createOfflineChain: State size=" << state.getSize() << std::endl;
        std::cerr.flush();

        // Create new instance
        std::cerr << "[RENDERER] createOfflineChain: Creating new plugin instance..." << std::endl;
        std::cerr.flush();
        auto instance = pluginManager.createPluginInstance(
            description,
            sampleRate,
            blockSize
        );

        if (instance == nullptr)
        {
            std::cerr << "[RENDERER] createOfflineChain: FAILED to create instance!" << std::endl;
            std::cerr.flush();
            DBG("PluginChainRenderer: Failed to create instance for " + description.name);
            continue;  // Skip failed plugins but continue with others
        }

        std::cerr << "[RENDERER] createOfflineChain: Instance created successfully" << std::endl;
        std::cerr.flush();

        // Apply state from source plugin
        if (state.getSize() > 0)
        {
            std::cerr << "[RENDERER] createOfflineChain: Applying state..." << std::endl;
            std::cerr.flush();
            instance->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            std::cerr << "[RENDERER] createOfflineChain: State applied" << std::endl;
            std::cerr.flush();
        }

        // IMPORTANT: Set non-realtime mode BEFORE prepareToPlay
        // Some plugins (like FabFilter) allocate different buffers based on this flag
        std::cerr << "[RENDERER] createOfflineChain: Setting non-realtime mode..." << std::endl;
        std::cerr.flush();
        instance->setNonRealtime(true);
        std::cerr << "[RENDERER] createOfflineChain: Non-realtime set" << std::endl;
        std::cerr.flush();

        // CRITICAL: Configure plugin for stereo processing BEFORE prepareToPlay
        // Many plugins (like FabFilter Pro-Q 4) require stereo even for mono files
        // This must be called BEFORE prepareToPlay() or the plugin won't allocate
        // the correct internal buffers
        const int processChannels = 2;  // Always use stereo for plugin processing
        std::cerr << "[RENDERER] createOfflineChain: Setting play config for " << processChannels << " channels..." << std::endl;
        std::cerr.flush();
        instance->setPlayConfigDetails(processChannels, processChannels, sampleRate, blockSize);
        std::cerr << "[RENDERER] createOfflineChain: Play config set" << std::endl;
        std::cerr.flush();

        // Prepare for playback with offline-compatible settings
        std::cerr << "[RENDERER] createOfflineChain: Calling prepareToPlay..." << std::endl;
        std::cerr.flush();
        instance->prepareToPlay(sampleRate, blockSize);
        std::cerr << "[RENDERER] createOfflineChain: prepareToPlay done" << std::endl;
        std::cerr.flush();

        // Track latency for non-bypassed plugins
        if (!bypassed)
        {
            int lat = instance->getLatencySamples();
            std::cerr << "[RENDERER] createOfflineChain: Plugin latency=" << lat << std::endl;
            std::cerr.flush();
            offlineChain.totalLatency += lat;
        }

        offlineChain.instances.push_back(std::move(instance));
        offlineChain.bypassed.push_back(bypassed);
        std::cerr << "[RENDERER] createOfflineChain: Plugin added to chain" << std::endl;
        std::cerr.flush();
    }

    std::cerr << "[RENDERER] createOfflineChain: Complete! Total plugins="
              << offlineChain.instances.size() << ", latency=" << offlineChain.totalLatency << std::endl;
    std::cerr.flush();

    DBG("PluginChainRenderer: Created offline chain with " +
        juce::String(offlineChain.instances.size()) + " plugins, total latency: " +
        juce::String(offlineChain.totalLatency) + " samples");

    return offlineChain;
}

//==============================================================================
bool PluginChainRenderer::processBlock(
    OfflineChain& offlineChain,
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midi)
{
    static int blockCounter = 0;
    blockCounter++;

    // Only log every 100 blocks to reduce spam
    bool shouldLog = (blockCounter <= 3) || (blockCounter % 100 == 0);

    if (shouldLog)
    {
        std::cerr << "[RENDERER] processBlock #" << blockCounter
                  << ": numChannels=" << buffer.getNumChannels()
                  << ", numSamples=" << buffer.getNumSamples() << std::endl;
        std::cerr.flush();
    }

    for (size_t i = 0; i < offlineChain.instances.size(); ++i)
    {
        auto& instance = offlineChain.instances[i];
        if (instance == nullptr || offlineChain.bypassed[i])
            continue;

        if (shouldLog)
        {
            std::cerr << "[RENDERER] processBlock: About to call plugin " << i
                      << " (" << instance->getName().toStdString() << ")" << std::endl;
            std::cerr.flush();
        }

        try
        {
            instance->processBlock(buffer, midi);
        }
        catch (const std::exception& e)
        {
            std::cerr << "[RENDERER] processBlock: EXCEPTION in plugin " << i
                      << ": " << e.what() << std::endl;
            std::cerr.flush();
            DBG("PluginChainRenderer: Exception in plugin processBlock: " + juce::String(e.what()));
            return false;
        }
        catch (...)
        {
            std::cerr << "[RENDERER] processBlock: UNKNOWN EXCEPTION in plugin " << i << std::endl;
            std::cerr.flush();
            DBG("PluginChainRenderer: Unknown exception in plugin processBlock");
            return false;
        }

        if (shouldLog)
        {
            std::cerr << "[RENDERER] processBlock: Plugin " << i << " completed" << std::endl;
            std::cerr.flush();
        }
    }

    return true;
}
