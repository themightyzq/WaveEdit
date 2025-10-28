/*
  ==============================================================================

    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

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

#include "Document.h"

Document::Document(const juce::File& file)
    : m_file(file),
      m_isModified(false),
      m_waveformDisplay(m_audioEngine.getFormatManager()),
      m_transportControls(m_audioEngine, m_waveformDisplay),
      m_regionDisplay(m_regionManager),  // Phase 3 Tier 2 - Region system
      m_savedPlaybackPosition(0.0)
{
    // Configure undo manager with transaction limits (100 undo levels)
    // minTransactions set to 90 to allow headroom for complex multi-unit transactions
    m_undoManager.setMaxNumberOfStoredUnits(100, 90);

    // Connect WaveformDisplay to RegionManager for region overlay rendering
    m_waveformDisplay.setRegionManager(&m_regionManager);

    // Connect WaveformDisplay to RegionDisplay for zoom/scroll synchronization
    // When the waveform view changes (zoom, scroll), update RegionDisplay's visible range
    m_waveformDisplay.onVisibleRangeChanged = [this](double startTime, double endTime)
    {
        m_regionDisplay.setVisibleRange(startTime, endTime);
        m_regionDisplay.repaint();
    };

    // Initialize audio engine
    if (!m_audioEngine.initializeAudioDevice())
    {
        juce::Logger::writeToLog("Warning: Failed to initialize audio device for document");
    }
}

Document::~Document()
{
    // Ensure audio engine is properly closed
    m_audioEngine.closeAudioFile();
}

juce::String Document::getFilename() const
{
    if (m_file == juce::File())
    {
        return "Untitled";
    }

    return m_file.getFileNameWithoutExtension();
}

void Document::setFile(const juce::File& file)
{
    m_file = file;
}

void Document::setModified(bool modified)
{
    if (m_isModified != modified)
    {
        m_isModified = modified;

        // Log state change
        juce::Logger::writeToLog(juce::String::formatted(
            "Document '%s' modified state: %s",
            getFilename().toRawUTF8(),
            modified ? "true" : "false"));
    }
}

bool Document::loadFile(const juce::File& file)
{
    if (!file.existsAsFile())
    {
        juce::Logger::writeToLog("Error: File does not exist: " + file.getFullPathName());
        return false;
    }

    // Load audio file using AudioEngine (for playback)
    if (!m_audioEngine.loadAudioFile(file))
    {
        juce::Logger::writeToLog("Error: Failed to load audio file: " + file.getFullPathName());
        return false;
    }

    // CRITICAL FIX: Load audio buffer into BufferManager (for editing)
    if (!m_bufferManager.loadFromFile(file, m_audioEngine.getFormatManager()))
    {
        juce::Logger::writeToLog("Error: Failed to load audio buffer for editing: " + file.getFullPathName());
        m_audioEngine.closeAudioFile();
        return false;
    }

    // Update file path
    m_file = file;
    m_isModified = false;

    // Load waveform display
    if (!m_waveformDisplay.loadFile(file, m_audioEngine.getSampleRate(), m_audioEngine.getNumChannels()))
    {
        juce::Logger::writeToLog("Warning: Failed to load waveform display for: " + file.getFullPathName());
    }

    m_waveformDisplay.clearSelection();

    // Initialize region display (Phase 3 Tier 2)
    m_regionDisplay.setSampleRate(m_audioEngine.getSampleRate());
    m_regionDisplay.setTotalDuration(m_bufferManager.getLengthInSeconds());
    m_regionDisplay.setVisibleRange(0.0, m_bufferManager.getLengthInSeconds());
    m_regionDisplay.setAudioBuffer(&m_bufferManager.getBuffer());  // Phase 3.3 - For zero-crossing snap

    // Load regions from sidecar JSON file (if exists)
    m_regionManager.loadFromFile(file);

    // Connect WaveformDisplay to RegionManager for region overlay rendering
    m_waveformDisplay.setRegionManager(&m_regionManager);

    // Reset playback position
    m_savedPlaybackPosition = 0.0;

    // Clear undo history for new file
    m_undoManager.clearUndoHistory();

    juce::Logger::writeToLog("Document loaded: " + file.getFullPathName());
    return true;
}

void Document::closeFile()
{
    m_audioEngine.closeAudioFile();
    m_bufferManager.clear();
    m_waveformDisplay.clear();
    m_waveformDisplay.clearSelection();
    m_undoManager.clearUndoHistory();

    m_file = juce::File();
    m_isModified = false;
    m_savedPlaybackPosition = 0.0;

    juce::Logger::writeToLog("Document closed");
}
