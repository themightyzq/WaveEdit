/*
  ==============================================================================

    LoopingToolsDialog.h
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
#include <juce_audio_formats/juce_audio_formats.h>
#include "../DSP/LoopRecipe.h"
#include "../DSP/LoopEngine.h"

/**
 * Looping Tools Dialog.
 *
 * Provides a full UI for the LoopEngine pipeline:
 *   1. Loop Settings  -- crossfade length, curve, max cap, zero-crossing refinement
 *   2. Variation Settings -- offset step, shuffle seed for multi-variation export
 *   3. Output Settings -- directory, suffix, bit depth, filename preview
 *   4. Waveform Preview -- renders the loop buffer with crossfade zone overlay
 *
 * The dialog owns its export logic (WAV write).
 * Buffer manipulation and undo registration are NOT performed here -- this
 * dialog produces standalone files, so no undo entry is required.
 *
 * Layout (top to bottom):
 *   - Section: Loop Settings     (crossfade controls + zero-crossing row)
 *   - Section: Variation Settings (disabled when loopCount == 1)
 *   - Section: Output            (directory, suffix, bit depth, filename preview)
 *   - Section: Preview           (waveform 140px + diagnostics label)
 *   - Button row: Preview | Export | Cancel
 */
class LoopingToolsDialog : public juce::Component,
                           private juce::Timer
{
public:
    /**
     * Creates the Looping Tools dialog.
     *
     * @param buffer         Source audio buffer (read-only; not modified)
     * @param sampleRate     Sample rate in Hz
     * @param selectionStart Selection start in samples (inclusive)
     * @param selectionEnd   Selection end in samples (exclusive)
     * @param sourceFile     Source file used to derive default output names
     */
    LoopingToolsDialog(const juce::AudioBuffer<float>& buffer,
                       double sampleRate,
                       int64_t selectionStart,
                       int64_t selectionEnd,
                       const juce::File& sourceFile);

    ~LoopingToolsDialog() override;

    //==========================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==========================================================================
    // Callbacks

    /** Invoked when the user clicks Cancel. */
    std::function<void()> onCancel;

    /** Callback to start preview playback with a loop buffer. */
    std::function<void(const juce::AudioBuffer<float>&, double)> onPreviewRequested;

    /** Callback to stop preview playback. */
    std::function<void()> onPreviewStopped;

private:
    //==========================================================================
    // Timer (debounce for live preview updates)
    void timerCallback() override;
    //==========================================================================
    // Nested waveform preview component

    /**
     * Lightweight waveform preview that displays a loop buffer and highlights
     * the crossfade zone (first N samples) with a semi-transparent blue overlay.
     */
    class WaveformPreview : public juce::Component
    {
    public:
        WaveformPreview() {}

        /** Replace the displayed buffer. Triggers a repaint. */
        void setBuffer(const juce::AudioBuffer<float>& buffer);

        /** Set the crossfade length in samples so the zone can be highlighted. */
        void setCrossfadeLength(int64_t samples);

        void paint(juce::Graphics& g) override;

    private:
        void drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds);

        const juce::AudioBuffer<float>* m_buffer = nullptr;
        int64_t m_crossfadeLen = 0;
    };

    //==========================================================================
    // Section 1: Loop Settings

    juce::Label     m_loopCountLabel;
    juce::Slider    m_loopCountSlider;
    juce::Label     m_loopCountValueLabel;

    juce::Label     m_crossfadeLenLabel;
    juce::Slider    m_crossfadeLenSlider;
    juce::Label     m_crossfadeLenValueLabel;
    juce::ComboBox  m_crossfadeModeCombo;   // "ms" (1), "%" (2)

    juce::Label     m_maxCrossfadeLabel;
    juce::Slider    m_maxCrossfadeSlider;
    juce::Label     m_maxCrossfadeValueLabel;

    juce::Label     m_crossfadeCurveLabel;
    juce::ComboBox  m_crossfadeCurveCombo;  // Linear(1), Equal Power(2), S-Curve(3)

    juce::ToggleButton m_zeroCrossingToggle;
    juce::Label     m_searchWindowLabel;
    juce::Slider    m_searchWindowSlider;
    juce::Label     m_searchWindowValueLabel;

    //==========================================================================
    // Section 2: Variation Settings

    juce::Label     m_offsetStepLabel;
    juce::Slider    m_offsetStepSlider;
    juce::Label     m_offsetStepValueLabel;

    juce::Label     m_shuffleSeedLabel;
    juce::Slider    m_shuffleSeedSlider;
    juce::Label     m_shuffleSeedValueLabel;

    //==========================================================================
    // Section 2.5: Shepard Tone

    juce::ToggleButton m_shepardEnable;
    juce::Label     m_shepardSemitonesLabel;
    juce::Slider    m_shepardSemitonesSlider;
    juce::Label     m_shepardSemitonesValueLabel;
    juce::Label     m_shepardDirectionLabel;
    juce::ComboBox  m_shepardDirectionCombo;
    juce::Label     m_shepardRampLabel;
    juce::ComboBox  m_shepardRampCombo;
    juce::Label     m_shepardConstraintLabel;

    //==========================================================================
    // Section 3: Output

    juce::Label     m_outputDirLabel;
    juce::Label     m_outputDirPathLabel;   // Shows current path
    juce::TextButton m_browseButton;

    juce::Label     m_suffixLabel;
    juce::TextEditor m_suffixEditor;

    juce::Label     m_bitDepthLabel;
    juce::ComboBox  m_bitDepthCombo;        // 16, 24, 32

    juce::Label     m_filePreviewHeaderLabel;
    juce::Label     m_filePreviewLabel;

    //==========================================================================
    // Section 4: Preview

    std::unique_ptr<WaveformPreview> m_waveformPreview;
    juce::Label     m_diagnosticsLabel;

    //==========================================================================
    // Buttons

    juce::TextButton m_previewPlaybackButton;  // Real-time preview playback toggle
    juce::TextButton m_previewButton;           // Generate waveform preview
    juce::TextButton m_exportButton;
    juce::TextButton m_cancelButton;

    //==========================================================================
    // State

    const juce::AudioBuffer<float>& m_audioBuffer;
    double      m_sampleRate;
    int64_t     m_selectionStart;
    int64_t     m_selectionEnd;
    juce::File  m_sourceFile;
    juce::File  m_outputDirectory;

    LoopResult  m_previewResult;    // Cached result from last updatePreview()
    bool        m_isPreviewPlaying = false;  // Real-time preview playback state

    //==========================================================================
    // Core logic

    /** Build a LoopRecipe from the current UI state. */
    LoopRecipe buildRecipe() const;

    /** Run LoopEngine::createLoop, update waveform preview and diagnostics. */
    void updatePreview();

    /** Rebuild the filename preview label from current recipe + suffix. */
    void updateFilePreview();

    /** Start or stop real-time preview playback. */
    void togglePreviewPlayback();

    /** Rebuild and send preview buffer to audio engine (called after debounce). */
    void rebuildPreviewPlayback();

    /** Schedule a debounced rebuild of preview playback (200ms). */
    void schedulePreviewRebuild();

    /** Export one or more loop WAV files to m_outputDirectory. */
    void exportLoops();

    /** Write a WAV file. Returns true on success. */
    bool writeWavFile(const juce::File& file,
                      const juce::AudioBuffer<float>& buffer,
                      double sampleRate,
                      int bitDepth);

    //==========================================================================
    // Layout helpers

    /**
     * Add a standard slider row (label | slider | valueLabel).
     * Registers onValueChange to keep valueLabel in sync.
     */
    void addSliderRow(juce::Label& label, const juce::String& text,
                      juce::Slider& slider, double min, double max,
                      double step, double defaultVal,
                      juce::Label& valueLabel, const juce::String& suffix);

    /** Update value label text from slider value. */
    void updateValueLabel(juce::Slider& slider, juce::Label& label,
                          const juce::String& suffix);

    /** Enable or disable all Variation Settings controls based on loop count. */
    void updateVariationControlsEnabled();

    /** Enable or disable search window controls based on zero-crossing toggle. */
    void updateZeroCrossingControlsEnabled();

    /** Enable or disable Shepard controls based on enable toggle. */
    void updateShepardControlsEnabled();

    /** Update Shepard constraint label with calculated loop length. */
    void updateShepardConstraint();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopingToolsDialog)
};
