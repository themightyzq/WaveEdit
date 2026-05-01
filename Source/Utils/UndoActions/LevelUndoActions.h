/*
  ==============================================================================

    LevelUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Level-domain audio undo actions split out of AudioUndoActions.h per
    CLAUDE.md §7.5: gain adjustment, normalization, DC offset removal.

    Reach this header through the umbrella `AudioUndoActions.h`; direct
    inclusion is fine but adds nothing the umbrella doesn't already
    provide.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../../Audio/AudioBufferManager.h"
#include "../../Audio/AudioEngine.h"
#include "../../Audio/AudioProcessor.h"
#include "../../UI/WaveformDisplay.h"

//==============================================================================
/**
 * Undo action for gain adjustment.
 * Stores the before state and region information for the affected samples.
 */
class GainUndoAction : public juce::UndoableAction
{
public:
    GainUndoAction(AudioBufferManager& bufferManager,
                  WaveformDisplay& waveform,
                  AudioEngine& audioEngine,
                  const juce::AudioBuffer<float>& beforeBuffer,
                  int startSample,
                  int numSamples,
                  float gainDB,
                  bool isSelection)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_beforeBuffer(),
          m_startSample(startSample),
          m_numSamples(numSamples),
          m_gainDB(gainDB),
          m_isSelection(isSelection)
    {
        // Store only the affected region to save memory
        m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
        m_beforeBuffer.makeCopyOf(beforeBuffer, true);
    }

    /**
     * Mark this action as already performed.
     * Used when the DSP operation was done by a progress-enabled method
     * before registering with the undo system.
     */
    void markAsAlreadyPerformed() { m_alreadyPerformed = true; }

    bool perform() override
    {
        // If already performed (by progress dialog), skip the DSP operation
        if (m_alreadyPerformed)
        {
            m_alreadyPerformed = false; // Reset for redo
            return true;
        }

        // Check playback state before applying gain (debug logging only;
        // reloadBufferPreservingPlayback handles the real preservation).
        [[maybe_unused]] const bool wasPlaying = m_audioEngine.isPlaying();
        [[maybe_unused]] const double positionBeforeEdit = m_audioEngine.getCurrentPosition();

        DBG(juce::String::formatted(
            "GainUndoAction::perform - Before edit: playing=%s, position=%.3f",
            wasPlaying ? "YES" : "NO", positionBeforeEdit));

        // Apply gain to the current buffer
        auto& buffer = m_bufferManager.getMutableBuffer();
        AudioProcessor::applyGainToRange(buffer, m_gainDB, m_startSample, m_numSamples);

        // Reload buffer in AudioEngine - preserve playback if active
        bool reloadSuccess = m_audioEngine.reloadBufferPreservingPlayback(
            buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

        DBG(juce::String::formatted(
            "GainUndoAction::perform - After reload: success=%s, playing=%s, position=%.3f",
            reloadSuccess ? "YES" : "NO",
            m_audioEngine.isPlaying() ? "YES" : "NO",
            m_audioEngine.getCurrentPosition()));

        // Update waveform display - preserve view and selection
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                          true, true); // preserveView=true, preserveEditCursor=true

        // Log the operation
        juce::String region = m_isSelection ? "selection" : "entire file";
        juce::String message = juce::String::formatted("Applied %+.1f dB gain to %s",
                                                        m_gainDB, region.toRawUTF8());
        DBG(message);

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
    float m_gainDB;
    bool m_isSelection;
    bool m_alreadyPerformed = false;  // For progress dialog integration
};

//==============================================================================
/**
 * Undo action for normalization.
 * Stores the before state and region information.
 */
class NormalizeUndoAction : public juce::UndoableAction
{
public:
    NormalizeUndoAction(AudioBufferManager& bufferManager,
                       WaveformDisplay& waveform,
                       AudioEngine& audioEngine,
                       const juce::AudioBuffer<float>& beforeBuffer,
                       int startSample,
                       int numSamples,
                       bool isSelection,
                       float targetDB = 0.0f)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_beforeBuffer(),
          m_startSample(startSample),
          m_numSamples(numSamples),
          m_isSelection(isSelection),
          m_targetDB(targetDB)
    {
        // Store only the affected region to save memory
        m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
        m_beforeBuffer.makeCopyOf(beforeBuffer, true);
    }

    bool perform() override
    {
        // Get the buffer and create a region buffer for normalization
        auto& buffer = m_bufferManager.getMutableBuffer();

        // Extract the region to normalize
        juce::AudioBuffer<float> regionBuffer;
        regionBuffer.setSize(buffer.getNumChannels(), m_numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            regionBuffer.copyFrom(ch, 0, buffer, ch, m_startSample, m_numSamples);
        }

        // Apply normalization to the region
        AudioProcessor::normalize(regionBuffer, m_targetDB);

        // Copy normalized region back to main buffer
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.copyFrom(ch, m_startSample, regionBuffer, ch, 0, m_numSamples);
        }

        // Reload buffer in AudioEngine - preserve playback if active
        m_audioEngine.reloadBufferPreservingPlayback(
            buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

        // Update waveform display - preserve view and selection
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                          true, true); // preserveView=true, preserveEditCursor=true

        // Log the operation
        juce::String region = m_isSelection ? "selection" : "entire file";
        juce::String message = juce::String::formatted("Normalized %s to 0dB peak",
                                                        region.toRawUTF8());
        DBG(message);

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
    bool m_isSelection;
    float m_targetDB;
};

//==============================================================================
/**
 * Undo action for DC offset removal.
 * Supports both selection-based and entire-file processing.
 */
class DCOffsetRemovalUndoAction : public juce::UndoableAction
{
public:
    DCOffsetRemovalUndoAction(AudioBufferManager& bufferManager,
                             WaveformDisplay& waveform,
                             AudioEngine& audioEngine,
                             const juce::AudioBuffer<float>& beforeBuffer,
                             int startSample = 0,
                             int numSamples = -1)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_beforeBuffer(),
          m_startSample(startSample),
          m_numSamples(numSamples)
    {
        // Store the affected region
        m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
        m_beforeBuffer.makeCopyOf(beforeBuffer, true);
    }

    void markAsAlreadyPerformed() { m_alreadyPerformed = true; }

    bool perform() override
    {
        if (m_alreadyPerformed)
        {
            m_alreadyPerformed = false;
            return true;
        }

        auto& buffer = m_bufferManager.getMutableBuffer();

        // Extract the region to process
        juce::AudioBuffer<float> regionBuffer;
        int actualNumSamples = (m_numSamples < 0) ? buffer.getNumSamples() : m_numSamples;
        regionBuffer.setSize(buffer.getNumChannels(), actualNumSamples);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            regionBuffer.copyFrom(ch, 0, buffer, ch, m_startSample, actualNumSamples);
        }

        // Apply DC offset removal to the region
        bool success = AudioProcessor::removeDCOffset(regionBuffer);

        if (!success)
        {
            DBG("DCOffsetRemovalUndoAction::perform - Failed to remove DC offset");
            return false;
        }

        // Copy the processed region back into the main buffer
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.copyFrom(ch, m_startSample, regionBuffer, ch, 0, actualNumSamples);
        }

        // Reload buffer in AudioEngine - preserve playback if active
        m_audioEngine.reloadBufferPreservingPlayback(
            buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

        // Update waveform display - preserve view and selection
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                          true, true); // preserveView=true, preserveEditCursor=true

        // Log the operation
        juce::String message = (m_numSamples < 0) ? "Removed DC offset from entire file"
                                                   : "Removed DC offset from selection";
        DBG(message);

        return true;
    }

    bool undo() override
    {
        // Restore the affected region from before buffer
        auto& buffer = m_bufferManager.getMutableBuffer();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.copyFrom(ch, m_startSample, m_beforeBuffer, ch, 0, m_beforeBuffer.getNumSamples());
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
    bool m_alreadyPerformed = false;
};
