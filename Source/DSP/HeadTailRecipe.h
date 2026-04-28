/*
  ==============================================================================

    HeadTailRecipe.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include "../Audio/AudioProcessor.h"  // For FadeCurveType

struct HeadTailRecipe
{
    // Detection
    bool detectEnabled = false;
    enum class DetectionMode { Peak, RMS };
    DetectionMode detectionMode = DetectionMode::Peak;
    float thresholdDB = -40.0f;
    float holdTimeMs = 10.0f;
    float leadingPadMs = 0.0f;
    float trailingPadMs = 0.0f;
    float minKeptDurationMs = 100.0f;
    float maxTrimMs = 0.0f;  // 0 = no limit
    enum class NotFoundBehavior { Skip, UseDefaults };
    NotFoundBehavior notFoundBehavior = NotFoundBehavior::Skip;

    // Fixed trims
    bool headTrimEnabled = false;
    float headTrimMs = 0.0f;
    bool tailTrimEnabled = false;
    float tailTrimMs = 0.0f;

    // Silence padding
    bool prependSilenceEnabled = false;
    float prependSilenceMs = 0.0f;
    bool appendSilenceEnabled = false;
    float appendSilenceMs = 0.0f;

    // Fades
    bool fadeInEnabled = false;
    float fadeInMs = 0.0f;
    FadeCurveType fadeInCurve = FadeCurveType::LINEAR;
    bool fadeOutEnabled = false;
    float fadeOutMs = 0.0f;
    FadeCurveType fadeOutCurve = FadeCurveType::LINEAR;
};

struct HeadTailReport
{
    bool success = false;
    juce::String errorMessage;
    int64_t detectedStartSample = 0;
    int64_t detectedEndSample = 0;
    bool detectionFound = true;
    int64_t samplesTrimmedFromHead = 0;
    int64_t samplesTrimmedFromTail = 0;
    int64_t silencePrependedSamples = 0;
    int64_t silenceAppendedSamples = 0;
    float fadeInAppliedMs = 0.0f;
    float fadeOutAppliedMs = 0.0f;
    int64_t originalLength = 0;
    int64_t finalLength = 0;
    double originalDurationMs = 0.0;
    double finalDurationMs = 0.0;
};
