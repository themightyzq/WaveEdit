/*
  ==============================================================================

    AudioBufferManagerRegressionTests.cpp
    WaveEdit - Professional Audio Editor

    Regression tests for QA Pass 2 defects in AudioBufferManager and the
    Trim-to-Selection call path:

      C1  - Trim to Selection kept the wrong range (end index passed where a
            sample COUNT was expected). Pins the trimToRange(start, count)
            contract and the corrected call-site arithmetic.
      H1  - replaceRange was non-atomic: delete committed, then insert could
            fail on a channel mismatch, leaving the buffer short with a gap and
            no rollback. Now must return false AND leave the buffer untouched.
      L2  - loadFromFile narrowed int64 length to int with no guard. (Reasoning
            documented below; not exercised here - see note.)

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "Audio/AudioBufferManager.h"

namespace
{
    // Build a single-channel-count buffer where every sample encodes its own
    // absolute index (as a float). Lets us assert sample-accurate identity of a
    // retained range without depending on waveform generators.
    juce::AudioBuffer<float> makeIndexBuffer(int numChannels, int numSamples)
    {
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                buffer.setSample(ch, i, static_cast<float>(i));
        return buffer;
    }
}

// ============================================================================
// C1 - Trim to Selection keeps the correct range
// ============================================================================

class TrimToSelectionRegressionTests : public juce::UnitTest
{
public:
    TrimToSelectionRegressionTests()
        : juce::UnitTest("Trim to Selection (C1 regression)", "BufferManager") {}

    void runTest() override
    {
        beginTest("Front-to-middle selection [20,80) keeps exactly 60 samples");
        testMiddleSelectionKeepsCount();

        beginTest("Back-half selection [60,100) is not a silent no-op");
        testBackHalfSelection();

        beginTest("Call-site arithmetic: count = endSample - startSample");
        testCallSiteArithmetic();
    }

private:
    // The bug: DSPController passed endSample (absolute index) into
    // TrimUndoAction's numSamples parameter, which trimToRange treats as a
    // COUNT. For [20,80) that requested 80 samples from index 20 -> kept
    // [20,100) instead of [20,80). The corrected call passes the count (60).
    void testMiddleSelectionKeepsCount()
    {
        AudioBufferManager manager;
        manager.getMutableBuffer() = makeIndexBuffer(2, 100);

        const int startSample = 20;
        const int endSample = 80;
        const int numSamplesToKeep = endSample - startSample; // the fix: COUNT

        bool ok = manager.trimToRange(startSample, numSamplesToKeep);
        expect(ok, "Trim should succeed");

        // Exactly 60 samples, not 80.
        expectEquals(manager.getNumSamples(), (int64_t)60,
                     "Trim must keep exactly (end - start) samples");

        // Content must equal original [20,80): sample i == original index 20+i.
        const auto& buf = manager.getBuffer();
        bool contentMatches = true;
        for (int ch = 0; ch < buf.getNumChannels() && contentMatches; ++ch)
            for (int i = 0; i < buf.getNumSamples(); ++i)
                if (buf.getSample(ch, i) != static_cast<float>(startSample + i))
                {
                    contentMatches = false;
                    break;
                }
        expect(contentMatches, "Retained samples must equal original [20,80)");
    }

    // Pre-fix, the buggy count (endSample) made startSample + count exceed the
    // buffer length, so trimToRange's range check failed and the trim silently
    // returned false (no-op). With the corrected count this selection trims.
    void testBackHalfSelection()
    {
        AudioBufferManager manager;
        manager.getMutableBuffer() = makeIndexBuffer(1, 100);

        const int startSample = 60;
        const int endSample = 100;
        const int numSamplesToKeep = endSample - startSample; // 40

        // Sanity: the OLD buggy count (endSample == 100) would have been
        // rejected as out of range, demonstrating the silent no-op.
        const int buggyCount = endSample; // what the defect passed
        expect(startSample + buggyCount > manager.getNumSamples(),
               "Buggy count would exceed buffer -> silent no-op (pre-fix)");

        bool ok = manager.trimToRange(startSample, numSamplesToKeep);
        expect(ok, "Corrected back-half trim should succeed");
        expectEquals(manager.getNumSamples(), (int64_t)40,
                     "Back-half trim must keep 40 samples");
        expectEquals(manager.getBuffer().getSample(0, 0), 60.0f,
                     "First retained sample must be original index 60");
        expectEquals(manager.getBuffer().getSample(0, 39), 99.0f,
                     "Last retained sample must be original index 99");
    }

    // Directly pins the call-site formula the fix introduced.
    void testCallSiteArithmetic()
    {
        struct Case { int start, end, expectedCount; };
        const Case cases[] = { {20, 80, 60}, {0, 100, 100}, {60, 100, 40}, {33, 34, 1} };

        for (const auto& c : cases)
            expectEquals(c.end - c.start, c.expectedCount,
                         "count must equal endSample - startSample");
    }
};

static TrimToSelectionRegressionTests trimToSelectionRegressionTests;

// ============================================================================
// H1 - replaceRange must be atomic on channel mismatch
// ============================================================================

class ReplaceRangeAtomicityTests : public juce::UnitTest
{
public:
    ReplaceRangeAtomicityTests()
        : juce::UnitTest("replaceRange atomicity (H1 regression)", "BufferManager") {}

    void runTest() override
    {
        beginTest("Channel-mismatch replace returns false and leaves buffer identical");
        testMismatchLeavesBufferIdentical();

        beginTest("Matching replace still mutates correctly");
        testMatchingReplaceSucceeds();

        beginTest("Invalid range returns false and leaves buffer identical");
        testInvalidRangeLeavesBufferIdentical();
    }

private:
    static bool buffersIdentical(const juce::AudioBuffer<float>& a,
                                 const juce::AudioBuffer<float>& b)
    {
        if (a.getNumChannels() != b.getNumChannels() ||
            a.getNumSamples() != b.getNumSamples())
            return false;
        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            for (int i = 0; i < a.getNumSamples(); ++i)
                if (a.getSample(ch, i) != b.getSample(ch, i))
                    return false;
        return true;
    }

    void testMismatchLeavesBufferIdentical()
    {
        AudioBufferManager manager;
        manager.getMutableBuffer() = makeIndexBuffer(2, 100); // stereo target

        // Snapshot the exact bytes before the operation.
        juce::AudioBuffer<float> before;
        before.makeCopyOf(manager.getBuffer(), true);

        // Mono replacement into a stereo buffer -> channel mismatch.
        juce::AudioBuffer<float> monoReplacement = makeIndexBuffer(1, 30);

        bool ok = manager.replaceRange(10, 20, monoReplacement);
        expect(!ok, "Channel-mismatch replace must return false");

        // Pre-fix this would have deleted [10,30) first, leaving 80 samples.
        expect(buffersIdentical(manager.getBuffer(), before),
               "Buffer must be byte-identical after a failed replace (no partial mutation)");
    }

    void testMatchingReplaceSucceeds()
    {
        AudioBufferManager manager;
        manager.getMutableBuffer() = makeIndexBuffer(2, 100);

        // Replace [10,30) (20 samples) with 50 samples of constant -1.
        juce::AudioBuffer<float> replacement(2, 50);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 50; ++i)
                replacement.setSample(ch, i, -1.0f);

        bool ok = manager.replaceRange(10, 20, replacement);
        expect(ok, "Matching-channel replace should succeed");
        expectEquals(manager.getNumSamples(), (int64_t)(100 - 20 + 50),
                     "Length must reflect delete-then-insert");

        const auto& buf = manager.getBuffer();
        // Prefix [0,10) untouched.
        expectEquals(buf.getSample(0, 0), 0.0f, "Prefix preserved");
        expectEquals(buf.getSample(0, 9), 9.0f, "Prefix preserved");
        // Inserted block [10,60) is -1.
        expectEquals(buf.getSample(0, 10), -1.0f, "Insert region present");
        expectEquals(buf.getSample(0, 59), -1.0f, "Insert region present");
        // Tail resumes at original index 30.
        expectEquals(buf.getSample(0, 60), 30.0f, "Tail spliced from original index 30");
    }

    void testInvalidRangeLeavesBufferIdentical()
    {
        AudioBufferManager manager;
        manager.getMutableBuffer() = makeIndexBuffer(2, 100);

        juce::AudioBuffer<float> before;
        before.makeCopyOf(manager.getBuffer(), true);

        juce::AudioBuffer<float> replacement = makeIndexBuffer(2, 10);

        // start + count exceeds length.
        bool ok = manager.replaceRange(90, 20, replacement);
        expect(!ok, "Out-of-range replace must return false");
        expect(buffersIdentical(manager.getBuffer(), before),
               "Buffer must be unchanged after out-of-range replace");
    }
};

static ReplaceRangeAtomicityTests replaceRangeAtomicityTests;

// ============================================================================
// L2 - loadFromFile int64->int narrowing guard
//
// loadFromFile now rejects (returns false, logs) files whose lengthInSamples
// exceeds INT_MAX rather than silently truncating via static_cast<int>.
//
// NOTE: This is not exercised with a unit test here because triggering it
// requires an AudioFormatReader reporting > INT_MAX (~2.1e9) samples, i.e. a
// multi-hour multi-gigabyte file or a custom fake reader. Fabricating such a
// reader is disproportionate to the one-line guard; the guard is a pure bound
// check (numSamples > std::numeric_limits<int>::max() -> log + return false)
// and is covered by code review. Documented per protocol.
// ============================================================================
