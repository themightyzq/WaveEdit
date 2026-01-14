/*
  ==============================================================================

    SaveAsFormatsTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Automated tests for Save As functionality across all supported formats.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Audio/AudioFileManager.h"

class SaveAsFormatsTests : public juce::UnitTest
{
public:
    SaveAsFormatsTests() : juce::UnitTest("Save As Formats", "Integration") {}

    void runTest() override
    {
        // Create test audio buffer (1 second of 440Hz sine wave)
        const double sampleRate = 96000.0;
        const int numSamples = (int)sampleRate;  // 1 second
        const int numChannels = 2;

        juce::AudioBuffer<float> testBuffer(numChannels, numSamples);

        // Generate 440Hz sine wave
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* channelData = testBuffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float phase = (i / sampleRate) * 440.0f * 2.0f * juce::MathConstants<float>::pi;
                channelData[i] = std::sin(phase) * 0.5f;  // 50% amplitude
            }
        }

        // Test output directory
        juce::File outputDir("/Users/zacharylquarles/PROJECTS_Apps/Project_WaveEditor/TestFiles/automated");
        if (!outputDir.exists())
            outputDir.createDirectory();

        AudioFileManager fileManager;

        beginTest("WAV 16-bit");
        testSaveFormat(fileManager, outputDir, testBuffer, sampleRate, "test_wav_16bit.wav", 16, 10);

        beginTest("WAV 24-bit");
        testSaveFormat(fileManager, outputDir, testBuffer, sampleRate, "test_wav_24bit.wav", 24, 10);

        beginTest("WAV 32-bit Float");
        testSaveFormat(fileManager, outputDir, testBuffer, sampleRate, "test_wav_32bit.wav", 32, 10);

        beginTest("FLAC Quality 0 (fastest)");
        testSaveFormat(fileManager, outputDir, testBuffer, sampleRate, "test_flac_q0.flac", 24, 0);

        beginTest("FLAC Quality 5 (balanced)");
        testSaveFormat(fileManager, outputDir, testBuffer, sampleRate, "test_flac_q5.flac", 24, 5);

        beginTest("FLAC Quality 10 (highest)");
        testSaveFormat(fileManager, outputDir, testBuffer, sampleRate, "test_flac_q10.flac", 24, 10);

        beginTest("OGG Quality 0 (lowest)");
        testSaveFormat(fileManager, outputDir, testBuffer, sampleRate, "test_ogg_q0.ogg", 24, 0);

        beginTest("OGG Quality 5 (balanced)");
        testSaveFormat(fileManager, outputDir, testBuffer, sampleRate, "test_ogg_q5.ogg", 24, 5);

        beginTest("OGG Quality 10 (highest)");
        testSaveFormat(fileManager, outputDir, testBuffer, sampleRate, "test_ogg_q10.ogg", 24, 10);

        // Only test MP3 if LAME is available
        juce::AudioFormatManager manager;
        manager.registerBasicFormats();
        auto* mp3Format = manager.findFormatForFileExtension(".mp3");

        if (mp3Format != nullptr)
        {
            // Test if we can actually create a writer (verify LAME)
            juce::MemoryOutputStream dummyStream;
            std::unique_ptr<juce::AudioFormatWriter> testWriter(
                mp3Format->createWriterFor(&dummyStream, 44100.0, 2, 16, {}, 5)
            );

            if (testWriter != nullptr)
            {
                beginTest("MP3 Quality 0 (~64kbps)");
                testSaveFormat(fileManager, outputDir, testBuffer, sampleRate, "test_mp3_q0.mp3", 24, 0);

                beginTest("MP3 Quality 5 (~128kbps)");
                testSaveFormat(fileManager, outputDir, testBuffer, sampleRate, "test_mp3_q5.mp3", 24, 5);

                beginTest("MP3 Quality 10 (~320kbps)");
                testSaveFormat(fileManager, outputDir, testBuffer, sampleRate, "test_mp3_q10.mp3", 24, 10);
            }
            else
            {
                logMessage("Skipping MP3 tests - LAME encoder not available");
            }
        }
        else
        {
            logMessage("Skipping MP3 tests - MP3 format not registered");
        }

        // Test sample rate conversion
        beginTest("Sample rate conversion 96kHz -> 48kHz");
        testSaveFormatWithResampling(fileManager, outputDir, testBuffer, sampleRate,
                                     "test_resample_48k.wav", 24, 10, 48000.0);

        beginTest("Sample rate conversion 96kHz -> 44.1kHz");
        testSaveFormatWithResampling(fileManager, outputDir, testBuffer, sampleRate,
                                     "test_resample_44k.wav", 24, 10, 44100.0);

        logMessage("=== All format tests completed ===");
        logMessage("Output files in: " + outputDir.getFullPathName());
    }

private:
    void testSaveFormat(AudioFileManager& manager, const juce::File& outputDir,
                       const juce::AudioBuffer<float>& buffer, double sampleRate,
                       const juce::String& filename, int bitDepth, int quality)
    {
        juce::File outputFile = outputDir.getChildFile(filename);

        // Delete existing file
        if (outputFile.exists())
            outputFile.deleteFile();

        // Save file
        bool success = manager.saveAudioFile(outputFile, buffer, sampleRate, bitDepth, quality);

        if (!success)
        {
            logMessage("ERROR: " + manager.getLastError());
            expect(false, "Failed to save " + filename);
            return;
        }

        expect(outputFile.exists(), "Output file should exist");

        if (outputFile.exists())
        {
            juce::int64 fileSize = outputFile.getSize();
            logMessage("SUCCESS: " + filename + " (" + juce::String(fileSize / 1024) + " KB)");

            // Verify we can read it back
            std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(outputFile));
            expect(reader != nullptr, "Should be able to read back the saved file");

            if (reader != nullptr)
            {
                expect(reader->numChannels == buffer.getNumChannels(),
                       "Channel count should match");

                // For compressed formats, allow slight sample count variation
                juce::String ext = outputFile.getFileExtension();
                if (ext == ".wav")
                {
                    expect(reader->lengthInSamples == buffer.getNumSamples(),
                           "Sample count should match for lossless format");
                }

                logMessage("  Verified: " + juce::String(reader->numChannels) + " channels, " +
                          juce::String(reader->lengthInSamples) + " samples, " +
                          juce::String(reader->sampleRate, 0) + " Hz");
            }
        }
    }

    void testSaveFormatWithResampling(AudioFileManager& manager, const juce::File& outputDir,
                                     const juce::AudioBuffer<float>& buffer, double sourceSampleRate,
                                     const juce::String& filename, int bitDepth, int quality,
                                     double targetSampleRate)
    {
        juce::File outputFile = outputDir.getChildFile(filename);

        // Delete existing file
        if (outputFile.exists())
            outputFile.deleteFile();

        // Resample buffer
        juce::AudioBuffer<float> resampledBuffer =
            AudioFileManager::resampleBuffer(buffer, sourceSampleRate, targetSampleRate);

        // Save resampled file
        bool success = manager.saveAudioFile(outputFile, resampledBuffer, targetSampleRate,
                                            bitDepth, quality);

        if (!success)
        {
            logMessage("ERROR: " + manager.getLastError());
            expect(false, "Failed to save resampled " + filename);
            return;
        }

        expect(outputFile.exists(), "Output file should exist");

        if (outputFile.exists())
        {
            juce::int64 fileSize = outputFile.getSize();
            logMessage("SUCCESS: " + filename + " (resampled, " +
                      juce::String(fileSize / 1024) + " KB)");

            // Verify sample rate
            std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(outputFile));
            expect(reader != nullptr, "Should be able to read back resampled file");

            if (reader != nullptr)
            {
                expect(std::abs(reader->sampleRate - targetSampleRate) < 0.01,
                       "Sample rate should match target");

                logMessage("  Verified: Sample rate = " + juce::String(reader->sampleRate, 0) + " Hz");
            }
        }
    }
};

static SaveAsFormatsTests saveAsFormatsTests;
