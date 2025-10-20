/*
  ==============================================================================

    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

/**
 * Represents a named audio region with start/end sample positions.
 *
 * Regions are used to mark and organize sections of audio (e.g., podcast segments,
 * sound effect takes, music sections). Each region has:
 * - Name (user-editable)
 * - Start/end sample positions (sample-accurate)
 * - Color (for visual distinction)
 *
 * Regions can be:
 * - Created from selections (Cmd+M)
 * - Clicked to select their audio range
 * - Exported individually (batch export)
 * - Used for "select inverse" workflow (select everything NOT in regions)
 * - Persisted to JSON sidecar files
 */
class Region
{
public:
    /**
     * Creates a new region with a name and sample range.
     *
     * @param name Region name (e.g., "Intro", "Verse 1", "Take 3")
     * @param startSample Starting sample position (inclusive)
     * @param endSample Ending sample position (inclusive)
     */
    Region(const juce::String& name, int64_t startSample, int64_t endSample);

    /**
     * Default constructor for JSON deserialization.
     */
    Region();

    /**
     * Gets the region name.
     */
    juce::String getName() const;

    /**
     * Sets the region name.
     */
    void setName(const juce::String& name);

    /**
     * Gets the starting sample position.
     */
    int64_t getStartSample() const;

    /**
     * Sets the starting sample position.
     */
    void setStartSample(int64_t sample);

    /**
     * Gets the ending sample position.
     */
    int64_t getEndSample() const;

    /**
     * Sets the ending sample position.
     */
    void setEndSample(int64_t sample);

    /**
     * Gets the length of the region in samples.
     */
    int64_t getLengthInSamples() const;

    /**
     * Gets the length of the region in seconds.
     *
     * @param sampleRate Sample rate for time conversion
     */
    double getLengthInSeconds(double sampleRate) const;

    /**
     * Gets the region color (for visual display).
     */
    juce::Colour getColor() const;

    /**
     * Sets the region color.
     */
    void setColor(const juce::Colour& color);

    /**
     * Serializes the region to JSON format.
     */
    juce::var toJSON() const;

    /**
     * Deserializes a region from JSON format.
     *
     * @param json JSON object containing region data
     * @return Deserialized Region object
     */
    static Region fromJSON(const juce::var& json);

    /**
     * Checks if a sample position is within this region.
     *
     * @param sample Sample position to check
     * @return true if sample is within [startSample, endSample]
     */
    bool containsSample(int64_t sample) const;

    /**
     * Checks if this region overlaps with another region.
     *
     * @param other Region to check for overlap
     * @return true if regions overlap
     */
    bool overlaps(const Region& other) const;

private:
    juce::String m_name;
    int64_t m_startSample;
    int64_t m_endSample;
    juce::Colour m_color;

    JUCE_LEAK_DETECTOR(Region)
};
