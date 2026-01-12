/*
  ==============================================================================

    BatchProcessorEngine.h
    Created: 2025
    Author:  ZQ SFX

    Orchestrates batch processing of multiple audio files.
    Manages thread pool, progress aggregation, and job coordination.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "BatchJob.h"
#include "BatchProcessorSettings.h"

namespace waveedit
{

/**
 * @brief Listener for batch processing events
 */
class BatchProcessorListener
{
public:
    virtual ~BatchProcessorListener() = default;

    /**
     * @brief Called when overall batch progress changes
     * @param progress Overall progress (0.0 to 1.0)
     * @param currentFile Index of current file being processed (1-based)
     * @param totalFiles Total number of files in batch
     * @param statusMessage Current status message
     */
    virtual void batchProgressChanged(float progress,
                                       int currentFile,
                                       int totalFiles,
                                       const juce::String& statusMessage) = 0;

    /**
     * @brief Called when a single job completes
     * @param jobIndex Index of the completed job (0-based)
     * @param result Result of the job
     */
    virtual void jobCompleted(int jobIndex, const BatchJobResult& result) = 0;

    /**
     * @brief Called when the entire batch completes
     * @param cancelled Whether the batch was cancelled
     * @param successCount Number of successful jobs
     * @param failedCount Number of failed jobs
     * @param skippedCount Number of skipped jobs
     */
    virtual void batchCompleted(bool cancelled,
                                 int successCount,
                                 int failedCount,
                                 int skippedCount) = 0;
};

/**
 * @brief Batch processing summary statistics
 */
struct BatchSummary
{
    int totalFiles = 0;
    int completedFiles = 0;
    int failedFiles = 0;
    int skippedFiles = 0;
    double totalDurationSeconds = 0.0;
    juce::int64 totalInputBytes = 0;
    juce::int64 totalOutputBytes = 0;
    juce::StringArray errorMessages;
};

/**
 * @brief Orchestrates batch processing of multiple files
 */
class BatchProcessorEngine : public juce::Thread
{
public:
    BatchProcessorEngine();
    ~BatchProcessorEngine() override;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the batch processing settings
     */
    void setSettings(const BatchProcessorSettings& settings);

    /**
     * @brief Get current settings
     */
    const BatchProcessorSettings& getSettings() const { return m_settings; }

    /**
     * @brief Add a listener for progress events
     */
    void addListener(BatchProcessorListener* listener);

    /**
     * @brief Remove a listener
     */
    void removeListener(BatchProcessorListener* listener);

    // =========================================================================
    // Execution
    // =========================================================================

    /**
     * @brief Start batch processing
     * @return true if started successfully
     */
    bool startProcessing();

    /**
     * @brief Cancel batch processing
     */
    void cancelProcessing();

    /**
     * @brief Check if currently processing
     */
    bool isProcessing() const { return isThreadRunning(); }

    /**
     * @brief Wait for processing to complete
     * @param timeoutMs Maximum time to wait (-1 for infinite)
     * @return true if completed, false if timeout
     */
    bool waitForCompletion(int timeoutMs = -1);

    // =========================================================================
    // Status
    // =========================================================================

    /**
     * @brief Get overall progress (0.0 to 1.0)
     */
    float getProgress() const { return m_overallProgress.load(); }

    /**
     * @brief Get index of current job being processed (0-based)
     */
    int getCurrentJobIndex() const { return m_currentJobIndex.load(); }

    /**
     * @brief Get batch summary (call after completion)
     */
    const BatchSummary& getSummary() const { return m_summary; }

    /**
     * @brief Get all job results
     */
    const std::vector<BatchJobResult>& getResults() const { return m_results; }

private:
    // =========================================================================
    // Thread implementation
    // =========================================================================

    void run() override;

    /**
     * @brief Process a single job
     * @param job The job to process
     * @param jobIndex Index of the job (0-based)
     * @return The result of the job
     */
    BatchJobResult processJob(BatchJob& job, int jobIndex);

    /**
     * @brief Notify listeners of progress change
     */
    void notifyProgressChanged(float progress, int currentFile, int totalFiles,
                                const juce::String& message);

    /**
     * @brief Notify listeners of job completion
     */
    void notifyJobCompleted(int jobIndex, const BatchJobResult& result);

    /**
     * @brief Notify listeners of batch completion
     */
    void notifyBatchCompleted();

    // =========================================================================
    // Member variables
    // =========================================================================

    BatchProcessorSettings m_settings;
    juce::ListenerList<BatchProcessorListener> m_listeners;

    // Progress tracking
    std::atomic<float> m_overallProgress{0.0f};
    std::atomic<int> m_currentJobIndex{0};
    std::atomic<bool> m_cancelled{false};

    // Results
    std::vector<BatchJobResult> m_results;
    BatchSummary m_summary;

    // Thread-safe status message
    juce::CriticalSection m_statusLock;
    juce::String m_currentStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BatchProcessorEngine)
};

} // namespace waveedit
