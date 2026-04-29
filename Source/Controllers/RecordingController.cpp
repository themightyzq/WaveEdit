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
            if (m_appendMode && m_targetDocument != nullptr)
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
                                              juce::AudioDeviceManager& audioDeviceManager)
{
    auto* currentDoc = documentManager.getCurrentDocument();
    bool appendToExisting = false;

    if (currentDoc != nullptr)
    {
        const int choice = juce::AlertWindow::showYesNoCancelBox(
            juce::AlertWindow::QuestionIcon,
            "Recording Destination",
            "A file is currently open. Where would you like to place the recording?\n\n"
            "• YES: Insert at cursor position (punch-in)\n"
            "• NO: Create new file with recording\n"
            "• CANCEL: Don't record",
            "Insert at Cursor",
            "Create New File",
            "Cancel");

        if (choice == 0) return;
        appendToExisting = (choice == 1);
    }

    // RecordingDialog takes ownership of the listener.
    RecordingDialog::showDialog(parent,
                                audioDeviceManager,
                                new RecordingApplyListener(&documentManager,
                                                           currentDoc,
                                                           appendToExisting));
}
