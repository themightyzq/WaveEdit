/*
  ==============================================================================

    DialogController.h - Application-level dialog launcher
    Part of WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Per CLAUDE.md §8.1: dialog creation/result handling does not belong in
    Main.cpp. Centralizes the simple application-level dialogs (About,
    Keyboard Cheat Sheet, etc.). Dialogs that mutate document state stay
    on the corresponding domain controller (DSPController for processing
    dialogs, FileController for save-as/file-properties, etc.).

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class KeymapManager;

class DialogController
{
public:
    DialogController() = default;
    ~DialogController() = default;

    /** Modal "About WaveEdit" dialog. */
    void showAboutDialog(juce::Component* parent);

    /** Searchable keyboard cheat-sheet (F1). */
    void showKeyboardShortcutsDialog(juce::Component* parent,
                                     KeymapManager& keymapManager,
                                     juce::ApplicationCommandManager& commandManager);

    /** Tools → Batch Processor entry point. */
    void showBatchProcessorDialog();
};
