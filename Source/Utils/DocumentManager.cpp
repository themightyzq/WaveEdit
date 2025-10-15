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

#include "DocumentManager.h"

DocumentManager::DocumentManager()
    : m_currentDocumentIndex(-1),
      m_interFileClipboardSampleRate(44100.0),
      m_hasInterFileClipboard(false)
{
}

DocumentManager::~DocumentManager()
{
    closeAllDocuments();
}

//==============================================================================
// Document Lifecycle

Document* DocumentManager::createDocument()
{
    auto* document = new Document();
    m_documents.add(document);

    int newIndex = m_documents.size() - 1;
    notifyDocumentAdded(document, newIndex);

    // Make this the current document if it's the first one
    if (m_documents.size() == 1)
    {
        setCurrentDocumentIndex(0);
    }

    juce::Logger::writeToLog("Created new document");
    return document;
}

Document* DocumentManager::openDocument(const juce::File& file)
{
    if (!file.existsAsFile())
    {
        juce::Logger::writeToLog("Error: Cannot open non-existent file: " + file.getFullPathName());
        return nullptr;
    }

    // Check if file is already open
    for (auto* doc : m_documents)
    {
        if (doc->getFile() == file)
        {
            // File already open - just switch to it
            setCurrentDocument(doc);
            juce::Logger::writeToLog("File already open, switched to existing document: " + file.getFullPathName());
            return doc;
        }
    }

    // Create new document and load file
    auto* document = new Document(file);
    if (!document->loadFile(file))
    {
        // Load failed - don't add to manager
        delete document;
        juce::Logger::writeToLog("Error: Failed to load file: " + file.getFullPathName());
        return nullptr;
    }

    m_documents.add(document);

    int newIndex = m_documents.size() - 1;
    notifyDocumentAdded(document, newIndex);

    // Make this the current document
    setCurrentDocumentIndex(newIndex);

    juce::Logger::writeToLog("Opened document: " + file.getFullPathName());
    return document;
}

bool DocumentManager::closeDocument(Document* document)
{
    if (document == nullptr)
        return false;

    int index = m_documents.indexOf(document);
    if (index < 0)
        return false;

    return closeDocumentAt(index);
}

bool DocumentManager::closeDocumentAt(int index)
{
    if (index < 0 || index >= m_documents.size())
        return false;

    Document* document = m_documents[index];

    // Notify before removal
    notifyDocumentRemoved(document, index);

    // Remove document (this will delete it)
    m_documents.remove(index);

    // Update current document index
    if (m_documents.size() == 0)
    {
        // No documents left
        m_currentDocumentIndex = -1;
        notifyCurrentDocumentChanged();
    }
    else if (index == m_currentDocumentIndex)
    {
        // We closed the current document - switch to another
        if (index >= m_documents.size())
        {
            // We closed the last document - switch to the new last one
            setCurrentDocumentIndex(m_documents.size() - 1);
        }
        else
        {
            // Switch to the document at the same index (next document)
            setCurrentDocumentIndex(index);
        }
    }
    else if (index < m_currentDocumentIndex)
    {
        // We closed a document before the current one - adjust index
        m_currentDocumentIndex--;
    }

    juce::Logger::writeToLog("Closed document at index " + juce::String(index));
    return true;
}

bool DocumentManager::closeCurrentDocument()
{
    if (m_currentDocumentIndex < 0)
        return false;

    return closeDocumentAt(m_currentDocumentIndex);
}

void DocumentManager::closeAllDocuments()
{
    while (m_documents.size() > 0)
    {
        closeDocumentAt(0);
    }

    juce::Logger::writeToLog("Closed all documents");
}

//==============================================================================
// Document Access

Document* DocumentManager::getCurrentDocument() const
{
    if (m_currentDocumentIndex >= 0 && m_currentDocumentIndex < m_documents.size())
    {
        return m_documents[m_currentDocumentIndex];
    }

    return nullptr;
}

bool DocumentManager::setCurrentDocumentIndex(int index)
{
    if (index < 0 || index >= m_documents.size())
        return false;

    if (m_currentDocumentIndex == index)
        return true; // Already current

    // Save playback position of current document
    if (auto* currentDoc = getCurrentDocument())
    {
        double position = currentDoc->getAudioEngine().getCurrentPosition();
        currentDoc->setPlaybackPosition(position);
    }

    m_currentDocumentIndex = index;
    notifyCurrentDocumentChanged();

    juce::Logger::writeToLog("Switched to document at index " + juce::String(index));
    return true;
}

bool DocumentManager::setCurrentDocument(Document* document)
{
    int index = m_documents.indexOf(document);
    if (index < 0)
        return false;

    return setCurrentDocumentIndex(index);
}

Document* DocumentManager::getDocument(int index) const
{
    if (index >= 0 && index < m_documents.size())
    {
        return m_documents[index];
    }

    return nullptr;
}

int DocumentManager::getDocumentIndex(const Document* document) const
{
    return m_documents.indexOf(const_cast<Document*>(document));
}

//==============================================================================
// Tab Navigation

void DocumentManager::selectNextDocument()
{
    if (m_documents.size() <= 1)
        return; // Nothing to switch to

    int nextIndex = (m_currentDocumentIndex + 1) % m_documents.size();
    setCurrentDocumentIndex(nextIndex);

    juce::Logger::writeToLog("Switched to next document");
}

void DocumentManager::selectPreviousDocument()
{
    if (m_documents.size() <= 1)
        return; // Nothing to switch to

    int prevIndex = m_currentDocumentIndex - 1;
    if (prevIndex < 0)
        prevIndex = m_documents.size() - 1;

    setCurrentDocumentIndex(prevIndex);

    juce::Logger::writeToLog("Switched to previous document");
}

bool DocumentManager::selectDocumentByNumber(int number)
{
    // Convert 1-based to 0-based index
    int index = number - 1;

    if (index < 0 || index >= m_documents.size())
        return false;

    return setCurrentDocumentIndex(index);
}

//==============================================================================
// Inter-File Clipboard

void DocumentManager::copyToInterFileClipboard(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        m_hasInterFileClipboard = false;
        return;
    }

    // Copy buffer
    m_interFileClipboard.makeCopyOf(buffer);
    m_interFileClipboardSampleRate = sampleRate;
    m_hasInterFileClipboard = true;

    juce::Logger::writeToLog(juce::String::formatted(
        "Copied to inter-file clipboard: %.2f seconds at %.0f Hz",
        buffer.getNumSamples() / sampleRate, sampleRate));
}

bool DocumentManager::pasteFromInterFileClipboard(Document* targetDoc, double position)
{
    if (!m_hasInterFileClipboard || targetDoc == nullptr)
        return false;

    if (m_interFileClipboard.getNumSamples() == 0)
        return false;

    // TODO: Implement actual paste operation on the document's buffer
    // This will require access to the document's AudioBufferManager
    // For now, just log the operation

    juce::Logger::writeToLog(juce::String::formatted(
        "Pasting from inter-file clipboard at position %.2f seconds", position));

    return true;
}

double DocumentManager::getInterFileClipboardDuration() const
{
    if (!m_hasInterFileClipboard || m_interFileClipboard.getNumSamples() == 0)
        return 0.0;

    return m_interFileClipboard.getNumSamples() / m_interFileClipboardSampleRate;
}

//==============================================================================
// Listener Management

void DocumentManager::addListener(Listener* listener)
{
    m_listeners.add(listener);
}

void DocumentManager::removeListener(Listener* listener)
{
    m_listeners.remove(listener);
}

//==============================================================================
// Private Helper Methods

void DocumentManager::notifyCurrentDocumentChanged()
{
    Document* currentDoc = getCurrentDocument();
    m_listeners.call([currentDoc](Listener& l) { l.currentDocumentChanged(currentDoc); });
}

void DocumentManager::notifyDocumentAdded(Document* document, int index)
{
    m_listeners.call([document, index](Listener& l) { l.documentAdded(document, index); });
}

void DocumentManager::notifyDocumentRemoved(Document* document, int index)
{
    m_listeners.call([document, index](Listener& l) { l.documentRemoved(document, index); });
}
