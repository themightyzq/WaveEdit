/*
  ==============================================================================

    RecordingEditLockTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Regression test for the Phase 1 finding "editing commands stay enabled
    during a recording": the recording dialog is non-modal, so Cut/Gain/EQ/
    region edits etc. could change the document a take is about to be
    inserted into. CommandHandler::getCommandInfo now disables every editing
    command while CommandContext::isRecording is set (MainComponent fills it
    from RecordingController::isRecording()).

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../../Source/Commands/CommandHandler.h"
#include "../../Source/Commands/CommandIDs.h"
#include "../../Source/Controllers/RecordingController.h"
#include "../../Source/Utils/DocumentManager.h"
#include "../../Source/Utils/Document.h"
#include "../../Source/Utils/KeymapManager.h"
#include "../../Source/Utils/Settings.h"

#ifndef WAVEEDIT_TEMPLATES_DIR
#error "WAVEEDIT_TEMPLATES_DIR must be defined by CMake for this test"
#endif

class RecordingEditLockTests : public juce::UnitTest
{
public:
    RecordingEditLockTests() : juce::UnitTest("Recording Disables Editing", "Unit") {}

    void runTest() override
    {
        beginTest("Editing commands are disabled while recording, other commands are not");

        const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("WaveEditRecordingLock_" + juce::Uuid().toString());
        dir.createDirectory();

        {
            juce::ApplicationCommandManager commands;
            Settings settings(dir.getChildFile("settings.json"));
            KeymapManager keymaps(commands, settings,
                                  juce::File(juce::String(WAVEEDIT_TEMPLATES_DIR)),
                                  dir.getChildFile("Keymaps"));

            DocumentManager docs;
            Document* doc = docs.createDocument();
            juce::AudioBuffer<float> audio(2, 48000);
            audio.clear();
            doc->getBufferManager().setBuffer(audio, 48000.0);
            doc->getAudioEngine().loadFromBuffer(doc->getBufferManager().getBuffer(), 48000.0, 2);
            doc->getWaveformDisplay().setSelection(0.1, 0.2);

            CommandHandler handler;
            CommandContext context;
            context.currentDoc = doc;
            context.documentManager = &docs;
            context.keymapManager = &keymaps;

            auto isActive = [&](juce::CommandID id, bool recording)
            {
                context.isRecording = recording;
                juce::ApplicationCommandInfo info(id);
                handler.getCommandInfo(id, info, context);
                return (info.flags & juce::ApplicationCommandInfo::isDisabled) == 0;
            };

            const juce::CommandID editing[] = {
                CommandIDs::editCut, CommandIDs::editDelete, CommandIDs::editSilence,
                CommandIDs::processGain, CommandIDs::processNormalize,
                CommandIDs::processGraphicalEQ, CommandIDs::processReverse,
                CommandIDs::generateTone, CommandIDs::regionAdd, CommandIDs::markerAdd
            };
            for (auto id : editing)
            {
                expect(isActive(id, false), "command " + juce::String(id) + " active when not recording");
                expect(!isActive(id, true), "command " + juce::String(id) + " disabled while recording");
                expect(CommandHandler::isEditingCommand(id), "classified as editing");
            }

            for (auto id : { (juce::CommandID) CommandIDs::playbackStop,
                             (juce::CommandID) CommandIDs::viewZoomIn,
                             (juce::CommandID) CommandIDs::fileSaveAs })
            {
                expect(isActive(id, true) == isActive(id, false),
                       "non-editing command " + juce::String(id) + " unaffected by recording");
                expect(!CommandHandler::isEditingCommand(id));
            }
        }

        dir.deleteRecursively();

        beginTest("RecordingController reports not-recording until a take starts");
        RecordingController controller;
        expect(!controller.isRecording());
    }
};

static RecordingEditLockTests recordingEditLockTests;
