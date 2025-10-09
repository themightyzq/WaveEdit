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

/**
 * Base class for undoable edit operations.
 * Provides common functionality for all edit operations that need undo/redo.
 */
class UndoableEditBase : public juce::UndoableAction
{
public:
    UndoableEditBase(AudioBufferManager& bufferManager,
                     AudioEngine& audioEngine,
                     WaveformDisplay& waveformDisplay)
        : m_bufferManager(bufferManager),
          m_audioEngine(audioEngine),
          m_waveformDisplay(waveformDisplay)
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
    }

    AudioBufferManager& m_bufferManager;
    AudioEngine& m_audioEngine;
    WaveformDisplay& m_waveformDisplay;

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
                 int64_t numSamples)
        : UndoableEditBase(bufferManager, audioEngine, waveformDisplay),
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
    }

    bool perform() override
    {
        // Perform the delete operation
        bool success = m_bufferManager.deleteRange(m_startSample, m_numSamples);

        if (success)
        {
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
