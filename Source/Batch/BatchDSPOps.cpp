/*
  ==============================================================================

    BatchDSPOps.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Moved out of BatchJob.cpp (the operation bodies are unchanged) so the
    batch Preview can run the same code. See BatchDSPOps.h.

  ==============================================================================
*/

#include "BatchDSPOps.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Audio/AudioProcessor.h"
#include "../DSP/DynamicParametricEQ.h"
#include "../DSP/EQPresetManager.h"
#include <cmath>
#include <limits>

namespace waveedit
{
namespace BatchDSPOps
{
namespace
{
    void applyGain(juce::AudioBuffer<float>& buffer, float gainDb)
    {
        buffer.applyGain(juce::Decibels::decibelsToGain(gainDb));
    }

    void applyNormalize(juce::AudioBuffer<float>& buffer, float targetDb)
    {
        const float targetLinear = juce::Decibels::decibelsToGain(targetDb);

        float peak = 0.0f;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto range = buffer.findMinMax(channel, 0, buffer.getNumSamples());
            peak = std::max(peak, std::max(std::abs(range.getStart()), std::abs(range.getEnd())));
        }

        if (peak > 0.0f)
            buffer.applyGain(targetLinear / peak);
    }

    void applyDCOffset(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        if (numSamples <= 0)
            return;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float sum = 0.0f;
            const float* data = buffer.getReadPointer(channel);
            for (int i = 0; i < numSamples; ++i)
                sum += data[i];

            const float dcOffset = sum / static_cast<float>(numSamples);

            float* writeData = buffer.getWritePointer(channel);
            for (int i = 0; i < numSamples; ++i)
                writeData[i] -= dcOffset;
        }
    }

    /** Fade gain at position t in [0, 1) of the fade, for a fade IN. */
    float fadeInGain(float t, int curveType)
    {
        switch (curveType)
        {
            case 1:  return t * t;                                                   // Exponential
            case 2:  return std::sqrt(t);                                            // Logarithmic
            case 3:  return 0.5f * (1.0f - std::cos(t * juce::MathConstants<float>::pi)); // S-Curve
            case 0:                                                                  // Linear
            default: return t;
        }
    }

    /** Fade gain at position t in [0, 1) of the fade, for a fade OUT. */
    float fadeOutGain(float t, int curveType)
    {
        switch (curveType)
        {
            case 1:  return (1.0f - t) * (1.0f - t);
            case 2:  return std::sqrt(1.0f - t);
            case 3:  return 0.5f * (1.0f + std::cos(t * juce::MathConstants<float>::pi));
            case 0:
            default: return 1.0f - t;
        }
    }

    int fadeLengthSamples(const juce::AudioBuffer<float>& buffer, double sampleRate, float durationMs)
    {
        const int fadeSamples = static_cast<int>((durationMs / 1000.0) * sampleRate);
        return std::min(fadeSamples, buffer.getNumSamples());
    }

    void applyFadeIn(juce::AudioBuffer<float>& buffer, double sampleRate, float durationMs, int curveType)
    {
        const int fadeSamples = fadeLengthSamples(buffer, sampleRate, durationMs);
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            for (int i = 0; i < fadeSamples; ++i)
                data[i] *= fadeInGain(static_cast<float>(i) / static_cast<float>(fadeSamples), curveType);
        }
    }

    void applyFadeOut(juce::AudioBuffer<float>& buffer, double sampleRate, float durationMs, int curveType)
    {
        const int fadeSamples = fadeLengthSamples(buffer, sampleRate, durationMs);
        const int startSample = buffer.getNumSamples() - fadeSamples;
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            for (int i = 0; i < fadeSamples; ++i)
                data[startSample + i] *= fadeOutGain(static_cast<float>(i) / static_cast<float>(fadeSamples),
                                                     curveType);
        }
    }

    void applyEQPreset(juce::AudioBuffer<float>& buffer, double sampleRate, const juce::String& presetName)
    {
        if (presetName.isEmpty())
            return;

        // Try the user preset first, then the factory preset of that name.
        DynamicParametricEQ::Parameters params;
        if (!EQPresetManager::loadPreset(params, presetName))
        {
            if (EQPresetManager::isFactoryPreset(presetName))
            {
                params = EQPresetManager::getFactoryPreset(presetName);
            }
            else
            {
                DBG("BatchDSPOps: Failed to load EQ preset: " + presetName);
                return;
            }
        }

        if (params.bands.empty() && juce::exactlyEqual(params.outputGain, 0.0f))
            return;

        DynamicParametricEQ eq;
        eq.prepare(sampleRate, buffer.getNumSamples());
        eq.setParameters(params);
        eq.applyEQ(buffer);
    }
}

void apply(juce::AudioBuffer<float>& buffer, double sampleRate, const BatchDSPSettings& op)
{
    if (!op.enabled)
        return;

    switch (op.operation)
    {
        case BatchDSPOperation::GAIN:         applyGain(buffer, op.gainDb); break;
        case BatchDSPOperation::NORMALIZE:    applyNormalize(buffer, op.normalizeTargetDb); break;
        case BatchDSPOperation::DC_OFFSET:    applyDCOffset(buffer); break;
        case BatchDSPOperation::FADE_IN:      applyFadeIn(buffer, sampleRate, op.fadeDurationMs, op.fadeType); break;
        case BatchDSPOperation::FADE_OUT:     applyFadeOut(buffer, sampleRate, op.fadeDurationMs, op.fadeType); break;
        case BatchDSPOperation::GRAPHICAL_EQ: applyEQPreset(buffer, sampleRate, op.eqPresetName); break;
        case BatchDSPOperation::REVERSE:      AudioProcessor::reverse(buffer); break;
        case BatchDSPOperation::INVERT:       AudioProcessor::invert(buffer); break;
        case BatchDSPOperation::PARAMETRIC_EQ:  // removed operation, skipped (see BatchJob)
        case BatchDSPOperation::NONE:
        default:
            break;
    }
}

void applyChain(juce::AudioBuffer<float>& buffer,
                double sampleRate,
                const std::vector<BatchDSPSettings>& chain)
{
    for (const auto& op : chain)
        apply(buffer, sampleRate, op);
}

bool needsWholeFile(const std::vector<BatchDSPSettings>& chain, double previewSeconds)
{
    for (const auto& op : chain)
    {
        if (!op.enabled)
            continue;

        switch (op.operation)
        {
            case BatchDSPOperation::NORMALIZE:
            case BatchDSPOperation::DC_OFFSET:
            case BatchDSPOperation::FADE_OUT:
            case BatchDSPOperation::REVERSE:
                return true;
            case BatchDSPOperation::FADE_IN:
                if (op.fadeDurationMs / 1000.0 >= previewSeconds)
                    return true;
                break;
            case BatchDSPOperation::NONE:
            case BatchDSPOperation::GAIN:
            case BatchDSPOperation::PARAMETRIC_EQ:
            case BatchDSPOperation::GRAPHICAL_EQ:   // causal IIR: the start is unchanged
            case BatchDSPOperation::INVERT:
            default:
                break;
        }
    }
    return false;
}

bool renderPreview(const juce::File& file,
                   const std::vector<BatchDSPSettings>& chain,
                   juce::AudioBuffer<float>& out,
                   double& sampleRate)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr || reader->sampleRate <= 0.0)
        return false;

    sampleRate = reader->sampleRate;
    juce::int64 numSamples = reader->lengthInSamples;

    // An excerpt is only used when it is sample-identical to the start of
    // the processed file; otherwise process the whole file, as batch does.
    if (!needsWholeFile(chain, kPreviewExcerptSeconds))
        numSamples = juce::jmin(numSamples, static_cast<juce::int64>(kPreviewExcerptSeconds * sampleRate));

    if (numSamples <= 0 || numSamples > static_cast<juce::int64>(std::numeric_limits<int>::max()))
        return false;

    out.setSize(static_cast<int>(reader->numChannels), static_cast<int>(numSamples));
    if (!reader->read(&out, 0, static_cast<int>(numSamples), 0, true, true))
        return false;

    applyChain(out, sampleRate, chain);
    return true;
}
}
} // namespace waveedit
