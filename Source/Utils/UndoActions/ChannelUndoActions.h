/*
  ==============================================================================

    ChannelUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Channel-domain undo actions per CLAUDE.md §8.1:
      - ChannelConvertAction       (downmix/upmix entire buffer)
      - ConvertToStereoAction      (mono → stereo upmix)
      - SilenceChannelsUndoAction  (silence specific channels in a range)
      - ReplaceChannelsAction      (paste audio into specific channels)

    The latter three were originally housed in AudioUndoActions.h; they
    were moved here as part of the §7.5 file-size split since they are
    channel-shape operations rather than generic audio-buffer DSP.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../UndoableEdits.h"

//==============================================================================
/**
 * Undoable channel conversion (downmix/upmix) for the entire buffer.
 *
 * Stores the original buffer in full because the channel count changes;
 * partial-range storage would not be reversible. Replaces the document's
 * buffer with the already-converted buffer on perform().
 */
class ChannelConvertAction : public UndoableEditBase
{
public:
    ChannelConvertAction(AudioBufferManager& bufferManager,
                         AudioEngine& audioEngine,
                         WaveformDisplay& waveformDisplay,
                         const juce::AudioBuffer<float>& convertedBuffer)
        : UndoableEditBase(bufferManager, audioEngine, waveformDisplay),
          m_convertedBuffer(convertedBuffer.getNumChannels(), convertedBuffer.getNumSamples())
    {
        const auto& buffer = m_bufferManager.getBuffer();
        m_originalBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples());
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            m_originalBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());

        for (int ch = 0; ch < convertedBuffer.getNumChannels(); ++ch)
            m_convertedBuffer.copyFrom(ch, 0, convertedBuffer, ch, 0, convertedBuffer.getNumSamples());

        m_sampleRate = m_bufferManager.getSampleRate();
    }

    bool perform() override
    {
        m_bufferManager.setBuffer(m_convertedBuffer, m_sampleRate);
        updatePlaybackAndDisplay();
        return true;
    }

    bool undo() override
    {
        m_bufferManager.setBuffer(m_originalBuffer, m_sampleRate);
        updatePlaybackAndDisplay();
        return true;
    }

    int getSizeInUnits() override
    {
        const size_t originalSize = static_cast<size_t>(m_originalBuffer.getNumSamples()) *
                                    static_cast<size_t>(m_originalBuffer.getNumChannels()) * sizeof(float);
        const size_t convertedSize = static_cast<size_t>(m_convertedBuffer.getNumSamples()) *
                                     static_cast<size_t>(m_convertedBuffer.getNumChannels()) * sizeof(float);
        return static_cast<int>(originalSize + convertedSize);
    }

private:
    juce::AudioBuffer<float> m_originalBuffer;
    juce::AudioBuffer<float> m_convertedBuffer;
    double m_sampleRate;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelConvertAction)
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
