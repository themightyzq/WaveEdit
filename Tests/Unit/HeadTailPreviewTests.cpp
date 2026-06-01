/*
  ==============================================================================

    HeadTailPreviewTests.cpp
    Copyright (C) 2026 ZQ SFX

    Unit coverage for the HeadTail preview empty-result guard
    (HeadTailDialog::previewIsPlayable). The audio/engine wiring of the
    preview is verified by manual/audio QA; this guards the pure decision
    that gates Preview/Bypass when a recipe trims the result to nothing.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "UI/HeadTailDialog.h"

class HeadTailPreviewTests : public juce::UnitTest
{
public:
    HeadTailPreviewTests() : juce::UnitTest("HeadTail Preview Guard", "DSP") {}

    void runTest() override
    {
        beginTest("Zero samples is not playable");
        expect(! HeadTailDialog::previewIsPlayable(0, 2));
        expect(! HeadTailDialog::previewIsPlayable(0, 0));

        beginTest("Zero channels is not playable");
        expect(! HeadTailDialog::previewIsPlayable(44100, 0));

        beginTest("Positive samples and channels is playable");
        expect(HeadTailDialog::previewIsPlayable(1, 1));
        expect(HeadTailDialog::previewIsPlayable(44100, 2));

        beginTest("Negative is not playable");
        expect(! HeadTailDialog::previewIsPlayable(-1, 2));
        expect(! HeadTailDialog::previewIsPlayable(44100, -1));
    }
};

static HeadTailPreviewTests headTailPreviewTests;
