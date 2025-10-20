/*
  ==============================================================================

    AudioUnits.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Audio-unit conversion and snapping utilities.
    Provides sample-accurate navigation and selection at any zoom level.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>

namespace AudioUnits
{
    /**
     * Snap mode determines how selection/navigation positions are quantized.
     */
    enum class SnapMode
    {
        Off,            // No snapping - free selection
        Samples,        // Snap to sample boundaries
        Milliseconds,   // Snap to millisecond boundaries
        Seconds,        // Snap to second boundaries
        Frames,         // Snap to video frame boundaries
        Grid,           // Snap to grid lines (Phase 3)
        ZeroCrossing    // Snap to zero crossing points
    };

    /**
     * Unit type for navigation increments (two-tier snap system).
     */
    enum class UnitType
    {
        Samples,
        Milliseconds,
        Seconds,
        Frames,
        Custom  // User-defined sample count
    };

    //==============================================================================
    // Snap increment presets (two-tier system)

    /**
     * Preset increment values for Samples unit.
     * Index 0 = "None" (snap off), subsequent values are sample counts.
     */
    const std::vector<int> SAMPLES_INCREMENTS = { 0, 1, 100, 500, 1000, 10000 };

    /**
     * Preset increment values for Milliseconds unit.
     * Index 0 = "None" (snap off), subsequent values are milliseconds.
     */
    const std::vector<int> MILLISECONDS_INCREMENTS = { 0, 1, 10, 100, 500, 1000 };

    /**
     * Preset increment values for Seconds unit.
     * Index 0 = "None" (snap off), subsequent values are tenths of seconds.
     * (e.g., 1 = 0.1s, 10 = 1.0s, 50 = 5.0s, 100 = 10.0s)
     */
    const std::vector<int> SECONDS_INCREMENTS = { 0, 1, 10, 50, 100 };

    /**
     * Preset increment values for Frames unit.
     * Index 0 = "None" (snap off), subsequent values are frame counts.
     */
    const std::vector<int> FRAMES_INCREMENTS = { 0, 1, 5, 10 };

    /**
     * Gets the increment presets for a given unit type.
     *
     * @param unitType The unit type
     * @return Vector of increment values for that unit
     */
    inline const std::vector<int>& getIncrementsForUnit(UnitType unitType)
    {
        switch (unitType)
        {
            case UnitType::Samples:       return SAMPLES_INCREMENTS;
            case UnitType::Milliseconds:  return MILLISECONDS_INCREMENTS;
            case UnitType::Seconds:       return SECONDS_INCREMENTS;
            case UnitType::Frames:        return FRAMES_INCREMENTS;
            case UnitType::Custom:
                jassertfalse; // Custom units should have their own implementation
                return MILLISECONDS_INCREMENTS;
            default:
                jassertfalse; // Unknown unit type
                return MILLISECONDS_INCREMENTS;
        }
    }

    /**
     * Gets the default increment index for a given unit type.
     * Returns a reasonable starting value (typically 100ms equivalent).
     *
     * @param unitType The unit type
     * @return Default index into the increment array
     */
    inline size_t getDefaultIncrementIndex(UnitType unitType)
    {
        switch (unitType)
        {
            case UnitType::Samples:       return 3;  // 1000 samples
            case UnitType::Milliseconds:  return 3;  // 100ms
            case UnitType::Seconds:       return 2;  // 1.0s (10 tenths)
            case UnitType::Frames:        return 2;  // 10 frames
            case UnitType::Custom:        return 0;  // Off by default
            default:                      return 0;
        }
    }

    /**
     * Formats an increment value as a user-friendly string.
     *
     * @param increment The increment value
     * @param unitType The unit type
     * @return Formatted string (e.g., "100 ms", "1 s", "Off")
     */
    inline juce::String formatIncrement(int increment, UnitType unitType)
    {
        if (increment == 0)
            return "Off";

        switch (unitType)
        {
            case UnitType::Samples:
                return juce::String(increment) + " samples";

            case UnitType::Milliseconds:
                return juce::String(increment) + " ms";

            case UnitType::Seconds:
                // Increment is in tenths of seconds
                {
                    double seconds = increment / 10.0;
                    if (seconds < 1.0)
                        return juce::String(seconds, 1) + " s";
                    else
                        return juce::String((int)seconds) + " s";
                }

            case UnitType::Frames:
                return juce::String(increment) + " frames";

            case UnitType::Custom:
                return juce::String(increment);

            default:
                return juce::String(increment);
        }
    }

    //==============================================================================
    // Conversion functions

    /**
     * Converts milliseconds to samples.
     *
     * @param ms Time in milliseconds
     * @param sampleRate Sample rate in Hz
     * @return Sample count
     */
    inline int64_t millisecondsToSamples(double ms, double sampleRate)
    {
        return static_cast<int64_t>(std::round((ms / 1000.0) * sampleRate));
    }

    /**
     * Converts samples to milliseconds.
     *
     * @param samples Sample count
     * @param sampleRate Sample rate in Hz
     * @return Time in milliseconds
     */
    inline double samplesToMilliseconds(int64_t samples, double sampleRate)
    {
        return (static_cast<double>(samples) / sampleRate) * 1000.0;
    }

    /**
     * Converts seconds to samples.
     *
     * @param seconds Time in seconds
     * @param sampleRate Sample rate in Hz
     * @return Sample count
     */
    inline int64_t secondsToSamples(double seconds, double sampleRate)
    {
        return static_cast<int64_t>(std::round(seconds * sampleRate));
    }

    /**
     * Converts samples to seconds.
     *
     * @param samples Sample count
     * @param sampleRate Sample rate in Hz
     * @return Time in seconds
     */
    inline double samplesToSeconds(int64_t samples, double sampleRate)
    {
        return static_cast<double>(samples) / sampleRate;
    }

    /**
     * Converts video frames to samples.
     *
     * @param frame Frame number
     * @param fps Frames per second
     * @param sampleRate Sample rate in Hz
     * @return Sample count
     */
    inline int64_t framesToSamples(int frame, double fps, double sampleRate)
    {
        return static_cast<int64_t>(std::round((static_cast<double>(frame) / fps) * sampleRate));
    }

    /**
     * Converts samples to video frames.
     *
     * @param samples Sample count
     * @param fps Frames per second
     * @param sampleRate Sample rate in Hz
     * @return Frame number
     */
    inline int samplesToFrames(int64_t samples, double fps, double sampleRate)
    {
        return static_cast<int>(std::round((static_cast<double>(samples) / sampleRate) * fps));
    }

    //==============================================================================
    // Snapping functions

    /**
     * Snaps a sample position to the nearest unit boundary based on snap mode.
     *
     * @param sample Input sample position
     * @param mode Snap mode to use
     * @param increment Snap increment (meaning depends on mode)
     * @param sampleRate Sample rate in Hz
     * @param fps Frames per second (for frame-based snapping)
     * @return Snapped sample position
     */
    inline int64_t snapToUnit(int64_t sample,
                              SnapMode mode,
                              int increment,
                              double sampleRate,
                              double fps = 30.0)
    {
        if (mode == SnapMode::Off)
            return sample;

        int64_t snapInterval = 0;

        switch (mode)
        {
            case SnapMode::Samples:
                snapInterval = increment;
                break;

            case SnapMode::Milliseconds:
                snapInterval = millisecondsToSamples(static_cast<double>(increment), sampleRate);
                break;

            case SnapMode::Seconds:
                // Increment is in tenths of seconds (e.g., 1 = 0.1s, 10 = 1.0s)
                snapInterval = secondsToSamples(static_cast<double>(increment) / 10.0, sampleRate);
                break;

            case SnapMode::Frames:
                snapInterval = framesToSamples(increment, fps, sampleRate);
                break;

            case SnapMode::Grid:
                // Grid snapping handled by WaveformDisplay (Phase 3)
                return sample;

            case SnapMode::ZeroCrossing:
                // Zero-crossing snapping requires audio buffer (handled separately)
                return sample;

            default:
                return sample;
        }

        if (snapInterval <= 0)
            return sample;

        // Round to nearest interval
        int64_t remainder = sample % snapInterval;
        if (remainder < snapInterval / 2)
            return sample - remainder;
        else
            return sample + (snapInterval - remainder);
    }

    /**
     * Snaps a time position to the nearest unit boundary.
     *
     * @param time Input time in seconds
     * @param mode Snap mode to use
     * @param increment Snap increment
     * @param sampleRate Sample rate in Hz
     * @param fps Frames per second
     * @return Snapped time in seconds
     */
    inline double snapTimeToUnit(double time,
                                 SnapMode mode,
                                 int increment,
                                 double sampleRate,
                                 double fps = 30.0)
    {
        int64_t samples = secondsToSamples(time, sampleRate);
        int64_t snappedSamples = snapToUnit(samples, mode, increment, sampleRate, fps);
        return samplesToSeconds(snappedSamples, sampleRate);
    }

    /**
     * Finds the nearest zero crossing in an audio buffer.
     *
     * @param sample Target sample position
     * @param buffer Audio buffer to search
     * @param channel Channel to analyze
     * @param searchRadius Maximum search radius in samples
     * @return Sample position of nearest zero crossing
     */
    inline int64_t snapToZeroCrossing(int64_t sample,
                                      const juce::AudioBuffer<float>& buffer,
                                      int channel,
                                      int searchRadius = 1000)
    {
        if (channel < 0 || channel >= buffer.getNumChannels())
            return sample;

        int numSamples = buffer.getNumSamples();
        if (sample < 0 || sample >= numSamples)
            return juce::jlimit(0, numSamples - 1, static_cast<int>(sample));

        const float* channelData = buffer.getReadPointer(channel);
        int sampleInt = static_cast<int>(sample);

        // Start at target and search outward in both directions
        int bestSample = sampleInt;
        float bestDistance = std::abs(channelData[sampleInt]);

        for (int radius = 1; radius <= searchRadius; ++radius)
        {
            // Check left
            int leftSample = sampleInt - radius;
            if (leftSample >= 0)
            {
                float leftValue = std::abs(channelData[leftSample]);
                if (leftValue < bestDistance)
                {
                    bestDistance = leftValue;
                    bestSample = leftSample;
                }

                // Check for actual zero crossing (sign change)
                if (leftSample > 0)
                {
                    float prevValue = channelData[leftSample - 1];
                    float currValue = channelData[leftSample];
                    if ((prevValue > 0.0f && currValue <= 0.0f) ||
                        (prevValue < 0.0f && currValue >= 0.0f))
                    {
                        return leftSample;
                    }
                }
            }

            // Check right
            int rightSample = sampleInt + radius;
            if (rightSample < numSamples)
            {
                float rightValue = std::abs(channelData[rightSample]);
                if (rightValue < bestDistance)
                {
                    bestDistance = rightValue;
                    bestSample = rightSample;
                }

                // Check for actual zero crossing (sign change)
                if (rightSample > 0)
                {
                    float prevValue = channelData[rightSample - 1];
                    float currValue = channelData[rightSample];
                    if ((prevValue > 0.0f && currValue <= 0.0f) ||
                        (prevValue < 0.0f && currValue >= 0.0f))
                    {
                        return rightSample;
                    }
                }
            }
        }

        return bestSample;
    }

    /**
     * Converts a snap mode to a user-friendly string.
     */
    inline juce::String snapModeToString(SnapMode mode)
    {
        switch (mode)
        {
            case SnapMode::Off:           return "Off";
            case SnapMode::Samples:       return "Samples";
            case SnapMode::Milliseconds:  return "Milliseconds";
            case SnapMode::Seconds:       return "Seconds";
            case SnapMode::Frames:        return "Frames";
            case SnapMode::Grid:          return "Grid";
            case SnapMode::ZeroCrossing:  return "Zero Crossing";
            default:                      return "Unknown";
        }
    }

    /**
     * Converts a unit type to a user-friendly string.
     */
    inline juce::String unitTypeToString(UnitType type)
    {
        switch (type)
        {
            case UnitType::Samples:       return "samples";
            case UnitType::Milliseconds:  return "ms";
            case UnitType::Seconds:       return "s";
            case UnitType::Frames:        return "frames";
            case UnitType::Custom:        return "custom";
            default:                      return "unknown";
        }
    }

    //==============================================================================
    // Time Format Display (Phase 3.5 - Priority #5)

    /**
     * Time format for status bar and time displays.
     */
    enum class TimeFormat
    {
        Samples,        // Raw sample count (e.g., "44100")
        Milliseconds,   // Milliseconds (e.g., "1000.00 ms")
        Seconds,        // Seconds with decimals (e.g., "1.00 s")
        Frames          // Video frames (e.g., "30 frames @ 30 fps")
    };

    /**
     * Formats a time value (in seconds) according to the specified format.
     *
     * @param timeInSeconds Time value in seconds
     * @param sampleRate Sample rate in Hz (used for sample count calculation)
     * @param format The time format to use
     * @param fps Frames per second (used for SMPTE and Frames formats, default 30.0)
     * @return Formatted time string
     */
    inline juce::String formatTime(double timeInSeconds, double sampleRate, TimeFormat format, double fps = 30.0)
    {
        switch (format)
        {
            case TimeFormat::Samples:
            {
                int64_t samples = secondsToSamples(timeInSeconds, sampleRate);
                return juce::String(samples) + " smp";
            }

            case TimeFormat::Milliseconds:
            {
                double ms = timeInSeconds * 1000.0;
                return juce::String(ms, 2) + " ms";
            }

            case TimeFormat::Seconds:
            {
                return juce::String(timeInSeconds, 2) + " s";
            }

            case TimeFormat::Frames:
            {
                int totalFrames = static_cast<int>(timeInSeconds * fps);
                return juce::String(totalFrames) + " fr @ " + juce::String(fps, 2) + " fps";
            }

            default:
                return juce::String(timeInSeconds, 2) + " s";
        }
    }

    /**
     * Gets the next time format in the cycle (for format cycling).
     */
    inline TimeFormat getNextTimeFormat(TimeFormat current)
    {
        switch (current)
        {
            case TimeFormat::Samples:       return TimeFormat::Milliseconds;
            case TimeFormat::Milliseconds:  return TimeFormat::Seconds;
            case TimeFormat::Seconds:       return TimeFormat::Frames;
            case TimeFormat::Frames:        return TimeFormat::Samples;
            default:                        return TimeFormat::Seconds;
        }
    }

    /**
     * Converts time format enum to user-friendly string.
     */
    inline juce::String timeFormatToString(TimeFormat format)
    {
        switch (format)
        {
            case TimeFormat::Samples:       return "Samples";
            case TimeFormat::Milliseconds:  return "Milliseconds";
            case TimeFormat::Seconds:       return "Seconds";
            case TimeFormat::Frames:        return "Frames";
            default:                        return "Unknown";
        }
    }

} // namespace AudioUnits
