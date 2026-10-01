/*
  ==============================================================================

    BWFMetadataTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Automated tests for BWF (Broadcast Wave Format) metadata system.
    Tests utility class operations and file I/O round-trip.

  ==============================================================================
*/

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include "../../Source/Utils/BWFMetadata.h"
#include "../../Source/Audio/AudioFileManager.h"
#include "../TestUtils/TestAudioFiles.h"

//==============================================================================
/**
 * BWF Metadata Tests
 *
 * Tests the BWF (Broadcast Wave Format) metadata utility class and
 * file I/O integration. Ensures metadata can be written to and read
 * from WAV files correctly.
 */
class BWFMetadataTests : public juce::UnitTest
{
public:
    BWFMetadataTests() : juce::UnitTest("BWF Metadata", "BWF") {}

    void runTest() override
    {
        // Test 1: BWFMetadata utility class operations
        testBWFMetadataBasicOperations();
        testBWFMetadataDateTimeFormatting();
        testBWFMetadataToJUCEConversion();
        testBWFMetadataFromJUCEConversion();
        testBWFMetadataDefaultCreation();

        // Test 2: File I/O round-trip
        testBWFMetadataFileRoundTrip();
        testBWFMetadataEmptyFields();
        testBWFMetadataTimeReference();
    }

private:
    //==========================================================================
    // Test 1: Basic BWFMetadata operations

    void testBWFMetadataBasicOperations()
    {
        beginTest("BWFMetadata - Basic operations");

        BWFMetadata metadata;

        // Test setters/getters
        metadata.setDescription("Test audio file");
        metadata.setOriginator("ZQ SFX WaveEdit");
        metadata.setOriginatorRef("REF123456");
        metadata.setOriginationDate("2025-01-15");
        metadata.setOriginationTime("14:30:00");
        metadata.setTimeReference(132300);
        metadata.setCodingHistory("A=PCM,F=44100,W=16,M=stereo");

        expect(metadata.getDescription() == "Test audio file",
               "Description should be set correctly");
        expect(metadata.getOriginator() == "ZQ SFX WaveEdit",
               "Originator should be set correctly");
        expect(metadata.getOriginatorRef() == "REF123456",
               "Originator reference should be set correctly");
        expect(metadata.getOriginationDate() == "2025-01-15",
               "Origination date should be set correctly");
        expect(metadata.getOriginationTime() == "14:30:00",
               "Origination time should be set correctly");
        expect(metadata.getTimeReference() == 132300,
               "Time reference should be set correctly");
        expect(metadata.getCodingHistory() == "A=PCM,F=44100,W=16,M=stereo",
               "Coding history should be set correctly");

        // Test hasMetadata()
        expect(metadata.hasMetadata(), "Should have metadata after setting fields");

        // Test clear()
        metadata.clear();
        expect(!metadata.hasMetadata(), "Should have no metadata after clear");
        expect(metadata.getDescription().isEmpty(), "Description should be empty");
        expect(metadata.getTimeReference() == 0, "Time reference should be 0");
    }

    void testBWFMetadataDateTimeFormatting()
    {
        beginTest("BWFMetadata - Date/Time formatting");

        BWFMetadata metadata;

        // Create a specific time: January 15, 2025, 14:30:45 (local time)
        juce::Time testTime(2025, 0, 15, 14, 30, 45, 0, true); // Month is 0-based, use local time

        metadata.setOriginationDateTime(testTime);

        expect(metadata.getOriginationDate() == "2025-01-15",
               "Date should be formatted as yyyy-mm-dd");
        expect(metadata.getOriginationTime() == "14:30:45",
               "Time should be formatted as hh:mm:ss");

        // Test round-trip conversion
        juce::Time retrievedTime = metadata.getOriginationDateTime();

        expect(retrievedTime.getYear() == 2025, "Year should match");
        expect(retrievedTime.getMonth() == 0, "Month should match (0-based)");
        expect(retrievedTime.getDayOfMonth() == 15, "Day should match");
        expect(retrievedTime.getHours() == 14, "Hour should match");
        expect(retrievedTime.getMinutes() == 30, "Minute should match");
        expect(retrievedTime.getSeconds() == 45, "Second should match");
    }

    void testBWFMetadataToJUCEConversion()
    {
        beginTest("BWFMetadata - To JUCE metadata conversion");

        BWFMetadata metadata;
        metadata.setDescription("Voice recording");
        metadata.setOriginator("ZQ SFX");
        metadata.setOriginatorRef("ABC123");
        metadata.setOriginationDate("2025-01-15");
        metadata.setOriginationTime("10:00:00");
        metadata.setTimeReference(44100);
        metadata.setCodingHistory("A=PCM,F=44100,W=24,M=stereo");

        juce::StringPairArray juceMetadata = metadata.toJUCEMetadata();

        // Verify all fields are present in JUCE format
        expect(juceMetadata.containsKey(juce::WavAudioFormat::bwavDescription),
               "Should contain description key");
        expect(juceMetadata.containsKey(juce::WavAudioFormat::bwavOriginator),
               "Should contain originator key");
        expect(juceMetadata.containsKey(juce::WavAudioFormat::bwavOriginatorRef),
               "Should contain originator ref key");
        expect(juceMetadata.containsKey(juce::WavAudioFormat::bwavOriginationDate),
               "Should contain origination date key");
        expect(juceMetadata.containsKey(juce::WavAudioFormat::bwavOriginationTime),
               "Should contain origination time key");
        expect(juceMetadata.containsKey(juce::WavAudioFormat::bwavTimeReference),
               "Should contain time reference key");
        expect(juceMetadata.containsKey(juce::WavAudioFormat::bwavCodingHistory),
               "Should contain coding history key");

        // Verify values match
        expect(juceMetadata[juce::WavAudioFormat::bwavDescription] == "Voice recording",
               "Description value should match");
        expect(juceMetadata[juce::WavAudioFormat::bwavTimeReference] == "44100",
               "Time reference should be converted to string");
    }

    void testBWFMetadataFromJUCEConversion()
    {
        beginTest("BWFMetadata - From JUCE metadata conversion");

        // Create JUCE metadata
        juce::StringPairArray juceMetadata;
        juceMetadata.set(juce::WavAudioFormat::bwavDescription, "Podcast episode");
        juceMetadata.set(juce::WavAudioFormat::bwavOriginator, "ZQ SFX WaveEdit");
        juceMetadata.set(juce::WavAudioFormat::bwavOriginatorRef, "POD001");
        juceMetadata.set(juce::WavAudioFormat::bwavOriginationDate, "2025-02-01");
        juceMetadata.set(juce::WavAudioFormat::bwavOriginationTime, "09:15:30");
        juceMetadata.set(juce::WavAudioFormat::bwavTimeReference, "96000");
        juceMetadata.set(juce::WavAudioFormat::bwavCodingHistory, "A=PCM,F=48000,W=32,M=mono");

        // Convert to BWFMetadata
        BWFMetadata metadata;
        metadata.fromJUCEMetadata(juceMetadata);

        // Verify all fields loaded correctly
        expect(metadata.getDescription() == "Podcast episode",
               "Description should be loaded");
        expect(metadata.getOriginator() == "ZQ SFX WaveEdit",
               "Originator should be loaded");
        expect(metadata.getOriginatorRef() == "POD001",
               "Originator ref should be loaded");
        expect(metadata.getOriginationDate() == "2025-02-01",
               "Origination date should be loaded");
        expect(metadata.getOriginationTime() == "09:15:30",
               "Origination time should be loaded");
        expect(metadata.getTimeReference() == 96000,
               "Time reference should be parsed as integer");
        expect(metadata.getCodingHistory() == "A=PCM,F=48000,W=32,M=mono",
               "Coding history should be loaded");
    }

    void testBWFMetadataDefaultCreation()
    {
        beginTest("BWFMetadata - Default creation");

        BWFMetadata metadata = BWFMetadata::createDefault("Test description");

        expect(metadata.hasMetadata(), "Default metadata should exist");
        expect(metadata.getDescription() == "Test description",
               "Should use provided description");
        expect(metadata.getOriginator() == "ZQ SFX WaveEdit",
               "Should have ZQ SFX originator");
        expect(metadata.getOriginationDate().isNotEmpty(),
               "Should have current date");
        expect(metadata.getOriginationTime().isNotEmpty(),
               "Should have current time");
        expect(metadata.getCodingHistory().isNotEmpty(),
               "Should have default coding history");
    }

    //==========================================================================
    // Test 2: File I/O round-trip

    void testBWFMetadataFileRoundTrip()
    {
        beginTest("BWFMetadata - File I/O round-trip");

        // Create test audio buffer
        juce::AudioBuffer<float> testBuffer = TestAudio::createSineWave(440.0, 0.5f, 44100.0, 1.0, 2);

        // Create metadata
        BWFMetadata originalMetadata;
        originalMetadata.setDescription("Round-trip test file");
        originalMetadata.setOriginator("ZQ SFX WaveEdit");
        originalMetadata.setOriginatorRef("RT001");
        originalMetadata.setOriginationDate("2025-01-20");
        originalMetadata.setOriginationTime("16:45:00");
        originalMetadata.setTimeReference(44100 * 60); // 1 minute offset
        originalMetadata.setCodingHistory("A=PCM,F=44100,W=16,M=stereo,T=WaveEdit");

        // Save file with metadata
        juce::File testFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("bwf_roundtrip_test.wav");
        AudioFileManager fileManager;

        bool saveSuccess = fileManager.saveAsWav(testFile, testBuffer, 44100.0, 16,
                                                  originalMetadata.toJUCEMetadata());

        expect(saveSuccess, "Should save file with metadata");

        // Load file and verify metadata
        AudioFileInfo fileInfo;
        bool loadSuccess = fileManager.getFileInfo(testFile, fileInfo);

        expect(loadSuccess, "Should load file info");

        // Convert loaded metadata back to BWFMetadata
        BWFMetadata loadedMetadata;
        loadedMetadata.fromJUCEMetadata(fileInfo.metadata);

        // Verify all metadata fields match
        expect(loadedMetadata.getDescription() == originalMetadata.getDescription(),
               "Description should match after round-trip");
        expect(loadedMetadata.getOriginator() == originalMetadata.getOriginator(),
               "Originator should match after round-trip");
        expect(loadedMetadata.getOriginatorRef() == originalMetadata.getOriginatorRef(),
               "Originator ref should match after round-trip");
        expect(loadedMetadata.getOriginationDate() == originalMetadata.getOriginationDate(),
               "Origination date should match after round-trip");
        expect(loadedMetadata.getOriginationTime() == originalMetadata.getOriginationTime(),
               "Origination time should match after round-trip");
        expect(loadedMetadata.getTimeReference() == originalMetadata.getTimeReference(),
               "Time reference should match after round-trip");
        expect(loadedMetadata.getCodingHistory() == originalMetadata.getCodingHistory(),
               "Coding history should match after round-trip");

        // Clean up
        testFile.deleteFile();
    }

    void testBWFMetadataEmptyFields()
    {
        beginTest("BWFMetadata - Empty fields handling");

        // Create minimal metadata (only some fields set)
        BWFMetadata metadata;
        metadata.setDescription("Minimal metadata test");
        // Leave other fields empty

        // Convert to JUCE format
        juce::StringPairArray juceMetadata = metadata.toJUCEMetadata();

        // Verify only non-empty fields are included
        expect(juceMetadata.containsKey(juce::WavAudioFormat::bwavDescription),
               "Description should be included");
        expect(!juceMetadata.containsKey(juce::WavAudioFormat::bwavOriginator) ||
               juceMetadata[juce::WavAudioFormat::bwavOriginator].isEmpty(),
               "Empty originator should not be included or be empty");

        // Time reference should always be included (even if 0)
        expect(juceMetadata.containsKey(juce::WavAudioFormat::bwavTimeReference),
               "Time reference should always be included");
    }

    void testBWFMetadataTimeReference()
    {
        beginTest("BWFMetadata - Time reference handling");

        BWFMetadata metadata;

        // Test various time reference values
        int64_t testValues[] = { 0, 44100, 96000, 192000, 88200 * 3600 }; // 0, 1s@44.1k, 1s@96k, 1s@192k, 1hr@88.2k

        for (auto testValue : testValues)
        {
            metadata.setTimeReference(testValue);

            juce::StringPairArray juceMetadata = metadata.toJUCEMetadata();
            juce::String timeRefString = juceMetadata[juce::WavAudioFormat::bwavTimeReference];

            expect(timeRefString.getLargeIntValue() == testValue,
                   "Time reference should convert to string and back correctly");

            // Test round-trip
            BWFMetadata loadedMetadata;
            loadedMetadata.fromJUCEMetadata(juceMetadata);

            expect(loadedMetadata.getTimeReference() == testValue,
                   "Time reference should survive round-trip");
        }
    }
};

// Register the test
static BWFMetadataTests bwfMetadataTests;
