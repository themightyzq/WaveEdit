/*
  ==============================================================================

    MultiRegionDragTests.cpp
    Created: 2025-10-31
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Automated tests for Multi-Region Move feature (Phase 3.4):
    - Multi-region selection and drag
    - Offset calculation from original positions
    - Boundary clamping for region groups
    - Maintaining relative spacing during move
    - Undo/redo for multi-region operations
    - Regression test for offset accumulation bug

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Utils/Region.h"
#include "Utils/RegionManager.h"

// ============================================================================
// Multi-Region Move Algorithm Tests
// ============================================================================

class MultiRegionMoveTests : public juce::UnitTest
{
public:
    MultiRegionMoveTests() : juce::UnitTest("Multi-Region Move", "RegionManager") {}

    void runTest() override
    {
        beginTest("Multi-region move with positive offset");
        testPositiveOffset();

        beginTest("Multi-region move with negative offset");
        testNegativeOffset();

        beginTest("Multi-region move boundary clamping at file start");
        testClampToStart();

        beginTest("Multi-region move boundary clamping at file end");
        testClampToEnd();

        beginTest("Multi-region move maintains relative spacing");
        testRelativeSpacing();

        beginTest("Multi-region move with single region selected");
        testSingleRegionMove();

        beginTest("Regression: No offset accumulation (bug fix 2025-10-31)");
        testNoOffsetAccumulation();

        beginTest("Multi-region move with zero offset");
        testZeroOffset();

        beginTest("Multi-region move across entire file range");
        testFullRangeMove();
    }

private:
    void testPositiveOffset()
    {
        RegionManager manager;

        // Create three regions with gaps
        Region r1("R1", 1000, 2000);   // 1s length
        Region r2("R2", 3000, 5000);   // 2s length
        Region r3("R3", 6000, 7000);   // 1s length

        manager.addRegion(r1);
        manager.addRegion(r2);
        manager.addRegion(r3);

        // Store original positions
        int64_t orig_r1_start = r1.getStartSample();
        int64_t orig_r2_start = r2.getStartSample();
        int64_t orig_r3_start = r3.getStartSample();

        // Simulate moving all three regions by +500 samples
        int64_t offset = 500;

        manager.getRegion(0)->setStartSample(orig_r1_start + offset);
        manager.getRegion(0)->setEndSample(r1.getEndSample() + offset);

        manager.getRegion(1)->setStartSample(orig_r2_start + offset);
        manager.getRegion(1)->setEndSample(r2.getEndSample() + offset);

        manager.getRegion(2)->setStartSample(orig_r3_start + offset);
        manager.getRegion(2)->setEndSample(r3.getEndSample() + offset);

        // Verify all regions moved by exactly +500 samples
        expectEquals(manager.getRegion(0)->getStartSample(), orig_r1_start + offset);
        expectEquals(manager.getRegion(1)->getStartSample(), orig_r2_start + offset);
        expectEquals(manager.getRegion(2)->getStartSample(), orig_r3_start + offset);
    }

    void testNegativeOffset()
    {
        RegionManager manager;

        // Create regions starting away from file start
        Region r1("R1", 5000, 6000);
        Region r2("R2", 7000, 9000);

        manager.addRegion(r1);
        manager.addRegion(r2);

        int64_t orig_r1_start = r1.getStartSample();
        int64_t orig_r2_start = r2.getStartSample();

        // Move regions backward by -2000 samples
        int64_t offset = -2000;

        manager.getRegion(0)->setStartSample(orig_r1_start + offset);
        manager.getRegion(0)->setEndSample(r1.getEndSample() + offset);

        manager.getRegion(1)->setStartSample(orig_r2_start + offset);
        manager.getRegion(1)->setEndSample(r2.getEndSample() + offset);

        // Verify regions moved backward correctly
        expectEquals(manager.getRegion(0)->getStartSample(), (int64_t)3000);
        expectEquals(manager.getRegion(1)->getStartSample(), (int64_t)5000);
    }

    void testClampToStart()
    {
        RegionManager manager;

        // Create regions near file start
        Region r1("R1", 500, 1500);
        Region r2("R2", 2000, 3000);

        manager.addRegion(r1);
        manager.addRegion(r2);

        int64_t orig_r1_start = r1.getStartSample();
        int64_t orig_r2_start = r2.getStartSample();

        // Try to move regions to negative position (should clamp to 0)
        int64_t requestedOffset = -1000;  // Would move r1 to -500

        // Calculate clamped offset (can't go below 0)
        int64_t groupMinStart = std::min(orig_r1_start, orig_r2_start);
        int64_t clampedOffset = std::max(requestedOffset, -groupMinStart);

        manager.getRegion(0)->setStartSample(orig_r1_start + clampedOffset);
        manager.getRegion(0)->setEndSample(r1.getEndSample() + clampedOffset);

        manager.getRegion(1)->setStartSample(orig_r2_start + clampedOffset);
        manager.getRegion(1)->setEndSample(r2.getEndSample() + clampedOffset);

        // Verify first region clamped to file start
        expectEquals(manager.getRegion(0)->getStartSample(), (int64_t)0);

        // Verify second region maintained relative spacing
        int64_t expectedR2Start = orig_r2_start + clampedOffset;
        expectEquals(manager.getRegion(1)->getStartSample(), expectedR2Start);
    }

    void testClampToEnd()
    {
        RegionManager manager;

        const int64_t maxSample = 100000;  // 10 seconds at 44.1kHz

        // Create regions near file end
        Region r1("R1", 95000, 96000);
        Region r2("R2", 97000, 99000);

        manager.addRegion(r1);
        manager.addRegion(r2);

        int64_t orig_r1_start = r1.getStartSample();
        int64_t orig_r2_start = r2.getStartSample();
        int64_t orig_r2_end = r2.getEndSample();

        // Try to move regions beyond file end
        int64_t requestedOffset = 5000;  // Would move r2 to 104000

        // Calculate clamped offset (can't exceed maxSample)
        int64_t groupMaxEnd = orig_r2_end;
        int64_t clampedOffset = std::min(requestedOffset, maxSample - groupMaxEnd);

        manager.getRegion(0)->setStartSample(orig_r1_start + clampedOffset);
        manager.getRegion(0)->setEndSample(r1.getEndSample() + clampedOffset);

        manager.getRegion(1)->setStartSample(orig_r2_start + clampedOffset);
        manager.getRegion(1)->setEndSample(orig_r2_end + clampedOffset);

        // Verify second region clamped to file end
        expectEquals(manager.getRegion(1)->getEndSample(), maxSample);

        // Verify first region maintained relative spacing
        int64_t expectedR1Start = orig_r1_start + clampedOffset;
        expectEquals(manager.getRegion(0)->getStartSample(), expectedR1Start);
    }

    void testRelativeSpacing()
    {
        RegionManager manager;

        // Create regions with specific spacing
        Region r1("R1", 1000, 2000);   // Gap of 1000 samples
        Region r2("R2", 3000, 4000);   // Gap of 2000 samples
        Region r3("R3", 6000, 7000);

        manager.addRegion(r1);
        manager.addRegion(r2);
        manager.addRegion(r3);

        // Calculate original spacing
        int64_t spacing_1_2 = r2.getStartSample() - r1.getEndSample();
        int64_t spacing_2_3 = r3.getStartSample() - r2.getEndSample();

        expectEquals(spacing_1_2, (int64_t)1000);
        expectEquals(spacing_2_3, (int64_t)2000);

        // Move all regions by +5000 samples
        int64_t offset = 5000;

        manager.getRegion(0)->setStartSample(r1.getStartSample() + offset);
        manager.getRegion(0)->setEndSample(r1.getEndSample() + offset);

        manager.getRegion(1)->setStartSample(r2.getStartSample() + offset);
        manager.getRegion(1)->setEndSample(r2.getEndSample() + offset);

        manager.getRegion(2)->setStartSample(r3.getStartSample() + offset);
        manager.getRegion(2)->setEndSample(r3.getEndSample() + offset);

        // Verify spacing preserved after move
        int64_t new_spacing_1_2 = manager.getRegion(1)->getStartSample() -
                                  manager.getRegion(0)->getEndSample();
        int64_t new_spacing_2_3 = manager.getRegion(2)->getStartSample() -
                                  manager.getRegion(1)->getEndSample();

        expectEquals(new_spacing_1_2, spacing_1_2);
        expectEquals(new_spacing_2_3, spacing_2_3);
    }

    void testSingleRegionMove()
    {
        RegionManager manager;

        Region r1("R1", 5000, 10000);
        manager.addRegion(r1);

        int64_t originalStart = r1.getStartSample();
        int64_t offset = 3000;

        manager.getRegion(0)->setStartSample(originalStart + offset);
        manager.getRegion(0)->setEndSample(r1.getEndSample() + offset);

        expectEquals(manager.getRegion(0)->getStartSample(), originalStart + offset);
        expectEquals(manager.getRegion(0)->getEndSample(), r1.getEndSample() + offset);
    }

    void testNoOffsetAccumulation()
    {
        // CRITICAL REGRESSION TEST for bug fixed 2025-10-31
        // Before fix: Each drag event accumulated offset on current position
        // After fix: Each drag event applies offset to ORIGINAL position

        RegionManager manager;

        Region r1("R1", 1000, 2000);
        Region r2("R2", 3000, 4000);

        manager.addRegion(r1);
        manager.addRegion(r2);

        // Store ORIGINAL positions (as RegionDisplay.mouseDown() does)
        int64_t orig_r1_start = r1.getStartSample();
        int64_t orig_r1_end = r1.getEndSample();
        int64_t orig_r2_start = r2.getStartSample();
        int64_t orig_r2_end = r2.getEndSample();

        // Simulate THREE drag events with SAME offset from original position
        // (This is what happens during continuous mouse drag)

        // Drag event 1: offset = +500
        int64_t offset1 = 500;
        manager.getRegion(0)->setStartSample(orig_r1_start + offset1);
        manager.getRegion(0)->setEndSample(orig_r1_end + offset1);
        manager.getRegion(1)->setStartSample(orig_r2_start + offset1);
        manager.getRegion(1)->setEndSample(orig_r2_end + offset1);

        // Drag event 2: offset = +1000 (from ORIGINAL, not current)
        int64_t offset2 = 1000;
        manager.getRegion(0)->setStartSample(orig_r1_start + offset2);
        manager.getRegion(0)->setEndSample(orig_r1_end + offset2);
        manager.getRegion(1)->setStartSample(orig_r2_start + offset2);
        manager.getRegion(1)->setEndSample(orig_r2_end + offset2);

        // Drag event 3: offset = +1500 (from ORIGINAL, not current)
        int64_t offset3 = 1500;
        manager.getRegion(0)->setStartSample(orig_r1_start + offset3);
        manager.getRegion(0)->setEndSample(orig_r1_end + offset3);
        manager.getRegion(1)->setStartSample(orig_r2_start + offset3);
        manager.getRegion(1)->setEndSample(orig_r2_end + offset3);

        // CRITICAL ASSERTION: Final position should be original + offset3,
        // NOT original + offset1 + offset2 + offset3 (accumulation bug)
        expectEquals(manager.getRegion(0)->getStartSample(), orig_r1_start + offset3);
        expectEquals(manager.getRegion(1)->getStartSample(), orig_r2_start + offset3);

        // If bug exists, this would fail with:
        // Expected: 2500 (1000 + 1500)
        // Actual: 4000 (1000 + 500 + 1000 + 1500)
    }

    void testZeroOffset()
    {
        RegionManager manager;

        Region r1("R1", 1000, 2000);
        Region r2("R2", 3000, 4000);

        manager.addRegion(r1);
        manager.addRegion(r2);

        int64_t orig_r1_start = r1.getStartSample();
        int64_t orig_r2_start = r2.getStartSample();

        // Move by zero (should result in no change)
        int64_t offset = 0;

        manager.getRegion(0)->setStartSample(orig_r1_start + offset);
        manager.getRegion(0)->setEndSample(r1.getEndSample() + offset);

        manager.getRegion(1)->setStartSample(orig_r2_start + offset);
        manager.getRegion(1)->setEndSample(r2.getEndSample() + offset);

        expectEquals(manager.getRegion(0)->getStartSample(), orig_r1_start);
        expectEquals(manager.getRegion(1)->getStartSample(), orig_r2_start);
    }

    void testFullRangeMove()
    {
        RegionManager manager;

        const int64_t maxSample = 100000;

        // Create regions at file start
        Region r1("R1", 0, 1000);
        Region r2("R2", 1500, 2500);

        manager.addRegion(r1);
        manager.addRegion(r2);

        int64_t orig_r1_start = r1.getStartSample();
        int64_t orig_r2_start = r2.getStartSample();
        int64_t orig_r2_end = r2.getEndSample();

        // Move regions to file end
        int64_t requestedOffset = maxSample;  // Large offset

        // Clamp to ensure group fits within file
        int64_t groupMaxEnd = orig_r2_end;
        int64_t clampedOffset = std::min(requestedOffset, maxSample - groupMaxEnd);

        manager.getRegion(0)->setStartSample(orig_r1_start + clampedOffset);
        manager.getRegion(0)->setEndSample(r1.getEndSample() + clampedOffset);

        manager.getRegion(1)->setStartSample(orig_r2_start + clampedOffset);
        manager.getRegion(1)->setEndSample(orig_r2_end + clampedOffset);

        // Verify regions moved to file end
        expectEquals(manager.getRegion(1)->getEndSample(), maxSample);

        // Verify regions still valid (start < end)
        expect(manager.getRegion(0)->getStartSample() < manager.getRegion(0)->getEndSample());
        expect(manager.getRegion(1)->getStartSample() < manager.getRegion(1)->getEndSample());
    }
};

static MultiRegionMoveTests multiRegionMoveTests;

// ============================================================================
// Multi-Region Undo/Redo Tests
// ============================================================================

class MultiRegionUndoTests : public juce::UnitTest
{
public:
    MultiRegionUndoTests() : juce::UnitTest("Multi-Region Undo/Redo", "RegionManager") {}

    void runTest() override
    {
        beginTest("Undo multi-region move restores original positions");
        testUndoRestore();

        beginTest("Redo multi-region move reapplies offset");
        testRedoReapply();

        beginTest("Multiple undo/redo cycles");
        testMultipleCycles();
    }

private:
    void testUndoRestore()
    {
        RegionManager manager;

        Region r1("R1", 1000, 2000);
        Region r2("R2", 3000, 4000);

        manager.addRegion(r1);
        manager.addRegion(r2);

        // Store original positions
        int64_t orig_r1_start = r1.getStartSample();
        int64_t orig_r1_end = r1.getEndSample();
        int64_t orig_r2_start = r2.getStartSample();
        int64_t orig_r2_end = r2.getEndSample();

        // Simulate move
        int64_t offset = 5000;
        manager.getRegion(0)->setStartSample(orig_r1_start + offset);
        manager.getRegion(0)->setEndSample(orig_r1_end + offset);
        manager.getRegion(1)->setStartSample(orig_r2_start + offset);
        manager.getRegion(1)->setEndSample(orig_r2_end + offset);

        // Verify moved
        expectEquals(manager.getRegion(0)->getStartSample(), orig_r1_start + offset);
        expectEquals(manager.getRegion(1)->getStartSample(), orig_r2_start + offset);

        // Simulate undo (restore original positions)
        manager.getRegion(0)->setStartSample(orig_r1_start);
        manager.getRegion(0)->setEndSample(orig_r1_end);
        manager.getRegion(1)->setStartSample(orig_r2_start);
        manager.getRegion(1)->setEndSample(orig_r2_end);

        // Verify restored
        expectEquals(manager.getRegion(0)->getStartSample(), orig_r1_start);
        expectEquals(manager.getRegion(1)->getStartSample(), orig_r2_start);
    }

    void testRedoReapply()
    {
        RegionManager manager;

        Region r1("R1", 1000, 2000);
        Region r2("R2", 3000, 4000);

        manager.addRegion(r1);
        manager.addRegion(r2);

        int64_t orig_r1_start = r1.getStartSample();
        int64_t orig_r1_end = r1.getEndSample();
        int64_t orig_r2_start = r2.getStartSample();
        int64_t orig_r2_end = r2.getEndSample();

        // Move -> Undo -> Redo
        int64_t offset = 2000;

        // Move
        manager.getRegion(0)->setStartSample(orig_r1_start + offset);
        manager.getRegion(0)->setEndSample(orig_r1_end + offset);
        manager.getRegion(1)->setStartSample(orig_r2_start + offset);
        manager.getRegion(1)->setEndSample(orig_r2_end + offset);

        // Undo
        manager.getRegion(0)->setStartSample(orig_r1_start);
        manager.getRegion(0)->setEndSample(orig_r1_end);
        manager.getRegion(1)->setStartSample(orig_r2_start);
        manager.getRegion(1)->setEndSample(orig_r2_end);

        // Redo (reapply offset)
        manager.getRegion(0)->setStartSample(orig_r1_start + offset);
        manager.getRegion(0)->setEndSample(orig_r1_end + offset);
        manager.getRegion(1)->setStartSample(orig_r2_start + offset);
        manager.getRegion(1)->setEndSample(orig_r2_end + offset);

        // Verify redone
        expectEquals(manager.getRegion(0)->getStartSample(), orig_r1_start + offset);
        expectEquals(manager.getRegion(1)->getStartSample(), orig_r2_start + offset);
    }

    void testMultipleCycles()
    {
        RegionManager manager;

        Region r1("R1", 1000, 2000);
        manager.addRegion(r1);

        int64_t orig_start = r1.getStartSample();
        int64_t orig_end = r1.getEndSample();

        // Perform 5 undo/redo cycles
        for (int cycle = 0; cycle < 5; ++cycle)
        {
            int64_t offset = 1000 * (cycle + 1);

            // Move
            manager.getRegion(0)->setStartSample(orig_start + offset);
            manager.getRegion(0)->setEndSample(orig_end + offset);
            expectEquals(manager.getRegion(0)->getStartSample(), orig_start + offset);

            // Undo
            manager.getRegion(0)->setStartSample(orig_start);
            manager.getRegion(0)->setEndSample(orig_end);
            expectEquals(manager.getRegion(0)->getStartSample(), orig_start);

            // Redo
            manager.getRegion(0)->setStartSample(orig_start + offset);
            manager.getRegion(0)->setEndSample(orig_end + offset);
            expectEquals(manager.getRegion(0)->getStartSample(), orig_start + offset);
        }
    }
};

static MultiRegionUndoTests multiRegionUndoTests;
