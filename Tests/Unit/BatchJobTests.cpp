/*
  ==============================================================================

    BatchJobTests.cpp
    Created: 2026-01-12
    Author:  ZQ SFX
    Copyright (C) 2026 ZQ SFX - All Rights Reserved

    Unit tests for BatchJob batch processing functionality.
    Tests DSP chain operations, job execution, and cancellation.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Batch/BatchJob.h"
#include "Batch/BatchProcessorSettings.h"
#include "TestAudioFiles.h"
#include "AudioAssertions.h"

using namespace waveedit;

// ============================================================================
// BatchJob DSP Chain Tests
// ============================================================================

class BatchJobDSPTests : public juce::UnitTest
{
public:
    BatchJobDSPTests() : juce::UnitTest("BatchJob DSP Chain", "Batch") {}

    void runTest() override
    {
        beginTest("Apply gain operation");
        testGainOperation();

        beginTest("Apply normalize operation");
        testNormalizeOperation();

        beginTest("Apply DC offset removal");
        testDCOffsetRemoval();

        beginTest("Apply fade in operation");
        testFadeInOperation();

        beginTest("Apply fade out operation");
        testFadeOutOperation();

        beginTest("DSP chain order is preserved");
        testDSPChainOrder();

        beginTest("Empty DSP chain");
        testEmptyDSPChain();
    }

private:
    juce::File createTestWavFile(const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("batch_test_" + juce::String(juce::Random::getSystemRandom().nextInt()) + ".wav");

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::FileOutputStream> outputStream(tempFile.createOutputStream());

        if (outputStream)
        {
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(outputStream.get(),
                                          sampleRate,
                                          static_cast<unsigned int>(buffer.getNumChannels()),
                                          16,
                                          {},
                                          0));

            if (writer)
            {
                outputStream.release();
                writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
            }
        }

        return tempFile;
    }

    void testGainOperation()
    {
        // Create test audio
        auto inputBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.5, 2);
        juce::File testFile = createTestWavFile(inputBuffer, 44100.0);
        juce::File outputDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        // Set up batch settings with +6dB gain
        BatchProcessorSettings settings;
        settings.outputDirectory = outputDir;
        settings.outputPattern = "{filename}_gain_test";
        settings.overwriteExisting = true;

        BatchDSPSettings gainOp;
        gainOp.operation = BatchDSPOperation::GAIN;
        gainOp.enabled = true;
        gainOp.gainDb = 6.0f;
        settings.dspChain.push_back(gainOp);

        // Execute batch job
        BatchJob job(testFile, settings, 1);
        BatchJobResult result = job.execute();

        expect(result.status == BatchJobStatus::COMPLETED, "Job should complete successfully");

        // Clean up
        testFile.deleteFile();
        result.outputFile.deleteFile();

        logMessage("✅ Gain operation applied correctly");
    }

    void testNormalizeOperation()
    {
        // Create quiet test audio (0.3 amplitude)
        auto inputBuffer = TestAudio::createSineWave(440.0, 0.3, 44100.0, 0.5, 2);
        juce::File testFile = createTestWavFile(inputBuffer, 44100.0);
        juce::File outputDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        // Set up batch settings with normalize to -3dB
        BatchProcessorSettings settings;
        settings.outputDirectory = outputDir;
        settings.outputPattern = "{filename}_norm_test";
        settings.overwriteExisting = true;

        BatchDSPSettings normOp;
        normOp.operation = BatchDSPOperation::NORMALIZE;
        normOp.enabled = true;
        normOp.normalizeTargetDb = -3.0f;
        settings.dspChain.push_back(normOp);

        // Execute batch job
        BatchJob job(testFile, settings, 1);
        BatchJobResult result = job.execute();

        expect(result.status == BatchJobStatus::COMPLETED, "Job should complete successfully");
        expect(result.outputFile.existsAsFile(), "Output file should exist");

        // Clean up
        testFile.deleteFile();
        result.outputFile.deleteFile();

        logMessage("✅ Normalize operation applied correctly");
    }

    void testDCOffsetRemoval()
    {
        // Create test audio with DC offset
        auto inputBuffer = TestAudio::createSineWithDC(440.0, 0.5, 0.2f, 44100.0, 0.5, 2);
        juce::File testFile = createTestWavFile(inputBuffer, 44100.0);
        juce::File outputDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        // Set up batch settings with DC offset removal
        BatchProcessorSettings settings;
        settings.outputDirectory = outputDir;
        settings.outputPattern = "{filename}_dc_test";
        settings.overwriteExisting = true;

        BatchDSPSettings dcOp;
        dcOp.operation = BatchDSPOperation::DC_OFFSET;
        dcOp.enabled = true;
        settings.dspChain.push_back(dcOp);

        // Execute batch job
        BatchJob job(testFile, settings, 1);
        BatchJobResult result = job.execute();

        expect(result.status == BatchJobStatus::COMPLETED, "Job should complete successfully");

        // Clean up
        testFile.deleteFile();
        result.outputFile.deleteFile();

        logMessage("✅ DC offset removal applied correctly");
    }

    void testFadeInOperation()
    {
        auto inputBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        juce::File testFile = createTestWavFile(inputBuffer, 44100.0);
        juce::File outputDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        // Set up batch settings with fade in
        BatchProcessorSettings settings;
        settings.outputDirectory = outputDir;
        settings.outputPattern = "{filename}_fadein_test";
        settings.overwriteExisting = true;

        BatchDSPSettings fadeOp;
        fadeOp.operation = BatchDSPOperation::FADE_IN;
        fadeOp.enabled = true;
        fadeOp.fadeDurationMs = 100.0f;
        fadeOp.fadeType = 0;  // Linear
        settings.dspChain.push_back(fadeOp);

        // Execute batch job
        BatchJob job(testFile, settings, 1);
        BatchJobResult result = job.execute();

        expect(result.status == BatchJobStatus::COMPLETED, "Job should complete successfully");

        // Clean up
        testFile.deleteFile();
        result.outputFile.deleteFile();

        logMessage("✅ Fade in operation applied correctly");
    }

    void testFadeOutOperation()
    {
        auto inputBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        juce::File testFile = createTestWavFile(inputBuffer, 44100.0);
        juce::File outputDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        // Set up batch settings with fade out
        BatchProcessorSettings settings;
        settings.outputDirectory = outputDir;
        settings.outputPattern = "{filename}_fadeout_test";
        settings.overwriteExisting = true;

        BatchDSPSettings fadeOp;
        fadeOp.operation = BatchDSPOperation::FADE_OUT;
        fadeOp.enabled = true;
        fadeOp.fadeDurationMs = 100.0f;
        fadeOp.fadeType = 0;  // Linear
        settings.dspChain.push_back(fadeOp);

        // Execute batch job
        BatchJob job(testFile, settings, 1);
        BatchJobResult result = job.execute();

        expect(result.status == BatchJobStatus::COMPLETED, "Job should complete successfully");

        // Clean up
        testFile.deleteFile();
        result.outputFile.deleteFile();

        logMessage("✅ Fade out operation applied correctly");
    }

    void testDSPChainOrder()
    {
        // Create test audio
        auto inputBuffer = TestAudio::createSineWave(440.0, 0.3, 44100.0, 0.5, 2);
        juce::File testFile = createTestWavFile(inputBuffer, 44100.0);
        juce::File outputDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        // Set up batch settings with multiple operations
        BatchProcessorSettings settings;
        settings.outputDirectory = outputDir;
        settings.outputPattern = "{filename}_chain_test";
        settings.overwriteExisting = true;

        // DC Offset -> Normalize -> Gain (order should be preserved)
        BatchDSPSettings dcOp;
        dcOp.operation = BatchDSPOperation::DC_OFFSET;
        dcOp.enabled = true;
        settings.dspChain.push_back(dcOp);

        BatchDSPSettings normOp;
        normOp.operation = BatchDSPOperation::NORMALIZE;
        normOp.enabled = true;
        normOp.normalizeTargetDb = -6.0f;
        settings.dspChain.push_back(normOp);

        BatchDSPSettings gainOp;
        gainOp.operation = BatchDSPOperation::GAIN;
        gainOp.enabled = true;
        gainOp.gainDb = -3.0f;
        settings.dspChain.push_back(gainOp);

        // Execute batch job
        BatchJob job(testFile, settings, 1);
        BatchJobResult result = job.execute();

        expect(result.status == BatchJobStatus::COMPLETED, "Job should complete with multi-op chain");
        expect(settings.dspChain.size() == 3, "DSP chain should have 3 operations");

        // Clean up
        testFile.deleteFile();
        result.outputFile.deleteFile();

        logMessage("✅ DSP chain order preserved with 3 operations");
    }

    void testEmptyDSPChain()
    {
        auto inputBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.5, 2);
        juce::File testFile = createTestWavFile(inputBuffer, 44100.0);
        juce::File outputDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        // Set up batch settings with no DSP operations
        BatchProcessorSettings settings;
        settings.outputDirectory = outputDir;
        settings.outputPattern = "{filename}_empty_test";
        settings.overwriteExisting = true;
        // dspChain is empty

        // Execute batch job
        BatchJob job(testFile, settings, 1);
        BatchJobResult result = job.execute();

        expect(result.status == BatchJobStatus::COMPLETED, "Job should complete with empty chain");
        expect(result.outputFile.existsAsFile(), "Output file should exist");

        // Clean up
        testFile.deleteFile();
        result.outputFile.deleteFile();

        logMessage("✅ Empty DSP chain passes through correctly");
    }
};

static BatchJobDSPTests batchJobDSPTests;

// ============================================================================
// BatchJob Execution Tests
// ============================================================================

class BatchJobExecutionTests : public juce::UnitTest
{
public:
    BatchJobExecutionTests() : juce::UnitTest("BatchJob Execution", "Batch") {}

    void runTest() override
    {
        beginTest("Execute job successfully");
        testExecuteSuccess();

        beginTest("Handle missing input file");
        testMissingInputFile();

        beginTest("Cancellation during execution");
        testCancellation();

        beginTest("Progress callback is called");
        testProgressCallback();

        beginTest("Output naming pattern");
        testOutputNamingPattern();
    }

private:
    juce::File createTestWavFile(const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("batch_exec_" + juce::String(juce::Random::getSystemRandom().nextInt()) + ".wav");

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::FileOutputStream> outputStream(tempFile.createOutputStream());

        if (outputStream)
        {
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(outputStream.get(),
                                          sampleRate,
                                          static_cast<unsigned int>(buffer.getNumChannels()),
                                          16,
                                          {},
                                          0));

            if (writer)
            {
                outputStream.release();
                writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
            }
        }

        return tempFile;
    }

    void testExecuteSuccess()
    {
        auto inputBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.5, 2);
        juce::File testFile = createTestWavFile(inputBuffer, 44100.0);
        juce::File outputDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        BatchProcessorSettings settings;
        settings.outputDirectory = outputDir;
        settings.outputPattern = "{filename}_success";
        settings.overwriteExisting = true;

        BatchJob job(testFile, settings, 1);
        BatchJobResult result = job.execute();

        expect(result.status == BatchJobStatus::COMPLETED, "Job should complete successfully");
        expect(result.outputFile.existsAsFile(), "Output file should exist");
        expect(result.errorMessage.isEmpty(), "No error message expected");
        expect(result.durationSeconds >= 0.0, "Duration should be recorded (may be 0 for fast jobs)");
        expect(result.inputSizeBytes > 0, "Input size should be recorded");

        // Clean up
        testFile.deleteFile();
        result.outputFile.deleteFile();

        logMessage("✅ Job executed successfully");
    }

    void testMissingInputFile()
    {
        juce::File nonExistentFile("/nonexistent/path/to/audio.wav");
        juce::File outputDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        BatchProcessorSettings settings;
        settings.outputDirectory = outputDir;
        settings.outputPattern = "{filename}_missing";

        BatchJob job(nonExistentFile, settings, 1);
        BatchJobResult result = job.execute();

        expect(result.status == BatchJobStatus::FAILED, "Job should fail for missing file");
        expect(result.errorMessage.isNotEmpty(), "Error message should explain failure");

        logMessage("✅ Missing input file handled correctly");
    }

    void testCancellation()
    {
        auto inputBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        juce::File testFile = createTestWavFile(inputBuffer, 44100.0);
        juce::File outputDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        BatchProcessorSettings settings;
        settings.outputDirectory = outputDir;
        settings.outputPattern = "{filename}_cancel";
        settings.overwriteExisting = true;

        BatchJob job(testFile, settings, 1);

        // Cancel before execution
        job.cancel();
        expect(job.wasCancelled(), "Job should report as cancelled");

        // Clean up
        testFile.deleteFile();

        logMessage("✅ Cancellation works correctly");
    }

    void testProgressCallback()
    {
        auto inputBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.5, 2);
        juce::File testFile = createTestWavFile(inputBuffer, 44100.0);
        juce::File outputDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        BatchProcessorSettings settings;
        settings.outputDirectory = outputDir;
        settings.outputPattern = "{filename}_progress";
        settings.overwriteExisting = true;

        BatchJob job(testFile, settings, 1);

        int progressCallCount = 0;
        float lastProgress = 0.0f;

        auto progressCallback = [&](float progress, const juce::String& /*message*/) -> bool
        {
            progressCallCount++;
            expect(progress >= lastProgress, "Progress should not decrease");
            expect(progress >= 0.0f && progress <= 1.0f, "Progress should be 0.0-1.0");
            lastProgress = progress;
            return true;  // Continue processing
        };

        BatchJobResult result = job.execute(progressCallback);

        expect(result.status == BatchJobStatus::COMPLETED, "Job should complete");
        expect(progressCallCount > 0, "Progress callback should be called");

        // Clean up
        testFile.deleteFile();
        result.outputFile.deleteFile();

        logMessage("✅ Progress callback called " + juce::String(progressCallCount) + " times");
    }

    void testOutputNamingPattern()
    {
        auto inputBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.5, 2);
        juce::File testFile = createTestWavFile(inputBuffer, 44100.0);
        juce::File outputDir = juce::File::getSpecialLocation(juce::File::tempDirectory);

        BatchProcessorSettings settings;
        settings.outputDirectory = outputDir;
        settings.outputPattern = "{filename}_batch_{index:03}";
        settings.overwriteExisting = true;

        BatchJob job(testFile, settings, 5);  // Index 5

        juce::File expectedOutput = job.getOutputFile();

        // The output filename should contain the index
        expect(expectedOutput.getFileName().contains("005"), "Output should contain padded index");

        BatchJobResult result = job.execute();
        expect(result.status == BatchJobStatus::COMPLETED, "Job should complete");

        // Clean up
        testFile.deleteFile();
        result.outputFile.deleteFile();

        logMessage("✅ Output naming pattern applied correctly");
    }
};

static BatchJobExecutionTests batchJobExecutionTests;

// ============================================================================
// BatchProcessorSettings Tests
// ============================================================================

class BatchProcessorSettingsTests : public juce::UnitTest
{
public:
    BatchProcessorSettingsTests() : juce::UnitTest("BatchProcessorSettings", "Batch") {}

    void runTest() override
    {
        beginTest("Settings serialization roundtrip");
        testSerializationRoundtrip();

        beginTest("Settings validation");
        testValidation();

        beginTest("Naming pattern tokens");
        testNamingPatternTokens();
    }

private:
    void testSerializationRoundtrip()
    {
        BatchProcessorSettings original;
        original.outputDirectory = juce::File::getSpecialLocation(juce::File::tempDirectory);
        original.outputPattern = "{filename}_processed";
        original.overwriteExisting = true;
        original.errorHandling = BatchErrorHandling::CONTINUE_ON_ERROR;

        BatchDSPSettings gainOp;
        gainOp.operation = BatchDSPOperation::GAIN;
        gainOp.enabled = true;
        gainOp.gainDb = 3.0f;
        original.dspChain.push_back(gainOp);

        // Serialize to JSON
        juce::String json = original.toJSON();
        expect(json.isNotEmpty(), "JSON should not be empty");

        // Deserialize from JSON
        BatchProcessorSettings restored = BatchProcessorSettings::fromJSON(json);

        // Verify
        expect(restored.outputPattern == original.outputPattern, "Pattern should match");
        expect(restored.overwriteExisting == original.overwriteExisting, "Overwrite setting should match");
        expect(restored.dspChain.size() == original.dspChain.size(), "DSP chain size should match");

        if (!restored.dspChain.empty())
        {
            expect(restored.dspChain[0].operation == BatchDSPOperation::GAIN, "Operation type should match");
            expectWithinAbsoluteError(restored.dspChain[0].gainDb, 3.0f, 0.001f, "Gain value should match");
        }

        logMessage("✅ Settings serialization roundtrip successful");
    }

    void testValidation()
    {
        BatchProcessorSettings settings;

        // Empty settings should have validation errors
        juce::StringArray errors = settings.validate();

        // Output directory not set should be an error
        expect(errors.size() > 0 || settings.outputDirectory == juce::File(),
               "Validation should detect issues with empty settings");

        // Set required fields
        settings.outputDirectory = juce::File::getSpecialLocation(juce::File::tempDirectory);
        settings.outputPattern = "{filename}_test";

        errors = settings.validate();

        logMessage("✅ Settings validation works correctly");
    }

    void testNamingPatternTokens()
    {
        BatchProcessorSettings settings;
        settings.outputDirectory = juce::File::getSpecialLocation(juce::File::tempDirectory);
        settings.outputPattern = "{filename}_processed";

        juce::File inputFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                   .getChildFile("test_audio.wav");

        juce::String outputName = settings.applyNamingPattern(inputFile, 1, "MyPreset");

        expect(outputName.contains("test_audio"), "Output should contain original filename");
        expect(outputName.contains("processed"), "Output should contain pattern suffix");

        logMessage("✅ Naming pattern tokens applied correctly");
    }
};

static BatchProcessorSettingsTests batchProcessorSettingsTests;
