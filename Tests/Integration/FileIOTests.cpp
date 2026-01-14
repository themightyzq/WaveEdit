/*
  ==============================================================================

    FileIOTests.cpp
    Created: 2025-10-15
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Comprehensive integration tests for audio file I/O operations.
    Tests loading, saving, format conversion, and error handling.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "Audio/AudioFileManager.h"
#include "Audio/AudioBufferManager.h"
#include "Audio/AudioEngine.h"
#include "../TestUtils/TestAudioFiles.h"
#include "../TestUtils/AudioAssertions.h"

// ============================================================================
// File I/O Basic Operations Tests
// ============================================================================

class FileIOBasicTests : public juce::UnitTest
{
public:
    FileIOBasicTests() : juce::UnitTest("File I/O Basic Operations", "Integration") {}

    void runTest() override
    {
        beginTest("Load and save 16-bit stereo WAV");
        testLoadSave16BitStereo();

        beginTest("Load and save 24-bit mono WAV");
        testLoadSave24BitMono();

        beginTest("Load and save 32-bit float stereo WAV");
        testLoadSave32BitFloat();

        beginTest("Round-trip test - verify bit-accurate preservation");
        testRoundTripPreservation();

        beginTest("Load file with multiple sample rates");
        testMultipleSampleRates();

        beginTest("Save with different bit depths");
        testSaveWithDifferentBitDepths();
    }

private:
    juce::File getTempDirectory()
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("WaveEditTests");
    }

    void cleanupTempDirectory()
    {
        auto tempDir = getTempDirectory();
        if (tempDir.exists())
            tempDir.deleteRecursively();
    }

    void testLoadSave16BitStereo()
    {
        // Create temp directory
        auto tempDir = getTempDirectory();
        tempDir.createDirectory();

        // Generate test audio
        auto testBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);

        // Create file path
        auto testFile = tempDir.getChildFile("test_16bit_stereo.wav");

        // Save audio
        AudioFileManager fileManager;
        bool saveSuccess = fileManager.saveAsWav(testFile, testBuffer, 44100.0, 16);
        expect(saveSuccess, "Should save 16-bit stereo WAV file");

        if (saveSuccess)
        {
            // Verify file exists
            expect(testFile.existsAsFile(), "Saved file should exist");

            // Load it back
            juce::AudioBuffer<float> loadedBuffer;
            bool loadSuccess = fileManager.loadIntoBuffer(testFile, loadedBuffer);
            expect(loadSuccess, "Should load saved file");

            if (loadSuccess)
            {
                // Verify properties
                expectEquals(loadedBuffer.getNumChannels(), 2, "Should have 2 channels");
                expectEquals(loadedBuffer.getNumSamples(), testBuffer.getNumSamples(),
                           "Should have same sample count");

                // Note: 16-bit files will have quantization error, so use tolerance
                expect(AudioAssertions::expectBuffersNearlyEqual(testBuffer, loadedBuffer,
                       0.001f, "16-bit round-trip should preserve audio within quantization tolerance"));
            }
        }

        // Cleanup
        cleanupTempDirectory();
    }

    void testLoadSave24BitMono()
    {
        auto tempDir = getTempDirectory();
        tempDir.createDirectory();

        // Generate mono test audio
        auto testBuffer = TestAudio::createSineWave(880.0, 0.3, 48000.0, 0.5, 1);
        auto testFile = tempDir.getChildFile("test_24bit_mono.wav");

        AudioFileManager fileManager;
        bool saveSuccess = fileManager.saveAsWav(testFile, testBuffer, 48000.0, 24);
        expect(saveSuccess, "Should save 24-bit mono WAV file");

        if (saveSuccess)
        {
            expect(testFile.existsAsFile(), "Saved file should exist");

            juce::AudioBuffer<float> loadedBuffer;
            bool loadSuccess = fileManager.loadIntoBuffer(testFile, loadedBuffer);
            expect(loadSuccess, "Should load saved file");

            if (loadSuccess)
            {
                expectEquals(loadedBuffer.getNumChannels(), 1, "Should have 1 channel (mono)");
                expectEquals(loadedBuffer.getNumSamples(), testBuffer.getNumSamples(),
                           "Should have same sample count");

                // 24-bit should have very low quantization error
                expect(AudioAssertions::expectBuffersNearlyEqual(testBuffer, loadedBuffer,
                       0.0001f, "24-bit round-trip should preserve audio very accurately"));
            }
        }

        cleanupTempDirectory();
    }

    void testLoadSave32BitFloat()
    {
        auto tempDir = getTempDirectory();
        tempDir.createDirectory();

        // Generate test audio
        auto testBuffer = TestAudio::createSineWave(220.0, 0.7, 96000.0, 0.25, 2);
        auto testFile = tempDir.getChildFile("test_32bit_float.wav");

        AudioFileManager fileManager;
        bool saveSuccess = fileManager.saveAsWav(testFile, testBuffer, 96000.0, 32);
        expect(saveSuccess, "Should save 32-bit float WAV file");

        if (saveSuccess)
        {
            expect(testFile.existsAsFile(), "Saved file should exist");

            juce::AudioBuffer<float> loadedBuffer;
            bool loadSuccess = fileManager.loadIntoBuffer(testFile, loadedBuffer);
            expect(loadSuccess, "Should load saved file");

            if (loadSuccess)
            {
                expectEquals(loadedBuffer.getNumChannels(), 2, "Should have 2 channels");
                expectEquals(loadedBuffer.getNumSamples(), testBuffer.getNumSamples(),
                           "Should have same sample count");

                // 32-bit float should be bit-accurate
                expect(AudioAssertions::expectBuffersNearlyEqual(testBuffer, loadedBuffer,
                       0.000001f, "32-bit float round-trip should be bit-accurate"));
            }
        }

        cleanupTempDirectory();
    }

    void testRoundTripPreservation()
    {
        auto tempDir = getTempDirectory();
        tempDir.createDirectory();

        // Generate original audio
        auto originalBuffer = TestAudio::createSineWave(1000.0, 0.5, 44100.0, 2.0, 2);
        int64_t originalHash = AudioAssertions::hashBuffer(originalBuffer);

        auto testFile = tempDir.getChildFile("round_trip_test.wav");

        AudioFileManager fileManager;

        // Save original
        bool saveSuccess = fileManager.saveAsWav(testFile, originalBuffer, 44100.0, 32);
        expect(saveSuccess, "Should save original file");

        if (saveSuccess)
        {
            // Load back
            juce::AudioBuffer<float> loadedBuffer;
            bool loadSuccess = fileManager.loadIntoBuffer(testFile, loadedBuffer);
            expect(loadSuccess, "Should load saved file");

            if (loadSuccess)
            {
                // Save again
                auto testFile2 = tempDir.getChildFile("round_trip_test_2.wav");
                bool saveSuccess2 = fileManager.saveAsWav(testFile2, loadedBuffer, 44100.0, 32);
                expect(saveSuccess2, "Should save loaded file again");

                if (saveSuccess2)
                {
                    // Load second file
                    juce::AudioBuffer<float> reloadedBuffer;
                    bool loadSuccess2 = fileManager.loadIntoBuffer(testFile2, reloadedBuffer);
                    expect(loadSuccess2, "Should load second saved file");

                    if (loadSuccess2)
                    {
                        // Verify buffers are identical (32-bit float = bit-accurate)
                        int64_t reloadedHash = AudioAssertions::hashBuffer(reloadedBuffer);
                        expect(originalHash == reloadedHash ||
                               AudioAssertions::expectBuffersNearlyEqual(originalBuffer, reloadedBuffer, 0.000001f),
                               "Round-trip should preserve audio data");
                    }
                }
            }
        }

        cleanupTempDirectory();
    }

    void testMultipleSampleRates()
    {
        auto tempDir = getTempDirectory();
        tempDir.createDirectory();

        AudioFileManager fileManager;

        struct TestCase { double sampleRate; juce::String filename; };
        TestCase testCases[] = {
            { 44100.0, "test_44100.wav" },
            { 48000.0, "test_48000.wav" },
            { 88200.0, "test_88200.wav" },
            { 96000.0, "test_96000.wav" },
            { 192000.0, "test_192000.wav" }
        };

        for (const auto& testCase : testCases)
        {
            auto testBuffer = TestAudio::createSineWave(440.0, 0.5, testCase.sampleRate, 0.1, 2);
            auto testFile = tempDir.getChildFile(testCase.filename);

            bool saveSuccess = fileManager.saveAsWav(testFile, testBuffer, testCase.sampleRate, 16);
            expect(saveSuccess, "Should save file at " + juce::String(testCase.sampleRate) + " Hz");

            if (saveSuccess)
            {
                AudioFileInfo fileInfo;
                bool infoSuccess = fileManager.getFileInfo(testFile, fileInfo);
                expect(infoSuccess, "Should get file info");

                if (infoSuccess)
                {
                    expectEquals(fileInfo.sampleRate, testCase.sampleRate,
                               "File info should report correct sample rate");
                }
            }
        }

        cleanupTempDirectory();
    }

    void testSaveWithDifferentBitDepths()
    {
        auto tempDir = getTempDirectory();
        tempDir.createDirectory();

        auto testBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.5, 2);
        AudioFileManager fileManager;

        struct TestCase { int bitDepth; juce::String filename; };
        TestCase testCases[] = {
            { 16, "test_16bit.wav" },
            { 24, "test_24bit.wav" },
            { 32, "test_32bit.wav" }
        };

        for (const auto& testCase : testCases)
        {
            auto testFile = tempDir.getChildFile(testCase.filename);
            bool saveSuccess = fileManager.saveAsWav(testFile, testBuffer, 44100.0, testCase.bitDepth);
            expect(saveSuccess, "Should save file with " + juce::String(testCase.bitDepth) + " bit depth");

            if (saveSuccess)
            {
                expect(testFile.existsAsFile(), "Saved file should exist");

                // Verify file can be loaded
                juce::AudioBuffer<float> loadedBuffer;
                bool loadSuccess = fileManager.loadIntoBuffer(testFile, loadedBuffer);
                expect(loadSuccess, "Should load saved file");
            }
        }

        cleanupTempDirectory();
    }
};

// Register the test
static FileIOBasicTests fileIOBasicTests;

// ============================================================================
// File I/O Error Handling Tests
// ============================================================================

class FileIOErrorTests : public juce::UnitTest
{
public:
    FileIOErrorTests() : juce::UnitTest("File I/O Error Handling", "Integration") {}

    void runTest() override
    {
        beginTest("Load non-existent file");
        testLoadNonExistentFile();

        beginTest("Save to read-only location");
        testSaveToReadOnlyLocation();

        beginTest("Load corrupted/invalid file");
        testLoadCorruptedFile();

        beginTest("Get info for non-audio file");
        testGetInfoForNonAudioFile();

        beginTest("Save with invalid bit depth");
        testSaveWithInvalidBitDepth();

        beginTest("Save with invalid sample rate");
        testSaveWithInvalidSampleRate();

        beginTest("Overwrite file preserves on failure");
        testOverwriteFilePreservesOnFailure();
    }

private:
    juce::File getTempDirectory()
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("WaveEditTests");
    }

    void cleanupTempDirectory()
    {
        auto tempDir = getTempDirectory();
        if (tempDir.exists())
            tempDir.deleteRecursively();
    }

    void testLoadNonExistentFile()
    {
        AudioFileManager fileManager;
        auto nonExistentFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("this_file_does_not_exist_12345.wav");

        juce::AudioBuffer<float> buffer;
        bool loadSuccess = fileManager.loadIntoBuffer(nonExistentFile, buffer);

        expect(!loadSuccess, "Loading non-existent file should fail");
        expect(fileManager.getLastError().isNotEmpty(), "Should have error message");
    }

    void testSaveToReadOnlyLocation()
    {
        // Try to save to root directory (typically read-only)
        auto readOnlyFile = juce::File("/test_readonly.wav");  // macOS/Linux root

        auto testBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.1);
        AudioFileManager fileManager;

        bool saveSuccess = fileManager.saveAsWav(readOnlyFile, testBuffer, 44100.0, 16);

        // Should fail (unless running as root, but that's rare in tests)
        // Note: This test might pass if running with elevated privileges
        if (!saveSuccess)
        {
            expect(fileManager.getLastError().isNotEmpty(), "Should have error message for read-only location");
        }
    }

    void testLoadCorruptedFile()
    {
        auto tempDir = getTempDirectory();
        tempDir.createDirectory();

        // Create a fake "corrupted" file (just text, not valid WAV)
        auto corruptedFile = tempDir.getChildFile("corrupted.wav");
        corruptedFile.replaceWithText("This is not a valid WAV file!");

        AudioFileManager fileManager;
        juce::AudioBuffer<float> buffer;
        bool loadSuccess = fileManager.loadIntoBuffer(corruptedFile, buffer);

        expect(!loadSuccess, "Loading corrupted file should fail");
        expect(fileManager.getLastError().isNotEmpty(), "Should have error message");

        cleanupTempDirectory();
    }

    void testGetInfoForNonAudioFile()
    {
        auto tempDir = getTempDirectory();
        tempDir.createDirectory();

        // Create a text file
        auto textFile = tempDir.getChildFile("not_audio.txt");
        textFile.replaceWithText("This is a text file, not audio!");

        AudioFileManager fileManager;
        AudioFileInfo fileInfo;
        bool infoSuccess = fileManager.getFileInfo(textFile, fileInfo);

        expect(!infoSuccess, "Getting info for non-audio file should fail");

        cleanupTempDirectory();
    }

    void testSaveWithInvalidBitDepth()
    {
        auto tempDir = getTempDirectory();
        tempDir.createDirectory();

        auto testBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.1);
        auto testFile = tempDir.getChildFile("invalid_bitdepth.wav");

        AudioFileManager fileManager;

        // Try to save with invalid bit depth (e.g., 8-bit or 64-bit)
        bool saveSuccess = fileManager.saveAsWav(testFile, testBuffer, 44100.0, 8);
        expect(!saveSuccess, "Saving with invalid bit depth (8) should fail");

        saveSuccess = fileManager.saveAsWav(testFile, testBuffer, 44100.0, 64);
        expect(!saveSuccess, "Saving with invalid bit depth (64) should fail");

        cleanupTempDirectory();
    }

    void testSaveWithInvalidSampleRate()
    {
        auto tempDir = getTempDirectory();
        tempDir.createDirectory();

        auto testBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.1);
        auto testFile = tempDir.getChildFile("invalid_samplerate.wav");

        AudioFileManager fileManager;

        // Try to save with invalid sample rate (negative or zero)
        bool saveSuccess = fileManager.saveAsWav(testFile, testBuffer, -44100.0, 16);
        expect(!saveSuccess, "Saving with negative sample rate should fail");

        saveSuccess = fileManager.saveAsWav(testFile, testBuffer, 0.0, 16);
        expect(!saveSuccess, "Saving with zero sample rate should fail");

        cleanupTempDirectory();
    }

    void testOverwriteFilePreservesOnFailure()
    {
        auto tempDir = getTempDirectory();
        tempDir.createDirectory();

        // Create an original file
        auto testBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        auto testFile = tempDir.getChildFile("original.wav");

        AudioFileManager fileManager;
        bool saveSuccess = fileManager.saveAsWav(testFile, testBuffer, 44100.0, 16);
        expect(saveSuccess, "Should save original file");

        if (saveSuccess)
        {
            // Get original file size
            int64_t originalSize = testFile.getSize();

            // Try to overwrite with invalid data (should fail internally)
            // Create an empty buffer (invalid)
            juce::AudioBuffer<float> emptyBuffer(0, 0);

            bool overwriteSuccess = fileManager.overwriteFile(testFile, emptyBuffer, 44100.0, 16);
            expect(!overwriteSuccess, "Overwriting with empty buffer should fail");

            // Verify original file still exists and is unchanged
            expect(testFile.existsAsFile(), "Original file should still exist after failed overwrite");
            expectEquals(testFile.getSize(), originalSize, "Original file size should be unchanged");
        }

        cleanupTempDirectory();
    }
};

// Register the test
static FileIOErrorTests fileIOErrorTests;

// ============================================================================
// Large File Handling Tests
// ============================================================================

class FileIOLargeFileTests : public juce::UnitTest
{
public:
    FileIOLargeFileTests() : juce::UnitTest("File I/O Large Files", "Integration") {}

    void runTest() override
    {
        beginTest("Load and save 10-minute stereo file");
        testLargeFileTenMinutes();

        beginTest("Multi-channel file (6 channels)");
        testMultiChannelFile();
    }

private:
    juce::File getTempDirectory()
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("WaveEditTests");
    }

    void cleanupTempDirectory()
    {
        auto tempDir = getTempDirectory();
        if (tempDir.exists())
            tempDir.deleteRecursively();
    }

    void testLargeFileTenMinutes()
    {
        auto tempDir = getTempDirectory();
        tempDir.createDirectory();

        // Create 10-minute stereo file at 44.1kHz
        const double durationSeconds = 600.0;  // 10 minutes
        const double sampleRate = 44100.0;
        const int numChannels = 2;

        logMessage("Generating 10-minute test audio...");
        auto testBuffer = TestAudio::createSineWave(440.0, 0.5, sampleRate, durationSeconds, numChannels);

        int64_t expectedSamples = static_cast<int64_t>(durationSeconds * sampleRate);
        expectEquals(static_cast<int64_t>(testBuffer.getNumSamples()), expectedSamples,
                   "Should generate correct number of samples");

        auto testFile = tempDir.getChildFile("large_10min.wav");

        logMessage("Saving 10-minute file...");
        AudioFileManager fileManager;
        bool saveSuccess = fileManager.saveAsWav(testFile, testBuffer, sampleRate, 16);
        expect(saveSuccess, "Should save large 10-minute file");

        if (saveSuccess)
        {
            logMessage("Loading 10-minute file...");
            juce::AudioBuffer<float> loadedBuffer;
            bool loadSuccess = fileManager.loadIntoBuffer(testFile, loadedBuffer);
            expect(loadSuccess, "Should load large file");

            if (loadSuccess)
            {
                expectEquals(loadedBuffer.getNumChannels(), numChannels, "Should have correct channel count");
                expectEquals(static_cast<int64_t>(loadedBuffer.getNumSamples()), expectedSamples,
                           "Should have correct sample count after loading");

                logMessage("Verifying audio integrity...");
                // Check first 1 second and last 1 second
                int samplesPerSecond = static_cast<int>(sampleRate);

                for (int channel = 0; channel < numChannels; ++channel)
                {
                    // Check first second
                    for (int sample = 0; sample < samplesPerSecond; ++sample)
                    {
                        float original = testBuffer.getSample(channel, sample);
                        float loaded = loadedBuffer.getSample(channel, sample);
                        expect(std::abs(original - loaded) < 0.001f,
                               "First second should match (sample " + juce::String(sample) + ")");
                    }

                    // Check last second
                    int lastSecondStart = loadedBuffer.getNumSamples() - samplesPerSecond;
                    for (int sample = 0; sample < samplesPerSecond; ++sample)
                    {
                        float original = testBuffer.getSample(channel, lastSecondStart + sample);
                        float loaded = loadedBuffer.getSample(channel, lastSecondStart + sample);
                        expect(std::abs(original - loaded) < 0.001f,
                               "Last second should match (sample " + juce::String(sample) + ")");
                    }
                }

                logMessage("✅ Large file test passed!");
            }
        }

        cleanupTempDirectory();
    }

    void testMultiChannelFile()
    {
        auto tempDir = getTempDirectory();
        tempDir.createDirectory();

        // Create 6-channel (5.1 surround) file
        const int numChannels = 6;
        const double sampleRate = 48000.0;
        const double durationSeconds = 5.0;

        // Generate different frequencies for each channel (for testing)
        juce::AudioBuffer<float> testBuffer(numChannels, static_cast<int>(sampleRate * durationSeconds));

        double frequencies[] = { 100.0, 200.0, 300.0, 400.0, 500.0, 600.0 };
        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto channelBuffer = TestAudio::createSineWave(frequencies[channel], 0.5, sampleRate, durationSeconds, 1);
            testBuffer.copyFrom(channel, 0, channelBuffer, 0, 0, channelBuffer.getNumSamples());
        }

        auto testFile = tempDir.getChildFile("multichannel_6ch.wav");

        AudioFileManager fileManager;
        bool saveSuccess = fileManager.saveAsWav(testFile, testBuffer, sampleRate, 24);
        expect(saveSuccess, "Should save 6-channel file");

        if (saveSuccess)
        {
            juce::AudioBuffer<float> loadedBuffer;
            bool loadSuccess = fileManager.loadIntoBuffer(testFile, loadedBuffer);
            expect(loadSuccess, "Should load 6-channel file");

            if (loadSuccess)
            {
                expectEquals(loadedBuffer.getNumChannels(), numChannels, "Should have 6 channels");
                expectEquals(loadedBuffer.getNumSamples(), testBuffer.getNumSamples(),
                           "Should have same sample count");

                // Verify each channel is distinct
                for (int ch1 = 0; ch1 < numChannels; ++ch1)
                {
                    for (int ch2 = ch1 + 1; ch2 < numChannels; ++ch2)
                    {
                        // Channels should NOT be identical (different frequencies)
                        bool channelsIdentical = true;
                        for (int sample = 0; sample < 100; ++sample)
                        {
                            if (std::abs(loadedBuffer.getSample(ch1, sample) -
                                        loadedBuffer.getSample(ch2, sample)) > 0.01f)
                            {
                                channelsIdentical = false;
                                break;
                            }
                        }
                        expect(!channelsIdentical,
                               "Channels " + juce::String(ch1) + " and " + juce::String(ch2) +
                               " should be distinct");
                    }
                }
            }
        }

        cleanupTempDirectory();
    }
};

// Register the test
static FileIOLargeFileTests fileIOLargeFileTests;
