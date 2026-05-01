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

    juce::File audioFile(m_fileInfos[selectedRow].fullPath);
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

    // Load the audio file
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(audioFile));

    if (!reader)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Cannot Read File",
            "Failed to load audio file for preview."
        );
        return;
    }

    // Read the entire file into a buffer
    int numChannels = static_cast<int>(reader->numChannels);
    juce::int64 numSamples = reader->lengthInSamples;
    double sampleRate = reader->sampleRate;

    // Limit preview to first 30 seconds to avoid memory issues
    const juce::int64 maxSamples = static_cast<juce::int64>(30.0 * sampleRate);
    if (numSamples > maxSamples)
        numSamples = maxSamples;

    m_previewBuffer = std::make_unique<juce::AudioBuffer<float>>(
        numChannels, static_cast<int>(numSamples));

    reader->read(m_previewBuffer.get(), 0, static_cast<int>(numSamples), 0, true, true);

    // Apply DSP chain
    auto dspChain = m_dspChainPanel->getDSPChain();
    for (const auto& dsp : dspChain)
    {
        if (!dsp.enabled)
            continue;

        switch (dsp.operation)
        {
            case BatchDSPOperation::GAIN:
            {
                float gainLinear = juce::Decibels::decibelsToGain(dsp.gainDb);
                m_previewBuffer->applyGain(gainLinear);
                break;
            }

            case BatchDSPOperation::NORMALIZE:
            {
                float peak = m_previewBuffer->getMagnitude(0, m_previewBuffer->getNumSamples());
                if (peak > 0.0f)
                {
                    float targetLinear = juce::Decibels::decibelsToGain(dsp.normalizeTargetDb);
                    float gainToApply = targetLinear / peak;
                    m_previewBuffer->applyGain(gainToApply);
                }
                break;
            }

            case BatchDSPOperation::DC_OFFSET:
            {
                for (int ch = 0; ch < m_previewBuffer->getNumChannels(); ++ch)
                {
                    float* data = m_previewBuffer->getWritePointer(ch);
                    int samples = m_previewBuffer->getNumSamples();

                    // Calculate DC offset
                    double sum = 0.0;
                    for (int i = 0; i < samples; ++i)
                        sum += data[i];
                    float dcOffset = static_cast<float>(sum / samples);

                    // Remove it
                    for (int i = 0; i < samples; ++i)
                        data[i] -= dcOffset;
                }
                break;
            }

            case BatchDSPOperation::FADE_IN:
            {
                int fadeSamples = static_cast<int>(dsp.fadeDurationMs * sampleRate / 1000.0);
                fadeSamples = juce::jmin(fadeSamples, m_previewBuffer->getNumSamples());

                for (int ch = 0; ch < m_previewBuffer->getNumChannels(); ++ch)
                {
                    float* data = m_previewBuffer->getWritePointer(ch);
                    for (int i = 0; i < fadeSamples; ++i)
                    {
                        float t = static_cast<float>(i) / static_cast<float>(fadeSamples);
                        // Simple linear fade
                        data[i] *= t;
                    }
                }
                break;
            }

            case BatchDSPOperation::FADE_OUT:
            {
                int fadeSamples = static_cast<int>(dsp.fadeDurationMs * sampleRate / 1000.0);
                int startSample = m_previewBuffer->getNumSamples() - fadeSamples;
                startSample = juce::jmax(0, startSample);
                fadeSamples = m_previewBuffer->getNumSamples() - startSample;

                for (int ch = 0; ch < m_previewBuffer->getNumChannels(); ++ch)
                {
                    float* data = m_previewBuffer->getWritePointer(ch);
                    for (int i = 0; i < fadeSamples; ++i)
                    {
                        float t = 1.0f - (static_cast<float>(i) / static_cast<float>(fadeSamples));
                        data[startSample + i] *= t;
                    }
                }
                break;
            }

            default:
                break;
        }
    }

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
