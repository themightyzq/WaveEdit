/*
  ==============================================================================

    AutoSaveRecovery.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    UI-free helpers for the crash-recovery flow:
      - locating the auto-save directory
      - finding auto-save files for a given original that are newer than
        the original on disk (= unsaved changes from a previous session)
      - deleting all auto-saves for a given original (called after a
        normal save supersedes them, or after the user picks "Discard").

    Lives in Utils/ (no UI / no audio / no JUCE GUI deps) so test code
    can link against it without dragging the rest of FileController and
    its dialog-class deps into the test binary.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>

namespace AutoSaveRecovery
{
    /** The directory where auto-saves are written. */
    juce::File getAutoSaveDirectory();

    /**
     * Return every auto-save file in @p autoSaveDir that targets
     * @p originalFile and is newer than the original on disk. Empty
     * result = no recovery offered.
     *
     * @param autoSaveDir   Directory to scan. Pass getAutoSaveDirectory()
     *                      in production; tests pass a fresh temp dir.
     * @param originalFile  The audio file the user just opened (or saved).
     */
    juce::Array<juce::File> findOrphanedAutoSaves(const juce::File& autoSaveDir,
                                                   const juce::File& originalFile);

    /** Convenience: scans the production auto-save directory. */
    juce::Array<juce::File> findOrphanedAutoSaves(const juce::File& originalFile);

    /**
     * Delete every auto-save file in @p autoSaveDir that targets
     * @p originalFile. Called after a successful save (the on-disk file
     * is now canonical) and after the user picks "Discard" in the
     * recovery dialog.
     */
    void deleteAutoSavesFor(const juce::File& autoSaveDir,
                            const juce::File& originalFile);

    /** Convenience: scans the production auto-save directory. */
    void deleteAutoSavesFor(const juce::File& originalFile);
}
