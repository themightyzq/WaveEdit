/*
  ==============================================================================

    HeadTailEngine.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "HeadTailEngine.h"
#include "../Audio/AudioProcessor.h"
#include <cmath>
#include <algorithm>

//==============================================================================

HeadTailReport HeadTailEngine::process(const juce::AudioBuffer<float>& input,
                                        double sampleRate,
                                        const HeadTailRecipe& recipe,
                                        juce::AudioBuffer<float>& output)
{
    HeadTailReport report;
    report.originalLength = input.getNumSamples();
    report.originalDurationMs = (report.originalLength / sampleRate) * 1000.0;

    // Validate input
    if (input.getNumSamples() == 0 || input.getNumChannels() == 0)
    {
        report.success = false;
        report.errorMessage = "Input buffer is empty.";
        return report;
    }

    int numChannels = input.getNumChannels();
    int64_t totalSamples = input.getNumSamples();

    // Working boundaries: start inclusive, end exclusive
    int64_t keepStart = 0;
    int64_t keepEnd = totalSamples;

    //--------------------------------------------------------------------------
    // Step 1: Silence detection
    //--------------------------------------------------------------------------
    if (recipe.detectEnabled)
    {
        auto [detStart, detEnd] = findSilenceBoundaries(
            input, sampleRate, recipe.thresholdDB,
            recipe.detectionMode, recipe.holdTimeMs);

        if (detStart == -1 || detEnd == -1)
        {
            // All silent
            report.detectionFound = false;

            if (recipe.notFoundBehavior == HeadTailRecipe::NotFoundBehavior::Skip)
            {
                // No trimming from detection; keep full buffer
                report.detectedStartSample = 0;
                report.detectedEndSample = totalSamples;
            }
            else
            {
                // UseDefaults: fall through, boundaries stay at 0..totalSamples
                report.detectedStartSample = 0;
                report.detectedEndSample = totalSamples;
            }
        }
        else
        {
            report.detectionFound = true;
            report.detectedStartSample = detStart;
            report.detectedEndSample = detEnd;

            // Apply leading/trailing padding
            int64_t leadPad = msToSamples(recipe.leadingPadMs, sampleRate);
            int64_t trailPad = msToSamples(recipe.trailingPadMs, sampleRate);

            keepStart = std::max(static_cast<int64_t>(0), detStart - leadPad);
            keepEnd = std::min(totalSamples, detEnd + trailPad);

            // Enforce maxTrimMs limit (0 = no limit)
            if (recipe.maxTrimMs > 0.0f)
            {
                int64_t maxTrimSamples = msToSamples(recipe.maxTrimMs, sampleRate);
                if (keepStart > maxTrimSamples)
                    keepStart = maxTrimSamples;
                if ((totalSamples - keepEnd) > maxTrimSamples)
                    keepEnd = totalSamples - maxTrimSamples;
            }

            // Enforce minKeptDurationMs
            int64_t minKeptSamples = msToSamples(recipe.minKeptDurationMs, sampleRate);
            if ((keepEnd - keepStart) < minKeptSamples)
            {
                // Expand symmetrically to meet minimum
                int64_t deficit = minKeptSamples - (keepEnd - keepStart);
                int64_t expandEach = deficit / 2;
                keepStart = std::max(static_cast<int64_t>(0), keepStart - expandEach);
                keepEnd = std::min(totalSamples, keepEnd + (deficit - expandEach));
            }
        }
    }

    //--------------------------------------------------------------------------
    // Step 2: Fixed trims (applied on top of detection results)
    //--------------------------------------------------------------------------
    if (recipe.headTrimEnabled && recipe.headTrimMs > 0.0f)
    {
        int64_t headTrimSamples = msToSamples(recipe.headTrimMs, sampleRate);
        keepStart += headTrimSamples;
        report.samplesTrimmedFromHead += headTrimSamples;
    }

    if (recipe.tailTrimEnabled && recipe.tailTrimMs > 0.0f)
    {
        int64_t tailTrimSamples = msToSamples(recipe.tailTrimMs, sampleRate);
        keepEnd -= tailTrimSamples;
        report.samplesTrimmedFromTail += tailTrimSamples;
    }

    // Also count detection-based trimming in the report
    if (recipe.detectEnabled && report.detectionFound)
    {
        report.samplesTrimmedFromHead += (keepStart - (recipe.headTrimEnabled
            ? msToSamples(recipe.headTrimMs, sampleRate) : 0));
        // The actual detection trim was already captured via keepStart
    }

    // Clamp boundaries
    keepStart = std::max(static_cast<int64_t>(0), keepStart);
    keepEnd = std::min(totalSamples, keepEnd);

    // Ensure keepEnd > keepStart (graceful handling of over-trimming)
    if (keepEnd <= keepStart)
    {
        // Trimmed everything -- produce a tiny buffer (1 sample of silence)
        keepStart = 0;
        keepEnd = std::min(static_cast<int64_t>(1), totalSamples);
    }

    //--------------------------------------------------------------------------
    // Step 3: Create trimmed buffer
    //--------------------------------------------------------------------------
    int64_t trimmedLength = keepEnd - keepStart;
    juce::AudioBuffer<float> trimmedBuffer(numChannels,
                                            static_cast<int>(trimmedLength));

    for (int ch = 0; ch < numChannels; ++ch)
    {
        trimmedBuffer.copyFrom(ch, 0, input, ch,
                               static_cast<int>(keepStart),
                               static_cast<int>(trimmedLength));
    }

    //--------------------------------------------------------------------------
    // Step 4: Silence padding
    //--------------------------------------------------------------------------
    int64_t prependSamples = 0;
    int64_t appendSamples = 0;

    if (recipe.prependSilenceEnabled && recipe.prependSilenceMs > 0.0f)
    {
        prependSamples = msToSamples(recipe.prependSilenceMs, sampleRate);
        report.silencePrependedSamples = prependSamples;
    }

    if (recipe.appendSilenceEnabled && recipe.appendSilenceMs > 0.0f)
    {
        appendSamples = msToSamples(recipe.appendSilenceMs, sampleRate);
        report.silenceAppendedSamples = appendSamples;
    }

    if (prependSamples > 0 || appendSamples > 0)
    {
        int64_t paddedLength = prependSamples + trimmedLength + appendSamples;
        juce::AudioBuffer<float> paddedBuffer(numChannels,
                                               static_cast<int>(paddedLength));
        paddedBuffer.clear();

        // Copy trimmed audio after prepend silence
        for (int ch = 0; ch < numChannels; ++ch)
        {
            paddedBuffer.copyFrom(ch, static_cast<int>(prependSamples),
                                  trimmedBuffer, ch, 0,
                                  static_cast<int>(trimmedLength));
        }

        trimmedBuffer = std::move(paddedBuffer);
    }

    //--------------------------------------------------------------------------
    // Step 5: Fades
    //--------------------------------------------------------------------------
    if (recipe.fadeInEnabled && recipe.fadeInMs > 0.0f)
    {
        int64_t fadeInSamples = msToSamples(recipe.fadeInMs, sampleRate);
        // Clamp to buffer length
        fadeInSamples = std::min(fadeInSamples,
                                 static_cast<int64_t>(trimmedBuffer.getNumSamples()));

        if (fadeInSamples > 0)
        {
            // Create a sub-buffer view for fade-in and apply
            // AudioProcessor::fadeIn operates on the first N samples
            juce::AudioBuffer<float> fadeRegion(numChannels,
                                                 static_cast<int>(fadeInSamples));
            for (int ch = 0; ch < numChannels; ++ch)
            {
                fadeRegion.copyFrom(ch, 0, trimmedBuffer, ch, 0,
                                    static_cast<int>(fadeInSamples));
            }

            AudioProcessor::fadeIn(fadeRegion, static_cast<int>(fadeInSamples),
                                   recipe.fadeInCurve);

            // Copy back
            for (int ch = 0; ch < numChannels; ++ch)
            {
                trimmedBuffer.copyFrom(ch, 0, fadeRegion, ch, 0,
                                       static_cast<int>(fadeInSamples));
            }

            report.fadeInAppliedMs = static_cast<float>(
                fadeInSamples * 1000.0 / sampleRate);
        }
    }

    if (recipe.fadeOutEnabled && recipe.fadeOutMs > 0.0f)
    {
        int64_t fadeOutSamples = msToSamples(recipe.fadeOutMs, sampleRate);
        int totalLen = trimmedBuffer.getNumSamples();
        fadeOutSamples = std::min(fadeOutSamples,
                                  static_cast<int64_t>(totalLen));

        if (fadeOutSamples > 0)
        {
            int fadeStart = totalLen - static_cast<int>(fadeOutSamples);

            // Extract fade region
            juce::AudioBuffer<float> fadeRegion(numChannels,
                                                 static_cast<int>(fadeOutSamples));
            for (int ch = 0; ch < numChannels; ++ch)
            {
                fadeRegion.copyFrom(ch, 0, trimmedBuffer, ch, fadeStart,
                                    static_cast<int>(fadeOutSamples));
            }

            AudioProcessor::fadeOut(fadeRegion, static_cast<int>(fadeOutSamples),
                                    recipe.fadeOutCurve);

            // Copy back
            for (int ch = 0; ch < numChannels; ++ch)
            {
                trimmedBuffer.copyFrom(ch, fadeStart, fadeRegion, ch, 0,
                                       static_cast<int>(fadeOutSamples));
            }

            report.fadeOutAppliedMs = static_cast<float>(
                fadeOutSamples * 1000.0 / sampleRate);
        }
    }

    //--------------------------------------------------------------------------
    // Step 6: Set output and finalize report
    //--------------------------------------------------------------------------
    output = std::move(trimmedBuffer);

    report.finalLength = output.getNumSamples();
    report.finalDurationMs = (report.finalLength / sampleRate) * 1000.0;
    report.success = true;

    return report;
}

//==============================================================================

std::pair<int64_t, int64_t> HeadTailEngine::detectBoundaries(
    const juce::AudioBuffer<float>& buffer,
    double sampleRate,
    const HeadTailRecipe& recipe)
{
    return findSilenceBoundaries(buffer, sampleRate, recipe.thresholdDB,
                                 recipe.detectionMode, recipe.holdTimeMs);
}

//==============================================================================

std::pair<int64_t, int64_t> HeadTailEngine::findSilenceBoundaries(
    const juce::AudioBuffer<float>& buffer,
    double sampleRate,
    float thresholdDB,
    HeadTailRecipe::DetectionMode mode,
    float holdTimeMs)
{
    int numChannels = buffer.getNumChannels();
    int64_t totalSamples = buffer.getNumSamples();

    if (totalSamples == 0 || numChannels == 0)
        return { -1, -1 };

    float threshold = std::pow(10.0f, thresholdDB / 20.0f);
    int64_t holdSamples = msToSamples(holdTimeMs, sampleRate);

    // Ensure hold is at least 1 sample
    holdSamples = std::max(holdSamples, static_cast<int64_t>(1));

    int64_t firstNonSilent = -1;
    int64_t lastNonSilent = -1;

    if (mode == HeadTailRecipe::DetectionMode::Peak)
    {
        //----------------------------------------------------------------------
        // Peak mode: check absolute sample values
        //----------------------------------------------------------------------

        // Scan forward for first sustained non-silent region
        int64_t consecutiveAbove = 0;
        for (int64_t s = 0; s < totalSamples; ++s)
        {
            bool aboveThreshold = false;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                if (std::abs(buffer.getSample(ch, static_cast<int>(s))) >= threshold)
                {
                    aboveThreshold = true;
                    break;
                }
            }

            if (aboveThreshold)
            {
                consecutiveAbove++;
                if (consecutiveAbove >= holdSamples)
                {
                    // First non-silent sample is where the sustained run started
                    firstNonSilent = s - holdSamples + 1;
                    break;
                }
            }
            else
            {
                consecutiveAbove = 0;
            }
        }

        // Scan backward for last sustained non-silent region
        consecutiveAbove = 0;
        for (int64_t s = totalSamples - 1; s >= 0; --s)
        {
            bool aboveThreshold = false;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                if (std::abs(buffer.getSample(ch, static_cast<int>(s))) >= threshold)
                {
                    aboveThreshold = true;
                    break;
                }
            }

            if (aboveThreshold)
            {
                consecutiveAbove++;
                if (consecutiveAbove >= holdSamples)
                {
                    // Last non-silent sample is where the sustained run started (from end)
                    lastNonSilent = s + holdSamples;  // exclusive end
                    break;
                }
            }
            else
            {
                consecutiveAbove = 0;
            }
        }
    }
    else
    {
        //----------------------------------------------------------------------
        // RMS mode: compute RMS over 10ms windows
        //----------------------------------------------------------------------
        int64_t windowSize = msToSamples(10.0f, sampleRate);
        windowSize = std::max(windowSize, static_cast<int64_t>(1));

        float thresholdLinear = threshold;  // Already linear from dB conversion above

        // Scan forward
        int64_t consecutiveAbove = 0;
        for (int64_t s = 0; s < totalSamples; s += windowSize)
        {
            int64_t windowEnd = std::min(s + windowSize, totalSamples);
            int64_t actualWindow = windowEnd - s;

            // Compute RMS across all channels for this window
            float sumSquares = 0.0f;
            int64_t sampleCount = 0;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                for (int64_t ws = s; ws < windowEnd; ++ws)
                {
                    float sample = buffer.getSample(ch, static_cast<int>(ws));
                    sumSquares += sample * sample;
                    sampleCount++;
                }
            }

            float rms = (sampleCount > 0)
                ? std::sqrt(sumSquares / static_cast<float>(sampleCount))
                : 0.0f;

            if (rms >= thresholdLinear)
            {
                consecutiveAbove += actualWindow;
                int64_t holdSamplesRms = msToSamples(holdTimeMs, sampleRate);
                holdSamplesRms = std::max(holdSamplesRms, static_cast<int64_t>(1));
                if (consecutiveAbove >= holdSamplesRms)
                {
                    firstNonSilent = s - (consecutiveAbove - actualWindow);
                    firstNonSilent = std::max(firstNonSilent, static_cast<int64_t>(0));
                    break;
                }
            }
            else
            {
                consecutiveAbove = 0;
            }
        }

        // Scan backward
        consecutiveAbove = 0;
        for (int64_t s = totalSamples; s > 0; s -= windowSize)
        {
            int64_t windowStart = std::max(s - windowSize, static_cast<int64_t>(0));
            int64_t actualWindow = s - windowStart;

            float sumSquares = 0.0f;
            int64_t sampleCount = 0;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                for (int64_t ws = windowStart; ws < s; ++ws)
                {
                    float sample = buffer.getSample(ch, static_cast<int>(ws));
                    sumSquares += sample * sample;
                    sampleCount++;
                }
            }

            float rms = (sampleCount > 0)
                ? std::sqrt(sumSquares / static_cast<float>(sampleCount))
                : 0.0f;

            if (rms >= thresholdLinear)
            {
                consecutiveAbove += actualWindow;
                int64_t holdSamplesRms = msToSamples(holdTimeMs, sampleRate);
                holdSamplesRms = std::max(holdSamplesRms, static_cast<int64_t>(1));
                if (consecutiveAbove >= holdSamplesRms)
                {
                    lastNonSilent = s + (consecutiveAbove - actualWindow);
                    lastNonSilent = std::min(lastNonSilent, totalSamples);
                    break;
                }
            }
            else
            {
                consecutiveAbove = 0;
            }
        }
    }

    return { firstNonSilent, lastNonSilent };
}
