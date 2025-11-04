/*
 * WaveEdit by ZQ SFX
 * Copyright (C) 2025 ZQ SFX
 * Licensed under GPL v3
 *
 * MarkerManager.cpp
 */

#include "MarkerManager.h"

MarkerManager::MarkerManager()
    : m_selectedMarkerIndex(-1)
{
}

void MarkerManager::ensureMessageThread(const juce::String& methodName) const
{
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        DBG("WARNING: MarkerManager::" + methodName + " called from non-message thread!");
        jassertfalse;  // Debug-time assertion
    }
}

int MarkerManager::addMarker(const Marker& marker)
{
    ensureMessageThread("addMarker");

    int index;
    {
        juce::ScopedLock lock(m_lock);

        // Binary search for insertion point (markers sorted by position)
        auto it = std::lower_bound(m_markers.begin(), m_markers.end(), marker,
                                   [](const Marker& a, const Marker& b) {
                                       return a.getPosition() < b.getPosition();
                                   });

        index = static_cast<int>(std::distance(m_markers.begin(), it));
        m_markers.insert(index, marker);
    }  // Lock released here

    sendChangeMessage();
    return index;
}

void MarkerManager::removeMarker(int index)
{
    ensureMessageThread("removeMarker");

    {
        juce::ScopedLock lock(m_lock);

        if (index < 0 || index >= m_markers.size())
            return;

        m_markers.remove(index);

        // Adjust selection if needed
        if (m_selectedMarkerIndex == index)
            m_selectedMarkerIndex = -1;
        else if (m_selectedMarkerIndex > index)
            m_selectedMarkerIndex--;
    }  // Lock released here

    sendChangeMessage();
}

void MarkerManager::removeAllMarkers()
{
    ensureMessageThread("removeAllMarkers");

    {
        juce::ScopedLock lock(m_lock);
        m_markers.clear();
        m_selectedMarkerIndex = -1;
    }  // Lock released here

    sendChangeMessage();
}

void MarkerManager::insertMarkerAt(int index, const Marker& marker)
{
    ensureMessageThread("insertMarkerAt");
    juce::ScopedLock lock(m_lock);

    if (index < 0 || index > m_markers.size())
        return;

    m_markers.insert(index, marker);
}

int MarkerManager::getNumMarkers() const
{
    juce::ScopedLock lock(m_lock);
    return m_markers.size();
}

Marker* MarkerManager::getMarker(int index)
{
    juce::ScopedLock lock(m_lock);

    if (index < 0 || index >= m_markers.size())
        return nullptr;

    return &m_markers.getReference(index);
}

const Marker* MarkerManager::getMarker(int index) const
{
    juce::ScopedLock lock(m_lock);

    if (index < 0 || index >= m_markers.size())
        return nullptr;

    return &m_markers.getReference(index);
}

juce::Array<Marker> MarkerManager::getAllMarkers() const
{
    juce::ScopedLock lock(m_lock);
    return m_markers;  // Returns a copy for thread safety
}

int MarkerManager::findMarkerAtSample(int64_t sample, int64_t tolerance) const
{
    juce::ScopedLock lock(m_lock);

    for (int i = 0; i < m_markers.size(); ++i)
    {
        if (m_markers.getReference(i).isNear(sample, tolerance))
            return i;
    }

    return -1;
}

int MarkerManager::getNextMarkerIndex(int64_t currentSample) const
{
    juce::ScopedLock lock(m_lock);

    // Markers are sorted by position, so find first marker after currentSample
    for (int i = 0; i < m_markers.size(); ++i)
    {
        if (m_markers.getReference(i).getPosition() > currentSample)
            return i;
    }

    return -1;  // No marker after current position
}

int MarkerManager::getPreviousMarkerIndex(int64_t currentSample) const
{
    juce::ScopedLock lock(m_lock);

    // Markers are sorted by position, so find last marker before currentSample
    for (int i = m_markers.size() - 1; i >= 0; --i)
    {
        if (m_markers.getReference(i).getPosition() < currentSample)
            return i;
    }

    return -1;  // No marker before current position
}

void MarkerManager::setSelectedMarkerIndex(int index)
{
    juce::ScopedLock lock(m_lock);

    // Validate index
    jassert(index >= -1 && index < m_markers.size());

    if (index >= -1 && index < m_markers.size())
        m_selectedMarkerIndex = index;
    else
        m_selectedMarkerIndex = -1;  // Clear selection on invalid index
}

bool MarkerManager::saveToFile(const juce::File& audioFile) const
{
    juce::ScopedLock lock(m_lock);

    // Create JSON structure
    auto* root = new juce::DynamicObject();
    root->setProperty("version", "1.0");
    root->setProperty("audioFile", audioFile.getFileName());

    // Serialize markers
    juce::Array<juce::var> markerArray;
    for (const auto& marker : m_markers)
        markerArray.add(marker.toJSON());

    root->setProperty("markers", markerArray);

    // Write to file
    juce::File markerFile = getMarkerFilePath(audioFile);
    juce::var jsonData(root);
    juce::String jsonString = juce::JSON::toString(jsonData, true);  // Pretty-printed

    return markerFile.replaceWithText(jsonString);
}

bool MarkerManager::loadFromFile(const juce::File& audioFile)
{
    ensureMessageThread("loadFromFile");

    juce::File markerFile = getMarkerFilePath(audioFile);

    if (!markerFile.existsAsFile())
        return false;  // No marker file exists (not an error)

    // Parse JSON
    juce::String jsonString = markerFile.loadFileAsString();
    juce::var jsonData = juce::JSON::parse(jsonString);

    if (!jsonData.isObject())
        return false;

    auto* root = jsonData.getDynamicObject();
    if (!root)
        return false;

    // Load markers
    juce::var markersVar = root->getProperty("markers");
    if (!markersVar.isArray())
        return false;

    // Parse to temporary array first (don't modify state until successful)
    juce::Array<Marker> tempMarkers;
    juce::Array<juce::var>* markerArray = markersVar.getArray();

    for (const auto& markerVar : *markerArray)
    {
        Marker marker = Marker::fromJSON(markerVar);
        tempMarkers.add(marker);
    }

    // Ensure markers are sorted by position
    std::sort(tempMarkers.begin(), tempMarkers.end(),
              [](const Marker& a, const Marker& b) { return a.getPosition() < b.getPosition(); });

    {
        juce::ScopedLock lock(m_lock);
        // Only update state if parse succeeded
        m_markers.swapWith(tempMarkers);
        m_selectedMarkerIndex = -1;  // Clear selection after load
    }  // Lock released here

    sendChangeMessage();
    return true;
}

juce::File MarkerManager::getMarkerFilePath(const juce::File& audioFile)
{
    // Create sidecar file: example.wav -> example.wav.markers.json
    return audioFile.getParentDirectory().getChildFile(audioFile.getFileName() + ".markers.json");
}
