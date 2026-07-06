/*
  ==============================================================================

    UndoableEdits.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Audio/AudioBufferManager.h"
#include "../Audio/AudioEngine.h"
#include "../UI/WaveformDisplay.h"
#include "../UI/RegionDisplay.h"
#include "../UI/MarkerDisplay.h"
#include "RegionManager.h"
#include "Region.h"
#include "MarkerManager.h"
#include "Marker.h"

// UndoableEditBase + the generic Delete/Insert/Replace primitives live here.
// Domain-specific actions live alongside their domain in Source/Utils/UndoActions/.
// AudioUndoActions.h and RegionUndoActions.h are umbrella headers per
// CLAUDE.md §7.5; the actual classes live in domain-split files:
//   AudioUndoActions.h          (umbrella)
//     ├─ LevelUndoActions.h      (Gain, Normalize, DCOffsetRemoval)
//     ├─ RangeUndoActions.h      (Silence, Trim)
//     ├─ TransformUndoActions.h  (Reverse, Invert, Resample, HeadTail)
//     └─ ChannelUndoActions.h    (ChannelConvert, ConvertToStereo,
//                                  SilenceChannels, ReplaceChannels)
//   RegionUndoActions.h         (umbrella)
//     ├─ RegionLifecycleUndoActions.h   (Add, Delete, Rename, BatchRename,
//     │                                   ChangeRegionColor)
//     ├─ RegionEditUndoActions.h        (Resize, Nudge, Merge, MultiMerge, Split)
//     └─ RegionDerivationUndoActions.h  (StripSilence, Retrospective,
//                                         MarkersToRegions)
//   FadeUndoActions.h     (FadeIn/FadeOut)
//   MarkerUndoActions.h   (Add/Delete/Move/Rename)
//   PluginUndoActions.h   (ApplyParametricEQ, ApplyDynamicParametricEQ,
//                          ApplyPluginChain)

/**
 * Base class for undoable edit operations.
 * Provides common functionality for all edit operations that need undo/redo.
 */
class UndoableEditBase : public juce::UndoableAction
{
public:
    UndoableEditBase(AudioBufferManager& bufferManager,
                     AudioEngine& audioEngine,
                     WaveformDisplay& waveformDisplay,
                     RegionManager* regionManager = nullptr,
                     RegionDisplay* regionDisplay = nullptr)
        : m_bufferManager(bufferManager),
          m_audioEngine(audioEngine),
          m_waveformDisplay(waveformDisplay),
          m_regionManager(regionManager),
          m_regionDisplay(regionDisplay)
    {
    }

    virtual ~UndoableEditBase() = default;

protected:
    /**
     * Updates both the audio engine and waveform display after a buffer
     * modification that changes the buffer's LENGTH (Delete/Insert/Replace,
     * and ApplyPluginChainAction when an effect tail extends the buffer).
     *
     * Deliberately stops playback and resets position to 0 rather than
     * preserving it: a length-changing edit moves audio content around, so
     * naively keeping the same raw position (as
     * AudioEngine::reloadBufferPreservingPlayback() does) would silently
     * resume playback on DIFFERENT audio content at that position -- worse
     * than stopping, because the user has no indication the content
     * changed. Correctly remapping the position through the shift (the way
     * DeleteAction already remaps region start/end) is possible in
     * principle but is not implemented here; until it is, stop-on-length-
     * change is the safe, deterministic contract this codebase tests for
     * (Tests/Unit/LargeOperationUndoTests.cpp,
     * "Buffer-length-changing edits stop playback deterministically at
     * scale"). Do NOT swap this for reloadBufferPreservingPlayback() without
     * also fixing that remap -- see updatePlaybackAndDisplayPreservingPlayback()
     * below for the actions where preserving is actually safe.
     *
     * CRITICAL FIX (Phase 1.4 - Instant Waveform Redraw):
     * Uses updateAfterEdit() instead of reloadFromBuffer() for seamless editing workflow.
     * - NO screen flash (no loading state)
     * - NO view jump (preserves zoom and scroll)
     * - NO progressive redraw (synchronous update)
     * - Preserves edit cursor position
     *
     * Region Fix: Also updates RegionDisplay to synchronize region positions with waveform changes.
     */
    void updatePlaybackAndDisplay()
    {
        // CRITICAL: Stop playback first to avoid race conditions
        // Updating the buffer while the audio callback is reading it can cause glitches or crashes
        m_audioEngine.stop();

        // Update audio engine for playback
        if (!m_audioEngine.loadFromBuffer(
            m_bufferManager.getBuffer(),
            m_bufferManager.getSampleRate(),
            m_bufferManager.getNumChannels()))
        {
            DBG("ERROR: Failed to update audio engine after undo/redo");
        }

        updateWaveformAndRegionDisplay();
    }

    /**
     * Updates the audio engine and waveform display after a buffer
     * modification that does NOT change the buffer's length -- channel
     * conversion (sample count preserved, only channel layout changes) and
     * in-place DSP (EQ, plugin chain without an effect tail). These are
     * exactly the "gain-type" edits CLAUDE.md §6.5 describes: content stays
     * at the same sample position, so it is both safe and correct to
     * restore playback via AudioEngine::reloadBufferPreservingPlayback()
     * instead of unconditionally stopping (matching the ~16 sibling
     * UndoableActions deriving straight from juce::UndoableAction -- Gain,
     * Fade, Normalize, etc. -- which already do this). Callers MUST verify
     * the edit is actually length-preserving before using this method; if
     * it might not be (e.g. a plugin chain render with a reverb/delay
     * tail), fall back to updatePlaybackAndDisplay() instead -- see
     * ApplyPluginChainAction::perform()/undo().
     */
    void updatePlaybackAndDisplayPreservingPlayback()
    {
        if (!m_audioEngine.reloadBufferPreservingPlayback(
            m_bufferManager.getBuffer(),
            m_bufferManager.getSampleRate(),
            m_bufferManager.getNumChannels()))
        {
            DBG("ERROR: Failed to update audio engine after undo/redo");
        }

        updateWaveformAndRegionDisplay();
    }

    /**
     * Refresh a MarkerDisplay after a length-changing edit shifted marker
     * positions. Mirrors the RegionDisplay refresh in
     * updateWaveformAndRegionDisplay() (total duration + visible range +
     * repaint): the display reads positions from its MarkerManager at paint
     * time, so a repaint after mutating the manager is enough to redraw the
     * moved markers. No-op when no display was supplied (e.g. unit tests pass
     * a MarkerManager but no MarkerDisplay). Used by InsertAction/ReplaceAction,
     * the only actions that shift markers (H3).
     */
    void refreshMarkerDisplay(MarkerDisplay* markerDisplay)
    {
        if (markerDisplay == nullptr)
            return;

        const double newDuration =
            static_cast<double>(m_bufferManager.getNumSamples()) / m_bufferManager.getSampleRate();
        markerDisplay->setTotalDuration(newDuration);
        markerDisplay->setVisibleRange(m_waveformDisplay.getVisibleRangeStart(),
                                       m_waveformDisplay.getVisibleRangeEnd());
        markerDisplay->repaint();
    }

    AudioBufferManager& m_bufferManager;
    AudioEngine& m_audioEngine;
    WaveformDisplay& m_waveformDisplay;
    RegionManager* m_regionManager;    // Optional - may be nullptr if no regions exist
    RegionDisplay* m_regionDisplay;    // Optional - may be nullptr if no region display

private:
    /** Shared waveform/region display refresh used by both playback-update
        variants above (the only difference between them is the AudioEngine
        reload strategy). */
    void updateWaveformAndRegionDisplay()
    {
        // CRITICAL FIX: Use reloadFromBuffer() with preserve flags
        // This preserves view position and edit cursor for seamless workflow
        if (!m_waveformDisplay.reloadFromBuffer(
            m_bufferManager.getBuffer(),
            m_bufferManager.getSampleRate(),
            true,  // preserveView = true
            true)) // preserveEditCursor = true
        {
            DBG("Warning: Failed to update waveform display after undo/redo");
        }

        // REGION FIX: Update RegionDisplay to synchronize with waveform changes
        // After delete/undo, the total duration changes and regions need to be redrawn
        if (m_regionDisplay)
        {
            // Update total duration so RegionDisplay can properly position regions
            double newDuration = static_cast<double>(m_bufferManager.getNumSamples()) / m_bufferManager.getSampleRate();
            m_regionDisplay->setTotalDuration(newDuration);

            // CRITICAL FIX: Also update the visible range from WaveformDisplay
            // This ensures regions redraw correctly after undo without needing to zoom
            double visibleStart = m_waveformDisplay.getVisibleRangeStart();
            double visibleEnd = m_waveformDisplay.getVisibleRangeEnd();
            m_regionDisplay->setVisibleRange(visibleStart, visibleEnd);

            // Force repaint to show updated region positions
            m_regionDisplay->repaint();
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndoableEditBase)
};

//==============================================================================
/**
 * Undoable delete operation.
 * Stores the deleted audio data and can restore it on undo.
 */
class DeleteAction : public UndoableEditBase
{
public:
    DeleteAction(AudioBufferManager& bufferManager,
                 AudioEngine& audioEngine,
                 WaveformDisplay& waveformDisplay,
                 int64_t startSample,
                 int64_t numSamples,
                 RegionManager* regionManager = nullptr,
                 RegionDisplay* regionDisplay = nullptr)
        : UndoableEditBase(bufferManager, audioEngine, waveformDisplay, regionManager, regionDisplay),
          m_startSample(startSample),
          m_numSamples(numSamples)
    {
        // Validate buffer state
        jassert(m_bufferManager.hasAudioData());
        jassert(startSample >= 0 && startSample < m_bufferManager.getNumSamples());
        jassert(numSamples > 0 && (startSample + numSamples) <= m_bufferManager.getNumSamples());

        // Store the audio data that will be deleted
        m_deletedAudio = m_bufferManager.getAudioRange(startSample, numSamples);
        m_sampleRate = m_bufferManager.getSampleRate();

        // Save all region positions BEFORE the delete (for undo)
        if (m_regionManager)
        {
            for (int i = 0; i < m_regionManager->getNumRegions(); ++i)
            {
                if (const auto* region = m_regionManager->getRegion(i))
                {
                    m_savedRegions.add(*region);
                }
            }
        }
    }

    bool perform() override
    {
        // Perform the delete operation on the buffer
        bool success = m_bufferManager.deleteRange(m_startSample, m_numSamples);

        if (success)
        {
            // INTELLIGENT REGION MANAGEMENT:
            // When deleting audio, we need to:
            // 1. Delete regions completely within the deleted range
            // 2. Shift regions after the deletion by the deleted amount
            // This makes regions "follow" the waveform intelligently
            if (m_regionManager && m_regionManager->getNumRegions() > 0)
            {
                int64_t deleteEnd = m_startSample + m_numSamples;

                // Iterate backwards to safely remove regions during iteration
                for (int i = m_regionManager->getNumRegions() - 1; i >= 0; --i)
                {
                    Region* region = m_regionManager->getRegion(i);
                    if (!region) continue;

                    int64_t regionStart = region->getStartSample();
                    int64_t regionEnd = region->getEndSample();

                    // Case 1: Region is completely before deletion - no change needed
                    if (regionEnd <= m_startSample)
                    {
                        // Keep as-is
                        continue;
                    }
                    // Case 2: Region is completely within deletion - remove it
                    else if (regionStart >= m_startSample && regionEnd <= deleteEnd)
                    {
                        DBG(juce::String::formatted(
                            "Region '%s' deleted (was within deleted range)",
                            region->getName().toRawUTF8()));
                        m_regionManager->removeRegion(i);
                    }
                    // Case 3: Region starts within deletion but ends after - partial overlap (remove for simplicity)
                    else if (regionStart >= m_startSample && regionStart < deleteEnd && regionEnd > deleteEnd)
                    {
                        DBG(juce::String::formatted(
                            "Region '%s' deleted (partially overlapped deletion)",
                            region->getName().toRawUTF8()));
                        m_regionManager->removeRegion(i);
                    }
                    // Case 4: Region starts before deletion but ends within - partial overlap (remove for simplicity)
                    else if (regionStart < m_startSample && regionEnd > m_startSample && regionEnd <= deleteEnd)
                    {
                        DBG(juce::String::formatted(
                            "Region '%s' deleted (partially overlapped deletion)",
                            region->getName().toRawUTF8()));
                        m_regionManager->removeRegion(i);
                    }
                    // Case 5: Region completely spans deletion (starts before, ends after) - remove for simplicity
                    else if (regionStart < m_startSample && regionEnd > deleteEnd)
                    {
                        DBG(juce::String::formatted(
                            "Region '%s' deleted (completely spanned deletion)",
                            region->getName().toRawUTF8()));
                        m_regionManager->removeRegion(i);
                    }
                    // Case 6: Region is completely after deletion - shift it back by deleted amount
                    else if (regionStart >= deleteEnd)
                    {
                        region->setStartSample(regionStart - m_numSamples);
                        region->setEndSample(regionEnd - m_numSamples);
                        DBG(juce::String::formatted(
                            "Region '%s' shifted back by %lld samples",
                            region->getName().toRawUTF8(),
                            m_numSamples));
                    }
                }
            }

            updatePlaybackAndDisplay();
        }

        return success;
    }

    bool undo() override
    {
        // Restore the deleted audio by inserting it back
        bool success = m_bufferManager.insertAudio(m_startSample, m_deletedAudio);

        if (success)
        {
            // Restore all saved region positions (undoes any shifts caused by the delete)
            if (m_regionManager && !m_savedRegions.isEmpty())
            {
                // Remove all current regions
                m_regionManager->removeAllRegions();

                // Restore original regions with their original positions
                for (const auto& region : m_savedRegions)
                {
                    m_regionManager->addRegion(region);
                }
            }

            updatePlaybackAndDisplay();
        }

        return success;
    }

    int getSizeInUnits() override
    {
        // Return approximate memory usage in bytes
        return static_cast<int>(static_cast<size_t>(m_deletedAudio.getNumSamples()) *
                                static_cast<size_t>(m_deletedAudio.getNumChannels()) * sizeof(float));
    }

private:
    int64_t m_startSample;
    int64_t m_numSamples;
    juce::AudioBuffer<float> m_deletedAudio;
    double m_sampleRate;
    juce::Array<Region> m_savedRegions;  // Saved region positions for undo

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeleteAction)
};

//==============================================================================
/**
 * Undoable insert/paste operation.
 * Stores information about the inserted audio and can remove it on undo.
 */
class InsertAction : public UndoableEditBase
{
public:
    InsertAction(AudioBufferManager& bufferManager,
                 AudioEngine& audioEngine,
                 WaveformDisplay& waveformDisplay,
                 int64_t insertPosition,
                 const juce::AudioBuffer<float>& audioToInsert,
                 RegionManager* regionManager = nullptr,
                 RegionDisplay* regionDisplay = nullptr,
                 MarkerManager* markerManager = nullptr,
                 MarkerDisplay* markerDisplay = nullptr)
        : UndoableEditBase(bufferManager, audioEngine, waveformDisplay, regionManager, regionDisplay),
          m_insertPosition(insertPosition),
          m_numSamples(audioToInsert.getNumSamples()),
          m_audioToInsert(audioToInsert.getNumChannels(), audioToInsert.getNumSamples()),
          m_markerManager(markerManager),
          m_markerDisplay(markerDisplay)
    {
        // Validate buffer state
        jassert(m_bufferManager.hasAudioData());
        jassert(insertPosition >= 0 && insertPosition <= m_bufferManager.getNumSamples());
        jassert(audioToInsert.getNumSamples() > 0);
        jassert(audioToInsert.getNumChannels() > 0);

        // Store a copy of the audio to insert
        for (int ch = 0; ch < audioToInsert.getNumChannels(); ++ch)
        {
            m_audioToInsert.copyFrom(ch, 0, audioToInsert, ch, 0, audioToInsert.getNumSamples());
        }

        m_sampleRate = m_bufferManager.getSampleRate();

        // Save all region positions BEFORE the insert (for undo), same
        // convention as DeleteAction.
        if (m_regionManager)
        {
            for (int i = 0; i < m_regionManager->getNumRegions(); ++i)
            {
                if (const auto* region = m_regionManager->getRegion(i))
                    m_savedRegions.add(*region);
            }
        }

        // Save all marker positions BEFORE the insert (for undo), mirroring the
        // region snapshot above. Markers are points, not ranges (H3).
        if (m_markerManager)
        {
            for (int i = 0; i < m_markerManager->getNumMarkers(); ++i)
            {
                if (const auto* marker = m_markerManager->getMarker(i))
                    m_savedMarkers.add(*marker);
            }
        }
    }

    bool perform() override
    {
        // Perform the insert operation
        bool success = m_bufferManager.insertAudio(m_insertPosition, m_audioToInsert);

        if (success)
        {
            // Region shift (mirrors DeleteAction's region handling): inserting
            // audio must not leave regions pointing at the pre-edit timeline.
            // - Region entirely before the insertion point: unaffected.
            // - Region entirely at/after the insertion point: shift forward
            //   by the inserted length.
            // - Insertion point falls inside the region: the region grows to
            //   include the newly inserted audio (its start doesn't move).
            if (m_regionManager && m_regionManager->getNumRegions() > 0)
            {
                for (int i = 0; i < m_regionManager->getNumRegions(); ++i)
                {
                    Region* region = m_regionManager->getRegion(i);
                    if (!region) continue;

                    const int64_t regionStart = region->getStartSample();
                    const int64_t regionEnd = region->getEndSample();

                    if (regionEnd <= m_insertPosition)
                    {
                        continue;
                    }
                    else if (regionStart >= m_insertPosition)
                    {
                        region->setStartSample(regionStart + m_numSamples);
                        region->setEndSample(regionEnd + m_numSamples);
                    }
                    else
                    {
                        region->setEndSample(regionEnd + m_numSamples);
                    }
                }
            }

            // Marker shift (points, not ranges): a marker AT or AFTER the insert
            // point moves forward by the inserted length so it keeps pointing at
            // the same audio; a marker strictly before it is unaffected. Mirrors
            // the region rule (regionStart >= insertPosition shifts) for the
            // zero-width case. (H3)
            if (m_markerManager && m_markerManager->getNumMarkers() > 0)
            {
                for (int i = 0; i < m_markerManager->getNumMarkers(); ++i)
                {
                    if (Marker* marker = m_markerManager->getMarker(i))
                    {
                        if (marker->getPosition() >= m_insertPosition)
                            marker->setPosition(marker->getPosition() + m_numSamples);
                    }
                }
            }

            updatePlaybackAndDisplay();
            refreshMarkerDisplay(m_markerDisplay);
        }

        return success;
    }

    bool undo() override
    {
        // Undo the insert by deleting the inserted audio
        bool success = m_bufferManager.deleteRange(m_insertPosition, m_numSamples);

        if (success)
        {
            // Restore all saved region positions (undoes the shift/grow above).
            if (m_regionManager && !m_savedRegions.isEmpty())
            {
                m_regionManager->removeAllRegions();
                for (const auto& region : m_savedRegions)
                    m_regionManager->addRegion(region);
            }

            // Restore all saved marker positions (undoes the shift above).
            if (m_markerManager && !m_savedMarkers.isEmpty())
            {
                m_markerManager->removeAllMarkers();
                for (const auto& marker : m_savedMarkers)
                    m_markerManager->addMarker(marker);
            }

            updatePlaybackAndDisplay();
            refreshMarkerDisplay(m_markerDisplay);
        }

        return success;
    }

    int getSizeInUnits() override
    {
        // Return approximate memory usage in bytes
        return static_cast<int>(static_cast<size_t>(m_audioToInsert.getNumSamples()) *
                                static_cast<size_t>(m_audioToInsert.getNumChannels()) * sizeof(float));
    }

private:
    int64_t m_insertPosition;
    int64_t m_numSamples;
    juce::AudioBuffer<float> m_audioToInsert;
    double m_sampleRate;
    juce::Array<Region> m_savedRegions;  // Saved region positions for undo
    MarkerManager* m_markerManager;      // Optional - may be nullptr if no markers
    MarkerDisplay* m_markerDisplay;      // Optional - may be nullptr if no marker display
    juce::Array<Marker> m_savedMarkers;  // Saved marker positions for undo

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InsertAction)
};

//==============================================================================
/**
 * Undoable replace operation (used for paste-over-selection).
 * Stores both the deleted and inserted audio data.
 */
class ReplaceAction : public UndoableEditBase
{
public:
    ReplaceAction(AudioBufferManager& bufferManager,
                  AudioEngine& audioEngine,
                  WaveformDisplay& waveformDisplay,
                  int64_t startSample,
                  int64_t numSamplesToReplace,
                  const juce::AudioBuffer<float>& newAudio,
                  RegionManager* regionManager = nullptr,
                  RegionDisplay* regionDisplay = nullptr,
                  MarkerManager* markerManager = nullptr,
                  MarkerDisplay* markerDisplay = nullptr)
        : UndoableEditBase(bufferManager, audioEngine, waveformDisplay, regionManager, regionDisplay),
          m_startSample(startSample),
          m_numSamplesToReplace(numSamplesToReplace),
          m_newAudio(newAudio.getNumChannels(), newAudio.getNumSamples()),
          m_markerManager(markerManager),
          m_markerDisplay(markerDisplay)
    {
        // Validate buffer state
        jassert(m_bufferManager.hasAudioData());
        jassert(startSample >= 0 && startSample < m_bufferManager.getNumSamples());
        jassert(numSamplesToReplace > 0 && (startSample + numSamplesToReplace) <= m_bufferManager.getNumSamples());
        jassert(newAudio.getNumSamples() > 0);
        jassert(newAudio.getNumChannels() > 0);

        // Store the original audio that will be replaced
        m_originalAudio = m_bufferManager.getAudioRange(startSample, numSamplesToReplace);

        // Store a copy of the new audio
        for (int ch = 0; ch < newAudio.getNumChannels(); ++ch)
        {
            m_newAudio.copyFrom(ch, 0, newAudio, ch, 0, newAudio.getNumSamples());
        }

        m_sampleRate = m_bufferManager.getSampleRate();

        // Save all region positions BEFORE the replace (for undo), same
        // convention as DeleteAction.
        if (m_regionManager)
        {
            for (int i = 0; i < m_regionManager->getNumRegions(); ++i)
            {
                if (const auto* region = m_regionManager->getRegion(i))
                    m_savedRegions.add(*region);
            }
        }

        // Save all marker positions BEFORE the replace (for undo), mirroring the
        // region snapshot above (H3).
        if (m_markerManager)
        {
            for (int i = 0; i < m_markerManager->getNumMarkers(); ++i)
            {
                if (const auto* marker = m_markerManager->getMarker(i))
                    m_savedMarkers.add(*marker);
            }
        }
    }

    bool perform() override
    {
        // Perform the replace operation
        bool success = m_bufferManager.replaceRange(m_startSample, m_numSamplesToReplace, m_newAudio);

        if (success)
        {
            shiftRegionsForReplace(m_numSamplesToReplace, m_newAudio.getNumSamples());
            shiftMarkersForReplace(m_numSamplesToReplace, m_newAudio.getNumSamples());
            updatePlaybackAndDisplay();
            refreshMarkerDisplay(m_markerDisplay);
        }

        return success;
    }

    bool undo() override
    {
        // Undo the replace by restoring the original audio
        bool success = m_bufferManager.replaceRange(m_startSample, m_newAudio.getNumSamples(), m_originalAudio);

        if (success)
        {
            // Restore all saved region positions (undoes the shift/removal above).
            if (m_regionManager && !m_savedRegions.isEmpty())
            {
                m_regionManager->removeAllRegions();
                for (const auto& region : m_savedRegions)
                    m_regionManager->addRegion(region);
            }

            // Restore all saved marker positions (undoes the shift/removal above).
            if (m_markerManager && !m_savedMarkers.isEmpty())
            {
                m_markerManager->removeAllMarkers();
                for (const auto& marker : m_savedMarkers)
                    m_markerManager->addMarker(marker);
            }

            updatePlaybackAndDisplay();
            refreshMarkerDisplay(m_markerDisplay);
        }

        return success;
    }

    int getSizeInUnits() override
    {
        // Return approximate memory usage in bytes (both buffers)
        size_t originalSize = static_cast<size_t>(m_originalAudio.getNumSamples()) *
                              static_cast<size_t>(m_originalAudio.getNumChannels()) * sizeof(float);
        size_t newSize = static_cast<size_t>(m_newAudio.getNumSamples()) *
                         static_cast<size_t>(m_newAudio.getNumChannels()) * sizeof(float);
        return static_cast<int>(originalSize + newSize);
    }

private:
    /**
     * Region shift for a (possibly length-changing) replace -- e.g.
     * paste-over-selection where the pasted audio is longer or shorter than
     * the replaced range. Without this, regions after the edit point keep
     * their pre-edit sample positions and silently drift out of sync with
     * the audio (the same defect class DeleteAction already guards
     * against).
     * - Region entirely before the replaced range: unaffected.
     * - Region entirely after the replaced range: shift by the length delta
     *   (newLength - oldLength; may be negative if the replacement is shorter).
     * - Region overlapping the replaced range: the underlying audio content
     *   changed, so -- matching DeleteAction's convention for the same
     *   ambiguous case -- remove it rather than guess new boundaries.
     */
    void shiftRegionsForReplace(int64_t oldLength, int64_t newLength)
    {
        if (!m_regionManager || m_regionManager->getNumRegions() == 0)
            return;

        const int64_t delta = newLength - oldLength;
        const int64_t replaceEnd = m_startSample + oldLength;

        for (int i = m_regionManager->getNumRegions() - 1; i >= 0; --i)
        {
            Region* region = m_regionManager->getRegion(i);
            if (!region) continue;

            const int64_t regionStart = region->getStartSample();
            const int64_t regionEnd = region->getEndSample();

            if (regionEnd <= m_startSample)
            {
                continue;
            }
            else if (regionStart >= replaceEnd)
            {
                if (delta != 0)
                {
                    region->setStartSample(regionStart + delta);
                    region->setEndSample(regionEnd + delta);
                }
            }
            else
            {
                m_regionManager->removeRegion(i);
            }
        }
    }

    /**
     * Marker equivalent of shiftRegionsForReplace(), treating each marker as a
     * zero-width point (H3):
     * - Marker at or before the replaced range start: unaffected.
     * - Marker at or after the replaced range end: shift by the length delta
     *   (newLength - oldLength; negative when the replacement is shorter).
     * - Marker strictly INSIDE the replaced range: the audio it pointed to was
     *   overwritten, so -- matching the region "overlapping -> remove"
     *   convention -- remove it rather than guess a new position.
     */
    void shiftMarkersForReplace(int64_t oldLength, int64_t newLength)
    {
        if (!m_markerManager || m_markerManager->getNumMarkers() == 0)
            return;

        const int64_t delta = newLength - oldLength;
        const int64_t replaceEnd = m_startSample + oldLength;

        for (int i = m_markerManager->getNumMarkers() - 1; i >= 0; --i)
        {
            Marker* marker = m_markerManager->getMarker(i);
            if (!marker) continue;

            const int64_t pos = marker->getPosition();

            if (pos <= m_startSample)
            {
                continue;
            }
            else if (pos >= replaceEnd)
            {
                if (delta != 0)
                    marker->setPosition(pos + delta);
            }
            else
            {
                m_markerManager->removeMarker(i);
            }
        }
    }

    int64_t m_startSample;
    int64_t m_numSamplesToReplace;
    juce::AudioBuffer<float> m_originalAudio;
    juce::AudioBuffer<float> m_newAudio;
    double m_sampleRate;
    juce::Array<Region> m_savedRegions;  // Saved region positions for undo
    MarkerManager* m_markerManager;      // Optional - may be nullptr if no markers
    MarkerDisplay* m_markerDisplay;      // Optional - may be nullptr if no marker display
    juce::Array<Marker> m_savedMarkers;  // Saved marker positions for undo

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReplaceAction)
};
