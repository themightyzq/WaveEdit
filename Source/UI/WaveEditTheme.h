/*
  ==============================================================================

    WaveEditTheme.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Phase 1 of the colour theme system: a typed token set + two
    built-in themes (Dark and Light). The token vocabulary lives here;
    `ThemeManager` is the runtime carrier. Production code that wants
    to draw with a token reads it from the active theme via the
    ThemeManager singleton — direct construction of `juce::Colour`
    literals is discouraged for newly-added paint paths.

    Token taxonomy (semantic, NOT cosmetic — pick the role, not the
    visible colour):

      Surface tokens
        background        — the application's outer canvas
        panel             — primary surface for panels (lists, dialogs)
        panelAlternate    — alternate row in striped lists
        border            — separator lines between surfaces

      Waveform-specific surfaces
        waveformBackground — the box the waveform draws into
        waveformLine       — the rendered waveform itself (default; per-
                              channel overrides still win when set)
        waveformBorder     — the focused-channel highlight border
        ruler              — top ruler strip
        rulerText          — text on the ruler
        gridLine           — minor vertical/horizontal divisions

      Foreground / text
        text              — primary text content
        textMuted         — secondary / disabled-state text
        accent            — interactive emphasis (links, selected items)

      Status / state
        selection         — selected ranges
        focus             — keyboard-focus indicator
        warning           — yellow/amber warning surfaces
        success           — green success indicators
        error             — red error indicators

    Phase 1 only the waveform surfaces are routed through the theme.
    Phase 2 will apply the rest of the tokens to dialogs, panels,
    toolbars.

  ==============================================================================
*/

#pragma once

#include <juce_graphics/juce_graphics.h>

namespace waveedit
{

struct Theme
{
    juce::String id;       // "dark", "light", or user theme id
    juce::String name;     // human-readable name shown in the picker

    // Surfaces
    juce::Colour background;
    juce::Colour panel;
    juce::Colour panelAlternate;
    juce::Colour border;

    // Waveform-specific
    juce::Colour waveformBackground;
    juce::Colour waveformLine;
    juce::Colour waveformBorder;       // focused-channel highlight
    juce::Colour ruler;
    juce::Colour rulerText;
    juce::Colour gridLine;

    // Foreground / text
    juce::Colour text;
    juce::Colour textMuted;
    juce::Colour accent;

    // Status
    juce::Colour selection;
    juce::Colour focus;
    juce::Colour warning;
    juce::Colour success;
    juce::Colour error;
};

namespace BuiltInThemes
{
    /** Dark — the existing visual identity, ported verbatim. */
    Theme dark();

    /** Light — neutral counterpart for daylight viewing. */
    Theme light();

    /** High Contrast — accessibility-focused; pure-black surfaces with
        saturated foreground tokens for users who need maximum legibility. */
    Theme highContrast();
}

}  // namespace waveedit
