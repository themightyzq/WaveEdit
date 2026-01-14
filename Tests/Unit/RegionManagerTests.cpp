/*
  ==============================================================================

    RegionManagerTests.cpp
    Created: 2025-10-16
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Automated tests for Region and Auto Region features (Phase 3.3):
    - Region creation, modification, deletion
    - Region persistence (JSON sidecar files)
    - Auto Region algorithm (silence detection)
    - Region navigation and selection

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Utils/Region.h"
#include "Utils/RegionManager.h"
#include "TestUtils/TestAudioFiles.h"
#include "TestUtils/AudioAssertions.h"

// ============================================================================
// Region Basic Operations Tests
// ============================================================================

class RegionBasicTests : public juce::UnitTest
{
public:
    RegionBasicTests() : juce::UnitTest("Region Basic Operations", "RegionManager") {}

    void runTest() override
    {
        beginTest("Region construction and getters");
        testRegionConstruction();

        beginTest("Region setters");
        testRegionSetters();

        beginTest("Region length calculation");
        testRegionLength();

        beginTest("Region JSON serialization");
        testRegionJSON();
    }

private:
    void testRegionConstruction()
    {
        Region region("Test Region", 1000, 2000);

        expectEquals(region.getName(), juce::String("Test Region"));
        expectEquals(region.getStartSample(), (int64_t)1000);
        expectEquals(region.getEndSample(), (int64_t)2000);
        expect(region.getColor() != juce::Colours::transparentBlack);
    }

    void testRegionSetters()
    {
        Region region("Original", 100, 200);

        region.setName("Modified");
        expectEquals(region.getName(), juce::String("Modified"));

        region.setStartSample(500);
        expectEquals(region.getStartSample(), (int64_t)500);

        region.setEndSample(1000);
        expectEquals(region.getEndSample(), (int64_t)1000);

        region.setColor(juce::Colours::red);
        expectEquals(region.getColor(), juce::Colours::red);
    }

    void testRegionLength()
    {
        Region region("Test", 100, 500);

        expectEquals(region.getLengthInSamples(), (int64_t)400);
        expectWithinAbsoluteError(region.getLengthInSeconds(44100.0), 400.0 / 44100.0, 0.0001);
    }

    void testRegionJSON()
    {
        Region original("Test Region", 1000, 5000);
        original.setColor(juce::Colours::blue);

        auto json = original.toJSON();

        Region restored = Region::fromJSON(json);

        expectEquals(restored.getName(), original.getName());
        expectEquals(restored.getStartSample(), original.getStartSample());
        expectEquals(restored.getEndSample(), original.getEndSample());
        expectEquals(restored.getColor(), original.getColor());
    }
};

static RegionBasicTests regionBasicTests;

// ============================================================================
// RegionManager Tests
// ============================================================================

class RegionManagerTests : public juce::UnitTest
{
public:
    RegionManagerTests() : juce::UnitTest("RegionManager Operations", "RegionManager") {}

    void runTest() override
    {
        beginTest("Add and retrieve regions");
        testAddRegions();

        beginTest("Remove regions");
        testRemoveRegions();

        beginTest("Find region at sample position");
        testFindRegionAtSample();

        beginTest("Get all regions");
        testGetAllRegions();

        beginTest("Clear all regions");
        testClearRegions();
    }

private:
    void testAddRegions()
    {
        RegionManager manager;

        Region region1("Region 1", 0, 1000);
        Region region2("Region 2", 2000, 3000);

        manager.addRegion(region1);
        manager.addRegion(region2);

        expectEquals(manager.getNumRegions(), 2);
        expect(manager.getRegion(0) != nullptr);
        expect(manager.getRegion(1) != nullptr);
        expectEquals(manager.getRegion(0)->getName(), juce::String("Region 1"));
        expectEquals(manager.getRegion(1)->getName(), juce::String("Region 2"));
    }

    void testRemoveRegions()
    {
        RegionManager manager;

        manager.addRegion(Region("R1", 0, 100));
        manager.addRegion(Region("R2", 200, 300));
        manager.addRegion(Region("R3", 400, 500));

        expectEquals(manager.getNumRegions(), 3);

        manager.removeRegion(1); // Remove R2

        expectEquals(manager.getNumRegions(), 2);
        expectEquals(manager.getRegion(0)->getName(), juce::String("R1"));
        expectEquals(manager.getRegion(1)->getName(), juce::String("R3"));
    }

    void testFindRegionAtSample()
    {
        RegionManager manager;

        manager.addRegion(Region("R1", 100, 500));
        manager.addRegion(Region("R2", 1000, 2000));

        expectEquals(manager.findRegionAtSample(50), -1);   // Before any region
        expectEquals(manager.findRegionAtSample(300), 0);   // Inside R1
        expectEquals(manager.findRegionAtSample(750), -1);  // Between regions
        expectEquals(manager.findRegionAtSample(1500), 1);  // Inside R2
        expectEquals(manager.findRegionAtSample(3000), -1); // After all regions
    }

    void testGetAllRegions()
    {
        RegionManager manager;

        manager.addRegion(Region("R1", 0, 100));
        manager.addRegion(Region("R2", 200, 300));

        const auto& regions = manager.getAllRegions();

        expectEquals((int)regions.size(), 2);
        expectEquals(regions[0].getName(), juce::String("R1"));
        expectEquals(regions[1].getName(), juce::String("R2"));
    }

    void testClearRegions()
    {
        RegionManager manager;

        manager.addRegion(Region("R1", 0, 100));
        manager.addRegion(Region("R2", 200, 300));

        expectEquals(manager.getNumRegions(), 2);

        manager.removeAllRegions();

        expectEquals(manager.getNumRegions(), 0);
    }
};

static RegionManagerTests regionManagerTests;

// ============================================================================
// Auto Region Algorithm Tests (Phase 3.3 - CRITICAL)
// ============================================================================

class AutoRegionTests : public juce::UnitTest
{
public:
    AutoRegionTests() : juce::UnitTest("Auto Region Algorithm", "RegionManager") {}

    void runTest() override
    {
        beginTest("Auto Region with simple silence pattern");
        testSimpleSilenceDetection();

        beginTest("Auto Region with varying threshold");
        testVaryingThreshold();

        beginTest("Auto Region minimum region length filtering");
        testMinRegionLength();

        beginTest("Auto Region minimum silence length");
        testMinSilenceLength();

        beginTest("Auto Region pre/post-roll margins");
        testPrePostRoll();

        beginTest("Auto Region with no silence (single region)");
        testNoSilence();

        beginTest("Auto Region with all silence (no regions)");
        testAllSilence();
    }

private:
    void testSimpleSilenceDetection()
    {
        // Create buffer: [sound 1s][silence 0.5s][sound 1s][silence 0.5s][sound 1s]
        const double sampleRate = 44100.0;
        const int soundDuration = (int)(1.0 * sampleRate);    // 1 second
        const int silenceDuration = (int)(0.5 * sampleRate);  // 0.5 seconds

        const int totalSamples = 3 * soundDuration + 2 * silenceDuration;
        juce::AudioBuffer<float> buffer(2, totalSamples);
        buffer.clear();

        // Fill sound sections with 0.5 amplitude
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);

            // Sound 1
            for (int i = 0; i < soundDuration; ++i)
                data[i] = 0.5f;

            // Silence 1 (already zero)

            // Sound 2
            int offset = soundDuration + silenceDuration;
            for (int i = 0; i < soundDuration; ++i)
                data[offset + i] = 0.5f;

            // Silence 2 (already zero)

            // Sound 3
            offset += soundDuration + silenceDuration;
            for (int i = 0; i < soundDuration; ++i)
                data[offset + i] = 0.5f;
        }

        RegionManager manager;
        manager.autoCreateRegions(buffer, sampleRate,
                                 -40.0f,  // threshold (silence < -40dB)
                                 100.0f,  // min region length (100ms)
                                 100.0f,  // min silence length (100ms)
                                 0.0f,    // pre-roll (0ms)
                                 0.0f);   // post-roll (0ms)

        // Should create 3 regions
        expectEquals(manager.getNumRegions(), 3);

        if (manager.getNumRegions() == 3)
        {
            // Verify region boundaries are approximately correct
            auto* r1 = manager.getRegion(0);
            auto* r2 = manager.getRegion(1);
            auto* r3 = manager.getRegion(2);

            expect(r1->getStartSample() < soundDuration);
            expect(r1->getEndSample() <= soundDuration + 100);

            expect(r2->getStartSample() >= soundDuration + silenceDuration - 100);
            expect(r2->getEndSample() <= 2 * soundDuration + silenceDuration + 100);

            expect(r3->getStartSample() >= 2 * soundDuration + 2 * silenceDuration - 100);
        }
    }

    void testVaryingThreshold()
    {
        // Create buffer with low-amplitude sound (0.1) and high-amplitude sound (0.8)
        const double sampleRate = 44100.0;
        const int duration = (int)(1.0 * sampleRate);

        juce::AudioBuffer<float> buffer(1, duration * 2);
        auto* data = buffer.getWritePointer(0);

        for (int i = 0; i < duration; ++i)
            data[i] = 0.1f;  // Low amplitude

        for (int i = duration; i < 2 * duration; ++i)
            data[i] = 0.8f;  // High amplitude

        // High threshold (-20dB) should only detect high-amplitude section
        RegionManager manager1;
        manager1.autoCreateRegions(buffer, sampleRate, -20.0f, 100.0f, 100.0f, 0.0f, 0.0f);

        expect(manager1.getNumRegions() >= 1); // At least one region for high amplitude

        // Low threshold (-50dB) should detect both sections
        RegionManager manager2;
        manager2.autoCreateRegions(buffer, sampleRate, -50.0f, 100.0f, 100.0f, 0.0f, 0.0f);

        expect(manager2.getNumRegions() >= manager1.getNumRegions());
    }

    void testMinRegionLength()
    {
        // Create very short sound bursts
        const double sampleRate = 44100.0;
        const int shortSound = (int)(0.05 * sampleRate);  // 50ms (below 100ms threshold)
        const int silence = (int)(0.2 * sampleRate);       // 200ms

        juce::AudioBuffer<float> buffer(1, 3 * (shortSound + silence));
        buffer.clear();
        auto* data = buffer.getWritePointer(0);

        // Three 50ms bursts
        for (int burst = 0; burst < 3; ++burst)
        {
            int offset = burst * (shortSound + silence);
            for (int i = 0; i < shortSound; ++i)
                data[offset + i] = 0.5f;
        }

        RegionManager manager;
        manager.autoCreateRegions(buffer, sampleRate,
                                 -40.0f,
                                 100.0f,  // min region length = 100ms (filters out 50ms bursts)
                                 50.0f,   // min silence length
                                 0.0f, 0.0f);

        // Should create 0 regions (all bursts too short)
        expectEquals(manager.getNumRegions(), 0);
    }

    void testMinSilenceLength()
    {
        // Create sound with brief gaps
        const double sampleRate = 44100.0;
        const int sound = (int)(1.0 * sampleRate);
        const int briefGap = (int)(0.05 * sampleRate);  // 50ms gap

        juce::AudioBuffer<float> buffer(1, 2 * sound + briefGap);
        buffer.clear();
        auto* data = buffer.getWritePointer(0);

        for (int i = 0; i < sound; ++i)
            data[i] = 0.5f;

        // Brief gap (50ms)

        for (int i = sound + briefGap; i < 2 * sound + briefGap; ++i)
            data[i] = 0.5f;

        RegionManager manager;
        manager.autoCreateRegions(buffer, sampleRate,
                                 -40.0f,
                                 100.0f,
                                 200.0f,  // min silence = 200ms (ignores 50ms gap)
                                 0.0f, 0.0f);

        // Should create 1 region (gap too short to split)
        expectEquals(manager.getNumRegions(), 1);
    }

    void testPrePostRoll()
    {
        const double sampleRate = 44100.0;
        const int sound = (int)(1.0 * sampleRate);
        const int silence = (int)(0.5 * sampleRate);

        juce::AudioBuffer<float> buffer(1, sound + silence + sound);
        buffer.clear();
        auto* data = buffer.getWritePointer(0);

        for (int i = 0; i < sound; ++i)
            data[i] = 0.5f;

        for (int i = sound + silence; i < buffer.getNumSamples(); ++i)
            data[i] = 0.5f;

        RegionManager manager;
        manager.autoCreateRegions(buffer, sampleRate,
                                 -40.0f,
                                 100.0f,
                                 100.0f,
                                 50.0f,   // pre-roll = 50ms
                                 100.0f); // post-roll = 100ms

        expectEquals(manager.getNumRegions(), 2);

        if (manager.getNumRegions() == 2)
        {
            auto* r1 = manager.getRegion(0);
            auto* r2 = manager.getRegion(1);

            // First region should extend into silence (post-roll)
            expect(r1->getEndSample() > sound);

            // Second region should start before actual sound (pre-roll)
            expect(r2->getStartSample() < sound + silence);
        }
    }

    void testNoSilence()
    {
        // Continuous sound, no silence
        const double sampleRate = 44100.0;
        const int duration = (int)(2.0 * sampleRate);

        juce::AudioBuffer<float> buffer(1, duration);
        auto* data = buffer.getWritePointer(0);

        for (int i = 0; i < duration; ++i)
            data[i] = 0.5f;

        RegionManager manager;
        manager.autoCreateRegions(buffer, sampleRate, -40.0f, 100.0f, 100.0f, 0.0f, 0.0f);

        // Should create 1 region covering entire file
        expectEquals(manager.getNumRegions(), 1);

        if (manager.getNumRegions() == 1)
        {
            auto* r = manager.getRegion(0);
            expectEquals(r->getStartSample(), (int64_t)0);
            expectEquals(r->getEndSample(), (int64_t)duration);
        }
    }

    void testAllSilence()
    {
        // All silence
        const double sampleRate = 44100.0;
        const int duration = (int)(2.0 * sampleRate);

        juce::AudioBuffer<float> buffer(1, duration);
        buffer.clear();

        RegionManager manager;
        manager.autoCreateRegions(buffer, sampleRate, -40.0f, 100.0f, 100.0f, 0.0f, 0.0f);

        // Should create 0 regions
        expectEquals(manager.getNumRegions(), 0);
    }
};

static AutoRegionTests autoRegionTests;

// ============================================================================
// Region Persistence Tests
// ============================================================================

class RegionPersistenceTests : public juce::UnitTest
{
public:
    RegionPersistenceTests() : juce::UnitTest("Region Persistence (JSON)", "RegionManager") {}

    void runTest() override
    {
        beginTest("Save and load regions to/from JSON");
        testSaveLoadJSON();

        beginTest("Verify sidecar file naming");
        testSidecarFileNaming();
    }

private:
    void testSaveLoadJSON()
    {
        // Create temporary audio file for testing
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto testFile = tempDir.getChildFile("test_audio.wav");

        RegionManager manager1;
        manager1.addRegion(Region("Intro", 0, 44100));
        manager1.addRegion(Region("Verse", 88200, 132300));

        // Save regions
        bool saved = manager1.saveToFile(testFile);
        expect(saved);

        // Load regions into new manager
        RegionManager manager2;
        bool loaded = manager2.loadFromFile(testFile);
        expect(loaded);

        // Verify loaded regions match
        expectEquals(manager2.getNumRegions(), 2);

        if (manager2.getNumRegions() == 2)
        {
            expectEquals(manager2.getRegion(0)->getName(), juce::String("Intro"));
            expectEquals(manager2.getRegion(0)->getStartSample(), (int64_t)0);
            expectEquals(manager2.getRegion(0)->getEndSample(), (int64_t)44100);

            expectEquals(manager2.getRegion(1)->getName(), juce::String("Verse"));
            expectEquals(manager2.getRegion(1)->getStartSample(), (int64_t)88200);
            expectEquals(manager2.getRegion(1)->getEndSample(), (int64_t)132300);
        }

        // Cleanup
        auto regionFile = testFile.withFileExtension(".wav.regions.json");
        if (regionFile.existsAsFile())
            regionFile.deleteFile();
        if (testFile.existsAsFile())
            testFile.deleteFile();
    }

    void testSidecarFileNaming()
    {
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto testFile = tempDir.getChildFile("audio_file.wav");

        RegionManager manager;
        manager.addRegion(Region("Test", 0, 1000));

        bool saved = manager.saveToFile(testFile);
        expect(saved);

        // Verify sidecar file exists with correct name
        auto regionFile = testFile.withFileExtension(".wav.regions.json");
        expect(regionFile.existsAsFile());

        // Cleanup
        if (regionFile.existsAsFile())
            regionFile.deleteFile();
        if (testFile.existsAsFile())
            testFile.deleteFile();
    }
};

static RegionPersistenceTests regionPersistenceTests;
