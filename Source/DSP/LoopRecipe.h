/*
  ==============================================================================

    LoopRecipe.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Settings and result types for the Loop Engine.
    Pure data structures -- no logic, no UI, no undo.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

struct LoopRecipe
{
    // Crossfade settings
    enum class CrossfadeMode { Milliseconds, Percentage };
    CrossfadeMode crossfadeMode = CrossfadeMode::Milliseconds;
    float crossfadeLengthMs = 50.0f;
    float crossfadePercent = 10.0f;
    float maxCrossfadeMs = 500.0f;

    enum class CrossfadeCurve { Linear, EqualPower, SCurve };
    CrossfadeCurve crossfadeCurve = CrossfadeCurve::EqualPower;

    // Zero-crossing settings
    bool zeroCrossingEnabled = true;
    int zeroCrossingSearchWindowSamples = 1000;
    bool fallbackToMinAmplitude = true;

    // Multi-variation output
    int loopCount = 1;
    float offsetStepMs = 0.0f;
    int shuffleSeed = 0;

    // Shepard Tone Mode
    bool shepardEnabled = false;
    float shepardSemitones = 1.0f;
    enum class ShepardDirection { Up, Down };
    ShepardDirection shepardDirection = ShepardDirection::Up;
    enum class ShepardRampShape { Linear, Exponential };
    ShepardRampShape shepardRampShape = ShepardRampShape::Linear;
};

struct LoopResult
{
    bool success = false;
    juce::String errorMessage;
    juce::AudioBuffer<float> loopBuffer;
    int64_t loopStartSample = 0;
    int64_t loopEndSample = 0;
    int64_t crossfadeLengthSamples = 0;
    bool zeroCrossingFound = false;
    float discontinuityBefore = 0.0f;
    float discontinuityAfter = 0.0f;
};
