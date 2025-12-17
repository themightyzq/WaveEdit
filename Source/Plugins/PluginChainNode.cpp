/*
  ==============================================================================

    PluginChainNode.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "PluginChainNode.h"

//==============================================================================
PluginChainNode::PluginChainNode(std::unique_ptr<juce::AudioPluginInstance> instance,
                                  const juce::PluginDescription& description)
    : m_instance(std::move(instance)),
      m_description(description)
{
    jassert(m_instance != nullptr);
}

PluginChainNode::~PluginChainNode()
{
    releaseResources();
}

//==============================================================================
void PluginChainNode::prepareToPlay(double sampleRate, int blockSize)
{
    if (m_instance == nullptr)
        return;

    m_sampleRate = sampleRate;
    m_blockSize = blockSize;

    m_instance->setPlayConfigDetails(
        m_instance->getTotalNumInputChannels(),
        m_instance->getTotalNumOutputChannels(),
        sampleRate,
        blockSize
    );

    m_instance->prepareToPlay(sampleRate, blockSize);
    m_prepared = true;

    DBG("PluginChainNode: Prepared " + m_description.name +
        " @ " + juce::String(sampleRate) + "Hz, " +
        juce::String(blockSize) + " samples");
}

void PluginChainNode::releaseResources()
{
    if (m_instance != nullptr && m_prepared)
    {
        m_instance->releaseResources();
        m_prepared = false;
    }
}

int PluginChainNode::getLatencySamples() const
{
    if (m_instance == nullptr)
        return 0;

    return m_instance->getLatencySamples();
}

//==============================================================================
void PluginChainNode::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (m_instance == nullptr || !m_prepared)
        return;

    // Handle pending state swap (lock-free)
    if (m_stateSwapPending.exchange(false, std::memory_order_acq_rel))
    {
        // Swap pending state to active state
        std::swap(m_pendingState, m_activeState);

        // Apply state to plugin
        if (m_activeState.getSize() > 0)
        {
            m_instance->setStateInformation(
                m_activeState.getData(),
                static_cast<int>(m_activeState.getSize())
            );
        }
    }

    // Skip processing if bypassed
    if (m_bypassed.load(std::memory_order_acquire))
        return;

    // Process audio through plugin
    m_instance->processBlock(buffer, midi);
}

//==============================================================================
void PluginChainNode::queueStateUpdate(const juce::MemoryBlock& state)
{
    // Protect write to pending state
    const juce::SpinLock::ScopedLockType lock(m_stateLock);

    m_pendingState = state;
    m_stateSwapPending.store(true, std::memory_order_release);
}

juce::MemoryBlock PluginChainNode::getState() const
{
    juce::MemoryBlock state;

    if (m_instance != nullptr)
    {
        m_instance->getStateInformation(state);
    }

    return state;
}

void PluginChainNode::setState(const juce::MemoryBlock& state)
{
    if (m_instance == nullptr || state.getSize() == 0)
        return;

    m_instance->setStateInformation(
        state.getData(),
        static_cast<int>(state.getSize())
    );
}

//==============================================================================
bool PluginChainNode::hasEditor() const
{
    if (m_instance == nullptr)
        return false;

    return m_instance->hasEditor();
}

std::unique_ptr<juce::AudioProcessorEditor> PluginChainNode::createEditor()
{
    if (m_instance == nullptr)
        return nullptr;

    return std::unique_ptr<juce::AudioProcessorEditor>(m_instance->createEditor());
}
