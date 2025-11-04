#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

/**
 * BWFMetadata - Broadcast Wave Format metadata utility class
 *
 * Handles reading and writing BWF (Broadcast Wave Format) metadata chunks
 * to WAV files. BWF is the industry standard for professional audio metadata,
 * used by Pro Tools, Sound Forge, and other professional audio applications.
 *
 * Supported Chunks:
 * - bext: Broadcast Extension (description, originator, timestamp)
 * - Future: cue points, playlists for region support
 *
 * @see https://tech.ebu.ch/docs/tech/tech3285.pdf (BWF Specification)
 *
 * Copyright © 2025 ZQ SFX
 * Licensed under GPL v3
 */
class BWFMetadata
{
public:
    /**
     * Creates an empty BWF metadata object
     */
    BWFMetadata();

    /**
     * Loads BWF metadata from an audio file
     * @param file The audio file to read metadata from
     * @return true if metadata was successfully loaded
     */
    bool loadFromFile(const juce::File& file);

    /**
     * Converts this BWF metadata to JUCE StringPairArray format
     * for writing to WAV files
     * @return StringPairArray compatible with AudioFormatWriter
     */
    juce::StringPairArray toJUCEMetadata() const;

    /**
     * Loads BWF metadata from JUCE StringPairArray
     * (typically from AudioFormatReader::metadataValues)
     * @param metadata The metadata key-value pairs
     */
    void fromJUCEMetadata(const juce::StringPairArray& metadata);

    // Getters
    juce::String getDescription() const { return m_description; }
    juce::String getOriginator() const { return m_originator; }
    juce::String getOriginatorRef() const { return m_originatorRef; }
    juce::String getOriginationDate() const { return m_originationDate; }
    juce::String getOriginationTime() const { return m_originationTime; }
    int64_t getTimeReference() const { return m_timeReference; }
    juce::String getCodingHistory() const { return m_codingHistory; }

    // Setters
    void setDescription(const juce::String& desc) { m_description = desc; }
    void setOriginator(const juce::String& orig) { m_originator = orig; }
    void setOriginatorRef(const juce::String& ref) { m_originatorRef = ref; }
    void setOriginationDate(const juce::String& date) { m_originationDate = date; }
    void setOriginationTime(const juce::String& time) { m_originationTime = time; }
    void setTimeReference(int64_t timeRef) { m_timeReference = timeRef; }
    void setCodingHistory(const juce::String& history) { m_codingHistory = history; }

    /**
     * Sets origination date/time from juce::Time object
     * @param time The time to convert to BWF format
     */
    void setOriginationDateTime(const juce::Time& time);

    /**
     * Gets origination date/time as juce::Time object
     * @return Time object, or default Time() if not set
     */
    juce::Time getOriginationDateTime() const;

    /**
     * Checks if any BWF metadata is present
     * @return true if any metadata field has been set
     */
    bool hasMetadata() const;

    /**
     * Clears all metadata fields
     */
    void clear();

    /**
     * Creates default BWF metadata for WaveEdit files
     * @param description Optional file description
     * @return BWFMetadata with default ZQ SFX originator info
     */
    static BWFMetadata createDefault(const juce::String& description = "");

private:
    // BWF "bext" chunk fields (EBU Tech 3285)
    juce::String m_description;        // Max 256 chars - Free text description
    juce::String m_originator;         // Max 32 chars - Organization name
    juce::String m_originatorRef;      // Max 32 chars - Reference identifier
    juce::String m_originationDate;    // 10 chars - Format: yyyy-mm-dd
    juce::String m_originationTime;    // 8 chars - Format: hh:mm:ss
    int64_t m_timeReference;           // Sample offset from midnight
    juce::String m_codingHistory;      // Multi-line processing history

    /**
     * Validates and formats date string to BWF format (yyyy-mm-dd)
     * @param date The date string to validate
     * @return Formatted date or empty string if invalid
     */
    static juce::String formatDate(const juce::String& date);

    /**
     * Validates and formats time string to BWF format (hh:mm:ss)
     * @param time The time string to validate
     * @return Formatted time or empty string if invalid
     */
    static juce::String formatTime(const juce::String& time);
};
