/*
  ==============================================================================

    SidecarNotifications.h
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

/**
 * Async, ASCII, themed notifications for the embedded-cue / sidecar system.
 *
 * These are thin UI shims over the pure decision logic on Document /
 * SidecarPolicy: they own no state and simply present the result to the user.
 */
namespace SidecarNotifications
{
    /**
     * Shows the "Companion File Created" warning the first time an edit makes a
     * document sidecar-required (custom color or non-ASCII name). No-op if the
     * document did not just transition, or if the user previously chose
     * "Don't show this again" (persisted under Settings key
     * "warnings.sidecarRequired").
     *
     * @param doc    The document that was just edited.
     * @param parent Optional dialog parent component (may be nullptr).
     */
    void warnIfSidecarNowRequired(Document& doc, juce::Component* parent);

    /**
     * If the document was opened with a stale sidecar over a file that carries
     * its own embedded cues, prompts the user to keep WaveEdit's sidecar or
     * adopt the file's embedded markers. Cancel keeps the sidecar. No-op when
     * there is no conflict.
     *
     * @param doc       The freshly opened document.
     * @param parent    Optional dialog parent component (may be nullptr).
     * @param onResolved Called after the user resolves the conflict (e.g. to
     *                   refresh region/marker displays). May be nullptr.
     */
    void promptOnStaleSidecar(Document& doc,
                              juce::Component* parent,
                              std::function<void()> onResolved);
}
