/*
  ==============================================================================

    CommandHandler.h
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
class KeymapManager;
class SpectrumAnalyzer;

/**
 * Context passed to CommandHandler methods containing state from MainComponent.
 * This struct allows CommandHandler to access necessary state without
 * direct coupling to MainComponent.
 */
struct CommandContext
{
    Document* currentDoc = nullptr;
    DocumentManager* documentManager = nullptr;
    KeymapManager* keymapManager = nullptr;
    bool hasRegionClipboard = false;
    bool autoPreviewRegions = false;
    bool regionsVisible = false;
    SpectrumAnalyzer* spectrumAnalyzer = nullptr;
    juce::DocumentWindow* spectrumWindow = nullptr;
    std::function<bool(Document*)> canMergeRegions;   // Function to check if regions can be merged
    std::function<bool(Document*)> canSplitRegion;    // Function to check if a region can be split
};

/**
 * CommandHandler handles routing of application commands.
 * Extracts getAllCommands() and getCommandInfo() logic from MainComponent.
 *
 * All command definitions and descriptions are centralized here.
 * MainComponent delegates to this class via ApplicationCommandTarget interface.
 */
class CommandHandler
{
public:
    CommandHandler();
    ~CommandHandler() = default;

    /**
     * Returns array of all available command IDs.
     * Called by ApplicationCommandTarget to populate the command manager.
     */
    void getAllCommands(juce::Array<juce::CommandID>& commands);

    /**
     * Provides metadata (name, description, category, shortcut) for a command.
     * Called by ApplicationCommandTarget for menu items and keyboard handling.
     *
     * @param commandID The ID of the command to describe
     * @param result The ApplicationCommandInfo struct to populate
     * @param context State context from MainComponent
     */
    void getCommandInfo(juce::CommandID commandID,
                        juce::ApplicationCommandInfo& result,
                        const CommandContext& context);
};
