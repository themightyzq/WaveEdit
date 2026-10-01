/*
  ==============================================================================

    BatchDSPOps.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The batch DSP chain operations (gain, normalize, DC offset, curved
    fades, Graphical EQ preset, reverse, invert), shared by batch
    processing (BatchJob) and the batch dialog's Preview so the preview
    plays exactly what processing will write.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include "BatchProcessorSettings.h"

namespace waveedit
{
namespace BatchDSPOps
{
    /** Apply one DSP-chain row to the whole buffer, as batch processing does.
        Disabled rows, NONE and the removed PARAMETRIC_EQ are no-ops. */
    void apply(juce::AudioBuffer<float>& buffer, double sampleRate, const BatchDSPSettings& op);

    /** Apply every row of @p chain in order. */
    void applyChain(juce::AudioBuffer<float>& buffer,
                    double sampleRate,
                    const std::vector<BatchDSPSettings>& chain);

    /**
     * True if processing only the first @p previewSeconds of a file would
     * not give the same samples as processing the whole file and keeping its
     * start: normalize and DC offset measure the whole file, fade out and
     * reverse act on its end, and a fade in longer than the excerpt is cut.
     */
    bool needsWholeFile(const std::vector<BatchDSPSettings>& chain, double previewSeconds);

    /** Seconds of audio the batch Preview reads when the chain does not
        need the whole file. */
    constexpr double kPreviewExcerptSeconds = 30.0;

    /**
     * Read @p file and run @p chain over it the way batch processing will,
     * for the batch dialog's Preview: the first kPreviewExcerptSeconds when
     * that is sample-identical to the start of the processed file, otherwise
     * the whole file. Returns false if the file cannot be read.
     */
    bool renderPreview(const juce::File& file,
                       const std::vector<BatchDSPSettings>& chain,
                       juce::AudioBuffer<float>& out,
                       double& sampleRate);
}
} // namespace waveedit
