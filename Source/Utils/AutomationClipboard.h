/*
  ==============================================================================

    AutomationClipboard.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Process-wide clipboard for AutomationPoint copy/paste in the
    Automation Lanes panel. Stores points with their times normalized to
    a leading zero (subtract earliest selected time on copy) so that
    paste can place them at any anchor by simply adding the anchor time.

    Lifetime: a static instance lives for the application's lifetime.
    Thread safety: message-thread only. There's no audio-thread access
    path; the clipboard is purely a UI affordance.

  ==============================================================================
*/

#pragma once

#include <vector>
#include <algorithm>

#include "../Automation/AutomationData.h"

class AutomationClipboard
{
public:
    /** Singleton accessor. Lazy-init; lives until process exit. */
    static AutomationClipboard& getInstance()
    {
        static AutomationClipboard instance;
        return instance;
    }

    AutomationClipboard(const AutomationClipboard&)            = delete;
    AutomationClipboard& operator=(const AutomationClipboard&) = delete;

    /**
     * Copy a set of points. Times are normalized so the earliest point
     * starts at 0; paste reapplies the anchor on retrieval.
     */
    void copy(const std::vector<AutomationPoint>& points)
    {
        m_points.clear();
        if (points.empty())
            return;

        double earliest = points.front().timeInSeconds;
        for (const auto& p : points)
            earliest = std::min(earliest, p.timeInSeconds);

        m_points.reserve(points.size());
        for (const auto& p : points)
        {
            AutomationPoint normalized = p;
            normalized.timeInSeconds -= earliest;
            m_points.push_back(normalized);
        }
        std::sort(m_points.begin(), m_points.end());
    }

    /**
     * Materialize the clipboard at @p anchorTime. Returns a fresh
     * vector with each point's `timeInSeconds = anchor + offset`.
     */
    std::vector<AutomationPoint> pasteAt(double anchorTime) const
    {
        std::vector<AutomationPoint> out;
        out.reserve(m_points.size());
        for (const auto& p : m_points)
        {
            AutomationPoint shifted = p;
            shifted.timeInSeconds = anchorTime + p.timeInSeconds;
            out.push_back(shifted);
        }
        return out;
    }

    bool isEmpty() const { return m_points.empty(); }
    int  getNumPoints() const { return static_cast<int>(m_points.size()); }
    const std::vector<AutomationPoint>& getPoints() const { return m_points; }

    /** Test hook: clear the clipboard. */
    void clear() { m_points.clear(); }

private:
    AutomationClipboard() = default;

    std::vector<AutomationPoint> m_points;
};
