/*
  ==============================================================================

    ChannelFocusUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Per-channel focus for the length-preserving DSP actions (gain,
    normalize, fades, DC offset, silence, reverse, invert, EQ, plugin
    apply, generate over a selection). Those actions process every
    channel; ChannelMaskedAction wraps one and puts the unfocused channels
    of the range back after it runs, so only the focused channels change --
    the same contract Cut/Copy/Delete already honour.

  ==============================================================================
*/

#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <vector>
#include "../../Audio/AudioBufferManager.h"
#include "../../Audio/AudioEngine.h"
#include "../../Audio/ChannelMask.h"
#include "../../UI/WaveformDisplay.h"
#include "UndoMemoryBudget.h"

/**
 * Runs a length-preserving buffer action, then restores the channels
 * outside the focus mask over [startSample, startSample + range length).
 *
 * The unfocused audio is taken from @p beforeRange, the range as it was
 * BEFORE the DSP ran (callers pass the same snapshot the inner action
 * keeps for undo). That also covers the progress-dialog paths, where the
 * DSP was applied before the action was registered (inner action marked
 * already-performed). Undo is the inner action's undo, which restores
 * every channel of the range.
 */
class ChannelMaskedAction : public juce::UndoableAction
{
public:
    ChannelMaskedAction(juce::UndoableAction* innerToOwn,
                        AudioBufferManager& bufferManager,
                        AudioEngine& audioEngine,
                        WaveformDisplay& waveformDisplay,
                        const juce::AudioBuffer<float>& beforeRange,
                        int64_t startSample,
                        int channelMask)
        : m_inner(innerToOwn),
          m_bufferManager(bufferManager),
          m_audioEngine(audioEngine),
          m_waveformDisplay(waveformDisplay),
          m_startSample(startSample)
    {
        for (int ch = 0; ch < beforeRange.getNumChannels(); ++ch)
            if (!ChannelMask::includes(channelMask, ch))
                m_keptChannels.push_back(ch);

        m_keptAudio.setSize(static_cast<int>(m_keptChannels.size()), beforeRange.getNumSamples());
        for (size_t i = 0; i < m_keptChannels.size(); ++i)
            m_keptAudio.copyFrom(static_cast<int>(i), 0, beforeRange, m_keptChannels[i],
                                 0, beforeRange.getNumSamples());

        m_sizeInUnits = (m_inner != nullptr ? m_inner->getSizeInUnits() : 0)
                        + UndoMemory::unitsForBuffer(m_keptAudio);
    }

    bool perform() override
    {
        if (m_inner == nullptr || !m_inner->perform())
            return false;

        auto& buffer = m_bufferManager.getMutableBuffer();
        const int numSamples = m_keptAudio.getNumSamples();

        // Length-preserving actions only: if the range no longer fits, leave
        // the inner result alone rather than write out of range.
        jassert(m_startSample >= 0 && m_startSample + numSamples <= buffer.getNumSamples());
        if (m_startSample < 0 || m_startSample + numSamples > buffer.getNumSamples())
            return true;

        for (size_t i = 0; i < m_keptChannels.size(); ++i)
            if (m_keptChannels[i] < buffer.getNumChannels())
                buffer.copyFrom(m_keptChannels[i], static_cast<int>(m_startSample),
                                m_keptAudio, static_cast<int>(i), 0, numSamples);

        m_audioEngine.reloadBufferPreservingPlayback(buffer, m_bufferManager.getSampleRate(),
                                                     buffer.getNumChannels());
        m_waveformDisplay.reloadFromBuffer(buffer, m_bufferManager.getSampleRate(), true, true);
        return true;
    }

    bool undo() override
    {
        return m_inner != nullptr && m_inner->undo();
    }

    int getSizeInUnits() override { return m_sizeInUnits; }

private:
    std::unique_ptr<juce::UndoableAction> m_inner;
    AudioBufferManager& m_bufferManager;
    AudioEngine& m_audioEngine;
    WaveformDisplay& m_waveformDisplay;
    int64_t m_startSample;
    std::vector<int> m_keptChannels;      // unfocused channel indices
    juce::AudioBuffer<float> m_keptAudio; // their pre-DSP audio, one row each
    int m_sizeInUnits = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelMaskedAction)
};
