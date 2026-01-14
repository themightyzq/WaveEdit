/*
  ==============================================================================

    RegionListBatchRenameTests.cpp
    Created: 2025-10-22
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Comprehensive integration tests for RegionListPanel batch rename feature.
    Tests Pattern mode, Find/Replace mode, Prefix/Suffix mode, undo/redo,
    and UI state management.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Utils/RegionManager.h"
#include "Utils/Region.h"
#include "UI/RegionListPanel.h"
#include "UI/RegionDisplay.h"
#include "Utils/UndoableEdits.h"
#include "../TestUtils/TestAudioFiles.h"

// ============================================================================
// Test Helper Classes
// ============================================================================

/**
 * Mock listener for testing RegionListPanel callbacks.
 */
class MockRegionListListener : public RegionListPanel::Listener
{
public:
    MockRegionListListener() = default;

    void regionListPanelJumpToRegion(int regionIndex) override
    {
        m_jumpToRegionCalled = true;
        m_lastJumpIndex = regionIndex;
    }

    void regionListPanelRegionDeleted(int regionIndex) override
    {
        m_regionDeletedCalled = true;
        m_lastDeletedIndex = regionIndex;
    }

    void regionListPanelRegionRenamed(int regionIndex, const juce::String& newName) override
    {
        m_regionRenamedCalled = true;
        m_lastRenamedIndex = regionIndex;
        m_lastRenamedName = newName;
    }

    void regionListPanelRegionSelected(int regionIndex) override
    {
        m_regionSelectedCalled = true;
        m_lastSelectedIndex = regionIndex;
    }

    void regionListPanelBatchRenameApply(const std::vector<int>& regionIndices,
                                          const std::vector<juce::String>& newNames) override
    {
        m_batchRenameApplyCalled = true;
        m_lastBatchIndices = regionIndices;
        m_lastBatchNewNames = newNames;
    }

    // Verification methods
    bool wasBatchRenameApplyCalled() const { return m_batchRenameApplyCalled; }
    const std::vector<int>& getLastBatchIndices() const { return m_lastBatchIndices; }
    const std::vector<juce::String>& getLastBatchNewNames() const { return m_lastBatchNewNames; }

    void reset()
    {
        m_jumpToRegionCalled = false;
        m_regionDeletedCalled = false;
        m_regionRenamedCalled = false;
        m_regionSelectedCalled = false;
        m_batchRenameApplyCalled = false;
        m_lastBatchIndices.clear();
        m_lastBatchNewNames.clear();
    }

private:
    bool m_jumpToRegionCalled = false;
    bool m_regionDeletedCalled = false;
    bool m_regionRenamedCalled = false;
    bool m_regionSelectedCalled = false;
    bool m_batchRenameApplyCalled = false;
    int m_lastJumpIndex = -1;
    int m_lastDeletedIndex = -1;
    int m_lastRenamedIndex = -1;
    int m_lastSelectedIndex = -1;
    juce::String m_lastRenamedName;
    std::vector<int> m_lastBatchIndices;
    std::vector<juce::String> m_lastBatchNewNames;
};

// ============================================================================
// Test Groups
// ============================================================================

/**
 * Group 1: Pattern Mode Tests ({n}, {N}, {original})
 */
class BatchRenamePatternModeTests : public juce::UnitTest
{
public:
    BatchRenamePatternModeTests()
        : juce::UnitTest("RegionList Batch Rename - Pattern Mode", "RegionListPanel")
    {
    }

    void runTest() override
    {
        beginTest("Pattern {n} - Sequential numbering");
        {
            RegionManager manager;
            manager.addRegion(Region("Old Name 1", 0, 1000));
            manager.addRegion(Region("Old Name 2", 1000, 2000));
            manager.addRegion(Region("Old Name 3", 2000, 3000));

            juce::UndoManager undoManager;

            std::vector<int> indices = {0, 1, 2};
            std::vector<juce::String> oldNames = {"Old Name 1", "Old Name 2", "Old Name 3"};
            std::vector<juce::String> newNames = {"Region 1", "Region 2", "Region 3"};

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "Region 1", "Region 0 should be renamed to 'Region 1'");
            expect(manager.getRegion(1)->getName() == "Region 2", "Region 1 should be renamed to 'Region 2'");
            expect(manager.getRegion(2)->getName() == "Region 3", "Region 2 should be renamed to 'Region 3'");
        }

        beginTest("Pattern {N} - Zero-padded numbering");
        {
            RegionManager manager;
            for (int i = 0; i < 12; ++i)
                manager.addRegion(Region("Old Name", i * 1000, (i + 1) * 1000));

            std::vector<int> indices;
            std::vector<juce::String> oldNames;
            std::vector<juce::String> newNames;

            for (int i = 0; i < 12; ++i)
            {
                indices.push_back(i);
                oldNames.push_back("Old Name");
                newNames.push_back(juce::String("Region ") + juce::String(i + 1).paddedLeft('0', 2));
            }

            juce::UndoManager undoManager;

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "Region 01", "Region 0 should be 'Region 01'");
            expect(manager.getRegion(9)->getName() == "Region 10", "Region 9 should be 'Region 10'");
            expect(manager.getRegion(11)->getName() == "Region 12", "Region 11 should be 'Region 12'");
        }

        beginTest("Pattern {original} - Preserve original name");
        {
            RegionManager manager;
            manager.addRegion(Region("Dialog", 0, 1000));
            manager.addRegion(Region("Music", 1000, 2000));
            manager.addRegion(Region("SFX", 2000, 3000));

            std::vector<int> indices = {0, 1, 2};
            std::vector<juce::String> oldNames = {"Dialog", "Music", "SFX"};
            std::vector<juce::String> newNames = {"Dialog_1", "Music_2", "SFX_3"};

            juce::UndoManager undoManager;

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "Dialog_1", "Region 0 should be 'Dialog_1'");
            expect(manager.getRegion(1)->getName() == "Music_2", "Region 1 should be 'Music_2'");
            expect(manager.getRegion(2)->getName() == "SFX_3", "Region 2 should be 'SFX_3'");
        }
    }
};

/**
 * Group 2: Find/Replace Mode Tests
 */
class BatchRenameFindReplaceTests : public juce::UnitTest
{
public:
    BatchRenameFindReplaceTests()
        : juce::UnitTest("RegionList Batch Rename - Find/Replace", "RegionListPanel")
    {
    }

    void runTest() override
    {
        beginTest("Find/Replace - Case sensitive");
        {
            RegionManager manager;
            manager.addRegion(Region("old_name_1", 0, 1000));
            manager.addRegion(Region("OLD_NAME_2", 1000, 2000));
            manager.addRegion(Region("Old_Name_3", 2000, 3000));

            std::vector<int> indices = {0, 1, 2};
            std::vector<juce::String> oldNames = {"old_name_1", "OLD_NAME_2", "Old_Name_3"};
            std::vector<juce::String> newNames = {"new_name_1", "OLD_NAME_2", "Old_Name_3"}; // Only lowercase matches

            juce::UndoManager undoManager;

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "new_name_1", "Region 0 should have 'old' replaced with 'new'");
            expect(manager.getRegion(1)->getName() == "OLD_NAME_2", "Region 1 should be unchanged (case sensitive)");
            expect(manager.getRegion(2)->getName() == "Old_Name_3", "Region 2 should be unchanged (case sensitive)");
        }

        beginTest("Find/Replace - Case insensitive");
        {
            RegionManager manager;
            manager.addRegion(Region("old_name_1", 0, 1000));
            manager.addRegion(Region("OLD_NAME_2", 1000, 2000));
            manager.addRegion(Region("Old_Name_3", 2000, 3000));

            std::vector<int> indices = {0, 1, 2};
            std::vector<juce::String> oldNames = {"old_name_1", "OLD_NAME_2", "Old_Name_3"};
            std::vector<juce::String> newNames = {"new_name_1", "NEW_NAME_2", "New_Name_3"}; // All case variations match

            juce::UndoManager undoManager;

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "new_name_1", "Region 0 should have 'old' replaced");
            expect(manager.getRegion(1)->getName() == "NEW_NAME_2", "Region 1 should have 'OLD' replaced");
            expect(manager.getRegion(2)->getName() == "New_Name_3", "Region 2 should have 'Old' replaced");
        }

        beginTest("Find/Replace - Multiple occurrences");
        {
            RegionManager manager;
            manager.addRegion(Region("test_test_test", 0, 1000));
            manager.addRegion(Region("test_file_test", 1000, 2000));

            std::vector<int> indices = {0, 1};
            std::vector<juce::String> oldNames = {"test_test_test", "test_file_test"};
            std::vector<juce::String> newNames = {"foo_foo_foo", "foo_file_foo"}; // Replace all occurrences

            juce::UndoManager undoManager;

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "foo_foo_foo", "All 'test' should be replaced with 'foo'");
            expect(manager.getRegion(1)->getName() == "foo_file_foo", "Both 'test' should be replaced with 'foo'");
        }
    }
};

/**
 * Group 3: Prefix/Suffix Mode Tests
 */
class BatchRenamePrefixSuffixTests : public juce::UnitTest
{
public:
    BatchRenamePrefixSuffixTests()
        : juce::UnitTest("RegionList Batch Rename - Prefix/Suffix", "RegionListPanel")
    {
    }

    void runTest() override
    {
        beginTest("Prefix only");
        {
            RegionManager manager;
            manager.addRegion(Region("Region1", 0, 1000));
            manager.addRegion(Region("Region2", 1000, 2000));
            manager.addRegion(Region("Region3", 2000, 3000));

            std::vector<int> indices = {0, 1, 2};
            std::vector<juce::String> oldNames = {"Region1", "Region2", "Region3"};
            std::vector<juce::String> newNames = {"Prefix_Region1", "Prefix_Region2", "Prefix_Region3"};

            juce::UndoManager undoManager;

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "Prefix_Region1", "Prefix should be added to Region1");
            expect(manager.getRegion(1)->getName() == "Prefix_Region2", "Prefix should be added to Region2");
            expect(manager.getRegion(2)->getName() == "Prefix_Region3", "Prefix should be added to Region3");
        }

        beginTest("Suffix only");
        {
            RegionManager manager;
            manager.addRegion(Region("Region1", 0, 1000));
            manager.addRegion(Region("Region2", 1000, 2000));
            manager.addRegion(Region("Region3", 2000, 3000));

            std::vector<int> indices = {0, 1, 2};
            std::vector<juce::String> oldNames = {"Region1", "Region2", "Region3"};
            std::vector<juce::String> newNames = {"Region1_Suffix", "Region2_Suffix", "Region3_Suffix"};

            juce::UndoManager undoManager;

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "Region1_Suffix", "Suffix should be added to Region1");
            expect(manager.getRegion(1)->getName() == "Region2_Suffix", "Suffix should be added to Region2");
            expect(manager.getRegion(2)->getName() == "Region3_Suffix", "Suffix should be added to Region3");
        }

        beginTest("Prefix + Suffix");
        {
            RegionManager manager;
            manager.addRegion(Region("Region1", 0, 1000));
            manager.addRegion(Region("Region2", 1000, 2000));

            std::vector<int> indices = {0, 1};
            std::vector<juce::String> oldNames = {"Region1", "Region2"};
            std::vector<juce::String> newNames = {"Pre_Region1_Suf", "Pre_Region2_Suf"};

            juce::UndoManager undoManager;

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "Pre_Region1_Suf", "Both prefix and suffix should be added");
            expect(manager.getRegion(1)->getName() == "Pre_Region2_Suf", "Both prefix and suffix should be added");
        }

        beginTest("Prefix + Suffix with numbering");
        {
            RegionManager manager;
            manager.addRegion(Region("Region1", 0, 1000));
            manager.addRegion(Region("Region2", 1000, 2000));
            manager.addRegion(Region("Region3", 2000, 3000));

            std::vector<int> indices = {0, 1, 2};
            std::vector<juce::String> oldNames = {"Region1", "Region2", "Region3"};
            std::vector<juce::String> newNames = {"Pre_Region1_Suf_1", "Pre_Region2_Suf_2", "Pre_Region3_Suf_3"};

            juce::UndoManager undoManager;

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "Pre_Region1_Suf_1", "Prefix + suffix + number");
            expect(manager.getRegion(1)->getName() == "Pre_Region2_Suf_2", "Prefix + suffix + number");
            expect(manager.getRegion(2)->getName() == "Pre_Region3_Suf_3", "Prefix + suffix + number");
        }
    }
};

/**
 * Group 4: Undo/Redo Tests
 */
class BatchRenameUndoRedoTests : public juce::UnitTest
{
public:
    BatchRenameUndoRedoTests()
        : juce::UnitTest("RegionList Batch Rename - Undo/Redo", "RegionListPanel")
    {
    }

    void runTest() override
    {
        beginTest("Undo batch rename");
        {
            RegionManager manager;
            manager.addRegion(Region("Original1", 0, 1000));
            manager.addRegion(Region("Original2", 1000, 2000));
            manager.addRegion(Region("Original3", 2000, 3000));

            juce::UndoManager undoManager;

            std::vector<int> indices = {0, 1, 2};
            std::vector<juce::String> oldNames = {"Original1", "Original2", "Original3"};
            std::vector<juce::String> newNames = {"Renamed1", "Renamed2", "Renamed3"};

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            // Verify rename
            expect(manager.getRegion(0)->getName() == "Renamed1", "Region should be renamed");
            expect(manager.getRegion(1)->getName() == "Renamed2", "Region should be renamed");
            expect(manager.getRegion(2)->getName() == "Renamed3", "Region should be renamed");

            // Undo
            undoManager.undo();

            // Verify original names restored
            expect(manager.getRegion(0)->getName() == "Original1", "Region should be restored to 'Original1'");
            expect(manager.getRegion(1)->getName() == "Original2", "Region should be restored to 'Original2'");
            expect(manager.getRegion(2)->getName() == "Original3", "Region should be restored to 'Original3'");
        }

        beginTest("Redo batch rename");
        {
            RegionManager manager;
            manager.addRegion(Region("Original1", 0, 1000));
            manager.addRegion(Region("Original2", 1000, 2000));

            juce::UndoManager undoManager;

            std::vector<int> indices = {0, 1};
            std::vector<juce::String> oldNames = {"Original1", "Original2"};
            std::vector<juce::String> newNames = {"Renamed1", "Renamed2"};

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);
            undoManager.undo();

            // Redo
            undoManager.redo();

            // Verify names are renamed again
            expect(manager.getRegion(0)->getName() == "Renamed1", "Region should be renamed again");
            expect(manager.getRegion(1)->getName() == "Renamed2", "Region should be renamed again");
        }

        beginTest("Multiple undo/redo cycles");
        {
            RegionManager manager;
            manager.addRegion(Region("Original", 0, 1000));

            juce::UndoManager undoManager;

            // Perform 5 renames
            for (int i = 0; i < 5; ++i)
            {
                std::vector<int> indices = {0};
                std::vector<juce::String> oldNames = {manager.getRegion(0)->getName()};
                std::vector<juce::String> newNames = {juce::String("Rename") + juce::String(i + 1)};

                auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
                undoManager.perform(action);
            }

            expect(manager.getRegion(0)->getName() == "Rename5", "Final rename should be 'Rename5'");

            // Undo all
            for (int i = 0; i < 5; ++i)
                undoManager.undo();

            expect(manager.getRegion(0)->getName() == "Original", "Should be back to 'Original'");

            // Redo all
            for (int i = 0; i < 5; ++i)
                undoManager.redo();

            expect(manager.getRegion(0)->getName() == "Rename5", "Should be 'Rename5' again");
        }
    }
};

/**
 * Group 5: Edge Cases and Validation
 */
class BatchRenameEdgeCasesTests : public juce::UnitTest
{
public:
    BatchRenameEdgeCasesTests()
        : juce::UnitTest("RegionList Batch Rename - Edge Cases", "RegionListPanel")
    {
    }

    void runTest() override
    {
        beginTest("Empty selection");
        {
            RegionManager manager;
            manager.addRegion(Region("Region1", 0, 1000));

            std::vector<int> indices; // Empty
            std::vector<juce::String> oldNames;
            std::vector<juce::String> newNames;

            juce::UndoManager undoManager;

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "Region1", "Region should be unchanged");
        }

        beginTest("Single region selection");
        {
            RegionManager manager;
            manager.addRegion(Region("OldName", 0, 1000));

            std::vector<int> indices = {0};
            std::vector<juce::String> oldNames = {"OldName"};
            std::vector<juce::String> newNames = {"NewName"};

            juce::UndoManager undoManager;

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "NewName", "Single region should be renamed");
        }

        beginTest("Non-contiguous selection");
        {
            RegionManager manager;
            manager.addRegion(Region("Region1", 0, 1000));
            manager.addRegion(Region("Region2", 1000, 2000));
            manager.addRegion(Region("Region3", 2000, 3000));
            manager.addRegion(Region("Region4", 3000, 4000));

            std::vector<int> indices = {0, 2}; // Select regions 0 and 2 (skip 1 and 3)
            std::vector<juce::String> oldNames = {"Region1", "Region3"};
            std::vector<juce::String> newNames = {"Renamed1", "Renamed3"};

            juce::UndoManager undoManager;

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "Renamed1", "Region 0 should be renamed");
            expect(manager.getRegion(1)->getName() == "Region2", "Region 1 should be unchanged");
            expect(manager.getRegion(2)->getName() == "Renamed3", "Region 2 should be renamed");
            expect(manager.getRegion(3)->getName() == "Region4", "Region 3 should be unchanged");
        }

        beginTest("Empty new name (edge case)");
        {
            RegionManager manager;
            manager.addRegion(Region("OldName", 0, 1000));

            std::vector<int> indices = {0};
            std::vector<juce::String> oldNames = {"OldName"};
            std::vector<juce::String> newNames = {""}; // Empty name

            juce::UndoManager undoManager;

            auto* action = new BatchRenameRegionUndoAction(manager, nullptr, indices, oldNames, newNames);
            undoManager.perform(action);

            expect(manager.getRegion(0)->getName() == "", "Region should accept empty name");
        }
    }
};

// ============================================================================
// Register All Test Groups
// ============================================================================

static BatchRenamePatternModeTests batchRenamePatternModeTests;
static BatchRenameFindReplaceTests batchRenameFindReplaceTests;
static BatchRenamePrefixSuffixTests batchRenamePrefixSuffixTests;
static BatchRenameUndoRedoTests batchRenameUndoRedoTests;
static BatchRenameEdgeCasesTests batchRenameEdgeCasesTests;
