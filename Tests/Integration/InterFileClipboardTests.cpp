/*
  ==============================================================================

    InterFileClipboardTests.cpp
    Created: 2025-10-15
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Comprehensive integration tests for inter-file clipboard operations.
    Tests copying audio from one document and pasting into another,
    including sample rate conversion scenarios.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Utils/DocumentManager.h"
#include "Utils/Document.h"
#include "Audio/AudioBufferManager.h"
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
 * Helper for creating temporary audio files with specific characteristics.
 */
class InterFileTestHelper
{
public:
    /**
     * Creates a temp WAV file with specified sample rate.
     */
    static juce::File createTempFile(double sampleRate, int numChannels = 2)
    {
        auto buffer = TestAudio::createSineWave(440.0, 0.5, sampleRate, 2.0, numChannels);
        auto tempFile = juce::File::createTempFile(".wav");

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::FileOutputStream> outputStream(tempFile.createOutputStream());

        if (outputStream != nullptr)
        {
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(outputStream.get(), sampleRate, numChannels, 16, {}, 0));

            if (writer != nullptr)
            {
                outputStream.release();
                writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
            }
        }

        return tempFile;
    }

    /**
     * Verifies that a buffer contains sine wave audio.
     * Returns true if detected sine wave characteristics.
     */
    static bool verifySineWavePresent(const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
            return false;

        // Check for non-silence (RMS > threshold)
        float rms = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                rms += data[i] * data[i];
            }
        }
        rms = std::sqrt(rms / (buffer.getNumSamples() * buffer.getNumChannels()));

        return rms > 0.1f; // Reasonable threshold for 0.5 amplitude sine
    }
};

// ============================================================================
// Basic Inter-File Clipboard Tests
// ============================================================================

class BasicInterFileClipboardTests : public juce::UnitTest
{
public:
    BasicInterFileClipboardTests() : juce::UnitTest("Basic Inter-File Clipboard", "InterFileClipboard") {}

    void runTest() override
    {
        beginTest("Copy and paste between documents");
        testBasicCopyPaste();

        beginTest("Clipboard has data after copy");
        testClipboardHasData();

        beginTest("Paste into empty document");
        testPasteIntoEmpty();

        beginTest("Multiple copy operations (clipboard overwrite)");
        testMultipleCopy();

        beginTest("Paste without prior copy (should fail gracefully)");
        testPasteWithoutCopy();
    }

private:
    void testBasicCopyPaste()
    {
        DocumentManager docMgr;

        // Create two documents with different sample rates (RAII guards ensure cleanup)
        TempFileGuard file1(InterFileTestHelper::createTempFile(44100.0, 2));
        TempFileGuard file2(InterFileTestHelper::createTempFile(48000.0, 2));

        Document* doc1 = docMgr.openDocument(file1);
        Document* doc2 = docMgr.openDocument(file2);

        expect(doc1 != nullptr, "Doc1 should load successfully");
        expect(doc2 != nullptr, "Doc2 should load successfully");

        // Get audio from doc1
        const auto& bufferMgr1 = doc1->getBufferManager();
        const auto& originalBuffer = bufferMgr1.getBuffer();
        double originalSampleRate = bufferMgr1.getSampleRate();

        expect(originalBuffer.getNumSamples() > 0, "Doc1 should have audio");

        // Copy to inter-file clipboard
        docMgr.copyToInterFileClipboard(originalBuffer, originalSampleRate);

        // Verify clipboard has data
        expect(docMgr.hasInterFileClipboard(), "Clipboard should have data after copy");

        double clipboardDuration = docMgr.getInterFileClipboardDuration();
        expect(clipboardDuration > 0.0, "Clipboard should have non-zero duration");

        // Paste into doc2 at position 0.5s
        bool pasteSuccess = docMgr.pasteFromInterFileClipboard(doc2, 0.5);
        expect(pasteSuccess, "Paste operation should succeed");

        // Verify doc2 is modified
        expect(doc2->isModified(), "Doc2 should be marked as modified after paste");

        // Cleanup
        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testClipboardHasData()
    {
        DocumentManager docMgr;

        // Initially clipboard should be empty
        expect(!docMgr.hasInterFileClipboard(), "Clipboard should start empty");
        expectWithinAbsoluteError(docMgr.getInterFileClipboardDuration(), 0.0, 0.001,
                                 "Clipboard duration should be 0");

        // Create and load document (RAII guard ensures cleanup)
        TempFileGuard file(InterFileTestHelper::createTempFile(44100.0, 2));
        Document* doc = docMgr.openDocument(file);

        const auto& buffer = doc->getBufferManager().getBuffer();
        double sampleRate = doc->getBufferManager().getSampleRate();

        // Copy to clipboard
        docMgr.copyToInterFileClipboard(buffer, sampleRate);

        // Verify clipboard state
        expect(docMgr.hasInterFileClipboard(), "Clipboard should have data");

        double expectedDuration = buffer.getNumSamples() / sampleRate;
        expectWithinAbsoluteError(docMgr.getInterFileClipboardDuration(), expectedDuration, 0.01,
                                 "Clipboard duration should match buffer duration");

        // No manual cleanup needed - TempFileGuard destructor handles it
    }

    void testPasteIntoEmpty()
    {
        DocumentManager docMgr;

        // Create source document with audio (RAII guard ensures cleanup)
        TempFileGuard file(InterFileTestHelper::createTempFile(44100.0, 2));
        Document* sourceDoc = docMgr.openDocument(file);

        // Copy audio to clipboard
        const auto& sourceBuffer = sourceDoc->getBufferManager().getBuffer();
        double sourceSampleRate = sourceDoc->getBufferManager().getSampleRate();
        docMgr.copyToInterFileClipboard(sourceBuffer, sourceSampleRate);

        // Create empty target document
        Document* targetDoc = docMgr.createDocument();
        expect(targetDoc != nullptr, "Should create empty document");

        // Note: Pasting into empty document requires buffer initialization first
        // In real app, this would fail or require special handling
        // For now, verify the operation returns false (expected behavior)
        bool pasteSuccess = docMgr.pasteFromInterFileClipboard(targetDoc, 0.0);

        // This should fail gracefully because empty document has no buffer initialized
        // Real app would show error: "Cannot paste into empty document"
        expect(!pasteSuccess, "Paste into uninitialized document should fail gracefully");

        // No manual cleanup needed - TempFileGuard destructor handles it
    }

    void testMultipleCopy()
    {
        DocumentManager docMgr;

        // Create two source documents (RAII guards ensure cleanup)
        TempFileGuard file1(InterFileTestHelper::createTempFile(44100.0, 2));
        TempFileGuard file2(InterFileTestHelper::createTempFile(48000.0, 2));

        Document* doc1 = docMgr.openDocument(file1);
        Document* doc2 = docMgr.openDocument(file2);

        // Copy from doc1
        const auto& buffer1 = doc1->getBufferManager().getBuffer();
        docMgr.copyToInterFileClipboard(buffer1, 44100.0);

        double duration1 = docMgr.getInterFileClipboardDuration();
        expect(duration1 > 0.0, "First copy should populate clipboard");

        // Copy from doc2 (should overwrite)
        const auto& buffer2 = doc2->getBufferManager().getBuffer();
        docMgr.copyToInterFileClipboard(buffer2, 48000.0);

        double duration2 = docMgr.getInterFileClipboardDuration();
        expect(duration2 > 0.0, "Second copy should replace clipboard content");

        // Clipboard should have doc2's content now
        expect(docMgr.hasInterFileClipboard(), "Clipboard should still have data");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testPasteWithoutCopy()
    {
        DocumentManager docMgr;

        // Create target document (RAII guard ensures cleanup)
        TempFileGuard file(InterFileTestHelper::createTempFile(44100.0, 2));
        Document* doc = docMgr.openDocument(file);

        // Verify clipboard is empty
        expect(!docMgr.hasInterFileClipboard(), "Clipboard should start empty");

        // Attempt to paste (should fail gracefully)
        bool pasteSuccess = docMgr.pasteFromInterFileClipboard(doc, 0.0);
        expect(!pasteSuccess, "Paste without clipboard data should fail");

        // Document should NOT be modified
        expect(!doc->isModified(), "Document should not be modified after failed paste");

        // No manual cleanup needed - TempFileGuard destructor handles it
    }
};

static BasicInterFileClipboardTests basicInterFileClipboardTests;

// ============================================================================
// Sample Rate Conversion Tests
// ============================================================================

class SampleRateConversionTests : public juce::UnitTest
{
public:
    SampleRateConversionTests() : juce::UnitTest("Sample Rate Conversion", "InterFileClipboard") {}

    void runTest() override
    {
        beginTest("Copy 44.1kHz and paste into 48kHz document");
        testCopy44To48();

        beginTest("Copy 48kHz and paste into 44.1kHz document");
        testCopy48To44();

        beginTest("Copy 96kHz and paste into 44.1kHz document");
        testCopy96To44();

        beginTest("Same sample rate (no conversion needed)");
        testSameSampleRate();
    }

private:
    void testCopy44To48()
    {
        DocumentManager docMgr;

        TempFileGuard file44k(InterFileTestHelper::createTempFile(44100.0, 2));
        TempFileGuard file48k(InterFileTestHelper::createTempFile(48000.0, 2));

        Document* doc44k = docMgr.openDocument(file44k);
        Document* doc48k = docMgr.openDocument(file48k);

        // Copy from 44.1kHz document
        const auto& buffer44k = doc44k->getBufferManager().getBuffer();
        docMgr.copyToInterFileClipboard(buffer44k, 44100.0);

        // Paste into 48kHz document
        int originalNumSamples48k = doc48k->getBufferManager().getBuffer().getNumSamples();

        bool pasteSuccess = docMgr.pasteFromInterFileClipboard(doc48k, 0.5);
        expect(pasteSuccess, "Paste with sample rate conversion should succeed");

        // Verify document was modified
        expect(doc48k->isModified(), "Target document should be modified");

        // Verify audio was inserted (buffer grew)
        int newNumSamples48k = doc48k->getBufferManager().getBuffer().getNumSamples();
        expect(newNumSamples48k > originalNumSamples48k,
               "Buffer should grow after paste");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testCopy48To44()
    {
        DocumentManager docMgr;

        TempFileGuard file48k(InterFileTestHelper::createTempFile(48000.0, 2));
        TempFileGuard file44k(InterFileTestHelper::createTempFile(44100.0, 2));

        Document* doc48k = docMgr.openDocument(file48k);
        Document* doc44k = docMgr.openDocument(file44k);

        // Copy from 48kHz document
        const auto& buffer48k = doc48k->getBufferManager().getBuffer();
        docMgr.copyToInterFileClipboard(buffer48k, 48000.0);

        // Paste into 44.1kHz document
        int originalNumSamples44k = doc44k->getBufferManager().getBuffer().getNumSamples();

        bool pasteSuccess = docMgr.pasteFromInterFileClipboard(doc44k, 0.5);
        expect(pasteSuccess, "Paste with sample rate conversion should succeed");

        expect(doc44k->isModified(), "Target document should be modified");

        int newNumSamples44k = doc44k->getBufferManager().getBuffer().getNumSamples();
        expect(newNumSamples44k > originalNumSamples44k,
               "Buffer should grow after paste");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testCopy96To44()
    {
        DocumentManager docMgr;

        TempFileGuard file96k(InterFileTestHelper::createTempFile(96000.0, 2));
        TempFileGuard file44k(InterFileTestHelper::createTempFile(44100.0, 2));

        Document* doc96k = docMgr.openDocument(file96k);
        Document* doc44k = docMgr.openDocument(file44k);

        // Copy from 96kHz document
        const auto& buffer96k = doc96k->getBufferManager().getBuffer();
        docMgr.copyToInterFileClipboard(buffer96k, 96000.0);

        // Paste into 44.1kHz document (major downsampling)
        bool pasteSuccess = docMgr.pasteFromInterFileClipboard(doc44k, 0.5);
        expect(pasteSuccess, "Paste with large sample rate conversion should succeed");

        expect(doc44k->isModified(), "Target document should be modified");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testSameSampleRate()
    {
        DocumentManager docMgr;

        TempFileGuard file1(InterFileTestHelper::createTempFile(44100.0, 2));
        TempFileGuard file2(InterFileTestHelper::createTempFile(44100.0, 2));

        Document* doc1 = docMgr.openDocument(file1);
        Document* doc2 = docMgr.openDocument(file2);

        // Copy from doc1
        const auto& buffer1 = doc1->getBufferManager().getBuffer();
        docMgr.copyToInterFileClipboard(buffer1, 44100.0);

        // Paste into doc2 (no conversion needed)
        int originalNumSamples = doc2->getBufferManager().getBuffer().getNumSamples();

        bool pasteSuccess = docMgr.pasteFromInterFileClipboard(doc2, 0.5);
        expect(pasteSuccess, "Paste with same sample rate should succeed");

        expect(doc2->isModified(), "Target document should be modified");

        int newNumSamples = doc2->getBufferManager().getBuffer().getNumSamples();
        expect(newNumSamples > originalNumSamples, "Buffer should grow after paste");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }
};

static SampleRateConversionTests sampleRateConversionTests;

// ============================================================================
// Workflow Integration Tests
// ============================================================================

class ClipboardWorkflowTests : public juce::UnitTest
{
public:
    ClipboardWorkflowTests() : juce::UnitTest("Clipboard Workflows", "InterFileClipboard") {}

    void runTest() override
    {
        beginTest("Copy from Doc1 -> Paste into Doc2 -> Paste into Doc3");
        testMultipleDocumentPaste();

        beginTest("Copy selection, switch tabs, paste, verify result");
        testCopyTabSwitchPaste();

        beginTest("Copy from mono, paste into stereo");
        testMonoToStereo();

        beginTest("Copy from stereo, paste into mono");
        testStereoToMono();
    }

private:
    void testMultipleDocumentPaste()
    {
        DocumentManager docMgr;

        // Create 3 documents (RAII guards ensure cleanup)
        TempFileGuard file1(InterFileTestHelper::createTempFile(44100.0, 2));
        TempFileGuard file2(InterFileTestHelper::createTempFile(44100.0, 2));
        TempFileGuard file3(InterFileTestHelper::createTempFile(44100.0, 2));

        Document* doc1 = docMgr.openDocument(file1);
        Document* doc2 = docMgr.openDocument(file2);
        Document* doc3 = docMgr.openDocument(file3);

        // Copy from doc1
        const auto& buffer1 = doc1->getBufferManager().getBuffer();
        docMgr.copyToInterFileClipboard(buffer1, 44100.0);

        // Paste into doc2
        bool pasteSuccess = docMgr.pasteFromInterFileClipboard(doc2, 0.0);
        expect(pasteSuccess, "Paste into doc2 should succeed");
        expect(doc2->isModified(), "Doc2 should be modified");

        // Paste same clipboard into doc3
        pasteSuccess = docMgr.pasteFromInterFileClipboard(doc3, 0.0);
        expect(pasteSuccess, "Paste into doc3 should succeed");
        expect(doc3->isModified(), "Doc3 should be modified");

        // Doc1 should remain unmodified (source)
        expect(!doc1->isModified(), "Doc1 (source) should not be modified");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testCopyTabSwitchPaste()
    {
        DocumentManager docMgr;

        TempFileGuard file1(InterFileTestHelper::createTempFile(44100.0, 2));
        TempFileGuard file2(InterFileTestHelper::createTempFile(44100.0, 2));

        Document* doc1 = docMgr.openDocument(file1);
        Document* doc2 = docMgr.openDocument(file2);

        // Set doc1 as current
        docMgr.setCurrentDocument(doc1);
        expect(docMgr.getCurrentDocument() == doc1, "Doc1 should be current");

        // Copy from doc1
        const auto& buffer1 = doc1->getBufferManager().getBuffer();
        docMgr.copyToInterFileClipboard(buffer1, 44100.0);

        // Switch to doc2
        docMgr.setCurrentDocument(doc2);
        expect(docMgr.getCurrentDocument() == doc2, "Doc2 should be current");

        // Paste into doc2
        bool pasteSuccess = docMgr.pasteFromInterFileClipboard(doc2, 0.5);
        expect(pasteSuccess, "Paste after tab switch should succeed");
        expect(doc2->isModified(), "Doc2 should be modified after paste");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testMonoToStereo()
    {
        DocumentManager docMgr;

        TempFileGuard fileMono(InterFileTestHelper::createTempFile(44100.0, 1));
        TempFileGuard fileStereo(InterFileTestHelper::createTempFile(44100.0, 2));

        Document* docMono = docMgr.openDocument(fileMono);
        Document* docStereo = docMgr.openDocument(fileStereo);

        expect(docMono->getBufferManager().getNumChannels() == 1, "Source should be mono");
        expect(docStereo->getBufferManager().getNumChannels() == 2, "Target should be stereo");

        // Copy from mono
        const auto& bufferMono = docMono->getBufferManager().getBuffer();
        docMgr.copyToInterFileClipboard(bufferMono, 44100.0);

        // Paste into stereo (should duplicate to both channels)
        bool pasteSuccess = docMgr.pasteFromInterFileClipboard(docStereo, 0.5);
        expect(pasteSuccess, "Paste mono into stereo should succeed");
        expect(docStereo->isModified(), "Stereo document should be modified");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }

    void testStereoToMono()
    {
        DocumentManager docMgr;

        TempFileGuard fileStereo(InterFileTestHelper::createTempFile(44100.0, 2));
        TempFileGuard fileMono(InterFileTestHelper::createTempFile(44100.0, 1));

        Document* docStereo = docMgr.openDocument(fileStereo);
        Document* docMono = docMgr.openDocument(fileMono);

        expect(docStereo->getBufferManager().getNumChannels() == 2, "Source should be stereo");
        expect(docMono->getBufferManager().getNumChannels() == 1, "Target should be mono");

        // Copy from stereo
        const auto& bufferStereo = docStereo->getBufferManager().getBuffer();
        docMgr.copyToInterFileClipboard(bufferStereo, 44100.0);

        // Paste into mono (should mix down or use left channel)
        bool pasteSuccess = docMgr.pasteFromInterFileClipboard(docMono, 0.5);
        expect(pasteSuccess, "Paste stereo into mono should succeed");
        expect(docMono->isModified(), "Mono document should be modified");

        // No manual cleanup needed - TempFileGuard destructors handle it
    }
};

static ClipboardWorkflowTests clipboardWorkflowTests;
