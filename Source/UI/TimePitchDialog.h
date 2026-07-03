/*
  ==============================================================================

    TimePitchDialog.h
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
#include <juce_audio_basics/juce_audio_basics.h>

class AudioEngine; // full include in the .cpp (preview playback)

/**
 * Shared Time-Stretch / Pitch-Shift processing dialog.
 *
 * A single dialog class serves both the Time Stretch and Pitch Shift commands
 * via the Mode enum. It follows the CLAUDE.md Sec 6.8 processing-dialog footer
 * (Preview / Bypass / Loop | Cancel / Apply) and mirrors HeadTailDialog's
 * offline-render preview architecture.
 *
 * The dialog is pure UI: it renders a short SoundTouch preview of an excerpt of
 * the source audio and calls onApply(value) when the user applies. All buffer
 * replacement, range validation, the identity short-circuit, and undo
 * registration live in DSPController.
 *
 * Preview excerpt: full-buffer SoundTouch rendering can be slow on long files,
 * so the preview renders from up to kPreviewSeconds of source audio starting at
 * the selection start (if any), else the edit cursor, else 0 -- clamped to the
 * buffer bounds. See computePreviewExcerpt().
 */
class TimePitchDialog : public juce::Component,
                        private juce::Timer,
                        private juce::ChangeListener
{
public:
    /** Which effect this dialog drives. */
    enum class Mode
    {
        TimeStretch, /**< Single parameter: tempo change in percent. */
        PitchShift   /**< Single parameter: pitch shift in semitones. */
    };

    /**
     * Creates the Time-Stretch / Pitch-Shift dialog.
     *
     * @param mode                  TimeStretch or PitchShift.
     * @param audioBuffer           Full source audio buffer (not modified).
     * @param sampleRate            Sample rate for time calculations.
     * @param hasSelection          True if the document has an active selection.
     * @param selectionStartSeconds Selection start in seconds (used when
     *                              hasSelection is true).
     * @param selectionEndSeconds   Selection end in seconds (used when
     *                              hasSelection is true) -- caps the preview
     *                              excerpt so preview and Apply scope agree.
     * @param cursorSeconds         Edit cursor position in seconds (used when
     *                              there is no selection).
     * @param audioEngine           Engine used for Sec 6.8 preview (may be null,
     *                              in which case the preview footer is inert).
     * @param documentLifeline      A Document-owned Component (its
     *                              WaveformDisplay) used as a SafePointer
     *                              lifeline for the async dialog.
     */
    TimePitchDialog(Mode mode,
                    const juce::AudioBuffer<float>& audioBuffer,
                    double sampleRate,
                    bool hasSelection,
                    double selectionStartSeconds,
                    double selectionEndSeconds,
                    double cursorSeconds,
                    AudioEngine* audioEngine = nullptr,
                    juce::Component* documentLifeline = nullptr);

    ~TimePitchDialog() override;

    //==========================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    //==========================================================================
    // Callbacks

    /** Invoked when the user clicks Apply. Receives the single parameter value
        (tempo percent or semitones, depending on Mode). */
    std::function<void(double value)> onApply;

    /** Invoked when the user clicks Cancel. */
    std::function<void()> onCancel;

    //==========================================================================
    // Pure helpers (unit-testable)

    /**
     * Computes the [start, end) sample range of the preview excerpt.
     *
     * The excerpt begins at the selection start (when hasSelection is true)
     * else the edit cursor, clamped into [0, totalSamples], and extends up to
     * kPreviewSeconds of audio, clamped to the end of the buffer. When a
     * selection exists, the excerpt is additionally capped at the selection end
     * so the preview never plays past the range Apply will actually process.
     *
     * @return An empty range {0,0} for an empty buffer or invalid sample rate.
     */
    static juce::Range<juce::int64> computePreviewExcerpt(juce::int64 totalSamples,
                                                          double sampleRate,
                                                          bool hasSelection,
                                                          double selectionStartSeconds,
                                                          double selectionEndSeconds,
                                                          double cursorSeconds) noexcept;

    /** True if a rendered preview buffer with this shape can be played. Pure +
        static so it is unit-testable. */
    static bool previewIsPlayable(int numSamples, int numChannels) noexcept;

    /** Maximum preview excerpt length in seconds. */
    static constexpr double kPreviewSeconds = 10.0;

private:
    //==========================================================================
    // Controls

    juce::Label  m_helpLabel;      // Muted description of the effect
    juce::Label  m_paramLabel;     // "Tempo change (%)" / "Semitones"
    juce::Slider m_paramSlider;    // The single parameter
    juce::Label  m_summaryLabel;   // Projected duration / status / errors
    juce::Label  m_scopeLabel;     // "Applies to: selection ..." / "entire file"

    // Target-duration entry (TimeStretch mode only). Typing a duration drives
    // the tempo slider; slider moves update the shown target.
    juce::Label      m_targetLabel;
    juce::TextEditor m_targetEditor;

    juce::TextButton   m_previewButton;
    juce::TextButton   m_bypassButton;   // Enabled only while previewing
    juce::ToggleButton m_loopToggle;     // Default ON per Sec 6.8
    juce::TextButton   m_applyButton;
    juce::TextButton   m_cancelButton;

    //==========================================================================
    // State

    Mode   m_mode;
    double m_sampleRate;
    double m_fullDurationSeconds = 0.0;  // Whole-file duration (for projections)

    // Scope of the operation. When a selection exists, Apply is selection-scoped
    // (see DSPController), so projections use the selection duration.
    bool   m_hasSelection          = false;
    double m_selectionStartSeconds = 0.0;
    double m_selectionEndSeconds   = 0.0;
    double m_processDurationSeconds = 0.0;  // Selection duration, else whole file

    //==========================================================================
    // Audio preview (Sec 6.8) -- offline-render the recipe on an excerpt.

    AudioEngine*             m_audioEngine = nullptr;
    juce::Component::SafePointer<juce::Component> m_documentLifeline;
    juce::AudioBuffer<float> m_originalExcerpt;   // owned excerpt copy (A/B, lifetime)
    juce::AudioBuffer<float> m_processedBuffer;   // last offline render
    bool m_processedPlayable = false;
    bool m_previewActive     = false;
    bool m_bypassActive      = false;

    static constexpr int kPreviewDebounceMs = 200;

    /** True only when the engine pointer is set AND its document is still alive. */
    bool engineUsable() const noexcept;
    /** Recompute and render the offline preview buffer from the current value. */
    void renderProcessed();
    /** Begin audio preview (render + load + loop + play). No-op if not playable. */
    void startPreview();
    /** Stop audio preview and restore the engine to normal. Idempotent + UAF-safe. */
    void stopPreview();
    /** (Re)load the active (processed or bypassed) excerpt and start playback.
        Returns false if the buffer was empty. */
    bool reloadActiveBuffer();
    void timerCallback() override;

    /** Re-applies cached theme colours when the active theme changes (Sec 6.11). */
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    //==========================================================================
    // UI helpers

    /** Rebuild the summary label (projected duration / note) from the value. */
    void updateSummary();

    /** Rebuild the scope line ("Applies to: ...") from the selection state. */
    void updateScopeLabel();

    /** Commit the typed target duration (TimeStretch): convert to a tempo
        percent and drive the slider. No-op for invalid / non-positive input. */
    void commitTargetDuration();

    /** Format a time in seconds as ASCII mm:ss.mmm for the scope line. */
    static juce::String formatTime(double seconds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TimePitchDialog)
};
