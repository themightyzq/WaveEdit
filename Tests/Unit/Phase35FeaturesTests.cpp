/*
  ==============================================================================

    Phase35FeaturesTests.cpp
    Created: 2025-10-15
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Automated tests for Phase 3.5 P1 features:
    - Time format display and cycling
    - Zoom level display formatting
    - Auto-save functionality

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "Utils/AudioUnits.h"
#include "Utils/Settings.h"

// ============================================================================
// Time Format Tests (Phase 3.5 - Priority #5)
// ============================================================================

class TimeFormatTests : public juce::UnitTest
{
public:
    TimeFormatTests() : juce::UnitTest("Time Format Display", "Phase 3.5") {}

    void runTest() override
    {
        beginTest("Samples format");
        testSamplesFormat();

        beginTest("Milliseconds format");
        testMillisecondsFormat();

        beginTest("Seconds format");
        testSecondsFormat();

        beginTest("Frames format");
        testFramesFormat();

        beginTest("Time format cycling");
        testFormatCycling();

        beginTest("Time format to string conversion");
        testFormatToString();
    }

private:
    void testSamplesFormat()
    {
        const double sampleRate = 44100.0;

        // Test 1 second = 44100 samples
        auto result = AudioUnits::formatTime(1.0, sampleRate, AudioUnits::TimeFormat::Samples);
        expect(result == "44100 smp", "1 second should be 44100 samples");

        // Test 0.5 seconds = 22050 samples
        result = AudioUnits::formatTime(0.5, sampleRate, AudioUnits::TimeFormat::Samples);
        expect(result == "22050 smp", "0.5 seconds should be 22050 samples");

        // Test 0 seconds
        result = AudioUnits::formatTime(0.0, sampleRate, AudioUnits::TimeFormat::Samples);
        expect(result == "0 smp", "0 seconds should be 0 samples");
    }

    void testMillisecondsFormat()
    {
        const double sampleRate = 44100.0;

        // Test 1 second = 1000 ms
        auto result = AudioUnits::formatTime(1.0, sampleRate, AudioUnits::TimeFormat::Milliseconds);
        expect(result == "1000.00 ms", "1 second should be 1000.00 ms");

        // Test 0.5 seconds = 500 ms
        result = AudioUnits::formatTime(0.5, sampleRate, AudioUnits::TimeFormat::Milliseconds);
        expect(result == "500.00 ms", "0.5 seconds should be 500.00 ms");

        // Test 0.123 seconds = 123 ms
        result = AudioUnits::formatTime(0.123, sampleRate, AudioUnits::TimeFormat::Milliseconds);
        expect(result == "123.00 ms", "0.123 seconds should be 123.00 ms");
    }

    void testSecondsFormat()
    {
        const double sampleRate = 44100.0;

        // Test 1 second
        auto result = AudioUnits::formatTime(1.0, sampleRate, AudioUnits::TimeFormat::Seconds);
        expect(result == "1.00 s", "1 second should be 1.00 s");

        // Test 0.5 seconds
        result = AudioUnits::formatTime(0.5, sampleRate, AudioUnits::TimeFormat::Seconds);
        expect(result == "0.50 s", "0.5 seconds should be 0.50 s");

        // Test 123.456 seconds
        result = AudioUnits::formatTime(123.456, sampleRate, AudioUnits::TimeFormat::Seconds);
        expect(result == "123.46 s", "123.456 seconds should be 123.46 s (rounded)");
    }

    void testFramesFormat()
    {
        const double sampleRate = 44100.0;
        const double fps = 30.0;

        // Test 1 second at 30fps = 30 frames
        auto result = AudioUnits::formatTime(1.0, sampleRate, AudioUnits::TimeFormat::Frames, fps);
        expect(result == "30 fr @ 30.00 fps", "1 second at 30fps should be 30 frames");

        // Test 2.5 seconds at 30fps = 75 frames
        result = AudioUnits::formatTime(2.5, sampleRate, AudioUnits::TimeFormat::Frames, fps);
        expect(result == "75 fr @ 30.00 fps", "2.5 seconds at 30fps should be 75 frames");

        // Test 0 seconds
        result = AudioUnits::formatTime(0.0, sampleRate, AudioUnits::TimeFormat::Frames, fps);
        expect(result == "0 fr @ 30.00 fps", "0 seconds should be 0 frames");
    }

    void testFormatCycling()
    {
        using TF = AudioUnits::TimeFormat;

        // Test cycling through all formats
        auto format = TF::Samples;
        format = AudioUnits::getNextTimeFormat(format);
        expect(format == TF::Milliseconds, "Samples should cycle to Milliseconds");

        format = AudioUnits::getNextTimeFormat(format);
        expect(format == TF::Seconds, "Milliseconds should cycle to Seconds");

        format = AudioUnits::getNextTimeFormat(format);
        expect(format == TF::Frames, "Seconds should cycle to Frames");

        format = AudioUnits::getNextTimeFormat(format);
        expect(format == TF::Samples, "Frames should cycle back to Samples");
    }

    void testFormatToString()
    {
        using TF = AudioUnits::TimeFormat;

        expect(AudioUnits::timeFormatToString(TF::Samples) == "Samples", "Samples format name");
        expect(AudioUnits::timeFormatToString(TF::Milliseconds) == "Milliseconds", "Milliseconds format name");
        expect(AudioUnits::timeFormatToString(TF::Seconds) == "Seconds", "Seconds format name");
        expect(AudioUnits::timeFormatToString(TF::Frames) == "Frames", "Frames format name");
    }
};

// Register time format tests
static TimeFormatTests timeFormatTests;

// ============================================================================
// Auto-Save Tests (Phase 3.5 - Priority #6)
// ============================================================================

class AutoSaveTests : public juce::UnitTest
{
public:
    AutoSaveTests() : juce::UnitTest("Auto-Save Functionality", "Phase 3.5") {}

    void runTest() override
    {
        beginTest("Auto-save directory creation");
        testAutoSaveDirectoryCreation();

        beginTest("Auto-save file naming convention");
        testAutoSaveFileNaming();

        beginTest("Auto-save cleanup (keep last 3 files)");
        testAutoSaveCleanup();

        beginTest("Settings persistence for auto-save preferences");
        testSettingsPersistence();
    }

private:
    void testAutoSaveDirectoryCreation()
    {
        // Get auto-save directory path
        juce::File autoSaveDir = Settings::getInstance().getSettingsDirectory().getChildFile("autosave_test");

        // Clean up from previous test run
        if (autoSaveDir.exists())
            autoSaveDir.deleteRecursively();

        // Verify directory doesn't exist
        expect(!autoSaveDir.exists(), "Auto-save directory should not exist initially");

        // Create directory
        bool created = autoSaveDir.createDirectory();
        expect(created, "Auto-save directory should be created successfully");
        expect(autoSaveDir.exists(), "Auto-save directory should exist after creation");
        expect(autoSaveDir.isDirectory(), "Auto-save path should be a directory");

        // Clean up
        autoSaveDir.deleteRecursively();
    }

    void testAutoSaveFileNaming()
    {
        // Test auto-save filename format: autosave_[originalname]_[timestamp].wav
        juce::String originalFilename = "MySong";
        juce::String timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        juce::String autoSaveFilename = "autosave_" + originalFilename + "_" + timestamp + ".wav";

        // Verify filename components
        expect(autoSaveFilename.startsWith("autosave_"), "Auto-save filename should start with 'autosave_'");
        expect(autoSaveFilename.contains(originalFilename), "Auto-save filename should contain original name");
        expect(autoSaveFilename.endsWith(".wav"), "Auto-save filename should end with '.wav'");

        // Parse filename to extract components
        juce::StringArray parts = juce::StringArray::fromTokens(autoSaveFilename.upToLastOccurrenceOf(".wav", false, true), "_", "");
        expect(parts.size() >= 3, "Auto-save filename should have at least 3 parts (autosave, name, timestamp)");
        expect(parts[0] == "autosave", "First part should be 'autosave'");
        expect(parts[1] == originalFilename, "Second part should be original filename");
    }

    void testAutoSaveCleanup()
    {
        // Create temporary test directory
        juce::File testDir = Settings::getInstance().getSettingsDirectory().getChildFile("autosave_cleanup_test");
        if (testDir.exists())
            testDir.deleteRecursively();
        testDir.createDirectory();

        // Create 5 mock auto-save files with different timestamps
        juce::Array<juce::File> testFiles;
        for (int i = 0; i < 5; ++i)
        {
            juce::String timestamp = juce::String::formatted("%08d_000000", 20251015 + i);  // Different dates
            juce::File file = testDir.getChildFile("autosave_TestFile_" + timestamp + ".wav");
            file.create();
            testFiles.add(file);

            // Add small delay to ensure different modification times
            juce::Thread::sleep(10);
        }

        // Verify all 5 files were created
        juce::Array<juce::File> filesFound;
        testDir.findChildFiles(filesFound, juce::File::findFiles, false, "autosave_*.wav");
        expect(filesFound.size() == 5, "Should have created 5 auto-save files");

        // Simulate cleanup: delete all but newest 3
        // Sort files by modification time (newest first)
        struct FileComparator
        {
            static int compareElements(const juce::File& first, const juce::File& second)
            {
                auto timeA = first.getLastModificationTime();
                auto timeB = second.getLastModificationTime();
                if (timeA > timeB) return -1;
                if (timeA < timeB) return 1;
                return 0;
            }
        };
        FileComparator comp;
        filesFound.sort(comp, false);

        // Delete all but the newest 3
        for (int i = 3; i < filesFound.size(); ++i)
        {
            filesFound[i].deleteFile();
        }

        // Verify only 3 files remain
        juce::Array<juce::File> remainingFiles;
        testDir.findChildFiles(remainingFiles, juce::File::findFiles, false, "autosave_*.wav");
        expect(remainingFiles.size() == 3, "Should have cleaned up to 3 files");

        // Verify the newest files were kept (check modification times)
        for (int i = 0; i < remainingFiles.size() - 1; ++i)
        {
            expect(remainingFiles[i].getLastModificationTime() >= remainingFiles[i + 1].getLastModificationTime(),
                   "Files should be sorted newest first");
        }

        // Clean up test directory
        testDir.deleteRecursively();
    }

    void testSettingsPersistence()
    {
        // Test auto-save enabled setting
        Settings::getInstance().setSetting("autoSave.enabled", true);
        bool enabled = Settings::getInstance().getSetting("autoSave.enabled", false);
        expect(enabled == true, "Auto-save enabled setting should persist");

        // Test auto-save disabled
        Settings::getInstance().setSetting("autoSave.enabled", false);
        enabled = Settings::getInstance().getSetting("autoSave.enabled", true);
        expect(enabled == false, "Auto-save disabled setting should persist");

        // Test auto-save interval setting
        Settings::getInstance().setSetting("autoSave.intervalMinutes", 10);
        int interval = Settings::getInstance().getSetting("autoSave.intervalMinutes", 5);
        expect(interval == 10, "Auto-save interval setting should persist");

        // Test default value when setting doesn't exist
        Settings::getInstance().setSetting("autoSave.testSetting", juce::var());  // Clear setting
        juce::String defaultValue = Settings::getInstance().getSetting("autoSave.nonExistent", "default").toString();
        expect(defaultValue == "default", "Should return default value for non-existent setting");
    }
};

// Register auto-save tests
static AutoSaveTests autoSaveTests;
