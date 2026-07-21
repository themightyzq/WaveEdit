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
#include <limits>
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
#include "../DSP/AudioGenerator.h"
#include "../UI/GenerateDialog.h"
#include "../UI/InsertSilenceDialog.h"
#include "../UI/GraphicalEQEditor.h"
#include "../UI/OfflinePluginDialog.h"
#include "../UI/ProgressDialog.h"
#include "../UI/ChannelConverterDialog.h"
#include "../UI/ChannelExtractorDialog.h"
#include "../UI/ErrorDialog.h"
#include "../DSP/HeadTailEngine.h"
#include "../UI/HeadTailDialog.h"
#include "../UI/TimePitchDialog.h"
#include "../UI/LoopingToolsDialog.h"
#include "../UI/ThemeManager.h"
#include "../Plugins/PluginManager.h"
#include "../Plugins/PluginChainRenderer.h"

//==============================================================================
// EQ dialogs
//==============================================================================

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

        case ChannelExtractorDialog::ExportFormat::AIFF:
            extension = ".aiff";
            audioFormat = std::make_unique<juce::AiffAudioFormat>();
            // JUCE AIFF supports 8/16/24-bit only (no 32-bit float)
            if (bitDepth > 24) bitDepth = 24;
            break;

        case ChannelExtractorDialog::ExportFormat::WAV:
        default:
            extension = ".wav";
            audioFormat = std::make_unique<juce::WavAudioFormat>();
            break;
    }

    // JUCE 8 createWriterFor takes the unique_ptr by reference and only
    // assumes ownership of the stream when writer creation succeeds.
    auto createWriter = [&](std::unique_ptr<juce::OutputStream>& stream, int numChannels)
        -> std::unique_ptr<juce::AudioFormatWriter>
    {
        const int writerBitDepth =
            (result->exportFormat == ChannelExtractorDialog::ExportFormat::OGG)
                ? 16 : bitDepth;
        const int qualityIndex =
            (result->exportFormat == ChannelExtractorDialog::ExportFormat::OGG)
                ? 5 : 0;

        return audioFormat->createWriterFor(stream,
                                            juce::AudioFormatWriterOptions()
                                                .withSampleRate(sampleRate)
                                                .withNumChannels(numChannels)
                                                .withBitsPerSample(writerBitDepth)
                                                .withQualityOptionIndex(qualityIndex));
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

                juce::AudioBuffer<float> monoBuffer(1, static_cast<int>(bufferManager.getNumSamples()));
                monoBuffer.copyFrom(0, 0, bufferManager.getBuffer(),
                                    srcChannel, 0, static_cast<int>(bufferManager.getNumSamples()));

                std::unique_ptr<juce::OutputStream> outputStream = outFile.createOutputStream();
                if (!outputStream) continue;

                auto writer = createWriter(outputStream, 1);
                if (!writer) continue;

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

            std::unique_ptr<juce::OutputStream> outputStream = outFile.createOutputStream();
            if (!outputStream)
            {
                ErrorDialog::show("Export Error", "Failed to create output file.");
                return;
            }

            auto writer = createWriter(outputStream, combinedBuffer.getNumChannels());
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

            // H1: refresh via the action's own length classification. A
            // tail-extending render (include-tail) grows the range and shifts
            // subsequent content, so this STOPS playback (reset to 0) instead of
            // unconditionally preserving a position that would resume on the
            // shifted timeline. Handles both the waveform + engine reload.
            undoAction->refreshAfterExternalReplace();
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
        // Async path with progress dialog. The dialog is NOT modal
        // (launchAsync), so the tab can be closed mid-render. Copy the source
        // range up front and re-check a lifeline before committing, so the
        // worker never touches the (possibly-freed) live Document -- same
        // pattern as the TimePitch path below (C1).
        auto processedBuffer = std::make_shared<juce::AudioBuffer<float>>();

        auto sourceSnapshot = std::make_shared<juce::AudioBuffer<float>>();
        sourceSnapshot->setSize(buffer.getNumChannels(), (int) numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            sourceSnapshot->copyFrom(ch, 0, buffer, ch, (int) startSample, (int) numSamples);

        juce::Component::SafePointer<WaveformDisplay> docLifeline(&doc->getWaveformDisplay());

        ProgressDialog::runWithProgress(
            transactionName,
            [sourceSnapshot, renderer, offlineChain, processedBuffer, numSamples,
             sampleRate, outputChannels, tailSamples]
            (std::function<bool(float, const juce::String&)> progress) -> bool
            {
                auto result = renderer->renderWithOfflineChain(
                    *sourceSnapshot,
                    *offlineChain,
                    sampleRate,
                    0,
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
            [processedBuffer, applyProcessed, docLifeline](bool success)
            {
                if (success && docLifeline.getComponent() != nullptr)
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
    // Stack-local chain (never audio-thread-visible), but use the configured
    // add anyway so state is applied pre-publish, matching the C4 pattern.
    const int nodeIndex = tempChain.addPluginConfigured(pluginDesc, pluginState,
                                                        /*bypassed*/ false);
    if (nodeIndex < 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Offline Plugin",
            "Failed to load plugin: " + pluginDesc.name,
            "OK");
        return;
    }

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

            // H1: same length-aware refresh as the plugin-chain path. An offline
            // plugin rendered with include-tail extends the range, so this stops
            // playback rather than resuming on shifted content.
            undoAction->refreshAfterExternalReplace();
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
        // Non-modal progress dialog: copy the source range and re-check a
        // lifeline so a mid-render tab close cannot free the Document out from
        // under the worker or the completion callback (C1).
        auto processedBuffer = std::make_shared<juce::AudioBuffer<float>>();

        auto sourceSnapshot = std::make_shared<juce::AudioBuffer<float>>();
        sourceSnapshot->setSize(buffer.getNumChannels(), (int) numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            sourceSnapshot->copyFrom(ch, 0, buffer, ch, (int) startSample, (int) numSamples);

        juce::Component::SafePointer<WaveformDisplay> docLifeline(&doc->getWaveformDisplay());

        ProgressDialog::runWithProgress(
            transactionName,
            [sourceSnapshot, renderer, offlineChain, processedBuffer, numSamples,
             sampleRate, outputChannels, tailSamples]
            (std::function<bool(float, const juce::String&)> progress) -> bool
            {
                auto result = renderer->renderWithOfflineChain(
                    *sourceSnapshot,
                    *offlineChain,
                    sampleRate,
                    0,
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
            [processedBuffer, applyProcessed, docLifeline](bool success)
            {
                if (success && docLifeline.getComponent() != nullptr)
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

        // Standard sample rates (game-audio staples 8k-16k through 192k).
        const juce::StringArray standardRates {
            "8000", "11025", "16000", "22050", "32000", "44100",
            "48000", "88200", "96000", "176400", "192000" };
        const juce::String customLabel("Custom...");

        // Create dialog with a combo box (standard rates + Custom) plus a
        // free-entry field used when Custom is chosen (UX11).
        juce::AlertWindow dialog("Resample",
            "Current sample rate: " + juce::String(currentRate, 0) + " Hz\n\n"
            "Select a target sample rate, or choose Custom... and type a rate "
            "(1000 - 384000 Hz):",
            juce::AlertWindow::NoIcon);

        juce::StringArray comboItems(standardRates);
        comboItems.add(customLabel);
        dialog.addComboBox("rate", comboItems, "Sample Rate (Hz)");
        dialog.addTextEditor("custom", juce::String(static_cast<int>(currentRate)),
                             "Custom rate (Hz)");
        dialog.addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
        dialog.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        // Pre-select the current rate when it is a standard rate, else the
        // nearest standard rate (Custom stays available for anything exact).
        auto* combo = dialog.getComboBoxComponent("rate");
        const juce::String currentRateStr = juce::String(static_cast<int>(currentRate));
        int matchIndex = standardRates.indexOf(currentRateStr);
        if (matchIndex < 0)
        {
            double nearestDelta = std::numeric_limits<double>::max();
            for (int i = 0; i < standardRates.size(); ++i)
            {
                const double delta = std::abs(standardRates[i].getDoubleValue() - currentRate);
                if (delta < nearestDelta)
                {
                    nearestDelta = delta;
                    matchIndex = i;
                }
            }
        }
        combo->setSelectedItemIndex(matchIndex, juce::dontSendNotification);

        if (dialog.runModalLoop() == 1)
        {
            // When Custom is selected, take the typed value; otherwise use the
            // combo's standard rate.
            double newRate = 0.0;
            if (combo->getText() == customLabel)
                newRate = dialog.getTextEditorContents("custom").trim().getDoubleValue();
            else
                newRate = combo->getText().getDoubleValue();

            // Validate the target rate before doing any work (UX11).
            if (newRate < 1000.0 || newRate > 384000.0)
            {
                ErrorDialog::show("Resample",
                    "Sample rate must be between 1000 and 384000 Hz.\n"
                    "No changes were made.");
                return;
            }

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
        ErrorDialog::show("Resample",
            "Could not resample the audio. No changes were made -- the original "
            "audio is intact.\n\nDetails: " + juce::String(e.what()));
    }
}

//==============================================================================
// Time stretch / pitch shift (SoundTouch-backed)
//==============================================================================

namespace
{
    // Progress-dialog threshold for time-stretch/pitch-shift renders. Mirrors
    // DSPController::kProgressDialogThreshold (private, so duplicated here); the
    // SoundTouch render is markedly more expensive per sample than gain/normalize,
    // so above this SOURCE length it runs behind a cancellable ProgressDialog (H4).
    constexpr juce::int64 kTimePitchProgressThreshold = 500000;

    // Register a finished time-pitch render as an undoable transaction. When
    // selection-scoped, splice the (unequal-length) result back over the source
    // range via ReplaceAction; else replace the whole file via TimePitchUndoAction.
    // Runs on the message thread (called from the sync path or the progress-dialog
    // completion callback).
    void commitTimePitchResult(Document* doc,
                               const juce::AudioBuffer<float>& processed,
                               juce::int64 rangeStart,
                               juce::int64 rangeLen,
                               bool selectionScoped,
                               double sampleRate,
                               const juce::String& description)
    {
        auto& bufferManager = doc->getBufferManager();

        doc->getUndoManager().beginNewTransaction(description);
        if (selectionScoped)
        {
            doc->getUndoManager().perform(new ReplaceAction(
                bufferManager,
                doc->getAudioEngine(),
                doc->getWaveformDisplay(),
                rangeStart,
                rangeLen,
                processed,
                &doc->getRegionManager(),
                &doc->getRegionDisplay(),
                &doc->getMarkerManager(),
                &doc->getMarkerDisplay()));
        }
        else
        {
            doc->getUndoManager().perform(new TimePitchUndoAction(
                bufferManager,
                doc->getWaveformDisplay(),
                doc->getAudioEngine(),
                bufferManager.getBuffer(),   // "before" == current whole-file buffer
                processed,
                sampleRate,
                description));
        }
        doc->setModified(true);
    }

    // Apply a TimePitchEngine recipe to a document. When a selection exists the
    // render is selection-scoped (M6/UX4): only the selected range is processed and
    // spliced back. The selection is captured (in seconds) at dialog-open time and
    // RE-VALIDATED here against the current buffer -- it may have changed while the
    // async dialog was open; the range is clamped and bails with a themed error if
    // it collapsed. Shared by both the Time Stretch and Pitch Shift Apply callbacks.
    void applyTimePitchToDocument(Document* doc,
                                   const TimePitchEngine::Recipe& recipe,
                                   const juce::String& description,
                                   bool hasSelection,
                                   double selStartSeconds,
                                   double selEndSeconds)
    {
        if (! doc || ! doc->getAudioEngine().isFileLoaded())
            return;

        auto& bufferManager = doc->getBufferManager();
        const auto& srcBuffer = bufferManager.getBuffer();
        const double sampleRate = doc->getAudioEngine().getSampleRate();
        const juce::int64 totalSamples = bufferManager.getNumSamples();

        // Resolve + re-validate the processing range against the CURRENT buffer.
        juce::int64 rangeStart = 0;
        juce::int64 rangeLen   = totalSamples;
        bool selectionScoped   = false;

        if (hasSelection)
        {
            // timeToSample returns int64_t, which GCC treats as a distinct
            // type from juce::int64 on Linux -- cast so jlimit's template
            // deduction sees one type on every platform.
            rangeStart = juce::jlimit((juce::int64) 0, totalSamples,
                                      (juce::int64) bufferManager.timeToSample(selStartSeconds));
            const juce::int64 rangeEnd = juce::jlimit((juce::int64) 0, totalSamples,
                                                      (juce::int64) bufferManager.timeToSample(selEndSeconds));
            rangeLen = rangeEnd - rangeStart;

            if (rangeLen <= 0)
            {
                ErrorDialog::show(description,
                                  "The selection is no longer valid. "
                                  "Reselect a range and try again.");
                return;
            }
            selectionScoped = true;
        }

        if (rangeLen <= 0 || srcBuffer.getNumChannels() <= 0)
            return;

        // Extract the source range to process (the whole file when not scoped).
        auto srcRange = std::make_shared<juce::AudioBuffer<float>>();
        srcRange->setSize(srcBuffer.getNumChannels(), (int) rangeLen);
        for (int ch = 0; ch < srcBuffer.getNumChannels(); ++ch)
            srcRange->copyFrom(ch, 0, srcBuffer, ch, (int) rangeStart, (int) rangeLen);

        // Below the threshold: process synchronously on the message thread.
        if (rangeLen < kTimePitchProgressThreshold)
        {
            try
            {
                const auto processed =
                    TimePitchEngine::apply(*srcRange, sampleRate, recipe);
                if (processed.getNumSamples() <= 0)
                {
                    ErrorDialog::show("Error",
                                       description + " produced an empty result.");
                    return;
                }
                commitTimePitchResult(doc, processed, rangeStart, rangeLen,
                                      selectionScoped, sampleRate, description);
            }
            catch (const std::exception& e)
            {
                juce::Logger::writeToLog(
                    "DSPController::applyTimePitchToDocument - " + juce::String(e.what()));
                ErrorDialog::show("Error",
                                   description + " failed: " + juce::String(e.what()));
            }
            return;
        }

        // Large render: offload to the ProgressDialog worker thread with a working
        // cancel; splice/commit on the message thread in the completion callback.
        // The ProgressDialog is NOT modal (launchAsync) and the native macOS
        // menu bar stays live, so the document CAN be closed mid-render --
        // re-check the lifeline before committing.
        juce::Component::SafePointer<WaveformDisplay> docLifeline(&doc->getWaveformDisplay());
        auto result = std::make_shared<juce::AudioBuffer<float>>();

        ProgressDialog::runWithProgress(
            description,
            [srcRange, result, sampleRate, recipe]
            (std::function<bool(float, const juce::String&)> progress) -> bool
            {
                try
                {
                    auto out = TimePitchEngine::apply(
                        *srcRange, sampleRate, recipe,
                        [&progress](float p) { return progress(p, "Processing..."); });
                    if (out.getNumSamples() <= 0)
                        return false;   // cancelled or empty
                    *result = std::move(out);
                    return true;
                }
                catch (const std::exception& e)
                {
                    juce::Logger::writeToLog(
                        "DSPController::applyTimePitchToDocument - " + juce::String(e.what()));
                    return false;
                }
            },
            [doc, docLifeline, result, rangeStart, rangeLen, selectionScoped, sampleRate, description]
            (bool success)
            {
                if (! success || result->getNumSamples() <= 0)
                    return;   // cancelled / failed -- buffer left untouched
                if (docLifeline.getComponent() == nullptr)
                    return;   // document closed during the render (C6 class)
                commitTimePitchResult(doc, *result, rangeStart, rangeLen,
                                      selectionScoped, sampleRate, description);
            });
    }

    // Validate the applied value, short-circuit identity, build the recipe, and
    // apply it. Threads the selection range (captured at dialog-open) through to
    // the selection-scoped Apply path.
    void applyTimePitchValue(Document* doc, TimePitchDialog::Mode mode, double value,
                             bool hasSelection, double selStartSeconds, double selEndSeconds)
    {
        if (std::abs(value) < 1.0e-6)
            return;  // identity

        TimePitchEngine::Recipe recipe;
        juce::String desc;

        if (mode == TimePitchDialog::Mode::TimeStretch)
        {
            if (value < -95.0 || value > 5000.0)
            {
                ErrorDialog::show("Time Stretch",
                                  "Tempo change out of range. Use -95 to +5000.");
                return;
            }
            recipe.tempoPercent = value;
            desc = juce::String::formatted("Time stretch %+.1f%%", value);
        }
        else
        {
            if (value < -60.0 || value > 60.0)
            {
                ErrorDialog::show("Pitch Shift",
                                  "Semitone count out of range. Use -60 to +60.");
                return;
            }
            recipe.pitchSemitones = value;
            desc = juce::String::formatted("Pitch shift %+.2f semitones", value);
        }

        applyTimePitchToDocument(doc, recipe, desc,
                                 hasSelection, selStartSeconds, selEndSeconds);
    }

    // Construct + launch the shared TimePitchDialog for either mode. Mirrors
    // DSPController::showHeadTailDialog: passes the AudioEngine plus the
    // document's WaveformDisplay as a SafePointer lifeline for the async dialog.
    void showTimePitchDialog(Document* doc, TimePitchDialog::Mode mode)
    {
        if (! doc || ! doc->getAudioEngine().isFileLoaded())
            return;

        auto& waveform = doc->getWaveformDisplay();
        const auto& buffer = doc->getBufferManager().getBuffer();
        const double sr = doc->getAudioEngine().getSampleRate();

        // Capture the selection (in seconds) now; re-validated at apply time.
        const bool   hasSel          = waveform.hasSelection();
        const double selStartSeconds = waveform.getSelectionStart();
        const double selEndSeconds   = waveform.getSelectionEnd();

        auto* dialog = new TimePitchDialog(mode, buffer, sr,
                                           hasSel,
                                           selStartSeconds,
                                           selEndSeconds,
                                           waveform.getEditCursorPosition(),
                                           &doc->getAudioEngine(),
                                           &waveform);

        // C6: the dialog is async. If the document (and its waveform/engine) is
        // closed while it is open, bail instead of writing through a dangling
        // Document*. The waveform doubles as the SafePointer lifeline.
        juce::Component::SafePointer<WaveformDisplay> lifeline(&waveform);
        dialog->onApply =
            [doc, mode, lifeline, hasSel, selStartSeconds, selEndSeconds](double value)
        {
            if (lifeline.getComponent() == nullptr)
                return;
            applyTimePitchValue(doc, mode, value, hasSel, selStartSeconds, selEndSeconds);
        };
        dialog->onCancel = []() {};

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = (mode == TimePitchDialog::Mode::TimeStretch)
            ? "Time Stretch" : "Pitch Shift";
        options.dialogBackgroundColour =
            waveedit::ThemeManager::getInstance().getCurrent().panel;
        options.content.setOwned(dialog);
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar            = true;
        options.resizable                    = false;
        options.launchAsync();
    }
}

void DSPController::showTimeStretchDialog(Document* doc, juce::Component* /*parent*/)
{
    showTimePitchDialog(doc, TimePitchDialog::Mode::TimeStretch);
}

void DSPController::showPitchShiftDialog(Document* doc, juce::Component* /*parent*/)
{
    showTimePitchDialog(doc, TimePitchDialog::Mode::PitchShift);
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

    // Pass the document's WaveformDisplay as a SafePointer lifeline: the dialog
    // is launched async, so if the document (and its AudioEngine) is closed
    // while the dialog is open, the dialog stops touching the dangling engine.
    auto* dialog = new HeadTailDialog(buffer, sr, &doc->getAudioEngine(),
                                      &doc->getWaveformDisplay());

    // C6: same guard as showTimePitchDialog -- the dialog is async, so if the
    // document (and its waveform/engine) is closed while it is open, bail instead
    // of writing through a dangling Document*.
    juce::Component::SafePointer<WaveformDisplay> lifeline(&doc->getWaveformDisplay());
    dialog->onApply = [this, doc, lifeline](const HeadTailRecipe& recipe)
    {
        if (lifeline.getComponent() == nullptr)
            return;
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

    // Wire preview playback callbacks to AudioEngine. The dialog is
    // non-modal (launchAsync), so guard every engine touch with a
    // SafePointer lifeline -- the document (and its engine) can be closed
    // while the dialog is still open (C6 class).
    auto* enginePtr = &doc->getAudioEngine();
    juce::Component::SafePointer<WaveformDisplay> loopLifeline(&waveform);
    dialog->onPreviewRequested =
        [enginePtr, loopLifeline](const juce::AudioBuffer<float>& previewBuffer, double sr)
    {
        if (loopLifeline.getComponent() == nullptr)
            return;   // document closed; engine pointer is dangling
        int numCh = previewBuffer.getNumChannels();
        enginePtr->loadPreviewBuffer(previewBuffer, sr, numCh);
        double loopDuration = static_cast<double>(previewBuffer.getNumSamples()) / sr;
        enginePtr->setLoopPoints(0.0, loopDuration);
        enginePtr->setLooping(true);
        enginePtr->setPreviewMode(PreviewMode::OFFLINE_BUFFER);
        enginePtr->play();
    };

    dialog->onPreviewStopped = [enginePtr, loopLifeline]()
    {
        if (loopLifeline.getComponent() == nullptr)
            return;
        enginePtr->stop();
        enginePtr->setPreviewMode(PreviewMode::DISABLED);
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

//==============================================================================
// Generate menu: Insert Silence / Generate Tone / Generate Noise.
//
// Placement is selection-aware (mirrors ClipboardController::pasteAtCursor):
// with a selection, the generated content overwrites it (ReplaceAction, same
// length); otherwise it is inserted at the edit cursor / playhead, growing the
// file and shifting regions + markers (InsertAction).
//==============================================================================
void DSPController::generateAndPlace(Document* doc, double durationSeconds,
                                     const juce::String& transactionName,
                                     const std::function<void(juce::AudioBuffer<float>&, double)>& fill)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    try
    {
        auto& bufferManager = doc->getBufferManager();
        auto& waveform = doc->getWaveformDisplay();
        const int numChannels = bufferManager.getBuffer().getNumChannels();
        const double sampleRate = doc->getAudioEngine().getSampleRate();
        if (numChannels <= 0 || sampleRate <= 0.0)
            return;

        const bool replaceSelection = waveform.hasSelection();
        int64_t startSample = 0;
        int64_t numSamples = 0;

        if (replaceSelection)
        {
            startSample = bufferManager.timeToSample(waveform.getSelectionStart());
            const int64_t endSample = bufferManager.timeToSample(waveform.getSelectionEnd());
            numSamples = endSample - startSample;
        }
        else
        {
            const double cursorPos = waveform.hasEditCursor()
                                         ? waveform.getEditCursorPosition()
                                         : waveform.getPlaybackPosition();
            startSample = juce::jlimit((int64_t) 0, bufferManager.getNumSamples(),
                                       bufferManager.timeToSample(cursorPos));
            numSamples = (int64_t) std::llround(durationSeconds * sampleRate);
        }

        // Defensive bound (the dialogs also validate): reject non-positive
        // lengths and anything that would overflow the int-sample AudioBuffer.
        if (numSamples <= 0 || numSamples > (int64_t) std::numeric_limits<int>::max())
            return;

        juce::AudioBuffer<float> generated(numChannels, (int) numSamples);
        generated.clear();
        if (fill)
            fill(generated, sampleRate);

        doc->getUndoManager().beginNewTransaction(transactionName);

        if (replaceSelection)
        {
            doc->getUndoManager().perform(new ReplaceAction(
                bufferManager, doc->getAudioEngine(), waveform,
                startSample, numSamples, generated,
                &doc->getRegionManager(), &doc->getRegionDisplay(),
                &doc->getMarkerManager(), &doc->getMarkerDisplay()));
        }
        else
        {
            doc->getUndoManager().perform(new InsertAction(
                bufferManager, doc->getAudioEngine(), waveform,
                startSample, generated,
                &doc->getRegionManager(), &doc->getRegionDisplay(),
                &doc->getMarkerManager(), &doc->getMarkerDisplay()));
        }

        doc->setModified(true);
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("DSPController::generateAndPlace - " + juce::String(e.what()));
        ErrorDialog::show("Generate",
                          "Could not generate audio. No changes were made.\n\nDetails: "
                          + juce::String(e.what()));
    }
}

void DSPController::showInsertSilenceDialog(Document* doc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    const bool hasSelection = doc->getWaveformDisplay().hasSelection();

    InsertSilenceDialog::showDialog(parent, 1.0, hasSelection,
        [this, doc](double durationSeconds)
        {
            // Silence = a zero-filled buffer (the fill leaves it cleared).
            generateAndPlace(doc, durationSeconds, "Insert Silence",
                             [](juce::AudioBuffer<float>&, double) {});
        });
}

void DSPController::showGenerateToneDialog(Document* doc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    auto& waveform = doc->getWaveformDisplay();
    const bool hasSelection = waveform.hasSelection();
    const double selectionSeconds = hasSelection
        ? (waveform.getSelectionEnd() - waveform.getSelectionStart()) : 0.0;
    const double sampleRate = doc->getAudioEngine().getSampleRate();
    const int numChannels = doc->getBufferManager().getBuffer().getNumChannels();

    GenerateDialog::showDialog(parent, GenerateDialog::Mode::Tone,
        &doc->getAudioEngine(),
        juce::Component::SafePointer<WaveformDisplay>(&waveform),
        hasSelection, selectionSeconds, sampleRate, numChannels,
        [this, doc](const GenerateDialog::Result& r)
        {
            const float amp = AudioProcessor::dBToLinear(r.amplitudeDb);
            generateAndPlace(doc, r.durationSeconds, "Generate Tone",
                [r, amp](juce::AudioBuffer<float>& buf, double sr)
                {
                    AudioGenerator gen;
                    gen.prepare(sr);
                    gen.generateTone(buf, r.waveform, r.frequencyHz, amp);
                });
        });
}

void DSPController::showGenerateNoiseDialog(Document* doc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    auto& waveform = doc->getWaveformDisplay();
    const bool hasSelection = waveform.hasSelection();
    const double selectionSeconds = hasSelection
        ? (waveform.getSelectionEnd() - waveform.getSelectionStart()) : 0.0;
    const double sampleRate = doc->getAudioEngine().getSampleRate();
    const int numChannels = doc->getBufferManager().getBuffer().getNumChannels();

    GenerateDialog::showDialog(parent, GenerateDialog::Mode::Noise,
        &doc->getAudioEngine(),
        juce::Component::SafePointer<WaveformDisplay>(&waveform),
        hasSelection, selectionSeconds, sampleRate, numChannels,
        [this, doc](const GenerateDialog::Result& r)
        {
            const float amp = AudioProcessor::dBToLinear(r.amplitudeDb);
            generateAndPlace(doc, r.durationSeconds, "Generate Noise",
                [r, amp](juce::AudioBuffer<float>& buf, double sr)
                {
                    AudioGenerator gen;
                    gen.prepare(sr);
                    gen.generateNoise(buf, r.noiseType, amp);
                });
        });
}
