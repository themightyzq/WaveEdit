/*
  ==============================================================================

    GeneratePlacementTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Integration tests for DSPController's Generate placement (the seam behind
    Insert Silence / Generate Tone / Generate Noise): selection-aware
    replace-vs-insert, correct lengths/content, region shifting, and undo/redo.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "Utils/DocumentManager.h"
#include "Utils/Document.h"
#include "Utils/Region.h"
#include "Controllers/DSPController.h"

namespace
{
constexpr double SR = 48000.0;

// Loads a Document with `seconds` of `channels`-wide audio, every sample = dc.
Document* makeLoadedDoc(DocumentManager& mgr, double seconds, int channels, float dc)
{
    Document* doc = mgr.createDocument();
    if (doc == nullptr)
        return nullptr;

    const int n = (int) std::llround(seconds * SR);
    juce::AudioBuffer<float> buf(channels, n);
    for (int ch = 0; ch < channels; ++ch)
        juce::FloatVectorOperations::fill(buf.getWritePointer(ch), dc, n);

    doc->getBufferManager().setBuffer(buf, SR);            // sets buffer + sample rate
    doc->getAudioEngine().loadFromBuffer(doc->getBufferManager().getBuffer(), SR, channels);
    doc->getWaveformDisplay().reloadFromBuffer(doc->getBufferManager().getBuffer(), SR, false, false);
    return doc;
}

// A fill that writes a constant to every sample (stands in for tone/noise).
auto constantFill(float value)
{
    return [value](juce::AudioBuffer<float>& b, double) {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            juce::FloatVectorOperations::fill(b.getWritePointer(ch), value, b.getNumSamples());
    };
}

float sampleAt(Document* doc, int ch, int64_t index)
{
    return doc->getBufferManager().getBuffer().getReadPointer(ch)[(int) index];
}
} // namespace

//==============================================================================
class GeneratePlacementTests : public juce::UnitTest
{
public:
    GeneratePlacementTests() : juce::UnitTest("Generate Placement", "Integration") {}

    void runTest() override
    {
        testInsertSilenceAtCursorGrows();
        testGenerateOverSelectionReplaces();
        testInsertShiftsTrailingRegion();
        testUndoRedoRestores();
        testOverflowDurationIsRejected();
    }

private:
    void testInsertSilenceAtCursorGrows()
    {
        beginTest("Insert silence at the cursor grows the file and inserts silence");

        DocumentManager mgr;
        DSPController dsp;
        Document* doc = makeLoadedDoc(mgr, 1.0, 2, 0.5f);
        expect(doc != nullptr);

        const int64_t original = doc->getBufferManager().getNumSamples();
        const double cursorSec = 0.5;
        doc->getWaveformDisplay().clearSelection();
        doc->getWaveformDisplay().setEditCursor(cursorSec);

        const double insertSec = 0.25;
        const int64_t insertSamples = (int64_t) std::llround(insertSec * SR);
        const int64_t cursorSample = doc->getBufferManager().timeToSample(cursorSec);

        dsp.generateAndPlace(doc, insertSec, "Insert Silence",
                             [](juce::AudioBuffer<float>&, double) {});  // silence

        expectEquals(doc->getBufferManager().getNumSamples(), original + insertSamples,
                     "buffer grew by the inserted length");
        // A sample in the middle of the inserted region is silent...
        expectWithinAbsoluteError(sampleAt(doc, 0, cursorSample + insertSamples / 2), 0.0f,
                                  1.0e-6f, "inserted region is silent");
        // ...and original content still surrounds it.
        expectWithinAbsoluteError(sampleAt(doc, 0, 0), 0.5f, 1.0e-6f, "head untouched");
        expectWithinAbsoluteError(sampleAt(doc, 0, doc->getBufferManager().getNumSamples() - 1),
                                  0.5f, 1.0e-6f, "tail untouched");
    }

    void testGenerateOverSelectionReplaces()
    {
        beginTest("Generate over a selection overwrites just the selection (no length change)");

        DocumentManager mgr;
        DSPController dsp;
        Document* doc = makeLoadedDoc(mgr, 1.0, 1, 0.5f);
        expect(doc != nullptr);

        const int64_t original = doc->getBufferManager().getNumSamples();
        const double selStart = 0.25, selEnd = 0.5;
        doc->getWaveformDisplay().setSelection(selStart, selEnd);

        dsp.generateAndPlace(doc, 999.0 /*ignored when selection present*/, "Generate Tone",
                             constantFill(0.8f));

        expectEquals(doc->getBufferManager().getNumSamples(), original,
                     "length unchanged for a same-length replace");

        const int64_t s = doc->getBufferManager().timeToSample(selStart);
        const int64_t e = doc->getBufferManager().timeToSample(selEnd);
        expectWithinAbsoluteError(sampleAt(doc, 0, (s + e) / 2), 0.8f, 1.0e-6f,
                                  "selection overwritten with generated content");
        expectWithinAbsoluteError(sampleAt(doc, 0, 0), 0.5f, 1.0e-6f, "before selection untouched");
        expectWithinAbsoluteError(sampleAt(doc, 0, original - 1), 0.5f, 1.0e-6f,
                                  "after selection untouched");
    }

    void testInsertShiftsTrailingRegion()
    {
        beginTest("Insert shifts a region that sits after the cursor");

        DocumentManager mgr;
        DSPController dsp;
        Document* doc = makeLoadedDoc(mgr, 2.0, 1, 0.5f);
        expect(doc != nullptr);

        // Region at [1.0s, 1.5s], cursor at 0.5s (before the region).
        const int64_t regionStart = doc->getBufferManager().timeToSample(1.0);
        const int64_t regionEnd = doc->getBufferManager().timeToSample(1.5);
        doc->getRegionManager().addRegion(Region("R", regionStart, regionEnd));

        doc->getWaveformDisplay().clearSelection();
        doc->getWaveformDisplay().setEditCursor(0.5);

        const double insertSec = 0.5;
        const int64_t insertSamples = (int64_t) std::llround(insertSec * SR);

        dsp.generateAndPlace(doc, insertSec, "Insert Silence",
                             [](juce::AudioBuffer<float>&, double) {});

        expect(doc->getRegionManager().getNumRegions() >= 1, "region survived the insert");
        if (const auto* r = doc->getRegionManager().getRegion(0))
            expectEquals(r->getStartSample(), regionStart + insertSamples,
                         "region shifted forward by the inserted length");
    }

    void testUndoRedoRestores()
    {
        beginTest("Undo restores original length/content; redo re-applies");

        DocumentManager mgr;
        DSPController dsp;
        Document* doc = makeLoadedDoc(mgr, 1.0, 1, 0.5f);
        expect(doc != nullptr);

        const int64_t original = doc->getBufferManager().getNumSamples();
        doc->getWaveformDisplay().clearSelection();
        doc->getWaveformDisplay().setEditCursor(0.5);

        const double insertSec = 0.25;
        const int64_t insertSamples = (int64_t) std::llround(insertSec * SR);

        dsp.generateAndPlace(doc, insertSec, "Insert Silence",
                             [](juce::AudioBuffer<float>&, double) {});
        expectEquals(doc->getBufferManager().getNumSamples(), original + insertSamples,
                     "grew after insert");

        expect(doc->getUndoManager().undo(), "undo succeeds");
        expectEquals(doc->getBufferManager().getNumSamples(), original, "length restored on undo");
        expectWithinAbsoluteError(sampleAt(doc, 0, doc->getBufferManager().timeToSample(0.5)),
                                  0.5f, 1.0e-6f, "content restored on undo");

        expect(doc->getUndoManager().redo(), "redo succeeds");
        expectEquals(doc->getBufferManager().getNumSamples(), original + insertSamples,
                     "grew again on redo");
    }

    void testOverflowDurationIsRejected()
    {
        beginTest("A duration that would overflow the int sample count is rejected safely");

        DocumentManager mgr;
        DSPController dsp;
        Document* doc = makeLoadedDoc(mgr, 1.0, 1, 0.5f);
        expect(doc != nullptr);

        const int64_t original = doc->getBufferManager().getNumSamples();
        doc->getWaveformDisplay().clearSelection();
        doc->getWaveformDisplay().setEditCursor(0.5);

        // 1e9 seconds * 48kHz overflows int32 -- the defensive bound must return
        // without touching the buffer (no crash, no change).
        dsp.generateAndPlace(doc, 1.0e9, "Insert Silence",
                             [](juce::AudioBuffer<float>&, double) {});

        expectEquals(doc->getBufferManager().getNumSamples(), original,
                     "buffer unchanged when the requested length overflows");
    }
};

static GeneratePlacementTests generatePlacementTests;
