/*
  ==============================================================================

    TransformUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Whole-buffer transform undo actions split out of AudioUndoActions.h
    per CLAUDE.md §7.5: reverse, polarity invert, resample, head/tail.

    Reach this header through the umbrella `AudioUndoActions.h`.

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

//==============================================================================
/**
 * Undo action for SoundTouch-backed time-stretch / pitch-shift.
 * Stores complete before/after buffers because tempo changes the
 * sample count.
 */
class TimePitchUndoAction : public juce::UndoableAction
{
public:
    TimePitchUndoAction(AudioBufferManager& bufferManager,
                        WaveformDisplay& waveform,
                        AudioEngine& audioEngine,
                        const juce::AudioBuffer<float>& beforeBuffer,
                        const juce::AudioBuffer<float>& afterBuffer,
                        double sampleRate,
                        const juce::String& description)
        : m_bufferManager(bufferManager),
          m_waveformDisplay(waveform),
          m_audioEngine(audioEngine),
          m_sampleRate(sampleRate),
          m_description(description)
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
        DBG("TimePitchUndoAction::perform - " + m_description);
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
    juce::String m_description;
};
