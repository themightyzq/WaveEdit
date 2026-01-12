/*
  ==============================================================================

    BatchProcessorSettings.h
    Created: 2025
    Author:  ZQ SFX

    Settings and configuration for batch processing operations.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

namespace waveedit
{

/**
 * @brief Error handling strategy for batch processing
 */
enum class BatchErrorHandling
{
    STOP_ON_ERROR,      ///< Stop entire batch on first error
    CONTINUE_ON_ERROR,  ///< Continue processing remaining files
    SKIP_AND_LOG        ///< Skip failed files and log errors
};

/**
 * @brief Output naming pattern tokens
 */
struct BatchNamingTokens
{
    static constexpr const char* FILENAME = "{filename}";     ///< Original filename without extension
    static constexpr const char* EXT = "{ext}";               ///< Original extension
    static constexpr const char* DATE = "{date}";             ///< Current date (YYYY-MM-DD)
    static constexpr const char* TIME = "{time}";             ///< Current time (HH-MM-SS)
    static constexpr const char* INDEX = "{index}";           ///< File index (1, 2, 3...)
    static constexpr const char* INDEX_PADDED = "{index:03}"; ///< Zero-padded index (001, 002...)
    static constexpr const char* PRESET = "{preset}";         ///< Batch preset name
};

/**
 * @brief DSP operation types for batch processing
 */
enum class BatchDSPOperation
{
    NONE,
    GAIN,
    NORMALIZE,
    DC_OFFSET,
    FADE_IN,
    FADE_OUT,
    PARAMETRIC_EQ,
    GRAPHICAL_EQ
};

/**
 * @brief Settings for a single DSP operation in the batch chain
 */
struct BatchDSPSettings
{
    BatchDSPOperation operation = BatchDSPOperation::NONE;
    bool enabled = true;

    // Gain settings
    float gainDb = 0.0f;

    // Normalize settings
    float normalizeTargetDb = 0.0f;

    // Fade settings
    float fadeDurationMs = 100.0f;
    int fadeType = 0;  // 0=Linear, 1=Exponential, 2=Logarithmic, 3=S-Curve

    // EQ preset name (for parametric/graphical EQ)
    juce::String eqPresetName;

    // Serialization
    juce::var toVar() const;
    static BatchDSPSettings fromVar(const juce::var& v);
};

/**
 * @brief Output format settings for batch export
 */
struct BatchOutputFormat
{
    juce::String format = "wav";           ///< wav, flac, mp3, ogg
    int sampleRate = 0;                    ///< 0 = keep original
    int bitDepth = 0;                      ///< 0 = keep original
    int mp3Bitrate = 320;                  ///< For MP3 only
    float mp3Quality = 0.0f;               ///< VBR quality (0-10, 0=highest)

    juce::var toVar() const;
    static BatchOutputFormat fromVar(const juce::var& v);
};

/**
 * @brief Complete settings for a batch processing job
 */
class BatchProcessorSettings
{
public:
    BatchProcessorSettings() = default;
    ~BatchProcessorSettings() = default;

    // =========================================================================
    // Input Files
    // =========================================================================

    juce::StringArray inputFiles;

    // =========================================================================
    // Output Settings
    // =========================================================================

    juce::File outputDirectory;
    juce::String outputPattern = "{filename}_processed";
    bool sameAsSource = false;            ///< Output to same folder as source file
    bool createSubfolders = false;
    bool overwriteExisting = false;

    // =========================================================================
    // DSP Chain
    // =========================================================================

    std::vector<BatchDSPSettings> dspChain;

    // =========================================================================
    // Plugin Processing
    // =========================================================================

    bool usePluginChain = false;
    juce::String pluginChainPresetPath;  ///< Path to saved plugin chain preset
    float pluginTailSeconds = 0.0f;      ///< Effect tail duration

    // =========================================================================
    // Output Format
    // =========================================================================

    BatchOutputFormat outputFormat;

    // =========================================================================
    // Error Handling
    // =========================================================================

    BatchErrorHandling errorHandling = BatchErrorHandling::SKIP_AND_LOG;
    int maxRetries = 0;

    // =========================================================================
    // Processing Options
    // =========================================================================

    int threadCount = 1;                  ///< Number of parallel processing threads
    bool preserveMetadata = true;         ///< Copy metadata from source to output

    // =========================================================================
    // Serialization
    // =========================================================================

    /**
     * @brief Serialize settings to JSON
     */
    juce::String toJSON() const;

    /**
     * @brief Deserialize settings from JSON
     */
    static BatchProcessorSettings fromJSON(const juce::String& json);

    /**
     * @brief Save settings to file
     */
    bool saveToFile(const juce::File& file) const;

    /**
     * @brief Load settings from file
     */
    static BatchProcessorSettings loadFromFile(const juce::File& file);

    /**
     * @brief Apply naming pattern to generate output filename
     */
    juce::String applyNamingPattern(const juce::File& inputFile,
                                     int index,
                                     const juce::String& presetName = "") const;

    /**
     * @brief Validate settings and return any error messages
     */
    juce::StringArray validate() const;

    juce::var toVar() const;
    static BatchProcessorSettings fromVar(const juce::var& v);
};

} // namespace waveedit
