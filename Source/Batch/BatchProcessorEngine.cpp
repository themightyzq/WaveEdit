/*
  ==============================================================================

    BatchProcessorEngine.cpp
    Created: 2025
    Author:  ZQ SFX

  ==============================================================================
*/

#include "BatchProcessorEngine.h"

namespace waveedit
{

BatchProcessorEngine::BatchProcessorEngine()
    : juce::Thread("BatchProcessor")
{
}

BatchProcessorEngine::~BatchProcessorEngine()
{
    // Ensure thread is stopped
    cancelProcessing();
    stopThread(5000);
}

void BatchProcessorEngine::setSettings(const BatchProcessorSettings& settings)
{
    jassert(!isThreadRunning()); // Don't change settings while processing
    m_settings = settings;
}

void BatchProcessorEngine::addListener(BatchProcessorListener* listener)
{
    m_listeners.add(listener);
}

void BatchProcessorEngine::removeListener(BatchProcessorListener* listener)
{
    m_listeners.remove(listener);
}

bool BatchProcessorEngine::startProcessing()
{
    if (isThreadRunning())
        return false;

    // Validate settings
    auto errors = m_settings.validate();
    if (!errors.isEmpty())
    {
        DBG("BatchProcessorEngine: Settings validation failed: " + errors[0]);
        return false;
    }

    // Reset state
    m_cancelled.store(false);
    m_overallProgress.store(0.0f);
    m_currentJobIndex.store(0);
    m_results.clear();
    m_summary = BatchSummary();
    m_summary.totalFiles = m_settings.inputFiles.size();

    // Start processing thread
    startThread();
    return true;
}

void BatchProcessorEngine::cancelProcessing()
{
    m_cancelled.store(true);
    signalThreadShouldExit();
}

bool BatchProcessorEngine::waitForCompletion(int timeoutMs)
{
    return waitForThreadToExit(timeoutMs);
}

void BatchProcessorEngine::run()
{
    auto startTime = juce::Time::getCurrentTime();

    DBG("BatchProcessorEngine: Starting batch processing of "
        + juce::String(m_settings.inputFiles.size()) + " files");

    // Prepare results vector
    m_results.resize(static_cast<size_t>(m_settings.inputFiles.size()));

    // Process each file
    for (int i = 0; i < m_settings.inputFiles.size(); ++i)
    {
        if (threadShouldExit() || m_cancelled.load())
            break;

        m_currentJobIndex.store(i);

        // Create job for this file
        juce::File inputFile(m_settings.inputFiles[i]);
        BatchJob job(inputFile, m_settings, i + 1, "batch");

        // Process the job
        auto result = processJob(job, i);
        m_results[static_cast<size_t>(i)] = result;

        // Update summary
        switch (result.status)
        {
            case BatchJobStatus::COMPLETED:
                m_summary.completedFiles++;
                m_summary.totalInputBytes += result.inputSizeBytes;
                m_summary.totalOutputBytes += result.outputSizeBytes;
                break;

            case BatchJobStatus::FAILED:
                m_summary.failedFiles++;
                m_summary.errorMessages.add(inputFile.getFileName() + ": " + result.errorMessage);

                // Handle error policy
                if (m_settings.errorHandling == BatchErrorHandling::STOP_ON_ERROR)
                {
                    DBG("BatchProcessorEngine: Stopping on error: " + result.errorMessage);
                    m_cancelled.store(true);
                }
                break;

            case BatchJobStatus::SKIPPED:
                m_summary.skippedFiles++;
                break;

            default:
                break;
        }

        // Notify job completed
        notifyJobCompleted(i, result);

        // Update overall progress
        float progress = static_cast<float>(i + 1) / static_cast<float>(m_settings.inputFiles.size());
        m_overallProgress.store(progress);
    }

    // Calculate total duration
    auto endTime = juce::Time::getCurrentTime();
    m_summary.totalDurationSeconds = (endTime - startTime).inSeconds();

    // Notify batch completed
    notifyBatchCompleted();

    DBG("BatchProcessorEngine: Batch processing complete. "
        + juce::String(m_summary.completedFiles) + " completed, "
        + juce::String(m_summary.failedFiles) + " failed, "
        + juce::String(m_summary.skippedFiles) + " skipped. "
        + "Duration: " + juce::String(m_summary.totalDurationSeconds, 1) + "s");
}

BatchJobResult BatchProcessorEngine::processJob(BatchJob& job, int jobIndex)
{
    int totalFiles = m_settings.inputFiles.size();

    // Create progress callback that updates overall progress
    auto progressCallback = [this, jobIndex, totalFiles](float jobProgress, const juce::String& message) -> bool
    {
        if (m_cancelled.load() || threadShouldExit())
            return false;

        // Calculate overall progress
        // Each job contributes 1/totalFiles to overall progress
        // Within each job, jobProgress goes from 0.0 to 1.0
        float baseProgress = static_cast<float>(jobIndex) / static_cast<float>(totalFiles);
        float jobContribution = jobProgress / static_cast<float>(totalFiles);
        float overallProgress = baseProgress + jobContribution;

        m_overallProgress.store(overallProgress);

        // Update status message (thread-safe)
        {
            juce::ScopedLock lock(m_statusLock);
            m_currentStatus = message;
        }

        // Notify listeners
        notifyProgressChanged(overallProgress, jobIndex + 1, totalFiles, message);

        return true; // Continue processing
    };

    // Execute the job with retry logic
    BatchJobResult result;
    int attempts = 0;
    int maxAttempts = m_settings.maxRetries + 1;

    while (attempts < maxAttempts)
    {
        attempts++;

        result = job.execute(progressCallback);

        // If successful or cancelled, don't retry
        if (result.status == BatchJobStatus::COMPLETED ||
            result.status == BatchJobStatus::SKIPPED ||
            m_cancelled.load() || threadShouldExit())
        {
            break;
        }

        // Retry on failure
        if (attempts < maxAttempts && result.status == BatchJobStatus::FAILED)
        {
            DBG("BatchProcessorEngine: Retrying job " + juce::String(jobIndex + 1)
                + " (attempt " + juce::String(attempts + 1) + "/" + juce::String(maxAttempts) + ")");

            // Small delay before retry
            wait(500);
        }
    }

    return result;
}

void BatchProcessorEngine::notifyProgressChanged(float progress, int currentFile, int totalFiles,
                                                   const juce::String& message)
{
    // Notify on message thread
    juce::MessageManager::callAsync([this, progress, currentFile, totalFiles, message]()
    {
        m_listeners.call([=](BatchProcessorListener& l)
        {
            l.batchProgressChanged(progress, currentFile, totalFiles, message);
        });
    });
}

void BatchProcessorEngine::notifyJobCompleted(int jobIndex, const BatchJobResult& result)
{
    // Notify on message thread
    juce::MessageManager::callAsync([this, jobIndex, result]()
    {
        m_listeners.call([=](BatchProcessorListener& l)
        {
            l.jobCompleted(jobIndex, result);
        });
    });
}

void BatchProcessorEngine::notifyBatchCompleted()
{
    bool cancelled = m_cancelled.load();
    int successCount = m_summary.completedFiles;
    int failedCount = m_summary.failedFiles;
    int skippedCount = m_summary.skippedFiles;

    // Notify on message thread
    juce::MessageManager::callAsync([this, cancelled, successCount, failedCount, skippedCount]()
    {
        m_listeners.call([=](BatchProcessorListener& l)
        {
            l.batchCompleted(cancelled, successCount, failedCount, skippedCount);
        });
    });
}

} // namespace waveedit
