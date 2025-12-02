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
#include "../Audio/AudioFileManager.h"

Document::Document(const juce::File& file)
    : m_file(file),
      m_isModified(false),
      m_waveformDisplay(m_audioEngine.getFormatManager()),
      m_transportControls(m_audioEngine, m_waveformDisplay),
      m_regionDisplay(m_regionManager),  // Phase 3 Tier 2 - Region system
      m_markerDisplay(m_markerManager),  // Phase 3.4 - Marker system
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

        // Also update MarkerDisplay (Phase 3.4)
        m_markerDisplay.setVisibleRange(startTime, endTime);
        m_markerDisplay.repaint();
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

    // Initialize marker display (Phase 3.4)
    m_markerDisplay.setSampleRate(m_audioEngine.getSampleRate());
    m_markerDisplay.setTotalDuration(m_bufferManager.getLengthInSeconds());
    m_markerDisplay.setVisibleRange(0.0, m_bufferManager.getLengthInSeconds());

    // Load markers from sidecar JSON file (if exists)
    m_markerManager.loadFromFile(file);

    // Load BWF metadata (Phase 4 Tier 1)
    if (m_bwfMetadata.loadFromFile(file))
    {
        juce::Logger::writeToLog("Loaded BWF metadata for: " + file.getFullPathName());
    }
    else
    {
        juce::Logger::writeToLog("No BWF metadata found in: " + file.getFullPathName());
        // Not an error - create default metadata for WaveEdit files
        m_bwfMetadata = BWFMetadata::createDefault(file.getFileNameWithoutExtension());
    }

    // Load iXML metadata (Phase 4 Tier 1 - UCS/SoundMiner compatibility)
    if (m_ixmlMetadata.loadFromFile(file))
    {
        juce::Logger::writeToLog("Loaded iXML metadata for: " + file.getFullPathName());
    }
    else
    {
        // Try parsing from UCS filename if no embedded iXML
        m_ixmlMetadata = iXMLMetadata::fromUCSFilename(file.getFileName());
        if (m_ixmlMetadata.hasMetadata())
        {
            juce::Logger::writeToLog("Parsed UCS metadata from filename: " + file.getFileName());
        }
        else
        {
            juce::Logger::writeToLog("No iXML or UCS metadata found in: " + file.getFullPathName());
        }
    }

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

bool Document::saveFile(const juce::File& file, int bitDepth, int quality, double targetSampleRate)
{
    // Validate parameters
    if (!file.getParentDirectory().exists())
    {
        juce::Logger::writeToLog("Error: Directory does not exist: " + file.getParentDirectory().getFullPathName());
        return false;
    }

    if (bitDepth != 8 && bitDepth != 16 && bitDepth != 24 && bitDepth != 32)
    {
        juce::Logger::writeToLog("Error: Invalid bit depth: " + juce::String(bitDepth) + " (must be 8, 16, 24, or 32)");
        return false;
    }

    if (quality < 0 || quality > 10)
    {
        juce::Logger::writeToLog("Error: Invalid quality: " + juce::String(quality) + " (must be 0-10)");
        return false;
    }

    // Get audio buffer and sample rate from buffer manager
    const juce::AudioBuffer<float>& buffer = m_bufferManager.getBuffer();
    double sourceSampleRate = m_audioEngine.getSampleRate();

    if (buffer.getNumSamples() == 0)
    {
        juce::Logger::writeToLog("Error: No audio data to save");
        return false;
    }

    // Determine final sample rate
    double finalSampleRate = (targetSampleRate > 0.0) ? targetSampleRate : sourceSampleRate;

    // Resample if necessary
    juce::AudioBuffer<float> bufferToSave;
    if (targetSampleRate > 0.0 && std::abs(targetSampleRate - sourceSampleRate) > 0.01)
    {
        juce::Logger::writeToLog("Resampling from " + juce::String(sourceSampleRate, 0) +
                                 " Hz to " + juce::String(targetSampleRate, 0) + " Hz");
        bufferToSave = AudioFileManager::resampleBuffer(buffer, sourceSampleRate, targetSampleRate);
    }
    else
    {
        // No resampling needed - create copy of buffer
        bufferToSave.setSize(buffer.getNumChannels(), buffer.getNumSamples());
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            bufferToSave.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
    }

    // Update BWF metadata with current timestamp if not set
    if (!m_bwfMetadata.hasMetadata())
    {
        m_bwfMetadata = BWFMetadata::createDefault(file.getFileNameWithoutExtension());
    }

    // Update origination date/time to now
    m_bwfMetadata.setOriginationDateTime(juce::Time::getCurrentTime());

    // Convert BWF metadata to JUCE format
    juce::StringPairArray metadata = m_bwfMetadata.toJUCEMetadata();

    // Add iXML metadata (Phase 4 Tier 1 - UCS/SoundMiner compatibility)
    if (m_ixmlMetadata.hasMetadata())
    {
        metadata.set("iXML", m_ixmlMetadata.toXMLString());
        juce::Logger::writeToLog("Embedding iXML metadata in file");
    }

    // Save using AudioFileManager
    AudioFileManager fileManager;

    // Use the universal saveAudioFile method which auto-detects format
    // For WAV files, this will use saveAsWav/overwriteFile internally
    bool success = fileManager.saveAudioFile(file, bufferToSave, finalSampleRate, bitDepth, quality, metadata);

    if (success)
    {
        // Append iXML chunk if we have iXML metadata (WAV files only)
        // Note: FLAC/OGG don't support iXML chunks
        if (m_ixmlMetadata.hasMetadata() && file.hasFileExtension(".wav"))
        {
            juce::String ixmlString = m_ixmlMetadata.toXMLString();
            bool ixmlSuccess = fileManager.appendiXMLChunk(file, ixmlString);

            if (!ixmlSuccess)
            {
                juce::Logger::writeToLog("Warning: Failed to write iXML chunk: " + fileManager.getLastError());
                // Continue anyway - BWF metadata was written successfully
            }
        }
        else if (m_ixmlMetadata.hasMetadata() && !file.hasFileExtension(".wav"))
        {
            juce::Logger::writeToLog("Note: iXML metadata not saved (not supported for " +
                                    file.getFileExtension() + " format)");
        }

        // Update document state
        m_file = file;
        m_isModified = false;

        // Save region data as sidecar JSON
        m_regionManager.saveToFile(file);

        // Save marker data as sidecar JSON
        m_markerManager.saveToFile(file);

        juce::Logger::writeToLog("Document saved: " + file.getFullPathName());
        return true;
    }
    else
    {
        juce::String errorMsg = fileManager.getLastError();
        juce::Logger::writeToLog("Error saving file: " + errorMsg);
        return false;
    }
}
