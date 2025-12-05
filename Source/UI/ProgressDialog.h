/*
  ==============================================================================

    ProgressDialog.h
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
#include <atomic>
#include <functional>
#include <memory>

/**
 * Modal progress dialog for long-running DSP operations.
 *
 * Shows a progress bar, status text, elapsed time, and cancel button.
 * Executes work on a background thread while keeping UI responsive.
 *
 * Thread Safety:
 * - Dialog must be shown from message thread
 * - Work function executes on background thread
 * - Progress updates use atomics for thread-safe communication
 *
 * Usage:
 * @code
 * ProgressDialog::runWithProgress(
 *     "Applying Gain",
 *     [&buffer](ProgressCallback progress) -> bool {
 *         // Do work, call progress(0.5f, "Processing...") periodically
 *         // Return true on success, false on failure/cancel
 *         return true;
 *     },
 *     [](bool success) {
 *         // Handle completion
 *     }
 * );
 * @endcode
 */
class ProgressDialog : public juce::Component,
                       private juce::Timer
{
public:
    /**
     * Function type for the work to be performed.
     * @param progress Callback to report progress (returns false if cancelled)
     * @return true if completed successfully, false if failed or cancelled
     */
    using WorkFunction = std::function<bool(std::function<bool(float, const juce::String&)>)>;

    /**
     * Callback invoked when work completes or is cancelled.
     * @param success true if completed successfully, false if cancelled or failed
     */
    using CompletionCallback = std::function<void(bool success)>;

    /**
     * Shows a progress dialog and executes work on a background thread.
     * This is the main entry point for using the progress system.
     *
     * @param title Dialog title (e.g., "Applying Gain (+3.0 dB)")
     * @param work Function containing the work to perform
     * @param onComplete Callback when work finishes (called on message thread)
     *
     * Thread Safety: Must be called from message thread.
     */
    static void runWithProgress(const juce::String& title,
                                WorkFunction work,
                                CompletionCallback onComplete);

    ~ProgressDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    explicit ProgressDialog(const juce::String& title);

    void startWork(WorkFunction work, CompletionCallback onComplete);
    void timerCallback() override;
    void onCancelClicked();
    void onWorkComplete(bool success);

    // UI Components
    juce::Label m_titleLabel;
    juce::Label m_statusLabel;
    juce::ProgressBar m_progressBar;
    juce::TextButton m_cancelButton;
    juce::Label m_elapsedTimeLabel;

    // Thread-safe state (accessed from both threads)
    std::atomic<float> m_progress{0.0f};
    std::atomic<bool> m_cancelRequested{false};
    std::atomic<bool> m_isComplete{false};
    std::atomic<bool> m_wasSuccessful{false};
    std::atomic<bool> m_completionHandled{false};  // Guard against double completion
    juce::SpinLock m_statusLock;
    juce::String m_currentStatus;

    // Message-thread only state
    double m_progressValue{0.0};  // For ProgressBar (requires double)
    juce::Time m_startTime;
    CompletionCallback m_onComplete;
    std::unique_ptr<juce::Thread> m_workerThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressDialog)
};
