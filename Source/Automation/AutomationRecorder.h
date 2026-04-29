/*
  ==============================================================================

    AutomationRecorder.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Captures user-driven plugin parameter changes during playback and
    appends them as AutomationPoints to the matching lane's curve.

    Phase 4 of the Plugin Parameter Automation feature: Phases 1-3
    delivered the data model + audio-thread playback; this is the
    recording side. Phases 5-6 (visual lane editor, persistence with
    plugin presets) are deferred.

    Thread model:
    - Lifetime / arm management: message thread.
    - juce::AudioProcessorParameter::Listener callbacks: arrive on
      whichever thread called setValue. We bounce off-thread callbacks
      onto the message thread via AsyncUpdater so all point appends
      happen on a known thread.
    - We never touch AutomationCurve from the audio thread directly —
      curve writes happen in the AsyncUpdater callback.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class AudioEngine;
class AutomationManager;
class PluginChain;

/**
 * AutomationRecorder
 * ------------------
 * Owned by AutomationManager (one per Document). Watches every
 * juce::AudioProcessorParameter on every plugin in the chain. When
 * (a) global record-arm is on AND (b) the matching lane has its
 * isRecording flag set AND (c) the AudioEngine is currently playing,
 * incoming parameter-change callbacks are coalesced and appended as
 * AutomationPoints on the lane's curve.
 */
class AutomationRecorder
{
public:
    AutomationRecorder(AutomationManager& manager, AudioEngine* engine);
    ~AutomationRecorder();

    AutomationRecorder(const AutomationRecorder&) = delete;
    AutomationRecorder& operator=(const AutomationRecorder&) = delete;

    //==========================================================================
    // Attachment lifecycle (message thread)

    /**
     * Walk the chain, attach a ParameterDispatcher to every plugin we
     * don't already have one for, and detach any dispatcher whose
     * plugin no longer exists. Idempotent. Call this when the
     * PluginChain changes (add / remove / reorder).
     */
    void refreshAttachments(PluginChain& chain);

    /** Detach all dispatchers. Call before destroying the chain. */
    void detachAll();

    //==========================================================================
    // Arming (message thread)

    /** Toggle the global "record-arm" flag. */
    void toggleGlobalArm();
    void setGlobalArm(bool armed);
    bool isGlobalArmed() const { return m_globalArm.load(); }

    /**
     * Convenience: ensure a lane exists for (pluginIndex,
     * parameterIndex), then set its isRecording flag. The combined
     * behaviour is "arm this parameter for recording" — global arm
     * still has to be on for capture to actually happen.
     */
    bool armParameter(int pluginIndex, int parameterIndex,
                      const juce::String& pluginName,
                      const juce::String& parameterName,
                      const juce::String& parameterID);
    bool disarmParameter(int pluginIndex, int parameterIndex);

    /**
     * For tests: synchronously deliver a parameter change as if it
     * came from a plugin. Skips the AsyncUpdater coalescing path —
     * call after putting the engine in a known playing state.
     */
    void debugInjectParameterChange(int pluginIndex, int parameterIndex, float newValue);

    /**
     * For tests: override transport queries. Production code wires
     * these to AudioEngine; tests can drive playback state and time
     * directly. Pass nullptr-equivalent (default-constructed) to
     * fall back to the engine wiring.
     */
    void setTransportProvidersForTest(std::function<bool()> isPlayingFn,
                                       std::function<double()> positionFn);

    //==========================================================================
    // Debounce tuning (message thread; mostly for tests)

    /** Minimum gap between recorded points for the same parameter. */
    void setDebounceMillis(int ms) { m_debounceMs.store(juce::jmax(0, ms)); }
    int getDebounceMillis() const { return m_debounceMs.load(); }

    //==========================================================================
    // Public nested type so tests can construct dispatchers directly
    // against a synthetic juce::AudioProcessor. Production code never
    // touches this — refreshAttachments() builds them internally.

    /**
     * One ParameterDispatcher per plugin. Listens to every parameter
     * on that plugin and bounces callbacks onto the message thread
     * via AsyncUpdater for safe curve writes.
     */
    class ParameterDispatcher : public juce::AudioProcessorParameter::Listener,
                                public juce::AsyncUpdater
    {
    public:
        // AudioProcessor base lets tests construct dispatchers with a
        // synthetic juce::AudioProcessor instead of a real
        // juce::AudioPluginInstance. Production wiring still passes
        // AudioPluginInstance* (implicit upcast).
        ParameterDispatcher(AutomationRecorder& owner, int pluginIndex,
                            juce::AudioProcessor* plugin);
        ~ParameterDispatcher() override;

        ParameterDispatcher(const ParameterDispatcher&) = delete;
        ParameterDispatcher& operator=(const ParameterDispatcher&) = delete;

        int getPluginIndex() const noexcept { return m_pluginIndex; }
        juce::AudioProcessor* getPlugin() const noexcept { return m_plugin; }
        void setPluginIndex(int newIndex) noexcept { m_pluginIndex = newIndex; }

        // juce::AudioProcessorParameter::Listener
        void parameterValueChanged(int parameterIndex, float newValue) override;
        void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;

        // juce::AsyncUpdater
        void handleAsyncUpdate() override;

        /** Synchronously drain the pending-value queue. Production
            code reaches this via handleAsyncUpdate; tests can call it
            directly to bypass the message-loop bounce. */
        void flushPending();

    private:
        struct PendingValue
        {
            int paramIndex = -1;
            float latestValue = 0.0f;
            bool gestureEnd = false;
        };

        AutomationRecorder& m_owner;
        int m_pluginIndex;
        juce::AudioProcessor* m_plugin;

        // Locked while editing the pending vector. Held only briefly.
        juce::CriticalSection m_pendingLock;
        std::vector<PendingValue> m_pending;
    };

private:
    AutomationManager& m_manager;
    std::function<bool()>   m_isPlayingFn;   // returns transport state
    std::function<double()> m_positionFn;    // returns current position (sec)

    std::atomic<bool> m_globalArm { false };
    std::atomic<int>  m_debounceMs { 20 };

    std::vector<std::unique_ptr<ParameterDispatcher>> m_dispatchers;

    /**
     * Apply a single recorded value: gate by global arm + lane
     * isRecording + transport state, append a point. Called on the
     * message thread from the dispatcher's AsyncUpdater handler (or
     * directly from debugInjectParameterChange).
     */
    void applyRecordedValue(int pluginIndex, int parameterIndex,
                            float newValue, bool isGestureEnd);

    // Per-lane debounce bookkeeping (last point time, last value).
    struct LaneDebounceState
    {
        int pluginIndex = -1;
        int parameterIndex = -1;
        double lastPointTimeSec = -1.0;
        float lastValue = -1.0f;
    };
    juce::CriticalSection m_debounceLock;
    std::vector<LaneDebounceState> m_debounce;

    /** Caller must hold m_debounceLock. Returns a stable index; safe
        across push_back-induced reallocations. */
    int findOrCreateDebounceIndex(int pluginIndex, int parameterIndex);
};
