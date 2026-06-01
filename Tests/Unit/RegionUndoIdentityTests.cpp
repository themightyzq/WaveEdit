/*
  ==============================================================================

    RegionUndoIdentityTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Pass-2 regression tests for the region/marker undo "stable ID" root fix.

    Covers:
      - H8/H9: region & marker metadata undo actions stay bound to the right
               item after a later insert/delete shifts the list.
      - H10:   MultiMerge undo restores originals order-independently.
      - H11:   markers<->regions conversion undo removes the items it created,
               not the "last N".
      - H14:   region auto-numbering does not duplicate names after a delete
               (tested via RegionManager identity + name uniqueness helper at
               the manager level; the controller helper is exercised in the
               app, this asserts the supporting invariant).
      - C3:    region paste is undoable (PasteRegionsUndoAction).

    These construct real RegionDisplay/MarkerDisplay components (headless) for
    the undo actions that take a display reference. juce::Component is safe to
    instantiate without a window in the unit-test runner.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "Utils/Region.h"
#include "Utils/RegionManager.h"
#include "Utils/Marker.h"
#include "Utils/MarkerManager.h"
#include "UI/RegionDisplay.h"
#include "UI/MarkerDisplay.h"
#include "Utils/UndoActions/RegionUndoActions.h"
#include "Utils/UndoActions/MarkerUndoActions.h"
#include "Controllers/RegionController.h"

// ============================================================================
class RegionUndoIdentityTests : public juce::UnitTest
{
public:
    RegionUndoIdentityTests()
        : juce::UnitTest("Region/Marker Undo Identity (Pass 2)", "RegionManager") {}

    void runTest() override
    {
        beginTest("Region has a stable, unique id that survives copies");
        testRegionIdStability();

        beginTest("H8: rename undo restores the right region after an insert");
        testRenameUndoAfterInsert();

        beginTest("H8: resize undo targets the right region after an insert");
        testResizeUndoAfterInsert();

        beginTest("H10: multi-merge undo restores originals order-independently");
        testMultiMergeUndo();

        beginTest("H11: markers->regions undo removes only created regions");
        testMarkersToRegionsUndo();

        beginTest("C3: region paste is undoable");
        testPasteUndoable();

        beginTest("H9: marker rename undo restores the right marker after add");
        testMarkerRenameUndoAfterInsert();

        beginTest("H14: auto-numbering does not duplicate names after a delete");
        testAutoNumberingUnique();
    }

private:
    juce::File tempAudioFile()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        return dir.getNonexistentChildFile("WaveEdit_UndoIdentity", ".wav", false);
    }

    void cleanup(const juce::File& audioFile)
    {
        RegionManager::getRegionFilePath(audioFile).deleteFile();
        MarkerManager::getMarkerFilePath(audioFile).deleteFile();
        audioFile.deleteFile();
    }

    void testRegionIdStability()
    {
        Region a("A", 0, 100);
        Region b("B", 0, 100);
        expect(a.getId() != b.getId(), "distinct regions get distinct ids");

        Region copy = a;
        expectEquals(copy.getId(), a.getId(), "copy keeps the same id");
    }

    void testRenameUndoAfterInsert()
    {
        RegionManager manager;
        RegionDisplay display(manager);
        const auto file = tempAudioFile();
        file.create();
        juce::UndoManager undo;

        manager.addRegion(Region("first",  0,    1000));
        manager.addRegion(Region("middle", 1000, 2000));   // index 1
        manager.addRegion(Region("last",   2000, 3000));

        const int64_t middleId = manager.getRegion(1)->getId();

        // Rename the middle region (index 1) to TEST.
        undo.perform(new RenameRegionUndoAction(
            manager, display, file, 1, "middle", "TEST"));
        expectEquals(manager.getRegionById(middleId)->getName(), juce::String("TEST"));

        // Now insert a region BEFORE it, shifting indices.
        manager.insertRegionAt(0, Region("inserted", 0, 10));
        // The middle region is now at index 2, not 1.
        expectEquals(manager.getIndexOfRegionId(middleId), 2);

        // Undo the rename. With a stale index this would rename the WRONG
        // region; with stable ids it restores the correct one.
        undo.undo();

        expectEquals(manager.getRegionById(middleId)->getName(), juce::String("middle"),
                     "the originally-renamed region is restored");
        // And the region that now sits at the old index 1 is untouched.
        expectEquals(manager.getRegion(1)->getName(), juce::String("first"),
                     "the region now at index 1 was not corrupted");

        cleanup(file);
    }

    void testResizeUndoAfterInsert()
    {
        RegionManager manager;
        RegionDisplay display(manager);
        const auto file = tempAudioFile();
        file.create();
        juce::UndoManager undo;

        manager.addRegion(Region("a", 0,    1000));
        manager.addRegion(Region("b", 1000, 2000));  // target
        const int64_t bId = manager.getRegion(1)->getId();

        undo.perform(new ResizeRegionUndoAction(
            manager, display, file, 1, 1000, 2000, 1200, 1800));
        expectEquals(manager.getRegionById(bId)->getStartSample(), (int64_t) 1200);

        manager.insertRegionAt(0, Region("x", 0, 10));  // shift
        undo.undo();

        expectEquals(manager.getRegionById(bId)->getStartSample(), (int64_t) 1000,
                     "resize undo restored the correct region");
        expectEquals(manager.getRegionById(bId)->getEndSample(), (int64_t) 2000);

        cleanup(file);
    }

    void testMultiMergeUndo()
    {
        RegionManager manager;
        RegionDisplay display(manager);
        const auto file = tempAudioFile();
        file.create();
        juce::UndoManager undo;

        manager.addRegion(Region("r0", 0,    1000));
        manager.addRegion(Region("r1", 1000, 2000));
        manager.addRegion(Region("r2", 2000, 3000));
        manager.addRegion(Region("r3", 3000, 4000));

        // Non-contiguous selection 0 and 2.
        juce::Array<int> indices;        indices.add(0); indices.add(2);
        juce::Array<Region> originals;
        originals.add(*manager.getRegion(0));
        originals.add(*manager.getRegion(2));

        manager.selectRegion(0, false);
        manager.selectRegion(2, true);

        undo.perform(new MultiMergeRegionsUndoAction(
            manager, display, file, indices, originals));

        // After merge: one merged region replaces 0 and 2 -> 3 regions total.
        expectEquals(manager.getNumRegions(), 3);

        undo.undo();

        // Originals restored: 4 regions, originals present by identity.
        expectEquals(manager.getNumRegions(), 4);
        expect(manager.getRegionById(originals[0].getId()) != nullptr,
               "first merged original restored");
        expect(manager.getRegionById(originals[1].getId()) != nullptr,
               "second merged original restored");

        cleanup(file);
    }

    void testMarkersToRegionsUndo()
    {
        RegionManager manager;
        RegionDisplay display(manager);
        const auto file = tempAudioFile();
        file.create();
        juce::UndoManager undo;

        // Pre-existing region that must survive the undo.
        manager.addRegion(Region("keep", 0, 100));
        const int64_t keepId = manager.getRegion(0)->getId();

        juce::Array<Region> created;
        created.add(Region("c0", 200, 300));
        created.add(Region("c1", 400, 500));

        undo.perform(new MarkersToRegionsUndoAction(manager, display, file, created));
        expectEquals(manager.getNumRegions(), 3);

        // Add an UNRELATED region after the conversion (would break a "last N"
        // removal).
        manager.addRegion(Region("after", 600, 700));
        expectEquals(manager.getNumRegions(), 4);

        undo.undo();

        // Only the two created regions are removed; "keep" and "after" remain.
        expectEquals(manager.getNumRegions(), 2);
        expect(manager.getRegionById(keepId) != nullptr, "pre-existing region kept");
        expect(manager.getRegionById(created[0].getId()) == nullptr, "created c0 removed");
        expect(manager.getRegionById(created[1].getId()) == nullptr, "created c1 removed");

        cleanup(file);
    }

    void testPasteUndoable()
    {
        RegionManager manager;
        RegionDisplay display(manager);
        const auto file = tempAudioFile();
        file.create();
        juce::UndoManager undo;

        juce::Array<Region> pasted;
        pasted.add(Region("p0", 1000, 2000));
        pasted.add(Region("p1", 3000, 4000));

        expectEquals(manager.getNumRegions(), 0);

        undo.perform(new PasteRegionsUndoAction(manager, display, file, pasted));
        expectEquals(manager.getNumRegions(), 2);

        expect(undo.canUndo(), "paste registered an undoable action");
        undo.undo();
        expectEquals(manager.getNumRegions(), 0);

        undo.redo();
        expectEquals(manager.getNumRegions(), 2);

        cleanup(file);
    }

    void testMarkerRenameUndoAfterInsert()
    {
        MarkerManager manager;
        MarkerDisplay display(manager);
        const auto file = tempAudioFile();
        file.create();
        juce::UndoManager undo;

        manager.addMarker(Marker("m1", 1000));
        manager.addMarker(Marker("m2", 2000));  // target
        manager.addMarker(Marker("m3", 3000));

        const int64_t targetId = manager.getMarker(1)->getId();

        undo.perform(new RenameMarkerUndoAction(manager, &display, file, 1, "m2", "RENAMED"));
        expectEquals(manager.getMarkerById(targetId)->getName(), juce::String("RENAMED"));

        // Add a marker BEFORE the target (markers re-sort by position).
        manager.addMarker(Marker("m0", 500));
        expectEquals(manager.getIndexOfMarkerId(targetId), 2);  // shifted

        undo.undo();
        expectEquals(manager.getMarkerById(targetId)->getName(), juce::String("m2"),
                     "marker rename undo restored the correct marker");

        cleanup(file);
    }

    void testAutoNumberingUnique()
    {
        RegionManager manager;

        // Simulate the controller's create flow: name = next unused 3-digit.
        auto addNext = [&manager]()
        {
            const auto name = RegionController::generateUniqueRegionName(manager);
            manager.addRegion(Region(name, 0, 100));
            return name;
        };

        expectEquals(addNext(), juce::String("001"));
        expectEquals(addNext(), juce::String("002"));
        expectEquals(addNext(), juce::String("003"));

        // Delete "001" (index 0). The OLD count-based scheme would next yield
        // "003" again (getNumRegions()+1 == 3) -> duplicate. The fix yields
        // the smallest free number, "001".
        manager.removeRegion(0);
        expectEquals(addNext(), juce::String("001"),
                     "reuses the freed low number, no duplicate of '003'");

        // All names must remain unique.
        std::set<juce::String> names;
        for (int i = 0; i < manager.getNumRegions(); ++i)
            names.insert(manager.getRegion(i)->getName());
        expectEquals((int) names.size(), manager.getNumRegions(),
                     "all region names are unique");
    }
};

static RegionUndoIdentityTests regionUndoIdentityTests;
