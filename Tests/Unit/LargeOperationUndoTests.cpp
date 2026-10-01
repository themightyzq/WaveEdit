/*
  ==============================================================================

    LargeOperationUndoTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Closes the "undo/redo with large operations" gap noted in TODO.md.

    UndoRedoTests.cpp already covers Delete/Insert/Replace/Gain correctness
    on small (<= 5s @ 44.1kHz) buffers and multi-level undo depth. It does
    NOT cover:
      - Buffer sizes in the multi-minute / multi-million-sample range
        (this file uses ~3,000,000 samples/channel @ 48kHz stereo, ~62.5s,
        the smallest size that is still meaningfully "large" while keeping
        the suite fast).
      - Deleting/inserting/replacing an INTERIOR region rather than always
        operating from sample 0.
      - An unequal-length replace (cut N samples, paste M != N samples),
        which AudioBufferManager::replaceRange() supports but
        UndoRedoTests.cpp only ever exercises with equal-length regions.
      - Playback-state (isPlaying/position) restoration on undo/redo for a
        large buffer, and the fact that length-changing edits (Delete/
        Insert/Replace, via UndoableEditBase::updatePlaybackAndDisplay())
        deterministically STOP playback, whereas the gain-type action
        (GainUndoAction, via AudioEngine::reloadBufferPreservingPlayback())
        PRESERVES playback across a large edit. Both behaviors are
        intentional per CLAUDE.md §6.5 but were previously unverified for
        buffers this large.

    Runtime discipline: no sample-by-sample expect() loops. Buffer-content
    correctness is verified with AudioAssertions::hashBuffer() (a single
    O(n) checksum comparison) and RMS/peak spot checks, matching the
    existing UndoRedoTests.cpp convention (see testDeleteUndoRedo there).

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
#include "Utils/UndoActions/LevelUndoActions.h"
#include "Utils/UndoActions/RangeUndoActions.h"
#include "Utils/UndoActions/TransformUndoActions.h"
#include "Utils/UndoActions/PluginUndoActions.h"
#include "../TestUtils/TestAudioFiles.h"
#include "../TestUtils/AudioAssertions.h"

namespace
{
    // A "large" buffer for this suite: ~62.5s @ 48kHz stereo = 3,000,000
    // samples/channel. Big enough to be a meaningful stress case (multi-
    // million samples, well past the <=5s buffers UndoRedoTests.cpp uses)
    // while keeping wall-clock runtime in the low single-digit seconds.
    constexpr double kSampleRate       = 48000.0;
    constexpr int64_t kLargeNumSamples = 3'000'000;

    //==========================================================================
    /**
     * Same file-based load workflow as UndoRedoTests.cpp's UndoTestHelper
     * (loads via a real temp WAV round-trip, not a synthetic buffer swap),
     * duplicated here (file-local, anonymous-namespace) rather than shared
     * via a header, per the existing per-file test-helper convention (see
     * also PluginChainTailTests.cpp's file-local TailPlugin).
     */
    class LargeOpUndoTestHelper
    {
    public:
        LargeOpUndoTestHelper()
            : waveformDisplay(formatManager)
        {
            formatManager.registerBasicFormats();
            audioEngine.stop();
        }

        ~LargeOpUndoTestHelper()
        {
            audioEngine.stop();
            if (tempFile.existsAsFile())
                tempFile.deleteFile();
        }

        bool loadTestBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate)
        {
            tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                .getChildFile("large_undo_test_"
                              + juce::String(juce::Random::getSystemRandom().nextInt()) + ".wav");

            if (!saveBufferToTempFile(buffer, sampleRate))
                return false;

            if (!bufferManager.loadFromFile(tempFile, formatManager))
            {
                tempFile.deleteFile();
                return false;
            }

            if (!audioEngine.loadFromBuffer(bufferManager.getBuffer(), sampleRate, buffer.getNumChannels()))
            {
                tempFile.deleteFile();
                return false;
            }

            if (!waveformDisplay.reloadFromBuffer(bufferManager.getBuffer(), sampleRate, false, false))
            {
                tempFile.deleteFile();
                return false;
            }

            return true;
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

            auto* rawStream = outputStream.get();
            std::unique_ptr<juce::AudioFormatWriter> writer(
                format->createWriterFor(rawStream, sampleRate, buffer.getNumChannels(), 16, {}, 0));

            if (writer == nullptr)
                return false;

            outputStream.release();

            bool writeSuccess = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
            writer.reset();

            return writeSuccess;
        }
    };
} // namespace

//==============================================================================
// Delete/Insert/Replace buffer integrity at large sample counts
//==============================================================================
class LargeBufferUndoRedoTests : public juce::UnitTest
{
public:
    LargeBufferUndoRedoTests() : juce::UnitTest("Undo/Redo Large Buffers", "Unit") {}

    void runTest() override
    {
        beginTest("Large mid-buffer delete: undo/redo integrity (~3M samples)");
        testLargeDeleteUndoRedo();

        beginTest("Large mid-buffer paste (insert): undo/redo integrity");
        testLargeInsertUndoRedo();

        beginTest("Large unequal-length replace (cut+paste): undo/redo integrity");
        testLargeUnequalReplaceUndoRedo();

        beginTest("Buffer-length-changing edits stop playback deterministically at scale");
        testLargeDeleteStopsPlaybackDeterministically();
    }

private:
    static juce::AudioBuffer<float> makeLargeSine(double frequency, float amplitude)
    {
        return TestAudio::createSineWave(frequency, amplitude, kSampleRate,
                                          static_cast<double>(kLargeNumSamples) / kSampleRate, 2);
    }

    void testLargeDeleteUndoRedo()
    {
        auto originalBuffer = makeLargeSine(440.0, 0.5f);
        expectEquals(static_cast<int64_t>(originalBuffer.getNumSamples()), kLargeNumSamples);

        LargeOpUndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, kSampleRate), "Should load the large test buffer");

        // Baseline captured AFTER the 16-bit WAV round-trip, so later
        // restores can be checksummed bit-exactly against this (not the
        // pre-quantization float source, which would spuriously mismatch).
        const int64_t loadedHash    = AudioAssertions::hashBuffer(helper.bufferManager.getBuffer());
        const int64_t loadedSamples = helper.bufferManager.getNumSamples();

        // Delete a 10s chunk out of the MIDDLE of the file -- UndoRedoTests.cpp
        // only ever deletes from sample 0.
        const int64_t deleteStart  = loadedSamples / 2;
        const int64_t deleteLength = 480000; // 10s @ 48kHz

        auto* deleteAction = new DeleteAction(helper.bufferManager, helper.audioEngine,
                                               helper.waveformDisplay, deleteStart, deleteLength);
        expect(helper.undoManager.perform(deleteAction), "Large mid-buffer delete should succeed");

        expectEquals(helper.bufferManager.getNumSamples(), loadedSamples - deleteLength,
                     "Buffer should shrink by exactly the deleted length");
        expectEquals(helper.bufferManager.getNumChannels(), 2,
                     "Channel count must survive a large edit");
        expectWithinAbsoluteError(helper.bufferManager.getSampleRate(), kSampleRate, 0.001,
                                  "Sample rate metadata must survive a large edit");

        // Spot check instead of a sample-by-sample loop: a continuous sine's
        // RMS is ~constant regardless of which interior chunk is removed, so
        // this still catches gross corruption (silence, garbage, wrong gain).
        expect(AudioAssertions::expectRMSLevel(helper.bufferManager.getBuffer(),
                                               0.5f / std::sqrt(2.0f), 0.02f),
              "RMS should still reflect the original sine amplitude after delete");

        expect(helper.undoManager.undo(), "Undo of the large delete should succeed");
        expectEquals(helper.bufferManager.getNumSamples(), loadedSamples,
                     "Buffer length should be restored exactly after undo");
        expectEquals(AudioAssertions::hashBuffer(helper.bufferManager.getBuffer()), loadedHash,
                     "Undo must restore the buffer bit-exactly (checksum match)");

        expect(helper.undoManager.redo(), "Redo of the large delete should succeed");
        expectEquals(helper.bufferManager.getNumSamples(), loadedSamples - deleteLength,
                     "Buffer should shrink again after redo");
    }

    void testLargeInsertUndoRedo()
    {
        auto originalBuffer = makeLargeSine(440.0, 0.5f);

        LargeOpUndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, kSampleRate), "Should load the large test buffer");

        const int64_t loadedHash    = AudioAssertions::hashBuffer(helper.bufferManager.getBuffer());
        const int64_t loadedSamples = helper.bufferManager.getNumSamples();

        // Paste 5s of a distinct tone into the middle of the file.
        auto pasteBuffer = TestAudio::createSineWave(880.0, 0.3f, kSampleRate, 5.0, 2);
        const int64_t pasteSamples    = pasteBuffer.getNumSamples();
        const int64_t insertPosition  = loadedSamples / 3;

        auto* insertAction = new InsertAction(helper.bufferManager, helper.audioEngine,
                                               helper.waveformDisplay, insertPosition, pasteBuffer);
        expect(helper.undoManager.perform(insertAction), "Large paste should succeed");

        expectEquals(helper.bufferManager.getNumSamples(), loadedSamples + pasteSamples,
                     "Buffer should grow by exactly the pasted length");

        expect(helper.undoManager.undo(), "Undo of the large paste should succeed");
        expectEquals(helper.bufferManager.getNumSamples(), loadedSamples,
                     "Buffer length should be restored exactly after undo");
        expectEquals(AudioAssertions::hashBuffer(helper.bufferManager.getBuffer()), loadedHash,
                     "Undo must restore the buffer bit-exactly (checksum match)");

        expect(helper.undoManager.redo(), "Redo of the large paste should succeed");
        expectEquals(helper.bufferManager.getNumSamples(), loadedSamples + pasteSamples,
                     "Buffer should grow again after redo");
    }

    void testLargeUnequalReplaceUndoRedo()
    {
        auto originalBuffer = makeLargeSine(440.0, 0.5f);

        LargeOpUndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, kSampleRate), "Should load the large test buffer");

        const int64_t loadedHash    = AudioAssertions::hashBuffer(helper.bufferManager.getBuffer());
        const int64_t loadedSamples = helper.bufferManager.getNumSamples();

        // Cut a 400,000-sample region and paste a 700,000-sample replacement:
        // an UNEQUAL-length replace (real-world cut+paste-over-selection).
        // AudioBufferManager::replaceRange() supports this (see its
        // newNumSamples computation), but UndoRedoTests.cpp's ReplaceAction
        // coverage only ever uses equal-length regions.
        const int64_t replaceStart  = loadedSamples / 4;
        const int64_t replaceLength = 400000;
        auto replacementBuffer = TestAudio::createSineWave(
            220.0, 0.4f, kSampleRate, 700000.0 / kSampleRate, 2);
        const int64_t replacementSamples = replacementBuffer.getNumSamples();

        auto* replaceAction = new ReplaceAction(helper.bufferManager, helper.audioEngine,
                                                 helper.waveformDisplay, replaceStart,
                                                 replaceLength, replacementBuffer);
        expect(helper.undoManager.perform(replaceAction),
              "Large unequal-length replace should succeed");

        const int64_t expectedAfterReplace = loadedSamples - replaceLength + replacementSamples;
        expectEquals(helper.bufferManager.getNumSamples(), expectedAfterReplace,
                     "Buffer should grow by (replacement length - cut length)");

        juce::AudioBuffer<float> afterReplace;
        afterReplace.makeCopyOf(helper.bufferManager.getBuffer());
        const int64_t afterReplaceHash = AudioAssertions::hashBuffer(afterReplace);

        expect(helper.undoManager.undo(), "Undo of the large unequal-length replace should succeed");
        expectEquals(helper.bufferManager.getNumSamples(), loadedSamples,
                     "Buffer length should be restored exactly after undo");
        expectEquals(AudioAssertions::hashBuffer(helper.bufferManager.getBuffer()), loadedHash,
                     "Undo must restore the buffer bit-exactly (checksum match)");

        expect(helper.undoManager.redo(), "Redo of the large unequal-length replace should succeed");
        expectEquals(helper.bufferManager.getNumSamples(), expectedAfterReplace,
                     "Buffer length after redo should match the original perform() result");
        expectEquals(AudioAssertions::hashBuffer(helper.bufferManager.getBuffer()), afterReplaceHash,
                     "Redo should reproduce exactly the same result as the original perform()");
    }

    void testLargeDeleteStopsPlaybackDeterministically()
    {
        auto originalBuffer = makeLargeSine(440.0, 0.5f);

        LargeOpUndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, kSampleRate), "Should load the large test buffer");

        helper.audioEngine.play();
        helper.audioEngine.setPosition(20.0);
        expect(helper.audioEngine.isPlaying(), "Engine should report playing before the edit");

        auto* deleteAction = new DeleteAction(helper.bufferManager, helper.audioEngine,
                                               helper.waveformDisplay, 0, 480000);
        expect(helper.undoManager.perform(deleteAction), "Large delete should succeed");

        // DeleteAction/InsertAction/ReplaceAction route through
        // UndoableEditBase::updatePlaybackAndDisplay(), which unconditionally
        // stops playback before reloading the buffer -- unlike GainUndoAction
        // (see LargeBufferGainUndoPlaybackTests below), which preserves
        // playback via AudioEngine::reloadBufferPreservingPlayback(). This is
        // intentional (a length-changing edit can invalidate the transport
        // position), but was previously unverified at this buffer scale.
        expect(! helper.audioEngine.isPlaying(),
              "A buffer-length-changing edit must stop playback, not silently corrupt position");
        expectWithinAbsoluteError(helper.audioEngine.getCurrentPosition(), 0.0, 1.0e-9,
                                  "Playback position must reset to 0 after a length-changing edit");

        helper.undoManager.undo();
        expect(! helper.audioEngine.isPlaying(), "Undo must not resurrect playback either");
    }
};

static LargeBufferUndoRedoTests largeBufferUndoRedoTests;

//==============================================================================
// GainUndoAction (the "gain-type buffer-replacing" action, per CLAUDE.md
// §6.5) on a large buffer: verifies both sample correctness AND that
// playback state (isPlaying + position) survives perform()/undo(), since
// this action uses reloadBufferPreservingPlayback() specifically to make
// that possible during real-time editing.
//==============================================================================
class LargeBufferGainUndoPlaybackTests : public juce::UnitTest
{
public:
    LargeBufferGainUndoPlaybackTests()
        : juce::UnitTest("Undo/Redo Large Buffers - Gain Playback State", "Unit") {}

    void runTest() override
    {
        beginTest("GainUndoAction on a large region preserves isPlaying + position across perform/undo");
        testGainPreservesPlaybackAcrossPerformAndUndo();
    }

private:
    void testGainPreservesPlaybackAcrossPerformAndUndo()
    {
        auto originalBuffer = TestAudio::createSineWave(
            440.0, 0.5f, kSampleRate, static_cast<double>(kLargeNumSamples) / kSampleRate, 2);

        LargeOpUndoTestHelper helper;
        expect(helper.loadTestBuffer(originalBuffer, kSampleRate), "Should load the large test buffer");

        helper.audioEngine.play();
        helper.audioEngine.setPosition(30.0);
        expect(helper.audioEngine.isPlaying(), "Engine should be playing before the gain edit");
        const double positionBeforeEdit = helper.audioEngine.getCurrentPosition();
        expectWithinAbsoluteError(positionBeforeEdit, 30.0, 0.05,
                                  "Position should be ~30s before the edit");

        // A large affected region (~20.8s @ 48kHz), not the whole buffer and
        // not the tiny regions UndoRedoTests.cpp uses.
        const int64_t startSample64 = helper.bufferManager.getNumSamples() / 3;
        const int64_t numSamples64  = 1000000;
        const int startSample = static_cast<int>(startSample64);
        const int numSamples  = static_cast<int>(numSamples64);

        auto beforeRegion = helper.bufferManager.getAudioRange(startSample64, numSamples64);
        const float gainDB = -6.0f;

        auto* gainAction = new GainUndoAction(helper.bufferManager, helper.waveformDisplay,
                                               helper.audioEngine, beforeRegion,
                                               startSample, numSamples, gainDB, true);
        expect(helper.undoManager.perform(gainAction),
              "Gain adjustment on a large region should succeed");

        // Gain-type edits use reloadBufferPreservingPlayback() (CLAUDE.md
        // §6.5) -- unlike Delete/Insert/Replace, playback must NOT stop and
        // position must survive (gain never changes buffer length).
        expect(helper.audioEngine.isPlaying(), "Gain edit must preserve the playing state");
        expectWithinAbsoluteError(helper.audioEngine.getCurrentPosition(), positionBeforeEdit, 0.1,
                                  "Gain edit must preserve playback position");
        expectEquals(helper.bufferManager.getNumSamples(),
                     static_cast<int64_t>(originalBuffer.getNumSamples()),
                     "Gain must not change buffer length");

        auto afterRegion = helper.bufferManager.getAudioRange(startSample64, numSamples64);
        expect(AudioAssertions::expectGainApplied(beforeRegion, afterRegion,
                                                  juce::Decibels::decibelsToGain(gainDB), 0.001f),
              "Gain should be applied bit-accurately across the whole affected region");

        expect(helper.undoManager.undo(), "Undo of the large gain edit should succeed");
        expect(helper.audioEngine.isPlaying(), "Undo of a gain edit must also preserve the playing state");
        expectWithinAbsoluteError(helper.audioEngine.getCurrentPosition(), positionBeforeEdit, 0.1,
                                  "Undo of a gain edit must preserve playback position");
        expectEquals(helper.bufferManager.getNumSamples(),
                     static_cast<int64_t>(originalBuffer.getNumSamples()),
                     "Undo must not corrupt buffer length either");

        auto restoredRegion = helper.bufferManager.getAudioRange(startSample64, numSamples64);
        expectEquals(AudioAssertions::hashBuffer(restoredRegion), AudioAssertions::hashBuffer(beforeRegion),
                     "Undo must restore the affected region bit-exactly");
    }
};

static LargeBufferGainUndoPlaybackTests largeBufferGainUndoPlaybackTests;

//==============================================================================
// H2: length-changing whole-buffer actions (Trim / Head&Tail / Time-Pitch)
// must STOP playback deterministically on perform() AND undo(), the same way
// Delete/Insert/Replace do -- restoring a seconds-position onto a buffer whose
// length/timeline changed would resume on the wrong content (CLAUDE.md Sec 6.5).
// These previously (mis)used reloadBufferPreservingPlayback().
//==============================================================================
class LengthChangingActionStopPlaybackTests : public juce::UnitTest
{
public:
    LengthChangingActionStopPlaybackTests()
        : juce::UnitTest("Length-Changing Actions Stop Playback (H2)", "Unit") {}

    void runTest() override
    {
        beginTest("Trim stops playback on perform and undo");
        testTrimStopsPlayback();

        beginTest("Head & Tail stops playback on perform and undo");
        testHeadTailStopsPlayback();

        beginTest("Time/Pitch stops playback on perform and undo");
        testTimePitchStopsPlayback();
    }

private:
    // ~2s @ 48kHz stereo -- small and fast; the defect is length-agnostic.
    static juce::AudioBuffer<float> makeSine(double seconds, double freq = 440.0)
    {
        return TestAudio::createSineWave(freq, 0.5f, kSampleRate, seconds, 2);
    }

    // Start playback mid-file, then run the supplied edit and assert the engine
    // stopped and reset to 0 (perform), and stayed stopped after undo.
    template <typename MakeAction>
    void runStopExpectation(const juce::AudioBuffer<float>& source, MakeAction makeAction)
    {
        LargeOpUndoTestHelper helper;
        expect(helper.loadTestBuffer(source, kSampleRate), "Should load the test buffer");

        helper.audioEngine.play();
        helper.audioEngine.setPosition(1.0);
        expect(helper.audioEngine.isPlaying(), "Engine should be playing before the edit");

        auto* action = makeAction(helper);
        expect(helper.undoManager.perform(action), "Length-changing edit should succeed");

        expect(! helper.audioEngine.isPlaying(),
              "A length-changing edit must STOP playback, not preserve position");
        expectWithinAbsoluteError(helper.audioEngine.getCurrentPosition(), 0.0, 1.0e-9,
                                  "Playback position must reset to 0 after a length-changing edit");

        helper.audioEngine.play();
        helper.audioEngine.setPosition(0.5);
        expect(helper.undoManager.undo(), "Undo should succeed");
        expect(! helper.audioEngine.isPlaying(),
              "Undo of a length-changing edit must also STOP playback");
        expectWithinAbsoluteError(helper.audioEngine.getCurrentPosition(), 0.0, 1.0e-9,
                                  "Playback position must reset to 0 after undo of a length-changing edit");
    }

    void testTrimStopsPlayback()
    {
        auto source = makeSine(2.0);
        runStopExpectation(source, [](LargeOpUndoTestHelper& h) -> juce::UndoableAction*
        {
            // Trim to the middle half of the file (removes samples -> shorter).
            const int total = static_cast<int>(h.bufferManager.getNumSamples());
            const int start = total / 4;
            const int len   = total / 2;
            return new TrimUndoAction(h.bufferManager, h.waveformDisplay, h.audioEngine,
                                      h.bufferManager.getBuffer(), start, len);
        });
    }

    void testHeadTailStopsPlayback()
    {
        auto source = makeSine(2.0);
        runStopExpectation(source, [](LargeOpUndoTestHelper& h) -> juce::UndoableAction*
        {
            // "after" prepends 0.5s of silence -> longer buffer, shifted timeline.
            juce::AudioBuffer<float> before;
            before.makeCopyOf(h.bufferManager.getBuffer());
            const int extra = static_cast<int>(kSampleRate / 2);
            juce::AudioBuffer<float> after(before.getNumChannels(),
                                           before.getNumSamples() + extra);
            after.clear();
            for (int ch = 0; ch < before.getNumChannels(); ++ch)
                after.copyFrom(ch, extra, before, ch, 0, before.getNumSamples());
            return new HeadTailUndoAction(h.bufferManager, h.waveformDisplay, h.audioEngine,
                                          before, after, kSampleRate);
        });
    }

    void testTimePitchStopsPlayback()
    {
        auto source = makeSine(2.0);
        runStopExpectation(source, [](LargeOpUndoTestHelper& h) -> juce::UndoableAction*
        {
            // Time-stretch shortens duration -> "after" has fewer samples.
            juce::AudioBuffer<float> before;
            before.makeCopyOf(h.bufferManager.getBuffer());
            juce::AudioBuffer<float> after(before.getNumChannels(),
                                           before.getNumSamples() * 3 / 4);
            for (int ch = 0; ch < before.getNumChannels(); ++ch)
                after.copyFrom(ch, 0, before, ch, 0, after.getNumSamples());
            return new TimePitchUndoAction(h.bufferManager, h.waveformDisplay, h.audioEngine,
                                           before, after, kSampleRate, "Time stretch -25%");
        });
    }
};

static LengthChangingActionStopPlaybackTests lengthChangingActionStopPlaybackTests;

//==============================================================================
// H1: the plugin-chain / offline-plugin APPLY path replaces the buffer itself
// and marks the action already-performed, then must refresh via the action's
// own length classification. A tail-extending render (include-tail) grows the
// range, so the refresh must STOP playback; an in-place render (equal length)
// must PRESERVE it. Exercises ApplyPluginChainAction::refreshAfterExternalReplace()
// -- the exact helper both production call sites now use.
//==============================================================================
class PluginChainExternalReplaceRefreshTests : public juce::UnitTest
{
public:
    PluginChainExternalReplaceRefreshTests()
        : juce::UnitTest("Plugin Chain External-Replace Refresh (H1)", "Unit") {}

    void runTest() override
    {
        beginTest("Tail-extending apply stops playback (length changed)");
        testTailExtendingStopsPlayback();

        beginTest("In-place apply preserves playback (length unchanged)");
        testInPlacePreservesPlayback();
    }

private:
    // Reproduce the production call-site sequence: external replaceRange, then
    // markAsAlreadyPerformed + perform (no-op) + refreshAfterExternalReplace.
    void applyExternally(LargeOpUndoTestHelper& h, int64_t startSample, int64_t numSamples,
                         const juce::AudioBuffer<float>& processed)
    {
        auto* action = new ApplyPluginChainAction(h.bufferManager, h.audioEngine,
                                                  h.waveformDisplay, startSample, numSamples,
                                                  processed, "test-chain");
        expect(h.bufferManager.replaceRange(startSample, numSamples, processed),
              "External replaceRange should succeed");
        action->markAsAlreadyPerformed();
        expect(h.undoManager.perform(action), "perform() no-op should return true");
        action->refreshAfterExternalReplace();
    }

    void testTailExtendingStopsPlayback()
    {
        auto source = TestAudio::createSineWave(440.0, 0.5f, kSampleRate, 2.0, 2);

        LargeOpUndoTestHelper helper;
        expect(helper.loadTestBuffer(source, kSampleRate), "Should load the test buffer");

        const int64_t start = 0;
        const int64_t numSamples = helper.bufferManager.getNumSamples() / 2;
        // Processed is LONGER than the selection (reverb/delay tail included).
        const int64_t tail = static_cast<int64_t>(kSampleRate / 2);
        auto processed = TestAudio::createSineWave(
            330.0, 0.4f, kSampleRate,
            static_cast<double>(numSamples + tail) / kSampleRate, 2);

        helper.audioEngine.play();
        helper.audioEngine.setPosition(1.0);
        expect(helper.audioEngine.isPlaying(), "Engine should be playing before apply");

        applyExternally(helper, start, numSamples, processed);

        expect(! helper.audioEngine.isPlaying(),
              "A tail-extending apply changes buffer length and must STOP playback");
        expectWithinAbsoluteError(helper.audioEngine.getCurrentPosition(), 0.0, 1.0e-9,
                                  "Position must reset to 0 after a length-changing apply");
    }

    void testInPlacePreservesPlayback()
    {
        auto source = TestAudio::createSineWave(440.0, 0.5f, kSampleRate, 2.0, 2);

        LargeOpUndoTestHelper helper;
        expect(helper.loadTestBuffer(source, kSampleRate), "Should load the test buffer");

        const int64_t start = 0;
        const int64_t numSamples = helper.bufferManager.getNumSamples() / 2;
        // Processed matches the selection length exactly (in-place EQ-style).
        auto processed = TestAudio::createSineWave(
            330.0, 0.4f, kSampleRate,
            static_cast<double>(numSamples) / kSampleRate, 2);
        expectEquals(static_cast<int64_t>(processed.getNumSamples()), numSamples,
                     "Processed buffer should match the selection length for this case");

        helper.audioEngine.play();
        helper.audioEngine.setPosition(1.0);
        const double positionBefore = helper.audioEngine.getCurrentPosition();
        expect(helper.audioEngine.isPlaying(), "Engine should be playing before apply");

        applyExternally(helper, start, numSamples, processed);

        expect(helper.audioEngine.isPlaying(),
              "An equal-length (in-place) apply must PRESERVE playback");
        expectWithinAbsoluteError(helper.audioEngine.getCurrentPosition(), positionBefore, 0.1,
                                  "An in-place apply must preserve playback position");
    }
};

static PluginChainExternalReplaceRefreshTests pluginChainExternalReplaceRefreshTests;
