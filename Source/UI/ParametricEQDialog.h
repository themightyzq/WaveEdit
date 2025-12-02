/*
  ==============================================================================

    ParametricEQDialog.h
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
#include <juce_gui_extra/juce_gui_extra.h>
#include "../DSP/ParametricEQ.h"
#include <optional>

// Forward declarations
class AudioEngine;
class AudioBufferManager;

/**
 * Modal dialog for configuring 3-band parametric EQ.
 *
 * Provides interactive controls for:
 * - Low Band (Shelf): Frequency, Gain, Q
 * - Mid Band (Peak): Frequency, Gain, Q
 * - High Band (Shelf): Frequency, Gain, Q
 *
 * Each band has:
 * - Frequency slider (20 Hz - 20 kHz, logarithmic scale)
 * - Gain slider (-20 dB to +20 dB)
 * - Q slider (0.1 to 10.0, resonance/bandwidth)
 *
 * Thread Safety: UI thread only. Must be shown from message thread.
 * The showDialog() method blocks until the user dismisses the dialog.
 */
class ParametricEQDialog : public juce::Component
{
public:
    /**
     * Constructor for preview-enabled dialog.
     *
     * @param audioEngine Audio engine for preview playback (optional - can be nullptr)
     * @param bufferManager Buffer manager for audio data (optional - can be nullptr)
     * @param selectionStart Start sample position of selection
     * @param selectionEnd End sample position of selection
     */
    ParametricEQDialog(AudioEngine* audioEngine = nullptr,
                       AudioBufferManager* bufferManager = nullptr,
                       int64_t selectionStart = 0,
                       int64_t selectionEnd = 0);
    ~ParametricEQDialog() override;

    /**
     * Show the dialog modally and return the user's EQ parameters.
     *
     * @param audioEngine Audio engine for preview playback (optional)
     * @param bufferManager Buffer manager for audio data (optional)
     * @param selectionStart Start sample position of selection
     * @param selectionEnd End sample position of selection
     * @param currentParams Current EQ parameters (for initialization)
     * @return std::optional<ParametricEQ::Parameters> containing EQ params if user clicked Apply,
     *         std::nullopt if user clicked Cancel or closed the dialog
     */
    static std::optional<ParametricEQ::Parameters> showDialog(
        AudioEngine* audioEngine = nullptr,
        AudioBufferManager* bufferManager = nullptr,
        int64_t selectionStart = 0,
        int64_t selectionEnd = 0,
        const ParametricEQ::Parameters& currentParams = ParametricEQ::Parameters::createNeutral()
    );

    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

private:
    /**
     * Single EQ band control panel with Frequency, Gain, Q sliders.
     */
    class BandControl : public juce::Component
    {
    public:
        BandControl(const juce::String& bandName);
        ~BandControl() override = default;

        void paint(juce::Graphics& g) override;
        void resized() override;

        /**
         * Set the band parameters.
         */
        void setParameters(const ParametricEQ::BandParameters& params);

        /**
         * Get the current band parameters.
         */
        ParametricEQ::BandParameters getParameters() const;

        /**
         * Reset to neutral (0 dB gain).
         */
        void resetToNeutral();

    private:
        juce::String m_bandName;

        juce::Label m_titleLabel;

        juce::Label m_freqLabel;
        juce::Slider m_freqSlider;
        juce::Label m_freqValueLabel;

        juce::Label m_gainLabel;
        juce::Slider m_gainSlider;
        juce::Label m_gainValueLabel;

        juce::Label m_qLabel;
        juce::Slider m_qSlider;
        juce::Label m_qValueLabel;

        /**
         * Update value labels when sliders change.
         */
        void updateValueLabels();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandControl)
    };

    void onApplyClicked();
    void onCancelClicked();
    void onResetClicked();
    void onPreviewClicked();
    void onParameterChanged();  // Called when any EQ parameter slider changes

    /**
     * Get current EQ parameters from UI controls.
     */
    ParametricEQ::Parameters getCurrentParameters() const;

    juce::Label m_titleLabel;

    BandControl m_lowBand;
    BandControl m_midBand;
    BandControl m_highBand;

    // Preview controls
    juce::TextButton m_previewButton;
    juce::ToggleButton m_loopToggle;

    juce::TextButton m_resetButton;
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;

    std::optional<ParametricEQ::Parameters> m_result;

    // Preview support
    AudioEngine* m_audioEngine;
    AudioBufferManager* m_bufferManager;
    int64_t m_selectionStart;
    int64_t m_selectionEnd;
    bool m_isPreviewPlaying;  // Track preview playback state for toggle

    // EQ processor for preview
    std::unique_ptr<ParametricEQ> m_parametricEQ;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParametricEQDialog)
};
