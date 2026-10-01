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
class MainComponent;

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
    bool isRecording = false;                         // A recording is being captured
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
     * True for commands that change a document's audio, regions or markers
     * (Edit, Process, Generate, Tools, region/marker edits, plugin apply,
     * Undo/Redo). getCommandInfo() disables these while a recording is being
     * captured, so the audio a take will be inserted into cannot change
     * under it.
     */
    static bool isEditingCommand(juce::CommandID commandID);

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

    /**
     * Dispatch a command invocation. Per CLAUDE.md §8.1 the perform()
     * switch belongs here rather than in MainComponent. The owning
     * MainComponent is passed by reference so this method can call back
     * into its public methods, controllers, and state. CommandHandler is
     * a friend of MainComponent so it can also touch private members it
     * legitimately collaborates with (the controller members).
     *
     * @return true if the command was handled (matches
     *         juce::ApplicationCommandTarget::perform contract).
     */
    bool performCommand(MainComponent& mc,
                        const juce::ApplicationCommandTarget::InvocationInfo& info);
};
