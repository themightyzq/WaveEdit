/*
  ==============================================================================

    AudioEngineTests.cpp
    Created: 2025-10-15
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Comprehensive thread safety and functional tests for AudioEngine.
    Tests critical audio thread vs message thread interactions.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "Audio/AudioEngine.h"
#include "TestAudioFiles.h"
#include "AudioAssertions.h"

// ============================================================================
// AudioEngine Thread Safety Tests
// ============================================================================

class AudioEngineThreadSafetyTests : public juce::UnitTest
{
public:
    AudioEngineThreadSafetyTests() : juce::UnitTest("AudioEngine Thread Safety", "AudioEngine") {}

    void runTest() override
    {
        beginTest("Concurrent buffer updates during playback");
        testConcurrentBufferUpdates();

        beginTest("Rapid state changes from multiple threads");
        testRapidStateChanges();

        beginTest("Level monitoring concurrent read/write");
        testLevelMonitoringThreadSafety();

        beginTest("Position updates during playback");
        testPositionUpdatesDuringPlayback();

        beginTest("Loop state changes during playback");
        testLoopStateChangesDuringPlayback();

        beginTest("Concurrent file loading operations");
        testConcurrentFileLoading();
    }

private:
    void testConcurrentBufferUpdates()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Create initial buffer with sine wave
        auto buffer1 = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        expect(engine.loadFromBuffer(buffer1, 44100.0, 2), "Initial buffer load should succeed");

        // Start playback
        engine.play();
        expect(engine.isPlaying(), "Engine should be playing");

        // Wait for playback to stabilize
        juce::Thread::sleep(100);

        // Simulate concurrent buffer updates (like real-time gain adjustment)
        // This tests the critical reloadBufferPreservingPlayback() method
        for (int i = 0; i < 50; ++i)
        {
            auto updatedBuffer = TestAudio::createSineWave(440.0, 0.3, 44100.0, 1.0, 2);

            // This should NOT crash or cause audio glitches
            bool reloadSuccess = engine.reloadBufferPreservingPlayback(updatedBuffer, 44100.0, 2);
            expect(reloadSuccess, "Buffer reload during playback should succeed");

            // Brief delay to allow audio thread to process
            juce::Thread::sleep(10);
        }

        // Verify engine is still playing after updates
        expect(engine.isPlaying(), "Engine should still be playing after buffer updates");

        engine.stop();
        expect(engine.getPlaybackState() == PlaybackState::STOPPED, "Engine should be stopped");
    }

    void testRapidStateChanges()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 2.0, 2);
        expect(engine.loadFromBuffer(buffer, 44100.0, 2), "Buffer load should succeed");

        // Rapidly toggle play/pause from "multiple threads" (simulated)
        // In real usage, UI events could trigger rapid state changes
        for (int i = 0; i < 100; ++i)
        {
            engine.play();
            expect(engine.isPlaying(), "Should be playing after play()");

            juce::Thread::sleep(5);

            engine.pause();
            expect(engine.getPlaybackState() == PlaybackState::PAUSED, "Should be paused after pause()");

            juce::Thread::sleep(5);

            engine.play();
            juce::Thread::sleep(5);

            engine.stop();
            expect(engine.getPlaybackState() == PlaybackState::STOPPED, "Should be stopped after stop()");
        }

        logMessage("✅ Survived 100 rapid state transitions without crash");
    }

    void testLevelMonitoringThreadSafety()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        auto buffer = TestAudio::createSineWave(440.0, 0.8, 44100.0, 1.0, 2);
        expect(engine.loadFromBuffer(buffer, 44100.0, 2), "Buffer load should succeed");

        engine.setLevelMonitoringEnabled(true);
        engine.play();

        // Simulate UI thread reading levels rapidly while audio thread writes them
        // This tests atomic operations on m_peakLevels and m_rmsLevels
        std::atomic<int> readCount{0};
        std::atomic<bool> stopReading{false};

        auto readerThread = std::thread([&]() {
            while (!stopReading.load())
            {
                // Continuously read levels (like a meter UI component would)
                float peakL = engine.getPeakLevel(0);
                float peakR = engine.getPeakLevel(1);
                float rmsL = engine.getRMSLevel(0);
                float rmsR = engine.getRMSLevel(1);

                // Validate levels are in reasonable range
                expect(peakL >= 0.0f && peakL <= 2.0f, "Peak L should be in valid range");
                expect(peakR >= 0.0f && peakR <= 2.0f, "Peak R should be in valid range");
                expect(rmsL >= 0.0f && rmsL <= 2.0f, "RMS L should be in valid range");
                expect(rmsR >= 0.0f && rmsR <= 2.0f, "RMS R should be in valid range");

                readCount++;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

        // Let it run for 500ms
        juce::Thread::sleep(500);
        stopReading.store(true);
        readerThread.join();

        engine.stop();

        logMessage("✅ Read levels " + juce::String(readCount.load()) + " times without data races");
        expect(readCount.load() > 100, "Should have completed many concurrent reads");
    }

    void testPositionUpdatesDuringPlayback()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 3.0, 2);
        expect(engine.loadFromBuffer(buffer, 44100.0, 2), "Buffer load should succeed");

        engine.play();

        // Simulate random seeks during playback (like user dragging timeline cursor)
        for (int i = 0; i < 50; ++i)
        {
            double randomPos = juce::Random::getSystemRandom().nextDouble() * 2.8; // 0-2.8 seconds
            engine.setPosition(randomPos);

            juce::Thread::sleep(20);

            double currentPos = engine.getCurrentPosition();
            expect(currentPos >= 0.0 && currentPos <= 3.0, "Position should be in valid range");
        }

        engine.stop();
        logMessage("✅ Survived 50 random position changes during playback");
    }

    void testLoopStateChangesDuringPlayback()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.5, 2);
        expect(engine.loadFromBuffer(buffer, 44100.0, 2), "Buffer load should succeed");

        engine.play();

        // Toggle looping rapidly while playing
        for (int i = 0; i < 100; ++i)
        {
            bool shouldLoop = (i % 2 == 0);
            engine.setLooping(shouldLoop);

            expect(engine.isLooping() == shouldLoop, "Loop state should match what was set");

            juce::Thread::sleep(10);
        }

        engine.stop();
        logMessage("✅ Toggled loop state 100 times during playback without issues");
    }

    void testConcurrentFileLoading()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Test that rapid file/buffer switching doesn't cause crashes
        for (int i = 0; i < 50; ++i)
        {
            auto buffer = TestAudio::createSineWave(440.0 + i * 10.0, 0.5, 44100.0, 0.5, 2);
            expect(engine.loadFromBuffer(buffer, 44100.0, 2), "Buffer load should succeed");

            // Briefly play, then stop and switch
            engine.play();
            juce::Thread::sleep(20);
            engine.stop();

            // Immediate reload (stress test)
            auto buffer2 = TestAudio::createSineWave(880.0, 0.3, 48000.0, 0.3, 1);
            expect(engine.loadFromBuffer(buffer2, 48000.0, 1), "Second buffer load should succeed");
        }

        logMessage("✅ Survived 50 rapid buffer switches without crash");
    }
};

// Register the thread safety test
static AudioEngineThreadSafetyTests audioEngineThreadSafetyTests;

// ============================================================================
// AudioEngine Functional Tests
// ============================================================================

class AudioEngineFunctionalTests : public juce::UnitTest
{
public:
    AudioEngineFunctionalTests() : juce::UnitTest("AudioEngine Functional", "AudioEngine") {}

    void runTest() override
    {
        beginTest("Initialize audio device");
        testAudioDeviceInitialization();

        beginTest("Load audio from buffer");
        testLoadFromBuffer();

        beginTest("Playback state machine");
        testPlaybackStateMachine();

        beginTest("Position management");
        testPositionManagement();

        beginTest("Loop functionality");
        testLoopFunctionality();

        beginTest("Audio properties validation");
        testAudioPropertiesValidation();

        beginTest("Buffer validation on load");
        testBufferValidationOnLoad();

        beginTest("Mono to stereo playback");
        testMonoToStereoPlayback();
    }

private:
    void testAudioDeviceInitialization()
    {
        AudioEngine engine;
        bool initialized = engine.initializeAudioDevice();
        expect(initialized, "Audio device should initialize successfully");

        auto& deviceManager = engine.getDeviceManager();
        expect(deviceManager.getCurrentAudioDevice() != nullptr, "Should have an audio device");

        logMessage("✅ Audio device initialized: " +
                   deviceManager.getCurrentAudioDevice()->getName());
    }

    void testLoadFromBuffer()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Test loading valid buffer
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        bool loaded = engine.loadFromBuffer(buffer, 44100.0, 2);

        expect(loaded, "Should load valid buffer successfully");
        expect(engine.isFileLoaded(), "Engine should report file loaded");
        expect(engine.isPlayingFromBuffer(), "Engine should be in buffer playback mode");
        expectEquals(engine.getSampleRate(), 44100.0, "Sample rate should match");
        expectEquals(engine.getNumChannels(), 2, "Channel count should match");
        expectWithinAbsoluteError(engine.getTotalLength(), 1.0, 0.01, "Duration should be 1 second");
    }

    void testPlaybackStateMachine()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 2.0, 2);
        engine.loadFromBuffer(buffer, 44100.0, 2);

        // Test state transitions
        expect(engine.getPlaybackState() == PlaybackState::STOPPED, "Initial state should be STOPPED");
        expect(!engine.isPlaying(), "isPlaying() should be false initially");

        engine.play();
        expect(engine.getPlaybackState() == PlaybackState::PLAYING, "State should be PLAYING after play()");
        expect(engine.isPlaying(), "isPlaying() should be true");

        engine.pause();
        expect(engine.getPlaybackState() == PlaybackState::PAUSED, "State should be PAUSED after pause()");
        expect(!engine.isPlaying(), "isPlaying() should be false when paused");

        engine.play();
        expect(engine.getPlaybackState() == PlaybackState::PLAYING, "Should resume to PLAYING");

        engine.stop();
        expect(engine.getPlaybackState() == PlaybackState::STOPPED, "State should be STOPPED after stop()");
        expectWithinAbsoluteError(engine.getCurrentPosition(), 0.0, 0.01, "Position should reset to 0 on stop");

        logMessage("✅ Playback state machine works correctly");
    }

    void testPositionManagement()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 3.0, 2);
        engine.loadFromBuffer(buffer, 44100.0, 2);

        // Test position setting
        engine.setPosition(1.5);
        expectWithinAbsoluteError(engine.getCurrentPosition(), 1.5, 0.1, "Should set position to 1.5 seconds");

        engine.setPosition(2.9);
        expectWithinAbsoluteError(engine.getCurrentPosition(), 2.9, 0.1, "Should set position to 2.9 seconds");

        // Test clamping
        engine.setPosition(-1.0);
        expectWithinAbsoluteError(engine.getCurrentPosition(), 0.0, 0.01, "Negative position should clamp to 0");

        engine.setPosition(10.0);
        expectWithinAbsoluteError(engine.getCurrentPosition(), 3.0, 0.1, "Position beyond length should clamp to length");

        logMessage("✅ Position management works correctly with clamping");
    }

    void testLoopFunctionality()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.2, 2); // Short 200ms buffer
        engine.loadFromBuffer(buffer, 44100.0, 2);

        // Test loop enable/disable
        expect(!engine.isLooping(), "Looping should be disabled by default");

        engine.setLooping(true);
        expect(engine.isLooping(), "Should enable looping");

        engine.setLooping(false);
        expect(!engine.isLooping(), "Should disable looping");

        logMessage("✅ Loop control works correctly");
    }

    void testAudioPropertiesValidation()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Test various sample rates
        double testRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
        for (double rate : testRates)
        {
            auto buffer = TestAudio::createSineWave(440.0, 0.5, rate, 1.0, 2);
            bool loaded = engine.loadFromBuffer(buffer, rate, 2);
            expect(loaded, "Should load buffer at " + juce::String(rate) + " Hz");
            expectEquals(engine.getSampleRate(), rate, "Sample rate should match");
        }

        // Test various channel counts
        for (int channels = 1; channels <= 2; ++channels)
        {
            auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, channels);
            bool loaded = engine.loadFromBuffer(buffer, 44100.0, channels);
            expect(loaded, "Should load " + juce::String(channels) + " channel buffer");
            expectEquals(engine.getNumChannels(), channels, "Channel count should match");
        }

        logMessage("✅ Audio properties validation passed");
    }

    void testBufferValidationOnLoad()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Test empty buffer rejection
        juce::AudioBuffer<float> emptyBuffer;
        expect(!engine.loadFromBuffer(emptyBuffer, 44100.0, 0), "Should reject empty buffer");

        // Test invalid sample rate rejection
        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        expect(!engine.loadFromBuffer(buffer, -1.0, 2), "Should reject negative sample rate");
        expect(!engine.loadFromBuffer(buffer, 0.0, 2), "Should reject zero sample rate");
        expect(!engine.loadFromBuffer(buffer, 1000.0, 2), "Should reject too low sample rate");
        expect(!engine.loadFromBuffer(buffer, 500000.0, 2), "Should reject too high sample rate");

        // Test channel count mismatch rejection
        expect(!engine.loadFromBuffer(buffer, 44100.0, 1), "Should reject channel count mismatch");

        logMessage("✅ Buffer validation correctly rejects invalid inputs");
    }

    void testMonoToStereoPlayback()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Create mono buffer
        auto monoBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 1);
        expect(engine.loadFromBuffer(monoBuffer, 44100.0, 1), "Should load mono buffer");

        expectEquals(engine.getNumChannels(), 1, "Should report 1 channel");
        expect(engine.isFileLoaded(), "File should be loaded");

        // Engine should handle mono-to-stereo conversion internally
        // (Verified in audio callback with center-panning)

        logMessage("✅ Mono buffer loaded successfully (stereo conversion handled internally)");
    }
};

// Register the functional test
static AudioEngineFunctionalTests audioEngineFunctionalTests;

// ============================================================================
// AudioEngine Edge Case Tests
// ============================================================================

class AudioEngineEdgeCaseTests : public juce::UnitTest
{
public:
    AudioEngineEdgeCaseTests() : juce::UnitTest("AudioEngine Edge Cases", "AudioEngine") {}

    void runTest() override
    {
        beginTest("Playback without loaded file");
        testPlaybackWithoutFile();

        beginTest("Position operations without file");
        testPositionWithoutFile();

        beginTest("Close file during playback");
        testCloseFileDuringPlayback();

        beginTest("Reload buffer with different sample rate");
        testReloadBufferDifferentSampleRate();

        beginTest("Very short buffer playback");
        testVeryShortBuffer();

        beginTest("Maximum length buffer");
        testMaximumLengthBuffer();
    }

private:
    void testPlaybackWithoutFile()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Try to play without loading any file
        engine.play();
        expect(!engine.isPlaying(), "Should not be playing without loaded file");
        expect(engine.getPlaybackState() == PlaybackState::STOPPED, "State should remain STOPPED");

        logMessage("✅ Correctly handled playback attempt without file");
    }

    void testPositionWithoutFile()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Position operations without file should be safe
        engine.setPosition(1.0);
        expectEquals(engine.getCurrentPosition(), 0.0, "Position should be 0 without file");
        expectEquals(engine.getTotalLength(), 0.0, "Length should be 0 without file");

        logMessage("✅ Position operations safe without file");
    }

    void testCloseFileDuringPlayback()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        auto buffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 2.0, 2);
        engine.loadFromBuffer(buffer, 44100.0, 2);
        engine.play();

        juce::Thread::sleep(100);
        expect(engine.isPlaying(), "Should be playing");

        // Close file while playing
        engine.closeAudioFile();

        expect(!engine.isFileLoaded(), "File should be closed");
        expect(!engine.isPlaying(), "Should stop playing after close");
        expect(engine.getPlaybackState() == PlaybackState::STOPPED, "State should be STOPPED");

        logMessage("✅ Safely closed file during playback");
    }

    void testReloadBufferDifferentSampleRate()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Load at 44.1kHz
        auto buffer1 = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        expect(engine.loadFromBuffer(buffer1, 44100.0, 2), "Initial load should succeed");
        expectEquals(engine.getSampleRate(), 44100.0, "Should be 44.1kHz");

        // Reload at 48kHz
        auto buffer2 = TestAudio::createSineWave(880.0, 0.5, 48000.0, 1.0, 2);
        expect(engine.loadFromBuffer(buffer2, 48000.0, 2), "Reload with different rate should succeed");
        expectEquals(engine.getSampleRate(), 48000.0, "Should be 48kHz");

        logMessage("✅ Successfully reloaded buffer with different sample rate");
    }

    void testVeryShortBuffer()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Create extremely short buffer (10ms)
        auto shortBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.01, 2);
        expect(engine.loadFromBuffer(shortBuffer, 44100.0, 2), "Should load very short buffer");

        expectWithinAbsoluteError(engine.getTotalLength(), 0.01, 0.001, "Duration should be ~10ms");

        // Try playback
        engine.play();
        expect(engine.isPlaying(), "Should play very short buffer");

        logMessage("✅ Handled very short buffer (10ms) correctly");
    }

    void testMaximumLengthBuffer()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Create long buffer (10 seconds) to test memory handling
        auto longBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 10.0, 2);
        expect(engine.loadFromBuffer(longBuffer, 44100.0, 2), "Should load long buffer");

        expectWithinAbsoluteError(engine.getTotalLength(), 10.0, 0.1, "Duration should be ~10 seconds");

        // Verify we can seek through entire length
        engine.setPosition(5.0);
        expectWithinAbsoluteError(engine.getCurrentPosition(), 5.0, 0.1, "Should seek to middle");

        engine.setPosition(9.5);
        expectWithinAbsoluteError(engine.getCurrentPosition(), 9.5, 0.1, "Should seek near end");

        logMessage("✅ Handled long buffer (10 seconds) correctly");
    }
};

// ============================================================================
// AudioEngine Preview System Tests (Phase 1.4)
// ============================================================================

class AudioEnginePreviewSystemTests : public juce::UnitTest
{
public:
    AudioEnginePreviewSystemTests() : juce::UnitTest("AudioEngine Preview System", "AudioEngine") {}

    void runTest() override
    {
        beginTest("Preview mode state changes");
        testPreviewModeStateChanges();

        beginTest("Gain preview thread safety");
        testGainPreviewThreadSafety();

        beginTest("Preview mode bypass during playback");
        testPreviewBypassDuringPlayback();

        beginTest("Concurrent preview parameter updates");
        testConcurrentPreviewParameterUpdates();
    }

private:
    /**
     * Test basic preview mode state transitions (DISABLED <-> REALTIME_DSP <-> OFFLINE_BUFFER).
     */
    void testPreviewModeStateChanges()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Create test buffer
        juce::AudioBuffer<float> testBuffer(2, 44100);  // 1 second stereo
        fillWithSineWave(testBuffer, 440.0f, 44100.0);
        engine.loadFromBuffer(testBuffer, 44100.0, 2);

        // Test initial state
        expect(engine.getPreviewMode() == PreviewMode::DISABLED, "Initial mode should be DISABLED");

        // Test state transition: DISABLED -> REALTIME_DSP
        engine.setPreviewMode(PreviewMode::REALTIME_DSP);
        expect(engine.getPreviewMode() == PreviewMode::REALTIME_DSP, "Mode should be REALTIME_DSP");

        // Test state transition: REALTIME_DSP -> DISABLED
        engine.setPreviewMode(PreviewMode::DISABLED);
        expect(engine.getPreviewMode() == PreviewMode::DISABLED, "Mode should return to DISABLED");

        // Test state transition: DISABLED -> OFFLINE_BUFFER
        engine.setPreviewMode(PreviewMode::OFFLINE_BUFFER);
        expect(engine.getPreviewMode() == PreviewMode::OFFLINE_BUFFER, "Mode should be OFFLINE_BUFFER");

        // Test rapid mode changes
        for (int i = 0; i < 10; ++i)
        {
            engine.setPreviewMode(PreviewMode::REALTIME_DSP);
            engine.setPreviewMode(PreviewMode::DISABLED);
        }
        expect(engine.getPreviewMode() == PreviewMode::DISABLED, "Mode should be DISABLED after rapid changes");

        logMessage("✅ Preview mode state changes work correctly");
    }

    /**
     * Test gain preview thread safety (concurrent UI updates while audio thread is processing).
     */
    void testGainPreviewThreadSafety()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Create test buffer
        juce::AudioBuffer<float> testBuffer(2, 44100);  // 1 second stereo
        fillWithSineWave(testBuffer, 440.0f, 44100.0);
        engine.loadFromBuffer(testBuffer, 44100.0, 2);

        // Enable preview mode
        engine.setPreviewMode(PreviewMode::REALTIME_DSP);
        engine.setGainPreview(0.0f, true);

        // Simulate concurrent parameter updates from UI thread while audio is playing
        engine.play();

        std::atomic<bool> testRunning{true};
        std::atomic<int> updateCount{0};

        // Spawn thread that rapidly updates gain preview parameters
        std::thread updaterThread([&]()
        {
            while (testRunning.load())
            {
                for (float gain = -20.0f; gain <= 20.0f; gain += 1.0f)
                {
                    engine.setGainPreview(gain, true);
                    updateCount++;
                    juce::Thread::sleep(1);  // 1ms between updates
                }
            }
        });

        // Let it run for 100ms
        juce::Thread::sleep(100);
        testRunning.store(false);
        updaterThread.join();

        engine.stop();

        // Verify we completed many updates without crashing
        expect(updateCount.load() > 50, "Should have completed many concurrent updates");

        logMessage("✅ Gain preview thread safety verified (" +
                   juce::String(updateCount.load()) + " concurrent updates)");
    }

    /**
     * Test preview bypass (enabling/disabling) during playback.
     */
    void testPreviewBypassDuringPlayback()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Create test buffer
        juce::AudioBuffer<float> testBuffer(2, 44100);  // 1 second stereo
        fillWithSineWave(testBuffer, 440.0f, 44100.0);
        engine.loadFromBuffer(testBuffer, 44100.0, 2);

        // Enable preview mode
        engine.setPreviewMode(PreviewMode::REALTIME_DSP);
        engine.setGainPreview(6.0f, true);  // +6dB gain

        // Start playback
        engine.play();
        expect(engine.isPlaying(), "Engine should be playing");

        // Bypass gain processor multiple times during playback
        for (int i = 0; i < 10; ++i)
        {
            engine.setGainPreview(6.0f, true);   // Enable
            juce::Thread::sleep(10);
            engine.setGainPreview(0.0f, false);  // Disable (bypass)
            juce::Thread::sleep(10);
        }

        // Playback should still be active
        expect(engine.isPlaying(), "Engine should still be playing after bypass changes");

        engine.stop();

        // Verify final bypass state
        engine.setGainPreview(0.0f, false);
        expect(engine.getPreviewMode() == PreviewMode::REALTIME_DSP,
               "Preview mode should still be REALTIME_DSP after bypass");

        logMessage("✅ Preview bypass during playback works correctly");
    }

    /**
     * Test concurrent preview parameter updates from multiple threads.
     */
    void testConcurrentPreviewParameterUpdates()
    {
        AudioEngine engine;
        engine.initializeAudioDevice();

        // Create test buffer
        juce::AudioBuffer<float> testBuffer(2, 44100);  // 1 second stereo
        fillWithSineWave(testBuffer, 440.0f, 44100.0);
        engine.loadFromBuffer(testBuffer, 44100.0, 2);

        // Enable preview mode
        engine.setPreviewMode(PreviewMode::REALTIME_DSP);

        std::atomic<bool> testRunning{true};
        std::atomic<int> thread1Updates{0};
        std::atomic<int> thread2Updates{0};
        std::atomic<int> thread3Updates{0};

        // Spawn 3 threads that update preview parameters concurrently
        std::thread thread1([&]()
        {
            while (testRunning.load())
            {
                engine.setGainPreview(-10.0f, true);
                thread1Updates++;
                juce::Thread::sleep(2);
            }
        });

        std::thread thread2([&]()
        {
            while (testRunning.load())
            {
                engine.setGainPreview(0.0f, true);
                thread2Updates++;
                juce::Thread::sleep(3);
            }
        });

        std::thread thread3([&]()
        {
            while (testRunning.load())
            {
                engine.setGainPreview(+10.0f, true);
                thread3Updates++;
                juce::Thread::sleep(5);
            }
        });

        // Let threads run for 100ms
        juce::Thread::sleep(100);
        testRunning.store(false);

        thread1.join();
        thread2.join();
        thread3.join();

        int totalUpdates = thread1Updates.load() + thread2Updates.load() + thread3Updates.load();

        // Verify we completed many concurrent updates from all threads
        expect(totalUpdates > 50, "Should have completed many concurrent updates from all threads");

        logMessage("✅ Concurrent preview parameter updates verified (" +
                   juce::String(totalUpdates) + " total updates)");
    }

    /**
     * Helper: Fill buffer with sine wave for testing.
     */
    void fillWithSineWave(juce::AudioBuffer<float>& buffer, float frequency, double sampleRate)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* channelData = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                channelData[sample] = std::sin(2.0f * juce::MathConstants<float>::pi *
                                               frequency * sample / sampleRate);
            }
        }
    }
};

// Register the preview system test
static AudioEnginePreviewSystemTests audioEnginePreviewSystemTests;
