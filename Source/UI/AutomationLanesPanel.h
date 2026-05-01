/*
  ==============================================================================

    AutomationLanesPanel.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Phase 5 of the Plugin Parameter Automation feature: visual lane
    editor. Lists every AutomationLane in the bound document and renders
    its AutomationCurve on a horizontal timeline. Click-to-add,
    drag-to-move, right-click-to-edit point editing.

    The panel binds to one Document at open time. Document switching
    requires reopening the window (matches PluginChainWindow's pattern).

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <set>

#include "../Automation/AutomationData.h"
#include "../Automation/AutomationManager.h"

class AudioEngine;

/**
 * Panel that visualizes and edits AutomationManager lanes.
 *
 * Layout: vertical scrollable list of lanes. Each lane row shows the
 * plugin/parameter labels, enable/record toggles, a delete button, and
 * a horizontal AutomationCurveView spanning the document timeline.
 */
class AutomationLanesPanel : public juce::Component,
                             public juce::Timer,
                             public AutomationManager::Listener,
                             public juce::ChangeListener
{
public:
    /**
     * Construct the panel. Any pointer may be null (empty state).
     *
     * @param manager      The document's AutomationManager. Owned by Document.
     * @param engine       The document's AudioEngine for transport queries.
     * @param undoManager  Optional. When non-null, point edits are
     *                     registered as undoable actions on this manager.
     */
    AutomationLanesPanel(AutomationManager* manager,
                         AudioEngine* engine,
                         juce::UndoManager* undoManager = nullptr);
    ~AutomationLanesPanel() override;


    /**
     * Wrap this panel in a DocumentWindow that routes shortcuts back to
     * the application command manager. Caller takes ownership of the
     * returned window.
     */
    juce::DocumentWindow* showInWindow(juce::ApplicationCommandManager* commandManager);

    /**
     * Scroll the lane for (pluginIndex, parameterIndex) into view and
     * highlight it briefly. No-op if no matching lane exists.
     */
    void scrollToLane(int pluginIndex, int parameterIndex);

    /** Number of currently-rendered lane rows (test hook). */
    int getNumLaneRows() const { return m_rows.size(); }

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // Timer override (refreshes playhead position during playback)
    void timerCallback() override;

    //==============================================================================
    // AutomationManager::Listener
    void automationDataChanged() override;

    //==============================================================================
    // juce::ChangeListener — repaints on theme switch
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    //==============================================================================
    // Inner components — public so tests can drive selection/clipboard
    // through their state-side hooks without needing a friend
    // declaration. Production code never needs to instantiate them
    // directly; the panel owns and manages them.

    /**
     * The horizontal curve view inside one lane row. Renders points,
     * connecting curve segments, and a playhead. Handles mouse input
     * for click-add / drag-move / right-click.
     */
    class AutomationCurveView : public juce::Component,
                                public juce::SettableTooltipClient
    {
    public:
        AutomationCurveView();

        void setLane(AutomationLane* lane);
        void setEngine(AudioEngine* engine);
        void setUndoContext(juce::UndoManager* undoManager,
                            AutomationManager* manager);

        /** Trigger a brief highlight (used by scrollToLane). */
        void flashHighlight();

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;
        bool keyPressed(const juce::KeyPress& key) override;

        //==============================================================================
        // Selection / clipboard test hooks (state-side only — no UI dispatch).
        //
        // Tests can drive selection / Cmd+A / Cmd+C / Cmd+V / Delete via
        // these helpers without going through mouse + keyboard events.

        void testSelectAll();
        void testToggleSelectPoint(int pointIndex);
        void testClearSelection();
        const std::set<int>& testGetSelectedIndices() const { return m_selectedIndices; }
        void testCopySelectionToClipboard();
        void testPasteFromClipboardAt(double anchorTime);
        void testDeleteSelected();
        /** Simulate a marquee select with the given pixel rectangle.
            @p additive=true mirrors a Shift-drag. Replaces (or extends)
            the current selection with the points inside the rect. */
        void testMarqueeSelect(juce::Rectangle<int> pixelRect, bool additive);

    private:
        struct PixelPoint
        {
            int index = -1;
            float x = 0.0f;
            float y = 0.0f;
        };

        double getDuration() const;
        double xToTime(float x) const;
        float timeToX(double t) const;
        float valueToY(float v) const;
        float yToValue(float y) const;

        /** Returns the index of the point under (x,y), or -1 if none. */
        int findPointAt(float x, float y) const;

        void showContextMenu(int pointIndex, juce::Point<int> screenPos);

        /** Snapshot the current curve points (for undo before-state). */
        std::vector<AutomationPoint> snapshotPoints() const;

        /** Register the gesture as an undoable action on the wired UndoManager.
            No-op if the UndoManager / AutomationManager isn't set or the lane
            has been removed. */
        void commitGesture(const juce::String& transactionName,
                           std::vector<AutomationPoint> beforeSnapshot);

        AutomationLane* m_lane = nullptr;
        AudioEngine* m_engine = nullptr;
        juce::UndoManager* m_undoManager = nullptr;
        AutomationManager* m_undoLookupManager = nullptr;

        int m_dragIndex = -1;
        int m_hoverIndex = -1;
        bool m_highlight = false;
        int m_highlightFramesRemaining = 0;

        // Drag bookkeeping: snapshot taken at mouseDown; transaction
        // committed at mouseUp if any movement occurred.
        std::vector<AutomationPoint> m_dragBeforeSnapshot;
        bool m_dragMoved = false;

        // Multi-selection. Indices are valid only between curve mutations
        // — automationDataChanged() in the panel clears the set whenever
        // the curve structure changes (recorder append, sidecar reload,
        // lane removal). Live single-drag preserves selection because
        // movePoint() doesn't trigger the Listener.
        std::set<int> m_selectedIndices;

        // Cached per-point time/value at multi-drag start so we can
        // re-position relative to the dragged point's delta.
        struct DragSnapshot
        {
            int    pointIndex = -1;
            double startTime  = 0.0;
            float  startValue = 0.0f;
        };
        std::vector<DragSnapshot> m_multiDragStartPositions;

        // Marquee (rectangle area-select) state. Marquee starts when
        // the user clicks empty space and drags past kMarqueeMinDragPx.
        // If the click never drags far enough, mouseUp falls through to
        // the legacy add-a-point gesture.
        bool m_marqueePending = false;   // mouseDown on empty fired; awaiting drag-vs-click
        bool m_marqueeActive  = false;   // drag exceeded threshold; rectangle is live
        bool m_marqueeAdditive = false;  // Shift held = add to existing selection
        juce::Point<int> m_marqueeStart;
        juce::Point<int> m_marqueeCurrent;
        std::set<int>    m_marqueeBaselineSelection;  // selection at gesture start (for additive mode)

        static constexpr int kMarqueeMinDragPx = 5;

        static constexpr float kPointRadius = 4.5f;
        static constexpr float kHitRadius = 9.0f;
    };

    /**
     * One lane row: header (labels + toggles + delete) plus the curve
     * view. Owned by m_lanesContainer via OwnedArray.
     */
    class AutomationLaneRow : public juce::Component
    {
    public:
        AutomationLaneRow(AutomationLanesPanel& owner, int laneIndex);

        void refresh();
        AutomationCurveView& getCurveView() { return m_curveView; }
        int getLaneIndex() const { return m_laneIndex; }
        bool matches(int pluginIndex, int parameterIndex) const;

        void paint(juce::Graphics& g) override;
        void resized() override;

    private:
        AutomationLanesPanel& m_owner;
        int m_laneIndex;

        juce::Label m_titleLabel;
        juce::Label m_subtitleLabel;
        juce::ToggleButton m_enableButton;
        juce::ToggleButton m_recordButton;
        juce::TextButton m_deleteButton;
        AutomationCurveView m_curveView;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationLaneRow)
    };

    /** Test hook: access a row's curve view by index (or nullptr if out of range). */
    AutomationCurveView* testGetCurveView(int rowIndex) const;

private:
    //==============================================================================
    // Internal state + helpers

    void rebuildRows();
    void updateEmptyState();
    void layoutRows();
    AutomationLane* getLaneForRow(int rowIndex) const;

    AutomationManager* m_manager;
    AudioEngine* m_engine;
    juce::UndoManager* m_undoManager;

    juce::Label m_titleLabel;
    juce::Label m_emptyLabel;
    juce::Label m_helpLabel;
    juce::Viewport m_viewport;
    juce::Component m_lanesContainer;
    juce::OwnedArray<AutomationLaneRow> m_rows;

    static constexpr int kRowHeight = 84;
    static constexpr int kRowSpacing = 4;
    static constexpr int kHeaderHeight = 36;
    static constexpr int kFooterHelpHeight = 22;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AutomationLanesPanel)
};
