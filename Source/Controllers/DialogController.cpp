/*
  ==============================================================================

    DialogController.cpp
    Copyright (C) 2025 ZQ SFX

  ==============================================================================
*/

#include "DialogController.h"

#include "../UI/KeyboardCheatSheetDialog.h"
#include "../Batch/BatchProcessorDialog.h"

void DialogController::showAboutDialog(juce::Component* parent)
{
    const juce::String aboutText =
        "WaveEdit - Professional Audio Editor\n"
        "Version 1.0\n\n"
        "Copyright © 2025 ZQ SFX\n"
        "Licensed under GNU GPL v3\n\n"
        "Built with JUCE " + juce::String(JUCE_MAJOR_VERSION) + "."
        + juce::String(JUCE_MINOR_VERSION) + "."
        + juce::String(JUCE_BUILDNUMBER);

    juce::AlertWindow::showMessageBox(juce::AlertWindow::InfoIcon,
                                      "About WaveEdit",
                                      aboutText,
                                      "OK",
                                      parent);
}

void DialogController::showKeyboardShortcutsDialog(juce::Component* parent,
                                                   KeymapManager& keymapManager,
                                                   juce::ApplicationCommandManager& commandManager)
{
    KeyboardCheatSheetDialog::showDialog(parent, keymapManager, commandManager);
}

void DialogController::showBatchProcessorDialog()
{
    waveedit::BatchProcessorDialog::showDialog();
}
