/*
  ==============================================================================

    RecordingController.h - Recording workflow controller
    Part of WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Per CLAUDE.md §8.1: recording workflow / RecordingDialog::Listener
    inline class belongs out of Main.cpp.

  ==============================================================================
*/

#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

class DocumentManager;

/**
 * RecordingController owns the recording flow: prompts for a destination
 * (insert at cursor of the current document, or create a new document),
 * launches the RecordingDialog, and applies the captured buffer when the
 * dialog completes.
 */
class RecordingController
{
public:
    RecordingController() = default;
    ~RecordingController() = default;

    /**
     * Entry point for the Record command.
     *
     * If a document is open, asks the user whether to insert at cursor
     * or create a new document. With no document open, always creates
     * a new one. Then opens the RecordingDialog with the appropriate
     * listener.
     *
     * @param parent            Component used as dialog parent
     * @param documentManager   The application's document manager
     * @param audioDeviceManager Device manager that the dialog will record from
     */
    void handleRecordCommand(juce::Component* parent,
                             DocumentManager& documentManager,
                             juce::AudioDeviceManager& audioDeviceManager);
};
