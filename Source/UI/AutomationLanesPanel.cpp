/*
  ==============================================================================

    AutomationLanesPanel.cpp
    Copyright (C) 2025 ZQ SFX

  ==============================================================================
*/

#include "AutomationLanesPanel.h"

#include "../Audio/AudioEngine.h"
#include "../Automation/AutomationRecorder.h"
#include "../Utils/AutomationClipboard.h"
#include "../Utils/UndoActions/AutomationUndoActions.h"
#include "ThemeManager.h"

namespace
{
    constexpr int kPanelWidth  = 720;
    constexpr int kPanelHeight = 480;

    constexpr int kCurveLeftPad    = 8;        // breathing room on each end of curve
    constexpr int kCurveRightPad   = 8;
    constexpr int kCurveTopPad     = 4;
    constexpr int kCurveBottomPad  = 4;
    constexpr int kCurveSegments   = 32;       // segments per inter-point curve render

    // Theme-driven palette. Resolved at call time so a theme switch
    // re-skins all surfaces on the next paint. Workflow-specific
    // accents (selected point, drag overlay) are derived from theme
    // tokens so they stay distinguishable in both Dark and Light.
    inline const waveedit::Theme& theme() { return waveedit::ThemeManager::getInstance().getCurrent(); }

    inline juce::Colour themeBackground()      { return theme().background; }
    inline juce::Colour themeRowBackground()   { return theme().panel; }
    inline juce::Colour themeRowAlt()          { return theme().panelAlternate; }
    inline juce::Colour themeBorder()          { return theme().border; }
    inline juce::Colour themeAccent()          { return theme().accent; }
    inline juce::Colour themeCurveColour()     { return theme().accent.brighter(0.3f); }
    inline juce::Colour themePointColour()     { return theme().text; }
    inline juce::Colour themePointHover()      { return theme().warning; }
    inline juce::Colour themePointDrag()       { return theme().warning.brighter(0.2f); }
    inline juce::Colour themePointSelected()   { return juce::Colour(0xffff8a30); /* orange — workflow-specific */ }
    inline juce::Colour themePlayheadColour()  { return theme().error.brighter(0.1f); }
    inline juce::Colour themeRecordOnColour()  { return theme().error; }
    inline juce::Colour themeHighlightColour() { return theme().accent.brighter(0.1f); }
    inline juce::Colour themeTextColour()      { return theme().text; }
    inline juce::Colour themeSubtitleColour()  { return theme().textMuted; }
    inline juce::Colour themeEmptyColour()     { return theme().textMuted; }

    juce::String formatTime(double seconds)
    {
        if (seconds < 0.0) seconds = 0.0;
        const int totalMillis = static_cast<int>(std::round(seconds * 1000.0));
        const int mins        = totalMillis / 60000;
        const int secs        = (totalMillis / 1000) % 60;
        const int millis      = totalMillis % 1000;
        return juce::String::formatted("%02d:%02d.%03d", mins, secs, millis);
    }

    juce::String curveTypeLabel(AutomationPoint::CurveType c)
    {
        switch (c)
        {
            case AutomationPoint::CurveType::Linear:      return "Linear";
            case AutomationPoint::CurveType::Step:        return "Step (Hold)";
            case AutomationPoint::CurveType::SCurve:      return "S-Curve";
            case AutomationPoint::CurveType::Exponential: return "Exponential";
        }
        return {};
    }
}

//==============================================================================
// AutomationCurveView
//==============================================================================

AutomationLanesPanel::AutomationCurveView::AutomationCurveView()
{
    setMouseCursor(juce::MouseCursor::NormalCursor);
    setWantsKeyboardFocus(true);  // Cmd+A / Cmd+C / Cmd+V / Delete handled here.
}

void AutomationLanesPanel::AutomationCurveView::setLane(AutomationLane* lane)
{
    // Clearing selection on every refresh is intentional — the manager's
    // Listener fires on lane lifecycle (add/remove/clear/sidecar-reload)
    // and stale indices from before the change can dangle. It does NOT
    // fire on individual point edits (recorder appends, drags, deletes
    // through the curve), so live editing keeps the selection stable.
    m_selectedIndices.clear();
    m_lane = lane;
    repaint();
}

void AutomationLanesPanel::AutomationCurveView::setEngine(AudioEngine* engine)
{
    m_engine = engine;
}

void AutomationLanesPanel::AutomationCurveView::setUndoContext(juce::UndoManager* undoManager,
                                                                AutomationManager* manager)
{
    m_undoManager       = undoManager;
    m_undoLookupManager = manager;
}

std::vector<AutomationPoint> AutomationLanesPanel::AutomationCurveView::snapshotPoints() const
{
    if (m_lane == nullptr)
        return {};
    return m_lane->curve.getPoints();
}

void AutomationLanesPanel::AutomationCurveView::commitGesture(const juce::String& transactionName,
                                                               std::vector<AutomationPoint> beforeSnapshot)
{
    if (m_undoManager == nullptr || m_undoLookupManager == nullptr || m_lane == nullptr)
        return;

    auto afterSnapshot = m_lane->curve.getPoints();

    // Skip empty transactions: cleared point list with no prior points,
    // or before == after (e.g. dragging back to the original position).
    if (beforeSnapshot.size() == afterSnapshot.size())
    {
        bool identical = true;
        for (size_t i = 0; i < beforeSnapshot.size(); ++i)
        {
            const auto& a = beforeSnapshot[i];
            const auto& b = afterSnapshot[i];
            if (std::abs(a.timeInSeconds - b.timeInSeconds) > 1.0e-9
                || std::abs(a.value - b.value) > 1.0e-9f
                || a.curve != b.curve)
            {
                identical = false;
                break;
            }
        }
        if (identical)
            return;
    }

    m_undoManager->beginNewTransaction(transactionName);
    m_undoManager->perform(new AutomationCurveUndoAction(
        *m_undoLookupManager,
        m_lane->pluginIndex,
        m_lane->parameterIndex,
        std::move(beforeSnapshot),
        std::move(afterSnapshot)));
}

void AutomationLanesPanel::AutomationCurveView::flashHighlight()
{
    m_highlight = true;
    m_highlightFramesRemaining = 12;
    repaint();
}

double AutomationLanesPanel::AutomationCurveView::getDuration() const
{
    if (m_engine != nullptr)
    {
        const double len = m_engine->getTotalLength();
        if (len > 0.0)
            return len;
    }
    // No engine / empty file: use the curve's own time range, or 1s fallback.
    if (m_lane != nullptr && m_lane->curve.hasPoints())
    {
        const auto range = m_lane->curve.getTimeRange();
        return juce::jmax(range.second, 1.0);
    }
    return 1.0;
}

double AutomationLanesPanel::AutomationCurveView::xToTime(float x) const
{
    const float left  = static_cast<float>(kCurveLeftPad);
    const float right = static_cast<float>(getWidth() - kCurveRightPad);
    if (right <= left)
        return 0.0;
    const float clamped = juce::jlimit(left, right, x);
    const double frac   = static_cast<double>(clamped - left) / (right - left);
    return frac * getDuration();
}

float AutomationLanesPanel::AutomationCurveView::timeToX(double t) const
{
    const float left  = static_cast<float>(kCurveLeftPad);
    const float right = static_cast<float>(getWidth() - kCurveRightPad);
    const double dur  = getDuration();
    if (dur <= 0.0)
        return left;
    const double frac = juce::jlimit(0.0, 1.0, t / dur);
    return left + static_cast<float>(frac) * (right - left);
}

float AutomationLanesPanel::AutomationCurveView::valueToY(float v) const
{
    const float top    = static_cast<float>(kCurveTopPad);
    const float bottom = static_cast<float>(getHeight() - kCurveBottomPad);
    const float clamped = juce::jlimit(0.0f, 1.0f, v);
    // Value 0 → bottom, value 1 → top.
    return bottom - clamped * (bottom - top);
}

float AutomationLanesPanel::AutomationCurveView::yToValue(float y) const
{
    const float top    = static_cast<float>(kCurveTopPad);
    const float bottom = static_cast<float>(getHeight() - kCurveBottomPad);
    if (bottom <= top)
        return 0.5f;
    const float clamped = juce::jlimit(top, bottom, y);
    const float frac = (bottom - clamped) / (bottom - top);
    return juce::jlimit(0.0f, 1.0f, frac);
}

int AutomationLanesPanel::AutomationCurveView::findPointAt(float x, float y) const
{
    if (m_lane == nullptr)
        return -1;
    const auto pts = m_lane->curve.getPoints();
    for (int i = 0; i < static_cast<int>(pts.size()); ++i)
    {
        const float px = timeToX(pts[static_cast<size_t>(i)].timeInSeconds);
        const float py = valueToY(pts[static_cast<size_t>(i)].value);
        const float dx = x - px;
        const float dy = y - py;
        if (dx * dx + dy * dy <= kHitRadius * kHitRadius)
            return i;
    }
    return -1;
}

void AutomationLanesPanel::AutomationCurveView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background + border
    g.setColour(juce::Colour(0xff202020));
    g.fillRect(bounds);
    g.setColour(m_highlight ? themeHighlightColour() : themeBorder());
    g.drawRect(bounds, m_highlight ? 2.0f : 1.0f);

    if (m_lane == nullptr)
        return;

    // Centre line at value=0.5
    g.setColour(juce::Colour(0xff404040));
    const float midY = valueToY(0.5f);
    g.drawHorizontalLine(static_cast<int>(midY), bounds.getX(), bounds.getRight());

    auto pts = m_lane->curve.getPoints();

    // Curve path
    if (! pts.empty())
    {
        juce::Path path;
        const float startX = juce::jmax(timeToX(pts.front().timeInSeconds), bounds.getX());
        path.startNewSubPath(startX, valueToY(pts.front().value));

        // Constant prefix from left edge to first point.
        path.lineTo(timeToX(pts.front().timeInSeconds), valueToY(pts.front().value));

        for (size_t i = 0; i + 1 < pts.size(); ++i)
        {
            const auto& a = pts[i];
            const auto& b = pts[i + 1];

            // Step curves get drawn as two segments: horizontal hold + vertical jump.
            if (a.curve == AutomationPoint::CurveType::Step)
            {
                path.lineTo(timeToX(b.timeInSeconds), valueToY(a.value));
                path.lineTo(timeToX(b.timeInSeconds), valueToY(b.value));
                continue;
            }

            // For other curves, sample the same interpolator the audio
            // thread uses so what you see is what you hear.
            for (int s = 1; s <= kCurveSegments; ++s)
            {
                const double frac = static_cast<double>(s) / kCurveSegments;
                const double tSec = a.timeInSeconds + (b.timeInSeconds - a.timeInSeconds) * frac;
                const float v = m_lane->curve.getValueAt(tSec);
                path.lineTo(timeToX(tSec), valueToY(v));
            }
        }

        // Constant suffix from last point to right edge.
        path.lineTo(bounds.getRight() - kCurveRightPad,
                    valueToY(pts.back().value));

        g.setColour(themeCurveColour());
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }

    // Points
    for (int i = 0; i < static_cast<int>(pts.size()); ++i)
    {
        const auto& pt = pts[static_cast<size_t>(i)];
        const float x = timeToX(pt.timeInSeconds);
        const float y = valueToY(pt.value);

        juce::Colour fill = themePointColour();
        const bool isSelected = m_selectedIndices.count(i) != 0;
        if (i == m_dragIndex)        fill = themePointDrag();
        else if (isSelected)         fill = themePointSelected();
        else if (i == m_hoverIndex)  fill = themePointHover();

        g.setColour(fill);
        g.fillEllipse(x - kPointRadius, y - kPointRadius,
                      kPointRadius * 2.0f, kPointRadius * 2.0f);
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawEllipse(x - kPointRadius, y - kPointRadius,
                      kPointRadius * 2.0f, kPointRadius * 2.0f, 0.8f);
    }

    // Playhead
    if (m_engine != nullptr && m_engine->isPlaying())
    {
        const float px = timeToX(m_engine->getCurrentPosition());
        if (px >= bounds.getX() && px <= bounds.getRight())
        {
            g.setColour(themePlayheadColour());
            g.drawVerticalLine(static_cast<int>(px), bounds.getY(), bounds.getBottom());
        }
    }

    // Decay highlight
    if (m_highlight && --m_highlightFramesRemaining <= 0)
    {
        m_highlight = false;
    }

    // Marquee rectangle (empty-space drag area select).
    if (m_marqueeActive)
    {
        const auto rect = juce::Rectangle<int>(m_marqueeStart, m_marqueeCurrent).toFloat();
        g.setColour(themeAccent().withAlpha(0.18f));
        g.fillRect(rect);
        g.setColour(themeAccent().withAlpha(0.85f));
        g.drawRect(rect, 1.0f);
    }
}

void AutomationLanesPanel::AutomationCurveView::mouseDown(const juce::MouseEvent& e)
{
    if (m_lane == nullptr)
        return;

    grabKeyboardFocus();  // so Delete / Cmd+A / Cmd+C / Cmd+V hit our keyPressed.

    const int hit = findPointAt(static_cast<float>(e.x), static_cast<float>(e.y));

    if (e.mods.isPopupMenu())
    {
        showContextMenu(hit, e.getScreenPosition());
        return;
    }

    // Cmd-click: toggle selection on a point. No drag, no add.
    if (e.mods.isCommandDown() && hit >= 0)
    {
        if (m_selectedIndices.count(hit) != 0)
            m_selectedIndices.erase(hit);
        else
            m_selectedIndices.insert(hit);
        repaint();
        return;
    }

    if (hit >= 0)
    {
        // Single-point click on an existing point. If the clicked point
        // is in the multi-selection, keep the selection and prepare a
        // multi-drag. Otherwise replace the selection with just this
        // point and prepare a single-point drag.
        if (m_selectedIndices.count(hit) == 0)
        {
            m_selectedIndices.clear();
            m_selectedIndices.insert(hit);
        }

        m_dragIndex = hit;
        m_dragBeforeSnapshot = snapshotPoints();
        m_dragMoved = false;

        // Cache start positions for every selected point so multi-drag
        // applies the same delta to all of them.
        m_multiDragStartPositions.clear();
        const auto pts = m_lane->curve.getPoints();
        for (int idx : m_selectedIndices)
        {
            if (idx >= 0 && idx < static_cast<int>(pts.size()))
            {
                DragSnapshot s;
                s.pointIndex = idx;
                s.startTime  = pts[static_cast<size_t>(idx)].timeInSeconds;
                s.startValue = pts[static_cast<size_t>(idx)].value;
                m_multiDragStartPositions.push_back(s);
            }
        }

        repaint();
        return;
    }

    // Click on empty area: don't commit anything yet. mouseDrag will
    // promote this to a marquee gesture if the user drags past the
    // threshold; mouseUp without a drag falls through to the legacy
    // "add a point at the click position" behavior.
    m_marqueePending  = true;
    m_marqueeActive   = false;
    m_marqueeAdditive = e.mods.isShiftDown();
    m_marqueeStart    = e.getPosition();
    m_marqueeCurrent  = m_marqueeStart;
    m_marqueeBaselineSelection = m_marqueeAdditive ? m_selectedIndices : std::set<int>{};
    repaint();
}

void AutomationLanesPanel::AutomationCurveView::mouseDrag(const juce::MouseEvent& e)
{
    if (m_lane == nullptr)
        return;

    // Marquee: empty-space drag.
    if (m_marqueePending)
    {
        const int dx = e.getPosition().getX() - m_marqueeStart.getX();
        const int dy = e.getPosition().getY() - m_marqueeStart.getY();
        if (! m_marqueeActive
            && (dx * dx + dy * dy) >= (kMarqueeMinDragPx * kMarqueeMinDragPx))
        {
            m_marqueeActive = true;
        }
        if (m_marqueeActive)
        {
            m_marqueeCurrent = e.getPosition();

            // Recompute selection to include every point inside the rect.
            const auto rect = juce::Rectangle<int>(m_marqueeStart, m_marqueeCurrent);
            std::set<int> hits = m_marqueeBaselineSelection;
            const auto pts = m_lane->curve.getPoints();
            for (int i = 0; i < static_cast<int>(pts.size()); ++i)
            {
                const float px = timeToX(pts[static_cast<size_t>(i)].timeInSeconds);
                const float py = valueToY(pts[static_cast<size_t>(i)].value);
                if (rect.contains(juce::Point<int>(static_cast<int>(px),
                                                    static_cast<int>(py))))
                {
                    hits.insert(i);
                }
            }
            m_selectedIndices = std::move(hits);
            repaint();
        }
        return;
    }

    if (m_dragIndex < 0)
        return;

    const double newTime = xToTime(static_cast<float>(e.x));
    const float  newVal  = yToValue(static_cast<float>(e.y));

    // For multi-drag, compute the dragged point's delta from its
    // mouseDown-time snapshot, then shift every selected point by the
    // same delta. Single-drag is the size-1 special case, which falls
    // through this loop.
    if (m_multiDragStartPositions.size() <= 1)
    {
        // Single-drag: classic behavior.
        if (m_lane->curve.movePoint(m_dragIndex, newTime, newVal))
        {
            m_dragMoved = true;
            const auto pts = m_lane->curve.getPoints();
            for (int i = 0; i < static_cast<int>(pts.size()); ++i)
            {
                const auto& p = pts[static_cast<size_t>(i)];
                if (std::abs(p.timeInSeconds - newTime) < 1.0e-9
                    && std::abs(p.value - juce::jlimit(0.0f, 1.0f, newVal)) < 1.0e-6f)
                {
                    m_dragIndex = i;
                    if (! m_selectedIndices.empty())
                    {
                        m_selectedIndices.clear();
                        m_selectedIndices.insert(i);
                    }
                    break;
                }
            }
            repaint();
        }
        return;
    }

    // Multi-drag: compute delta from dragged-point's start.
    DragSnapshot anchor;
    anchor.pointIndex = -1;
    for (const auto& s : m_multiDragStartPositions)
    {
        if (s.pointIndex == m_dragIndex)
        {
            anchor = s;
            break;
        }
    }
    if (anchor.pointIndex < 0)
        return;

    const double deltaTime  = newTime - anchor.startTime;
    const float  deltaValue = newVal  - anchor.startValue;
    if (std::abs(deltaTime) < 1.0e-9 && std::abs(deltaValue) < 1.0e-9f)
        return;

    // Apply by replacing the curve's point list with the shifted set
    // for selected points, untouched for unselected points. Build the
    // new list, sort, publish.
    auto pts = m_lane->curve.getPoints();
    std::set<int> newSelection;
    std::vector<AutomationPoint> rebuilt;
    rebuilt.reserve(pts.size());
    for (int i = 0; i < static_cast<int>(pts.size()); ++i)
    {
        AutomationPoint p = pts[static_cast<size_t>(i)];
        if (m_selectedIndices.count(i) != 0)
        {
            // Find this point's start position from the snapshot.
            for (const auto& s : m_multiDragStartPositions)
            {
                if (s.pointIndex == i)
                {
                    p.timeInSeconds = s.startTime  + deltaTime;
                    p.value         = juce::jlimit(0.0f, 1.0f, s.startValue + deltaValue);
                    break;
                }
            }
        }
        rebuilt.push_back(p);
    }
    std::sort(rebuilt.begin(), rebuilt.end());

    // Re-resolve selection indices after the sort.
    for (int i = 0; i < static_cast<int>(rebuilt.size()); ++i)
    {
        const auto& r = rebuilt[static_cast<size_t>(i)];
        for (const auto& s : m_multiDragStartPositions)
        {
            const double expectedT = s.startTime + deltaTime;
            const float  expectedV = juce::jlimit(0.0f, 1.0f, s.startValue + deltaValue);
            if (std::abs(r.timeInSeconds - expectedT) < 1.0e-9
                && std::abs(r.value - expectedV) < 1.0e-6f)
            {
                newSelection.insert(i);
                if (s.pointIndex == m_dragIndex)
                    m_dragIndex = i;
                break;
            }
        }
    }

    // Apply via clear + add (atomic publish per point; semantically
    // equivalent to a bulk replace for the curve's COW model).
    m_lane->curve.clear();
    for (const auto& p : rebuilt)
        m_lane->curve.addPoint(p);

    m_selectedIndices = std::move(newSelection);
    m_dragMoved = true;
    repaint();
}

void AutomationLanesPanel::AutomationCurveView::mouseUp(const juce::MouseEvent&)
{
    // Marquee gesture wraps up here.
    if (m_marqueePending)
    {
        if (! m_marqueeActive && m_lane != nullptr)
        {
            // No drag → legacy "add a point at click position" behavior.
            auto beforeAdd = snapshotPoints();

            AutomationPoint pt;
            pt.timeInSeconds = xToTime(static_cast<float>(m_marqueeStart.getX()));
            pt.value         = yToValue(static_cast<float>(m_marqueeStart.getY()));
            pt.curve         = AutomationPoint::CurveType::Linear;
            m_lane->curve.addPoint(pt);
            commitGesture("Add automation point", std::move(beforeAdd));

            // Select just the newly-added point so the user sees it.
            const auto pts = m_lane->curve.getPoints();
            m_selectedIndices.clear();
            for (int i = 0; i < static_cast<int>(pts.size()); ++i)
            {
                const auto& q = pts[static_cast<size_t>(i)];
                if (std::abs(q.timeInSeconds - pt.timeInSeconds) < 1.0e-6
                    && std::abs(q.value - pt.value) < 1.0e-6f)
                {
                    m_selectedIndices.insert(i);
                    break;
                }
            }
        }
        m_marqueePending  = false;
        m_marqueeActive   = false;
        m_marqueeAdditive = false;
        m_marqueeBaselineSelection.clear();
        repaint();
        return;
    }

    if (m_dragIndex >= 0)
    {
        // Whole-drag = one undo step. commitGesture is itself a no-op
        // when before == after (e.g. dragging a point and dropping it
        // back where it started).
        if (m_dragMoved)
        {
            const auto txnName = (m_multiDragStartPositions.size() > 1)
                                    ? juce::String("Move automation points")
                                    : juce::String("Move automation point");
            commitGesture(txnName, std::move(m_dragBeforeSnapshot));
        }
        m_dragBeforeSnapshot.clear();
        m_dragIndex = -1;
        m_dragMoved = false;
        m_multiDragStartPositions.clear();
        repaint();
    }
}

bool AutomationLanesPanel::AutomationCurveView::keyPressed(const juce::KeyPress& key)
{
    if (m_lane == nullptr)
        return false;

    if (key == juce::KeyPress('a', juce::ModifierKeys::commandModifier, 0))
    {
        testSelectAll();
        return true;
    }

    if (key == juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0))
    {
        testCopySelectionToClipboard();
        return true;
    }

    if (key == juce::KeyPress('v', juce::ModifierKeys::commandModifier, 0))
    {
        const double anchor = (m_engine != nullptr && m_engine->isPlaying())
                                ? m_engine->getCurrentPosition()
                                : 0.0;
        testPasteFromClipboardAt(anchor);
        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (! m_selectedIndices.empty())
        {
            testDeleteSelected();
            return true;
        }
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (! m_selectedIndices.empty())
        {
            m_selectedIndices.clear();
            repaint();
            return true;
        }
    }

    return false;
}

void AutomationLanesPanel::AutomationCurveView::testSelectAll()
{
    if (m_lane == nullptr) return;
    m_selectedIndices.clear();
    const int n = m_lane->curve.getNumPoints();
    for (int i = 0; i < n; ++i)
        m_selectedIndices.insert(i);
    repaint();
}

void AutomationLanesPanel::AutomationCurveView::testToggleSelectPoint(int pointIndex)
{
    if (m_lane == nullptr || pointIndex < 0 || pointIndex >= m_lane->curve.getNumPoints())
        return;
    if (m_selectedIndices.count(pointIndex) != 0)
        m_selectedIndices.erase(pointIndex);
    else
        m_selectedIndices.insert(pointIndex);
    repaint();
}

void AutomationLanesPanel::AutomationCurveView::testClearSelection()
{
    if (m_selectedIndices.empty()) return;
    m_selectedIndices.clear();
    repaint();
}

void AutomationLanesPanel::AutomationCurveView::testCopySelectionToClipboard()
{
    if (m_lane == nullptr || m_selectedIndices.empty())
        return;
    const auto pts = m_lane->curve.getPoints();
    std::vector<AutomationPoint> selected;
    selected.reserve(m_selectedIndices.size());
    for (int idx : m_selectedIndices)
        if (idx >= 0 && idx < static_cast<int>(pts.size()))
            selected.push_back(pts[static_cast<size_t>(idx)]);
    AutomationClipboard::getInstance().copy(selected);
}

void AutomationLanesPanel::AutomationCurveView::testPasteFromClipboardAt(double anchorTime)
{
    if (m_lane == nullptr) return;
    auto& clip = AutomationClipboard::getInstance();
    if (clip.isEmpty()) return;

    auto before = snapshotPoints();
    const auto incoming = clip.pasteAt(anchorTime);
    for (const auto& p : incoming)
        m_lane->curve.addPoint(p);

    // Reselect the freshly-pasted points so user can immediately
    // move/copy/delete them.
    const auto pts = m_lane->curve.getPoints();
    std::set<int> newSelection;
    for (const auto& p : incoming)
    {
        for (int i = 0; i < static_cast<int>(pts.size()); ++i)
        {
            const auto& q = pts[static_cast<size_t>(i)];
            if (std::abs(q.timeInSeconds - p.timeInSeconds) < 1.0e-9
                && std::abs(q.value - p.value) < 1.0e-6f)
            {
                newSelection.insert(i);
                break;
            }
        }
    }
    m_selectedIndices = std::move(newSelection);

    commitGesture("Paste automation points", std::move(before));
    repaint();
}

void AutomationLanesPanel::AutomationCurveView::testMarqueeSelect(juce::Rectangle<int> rect,
                                                                    bool additive)
{
    if (m_lane == nullptr) return;

    std::set<int> hits = additive ? m_selectedIndices : std::set<int>{};
    const auto pts = m_lane->curve.getPoints();
    for (int i = 0; i < static_cast<int>(pts.size()); ++i)
    {
        const float px = timeToX(pts[static_cast<size_t>(i)].timeInSeconds);
        const float py = valueToY(pts[static_cast<size_t>(i)].value);
        if (rect.contains(juce::Point<int>(static_cast<int>(px),
                                            static_cast<int>(py))))
        {
            hits.insert(i);
        }
    }
    m_selectedIndices = std::move(hits);
    repaint();
}

void AutomationLanesPanel::AutomationCurveView::testDeleteSelected()
{
    if (m_lane == nullptr || m_selectedIndices.empty())
        return;

    auto before = snapshotPoints();

    // Resolve indices to time keys before mutating, since indices shift
    // as we remove points.
    const auto pts = m_lane->curve.getPoints();
    std::vector<double> timesToRemove;
    timesToRemove.reserve(m_selectedIndices.size());
    for (int idx : m_selectedIndices)
        if (idx >= 0 && idx < static_cast<int>(pts.size()))
            timesToRemove.push_back(pts[static_cast<size_t>(idx)].timeInSeconds);

    for (double t : timesToRemove)
        m_lane->curve.removePointAt(t, 0.0001);

    m_selectedIndices.clear();
    commitGesture("Delete automation points", std::move(before));
    repaint();
}

void AutomationLanesPanel::AutomationCurveView::mouseMove(const juce::MouseEvent& e)
{
    const int newHover = findPointAt(static_cast<float>(e.x), static_cast<float>(e.y));
    if (newHover != m_hoverIndex)
    {
        m_hoverIndex = newHover;
        if (m_lane != nullptr && m_hoverIndex >= 0)
        {
            const auto pts = m_lane->curve.getPoints();
            if (m_hoverIndex < static_cast<int>(pts.size()))
            {
                const auto& p = pts[static_cast<size_t>(m_hoverIndex)];
                setTooltip(formatTime(p.timeInSeconds)
                           + "  |  "
                           + juce::String(static_cast<int>(std::round(p.value * 100.0f)))
                           + "%  |  "
                           + curveTypeLabel(p.curve));
            }
        }
        else
        {
            setTooltip({});
        }
        repaint();
    }
}

void AutomationLanesPanel::AutomationCurveView::mouseExit(const juce::MouseEvent&)
{
    if (m_hoverIndex >= 0)
    {
        m_hoverIndex = -1;
        setTooltip({});
        repaint();
    }
}

void AutomationLanesPanel::AutomationCurveView::showContextMenu(int pointIndex,
                                                                 juce::Point<int> /*screenPos*/)
{
    if (m_lane == nullptr)
        return;

    juce::PopupMenu menu;

    if (pointIndex >= 0)
    {
        menu.addSectionHeader("Point");
        menu.addItem(1, "Delete Point");

        juce::PopupMenu curveMenu;
        const auto pts = m_lane->curve.getPoints();
        const auto current = (pointIndex < static_cast<int>(pts.size()))
                                ? pts[static_cast<size_t>(pointIndex)].curve
                                : AutomationPoint::CurveType::Linear;

        auto add = [&](int id, AutomationPoint::CurveType type)
        {
            curveMenu.addItem(id, curveTypeLabel(type), true, current == type);
        };
        add(10, AutomationPoint::CurveType::Linear);
        add(11, AutomationPoint::CurveType::Step);
        add(12, AutomationPoint::CurveType::SCurve);
        add(13, AutomationPoint::CurveType::Exponential);
        menu.addSubMenu("Curve to Next Point", curveMenu);
    }
    else
    {
        menu.addSectionHeader("Lane");
        menu.addItem(20, "Clear All Points",
                     m_lane->curve.hasPoints());
    }

    juce::Component::SafePointer<AutomationCurveView> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options(),
        [safeThis, pointIndex](int result)
        {
            if (safeThis == nullptr || safeThis->m_lane == nullptr)
                return;
            if (result == 0)
                return;

            auto before = safeThis->snapshotPoints();
            juce::String txnName;

            switch (result)
            {
                case 1:
                    {
                        // Resolve point index back to the point's time
                        // before deleting — the index may have shifted
                        // since the menu opened.
                        const auto pts = safeThis->m_lane->curve.getPoints();
                        if (pointIndex >= 0 && pointIndex < static_cast<int>(pts.size()))
                            safeThis->m_lane->curve.removePointAt(
                                pts[static_cast<size_t>(pointIndex)].timeInSeconds, 0.0001);
                        txnName = "Delete automation point";
                    }
                    break;
                case 10: safeThis->m_lane->curve.setPointCurve(pointIndex, AutomationPoint::CurveType::Linear);      txnName = "Set curve type: Linear"; break;
                case 11: safeThis->m_lane->curve.setPointCurve(pointIndex, AutomationPoint::CurveType::Step);        txnName = "Set curve type: Step"; break;
                case 12: safeThis->m_lane->curve.setPointCurve(pointIndex, AutomationPoint::CurveType::SCurve);      txnName = "Set curve type: S-Curve"; break;
                case 13: safeThis->m_lane->curve.setPointCurve(pointIndex, AutomationPoint::CurveType::Exponential); txnName = "Set curve type: Exponential"; break;
                case 20: safeThis->m_lane->curve.clear(); txnName = "Clear automation lane"; break;
                default: return;
            }

            safeThis->commitGesture(txnName, std::move(before));
            safeThis->repaint();
        });
}

//==============================================================================
// AutomationLaneRow
//==============================================================================

AutomationLanesPanel::AutomationLaneRow::AutomationLaneRow(AutomationLanesPanel& owner, int laneIndex)
    : m_owner(owner), m_laneIndex(laneIndex)
{
    m_titleLabel.setFont(juce::FontOptions(13.5f).withStyle("Bold"));
    m_titleLabel.setColour(juce::Label::textColourId, themeTextColour());
    m_titleLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(m_titleLabel);

    m_subtitleLabel.setFont(juce::FontOptions(11.0f));
    m_subtitleLabel.setColour(juce::Label::textColourId, themeSubtitleColour());
    m_subtitleLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(m_subtitleLabel);

    m_enableButton.setButtonText("Enabled");
    m_enableButton.setTooltip("Apply this automation during playback");
    m_enableButton.setColour(juce::ToggleButton::tickColourId, themeAccent());
    m_enableButton.onClick = [this]()
    {
        if (auto* lane = m_owner.getLaneForRow(m_laneIndex))
            lane->enabled = m_enableButton.getToggleState();
    };
    addAndMakeVisible(m_enableButton);

    m_recordButton.setButtonText("Record");
    m_recordButton.setTooltip("Capture knob movements into this lane while transport is playing "
                              "(global automation arm must also be on).");
    m_recordButton.setColour(juce::ToggleButton::tickColourId, themeRecordOnColour());
    m_recordButton.onClick = [this]()
    {
        if (auto* lane = m_owner.getLaneForRow(m_laneIndex))
            lane->isRecording = m_recordButton.getToggleState();
    };
    addAndMakeVisible(m_recordButton);

    m_deleteButton.setButtonText("Remove");
    m_deleteButton.setTooltip("Remove this automation lane");
    m_deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff404040));
    m_deleteButton.onClick = [this]()
    {
        if (m_owner.m_manager != nullptr)
            m_owner.m_manager->removeLane(m_laneIndex);
    };
    addAndMakeVisible(m_deleteButton);

    addAndMakeVisible(m_curveView);
}

void AutomationLanesPanel::AutomationLaneRow::refresh()
{
    auto* lane = m_owner.getLaneForRow(m_laneIndex);
    if (lane == nullptr)
    {
        m_curveView.setLane(nullptr);
        return;
    }

    m_titleLabel.setText(lane->parameterName.isNotEmpty() ? lane->parameterName
                                                          : juce::String("Parameter ")
                                                              + juce::String(lane->parameterIndex),
                         juce::dontSendNotification);

    juce::String subtitle = lane->pluginName;
    if (subtitle.isEmpty())
        subtitle = "Plugin " + juce::String(lane->pluginIndex);
    subtitle += "  •  ";
    subtitle += juce::String(lane->curve.getNumPoints()) + " point"
                + (lane->curve.getNumPoints() == 1 ? "" : "s");
    m_subtitleLabel.setText(subtitle, juce::dontSendNotification);

    m_enableButton.setToggleState(lane->enabled, juce::dontSendNotification);
    m_recordButton.setToggleState(lane->isRecording, juce::dontSendNotification);

    m_curveView.setLane(lane);
    m_curveView.setEngine(m_owner.m_engine);
    m_curveView.setUndoContext(m_owner.m_undoManager, m_owner.m_manager);
}

bool AutomationLanesPanel::AutomationLaneRow::matches(int pluginIndex, int parameterIndex) const
{
    if (auto* lane = m_owner.getLaneForRow(m_laneIndex))
        return lane->matches(pluginIndex, parameterIndex);
    return false;
}

void AutomationLanesPanel::AutomationLaneRow::paint(juce::Graphics& g)
{
    g.fillAll((m_laneIndex % 2 == 0) ? themeRowBackground() : themeRowAlt());
    g.setColour(themeBorder());
    g.drawHorizontalLine(getHeight() - 1, 0.0f, static_cast<float>(getWidth()));
}

void AutomationLanesPanel::AutomationLaneRow::resized()
{
    auto bounds = getLocalBounds().reduced(6, 4);

    auto header = bounds.removeFromTop(kHeaderHeight);

    auto labels = header.removeFromLeft(juce::jmin(280, header.getWidth() - 240));
    m_titleLabel.setBounds(labels.removeFromTop(18));
    m_subtitleLabel.setBounds(labels);

    // Buttons on the right side of the header strip.
    auto controls = header;
    m_deleteButton.setBounds(controls.removeFromRight(70).reduced(2));
    m_recordButton.setBounds(controls.removeFromRight(80).reduced(2));
    m_enableButton.setBounds(controls.removeFromRight(80).reduced(2));

    bounds.removeFromTop(2);
    m_curveView.setBounds(bounds);
}

//==============================================================================
// AutomationLanesPanel
//==============================================================================

AutomationLanesPanel::AutomationLanesPanel(AutomationManager* manager,
                                            AudioEngine* engine,
                                            juce::UndoManager* undoManager)
    : m_manager(manager), m_engine(engine), m_undoManager(undoManager)
{
    m_titleLabel.setText("Automation Lanes", juce::dontSendNotification);
    m_titleLabel.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    m_titleLabel.setColour(juce::Label::textColourId, themeTextColour());
    addAndMakeVisible(m_titleLabel);

    m_helpLabel.setText("Click empty space = add. Drag empty = marquee (Shift+drag adds). "
                        "Cmd-click multi-selects; Cmd+A all; Cmd+C/V copy/paste; Delete removes; Esc clears.",
                        juce::dontSendNotification);
    m_helpLabel.setFont(juce::FontOptions(11.0f));
    m_helpLabel.setColour(juce::Label::textColourId, themeSubtitleColour());
    m_helpLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_helpLabel);

    m_emptyLabel.setText("No automation lanes for this file yet.\n\n"
                         "Open a plugin editor, right-click a parameter, and choose\n"
                         "\"Record this parameter\" to create a lane.",
                         juce::dontSendNotification);
    m_emptyLabel.setJustificationType(juce::Justification::centred);
    m_emptyLabel.setColour(juce::Label::textColourId, themeEmptyColour());
    addAndMakeVisible(m_emptyLabel);

    m_viewport.setViewedComponent(&m_lanesContainer, false);
    m_viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(m_viewport);

    if (m_manager != nullptr)
        m_manager->addListener(this);

    waveedit::ThemeManager::getInstance().addChangeListener(this);

    rebuildRows();
    setSize(kPanelWidth, kPanelHeight);
    startTimerHz(30);
}

AutomationLanesPanel::~AutomationLanesPanel()
{
    stopTimer();
    waveedit::ThemeManager::getInstance().removeChangeListener(this);
    if (m_manager != nullptr)
        m_manager->removeListener(this);
}

void AutomationLanesPanel::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &waveedit::ThemeManager::getInstance())
    {
        // Theme switched: repaint everything.
        for (auto* row : m_rows)
            if (row != nullptr) row->repaint();
        repaint();
    }
}

juce::DocumentWindow* AutomationLanesPanel::showInWindow(juce::ApplicationCommandManager* commandManager)
{
    // DocumentWindow that forwards keyboard shortcuts back to the
    // application-wide command manager (mirrors PluginChainWindow's
    // PluginChainDocumentWindow).
    class LanesDocumentWindow : public juce::DocumentWindow,
                                public juce::ApplicationCommandTarget
    {
    public:
        LanesDocumentWindow(juce::ApplicationCommandManager* cmdManager)
            : DocumentWindow("Automation Lanes",
                              themeBackground(),
                              juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
            , m_commandManager(cmdManager)
        {
            if (m_commandManager != nullptr)
            {
                addKeyListener(m_commandManager->getKeyMappings());
                m_mainTarget = m_commandManager->getFirstCommandTarget(0);
            }
        }

        ~LanesDocumentWindow() override
        {
            if (m_commandManager != nullptr)
                removeKeyListener(m_commandManager->getKeyMappings());
        }

        void closeButtonPressed() override { delete this; }

        juce::ApplicationCommandTarget* getNextCommandTarget() override { return m_mainTarget; }
        void getAllCommands(juce::Array<juce::CommandID>&) override {}
        void getCommandInfo(juce::CommandID, juce::ApplicationCommandInfo&) override {}
        bool perform(const InvocationInfo&) override { return false; }

    private:
        juce::ApplicationCommandManager* m_commandManager = nullptr;
        juce::ApplicationCommandTarget*  m_mainTarget     = nullptr;
    };

    auto* window = new LanesDocumentWindow(commandManager);
    window->setUsingNativeTitleBar(true);
    window->setContentOwned(this, true);
    window->centreWithSize(getWidth(), getHeight());
    window->setVisible(true);
    window->setResizable(true, true);
    window->setResizeLimits(560, 320, 1600, 1200);
    return window;
}

void AutomationLanesPanel::scrollToLane(int pluginIndex, int parameterIndex)
{
    for (int i = 0; i < m_rows.size(); ++i)
    {
        if (auto* row = m_rows[i]; row != nullptr && row->matches(pluginIndex, parameterIndex))
        {
            const auto rowBounds = row->getBounds();
            m_viewport.setViewPosition(0,
                juce::jmax(0, rowBounds.getY() - 8));
            row->getCurveView().flashHighlight();
            return;
        }
    }
}

//==============================================================================
// Component overrides
//==============================================================================

void AutomationLanesPanel::paint(juce::Graphics& g)
{
    g.fillAll(themeBackground());
}

void AutomationLanesPanel::resized()
{
    auto bounds = getLocalBounds().reduced(8, 6);

    m_titleLabel.setBounds(bounds.removeFromTop(kHeaderHeight));
    m_helpLabel.setBounds(bounds.removeFromBottom(kFooterHelpHeight));

    m_emptyLabel.setBounds(bounds);
    m_viewport.setBounds(bounds);

    layoutRows();
}

//==============================================================================
// Timer
//==============================================================================

void AutomationLanesPanel::timerCallback()
{
    // Repaint curve views when transport is moving so the playhead
    // line follows. When stopped, idle quietly.
    if (m_engine == nullptr || ! m_engine->isPlaying())
        return;

    for (auto* row : m_rows)
        if (row != nullptr)
            row->getCurveView().repaint();
}

//==============================================================================
// AutomationManager::Listener
//==============================================================================

void AutomationLanesPanel::automationDataChanged()
{
    rebuildRows();
}

//==============================================================================
// Internals
//==============================================================================

void AutomationLanesPanel::rebuildRows()
{
    const int numLanes = (m_manager != nullptr) ? m_manager->getNumLanes() : 0;

    // Resize the row pool.
    while (m_rows.size() > numLanes)
    {
        auto* row = m_rows.getLast();
        m_lanesContainer.removeChildComponent(row);
        m_rows.removeLast();
    }
    while (m_rows.size() < numLanes)
    {
        auto* row = m_rows.add(new AutomationLaneRow(*this, m_rows.size()));
        m_lanesContainer.addAndMakeVisible(row);
    }

    for (auto* row : m_rows)
        if (row != nullptr)
            row->refresh();

    updateEmptyState();
    layoutRows();
}

void AutomationLanesPanel::updateEmptyState()
{
    const bool isEmpty = m_rows.isEmpty();
    m_emptyLabel.setVisible(isEmpty);
    m_viewport.setVisible(! isEmpty);
}

void AutomationLanesPanel::layoutRows()
{
    auto vpBounds = m_viewport.getBounds();
    const int viewWidth = juce::jmax(200, vpBounds.getWidth() - 16); // leave room for scrollbar
    const int rowHeight = kRowHeight;

    int y = 0;
    for (auto* row : m_rows)
    {
        if (row == nullptr) continue;
        row->setBounds(0, y, viewWidth, rowHeight);
        y += rowHeight + kRowSpacing;
    }
    m_lanesContainer.setSize(viewWidth, juce::jmax(0, y));
}

AutomationLane* AutomationLanesPanel::getLaneForRow(int rowIndex) const
{
    if (m_manager == nullptr) return nullptr;
    return m_manager->getLane(rowIndex);
}

AutomationLanesPanel::AutomationCurveView* AutomationLanesPanel::testGetCurveView(int rowIndex) const
{
    if (rowIndex < 0 || rowIndex >= m_rows.size()) return nullptr;
    auto* row = m_rows[rowIndex];
    return row != nullptr ? &row->getCurveView() : nullptr;
}
