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

    int getSizeInUnits() override
    {
        const int pointSize = static_cast<int>(sizeof(AutomationPoint));
        return static_cast<int>(sizeof(*this))
             + static_cast<int>(m_before.size()) * pointSize
             + static_cast<int>(m_after.size())  * pointSize;
    }

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationCurveUndoAction)
};
