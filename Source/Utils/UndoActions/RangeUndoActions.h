/*
  ==============================================================================

    RangeUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Range-domain audio undo actions split out of AudioUndoActions.h per
    CLAUDE.md §7.5: silence (preserves length) and trim (changes length).

    Reach this header through the umbrella `AudioUndoActions.h`.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../../Audio/AudioBufferManager.h"
#include "../../Audio/AudioEngine.h"
#include "../../UI/WaveformDisplay.h"

//==============================================================================
/**
 * Undo action for silence.
 * Stores the before state and region information.
 */
class SilenceUndoAction : public juce::UndoableAction
{
public:
    SilenceUndoAction(AudioBufferManager& bufferManager,
                     WaveformDisplay& waveform,
                     AudioEngine& audioEngine,
                     const juce::AudioBuffer<float>& beforeBuffer,
                     int startSample,
                     int numSamples)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_beforeBuffer(),
          m_startSample(startSample),
          m_numSamples(numSamples)
    {
        // Store only the affected region to save memory
        m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
        m_beforeBuffer.makeCopyOf(beforeBuffer, true);
    }

    bool perform() override
    {
        // Silence the region using AudioBufferManager
        bool success = m_bufferManager.silenceRange(m_startSample, m_numSamples);

        if (!success)
        {
            DBG("SilenceUndoAction::perform - Failed to silence range");
            return false;
        }

        // Get the updated buffer
        auto& buffer = m_bufferManager.getMutableBuffer();

        // Reload buffer in AudioEngine - preserve playback if active
        m_audioEngine.reloadBufferPreservingPlayback(
            buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

        // Update waveform display - preserve view and selection
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                          true, true); // preserveView=true, preserveEditCursor=true

        // Log the operation
        DBG("Applied silence to selection");

        return true;
    }

    bool undo() override
    {
        // Restore the before state (only the affected region)
        auto& buffer = m_bufferManager.getMutableBuffer();

        // Copy the affected region from before buffer back to original position
        for (int ch = 0; ch < m_beforeBuffer.getNumChannels(); ++ch)
        {
            buffer.copyFrom(ch, m_startSample, m_beforeBuffer, ch, 0, m_numSamples);
        }

        // Reload buffer in AudioEngine - preserve playback if active
        m_audioEngine.reloadBufferPreservingPlayback(buffer, m_bufferManager.getSampleRate(),
                                                    buffer.getNumChannels());

        // Update waveform display - preserve view and selection
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                          true, true); // preserveView=true, preserveEditCursor=true

        return true;
    }

private:
    AudioBufferManager& m_bufferManager;
    WaveformDisplay& m_waveformDisplay;
    AudioEngine& m_audioEngine;
    juce::AudioBuffer<float> m_beforeBuffer;
    int m_startSample;
    int m_numSamples;
};

//==============================================================================
/**
 * Undo action for trim.
 * Stores the entire buffer before trimming since trim changes the file length.
 */
class TrimUndoAction : public juce::UndoableAction
{
public:
    TrimUndoAction(AudioBufferManager& bufferManager,
                  WaveformDisplay& waveform,
                  AudioEngine& audioEngine,
                  const juce::AudioBuffer<float>& beforeBuffer,
                  int startSample,
                  int numSamples)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_beforeBuffer(),
          m_startSample(startSample),
          m_numSamples(numSamples)
    {
        // Store entire buffer since trim changes the file length
        m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
        m_beforeBuffer.makeCopyOf(beforeBuffer, true);
    }

    bool perform() override
    {
        // Trim to the range using AudioBufferManager
        bool success = m_bufferManager.trimToRange(m_startSample, m_numSamples);

        if (!success)
        {
            DBG("TrimUndoAction::perform - Failed to trim range");
            return false;
        }

        // Get the updated buffer
        auto& buffer = m_bufferManager.getMutableBuffer();

        // Reload buffer in AudioEngine - preserve playback if active
        m_audioEngine.reloadBufferPreservingPlayback(
            buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

        // Update waveform display - clear selection since file length changed
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                          false, false); // preserveView=false, preserveEditCursor=false
        m_waveformDisplay.clearSelection();
        m_waveformDisplay.setEditCursor(0.0);

        // Log the operation
        DBG("Trimmed to selection");

        return true;
    }

    bool undo() override
    {
        // Restore the entire buffer (before trim)
        auto& buffer = m_bufferManager.getMutableBuffer();
        buffer.setSize(m_beforeBuffer.getNumChannels(), m_beforeBuffer.getNumSamples());
        buffer.makeCopyOf(m_beforeBuffer, true);

        // Reload buffer in AudioEngine - preserve playback if active
        m_audioEngine.reloadBufferPreservingPlayback(buffer, m_bufferManager.getSampleRate(),
                                                    buffer.getNumChannels());

        // Update waveform display - clear selection since file length changed
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                          false, false); // preserveView=false, preserveEditCursor=false

        return true;
    }

private:
    AudioBufferManager& m_bufferManager;
    WaveformDisplay& m_waveformDisplay;
    AudioEngine& m_audioEngine;
    juce::AudioBuffer<float> m_beforeBuffer;
    int m_startSample;
    int m_numSamples;
};
