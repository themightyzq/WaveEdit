/*
  ==============================================================================

    BatchJob.h
    Created: 2025
    Author:  ZQ SFX

    Represents a single file processing job in the batch.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "BatchProcessorSettings.h"

namespace waveedit
{

/**
 * @brief Status of a batch job
 */
enum class BatchJobStatus
{
    PENDING,      ///< Not started
    LOADING,      ///< Loading input file
    PROCESSING,   ///< Running DSP/plugin chain
    SAVING,       ///< Writing output file
    COMPLETED,    ///< Successfully completed
    FAILED,       ///< Failed with error
    SKIPPED       ///< Skipped (e.g., due to error handling policy)
};

/**
 * @brief Result of a batch job
 */
struct BatchJobResult
{
    BatchJobStatus status = BatchJobStatus::PENDING;
    juce::String errorMessage;
    juce::File outputFile;
    double durationSeconds = 0.0;
    juce::int64 inputSizeBytes = 0;
    juce::int64 outputSizeBytes = 0;
};

/**
 * @brief Represents a single file to be processed in the batch
 */
class BatchJob
{
public:
    /**
     * @brief Construct a batch job
     * @param inputFile Path to the input audio file
     * @param settings Shared settings for this batch
     * @param index Index of this job in the batch (1-based)
     * @param presetName Name of the preset being used (for output naming)
     */
    BatchJob(const juce::File& inputFile,
             const BatchProcessorSettings& settings,
             int index,
             const juce::String& presetName = "");

    ~BatchJob() = default;

    // =========================================================================
    // Execution
    // =========================================================================

    /**
     * @brief Execute this job
     * @param progressCallback Called with (0.0-1.0, status message). Return false to cancel.
     * @return The result of the job
     */
    BatchJobResult execute(std::function<bool(float, const juce::String&)> progressCallback = nullptr);

    /**
     * @brief Cancel this job (if running)
     */
    void cancel();

    /**
     * @brief Check if job was cancelled
     */
    bool wasCancelled() const { return m_cancelled.load(); }

    // =========================================================================
    // Accessors
    // =========================================================================

    const juce::File& getInputFile() const { return m_inputFile; }
    juce::File getOutputFile() const;
    int getIndex() const { return m_index; }
    BatchJobStatus getStatus() const { return m_result.status; }
    const BatchJobResult& getResult() const { return m_result; }

private:
    // =========================================================================
    // Processing phases
    // =========================================================================

    bool loadInputFile(std::function<bool(float, const juce::String&)>& progress);
    bool applyDSPChain(std::function<bool(float, const juce::String&)>& progress);
    bool applyPluginChain(std::function<bool(float, const juce::String&)>& progress);
    bool convertFormat(std::function<bool(float, const juce::String&)>& progress);
    bool saveOutputFile(std::function<bool(float, const juce::String&)>& progress);

    // =========================================================================
    // DSP Operations
    // =========================================================================

    void applyGain(float gainDb);
    void applyNormalize(float targetDb);
    void applyDCOffset();
    void applyFadeIn(float durationMs, int curveType);
    void applyFadeOut(float durationMs, int curveType);
    void applyEQPreset(const juce::String& presetName);

    // =========================================================================
    // Member variables
    // =========================================================================

    juce::File m_inputFile;
    BatchProcessorSettings m_settings;
    int m_index;
    juce::String m_presetName;

    // Audio data
    juce::AudioBuffer<float> m_buffer;
    double m_sampleRate = 44100.0;
    int m_numChannels = 2;

    // State
    std::atomic<bool> m_cancelled{false};
    BatchJobResult m_result;
    juce::Time m_startTime;
};

} // namespace waveedit
