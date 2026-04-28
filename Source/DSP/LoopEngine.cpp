/*
  ==============================================================================

    LoopEngine.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Core loop creation engine implementation.

  ==============================================================================
*/

#include "LoopEngine.h"
#include <cmath>
#include <limits>

//==============================================================================
LoopResult LoopEngine::createLoop(
    const juce::AudioBuffer<float>& sourceBuffer,
    double sampleRate,
    int64_t startSample,
    int64_t endSample,
    const LoopRecipe& recipe)
{
    LoopResult result;

    // ------------------------------------------------------------------
    // 1. Validate inputs
    // ------------------------------------------------------------------
    if (sourceBuffer.getNumSamples() == 0 || sourceBuffer.getNumChannels() == 0)
    {
        result.errorMessage = "Source buffer is empty";
        return result;
    }

    if (startSample < 0)
        startSample = 0;

    if (endSample > sourceBuffer.getNumSamples())
        endSample = sourceBuffer.getNumSamples();

    if (startSample >= endSample)
    {
        result.errorMessage = "Invalid selection: start >= end";
        return result;
    }

    int64_t selLen = endSample - startSample;

    if (selLen < 2)
    {
        result.errorMessage = "Selection too short (< 2 samples)";
        return result;
    }

    // ------------------------------------------------------------------
    // 2. Calculate crossfade length
    // ------------------------------------------------------------------
    int64_t xfadeLen;

    if (recipe.crossfadeMode == LoopRecipe::CrossfadeMode::Milliseconds)
        xfadeLen = msToSamples(recipe.crossfadeLengthMs, sampleRate);
    else
        xfadeLen = static_cast<int64_t>(selLen * recipe.crossfadePercent / 100.0f);

    // Apply safety cap
    xfadeLen = std::min(xfadeLen, msToSamples(recipe.maxCrossfadeMs, sampleRate));
    // Can't exceed half the selection
    xfadeLen = std::min(xfadeLen, selLen / 2);
    // Must be at least 1
    xfadeLen = std::max(xfadeLen, static_cast<int64_t>(1));

    // ------------------------------------------------------------------
    // 3. Refine boundaries to zero crossings
    // ------------------------------------------------------------------
    int64_t refinedStart = startSample;
    int64_t refinedEnd = endSample;
    bool zcFound = false;

    if (recipe.zeroCrossingEnabled)
    {
        int64_t newStart = refineToZeroCrossing(
            sourceBuffer, startSample, recipe.zeroCrossingSearchWindowSamples);
        int64_t newEnd = refineToZeroCrossing(
            sourceBuffer, endSample, recipe.zeroCrossingSearchWindowSamples);

        // Only use refined positions if they don't make selection too short
        if (newEnd - newStart > xfadeLen * 2)
        {
            zcFound = (newStart != startSample || newEnd != endSample);
            refinedStart = newStart;
            refinedEnd = newEnd;
        }
    }

    // Recalculate selection length after refinement
    selLen = refinedEnd - refinedStart;

    // Re-clamp crossfade to refined selection
    xfadeLen = std::min(xfadeLen, selLen / 2);
    xfadeLen = std::max(xfadeLen, static_cast<int64_t>(1));

    // ------------------------------------------------------------------
    // 4. Measure pre-crossfade discontinuity
    // ------------------------------------------------------------------
    float discBefore = 0.0f;
    for (int ch = 0; ch < sourceBuffer.getNumChannels(); ++ch)
    {
        float lastSample = sourceBuffer.getSample(ch, static_cast<int>(refinedEnd - 1));
        float firstSample = sourceBuffer.getSample(ch, static_cast<int>(refinedStart));
        discBefore = std::max(discBefore, std::abs(lastSample - firstSample));
    }

    // ------------------------------------------------------------------
    // 5. Extract regions and crossfade
    // ------------------------------------------------------------------
    int numChannels = sourceBuffer.getNumChannels();
    int64_t loopLength = selLen - xfadeLen;

    // Extract tail (last xfadeLen samples of selection)
    juce::AudioBuffer<float> tailRegion(numChannels, static_cast<int>(xfadeLen));
    for (int ch = 0; ch < numChannels; ++ch)
    {
        tailRegion.copyFrom(ch, 0, sourceBuffer, ch,
                            static_cast<int>(refinedEnd - xfadeLen),
                            static_cast<int>(xfadeLen));
    }

    // Extract head (first xfadeLen samples of selection)
    juce::AudioBuffer<float> headRegion(numChannels, static_cast<int>(xfadeLen));
    for (int ch = 0; ch < numChannels; ++ch)
    {
        headRegion.copyFrom(ch, 0, sourceBuffer, ch,
                            static_cast<int>(refinedStart),
                            static_cast<int>(xfadeLen));
    }

    // Crossfade tail into head
    juce::AudioBuffer<float> crossfaded(numChannels, static_cast<int>(xfadeLen));
    applyCrossfade(tailRegion, headRegion, crossfaded, recipe.crossfadeCurve);

    // Assemble: crossfaded region + middle
    juce::AudioBuffer<float> loopBuffer(numChannels, static_cast<int>(loopLength));

    // Copy crossfaded region at the start
    for (int ch = 0; ch < numChannels; ++ch)
    {
        loopBuffer.copyFrom(ch, 0, crossfaded, ch, 0,
                            static_cast<int>(xfadeLen));
    }

    // Copy middle section (everything after head region, before tail region)
    int64_t middleStart = refinedStart + xfadeLen;
    int64_t middleLength = loopLength - xfadeLen;
    if (middleLength > 0)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            loopBuffer.copyFrom(ch, static_cast<int>(xfadeLen),
                                sourceBuffer, ch,
                                static_cast<int>(middleStart),
                                static_cast<int>(middleLength));
        }
    }

    // ------------------------------------------------------------------
    // 6. Measure post-crossfade discontinuity
    // ------------------------------------------------------------------
    float discAfter = measureDiscontinuity(loopBuffer);

    // ------------------------------------------------------------------
    // 7. Fill result
    // ------------------------------------------------------------------
    result.success = true;
    result.loopBuffer = std::move(loopBuffer);
    result.loopStartSample = refinedStart;
    result.loopEndSample = refinedEnd;
    result.crossfadeLengthSamples = xfadeLen;
    result.zeroCrossingFound = zcFound;
    result.discontinuityBefore = discBefore;
    result.discontinuityAfter = discAfter;
    return result;
}

//==============================================================================
std::vector<LoopResult> LoopEngine::createVariations(
    const juce::AudioBuffer<float>& sourceBuffer,
    double sampleRate,
    int64_t startSample,
    int64_t endSample,
    const LoopRecipe& recipe)
{
    std::vector<LoopResult> results;
    int count = std::max(1, recipe.loopCount);
    int64_t offsetStep = msToSamples(recipe.offsetStepMs, sampleRate);
    int64_t selLen = endSample - startSample;

    // Generate offsets
    std::vector<int64_t> offsets(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
        offsets[static_cast<size_t>(i)] = static_cast<int64_t>(i) * offsetStep;

    // Shuffle if seed > 0
    if (recipe.shuffleSeed > 0 && count > 1)
    {
        juce::Random rng(recipe.shuffleSeed);
        for (int i = count - 1; i > 0; --i)
        {
            int j = rng.nextInt(i + 1);
            std::swap(offsets[static_cast<size_t>(i)],
                      offsets[static_cast<size_t>(j)]);
        }
    }

    for (int i = 0; i < count; ++i)
    {
        int64_t offset = offsets[static_cast<size_t>(i)];
        int64_t adjStart = startSample + offset;
        int64_t adjEnd = endSample + offset;

        // Clamp to buffer
        if (adjEnd > sourceBuffer.getNumSamples())
        {
            adjEnd = sourceBuffer.getNumSamples();
            adjStart = std::max(static_cast<int64_t>(0), adjEnd - selLen);
        }

        auto result = createLoop(sourceBuffer, sampleRate, adjStart, adjEnd, recipe);
        results.push_back(std::move(result));
    }

    return results;
}

//==============================================================================
float LoopEngine::measureDiscontinuity(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() < 2)
        return 0.0f;

    float maxDisc = 0.0f;
    int lastIdx = buffer.getNumSamples() - 1;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float lastSample = buffer.getSample(ch, lastIdx);
        float firstSample = buffer.getSample(ch, 0);
        maxDisc = std::max(maxDisc, std::abs(lastSample - firstSample));
    }

    return maxDisc;
}

//==============================================================================
std::pair<float, float> LoopEngine::getCrossfadeGains(
    float t, LoopRecipe::CrossfadeCurve curve)
{
    switch (curve)
    {
        case LoopRecipe::CrossfadeCurve::Linear:
            return { 1.0f - t, t };

        case LoopRecipe::CrossfadeCurve::EqualPower:
        {
            float fadeOut = std::cos(t * juce::MathConstants<float>::halfPi);
            float fadeIn  = std::sin(t * juce::MathConstants<float>::halfPi);
            return { fadeOut, fadeIn };
        }

        case LoopRecipe::CrossfadeCurve::SCurve:
        {
            // Hermite interpolation: smooth start and end
            float s = t * t * (3.0f - 2.0f * t);
            return { 1.0f - s, s };
        }

        default:
            return { 1.0f - t, t };
    }
}

//==============================================================================
void LoopEngine::applyCrossfade(
    const juce::AudioBuffer<float>& tailRegion,
    const juce::AudioBuffer<float>& headRegion,
    juce::AudioBuffer<float>& output,
    LoopRecipe::CrossfadeCurve curve)
{
    int numSamples = output.getNumSamples();
    int numChannels = output.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const float* tail = tailRegion.getReadPointer(ch);
        const float* head = headRegion.getReadPointer(ch);
        float* out = output.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            float t = (numSamples > 1)
                ? static_cast<float>(i) / static_cast<float>(numSamples - 1)
                : 0.5f;
            auto [fadeOut, fadeIn] = getCrossfadeGains(t, curve);
            out[i] = tail[i] * fadeOut + head[i] * fadeIn;
        }
    }
}

//==============================================================================
int64_t LoopEngine::refineToZeroCrossing(
    const juce::AudioBuffer<float>& buffer,
    int64_t targetSample,
    int searchRadius)
{
    int numChannels = buffer.getNumChannels();
    if (numChannels == 0)
        return targetSample;

    // Find the sample with the smallest absolute amplitude across all channels
    int64_t bestSample = targetSample;
    float bestScore = std::numeric_limits<float>::max();

    int64_t searchStart = std::max(static_cast<int64_t>(0),
                                    targetSample - searchRadius);
    int64_t searchEnd = std::min(static_cast<int64_t>(buffer.getNumSamples() - 1),
                                  targetSample + searchRadius);

    for (int64_t s = searchStart; s <= searchEnd; ++s)
    {
        // Score = max absolute amplitude across all channels (lower = better)
        float score = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            score = std::max(score,
                             std::abs(buffer.getSample(ch, static_cast<int>(s))));
        }

        if (score < bestScore)
        {
            bestScore = score;
            bestSample = s;
        }
    }

    return bestSample;
}

//==============================================================================
float LoopEngine::lerpSample(const juce::AudioBuffer<float>& buffer,
                              int channel, float position)
{
    int idx0 = static_cast<int>(std::floor(position));
    int idx1 = idx0 + 1;
    float frac = position - static_cast<float>(idx0);

    int numSamples = buffer.getNumSamples();
    if (idx0 < 0 || idx0 >= numSamples) return 0.0f;
    if (idx1 >= numSamples) return buffer.getSample(channel, idx0);

    return buffer.getSample(channel, idx0) * (1.0f - frac)
         + buffer.getSample(channel, idx1) * frac;
}

//==============================================================================
LoopResult LoopEngine::createShepardLoop(
    const juce::AudioBuffer<float>& sourceBuffer,
    double sampleRate,
    int64_t startSample,
    int64_t endSample,
    const LoopRecipe& recipe)
{
    LoopResult result;

    // Validate inputs
    if (sourceBuffer.getNumSamples() == 0 || startSample >= endSample)
    {
        result.errorMessage = "Invalid selection for Shepard loop";
        return result;
    }

    int64_t selLength = endSample - startSample;
    int numChannels = sourceBuffer.getNumChannels();
    float semitones = recipe.shepardSemitones;
    bool goingUp = (recipe.shepardDirection == LoopRecipe::ShepardDirection::Up);

    // Calculate constrained loop length
    // The pitch-shifted version must align: loop_len = sel_len / pitch_ratio
    float pitchRatio = std::pow(2.0f, semitones / 12.0f);
    int64_t loopLength = static_cast<int64_t>(static_cast<float>(selLength) / pitchRatio);

    if (loopLength < 100)
    {
        result.errorMessage = "Selection too short for Shepard tone with this pitch range";
        return result;
    }

    // Clamp to available source
    loopLength = std::min(loopLength, selLength);

    // Create output buffer
    juce::AudioBuffer<float> loopBuffer(numChannels, static_cast<int>(loopLength));
    loopBuffer.clear();

    // For each output sample, create two layers with opposing pitch ramps
    // and crossfade between them
    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int64_t i = 0; i < loopLength; ++i)
        {
            float progress = static_cast<float>(i) / static_cast<float>(loopLength);

            // Calculate current pitch ratios for both layers
            float layerAPitchRatio, layerBPitchRatio;

            if (goingUp)
            {
                // Layer A ramps up from 1.0 to pitchRatio
                if (recipe.shepardRampShape == LoopRecipe::ShepardRampShape::Linear)
                    layerAPitchRatio = 1.0f + progress * (pitchRatio - 1.0f);
                else
                    layerAPitchRatio = std::pow(pitchRatio, progress);

                // Layer B is offset by half cycle
                float bProgress = std::fmod(progress + 0.5f, 1.0f);
                if (recipe.shepardRampShape == LoopRecipe::ShepardRampShape::Linear)
                    layerBPitchRatio = 1.0f + bProgress * (pitchRatio - 1.0f);
                else
                    layerBPitchRatio = std::pow(pitchRatio, bProgress);
            }
            else
            {
                // Layer A ramps down
                float invRatio = 1.0f / pitchRatio;
                if (recipe.shepardRampShape == LoopRecipe::ShepardRampShape::Linear)
                    layerAPitchRatio = 1.0f + progress * (invRatio - 1.0f);
                else
                    layerAPitchRatio = std::pow(invRatio, progress);

                float bProgress = std::fmod(progress + 0.5f, 1.0f);
                if (recipe.shepardRampShape == LoopRecipe::ShepardRampShape::Linear)
                    layerBPitchRatio = 1.0f + bProgress * (invRatio - 1.0f);
                else
                    layerBPitchRatio = std::pow(invRatio, bProgress);
            }

            // Calculate source positions for each layer
            // Simple approach: sourcePos = startSample + i * currentPitchRatio
            float sourcePosA = static_cast<float>(startSample) + static_cast<float>(i) * layerAPitchRatio;
            float sourcePosB = static_cast<float>(startSample) + static_cast<float>(i) * layerBPitchRatio;

            // Clamp to selection range
            float maxPos = static_cast<float>(endSample - 1);
            sourcePosA = std::min(sourcePosA, maxPos);
            sourcePosB = std::min(sourcePosB, maxPos);

            // Read samples with linear interpolation
            float sampleA = lerpSample(sourceBuffer, ch, sourcePosA);
            float sampleB = lerpSample(sourceBuffer, ch, sourcePosB);

            // Crossfade between layers using equal-power curve
            // Layer A fades out, Layer B fades in, with smooth overlap
            float layerAGain = std::cos(progress * juce::MathConstants<float>::halfPi);
            float layerBGain = std::sin(progress * juce::MathConstants<float>::halfPi);

            loopBuffer.setSample(ch, static_cast<int>(i),
                                sampleA * layerAGain + sampleB * layerBGain);
        }
    }

    // Now apply the normal crossfade at the loop boundary point for seamless tiling
    // Use the regular crossfade approach from createLoop
    int64_t xfadeLen;
    if (recipe.crossfadeMode == LoopRecipe::CrossfadeMode::Milliseconds)
        xfadeLen = msToSamples(recipe.crossfadeLengthMs, sampleRate);
    else
        xfadeLen = static_cast<int64_t>(loopLength * recipe.crossfadePercent / 100.0f);

    xfadeLen = std::min(xfadeLen, msToSamples(recipe.maxCrossfadeMs, sampleRate));
    xfadeLen = std::min(xfadeLen, loopLength / 2);
    xfadeLen = std::max(xfadeLen, (int64_t)1);

    // Apply boundary crossfade to make it tile seamlessly
    // Blend the last xfadeLen samples with the first xfadeLen samples
    int64_t finalLength = loopLength - xfadeLen;

    if (finalLength > 0)
    {
        juce::AudioBuffer<float> finalBuffer(numChannels, static_cast<int>(finalLength));

        // Extract tail and head from the shepard buffer
        juce::AudioBuffer<float> tail(numChannels, static_cast<int>(xfadeLen));
        juce::AudioBuffer<float> head(numChannels, static_cast<int>(xfadeLen));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            tail.copyFrom(ch, 0, loopBuffer, ch, static_cast<int>(loopLength - xfadeLen), static_cast<int>(xfadeLen));
            head.copyFrom(ch, 0, loopBuffer, ch, 0, static_cast<int>(xfadeLen));
        }

        // Crossfade
        juce::AudioBuffer<float> crossfaded(numChannels, static_cast<int>(xfadeLen));
        applyCrossfade(tail, head, crossfaded, recipe.crossfadeCurve);

        // Assemble: crossfaded + middle
        for (int ch = 0; ch < numChannels; ++ch)
        {
            finalBuffer.copyFrom(ch, 0, crossfaded, ch, 0, static_cast<int>(xfadeLen));
            if (finalLength > xfadeLen)
            {
                finalBuffer.copyFrom(ch, static_cast<int>(xfadeLen), loopBuffer, ch,
                                    static_cast<int>(xfadeLen), static_cast<int>(finalLength - xfadeLen));
            }
        }

        loopBuffer = std::move(finalBuffer);
    }

    // Fill result
    result.success = true;
    result.loopBuffer = std::move(loopBuffer);
    result.loopStartSample = startSample;
    result.loopEndSample = endSample;
    result.crossfadeLengthSamples = xfadeLen;
    result.discontinuityAfter = measureDiscontinuity(result.loopBuffer);

    return result;
}

//==============================================================================
int64_t LoopEngine::msToSamples(float ms, double sampleRate)
{
    return static_cast<int64_t>(ms * sampleRate / 1000.0);
}
