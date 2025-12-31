/*
  ==============================================================================

    PluginChain.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "PluginChain.h"

//==============================================================================
PluginChain::PluginChain()
{
    // Initialize with empty chain
    m_messageThreadChain = std::make_shared<NodeList>();
    m_activeChain.store(m_messageThreadChain.get(), std::memory_order_release);
}

PluginChain::~PluginChain()
{
    releaseResources();
    clear();

    // Set active chain to nullptr to prevent audio thread access during destruction
    m_activeChain.store(nullptr, std::memory_order_release);

    // Clean up any pending deletes
    processPendingDeletes();
}

//==============================================================================
void PluginChain::prepareToPlay(double sampleRate, int blockSize)
{
    m_sampleRate = sampleRate;
    m_blockSize = blockSize;

    // Get current chain (message thread only)
    auto chain = m_messageThreadChain;
    if (chain != nullptr)
    {
        for (auto& node : *chain)
        {
            if (node != nullptr)
            {
                node->prepareToPlay(sampleRate, blockSize);
            }
        }
    }

    m_prepared.store(true, std::memory_order_release);

    DBG("PluginChain: Prepared " + juce::String(chain ? chain->size() : 0) + " plugins @ " +
        juce::String(sampleRate) + "Hz");
}

void PluginChain::releaseResources()
{
    // Get current chain (message thread only)
    auto chain = m_messageThreadChain;
    if (chain != nullptr)
    {
        for (auto& node : *chain)
        {
            if (node != nullptr)
            {
                node->releaseResources();
            }
        }
    }

    m_prepared.store(false, std::memory_order_release);
}

//==============================================================================
void PluginChain::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // LOCK-FREE: Atomic load of active chain pointer
    // Audio thread never blocks - worst case it processes with stale chain
    NodeList* chain = m_activeChain.load(std::memory_order_acquire);

    if (chain == nullptr)
        return;

    // Process through all nodes in the chain
    // The chain pointer is stable - modifications create new chains
    for (auto& node : *chain)
    {
        if (node != nullptr)
        {
            node->processBlock(buffer, midi);
        }
    }
}

//==============================================================================
PluginChain::ChainPtr PluginChain::copyCurrentChain() const
{
    // Create a new chain with copies of all node shared_ptrs
    auto newChain = std::make_shared<NodeList>();

    if (m_messageThreadChain != nullptr)
    {
        newChain->reserve(m_messageThreadChain->size());
        for (const auto& node : *m_messageThreadChain)
        {
            newChain->push_back(node);
        }
    }

    return newChain;
}

void PluginChain::publishChain(ChainPtr newChain)
{
    // Save old chain for deferred deletion
    auto oldChain = m_messageThreadChain;

    // Update message thread's reference
    m_messageThreadChain = newChain;

    // Atomically swap the active chain pointer
    // Audio thread will see the new chain on next processBlock() call
    m_activeChain.store(newChain.get(), std::memory_order_release);

    // Queue old chain for deferred deletion
    // We can't delete immediately because audio thread might still be using it
    if (oldChain != nullptr)
    {
        std::lock_guard<std::mutex> lock(m_pendingDeleteMutex);
        m_pendingDeletes.push_back(oldChain);
    }

    // Clean up old chains that are no longer in use
    processPendingDeletes();
}

void PluginChain::processPendingDeletes()
{
    std::lock_guard<std::mutex> lock(m_pendingDeleteMutex);

    // Generation-based deletion: Keep the last kMinPendingGenerations chains alive
    // to guarantee the audio thread has finished iterating before we delete.
    //
    // The audio thread holds a raw pointer (not shared_ptr), so use_count() doesn't
    // tell us if it's still iterating. Instead, we wait a minimum number of audio
    // callbacks (~93ms at 44.1kHz/512 samples with 8 generations) before deletion.
    while (m_pendingDeletes.size() > kMinPendingGenerations)
    {
        m_pendingDeletes.erase(m_pendingDeletes.begin());
    }
}

//==============================================================================
std::unique_ptr<PluginChainNode> PluginChain::createNode(const juce::PluginDescription& description)
{
    try
    {
        auto& pluginManager = PluginManager::getInstance();

        auto instance = pluginManager.createPluginInstance(
            description,
            m_sampleRate,
            m_blockSize
        );

        if (instance == nullptr)
        {
            DBG("PluginChain: Failed to create plugin instance for " + description.name);
            return nullptr;
        }

        auto node = std::make_unique<PluginChainNode>(std::move(instance), description);

        // Prepare if chain is already prepared
        if (m_prepared.load(std::memory_order_acquire))
        {
            node->prepareToPlay(m_sampleRate, m_blockSize);
        }

        return node;
    }
    catch (const std::exception& e)
    {
        DBG("PluginChain: Exception creating node for " + description.name + ": " + juce::String(e.what()));
        return nullptr;
    }
    catch (...)
    {
        DBG("PluginChain: Unknown exception creating node for " + description.name);
        return nullptr;
    }
}

int PluginChain::addPlugin(const juce::PluginDescription& description)
{
    return insertPlugin(description, getNumPlugins());
}

int PluginChain::insertPlugin(const juce::PluginDescription& description, int index)
{
    // Create node first (this may involve plugin loading - done outside any locks)
    auto node = createNode(description);
    if (node == nullptr)
        return -1;

    // Convert to shared_ptr for COW storage
    auto sharedNode = std::shared_ptr<PluginChainNode>(std::move(node));

    // Copy current chain
    auto newChain = copyCurrentChain();

    // Clamp index to valid range
    index = juce::jlimit(0, static_cast<int>(newChain->size()), index);

    // Insert at the specified position
    newChain->insert(newChain->begin() + index, sharedNode);

    // Atomically publish new chain
    publishChain(newChain);

    DBG("PluginChain: Added " + description.name + " at index " + juce::String(index));

    notifyChainChanged();
    return index;
}

bool PluginChain::removePlugin(int index)
{
    if (m_messageThreadChain == nullptr)
        return false;

    if (index < 0 || index >= static_cast<int>(m_messageThreadChain->size()))
        return false;

    // Get name for logging before we modify
    auto& node = (*m_messageThreadChain)[static_cast<size_t>(index)];
    juce::String name = node ? node->getName() : "Unknown";

    // IMPORTANT: Do NOT call releaseResources() here!
    // The audio thread may still be iterating the chain and calling processBlock()
    // on this node. We use COW (Copy-on-Write) to safely remove it:
    // 1. Copy the chain
    // 2. Remove node from copy
    // 3. Atomically swap to new chain
    // 4. Node cleanup happens via delayed deletion (kMinPendingGenerations)
    //
    // The node's destructor will handle cleanup when the old chain is finally deleted.

    // Copy current chain
    auto newChain = copyCurrentChain();

    // Remove from the copy
    newChain->erase(newChain->begin() + index);

    // Atomically publish new chain
    publishChain(newChain);

    DBG("PluginChain: Removed " + name + " from index " + juce::String(index));

    notifyChainChanged();
    return true;
}

bool PluginChain::movePlugin(int fromIndex, int toIndex)
{
    if (m_messageThreadChain == nullptr)
        return false;

    const int size = static_cast<int>(m_messageThreadChain->size());
    // Note: toIndex can be == size when moving down (the caller passes m_index + 2)
    // because after the internal adjustment (--toIndex when toIndex > fromIndex),
    // it will become size - 1 which is valid.
    if (fromIndex < 0 || fromIndex >= size || toIndex < 0 || toIndex > size)
        return false;

    if (fromIndex == toIndex)
        return true;

    // Copy current chain
    auto newChain = copyCurrentChain();

    // Move the node
    auto node = (*newChain)[static_cast<size_t>(fromIndex)];
    newChain->erase(newChain->begin() + fromIndex);

    // Adjust target index if needed
    if (toIndex > fromIndex)
        --toIndex;

    newChain->insert(newChain->begin() + toIndex, node);

    // Atomically publish new chain
    publishChain(newChain);

    DBG("PluginChain: Moved plugin from " + juce::String(fromIndex) + " to " + juce::String(toIndex));

    notifyChainChanged();
    return true;
}

void PluginChain::clear()
{
    if (m_messageThreadChain == nullptr || m_messageThreadChain->empty())
        return;

    // IMPORTANT: Do NOT call releaseResources() here!
    // The audio thread may still be iterating the chain and calling processBlock()
    // on these nodes. We use COW to safely clear:
    // 1. Create empty chain
    // 2. Atomically swap to empty chain
    // 3. Old nodes are cleaned up via delayed deletion (kMinPendingGenerations)
    //
    // The nodes' destructors will handle cleanup when the old chain is finally deleted.

    // Create empty chain
    auto newChain = std::make_shared<NodeList>();

    // Atomically publish empty chain
    publishChain(newChain);

    DBG("PluginChain: Cleared all plugins");

    notifyChainChanged();
}

//==============================================================================
int PluginChain::getNumPlugins() const
{
    if (m_messageThreadChain == nullptr)
        return 0;
    return static_cast<int>(m_messageThreadChain->size());
}

PluginChainNode* PluginChain::getPlugin(int index)
{
    if (m_messageThreadChain == nullptr)
        return nullptr;

    if (index < 0 || index >= static_cast<int>(m_messageThreadChain->size()))
        return nullptr;

    return (*m_messageThreadChain)[static_cast<size_t>(index)].get();
}

const PluginChainNode* PluginChain::getPlugin(int index) const
{
    if (m_messageThreadChain == nullptr)
        return nullptr;

    if (index < 0 || index >= static_cast<int>(m_messageThreadChain->size()))
        return nullptr;

    return (*m_messageThreadChain)[static_cast<size_t>(index)].get();
}

//==============================================================================
void PluginChain::setAllBypassed(bool bypassed)
{
    if (m_messageThreadChain == nullptr)
        return;

    for (auto& node : *m_messageThreadChain)
    {
        if (node != nullptr)
        {
            node->setBypassed(bypassed);
        }
    }
}

bool PluginChain::areAllBypassed() const
{
    if (m_messageThreadChain == nullptr || m_messageThreadChain->empty())
        return false;

    for (const auto& node : *m_messageThreadChain)
    {
        if (node != nullptr && !node->isBypassed())
        {
            return false;
        }
    }

    return true;
}

//==============================================================================
int PluginChain::getTotalLatency() const
{
    if (m_messageThreadChain == nullptr)
        return 0;

    int totalLatency = 0;
    for (const auto& node : *m_messageThreadChain)
    {
        if (node != nullptr && !node->isBypassed())
        {
            totalLatency += node->getLatencySamples();
        }
    }

    return totalLatency;
}

//==============================================================================
std::unique_ptr<juce::XmlElement> PluginChain::saveToXml() const
{
    auto xml = std::make_unique<juce::XmlElement>("PluginChain");
    xml->setAttribute("version", 1);

    if (m_messageThreadChain == nullptr)
        return xml;

    for (const auto& node : *m_messageThreadChain)
    {
        if (node == nullptr)
            continue;

        auto pluginXml = xml->createNewChildElement("Plugin");

        // Save plugin description
        const auto& desc = node->getDescription();
        pluginXml->setAttribute("name", desc.name);
        pluginXml->setAttribute("identifier", desc.createIdentifierString());
        pluginXml->setAttribute("manufacturer", desc.manufacturerName);
        pluginXml->setAttribute("version", desc.version);
        pluginXml->setAttribute("format", desc.pluginFormatName);

        // Save bypass state
        pluginXml->setAttribute("bypassed", node->isBypassed());

        // Save plugin state
        auto state = node->getState();
        if (state.getSize() > 0)
        {
            pluginXml->setAttribute("state", state.toBase64Encoding());
        }
    }

    return xml;
}

bool PluginChain::loadFromXml(const juce::XmlElement& xml)
{
    if (xml.getTagName() != "PluginChain")
        return false;

    // Clear existing chain
    clear();

    auto& pluginManager = PluginManager::getInstance();

    // Load each plugin
    for (auto* pluginXml : xml.getChildWithTagNameIterator("Plugin"))
    {
        juce::String identifier = pluginXml->getStringAttribute("identifier");

        // Find plugin by identifier
        auto desc = pluginManager.getPluginByIdentifier(identifier);
        if (!desc.has_value())
        {
            DBG("PluginChain: Plugin not found: " + identifier);
            continue;
        }

        // Add plugin to chain
        int index = addPlugin(*desc);
        if (index < 0)
            continue;

        auto* node = getPlugin(index);
        if (node == nullptr)
            continue;

        // Restore bypass state
        node->setBypassed(pluginXml->getBoolAttribute("bypassed", false));

        // Restore plugin state
        juce::String stateBase64 = pluginXml->getStringAttribute("state");
        if (stateBase64.isNotEmpty())
        {
            juce::MemoryBlock state;
            if (state.fromBase64Encoding(stateBase64))
            {
                node->setState(state);
            }
        }
    }

    DBG("PluginChain: Loaded " + juce::String(getNumPlugins()) + " plugins from XML");
    return true;
}

juce::var PluginChain::saveToJson() const
{
    juce::Array<juce::var> pluginsArray;

    if (m_messageThreadChain != nullptr)
    {
        for (const auto& node : *m_messageThreadChain)
        {
            if (node == nullptr)
                continue;

            auto* pluginObj = new juce::DynamicObject();

            const auto& desc = node->getDescription();
            pluginObj->setProperty("name", desc.name);
            pluginObj->setProperty("identifier", desc.createIdentifierString());
            pluginObj->setProperty("manufacturer", desc.manufacturerName);
            pluginObj->setProperty("bypassed", node->isBypassed());

            // Save plugin state as base64
            auto state = node->getState();
            if (state.getSize() > 0)
            {
                pluginObj->setProperty("state", state.toBase64Encoding());
            }

            pluginsArray.add(juce::var(pluginObj));
        }
    }

    auto* root = new juce::DynamicObject();
    root->setProperty("version", 1);
    root->setProperty("plugins", pluginsArray);

    return juce::var(root);
}

bool PluginChain::loadFromJson(const juce::var& json)
{
    if (!json.isObject())
        return false;

    auto* root = json.getDynamicObject();
    if (root == nullptr)
        return false;

    auto pluginsVar = root->getProperty("plugins");
    if (!pluginsVar.isArray())
        return false;

    // Clear existing chain
    clear();

    auto& pluginManager = PluginManager::getInstance();
    auto* pluginsArray = pluginsVar.getArray();

    for (const auto& pluginVar : *pluginsArray)
    {
        auto* pluginObj = pluginVar.getDynamicObject();
        if (pluginObj == nullptr)
            continue;

        juce::String identifier = pluginObj->getProperty("identifier").toString();

        // Find plugin by identifier
        auto desc = pluginManager.getPluginByIdentifier(identifier);
        if (!desc.has_value())
        {
            DBG("PluginChain: Plugin not found: " + identifier);
            continue;
        }

        // Add plugin to chain
        int index = addPlugin(*desc);
        if (index < 0)
            continue;

        auto* node = getPlugin(index);
        if (node == nullptr)
            continue;

        // Restore bypass state
        node->setBypassed(pluginObj->getProperty("bypassed"));

        // Restore plugin state
        juce::String stateBase64 = pluginObj->getProperty("state").toString();
        if (stateBase64.isNotEmpty())
        {
            juce::MemoryBlock state;
            if (state.fromBase64Encoding(stateBase64))
            {
                node->setState(state);
            }
        }
    }

    DBG("PluginChain: Loaded " + juce::String(getNumPlugins()) + " plugins from JSON");
    return true;
}

//==============================================================================
void PluginChain::notifyChainChanged()
{
    // Notify listeners on message thread
    juce::MessageManager::callAsync([this]()
    {
        sendChangeMessage();
    });
}
