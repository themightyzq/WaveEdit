/*
  ==============================================================================

    DSPController_Advanced.cpp
    Part of WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Split out from DSPController.cpp under CLAUDE.md §7.5 (file size cap).
    Hosts the heavier "advanced" DSP operations: EQ dialogs, channel
    converter / extractor, plugin chain + offline plugin application,
    head & tail processing, looping tools, resample, and time-stretch /
    pitch-shift. Simple level operations (gain, normalize, fade,
    DC offset, silence/reverse/invert/trim) stay in DSPController.cpp.

  ==============================================================================
*/

#include "DSPController.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioFileManager.h"
#include "../Audio/AudioBufferManager.h"
#include "../Audio/AudioProcessor.h"
#include "../Audio/ChannelLayout.h"
#include "../Utils/UndoActions/AudioUndoActions.h"
#include "../Utils/UndoActions/PluginUndoActions.h"
#include "../Utils/UndoActions/ChannelUndoActions.h"
#include "../DSP/TimePitchEngine.h"
#include "../Utils/UndoableEdits.h"
#include "../UI/ParametricEQDialog.h"
#include "../UI/GraphicalEQEditor.h"
#include "../UI/OfflinePluginDialog.h"
#include "../UI/ProgressDialog.h"
#include "../UI/ChannelConverterDialog.h"
#include "../UI/ChannelExtractorDialog.h"
#include "../UI/ErrorDialog.h"
#include "../DSP/HeadTailEngine.h"
#include "../UI/HeadTailDialog.h"
#include "../UI/LoopingToolsDialog.h"
#include "../UI/ThemeManager.h"
#include "../Plugins/PluginManager.h"
#include "../Plugins/PluginChainRenderer.h"

//==============================================================================
// EQ dialogs
//==============================================================================

void DSPController::showParametricEQDialog(Document* doc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    ParametricEQDialog dialog(&doc->getAudioEngine(), &doc->getBufferManager());

    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(&dialog);
    options.componentToCentreAround = parent;
    options.dialogTitle = "Parametric EQ";
    options.dialogBackgroundColour = waveedit::ThemeManager::getInstance().getCurrent().panel;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;

    options.runModal();
}

void DSPController::showGraphicalEQDialog(Document* doc, juce::Component* /*parent*/)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    auto& waveform = doc->getWaveformDisplay();
    auto& engine = doc->getAudioEngine();
    const bool hasSelection = waveform.hasSelection();
    const double sampleRate = engine.getSampleRate();

    const int64_t startSample = hasSelection
        ? static_cast<int64_t>(waveform.getSelectionStart() * sampleRate)
        : 0;
    const int64_t endSample = hasSelection
        ? static_cast<int64_t>(waveform.getSelectionEnd() * sampleRate)
        : static_cast<int64_t>(engine.getTotalLength() * sampleRate);
    const int64_t numSamples = endSample - startSample;

    auto eqParams = DynamicParametricEQ::createDefaultPreset();
    auto result = GraphicalEQEditor::showDialog(&engine, eqParams, startSample, endSample);
    if (!result.has_value())
        return;

    try
    {
        doc->getUndoManager().beginNewTransaction("Graphical EQ");
        doc->getUndoManager().perform(new ApplyDynamicParametricEQAction(
            doc->getBufferManager(),
            doc->getAudioEngine(),
            doc->getWaveformDisplay(),
            startSample,
            numSamples,
            result.value()));
        doc->setModified(true);
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("DSPController::showGraphicalEQDialog - "
                                 + juce::String(e.what()));
        ErrorDialog::show("Error",
                          "Graphical EQ failed: " + juce::String(e.what()));
    }
}

//==============================================================================
// Channel converter / extractor
//==============================================================================

void DSPController::showChannelConverterDialog(Document* doc, juce::Component* /*parent*/)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    const int currentChannels = doc->getBufferManager().getBuffer().getNumChannels();

    auto result = ChannelConverterDialog::showDialog(currentChannels);
    if (!result.has_value())
        return;

    if (result->targetChannels == currentChannels)
        return;

    try
    {
        const auto& source = doc->getBufferManager().getBuffer();
        auto converted = waveedit::ChannelConverter::convert(
            source,
            result->targetChannels,
            result->targetLayout,
            result->downmixPreset,
            result->lfeHandling,
            result->upmixStrategy);

        doc->getUndoManager().beginNewTransaction("Channel Conversion");
        doc->getUndoManager().perform(new ChannelConvertAction(
            doc->getBufferManager(),
            doc->getAudioEngine(),
            doc->getWaveformDisplay(),
            converted));
        doc->setModified(true);
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("DSPController::showChannelConverterDialog - "
                                 + juce::String(e.what()));
        ErrorDialog::show("Error",
                          "Channel conversion failed: " + juce::String(e.what()));
    }
}

void DSPController::showChannelExtractorDialog(Document* doc, juce::Component* /*parent*/)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    auto& bufferManager = doc->getBufferManager();
    const int currentChannels = bufferManager.getNumChannels();
    const juce::File sourceFile = doc->getAudioEngine().getCurrentFile();
    const juce::String sourceFileName = sourceFile.getFileName();

    auto result = ChannelExtractorDialog::showDialog(currentChannels, sourceFileName);
    if (!result.has_value())
        return;

    const juce::String baseName = sourceFile.getFileNameWithoutExtension();
    const double sampleRate = bufferManager.getSampleRate();
    int bitDepth = bufferManager.getBitDepth();
    if (bitDepth <= 0) bitDepth = 16;

    waveedit::ChannelLayout layout(currentChannels);

    juce::String extension;
    std::unique_ptr<juce::AudioFormat> audioFormat;

    switch (result->exportFormat)
    {
        case ChannelExtractorDialog::ExportFormat::FLAC:
            extension = ".flac";
            audioFormat = std::make_unique<juce::FlacAudioFormat>();
            // JUCE FLAC supports 16/24-bit only
            bitDepth = juce::jlimit(16, 24, bitDepth);
            break;

        case ChannelExtractorDialog::ExportFormat::OGG:
            extension = ".ogg";
            audioFormat = std::make_unique<juce::OggVorbisAudioFormat>();
            break;

        case ChannelExtractorDialog::ExportFormat::WAV:
        default:
            extension = ".wav";
            audioFormat = std::make_unique<juce::WavAudioFormat>();
            break;
    }

    // createWriterFor takes ownership of the stream and may delete it on
    // failure — release the unique_ptr BEFORE calling to avoid double-free.
    auto createWriter = [&](juce::OutputStream* stream, int numChannels)
        -> std::unique_ptr<juce::AudioFormatWriter>
    {
        const int writerBitDepth =
            (result->exportFormat == ChannelExtractorDialog::ExportFormat::OGG)
                ? 16 : bitDepth;
        const int qualityIndex =
            (result->exportFormat == ChannelExtractorDialog::ExportFormat::OGG)
                ? 5 : 0;

        return std::unique_ptr<juce::AudioFormatWriter>(
            audioFormat->createWriterFor(stream, sampleRate,
                                         static_cast<unsigned int>(numChannels),
                                         writerBitDepth, {}, qualityIndex));
    };

    try
    {
        if (result->exportMode == ChannelExtractorDialog::ExportMode::IndividualMono)
        {
            int successCount = 0;
            for (int srcChannel : result->channels)
            {
                const juce::String channelLabel = layout.getShortLabel(srcChannel);
                const juce::String filename = baseName + "_Ch"
                    + juce::String(srcChannel + 1) + "_"
                    + channelLabel + extension;
                const juce::File outFile = result->outputDirectory.getChildFile(filename);

                juce::AudioBuffer<float> monoBuffer(1, bufferManager.getNumSamples());
                monoBuffer.copyFrom(0, 0, bufferManager.getBuffer(),
                                    srcChannel, 0, bufferManager.getNumSamples());

                auto outputStream = outFile.createOutputStream();
                if (!outputStream) continue;

                auto* rawStream = outputStream.release();
                auto writer = createWriter(rawStream, 1);
                if (!writer) continue; // stream deleted by createWriterFor on failure

                if (writer->writeFromAudioSampleBuffer(monoBuffer, 0, monoBuffer.getNumSamples()))
                    ++successCount;
            }

            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Export Complete",
                "Successfully exported " + juce::String(successCount) + " of "
                + juce::String(static_cast<int>(result->channels.size()))
                + " channel(s) to:\n" + result->outputDirectory.getFullPathName());
        }
        else // CombinedMulti
        {
            juce::String channelSuffix;
            for (size_t i = 0; i < result->channels.size(); ++i)
            {
                if (i > 0) channelSuffix += "-";
                channelSuffix += layout.getShortLabel(result->channels[i]);
            }
            const juce::String filename = baseName + "_" + channelSuffix
                + "_extracted" + extension;
            const juce::File outFile = result->outputDirectory.getChildFile(filename);

            auto combinedBuffer = waveedit::ChannelConverter::extractChannels(
                bufferManager.getBuffer(), result->channels);

            auto outputStream = outFile.createOutputStream();
            if (!outputStream)
            {
                ErrorDialog::show("Export Error", "Failed to create output file.");
                return;
            }

            auto* rawStream = outputStream.release();
            auto writer = createWriter(rawStream, combinedBuffer.getNumChannels());
            if (!writer)
            {
                ErrorDialog::show("Export Error",
                    "Failed to create audio writer. Check format compatibility.");
                return;
            }

            if (writer->writeFromAudioSampleBuffer(combinedBuffer, 0,
                                                   combinedBuffer.getNumSamples()))
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Export Complete",
                    "Successfully exported "
                    + juce::String(combinedBuffer.getNumChannels())
                    + " channel(s) to:\n" + outFile.getFullPathName());
            }
            else
            {
                ErrorDialog::show("Export Error",
                                  "Failed to write audio data to file.");
            }
        }
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("DSPController::showChannelExtractorDialog - "
                                 + juce::String(e.what()));
        ErrorDialog::show("Error",
                          "Channel extraction failed: " + juce::String(e.what()));
    }
}

//==============================================================================
// Plugin chain operations
//==============================================================================

void DSPController::applyPluginChainToSelectionWithOptions(Document* doc, bool convertToStereo, bool includeTail, double tailLengthSeconds)
{
    applyPluginChainToSelectionInternal(doc, convertToStereo, includeTail, tailLengthSeconds);
}

void DSPController::applyPluginChainToSelection(Document* doc)
{
    applyPluginChainToSelectionInternal(doc, false, false, 0.0);
}

void DSPController::applyPluginChainToSelectionInternal(Document* doc,
                                                         bool convertToStereo,
                                                         bool includeTail,
                                                         double tailLengthSeconds)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    auto& engine = doc->getAudioEngine();
    auto& chain  = engine.getPluginChain();

    if (chain.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Apply Plugin Chain",
            "The plugin chain is empty. Add plugins first.",
            "OK");
        return;
    }

    if (chain.areAllBypassed())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Apply Plugin Chain",
            "All plugins are bypassed. Un-bypass at least one plugin to apply effects.",
            "OK");
        return;
    }

    auto& bufferManager = doc->getBufferManager();
    auto& buffer = bufferManager.getMutableBuffer();
    if (buffer.getNumSamples() == 0)
        return;

    int64_t startSample = 0;
    int64_t numSamples  = buffer.getNumSamples();
    const bool hasSelection = doc->getWaveformDisplay().hasSelection();

    if (hasSelection)
    {
        startSample = bufferManager.timeToSample(doc->getWaveformDisplay().getSelectionStart());
        const int64_t endSample = bufferManager.timeToSample(doc->getWaveformDisplay().getSelectionEnd());
        numSamples = endSample - startSample;
        if (numSamples <= 0)
            return;
    }

    const juce::String chainDescription = PluginChainRenderer::buildChainDescription(chain);
    const juce::String transactionName  = "Apply Plugin Chain: " + chainDescription;
    const double sampleRate = bufferManager.getSampleRate();

    int outputChannels = 0; // 0 = match source
    if (convertToStereo && buffer.getNumChannels() == 1)
    {
        // Convert to stereo BEFORE processing so display + engine update properly.
        doc->getUndoManager().beginNewTransaction("Convert to Stereo");
        doc->getUndoManager().perform(new ConvertToStereoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine()));
        doc->setModified(true);
        outputChannels = 2;
    }

    int64_t tailSamples = 0;
    if (includeTail && tailLengthSeconds > 0.0)
        tailSamples = static_cast<int64_t>(tailLengthSeconds * sampleRate);

    auto renderer = std::make_shared<PluginChainRenderer>();
    auto offlineChain = std::make_shared<PluginChainRenderer::OfflineChain>(
        PluginChainRenderer::createOfflineChain(chain, sampleRate, renderer->getBlockSize()));

    if (!offlineChain->isValid())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Apply Plugin Chain",
            "Failed to create offline plugin instances. Some plugins may not "
            "support offline rendering.",
            "OK");
        return;
    }

    // Apply the rendered buffer to the document (message thread).
    auto applyProcessed = [doc, startSample, numSamples, transactionName, chainDescription]
        (const juce::AudioBuffer<float>& processed)
    {
        if (processed.getNumSamples() <= 0)
            return;

        try
        {
            doc->getUndoManager().beginNewTransaction(transactionName);
            auto* undoAction = new ApplyPluginChainAction(
                doc->getBufferManager(),
                doc->getAudioEngine(),
                doc->getWaveformDisplay(),
                startSample,
                numSamples,
                processed,
                chainDescription);

            const bool replaced = doc->getBufferManager().replaceRange(
                startSample, numSamples, processed);
            if (!replaced)
            {
                delete undoAction;
                ErrorDialog::show("Apply Plugin Chain",
                                  "Failed to replace audio range with processed buffer.");
                return;
            }

            undoAction->markAsAlreadyPerformed();
            doc->getUndoManager().perform(undoAction);
            doc->setModified(true);

            doc->getWaveformDisplay().reloadFromBuffer(
                doc->getBufferManager().getBuffer(),
                doc->getBufferManager().getSampleRate(),
                true, true);

            doc->getAudioEngine().reloadBufferPreservingPlayback(
                doc->getBufferManager().getBuffer(),
                doc->getBufferManager().getSampleRate(),
                doc->getBufferManager().getBuffer().getNumChannels());
        }
        catch (const std::exception& e)
        {
            juce::Logger::writeToLog(
                "DSPController::applyPluginChainToSelectionInternal apply - "
                + juce::String(e.what()));
            ErrorDialog::show("Error",
                              "Plugin chain application failed: "
                              + juce::String(e.what()));
        }
    };

    if (numSamples > kProgressDialogThreshold)
    {
        // Async path with progress dialog
        auto processedBuffer = std::make_shared<juce::AudioBuffer<float>>();

        ProgressDialog::runWithProgress(
            transactionName,
            [doc, renderer, offlineChain, processedBuffer, startSample, numSamples,
             sampleRate, outputChannels, tailSamples]
            (std::function<bool(float, const juce::String&)> progress) -> bool
            {
                auto result = renderer->renderWithOfflineChain(
                    doc->getBufferManager().getBuffer(),
                    *offlineChain,
                    sampleRate,
                    startSample,
                    numSamples,
                    progress,
                    outputChannels,
                    tailSamples);

                if (!result.success)
                    return false;

                processedBuffer->setSize(result.processedBuffer.getNumChannels(),
                                         result.processedBuffer.getNumSamples());
                for (int ch = 0; ch < result.processedBuffer.getNumChannels(); ++ch)
                {
                    processedBuffer->copyFrom(ch, 0, result.processedBuffer,
                                              ch, 0, result.processedBuffer.getNumSamples());
                }
                return true;
            },
            [processedBuffer, applyProcessed](bool success)
            {
                if (success)
                    applyProcessed(*processedBuffer);
            });
    }
    else
    {
        // Synchronous small-selection path
        auto result = renderer->renderWithOfflineChain(
            buffer, *offlineChain, sampleRate,
            startSample, numSamples, nullptr,
            outputChannels, tailSamples);

        if (result.success)
        {
            applyProcessed(result.processedBuffer);
        }
        else if (!result.cancelled)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Apply Plugin Chain",
                "Failed to apply plugin chain:\n" + result.errorMessage,
                "OK");
        }
    }
}

void DSPController::showOfflinePluginDialog(Document* doc, juce::Component* /*parent*/)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    auto& engine = doc->getAudioEngine();
    auto& bufferManager = doc->getBufferManager();

    int64_t selectionStart = 0;
    int64_t selectionEnd   = bufferManager.getBuffer().getNumSamples();

    if (doc->getWaveformDisplay().hasSelection())
    {
        selectionStart = bufferManager.timeToSample(doc->getWaveformDisplay().getSelectionStart());
        selectionEnd   = bufferManager.timeToSample(doc->getWaveformDisplay().getSelectionEnd());
    }

    auto result = OfflinePluginDialog::showDialog(&engine, &bufferManager,
                                                  selectionStart, selectionEnd);
    if (!result || !result->applied)
        return;

    applyOfflinePluginToSelection(
        doc,
        result->pluginDescription,
        result->pluginState,
        selectionStart,
        selectionEnd - selectionStart,
        result->renderOptions.convertToStereo,
        result->renderOptions.includeTail,
        result->renderOptions.tailLengthSeconds);
}

void DSPController::applyOfflinePluginToSelection(Document* doc,
                                                  const juce::PluginDescription& pluginDesc,
                                                  const juce::MemoryBlock& pluginState,
                                                  int64_t startSample,
                                                  int64_t numSamples,
                                                  bool convertToStereo,
                                                  bool includeTail,
                                                  double tailLengthSeconds)
{
    if (!doc || numSamples <= 0)
        return;

    auto& bufferManager = doc->getBufferManager();
    auto& buffer = bufferManager.getMutableBuffer();
    const double sampleRate = bufferManager.getSampleRate();

    int outputChannels = 0;
    if (convertToStereo && buffer.getNumChannels() == 1)
    {
        doc->getUndoManager().beginNewTransaction("Convert to Stereo");
        doc->getUndoManager().perform(new ConvertToStereoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine()));
        doc->setModified(true);
        outputChannels = 2;
    }

    int64_t tailSamples = 0;
    if (includeTail && tailLengthSeconds > 0.0)
        tailSamples = static_cast<int64_t>(tailLengthSeconds * sampleRate);

    PluginChain tempChain;
    const int nodeIndex = tempChain.addPlugin(pluginDesc);
    if (nodeIndex < 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Offline Plugin",
            "Failed to load plugin: " + pluginDesc.name,
            "OK");
        return;
    }

    if (auto* node = tempChain.getPlugin(nodeIndex); node && pluginState.getSize() > 0)
        node->setState(pluginState);

    auto renderer = std::make_shared<PluginChainRenderer>();
    auto offlineChain = std::make_shared<PluginChainRenderer::OfflineChain>(
        PluginChainRenderer::createOfflineChain(tempChain, sampleRate, renderer->getBlockSize()));

    if (!offlineChain->isValid())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Offline Plugin",
            "Failed to create offline plugin instance.",
            "OK");
        return;
    }

    const juce::String transactionName = "Apply Plugin: " + pluginDesc.name;

    auto applyProcessed = [doc, startSample, numSamples, transactionName, pluginDesc]
        (const juce::AudioBuffer<float>& processed)
    {
        if (processed.getNumSamples() <= 0)
            return;

        try
        {
            doc->getUndoManager().beginNewTransaction(transactionName);
            auto* undoAction = new ApplyPluginChainAction(
                doc->getBufferManager(),
                doc->getAudioEngine(),
                doc->getWaveformDisplay(),
                startSample,
                numSamples,
                processed,
                pluginDesc.name);

            const bool replaced = doc->getBufferManager().replaceRange(
                startSample, numSamples, processed);
            if (!replaced)
            {
                delete undoAction;
                ErrorDialog::show("Offline Plugin",
                                  "Failed to replace audio range with processed buffer.");
                return;
            }

            undoAction->markAsAlreadyPerformed();
            doc->getUndoManager().perform(undoAction);
            doc->setModified(true);

            doc->getWaveformDisplay().reloadFromBuffer(
                doc->getBufferManager().getBuffer(),
                doc->getBufferManager().getSampleRate(),
                true, true);

            doc->getAudioEngine().reloadBufferPreservingPlayback(
                doc->getBufferManager().getBuffer(),
                doc->getBufferManager().getSampleRate(),
                doc->getBufferManager().getBuffer().getNumChannels());
        }
        catch (const std::exception& e)
        {
            juce::Logger::writeToLog("DSPController::applyOfflinePluginToSelection - "
                                     + juce::String(e.what()));
            ErrorDialog::show("Error",
                              "Offline plugin failed: " + juce::String(e.what()));
        }
    };

    if (numSamples > kProgressDialogThreshold)
    {
        auto processedBuffer = std::make_shared<juce::AudioBuffer<float>>();

        ProgressDialog::runWithProgress(
            transactionName,
            [doc, renderer, offlineChain, processedBuffer, startSample, numSamples,
             sampleRate, outputChannels, tailSamples]
            (std::function<bool(float, const juce::String&)> progress) -> bool
            {
                auto result = renderer->renderWithOfflineChain(
                    doc->getBufferManager().getBuffer(),
                    *offlineChain,
                    sampleRate,
                    startSample,
                    numSamples,
                    progress,
                    outputChannels,
                    tailSamples);

                if (!result.success)
                    return false;

                processedBuffer->setSize(result.processedBuffer.getNumChannels(),
                                         result.processedBuffer.getNumSamples());
                for (int ch = 0; ch < result.processedBuffer.getNumChannels(); ++ch)
                {
                    processedBuffer->copyFrom(ch, 0, result.processedBuffer,
                                              ch, 0, result.processedBuffer.getNumSamples());
                }
                return true;
            },
            [processedBuffer, applyProcessed](bool success)
            {
                if (success)
                    applyProcessed(*processedBuffer);
            });
    }
    else
    {
        auto result = renderer->renderWithOfflineChain(
            buffer, *offlineChain, sampleRate,
            startSample, numSamples, nullptr,
            outputChannels, tailSamples);

        if (result.success)
        {
            applyProcessed(result.processedBuffer);
        }
        else if (!result.cancelled)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Offline Plugin",
                "Failed to apply plugin:\n" + result.errorMessage,
                "OK");
        }
    }
}

//==============================================================================
// Resample
//==============================================================================

/**
 * Show resample dialog and apply resampling to the document.
 * Uses a simple AlertWindow with a combo box for sample rate selection.
 */
void DSPController::showResampleDialog(Document* doc, juce::Component* /*parent*/)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    try
    {
        double currentRate = doc->getAudioEngine().getSampleRate();

        // Create simple dialog with combo box for sample rate
        juce::AlertWindow dialog("Resample",
            "Current sample rate: " + juce::String(currentRate, 0) + " Hz\n\nSelect target sample rate:",
            juce::AlertWindow::NoIcon);

        dialog.addComboBox("rate", {"22050", "44100", "48000", "88200", "96000", "192000"}, "Sample Rate (Hz)");
        dialog.addButton("OK", 1);
        dialog.addButton("Cancel", 0);

        // Pre-select current rate if it matches
        auto* combo = dialog.getComboBoxComponent("rate");
        juce::String currentRateStr = juce::String(static_cast<int>(currentRate));
        for (int i = 0; i < combo->getNumItems(); ++i)
        {
            if (combo->getItemText(i) == currentRateStr)
            {
                combo->setSelectedItemIndex(i, juce::dontSendNotification);
                break;
            }
        }

        if (dialog.runModalLoop() == 1)
        {
            double newRate = dialog.getComboBoxComponent("rate")->getText().getDoubleValue();
            if (newRate > 0 && std::abs(newRate - currentRate) > 0.01)
            {
                auto& buffer = doc->getBufferManager().getBuffer();

                doc->getUndoManager().beginNewTransaction(
                    "Resample to " + juce::String(newRate, 0) + " Hz");
                doc->getUndoManager().perform(new ResampleUndoAction(
                    doc->getBufferManager(),
                    doc->getWaveformDisplay(),
                    doc->getAudioEngine(),
                    buffer,
                    currentRate,
                    newRate
                ));

                doc->setModified(true);
            }
        }
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("DSPController::showResampleDialog - " + juce::String(e.what()));
        ErrorDialog::show("Error", "Resample failed: " + juce::String(e.what()));
    }
}

//==============================================================================
// Time stretch / pitch shift (SoundTouch-backed)
//==============================================================================

namespace
{
    // Apply a TimePitchEngine recipe to the whole-buffer of a document
    // and register the result as an undoable transaction. Shared by
    // both showTimeStretchDialog and showPitchShiftDialog so the
    // buffer-handling, error-reporting, and undo wiring are identical.
    void applyTimePitchToDocument(Document* doc,
                                   const TimePitchEngine::Recipe& recipe,
                                   const juce::String& description)
    {
        if (! doc || ! doc->getAudioEngine().isFileLoaded())
            return;

        auto& bufferManager = doc->getBufferManager();
        const auto& srcBuffer = bufferManager.getBuffer();
        const double sampleRate = doc->getAudioEngine().getSampleRate();

        try
        {
            const auto processed =
                TimePitchEngine::apply(srcBuffer, sampleRate, recipe);
            if (processed.getNumSamples() <= 0)
            {
                ErrorDialog::show("Error",
                                   description + " produced an empty result.");
                return;
            }

            doc->getUndoManager().beginNewTransaction(description);
            doc->getUndoManager().perform(new TimePitchUndoAction(
                bufferManager,
                doc->getWaveformDisplay(),
                doc->getAudioEngine(),
                srcBuffer,
                processed,
                sampleRate,
                description));

            doc->setModified(true);
        }
        catch (const std::exception& e)
        {
            juce::Logger::writeToLog(
                "DSPController::applyTimePitchToDocument - " + juce::String(e.what()));
            ErrorDialog::show("Error",
                               description + " failed: " + juce::String(e.what()));
        }
    }
}

void DSPController::showTimeStretchDialog(Document* doc, juce::Component* /*parent*/)
{
    if (! doc || ! doc->getAudioEngine().isFileLoaded())
        return;

    juce::AlertWindow dialog(
        "Time Stretch",
        "Stretch or compress the file's tempo without changing pitch.\n"
        "Range: -50% (half speed) to +500% (6x speed).",
        juce::AlertWindow::NoIcon);

    dialog.addTextEditor("tempoPercent", "0.0", "Tempo change (%)");
    dialog.addButton("Apply",  1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (dialog.runModalLoop() != 1)
        return;

    const double tempoPercent = dialog.getTextEditorContents("tempoPercent").getDoubleValue();
    if (std::abs(tempoPercent) < 1.0e-6)
        return;  // identity

    if (tempoPercent < -50.0 || tempoPercent > 500.0)
    {
        ErrorDialog::show("Time Stretch",
                          "Tempo change out of range. Use -50 to +500.");
        return;
    }

    TimePitchEngine::Recipe recipe;
    recipe.tempoPercent = tempoPercent;

    const auto desc = juce::String::formatted("Time stretch %+.1f%%", tempoPercent);
    applyTimePitchToDocument(doc, recipe, desc);
}

void DSPController::showPitchShiftDialog(Document* doc, juce::Component* /*parent*/)
{
    if (! doc || ! doc->getAudioEngine().isFileLoaded())
        return;

    juce::AlertWindow dialog(
        "Pitch Shift",
        "Shift the file's pitch without changing length.\n"
        "Use whole semitones (e.g. 12 = 1 octave up) or fractions for cents.\n"
        "Range: -24 to +24 semitones.",
        juce::AlertWindow::NoIcon);

    dialog.addTextEditor("semitones", "0.0", "Semitones");
    dialog.addButton("Apply",  1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (dialog.runModalLoop() != 1)
        return;

    const double semitones = dialog.getTextEditorContents("semitones").getDoubleValue();
    if (std::abs(semitones) < 1.0e-6)
        return;  // identity

    if (semitones < -24.0 || semitones > 24.0)
    {
        ErrorDialog::show("Pitch Shift",
                          "Semitone count out of range. Use -24 to +24.");
        return;
    }

    TimePitchEngine::Recipe recipe;
    recipe.pitchSemitones = semitones;

    const auto desc = juce::String::formatted("Pitch shift %+.2f semitones", semitones);
    applyTimePitchToDocument(doc, recipe, desc);
}

//==============================================================================
// Head & Tail Processing
//==============================================================================

void DSPController::applyHeadTail(Document* doc, const HeadTailRecipe& recipe)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    try
    {
        const auto& inputBuffer = doc->getBufferManager().getBuffer();
        double sampleRate = doc->getAudioEngine().getSampleRate();

        juce::AudioBuffer<float> outputBuffer;
        auto report = HeadTailEngine::process(inputBuffer, sampleRate,
                                              recipe, outputBuffer);

        if (!report.success)
        {
            ErrorDialog::show("Head & Tail", report.errorMessage);
            return;
        }

        doc->getUndoManager().beginNewTransaction("Head & Tail Processing");
        doc->getUndoManager().perform(
            new HeadTailUndoAction(
                doc->getBufferManager(),
                doc->getWaveformDisplay(),
                doc->getAudioEngine(),
                inputBuffer, outputBuffer, sampleRate));
        doc->setModified(true);

        DBG("Head & Tail: " + juce::String(report.originalDurationMs, 1) + "ms -> "
            + juce::String(report.finalDurationMs, 1) + "ms");
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("DSPController::applyHeadTail - "
                                 + juce::String(e.what()));
        ErrorDialog::show("Error", "Head & Tail processing failed: "
                          + juce::String(e.what()));
    }
}

void DSPController::showHeadTailDialog(Document* doc, juce::Component* /*parent*/)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    const auto& buffer  = doc->getBufferManager().getBuffer();
    double      sr      = doc->getAudioEngine().getSampleRate();

    auto* dialog = new HeadTailDialog(buffer, sr);

    dialog->onApply = [this, doc](const HeadTailRecipe& recipe)
    {
        applyHeadTail(doc, recipe);
    };

    dialog->onCancel = []() {};

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle               = "Head & Tail Processing";
    options.dialogBackgroundColour    = waveedit::ThemeManager::getInstance().getCurrent().panel;
    options.content.setOwned(dialog);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = true;
    options.resizable                    = false;
    options.launchAsync();
}

void DSPController::showLoopingToolsDialog(Document* doc, juce::Component* /*parent*/)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    if (!doc->getWaveformDisplay().hasSelection())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Looping Tools",
            "Please select a region of audio to create a loop.",
            "OK");
        return;
    }

    const auto& buffer     = doc->getBufferManager().getBuffer();
    double      sampleRate = doc->getAudioEngine().getSampleRate();
    auto&       waveform   = doc->getWaveformDisplay();

    int64_t selStart = static_cast<int64_t>(waveform.getSelectionStart() * sampleRate);
    int64_t selEnd   = static_cast<int64_t>(waveform.getSelectionEnd()   * sampleRate);

    juce::File sourceFile = doc->getAudioEngine().getCurrentFile();

    auto* dialog = new LoopingToolsDialog(buffer, sampleRate, selStart, selEnd, sourceFile);
    dialog->onCancel = []() {};

    // Wire preview playback callbacks to AudioEngine
    auto& engine = doc->getAudioEngine();
    dialog->onPreviewRequested = [&engine](const juce::AudioBuffer<float>& previewBuffer, double sr)
    {
        int numCh = previewBuffer.getNumChannels();
        engine.loadPreviewBuffer(previewBuffer, sr, numCh);
        double loopDuration = static_cast<double>(previewBuffer.getNumSamples()) / sr;
        engine.setLoopPoints(0.0, loopDuration);
        engine.setLooping(true);
        engine.setPreviewMode(PreviewMode::OFFLINE_BUFFER);
        engine.play();
    };

    dialog->onPreviewStopped = [&engine]()
    {
        engine.stop();
        engine.setPreviewMode(PreviewMode::DISABLED);
    };

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle               = "Looping Tools";
    options.dialogBackgroundColour    = waveedit::ThemeManager::getInstance().getCurrent().panel;
    options.content.setOwned(dialog);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = true;
    options.resizable                    = false;
    options.launchAsync();
}
