/*
  ==============================================================================

    AutomationRecorder.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

  ==============================================================================
*/

#include "AutomationRecorder.h"
#include "AutomationManager.h"
#include "../Audio/AudioEngine.h"
#include "../Plugins/PluginChain.h"
#include "../Plugins/PluginChainNode.h"

//==============================================================================
// AutomationRecorder
//==============================================================================

AutomationRecorder::AutomationRecorder(AutomationManager& manager, AudioEngine* engine)
    : m_manager(manager)
{
    if (engine != nullptr)
    {
        // Lambdas capture the raw engine pointer. Lifetime is
        // guaranteed by Document's member declaration order
        // (m_audioEngine declared before m_automationManager → audio
        // engine outlives the recorder owned via the manager). Don't
        // change Document's member order without updating this.
        m_isPlayingFn = [engine] { return engine->isPlaying(); };
        m_positionFn  = [engine] { return engine->getCurrentPosition(); };
    }
    else
    {
        // Tests that don't pass an engine: default to "always playing
        // at time 0" so applyRecordedValue gating doesn't block. Tests
        // can override via setTransportProvidersForTest.
        m_isPlayingFn = [] { return true; };
        m_positionFn  = [] { return 0.0; };
    }
}

void AutomationRecorder::setTransportProvidersForTest(
    std::function<bool()> isPlayingFn, std::function<double()> positionFn)
{
    if (isPlayingFn) m_isPlayingFn = std::move(isPlayingFn);
    if (positionFn)  m_positionFn  = std::move(positionFn);
}

AutomationRecorder::~AutomationRecorder()
{
    detachAll();
}

void AutomationRecorder::refreshAttachments(PluginChain& chain)
{
    const int numPlugins = chain.getNumPlugins();

    // Detach dispatchers whose plugin pointer is no longer in the chain
    // (or whose index would now be out of range).
    m_dispatchers.erase(
        std::remove_if(m_dispatchers.begin(), m_dispatchers.end(),
            [&](const std::unique_ptr<ParameterDispatcher>& d)
            {
                if (d == nullptr) return true;
                if (d->getPluginIndex() >= numPlugins) return true;
                auto* node = chain.getPlugin(d->getPluginIndex());
                if (node == nullptr) return true;
                if (node->getPlugin() != d->getPlugin()) return true;
                return false;
            }),
        m_dispatchers.end());

    // Attach a dispatcher for any plugin we don't yet cover.
    for (int i = 0; i < numPlugins; ++i)
    {
        auto* node = chain.getPlugin(i);
        if (node == nullptr) continue;
        auto* plug = node->getPlugin();
        if (plug == nullptr) continue;

        bool alreadyAttached = false;
        for (const auto& d : m_dispatchers)
        {
            if (d != nullptr && d->getPlugin() == plug)
            {
                d->setPluginIndex(i);  // index may have shifted on reorder
                alreadyAttached = true;
                break;
            }
        }
        if (!alreadyAttached)
        {
            m_dispatchers.emplace_back(
                std::make_unique<ParameterDispatcher>(*this, i, plug));
        }
    }
}

void AutomationRecorder::detachAll()
{
    m_dispatchers.clear();
}

void AutomationRecorder::toggleGlobalArm()
{
    setGlobalArm(!m_globalArm.load());
}

void AutomationRecorder::setGlobalArm(bool armed)
{
    m_globalArm.store(armed);
    if (!armed)
        m_manager.stopAllRecording();
}

bool AutomationRecorder::armParameter(int pluginIndex, int parameterIndex,
                                       const juce::String& pluginName,
                                       const juce::String& parameterName,
                                       const juce::String& parameterID)
{
    auto* lane = m_manager.findLane(pluginIndex, parameterIndex);
    if (lane == nullptr)
        lane = &m_manager.addLane(pluginIndex, parameterIndex,
                                   pluginName, parameterName, parameterID);
    lane->isRecording = true;
    return true;
}

bool AutomationRecorder::disarmParameter(int pluginIndex, int parameterIndex)
{
    auto* lane = m_manager.findLane(pluginIndex, parameterIndex);
    if (lane == nullptr) return false;
    lane->isRecording = false;
    return true;
}

void AutomationRecorder::debugInjectParameterChange(int pluginIndex, int parameterIndex,
                                                     float newValue)
{
    applyRecordedValue(pluginIndex, parameterIndex, newValue, /*isGestureEnd=*/false);
}

void AutomationRecorder::applyRecordedValue(int pluginIndex, int parameterIndex,
                                             float newValue, bool isGestureEnd)
{
    if (!m_globalArm.load())
        return;

    auto* lane = m_manager.findLane(pluginIndex, parameterIndex);
    if (lane == nullptr || !lane->isRecording)
        return;

    if (!m_isPlayingFn())
        return;

    const double position = m_positionFn();
    const int    debounceMs  = m_debounceMs.load();
    const double debounceSec = static_cast<double>(debounceMs) / 1000.0;

    // Read-modify-write the debounce state under a single lock. Holding
    // m_debounceLock across the curve write is safe (AutomationCurve
    // takes its own internal lock and never reaches back into the
    // recorder).
    bool emit = false;
    AutomationPoint pt;
    pt.timeInSeconds = position;
    pt.value = juce::jlimit(0.0f, 1.0f, newValue);
    pt.curve = AutomationPoint::CurveType::Linear;

    {
        const juce::ScopedLock sl(m_debounceLock);
        const int idx = findOrCreateDebounceIndex(pluginIndex, parameterIndex);
        auto& state = m_debounce[static_cast<size_t>(idx)];

        const bool firstPoint   = (state.lastPointTimeSec < 0.0);
        const bool elapsed      = (position - state.lastPointTimeSec) >= debounceSec;
        const bool valueChanged = std::abs(newValue - state.lastValue) > 0.0001f;

        emit = isGestureEnd || firstPoint || (elapsed && valueChanged);
        if (emit)
        {
            state.lastPointTimeSec = position;
            state.lastValue = pt.value;
        }
    }

    if (emit)
        lane->curve.addPoint(pt);
}

int AutomationRecorder::findOrCreateDebounceIndex(int pluginIndex, int parameterIndex)
{
    // Caller must hold m_debounceLock. Returns a stable index — using
    // an index instead of an iterator/reference avoids invalidation
    // when push_back triggers a reallocation.
    for (size_t i = 0; i < m_debounce.size(); ++i)
    {
        if (m_debounce[i].pluginIndex == pluginIndex
            && m_debounce[i].parameterIndex == parameterIndex)
            return static_cast<int>(i);
    }
    LaneDebounceState fresh;
    fresh.pluginIndex = pluginIndex;
    fresh.parameterIndex = parameterIndex;
    m_debounce.push_back(fresh);
    return static_cast<int>(m_debounce.size() - 1);
}

//==============================================================================
// AutomationRecorder::ParameterDispatcher
//==============================================================================

AutomationRecorder::ParameterDispatcher::ParameterDispatcher(
    AutomationRecorder& owner, int pluginIndex, juce::AudioProcessor* plugin)
    : m_owner(owner), m_pluginIndex(pluginIndex), m_plugin(plugin)
{
    if (plugin == nullptr)
        return;

    for (auto* p : plugin->getParameters())
        if (p != nullptr)
            p->addListener(this);
}

AutomationRecorder::ParameterDispatcher::~ParameterDispatcher()
{
    cancelPendingUpdate();

    if (m_plugin == nullptr)
        return;

    for (auto* p : m_plugin->getParameters())
        if (p != nullptr)
            p->removeListener(this);
}

void AutomationRecorder::ParameterDispatcher::parameterValueChanged(
    int parameterIndex, float newValue)
{
    {
        const juce::ScopedLock sl(m_pendingLock);
        for (auto& pv : m_pending)
        {
            if (pv.paramIndex == parameterIndex)
            {
                pv.latestValue = newValue;
                return;
            }
        }
        PendingValue pv;
        pv.paramIndex = parameterIndex;
        pv.latestValue = newValue;
        pv.gestureEnd = false;
        m_pending.push_back(pv);
    }
    triggerAsyncUpdate();
}

void AutomationRecorder::ParameterDispatcher::parameterGestureChanged(
    int parameterIndex, bool gestureIsStarting)
{
    if (gestureIsStarting)
        return;  // we only care about gesture-end

    {
        const juce::ScopedLock sl(m_pendingLock);
        bool found = false;
        for (auto& pv : m_pending)
        {
            if (pv.paramIndex == parameterIndex)
            {
                pv.gestureEnd = true;
                found = true;
                break;
            }
        }
        if (!found)
        {
            PendingValue pv;
            pv.paramIndex = parameterIndex;
            pv.gestureEnd = true;
            // latestValue left at default; we'll skip the append if no
            // recent value was queued for this parameter.
            pv.latestValue = std::numeric_limits<float>::quiet_NaN();
            m_pending.push_back(pv);
        }
    }
    triggerAsyncUpdate();
}

void AutomationRecorder::ParameterDispatcher::handleAsyncUpdate()
{
    flushPending();
}

void AutomationRecorder::ParameterDispatcher::flushPending()
{
    std::vector<PendingValue> snapshot;
    {
        const juce::ScopedLock sl(m_pendingLock);
        snapshot.swap(m_pending);
    }

    for (const auto& pv : snapshot)
    {
        if (std::isnan(pv.latestValue))
            continue;  // gesture-end with no preceding value change
        m_owner.applyRecordedValue(m_pluginIndex, pv.paramIndex,
                                    pv.latestValue, pv.gestureEnd);
    }
}
