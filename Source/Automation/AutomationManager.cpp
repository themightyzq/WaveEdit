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
    {
        const juce::ScopedLock sl(m_lanesLock);
        m_lanes.push_back(std::move(lane));
    }
    notifyListeners();
    // Safe: only the message thread reallocates m_lanes, and back()
    // is read on the message thread immediately after the push above.
    return m_lanes.back();
}

void AutomationManager::removeLane(int laneIndex)
{
    {
        const juce::ScopedLock sl(m_lanesLock);
        if (laneIndex < 0 || laneIndex >= static_cast<int>(m_lanes.size()))
            return;
        m_lanes.erase(m_lanes.begin() + laneIndex);
    }
    notifyListeners();
}

AutomationLane* AutomationManager::getLane(int index)
{
    const juce::ScopedLock sl(m_lanesLock);
    if (index >= 0 && index < static_cast<int>(m_lanes.size()))
        return &m_lanes[static_cast<size_t>(index)];
    return nullptr;
}

AutomationLane* AutomationManager::findLane(int pluginIndex, int parameterIndex)
{
    const juce::ScopedLock sl(m_lanesLock);
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
    {
        const juce::ScopedLock sl(m_lanesLock);
        m_lanes.clear();
    }
    notifyListeners();
}

void AutomationManager::removePluginLanes(int pluginIndex)
{
    {
        const juce::ScopedLock sl(m_lanesLock);
        m_lanes.erase(
            std::remove_if(m_lanes.begin(), m_lanes.end(),
                [pluginIndex](const AutomationLane& lane)
                { return lane.pluginIndex == pluginIndex; }),
            m_lanes.end());
    }
    notifyListeners();
}

void AutomationManager::shiftPluginIndices(int fromIndex, int delta)
{
    {
        const juce::ScopedLock sl(m_lanesLock);
        for (auto& lane : m_lanes)
        {
            if (lane.pluginIndex >= fromIndex)
                lane.pluginIndex += delta;
        }
    }
    notifyListeners();
}

//==============================================================================
// Playback (audio thread)
//==============================================================================

void AutomationManager::applyAutomation(PluginChain& chain, double timeInSeconds)
{
    // C7: audio thread. Try-lock only — never block the audio thread.
    // If the message thread is mid-mutation of m_lanes, skip automation
    // for this one buffer; it resumes on the next callback. The lock
    // protects against iterator invalidation / UAF on add/remove/clear.
    if (! m_lanesLock.tryEnter())
        return;

    // RAII exit so every return path inside the loop releases the lock.
    struct ScopedExit
    {
        juce::CriticalSection& lock;
        ~ScopedExit() { lock.exit(); }
    } scopedExit { m_lanesLock };

    // H23: suppress recording of the parameterValueChanged echoes that
    // setValue() fires synchronously on this (audio) thread. The guard
    // is lock-free (atomic flag); the dispatcher checks it and bails.
    // Stack-only, no allocation: pass the (possibly null) recorder and
    // the guard no-ops when there is none.
    AutomationRecorder::ScopedApplyGuard applyGuard(m_recorder.get());

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
    const juce::ScopedLock sl(m_lanesLock);
    for (const auto& lane : m_lanes)
    {
        if (lane.isRecording)
            return true;
    }
    return false;
}

void AutomationManager::stopAllRecording()
{
    {
        const juce::ScopedLock sl(m_lanesLock);
        for (auto& lane : m_lanes)
            lane.isRecording = false;
    }
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
    {
        const juce::ScopedLock sl(m_lanesLock);
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
    }

    notifyListeners();
}

//==============================================================================
// Sidecar persistence
//==============================================================================

juce::File AutomationManager::getAutomationFilePath(const juce::File& audioFile)
{
    // Mirrors RegionManager::getRegionFilePath — append ".automation.json"
    // to the audio file's full extension so example.wav becomes
    // example.wav.automation.json.
    return audioFile.withFileExtension(audioFile.getFileExtension() + ".automation.json");
}

bool AutomationManager::saveToFile(const juce::File& audioFile) const
{
    auto sidecar = getAutomationFilePath(audioFile);

    // No lanes? Don't litter the disk. If a stale sidecar exists
    // from a previous save, leave it alone — clearing automation
    // is an explicit user action, not a side effect of "save".
    if (m_lanes.empty())
        return true;

    auto* root = new juce::DynamicObject();
    root->setProperty("version", "1.0");
    root->setProperty("audioFile", audioFile.getFileName());
    root->setProperty("lanes", toVar());

    juce::var jsonData(root);
    juce::String jsonString = juce::JSON::toString(jsonData, true);

    if (!sidecar.replaceWithText(jsonString))
    {
        juce::Logger::writeToLog("AutomationManager::saveToFile failed for "
                                  + sidecar.getFullPathName());
        return false;
    }
    return true;
}

bool AutomationManager::loadFromFile(const juce::File& audioFile)
{
    auto sidecar = getAutomationFilePath(audioFile);
    if (!sidecar.existsAsFile())
        return false;

    juce::String jsonString = sidecar.loadFileAsString();
    juce::var jsonData = juce::JSON::parse(jsonString);
    if (!jsonData.isObject())
    {
        juce::Logger::writeToLog("AutomationManager::loadFromFile: not a JSON object — "
                                  + sidecar.getFullPathName());
        return false;
    }

    auto* root = jsonData.getDynamicObject();
    if (root == nullptr)
        return false;

    fromVar(root->getProperty("lanes"));
    return true;
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
