/*
  ==============================================================================

    BatchJob.cpp
    Created: 2025
    Author:  ZQ SFX

  ==============================================================================
*/

#include "BatchJob.h"
#include "../Audio/AudioProcessor.h"
#include "../DSP/DynamicParametricEQ.h"
#include "../DSP/EQPresetManager.h"
#include "../Plugins/PluginChain.h"
#include "../Plugins/PluginChainRenderer.h"
#include "../Plugins/PluginPresetManager.h"

namespace waveedit
{

BatchJob::BatchJob(const juce::File& inputFile,
                   const BatchProcessorSettings& settings,
                   int index,
                   const juce::String& presetName)
    : m_inputFile(inputFile)
    , m_settings(settings)
    , m_index(index)
    , m_presetName(presetName)
{
}

juce::File BatchJob::getOutputFile() const
{
    juce::String outputName = m_settings.applyNamingPattern(m_inputFile, m_index, m_presetName);

    // If "Same as Source" is enabled, use input file's parent directory
    if (m_settings.sameAsSource)
        return m_inputFile.getParentDirectory().getChildFile(outputName);

    return m_settings.outputDirectory.getChildFile(outputName);
}

BatchJobResult BatchJob::execute(std::function<bool(float, const juce::String&)> progressCallback)
{
    m_startTime = juce::Time::getCurrentTime();
    m_result = BatchJobResult();
    m_result.inputSizeBytes = m_inputFile.getSize();

    // Default progress callback that does nothing
    auto progress = progressCallback ? progressCallback
                                      : [](float, const juce::String&) { return true; };

    try
    {
        // Phase 1: Load input file (0-20%)
        m_result.status = BatchJobStatus::LOADING;
        if (!loadInputFile(progress))
        {
            if (m_cancelled.load())
            {
                m_result.status = BatchJobStatus::SKIPPED;
                m_result.errorMessage = "Cancelled by user";
            }
            return m_result;
        }

        // Phase 2: Apply DSP chain (20-50%)
        m_result.status = BatchJobStatus::PROCESSING;
        if (!applyDSPChain(progress))
        {
            if (m_cancelled.load())
            {
                m_result.status = BatchJobStatus::SKIPPED;
                m_result.errorMessage = "Cancelled by user";
            }
            return m_result;
        }

        // Phase 3: Apply plugin chain (50-80%)
        if (m_settings.usePluginChain)
        {
            if (!applyPluginChain(progress))
            {
                if (m_cancelled.load())
                {
                    m_result.status = BatchJobStatus::SKIPPED;
                    m_result.errorMessage = "Cancelled by user";
                }
                return m_result;
            }
        }

        // Phase 4: Convert format if needed (80-90%)
        if (!convertFormat(progress))
        {
            if (m_cancelled.load())
            {
                m_result.status = BatchJobStatus::SKIPPED;
                m_result.errorMessage = "Cancelled by user";
            }
            return m_result;
        }

        // Phase 5: Save output file (90-100%)
        m_result.status = BatchJobStatus::SAVING;
        if (!saveOutputFile(progress))
        {
            if (m_cancelled.load())
            {
                m_result.status = BatchJobStatus::SKIPPED;
                m_result.errorMessage = "Cancelled by user";
            }
            return m_result;
        }

        // Success!
        m_result.status = BatchJobStatus::COMPLETED;
        m_result.outputFile = getOutputFile();
        m_result.outputSizeBytes = m_result.outputFile.getSize();
    }
    catch (const std::exception& e)
    {
        m_result.status = BatchJobStatus::FAILED;
        m_result.errorMessage = e.what();
    }

    // Calculate duration
    auto endTime = juce::Time::getCurrentTime();
    m_result.durationSeconds = (endTime - m_startTime).inSeconds();

    return m_result;
}

void BatchJob::cancel()
{
    m_cancelled.store(true);
}

bool BatchJob::loadInputFile(std::function<bool(float, const juce::String&)>& progress)
{
    if (!progress(0.0f, "Loading " + m_inputFile.getFileName()))
        return false;

    if (!m_inputFile.existsAsFile())
    {
        m_result.status = BatchJobStatus::FAILED;
        m_result.errorMessage = "Input file not found: " + m_inputFile.getFullPathName();
        return false;
    }

    // Create audio format reader
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(m_inputFile));

    if (!reader)
    {
        m_result.status = BatchJobStatus::FAILED;
        m_result.errorMessage = "Cannot read audio format: " + m_inputFile.getFullPathName();
        return false;
    }

    // Store metadata
    m_sampleRate = reader->sampleRate;
    m_numChannels = static_cast<int>(reader->numChannels);

    // Allocate buffer
    auto numSamples = static_cast<int>(reader->lengthInSamples);
    m_buffer.setSize(m_numChannels, numSamples);

    // Read audio data
    if (!reader->read(&m_buffer, 0, numSamples, 0, true, true))
    {
        m_result.status = BatchJobStatus::FAILED;
        m_result.errorMessage = "Failed to read audio data from: " + m_inputFile.getFullPathName();
        return false;
    }

    if (!progress(0.2f, "Loaded " + m_inputFile.getFileName()))
        return false;

    return true;
}

bool BatchJob::applyDSPChain(std::function<bool(float, const juce::String&)>& progress)
{
    if (m_settings.dspChain.empty())
    {
        if (!progress(0.5f, "No DSP operations"))
            return false;
        return true;
    }

    float progressPerOp = 0.3f / static_cast<float>(m_settings.dspChain.size());
    float currentProgress = 0.2f;

    for (const auto& dsp : m_settings.dspChain)
    {
        if (m_cancelled.load())
            return false;

        if (!dsp.enabled)
            continue;

        switch (dsp.operation)
        {
            case BatchDSPOperation::GAIN:
                if (!progress(currentProgress, "Applying gain..."))
                    return false;
                applyGain(dsp.gainDb);
                break;

            case BatchDSPOperation::NORMALIZE:
                if (!progress(currentProgress, "Normalizing..."))
                    return false;
                applyNormalize(dsp.normalizeTargetDb);
                break;

            case BatchDSPOperation::DC_OFFSET:
                if (!progress(currentProgress, "Removing DC offset..."))
                    return false;
                applyDCOffset();
                break;

            case BatchDSPOperation::FADE_IN:
                if (!progress(currentProgress, "Applying fade in..."))
                    return false;
                applyFadeIn(dsp.fadeDurationMs, dsp.fadeType);
                break;

            case BatchDSPOperation::FADE_OUT:
                if (!progress(currentProgress, "Applying fade out..."))
                    return false;
                applyFadeOut(dsp.fadeDurationMs, dsp.fadeType);
                break;

            case BatchDSPOperation::PARAMETRIC_EQ:
            case BatchDSPOperation::GRAPHICAL_EQ:
                if (!progress(currentProgress, "Applying EQ..."))
                    return false;
                applyEQPreset(dsp.eqPresetName);
                break;

            case BatchDSPOperation::REVERSE:
                if (!progress(currentProgress, "Reversing..."))
                    return false;
                AudioProcessor::reverse(m_buffer);
                break;

            case BatchDSPOperation::INVERT:
                if (!progress(currentProgress, "Inverting..."))
                    return false;
                AudioProcessor::invert(m_buffer);
                break;

            case BatchDSPOperation::NONE:
            default:
                break;
        }

        currentProgress += progressPerOp;
    }

    if (!progress(0.5f, "DSP chain complete"))
        return false;

    return true;
}

bool BatchJob::applyPluginChain(std::function<bool(float, const juce::String&)>& progress)
{
    if (m_settings.pluginChainPresetPath.isEmpty())
    {
        if (!progress(0.8f, "No plugin chain configured"))
            return false;
        return true;
    }

    if (!progress(0.5f, "Processing with plugin chain..."))
        return false;

    // Load the plugin chain from the preset file.
    // Held by shared_ptr because the message-thread lambda below co-owns it: if
    // this worker times out or is cancelled and returns early, the chain must
    // stay alive until the (possibly still-queued) lambda runs, otherwise it
    // would dereference a destroyed stack object.
    auto chain = std::make_shared<PluginChain>();
    juce::File presetFile(m_settings.pluginChainPresetPath);

    bool loaded = false;
    if (presetFile.existsAsFile())
    {
        loaded = PluginPresetManager::importPreset(*chain, presetFile);
    }
    else
    {
        // Try loading as a preset name
        loaded = PluginPresetManager::loadPreset(*chain, m_settings.pluginChainPresetPath);
    }

    if (!loaded || chain->isEmpty())
    {
        DBG("BatchJob: Failed to load plugin chain preset: " + m_settings.pluginChainPresetPath);
        if (!progress(0.8f, "Plugin chain not found, skipping..."))
            return false;
        return true;
    }

    if (!progress(0.55f, "Initializing plugins..."))
        return false;

    // Plugin chain processing requires message thread coordination.
    // We use MessageManager to create the offline chain on the message thread,
    // then process on this background thread.
    PluginChainRenderer renderer;

    // All cross-thread state is shared_ptr-owned so the async lambda remains
    // valid even if this worker returns before the lambda executes (timeout /
    // cancel). No raw references to this function's stack escape to the lambda.
    auto chainFailed = std::make_shared<std::atomic<bool>>(false);
    auto offlineChain = std::make_shared<PluginChainRenderer::OfflineChain>();

    // Signalled by the message-thread job once chain creation is done (success
    // or failure). Using a WaitableEvent instead of a sleep-spin lets this
    // worker block efficiently and wake the instant the chain is ready.
    auto chainReady = std::make_shared<juce::WaitableEvent>();

    juce::MessageManager::callAsync([chain, chainReady, chainFailed, offlineChain,
                                      sampleRate = m_sampleRate, blockSize = renderer.getBlockSize()]()
    {
        *offlineChain = PluginChainRenderer::createOfflineChain(*chain, sampleRate, blockSize);
        if (!offlineChain->isValid())
            chainFailed->store(true);
        chainReady->signal();
    });

    // Wait for chain creation. Poll in short slices so we stay responsive to
    // cancellation; bail out promptly if the job was cancelled. A bounded total
    // timeout prevents a permanent stall if the message thread is wedged.
    constexpr int kMaxWaitMs = 30000;   // 30 second overall ceiling
    constexpr int kSliceMs = 50;        // wake-up granularity for cancel checks
    int waitedMs = 0;
    bool ready = false;
    while (waitedMs < kMaxWaitMs)
    {
        if (m_cancelled.load())
            return false;

        if (chainReady->wait(kSliceMs))
        {
            ready = true;
            break;
        }
        waitedMs += kSliceMs;
    }

    if (m_cancelled.load())
        return false;

    if (!ready)
    {
        // Timed out waiting for the message thread to build the chain. Report
        // this as a failure (do NOT silently pretend the chain was applied) so
        // the batch summary reflects that plugin processing did not happen.
        DBG("BatchJob: Timed out waiting for plugin chain creation");
        m_result.status = BatchJobStatus::FAILED;
        m_result.errorMessage = "Plugin chain creation timed out";
        progress(0.8f, "Plugin chain timed out");
        return false;
    }

    if (chainFailed->load() || !offlineChain->isValid())
    {
        DBG("BatchJob: Failed to create offline plugin instances");
        m_result.status = BatchJobStatus::FAILED;
        m_result.errorMessage = "Plugin instantiation failed";
        progress(0.8f, "Plugin instantiation failed");
        return false;
    }

    if (!progress(0.6f, "Rendering through plugins..."))
        return false;

    // Calculate tail samples from settings
    int tailSamples = static_cast<int>(m_settings.pluginTailSeconds * m_sampleRate);

    // Render the entire buffer through the plugin chain
    auto result = renderer.renderWithOfflineChain(
        m_buffer,
        *offlineChain,
        m_sampleRate,
        0,
        m_buffer.getNumSamples(),
        [&progress, this](float p, const juce::String& msg) -> bool {
            if (m_cancelled.load())
                return false;
            // Map 0.6-0.8 progress range for plugin rendering
            float mappedProgress = 0.6f + (p * 0.2f);
            return progress(mappedProgress, msg);
        },
        0,  // outputChannels = match source
        tailSamples
    );

    if (result.cancelled)
        return false;

    if (result.success)
    {
        // Copy processed audio back to our buffer. When the user requested an
        // effect tail, result.processedBuffer is (numSamples + tailSamples) long.
        // The old code clamped the copy to the ORIGINAL m_buffer length, silently
        // discarding the reverb/delay tail (H25). Resize m_buffer to the full
        // processed length first so the requested tail is preserved.
        const int processedSamples = result.processedBuffer.getNumSamples();
        const int resultChannels   = result.processedBuffer.getNumChannels();

        if (processedSamples > m_buffer.getNumSamples())
        {
            // keepExistingContent=true preserves any channels we don't overwrite;
            // clearExtraSpace=true zeroes the newly grown region before we copy.
            m_buffer.setSize(m_numChannels, processedSamples, true, true, true);
        }

        const int channelsToCopy = juce::jmin(m_numChannels, resultChannels);
        for (int channel = 0; channel < channelsToCopy; ++channel)
        {
            m_buffer.copyFrom(channel, 0, result.processedBuffer, channel, 0, processedSamples);
        }
    }
    else if (!result.errorMessage.isEmpty())
    {
        DBG("BatchJob: Plugin chain error: " + result.errorMessage);
    }

    if (!progress(0.8f, "Plugin chain complete"))
        return false;

    return true;
}

bool BatchJob::convertFormat(std::function<bool(float, const juce::String&)>& progress)
{
    const auto& fmt = m_settings.outputFormat;

    // Check if sample rate conversion is needed
    if (fmt.sampleRate > 0 && static_cast<int>(m_sampleRate) != fmt.sampleRate)
    {
        if (!progress(0.8f, "Converting sample rate..."))
            return false;

        // Create resampler
        juce::LagrangeInterpolator interpolator;

        double ratio = static_cast<double>(fmt.sampleRate) / m_sampleRate;
        int newNumSamples = static_cast<int>(std::ceil(m_buffer.getNumSamples() * ratio));

        juce::AudioBuffer<float> resampledBuffer(m_numChannels, newNumSamples);

        for (int channel = 0; channel < m_numChannels; ++channel)
        {
            interpolator.reset();
            int outputSamples = interpolator.process(
                ratio,
                m_buffer.getReadPointer(channel),
                resampledBuffer.getWritePointer(channel),
                newNumSamples);

            // Handle any difference in output length
            if (outputSamples < newNumSamples)
                resampledBuffer.clear(channel, outputSamples, newNumSamples - outputSamples);
        }

        m_buffer = std::move(resampledBuffer);
        m_sampleRate = fmt.sampleRate;
    }

    if (!progress(0.9f, "Format conversion complete"))
        return false;

    return true;
}

bool BatchJob::saveOutputFile(std::function<bool(float, const juce::String&)>& progress)
{
    juce::File outputFile = getOutputFile();

    if (!progress(0.9f, "Saving " + outputFile.getFileName()))
        return false;

    // Create output directory if needed
    if (!outputFile.getParentDirectory().exists())
    {
        if (!outputFile.getParentDirectory().createDirectory())
        {
            m_result.status = BatchJobStatus::FAILED;
            m_result.errorMessage = "Cannot create output directory: " +
                                    outputFile.getParentDirectory().getFullPathName();
            return false;
        }
    }

    // Check for overwrite
    if (outputFile.existsAsFile() && !m_settings.overwriteExisting)
    {
        m_result.status = BatchJobStatus::FAILED;
        m_result.errorMessage = "Output file already exists: " + outputFile.getFullPathName();
        return false;
    }

    // Create writer
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormat> format;
    int bitsPerSample = m_settings.outputFormat.bitDepth > 0
                            ? m_settings.outputFormat.bitDepth
                            : 16;

    juce::String ext = outputFile.getFileExtension().toLowerCase();

    if (ext == ".wav")
    {
        format = std::make_unique<juce::WavAudioFormat>();
    }
    else if (ext == ".flac")
    {
        format = std::make_unique<juce::FlacAudioFormat>();
    }
    else if (ext == ".ogg")
    {
        format = std::make_unique<juce::OggVorbisAudioFormat>();
    }
    else
    {
        // Default to WAV
        format = std::make_unique<juce::WavAudioFormat>();
    }

    std::unique_ptr<juce::OutputStream> outputStream(outputFile.createOutputStream());

    if (!outputStream)
    {
        m_result.status = BatchJobStatus::FAILED;
        m_result.errorMessage = "Cannot create output file: " + outputFile.getFullPathName();
        return false;
    }

    // JUCE 8 API: takes the unique_ptr by reference and only assumes
    // ownership of the stream when writer creation succeeds
    auto writer = format->createWriterFor(outputStream,
                                          juce::AudioFormatWriterOptions()
                                              .withSampleRate(m_sampleRate)
                                              .withNumChannels(m_numChannels)
                                              .withBitsPerSample(bitsPerSample));

    if (!writer)
    {
        m_result.status = BatchJobStatus::FAILED;
        m_result.errorMessage = "Cannot create audio writer for: " + outputFile.getFullPathName();
        return false;
    }

    // Write audio data
    if (!writer->writeFromAudioSampleBuffer(m_buffer, 0, m_buffer.getNumSamples()))
    {
        m_result.status = BatchJobStatus::FAILED;
        m_result.errorMessage = "Failed to write audio data to: " + outputFile.getFullPathName();
        return false;
    }

    if (!progress(1.0f, "Saved " + outputFile.getFileName()))
        return false;

    return true;
}

// =============================================================================
// DSP Operations
// =============================================================================

void BatchJob::applyGain(float gainDb)
{
    float gainLinear = juce::Decibels::decibelsToGain(gainDb);
    m_buffer.applyGain(gainLinear);
}

void BatchJob::applyNormalize(float targetDb)
{
    float targetLinear = juce::Decibels::decibelsToGain(targetDb);

    // Find peak
    float peak = 0.0f;
    for (int channel = 0; channel < m_numChannels; ++channel)
    {
        auto range = m_buffer.findMinMax(channel, 0, m_buffer.getNumSamples());
        peak = std::max(peak, std::max(std::abs(range.getStart()), std::abs(range.getEnd())));
    }

    // Apply normalization
    if (peak > 0.0f)
    {
        float gainToApply = targetLinear / peak;
        m_buffer.applyGain(gainToApply);
    }
}

void BatchJob::applyDCOffset()
{
    for (int channel = 0; channel < m_numChannels; ++channel)
    {
        // Calculate DC offset
        float sum = 0.0f;
        const float* data = m_buffer.getReadPointer(channel);
        int numSamples = m_buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
            sum += data[i];

        float dcOffset = sum / static_cast<float>(numSamples);

        // Remove DC offset
        float* writeData = m_buffer.getWritePointer(channel);
        for (int i = 0; i < numSamples; ++i)
            writeData[i] -= dcOffset;
    }
}

void BatchJob::applyFadeIn(float durationMs, int curveType)
{
    int fadeSamples = static_cast<int>((durationMs / 1000.0) * m_sampleRate);
    fadeSamples = std::min(fadeSamples, m_buffer.getNumSamples());

    for (int channel = 0; channel < m_numChannels; ++channel)
    {
        float* data = m_buffer.getWritePointer(channel);

        for (int i = 0; i < fadeSamples; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(fadeSamples);
            float gain;

            switch (curveType)
            {
                case 1: // Exponential
                    gain = t * t;
                    break;
                case 2: // Logarithmic
                    gain = std::sqrt(t);
                    break;
                case 3: // S-Curve
                    gain = 0.5f * (1.0f - std::cos(t * juce::MathConstants<float>::pi));
                    break;
                case 0: // Linear
                default:
                    gain = t;
                    break;
            }

            data[i] *= gain;
        }
    }
}

void BatchJob::applyFadeOut(float durationMs, int curveType)
{
    int fadeSamples = static_cast<int>((durationMs / 1000.0) * m_sampleRate);
    fadeSamples = std::min(fadeSamples, m_buffer.getNumSamples());

    int startSample = m_buffer.getNumSamples() - fadeSamples;

    for (int channel = 0; channel < m_numChannels; ++channel)
    {
        float* data = m_buffer.getWritePointer(channel);

        for (int i = 0; i < fadeSamples; ++i)
        {
            float t = static_cast<float>(i) / static_cast<float>(fadeSamples);
            float gain;

            switch (curveType)
            {
                case 1: // Exponential
                    gain = (1.0f - t) * (1.0f - t);
                    break;
                case 2: // Logarithmic
                    gain = std::sqrt(1.0f - t);
                    break;
                case 3: // S-Curve
                    gain = 0.5f * (1.0f + std::cos(t * juce::MathConstants<float>::pi));
                    break;
                case 0: // Linear
                default:
                    gain = 1.0f - t;
                    break;
            }

            data[startSample + i] *= gain;
        }
    }
}

void BatchJob::applyEQPreset(const juce::String& presetName)
{
    if (presetName.isEmpty())
        return;

    // Load EQ parameters from preset
    DynamicParametricEQ::Parameters params;

    // Try loading as user preset first, then as factory preset
    if (!EQPresetManager::loadPreset(params, presetName))
    {
        // Try factory preset
        if (EQPresetManager::isFactoryPreset(presetName))
        {
            params = EQPresetManager::getFactoryPreset(presetName);
        }
        else
        {
            DBG("BatchJob: Failed to load EQ preset: " + presetName);
            return;
        }
    }

    // Skip if no bands to apply
    if (params.bands.empty() && params.outputGain == 0.0f)
        return;

    // Create and prepare EQ processor
    DynamicParametricEQ eq;
    eq.prepare(m_sampleRate, m_buffer.getNumSamples());
    eq.setParameters(params);

    // Apply EQ to the buffer
    eq.applyEQ(m_buffer);
}

} // namespace waveedit
