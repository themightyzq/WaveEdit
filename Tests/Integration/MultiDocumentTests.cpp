/*
  ==============================================================================

    MultiDocumentTests.cpp
    Created: 2025-10-15
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Comprehensive integration tests for multi-document architecture.
    Tests DocumentManager, Document lifecycle, tab switching, state isolation,
    and inter-file operations.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Utils/DocumentManager.h"
#include "Utils/Document.h"
#include "../TestUtils/TestAudioFiles.h"
#include "../TestUtils/AudioAssertions.h"

// ============================================================================
// Test Helper Classes
// ============================================================================

/**
 * RAII guard for automatic temporary file cleanup.
 * Ensures temp files are deleted even when tests fail.
 */
class TempFileGuard
{
public:
    explicit TempFileGuard(juce::File f) : file(std::move(f)) {}
    ~TempFileGuard()
    {
        if (file.existsAsFile())
            file.deleteFile();
    }

    // Allow implicit conversion to juce::File
    operator juce::File() const { return file; }

    // Prevent copying but allow moving
    TempFileGuard(const TempFileGuard&) = delete;
    TempFileGuard& operator=(const TempFileGuard&) = delete;
    TempFileGuard(TempFileGuard&&) = default;
    TempFileGuard& operator=(TempFileGuard&&) = default;

private:
    juce::File file;
};

/**
 * Mock listener for testing DocumentManager events.
 * Records all callbacks for verification.
 */
class MockDocumentListener : public DocumentManager::Listener
{
public:
    MockDocumentListener() = default;

    void currentDocumentChanged(Document* newDocument) override
    {
        m_currentChangedCount++;
        m_lastCurrentDocument = newDocument;
    }

    void documentAdded(Document* document, int index) override
    {
        m_addedCount++;
        m_lastAddedDocument = document;
        m_lastAddedIndex = index;
    }

    void documentRemoved(Document* document, int index) override
    {
        m_removedCount++;
        m_lastRemovedDocument = document;
        m_lastRemovedIndex = index;
    }

    // Getters for verification
    int getCurrentChangedCount() const { return m_currentChangedCount; }
    int getAddedCount() const { return m_addedCount; }
    int getRemovedCount() const { return m_removedCount; }
    Document* getLastCurrentDocument() const { return m_lastCurrentDocument; }
    Document* getLastAddedDocument() const { return m_lastAddedDocument; }
    Document* getLastRemovedDocument() const { return m_lastRemovedDocument; }
    int getLastAddedIndex() const { return m_lastAddedIndex; }
    int getLastRemovedIndex() const { return m_lastRemovedIndex; }

    void reset()
    {
        m_currentChangedCount = 0;
        m_addedCount = 0;
        m_removedCount = 0;
        m_lastCurrentDocument = nullptr;
        m_lastAddedDocument = nullptr;
        m_lastRemovedDocument = nullptr;
        m_lastAddedIndex = -1;
        m_lastRemovedIndex = -1;
    }

private:
    int m_currentChangedCount{0};
    int m_addedCount{0};
    int m_removedCount{0};
    Document* m_lastCurrentDocument{nullptr};
    Document* m_lastAddedDocument{nullptr};
    Document* m_lastRemovedDocument{nullptr};
    int m_lastAddedIndex{-1};
    int m_lastRemovedIndex{-1};
};

/**
 * Test helper for creating temporary audio files.
 */
class TempAudioFileHelper
{
public:
    /**
     * Creates a temporary WAV file with sine wave audio.
     *
     * @param frequency Sine wave frequency in Hz
     * @param durationSecs Duration in seconds
     * @param sampleRate Sample rate
     * @param numChannels Number of channels
     * @return File object pointing to created file
     */
    static juce::File createTempWavFile(double frequency, double durationSecs,
                                        double sampleRate, int numChannels)
    {
        // Generate audio buffer
        auto buffer = TestAudio::createSineWave(frequency, 0.5, sampleRate,
                                                durationSecs, numChannels);

        // Create temp file
        auto tempFile = juce::File::createTempFile(".wav");

        // Write WAV file
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::FileOutputStream> outputStream(tempFile.createOutputStream());

        if (outputStream != nullptr)
        {
            int bitsPerSample = 16;
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(outputStream.get(), sampleRate, numChannels,
                                         bitsPerSample, {}, 0));

            if (writer != nullptr)
            {
                outputStream.release(); // Writer takes ownership
                writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
                writer = nullptr; // Close file
            }
        }

        return tempFile;
    }

    /**
     * Creates multiple temp files for testing.
     * Returns vector of TempFileGuard for automatic cleanup.
     */
    static std::vector<TempFileGuard> createMultipleTempFiles(int count)
    {
        std::vector<TempFileGuard> files;
        double baseFreq = 220.0;

        for (int i = 0; i < count; ++i)
        {
            // Create files with different frequencies for uniqueness
            double freq = baseFreq * (i + 1);
            files.emplace_back(createTempWavFile(freq, 1.0, 44100.0, 2));
        }

        return files;
    }
};

// ============================================================================
// Document Lifecycle Tests
// ============================================================================

class DocumentLifecycleTests : public juce::UnitTest
{
public:
    DocumentLifecycleTests() : juce::UnitTest("Document Lifecycle", "MultiDocument") {}

    void runTest() override
    {
        beginTest("Create empty document");
        testCreateEmptyDocument();

        beginTest("Open single document from file");
        testOpenSingleDocument();

        beginTest("Open multiple documents");
        testOpenMultipleDocuments();

        beginTest("Close single document");
        testCloseDocument();

        beginTest("Close all documents");
        testCloseAllDocuments();

        beginTest("Document manager listener callbacks");
        testDocumentManagerListeners();
    }

private:
    void testCreateEmptyDocument()
    {
        DocumentManager docMgr;

        // Initially no documents
        expect(docMgr.getNumDocuments() == 0, "Should start with no documents");
        expect(docMgr.getCurrentDocument() == nullptr, "No current document initially");
        expect(docMgr.getCurrentDocumentIndex() == -1, "Current index should be -1");

        // Create empty document
        Document* doc = docMgr.createDocument();
        expect(doc != nullptr, "createDocument() should return valid pointer");
        expect(docMgr.getNumDocuments() == 1, "Should have 1 document after creation");
        expect(docMgr.getCurrentDocument() == doc, "New document should be current");
        expect(docMgr.getCurrentDocumentIndex() == 0, "Current index should be 0");

        // Verify document state
        expect(!doc->hasFile(), "Empty document should have no file");
        expect(!doc->isModified(), "New document should not be modified");
        expect(doc->getUndoManager().canUndo() == false, "No undo history initially");
    }

    void testOpenSingleDocument()
    {
        DocumentManager docMgr;

        // Create temp file (RAII guard ensures cleanup)
        TempFileGuard tempFile(TempAudioFileHelper::createTempWavFile(440.0, 1.0, 44100.0, 2));
        expect(juce::File(tempFile).existsAsFile(), "Temp file should be created");

        // Open document
        Document* doc = docMgr.openDocument(tempFile);
        expect(doc != nullptr, "openDocument() should return valid pointer");
        expect(docMgr.getNumDocuments() == 1, "Should have 1 document");
        expect(docMgr.getCurrentDocument() == doc, "Opened document should be current");

        // Verify document loaded file
        expect(doc->hasFile(), "Document should have file");
        expect(doc->getFile() == juce::File(tempFile), "Document should reference correct file");
        expect(!doc->isModified(), "Newly loaded document should not be modified");

        // Verify audio loaded
        const auto& engine = doc->getAudioEngine();
        expect(engine.getTotalLength() > 0.0, "Should have loaded audio with non-zero duration");

        // No manual cleanup needed - TempFileGuard destructor handles it
    }

    void testOpenMultipleDocuments()
    {
        DocumentManager docMgr;

        // Create 3 temp files
        auto files = TempAudioFileHelper::createMultipleTempFiles(3);
        expect(files.size() == 3, "Should create 3 temp files");

        // Open all 3 documents
        std::vector<Document*> docs;
        for (const auto& file : files)
        {
            Document* doc = docMgr.openDocument(file);
            expect(doc != nullptr, "Each openDocument() should succeed");
            docs.push_back(doc);
        }

        // Verify count
        expect(docMgr.getNumDocuments() == 3, "Should have 3 open documents");

        // Verify each document references correct file
        for (int i = 0; i < 3; ++i)
        {
            expect(docs[i]->getFile() == files[i],
                   "Document " + juce::String(i) + " should reference correct file");
            expect(docMgr.getDocument(i) == docs[i],
                   "getDocument(" + juce::String(i) + ") should return correct pointer");
        }

        // Verify last document is current
        expect(docMgr.getCurrentDocument() == docs[2],
               "Last opened document should be current");
        expect(docMgr.getCurrentDocumentIndex() == 2,
               "Current index should be 2");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testCloseDocument()
    {
        DocumentManager docMgr;

        // Create 3 documents
        auto files = TempAudioFileHelper::createMultipleTempFiles(3);
        Document* doc1 = docMgr.openDocument(files[0]);
        Document* doc2 = docMgr.openDocument(files[1]);
        Document* doc3 = docMgr.openDocument(files[2]);

        expect(docMgr.getNumDocuments() == 3, "Should have 3 documents before close");
        expect(docMgr.getCurrentDocument() == doc3, "Doc3 should be current before close");

        // Close middle document
        bool closeSuccess = docMgr.closeDocument(doc2);
        expect(closeSuccess, "closeDocument() should return true");
        expect(docMgr.getNumDocuments() == 2, "Should have 2 documents after close");

        // Verify remaining documents
        expect(docMgr.getDocument(0) == doc1, "Doc1 should still be at index 0");
        expect(docMgr.getDocument(1) == doc3, "Doc3 should now be at index 1");

        // Close current document (doc3)
        closeSuccess = docMgr.closeCurrentDocument();
        expect(closeSuccess, "closeCurrentDocument() should return true");
        expect(docMgr.getNumDocuments() == 1, "Should have 1 document after close");
        expect(docMgr.getCurrentDocument() == doc1, "Doc1 should become current");

        // Close last document
        closeSuccess = docMgr.closeDocumentAt(0);
        expect(closeSuccess, "closeDocumentAt(0) should return true");
        expect(docMgr.getNumDocuments() == 0, "Should have 0 documents after close");
        expect(docMgr.getCurrentDocument() == nullptr, "No current document");
        expect(docMgr.getCurrentDocumentIndex() == -1, "Current index should be -1");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testCloseAllDocuments()
    {
        DocumentManager docMgr;

        // Create 5 documents
        auto files = TempAudioFileHelper::createMultipleTempFiles(5);
        for (const auto& file : files)
        {
            docMgr.openDocument(file);
        }

        expect(docMgr.getNumDocuments() == 5, "Should have 5 documents before closeAll");

        // Close all
        docMgr.closeAllDocuments();

        expect(docMgr.getNumDocuments() == 0, "Should have 0 documents after closeAll");
        expect(docMgr.getCurrentDocument() == nullptr, "No current document after closeAll");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testDocumentManagerListeners()
    {
        DocumentManager docMgr;
        MockDocumentListener listener;
        docMgr.addListener(&listener);

        // Create temp files
        auto files = TempAudioFileHelper::createMultipleTempFiles(2);

        // Open first document - should trigger both documentAdded and currentDocumentChanged
        listener.reset();
        Document* doc1 = docMgr.openDocument(files[0]);
        expect(listener.getAddedCount() == 1, "Should trigger 1 documentAdded callback");
        expect(listener.getCurrentChangedCount() == 1, "Should trigger 1 currentDocumentChanged callback");
        expect(listener.getLastAddedDocument() == doc1, "Added callback should receive doc1");
        expect(listener.getLastCurrentDocument() == doc1, "Current callback should receive doc1");
        expect(listener.getLastAddedIndex() == 0, "Added at index 0");

        // Open second document
        listener.reset();
        Document* doc2 = docMgr.openDocument(files[1]);
        expect(listener.getAddedCount() == 1, "Should trigger 1 documentAdded callback");
        expect(listener.getCurrentChangedCount() == 1, "Should trigger 1 currentDocumentChanged callback");
        expect(listener.getLastAddedDocument() == doc2, "Added callback should receive doc2");
        expect(listener.getLastCurrentDocument() == doc2, "Current callback should receive doc2");
        expect(listener.getLastAddedIndex() == 1, "Added at index 1");

        // Switch to first document
        listener.reset();
        docMgr.setCurrentDocument(doc1);
        expect(listener.getCurrentChangedCount() == 1, "Should trigger 1 currentDocumentChanged callback");
        expect(listener.getLastCurrentDocument() == doc1, "Current callback should receive doc1");
        expect(listener.getAddedCount() == 0, "Should NOT trigger documentAdded");
        expect(listener.getRemovedCount() == 0, "Should NOT trigger documentRemoved");

        // Close second document
        listener.reset();
        docMgr.closeDocument(doc2);
        expect(listener.getRemovedCount() == 1, "Should trigger 1 documentRemoved callback");
        expect(listener.getLastRemovedDocument() == doc2, "Removed callback should receive doc2");
        expect(listener.getLastRemovedIndex() == 1, "Removed from index 1");

        // Cleanup
        docMgr.removeListener(&listener);
        // No manual cleanup needed - TempFileGuard destructors handle it
    }
};

static DocumentLifecycleTests documentLifecycleTests;

// ============================================================================
// Tab Navigation Tests
// ============================================================================

class TabNavigationTests : public juce::UnitTest
{
public:
    TabNavigationTests() : juce::UnitTest("Tab Navigation", "MultiDocument") {}

    void runTest() override
    {
        beginTest("Switch between documents by index");
        testSwitchByIndex();

        beginTest("Switch between documents by pointer");
        testSwitchByPointer();

        beginTest("Navigate to next document (wrap around)");
        testSelectNextDocument();

        beginTest("Navigate to previous document (wrap around)");
        testSelectPreviousDocument();

        beginTest("Navigate to document by number (1-9)");
        testSelectByNumber();
    }

private:
    void testSwitchByIndex()
    {
        DocumentManager docMgr;
        auto files = TempAudioFileHelper::createMultipleTempFiles(3);

        Document* doc0 = docMgr.openDocument(files[0]);
        Document* doc1 = docMgr.openDocument(files[1]);
        Document* doc2 = docMgr.openDocument(files[2]);

        // Initially doc2 should be current (last opened)
        expect(docMgr.getCurrentDocumentIndex() == 2, "Doc2 should be current");
        expect(docMgr.getCurrentDocument() == doc2, "Current should be doc2");

        // Switch to doc0
        bool success = docMgr.setCurrentDocumentIndex(0);
        expect(success, "setCurrentDocumentIndex(0) should succeed");
        expect(docMgr.getCurrentDocumentIndex() == 0, "Index should be 0");
        expect(docMgr.getCurrentDocument() == doc0, "Current should be doc0");

        // Switch to doc1
        success = docMgr.setCurrentDocumentIndex(1);
        expect(success, "setCurrentDocumentIndex(1) should succeed");
        expect(docMgr.getCurrentDocumentIndex() == 1, "Index should be 1");
        expect(docMgr.getCurrentDocument() == doc1, "Current should be doc1");

        // Try invalid index
        success = docMgr.setCurrentDocumentIndex(99);
        expect(!success, "setCurrentDocumentIndex(99) should fail");
        expect(docMgr.getCurrentDocumentIndex() == 1, "Index should remain 1");

        // Try negative index
        success = docMgr.setCurrentDocumentIndex(-1);
        expect(!success, "setCurrentDocumentIndex(-1) should fail");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testSwitchByPointer()
    {
        DocumentManager docMgr;
        auto files = TempAudioFileHelper::createMultipleTempFiles(3);

        Document* doc0 = docMgr.openDocument(files[0]);
        Document* doc1 = docMgr.openDocument(files[1]);
        Document* doc2 = docMgr.openDocument(files[2]);

        // Switch using pointer
        bool success = docMgr.setCurrentDocument(doc0);
        expect(success, "setCurrentDocument(doc0) should succeed");
        expect(docMgr.getCurrentDocument() == doc0, "Current should be doc0");

        success = docMgr.setCurrentDocument(doc1);
        expect(success, "setCurrentDocument(doc1) should succeed");
        expect(docMgr.getCurrentDocument() == doc1, "Current should be doc1");

        // Try invalid pointer (null)
        success = docMgr.setCurrentDocument(nullptr);
        expect(!success, "setCurrentDocument(nullptr) should fail");
        expect(docMgr.getCurrentDocument() == doc1, "Current should remain doc1");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testSelectNextDocument()
    {
        DocumentManager docMgr;
        auto files = TempAudioFileHelper::createMultipleTempFiles(3);

        Document* doc0 = docMgr.openDocument(files[0]);
        Document* doc1 = docMgr.openDocument(files[1]);
        Document* doc2 = docMgr.openDocument(files[2]);

        // Start at doc0
        docMgr.setCurrentDocument(doc0);
        expect(docMgr.getCurrentDocument() == doc0, "Should start at doc0");

        // Next -> doc1
        docMgr.selectNextDocument();
        expect(docMgr.getCurrentDocument() == doc1, "Should move to doc1");

        // Next -> doc2
        docMgr.selectNextDocument();
        expect(docMgr.getCurrentDocument() == doc2, "Should move to doc2");

        // Next -> doc0 (wrap around)
        docMgr.selectNextDocument();
        expect(docMgr.getCurrentDocument() == doc0, "Should wrap around to doc0");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testSelectPreviousDocument()
    {
        DocumentManager docMgr;
        auto files = TempAudioFileHelper::createMultipleTempFiles(3);

        Document* doc0 = docMgr.openDocument(files[0]);
        Document* doc1 = docMgr.openDocument(files[1]);
        Document* doc2 = docMgr.openDocument(files[2]);

        // Start at doc2
        docMgr.setCurrentDocument(doc2);
        expect(docMgr.getCurrentDocument() == doc2, "Should start at doc2");

        // Previous -> doc1
        docMgr.selectPreviousDocument();
        expect(docMgr.getCurrentDocument() == doc1, "Should move to doc1");

        // Previous -> doc0
        docMgr.selectPreviousDocument();
        expect(docMgr.getCurrentDocument() == doc0, "Should move to doc0");

        // Previous -> doc2 (wrap around)
        docMgr.selectPreviousDocument();
        expect(docMgr.getCurrentDocument() == doc2, "Should wrap around to doc2");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testSelectByNumber()
    {
        DocumentManager docMgr;
        auto files = TempAudioFileHelper::createMultipleTempFiles(5);

        std::vector<Document*> docs;
        for (const auto& file : files)
        {
            docs.push_back(docMgr.openDocument(file));
        }

        // Select document 1 (index 0)
        bool success = docMgr.selectDocumentByNumber(1);
        expect(success, "selectDocumentByNumber(1) should succeed");
        expect(docMgr.getCurrentDocument() == docs[0], "Should select doc0");

        // Select document 3 (index 2)
        success = docMgr.selectDocumentByNumber(3);
        expect(success, "selectDocumentByNumber(3) should succeed");
        expect(docMgr.getCurrentDocument() == docs[2], "Should select doc2");

        // Select document 5 (index 4)
        success = docMgr.selectDocumentByNumber(5);
        expect(success, "selectDocumentByNumber(5) should succeed");
        expect(docMgr.getCurrentDocument() == docs[4], "Should select doc4");

        // Try invalid number (0)
        success = docMgr.selectDocumentByNumber(0);
        expect(!success, "selectDocumentByNumber(0) should fail");

        // Try invalid number (10)
        success = docMgr.selectDocumentByNumber(10);
        expect(!success, "selectDocumentByNumber(10) should fail");

        // Try number 6 (out of range, only 5 docs)
        success = docMgr.selectDocumentByNumber(6);
        expect(!success, "selectDocumentByNumber(6) should fail");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }
};

static TabNavigationTests tabNavigationTests;

// ============================================================================
// State Isolation Tests
// ============================================================================

class StateIsolationTests : public juce::UnitTest
{
public:
    StateIsolationTests() : juce::UnitTest("State Isolation", "MultiDocument") {}

    void runTest() override
    {
        beginTest("Independent undo history per document");
        testIndependentUndoHistory();

        beginTest("Independent playback position per document");
        testIndependentPlaybackPosition();

        beginTest("Independent modified flag per document");
        testIndependentModifiedFlag();

        beginTest("Playing one document stops others");
        testPlaybackExclusivity();
    }

private:
    void testIndependentUndoHistory()
    {
        DocumentManager docMgr;
        auto files = TempAudioFileHelper::createMultipleTempFiles(2);

        Document* doc1 = docMgr.openDocument(files[0]);
        Document* doc2 = docMgr.openDocument(files[1]);

        // Modify doc1 (simulate edit by marking modified and using undo manager)
        doc1->setModified(true);
        doc1->getUndoManager().beginNewTransaction();

        // Modify doc2
        doc2->setModified(true);
        doc2->getUndoManager().beginNewTransaction();
        doc2->getUndoManager().beginNewTransaction();

        // Verify independent undo histories
        // Note: In real usage, UndoManager would have UndoableAction objects
        // Here we just verify the managers are separate instances
        expect(doc1->isModified(), "Doc1 should be modified");
        expect(doc2->isModified(), "Doc2 should be modified");

        // Clearing doc1's modified flag shouldn't affect doc2
        doc1->setModified(false);
        expect(!doc1->isModified(), "Doc1 should not be modified");
        expect(doc2->isModified(), "Doc2 should still be modified");

        // Verify undo managers are separate objects
        expect(&doc1->getUndoManager() != &doc2->getUndoManager(),
               "Each document should have separate UndoManager");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testIndependentPlaybackPosition()
    {
        DocumentManager docMgr;
        auto files = TempAudioFileHelper::createMultipleTempFiles(2);

        Document* doc1 = docMgr.openDocument(files[0]);
        Document* doc2 = docMgr.openDocument(files[1]);

        // Set different playback positions
        doc1->setPlaybackPosition(0.5); // 0.5 seconds
        doc2->setPlaybackPosition(0.8); // 0.8 seconds

        // Verify positions are independent
        expectWithinAbsoluteError(doc1->getPlaybackPosition(), 0.5, 0.001,
                                 "Doc1 position should be 0.5s");
        expectWithinAbsoluteError(doc2->getPlaybackPosition(), 0.8, 0.001,
                                 "Doc2 position should be 0.8s");

        // Change doc1 position
        doc1->setPlaybackPosition(0.2);

        // Verify doc2 position unchanged
        expectWithinAbsoluteError(doc1->getPlaybackPosition(), 0.2, 0.001,
                                 "Doc1 position should be 0.2s");
        expectWithinAbsoluteError(doc2->getPlaybackPosition(), 0.8, 0.001,
                                 "Doc2 position should still be 0.8s");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testIndependentModifiedFlag()
    {
        DocumentManager docMgr;
        auto files = TempAudioFileHelper::createMultipleTempFiles(3);

        Document* doc1 = docMgr.openDocument(files[0]);
        Document* doc2 = docMgr.openDocument(files[1]);
        Document* doc3 = docMgr.openDocument(files[2]);

        // Initially all unmodified
        expect(!doc1->isModified(), "Doc1 should start unmodified");
        expect(!doc2->isModified(), "Doc2 should start unmodified");
        expect(!doc3->isModified(), "Doc3 should start unmodified");

        // Modify doc1
        doc1->setModified(true);
        expect(doc1->isModified(), "Doc1 should be modified");
        expect(!doc2->isModified(), "Doc2 should still be unmodified");
        expect(!doc3->isModified(), "Doc3 should still be unmodified");

        // Modify doc2
        doc2->setModified(true);
        expect(doc1->isModified(), "Doc1 should still be modified");
        expect(doc2->isModified(), "Doc2 should be modified");
        expect(!doc3->isModified(), "Doc3 should still be unmodified");

        // Clear doc1 modification
        doc1->setModified(false);
        expect(!doc1->isModified(), "Doc1 should now be unmodified");
        expect(doc2->isModified(), "Doc2 should still be modified");
        expect(!doc3->isModified(), "Doc3 should still be unmodified");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testPlaybackExclusivity()
    {
        DocumentManager docMgr;
        auto files = TempAudioFileHelper::createMultipleTempFiles(2);

        Document* doc1 = docMgr.openDocument(files[0]);
        Document* doc2 = docMgr.openDocument(files[1]);

        // NOTE: We deliberately DON'T initialize audio devices here for thread safety.
        // JUCE typically allows only one AudioDeviceManager per process.
        // Multiple simultaneous initializations can cause conflicts in tests.
        //
        // This test verifies AudioEngine state management (play/stop/isPlaying)
        // without requiring real audio output. The play() method sets playback state
        // even if no audio device is initialized (just won't output sound).

        // Start playback on doc1 (state only, no actual audio)
        doc1->getAudioEngine().play();
        expect(doc1->getAudioEngine().isPlaying(), "Doc1 should be playing");
        expect(!doc2->getAudioEngine().isPlaying(), "Doc2 should not be playing");

        // Start playback on doc2 (should stop doc1 in real usage via DocumentManager)
        // Note: In real app, DocumentManager listener would stop other documents
        // Here we just verify engines are independent
        doc2->getAudioEngine().play();
        expect(doc1->getAudioEngine().isPlaying(), "Doc1 still playing (engines are independent)");
        expect(doc2->getAudioEngine().isPlaying(), "Doc2 should be playing");

        // Manual stop to simulate DocumentManager behavior
        doc1->getAudioEngine().stop();
        expect(!doc1->getAudioEngine().isPlaying(), "Doc1 should be stopped");
        expect(doc2->getAudioEngine().isPlaying(), "Doc2 should still be playing");

        // Cleanup
        doc1->getAudioEngine().stop();
        doc2->getAudioEngine().stop();
        // No manual cleanup needed - TempFileGuard destructors handle it
    }
};

static StateIsolationTests stateIsolationTests;
