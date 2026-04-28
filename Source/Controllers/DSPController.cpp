/*
  ==============================================================================

    DSPController.cpp - DSP Application Controller Implementation
    Part of WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/

#include "DSPController.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioFileManager.h"
#include "../Audio/AudioBufferManager.h"
#include "../Audio/AudioProcessor.h"
#include "../Audio/ChannelLayout.h"
#include "../Utils/UndoActions/AudioUndoActions.h"
#include "../UI/GainDialog.h"
#include "../UI/NormalizeDialog.h"
#include "../UI/FadeDialog.h"
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
#include "../Plugins/PluginManager.h"

//==============================================================================

DSPController::DSPController()
{
}

//==============================================================================
// Gain adjustment helpers

/**
 * Apply gain adjustment to entire file or selection.
 * Creates undo action for the gain adjustment.
 *
 * @param doc Document to operate on
 * @param gainDB Gain adjustment in decibels (positive or negative)
 * @param startSample Optional start sample (for dialog-based apply with explicit bounds)
 * @param endSample Optional end sample (for dialog-based apply with explicit bounds)
 */
void DSPController::applyGainAdjustment(Document* doc, float gainDB, int64_t startSample, int64_t endSample)
{
    if (!doc) return;

    try
    {
        if (!doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        // Note: We no longer stop/restart playback here.
        // GainUndoAction::perform() uses reloadBufferPreservingPlayback() which safely
        // updates the audio buffer while preserving playback position if currently playing.
        // This allows real-time gain adjustments during playback without interruption.

        // Get current buffer
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        if (buffer.getNumSamples() == 0)
        {
            return;
        }

        // Determine region to process
        int startSampleInt = 0;
        int numSamples = buffer.getNumSamples();
        bool isSelection = false;

        // CRITICAL FIX: Use explicit bounds if provided (from dialog preview)
        // This ensures we apply to the SAME region that was previewed
        if (startSample >= 0 && endSample >= 0)
        {
            // Explicit bounds provided (from dialog) - use these
            startSampleInt = static_cast<int>(startSample);
            numSamples = static_cast<int>(endSample - startSample);
            isSelection = (startSample != 0 || endSample != buffer.getNumSamples());
        }
        else if (doc->getWaveformDisplay().hasSelection())
        {
            // No explicit bounds - check current selection (for keyboard shortcuts)
            startSampleInt = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
            int endSampleInt = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
            numSamples = endSampleInt - startSampleInt;
            isSelection = true;
        }

        // Store before state for undo (only the affected region for memory efficiency)
        juce::AudioBuffer<float> beforeBuffer;
        beforeBuffer.setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            beforeBuffer.copyFrom(ch, 0, buffer, ch, startSampleInt, numSamples);
        }

        // CRITICAL FIX: Start a new transaction so each gain adjustment is a separate undo step
        // Without this, JUCE groups all actions into one transaction and undo undoes everything at once
        juce::String transactionName = juce::String::formatted(
            "Gain %+.1f dB (%s)",
            gainDB,
            isSelection ? "selection" : "entire file"
        );
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action (perform() will apply the gain)
        auto* undoAction = new GainUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer,
            startSampleInt,
            numSamples,
            gainDB,
            isSelection
        );

        // perform() calls GainUndoAction::perform() which applies gain and updates display
        // while preserving playback state (uses reloadBufferPreservingPlayback internally)
        doc->getUndoManager().perform(undoAction);

        // Mark as modified
        doc->setModified(true);
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("DSPController::applyGainAdjustment - Error: " + juce::String(e.what()));
        ErrorDialog::show("Error", "Gain adjustment failed: " + juce::String(e.what()));
    }
}

/**
 * Show gain dialog and apply user-entered gain value.
 * Passes AudioEngine to enable real-time preview.
 * Supports selection-based preview - if user has selection, only that region is previewed.
 */
void DSPController::showGainDialog(Document* doc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
    {
        // No document or file loaded - show dialog without preview
        auto result = GainDialog::showDialog(nullptr, nullptr, 0, 0);

        if (result.has_value())
        {
            applyGainAdjustment(doc, result.value());
        }
        return;
    }

    // Get selection bounds (or entire file if no selection)
    auto& waveform = doc->getWaveformDisplay();
    auto& engine = doc->getAudioEngine();
    bool hasSelection = waveform.hasSelection();

    // Convert time-based selection (seconds) to sample-based (samples)
    double sampleRate = engine.getSampleRate();
    int64_t startSampleInt = hasSelection ?
        static_cast<int64_t>(waveform.getSelectionStart() * sampleRate) : 0;
    int64_t endSampleInt = hasSelection ?
        static_cast<int64_t>(waveform.getSelectionEnd() * sampleRate) :
        static_cast<int64_t>(engine.getTotalLength() * sampleRate);

    // Show dialog with preview support and selection bounds
    auto result = GainDialog::showDialog(&doc->getAudioEngine(), &doc->getBufferManager(), startSampleInt, endSampleInt);

    if (result.has_value())
    {
        // CRITICAL: Pass the SAME bounds that were previewed to ensure apply matches preview
        applyGainAdjustment(doc, result.value(), startSampleInt, endSampleInt);
    }
}

/**
 * Show normalize dialog and apply normalization to selection (or entire file).
 * Allows user to set target peak level and preview before applying.
 */
void DSPController::showNormalizeDialog(Document* doc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    // Get selection bounds (or entire file if no selection)
    auto& waveform = doc->getWaveformDisplay();
    auto& engine = doc->getAudioEngine();
    bool hasSelection = waveform.hasSelection();

    // Convert time-based selection (seconds) to sample-based (samples)
    double sampleRate = engine.getSampleRate();
    int64_t startSample = hasSelection ?
        static_cast<int64_t>(waveform.getSelectionStart() * sampleRate) : 0;
    int64_t endSample = hasSelection ?
        static_cast<int64_t>(waveform.getSelectionEnd() * sampleRate) :
        static_cast<int64_t>(engine.getTotalLength() * sampleRate);

    // DEBUG: Log the bounds being passed to the dialog
    DBG("showNormalizeDialog - Creating dialog with bounds:");
    DBG("  Has selection: " + juce::String(hasSelection ? "YES" : "NO"));
    if (hasSelection) {
        DBG("  Selection start time: " + juce::String(waveform.getSelectionStart()) + " seconds");
        DBG("  Selection end time: " + juce::String(waveform.getSelectionEnd()) + " seconds");
    }
    DBG("  Start sample: " + juce::String(startSample));
    DBG("  End sample: " + juce::String(endSample));
    DBG("  Sample rate: " + juce::String(sampleRate));

    // Create and configure dialog
    NormalizeDialog dialog(&doc->getAudioEngine(), &doc->getBufferManager(), startSample, endSample);

    // Set up callbacks
    dialog.onApply([this, doc, &dialog](float targetDB) {
        // Get selection or entire file
        int startSampleInt = 0;
        int numSamples = doc->getBufferManager().getBuffer().getNumSamples();
        bool isSelection = false;

        if (doc->getWaveformDisplay().hasSelection())
        {
            startSampleInt = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
            int endSampleInt = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
            numSamples = endSampleInt - startSampleInt;
            isSelection = true;
        }

        // Store before state for undo (MUST happen before any processing)
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        auto beforeBuffer = std::make_shared<juce::AudioBuffer<float>>();
        beforeBuffer->setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            beforeBuffer->copyFrom(ch, 0, buffer, ch, startSampleInt, numSamples);
        }

        // Get mode and calculate required gain
        NormalizeDialog::NormalizeMode mode = dialog.getMode();
        float currentLevel = (mode == NormalizeDialog::NormalizeMode::RMS) ?
            dialog.getCurrentRMSDB() : dialog.getCurrentPeakDB();
        float requiredGainDB = targetDB - currentLevel;

        // Build transaction name
        juce::String modeStr = (mode == NormalizeDialog::NormalizeMode::RMS) ? "RMS" : "Peak";
        juce::String transactionName = juce::String::formatted(
            "Normalize %s to %.1f dB (%s)",
            modeStr.toRawUTF8(),
            targetDB,
            isSelection ? "selection" : "entire file"
        );

        // Close the dialog first (before potentially showing progress dialog)
        if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState(1);

        // Check if we need progress dialog for large operations
        if (numSamples >= kProgressDialogThreshold)
        {
            // ASYNCHRONOUS PATH: Large operation - show progress dialog
            // Create a working copy of the selection region for processing
            auto regionBuffer = std::make_shared<juce::AudioBuffer<float>>();
            regionBuffer->setSize(buffer.getNumChannels(), numSamples);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                regionBuffer->copyFrom(ch, 0, buffer, ch, startSampleInt, numSamples);
            }

            ProgressDialog::runWithProgress(
                transactionName,
                [regionBuffer, requiredGainDB, numSamples](ProgressCallback progress) -> bool {
                    // Apply gain to the region copy (not the main buffer)
                    // Use startSample=0 since regionBuffer is already extracted from startSample
                    return AudioProcessor::applyGainWithProgress(*regionBuffer, requiredGainDB, 0, numSamples, progress);
                },
                [doc, beforeBuffer, regionBuffer, startSampleInt, numSamples, transactionName, requiredGainDB, isSelection](bool success) {
                    if (success)
                    {
                        // Copy processed region back to main buffer at correct position
                        auto& buf = doc->getBufferManager().getMutableBuffer();
                        for (int ch = 0; ch < regionBuffer->getNumChannels(); ++ch)
                        {
                            buf.copyFrom(ch, startSampleInt, *regionBuffer, ch, 0, numSamples);
                        }

                        // Register undo action (operation already applied)
                        doc->getUndoManager().beginNewTransaction(transactionName);
                        auto* undoAction = new GainUndoAction(
                            doc->getBufferManager(),
                            doc->getWaveformDisplay(),
                            doc->getAudioEngine(),
                            *beforeBuffer,
                            startSampleInt,
                            numSamples,
                            requiredGainDB,  // float gainDB
                            isSelection      // bool isSelection
                        );
                        // Mark as already performed so undo() will restore, but perform() is no-op
                        undoAction->markAsAlreadyPerformed();
                        doc->getUndoManager().perform(undoAction);
                        doc->setModified(true);

                        // Update waveform display
                        doc->getAudioEngine().reloadBufferPreservingPlayback(
                            buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                        doc->getWaveformDisplay().reloadFromBuffer(
                            buf, doc->getAudioEngine().getSampleRate(), true, true);
                    }
                    else
                    {
                        // Cancelled: Restore buffer from snapshot
                        auto& buf = doc->getBufferManager().getMutableBuffer();
                        for (int ch = 0; ch < beforeBuffer->getNumChannels(); ++ch)
                        {
                            buf.copyFrom(ch, startSampleInt, *beforeBuffer, ch, 0, numSamples);
                        }
                        // Update display to show restored state
                        doc->getAudioEngine().reloadBufferPreservingPlayback(
                            buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                        doc->getWaveformDisplay().reloadFromBuffer(
                            buf, doc->getAudioEngine().getSampleRate(), true, true);
                    }
                }
            );
        }
        else
        {
            // SYNCHRONOUS PATH: Small operation - existing behavior
            doc->getUndoManager().beginNewTransaction(transactionName);

            auto* undoAction = new GainUndoAction(
                doc->getBufferManager(),
                doc->getWaveformDisplay(),
                doc->getAudioEngine(),
                *beforeBuffer,
                startSampleInt,
                numSamples,
                requiredGainDB,  // float gainDB
                isSelection      // bool isSelection
            );

            doc->getUndoManager().perform(undoAction);
            doc->setModified(true);
        }
    });

    dialog.onCancel([&dialog]() {
        if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState(0);
    });

    // Show dialog modally
    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(&dialog);  // Use setNonOwned for stack-allocated dialog
    options.componentToCentreAround = parent;
    options.dialogTitle = "Normalize";
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    options.runModal();
}

/**
 * Show fade in dialog and apply fade to selection.
 * Requires selection (won't work on entire file).
 */
void DSPController::showFadeInDialog(Document* doc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    // Fade in requires a selection
    if (!doc->getWaveformDisplay().hasSelection())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Fade In",
            "Please select a region of audio to fade in.",
            "OK");
        return;
    }

    // Get selection bounds
    auto& waveform = doc->getWaveformDisplay();
    auto& engine = doc->getAudioEngine();
    double sampleRate = engine.getSampleRate();
    int64_t startSample = static_cast<int64_t>(waveform.getSelectionStart() * sampleRate);
    int64_t endSample = static_cast<int64_t>(waveform.getSelectionEnd() * sampleRate);

    // Create and configure dialog
    FadeDialog dialog(FadeDirection::FadeIn, &doc->getAudioEngine(), &doc->getBufferManager(), startSample, endSample);

    // Set up callbacks
    dialog.onApply([this, doc, &dialog]() {
        // Get selection
        int startSampleInt = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
        int endSampleInt = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
        int numSamples = endSampleInt - startSampleInt;

        // Store before state for undo (MUST happen before any processing)
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        auto beforeBuffer = std::make_shared<juce::AudioBuffer<float>>();
        beforeBuffer->setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            beforeBuffer->copyFrom(ch, 0, buffer, ch, startSampleInt, numSamples);
        }

        // Get selected curve type from dialog
        FadeCurveType curveType = dialog.getSelectedCurveType();

        // Close the dialog first (before potentially showing progress dialog)
        if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState(1);

        // Check if we need progress dialog for large operations
        if (numSamples >= kProgressDialogThreshold)
        {
            // ASYNCHRONOUS PATH: Large operation - show progress dialog
            // Create a working copy of the selection region for processing
            auto regionBuffer = std::make_shared<juce::AudioBuffer<float>>();
            regionBuffer->setSize(buffer.getNumChannels(), numSamples);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                regionBuffer->copyFrom(ch, 0, buffer, ch, startSampleInt, numSamples);
            }

            ProgressDialog::runWithProgress(
                "Fade In",
                [regionBuffer, numSamples, curveType](ProgressCallback progress) -> bool {
                    // Apply fade to the region copy (not the main buffer)
                    return AudioProcessor::fadeInWithProgress(*regionBuffer, numSamples, curveType, progress);
                },
                [doc, beforeBuffer, regionBuffer, startSampleInt, numSamples, curveType](bool success) {
                    if (success)
                    {
                        // Copy processed region back to main buffer at correct position
                        auto& buf = doc->getBufferManager().getMutableBuffer();
                        for (int ch = 0; ch < regionBuffer->getNumChannels(); ++ch)
                        {
                            buf.copyFrom(ch, startSampleInt, *regionBuffer, ch, 0, numSamples);
                        }

                        // Register undo action (operation already applied)
                        doc->getUndoManager().beginNewTransaction("Fade In");
                        auto* undoAction = new FadeInUndoAction(
                            doc->getBufferManager(),
                            doc->getWaveformDisplay(),
                            doc->getAudioEngine(),
                            *beforeBuffer,
                            startSampleInt,
                            numSamples,
                            curveType
                        );
                        undoAction->markAsAlreadyPerformed();
                        doc->getUndoManager().perform(undoAction);
                        doc->setModified(true);

                        // Update waveform display
                        doc->getAudioEngine().reloadBufferPreservingPlayback(
                            buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                        doc->getWaveformDisplay().reloadFromBuffer(
                            buf, doc->getAudioEngine().getSampleRate(), true, true);
                    }
                    else
                    {
                        // Cancelled: Restore buffer from snapshot
                        auto& buf = doc->getBufferManager().getMutableBuffer();
                        for (int ch = 0; ch < beforeBuffer->getNumChannels(); ++ch)
                        {
                            buf.copyFrom(ch, startSampleInt, *beforeBuffer, ch, 0, numSamples);
                        }
                        doc->getAudioEngine().reloadBufferPreservingPlayback(
                            buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                        doc->getWaveformDisplay().reloadFromBuffer(
                            buf, doc->getAudioEngine().getSampleRate(), true, true);
                    }
                }
            );
        }
        else
        {
            // SYNCHRONOUS PATH: Small operation - existing behavior
            doc->getUndoManager().beginNewTransaction("Fade In");

            auto* undoAction = new FadeInUndoAction(
                doc->getBufferManager(),
                doc->getWaveformDisplay(),
                doc->getAudioEngine(),
                *beforeBuffer,
                startSampleInt,
                numSamples,
                curveType
            );

            doc->getUndoManager().perform(undoAction);
            doc->setModified(true);
        }
    });

    dialog.onCancel([&dialog]() {
        if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState(0);
    });

    // Show dialog modally
    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(&dialog);  // Use setNonOwned for stack-allocated dialog
    options.componentToCentreAround = parent;
    options.dialogTitle = "Fade In";
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    options.runModal();
}

/**
 * Show fade out dialog and apply fade to selection.
 * Requires selection (won't work on entire file).
 */
void DSPController::showFadeOutDialog(Document* doc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    // Fade out requires a selection
    if (!doc->getWaveformDisplay().hasSelection())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Fade Out",
            "Please select a region of audio to fade out.",
            "OK");
        return;
    }

    // Get selection bounds
    auto& waveform = doc->getWaveformDisplay();
    auto& engine = doc->getAudioEngine();
    double sampleRate = engine.getSampleRate();
    int64_t startSample = static_cast<int64_t>(waveform.getSelectionStart() * sampleRate);
    int64_t endSample = static_cast<int64_t>(waveform.getSelectionEnd() * sampleRate);

    // Create and configure dialog
    FadeDialog dialog(FadeDirection::FadeOut, &doc->getAudioEngine(), &doc->getBufferManager(), startSample, endSample);

    // Set up callbacks
    dialog.onApply([this, doc, &dialog]() {
        // Get selection
        int startSampleInt = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
        int endSampleInt = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
        int numSamples = endSampleInt - startSampleInt;

        // Store before state for undo (MUST happen before any processing)
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        auto beforeBuffer = std::make_shared<juce::AudioBuffer<float>>();
        beforeBuffer->setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            beforeBuffer->copyFrom(ch, 0, buffer, ch, startSampleInt, numSamples);
        }

        // Get selected curve type from dialog
        FadeCurveType curveType = dialog.getSelectedCurveType();

        // Close the dialog first (before potentially showing progress dialog)
        if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState(1);

        // Check if we need progress dialog for large operations
        if (numSamples >= kProgressDialogThreshold)
        {
            // ASYNCHRONOUS PATH: Large operation - show progress dialog
            // Create a working copy of the selection region for processing
            auto regionBuffer = std::make_shared<juce::AudioBuffer<float>>();
            regionBuffer->setSize(buffer.getNumChannels(), numSamples);
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                regionBuffer->copyFrom(ch, 0, buffer, ch, startSampleInt, numSamples);
            }

            ProgressDialog::runWithProgress(
                "Fade Out",
                [regionBuffer, numSamples, curveType](ProgressCallback progress) -> bool {
                    // Apply fade to the region copy (not the main buffer)
                    return AudioProcessor::fadeOutWithProgress(*regionBuffer, numSamples, curveType, progress);
                },
                [doc, beforeBuffer, regionBuffer, startSampleInt, numSamples, curveType](bool success) {
                    if (success)
                    {
                        // Copy processed region back to main buffer at correct position
                        auto& buf = doc->getBufferManager().getMutableBuffer();
                        for (int ch = 0; ch < regionBuffer->getNumChannels(); ++ch)
                        {
                            buf.copyFrom(ch, startSampleInt, *regionBuffer, ch, 0, numSamples);
                        }

                        // Register undo action (operation already applied)
                        doc->getUndoManager().beginNewTransaction("Fade Out");
                        auto* undoAction = new FadeOutUndoAction(
                            doc->getBufferManager(),
                            doc->getWaveformDisplay(),
                            doc->getAudioEngine(),
                            *beforeBuffer,
                            startSampleInt,
                            numSamples,
                            curveType
                        );
                        undoAction->markAsAlreadyPerformed();
                        doc->getUndoManager().perform(undoAction);
                        doc->setModified(true);

                        // Update waveform display
                        doc->getAudioEngine().reloadBufferPreservingPlayback(
                            buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                        doc->getWaveformDisplay().reloadFromBuffer(
                            buf, doc->getAudioEngine().getSampleRate(), true, true);
                    }
                    else
                    {
                        // Cancelled: Restore buffer from snapshot
                        auto& buf = doc->getBufferManager().getMutableBuffer();
                        for (int ch = 0; ch < beforeBuffer->getNumChannels(); ++ch)
                        {
                            buf.copyFrom(ch, startSampleInt, *beforeBuffer, ch, 0, numSamples);
                        }
                        doc->getAudioEngine().reloadBufferPreservingPlayback(
                            buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                        doc->getWaveformDisplay().reloadFromBuffer(
                            buf, doc->getAudioEngine().getSampleRate(), true, true);
                    }
                }
            );
        }
        else
        {
            // SYNCHRONOUS PATH: Small operation - existing behavior
            doc->getUndoManager().beginNewTransaction("Fade Out");

            auto* undoAction = new FadeOutUndoAction(
                doc->getBufferManager(),
                doc->getWaveformDisplay(),
                doc->getAudioEngine(),
                *beforeBuffer,
                startSampleInt,
                numSamples,
                curveType
            );

            doc->getUndoManager().perform(undoAction);
            doc->setModified(true);
        }
    });

    dialog.onCancel([&dialog]() {
        if (auto* window = dialog.findParentComponentOfClass<juce::DialogWindow>())
            window->exitModalState(0);
    });

    // Show dialog modally
    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(&dialog);  // Use setNonOwned for stack-allocated dialog
    options.componentToCentreAround = parent;
    options.dialogTitle = "Fade Out";
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;

    options.runModal();
}

/**
 * Apply DC offset removal automatically (no dialog).
 * Works on selection if present, otherwise on entire file.
 */
void DSPController::applyDCOffset(Document* doc)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    try
    {
        // Determine if we have a selection or should process entire file
        auto& waveform = doc->getWaveformDisplay();
        auto& engine = doc->getAudioEngine();
        bool hasSelection = waveform.hasSelection();

        // Get bounds (selection or entire file)
        double sampleRate = engine.getSampleRate();
        int startSampleInt, endSampleInt, numSamples;

        if (hasSelection)
        {
            startSampleInt = doc->getBufferManager().timeToSample(waveform.getSelectionStart());
            endSampleInt = doc->getBufferManager().timeToSample(waveform.getSelectionEnd());
        }
        else
        {
            startSampleInt = 0;
            endSampleInt = doc->getBufferManager().getBuffer().getNumSamples();
        }
        numSamples = endSampleInt - startSampleInt;

        if (numSamples <= 0)
            return;

        // Store before state for undo (MUST happen before any processing)
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        auto beforeBuffer = std::make_shared<juce::AudioBuffer<float>>();
        beforeBuffer->setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            beforeBuffer->copyFrom(ch, 0, buffer, ch, startSampleInt, numSamples);
        }

        juce::String transactionName = hasSelection ? "Remove DC Offset (selection)" : "Remove DC Offset (entire file)";

        // Check if we need progress dialog for large operations
        if (numSamples >= kProgressDialogThreshold)
    {
        // ASYNCHRONOUS PATH: Large operation - show progress dialog
        // Create a working copy of the region for processing
        auto regionBuffer = std::make_shared<juce::AudioBuffer<float>>();
        regionBuffer->setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            regionBuffer->copyFrom(ch, 0, buffer, ch, startSampleInt, numSamples);
        }

        ProgressDialog::runWithProgress(
            "Remove DC Offset",
            [regionBuffer](ProgressCallback progress) -> bool {
                // Apply DC offset removal to the region copy (not the main buffer)
                return AudioProcessor::removeDCOffsetWithProgress(*regionBuffer, progress);
            },
            [doc, beforeBuffer, regionBuffer, startSampleInt, numSamples, transactionName](bool success) {
                if (success)
                {
                    // Copy processed region back to main buffer at correct position
                    auto& buf = doc->getBufferManager().getMutableBuffer();
                    for (int ch = 0; ch < regionBuffer->getNumChannels(); ++ch)
                    {
                        buf.copyFrom(ch, startSampleInt, *regionBuffer, ch, 0, numSamples);
                    }

                    // Register undo action (operation already applied)
                    doc->getUndoManager().beginNewTransaction(transactionName);
                    auto* undoAction = new DCOffsetRemovalUndoAction(
                        doc->getBufferManager(),
                        doc->getWaveformDisplay(),
                        doc->getAudioEngine(),
                        *beforeBuffer,
                        startSampleInt,
                        numSamples
                    );
                    undoAction->markAsAlreadyPerformed();
                    doc->getUndoManager().perform(undoAction);
                    doc->setModified(true);

                    // Update waveform display
                    doc->getAudioEngine().reloadBufferPreservingPlayback(
                        buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                    doc->getWaveformDisplay().reloadFromBuffer(
                        buf, doc->getAudioEngine().getSampleRate(), true, true);
                }
                else
                {
                    // Cancelled: Restore buffer from snapshot
                    auto& buf = doc->getBufferManager().getMutableBuffer();
                    for (int ch = 0; ch < beforeBuffer->getNumChannels(); ++ch)
                    {
                        buf.copyFrom(ch, startSampleInt, *beforeBuffer, ch, 0, numSamples);
                    }
                    // Update display to show restored state
                    doc->getAudioEngine().reloadBufferPreservingPlayback(
                        buf, doc->getBufferManager().getSampleRate(), buf.getNumChannels());
                    doc->getWaveformDisplay().reloadFromBuffer(
                        buf, doc->getAudioEngine().getSampleRate(), true, true);
                }
            }
        );
    }
    else
    {
        // SYNCHRONOUS PATH: Small operation
        doc->getUndoManager().beginNewTransaction(transactionName);

        auto* undoAction = new DCOffsetRemovalUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            *beforeBuffer,
            startSampleInt,
            numSamples
        );

        doc->getUndoManager().perform(undoAction);
        doc->setModified(true);
    }
    }
    catch (const std::exception& e)
    {
        DBG("DSPController::applyDCOffset - Error: " + juce::String(e.what()));
        ErrorDialog::show("Error", "DC offset removal failed: " + juce::String(e.what()));
    }
}

void DSPController::showParametricEQDialog(Document* doc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    ParametricEQDialog dialog(&doc->getAudioEngine(), &doc->getBufferManager());

    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(&dialog);
    options.componentToCentreAround = parent;
    options.dialogTitle = "Parametric EQ";
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;

    options.runModal();
}

void DSPController::showGraphicalEQDialog(Document* doc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    // TODO: Implement graphical EQ dialog in DSPController
    // This requires review of GraphicalEQEditor constructor and integration
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Graphical EQ",
        "Graphical EQ is not yet fully implemented in DSPController.",
        "OK");
}

void DSPController::showChannelConverterDialog(Document* doc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    // TODO: Implement channel converter dialog
    // This requires proper integration with ChannelConverterDialog and ChannelConvertUndoAction
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Channel Converter",
        "Channel converter is not yet fully implemented in DSPController.",
        "OK");
}

void DSPController::showChannelExtractorDialog(Document* doc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    // TODO: Implement channel extractor dialog
    // This requires proper integration with ChannelExtractorDialog and ChannelExtractUndoAction
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Channel Extractor",
        "Channel extractor is not yet fully implemented in DSPController.",
        "OK");
}

//==============================================================================
// Plugin chain operations

void DSPController::applyPluginChainToSelectionWithOptions(Document* doc, bool convertToStereo, bool includeTail, double tailLengthSeconds)
{
    applyPluginChainToSelectionInternal(doc, convertToStereo, includeTail, tailLengthSeconds);
}

void DSPController::applyPluginChainToSelection(Document* doc)
{
    applyPluginChainToSelectionInternal(doc, false, false, 0.0);
}

void DSPController::applyPluginChainToSelectionInternal(Document* doc, bool convertToStereo, bool includeTail, double tailLengthSeconds)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    try
    {
        // TODO: Implement plugin chain application in DSPController
        // This requires integration with the PluginChain system and off-line rendering
        DBG("Plugin chain application not yet fully implemented in DSPController");
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("DSPController::applyPluginChainToSelectionInternal - Error: " + juce::String(e.what()));
        ErrorDialog::show("Error", "Plugin chain application failed: " + juce::String(e.what()));
    }
}

void DSPController::showOfflinePluginDialog(Document* doc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    // TODO: Implement offline plugin dialog in DSPController
    // This requires integration with OfflinePluginDialog and plugin processing
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Offline Plugin",
        "Offline plugin processing is not yet fully implemented in DSPController.",
        "OK");
}

void DSPController::applyOfflinePlugin(Document* doc, const juce::PluginDescription& pluginDesc, juce::Component* parent)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    // TODO: Implement offline plugin application in DSPController
    // This requires integration with the plugin system and off-line rendering
    DBG("Offline plugin application not yet fully implemented in DSPController");
}

//==============================================================================
// Other DSP operations

/**
 * Apply normalization to entire file or selection.
 * This is a convenience method that normalizes to 0dB peak.
 */
void DSPController::applyNormalize(Document* doc)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    try
    {
        // Get current buffer
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        int startSample = 0;
        int numSamples = buffer.getNumSamples();
        bool isSelection = false;

        if (doc->getWaveformDisplay().hasSelection())
        {
            startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
            int endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
            numSamples = endSample - startSample;
            isSelection = true;
        }

        if (numSamples <= 0)
            return;

        // Store before state for undo
        juce::AudioBuffer<float> beforeBuffer;
        beforeBuffer.setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            beforeBuffer.copyFrom(ch, 0, buffer, ch, startSample, numSamples);
        }

        // Find peak level
        float peakLevel = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const auto* samples = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                peakLevel = juce::jmax(peakLevel, std::abs(samples[startSample + i]));
            }
        }

        // Calculate gain needed to reach 0dB
        float gainDB = 0.0f;
        if (peakLevel > 0.0f)
        {
            gainDB = 20.0f * std::log10(1.0f / peakLevel);
        }

        juce::String transactionName = juce::String::formatted(
            "Normalize to 0dB (%s)",
            isSelection ? "selection" : "entire file"
        );

        // Start transaction
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action
        auto* undoAction = new GainUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer,
            startSample,
            numSamples,
            gainDB,
            isSelection
        );

        doc->getUndoManager().perform(undoAction);
        doc->setModified(true);
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("DSPController::applyNormalize - Error: " + juce::String(e.what()));
        ErrorDialog::show("Error", "Normalize operation failed: " + juce::String(e.what()));
    }
}

/**
 * Apply fade in curve to selection.
 */
void DSPController::applyFadeIn(Document* doc)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    try
    {
        // Require selection
        if (!doc->getWaveformDisplay().hasSelection())
        {
            DBG("Fade In requires a selection");
            return;
        }

        // Get selection
        int startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
        int endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
        int numSamples = endSample - startSample;

        if (numSamples <= 0)
            return;

        // Store before state for undo
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        juce::AudioBuffer<float> beforeBuffer;
        beforeBuffer.setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            beforeBuffer.copyFrom(ch, 0, buffer, ch, startSample, numSamples);
        }

        // Create undo action
        doc->getUndoManager().beginNewTransaction("Fade In");
        auto* undoAction = new FadeInUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer,
            startSample,
            numSamples,
            FadeCurveType::LINEAR
        );

        doc->getUndoManager().perform(undoAction);
        doc->setModified(true);
    }
    catch (const std::exception& e)
    {
        DBG("DSPController::applyFadeIn - Error: " + juce::String(e.what()));
        ErrorDialog::show("Error", "Fade In operation failed: " + juce::String(e.what()));
    }
}

/**
 * Apply fade out curve to selection.
 */
void DSPController::applyFadeOut(Document* doc)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    try
    {
        // Require selection
        if (!doc->getWaveformDisplay().hasSelection())
        {
            DBG("Fade Out requires a selection");
            return;
        }

        // Get selection
        int startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
        int endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
        int numSamples = endSample - startSample;

        if (numSamples <= 0)
            return;

        // Store before state for undo
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        juce::AudioBuffer<float> beforeBuffer;
        beforeBuffer.setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            beforeBuffer.copyFrom(ch, 0, buffer, ch, startSample, numSamples);
        }

        // Create undo action
        doc->getUndoManager().beginNewTransaction("Fade Out");
        auto* undoAction = new FadeOutUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer,
            startSample,
            numSamples,
            FadeCurveType::LINEAR
        );

        doc->getUndoManager().perform(undoAction);
        doc->setModified(true);
    }
    catch (const std::exception& e)
    {
        DBG("DSPController::applyFadeOut - Error: " + juce::String(e.what()));
        ErrorDialog::show("Error", "Fade Out operation failed: " + juce::String(e.what()));
    }
}

/**
 * Silence the current selection (set all samples to zero).
 */
void DSPController::silenceSelection(Document* doc)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    try
    {
        // Require selection
        if (!doc->getWaveformDisplay().hasSelection())
        {
            DBG("Silence requires a selection");
            return;
        }

        // Get selection
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        int startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
        int endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());
        int numSamples = endSample - startSample;

        if (numSamples <= 0)
            return;

        // Store before state for undo
        juce::AudioBuffer<float> beforeBuffer;
        beforeBuffer.setSize(buffer.getNumChannels(), numSamples);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            beforeBuffer.copyFrom(ch, 0, buffer, ch, startSample, numSamples);
        }

        // Create undo action
        doc->getUndoManager().beginNewTransaction("Silence");
        auto* undoAction = new SilenceUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer,
            startSample,
            numSamples
        );

        doc->getUndoManager().perform(undoAction);
        doc->setModified(true);
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("DSPController::silenceSelection - Error: " + juce::String(e.what()));
        ErrorDialog::show("Error", "Silence operation failed: " + juce::String(e.what()));
    }
}

/**
 * Reverse the selection (or entire file if no selection).
 * Reverse is self-inverse so undo simply re-reverses.
 */
void DSPController::reverseSelection(Document* doc)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    try
    {
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        if (buffer.getNumSamples() == 0)
            return;

        // Determine region to process
        int startSample = 0;
        int numSamples = buffer.getNumSamples();
        bool isSelection = false;

        if (doc->getWaveformDisplay().hasSelection())
        {
            startSample = static_cast<int>(doc->getBufferManager().timeToSample(
                doc->getWaveformDisplay().getSelectionStart()));
            int endSample = static_cast<int>(doc->getBufferManager().timeToSample(
                doc->getWaveformDisplay().getSelectionEnd()));
            numSamples = endSample - startSample;
            isSelection = true;
        }

        if (numSamples <= 0)
            return;

        // Create undo action (no before buffer needed -- reverse is self-inverse)
        juce::String transactionName = isSelection ? "Reverse Selection" : "Reverse";
        doc->getUndoManager().beginNewTransaction(transactionName);
        doc->getUndoManager().perform(new ReverseUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            startSample,
            numSamples,
            isSelection
        ));

        doc->setModified(true);
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("DSPController::reverseSelection - Error: " + juce::String(e.what()));
        ErrorDialog::show("Error", "Reverse failed: " + juce::String(e.what()));
    }
}

/**
 * Invert polarity of the selection (or entire file if no selection).
 * Invert is self-inverse so undo simply re-inverts.
 */
void DSPController::invertSelection(Document* doc)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    try
    {
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        if (buffer.getNumSamples() == 0)
            return;

        // Determine region to process
        int startSample = 0;
        int numSamples = buffer.getNumSamples();
        bool isSelection = false;

        if (doc->getWaveformDisplay().hasSelection())
        {
            startSample = static_cast<int>(doc->getBufferManager().timeToSample(
                doc->getWaveformDisplay().getSelectionStart()));
            int endSample = static_cast<int>(doc->getBufferManager().timeToSample(
                doc->getWaveformDisplay().getSelectionEnd()));
            numSamples = endSample - startSample;
            isSelection = true;
        }

        if (numSamples <= 0)
            return;

        // Create undo action (no before buffer needed -- invert is self-inverse)
        juce::String transactionName = isSelection ? "Invert Selection" : "Invert";
        doc->getUndoManager().beginNewTransaction(transactionName);
        doc->getUndoManager().perform(new InvertUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            startSample,
            numSamples,
            isSelection
        ));

        doc->setModified(true);
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("DSPController::invertSelection - Error: " + juce::String(e.what()));
        ErrorDialog::show("Error", "Invert failed: " + juce::String(e.what()));
    }
}

/**
 * Show resample dialog and apply resampling to the document.
 * Uses a simple AlertWindow with a combo box for sample rate selection.
 */
void DSPController::showResampleDialog(Document* doc, juce::Component* parent)
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

/**
 * Trim to selection (delete everything outside the selection).
 */
void DSPController::trimToSelection(Document* doc)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
        return;

    try
    {
        // Require selection
        if (!doc->getWaveformDisplay().hasSelection())
        {
            DBG("Trim requires a selection");
            return;
        }

        // Get selection
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        int startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
        int endSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionEnd());

        if (startSample < 0 || endSample > buffer.getNumSamples() || startSample >= endSample)
        {
            DBG("Invalid trim selection");
            return;
        }

        // Store entire buffer before for undo
        juce::AudioBuffer<float> beforeBuffer;
        beforeBuffer.makeCopyOf(buffer, true);

        // Create undo action
        doc->getUndoManager().beginNewTransaction("Trim to Selection");
        auto* undoAction = new TrimUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer,
            startSample,
            endSample
        );

        doc->getUndoManager().perform(undoAction);
        doc->setModified(true);
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("DSPController::trimToSelection - Error: " + juce::String(e.what()));
        ErrorDialog::show("Error", "Trim operation failed: " + juce::String(e.what()));
    }
}

/**
 * Remove DC offset from entire file.
 */
void DSPController::applyDCOffsetRemoval(Document* doc)
{
    if (!doc) return;

    try
    {
        if (!doc->getAudioEngine().isFileLoaded())
        {
            return;
        }

        // Get current buffer
        auto& buffer = doc->getBufferManager().getMutableBuffer();
        if (buffer.getNumSamples() == 0)
        {
            return;
        }

        // Store entire buffer before processing for undo
        juce::AudioBuffer<float> beforeBuffer;
        beforeBuffer.makeCopyOf(buffer, true);

        // Start a new transaction
        juce::String transactionName = "Remove DC Offset (entire file)";
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action
        auto* undoAction = new DCOffsetRemovalUndoAction(
            doc->getBufferManager(),
            doc->getWaveformDisplay(),
            doc->getAudioEngine(),
            beforeBuffer
        );

        // perform() calls DCOffsetRemovalUndoAction::perform() which removes DC offset and updates display
        doc->getUndoManager().perform(undoAction);

        // Mark as modified
        doc->setModified(true);

        // Log the operation
        juce::String message = "Removed DC offset from entire file";
        DBG(message);
    }
    catch (const std::exception& e)
    {
        DBG("DSPController::applyDCOffsetRemoval - Error: " + juce::String(e.what()));
        ErrorDialog::show("Error", "DC offset removal failed: " + juce::String(e.what()));
    }
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

void DSPController::showHeadTailDialog(Document* doc, juce::Component* parent)
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
    options.dialogBackgroundColour    = juce::Colour(0xff2a2a2a);
    options.content.setOwned(dialog);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = true;
    options.resizable                    = false;
    options.launchAsync();
}

void DSPController::showLoopingToolsDialog(Document* doc, juce::Component* parent)
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
    options.dialogBackgroundColour    = juce::Colour(0xff2a2a2a);
    options.content.setOwned(dialog);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar            = true;
    options.resizable                    = false;
    options.launchAsync();
}
