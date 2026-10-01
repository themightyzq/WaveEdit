/*
  ==============================================================================

    BatchPreviewMatchTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Regression test for the Phase 1 finding "batch preview ignores curved
    fades, EQ, Reverse and Invert": the batch dialog's Preview had its own
    copy of the DSP chain that only knew gain, normalize, DC offset and
    linear fades. Preview now renders through BatchDSPOps, the code batch
    processing itself runs, so it must match the processed file sample for
    sample.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "../../Source/Batch/BatchDSPOps.h"
#include "../../Source/Batch/BatchJob.h"
#include "../../Source/Audio/AudioFileManager.h"
#include "../../Source/DSP/EQPresetManager.h"

using namespace waveedit;

class BatchPreviewMatchTests : public juce::UnitTest
{
public:
    BatchPreviewMatchTests() : juce::UnitTest("Batch Preview Matches Processing", "Batch") {}

    void runTest() override
    {
        dir().deleteRecursively();
        dir().createDirectory();

        testPreviewMatchesProcessedFile();
        testWholeFileRule();

        dir().deleteRecursively();
    }

private:
    static juce::File dir()
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("WaveEditBatchPreviewMatch");
    }

    static BatchDSPSettings op(BatchDSPOperation operation)
    {
        BatchDSPSettings s;
        s.operation = operation;
        s.enabled = true;
        return s;
    }

    void testPreviewMatchesProcessedFile()
    {
        beginTest("Preview of curved fades + EQ + Reverse + Invert equals the batch output");

        constexpr double rate = 48000.0;
        juce::AudioBuffer<float> source(2, (int) (2.0 * rate));
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < source.getNumSamples(); ++i)
                source.setSample(ch, i, 0.4f * std::sin(0.03f * (float) i * (float) (ch + 1))
                                        + 0.0005f * (float) (i % 100));

        const auto input = dir().getChildFile("input.wav");
        AudioFileManager fm;
        expect(fm.saveAsWav(input, source, rate, 32), "fixture written as 32-bit float");

        const auto eqPresets = EQPresetManager::getFactoryPresetNames();
        expect(!eqPresets.isEmpty(), "a factory EQ preset exists");

        std::vector<BatchDSPSettings> chain;
        auto fadeIn = op(BatchDSPOperation::FADE_IN);
        fadeIn.fadeDurationMs = 250.0f;
        fadeIn.fadeType = 3;   // S-curve
        chain.push_back(fadeIn);
        auto eq = op(BatchDSPOperation::GRAPHICAL_EQ);
        eq.eqPresetName = eqPresets[0];
        chain.push_back(eq);
        chain.push_back(op(BatchDSPOperation::REVERSE));
        chain.push_back(op(BatchDSPOperation::INVERT));
        auto fadeOut = op(BatchDSPOperation::FADE_OUT);
        fadeOut.fadeDurationMs = 300.0f;
        fadeOut.fadeType = 1;  // exponential
        chain.push_back(fadeOut);

        // What the batch dialog's Preview plays.
        juce::AudioBuffer<float> preview;
        double previewRate = 0.0;
        expect(BatchDSPOps::renderPreview(input, chain, preview, previewRate), "preview renders");
        expectEquals(previewRate, rate);

        // What batch processing writes.
        BatchProcessorSettings settings;
        settings.outputDirectory = dir();
        settings.outputPattern = "{filename}_processed";
        settings.overwriteExisting = true;
        settings.outputFormat.bitDepth = 32;
        settings.dspChain = chain;
        BatchJob job(input, settings, 1);
        const auto result = job.execute();
        expect(result.status == BatchJobStatus::COMPLETED, "batch job completes: " + result.errorMessage);

        juce::AudioBuffer<float> processed;
        expect(fm.loadIntoBufferUnchecked(result.outputFile, processed), "batch output reads back");

        expectEquals(preview.getNumSamples(), processed.getNumSamples(), "same length");
        expectEquals(preview.getNumChannels(), processed.getNumChannels(), "same channels");
        if (preview.getNumSamples() != processed.getNumSamples()
            || preview.getNumChannels() != processed.getNumChannels())
            return;

        float worst = 0.0f;
        for (int ch = 0; ch < preview.getNumChannels(); ++ch)
            for (int i = 0; i < preview.getNumSamples(); ++i)
                worst = juce::jmax(worst, std::abs(preview.getSample(ch, i) - processed.getSample(ch, i)));
        expectLessOrEqual(worst, 1.0e-6f, "preview equals the processed file (max abs difference)");
    }

    void testWholeFileRule()
    {
        beginTest("Preview reads the whole file exactly when the chain needs it");

        std::vector<BatchDSPSettings> local { op(BatchDSPOperation::GAIN), op(BatchDSPOperation::INVERT) };
        auto shortFade = op(BatchDSPOperation::FADE_IN);
        shortFade.fadeDurationMs = 100.0f;
        local.push_back(shortFade);
        expect(!BatchDSPOps::needsWholeFile(local, 30.0), "gain/invert/short fade in: excerpt is exact");

        for (auto whole : { BatchDSPOperation::NORMALIZE, BatchDSPOperation::DC_OFFSET,
                            BatchDSPOperation::FADE_OUT, BatchDSPOperation::REVERSE })
            expect(BatchDSPOps::needsWholeFile({ op(whole) }, 30.0),
                   "operation " + juce::String((int) whole) + " needs the whole file");

        auto longFade = op(BatchDSPOperation::FADE_IN);
        longFade.fadeDurationMs = 31000.0f;
        expect(BatchDSPOps::needsWholeFile({ longFade }, 30.0), "a fade in longer than the excerpt");

        auto disabled = op(BatchDSPOperation::NORMALIZE);
        disabled.enabled = false;
        expect(!BatchDSPOps::needsWholeFile({ disabled }, 30.0), "disabled rows are ignored");
    }
};

static BatchPreviewMatchTests batchPreviewMatchTests;
