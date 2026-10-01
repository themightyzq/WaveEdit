/*
  ==============================================================================

    DSPController_Plugins.cpp
    Part of WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Split out from DSPController_Advanced.cpp under CLAUDE.md 7.5 (file size
    cap). Hosts plugin chain and offline plugin application: offline render
    (with recorded parameter automation), then one undoable commit that also
    moves regions/markers when an effect tail lengthens the file.

  ==============================================================================
*/

#include "DSPController.h"
#include "ChannelFocus.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioBufferManager.h"
#include "../Utils/UndoActions/AudioUndoActions.h"
#include "../Utils/UndoActions/PluginUndoActions.h"
#include "../Utils/UndoActions/ChannelUndoActions.h"
#include "../UI/OfflinePluginDialog.h"
#include "../UI/ProgressDialog.h"
#include "../UI/ErrorDialog.h"
#include "../Plugins/PluginChainRenderer.h"

namespace
{
    /** DSPController::commitPluginRender() plus the caller's error dialogs
        (titles and messages unchanged from before the extraction). */
    void commitRenderedOutput(Document* doc,
                              int64_t startSample,
                              int64_t numSamples,
                              const juce::AudioBuffer<float>& processed,
                              const juce::String& transactionName,
                              const juce::String& description,
                              const juce::String& errorTitle,
                              const juce::String& failurePrefix)
    {
        try
        {
            if (!DSPController::commitPluginRender(doc, startSample, numSamples, processed,
                                                   transactionName, description))
                ErrorDialog::show(errorTitle,
                                  "Failed to replace audio range with processed buffer.");
        }
        catch (const std::exception& e)
        {
            juce::Logger::writeToLog("DSPController plugin apply - " + juce::String(e.what()));
            ErrorDialog::show("Error", failurePrefix + juce::String(e.what()));
        }
    }
}

bool DSPController::commitPluginRender(Document* doc,
                                       int64_t startSample,
                                       int64_t numSamples,
                                       const juce::AudioBuffer<float>& processed,
                                       const juce::String& transactionName,
                                       const juce::String& description)
{
    if (doc == nullptr || processed.getNumSamples() <= 0)
        return true;  // nothing to commit is not a failure

    doc->getUndoManager().beginNewTransaction(transactionName);
    auto* undoAction = new ApplyPluginChainAction(
        doc->getBufferManager(),
        doc->getAudioEngine(),
        doc->getWaveformDisplay(),
        startSample,
        numSamples,
        processed,
        description,
        &doc->getRegionManager(),
        &doc->getRegionDisplay(),
        &doc->getMarkerManager(),
        &doc->getMarkerDisplay());

    // With a per-channel focus only the focused channels take the render
    // (length-preserving renders only: a tail is refused under a partial
    // focus before rendering). Snapshot before the buffer is replaced.
    const bool lengthPreserving = processed.getNumSamples() == numSamples;
    const auto beforeRange = lengthPreserving
                                 ? ChannelFocus::snapshotIfPartial(*doc, startSample, numSamples)
                                 : juce::AudioBuffer<float>();

    const bool replaced = doc->getBufferManager().replaceRange(startSample, numSamples, processed);
    if (!replaced)
    {
        delete undoAction;
        return false;
    }

    undoAction->markAsAlreadyPerformed();
    doc->getUndoManager().perform(lengthPreserving
                                      ? ChannelFocus::wrap(*doc, undoAction, beforeRange, startSample)
                                      : undoAction);
    doc->setModified(true);

    // H1: refresh via the action's own length classification. A
    // tail-extending render (include-tail) grows the range and shifts
    // subsequent content, so this STOPS playback (reset to 0) instead of
    // unconditionally preserving a position that would resume on the
    // shifted timeline. Handles both the waveform + engine reload.
    undoAction->refreshAfterExternalReplace();
    return true;
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

    // An effect tail lengthens the file: not on some channels only.
    if (includeTail && tailLengthSeconds > 0.0
        && ChannelFocus::refuseIfPartial(*doc, "Apply Plugin Chain"))
        return;

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

    // Render the recorded parameter automation too, evaluated at file time.
    PluginChainRenderer::captureAutomation(*offlineChain, doc->getAutomationManager());

    // Apply the rendered buffer to the document (message thread).
    auto applyProcessed = [doc, startSample, numSamples, transactionName, chainDescription]
        (const juce::AudioBuffer<float>& processed)
    {
        commitRenderedOutput(doc, startSample, numSamples, processed, transactionName,
                             chainDescription, "Apply Plugin Chain",
                             "Plugin chain application failed: ");
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

        // The worker renders a copy that starts at index 0; automation is
        // evaluated at file time, so tell the renderer where the copy starts.
        offlineChain->automationFileOffset = startSample;

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

    std::optional<OfflinePluginDialog::Result> result;
    {
        const ChannelFocus::ScopedPreviewMask previewMask(*doc);  // preview the focused channels
        result = OfflinePluginDialog::showDialog(&engine, &bufferManager,
                                                  selectionStart, selectionEnd);
    }
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

    // An effect tail lengthens the file: not on some channels only.
    if (includeTail && tailLengthSeconds > 0.0
        && ChannelFocus::refuseIfPartial(*doc, "Offline Plugin"))
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
        commitRenderedOutput(doc, startSample, numSamples, processed, transactionName,
                             pluginDesc.name, "Offline Plugin", "Offline plugin failed: ");
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
