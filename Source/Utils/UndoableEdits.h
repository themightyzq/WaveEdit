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
#include "RegionManager.h"
#include "Region.h"
#include "../DSP/ParametricEQ.h"
#include "../DSP/DynamicParametricEQ.h"

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
     * Updates both the audio engine and waveform display after a buffer modification.
     * This ensures the user can both hear and see the changes.
     *
     * Thread Safety: Stops playback first to prevent race conditions during buffer updates.
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
            juce::Logger::writeToLog("ERROR: Failed to update audio engine after undo/redo");
        }

        // CRITICAL FIX: Use reloadFromBuffer() with preserve flags
        // This preserves view position and edit cursor for seamless workflow
        if (!m_waveformDisplay.reloadFromBuffer(
            m_bufferManager.getBuffer(),
            m_bufferManager.getSampleRate(),
            true,  // preserveView = true
            true)) // preserveEditCursor = true
        {
            juce::Logger::writeToLog("Warning: Failed to update waveform display after undo/redo");
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

    AudioBufferManager& m_bufferManager;
    AudioEngine& m_audioEngine;
    WaveformDisplay& m_waveformDisplay;
    RegionManager* m_regionManager;    // Optional - may be nullptr if no regions exist
    RegionDisplay* m_regionDisplay;    // Optional - may be nullptr if no region display

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
                        juce::Logger::writeToLog(juce::String::formatted(
                            "Region '%s' deleted (was within deleted range)",
                            region->getName().toRawUTF8()));
                        m_regionManager->removeRegion(i);
                    }
                    // Case 3: Region starts within deletion but ends after - partial overlap (remove for simplicity)
                    else if (regionStart >= m_startSample && regionStart < deleteEnd && regionEnd > deleteEnd)
                    {
                        juce::Logger::writeToLog(juce::String::formatted(
                            "Region '%s' deleted (partially overlapped deletion)",
                            region->getName().toRawUTF8()));
                        m_regionManager->removeRegion(i);
                    }
                    // Case 4: Region starts before deletion but ends within - partial overlap (remove for simplicity)
                    else if (regionStart < m_startSample && regionEnd > m_startSample && regionEnd <= deleteEnd)
                    {
                        juce::Logger::writeToLog(juce::String::formatted(
                            "Region '%s' deleted (partially overlapped deletion)",
                            region->getName().toRawUTF8()));
                        m_regionManager->removeRegion(i);
                    }
                    // Case 5: Region completely spans deletion (starts before, ends after) - remove for simplicity
                    else if (regionStart < m_startSample && regionEnd > deleteEnd)
                    {
                        juce::Logger::writeToLog(juce::String::formatted(
                            "Region '%s' deleted (completely spanned deletion)",
                            region->getName().toRawUTF8()));
                        m_regionManager->removeRegion(i);
                    }
                    // Case 6: Region is completely after deletion - shift it back by deleted amount
                    else if (regionStart >= deleteEnd)
                    {
                        region->setStartSample(regionStart - m_numSamples);
                        region->setEndSample(regionEnd - m_numSamples);
                        juce::Logger::writeToLog(juce::String::formatted(
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
                 const juce::AudioBuffer<float>& audioToInsert)
        : UndoableEditBase(bufferManager, audioEngine, waveformDisplay),
          m_insertPosition(insertPosition),
          m_numSamples(audioToInsert.getNumSamples()),
          m_audioToInsert(audioToInsert.getNumChannels(), audioToInsert.getNumSamples())
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
    }

    bool perform() override
    {
        // Perform the insert operation
        bool success = m_bufferManager.insertAudio(m_insertPosition, m_audioToInsert);

        if (success)
        {
            updatePlaybackAndDisplay();
        }

        return success;
    }

    bool undo() override
    {
        // Undo the insert by deleting the inserted audio
        bool success = m_bufferManager.deleteRange(m_insertPosition, m_numSamples);

        if (success)
        {
            updatePlaybackAndDisplay();
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
                  const juce::AudioBuffer<float>& newAudio)
        : UndoableEditBase(bufferManager, audioEngine, waveformDisplay),
          m_startSample(startSample),
          m_numSamplesToReplace(numSamplesToReplace),
          m_newAudio(newAudio.getNumChannels(), newAudio.getNumSamples())
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
    }

    bool perform() override
    {
        // Perform the replace operation
        bool success = m_bufferManager.replaceRange(m_startSample, m_numSamplesToReplace, m_newAudio);

        if (success)
        {
            updatePlaybackAndDisplay();
        }

        return success;
    }

    bool undo() override
    {
        // Undo the replace by restoring the original audio
        bool success = m_bufferManager.replaceRange(m_startSample, m_newAudio.getNumSamples(), m_originalAudio);

        if (success)
        {
            updatePlaybackAndDisplay();
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
    int64_t m_startSample;
    int64_t m_numSamplesToReplace;
    juce::AudioBuffer<float> m_originalAudio;
    juce::AudioBuffer<float> m_newAudio;
    double m_sampleRate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReplaceAction)
};

//==============================================================================
/**
 * Undoable region boundary nudge operation.
 * Stores the old and new boundary positions for a region.
 */
class NudgeRegionUndoAction : public juce::UndoableAction
{
public:
    /**
     * Creates a nudge action for a region boundary.
     *
     * @param regionManager Reference to the RegionManager
     * @param regionDisplay Optional pointer to RegionDisplay for repainting
     * @param regionIndex Index of the region to nudge
     * @param nudgeType Which boundary to nudge (start or end)
     * @param oldPosition Old boundary position in samples
     * @param newPosition New boundary position in samples
     */
    NudgeRegionUndoAction(RegionManager& regionManager,
                          RegionDisplay* regionDisplay,
                          int regionIndex,
                          bool nudgeStart,
                          int64_t oldPosition,
                          int64_t newPosition)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_regionIndex(regionIndex),
          m_nudgeStart(nudgeStart),
          m_oldPosition(oldPosition),
          m_newPosition(newPosition)
    {
        jassert(regionIndex >= 0 && regionIndex < m_regionManager.getNumRegions());
    }

    bool perform() override
    {
        // Apply the new boundary position
        Region* region = m_regionManager.getRegion(m_regionIndex);
        if (!region) return false;

        if (m_nudgeStart)
            region->setStartSample(m_newPosition);
        else
            region->setEndSample(m_newPosition);

        // Update visual display
        if (m_regionDisplay)
            m_regionDisplay->repaint();

        return true;
    }

    bool undo() override
    {
        // Restore the old boundary position
        Region* region = m_regionManager.getRegion(m_regionIndex);
        if (!region) return false;

        if (m_nudgeStart)
            region->setStartSample(m_oldPosition);
        else
            region->setEndSample(m_oldPosition);

        // Update visual display
        if (m_regionDisplay)
            m_regionDisplay->repaint();

        return true;
    }

    int getSizeInUnits() override
    {
        // Minimal memory footprint (just storing integers)
        return sizeof(*this);
    }

private:
    RegionManager& m_regionManager;
    RegionDisplay* m_regionDisplay;
    int m_regionIndex;
    bool m_nudgeStart;  // true = nudge start, false = nudge end
    int64_t m_oldPosition;
    int64_t m_newPosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NudgeRegionUndoAction)
};

//==============================================================================
/**
 * Undoable batch rename operation for multiple regions.
 * Stores old and new names for all renamed regions.
 */
class BatchRenameRegionUndoAction : public juce::UndoableAction
{
public:
    /**
     * Creates a batch rename action.
     *
     * @param regionManager Reference to the RegionManager
     * @param regionDisplay Optional pointer to RegionDisplay for repainting
     * @param regionIndices Indices of regions to rename
     * @param oldNames Old names of the regions
     * @param newNames New names for the regions
     */
    BatchRenameRegionUndoAction(RegionManager& regionManager,
                                 RegionDisplay* regionDisplay,
                                 const std::vector<int>& regionIndices,
                                 const std::vector<juce::String>& oldNames,
                                 const std::vector<juce::String>& newNames)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_regionIndices(regionIndices),
          m_oldNames(oldNames),
          m_newNames(newNames)
    {
        jassert(regionIndices.size() == oldNames.size());
        jassert(regionIndices.size() == newNames.size());
        jassert(!regionIndices.empty());
    }

    bool perform() override
    {
        // Apply new names to all regions
        for (size_t i = 0; i < m_regionIndices.size(); ++i)
        {
            Region* region = m_regionManager.getRegion(m_regionIndices[i]);
            if (!region) return false;

            region->setName(m_newNames[i]);
        }

        // Update visual display
        if (m_regionDisplay)
            m_regionDisplay->repaint();

        return true;
    }

    bool undo() override
    {
        // Restore old names to all regions
        for (size_t i = 0; i < m_regionIndices.size(); ++i)
        {
            Region* region = m_regionManager.getRegion(m_regionIndices[i]);
            if (!region) return false;

            region->setName(m_oldNames[i]);
        }

        // Update visual display
        if (m_regionDisplay)
            m_regionDisplay->repaint();

        return true;
    }

    int getSizeInUnits() override
    {
        // Approximate memory usage (indices + string data)
        int size = sizeof(*this);
        for (const auto& name : m_oldNames)
            size += name.length();
        for (const auto& name : m_newNames)
            size += name.length();
        return size;
    }

private:
    RegionManager& m_regionManager;
    RegionDisplay* m_regionDisplay;
    std::vector<int> m_regionIndices;
    std::vector<juce::String> m_oldNames;
    std::vector<juce::String> m_newNames;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BatchRenameRegionUndoAction)
};

//==============================================================================
/**
 * Undoable merge operation for two adjacent regions.
 * Stores original regions so they can be restored on undo.
 */
class MergeRegionsUndoAction : public juce::UndoableAction
{
public:
    /**
     * Creates a merge regions action.
     *
     * @param regionManager Reference to the RegionManager
     * @param regionDisplay Optional pointer to RegionDisplay for repainting
     * @param firstIndex Index of the first region
     * @param secondIndex Index of the second region
     * @param originalFirst Copy of the first region before merge
     * @param originalSecond Copy of the second region before merge
     */
    MergeRegionsUndoAction(RegionManager& regionManager,
                           RegionDisplay* regionDisplay,
                           int firstIndex,
                           int secondIndex,
                           const Region& originalFirst,
                           const Region& originalSecond)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_firstIndex(firstIndex),
          m_secondIndex(secondIndex),
          m_originalFirst(originalFirst),
          m_originalSecond(originalSecond)
    {
    }

    bool perform() override
    {
        bool success = m_regionManager.mergeRegions(m_firstIndex, m_secondIndex);
        if (success && m_regionDisplay)
            m_regionDisplay->repaint();
        return success;
    }

    bool undo() override
    {
        // Restore the original two regions
        Region* mergedRegion = m_regionManager.getRegion(m_firstIndex);
        if (!mergedRegion) return false;

        // Verify index is still valid before insertion
        if (m_secondIndex > m_regionManager.getNumRegions())
        {
            juce::Logger::writeToLog("Warning: Cannot undo merge - region index out of bounds");
            return false;
        }

        // Restore first region's original state
        mergedRegion->setName(m_originalFirst.getName());
        mergedRegion->setEndSample(m_originalFirst.getEndSample());

        // Re-insert the second region
        m_regionManager.insertRegionAt(m_secondIndex, m_originalSecond);

        if (m_regionDisplay)
            m_regionDisplay->repaint();

        return true;
    }

    int getSizeInUnits() override { return sizeof(*this); }

private:
    RegionManager& m_regionManager;
    RegionDisplay* m_regionDisplay;
    int m_firstIndex, m_secondIndex;
    Region m_originalFirst, m_originalSecond;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MergeRegionsUndoAction)
};

//==============================================================================
/**
 * Undoable split operation for a region.
 * Stores original region so it can be restored on undo.
 */
class SplitRegionUndoAction : public juce::UndoableAction
{
public:
    /**
     * Creates a split region action.
     *
     * @param regionManager Reference to the RegionManager
     * @param regionDisplay Optional pointer to RegionDisplay for repainting
     * @param regionIndex Index of the region to split
     * @param splitSample Sample position for the split
     * @param originalRegion Copy of the region before split
     */
    SplitRegionUndoAction(RegionManager& regionManager,
                          RegionDisplay* regionDisplay,
                          int regionIndex,
                          int64_t splitSample,
                          const Region& originalRegion)
        : m_regionManager(regionManager),
          m_regionDisplay(regionDisplay),
          m_regionIndex(regionIndex),
          m_splitSample(splitSample),
          m_originalRegion(originalRegion)
    {
    }

    bool perform() override
    {
        bool success = m_regionManager.splitRegion(m_regionIndex, m_splitSample);
        if (success && m_regionDisplay)
            m_regionDisplay->repaint();
        return success;
    }

    bool undo() override
    {
        // Remove the second half of the split region
        m_regionManager.removeRegion(m_regionIndex + 1);

        // Restore first region to original state
        Region* firstHalf = m_regionManager.getRegion(m_regionIndex);
        if (!firstHalf) return false;

        firstHalf->setName(m_originalRegion.getName());
        firstHalf->setEndSample(m_originalRegion.getEndSample());

        if (m_regionDisplay)
            m_regionDisplay->repaint();

        return true;
    }

    int getSizeInUnits() override { return sizeof(*this); }

private:
    RegionManager& m_regionManager;
    RegionDisplay* m_regionDisplay;
    int m_regionIndex;
    int64_t m_splitSample;
    Region m_originalRegion;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SplitRegionUndoAction)
};

//==============================================================================
/**
 * Undoable action for applying 3-band parametric EQ to audio selection.
 */
class ApplyParametricEQAction : public UndoableEditBase
{
public:
    ApplyParametricEQAction(AudioBufferManager& bufferManager,
                           AudioEngine& audioEngine,
                           WaveformDisplay& waveformDisplay,
                           int64_t startSample,
                           int64_t numSamples,
                           const ParametricEQ::Parameters& eqParams)
        : UndoableEditBase(bufferManager, audioEngine, waveformDisplay),
          m_startSample(startSample),
          m_numSamples(numSamples),
          m_eqParams(eqParams)
    {
        // Validate parameters
        jassert(m_bufferManager.hasAudioData());
        jassert(startSample >= 0 && startSample < m_bufferManager.getNumSamples());
        jassert(numSamples > 0 && (startSample + numSamples) <= m_bufferManager.getNumSamples());

        // Store original audio for undo
        m_originalAudio = m_bufferManager.getAudioRange(startSample, numSamples);

        m_sampleRate = m_bufferManager.getSampleRate();
    }

    bool perform() override
    {
        // Get the audio range to process
        auto audioToProcess = m_bufferManager.getAudioRange(m_startSample, m_numSamples);

        // Apply EQ
        ParametricEQ eq;
        eq.prepare(m_sampleRate, audioToProcess.getNumSamples());
        eq.applyEQ(audioToProcess, m_eqParams);

        // Replace in buffer
        bool success = m_bufferManager.replaceRange(m_startSample, m_numSamples, audioToProcess);

        if (success)
        {
            updatePlaybackAndDisplay();
        }

        return success;
    }

    bool undo() override
    {
        // Restore original audio
        bool success = m_bufferManager.replaceRange(m_startSample, m_numSamples, m_originalAudio);

        if (success)
        {
            updatePlaybackAndDisplay();
        }

        return success;
    }

    int getSizeInUnits() override
    {
        // Return approximate memory usage in bytes
        size_t size = static_cast<size_t>(m_originalAudio.getNumSamples()) *
                      static_cast<size_t>(m_originalAudio.getNumChannels()) * sizeof(float);
        return static_cast<int>(size);
    }

private:
    int64_t m_startSample;
    int64_t m_numSamples;
    ParametricEQ::Parameters m_eqParams;
    juce::AudioBuffer<float> m_originalAudio;
    double m_sampleRate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ApplyParametricEQAction)
};

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
        // Validate parameters
        jassert(m_bufferManager.hasAudioData());
        jassert(startSample >= 0 && startSample < m_bufferManager.getNumSamples());
        jassert(numSamples > 0 && (startSample + numSamples) <= m_bufferManager.getNumSamples());

        // Store original audio for undo
        m_originalAudio = m_bufferManager.getAudioRange(startSample, numSamples);

        m_sampleRate = m_bufferManager.getSampleRate();
    }

    bool perform() override
    {
        // Get the audio range to process
        auto audioToProcess = m_bufferManager.getAudioRange(m_startSample, m_numSamples);

        // Apply EQ using DynamicParametricEQ
        DynamicParametricEQ eq;
        eq.prepare(m_sampleRate, static_cast<int>(audioToProcess.getNumSamples()));
        eq.setParameters(m_eqParams);
        eq.applyEQ(audioToProcess);

        // Replace in buffer
        bool success = m_bufferManager.replaceRange(m_startSample, m_numSamples, audioToProcess);

        if (success)
        {
            updatePlaybackAndDisplay();
        }

        return success;
    }

    bool undo() override
    {
        // Restore original audio
        bool success = m_bufferManager.replaceRange(m_startSample, m_numSamples, m_originalAudio);

        if (success)
        {
            updatePlaybackAndDisplay();
        }

        return success;
    }

    int getSizeInUnits() override
    {
        // Return approximate memory usage in bytes
        size_t size = static_cast<size_t>(m_originalAudio.getNumSamples()) *
                      static_cast<size_t>(m_originalAudio.getNumChannels()) * sizeof(float);
        return static_cast<int>(size);
    }

private:
    int64_t m_startSample;
    int64_t m_numSamples;
    DynamicParametricEQ::Parameters m_eqParams;
    juce::AudioBuffer<float> m_originalAudio;
    double m_sampleRate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ApplyDynamicParametricEQAction)
};

//==============================================================================
/**
 * Undoable action for applying plugin chain effects to a selection.
 *
 * This action receives the already-processed audio buffer and handles
 * the undo/redo by storing original/processed audio and swapping them.
 *
 * Thread Safety:
 * - Must be created and performed from message thread
 * - updatePlaybackAndDisplay() stops playback before buffer modification
 *
 * Usage:
 * The plugin chain rendering is done asynchronously before creating this action:
 * 1. User invokes "Apply Plugin Chain"
 * 2. PluginChainRenderer processes audio in background
 * 3. On success, create ApplyPluginChainAction with processed buffer
 * 4. UndoManager::perform() applies it
 */
class ApplyPluginChainAction : public UndoableEditBase
{
public:
    /**
     * Creates an undo action for plugin chain application.
     *
     * @param bufferManager Reference to the document's buffer manager
     * @param audioEngine Reference to the document's audio engine
     * @param waveformDisplay Reference to the waveform display for updates
     * @param startSample Start of the affected range
     * @param numSamples Length of the original range being replaced
     * @param processedAudio The new audio data to apply (already rendered, may be longer than numSamples for effect tail)
     * @param chainDescription Human-readable description of the chain for undo menu
     */
    ApplyPluginChainAction(AudioBufferManager& bufferManager,
                          AudioEngine& audioEngine,
                          WaveformDisplay& waveformDisplay,
                          int64_t startSample,
                          int64_t numSamples,
                          const juce::AudioBuffer<float>& processedAudio,
                          const juce::String& chainDescription)
        : UndoableEditBase(bufferManager, audioEngine, waveformDisplay),
          m_startSample(startSample),
          m_numSamples(numSamples),
          m_processedAudio(processedAudio.getNumChannels(), processedAudio.getNumSamples()),
          m_chainDescription(chainDescription)
    {
        // Validate parameters
        jassert(m_bufferManager.hasAudioData());
        jassert(startSample >= 0 && startSample < m_bufferManager.getNumSamples());
        jassert(numSamples > 0 && (startSample + numSamples) <= m_bufferManager.getNumSamples());
        // Note: processedAudio may be longer than numSamples when including effect tail
        jassert(processedAudio.getNumSamples() >= numSamples);

        // Store original audio for undo
        m_originalAudio = m_bufferManager.getAudioRange(startSample, numSamples);

        // Copy processed audio
        for (int ch = 0; ch < processedAudio.getNumChannels(); ++ch)
        {
            m_processedAudio.copyFrom(ch, 0, processedAudio, ch, 0, processedAudio.getNumSamples());
        }

        m_sampleRate = m_bufferManager.getSampleRate();
    }

    /**
     * Mark this action as already performed.
     * Use when the processing was done by ProgressDialog before registering with undo system.
     */
    void markAsAlreadyPerformed() { m_alreadyPerformed = true; }

    bool perform() override
    {
        // If already performed via progress dialog, skip - just return success
        if (m_alreadyPerformed)
        {
            m_alreadyPerformed = false;  // Reset for redo
            return true;
        }

        // Replace original audio with processed audio
        bool success = m_bufferManager.replaceRange(m_startSample, m_numSamples, m_processedAudio);

        if (success)
        {
            updatePlaybackAndDisplay();
        }

        return success;
    }

    bool undo() override
    {
        // Restore original audio
        // Note: When effect tail was included, m_processedAudio is larger than m_originalAudio.
        // We need to replace the extended range (processedAudio size) with the original range.
        int64_t samplesToReplace = m_processedAudio.getNumSamples();
        bool success = m_bufferManager.replaceRange(m_startSample, samplesToReplace, m_originalAudio);

        if (success)
        {
            updatePlaybackAndDisplay();
        }

        return success;
    }

    int getSizeInUnits() override
    {
        // Return approximate memory usage in bytes (original + processed buffers)
        size_t originalSize = static_cast<size_t>(m_originalAudio.getNumSamples()) *
                              static_cast<size_t>(m_originalAudio.getNumChannels()) * sizeof(float);
        size_t processedSize = static_cast<size_t>(m_processedAudio.getNumSamples()) *
                               static_cast<size_t>(m_processedAudio.getNumChannels()) * sizeof(float);
        return static_cast<int>(originalSize + processedSize);
    }

    /**
     * Gets the undo/redo action name.
     * Returns "Apply Plugin Chain: <list of plugin names>"
     */
    juce::String getName() const
    {
        return "Apply Plugin Chain: " + m_chainDescription;
    }

private:
    int64_t m_startSample;
    int64_t m_numSamples;
    juce::AudioBuffer<float> m_originalAudio;
    juce::AudioBuffer<float> m_processedAudio;
    juce::String m_chainDescription;
    double m_sampleRate;
    bool m_alreadyPerformed = false;  ///< Skip first perform() when action was pre-applied

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ApplyPluginChainAction)
};

//==============================================================================
/**
 * Undoable action for channel conversion (e.g., stereo to mono, mono to 5.1, etc.)
 * Stores the entire original buffer since channel count changes.
 */
class ChannelConvertAction : public UndoableEditBase
{
public:
    /**
     * Creates a channel conversion action.
     *
     * @param bufferManager Reference to the document's buffer manager
     * @param audioEngine Reference to the document's audio engine
     * @param waveformDisplay Reference to the waveform display for updates
     * @param convertedBuffer The new audio data with target channel count (already converted)
     */
    ChannelConvertAction(AudioBufferManager& bufferManager,
                         AudioEngine& audioEngine,
                         WaveformDisplay& waveformDisplay,
                         const juce::AudioBuffer<float>& convertedBuffer)
        : UndoableEditBase(bufferManager, audioEngine, waveformDisplay),
          m_convertedBuffer(convertedBuffer.getNumChannels(), convertedBuffer.getNumSamples())
    {
        // Store the entire original buffer (needed because channel count changes)
        const auto& buffer = m_bufferManager.getBuffer();
        m_originalBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples());
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            m_originalBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
        }

        // Copy converted buffer
        for (int ch = 0; ch < convertedBuffer.getNumChannels(); ++ch)
        {
            m_convertedBuffer.copyFrom(ch, 0, convertedBuffer, ch, 0, convertedBuffer.getNumSamples());
        }

        m_sampleRate = m_bufferManager.getSampleRate();
    }

    bool perform() override
    {
        // Replace entire buffer with converted version
        m_bufferManager.setBuffer(m_convertedBuffer, m_sampleRate);
        updatePlaybackAndDisplay();
        return true;
    }

    bool undo() override
    {
        // Restore original buffer
        m_bufferManager.setBuffer(m_originalBuffer, m_sampleRate);
        updatePlaybackAndDisplay();
        return true;
    }

    int getSizeInUnits() override
    {
        // Return approximate memory usage in bytes (both buffers)
        size_t originalSize = static_cast<size_t>(m_originalBuffer.getNumSamples()) *
                              static_cast<size_t>(m_originalBuffer.getNumChannels()) * sizeof(float);
        size_t convertedSize = static_cast<size_t>(m_convertedBuffer.getNumSamples()) *
                               static_cast<size_t>(m_convertedBuffer.getNumChannels()) * sizeof(float);
        return static_cast<int>(originalSize + convertedSize);
    }

private:
    juce::AudioBuffer<float> m_originalBuffer;
    juce::AudioBuffer<float> m_convertedBuffer;
    double m_sampleRate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelConvertAction)
};
