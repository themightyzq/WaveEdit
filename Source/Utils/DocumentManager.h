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
#include <juce_audio_basics/juce_audio_basics.h>
#include "Document.h"

/**
 * DocumentManager manages multiple open documents with tab interface.
 *
 * This class provides:
 * - Multiple document management (open, close, switch)
 * - Tab navigation (next, previous, by index)
 * - Inter-file clipboard (copy/paste between documents)
 * - Document lifecycle management
 * - Current document tracking
 *
 * The manager ensures only one document is "active" at a time, which determines
 * which document's UI components are visible and which receives input.
 *
 * Thread Safety: All methods must be called from the message thread only.
 *
 * Usage Example:
 * ```cpp
 * DocumentManager docMgr;
 * Document* doc = docMgr.createDocument(File("audio.wav"));
 * docMgr.setCurrentDocument(doc);
 * // ... edit ...
 * docMgr.closeDocument(doc);
 * ```
 */
class DocumentManager
{
public:
    /**
     * Listener interface for document events.
     * Implement this to respond to document changes (e.g., to update UI).
     */
    class Listener
    {
    public:
        virtual ~Listener() = default;

        /**
         * Called when the current document changes.
         *
         * @param newDocument The newly active document (may be nullptr if all closed)
         */
        virtual void currentDocumentChanged(Document* newDocument) = 0;

        /**
         * Called when a document is added to the manager.
         *
         * @param document The document that was added
         * @param index The index of the new document
         */
        virtual void documentAdded(Document* document, int index) = 0;

        /**
         * Called when a document is about to be removed.
         *
         * @param document The document being removed
         * @param index The index of the document being removed
         */
        virtual void documentRemoved(Document* document, int index) = 0;
    };

    /**
     * Creates an empty DocumentManager with no open documents.
     */
    DocumentManager();

    /**
     * Destructor. Closes all open documents.
     */
    ~DocumentManager();

    //==============================================================================
    // Document Lifecycle

    /**
     * Creates a new empty document and adds it to the manager.
     *
     * @return Pointer to the newly created document (manager owns it)
     */
    Document* createDocument();

    /**
     * Creates a new document and loads the specified file.
     *
     * @param file The audio file to load
     * @return Pointer to the document if successful, nullptr on load failure
     */
    Document* openDocument(const juce::File& file);

    /**
     * Closes the specified document.
     * If the document is modified, caller should prompt to save first.
     *
     * @param document The document to close
     * @return true if closed successfully, false if document not found
     */
    bool closeDocument(Document* document);

    /**
     * Closes the document at the specified index.
     *
     * @param index The index of the document to close
     * @return true if closed successfully, false if index out of range
     */
    bool closeDocumentAt(int index);

    /**
     * Closes the current document.
     * If no document is current, does nothing.
     *
     * @return true if a document was closed, false if no current document
     */
    bool closeCurrentDocument();

    /**
     * Closes all open documents.
     * Caller should prompt to save modified documents first.
     */
    void closeAllDocuments();

    //==============================================================================
    // Document Access

    /**
     * Gets the currently active document.
     *
     * @return Pointer to current document, or nullptr if no documents open
     */
    Document* getCurrentDocument() const;

    /**
     * Gets the index of the currently active document.
     *
     * @return Index of current document, or -1 if no documents open
     */
    int getCurrentDocumentIndex() const { return m_currentDocumentIndex; }

    /**
     * Sets the current document by index.
     *
     * @param index The index of the document to make current
     * @return true if successful, false if index out of range
     */
    bool setCurrentDocumentIndex(int index);

    /**
     * Sets the current document by pointer.
     *
     * @param document The document to make current
     * @return true if successful, false if document not found
     */
    bool setCurrentDocument(Document* document);

    /**
     * Gets the total number of open documents.
     *
     * @return Number of documents
     */
    int getNumDocuments() const { return m_documents.size(); }

    /**
     * Gets the document at the specified index.
     *
     * @param index The index of the document
     * @return Pointer to document, or nullptr if index out of range
     */
    Document* getDocument(int index) const;

    /**
     * Finds the index of the specified document.
     *
     * @param document The document to find
     * @return Index of document, or -1 if not found
     */
    int getDocumentIndex(const Document* document) const;

    //==============================================================================
    // Tab Navigation

    /**
     * Switches to the next document (wraps around).
     * Keyboard shortcut: Cmd+Tab or Cmd+Option+Right
     */
    void selectNextDocument();

    /**
     * Switches to the previous document (wraps around).
     * Keyboard shortcut: Cmd+Shift+Tab or Cmd+Option+Left
     */
    void selectPreviousDocument();

    /**
     * Switches to document by number (1-9).
     * Keyboard shortcut: Cmd+1 through Cmd+9
     *
     * @param number The document number (1-9)
     * @return true if successful, false if number out of range
     */
    bool selectDocumentByNumber(int number);

    //==============================================================================
    // Inter-File Clipboard

    /**
     * Copies audio to the inter-file clipboard.
     * This allows copying from one document and pasting into another.
     *
     * @param buffer The audio buffer to copy
     * @param sampleRate The sample rate of the audio
     */
    void copyToInterFileClipboard(const juce::AudioBuffer<float>& buffer, double sampleRate);

    /**
     * Pastes audio from the inter-file clipboard.
     *
     * @param targetDoc The document to paste into
     * @param position The position in seconds where to paste
     * @return true if successful, false if no clipboard data or paste failed
     */
    bool pasteFromInterFileClipboard(Document* targetDoc, double position);

    /**
     * Checks if the inter-file clipboard has audio data.
     *
     * @return true if clipboard has audio, false if empty
     */
    bool hasInterFileClipboard() const { return m_hasInterFileClipboard; }

    /**
     * Gets the duration of audio in the inter-file clipboard.
     *
     * @return Duration in seconds, or 0.0 if clipboard empty
     */
    double getInterFileClipboardDuration() const;

    //==============================================================================
    // Listener Management

    /**
     * Adds a listener to receive document events.
     *
     * @param listener The listener to add
     */
    void addListener(Listener* listener);

    /**
     * Removes a listener.
     *
     * @param listener The listener to remove
     */
    void removeListener(Listener* listener);

private:
    // Document storage
    juce::OwnedArray<Document> m_documents;
    int m_currentDocumentIndex;

    // Inter-file clipboard
    juce::AudioBuffer<float> m_interFileClipboard;
    double m_interFileClipboardSampleRate;
    bool m_hasInterFileClipboard;

    // Listeners
    juce::ListenerList<Listener> m_listeners;

    // Helper methods
    void notifyCurrentDocumentChanged();
    void notifyDocumentAdded(Document* document, int index);
    void notifyDocumentRemoved(Document* document, int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DocumentManager)
};
