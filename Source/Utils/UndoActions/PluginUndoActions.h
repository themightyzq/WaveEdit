/*
  ==============================================================================

    PluginUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Plugin/EQ-domain undo actions per CLAUDE.md §8.1:
      - ApplyDynamicParametricEQAction
      - ApplyPluginChainAction

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../UndoableEdits.h"
#include "../../DSP/DynamicParametricEQ.h"

//==============================================================================
/**
 * Undoable action for applying dynamic parametric EQ (20-band) to audio selection.
 */
class ApplyDynamicParametricEQAction : public UndoableEditBase
{
public:
    ApplyDynamicParametricEQAction(AudioBufferManager& bufferManager,
                                   AudioEngine& audioEngine,
                                   WaveformDisplay& waveformDisplay,
                                   int64_t startSample,
                                   int64_t numSamples,
                                   const DynamicParametricEQ::Parameters& eqParams)
        : UndoableEditBase(bufferManager, audioEngine, waveformDisplay),
          m_startSample(startSample),
          m_numSamples(numSamples),
          m_eqParams(eqParams)
    {
        jassert(m_bufferManager.hasAudioData());
        jassert(startSample >= 0 && startSample < m_bufferManager.getNumSamples());
        jassert(numSamples > 0 && (startSample + numSamples) <= m_bufferManager.getNumSamples());

        m_originalAudio = m_bufferManager.getAudioRange(startSample, numSamples);
        m_sampleRate = m_bufferManager.getSampleRate();

        // Fixed once here, from the only buffer this action holds (the EQ'd
        // audio is computed fresh from the live buffer on each perform(),
        // never stored by this action).
        m_sizeInUnits = UndoMemory::unitsForBuffer(m_originalAudio);
    }

    bool perform() override
    {
        auto audioToProcess = m_bufferManager.getAudioRange(m_startSample, m_numSamples);

        DynamicParametricEQ eq;
        eq.prepare(m_sampleRate, static_cast<int>(audioToProcess.getNumSamples()));
        eq.setParameters(m_eqParams);
        eq.applyEQ(audioToProcess);

        const bool success = m_bufferManager.replaceRange(m_startSample, m_numSamples, audioToProcess);
        if (success)
            // EQ processes the range in place -- length is always m_numSamples
            // in and out, so preserving playback (§6.5) is safe here, unlike
            // the length-changing actions (Delete/Insert/Replace).
            updatePlaybackAndDisplayPreservingPlayback();
        return success;
    }

    bool undo() override
    {
        const bool success = m_bufferManager.replaceRange(m_startSample, m_numSamples, m_originalAudio);
        if (success)
            updatePlaybackAndDisplayPreservingPlayback();
        return success;
    }

    int getSizeInUnits() override { return m_sizeInUnits; }

private:
    int64_t m_startSample;
    int64_t m_numSamples;
    DynamicParametricEQ::Parameters m_eqParams;
    juce::AudioBuffer<float> m_originalAudio;
    double m_sampleRate;
    int m_sizeInUnits = 0;  // Fixed at construction; see ctor.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ApplyDynamicParametricEQAction)
};

//==============================================================================
/**
 * Undoable action for applying plugin chain effects to a selection.
 *
 * Receives the already-processed audio buffer and handles undo/redo by
 * storing original/processed audio and swapping them. The plugin chain
 * rendering is done asynchronously by PluginChainRenderer before the
 * action is registered with the UndoManager.
 *
 * Use markAsAlreadyPerformed() when the buffer was modified by the
 * background renderer before the action is registered, so the first
 * UndoManager::perform() does not re-apply.
 *
 * Thread safety: must be created and performed on the message thread.
 */
class ApplyPluginChainAction : public UndoableEditBase
{
public:
    ApplyPluginChainAction(AudioBufferManager& bufferManager,
                          AudioEngine& audioEngine,
                          WaveformDisplay& waveformDisplay,
                          int64_t startSample,
                          int64_t numSamples,
                          const juce::AudioBuffer<float>& processedAudio,
                          const juce::String& chainDescription,
                          RegionManager* regionManager = nullptr,
                          RegionDisplay* regionDisplay = nullptr,
                          MarkerManager* markerManager = nullptr,
                          MarkerDisplay* markerDisplay = nullptr)
        : UndoableEditBase(bufferManager, audioEngine, waveformDisplay, regionManager, regionDisplay),
          m_startSample(startSample),
          m_numSamples(numSamples),
          m_processedAudio(processedAudio.getNumChannels(), processedAudio.getNumSamples()),
          m_chainDescription(chainDescription),
          m_markerManager(markerManager),
          m_markerDisplay(markerDisplay)
    {
        jassert(m_bufferManager.hasAudioData());
        jassert(startSample >= 0 && startSample < m_bufferManager.getNumSamples());
        jassert(numSamples > 0 && (startSample + numSamples) <= m_bufferManager.getNumSamples());
        // processedAudio may exceed numSamples when an effect tail is included.
        jassert(processedAudio.getNumSamples() >= numSamples);

        m_originalAudio = m_bufferManager.getAudioRange(startSample, numSamples);
        for (int ch = 0; ch < processedAudio.getNumChannels(); ++ch)
            m_processedAudio.copyFrom(ch, 0, processedAudio, ch, 0, processedAudio.getNumSamples());

        m_sampleRate = m_bufferManager.getSampleRate();

        // Fixed once here: this action holds BOTH buffers for its whole
        // lifetime, so the size is their combined bytes, converted once
        // (avoids the previous overflow-prone static_cast<int> of raw bytes).
        const size_t originalBytes = static_cast<size_t>(m_originalAudio.getNumChannels()) *
                                     static_cast<size_t>(m_originalAudio.getNumSamples()) * sizeof(float);
        const size_t processedBytes = static_cast<size_t>(m_processedAudio.getNumChannels()) *
                                      static_cast<size_t>(m_processedAudio.getNumSamples()) * sizeof(float);
        m_sizeInUnits = UndoMemory::unitsForBytes(originalBytes + processedBytes);

        // Region/marker positions BEFORE the edit, so undo can put back what
        // the tail shift below moves.
        m_savedTimeline = TimelineShift::capture(m_regionManager, m_markerManager);
    }

    void markAsAlreadyPerformed() { m_alreadyPerformed = true; }

    /**
     * Post-apply refresh for the production call sites that replace the buffer
     * THEMSELVES before registering the action (they call markAsAlreadyPerformed()
     * so perform() is a no-op). Those sites must still update the engine + display,
     * and -- critically -- must honour the SAME length classification perform()
     * uses: a tail-extending render (reverb/delay with include-tail) grows the
     * range and shifts everything after it, so playback must STOP, not resume on
     * shifted content. Delegates to the shared updatePlaybackForLengthChange() so
     * the rule lives in exactly one place (H1). Must run AFTER the external
     * replaceRange() so m_bufferManager holds the processed buffer.
     */
    void refreshAfterExternalReplace()
    {
        updatePlaybackForLengthChange(m_numSamples, m_processedAudio.getNumSamples());
        refreshMarkerDisplay(m_markerDisplay);
    }

    bool perform() override
    {
        if (m_alreadyPerformed)
        {
            // The caller already replaced the buffer; the timeline shift is
            // still ours to do, in the same undo step.
            m_alreadyPerformed = false;  // reset so redo re-applies
            shiftTimelineForTail();
            return true;
        }

        const bool success = m_bufferManager.replaceRange(m_startSample, m_numSamples, m_processedAudio);
        if (success)
        {
            shiftTimelineForTail();
            updatePlaybackForLengthChange(m_numSamples, m_processedAudio.getNumSamples());
            refreshMarkerDisplay(m_markerDisplay);
        }
        return success;
    }

    bool undo() override
    {
        // When effect tail was included, m_processedAudio is larger than m_originalAudio.
        // Replace the extended range with the original range.
        const int64_t samplesToReplace = m_processedAudio.getNumSamples();
        const bool success = m_bufferManager.replaceRange(m_startSample, samplesToReplace, m_originalAudio);
        if (success)
        {
            if (samplesToReplace != m_numSamples)
                TimelineShift::restore(m_savedTimeline, m_regionManager, m_markerManager);
            updatePlaybackForLengthChange(samplesToReplace, m_originalAudio.getNumSamples());
            refreshMarkerDisplay(m_markerDisplay);
        }
        return success;
    }

    int getSizeInUnits() override { return m_sizeInUnits; }

    // NOTE: a getName() used to live here, but juce::UndoableAction has no
    // virtual getName() to override (only perform()/undo()/getSizeInUnits()/
    // createCoalescedAction() -- see juce_UndoableAction.h) and it had zero
    // callers (confirmed by grep): the UndoManager transaction name is set
    // separately via beginNewTransaction() at the call site in
    // DSPController_Advanced.cpp. It was dead, misleading code (a future
    // reader could believe renaming this action renames the transaction,
    // which it never did) -- removed rather than left as a maintenance trap.
    // m_chainDescription is kept (still passed by the existing call site)
    // for a future real use, e.g. surfacing the chain description in the UI.

private:
    /**
     * A plugin chain render is only length-preserving when no effect tail
     * was included -- otherwise it grows the affected range (reverb/delay
     * tail), which shifts everything after it, the same content-position
     * hazard Delete/Insert/Replace have. Preserve playback (§6.5) only in
     * the genuinely safe case; otherwise fall back to the deterministic
     * stop-on-length-change behavior those actions use.
     */
    /**
     * An effect tail is audio inserted at the end of the processed range:
     * everything after the selection moves later by the tail length, so
     * regions and markers there must move with it (same rules as
     * InsertAction). No-op without a tail.
     */
    void shiftTimelineForTail()
    {
        const int64_t tailSamples = m_processedAudio.getNumSamples() - m_numSamples;
        if (tailSamples > 0)
            TimelineShift::shiftForInsert(m_regionManager, m_markerManager,
                                          m_startSample + m_numSamples, tailSamples);
    }

    void updatePlaybackForLengthChange(int64_t oldLength, int64_t newLength)
    {
        if (oldLength == newLength)
            updatePlaybackAndDisplayPreservingPlayback();
        else
            updatePlaybackAndDisplay();
    }

    int64_t m_startSample;
    int64_t m_numSamples;
    juce::AudioBuffer<float> m_originalAudio;
    juce::AudioBuffer<float> m_processedAudio;
    juce::String m_chainDescription;
    MarkerManager* m_markerManager;      // Optional - may be nullptr
    MarkerDisplay* m_markerDisplay;      // Optional - may be nullptr
    TimelineShift::Snapshot m_savedTimeline;  // Region/marker positions for undo
    double m_sampleRate;
    bool m_alreadyPerformed = false;
    int m_sizeInUnits = 0;  // Fixed at construction; see ctor.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ApplyPluginChainAction)
};
