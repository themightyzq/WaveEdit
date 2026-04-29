/*
  ==============================================================================

    ChannelUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Channel-domain undo actions per CLAUDE.md §8.1:
      - ChannelConvertAction (downmix/upmix entire buffer)

    Note: ConvertToStereoAction and ReplaceChannelsAction live in
    AudioUndoActions.h alongside other audio-buffer operations.

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
