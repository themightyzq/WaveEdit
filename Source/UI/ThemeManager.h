/*
  ==============================================================================

    ThemeManager.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Process-wide carrier for the active `waveedit::Theme`. Loads the
    user's chosen theme id from Settings on first access; broadcasts a
    juce::ChangeMessage when the theme switches so listeners can
    re-render themselves.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <map>

#include "WaveEditTheme.h"

namespace waveedit
{

class ThemeManager : public juce::ChangeBroadcaster
{
public:
    /** Singleton accessor. */
    static ThemeManager& getInstance();

    /** The currently-active theme. */
    const Theme& getCurrent() const { return m_current; }

    /** List of available theme ids (for settings UI). */
    juce::StringArray getAvailableIds() const;

    /** Switch to the theme with @p themeId. Persists to settings and
        notifies listeners. No-op if themeId is unknown or already
        active. */
    void setActiveById(const juce::String& themeId);

    /** Force-reload from Settings. Useful at app startup. */
    void reloadFromSettings();

    /** Serialise a theme to a JSON string (round-trippable with
        @ref themeFromJson). */
    static juce::String themeToJson(const Theme& theme);

    /** Parse a theme JSON document. Returns true on success and writes
        the result to @p out; returns false (with @p errorOut populated)
        on missing fields or malformed values. Unspecified tokens
        inherit from the dark theme so partial files still load. */
    static bool themeFromJson(const juce::String& json, Theme& out, juce::String& errorOut);

    /** Export the currently-active theme to @p file as JSON. */
    bool exportCurrentTheme(const juce::File& file, juce::String& errorOut) const;

    /** Load a custom theme from @p file, register it, and switch to it.
        The imported theme's id is appended to the available list and
        persisted to settings. Returns false (with @p errorOut populated)
        if the file is malformed. */
    bool importCustomTheme(const juce::File& file, juce::String& errorOut);

    ThemeManager(const ThemeManager&)            = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

private:
    ThemeManager();

    void applyById(const juce::String& themeId);

    Theme m_current;

    // Imported user themes: id → theme. Persisted JSON files are kept
    // separately on disk; this map only holds them for the current run.
    std::map<juce::String, Theme> m_customThemes;
};

}  // namespace waveedit
