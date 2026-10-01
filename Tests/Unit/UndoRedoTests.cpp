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

#include <cmath>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Audio/AudioBufferManager.h"
#include "Audio/AudioEngine.h"
#include "UI/WaveformDisplay.h"
#include "Utils/UndoableEdits.h"
#include "Utils/RegionManager.h"
#include "Utils/Region.h"
#include "Utils/MarkerManager.h"
#include "Utils/Marker.h"
#include "Utils/UndoActions/ChannelUndoActions.h"
#include "Utils/UndoActions/PluginUndoActions.h"
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

        // v0.9.1: undo actions now report real KiB-based sizes via
        // UndoMemory::unitsForBytes() instead of ad hoc small numbers, so
        // this bare UndoManager needs the same real budget Document wires
        // up in production (Document.cpp) -- JUCE's tiny 30000-unit/30-
        // transaction constructor default no longer has headroom for
        // realistic sizes and would prematurely trim the many small,
        // separately-transacted edits these tests perform (see
        // testMultiLevelUndo50 below).
        undoManager.setMaxNumberOfStoredUnits(UndoMemory::maxUnits(), UndoMemory::kMinTransactionsKept);
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
            // Each operation is its own undoable transaction. Without a fresh
            // transaction boundary, JUCE's UndoManager coalesces consecutive
            // perform() calls so a single undo() rolls all of them back.
            helper.undoManager.beginNewTransaction(
                "Delete " + juce::String(i + 1));

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
            // See note in testMultiLevelUndo10: explicit transaction per op so
            // each undo() unwinds one delete.
            helper.undoManager.beginNewTransaction(
                "Delete " + juce::String(i + 1));

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

        // Perform 5 operations, each in its own transaction so 5 undos are
        // required to unwind them all.
        for (int i = 0; i < 5; ++i)
        {
            helper.undoManager.beginNewTransaction(
                "Delete " + juce::String(i + 1));

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
        helper.undoManager.beginNewTransaction("Delete (post-undo)");
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

// ============================================================================
// Region-Aware Insert/Replace Undo Tests
//
// Regression coverage for the finding that InsertAction/ReplaceAction never
// shifted regions the way DeleteAction always has, leaving region start/end
// samples pointing at the pre-edit timeline after any paste. Both actions
// now take an optional RegionManager* and shift/grow/undo regions the same
// way DeleteAction does.
// ============================================================================

class RegionAwareEditUndoTests : public juce::UnitTest
{
public:
    RegionAwareEditUndoTests() : juce::UnitTest("Region-Aware Insert/Replace Undo", "Unit") {}

    void runTest() override
    {
        beginTest("InsertAction shifts/grows regions and undo restores them");
        testInsertShiftsRegions();

        beginTest("ReplaceAction shifts trailing regions on a length-changing replace");
        testReplaceShiftsRegionsOnLengthChange();

        beginTest("ReplaceAction removes an overlapping region rather than leaving it misaligned");
        testReplaceRemovesOverlappingRegion();
    }

private:
    static const Region* findRegionByName(const RegionManager& regions, const juce::String& name)
    {
        for (const auto& r : regions.getAllRegions())
            if (r.getName() == name)
                return &r;
        return nullptr;
    }

    void testInsertShiftsRegions()
    {
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);   // 44100 samples
        auto insertBuffer = TestAudio::createSineWave(880.0, 0.3, 44100.0, 0.2, 2);      // 8820 samples
        const int64_t insertedLength = insertBuffer.getNumSamples();

        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        RegionManager regions;
        regions.addRegion(Region("Before", 0, 10000));         // entirely before the insert point
        regions.addRegion(Region("After", 20000, 30000));      // entirely after the insert point
        regions.addRegion(Region("Straddle", 12000, 18000));   // insert point falls inside it

        const int64_t insertPosition = 15000;

        auto insertAction = new InsertAction(
            helper.bufferManager,
            helper.audioEngine,
            helper.waveformDisplay,
            insertPosition,
            insertBuffer,
            &regions);

        expect(helper.undoManager.perform(insertAction), "Insert should succeed");
        expectEquals(regions.getNumRegions(), 3, "no regions should be dropped by an insert");

        if (const auto* r = findRegionByName(regions, "Before"))
        {
            expectEquals((int) r->getStartSample(), 0, "Before.start unaffected by a later insert");
            expectEquals((int) r->getEndSample(), 10000, "Before.end unaffected by a later insert");
        }
        else
            expect(false, "'Before' region missing after insert");

        if (const auto* r = findRegionByName(regions, "After"))
        {
            expectEquals((int) r->getStartSample(), (int) (20000 + insertedLength),
                       "After.start shifts forward by the inserted length");
            expectEquals((int) r->getEndSample(), (int) (30000 + insertedLength),
                       "After.end shifts forward by the inserted length");
        }
        else
            expect(false, "'After' region missing after insert");

        if (const auto* r = findRegionByName(regions, "Straddle"))
        {
            expectEquals((int) r->getStartSample(), 12000,
                       "Straddle.start unaffected (insertion point is inside the region)");
            expectEquals((int) r->getEndSample(), (int) (18000 + insertedLength),
                       "Straddle.end grows to include the newly inserted audio");
        }
        else
            expect(false, "'Straddle' region missing after insert");

        expect(helper.undoManager.undo(), "Undo should succeed");
        expectEquals(regions.getNumRegions(), 3, "all 3 regions restored after undo");
        if (const auto* r = findRegionByName(regions, "Straddle"))
            expectEquals((int) r->getEndSample(), 18000, "Straddle.end restored to its pre-insert value");
        if (const auto* r = findRegionByName(regions, "After"))
            expectEquals((int) r->getStartSample(), 20000, "After.start restored to its pre-insert value");
    }

    void testReplaceShiftsRegionsOnLengthChange()
    {
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);        // 44100 samples
        auto replacementBuffer = TestAudio::createSineWave(880.0, 0.3, 44100.0, 0.4, 2);     // 17640 samples (longer)

        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        RegionManager regions;
        regions.addRegion(Region("Before", 0, 5000));       // entirely before the replaced range
        regions.addRegion(Region("After", 15000, 20000));   // entirely after the replaced range

        const int64_t startSample = 8000;
        const int64_t numSamplesToReplace = 4000;            // replaced range: [8000, 12000)
        const int64_t delta = replacementBuffer.getNumSamples() - numSamplesToReplace;

        auto replaceAction = new ReplaceAction(
            helper.bufferManager,
            helper.audioEngine,
            helper.waveformDisplay,
            startSample,
            numSamplesToReplace,
            replacementBuffer,
            &regions);

        expect(helper.undoManager.perform(replaceAction), "Replace should succeed");
        expectEquals(regions.getNumRegions(), 2, "non-overlapping regions must survive a replace");

        if (const auto* r = findRegionByName(regions, "Before"))
        {
            expectEquals((int) r->getStartSample(), 0, "Before.start unaffected by a later replace");
            expectEquals((int) r->getEndSample(), 5000, "Before.end unaffected by a later replace");
        }
        else
            expect(false, "'Before' region missing after replace");

        if (const auto* r = findRegionByName(regions, "After"))
        {
            expectEquals((int) r->getStartSample(), (int) (15000 + delta),
                       "After.start shifts by the length delta introduced by the replace");
            expectEquals((int) r->getEndSample(), (int) (20000 + delta),
                       "After.end shifts by the length delta introduced by the replace");
        }
        else
            expect(false, "'After' region missing after replace");

        expect(helper.undoManager.undo(), "Undo should succeed");
        if (const auto* r = findRegionByName(regions, "After"))
            expectEquals((int) r->getStartSample(), 15000, "After.start restored to its pre-replace value");
    }

    void testReplaceRemovesOverlappingRegion()
    {
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);
        auto replacementBuffer = TestAudio::createSineWave(880.0, 0.3, 44100.0, 0.1, 2);  // 4410 samples

        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        RegionManager regions;
        // Overlaps the replaced range [8000, 12000) -- its audio content changed.
        regions.addRegion(Region("Overlap", 9000, 11000));

        auto replaceAction = new ReplaceAction(
            helper.bufferManager,
            helper.audioEngine,
            helper.waveformDisplay,
            8000,
            4000,
            replacementBuffer,
            &regions);

        expect(helper.undoManager.perform(replaceAction), "Replace should succeed");
        expectEquals(regions.getNumRegions(), 0,
                   "a region overlapping the replaced range is removed rather than left misaligned");

        // Undo must restore the original region, matching DeleteAction's convention.
        expect(helper.undoManager.undo(), "Undo should succeed");
        expectEquals(regions.getNumRegions(), 1, "overlapping region restored after undo");
        if (const auto* r = findRegionByName(regions, "Overlap"))
        {
            expectEquals((int) r->getStartSample(), 9000, "restored region start matches pre-replace value");
            expectEquals((int) r->getEndSample(), 11000, "restored region end matches pre-replace value");
        }
    }
};

// Register the test
static RegionAwareEditUndoTests regionAwareEditUndoTests;

// ============================================================================
// Length-Preserving Action Playback Preservation Tests
//
// UndoableEditBase::updatePlaybackAndDisplay() stops playback deterministically
// because Delete/Insert/Replace change the buffer's length, and naively
// preserving a raw position across a length change would silently resume
// playback on unrelated audio content (see the "Buffer-length-changing edits
// stop playback deterministically at scale" test in LargeOperationUndoTests.cpp,
// which locks that behavior in for those three actions specifically).
//
// ChannelConvertAction, ApplyDynamicParametricEQAction,
// and a no-tail ApplyPluginChainAction are different: they never change the
// sample count, so there is no content-position hazard, and per CLAUDE.md
// §6.5 they should preserve playback via
// updatePlaybackAndDisplayPreservingPlayback() the same way GainUndoAction
// and friends do. A plugin chain render WITH an effect tail, however, is
// length-changing, so it must fall back to the deterministic stop -- both
// halves are covered below.
// ============================================================================

class LengthPreservingActionPlaybackTests : public juce::UnitTest
{
public:
    LengthPreservingActionPlaybackTests()
        : juce::UnitTest("Length-Preserving Actions Preserve Playback", "Unit") {}

    void runTest() override
    {
        beginTest("ChannelConvertAction preserves isPlaying + position (channel count changes, sample count doesn't)");
        testChannelConvertPreservesPlayback();

        beginTest("ApplyDynamicParametricEQAction preserves isPlaying + position");
        testDynamicParametricEQPreservesPlayback();

        beginTest("ApplyPluginChainAction preserves playback with no effect tail, "
                  "but stops deterministically when a tail extends the buffer");
        testPluginChainActionPlaybackDependsOnTail();
    }

private:
    void testChannelConvertPreservesPlayback()
    {
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);  // stereo

        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        helper.audioEngine.play();
        helper.audioEngine.setPosition(0.4);
        expect(helper.audioEngine.isPlaying(), "Engine should be playing before the conversion");

        // Downmix to mono: same sample count, different channel count.
        juce::AudioBuffer<float> monoBuffer(1, originalBuffer.getNumSamples());
        monoBuffer.clear();
        for (int ch = 0; ch < originalBuffer.getNumChannels(); ++ch)
            monoBuffer.addFrom(0, 0, originalBuffer, ch, 0, originalBuffer.getNumSamples(),
                               1.0f / originalBuffer.getNumChannels());

        auto* convertAction = new ChannelConvertAction(
            helper.bufferManager, helper.audioEngine, helper.waveformDisplay, monoBuffer);

        expect(helper.undoManager.perform(convertAction), "Channel conversion should succeed");
        expect(helper.audioEngine.isPlaying(), "Channel conversion must preserve the playing state");
        expectWithinAbsoluteError(helper.audioEngine.getCurrentPosition(), 0.4, 0.05,
                                  "Channel conversion must preserve playback position");

        expect(helper.undoManager.undo(), "Undo of channel conversion should succeed");
        expect(helper.audioEngine.isPlaying(), "Undo of channel conversion must also preserve playing state");
    }

    void testDynamicParametricEQPreservesPlayback()
    {
        auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);

        UndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

        helper.audioEngine.play();
        helper.audioEngine.setPosition(0.3);
        expect(helper.audioEngine.isPlaying(), "Engine should be playing before the EQ edit");

        auto* eqAction = new ApplyDynamicParametricEQAction(
            helper.bufferManager, helper.audioEngine, helper.waveformDisplay,
            0, helper.bufferManager.getNumSamples(), DynamicParametricEQ::Parameters());

        expect(helper.undoManager.perform(eqAction), "Dynamic parametric EQ apply should succeed");
        expect(helper.audioEngine.isPlaying(), "Dynamic parametric EQ must preserve the playing state");
        expectWithinAbsoluteError(helper.audioEngine.getCurrentPosition(), 0.3, 0.05,
                                  "Dynamic parametric EQ must preserve playback position");

        expect(helper.undoManager.undo(), "Undo of dynamic parametric EQ should succeed");
        expect(helper.audioEngine.isPlaying(), "Undo of dynamic parametric EQ must also preserve playing state");
    }

    void testPluginChainActionPlaybackDependsOnTail()
    {
        // --- No tail: processed audio is exactly the replaced length -> preserve ---
        {
            auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);

            UndoTestHelper helper;
            expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

            helper.audioEngine.play();
            helper.audioEngine.setPosition(0.3);

            const int64_t numSamples = helper.bufferManager.getNumSamples();
            auto processed = helper.bufferManager.getAudioRange(0, numSamples);  // identity "processing"

            auto* chainAction = new ApplyPluginChainAction(
                helper.bufferManager, helper.audioEngine, helper.waveformDisplay,
                0, numSamples, processed, "Identity (no tail)");

            expect(helper.undoManager.perform(chainAction), "Plugin chain apply (no tail) should succeed");
            expect(helper.audioEngine.isPlaying(),
                  "A no-tail plugin chain apply must preserve the playing state");
            expectWithinAbsoluteError(helper.audioEngine.getCurrentPosition(), 0.3, 0.05,
                                      "A no-tail plugin chain apply must preserve playback position");
        }

        // --- With tail: processed audio is longer than the replaced range -> stop ---
        {
            auto originalBuffer = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);

            UndoTestHelper helper;
            expect(helper.loadTestBuffer(originalBuffer, 44100.0), "Should load test buffer");

            helper.audioEngine.play();
            helper.audioEngine.setPosition(0.3);

            const int64_t numSamples = helper.bufferManager.getNumSamples();
            const int64_t tailSamples = 4410;  // 0.1s @ 44.1kHz reverb/delay tail
            juce::AudioBuffer<float> processed(originalBuffer.getNumChannels(),
                                                static_cast<int>(numSamples + tailSamples));
            processed.clear();
            for (int ch = 0; ch < originalBuffer.getNumChannels(); ++ch)
                processed.copyFrom(ch, 0, helper.bufferManager.getBuffer(), ch, 0,
                                   static_cast<int>(numSamples));

            auto* chainAction = new ApplyPluginChainAction(
                helper.bufferManager, helper.audioEngine, helper.waveformDisplay,
                0, numSamples, processed, "Reverb (with tail)");

            expect(helper.undoManager.perform(chainAction), "Plugin chain apply (with tail) should succeed");
            expect(! helper.audioEngine.isPlaying(),
                  "A tail-extending plugin chain apply changes buffer length and must stop "
                  "playback deterministically, matching Delete/Insert/Replace");
        }
    }
};

static LengthPreservingActionPlaybackTests lengthPreservingActionPlaybackTests;

// ============================================================================
// H3: Insert/Replace must shift REGIONS *and* MARKERS on a length-changing
// paste/splice, and restore both exactly on undo. Markers are points; a marker
// exactly at the insert / replace boundary is the interesting case. Verified as
// a full perform -> undo -> redo round trip. Displays are passed as nullptr --
// the shift logic operates on the managers, which is what this asserts.
// ============================================================================
class InsertReplaceRegionMarkerShiftTests : public juce::UnitTest
{
public:
    InsertReplaceRegionMarkerShiftTests()
        : juce::UnitTest("Insert/Replace Region+Marker Shift (H3)", "Unit") {}

    void runTest() override
    {
        beginTest("Insert shifts regions and markers (round trip, boundary marker)");
        testInsertShift();

        beginTest("Replace (grow) shifts/removes regions and markers (round trip)");
        testReplaceGrowShift();

        beginTest("Replace (shrink) shifts regions and markers left (round trip)");
        testReplaceShrinkShift();
    }

private:
    // Exact-size stereo buffer (a simple ramp): the shift logic is content-
    // agnostic, and building by sample count avoids duration->samples rounding
    // that would make the +/-500 delta assertions fragile.
    static juce::AudioBuffer<float> makeAudio(int numSamples)
    {
        juce::AudioBuffer<float> b(2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < numSamples; ++i)
                b.setSample(ch, i, 0.25f * std::sin(0.01f * static_cast<float>(i)));
        return b;
    }

    void testInsertShift()
    {
        auto source = TestAudio::createSineWave(440.0, 0.5, 44100.0, 2.0, 2);
        UndoTestHelper helper;
        expect(helper.loadTestBuffer(source, 44100.0), "Should load test buffer");

        RegionManager rm;
        MarkerManager mm;

        // Insert exactly 500 samples at position 1000.
        const int64_t insertPos = 1000;
        auto pasteBuffer = makeAudio(500);
        const int64_t insLen = pasteBuffer.getNumSamples();

        const int64_t idBefore = rm.getRegion(rm.addRegion(Region("A_before", 0, 500)))->getId();
        const int64_t idAt     = rm.getRegion(rm.addRegion(Region("B_at", 1000, 2000)))->getId();
        const int64_t idSpan   = rm.getRegion(rm.addRegion(Region("C_span", 800, 1200)))->getId();

        const int64_t mBefore   = mm.getMarker(mm.addMarker(Marker("m_before", 200)))->getId();
        const int64_t mBoundary = mm.getMarker(mm.addMarker(Marker("m_boundary", 1000)))->getId();
        const int64_t mAfter    = mm.getMarker(mm.addMarker(Marker("m_after", 1500)))->getId();

        auto* action = new InsertAction(helper.bufferManager, helper.audioEngine,
                                        helper.waveformDisplay, insertPos, pasteBuffer,
                                        &rm, nullptr, &mm, nullptr);
        expect(helper.undoManager.perform(action), "Insert should succeed");

        auto checkPerformed = [&](const char* stage)
        {
            // Region entirely before the insert point: unchanged.
            expectEquals(rm.getRegionById(idBefore)->getStartSample(), (int64_t) 0, stage);
            expectEquals(rm.getRegionById(idBefore)->getEndSample(),   (int64_t) 500, stage);
            // Region at/after: shifted forward by insLen.
            expectEquals(rm.getRegionById(idAt)->getStartSample(), 1000 + insLen, stage);
            expectEquals(rm.getRegionById(idAt)->getEndSample(),   2000 + insLen, stage);
            // Region spanning the insert point: start fixed, end grows.
            expectEquals(rm.getRegionById(idSpan)->getStartSample(), (int64_t) 800, stage);
            expectEquals(rm.getRegionById(idSpan)->getEndSample(),   1200 + insLen, stage);
            // Markers: before unchanged; boundary (== insertPos) and after shift.
            expectEquals(mm.getMarkerById(mBefore)->getPosition(),   (int64_t) 200, stage);
            expectEquals(mm.getMarkerById(mBoundary)->getPosition(), 1000 + insLen, stage);
            expectEquals(mm.getMarkerById(mAfter)->getPosition(),    1500 + insLen, stage);
        };
        checkPerformed("after perform");

        expect(helper.undoManager.undo(), "Undo should succeed");
        expectEquals(rm.getRegionById(idAt)->getStartSample(), (int64_t) 1000, "region restored");
        expectEquals(rm.getRegionById(idSpan)->getEndSample(),  (int64_t) 1200, "spanning region restored");
        expectEquals(mm.getMarkerById(mBoundary)->getPosition(), (int64_t) 1000, "boundary marker restored");
        expectEquals(mm.getMarkerById(mAfter)->getPosition(),    (int64_t) 1500, "after marker restored");

        expect(helper.undoManager.redo(), "Redo should succeed");
        checkPerformed("after redo");
    }

    void testReplaceGrowShift()
    {
        auto source = TestAudio::createSineWave(440.0, 0.5, 44100.0, 2.0, 2);
        UndoTestHelper helper;
        expect(helper.loadTestBuffer(source, 44100.0), "Should load test buffer");

        RegionManager rm;
        MarkerManager mm;

        // Replace [1000, 2000) (oldLen 1000) with 1500 samples: delta = +500.
        const int64_t start = 1000;
        const int64_t oldLen = 1000;
        auto newAudio = makeAudio(1500);
        const int64_t delta = newAudio.getNumSamples() - oldLen;
        expectEquals(delta, (int64_t) 500, "grow delta should be +500");

        const int64_t idBefore  = rm.getRegion(rm.addRegion(Region("before", 0, 500)))->getId();
        const int64_t idAfter   = rm.getRegion(rm.addRegion(Region("after", 3000, 4000)))->getId();
        const int64_t idInsideId = rm.getRegion(rm.addRegion(Region("inside", 1200, 1800)))->getId();

        const int64_t mBefore    = mm.getMarker(mm.addMarker(Marker("before", 500)))->getId();      // == start, kept
        const int64_t mStartEdge = mm.getMarker(mm.addMarker(Marker("start_edge", 1000)))->getId(); // == start, kept
        const int64_t mInside    = mm.getMarker(mm.addMarker(Marker("inside", 1500)))->getId();      // removed
        const int64_t mEndEdge   = mm.getMarker(mm.addMarker(Marker("end_edge", 2000)))->getId();    // == replaceEnd, shift
        const int64_t mAfter     = mm.getMarker(mm.addMarker(Marker("after", 3000)))->getId();       // shift

        auto* action = new ReplaceAction(helper.bufferManager, helper.audioEngine,
                                         helper.waveformDisplay, start, oldLen, newAudio,
                                         &rm, nullptr, &mm, nullptr);
        expect(helper.undoManager.perform(action), "Replace should succeed");

        auto checkPerformed = [&](const char* stage)
        {
            expectEquals(rm.getRegionById(idBefore)->getEndSample(), (int64_t) 500, stage);
            expectEquals(rm.getRegionById(idAfter)->getStartSample(), 3000 + delta, stage);
            expectEquals(rm.getRegionById(idAfter)->getEndSample(),   4000 + delta, stage);
            expect(rm.getRegionById(idInsideId) == nullptr, "overlapping region removed");

            expectEquals(mm.getMarkerById(mBefore)->getPosition(),    (int64_t) 500, stage);
            expectEquals(mm.getMarkerById(mStartEdge)->getPosition(), (int64_t) 1000, stage);
            expect(mm.getMarkerById(mInside) == nullptr, "marker inside the replaced range removed");
            expectEquals(mm.getMarkerById(mEndEdge)->getPosition(), 2000 + delta, stage);
            expectEquals(mm.getMarkerById(mAfter)->getPosition(),   3000 + delta, stage);
        };
        checkPerformed("after perform");

        expect(helper.undoManager.undo(), "Undo should succeed");
        expect(rm.getRegionById(idInsideId) != nullptr, "removed region restored on undo");
        expectEquals(rm.getRegionById(idAfter)->getStartSample(), (int64_t) 3000, "after region restored");
        expect(mm.getMarkerById(mInside) != nullptr, "removed marker restored on undo");
        expectEquals(mm.getMarkerById(mInside)->getPosition(), (int64_t) 1500, "restored marker position");
        expectEquals(mm.getMarkerById(mEndEdge)->getPosition(), (int64_t) 2000, "end-edge marker restored");

        expect(helper.undoManager.redo(), "Redo should succeed");
        checkPerformed("after redo");
    }

    void testReplaceShrinkShift()
    {
        auto source = TestAudio::createSineWave(440.0, 0.5, 44100.0, 2.0, 2);
        UndoTestHelper helper;
        expect(helper.loadTestBuffer(source, 44100.0), "Should load test buffer");

        RegionManager rm;
        MarkerManager mm;

        // Replace [1000, 2000) (oldLen 1000) with 500 samples: delta = -500.
        const int64_t start = 1000;
        const int64_t oldLen = 1000;
        auto newAudio = makeAudio(500);
        const int64_t delta = newAudio.getNumSamples() - oldLen;
        expectEquals(delta, (int64_t) -500, "shrink delta should be -500");

        const int64_t idAfter = rm.getRegion(rm.addRegion(Region("after", 3000, 4000)))->getId();
        const int64_t mAfter  = mm.getMarker(mm.addMarker(Marker("after", 3000)))->getId();
        const int64_t mEdge   = mm.getMarker(mm.addMarker(Marker("end_edge", 2000)))->getId();

        auto* action = new ReplaceAction(helper.bufferManager, helper.audioEngine,
                                         helper.waveformDisplay, start, oldLen, newAudio,
                                         &rm, nullptr, &mm, nullptr);
        expect(helper.undoManager.perform(action), "Replace should succeed");

        // Content after the replaced range moves LEFT by 500.
        expectEquals(rm.getRegionById(idAfter)->getStartSample(), (int64_t) 2500, "region shifted left");
        expectEquals(mm.getMarkerById(mEdge)->getPosition(),  (int64_t) 1500, "end-edge marker shifted left");
        expectEquals(mm.getMarkerById(mAfter)->getPosition(), (int64_t) 2500, "after marker shifted left");

        expect(helper.undoManager.undo(), "Undo should succeed");
        expectEquals(rm.getRegionById(idAfter)->getStartSample(), (int64_t) 3000, "region restored");
        expectEquals(mm.getMarkerById(mAfter)->getPosition(), (int64_t) 3000, "after marker restored");
    }
};

static InsertReplaceRegionMarkerShiftTests insertReplaceRegionMarkerShiftTests;
