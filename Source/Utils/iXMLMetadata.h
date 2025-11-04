/*
  ==============================================================================

    iXMLMetadata.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

/**
 * iXMLMetadata - iXML metadata utility class for sound design
 *
 * Handles reading and writing iXML metadata chunks to WAV files.
 * iXML is an open standard used by professional audio applications for
 * location sound metadata, including UCS (Universal Category System) fields.
 *
 * Supported Fields:
 * - CATEGORY: UCS Category (ALL CAPS, e.g., "DOOR")
 * - SUBCATEGORY: UCS Subcategory (Title Case, e.g., "Wood")
 * - TRACK_TITLE: FX Name/Description
 * - PROJECT: Source ID / Project Name
 * - TAPE: Library / Manufacturer Name
 * - SCENE: Scene number (optional)
 * - TAKE: Take number (optional)
 * - NOTES: Additional notes (optional)
 *
 * Compatible with:
 * - SoundMiner (reads Category, Subcategory, Track Title)
 * - Steinberg Nuendo/WaveLab
 * - iZotope RX
 * - BaseHead
 * - Pro Tools (partial support)
 *
 * @see http://www.gallery.co.uk/ixml/ (iXML Specification)
 *
 * Copyright © 2025 ZQ SFX
 * Licensed under GPL v3
 */
class iXMLMetadata
{
public:
    /**
     * Creates an empty iXML metadata object
     */
    iXMLMetadata();

    /**
     * Loads iXML metadata from an audio file
     * @param file The audio file to read metadata from
     * @return true if metadata was successfully loaded
     */
    bool loadFromFile(const juce::File& file);

    /**
     * Converts this iXML metadata to XML string for embedding in WAV files
     * @return XML string in iXML format
     */
    juce::String toXMLString() const;

    /**
     * Loads iXML metadata from XML string
     * @param xmlString The iXML XML data
     * @return true if parsing was successful
     */
    bool fromXMLString(const juce::String& xmlString);

    /**
     * Loads iXML metadata from JUCE StringPairArray
     * (for compatibility with AudioFormatReader::metadataValues)
     * @param metadata The metadata key-value pairs
     */
    void fromJUCEMetadata(const juce::StringPairArray& metadata);

    // UCS / Sound Design Getters
    juce::String getCategory() const { return m_category; }
    juce::String getSubcategory() const { return m_subcategory; }
    juce::String getTrackTitle() const { return m_trackTitle; }
    juce::String getProject() const { return m_project; }
    juce::String getTape() const { return m_tape; }

    // SoundMiner Extended Getters
    juce::String getFXName() const { return m_fxName; }
    juce::String getDescription() const { return m_description; }
    juce::String getKeywords() const { return m_keywords; }
    juce::String getDesigner() const { return m_designer; }
    juce::String getCategoryFull() const;  // Computed: Category-Subcategory

    // Production Getters
    juce::String getScene() const { return m_scene; }
    juce::String getTake() const { return m_take; }
    juce::String getNotes() const { return m_notes; }

    // UCS / Sound Design Setters
    void setCategory(const juce::String& cat) { m_category = cat; }
    void setSubcategory(const juce::String& subcat) { m_subcategory = subcat; }
    void setTrackTitle(const juce::String& title) { m_trackTitle = title; }
    void setProject(const juce::String& proj) { m_project = proj; }
    void setTape(const juce::String& tape) { m_tape = tape; }

    // SoundMiner Extended Setters
    void setFXName(const juce::String& fxName) { m_fxName = fxName; }
    void setDescription(const juce::String& desc) { m_description = desc; }
    void setKeywords(const juce::String& keywords) { m_keywords = keywords; }
    void setDesigner(const juce::String& designer) { m_designer = designer; }

    // Production Setters
    void setScene(const juce::String& scene) { m_scene = scene; }
    void setTake(const juce::String& take) { m_take = take; }
    void setNotes(const juce::String& notes) { m_notes = notes; }

    /**
     * Checks if any iXML metadata is present
     * @return true if any metadata field has been set
     */
    bool hasMetadata() const;

    /**
     * Clears all metadata fields
     */
    void clear();

    /**
     * Creates default iXML metadata for WaveEdit files
     * @param category UCS Category (e.g., "DOOR")
     * @param subcategory UCS Subcategory (e.g., "Wood")
     * @return iXMLMetadata with default values
     */
    static iXMLMetadata createDefault(const juce::String& category = "",
                                       const juce::String& subcategory = "");

    /**
     * Parses UCS filename to extract metadata fields
     * Format: CatID_FXName_CreatorID_SourceID.wav
     * Example: DOORWood_Front Door Open_ZQX_Cabin.wav
     *
     * @param filename The UCS-formatted filename
     * @return iXMLMetadata populated from filename, or empty if parsing fails
     */
    static iXMLMetadata fromUCSFilename(const juce::String& filename);

    /**
     * Generates UCS-formatted filename from metadata
     * Format: CatID_FXName_CreatorID_SourceID.wav
     *
     * @param creatorID Creator/Recordist ID (e.g., "ZQX")
     * @return UCS-formatted filename (without extension)
     */
    juce::String toUCSFilename(const juce::String& creatorID = "ZQX") const;

private:
    // UCS / Sound Design Fields (Primary)
    juce::String m_category;      // UCS Category (ALL CAPS, e.g., "DOOR")
    juce::String m_subcategory;   // UCS Subcategory (Title Case, e.g., "Wood")
    juce::String m_trackTitle;    // Track title (user-editable, can differ from FXName)
    juce::String m_project;       // Source ID / Project Name
    juce::String m_tape;          // Library / Manufacturer

    // SoundMiner Extended Fields
    juce::String m_fxName;        // FX Name (from UCS filename, e.g., "Front Door Open")
    juce::String m_description;   // Long descriptive text with details
    juce::String m_keywords;      // Searchable keywords (comma-separated)
    juce::String m_designer;      // Creator/Recordist/Designer (e.g., "ZQ")

    // Production Fields (Optional)
    juce::String m_scene;         // Scene number
    juce::String m_take;          // Take number
    juce::String m_notes;         // Additional notes

    /**
     * Converts Category+Subcategory to CatID format
     * Example: "DOOR" + "Wood" → "DOORWood"
     */
    juce::String getCatID() const;

    /**
     * Parses CatID to extract Category and Subcategory
     * Example: "DOORWood" → "DOOR" + "Wood"
     */
    static void parseCatID(const juce::String& catID,
                           juce::String& outCategory,
                           juce::String& outSubcategory);
};
