/*
  ==============================================================================

    UndoMemoryBudget.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Shared sizing math for juce::UndoableAction::getSizeInUnits().
    JUCE's UndoManager (juce_UndoManager.cpp, perform()) only trims old
    transactions once the running total of reported units exceeds the
    configured maximum AND more transactions than the configured minimum
    are on the stack. Every UndoableAction in this codebase reports its
    size through the helpers here so that budget bounds real memory use
    instead of a fixed transaction count. See Document.cpp for where the
    budget is wired into setMaxNumberOfStoredUnits().

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <cstdint>
#include <limits>

namespace UndoMemory
{
    // Units are KiB (1024 bytes) -- an arbitrary but fixed scale; what
    // matters is that every action uses the same one.

    // JUCE keeps "up to 100 levels" as the product's stated undo depth.
    constexpr int kMaxLevels = 100;

    // Passed as UndoManager's minimumTransactionsToKeep: a single
    // oversized action (e.g. a 1-hour-file snapshot) must still be
    // undoable, even alone, so this floor is small.
    constexpr int kMinTransactionsKept = 2;

    /**
     * Overall undo memory budget, in KiB, computed once per process
     * (function-local static -- physical RAM doesn't change at runtime).
     *
     * Budget = min(25% of physical RAM, 4 GiB), then HALVED: JUCE's
     * UndoManager keeps performed-then-undone actions around for redo on
     * an uncounted second stack, so the real worst-case footprint can be
     * up to double what getSizeInUnits() reports. Halving the budget here
     * keeps the real footprint within the original 25%-of-RAM/4GiB
     * ceiling.
     */
    inline int maxUnits()
    {
        static const int cached = []() -> int
        {
            int64_t ramMB = juce::SystemStats::getMemorySizeInMegabytes();
            if (ramMB <= 0)
                ramMB = 8192; // sysinfo() can fail on Linux; assume 8 GB.

            const int64_t ramKiB      = ramMB * 1024;
            const int64_t quarterKiB  = ramKiB / 4;
            const int64_t fourGiBKiB  = 4LL * 1024 * 1024;
            const int64_t budgetKiB   = juce::jmin(quarterKiB, fourGiBKiB) / 2;

            // Must be usable as an int (UndoManager stores unit counts as
            // int) and large enough that unitsForBytes()'s floor below is
            // never zero.
            const int64_t clamped = juce::jlimit((int64_t) 100,
                                                  (int64_t) std::numeric_limits<int>::max(),
                                                  budgetKiB);
            return (int) clamped;
        }();

        return cached;
    }

    /**
     * Convert a byte count to budget units (KiB). The arithmetic runs in
     * 64-bit before anything is narrowed to int, so a huge input (e.g. a
     * corrupt or adversarial size) cannot overflow on the way in.
     *
     * - Floor (maxUnits() / kMaxLevels): guarantees ~100 small edits can
     *   still fill the whole budget, so "up to 100 undo levels" stays
     *   true for cheap actions.
     * - Ceiling (maxUnits()): a single action can never report more than
     *   the whole budget, which keeps JUCE's int running total
     *   (totalUnitsStored in juce_UndoManager.cpp) from overflowing.
     */
    inline int unitsForBytes(size_t bytes)
    {
        // ceil(bytes / 1024), computed as a double BEFORE narrowing. An
        // integer "(bytes + 1023) / 1024" trick would wrap around for
        // bytes near SIZE_MAX (bytes + 1023 overflows), silently producing
        // a tiny result instead of a huge one; double arithmetic has no
        // such wraparound (it loses precision for astronomical inputs, but
        // stays a huge positive number, which is all the clamp below needs).
        const double kibExact = std::ceil(static_cast<double>(bytes) / 1024.0);
        const int64_t intMax64 = static_cast<int64_t>(std::numeric_limits<int>::max());
        const int64_t kib = kibExact >= static_cast<double>(intMax64)
                                ? intMax64
                                : static_cast<int64_t>(kibExact);

        const int64_t budget = maxUnits();
        return static_cast<int>(juce::jlimit(budget / kMaxLevels, budget, kib));
    }

    /** Convenience for the common case: one stored interleaved-by-channel buffer. */
    inline int unitsForBuffer(const juce::AudioBuffer<float>& buffer)
    {
        const size_t bytes = static_cast<size_t>(buffer.getNumChannels()) *
                             static_cast<size_t>(buffer.getNumSamples()) * sizeof(float);
        return unitsForBytes(bytes);
    }
}
