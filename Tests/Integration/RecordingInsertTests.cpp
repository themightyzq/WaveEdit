/*
  ==============================================================================

    RecordingInsertTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Integration tests for RecordingController::insertTake() /
    populateNewDocument() (the v0.9.1 hotfix): resampling a take to the
    document's own rate, conforming channel counts, applying the insert as
    one undoable step with region/marker shifting, and never corrupting or
    losing a take on an unreconcilable channel mismatch.

  ==============================================================================
*/

#include <cmath>
#include <cstring>

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "Controllers/RecordingController.h"
#include "Utils/DocumentManager.h"
#include "Utils/Document.h"
#include "Utils/Region.h"
#include "Utils/Marker.h"
#include "../TestUtils/TestAudioFiles.h"

namespace
{
// Loads a Document with `seconds` of `channels`-wide audio at `rate`, every
// sample = dc. Mirrors GeneratePlacementTests' makeLoadedDoc so the document
// is fully wired up (buffer manager + engine + waveform display) the way the
// real app leaves it before a recording can be inserted.
Document* makeLoadedDoc(DocumentManager& mgr, double rate, double seconds, int channels, float dc)
{
    Document* doc = mgr.createDocument();
    if (doc == nullptr)
        return nullptr;

    const int n = (int) std::llround(seconds * rate);
    juce::AudioBuffer<float> buf(channels, n);
    for (int ch = 0; ch < channels; ++ch)
        juce::FloatVectorOperations::fill(buf.getWritePointer(ch), dc, n);

    doc->getBufferManager().setBuffer(buf, rate);
    doc->getAudioEngine().loadFromBuffer(doc->getBufferManager().getBuffer(), rate, channels);
    doc->getWaveformDisplay().reloadFromBuffer(doc->getBufferManager().getBuffer(), rate, false, false);
    return doc;
}

float sampleAt(Document* doc, int ch, int64_t index)
{
    return doc->getBufferManager().getBuffer().getReadPointer(ch)[(int) index];
}
} // namespace

//==============================================================================
class RecordingInsertTests : public juce::UnitTest
{
public:
    RecordingInsertTests() : juce::UnitTest("Recording Insert (v0.9.1)", "Integration") {}

    void runTest() override
    {
        testSameRateSameChannelsInsertMidFile();
        testResamplesTakeToDocumentRate();
        testMonoTakeIntoStereoDocument();
        testUnsupportedChannelMismatchLeavesDocumentUntouched();
        testRegionAndMarkerShiftAfterInsertPoint();
        testPopulateNewDocumentRecordsTakeSampleRate();
    }

private:
    void testSameRateSameChannelsInsertMidFile()
    {
        beginTest("Same-rate, same-channel take inserted mid-file: length, content, undo");

        DocumentManager mgr;
        constexpr double rate = 44100.0;
        Document* doc = makeLoadedDoc(mgr, rate, 2.0, 2, 0.5f);
        expect(doc != nullptr);

        // Snapshot the original buffer for an exact sample-for-sample undo check.
        juce::AudioBuffer<float> original;
        original.makeCopyOf(doc->getBufferManager().getBuffer());
        const int64_t originalLength = doc->getBufferManager().getNumSamples();

        const double insertSeconds = 1.0;
        const int64_t insertSample = doc->getBufferManager().timeToSample(insertSeconds);

        juce::AudioBuffer<float> take = TestAudio::createDCOffset(0.75f, rate, 0.5, 2);
        const int64_t takeLength = take.getNumSamples();

        juce::String error;
        const bool ok = RecordingController::insertTake(*doc, take, rate, insertSeconds, error);
        expect(ok, "insertTake succeeded: " + error);

        expectEquals(doc->getBufferManager().getNumSamples(), originalLength + takeLength,
                     "length grew by the take length");

        // Before the insert point: unchanged.
        expectWithinAbsoluteError(sampleAt(doc, 0, 0), 0.5f, 1.0e-6f, "head untouched");
        expectWithinAbsoluteError(sampleAt(doc, 0, insertSample - 1), 0.5f, 1.0e-6f,
                                  "sample just before insert point untouched");

        // Inserted range: equals the take.
        expectWithinAbsoluteError(sampleAt(doc, 0, insertSample), 0.75f, 1.0e-6f,
                                  "first inserted sample equals the take");
        expectWithinAbsoluteError(sampleAt(doc, 1, insertSample + takeLength - 1), 0.75f, 1.0e-6f,
                                  "last inserted sample (ch1) equals the take");

        // After the insert: original tail shifted exactly by takeLength.
        expectWithinAbsoluteError(sampleAt(doc, 0, insertSample + takeLength), 0.5f, 1.0e-6f,
                                  "original content resumes right after the inserted range");
        expectWithinAbsoluteError(
            sampleAt(doc, 0, doc->getBufferManager().getNumSamples() - 1), 0.5f, 1.0e-6f,
            "tail (shifted) untouched");

        // One undo restores the original buffer exactly, sample-for-sample.
        expect(doc->getUndoManager().undo(), "undo succeeds");
        expectEquals(doc->getBufferManager().getNumSamples(), originalLength, "length restored on undo");

        const auto& restored = doc->getBufferManager().getBuffer();
        bool identical = (restored.getNumChannels() == original.getNumChannels());
        for (int ch = 0; ch < restored.getNumChannels() && identical; ++ch)
        {
            if (std::memcmp(restored.getReadPointer(ch), original.getReadPointer(ch),
                            (size_t) restored.getNumSamples() * sizeof(float)) != 0)
                identical = false;
        }
        expect(identical, "undo restores the buffer sample-for-sample");
    }

    void testResamplesTakeToDocumentRate()
    {
        beginTest("A 48kHz take into a 44.1kHz document is resampled to the document's rate");

        DocumentManager mgr;
        constexpr double docRate = 44100.0;
        constexpr double takeRate = 48000.0;
        Document* doc = makeLoadedDoc(mgr, docRate, 2.0, 1, 0.2f);
        expect(doc != nullptr);

        const int64_t originalLength = doc->getBufferManager().getNumSamples();
        juce::AudioBuffer<float> take = TestAudio::createSineWave(440.0, 0.5f, takeRate, 0.5, 1);
        const int64_t takeLenAtTakeRate = take.getNumSamples();
        const int64_t expectedAddedLength =
            (int64_t) std::llround((double) takeLenAtTakeRate * docRate / takeRate);

        juce::String error;
        const bool ok = RecordingController::insertTake(*doc, take, takeRate, 1.0, error);
        expect(ok, "insertTake succeeded: " + error);

        const int64_t addedLength = doc->getBufferManager().getNumSamples() - originalLength;
        expect(std::abs(addedLength - expectedAddedLength) <= 2,
              "added length matches the take resampled to the document rate (+/-2 samples)");

        expectEquals(doc->getBufferManager().getSampleRate(), docRate,
                     "buffer manager rate stays at the document's rate");
        expectEquals(doc->getAudioEngine().getSampleRate(), docRate,
                     "audio engine rate stays at the document's rate");
    }

    void testMonoTakeIntoStereoDocument()
    {
        beginTest("A mono take into a stereo document lands on both channels (no garbage)");

        DocumentManager mgr;
        constexpr double rate = 44100.0;
        Document* doc = makeLoadedDoc(mgr, rate, 1.0, 2, 0.0f);
        expect(doc != nullptr);

        const double insertSeconds = 0.5;
        const int64_t insertSample = doc->getBufferManager().timeToSample(insertSeconds);

        // Mono DC take: conformChannels duplicates the mono channel to every
        // target channel, so both channels of the stereo doc should read 0.25.
        juce::AudioBuffer<float> take = TestAudio::createDCOffset(0.25f, rate, 0.2, 1);
        const int64_t takeLength = take.getNumSamples();

        juce::String error;
        const bool ok = RecordingController::insertTake(*doc, take, rate, insertSeconds, error);
        expect(ok, "insertTake succeeded: " + error);

        for (int ch = 0; ch < 2; ++ch)
        {
            expectWithinAbsoluteError(sampleAt(doc, ch, insertSample), 0.25f, 1.0e-6f,
                                      "channel " + juce::String(ch) + " start of inserted range is 0.25");
            expectWithinAbsoluteError(sampleAt(doc, ch, insertSample + takeLength - 1), 0.25f, 1.0e-6f,
                                      "channel " + juce::String(ch) + " end of inserted range is 0.25");
        }
    }

    void testUnsupportedChannelMismatchLeavesDocumentUntouched()
    {
        beginTest("A 3-channel take into a stereo document is rejected without changing the document");

        DocumentManager mgr;
        constexpr double rate = 44100.0;
        Document* doc = makeLoadedDoc(mgr, rate, 1.0, 2, 0.5f);
        expect(doc != nullptr);

        juce::AudioBuffer<float> original;
        original.makeCopyOf(doc->getBufferManager().getBuffer());
        const int64_t originalLength = doc->getBufferManager().getNumSamples();

        juce::AudioBuffer<float> take(3, (int) std::llround(0.2 * rate));
        for (int ch = 0; ch < 3; ++ch)
            juce::FloatVectorOperations::fill(take.getWritePointer(ch), 0.9f, take.getNumSamples());

        juce::String error;
        const bool ok = RecordingController::insertTake(*doc, take, rate, 0.5, error);

        expect(!ok, "insertTake returns false for an unreconcilable channel mismatch");
        expect(error.isNotEmpty(), "a user-facing error message is set");

        expectEquals(doc->getBufferManager().getNumSamples(), originalLength,
                     "document length is unchanged");
        expect(!doc->isModified(), "document is not marked modified");

        const auto& unchanged = doc->getBufferManager().getBuffer();
        bool identical = (unchanged.getNumChannels() == original.getNumChannels());
        for (int ch = 0; ch < unchanged.getNumChannels() && identical; ++ch)
        {
            if (std::memcmp(unchanged.getReadPointer(ch), original.getReadPointer(ch),
                            (size_t) unchanged.getNumSamples() * sizeof(float)) != 0)
                identical = false;
        }
        expect(identical, "document buffer content is byte-for-byte unchanged");
    }

    void testRegionAndMarkerShiftAfterInsertPoint()
    {
        beginTest("A region and marker after the insert point shift; one before does not");

        DocumentManager mgr;
        constexpr double rate = 44100.0;
        Document* doc = makeLoadedDoc(mgr, rate, 3.0, 1, 0.5f);
        expect(doc != nullptr);

        const int64_t regionBeforeStart = doc->getBufferManager().timeToSample(0.2);
        const int64_t regionBeforeEnd   = doc->getBufferManager().timeToSample(0.4);
        doc->getRegionManager().addRegion(Region("Before", regionBeforeStart, regionBeforeEnd));

        const int64_t regionAfterStart = doc->getBufferManager().timeToSample(2.0);
        const int64_t regionAfterEnd   = doc->getBufferManager().timeToSample(2.5);
        doc->getRegionManager().addRegion(Region("After", regionAfterStart, regionAfterEnd));

        const int64_t markerBeforePos = doc->getBufferManager().timeToSample(0.3);
        doc->getMarkerManager().addMarker(Marker("MBefore", markerBeforePos));

        const int64_t markerAfterPos = doc->getBufferManager().timeToSample(2.2);
        doc->getMarkerManager().addMarker(Marker("MAfter", markerAfterPos));

        const double insertSeconds = 1.0;
        juce::AudioBuffer<float> take = TestAudio::createDCOffset(0.9f, rate, 0.3, 1);
        const int64_t takeLength = take.getNumSamples();

        juce::String error;
        const bool ok = RecordingController::insertTake(*doc, take, rate, insertSeconds, error);
        expect(ok, "insertTake succeeded: " + error);

        expectEquals(doc->getRegionManager().getNumRegions(), 2, "both regions survived the insert");
        for (int i = 0; i < doc->getRegionManager().getNumRegions(); ++i)
        {
            const auto* r = doc->getRegionManager().getRegion(i);
            expect(r != nullptr);
            if (r->getName() == "Before")
            {
                expectEquals(r->getStartSample(), regionBeforeStart, "region before insert point unshifted (start)");
                expectEquals(r->getEndSample(), regionBeforeEnd, "region before insert point unshifted (end)");
            }
            else if (r->getName() == "After")
            {
                expectEquals(r->getStartSample(), regionAfterStart + takeLength,
                             "region after insert point shifted by take length (start)");
                expectEquals(r->getEndSample(), regionAfterEnd + takeLength,
                             "region after insert point shifted by take length (end)");
            }
        }

        expectEquals(doc->getMarkerManager().getNumMarkers(), 2, "both markers survived the insert");
        for (int i = 0; i < doc->getMarkerManager().getNumMarkers(); ++i)
        {
            const auto* m = doc->getMarkerManager().getMarker(i);
            expect(m != nullptr);
            if (m->getName() == "MBefore")
                expectEquals(m->getPosition(), markerBeforePos, "marker before insert point unshifted");
            else if (m->getName() == "MAfter")
                expectEquals(m->getPosition(), markerAfterPos + takeLength,
                             "marker after insert point shifted by take length");
        }
    }

    void testPopulateNewDocumentRecordsTakeSampleRate()
    {
        beginTest("populateNewDocument records the take's own sample rate on a fresh document");

        DocumentManager mgr;
        Document* doc = mgr.createDocument();
        expect(doc != nullptr);
        expectEquals(doc->getBufferManager().getNumSamples(), (int64_t) 0,
                     "fresh document starts with no audio");

        constexpr double takeRate = 48000.0;
        juce::AudioBuffer<float> take = TestAudio::createSineWave(220.0, 0.4f, takeRate, 0.3, 2);

        RecordingController::populateNewDocument(*doc, take, takeRate);

        expectEquals(doc->getBufferManager().getSampleRate(), takeRate,
                     "buffer manager rate is the take's own rate, not the 44.1kHz default");
        expectEquals(doc->getBufferManager().getNumSamples(), (int64_t) take.getNumSamples(),
                     "buffer holds the full take");
        expect(doc->isModified(), "document is marked modified");
    }
};

static RecordingInsertTests recordingInsertTests;
