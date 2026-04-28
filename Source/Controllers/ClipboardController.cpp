/*
  ==============================================================================

    ClipboardController.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "ClipboardController.h"
#include "../Utils/Document.h"
#include "../Utils/DocumentManager.h"
#include "../Utils/AudioClipboard.h"
#include "../Utils/UndoableEdits.h"
#include "../Utils/UndoActions/AudioUndoActions.h"
#include "../UI/ErrorDialog.h"

ClipboardController::ClipboardController(DocumentManager& docManager)
    : m_documentManager(docManager)
{
}

void ClipboardController::selectAll(Document* doc)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
    {
        return;
    }

    doc->getWaveformDisplay().setSelection(0.0, doc->getBufferManager().getLengthInSeconds());
    DBG("Selected all audio");
}

bool ClipboardController::validateSelection(Document* doc) const
{
    if (!doc || !doc->getWaveformDisplay().hasSelection())
    {
        return false;
    }

    auto selectionStart = doc->getWaveformDisplay().getSelectionStart();
    auto selectionEnd = doc->getWaveformDisplay().getSelectionEnd();

    // Check for negative times
    if (selectionStart < 0.0 || selectionEnd < 0.0)
    {
        DBG(juce::String::formatted(
            "Invalid selection: negative time (start=%.6f, end=%.6f)",
            selectionStart, selectionEnd));
        return false;
    }

    // Check for times beyond duration
    double maxDuration = doc->getBufferManager().getLengthInSeconds();
    if (selectionStart > maxDuration || selectionEnd > maxDuration)
    {
        DBG(juce::String::formatted(
            "Invalid selection: beyond duration (start=%.6f, end=%.6f, max=%.6f)",
            selectionStart, selectionEnd, maxDuration));
        return false;
    }

    // Check for inverted selection (should not happen, but be safe)
    if (selectionStart >= selectionEnd)
    {
        DBG(juce::String::formatted(
            "Invalid selection: start >= end (start=%.6f, end=%.6f)",
            selectionStart, selectionEnd));
        return false;
    }

    return true;
}

void ClipboardController::copySelection(Document* doc)
{
    if (!doc) return;

    try
    {
        // CRITICAL FIX (2025-10-07): Validate selection before converting to samples
        if (!validateSelection(doc) || !doc->getBufferManager().hasAudioData())
        {
            return;
        }

        auto selectionStart = doc->getWaveformDisplay().getSelectionStart();
        auto selectionEnd = doc->getWaveformDisplay().getSelectionEnd();

        // Convert time to samples (now safe - selection is validated)
        auto startSample = doc->getBufferManager().timeToSample(selectionStart);
        auto endSample = doc->getBufferManager().timeToSample(selectionEnd);

        // CRITICAL FIX (2025-10-08): getAudioRange expects COUNT, not end position!
        // Calculate number of samples in selection
        auto numSamples = endSample - startSample;

        // Get focused channels for per-channel copy
        int focusedChannels = doc->getWaveformDisplay().getFocusedChannels();

        // Get the audio data for the selection (respecting focused channels)
        auto audioRange = doc->getBufferManager().getAudioRangeForChannels(startSample, numSamples, focusedChannels);

        if (audioRange.getNumSamples() > 0)
        {
            AudioClipboard::getInstance().copyAudio(audioRange, doc->getBufferManager().getSampleRate());

            if (focusedChannels != -1)
            {
                // Count focused channels for log message
                int focusedCount = 0;
                for (int ch = 0; ch < doc->getBufferManager().getNumChannels(); ++ch)
                    if ((focusedChannels & (1 << ch)) != 0) focusedCount++;

                DBG(juce::String::formatted(
                    "Copied %.2f seconds (%d channel%s) to clipboard",
                    selectionEnd - selectionStart, focusedCount, focusedCount != 1 ? "s" : ""));
            }
            else
            {
                DBG(juce::String::formatted(
                    "Copied %.2f seconds to clipboard", selectionEnd - selectionStart));
            }
        }
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("ClipboardController::copySelection - " + juce::String(e.what()));
        ErrorDialog::show("Error", "Copy failed: " + juce::String(e.what()));
    }
}

void ClipboardController::cutSelection(Document* doc)
{
    if (!doc) return;

    try
    {
        // CRITICAL FIX (2025-10-07): Validate selection before operations
        if (!validateSelection(doc) || !doc->getBufferManager().hasAudioData())
        {
            return;
        }

        // Get focused channels before any operations
        int focusedChannels = doc->getWaveformDisplay().getFocusedChannels();

        // First copy to clipboard (will respect focused channels)
        copySelection(doc);

        // Then delete/silence the selection (will respect focused channels)
        deleteSelection(doc);

        if (focusedChannels != -1)
        {
            int focusedCount = 0;
            for (int ch = 0; ch < doc->getBufferManager().getNumChannels(); ++ch)
                if ((focusedChannels & (1 << ch)) != 0) focusedCount++;

            DBG(juce::String::formatted(
                "Cut %d channel%s to clipboard (silenced)", focusedCount, focusedCount != 1 ? "s" : ""));
        }
        else
        {
            DBG("Cut selection to clipboard");
        }
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("ClipboardController::cutSelection - " + juce::String(e.what()));
        ErrorDialog::show("Error", "Cut failed: " + juce::String(e.what()));
    }
}

void ClipboardController::pasteAtCursor(Document* doc)
{
    if (!doc || !AudioClipboard::getInstance().hasAudio() || !doc->getBufferManager().hasAudioData())
    {
        return;
    }

    try
    {
        // Use edit cursor position if available, otherwise use playback position
        double cursorPos;
        if (doc->getWaveformDisplay().hasEditCursor())
        {
            cursorPos = doc->getWaveformDisplay().getEditCursorPosition();
        }
        else
        {
            cursorPos = doc->getWaveformDisplay().getPlaybackPosition();
        }

        auto insertSample = doc->getBufferManager().timeToSample(cursorPos);

        // Check for sample rate mismatch
        auto clipboardSampleRate = AudioClipboard::getInstance().getSampleRate();
        auto currentSampleRate = doc->getBufferManager().getSampleRate();

        if (std::abs(clipboardSampleRate - currentSampleRate) > 0.01)
        {
            auto result = juce::NativeMessageBox::showOkCancelBox(
                juce::MessageBoxIconType::WarningIcon,
                "Sample Rate Mismatch",
                juce::String::formatted(
                    "The clipboard audio has a different sample rate (%.0f Hz) "
                    "than the current file (%.0f Hz).\n\n"
                    "Paste anyway? (May affect pitch and speed)",
                    clipboardSampleRate, currentSampleRate),
                nullptr,
                nullptr);

            if (!result)
            {
                return;
            }
        }

        // Get clipboard audio
        auto clipboardAudio = AudioClipboard::getInstance().getAudio();

        if (clipboardAudio.getNumSamples() <= 0)
        {
            return;
        }

        // Check if we're in per-channel edit mode
        int focusedChannels = doc->getWaveformDisplay().getFocusedChannels();

        if (focusedChannels != -1)
        {
            // Per-channel mode: replace focused channels only (no length change)
            doc->getUndoManager().beginNewTransaction("Paste to Channels");

            // Determine paste location and length
            int64_t startSample = insertSample;
            int numSamples = clipboardAudio.getNumSamples();

            // If there's a selection, use selection start
            if (doc->getWaveformDisplay().hasSelection() && validateSelection(doc))
            {
                startSample = doc->getBufferManager().timeToSample(doc->getWaveformDisplay().getSelectionStart());
            }

            // Clamp to buffer length
            int64_t maxSamples = doc->getBufferManager().getNumSamples() - startSample;
            if (numSamples > maxSamples)
            {
                numSamples = static_cast<int>(maxSamples);
            }

            if (numSamples <= 0)
            {
                DBG("Paste: No room at cursor position");
                return;
            }

            // Truncate clipboard if necessary
            juce::AudioBuffer<float> pasteAudio;
            if (numSamples < clipboardAudio.getNumSamples())
            {
                pasteAudio.setSize(clipboardAudio.getNumChannels(), numSamples);
                for (int ch = 0; ch < clipboardAudio.getNumChannels(); ++ch)
                {
                    pasteAudio.copyFrom(ch, 0, clipboardAudio, ch, 0, numSamples);
                }
            }
            else
            {
                pasteAudio = clipboardAudio;
            }

            // Get the before state for the focused channels
            auto beforeBuffer = doc->getBufferManager().getAudioRangeForChannels(
                startSample, pasteAudio.getNumSamples(), focusedChannels);

            // Create undoable action for channel replacement
            auto replaceAction = new ReplaceChannelsAction(
                doc->getBufferManager(),
                doc->getWaveformDisplay(),
                doc->getAudioEngine(),
                static_cast<int>(startSample),
                beforeBuffer,
                pasteAudio,
                focusedChannels
            );

            // Perform the replace and add to undo manager
            doc->getUndoManager().perform(replaceAction);

            // Count focused channels for log message
            int focusedCount = 0;
            for (int ch = 0; ch < doc->getBufferManager().getNumChannels(); ++ch)
                if ((focusedChannels & (1 << ch)) != 0) focusedCount++;

            DBG(juce::String::formatted(
                "Pasted %.2f seconds to %d channel%s (per-channel paste)",
                pasteAudio.getNumSamples() / currentSampleRate,
                focusedCount, focusedCount != 1 ? "s" : ""));
        }
        else
        {
            // Normal mode: insert or replace selection
            doc->getUndoManager().beginNewTransaction("Paste");

            // If there's a selection, replace it; otherwise insert at cursor
            if (doc->getWaveformDisplay().hasSelection() && validateSelection(doc))
            {
                auto selectionStart = doc->getWaveformDisplay().getSelectionStart();
                auto selectionEnd = doc->getWaveformDisplay().getSelectionEnd();
                auto startSample = doc->getBufferManager().timeToSample(selectionStart);
                auto endSample = doc->getBufferManager().timeToSample(selectionEnd);

                // Create undoable replace action
                auto replaceAction = new ReplaceAction(
                    doc->getBufferManager(),
                    doc->getAudioEngine(),
                    doc->getWaveformDisplay(),
                    startSample,
                    endSample - startSample,
                    clipboardAudio
                );

                // Perform the replace and add to undo manager
                doc->getUndoManager().perform(replaceAction);

                DBG(juce::String::formatted(
                    "Pasted %.2f seconds from clipboard, replacing selection (undoable)",
                    clipboardAudio.getNumSamples() / currentSampleRate));
            }
            else
            {
                // Create undoable insert action
                auto insertAction = new InsertAction(
                    doc->getBufferManager(),
                    doc->getAudioEngine(),
                    doc->getWaveformDisplay(),
                    insertSample,
                    clipboardAudio
                );

                // Perform the insert and add to undo manager
                doc->getUndoManager().perform(insertAction);

                DBG(juce::String::formatted(
                    "Pasted %.2f seconds from clipboard (undoable)",
                    clipboardAudio.getNumSamples() / currentSampleRate));
            }
        }

        // Mark as modified
        doc->setModified(true);

        // Clear selection after paste
        doc->getWaveformDisplay().clearSelection();

        // NOTE: Waveform display already updated by undo action's updatePlaybackAndDisplay()
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("ClipboardController::pasteAtCursor - " + juce::String(e.what()));
        ErrorDialog::show("Error", "Paste failed: " + juce::String(e.what()));
    }
}

void ClipboardController::deleteSelection(Document* doc)
{
    if (!doc) return;

    try
    {
        // CRITICAL FIX (2025-10-07): Validate selection before converting to samples
        if (!validateSelection(doc) || !doc->getBufferManager().hasAudioData())
        {
            return;
        }

        auto selectionStart = doc->getWaveformDisplay().getSelectionStart();
        auto selectionEnd = doc->getWaveformDisplay().getSelectionEnd();

        // Convert time to samples (now safe - selection is validated)
        auto startSample = doc->getBufferManager().timeToSample(selectionStart);
        auto endSample = doc->getBufferManager().timeToSample(selectionEnd);
        auto numSamples = endSample - startSample;

        // Check if we're in per-channel edit mode
        int focusedChannels = doc->getWaveformDisplay().getFocusedChannels();

        if (focusedChannels != -1)
        {
            // Per-channel mode: silence focused channels instead of deleting
            // (Can't delete samples from specific channels - would make channels different lengths)
            doc->getUndoManager().beginNewTransaction("Silence Channels");

            // Get the before state for the focused channels
            auto beforeBuffer = doc->getBufferManager().getAudioRangeForChannels(startSample, numSamples, focusedChannels);

            // Create undoable silence action for specific channels
            auto silenceAction = new SilenceChannelsUndoAction(
                doc->getBufferManager(),
                doc->getWaveformDisplay(),
                doc->getAudioEngine(),
                beforeBuffer,
                static_cast<int>(startSample),
                static_cast<int>(numSamples),
                focusedChannels
            );

            // Perform the silence and add to undo manager
            doc->getUndoManager().perform(silenceAction);

            // Count focused channels for log message
            int focusedCount = 0;
            for (int ch = 0; ch < doc->getBufferManager().getNumChannels(); ++ch)
                if ((focusedChannels & (1 << ch)) != 0) focusedCount++;

            DBG(juce::String::formatted(
                "Silenced %.2f seconds on %d channel%s (per-channel delete)",
                selectionEnd - selectionStart, focusedCount, focusedCount != 1 ? "s" : ""));
        }
        else
        {
            // Normal mode: delete all channels (removes samples, changes file length)
            doc->getUndoManager().beginNewTransaction("Delete");

            // Create undoable delete action (with region manager and display for undo support)
            auto deleteAction = new DeleteAction(
                doc->getBufferManager(),
                doc->getAudioEngine(),
                doc->getWaveformDisplay(),
                startSample,
                endSample - startSample,
                &doc->getRegionManager(),  // Pass region manager for undo support
                &doc->getRegionDisplay()   // Pass region display to update visuals after undo
            );

            // Perform the delete and add to undo manager
            doc->getUndoManager().perform(deleteAction);

            DBG(juce::String::formatted(
                "Deleted %.2f seconds (undoable), cursor set at %.2f",
                selectionEnd - selectionStart, selectionStart));
        }

        // Mark as modified
        doc->setModified(true);

        // Clear selection after delete
        doc->getWaveformDisplay().clearSelection();

        // CRITICAL FIX (Phase 1.4 - Edit Cursor Preservation):
        // Set edit cursor at the deletion point for professional workflow
        doc->getWaveformDisplay().setEditCursor(selectionStart);
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("ClipboardController::deleteSelection - " + juce::String(e.what()));
        ErrorDialog::show("Error", "Delete failed: " + juce::String(e.what()));
    }
}
