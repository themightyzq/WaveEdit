/*
  ==============================================================================

    TimePitchEngine.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SoundTouch-backed time-stretch and pitch-shift DSP. The engine
    processes a JUCE AudioBuffer<float> offline (i.e. not on the audio
    thread) and returns a new buffer with the requested length / pitch.

    This is a thin C++ wrapper over the SoundTouch library; the
    interesting algorithms live in libSoundTouch (LGPL-2.1). Bundling
    the dynamic library into release builds and updating NOTICE is
    handled at the build-system level.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>

namespace TimePitchEngine
{
    struct Recipe
    {
        /** Tempo change in percent. 0 = no change. -50 = half speed, +100 = 2x speed.
            Range: -95 .. +5000 (SoundTouch handles wide factors; quality degrades at extremes). */
        double tempoPercent = 0.0;

        /** Pitch shift in semitones (with fractional support).
            Range: -60 .. +60 (5 octaves). */
        double pitchSemitones = 0.0;
    };

    /** Returns true if the recipe is a no-op (no audible change). */
    inline bool isIdentity(const Recipe& r)
    {
        return std::abs(r.tempoPercent)    < 1.0e-6
            && std::abs(r.pitchSemitones) < 1.0e-6;
    }

    /**
     * Process @p input through SoundTouch with the given @p recipe and
     * return the resulting buffer. Channel count is preserved; sample
     * count scales with tempo (slower → more samples).
     *
     * @param input          Source audio (planar, JUCE convention).
     * @param sampleRate     Sample rate in Hz.
     * @param recipe         Tempo + pitch parameters.
     * @param onProgress     Optional progress callback invoked from the chunked
     *                       processing loop with a 0..1 fraction. Return false to
     *                       cancel: the engine stops and returns an empty buffer.
     *                       May be null/empty (no reporting, never cancelled).
     * @return Processed audio buffer. Empty buffer on invalid input or cancel.
     */
    juce::AudioBuffer<float> apply(const juce::AudioBuffer<float>& input,
                                    double sampleRate,
                                    const Recipe& recipe,
                                    std::function<bool(float)> onProgress = {});
}
