#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <optional>

// Forward declarations
class AudioEngine;
class AudioBufferManager;

/**
 * Modal dialog for entering precise gain adjustment values.
 *
 * Allows user to enter any decimal gain value (positive or negative)
 * and apply it to the audio buffer. Used for precise gain control
 * beyond the keyboard shortcuts (Shift+Up/Down).
 *
 * Valid gain range: -100.0 to +100.0 dB
 *
 * NEW: Preview System Integration (Phase 1.3)
 * - Adds "Preview" button to toggle real-time gain preview
 * - Slider for interactive gain adjustment with instant feedback
 * - Uses AudioEngine::setPreviewMode(REALTIME_DSP) for zero-latency preview
 * - Preview automatically disabled on Apply or Cancel
 *
 * Thread Safety: UI thread only. Must be shown from message thread.
 * The showDialog() method blocks until the user dismisses the dialog.
 */
class GainDialog : public juce::Component
{
public:
    /**
     * Creates a GainDialog with optional AudioEngine for preview support.
     *
     * @param audioEngine Optional AudioEngine pointer for preview functionality.
     *                    If nullptr, preview button will be disabled.
     * @param bufferManager Optional AudioBufferManager for extracting preview selection.
     *                      If nullptr, preview will play entire file.
     * @param selectionStart Start sample of selection (0 if no selection)
     * @param selectionEnd End sample of selection (total length if no selection)
     */
    explicit GainDialog(AudioEngine* audioEngine = nullptr,
                       AudioBufferManager* bufferManager = nullptr,
                       int64_t selectionStart = 0,
                       int64_t selectionEnd = 0);
    ~GainDialog() override = default;

    /**
     * Show the dialog modally and return the user's gain input.
     *
     * @param audioEngine Optional AudioEngine pointer for preview functionality.
     *                    Pass nullptr to disable preview (backward compatible).
     * @param bufferManager Optional AudioBufferManager for extracting preview selection.
     *                      Pass nullptr to preview entire file.
     * @param selectionStart Start sample of selection (0 if no selection)
     * @param selectionEnd End sample of selection (total length if no selection)
     * @return std::optional<float> containing gain in dB if user clicked Apply,
     *         std::nullopt if user clicked Cancel or closed the dialog
     */
    static std::optional<float> showDialog(AudioEngine* audioEngine = nullptr,
                                           AudioBufferManager* bufferManager = nullptr,
                                           int64_t selectionStart = 0,
                                           int64_t selectionEnd = 0);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    /**
     * Validates the text input and returns the gain value if valid.
     *
     * @return std::optional<float> containing validated gain value,
     *         std::nullopt if input is invalid
     */
    std::optional<float> getValidatedGain() const;

    void onApplyClicked();
    void onCancelClicked();
    void onPreviewClicked();
    void onBypassClicked();
    void onSliderValueChanged();
    void disablePreview();

    // UI Components
    juce::Label m_titleLabel;
    juce::Label m_gainLabel;
    juce::TextEditor m_gainInput;
    juce::Slider m_gainSlider;          // NEW: Real-time gain slider
    juce::Label m_gainValueLabel;       // NEW: Display current slider value
    juce::ToggleButton m_loopCheckbox;  // NEW: Loop preview playback
    juce::TextButton m_previewButton;   // NEW: Toggle preview on/off
    juce::TextButton m_bypassButton;    // NEW: Toggle bypass for A/B comparison
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;

    // State
    std::optional<float> m_result;
    AudioEngine* m_audioEngine;              // AudioEngine for preview (not owned)
    AudioBufferManager* m_bufferManager;     // BufferManager for selection extraction (not owned)
    bool m_isPreviewActive;                  // Track preview state
    bool m_isPreviewPlaying;                 // Track preview playback state for toggle
    int64_t m_selectionStart;                // Start sample of selection
    int64_t m_selectionEnd;                  // End sample of selection

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GainDialog)
};
