/*
  ==============================================================================

    ClipboardController.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class Document;
class DocumentManager;

/**
 * Controller for clipboard operations: select, copy, cut, paste, delete.
 *
 * Handles:
 * - selectAll(): Select entire audio duration
 * - validateSelection(): Validate selection bounds and return bool
 * - copySelection(): Copy selection (respecting focused channels)
 * - cutSelection(): Copy then delete/silence selection
 * - pasteAtCursor(): Paste from clipboard (handles per-channel and normal modes, insert vs replace)
 * - deleteSelection(): Delete or silence selection (respecting focused channels)
 *
 * All methods operate on a Document passed as parameter.
 */
class ClipboardController
{
public:
    ClipboardController(DocumentManager& docManager);

    /**
     * Select all audio in the current document.
     */
    void selectAll(Document* doc);

    /**
     * Validate that the current selection is within valid bounds.
     * @return true if selection is valid, false otherwise
     */
    bool validateSelection(Document* doc) const;

    /**
     * Copy the current selection to the clipboard.
     * Respects focused channels - if specific channels are focused, only copies those.
     */
    void copySelection(Document* doc);

    /**
     * Cut the current selection to the clipboard.
     * Respects focused channels - if specific channels are focused, copies those
     * and then silences them (rather than deleting samples).
     */
    void cutSelection(Document* doc);

    /**
     * Paste from the clipboard at the current cursor position.
     * When specific channels are focused, replaces only those channels (no length change).
     * When all channels are focused (normal mode), inserts or replaces as usual.
     */
    void pasteAtCursor(Document* doc);

    /**
     * Delete the current selection.
     * When specific channels are focused, silences those channels instead of deleting.
     * When all channels are focused (normal mode), performs a full delete.
     */
    void deleteSelection(Document* doc);

private:
    DocumentManager& m_documentManager;
};
