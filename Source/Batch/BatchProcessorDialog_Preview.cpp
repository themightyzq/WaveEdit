/*
  ==============================================================================

    BatchProcessorDialog_Preview.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Split out from BatchProcessorDialog.cpp under CLAUDE.md §7.5
    (file-size cap). Hosts the audio preview surface — Preview button
    handler, transport playback wiring (loadPreviewBuffer-style), and
    the stop / cleanup teardown.

  ==============================================================================
*/

#include "BatchProcessorDialog.h"
#include "BatchDSPOps.h"

namespace waveedit
{

// =============================================================================
// Audio Preview
// =============================================================================

void BatchProcessorDialog::onPreviewClicked()
{
    if (m_isPreviewing)
    {
        stopPreview();
        return;
    }

    // Get selected file from list
    int selectedRow = m_fileListBox.getSelectedRow();
    if (selectedRow < 0 || selectedRow >= static_cast<int>(m_fileInfos.size()))
    {
        // If nothing selected, use first file
        if (m_fileInfos.empty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "No Files",
                "Add files to preview the DSP chain."
            );
            return;
        }
        selectedRow = 0;
    }

    juce::File audioFile(m_fileInfos[static_cast<size_t>(selectedRow)].fullPath);
    if (!audioFile.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "File Not Found",
            "The selected file no longer exists."
        );
        return;
    }

    startPreviewPlayback(audioFile);
}

void BatchProcessorDialog::startPreviewPlayback(const juce::File& audioFile)
{
    // Clean up any existing preview
    cleanupPreview();

    // Read the file and run the DSP chain through the same code batch
    // processing uses (BatchDSPOps), so the preview plays exactly what will
    // be written: curved fades, Graphical EQ, Reverse and Invert included.
    // Only the first 30 s is read unless the chain needs the whole file
    // (normalize, DC offset, fade out, reverse) to match the output.
    juce::AudioBuffer<float> rendered;
    double sampleRate = 0.0;
    if (!BatchDSPOps::renderPreview(audioFile, m_dspChainPanel->getDSPChain(), rendered, sampleRate))
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Cannot Read File",
            "Failed to load audio file for preview."
        );
        return;
    }

    const int numChannels = rendered.getNumChannels();
    m_previewBuffer = std::make_unique<juce::AudioBuffer<float>>(std::move(rendered));

    // Create audio source from buffer
    m_previewMemorySource = std::make_unique<juce::MemoryAudioSource>(
        *m_previewBuffer, false, false);

    m_previewTransport = std::make_unique<juce::AudioTransportSource>();
    m_previewTransport->setSource(m_previewMemorySource.get(), 0, nullptr, sampleRate, numChannels);

    m_previewSourcePlayer.setSource(m_previewTransport.get());

    // Start playback
    m_previewTransport->start();

    m_isPreviewing = true;
    m_previewButton.setButtonText("Stop");

    m_statusLabel.setText("Previewing: " + audioFile.getFileName(), juce::dontSendNotification);
}

void BatchProcessorDialog::stopPreview()
{
    if (m_previewTransport)
    {
        m_previewTransport->stop();
    }

    m_isPreviewing = false;
    m_previewButton.setButtonText("Preview");
    m_statusLabel.setText("Ready", juce::dontSendNotification);
}

void BatchProcessorDialog::cleanupPreview()
{
    m_previewSourcePlayer.setSource(nullptr);

    if (m_previewTransport)
    {
        m_previewTransport->setSource(nullptr);
        m_previewTransport.reset();
    }

    m_previewMemorySource.reset();
    m_previewBuffer.reset();
}

} // namespace waveedit
