/*
  ==============================================================================

    RecordingController.cpp
    Copyright (C) 2025 ZQ SFX

  ==============================================================================
*/

#include "RecordingController.h"

#include "../Utils/Document.h"
#include "../Utils/DocumentManager.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioBufferManager.h"
#include "../UI/WaveformDisplay.h"
#include "../UI/RegionDisplay.h"
#include "../UI/MarkerDisplay.h"
#include "../UI/RecordingDialog.h"

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
                              int /*numChannels*/)
        {
            const double cursorSeconds = targetDoc->getWaveformDisplay().getPlaybackPosition();
            auto& currentBuffer = targetDoc->getBufferManager().getMutableBuffer();
            const double currentSampleRate = targetDoc->getAudioEngine().getSampleRate();

            int insertPositionSamples = static_cast<int>(cursorSeconds * currentSampleRate);
            insertPositionSamples = juce::jlimit(0, currentBuffer.getNumSamples(), insertPositionSamples);

            const int currentSamples = currentBuffer.getNumSamples();
            const int newSamples     = audioBuffer.getNumSamples();
            const int totalSamples   = currentSamples + newSamples;

            juce::AudioBuffer<float> combined(
                juce::jmax(currentBuffer.getNumChannels(), audioBuffer.getNumChannels()),
                totalSamples);

            for (int ch = 0; ch < currentBuffer.getNumChannels(); ++ch)
                combined.copyFrom(ch, 0, currentBuffer, ch, 0, insertPositionSamples);

            for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
                combined.copyFrom(ch, insertPositionSamples, audioBuffer, ch, 0, newSamples);

            const int remaining = currentSamples - insertPositionSamples;
            if (remaining > 0)
            {
                for (int ch = 0; ch < currentBuffer.getNumChannels(); ++ch)
                {
                    combined.copyFrom(ch, insertPositionSamples + newSamples,
                                      currentBuffer, ch,
                                      insertPositionSamples, remaining);
                }
            }

            currentBuffer.makeCopyOf(combined, true);
            targetDoc->getAudioEngine().loadFromBuffer(combined, sampleRate,
                                                       combined.getNumChannels());
            targetDoc->getWaveformDisplay().reloadFromBuffer(combined, sampleRate, false, false);
            targetDoc->getRegionDisplay().setTotalDuration(totalSamples / sampleRate);
            targetDoc->getMarkerDisplay().setTotalDuration(totalSamples / sampleRate);
            targetDoc->setModified(true);
        }

        void createNewDocument(const juce::AudioBuffer<float>& audioBuffer,
                               double sampleRate,
                               int numChannels)
        {
            auto* newDoc = m_documentManager->createDocument();
            if (newDoc == nullptr)
            {
                juce::Logger::writeToLog("RecordingController: failed to create new document for recording");
                return;
            }

            auto& buffer = newDoc->getBufferManager().getMutableBuffer();
            buffer.setSize(audioBuffer.getNumChannels(), audioBuffer.getNumSamples());
            buffer.makeCopyOf(audioBuffer, true);

            newDoc->getAudioEngine().loadFromBuffer(audioBuffer, sampleRate, numChannels);
            newDoc->getWaveformDisplay().reloadFromBuffer(audioBuffer, sampleRate, false, false);

            const double durationSeconds = audioBuffer.getNumSamples() / sampleRate;

            newDoc->getRegionDisplay().setSampleRate(sampleRate);
            newDoc->getRegionDisplay().setTotalDuration(durationSeconds);
            newDoc->getRegionDisplay().setVisibleRange(0.0, durationSeconds);
            newDoc->getRegionDisplay().setAudioBuffer(&buffer);

            newDoc->getMarkerDisplay().setSampleRate(sampleRate);
            newDoc->getMarkerDisplay().setTotalDuration(durationSeconds);

            newDoc->setModified(true);
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

    // RecordingDialog takes ownership of the listener.
    RecordingDialog::showDialog(parent,
                                audioDeviceManager,
                                new RecordingApplyListener(&documentManager,
                                                           currentDoc,
                                                           appendToExisting),
                                std::move(recordingStateCallback));
}
