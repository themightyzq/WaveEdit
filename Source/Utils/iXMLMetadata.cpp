/*
  ==============================================================================

    iXMLMetadata.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "iXMLMetadata.h"
#include "../Audio/AudioFileManager.h"

iXMLMetadata::iXMLMetadata()
{
}

bool iXMLMetadata::loadFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    // CRITICAL: JUCE doesn't read custom WAV chunks like iXML
    // We must read it manually using our custom chunk reader
    AudioFileManager fileManager;
    juce::String ixmlData;

    if (fileManager.readiXMLChunk(file, ixmlData))
    {
        // Found and read iXML chunk successfully
        return fromXMLString(ixmlData);
    }

    // No iXML chunk found - try JUCE metadata as fallback
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader == nullptr)
        return false;

    // Try parsing from standard metadata keys (some apps use these)
    fromJUCEMetadata(reader->metadataValues);

    return hasMetadata();
}

juce::String iXMLMetadata::toXMLString() const
{
    // Generate iXML XML format
    // <?xml version="1.0" encoding="UTF-8"?>
    // <BWFXML>
    //   <IXML_VERSION>2.41</IXML_VERSION>
    //   <PROJECT>ProjectName</PROJECT>
    //   <SCENE>SceneNumber</SCENE>
    //   <TAKE>TakeNumber</TAKE>
    //   <TAPE>LibraryName</TAPE>
    //   <CIRCLED>FALSE</CIRCLED>
    //   <TRACK_LIST>
    //     <TRACK_COUNT>1</TRACK_COUNT>
    //     <TRACK>
    //       <CHANNEL_INDEX>1</CHANNEL_INDEX>
    //       <INTERLEAVE_INDEX>1</INTERLEAVE_INDEX>
    //       <NAME>TrackTitle</NAME>
    //       <FUNCTION>sfx</FUNCTION>
    //     </TRACK>
    //   </TRACK_LIST>
    //   <NOTE>Notes</NOTE>
    //   <USER>
    //     <CATEGORY>DOOR</CATEGORY>
    //     <SUBCATEGORY>Wood</SUBCATEGORY>
    //     <FXNAME>Front Door Open</FXNAME>
    //     <DESCRIPTION>Detailed description text</DESCRIPTION>
    //     <KEYWORDS>door, wood, open, creak</KEYWORDS>
    //     <DESIGNER>ZQ</DESIGNER>
    //   </USER>
    // </BWFXML>

    juce::String xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml << "<BWFXML>\n";
    xml << "  <IXML_VERSION>2.41</IXML_VERSION>\n";

    // Project/Scene/Take/Tape (Standard iXML fields)
    if (m_project.isNotEmpty())
        xml << "  <PROJECT>" << juce::XmlElement::createTextElement(m_project)->getText() << "</PROJECT>\n";

    if (m_scene.isNotEmpty())
        xml << "  <SCENE>" << juce::XmlElement::createTextElement(m_scene)->getText() << "</SCENE>\n";

    if (m_take.isNotEmpty())
        xml << "  <TAKE>" << juce::XmlElement::createTextElement(m_take)->getText() << "</TAKE>\n";

    if (m_tape.isNotEmpty())
        xml << "  <TAPE>" << juce::XmlElement::createTextElement(m_tape)->getText() << "</TAPE>\n";

    xml << "  <CIRCLED>FALSE</CIRCLED>\n";

    // Track list with Track Title
    if (m_trackTitle.isNotEmpty())
    {
        xml << "  <TRACK_LIST>\n";
        xml << "    <TRACK_COUNT>1</TRACK_COUNT>\n";
        xml << "    <TRACK>\n";
        xml << "      <CHANNEL_INDEX>1</CHANNEL_INDEX>\n";
        xml << "      <INTERLEAVE_INDEX>1</INTERLEAVE_INDEX>\n";
        xml << "      <NAME>" << juce::XmlElement::createTextElement(m_trackTitle)->getText() << "</NAME>\n";
        xml << "      <FUNCTION>sfx</FUNCTION>\n";
        xml << "    </TRACK>\n";
        xml << "  </TRACK_LIST>\n";
    }

    // Notes
    if (m_notes.isNotEmpty())
        xml << "  <NOTE>" << juce::XmlElement::createTextElement(m_notes)->getText() << "</NOTE>\n";

    // UCS Category/Subcategory + SoundMiner Extended (Steinberg USER extension)
    if (m_category.isNotEmpty() || m_subcategory.isNotEmpty() ||
        m_fxName.isNotEmpty() || m_description.isNotEmpty() ||
        m_keywords.isNotEmpty() || m_designer.isNotEmpty())
    {
        xml << "  <USER>\n";
        if (m_category.isNotEmpty())
            xml << "    <CATEGORY>" << juce::XmlElement::createTextElement(m_category)->getText() << "</CATEGORY>\n";
        if (m_subcategory.isNotEmpty())
            xml << "    <SUBCATEGORY>" << juce::XmlElement::createTextElement(m_subcategory)->getText() << "</SUBCATEGORY>\n";

        // SoundMiner Extended Fields
        if (m_fxName.isNotEmpty())
            xml << "    <FXNAME>" << juce::XmlElement::createTextElement(m_fxName)->getText() << "</FXNAME>\n";
        if (m_description.isNotEmpty())
            xml << "    <DESCRIPTION>" << juce::XmlElement::createTextElement(m_description)->getText() << "</DESCRIPTION>\n";
        if (m_keywords.isNotEmpty())
            xml << "    <KEYWORDS>" << juce::XmlElement::createTextElement(m_keywords)->getText() << "</KEYWORDS>\n";
        if (m_designer.isNotEmpty())
            xml << "    <DESIGNER>" << juce::XmlElement::createTextElement(m_designer)->getText() << "</DESIGNER>\n";

        xml << "  </USER>\n";
    }

    xml << "</BWFXML>\n";

    return xml;
}

bool iXMLMetadata::fromXMLString(const juce::String& xmlString)
{
    if (xmlString.isEmpty())
        return false;

    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(xmlString);
    if (xml == nullptr)
        return false;

    // Parse standard iXML fields
    if (auto* projectElem = xml->getChildByName("PROJECT"))
        m_project = projectElem->getAllSubText();

    if (auto* sceneElem = xml->getChildByName("SCENE"))
        m_scene = sceneElem->getAllSubText();

    if (auto* takeElem = xml->getChildByName("TAKE"))
        m_take = takeElem->getAllSubText();

    if (auto* tapeElem = xml->getChildByName("TAPE"))
        m_tape = tapeElem->getAllSubText();

    if (auto* noteElem = xml->getChildByName("NOTE"))
        m_notes = noteElem->getAllSubText();

    // Parse track title from TRACK_LIST
    if (auto* trackList = xml->getChildByName("TRACK_LIST"))
    {
        if (auto* track = trackList->getChildByName("TRACK"))
        {
            if (auto* nameElem = track->getChildByName("NAME"))
                m_trackTitle = nameElem->getAllSubText();
        }
    }

    // Parse UCS Category/Subcategory + SoundMiner Extended from USER extension
    if (auto* userElem = xml->getChildByName("USER"))
    {
        if (auto* catElem = userElem->getChildByName("CATEGORY"))
            m_category = catElem->getAllSubText();

        if (auto* subcatElem = userElem->getChildByName("SUBCATEGORY"))
            m_subcategory = subcatElem->getAllSubText();

        // Parse SoundMiner Extended Fields
        if (auto* fxNameElem = userElem->getChildByName("FXNAME"))
            m_fxName = fxNameElem->getAllSubText();

        if (auto* descElem = userElem->getChildByName("DESCRIPTION"))
            m_description = descElem->getAllSubText();

        if (auto* keywordsElem = userElem->getChildByName("KEYWORDS"))
            m_keywords = keywordsElem->getAllSubText();

        if (auto* designerElem = userElem->getChildByName("DESIGNER"))
            m_designer = designerElem->getAllSubText();
    }

    return hasMetadata();
}

void iXMLMetadata::fromJUCEMetadata(const juce::StringPairArray& metadata)
{
    // Try standard key names
    if (metadata.containsKey("CATEGORY"))
        m_category = metadata["CATEGORY"];

    if (metadata.containsKey("SUBCATEGORY"))
        m_subcategory = metadata["SUBCATEGORY"];

    if (metadata.containsKey("TRACK_TITLE") || metadata.containsKey("TrackTitle"))
        m_trackTitle = metadata.containsKey("TRACK_TITLE") ? metadata["TRACK_TITLE"] : metadata["TrackTitle"];

    if (metadata.containsKey("PROJECT"))
        m_project = metadata["PROJECT"];

    if (metadata.containsKey("TAPE") || metadata.containsKey("Library"))
        m_tape = metadata.containsKey("TAPE") ? metadata["TAPE"] : metadata["Library"];

    if (metadata.containsKey("SCENE"))
        m_scene = metadata["SCENE"];

    if (metadata.containsKey("TAKE"))
        m_take = metadata["TAKE"];

    if (metadata.containsKey("NOTE") || metadata.containsKey("Notes"))
        m_notes = metadata.containsKey("NOTE") ? metadata["NOTE"] : metadata["Notes"];

    // SoundMiner Extended Fields
    if (metadata.containsKey("FXNAME") || metadata.containsKey("FXName"))
        m_fxName = metadata.containsKey("FXNAME") ? metadata["FXNAME"] : metadata["FXName"];

    if (metadata.containsKey("DESCRIPTION") || metadata.containsKey("Description"))
        m_description = metadata.containsKey("DESCRIPTION") ? metadata["DESCRIPTION"] : metadata["Description"];

    if (metadata.containsKey("KEYWORDS") || metadata.containsKey("Keywords"))
        m_keywords = metadata.containsKey("KEYWORDS") ? metadata["KEYWORDS"] : metadata["Keywords"];

    if (metadata.containsKey("DESIGNER") || metadata.containsKey("Designer"))
        m_designer = metadata.containsKey("DESIGNER") ? metadata["DESIGNER"] : metadata["Designer"];
}

bool iXMLMetadata::hasMetadata() const
{
    return m_category.isNotEmpty() || m_subcategory.isNotEmpty() ||
           m_trackTitle.isNotEmpty() || m_project.isNotEmpty() ||
           m_tape.isNotEmpty() || m_scene.isNotEmpty() ||
           m_take.isNotEmpty() || m_notes.isNotEmpty() ||
           m_fxName.isNotEmpty() || m_description.isNotEmpty() ||
           m_keywords.isNotEmpty() || m_designer.isNotEmpty();
}

void iXMLMetadata::clear()
{
    m_category.clear();
    m_subcategory.clear();
    m_trackTitle.clear();
    m_project.clear();
    m_tape.clear();
    m_scene.clear();
    m_take.clear();
    m_notes.clear();
    m_fxName.clear();
    m_description.clear();
    m_keywords.clear();
    m_designer.clear();
}

juce::String iXMLMetadata::getCategoryFull() const
{
    // Compute CategoryFull in "CATEGORY-SUBCATEGORY" format
    // Example: "MAG-Evil" or "AMBIENCE-BIRDSONG"
    if (m_category.isEmpty())
        return juce::String();

    if (m_subcategory.isEmpty())
        return m_category;

    return m_category + "-" + m_subcategory;
}

iXMLMetadata iXMLMetadata::createDefault(const juce::String& category,
                                          const juce::String& subcategory)
{
    iXMLMetadata metadata;
    metadata.setCategory(category);
    metadata.setSubcategory(subcategory);
    metadata.setTape("ZQ SFX");
    return metadata;
}

iXMLMetadata iXMLMetadata::fromUCSFilename(const juce::String& filename)
{
    iXMLMetadata metadata;

    // Remove file extension
    juce::String name = juce::File::createFileWithoutCheckingPath(filename)
                            .getFileNameWithoutExtension();

    // UCS Format: CatID_FXName_CreatorID_SourceID
    // Example: MAGEvil_DESIGNED airMagic explosion 01_PGN_DM
    // Parts:   [0]       [1]                            [2]  [3]
    juce::StringArray parts;
    parts.addTokens(name, "_", "");

    if (parts.size() < 2)
        return metadata; // Not a valid UCS filename

    // Parse CatID (Category + Subcategory)
    juce::String catID = parts[0];
    juce::String category, subcategory;
    parseCatID(catID, category, subcategory);
    metadata.setCategory(category);
    metadata.setSubcategory(subcategory);

    // Parse FXName (SoundMiner FXName field, separate from TrackTitle)
    if (parts.size() >= 2)
    {
        metadata.setFXName(parts[1]);
        // Also set TrackTitle for backwards compatibility
        metadata.setTrackTitle(parts[1]);
    }

    // Parse Designer (CreatorID)
    if (parts.size() >= 3)
        metadata.setDesigner(parts[2]);

    // Parse SourceID (Project) - last part
    if (parts.size() >= 4)
        metadata.setProject(parts[3]);

    return metadata;
}

juce::String iXMLMetadata::toUCSFilename(const juce::String& creatorID) const
{
    // Generate UCS filename: CatID_FXName_CreatorID_SourceID
    juce::String filename;

    // CatID (Category + Subcategory)
    juce::String catID = getCatID();
    if (catID.isEmpty())
        catID = "UNKNUnkn"; // Unknown category

    filename << catID;

    // FXName (Track Title)
    juce::String fxName = m_trackTitle.isNotEmpty() ? m_trackTitle : "Untitled";
    // Remove illegal filename characters
    fxName = fxName.replaceCharacters(",.-", "   ").trim();
    filename << "_" << fxName;

    // CreatorID
    filename << "_" << creatorID;

    // SourceID (Project)
    juce::String sourceID = m_project.isNotEmpty() ? m_project : "WaveEdit";
    sourceID = sourceID.replaceCharacters(",.-_", "    ").trim();
    filename << "_" << sourceID;

    return filename;
}

juce::String iXMLMetadata::getCatID() const
{
    if (m_category.isEmpty())
        return juce::String();

    // CatID Format: CategorySubcategory (no separator)
    // Example: DOOR + Wood → DOORWood
    juce::String catID = m_category.toUpperCase();

    if (m_subcategory.isNotEmpty())
    {
        // Subcategory in Title Case
        juce::String subcat = m_subcategory;
        if (subcat.length() > 0)
        {
            subcat = subcat.substring(0, 1).toUpperCase() +
                     subcat.substring(1).toLowerCase();
        }
        catID << subcat;
    }

    return catID;
}

void iXMLMetadata::parseCatID(const juce::String& catID,
                               juce::String& outCategory,
                               juce::String& outSubcategory)
{
    if (catID.isEmpty())
    {
        outCategory.clear();
        outSubcategory.clear();
        return;
    }

    // CatID Format: CategorySubcategory
    // Category is ALL CAPS, Subcategory is Title Case
    // Example: DOORWood → DOOR + Wood

    // Find where Title Case begins (first lowercase after uppercase)
    int subcatStart = -1;
    for (int i = 1; i < catID.length(); ++i)
    {
        if (catID[i] >= 'A' && catID[i] <= 'Z' &&
            i + 1 < catID.length() &&
            catID[i + 1] >= 'a' && catID[i + 1] <= 'z')
        {
            subcatStart = i;
            break;
        }
    }

    if (subcatStart > 0)
    {
        outCategory = catID.substring(0, subcatStart);
        outSubcategory = catID.substring(subcatStart);
    }
    else
    {
        // No subcategory found, entire string is category
        outCategory = catID;
        outSubcategory.clear();
    }
}
