/*
  ==============================================================================

    NormalizeDialog.h
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

// Forward declarations
class AudioEngine;
class AudioBufferManager;

/**
 * Dialog for normalizing audio to a target peak level.
 *
 * Features:
 * - Target peak level control (0 to -80 dB)
 * - Current peak level display
 * - Required gain calculation display
 * - Preview button for non-destructive audition
 * - Apply button for permanent edit with undo support
 *
 * Threading: All operations run on message thread.
 * Preview uses AudioEngine::PreviewMode::OFFLINE_BUFFER.
 */
class NormalizeDialog : public juce::Component
{
public:
    // Normalization mode
    enum class NormalizeMode
    {
        PEAK,  // Normalize to peak level (traditional)
        RMS    // Normalize to RMS level (perceived loudness)
    };
    /**
     * Constructor.
     *
     * @param audioEngine Pointer to the audio engine for preview playback
     * @param bufferManager Pointer to buffer manager for audio extraction
     * @param selectionStart Start sample of selection (0 if no selection)
     * @param selectionEnd End sample of selection (total length if no selection)
     */
    NormalizeDialog(AudioEngine* audioEngine,
                   AudioBufferManager* bufferManager,
                   int64_t selectionStart,
                   int64_t selectionEnd);
    ~NormalizeDialog() override;

    /**
     * Get the target peak level in dB.
     * @return Target peak level (-80.0 to 0.0 dB)
     */
    float getTargetLevel() const { return static_cast<float>(m_targetLevelSlider.getValue()); }

    /**
     * Get the current normalization mode.
     * @return Current mode (PEAK or RMS)
     */
    NormalizeMode getMode() const { return m_mode; }

    /**
     * Get the current peak level in dB.
     * @return Current peak level in dB
     */
    float getCurrentPeakDB() const { return m_currentPeakDB; }

    /**
     * Get the current RMS level in dB.
     * @return Current RMS level in dB
     */
    float getCurrentRMSDB() const { return m_currentRMSDB; }

    /**
     * Set a callback to be invoked when Apply is clicked.
     * Callback receives the target level in dB.
     */
    void onApply(std::function<void(float)> callback) { m_applyCallback = callback; }

    /**
     * Set a callback to be invoked when Cancel is clicked.
     */
    void onCancel(std::function<void()> callback) { m_cancelCallback = callback; }

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;

private:
    /**
     * Analyze current peak level in the selection or entire file.
     * Updates UI labels with current peak and required gain.
     */
    void analyzePeakLevel();

    /**
     * Update current levels (both peak and RMS).
     * Called when mode changes or dialog opens.
     */
    void updateCurrentLevels();

    /**
     * Update the required gain label based on current/target peaks.
     */
    void updateRequiredGain();

    /**
     * Handle mode selector change.
     * Updates UI and recalculates levels.
     */
    void onModeChanged();

    /**
     * Preview button callback.
     * Creates a normalized copy of the selection and plays it using preview system.
     */
    void onPreviewClicked();

    /**
     * Apply button callback.
     * Invokes the apply callback with the target level.
     */
    void onApplyClicked();

    /**
     * Cancel button callback.
     * Stops preview playback and invokes cancel callback.
     */
    void onCancelClicked();

    /**
     * Bypass button callback.
     * Toggles bypass state during preview for A/B comparison.
     */
    void onBypassClicked();

    /**
     * Target level slider changed callback.
     * Updates required gain display.
     */
    void onTargetLevelChanged();

    // UI Components
    juce::Label m_titleLabel;
    juce::Label m_modeLabel;
    juce::ComboBox m_modeSelector;
    juce::Slider m_targetLevelSlider;
    juce::Label m_targetLevelLabel;
    juce::Label m_currentPeakLabel;
    juce::Label m_currentPeakValue;
    juce::Label m_currentRMSLabel;
    juce::Label m_currentRMSValue;
    juce::Label m_requiredGainLabel;
    juce::Label m_requiredGainValue;
    juce::ToggleButton m_loopToggle;
    juce::TextButton m_previewButton;
    juce::TextButton m_bypassButton;      // NEW: Toggle bypass for A/B comparison
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;

    // Audio system references
    AudioEngine* m_audioEngine;
    AudioBufferManager* m_bufferManager;

    // Selection bounds
    int64_t m_selectionStart;
    int64_t m_selectionEnd;

    // State
    NormalizeMode m_mode {NormalizeMode::PEAK};  // Default to Peak for backward compatibility
    float m_currentPeakDB {0.0f};
    float m_currentRMSDB {-std::numeric_limits<float>::infinity()};
    bool m_isPreviewPlaying {false};  // Track preview playback state for toggle
    std::function<void(float)> m_applyCallback;
    std::function<void()> m_cancelCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NormalizeDialog)
};
