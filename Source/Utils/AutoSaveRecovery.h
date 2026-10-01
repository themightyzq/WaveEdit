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
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>

namespace AutoSaveRecovery
{
    /** The directory where auto-saves are written. */
    juce::File getAutoSaveDirectory();

    /**
     * The stable per-original prefix shared by every auto-save filename for
     * @p originalFile, including the trailing underscore before the
     * timestamp: "autosave_<stem>_<pathHash>_".
     *
     * The path hash disambiguates two files with the same stem in different
     * directories (e.g. /A/kick.wav vs /B/kick.wav), which previously
     * collided because the filename was keyed on the stem alone (M4). It
     * also gives cleanup a robust token to group on instead of splitting on
     * '_' (which mis-grouped stems containing underscores, M5).
     *
     * Writers (FileController::performAutoSave) MUST build filenames as
     * prefix + timestamp + ".wav" so this prefix matches them.
     */
    juce::String autoSavePrefixFor(const juce::File& originalFile);

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

    /**
     * Write one auto-save: a 32-bit float WAV, so the recovered audio is
     * exactly the in-memory float buffer (no truncation to the document's
     * 16/24-bit save depth, no clipping of samples beyond full scale).
     * Background-thread safe (touches only its arguments).
     *
     * @return false (and @p error set) if the file could not be written.
     */
    bool writeAutoSave(const juce::File& target,
                       const juce::AudioBuffer<float>& buffer,
                       double sampleRate,
                       juce::String& error);

    /** Loads one auto-save into a buffer; false if it cannot be read. */
    using AutoSaveLoader = std::function<bool(const juce::File&, juce::AudioBuffer<float>&)>;

    /**
     * Recovery source selection: try @p newestFirst in order and return the
     * index of the first file that @p loader reads into a non-empty buffer
     * (stored in @p recovered), or -1 if none can be read. A crash during an
     * auto-save write leaves the newest file truncated; this falls back to
     * the previous auto-save instead of giving up.
     */
    int loadFirstReadable(const juce::Array<juce::File>& newestFirst,
                          const AutoSaveLoader& loader,
                          juce::AudioBuffer<float>& recovered);
}
