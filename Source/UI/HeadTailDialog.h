/*
  ==============================================================================

    HeadTailDialog.h
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
#include "../DSP/HeadTailRecipe.h"

/**
 * Head & Tail Processing Dialog.
 *
 * Provides a comprehensive UI for the HeadTailEngine pipeline:
 * 1. Intelligent detection — auto-trim silence from head/tail using peak or RMS analysis
 * 2. Time-based edits — fixed head/tail trim, prepend/append silence, fade in/out
 *
 * The dialog is pure UI: it builds a HeadTailRecipe and calls onApply.
 * All buffer manipulation and undo registration lives in DSPController::applyHeadTail.
 *
 * Layout (top to bottom):
 *   - Section header "Intelligent Trim" + enable checkbox + mode combo
 *   - Detection parameter rows (disabled when checkbox is off)
 *   - Section header "Time-Based Edits"
 *   - Trim / padding / fade parameter rows
 *   - Waveform preview (140px)
 *   - Recipe summary text editor (80px)
 *   - Button row: Preview | Apply | Cancel (centred)
 */
class HeadTailDialog : public juce::Component
{
public:
    /**
     * Creates the Head & Tail Processing dialog.
     *
     * @param audioBuffer   Source audio buffer (not modified by dialog)
     * @param sampleRate    Sample rate for time calculations
     */
    HeadTailDialog(const juce::AudioBuffer<float>& audioBuffer,
                   double sampleRate);

    ~HeadTailDialog() override;

    //==========================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==========================================================================
    // Callbacks

    /** Invoked when the user clicks Apply. Receives the built recipe. */
    std::function<void(const HeadTailRecipe&)> onApply;

    /** Invoked when the user clicks Cancel. */
    std::function<void()> onCancel;

private:
    //==========================================================================
    // Section 1: Intelligent Trim

    juce::ToggleButton  m_detectEnableToggle;
    juce::Label         m_detectModeLabel;
    juce::ComboBox      m_detectModeCombo;     // "Peak" (1), "RMS" (2)

    juce::Label         m_thresholdLabel;
    juce::Slider        m_thresholdSlider;
    juce::Label         m_thresholdValueLabel;

    juce::Label         m_holdTimeLabel;
    juce::Slider        m_holdTimeSlider;
    juce::Label         m_holdTimeValueLabel;

    juce::Label         m_leadingPadLabel;
    juce::Slider        m_leadingPadSlider;
    juce::Label         m_leadingPadValueLabel;

    juce::Label         m_trailingPadLabel;
    juce::Slider        m_trailingPadSlider;
    juce::Label         m_trailingPadValueLabel;

    juce::Label         m_minKeptLabel;
    juce::Slider        m_minKeptSlider;
    juce::Label         m_minKeptValueLabel;

    juce::Label         m_maxTrimLabel;
    juce::Slider        m_maxTrimSlider;
    juce::Label         m_maxTrimValueLabel;   // Shows "no limit" when value is 0

    //==========================================================================
    // Section 2: Time-Based Edits

    juce::Label         m_headTrimLabel;
    juce::Slider        m_headTrimSlider;
    juce::Label         m_headTrimValueLabel;

    juce::Label         m_tailTrimLabel;
    juce::Slider        m_tailTrimSlider;
    juce::Label         m_tailTrimValueLabel;

    juce::Label         m_prependLabel;
    juce::Slider        m_prependSlider;
    juce::Label         m_prependValueLabel;

    juce::Label         m_appendLabel;
    juce::Slider        m_appendSlider;
    juce::Label         m_appendValueLabel;

    juce::Label         m_fadeInLabel;
    juce::Slider        m_fadeInSlider;
    juce::Label         m_fadeInValueLabel;
    juce::Label         m_fadeInCurveLabel;
    juce::ComboBox      m_fadeInCurveCombo;    // Linear(1), Exp(2), Log(3), S-Curve(4)

    juce::Label         m_fadeOutLabel;
    juce::Slider        m_fadeOutSlider;
    juce::Label         m_fadeOutValueLabel;
    juce::Label         m_fadeOutCurveLabel;
    juce::ComboBox      m_fadeOutCurveCombo;

    //==========================================================================
    // Waveform preview

    class WaveformPreview : public juce::Component
    {
    public:
        explicit WaveformPreview(const juce::AudioBuffer<float>& audioBuffer);

        void paint(juce::Graphics& g) override;

        /** Show grayed overlay on [0, keepStart) and (keepEnd, end] */
        void setTrimRegion(int64_t keepStart, int64_t keepEnd);

        /** Show green vertical lines at content boundaries */
        void setDetectionBoundaries(int64_t start, int64_t end);

        /** Remove all markers and overlays */
        void clearMarkers();

    private:
        void drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawTrimOverlay(juce::Graphics& g, juce::Rectangle<int> bounds);
        void drawBoundaryLines(juce::Graphics& g, juce::Rectangle<int> bounds);

        const juce::AudioBuffer<float>& m_audioBuffer;
        int64_t m_keepStart = -1;
        int64_t m_keepEnd   = -1;
        int64_t m_boundaryStart = -1;
        int64_t m_boundaryEnd   = -1;
        bool m_hasMarkers = false;
    };

    std::unique_ptr<WaveformPreview> m_waveformPreview;

    //==========================================================================
    // Recipe summary

    juce::TextEditor m_summaryEditor;

    //==========================================================================
    // Buttons

    juce::TextButton m_previewButton;
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;

    //==========================================================================
    // State

    const juce::AudioBuffer<float>& m_audioBuffer;
    double                           m_sampleRate;

    //==========================================================================
    // Helper methods

    /**
     * Adds a standard slider row (Label 180px | Slider 280px | ValueLabel 80px).
     * Registers an onValueChange callback that keeps valueLabel in sync.
     */
    void addSliderRow(juce::Label& label, const juce::String& text,
                      juce::Slider& slider, double min, double max,
                      double step, double defaultVal,
                      juce::Label& valueLabel, const juce::String& suffix);

    /** Build a HeadTailRecipe from the current UI state. */
    HeadTailRecipe buildRecipe() const;

    /** Rebuild the summary TextEditor from the current recipe. */
    void updateSummary();

    /** Update a value label from its slider's current value. */
    void updateValueLabel(juce::Slider& slider, juce::Label& label,
                          const juce::String& suffix);

    /** Enable or disable all detection controls based on the toggle state. */
    void updateDetectionControlsEnabled();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HeadTailDialog)
};
