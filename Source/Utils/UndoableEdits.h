/*
  ==============================================================================

    UndoableEdits.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

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
        return static_cast<int>(m_deletedAudio.getNumSamples() * m_deletedAudio.getNumChannels() * sizeof(float));
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
        return static_cast<int>(m_audioToInsert.getNumSamples() * m_audioToInsert.getNumChannels() * sizeof(float));
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
        int originalSize = m_originalAudio.getNumSamples() * m_originalAudio.getNumChannels() * sizeof(float);
        int newSize = m_newAudio.getNumSamples() * m_newAudio.getNumChannels() * sizeof(float);
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
