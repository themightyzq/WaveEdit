/*
 * WaveEdit by ZQ SFX
 * Copyright (C) 2025 ZQ SFX
 * Licensed under GPL v3
 *
 * Marker.h
 *
 * Single-point markers for audio timeline
 * Unlike regions (which span a range), markers are reference points at specific sample positions
 */

#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

/**
 * Single-point timeline marker
 *
 * Markers are used for:
 * - Metadata points (e.g., "Chorus starts here")
 * - Reference points during editing
 * - Cue points for playback
 * - Loop/sync points
 */
class Marker
{
public:
    /**
     * Default constructor - creates marker at sample 0
     */
    Marker();

    /**
     * Constructor with all parameters
     *
     * @param name User-visible marker name
     * @param position Sample position (0-based, sample-accurate)
     * @param color Visual marker color (default: yellow)
     */
    Marker(const juce::String& name, int64_t position, juce::Colour color = juce::Colours::yellow);

    // Accessors
    juce::String getName() const { return m_name; }
    int64_t getPosition() const { return m_position; }
    juce::Colour getColor() const { return m_color; }

    // Mutators
    void setName(const juce::String& name) { m_name = name; }
    void setPosition(int64_t position);
    void setColor(juce::Colour color) { m_color = color; }

    /**
     * Get marker position in seconds
     *
     * @param sampleRate Audio sample rate
     * @return Position in seconds
     */
    double getPositionInSeconds(double sampleRate) const;

    /**
     * Check if marker is at or near a sample position
     *
     * @param sample Sample position to check
     * @param tolerance Tolerance in samples (default: 10 samples ~= 0.2ms @ 44.1kHz)
     * @return true if marker is within tolerance of sample
     */
    bool isNear(int64_t sample, int64_t tolerance = 10) const;

    /**
     * Serialize marker to JSON
     *
     * @return JSON object with name, position, color
     */
    juce::var toJSON() const;

    /**
     * Deserialize marker from JSON
     *
     * @param json JSON object from toJSON()
     * @return Marker instance, or default marker if parsing fails
     */
    static Marker fromJSON(const juce::var& json);

    // Comparison operators (for sorting by position)
    bool operator<(const Marker& other) const { return m_position < other.m_position; }
    bool operator==(const Marker& other) const;
    bool operator!=(const Marker& other) const { return !(*this == other); }

private:
    juce::String m_name;        // User-editable name
    int64_t m_position;         // Sample position (sample-accurate)
    juce::Colour m_color;       // Visual distinction color
};
