/*
  ==============================================================================

    DCOffsetDialog.h
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
 * Dialog for removing DC offset from selection.
 *
 * Features:
 * - Calculates and removes DC offset (average signal level)
 * - Preview button for non-destructive audition
 * - Apply button for permanent edit with undo support
 * - Requires selection (won't work on entire file)
 *
 * Threading: All operations run on message thread.
 * Preview uses AudioEngine::PreviewMode::OFFLINE_BUFFER.
 */
class DCOffsetDialog : public juce::Component
{
public:
    /**
     * Constructor.
     *
     * @param audioEngine Pointer to the audio engine for preview playback
     * @param bufferManager Pointer to buffer manager for audio extraction
     * @param selectionStart Start sample of selection
     * @param selectionEnd End sample of selection
     */
    DCOffsetDialog(AudioEngine* audioEngine,
                   AudioBufferManager* bufferManager,
                   int64_t selectionStart,
                   int64_t selectionEnd);
    ~DCOffsetDialog() override;

    /**
     * Set a callback to be invoked when Apply is clicked.
     */
    void onApply(std::function<void()> callback) { m_applyCallback = callback; }

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
     * Preview button callback.
     * Creates a DC-corrected copy of the selection and plays it using preview system.
     */
    void onPreviewClicked();

    /**
     * Apply button callback.
     * Invokes the apply callback.
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

    // UI Components
    juce::Label m_titleLabel;
    juce::Label m_instructionLabel;
    juce::ToggleButton m_loopToggle;
    juce::TextButton m_previewButton;
    juce::TextButton m_bypassButton;      // Toggle bypass for A/B comparison
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;

    // Audio system references
    AudioEngine* m_audioEngine;
    AudioBufferManager* m_bufferManager;

    // Selection bounds
    int64_t m_selectionStart;
    int64_t m_selectionEnd;

    // State
    bool m_isPreviewPlaying {false};  // Track preview playback state for toggle
    std::function<void()> m_applyCallback;
    std::function<void()> m_cancelCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DCOffsetDialog)
};
