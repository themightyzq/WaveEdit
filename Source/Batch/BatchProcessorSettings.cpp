/*
  ==============================================================================

    BatchProcessorSettings.cpp
    Created: 2025
    Author:  ZQ SFX

  ==============================================================================
*/

#include "BatchProcessorSettings.h"

namespace waveedit
{

// =============================================================================
// BatchDSPSettings
// =============================================================================

juce::var BatchDSPSettings::toVar() const
{
    auto obj = new juce::DynamicObject();
    obj->setProperty("operation", static_cast<int>(operation));
    obj->setProperty("enabled", enabled);
    obj->setProperty("gainDb", gainDb);
    obj->setProperty("normalizeTargetDb", normalizeTargetDb);
    obj->setProperty("fadeDurationMs", fadeDurationMs);
    obj->setProperty("fadeType", fadeType);
    obj->setProperty("eqPresetName", eqPresetName);
    return juce::var(obj);
}

BatchDSPSettings BatchDSPSettings::fromVar(const juce::var& v)
{
    BatchDSPSettings settings;
    if (auto* obj = v.getDynamicObject())
    {
        settings.operation = static_cast<BatchDSPOperation>(static_cast<int>(obj->getProperty("operation")));
        settings.enabled = obj->getProperty("enabled");
        settings.gainDb = obj->getProperty("gainDb");
        settings.normalizeTargetDb = obj->getProperty("normalizeTargetDb");
        settings.fadeDurationMs = obj->getProperty("fadeDurationMs");
        settings.fadeType = obj->getProperty("fadeType");
        settings.eqPresetName = obj->getProperty("eqPresetName").toString();
    }
    return settings;
}

// =============================================================================
// BatchOutputFormat
// =============================================================================

juce::var BatchOutputFormat::toVar() const
{
    auto obj = new juce::DynamicObject();
    obj->setProperty("format", format);
    obj->setProperty("sampleRate", sampleRate);
    obj->setProperty("bitDepth", bitDepth);
    obj->setProperty("mp3Bitrate", mp3Bitrate);
    obj->setProperty("mp3Quality", mp3Quality);
    return juce::var(obj);
}

BatchOutputFormat BatchOutputFormat::fromVar(const juce::var& v)
{
    BatchOutputFormat fmt;
    if (auto* obj = v.getDynamicObject())
    {
        fmt.format = obj->getProperty("format").toString();
        fmt.sampleRate = obj->getProperty("sampleRate");
        fmt.bitDepth = obj->getProperty("bitDepth");
        fmt.mp3Bitrate = obj->getProperty("mp3Bitrate");
        fmt.mp3Quality = obj->getProperty("mp3Quality");
    }
    return fmt;
}

// =============================================================================
// BatchProcessorSettings
// =============================================================================

juce::var BatchProcessorSettings::toVar() const
{
    auto obj = new juce::DynamicObject();

    // Input files
    juce::Array<juce::var> filesArray;
    for (const auto& f : inputFiles)
        filesArray.add(f);
    obj->setProperty("inputFiles", filesArray);

    // Output settings
    obj->setProperty("outputDirectory", outputDirectory.getFullPathName());
    obj->setProperty("outputPattern", outputPattern);
    obj->setProperty("sameAsSource", sameAsSource);
    obj->setProperty("createSubfolders", createSubfolders);
    obj->setProperty("overwriteExisting", overwriteExisting);

    // DSP chain
    juce::Array<juce::var> dspArray;
    for (const auto& dsp : dspChain)
        dspArray.add(dsp.toVar());
    obj->setProperty("dspChain", dspArray);

    // Plugin processing
    obj->setProperty("usePluginChain", usePluginChain);
    obj->setProperty("pluginChainPresetPath", pluginChainPresetPath);
    obj->setProperty("pluginTailSeconds", pluginTailSeconds);

    // Output format
    obj->setProperty("outputFormat", outputFormat.toVar());

    // Error handling
    obj->setProperty("errorHandling", static_cast<int>(errorHandling));
    obj->setProperty("maxRetries", maxRetries);

    // Processing options
    obj->setProperty("threadCount", threadCount);
    obj->setProperty("preserveMetadata", preserveMetadata);

    return juce::var(obj);
}

BatchProcessorSettings BatchProcessorSettings::fromVar(const juce::var& v)
{
    BatchProcessorSettings settings;

    if (auto* obj = v.getDynamicObject())
    {
        // Input files
        if (auto* filesArray = obj->getProperty("inputFiles").getArray())
        {
            for (const auto& f : *filesArray)
                settings.inputFiles.add(f.toString());
        }

        // Output settings
        settings.outputDirectory = juce::File(obj->getProperty("outputDirectory").toString());
        settings.outputPattern = obj->getProperty("outputPattern").toString();
        settings.sameAsSource = obj->getProperty("sameAsSource");
        settings.createSubfolders = obj->getProperty("createSubfolders");
        settings.overwriteExisting = obj->getProperty("overwriteExisting");

        // DSP chain
        if (auto* dspArray = obj->getProperty("dspChain").getArray())
        {
            for (const auto& dsp : *dspArray)
                settings.dspChain.push_back(BatchDSPSettings::fromVar(dsp));
        }

        // Plugin processing
        settings.usePluginChain = obj->getProperty("usePluginChain");
        settings.pluginChainPresetPath = obj->getProperty("pluginChainPresetPath").toString();
        settings.pluginTailSeconds = obj->getProperty("pluginTailSeconds");

        // Output format
        settings.outputFormat = BatchOutputFormat::fromVar(obj->getProperty("outputFormat"));

        // Error handling
        settings.errorHandling = static_cast<BatchErrorHandling>(
            static_cast<int>(obj->getProperty("errorHandling")));
        settings.maxRetries = obj->getProperty("maxRetries");

        // Processing options
        settings.threadCount = obj->getProperty("threadCount");
        settings.preserveMetadata = obj->getProperty("preserveMetadata");
    }

    return settings;
}

juce::String BatchProcessorSettings::toJSON() const
{
    return juce::JSON::toString(toVar(), true);
}

BatchProcessorSettings BatchProcessorSettings::fromJSON(const juce::String& json)
{
    auto parsed = juce::JSON::parse(json);
    return fromVar(parsed);
}

bool BatchProcessorSettings::saveToFile(const juce::File& file) const
{
    return file.replaceWithText(toJSON());
}

BatchProcessorSettings BatchProcessorSettings::loadFromFile(const juce::File& file)
{
    if (file.existsAsFile())
        return fromJSON(file.loadFileAsString());
    return BatchProcessorSettings();
}

juce::String BatchProcessorSettings::applyNamingPattern(const juce::File& inputFile,
                                                         int index,
                                                         const juce::String& presetName) const
{
    juce::String result = outputPattern;

    // Get current date/time
    auto now = juce::Time::getCurrentTime();

    // Replace tokens
    result = result.replace(BatchNamingTokens::FILENAME,
                            inputFile.getFileNameWithoutExtension());
    result = result.replace(BatchNamingTokens::EXT,
                            inputFile.getFileExtension().trimCharactersAtStart("."));
    result = result.replace(BatchNamingTokens::DATE,
                            now.formatted("%Y-%m-%d"));
    result = result.replace(BatchNamingTokens::TIME,
                            now.formatted("%H-%M-%S"));
    result = result.replace(BatchNamingTokens::INDEX_PADDED,
                            juce::String(index).paddedLeft('0', 3));
    result = result.replace(BatchNamingTokens::INDEX,
                            juce::String(index));
    result = result.replace(BatchNamingTokens::PRESET,
                            presetName.isEmpty() ? "batch" : presetName);

    // Add output extension
    juce::String ext = outputFormat.format;
    if (ext.isEmpty() || ext == "wav")
        ext = "wav";

    return result + "." + ext;
}

juce::StringArray BatchProcessorSettings::validate() const
{
    juce::StringArray errors;

    // Check input files
    if (inputFiles.isEmpty())
        errors.add("No input files specified");
    else
    {
        for (const auto& path : inputFiles)
        {
            juce::File f(path);
            if (!f.existsAsFile())
                errors.add("Input file not found: " + path);
        }
    }

    // Check output directory
    if (!outputDirectory.exists() && !outputDirectory.getFullPathName().isEmpty())
    {
        // Try to create it
        if (!outputDirectory.createDirectory())
            errors.add("Cannot create output directory: " + outputDirectory.getFullPathName());
    }

    // Check output pattern
    if (outputPattern.isEmpty())
        errors.add("Output naming pattern is empty");

    // Check thread count
    if (threadCount < 1)
        errors.add("Thread count must be at least 1");
    else if (threadCount > juce::SystemStats::getNumCpus() * 2)
        errors.add("Thread count exceeds recommended limit");

    // Check plugin chain preset if enabled
    if (usePluginChain && !pluginChainPresetPath.isEmpty())
    {
        juce::File presetFile(pluginChainPresetPath);
        if (!presetFile.existsAsFile())
            errors.add("Plugin chain preset not found: " + pluginChainPresetPath);
    }

    return errors;
}

} // namespace waveedit
