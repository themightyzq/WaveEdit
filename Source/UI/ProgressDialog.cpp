/*
  ==============================================================================

    ProgressDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "ProgressDialog.h"

//==============================================================================
// Worker Thread Class
//==============================================================================
class ProgressWorkerThread : public juce::Thread
{
public:
    ProgressWorkerThread(ProgressDialog::WorkFunction work,
                         std::atomic<float>& progress,
                         std::atomic<bool>& cancelRequested,
                         std::atomic<bool>& isComplete,
                         std::atomic<bool>& wasSuccessful,
                         juce::SpinLock& statusLock,
                         juce::String& currentStatus)
        : juce::Thread("ProgressWorker"),
          m_work(std::move(work)),
          m_progress(progress),
          m_cancelRequested(cancelRequested),
          m_isComplete(isComplete),
          m_wasSuccessful(wasSuccessful),
          m_statusLock(statusLock),
          m_currentStatus(currentStatus)
    {
    }

    void run() override
    {
        // Create progress callback that updates atomics
        auto progressCallback = [this](float progress, const juce::String& status) -> bool
        {
            m_progress.store(progress);

            {
                const juce::SpinLock::ScopedLockType lock(m_statusLock);
                m_currentStatus = status;
            }

            // Return false if cancellation requested
            return !m_cancelRequested.load();
        };

        // Execute the work
        bool success = false;
        try
        {
            success = m_work(progressCallback);
        }
        catch (const std::exception& e)
        {
            DBG("ProgressWorkerThread exception: " + juce::String(e.what()));
            success = false;
        }
        catch (...)
        {
            DBG("ProgressWorkerThread: unknown exception");
            success = false;
        }

        // Mark completion
        m_wasSuccessful.store(success && !m_cancelRequested.load());
        m_isComplete.store(true);
    }

private:
    ProgressDialog::WorkFunction m_work;
    std::atomic<float>& m_progress;
    std::atomic<bool>& m_cancelRequested;
    std::atomic<bool>& m_isComplete;
    std::atomic<bool>& m_wasSuccessful;
    juce::SpinLock& m_statusLock;
    juce::String& m_currentStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressWorkerThread)
};

//==============================================================================
// ProgressDialog Implementation
//==============================================================================
ProgressDialog::ProgressDialog(const juce::String& title)
    : m_titleLabel("titleLabel", title),
      m_statusLabel("statusLabel", "Preparing..."),
      m_progressBar(m_progressValue),
      m_cancelButton("Cancel"),
      m_elapsedTimeLabel("elapsedLabel", "Elapsed: 0:00")
{
    // Title label
    m_titleLabel.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Status label
    m_statusLabel.setFont(juce::FontOptions(13.0f));
    m_statusLabel.setJustificationType(juce::Justification::centredLeft);
    m_statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(m_statusLabel);

    // Progress bar
    m_progressBar.setPercentageDisplay(true);
    addAndMakeVisible(m_progressBar);

    // Cancel button
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    // Elapsed time label
    m_elapsedTimeLabel.setFont(juce::FontOptions(12.0f));
    m_elapsedTimeLabel.setJustificationType(juce::Justification::centredRight);
    m_elapsedTimeLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(m_elapsedTimeLabel);

    setSize(400, 160);
}

ProgressDialog::~ProgressDialog()
{
    stopTimer();

    // Ensure worker thread is stopped
    if (m_workerThread != nullptr)
    {
        m_cancelRequested.store(true);

        if (!m_workerThread->stopThread(2000))
        {
            // Force termination if cancellation didn't work in 2 seconds
            DBG("ProgressDialog: Worker thread failed to stop gracefully, forcing termination");
            m_workerThread->stopThread(-1);
        }
        m_workerThread.reset();
    }
}

void ProgressDialog::runWithProgress(const juce::String& title,
                                     WorkFunction work,
                                     CompletionCallback onComplete)
{
    // Create dialog on heap (will be managed by DialogWindow)
    auto* dialog = new ProgressDialog(title);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = title;
    options.dialogBackgroundColour = juce::Colour(0xff2b2b2b);
    options.escapeKeyTriggersCloseButton = false;
    options.useNativeTitleBar = false;
    options.resizable = false;

    // Show dialog (non-modal - we manage completion ourselves)
    auto* window = options.launchAsync();
    if (window != nullptr)
    {
        window->centreWithSize(dialog->getWidth(), dialog->getHeight());
    }

    // Start the work
    dialog->startWork(std::move(work), std::move(onComplete));
}

void ProgressDialog::startWork(WorkFunction work, CompletionCallback onComplete)
{
    m_onComplete = std::move(onComplete);
    m_startTime = juce::Time::getCurrentTime();

    // Create and start worker thread
    m_workerThread = std::make_unique<ProgressWorkerThread>(
        std::move(work),
        m_progress,
        m_cancelRequested,
        m_isComplete,
        m_wasSuccessful,
        m_statusLock,
        m_currentStatus
    );

    m_workerThread->startThread();

    // Start timer for UI updates (20 Hz)
    startTimer(50);
}

void ProgressDialog::timerCallback()
{
    // Update progress bar
    m_progressValue = static_cast<double>(m_progress.load());

    // Update status text
    {
        const juce::SpinLock::ScopedLockType lock(m_statusLock);
        m_statusLabel.setText(m_currentStatus, juce::dontSendNotification);
    }

    // Update elapsed time
    auto elapsed = juce::Time::getCurrentTime() - m_startTime;
    int totalSeconds = static_cast<int>(elapsed.inSeconds());
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    m_elapsedTimeLabel.setText(
        juce::String::formatted("Elapsed: %d:%02d", minutes, seconds),
        juce::dontSendNotification
    );

    // Force repaint of progress bar
    m_progressBar.repaint();

    // Check for completion with atomic guard against double-call
    if (m_isComplete.load())
    {
        bool expected = false;
        if (m_completionHandled.compare_exchange_strong(expected, true))
        {
            onWorkComplete(m_wasSuccessful.load());
        }
    }
}

void ProgressDialog::onCancelClicked()
{
    m_cancelRequested.store(true);
    m_cancelButton.setEnabled(false);
    m_cancelButton.setButtonText("Cancelling...");

    // Update status
    {
        const juce::SpinLock::ScopedLockType lock(m_statusLock);
        m_currentStatus = "Cancelling...";
    }
}

void ProgressDialog::onWorkComplete(bool success)
{
    stopTimer();

    // Wait for worker thread to finish
    if (m_workerThread != nullptr)
    {
        m_workerThread->stopThread(1000);
        m_workerThread.reset();
    }

    // Close the dialog window safely
    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
    {
        // Capture callback and window pointer before scheduling deletion
        auto callback = std::move(m_onComplete);

        // Schedule both callback execution and window deletion together
        // This ensures the callback runs before the window (and this dialog) are deleted
        juce::MessageManager::callAsync([window, callback, success]()
        {
            // Execute completion callback first
            if (callback)
            {
                callback(success);
            }

            // Now safe to close and delete the window
            window->exitModalState(success ? 1 : 0);
            window->setVisible(false);
            delete window;
        });
    }
}

void ProgressDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
}

void ProgressDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // Title at top
    m_titleLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(15);

    // Status text
    m_statusLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(10);

    // Progress bar
    m_progressBar.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(10);

    // Bottom row: elapsed time on left, cancel button on right
    auto bottomRow = bounds.removeFromTop(30);
    m_elapsedTimeLabel.setBounds(bottomRow.removeFromLeft(150));
    m_cancelButton.setBounds(bottomRow.removeFromRight(100));
}
