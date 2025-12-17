/*
  ==============================================================================

    PluginScanDialogs.h
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
#include "PluginScanState.h"

/**
 * Dialog shown when a plugin fails to scan.
 * Provides Retry, Skip, and Cancel options.
 */
class PluginScanErrorDialog : public juce::Component
{
public:
    enum class Result
    {
        Retry,      // User wants to retry scanning this plugin
        Skip,       // Skip this plugin, continue with others
        Cancel,     // Cancel the entire scan
        None        // Dialog still open / no result yet
    };

    /**
     * Create the error dialog.
     *
     * @param pluginName Name/path of the failed plugin
     * @param errorMessage Description of what went wrong
     * @param isCrash If true, plugin crashed (more severe)
     */
    PluginScanErrorDialog(const juce::String& pluginName,
                           const juce::String& errorMessage,
                           bool isCrash = false);

    ~PluginScanErrorDialog() override;

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    /** Get the user's choice (after dialog closes) */
    Result getResult() const { return m_result; }

    /**
     * Show the dialog modally and return the result.
     *
     * @return User's choice (Retry, Skip, or Cancel)
     */
    static Result showDialog(const juce::String& pluginName,
                             const juce::String& errorMessage,
                             bool isCrash = false);

private:
    void onRetryClicked();
    void onSkipClicked();
    void onCancelClicked();

    juce::Label m_titleLabel;
    juce::Label m_pluginLabel;
    juce::Label m_errorLabel;
    juce::Label m_hintLabel;

    juce::TextButton m_retryButton;
    juce::TextButton m_skipButton;
    juce::TextButton m_cancelButton;

    juce::String m_pluginName;
    juce::String m_errorMessage;
    bool m_isCrash;

    Result m_result = Result::None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginScanErrorDialog)
};

/**
 * Dialog shown at the end of a scan.
 * Shows summary of results: successes, failures, and details.
 */
class PluginScanSummaryDialog : public juce::Component,
                                  public juce::TableListBoxModel
{
public:
    /**
     * Create the summary dialog.
     *
     * @param summary The completed scan summary with all results
     */
    explicit PluginScanSummaryDialog(const PluginScanSummary& summary);

    ~PluginScanSummaryDialog() override;

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // TableListBoxModel overrides
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height,
                           bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId,
                   int width, int height, bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;

    //==============================================================================
    /**
     * Show the summary dialog.
     *
     * @param summary The completed scan summary
     */
    static void showDialog(const PluginScanSummary& summary);

private:
    enum ColumnId
    {
        StatusColumn = 1,
        NameColumn = 2,
        ReasonColumn = 3
    };

    void onCloseClicked();
    void onViewLogClicked();

    juce::Label m_titleLabel;
    juce::Label m_summaryLabel;
    juce::Label m_detailsLabel;

    juce::TableListBox m_failedTable;
    juce::TextButton m_closeButton;
    juce::TextButton m_viewLogButton;

    // Summary data
    int m_successCount;
    int m_failedCount;
    int m_skippedCount;
    int m_cachedCount;
    int m_totalPlugins;
    juce::RelativeTime m_duration;

    // Failed plugins for the table
    juce::Array<PluginScanResult> m_failedResults;

    // Full summary for log generation
    PluginScanSummary m_summary;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginScanSummaryDialog)
};

/**
 * Dialog shown when a plugin scan times out.
 * Provides Wait Longer, Skip, and Always Skip (Blacklist) options.
 *
 * This replaces the previous auto-blacklisting behavior to give users
 * control over slow-loading plugins (AI/ML plugins, complex DSP, etc.)
 */
class PluginTimeoutDialog : public juce::Component
{
public:
    enum class Result
    {
        WaitLonger,     // User wants to wait another timeout period
        Skip,           // Skip this plugin for this scan only
        Blacklist,      // Skip and add to blacklist (never scan again)
        None            // Dialog still open / no result yet
    };

    /**
     * Create the timeout dialog.
     *
     * @param pluginName Name/path of the slow plugin
     * @param timeoutSeconds How long we've already waited
     */
    PluginTimeoutDialog(const juce::String& pluginName, int timeoutSeconds);

    ~PluginTimeoutDialog() override;

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    /** Get the user's choice (after dialog closes) */
    Result getResult() const { return m_result; }

    /**
     * Show the dialog modally and return the result.
     *
     * @param pluginName Name/path of the slow plugin
     * @param timeoutSeconds How long we've already waited
     * @return User's choice (WaitLonger, Skip, or Blacklist)
     */
    static Result showDialog(const juce::String& pluginName, int timeoutSeconds);

private:
    void onWaitLongerClicked();
    void onSkipClicked();
    void onBlacklistClicked();

    juce::Label m_titleLabel;
    juce::Label m_pluginLabel;
    juce::Label m_messageLabel;
    juce::Label m_hintLabel;

    juce::TextButton m_waitLongerButton;
    juce::TextButton m_skipButton;
    juce::TextButton m_blacklistButton;

    juce::String m_pluginName;

    Result m_result = Result::None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginTimeoutDialog)
};

/**
 * Progress dialog shown during plugin scanning.
 * Shows current plugin, progress bar, and Cancel button.
 *
 * Thread Safety:
 * - setProgressData() can be called from any thread (stores to atomics)
 * - All UI updates happen on the message thread via Timer
 */
class PluginScanProgressDialog : public juce::Component,
                                   private juce::Timer
{
public:
    using CancelCallback = std::function<void()>;

    /**
     * Create the progress dialog.
     *
     * @param onCancel Callback when user clicks Cancel
     */
    explicit PluginScanProgressDialog(CancelCallback onCancel = nullptr);

    ~PluginScanProgressDialog() override;

    //==============================================================================
    /**
     * Thread-safe progress update.
     * Can be called from any thread - stores values atomically.
     * UI updates happen on next timer tick (message thread).
     */
    void setProgressData(float progress, const juce::String& currentPlugin,
                         int currentIndex, int totalCount);

    /** Mark scan as complete (call from message thread or async) */
    void setComplete();

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    /**
     * Show the progress dialog in a window.
     *
     * @return Pointer to the created window (caller owns it)
     */
    juce::DialogWindow* showInWindow();

private:
    void timerCallback() override;
    void onCancelClicked();
    void updateUIFromAtomics();  // Called on message thread to sync UI

    juce::Label m_titleLabel;
    juce::Label m_statusLabel;
    juce::Label m_currentPluginLabel;
    juce::ProgressBar m_progressBar;
    juce::TextButton m_cancelButton;
    juce::Label m_elapsedTimeLabel;

    double m_progressValue = 0.0;
    juce::Time m_startTime;
    std::atomic<bool> m_isComplete{false};

    CancelCallback m_onCancel;

    // Thread-safe progress data (written from any thread, read on message thread)
    std::atomic<float> m_atomicProgress{0.0f};
    std::atomic<int> m_atomicCurrentIndex{0};
    std::atomic<int> m_atomicTotalCount{0};
    juce::CriticalSection m_pluginNameLock;
    juce::String m_atomicPluginName;
    std::atomic<bool> m_hasNewData{false};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginScanProgressDialog)
};
