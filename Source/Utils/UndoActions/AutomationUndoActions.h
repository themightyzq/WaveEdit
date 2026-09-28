/*
  ==============================================================================

    AutomationUndoActions.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Undo actions for plugin parameter automation point edits made
    through the AutomationLanesPanel curve view (add / move / delete /
    curve-type change / clear).

    Each user gesture stores a before+after snapshot of the lane's
    AutomationCurve point list. The lane is looked up at perform/undo
    time by (pluginIndex, parameterIndex), so deleting a lane while
    its history is still on the UndoManager stack does NOT crash —
    perform/undo simply return false when the lane is gone.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../../Automation/AutomationData.h"
#include "../../Automation/AutomationManager.h"
#include "UndoMemoryBudget.h"

//==============================================================================
/**
 * Apply a snapshot of automation points to the lane identified by
 * (pluginIndex, parameterIndex). Used by both perform() and undo().
 *
 * The first call to perform() (when the UndoManager registers the
 * action) is a no-op because the gesture handler already applied the
 * change for live visual feedback. Subsequent perform() / undo()
 * calls do real work.
 */
class AutomationCurveUndoAction : public juce::UndoableAction
{
public:
    AutomationCurveUndoAction(AutomationManager& manager,
                              int pluginIndex,
                              int parameterIndex,
                              std::vector<AutomationPoint> beforeSnapshot,
                              std::vector<AutomationPoint> afterSnapshot)
        : m_manager(manager),
          m_pluginIndex(pluginIndex),
          m_parameterIndex(parameterIndex),
          m_before(std::move(beforeSnapshot)),
          m_after(std::move(afterSnapshot))
    {
        // No audio storage -- report the (tiny, fixed) size of this object
        // plus its two point snapshots, which hits UndoMemory's floor for
        // all but pathologically dense curves.
        const size_t pointBytes = static_cast<size_t>(m_before.size() + m_after.size()) * sizeof(AutomationPoint);
        m_sizeInUnits = UndoMemory::unitsForBytes(sizeof(*this) + pointBytes);
    }

    bool perform() override
    {
        if (m_skipFirstPerform)
        {
            // The gesture handler already applied the change for live
            // visual feedback. Don't double-apply on initial registration.
            m_skipFirstPerform = false;
            return true;
        }
        return applySnapshot(m_after);
    }

    bool undo() override
    {
        return applySnapshot(m_before);
    }

    int getSizeInUnits() override { return m_sizeInUnits; }

private:
    bool applySnapshot(const std::vector<AutomationPoint>& snapshot)
    {
        auto* lane = m_manager.findLane(m_pluginIndex, m_parameterIndex);
        if (lane == nullptr)
            return false;  // Lane removed since gesture — graceful no-op.

        lane->curve.clear();
        for (const auto& pt : snapshot)
            lane->curve.addPoint(pt);
        return true;
    }

    AutomationManager& m_manager;
    int m_pluginIndex;
    int m_parameterIndex;
    std::vector<AutomationPoint> m_before;
    std::vector<AutomationPoint> m_after;
    bool m_skipFirstPerform = true;
    int m_sizeInUnits = 0;  // Fixed at construction; see ctor.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationCurveUndoAction)
};
