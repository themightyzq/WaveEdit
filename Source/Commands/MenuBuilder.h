/*
  ==============================================================================

    MenuBuilder.h
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
#include <functional>

class SpectrumAnalyzer;

/**
 * Context passed to MenuBuilder methods containing state from MainComponent.
 * This struct provides callbacks and components needed to build menus
 * without direct coupling to MainComponent.
 */
struct MenuContext
{
    juce::ApplicationCommandManager* commandManager = nullptr;
    SpectrumAnalyzer* spectrumAnalyzer = nullptr;
    std::function<void(const juce::File&)> onLoadFile;
    std::function<void()> onShowAbout;
};

/**
 * MenuBuilder handles construction of the menu bar and menu items.
 * Extracts getMenuBarNames() and getMenuForIndex() logic from MainComponent.
 *
 * MainComponent delegates to this class via MenuBarModel interface.
 */
class MenuBuilder
{
public:
    MenuBuilder();
    ~MenuBuilder() = default;

    /**
     * Returns the names of menu bar items (File, Edit, View, etc).
     * Called by MenuBarModel to populate the menu bar.
     */
    juce::StringArray getMenuBarNames();

    /**
     * Constructs the popup menu for a given menu index.
     * Called by MenuBarModel when user clicks on a menu item.
     *
     * @param menuIndex The index of the menu (0=File, 1=Edit, etc)
     * @param menuName The name of the menu (for reference)
     * @param context State context from MainComponent
     * @return The constructed PopupMenu
     */
    juce::PopupMenu getMenuForIndex(int menuIndex,
                                    const juce::String& menuName,
                                    const MenuContext& context);
};
