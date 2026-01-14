/*
  ==============================================================================

    SaveAs8BitTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Tests for 8-bit depth and lower sample rate save functionality

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Audio/AudioFileManager.h"

class SaveAs8BitTests : public juce::UnitTest
{
public:
    SaveAs8BitTests() : juce::UnitTest("Save As 8-bit and Low Sample Rates", "Integration") {}

    void runTest() override
    {
        // Create test audio buffer (0.5 second of 440Hz sine wave)
        const double sourceSampleRate = 44100.0;
        const int numSamples = (int)(sourceSampleRate * 0.5);  // 0.5 seconds
        const int numChannels = 2;

        juce::AudioBuffer<float> testBuffer(numChannels, numSamples);

        // Generate 440Hz sine wave
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* channelData = testBuffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float phase = (i / sourceSampleRate) * 440.0f * 2.0f * juce::MathConstants<float>::pi;
                channelData[i] = std::sin(phase) * 0.5f;  // 50% amplitude
            }
        }

        // Test output directory
        juce::File outputDir("/Users/zacharylquarles/PROJECTS_Apps/Project_WaveEditor/TestFiles/automated");
        if (!outputDir.exists())
            outputDir.createDirectory();

        AudioFileManager fileManager;

        beginTest("WAV 8-bit");
        {
            juce::File outputFile = outputDir.getChildFile("test_wav_8bit.wav");
            if (outputFile.exists())
                outputFile.deleteFile();

            bool success = fileManager.saveAsWav(outputFile, testBuffer, sourceSampleRate, 8);

            if (!success)
            {
                logMessage("ERROR: " + fileManager.getLastError());
                expect(false, "Failed to save 8-bit WAV");
            }
            else
            {
                expect(outputFile.exists(), "8-bit WAV file should exist");

                // Verify we can read it back
                std::unique_ptr<juce::AudioFormatReader> reader(fileManager.createReaderFor(outputFile));
                expect(reader != nullptr, "Should be able to read 8-bit WAV");

                if (reader != nullptr)
                {
                    expect(reader->bitsPerSample == 8, "Should be 8-bit");
                    expect(reader->numChannels == numChannels, "Channel count should match");
                    expect(std::abs(reader->sampleRate - sourceSampleRate) < 0.01, "Sample rate should match");

                    logMessage("SUCCESS: 8-bit WAV saved and verified");
                }
            }
        }

        beginTest("WAV with 8kHz sample rate");
        testLowSampleRate(fileManager, outputDir, testBuffer, sourceSampleRate, 8000.0, 16);

        beginTest("WAV with 11.025kHz sample rate");
        testLowSampleRate(fileManager, outputDir, testBuffer, sourceSampleRate, 11025.0, 16);

        beginTest("WAV with 16kHz sample rate");
        testLowSampleRate(fileManager, outputDir, testBuffer, sourceSampleRate, 16000.0, 16);

        beginTest("WAV with 22.05kHz sample rate");
        testLowSampleRate(fileManager, outputDir, testBuffer, sourceSampleRate, 22050.0, 16);

        beginTest("WAV with 32kHz sample rate");
        testLowSampleRate(fileManager, outputDir, testBuffer, sourceSampleRate, 32000.0, 16);

        beginTest("WAV 8-bit with 8kHz (telephone quality)");
        {
            juce::File outputFile = outputDir.getChildFile("test_wav_8bit_8khz.wav");
            if (outputFile.exists())
                outputFile.deleteFile();

            // Resample to 8kHz first
            juce::AudioBuffer<float> resampledBuffer =
                AudioFileManager::resampleBuffer(testBuffer, sourceSampleRate, 8000.0);

            bool success = fileManager.saveAsWav(outputFile, resampledBuffer, 8000.0, 8);

            if (!success)
            {
                logMessage("ERROR: " + fileManager.getLastError());
                expect(false, "Failed to save 8-bit 8kHz WAV");
            }
            else
            {
                expect(outputFile.exists(), "8-bit 8kHz WAV file should exist");

                std::unique_ptr<juce::AudioFormatReader> reader(fileManager.createReaderFor(outputFile));
                expect(reader != nullptr, "Should be able to read 8-bit 8kHz WAV");

                if (reader != nullptr)
                {
                    expect(reader->bitsPerSample == 8, "Should be 8-bit");
                    expect(std::abs(reader->sampleRate - 8000.0) < 0.01, "Sample rate should be 8kHz");

                    juce::int64 fileSize = outputFile.getSize();
                    logMessage("SUCCESS: 8-bit 8kHz WAV saved (" + juce::String(fileSize) + " bytes)");
                }
            }
        }

        logMessage("=== All 8-bit and low sample rate tests completed ===");
    }

private:
    void testLowSampleRate(AudioFileManager& manager, const juce::File& outputDir,
                           const juce::AudioBuffer<float>& sourceBuffer, double sourceSampleRate,
                           double targetSampleRate, int bitDepth)
    {
        juce::String filename = "test_wav_" + juce::String(targetSampleRate, 0) + "hz.wav";
        juce::File outputFile = outputDir.getChildFile(filename);

        if (outputFile.exists())
            outputFile.deleteFile();

        // Resample buffer to target rate
        juce::AudioBuffer<float> resampledBuffer =
            AudioFileManager::resampleBuffer(sourceBuffer, sourceSampleRate, targetSampleRate);

        bool success = manager.saveAsWav(outputFile, resampledBuffer, targetSampleRate, bitDepth);

        if (!success)
        {
            logMessage("ERROR: " + manager.getLastError());
            expect(false, "Failed to save " + juce::String(targetSampleRate, 0) + "Hz WAV");
        }
        else
        {
            expect(outputFile.exists(), filename + " should exist");

            std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(outputFile));
            expect(reader != nullptr, "Should be able to read " + filename);

            if (reader != nullptr)
            {
                expect(std::abs(reader->sampleRate - targetSampleRate) < 0.01,
                       "Sample rate should be " + juce::String(targetSampleRate, 0) + "Hz");

                juce::int64 fileSize = outputFile.getSize();
                logMessage("SUCCESS: " + filename + " saved (" + juce::String(fileSize) + " bytes)");
            }
        }
    }
};

static SaveAs8BitTests saveAs8BitTests;