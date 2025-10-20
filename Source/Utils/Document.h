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

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioBufferManager.h"
#include "../UI/WaveformDisplay.h"
#include "../UI/TransportControls.h"
#include "RegionManager.h"
#include "../UI/RegionDisplay.h"

/**
 * Document class represents a single audio file with all associated state.
 *
 * This class encapsulates everything needed for editing one audio file:
 * - Audio engine for playback and recording
 * - Buffer manager for audio data
 * - Waveform display for visualization
 * - Transport controls for playback UI
 * - Undo manager for edit history
 * - Modified flag for save prompts
 * - Playback position and selection state
 *
 * Each Document instance is independent, allowing multiple files to be
 * open simultaneously with separate undo histories, selections, and playback states.
 *
 * Thread Safety: All methods must be called from the message thread only.
 */
class Document
{
public:
    /**
     * Creates a new document.
     *
     * @param file The audio file to load (may be invalid for new/empty documents)
     */
    explicit Document(const juce::File& file = juce::File());

    /**
     * Destructor. Ensures audio engine is properly closed.
     */
    ~Document();

    //==============================================================================
    // File Information

    /**
     * Gets the filename without path or extension.
     *
     * @return Filename string (e.g., "myfile" from "/path/to/myfile.wav")
     */
    juce::String getFilename() const;

    /**
     * Gets the full file path.
     *
     * @return File object representing the document's file
     */
    juce::File getFile() const { return m_file; }

    /**
     * Sets the file path (used when saving to a new location).
     *
     * @param file The new file path
     */
    void setFile(const juce::File& file);

    /**
     * Checks if this document has unsaved changes.
     *
     * @return true if modified since last save, false otherwise
     */
    bool isModified() const { return m_isModified; }

    /**
     * Sets the modified flag.
     * Typically set to true after edits, false after saves.
     *
     * @param modified The new modified state
     */
    void setModified(bool modified);

    /**
     * Checks if a file is loaded in this document.
     *
     * @return true if a valid audio file is loaded, false for empty documents
     */
    bool hasFile() const { return m_file.existsAsFile(); }

    //==============================================================================
    // Audio Engine Access

    /**
     * Gets the audio engine for this document.
     * The engine handles playback, recording, and audio I/O.
     *
     * @return Reference to the document's AudioEngine
     */
    AudioEngine& getAudioEngine() { return m_audioEngine; }
    const AudioEngine& getAudioEngine() const { return m_audioEngine; }

    /**
     * Gets the buffer manager for this document.
     * The buffer manager holds the audio data and handles conversions.
     *
     * @return Reference to the document's AudioBufferManager
     */
    AudioBufferManager& getBufferManager() { return m_bufferManager; }
    const AudioBufferManager& getBufferManager() const { return m_bufferManager; }

    /**
     * Gets the waveform display component for this document.
     * The waveform displays the audio visually and handles selection.
     *
     * @return Reference to the document's WaveformDisplay
     */
    WaveformDisplay& getWaveformDisplay() { return m_waveformDisplay; }
    const WaveformDisplay& getWaveformDisplay() const { return m_waveformDisplay; }

    /**
     * Gets the transport controls component for this document.
     * The transport provides play/pause/stop/loop buttons.
     *
     * @return Reference to the document's TransportControls
     */
    TransportControls& getTransportControls() { return m_transportControls; }
    const TransportControls& getTransportControls() const { return m_transportControls; }

    /**
     * Gets the undo manager for this document.
     * Each document has independent undo history (100 levels).
     *
     * @return Reference to the document's UndoManager
     */
    juce::UndoManager& getUndoManager() { return m_undoManager; }
    const juce::UndoManager& getUndoManager() const { return m_undoManager; }

    /**
     * Gets the region manager for this document.
     * The region manager tracks named regions/markers in the audio.
     *
     * @return Reference to the document's RegionManager
     */
    RegionManager& getRegionManager() { return m_regionManager; }
    const RegionManager& getRegionManager() const { return m_regionManager; }

    /**
     * Gets the region display component for this document.
     * The region display shows colored bars above the waveform.
     *
     * @return Reference to the document's RegionDisplay
     */
    RegionDisplay& getRegionDisplay() { return m_regionDisplay; }
    const RegionDisplay& getRegionDisplay() const { return m_regionDisplay; }

    //==============================================================================
    // State Management

    /**
     * Saves the current playback position for restoration later.
     * Used when switching between documents.
     *
     * @param position The playback position in seconds
     */
    void setPlaybackPosition(double position) { m_savedPlaybackPosition = position; }

    /**
     * Gets the saved playback position.
     *
     * @return The last saved playback position in seconds
     */
    double getPlaybackPosition() const { return m_savedPlaybackPosition; }

    /**
     * Loads an audio file into this document.
     *
     * @param file The audio file to load
     * @return true if load successful, false on error
     */
    bool loadFile(const juce::File& file);

    /**
     * Closes the current file and clears all state.
     */
    void closeFile();

private:
    // File information
    juce::File m_file;
    bool m_isModified;

    // Audio components (owned by document)
    AudioEngine m_audioEngine;
    AudioBufferManager m_bufferManager;
    WaveformDisplay m_waveformDisplay;
    TransportControls m_transportControls;

    // Edit history
    juce::UndoManager m_undoManager;

    // Region system (Phase 3 Tier 2)
    RegionManager m_regionManager;
    RegionDisplay m_regionDisplay;

    // Saved state (for tab switching)
    double m_savedPlaybackPosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Document)
};
