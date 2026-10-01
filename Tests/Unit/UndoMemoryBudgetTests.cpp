/*
  ==============================================================================

    UndoMemoryBudgetTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Unit tests for the v0.9.1 undo memory-budget fix
    (Source/Utils/UndoActions/UndoMemoryBudget.h).

    Before this fix, Document::m_undoManager was configured with
    setMaxNumberOfStoredUnits(100, 90), but every WaveEdit UndoableAction
    reported an inconsistent, disconnected-from-reality size: some
    returned a hardcoded 100, some never overrode getSizeInUnits() (JUCE's
    default of 10), and some truncated a raw byte count into an int (an
    overflow for buffers over ~2 GB). Since JUCE's UndoManager only trims
    old transactions once the running total of reported units exceeds the
    configured maximum, the "100 levels" limit never actually bounded real
    memory use -- up to ~90-100 full-buffer snapshots of a large file
    could be retained at once.

    These tests cover the new UndoMemory:: helpers directly, and then
    exercise them through a real Document's juce::UndoManager, following
    the patterns in UndoRedoTests.cpp (UndoTestHelper), and
    Tests/Integration/CueChunkTests.cpp (constructing a real Document
    with Document::loadFile() against a temp WAV).

  ==============================================================================
*/

#include <cmath>
#include <limits>

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Audio/AudioFileManager.h"
#include "Utils/Document.h"
#include "Utils/UndoableEdits.h"
#include "Utils/UndoActions/UndoMemoryBudget.h"
#include "Utils/UndoActions/LevelUndoActions.h"
#include "Utils/UndoActions/TransformUndoActions.h"
#include "Utils/UndoActions/MarkerUndoActions.h"
#include "../TestUtils/TestAudioFiles.h"
#include "../TestUtils/AudioAssertions.h"

namespace
{
    /**
     * Saves `buffer` to a uniquely-named temp WAV and returns the File, so
     * a test can drive it through Document::loadFile() -- the same real
     * load path production code uses, matching the convention in
     * UndoRedoTests.cpp's UndoTestHelper and CueChunkTests.cpp.
     */
    juce::File makeTempWav(const juce::String& baseName,
                           const juce::AudioBuffer<float>& buffer,
                           double sampleRate)
    {
        auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile(baseName + "_"
                          + juce::String(juce::Random::getSystemRandom().nextInt()) + ".wav");
        AudioFileManager fm;
        fm.saveAsWav(file, buffer, sampleRate, 16);
        return file;
    }
}

//==============================================================================
class UndoMemoryBudgetTests : public juce::UnitTest
{
public:
    UndoMemoryBudgetTests() : juce::UnitTest("Undo Memory Budget (v0.9.1)", "Unit") {}

    void runTest() override
    {
        beginTest("unitsForBytes floors 0 and tiny sizes");
        testFloor();

        beginTest("unitsForBytes ceilings huge sizes without overflow");
        testCeiling();

        beginTest("unitsForBytes is monotonic non-decreasing");
        testMonotonic();

        beginTest("maxUnits is positive and stable");
        testMaxUnitsSane();

        beginTest("Document UndoManager keeps exactly 100 small transactions");
        testDocumentKeeps100SmallTransactions();

        beginTest("Large-buffer actions trim down to the minimum transaction count");
        testLargeBufferTrimsToMinimum();

        beginTest("A single oversized action is still kept and undoes correctly");
        testOversizedSingleActionKept();

        beginTest("getSizeInUnits is fixed across perform/undo for each changed category");
        testSizeStableAcrossPerformUndo();
    }

private:
    //==========================================================================
    // Pure UndoMemory:: math -- no Document needed.
    //==========================================================================

    void testFloor()
    {
        const int floorUnits = UndoMemory::maxUnits() / UndoMemory::kMaxLevels;

        // ceil(bytes / 1024) for 0, 1, and 1024 bytes is 0, 1, and 1
        // respectively -- all <= floorUnits (floorUnits is always >= 1,
        // since maxUnits() >= 100), so all three must clamp UP to the floor.
        expectEquals(UndoMemory::unitsForBytes(0), floorUnits, "0 bytes hits the floor");
        expectEquals(UndoMemory::unitsForBytes(1), floorUnits, "1 byte hits the floor");
        expectEquals(UndoMemory::unitsForBytes(1024), floorUnits, "1 KiB hits the floor");
    }

    void testCeiling()
    {
        const int ceilingUnits = UndoMemory::maxUnits();
        const size_t oneTiB = (size_t) 1 << 40;

        expectEquals(UndoMemory::unitsForBytes(oneTiB), ceilingUnits,
                     "1 TiB clamps to the ceiling");
        expectEquals(UndoMemory::unitsForBytes(std::numeric_limits<size_t>::max()), ceilingUnits,
                     "SIZE_MAX must not overflow the internal 64-bit math and must clamp "
                     "to the ceiling");
    }

    void testMonotonic()
    {
        const size_t steps[] = {
            0, 1, 100, 1024, 10 * 1024, 100 * 1024, 1024 * 1024,
            100ull * 1024 * 1024, 1024ull * 1024 * 1024,
            10ull * 1024 * 1024 * 1024, (size_t) 1 << 40
        };

        int previous = UndoMemory::unitsForBytes(0);
        for (size_t bytes : steps)
        {
            const int units = UndoMemory::unitsForBytes(bytes);
            expect(units >= previous, "units must never decrease as the byte count grows");
            previous = units;
        }
    }

    void testMaxUnitsSane()
    {
        const int firstCall = UndoMemory::maxUnits();
        expect(firstCall >= 100, "maxUnits() must satisfy its own documented >= 100 floor");
        expect(UndoMemory::maxUnits() == firstCall,
               "maxUnits() must be stable across calls (function-local static)");
    }

    //==========================================================================
    // Through a real Document's juce::UndoManager.
    //==========================================================================

    void testDocumentKeeps100SmallTransactions()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.5f, 44100.0, 0.05, 1);
        auto file = makeTempWav("undomem_100tx", buffer, 44100.0);

        Document doc;
        expect(doc.loadFile(file), "document should load the tiny test WAV");

        // Document's ctor already wires
        // setMaxNumberOfStoredUnits(UndoMemory::maxUnits(), UndoMemory::kMinTransactionsKept)
        // -- exercise that real production wiring as-is, not a re-applied override.
        juce::UndoManager& undoManager = doc.getUndoManager();

        // 101 floor-sized metadata actions, each its own transaction (a
        // fresh transaction boundary before EACH, so they don't coalesce):
        // the floor is maxUnits()/kMaxLevels, so ~100 of them fill the
        // whole budget and the 101st must push the total over it.
        for (int i = 0; i < UndoMemory::kMaxLevels + 1; ++i)
        {
            undoManager.beginNewTransaction("Add marker " + juce::String(i));
            Marker marker("m" + juce::String(i), i);
            auto* action = new AddMarkerUndoAction(doc.getMarkerManager(), &doc.getMarkerDisplay(), marker);
            expect(undoManager.perform(action), "AddMarkerUndoAction perform should succeed");
        }

        expectEquals(undoManager.getUndoDescriptions().size(), UndoMemory::kMaxLevels,
                     "exactly 100 transactions should remain after 101 floor-sized edits");

        file.deleteFile();
    }

    void testLargeBufferTrimsToMinimum()
    {
        // Size each action's buffer relative to the REAL floor (not an
        // arbitrary constant): the floor is RAM-derived, so this scales
        // sanely on any machine while staying comfortably above the floor
        // (so the byte count -- not the floor clamp -- drives the size).
        const int floorUnits = UndoMemory::maxUnits() / UndoMemory::kMaxLevels;
        const int smallBudgetUnits = floorUnits * 4;
        const int perActionUnits = juce::jmax(floorUnits + 1, (smallBudgetUnits * 40) / 100);
        const size_t perActionBytes = (size_t) perActionUnits * 1024;
        const int numSamples = juce::jmax(1, (int) (perActionBytes / sizeof(float)));

        juce::AudioBuffer<float> buffer(1, numSamples);
        buffer.clear();
        auto file = makeTempWav("undomem_largebuf", buffer, 44100.0);

        Document doc;
        expect(doc.loadFile(file), "document should load the large test WAV");

        juce::UndoManager& undoManager = doc.getUndoManager();
        // Document-local override -- NOT UndoMemory::maxUnits() -- so this
        // test controls exactly when the budget is exceeded.
        undoManager.setMaxNumberOfStoredUnits(smallBudgetUnits, 2);

        // 3 actions at ~40% of the budget each -> 120% of budget, so the
        // oldest must be dropped, leaving exactly the 2-transaction minimum.
        for (int i = 0; i < 3; ++i)
        {
            undoManager.beginNewTransaction("Gain " + juce::String(i));
            auto* action = new GainUndoAction(doc.getBufferManager(), doc.getWaveformDisplay(),
                                              doc.getAudioEngine(), doc.getBufferManager().getBuffer(),
                                              0, numSamples, 1.0f, false);
            expect(undoManager.perform(action), "GainUndoAction perform should succeed");
        }

        expectEquals(undoManager.getUndoDescriptions().size(), 2,
                     "3 actions at ~40% of a small budget each must trim down to the "
                     "2-transaction minimum");
        expect(undoManager.getNumberOfUnitsTakenUpByStoredCommands() >= 0,
               "reported total units must never go negative (JUCE jasserts this internally)");

        file.deleteFile();
    }

    void testOversizedSingleActionKept()
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.5f, 44100.0, 1.0, 1);
        auto file = makeTempWav("undomem_oversized", buffer, 44100.0);

        Document doc;
        expect(doc.loadFile(file), "document should load the test WAV");

        // A budget far smaller than even the floor ANY single action can
        // report: this one action is "larger than the whole budget" on its
        // own, but the minimum-transactions-kept rule must still preserve it
        // (dropOldTransactionsIfTooLarge() only drops while
        // transactions.size() > minimumTransactionsToKeep).
        doc.getUndoManager().setMaxNumberOfStoredUnits(1, 2);

        auto* deleteAction = new DeleteAction(doc.getBufferManager(), doc.getAudioEngine(),
                                              doc.getWaveformDisplay(), 0, 4410);
        expect(doc.getUndoManager().perform(deleteAction), "delete should succeed");

        expect(doc.getUndoManager().canUndo(),
               "the only transaction must be kept even though its size exceeds the tiny budget");
        expectEquals(doc.getUndoManager().getUndoDescriptions().size(), 1,
                     "exactly one transaction should be present");

        expect(doc.getUndoManager().undo(), "undo should succeed");
        expectEquals(doc.getBufferManager().getNumSamples(), (int64_t) buffer.getNumSamples(),
                     "undo should restore the original sample count");
        expect(AudioAssertions::expectBuffersNearlyEqual(buffer, doc.getBufferManager().getBuffer(), 0.001f),
               "undo should restore the original audio");

        file.deleteFile();
    }

    void testSizeStableAcrossPerformUndo()
    {
        // One representative action per category this fix changed:
        //  - DeleteAction:        overflow-prone static_cast<int> of raw bytes -> UndoMemory
        //  - GainUndoAction:      no override (JUCE default 10) -> UndoMemory
        //  - ResampleUndoAction:  hardcoded `return 100` -> UndoMemory
        //  - ReverseUndoAction:   no override, no audio stored -> UndoMemory floor
        //  - AddMarkerUndoAction: sizeof-based metadata -> UndoMemory floor
        auto buffer = TestAudio::createSineWave(440.0, 0.5f, 44100.0, 1.0, 1);
        auto file = makeTempWav("undomem_sizestable", buffer, 44100.0);

        Document doc;
        expect(doc.loadFile(file), "document should load the test WAV");
        juce::UndoManager& undoManager = doc.getUndoManager();

        // DeleteAction
        {
            undoManager.beginNewTransaction("Delete");
            auto* action = new DeleteAction(doc.getBufferManager(), doc.getAudioEngine(),
                                            doc.getWaveformDisplay(), 0, 4410);
            const int sizeBeforePerform = action->getSizeInUnits();
            expect(sizeBeforePerform > 0, "DeleteAction size should be > 0");
            expect(undoManager.perform(action), "delete perform should succeed");
            expectEquals(action->getSizeInUnits(), sizeBeforePerform,
                         "DeleteAction size must not change after perform()");
            expect(undoManager.undo(), "delete undo should succeed");
            expectEquals(action->getSizeInUnits(), sizeBeforePerform,
                         "DeleteAction size must not change after undo()");
        }

        // GainUndoAction (buffer is back to its original length after the undo above)
        {
            undoManager.beginNewTransaction("Gain");
            auto* action = new GainUndoAction(doc.getBufferManager(), doc.getWaveformDisplay(),
                                              doc.getAudioEngine(), doc.getBufferManager().getBuffer(),
                                              0, (int) doc.getBufferManager().getNumSamples(), 3.0f, false);
            const int sizeBeforePerform = action->getSizeInUnits();
            expect(sizeBeforePerform > 0, "GainUndoAction size should be > 0");
            expect(undoManager.perform(action), "gain perform should succeed");
            expectEquals(action->getSizeInUnits(), sizeBeforePerform,
                         "GainUndoAction size must not change after perform()");
            expect(undoManager.undo(), "gain undo should succeed");
            expectEquals(action->getSizeInUnits(), sizeBeforePerform,
                         "GainUndoAction size must not change after undo()");
        }

        // ResampleUndoAction
        {
            undoManager.beginNewTransaction("Resample");
            auto* action = new ResampleUndoAction(doc.getBufferManager(), doc.getWaveformDisplay(),
                                                  doc.getAudioEngine(), doc.getBufferManager().getBuffer(),
                                                  44100.0, 48000.0);
            const int sizeBeforePerform = action->getSizeInUnits();
            expect(sizeBeforePerform > 0, "ResampleUndoAction size should be > 0");
            expect(undoManager.perform(action), "resample perform should succeed");
            expectEquals(action->getSizeInUnits(), sizeBeforePerform,
                         "ResampleUndoAction size must not change after perform()");
            expect(undoManager.undo(), "resample undo should succeed");
            expectEquals(action->getSizeInUnits(), sizeBeforePerform,
                         "ResampleUndoAction size must not change after undo()");
        }

        // ReverseUndoAction (no audio stored -- must hit the floor exactly)
        {
            undoManager.beginNewTransaction("Reverse");
            auto* action = new ReverseUndoAction(doc.getBufferManager(), doc.getWaveformDisplay(),
                                                 doc.getAudioEngine(), 0,
                                                 (int) doc.getBufferManager().getNumSamples(), false);
            const int sizeBeforePerform = action->getSizeInUnits();
            const int floorUnits = UndoMemory::maxUnits() / UndoMemory::kMaxLevels;
            expect(sizeBeforePerform > 0, "ReverseUndoAction size should be > 0");
            expectEquals(sizeBeforePerform, floorUnits,
                         "a no-audio action should hit the floor exactly");
            expect(undoManager.perform(action), "reverse perform should succeed");
            expectEquals(action->getSizeInUnits(), sizeBeforePerform,
                         "ReverseUndoAction size must not change after perform()");
            expect(undoManager.undo(), "reverse undo should succeed"); // undo() == perform() for Reverse
            expectEquals(action->getSizeInUnits(), sizeBeforePerform,
                         "ReverseUndoAction size must not change after undo()");
        }

        // AddMarkerUndoAction (metadata -- must hit the floor)
        {
            undoManager.beginNewTransaction("Add marker");
            Marker marker("size-test", 0);
            auto* action = new AddMarkerUndoAction(doc.getMarkerManager(), &doc.getMarkerDisplay(), marker);
            const int sizeBeforePerform = action->getSizeInUnits();
            expect(sizeBeforePerform > 0, "AddMarkerUndoAction size should be > 0");
            expect(undoManager.perform(action), "add marker perform should succeed");
            expectEquals(action->getSizeInUnits(), sizeBeforePerform,
                         "AddMarkerUndoAction size must not change after perform()");
            expect(undoManager.undo(), "add marker undo should succeed");
            expectEquals(action->getSizeInUnits(), sizeBeforePerform,
                         "AddMarkerUndoAction size must not change after undo()");
        }

        file.deleteFile();
    }
};

static UndoMemoryBudgetTests undoMemoryBudgetTests;
