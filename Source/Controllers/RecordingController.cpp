/*
  ==============================================================================

    RecordingController.cpp
    Copyright (C) 2025 ZQ SFX

  ==============================================================================
*/

#include "RecordingController.h"

#include <cmath>

#include "../Utils/Document.h"
#include "../Utils/DocumentManager.h"
#include "../Utils/UndoableEdits.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioBufferManager.h"
#include "../Audio/AudioFileManager.h"
#include "ClipboardController.h"
#include "../UI/WaveformDisplay.h"
#include "../UI/RegionDisplay.h"
#include "../UI/MarkerDisplay.h"
#include "../UI/RecordingDialog.h"
#include "../UI/ErrorDialog.h"

namespace
{
    /**
     * RecordingDialog::Listener implementation that either appends recorded
     * audio at the cursor of an existing document or creates a fresh one.
     * Owned by RecordingDialog (raw new is the contract — RecordingDialog
     * deletes the listener when it closes).
     */
    class RecordingApplyListener : public RecordingDialog::Listener
    {
    public:
        RecordingApplyListener(DocumentManager* docMgr, Document* targetDoc, bool append)
            : m_documentManager(docMgr),
              m_targetDocument(targetDoc),
              m_appendMode(append)
        {
        }

        void recordingCompleted(const juce::AudioBuffer<float>& audioBuffer,
                                double sampleRate,
                                int numChannels) override
        {
            // H19: m_targetDocument is a raw pointer captured when the
            // dialog opened; the user may have closed that file mid-
            // recording. Verify it is still open via the DocumentManager
            // (getDocumentIndex returns -1 for a closed/unknown doc)
            // before touching it — otherwise we'd dereference freed
            // memory. If it's gone, fall back to creating a new document
            // so the take isn't silently lost.
            const bool targetStillOpen =
                m_appendMode
                && m_targetDocument != nullptr
                && m_documentManager != nullptr
                && m_documentManager->getDocumentIndex(m_targetDocument) >= 0;

            if (targetStillOpen)
                appendToDocument(m_targetDocument, audioBuffer, sampleRate, numChannels);
            else
                createNewDocument(audioBuffer, sampleRate, numChannels);
        }

    private:
        void appendToDocument(Document* targetDoc,
                              const juce::AudioBuffer<float>& audioBuffer,
                              double sampleRate,
                              int numChannels)
        {
            auto& waveform = targetDoc->getWaveformDisplay();
            const double insertSeconds = waveform.hasEditCursor()
                                             ? waveform.getEditCursorPosition()
                                             : waveform.getPlaybackPosition();

            juce::String error;
            if (RecordingController::insertTake(*targetDoc, audioBuffer, sampleRate,
                                                insertSeconds, error))
                return;

            // Insertion failed (e.g. an unreconcilable channel-count mismatch).
            // The document was left untouched -- tell the user why, then make
            // sure the take itself is never lost by dropping it into a new
            // document instead.
            ErrorDialog::show("Insert Recording", error, ErrorDialog::Severity::Error);
            createNewDocument(audioBuffer, sampleRate, numChannels);
        }

        void createNewDocument(const juce::AudioBuffer<float>& audioBuffer,
                               double sampleRate,
                               int /*numChannels*/)
        {
            auto* newDoc = m_documentManager->createDocument();
            if (newDoc == nullptr)
            {
                juce::Logger::writeToLog("RecordingController: failed to create new document for recording");
                return;
            }

            RecordingController::populateNewDocument(*newDoc, audioBuffer, sampleRate);
        }

        DocumentManager* m_documentManager;
        Document* m_targetDocument;
        bool m_appendMode;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RecordingApplyListener)
    };
}

void RecordingController::handleRecordCommand(juce::Component* parent,
                                              DocumentManager& documentManager,
                                              juce::AudioDeviceManager& audioDeviceManager,
                                              std::function<void(bool)> recordingStateCallback)
{
    auto* currentDoc = documentManager.getCurrentDocument();
    bool appendToExisting = false;

    if (currentDoc != nullptr)
    {
        // Clear, descriptive three-button choice. The button labels carry
        // the meaning (no "YES/NO/CANCEL" word puzzle in the body).
        const int choice = juce::AlertWindow::showYesNoCancelBox(
            juce::AlertWindow::QuestionIcon,
            "Recording Destination",
            "A file is already open. Where should the new recording go?",
            "Insert at Cursor",   // button 1 -> returns 1
            "New File",           // button 2 -> returns 2
            "Cancel");            // button 3 -> returns 0

        if (choice == 0) return;              // Cancel
        appendToExisting = (choice == 1);     // Insert at Cursor; else New File
    }

    // Track the live capture state ourselves (isRecording()), then forward
    // it to the host's own indicator callback.
    auto state = m_recordingState;
    auto trackingCallback = [state, forward = std::move(recordingStateCallback)](bool isRecording)
    {
        state->store(isRecording);
        if (forward)
            forward(isRecording);
    };

    // RecordingDialog takes ownership of the listener.
    RecordingDialog::showDialog(parent,
                                audioDeviceManager,
                                new RecordingApplyListener(&documentManager,
                                                           currentDoc,
                                                           appendToExisting),
                                std::move(trackingCallback));
}

bool RecordingController::insertTake(Document& doc,
                                     const juce::AudioBuffer<float>& take,
                                     double takeSampleRate,
                                     double insertSeconds,
                                     juce::String& error)
{
    auto& bufferManager = doc.getBufferManager();
    const double docRate = bufferManager.getSampleRate();

    // An empty document isn't really an "insert" -- treat it the same way
    // Record > New File does, so the take's own rate is recorded rather
    // than silently keeping the buffer manager's 44.1kHz default.
    if (bufferManager.getBuffer().getNumSamples() == 0)
    {
        populateNewDocument(doc, take, takeSampleRate);
        return true;
    }

    // Resample the take to the DOCUMENT's rate (not the engine's) so the
    // existing audio's playback speed is never affected by the insert.
    juce::AudioBuffer<float> resampledStorage;
    const juce::AudioBuffer<float>* takeAtDocRate = &take;
    if (std::abs(takeSampleRate - docRate) > 0.01)
    {
        resampledStorage = AudioFileManager::resampleBuffer(take, takeSampleRate, docRate);
        takeAtDocRate = &resampledStorage;
    }

    // Conform channel count. Only mono<->multichannel conversions are
    // deterministic (see ClipboardController::conformChannels); anything
    // else is rejected without touching the document.
    juce::AudioBuffer<float> conformedTake;
    if (!ClipboardController::conformChannels(*takeAtDocRate, bufferManager.getNumChannels(), conformedTake))
    {
        error = juce::String::formatted(
            "Can't insert a %d-channel take into a %d-channel file.",
            takeAtDocRate->getNumChannels(), bufferManager.getNumChannels());
        return false;
    }

    const int64_t numSamples = bufferManager.getNumSamples();
    int64_t insertSample = (int64_t) std::llround(insertSeconds * docRate);
    insertSample = juce::jlimit((int64_t) 0, numSamples, insertSample);

    // Applied as one undoable step, exactly like the Generate feature
    // (DSPController_Advanced.cpp): InsertAction handles the buffer splice,
    // region/marker shifting, and engine/waveform reload.
    doc.getUndoManager().beginNewTransaction("Insert Recording");
    doc.getUndoManager().perform(new InsertAction(
        bufferManager, doc.getAudioEngine(), doc.getWaveformDisplay(),
        insertSample, conformedTake,
        &doc.getRegionManager(), &doc.getRegionDisplay(),
        &doc.getMarkerManager(), &doc.getMarkerDisplay()));
    doc.setModified(true);

    return true;
}

void RecordingController::populateNewDocument(Document& doc,
                                              const juce::AudioBuffer<float>& take,
                                              double sampleRate)
{
    // setBuffer() (not getMutableBuffer()) records the take's own sample
    // rate on the AudioBufferManager -- otherwise it silently keeps its
    // 44.1kHz default and later edits/saves use the wrong rate.
    doc.getBufferManager().setBuffer(take, sampleRate);
    const auto& buffer = doc.getBufferManager().getBuffer();

    doc.getAudioEngine().loadFromBuffer(buffer, sampleRate, buffer.getNumChannels());
    doc.getWaveformDisplay().reloadFromBuffer(buffer, sampleRate, false, false);

    const double durationSeconds = buffer.getNumSamples() / sampleRate;

    doc.getRegionDisplay().setSampleRate(sampleRate);
    doc.getRegionDisplay().setTotalDuration(durationSeconds);
    doc.getRegionDisplay().setVisibleRange(0.0, durationSeconds);
    doc.getRegionDisplay().setAudioBuffer(&buffer);

    doc.getMarkerDisplay().setSampleRate(sampleRate);
    doc.getMarkerDisplay().setTotalDuration(durationSeconds);

    doc.setModified(true);
}
