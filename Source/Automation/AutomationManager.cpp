/*
  ==============================================================================

    AutomationManager.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "AutomationManager.h"
#include "AutomationRecorder.h"
#include "../Plugins/PluginChain.h"
#include "../Plugins/PluginChainNode.h"

//==============================================================================
AutomationManager::AutomationManager()
{
}

AutomationManager::~AutomationManager()
{
    // Destroy recorder before lane vector so its dispatchers detach
    // their plugin listeners while the lanes still exist.
    m_recorder.reset();
}

void AutomationManager::setAudioEngine(AudioEngine* engine)
{
    // Construct the recorder once. Replacing it later would race
    // against in-flight ParameterDispatcher AsyncUpdater callbacks
    // (which capture a reference to the old recorder). Assert the
    // single-call contract; in practice Document calls this exactly
    // once during construction.
    jassert(m_recorder == nullptr && "setAudioEngine should be called at most once per AutomationManager");
    if (m_recorder == nullptr)
        m_recorder = std::make_unique<AutomationRecorder>(*this, engine);
}

//==============================================================================
// Lane management (message thread)
//==============================================================================

AutomationLane& AutomationManager::addLane(int pluginIndex, int parameterIndex,
                                             const juce::String& pluginName,
                                             const juce::String& parameterName,
                                             const juce::String& parameterID)
{
    AutomationLane lane;
    lane.pluginIndex = pluginIndex;
    lane.parameterIndex = parameterIndex;
    lane.pluginName = pluginName;
    lane.parameterName = parameterName;
    lane.parameterID = parameterID;
    m_lanes.push_back(std::move(lane));
    notifyListeners();
    return m_lanes.back();
}

void AutomationManager::removeLane(int laneIndex)
{
    if (laneIndex >= 0 && laneIndex < static_cast<int>(m_lanes.size()))
    {
        m_lanes.erase(m_lanes.begin() + laneIndex);
        notifyListeners();
    }
}

AutomationLane* AutomationManager::getLane(int index)
{
    if (index >= 0 && index < static_cast<int>(m_lanes.size()))
        return &m_lanes[static_cast<size_t>(index)];
    return nullptr;
}

AutomationLane* AutomationManager::findLane(int pluginIndex, int parameterIndex)
{
    for (auto& lane : m_lanes)
    {
        if (lane.matches(pluginIndex, parameterIndex))
            return &lane;
    }
    return nullptr;
}

const std::vector<AutomationLane>& AutomationManager::getLanes() const
{
    return m_lanes;
}

int AutomationManager::getNumLanes() const
{
    return static_cast<int>(m_lanes.size());
}

void AutomationManager::clearAll()
{
    m_lanes.clear();
    notifyListeners();
}

void AutomationManager::removePluginLanes(int pluginIndex)
{
    m_lanes.erase(
        std::remove_if(m_lanes.begin(), m_lanes.end(),
            [pluginIndex](const AutomationLane& lane)
            { return lane.pluginIndex == pluginIndex; }),
        m_lanes.end());
    notifyListeners();
}

void AutomationManager::shiftPluginIndices(int fromIndex, int delta)
{
    for (auto& lane : m_lanes)
    {
        if (lane.pluginIndex >= fromIndex)
            lane.pluginIndex += delta;
    }
    notifyListeners();
}

//==============================================================================
// Playback (audio thread)
//==============================================================================

void AutomationManager::applyAutomation(PluginChain& chain, double timeInSeconds)
{
    for (auto& lane : m_lanes)
    {
        if (!lane.enabled || !lane.curve.hasPoints())
            continue;

        auto* node = chain.getPlugin(lane.pluginIndex);
        if (node == nullptr || node->isBypassed())
            continue;

        auto* plugin = node->getPlugin();
        if (plugin == nullptr)
            continue;

        auto& params = plugin->getParameters();
        if (lane.parameterIndex >= params.size())
            continue;

        float value = lane.curve.getValueAt(timeInSeconds);
        params[lane.parameterIndex]->setValue(value);
    }
}

//==============================================================================
// Global state
//==============================================================================

bool AutomationManager::isAnyLaneRecording() const
{
    for (const auto& lane : m_lanes)
    {
        if (lane.isRecording)
            return true;
    }
    return false;
}

void AutomationManager::stopAllRecording()
{
    for (auto& lane : m_lanes)
        lane.isRecording = false;
    notifyListeners();
}

//==============================================================================
// Serialization
//==============================================================================

juce::var AutomationManager::toVar() const
{
    auto arr = juce::Array<juce::var>();

    for (const auto& lane : m_lanes)
    {
        auto obj = new juce::DynamicObject();
        obj->setProperty("pluginIndex", lane.pluginIndex);
        obj->setProperty("parameterIndex", lane.parameterIndex);
        obj->setProperty("pluginName", lane.pluginName);
        obj->setProperty("parameterName", lane.parameterName);
        obj->setProperty("parameterID", lane.parameterID);
        obj->setProperty("enabled", lane.enabled);
        obj->setProperty("points", lane.curve.toVar());
        arr.add(juce::var(obj));
    }

    return juce::var(arr);
}

void AutomationManager::fromVar(const juce::var& data)
{
    m_lanes.clear();

    if (auto* arr = data.getArray())
    {
        for (const auto& item : *arr)
        {
            AutomationLane lane;
            lane.pluginIndex = static_cast<int>(item.getProperty("pluginIndex", -1));
            lane.parameterIndex = static_cast<int>(item.getProperty("parameterIndex", -1));
            lane.pluginName = item.getProperty("pluginName", "").toString();
            lane.parameterName = item.getProperty("parameterName", "").toString();
            lane.parameterID = item.getProperty("parameterID", "").toString();
            lane.enabled = static_cast<bool>(item.getProperty("enabled", true));
            lane.curve.fromVar(item.getProperty("points", juce::var()));
            m_lanes.push_back(std::move(lane));
        }
    }

    notifyListeners();
}

//==============================================================================
// Listener
//==============================================================================

void AutomationManager::addListener(Listener* l)
{
    m_listeners.add(l);
}

void AutomationManager::removeListener(Listener* l)
{
    m_listeners.remove(l);
}

void AutomationManager::notifyListeners()
{
    m_listeners.call([](Listener& l) { l.automationDataChanged(); });
}
