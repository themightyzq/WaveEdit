/*
  ==============================================================================

    PluginUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Plugin/EQ-domain undo actions per CLAUDE.md §8.1:
      - ApplyParametricEQAction
      - ApplyDynamicParametricEQAction
      - ApplyPluginChainAction

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../UndoableEdits.h"
#include "../../DSP/ParametricEQ.h"
#include "../../DSP/DynamicParametricEQ.h"

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
        jassert(m_bufferManager.hasAudioData());
        jassert(startSample >= 0 && startSample < m_bufferManager.getNumSamples());
        jassert(numSamples > 0 && (startSample + numSamples) <= m_bufferManager.getNumSamples());

        m_originalAudio = m_bufferManager.getAudioRange(startSample, numSamples);
        m_sampleRate = m_bufferManager.getSampleRate();
    }

    bool perform() override
    {
        auto audioToProcess = m_bufferManager.getAudioRange(m_startSample, m_numSamples);

        ParametricEQ eq;
        eq.prepare(m_sampleRate, audioToProcess.getNumSamples());
        eq.applyEQ(audioToProcess, m_eqParams);

        const bool success = m_bufferManager.replaceRange(m_startSample, m_numSamples, audioToProcess);
        if (success)
            updatePlaybackAndDisplay();
        return success;
    }

    bool undo() override
    {
        const bool success = m_bufferManager.replaceRange(m_startSample, m_numSamples, m_originalAudio);
        if (success)
            updatePlaybackAndDisplay();
        return success;
    }

    int getSizeInUnits() override
    {
        const size_t size = static_cast<size_t>(m_originalAudio.getNumSamples()) *
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
        jassert(m_bufferManager.hasAudioData());
        jassert(startSample >= 0 && startSample < m_bufferManager.getNumSamples());
        jassert(numSamples > 0 && (startSample + numSamples) <= m_bufferManager.getNumSamples());

        m_originalAudio = m_bufferManager.getAudioRange(startSample, numSamples);
        m_sampleRate = m_bufferManager.getSampleRate();
    }

    bool perform() override
    {
        auto audioToProcess = m_bufferManager.getAudioRange(m_startSample, m_numSamples);

        DynamicParametricEQ eq;
        eq.prepare(m_sampleRate, static_cast<int>(audioToProcess.getNumSamples()));
        eq.setParameters(m_eqParams);
        eq.applyEQ(audioToProcess);

        const bool success = m_bufferManager.replaceRange(m_startSample, m_numSamples, audioToProcess);
        if (success)
            updatePlaybackAndDisplay();
        return success;
    }

    bool undo() override
    {
        const bool success = m_bufferManager.replaceRange(m_startSample, m_numSamples, m_originalAudio);
        if (success)
            updatePlaybackAndDisplay();
        return success;
    }

    int getSizeInUnits() override
    {
        const size_t size = static_cast<size_t>(m_originalAudio.getNumSamples()) *
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
 * Receives the already-processed audio buffer and handles undo/redo by
 * storing original/processed audio and swapping them. The plugin chain
 * rendering is done asynchronously by PluginChainRenderer before the
 * action is registered with the UndoManager.
 *
 * Use markAsAlreadyPerformed() when the buffer was modified by the
 * background renderer before the action is registered, so the first
 * UndoManager::perform() does not re-apply.
 *
 * Thread safety: must be created and performed on the message thread.
 */
class ApplyPluginChainAction : public UndoableEditBase
{
public:
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
        jassert(m_bufferManager.hasAudioData());
        jassert(startSample >= 0 && startSample < m_bufferManager.getNumSamples());
        jassert(numSamples > 0 && (startSample + numSamples) <= m_bufferManager.getNumSamples());
        // processedAudio may exceed numSamples when an effect tail is included.
        jassert(processedAudio.getNumSamples() >= numSamples);

        m_originalAudio = m_bufferManager.getAudioRange(startSample, numSamples);
        for (int ch = 0; ch < processedAudio.getNumChannels(); ++ch)
            m_processedAudio.copyFrom(ch, 0, processedAudio, ch, 0, processedAudio.getNumSamples());

        m_sampleRate = m_bufferManager.getSampleRate();
    }

    void markAsAlreadyPerformed() { m_alreadyPerformed = true; }

    bool perform() override
    {
        if (m_alreadyPerformed)
        {
            m_alreadyPerformed = false;  // reset so redo re-applies
            return true;
        }

        const bool success = m_bufferManager.replaceRange(m_startSample, m_numSamples, m_processedAudio);
        if (success)
            updatePlaybackAndDisplay();
        return success;
    }

    bool undo() override
    {
        // When effect tail was included, m_processedAudio is larger than m_originalAudio.
        // Replace the extended range with the original range.
        const int64_t samplesToReplace = m_processedAudio.getNumSamples();
        const bool success = m_bufferManager.replaceRange(m_startSample, samplesToReplace, m_originalAudio);
        if (success)
            updatePlaybackAndDisplay();
        return success;
    }

    int getSizeInUnits() override
    {
        const size_t originalSize = static_cast<size_t>(m_originalAudio.getNumSamples()) *
                                    static_cast<size_t>(m_originalAudio.getNumChannels()) * sizeof(float);
        const size_t processedSize = static_cast<size_t>(m_processedAudio.getNumSamples()) *
                                     static_cast<size_t>(m_processedAudio.getNumChannels()) * sizeof(float);
        return static_cast<int>(originalSize + processedSize);
    }

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
    bool m_alreadyPerformed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ApplyPluginChainAction)
};
