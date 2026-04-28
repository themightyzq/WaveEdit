/*
  ==============================================================================

    AudioUndoActions.h
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
#include "../../Audio/AudioBufferManager.h"
#include "../../Audio/AudioEngine.h"
#include "../../Audio/AudioFileManager.h"
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

        // Check playback state before applying gain
        bool wasPlaying = m_audioEngine.isPlaying();
        double positionBeforeEdit = m_audioEngine.getCurrentPosition();

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
 * Undo action for converting mono to stereo.
 * Stores the original mono buffer for undo.
 */
class ConvertToStereoAction : public juce::UndoableAction
{
public:
    ConvertToStereoAction(AudioBufferManager& bufferManager,
                          WaveformDisplay& waveform,
                          AudioEngine& audioEngine)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine)
    {
        // Store the original mono buffer for undo
        const auto& originalBuffer = m_bufferManager.getBuffer();
        m_originalMonoBuffer.setSize(originalBuffer.getNumChannels(),
                                     originalBuffer.getNumSamples());
        m_originalMonoBuffer.makeCopyOf(originalBuffer, true);
    }

    void markAsAlreadyPerformed() { m_alreadyPerformed = true; }

    bool perform() override
    {
        if (m_alreadyPerformed)
        {
            m_alreadyPerformed = false;
            return true;
        }

        // Convert mono to stereo
        if (!m_bufferManager.convertToStereo())
        {
            return false;
        }

        // Reload audio engine with new channel count
        m_audioEngine.reloadBufferPreservingPlayback(
            m_bufferManager.getBuffer(),
            m_bufferManager.getSampleRate(),
            m_bufferManager.getBuffer().getNumChannels()
        );

        // Reload waveform display
        m_waveformDisplay.reloadFromBuffer(
            m_bufferManager.getBuffer(),
            m_bufferManager.getSampleRate(),
            true, true
        );

        return true;
    }

    bool undo() override
    {
        // Restore the original mono buffer
        auto& buffer = m_bufferManager.getMutableBuffer();
        buffer.setSize(m_originalMonoBuffer.getNumChannels(),
                      m_originalMonoBuffer.getNumSamples());
        buffer.makeCopyOf(m_originalMonoBuffer, true);

        // Reload audio engine with mono
        m_audioEngine.reloadBufferPreservingPlayback(
            buffer,
            m_bufferManager.getSampleRate(),
            buffer.getNumChannels()
        );

        // Reload waveform display
        m_waveformDisplay.reloadFromBuffer(
            buffer,
            m_bufferManager.getSampleRate(),
            true, true
        );

        return true;
    }

    int getSizeInUnits() override { return 100; }

private:
    AudioBufferManager& m_bufferManager;
    WaveformDisplay& m_waveformDisplay;
    AudioEngine& m_audioEngine;
    juce::AudioBuffer<float> m_originalMonoBuffer;
    bool m_alreadyPerformed = false;
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
 * Undo action for fade in.
 * Stores the before state and region information.
 */
class FadeInUndoAction : public juce::UndoableAction
{
public:
    FadeInUndoAction(AudioBufferManager& bufferManager,
                    WaveformDisplay& waveform,
                    AudioEngine& audioEngine,
                    const juce::AudioBuffer<float>& beforeBuffer,
                    int startSample,
                    int numSamples,
                    FadeCurveType curveType = FadeCurveType::LINEAR)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_beforeBuffer(),
          m_startSample(startSample),
          m_numSamples(numSamples),
          m_curveType(curveType)
    {
        // Store only the affected region to save memory
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

        // Get the buffer and create a region buffer for fade in
        auto& buffer = m_bufferManager.getMutableBuffer();

        // Extract the region to fade
        juce::AudioBuffer<float> regionBuffer;
        regionBuffer.setSize(buffer.getNumChannels(), m_numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            regionBuffer.copyFrom(ch, 0, buffer, ch, m_startSample, m_numSamples);
        }

        // Apply fade in to the region with selected curve type
        AudioProcessor::fadeIn(regionBuffer, m_numSamples, m_curveType);

        // Copy faded region back to main buffer
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
        juce::String message = "Applied fade in to selection";
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
    FadeCurveType m_curveType;
    bool m_alreadyPerformed = false;
};

//==============================================================================
/**
 * Undo action for fade out.
 * Stores the before state and region information.
 */
class FadeOutUndoAction : public juce::UndoableAction
{
public:
    FadeOutUndoAction(AudioBufferManager& bufferManager,
                     WaveformDisplay& waveform,
                     AudioEngine& audioEngine,
                     const juce::AudioBuffer<float>& beforeBuffer,
                     int startSample,
                     int numSamples,
                     FadeCurveType curveType = FadeCurveType::LINEAR)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_beforeBuffer(),
          m_startSample(startSample),
          m_numSamples(numSamples),
          m_curveType(curveType)
    {
        // Store only the affected region to save memory
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

        // Get the buffer and create a region buffer for fade out
        auto& buffer = m_bufferManager.getMutableBuffer();

        // Extract the region to fade
        juce::AudioBuffer<float> regionBuffer;
        regionBuffer.setSize(buffer.getNumChannels(), m_numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            regionBuffer.copyFrom(ch, 0, buffer, ch, m_startSample, m_numSamples);
        }

        // Apply fade out to the region with selected curve type
        AudioProcessor::fadeOut(regionBuffer, m_numSamples, m_curveType);

        // Copy faded region back to main buffer
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
        juce::String message = "Applied fade out to selection";
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
    FadeCurveType m_curveType;
    bool m_alreadyPerformed = false;
};

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
 * Undo action for silencing specific channels.
 * Used for per-channel delete operations.
 * Stores the original audio data only for the affected channels.
 */
class SilenceChannelsUndoAction : public juce::UndoableAction
{
public:
    SilenceChannelsUndoAction(AudioBufferManager& bufferManager,
                              WaveformDisplay& waveform,
                              AudioEngine& audioEngine,
                              const juce::AudioBuffer<float>& beforeBuffer,
                              int startSample,
                              int numSamples,
                              int channelMask)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_beforeBuffer(),
          m_startSample(startSample),
          m_numSamples(numSamples),
          m_channelMask(channelMask)
    {
        // Store only the affected region for the affected channels
        m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
        m_beforeBuffer.makeCopyOf(beforeBuffer, true);
    }

    bool perform() override
    {
        // Silence only the specified channels
        bool success = m_bufferManager.silenceRangeForChannels(m_startSample, m_numSamples, m_channelMask);

        if (!success)
        {
            DBG("SilenceChannelsUndoAction::perform - Failed to silence channels");
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
        int channelCount = 0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            if ((m_channelMask & (1 << ch)) != 0) channelCount++;

        DBG(juce::String::formatted(
            "Applied silence to %d channel(s) in selection", channelCount));

        return true;
    }

    bool undo() override
    {
        // Restore the before state for the affected channels
        auto& buffer = m_bufferManager.getMutableBuffer();

        // Map the stored channels back to their original positions
        int storedCh = 0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            if ((m_channelMask & (1 << ch)) != 0 && storedCh < m_beforeBuffer.getNumChannels())
            {
                buffer.copyFrom(ch, m_startSample, m_beforeBuffer, storedCh, 0, m_numSamples);
                storedCh++;
            }
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
    int m_channelMask;
};

//==============================================================================
/**
 * Undo action for replacing/pasting audio to specific channels.
 * Used for per-channel paste operations.
 * Does not change buffer length - replaces data in-place.
 */
class ReplaceChannelsAction : public juce::UndoableAction
{
public:
    ReplaceChannelsAction(AudioBufferManager& bufferManager,
                          WaveformDisplay& waveform,
                          AudioEngine& audioEngine,
                          int startSample,
                          const juce::AudioBuffer<float>& beforeBuffer,
                          const juce::AudioBuffer<float>& newAudio,
                          int channelMask)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_startSample(startSample),
          m_beforeBuffer(),
          m_newAudio(),
          m_channelMask(channelMask)
    {
        // Store the before state for undo
        m_beforeBuffer.setSize(beforeBuffer.getNumChannels(), beforeBuffer.getNumSamples());
        m_beforeBuffer.makeCopyOf(beforeBuffer, true);

        // Store the new audio for redo
        m_newAudio.setSize(newAudio.getNumChannels(), newAudio.getNumSamples());
        m_newAudio.makeCopyOf(newAudio, true);
    }

    bool perform() override
    {
        // Replace the specified channels with new audio
        bool success = m_bufferManager.replaceChannelsInRange(m_startSample, m_newAudio, m_channelMask);

        if (!success)
        {
            DBG("ReplaceChannelsAction::perform - Failed to replace channels");
            return false;
        }

        // Get the updated buffer
        auto& buffer = m_bufferManager.getMutableBuffer();

        // Reload buffer in AudioEngine - preserve playback if active
        m_audioEngine.reloadBufferPreservingPlayback(
            buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

        // Update waveform display - preserve view
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                          true, true);

        return true;
    }

    bool undo() override
    {
        // Restore the before state
        bool success = m_bufferManager.replaceChannelsInRange(m_startSample, m_beforeBuffer, m_channelMask);

        if (!success)
        {
            DBG("ReplaceChannelsAction::undo - Failed to restore channels");
            return false;
        }

        auto& buffer = m_bufferManager.getMutableBuffer();

        // Reload buffer in AudioEngine
        m_audioEngine.reloadBufferPreservingPlayback(buffer, m_bufferManager.getSampleRate(),
                                                    buffer.getNumChannels());

        // Update waveform display
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                          true, true);

        return true;
    }

private:
    AudioBufferManager& m_bufferManager;
    WaveformDisplay& m_waveformDisplay;
    AudioEngine& m_audioEngine;
    int m_startSample;
    juce::AudioBuffer<float> m_beforeBuffer;
    juce::AudioBuffer<float> m_newAudio;
    int m_channelMask;
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

//==============================================================================
/**
 * Undo action for reversing audio.
 * Reverse is its own inverse, so perform() and undo() do the same thing.
 */
class ReverseUndoAction : public juce::UndoableAction
{
public:
    ReverseUndoAction(AudioBufferManager& bufferManager,
                      WaveformDisplay& waveform,
                      AudioEngine& audioEngine,
                      int startSample, int numSamples, bool isSelection)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_startSample(startSample),
          m_numSamples(numSamples),
          m_isSelection(isSelection)
    {
    }

    bool perform() override
    {
        auto& buffer = m_bufferManager.getMutableBuffer();
        AudioProcessor::reverseRange(buffer, m_startSample, m_numSamples);

        // Reload buffer in AudioEngine - preserve playback if active
        m_audioEngine.reloadBufferPreservingPlayback(
            buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

        // Update waveform display - preserve view and selection
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                          true, true);

        juce::String region = m_isSelection ? "selection" : "entire file";
        DBG("Reversed " + region);

        return true;
    }

    bool undo() override { return perform(); }  // Reverse is self-inverse

private:
    AudioBufferManager& m_bufferManager;
    WaveformDisplay& m_waveformDisplay;
    AudioEngine& m_audioEngine;
    int m_startSample;
    int m_numSamples;
    bool m_isSelection;
};

//==============================================================================
/**
 * Undo action for inverting polarity.
 * Invert is its own inverse, so perform() and undo() do the same thing.
 */
class InvertUndoAction : public juce::UndoableAction
{
public:
    InvertUndoAction(AudioBufferManager& bufferManager,
                     WaveformDisplay& waveform,
                     AudioEngine& audioEngine,
                     int startSample, int numSamples, bool isSelection)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_startSample(startSample),
          m_numSamples(numSamples),
          m_isSelection(isSelection)
    {
    }

    bool perform() override
    {
        auto& buffer = m_bufferManager.getMutableBuffer();
        AudioProcessor::invertRange(buffer, m_startSample, m_numSamples);

        // Reload buffer in AudioEngine - preserve playback if active
        m_audioEngine.reloadBufferPreservingPlayback(
            buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());

        // Update waveform display - preserve view and selection
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                          true, true);

        juce::String region = m_isSelection ? "selection" : "entire file";
        DBG("Inverted polarity of " + region);

        return true;
    }

    bool undo() override { return perform(); }  // Invert is self-inverse

private:
    AudioBufferManager& m_bufferManager;
    WaveformDisplay& m_waveformDisplay;
    AudioEngine& m_audioEngine;
    int m_startSample;
    int m_numSamples;
    bool m_isSelection;
};

//==============================================================================
/**
 * Undo action for resampling audio.
 * Stores the original buffer for undo since resampling changes buffer length.
 */
class ResampleUndoAction : public juce::UndoableAction
{
public:
    ResampleUndoAction(AudioBufferManager& bufferManager,
                       WaveformDisplay& waveform,
                       AudioEngine& audioEngine,
                       const juce::AudioBuffer<float>& beforeBuffer,
                       double oldSampleRate, double newSampleRate)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_oldSampleRate(oldSampleRate),
          m_newSampleRate(newSampleRate)
    {
        m_beforeBuffer.makeCopyOf(beforeBuffer, true);
    }

    bool perform() override
    {
        auto resampled = AudioFileManager::resampleBuffer(
            m_beforeBuffer, m_oldSampleRate, m_newSampleRate);

        // Use setBuffer() which updates both the buffer and sample rate
        m_bufferManager.setBuffer(resampled, m_newSampleRate);

        auto& buffer = m_bufferManager.getMutableBuffer();
        m_audioEngine.reloadBufferPreservingPlayback(
            buffer, m_newSampleRate, buffer.getNumChannels());
        m_waveformDisplay.reloadFromBuffer(buffer, m_newSampleRate, false, false);

        DBG(juce::String::formatted(
            "Resampled from %.0f Hz to %.0f Hz", m_oldSampleRate, m_newSampleRate));

        return true;
    }

    bool undo() override
    {
        // Restore original buffer and sample rate
        m_bufferManager.setBuffer(m_beforeBuffer, m_oldSampleRate);

        auto& buffer = m_bufferManager.getMutableBuffer();
        m_audioEngine.reloadBufferPreservingPlayback(
            buffer, m_oldSampleRate, buffer.getNumChannels());
        m_waveformDisplay.reloadFromBuffer(buffer, m_oldSampleRate, false, false);

        DBG(juce::String::formatted(
            "Undo resample: restored to %.0f Hz", m_oldSampleRate));

        return true;
    }

    int getSizeInUnits() override { return 100; }

private:
    AudioBufferManager& m_bufferManager;
    WaveformDisplay& m_waveformDisplay;
    AudioEngine& m_audioEngine;
    juce::AudioBuffer<float> m_beforeBuffer;
    double m_oldSampleRate;
    double m_newSampleRate;
};

//==============================================================================
/**
 * Undo action for Head & Tail processing.
 * Stores complete before/after buffers since the operation can change buffer length.
 */
class HeadTailUndoAction : public juce::UndoableAction
{
public:
    HeadTailUndoAction(AudioBufferManager& bufferManager,
                       WaveformDisplay& waveform,
                       AudioEngine& audioEngine,
                       const juce::AudioBuffer<float>& beforeBuffer,
                       const juce::AudioBuffer<float>& afterBuffer,
                       double sampleRate)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_sampleRate(sampleRate)
    {
        m_beforeBuffer.makeCopyOf(beforeBuffer, true);
        m_afterBuffer.makeCopyOf(afterBuffer, true);
    }

    bool perform() override
    {
        auto& buffer = m_bufferManager.getMutableBuffer();
        buffer.setSize(m_afterBuffer.getNumChannels(), m_afterBuffer.getNumSamples());
        buffer.makeCopyOf(m_afterBuffer, true);
        m_audioEngine.reloadBufferPreservingPlayback(buffer, m_sampleRate,
                                                      buffer.getNumChannels());
        m_waveformDisplay.reloadFromBuffer(buffer, m_sampleRate, false, false);
        return true;
    }

    bool undo() override
    {
        auto& buffer = m_bufferManager.getMutableBuffer();
        buffer.setSize(m_beforeBuffer.getNumChannels(), m_beforeBuffer.getNumSamples());
        buffer.makeCopyOf(m_beforeBuffer, true);
        m_audioEngine.reloadBufferPreservingPlayback(buffer, m_sampleRate,
                                                      buffer.getNumChannels());
        m_waveformDisplay.reloadFromBuffer(buffer, m_sampleRate, false, false);
        return true;
    }

    int getSizeInUnits() override { return 100; }

private:
    AudioBufferManager& m_bufferManager;
    WaveformDisplay& m_waveformDisplay;
    AudioEngine& m_audioEngine;
    juce::AudioBuffer<float> m_beforeBuffer;
    juce::AudioBuffer<float> m_afterBuffer;
    double m_sampleRate;
};
