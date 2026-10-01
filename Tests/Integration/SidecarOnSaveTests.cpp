/*
  ==============================================================================

    SidecarOnSaveTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Regression tests for two v0.9.1 follow-ups:

    1. Region/marker undo actions wrote the JSON sidecar at edit time (before
       Save) using the path they were created with, so the sidecar described
       unsaved state and an undo after Save As wrote next to the OLD file.
       Sidecars are now written by Document::saveFile only, and region edits
       mark the document modified so the user is prompted to save them.

    2. After a rate-converting Save As the in-memory regions stay in
       source-rate samples. Edit-time sidecar writes were not rescaled, so a
       region edit after such a Save As left wrong positions on disk. With
       sidecars written on save only, every write goes through the rescale.

  ==============================================================================
*/

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include "../../Source/Audio/AudioFileManager.h"
#include "../../Source/Utils/RegionManager.h"
#include "../../Source/Utils/MarkerManager.h"
#include "../../Source/Utils/Document.h"
#include "../../Source/Utils/UndoActions/RegionUndoActions.h"
#include "../../Source/Controllers/RegionController.h"
#include "../TestUtils/TestAudioFiles.h"

//==============================================================================
class SidecarOnSaveTests : public juce::UnitTest
{
public:
    SidecarOnSaveTests() : juce::UnitTest("Sidecar Written On Save Only", "Integration") {}

    void runTest() override
    {
        tempDir().deleteRecursively();
        tempDir().createDirectory();

        testRegionEditDoesNotWriteSidecarBeforeSave();
        testUndoAfterSaveAsLeavesOldSidecarAlone();
        testRateConvertingSaveAsKeepsSidecarScaled();

        tempDir().deleteRecursively();
    }

private:
    static constexpr double kSourceRate = 44100.0;

    juce::File tempDir()
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("WaveEditSidecarOnSaveTests");
    }

    juce::File tempFile(const juce::String& name) { return tempDir().getChildFile(name); }

    juce::File makeWav(const juce::String& name)
    {
        auto file = tempFile(name);
        cleanup(file);
        auto buffer = TestAudio::createSineWave(440.0, 0.5f, kSourceRate, 3.0, 1);
        AudioFileManager fm;
        expect(fm.saveAsWav(file, buffer, kSourceRate, 16), "WAV fixture should save");
        return file;
    }

    void cleanup(const juce::File& file)
    {
        file.deleteFile();
        RegionManager::getRegionFilePath(file).deleteFile();
        MarkerManager::getMarkerFilePath(file).deleteFile();
    }

    /** A region the WAV cue chunk cannot represent (custom colour), so every
        save writes a sidecar for it. */
    static Region pinkRegion(int64_t start, int64_t end)
    {
        Region region("pink", start, end);
        region.setColor(juce::Colours::hotpink);
        return region;
    }

    static juce::String sidecarText(const juce::File& audio)
    {
        return RegionManager::getRegionFilePath(audio).loadFileAsString();
    }

    /** Add a region the way the UI does (RegionController, selection-based). */
    static void addRegionViaController(Document& doc, double startSec, double endSec)
    {
        RegionController controller;
        doc.getWaveformDisplay().setSelection(startSec, endSec);
        controller.addRegionFromSelection(&doc);
    }

    //==========================================================================
    void testRegionEditDoesNotWriteSidecarBeforeSave()
    {
        beginTest("A region edit leaves the sidecar alone until Save, and marks the document modified");

        auto file = makeWav("edit.wav");
        Document doc;
        expect(doc.loadFile(file), "load");
        doc.getRegionManager().addRegion(pinkRegion(1000, 2000));
        expect(doc.saveFile(file, 16, 10, 0.0), "first save");
        const auto savedSidecar = sidecarText(file);
        expect(savedSidecar.isNotEmpty(), "the custom-colour region forces a sidecar");
        expect(!doc.isModified(), "clean after save");

        addRegionViaController(doc, 0.5, 1.0);
        expectEquals(doc.getRegionManager().getNumRegions(), 2, "region added in memory");
        expect(doc.isModified(), "a region edit marks the document modified");
        expectEquals(sidecarText(file), savedSidecar, "sidecar on disk unchanged before Save");

        doc.getUndoManager().undo();
        expectEquals(sidecarText(file), savedSidecar, "undo does not write the sidecar either");
        doc.getUndoManager().redo();

        expect(doc.saveFile(file, 16, 10, 0.0), "second save");
        RegionManager reloaded;
        expect(reloaded.loadFromFile(file), "sidecar parses");
        expectEquals(reloaded.getNumRegions(), 2, "Save writes the edit");
    }

    void testUndoAfterSaveAsLeavesOldSidecarAlone()
    {
        beginTest("Undo after Save As does not write a sidecar next to the old file");

        auto oldFile = makeWav("old.wav");
        auto newFile = tempFile("new.wav");
        cleanup(newFile);

        Document doc;
        expect(doc.loadFile(oldFile), "load");
        doc.getRegionManager().addRegion(pinkRegion(1000, 2000));
        expect(doc.saveFile(oldFile, 16, 10, 0.0), "save old");

        // The edit is made while the document still points at old.wav.
        addRegionViaController(doc, 0.5, 1.0);
        const auto oldSidecar = sidecarText(oldFile);
        expect(doc.saveFile(newFile, 16, 10, 0.0), "Save As");
        const auto newSidecar = sidecarText(newFile);

        doc.getUndoManager().undo();
        expectEquals(sidecarText(oldFile), oldSidecar, "old file's sidecar untouched by the undo");
        expectEquals(sidecarText(newFile), newSidecar, "new file's sidecar changes only on Save");
        expectEquals(doc.getRegionManager().getNumRegions(), 1, "undo removed the region in memory");
    }

    void testRateConvertingSaveAsKeepsSidecarScaled()
    {
        beginTest("Rate-converting Save As, then a region edit and Save: sidecar stays in target-rate samples");

        auto source = makeWav("rate_source.wav");
        auto target = tempFile("rate_target.wav");
        cleanup(target);

        Document doc;
        expect(doc.loadFile(source), "load");
        doc.getRegionManager().addRegion(pinkRegion(44100, 88200));  // 1 s .. 2 s

        constexpr double kTargetRate = 48000.0;
        expect(doc.saveFile(target, 24, 10, kTargetRate), "Save As at 48 kHz");

        auto readTarget = [this, &target](int expectedCount, const juce::String& when)
        {
            RegionManager onDisk;
            expect(onDisk.loadFromFile(target), "sidecar parses " + when);
            expectEquals(onDisk.getNumRegions(), expectedCount, "region count " + when);
            for (const auto& r : onDisk.getAllRegions())
            {
                if (r.getName() == "pink")
                {
                    expectEquals((int) r.getStartSample(), 48000, "pink start " + when);
                    expectEquals((int) r.getEndSample(), 96000, "pink end " + when);
                }
                else
                {
                    expectEquals((int) r.getStartSample(), 24000, "new region start " + when);
                    expectEquals((int) r.getEndSample(), 48000, "new region end " + when);
                }
            }
        };
        readTarget(1, "after Save As");

        // In memory the document is still at the source rate.
        expectEquals(doc.getBufferManager().getSampleRate(), kSourceRate, "buffer stays at 44.1 kHz");

        doc.getUndoManager().beginNewTransaction("Add Region");
        doc.getUndoManager().perform(new AddRegionUndoAction(doc.getRegionManager(),
                                                             doc.getRegionDisplay(),
                                                             Region("002", 22050, 44100)));
        readTarget(1, "after the edit, before Save");

        expect(doc.saveFile(target, 24, 10, doc.getSaveTargetSampleRate()), "Save in place");
        readTarget(2, "after Save");
    }
};

static SidecarOnSaveTests sidecarOnSaveTests;
