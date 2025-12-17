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
}

PluginChain::~PluginChain()
{
    releaseResources();
    clear();
}

//==============================================================================
void PluginChain::prepareToPlay(double sampleRate, int blockSize)
{
    std::lock_guard<std::mutex> lock(m_chainMutex);

    m_sampleRate = sampleRate;
    m_blockSize = blockSize;

    // SpinLock ensures audio thread is not processing while we prepare
    const juce::SpinLock::ScopedLockType sl(m_processLock);

    // Prepare all plugins
    for (auto& node : m_nodes)
    {
        if (node != nullptr)
        {
            node->prepareToPlay(sampleRate, blockSize);
        }
    }

    m_prepared = true;

    DBG("PluginChain: Prepared " + juce::String(m_nodes.size()) + " plugins @ " +
        juce::String(sampleRate) + "Hz");
}

void PluginChain::releaseResources()
{
    std::lock_guard<std::mutex> lock(m_chainMutex);

    // SpinLock ensures audio thread is not processing while we release
    const juce::SpinLock::ScopedLockType sl(m_processLock);

    for (auto& node : m_nodes)
    {
        if (node != nullptr)
        {
            node->releaseResources();
        }
    }

    m_prepared = false;
}

//==============================================================================
void PluginChain::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // SpinLock provides audio-safe synchronization with chain modifications
    const juce::SpinLock::ScopedLockType sl(m_processLock);

    for (auto& node : m_nodes)
    {
        if (node != nullptr)
        {
            node->processBlock(buffer, midi);
        }
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
        if (m_prepared)
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
    return insertPlugin(description, static_cast<int>(m_nodes.size()));
}

int PluginChain::insertPlugin(const juce::PluginDescription& description, int index)
{
    auto node = createNode(description);
    if (node == nullptr)
        return -1;

    std::lock_guard<std::mutex> lock(m_chainMutex);

    // SpinLock ensures audio thread is not iterating while we modify
    const juce::SpinLock::ScopedLockType sl(m_processLock);

    // Clamp index to valid range
    index = juce::jlimit(0, static_cast<int>(m_nodes.size()), index);

    m_nodes.insert(m_nodes.begin() + index, std::move(node));

    DBG("PluginChain: Added " + description.name + " at index " + juce::String(index));

    notifyChainChanged();
    return index;
}

bool PluginChain::removePlugin(int index)
{
    std::lock_guard<std::mutex> lock(m_chainMutex);

    if (index < 0 || index >= static_cast<int>(m_nodes.size()))
        return false;

    auto& node = m_nodes[static_cast<size_t>(index)];
    juce::String name = node ? node->getName() : "Unknown";

    // Release resources before removing
    if (node != nullptr)
    {
        node->releaseResources();
    }

    // SpinLock ensures audio thread is not iterating while we erase
    {
        const juce::SpinLock::ScopedLockType sl(m_processLock);
        m_nodes.erase(m_nodes.begin() + index);
    }

    DBG("PluginChain: Removed " + name + " from index " + juce::String(index));

    notifyChainChanged();
    return true;
}

bool PluginChain::movePlugin(int fromIndex, int toIndex)
{
    std::lock_guard<std::mutex> lock(m_chainMutex);

    const int size = static_cast<int>(m_nodes.size());
    if (fromIndex < 0 || fromIndex >= size || toIndex < 0 || toIndex >= size)
        return false;

    if (fromIndex == toIndex)
        return true;

    // SpinLock ensures audio thread is not iterating while we modify
    const juce::SpinLock::ScopedLockType sl(m_processLock);

    // Move the node
    auto node = std::move(m_nodes[static_cast<size_t>(fromIndex)]);
    m_nodes.erase(m_nodes.begin() + fromIndex);

    // Adjust target index if needed
    if (toIndex > fromIndex)
        --toIndex;

    m_nodes.insert(m_nodes.begin() + toIndex, std::move(node));

    DBG("PluginChain: Moved plugin from " + juce::String(fromIndex) + " to " + juce::String(toIndex));

    notifyChainChanged();
    return true;
}

void PluginChain::clear()
{
    std::lock_guard<std::mutex> lock(m_chainMutex);

    // Release all resources first
    for (auto& node : m_nodes)
    {
        if (node != nullptr)
        {
            node->releaseResources();
        }
    }

    // SpinLock ensures audio thread is not iterating while we clear
    {
        const juce::SpinLock::ScopedLockType sl(m_processLock);
        m_nodes.clear();
    }

    DBG("PluginChain: Cleared all plugins");

    notifyChainChanged();
}

//==============================================================================
int PluginChain::getNumPlugins() const
{
    std::lock_guard<std::mutex> lock(m_chainMutex);
    return static_cast<int>(m_nodes.size());
}

PluginChainNode* PluginChain::getPlugin(int index)
{
    std::lock_guard<std::mutex> lock(m_chainMutex);

    if (index < 0 || index >= static_cast<int>(m_nodes.size()))
        return nullptr;

    return m_nodes[static_cast<size_t>(index)].get();
}

const PluginChainNode* PluginChain::getPlugin(int index) const
{
    std::lock_guard<std::mutex> lock(m_chainMutex);

    if (index < 0 || index >= static_cast<int>(m_nodes.size()))
        return nullptr;

    return m_nodes[static_cast<size_t>(index)].get();
}

//==============================================================================
void PluginChain::setAllBypassed(bool bypassed)
{
    std::lock_guard<std::mutex> lock(m_chainMutex);

    for (auto& node : m_nodes)
    {
        if (node != nullptr)
        {
            node->setBypassed(bypassed);
        }
    }
}

bool PluginChain::areAllBypassed() const
{
    std::lock_guard<std::mutex> lock(m_chainMutex);

    if (m_nodes.empty())
        return false;

    for (const auto& node : m_nodes)
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
    std::lock_guard<std::mutex> lock(m_chainMutex);

    int totalLatency = 0;
    for (const auto& node : m_nodes)
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
    std::lock_guard<std::mutex> lock(m_chainMutex);

    auto xml = std::make_unique<juce::XmlElement>("PluginChain");
    xml->setAttribute("version", 1);

    for (const auto& node : m_nodes)
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
    std::lock_guard<std::mutex> lock(m_chainMutex);

    juce::Array<juce::var> pluginsArray;

    for (const auto& node : m_nodes)
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
