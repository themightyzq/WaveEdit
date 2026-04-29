/*
  ==============================================================================

    PlaybackController.h - Playback transport controller
    Part of WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Per CLAUDE.md §8.1: playback transport logic does not belong in
    Main.cpp. Receives a Document and performs playback transitions
    on its AudioEngine.

  ==============================================================================
*/

#pragma once

class Document;

/**
 * PlaybackController owns the play / stop / pause / loop state machine.
 *
 * Each method takes the Document to operate on; controllers do not own UI
 * components, so the caller is responsible for any UI repaint that should
 * follow a state change.
 */
class PlaybackController
{
public:
    PlaybackController() = default;
    ~PlaybackController() = default;

    /**
     * Toggle play/stop. If already playing, stop. If not playing, start
     * playback honoring the active selection (selection-bounded once),
     * the edit cursor (play to end), or the file start (play whole file).
     *
     * Always clears stale loop points before starting fresh playback so
     * a previous session's loop doesn't leak into the new one.
     */
    void togglePlayback(Document* doc);

    /** Hard stop. */
    void stopPlayback(Document* doc);

    /** Pause / resume from pause. No-op if stopped. */
    void pausePlayback(Document* doc);

    /**
     * Toggle the transport's loop button and mirror the state into the
     * AudioEngine.
     */
    void toggleLoop(Document* doc);

    /**
     * Jump playback to the currently selected region and start looping
     * that range. No-op if no region is selected.
     */
    void loopRegion(Document* doc);
};
