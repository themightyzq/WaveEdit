/*
  ==============================================================================

    FadeUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Fade-domain undo actions split out of AudioUndoActions.h to keep that
    file under the CLAUDE.md §7.5 500-line cap. Each action stores the
    before-state of the affected range and replays the fade curve on
    redo.

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

        juce::AudioBuffer<float> regionBuffer;
        regionBuffer.setSize(buffer.getNumChannels(), m_numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            regionBuffer.copyFrom(ch, 0, buffer, ch, m_startSample, m_numSamples);

        AudioProcessor::fadeIn(regionBuffer, m_numSamples, m_curveType);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.copyFrom(ch, m_startSample, regionBuffer, ch, 0, m_numSamples);

        m_audioEngine.reloadBufferPreservingPlayback(
            buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                           true, true);
        return true;
    }

    bool undo() override
    {
        auto& buffer = m_bufferManager.getMutableBuffer();

        for (int ch = 0; ch < m_beforeBuffer.getNumChannels(); ++ch)
            buffer.copyFrom(ch, m_startSample, m_beforeBuffer, ch, 0, m_numSamples);

        m_audioEngine.reloadBufferPreservingPlayback(buffer, m_bufferManager.getSampleRate(),
                                                     buffer.getNumChannels());
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                           true, true);
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

        juce::AudioBuffer<float> regionBuffer;
        regionBuffer.setSize(buffer.getNumChannels(), m_numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            regionBuffer.copyFrom(ch, 0, buffer, ch, m_startSample, m_numSamples);

        AudioProcessor::fadeOut(regionBuffer, m_numSamples, m_curveType);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.copyFrom(ch, m_startSample, regionBuffer, ch, 0, m_numSamples);

        m_audioEngine.reloadBufferPreservingPlayback(
            buffer, m_bufferManager.getSampleRate(), buffer.getNumChannels());
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                           true, true);
        return true;
    }

    bool undo() override
    {
        auto& buffer = m_bufferManager.getMutableBuffer();

        for (int ch = 0; ch < m_beforeBuffer.getNumChannels(); ++ch)
            buffer.copyFrom(ch, m_startSample, m_beforeBuffer, ch, 0, m_numSamples);

        m_audioEngine.reloadBufferPreservingPlayback(buffer, m_bufferManager.getSampleRate(),
                                                     buffer.getNumChannels());
        m_waveformDisplay.reloadFromBuffer(buffer, m_audioEngine.getSampleRate(),
                                           true, true);
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
