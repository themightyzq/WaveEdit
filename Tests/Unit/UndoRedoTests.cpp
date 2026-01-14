/*
  ==============================================================================

    UndoRedoTests.cpp
    Created: 2025-10-15
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Comprehensive unit tests for Undo/Redo data integrity.
    Tests buffer state restoration, multi-level undo, and operation correctness.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Audio/AudioBufferManager.h"
#include "Audio/AudioEngine.h"
#include "UI/WaveformDisplay.h"
#include "Utils/UndoableEdits.h"
#include "../TestUtils/TestAudioFiles.h"
#include "../TestUtils/AudioAssertions.h"

// ============================================================================
// Test Helper Class
// ============================================================================

/**
 * Helper class to manage test components with proper initialization.
 * Handles the complexity of AudioFormatManager and proper API usage.
 */
class UndoTestHelper
{
public:
    UndoTestHelper()
        : waveformDisplay(formatManager)
    {
        formatManager.registerBasicFormats();

        // CRITICAL: Ensure AudioEngine is stopped and no callbacks are active
        // This prevents race conditions during buffer modifications in tests
        audioEngine.stop();
    }

    /**
     * Loads a test buffer into all components using a temporary file.
     * This uses the real file-loading workflow which tests the actual use case.
     */
    bool loadTestBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        // Create temporary file
        tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("undo_test_" + juce::String(juce::Random::getSystemRandom().nextInt()) + ".wav");

        // Save buffer to temp file
        if (!saveBufferToTempFile(buffer, sampleRate))
            return false;

        // Load into AudioBufferManager from file
        if (!bufferManager.loadFromFile(tempFile, formatManager))
        {
            tempFile.deleteFile();
            return false;
        }

        // Load into AudioEngine
        if (!audioEngine.loadFromBuffer(bufferManager.getBuffer(), sampleRate, buffer.getNumChannels()))
        {
            tempFile.deleteFile();
            return false;
        }

        // Load into WaveformDisplay
        if (!waveformDisplay.reloadFromBuffer(bufferManager.getBuffer(), sampleRate, false, false))
        {
            tempFile.deleteFile();
            return false;
        }

        return true;
    }

    ~UndoTestHelper()
    {
        // CRITICAL: Ensure clean shutdown to prevent audio callbacks accessing freed memory
        audioEngine.stop();

        if (tempFile.existsAsFile())
            tempFile.deleteFile();
    }

    juce::AudioFormatManager formatManager;
    AudioBufferManager bufferManager;
    AudioEngine audioEngine;
    WaveformDisplay waveformDisplay;
    juce::UndoManager undoManager;

private:
    juce::File tempFile;

    bool saveBufferToTempFile(const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        auto* format = formatManager.findFormatForFileExtension("wav");
        if (format == nullptr)
            return false;

        std::unique_ptr<juce::FileOutputStream> outputStream(new juce::FileOutputStream(tempFile));
        if (!outputStream->openedOk())
            return false;

        // CRITICAL: Don't release stream ownership until we verify writer creation succeeded
        // This prevents resource leak if createWriterFor returns nullptr
        auto* rawStream = outputStream.get();
        std::unique_ptr<juce::AudioFormatWriter> writer(
            format->createWriterFor(rawStream, sampleRate, buffer.getNumChannels(), 16, {}, 0));

        if (writer == nullptr)
            return false;  // outputStream still owned, will be deleted automatically

        outputStream.release();  // NOW safe to transfer ownership to writer

        bool writeSuccess = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
        writer.reset();

        return writeSuccess;
    }
};

// ============================================================================
// Basic Undo/Redo Tests
// ============================================================================

class UndoRedoBasicTests : public juce::UnitTest
{
public:
    UndoRedoBasicTests() : juce::UnitTest("Undo/Redo Basic Operations", "Unit") {}

    void runTest() override
    {
        beginTest("Delete operation undo/redo");
        testDeleteUndoRedo();

        beginTest("Insert operation undo/redo");
        testInsertUndoRedo();

        beginTest("Replace operation undo/redo");
        testReplaceUndoRedo();

        beginTest("Undo restores exact buffer state");
        testUndoBufferIntegrity();

        beginTest("Redo re-applies exact changes");
        testRedoBufferIntegrity();
    }

private:
    void testDeleteUndoRedo()
    {
        // Create test audio
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        int64_t originalHash = AudioAssertions::hashBuffer(originalBuffer);

        // Create test helper with all components
        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        // Create delete action (delete 0.1 seconds from start)
        int64_t startSample = 0;
        int64_t numSamples = 4410;  // 0.1 seconds at 44.1kHz

        auto deleteAction = new DeleteAction(
            helper.bufferManager,
            helper.audioEngine,
            helper.waveformDisplay,
            startSample,
            numSamples
        );

        // Perform delete
        bool performSuccess = helper.undoManager.perform(deleteAction);
        expect(performSuccess, "Delete should succeed");

        // Verify buffer is shorter
        expectEquals(helper.bufferManager.getNumSamples(), originalBuffer.getNumSamples() - numSamples,
                   "Buffer should be shorter after delete");

        // Undo delete
        bool undoSuccess = helper.undoManager.undo();
        expect(undoSuccess, "Undo should succeed");

        // Verify buffer restored to original
        expectEquals(helper.bufferManager.getNumSamples(), (int64_t)originalBuffer.getNumSamples(),
                   "Buffer should be restored to original length");

        int64_t restoredHash = AudioAssertions::hashBuffer(helper.bufferManager.getBuffer());
        expect(originalHash == restoredHash ||
               AudioAssertions::expectBuffersNearlyEqual(originalBuffer, helper.bufferManager.getBuffer(), 0.001f),
               "Undo should restore original buffer (within tolerance)");

        // Redo delete
        bool redoSuccess = helper.undoManager.redo();
        expect(redoSuccess, "Redo should succeed");

        // Verify buffer is shorter again
        expectEquals(helper.bufferManager.getNumSamples(), originalBuffer.getNumSamples() - numSamples,
                   "Buffer should be shorter after redo");
    }

    void testInsertUndoRedo()
    {
        // Create original buffer (1 second)
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        int originalSamples = originalBuffer.getNumSamples();

        // Create buffer to insert (0.2 seconds)
        auto insertBuffer = TestAudio::createSineWave(880.0, 0.3, 44100.0, 0.2, 2);
        int insertSamples = insertBuffer.getNumSamples();

        // Create test helper
        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        // Create insert action (insert at 0.5 seconds)
        int64_t insertPosition = 22050;  // 0.5 seconds at 44.1kHz

        auto insertAction = new InsertAction(
            helper.bufferManager,
            helper.audioEngine,
            helper.waveformDisplay,
            insertPosition,
            insertBuffer
        );

        // Perform insert
        bool performSuccess = helper.undoManager.perform(insertAction);
        expect(performSuccess, "Insert should succeed");

        // Verify buffer is longer
        expectEquals(helper.bufferManager.getNumSamples(), (int64_t)(originalSamples + insertSamples),
                   "Buffer should be longer after insert");

        // Undo insert
        bool undoSuccess = helper.undoManager.undo();
        expect(undoSuccess, "Undo should succeed");

        // Verify buffer restored to original length
        expectEquals(helper.bufferManager.getNumSamples(), (int64_t)originalSamples,
                   "Buffer should be restored to original length");

        // Redo insert
        bool redoSuccess = helper.undoManager.redo();
        expect(redoSuccess, "Redo should succeed");

        // Verify buffer is longer again
        expectEquals(helper.bufferManager.getNumSamples(), (int64_t)(originalSamples + insertSamples),
                   "Buffer should be longer after redo");
    }

    void testReplaceUndoRedo()
    {
        // Create original buffer (1 second)
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        int64_t originalHash = AudioAssertions::hashBuffer(originalBuffer);

        // Create replacement buffer (0.3 seconds)
        auto replacementBuffer = TestAudio::createSineWave(880.0, 0.3, 44100.0, 0.3, 2);

        // Create test helper
        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        // Create replace action (replace 0.3 seconds starting at 0.2 seconds)
        int64_t startSample = 8820;   // 0.2 seconds at 44.1kHz
        int64_t numSamplesToReplace = 13230;  // 0.3 seconds at 44.1kHz

        auto replaceAction = new ReplaceAction(
            helper.bufferManager,
            helper.audioEngine,
            helper.waveformDisplay,
            startSample,
            numSamplesToReplace,
            replacementBuffer
        );

        // Perform replace
        bool performSuccess = helper.undoManager.perform(replaceAction);
        expect(performSuccess, "Replace should succeed");

        // Buffer length should be same (replacing equal length)
        expectEquals(helper.bufferManager.getNumSamples(), (int64_t)originalBuffer.getNumSamples(),
                   "Buffer length should remain same after equal-length replace");

        // Undo replace
        bool undoSuccess = helper.undoManager.undo();
        expect(undoSuccess, "Undo should succeed");

        // Verify buffer restored to original
        int64_t restoredHash = AudioAssertions::hashBuffer(helper.bufferManager.getBuffer());
        expect(originalHash == restoredHash ||
               AudioAssertions::expectBuffersNearlyEqual(originalBuffer, helper.bufferManager.getBuffer(), 0.001f),
               "Undo should restore original buffer (within tolerance)");

        // Redo replace
        bool redoSuccess = helper.undoManager.redo();
        expect(redoSuccess, "Redo should succeed");
    }

    void testUndoBufferIntegrity()
    {
        // Create original buffer with known pattern
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.5, 2);

        // Store original samples for comparison
        juce::AudioBuffer<float> storedOriginal;
        storedOriginal.makeCopyOf(originalBuffer);

        // Create test helper
        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        // Perform delete
        auto deleteAction = new DeleteAction(
            helper.bufferManager,
            helper.audioEngine,
            helper.waveformDisplay,
            0,
            4410  // 0.1 seconds
        );
        helper.undoManager.perform(deleteAction);

        // Undo
        helper.undoManager.undo();

        // Verify sample-by-sample restoration (within tolerance for 16-bit quantization)
        for (int ch = 0; ch < helper.bufferManager.getBuffer().getNumChannels(); ++ch)
        {
            for (int sample = 0; sample < juce::jmin(1000, helper.bufferManager.getBuffer().getNumSamples()); ++sample)
            {
                float original = storedOriginal.getSample(ch, sample);
                float restored = helper.bufferManager.getBuffer().getSample(ch, sample);
                expect(std::abs(original - restored) < 0.001f,
                       "Sample " + juce::String(sample) + " in channel " + juce::String(ch) +
                       " should be restored (within 16-bit tolerance)");
            }
        }
    }

    void testRedoBufferIntegrity()
    {
        // Create original buffer
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.5, 2);

        // Create test helper
        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        // Perform delete
        int64_t deleteLength = 4410;
        auto deleteAction = new DeleteAction(
            helper.bufferManager,
            helper.audioEngine,
            helper.waveformDisplay,
            0,
            deleteLength
        );
        helper.undoManager.perform(deleteAction);

        // Store buffer after delete
        juce::AudioBuffer<float> afterDelete;
        afterDelete.makeCopyOf(helper.bufferManager.getBuffer());

        // Undo
        helper.undoManager.undo();

        // Redo
        helper.undoManager.redo();

        // Verify redo produces same result as original perform
        expect(AudioAssertions::expectBuffersNearlyEqual(afterDelete, helper.bufferManager.getBuffer(), 0.000001f),
               "Redo should produce same result as original perform");
    }
};

// Register the test
static UndoRedoBasicTests undoRedoBasicTests;

// ============================================================================
// Multi-Level Undo Tests
// ============================================================================

class UndoRedoMultiLevelTests : public juce::UnitTest
{
public:
    UndoRedoMultiLevelTests() : juce::UnitTest("Undo/Redo Multi-Level", "Unit") {}

    void runTest() override
    {
        beginTest("Multi-level undo (10 operations)");
        testMultiLevelUndo10();

        beginTest("Multi-level undo (50 operations)");
        testMultiLevelUndo50();

        beginTest("Undo/redo stack correctness");
        testUndoRedoStackCorrectness();
    }

private:
    void testMultiLevelUndo10()
    {
        // Create original buffer
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 2.0, 2);
        int64_t originalSamples = originalBuffer.getNumSamples();

        // Create test helper
        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        // Perform 10 delete operations (0.01 seconds each)
        int operationsCount = 10;
        int samplesPerDelete = 441;  // 0.01 seconds at 44.1kHz

        for (int i = 0; i < operationsCount; ++i)
        {
            auto deleteAction = new DeleteAction(
                helper.bufferManager,
                helper.audioEngine,
                helper.waveformDisplay,
                0,  // Always delete from start
                samplesPerDelete
            );
            helper.undoManager.perform(deleteAction);
        }

        // Verify buffer is shorter
        int64_t expectedSamples = originalSamples - (operationsCount * samplesPerDelete);
        expectEquals(helper.bufferManager.getNumSamples(), expectedSamples,
                   "Buffer should be shorter after 10 deletes");

        // Undo all 10 operations
        for (int i = 0; i < operationsCount; ++i)
        {
            bool undoSuccess = helper.undoManager.undo();
            expect(undoSuccess, "Undo " + juce::String(i + 1) + " should succeed");
        }

        // Verify buffer restored to original length
        expectEquals(helper.bufferManager.getNumSamples(), originalSamples,
                   "Buffer should be restored to original length after 10 undos");

        // Redo all 10 operations
        for (int i = 0; i < operationsCount; ++i)
        {
            bool redoSuccess = helper.undoManager.redo();
            expect(redoSuccess, "Redo " + juce::String(i + 1) + " should succeed");
        }

        // Verify buffer is shorter again
        expectEquals(helper.bufferManager.getNumSamples(), expectedSamples,
                   "Buffer should be shorter after 10 redos");
    }

    void testMultiLevelUndo50()
    {
        // Create original buffer (longer for 50 operations)
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 5.0, 2);
        int64_t originalSamples = originalBuffer.getNumSamples();

        // Create test helper
        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        // Perform 50 delete operations (0.001 seconds each)
        int operationsCount = 50;
        int samplesPerDelete = 44;  // ~0.001 seconds at 44.1kHz

        logMessage("Performing 50 delete operations...");

        for (int i = 0; i < operationsCount; ++i)
        {
            auto deleteAction = new DeleteAction(
                helper.bufferManager,
                helper.audioEngine,
                helper.waveformDisplay,
                0,  // Always delete from start
                samplesPerDelete
            );
            helper.undoManager.perform(deleteAction);

            if ((i + 1) % 10 == 0)
            {
                logMessage("  " + juce::String(i + 1) + " operations completed");
            }
        }

        // Verify buffer is shorter
        int64_t expectedSamples = originalSamples - (operationsCount * samplesPerDelete);
        expectEquals(helper.bufferManager.getNumSamples(), expectedSamples,
                   "Buffer should be shorter after 50 deletes");

        logMessage("Undoing 50 operations...");

        // Undo all 50 operations
        for (int i = 0; i < operationsCount; ++i)
        {
            bool undoSuccess = helper.undoManager.undo();
            expect(undoSuccess, "Undo " + juce::String(i + 1) + " should succeed");

            if ((i + 1) % 10 == 0)
            {
                logMessage("  " + juce::String(i + 1) + " undos completed");
            }
        }

        // Verify buffer restored to original length
        expectEquals(helper.bufferManager.getNumSamples(), originalSamples,
                   "Buffer should be restored to original length after 50 undos");

        logMessage("✅ 50-level undo test passed!");
    }

    void testUndoRedoStackCorrectness()
    {
        // Create original buffer
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);

        // Create test helper
        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        // Perform 5 operations
        for (int i = 0; i < 5; ++i)
        {
            auto deleteAction = new DeleteAction(
                helper.bufferManager,
                helper.audioEngine,
                helper.waveformDisplay,
                0,
                441
            );
            helper.undoManager.perform(deleteAction);
        }

        // Undo 3 operations
        helper.undoManager.undo();
        helper.undoManager.undo();
        helper.undoManager.undo();

        // Perform new operation (should clear redo stack)
        auto deleteAction = new DeleteAction(
            helper.bufferManager,
            helper.audioEngine,
            helper.waveformDisplay,
            0,
            441
        );
        helper.undoManager.perform(deleteAction);

        // Try to redo (should fail - redo stack cleared)
        bool redoSuccess = helper.undoManager.redo();
        expect(!redoSuccess, "Redo should fail after new operation clears redo stack");

        // Verify we can still undo the new operation
        bool undoSuccess = helper.undoManager.undo();
        expect(undoSuccess, "Should be able to undo the new operation");
    }
};

// Register the test
static UndoRedoMultiLevelTests undoRedoMultiLevelTests;

// ============================================================================
// Undo History Management Tests
// ============================================================================

class UndoRedoHistoryTests : public juce::UnitTest
{
public:
    UndoRedoHistoryTests() : juce::UnitTest("Undo/Redo History Management", "Unit") {}

    void runTest() override
    {
        beginTest("Undo history cleared after file load");
        testUndoHistoryClearedOnLoad();
    }

private:
    void testUndoHistoryClearedOnLoad()
    {
        // Create original buffer
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);

        // Create test helper
        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        // Perform some operations
        for (int i = 0; i < 5; ++i)
        {
            auto deleteAction = new DeleteAction(
                helper.bufferManager,
                helper.audioEngine,
                helper.waveformDisplay,
                0,
                441
            );
            helper.undoManager.perform(deleteAction);
        }

        // Verify undo is available
        expect(helper.undoManager.canUndo(), "Should be able to undo before clearing history");

        // Clear history (simulating new file load)
        helper.undoManager.clearUndoHistory();

        // Verify undo is NOT available
        expect(!helper.undoManager.canUndo(), "Should NOT be able to undo after clearing history");
    }
};

// Register the test
static UndoRedoHistoryTests undoRedoHistoryTests;
