/*
  ==============================================================================

    TimePitchDialogTests.cpp
    Copyright (C) 2026 ZQ SFX

    Unit coverage for TimePitchDialog::computePreviewExcerpt -- the pure
    computation that selects the up-to-10-second source excerpt fed to the
    SoundTouch preview. The audio/engine wiring is verified by manual/audio QA;
    this guards the excerpt-range arithmetic (selection vs cursor start,
    end-of-file clamping, short-file and empty-buffer edge cases).

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "UI/TimePitchDialog.h"

class TimePitchDialogTests : public juce::UnitTest
{
public:
    TimePitchDialogTests() : juce::UnitTest("TimePitch Preview Excerpt", "DSP") {}

    void runTest() override
    {
        const double sr = 44100.0;
        const juce::int64 tenSeconds = (juce::int64) (TimePitchDialog::kPreviewSeconds * sr);

        beginTest("No selection uses the cursor position");
        {
            const juce::int64 total = (juce::int64) (30.0 * sr);
            auto r = TimePitchDialog::computePreviewExcerpt(total, sr,
                                                            /*hasSelection*/ false,
                                                            /*selStart*/ 5.0,
                                                            /*selEnd*/ 0.0,
                                                            /*cursor*/ 15.0);
            expect(r.getStart() == (juce::int64) (15.0 * sr), "start at cursor");
            expect(r.getLength() == tenSeconds, "full 10s window mid-file");
        }

        beginTest("Selection start is used when present");
        {
            const juce::int64 total = (juce::int64) (30.0 * sr);
            auto r = TimePitchDialog::computePreviewExcerpt(total, sr,
                                                            /*hasSelection*/ true,
                                                            /*selStart*/ 5.0,
                                                            /*selEnd*/ 30.0,
                                                            /*cursor*/ 20.0);
            expect(r.getStart() == (juce::int64) (5.0 * sr), "start at selection, not cursor");
            expect(r.getLength() == tenSeconds, "full 10s window from selection");
        }

        beginTest("Selection shorter than 10s -> excerpt equals selection");
        {
            const juce::int64 total = (juce::int64) (30.0 * sr);
            auto r = TimePitchDialog::computePreviewExcerpt(total, sr,
                                                            /*hasSelection*/ true,
                                                            /*selStart*/ 5.0,
                                                            /*selEnd*/ 8.0,
                                                            /*cursor*/ 0.0);
            expect(r.getStart() == (juce::int64) (5.0 * sr), "start at selection start");
            expect(r.getEnd() == (juce::int64) (8.0 * sr), "end capped at selection end");
            expect(r.getLength() == (juce::int64) (3.0 * sr), "excerpt equals the 3s selection");
        }

        beginTest("Selection longer than 10s -> excerpt capped at 10s");
        {
            const juce::int64 total = (juce::int64) (60.0 * sr);
            auto r = TimePitchDialog::computePreviewExcerpt(total, sr,
                                                            /*hasSelection*/ true,
                                                            /*selStart*/ 5.0,
                                                            /*selEnd*/ 40.0,
                                                            /*cursor*/ 0.0);
            expect(r.getStart() == (juce::int64) (5.0 * sr), "start at selection start");
            expect(r.getLength() == tenSeconds, "capped at 10s despite 35s selection");
            expect(r.getEnd() == (juce::int64) (15.0 * sr), "end at start + 10s");
        }

        beginTest("Excerpt clamps at end-of-file");
        {
            const juce::int64 total = (juce::int64) (30.0 * sr);
            auto r = TimePitchDialog::computePreviewExcerpt(total, sr,
                                                            /*hasSelection*/ false,
                                                            /*selStart*/ 0.0,
                                                            /*selEnd*/ 0.0,
                                                            /*cursor*/ 25.0);
            expect(r.getStart() == (juce::int64) (25.0 * sr), "start near end");
            expect(r.getEnd() == total, "end clamped to total samples");
            expect(r.getLength() == total - (juce::int64) (25.0 * sr), "shortened window");
        }

        beginTest("Short file (< 10s) yields the full range");
        {
            const juce::int64 total = (juce::int64) (5.0 * sr);
            auto r = TimePitchDialog::computePreviewExcerpt(total, sr,
                                                            /*hasSelection*/ false,
                                                            /*selStart*/ 0.0,
                                                            /*selEnd*/ 0.0,
                                                            /*cursor*/ 0.0);
            expect(r.getStart() == 0, "start at 0");
            expect(r.getEnd() == total, "end at total");
            expect(r.getLength() == total, "whole file is the excerpt");
        }

        beginTest("Empty buffer returns an empty range");
        {
            auto r = TimePitchDialog::computePreviewExcerpt(0, sr, false, 0.0, 0.0, 0.0);
            expect(r.getStart() == 0 && r.getEnd() == 0, "empty range for empty buffer");
            expect(r.isEmpty(), "range reports empty");
        }

        beginTest("Invalid sample rate returns an empty range");
        {
            auto r = TimePitchDialog::computePreviewExcerpt((juce::int64) (5.0 * sr),
                                                            0.0, false, 0.0, 0.0, 0.0);
            expect(r.isEmpty(), "empty range for zero sample rate");
        }

        beginTest("Selection start beyond end-of-file yields an empty range");
        {
            const juce::int64 total = (juce::int64) (10.0 * sr);
            auto r = TimePitchDialog::computePreviewExcerpt(total, sr,
                                                            /*hasSelection*/ true,
                                                            /*selStart*/ 60.0,
                                                            /*selEnd*/ 70.0,
                                                            /*cursor*/ 0.0);
            expect(r.getStart() == total, "start clamped to total");
            expect(r.isEmpty(), "empty range past end-of-file");
        }

        beginTest("Negative cursor/selection clamps to zero");
        {
            const juce::int64 total = (juce::int64) (30.0 * sr);
            auto rCursor = TimePitchDialog::computePreviewExcerpt(total, sr,
                                                                  false, 0.0, 0.0, -3.0);
            expect(rCursor.getStart() == 0, "negative cursor clamps to 0");
            expect(rCursor.getLength() == tenSeconds, "full window from 0");

            auto rSel = TimePitchDialog::computePreviewExcerpt(total, sr,
                                                               true, -7.5, 30.0, 0.0);
            expect(rSel.getStart() == 0, "negative selection clamps to 0");
        }

        beginTest("previewIsPlayable guards empty and channel-less buffers");
        {
            expect(!TimePitchDialog::previewIsPlayable(0, 2), "no samples -> not playable");
            expect(!TimePitchDialog::previewIsPlayable(1024, 0), "no channels -> not playable");
            expect(TimePitchDialog::previewIsPlayable(1024, 1), "mono with samples -> playable");
            expect(TimePitchDialog::previewIsPlayable(1, 8), "single sample, 8ch -> playable");
        }
    }
};

static TimePitchDialogTests timePitchDialogTests;
