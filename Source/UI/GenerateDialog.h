/*
  ==============================================================================

    GenerateDialog.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../DSP/AudioGenerator.h"

class AudioEngine;
class WaveformDisplay;

/**
 * Generate Tone / Generate Noise dialog (Generate menu). One dialog serves
 * both modes to avoid duplicating the Preview lifecycle. Uses the simple
 * (non-§6.8) dialog pattern plus an audition Preview that loops the generated
 * signal through the engine's OFFLINE_BUFFER path without touching the document.
 */
class GenerateDialog : public juce::Component
{
public:
    enum class Mode { Tone, Noise };

    struct Result
    {
        Mode mode = Mode::Tone;
        AudioGenerator::Waveform waveform = AudioGenerator::Waveform::Sine;
        AudioGenerator::NoiseType noiseType = AudioGenerator::NoiseType::White;
        double frequencyHz = 1000.0;
        float amplitudeDb = -12.0f;
        double durationSeconds = 1.0;
    };

    GenerateDialog(Mode mode,
                   AudioEngine* engine,
                   juce::Component::SafePointer<WaveformDisplay> lifeline,
                   bool hasSelection,
                   double selectionSeconds,
                   double sampleRate,
                   int numChannels);
    ~GenerateDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Shows the dialog; the callback receives the chosen parameters. */
    static void showDialog(juce::Component* parent,
                           Mode mode,
                           AudioEngine* engine,
                           juce::Component::SafePointer<WaveformDisplay> lifeline,
                           bool hasSelection,
                           double selectionSeconds,
                           double sampleRate,
                           int numChannels,
                           std::function<void(const Result&)> callback);

private:
    Result readControls() const;
    double effectiveDurationSeconds() const;
    void fillBuffer(juce::AudioBuffer<float>& buffer) const;

    /** Enables Generate/Preview only when the duration is usable. */
    void updateValidity();
    bool isDurationValid() const;

    bool engineUsable() const noexcept;
    void startPreview();
    void stopPreview();
    void restartPreviewIfActive();

    void confirmDialog();

    const Mode m_mode;
    AudioEngine* m_engine = nullptr;
    juce::Component::SafePointer<WaveformDisplay> m_lifeline;
    const bool m_hasSelection;
    const double m_selectionSeconds;
    const double m_sampleRate;
    const int m_numChannels;
    bool m_previewActive = false;

    std::function<void(const Result&)> m_callback;

    juce::Label m_titleLabel;
    juce::Label m_typeLabel;
    juce::ComboBox m_typeCombo;
    juce::Label m_freqLabel;
    juce::Slider m_freqSlider;
    juce::Label m_ampLabel;
    juce::Slider m_ampSlider;
    juce::Label m_durationLabel;
    juce::TextEditor m_durationEditor;
    juce::Label m_noteLabel;
    juce::Label m_validationLabel;
    juce::TextButton m_previewButton;
    juce::TextButton m_generateButton;
    juce::TextButton m_cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GenerateDialog)
};
