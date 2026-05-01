/*
  ==============================================================================

    AutomationLanesPanelTests.cpp
    WaveEdit - Professional Audio Editor

    Phase 5 of Plugin Parameter Automation: validate the visual lane
    editor's state-side contract — the AutomationCurve helpers it
    depends on, and the panel's listener-driven row management.

    These tests do not dispatch mouse/keyboard events (no MessageManager
    pump in the test runner). Mouse-driven point editing is exercised
    indirectly: every gesture eventually maps to AutomationCurve::add /
    movePoint / removePointAt / setPointCurve, which are directly tested
    here.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Automation/AutomationData.h"
#include "Automation/AutomationManager.h"
#include "UI/AutomationLanesPanel.h"
#include "Utils/AutomationClipboard.h"
#include "Utils/UndoActions/AutomationUndoActions.h"

namespace
{
    // Helper: read a lane's points by lookup, since the panel's
    // gesture handlers always go through findLane.
    std::vector<AutomationPoint> readPoints(AutomationManager& mgr, int p, int q)
    {
        auto* lane = mgr.findLane(p, q);
        return lane != nullptr ? lane->curve.getPoints()
                               : std::vector<AutomationPoint>{};
    }
}

//==============================================================================
class AutomationLanesPanelTests : public juce::UnitTest
{
public:
    AutomationLanesPanelTests()
        : juce::UnitTest("AutomationLanesPanel", "Unit") {}

    void runTest() override
    {
        beginTest("AutomationCurve::setPointCurve flips type without changing index");
        {
            AutomationCurve curve;
            AutomationPoint a; a.timeInSeconds = 0.0; a.value = 0.0f; a.curve = AutomationPoint::CurveType::Linear;
            AutomationPoint b; b.timeInSeconds = 1.0; b.value = 1.0f; b.curve = AutomationPoint::CurveType::Linear;
            curve.addPoint(a);
            curve.addPoint(b);

            expect(curve.setPointCurve(0, AutomationPoint::CurveType::SCurve));
            const auto pts = curve.getPoints();
            expect(pts.size() == 2);
            expect(pts[0].curve == AutomationPoint::CurveType::SCurve);
            expect(pts[1].curve == AutomationPoint::CurveType::Linear);
        }

        beginTest("AutomationCurve::setPointCurve no-ops when curve unchanged");
        {
            AutomationCurve curve;
            AutomationPoint a; a.timeInSeconds = 0.0; a.value = 0.5f; a.curve = AutomationPoint::CurveType::Step;
            curve.addPoint(a);

            expect(! curve.setPointCurve(0, AutomationPoint::CurveType::Step),
                   "setPointCurve returns false when value is identical");
        }

        beginTest("AutomationCurve::setPointCurve out-of-range is a safe no-op");
        {
            AutomationCurve curve;
            expect(! curve.setPointCurve(-1, AutomationPoint::CurveType::Linear));
            expect(! curve.setPointCurve(0,  AutomationPoint::CurveType::Linear));
            expect(! curve.setPointCurve(99, AutomationPoint::CurveType::Linear));
        }

        beginTest("Panel construction with null manager is safe (empty state)");
        {
            auto panel = std::make_unique<AutomationLanesPanel>(nullptr, nullptr);
            expectEquals(panel->getNumLaneRows(), 0);
        }

        beginTest("Panel listener wiring: row count tracks lane count");
        {
            AutomationManager mgr;
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr);
            expectEquals(panel->getNumLaneRows(), 0);

            mgr.addLane(0, 0, "Compressor", "Threshold", "thresh");
            expectEquals(panel->getNumLaneRows(), 1,
                         "listener notification adds a row");

            mgr.addLane(0, 1, "Compressor", "Ratio", "ratio");
            mgr.addLane(1, 0, "EQ", "Gain", "g");
            expectEquals(panel->getNumLaneRows(), 3);

            mgr.removeLane(1);
            expectEquals(panel->getNumLaneRows(), 2,
                         "row pool shrinks when a lane is removed");

            mgr.clearAll();
            expectEquals(panel->getNumLaneRows(), 0,
                         "row pool empties on clearAll");
        }

        beginTest("Panel survives manager outliving panel destruction");
        {
            AutomationManager mgr;
            mgr.addLane(0, 0, "Plugin", "Param", "p");
            {
                auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr);
                expectEquals(panel->getNumLaneRows(), 1);
            }  // panel destroyed → unregisters listener
            mgr.addLane(0, 1, "Plugin", "Param2", "p2");
            // No assertion — we're verifying "doesn't crash because of
            // a stale listener." Reaching this line means the panel
            // unhooked itself cleanly.
            expect(true);
        }

        beginTest("scrollToLane on missing lane is a safe no-op");
        {
            AutomationManager mgr;
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr);
            mgr.addLane(0, 0, "Plugin", "Param", "p");
            panel->scrollToLane(99, 99);
            expect(true, "scrollToLane(non-existent) returned without crashing");
        }

        //======================================================================
        // Phase 5 follow-up: undo/redo for point edits
        //======================================================================

        beginTest("UndoAction first perform() is a no-op (skipFirstPerform pattern)");
        {
            // Simulates the gesture-handler flow: change is already applied
            // when the action is registered, so the UndoManager's initial
            // perform() must NOT double-apply.
            AutomationManager mgr;
            auto& lane = mgr.addLane(0, 0, "Plug", "Param", "p");

            // Pretend the gesture handler already added a point.
            AutomationPoint applied;
            applied.timeInSeconds = 1.0;
            applied.value         = 0.7f;
            lane.curve.addPoint(applied);

            std::vector<AutomationPoint> before{};                 // empty before
            std::vector<AutomationPoint> after = lane.curve.getPoints();

            juce::UndoManager undo;
            undo.beginNewTransaction("test");
            const bool ok = undo.perform(new AutomationCurveUndoAction(
                mgr, 0, 0, before, after));

            expect(ok, "UndoManager::perform returned true");
            expectEquals(static_cast<int>(readPoints(mgr, 0, 0).size()), 1,
                         "first perform() did not double-add the point");
        }

        beginTest("UndoAction round-trip: add → undo → redo");
        {
            AutomationManager mgr;
            auto& lane = mgr.addLane(0, 0, "Plug", "Param", "p");

            AutomationPoint pt;
            pt.timeInSeconds = 0.5; pt.value = 0.3f;
            lane.curve.addPoint(pt);

            std::vector<AutomationPoint> before{};
            std::vector<AutomationPoint> after = lane.curve.getPoints();

            juce::UndoManager undo;
            undo.beginNewTransaction("add");
            undo.perform(new AutomationCurveUndoAction(mgr, 0, 0, before, after));

            expect(undo.canUndo());
            expect(undo.undo());
            expectEquals(static_cast<int>(readPoints(mgr, 0, 0).size()), 0,
                         "undo emptied the curve");

            expect(undo.canRedo());
            expect(undo.redo());
            expectEquals(static_cast<int>(readPoints(mgr, 0, 0).size()), 1,
                         "redo restored the added point");
        }

        beginTest("UndoAction round-trip: move → undo → redo");
        {
            AutomationManager mgr;
            auto& lane = mgr.addLane(0, 0, "Plug", "Param", "p");

            AutomationPoint p0;
            p0.timeInSeconds = 1.0; p0.value = 0.2f;
            lane.curve.addPoint(p0);

            auto before = lane.curve.getPoints();
            // Apply the move directly (gesture-handler simulation).
            lane.curve.movePoint(0, 2.5, 0.9f);
            auto after = lane.curve.getPoints();

            juce::UndoManager undo;
            undo.beginNewTransaction("move");
            undo.perform(new AutomationCurveUndoAction(mgr, 0, 0, before, after));

            expect(undo.undo());
            auto pts = readPoints(mgr, 0, 0);
            expectEquals(static_cast<int>(pts.size()), 1);
            expectWithinAbsoluteError(pts[0].timeInSeconds, 1.0, 1.0e-9);
            expectWithinAbsoluteError(static_cast<double>(pts[0].value), 0.2, 1.0e-6);

            expect(undo.redo());
            pts = readPoints(mgr, 0, 0);
            expectWithinAbsoluteError(pts[0].timeInSeconds, 2.5, 1.0e-9);
            expectWithinAbsoluteError(static_cast<double>(pts[0].value), 0.9, 1.0e-6);
        }

        beginTest("UndoAction round-trip: delete → undo → redo");
        {
            AutomationManager mgr;
            auto& lane = mgr.addLane(0, 0, "Plug", "Param", "p");

            AutomationPoint p0; p0.timeInSeconds = 1.0; p0.value = 0.4f;
            AutomationPoint p1; p1.timeInSeconds = 2.0; p1.value = 0.6f;
            lane.curve.addPoint(p0);
            lane.curve.addPoint(p1);

            auto before = lane.curve.getPoints();
            lane.curve.removePointAt(1.0, 0.0001);
            auto after = lane.curve.getPoints();

            juce::UndoManager undo;
            undo.beginNewTransaction("delete");
            undo.perform(new AutomationCurveUndoAction(mgr, 0, 0, before, after));

            expectEquals(static_cast<int>(readPoints(mgr, 0, 0).size()), 1,
                         "delete left one point");

            expect(undo.undo());
            expectEquals(static_cast<int>(readPoints(mgr, 0, 0).size()), 2,
                         "undo restored the deleted point");
        }

        beginTest("UndoAction round-trip: setPointCurve → undo");
        {
            AutomationManager mgr;
            auto& lane = mgr.addLane(0, 0, "Plug", "Param", "p");
            AutomationPoint p; p.timeInSeconds = 0.0; p.value = 0.5f;
            lane.curve.addPoint(p);

            auto before = lane.curve.getPoints();
            lane.curve.setPointCurve(0, AutomationPoint::CurveType::Step);
            auto after = lane.curve.getPoints();

            juce::UndoManager undo;
            undo.beginNewTransaction("curve type");
            undo.perform(new AutomationCurveUndoAction(mgr, 0, 0, before, after));

            expect(readPoints(mgr, 0, 0)[0].curve == AutomationPoint::CurveType::Step);
            expect(undo.undo());
            expect(readPoints(mgr, 0, 0)[0].curve == AutomationPoint::CurveType::Linear,
                   "undo restored Linear curve type");
        }

        beginTest("UndoAction round-trip: clear → undo");
        {
            AutomationManager mgr;
            auto& lane = mgr.addLane(0, 0, "Plug", "Param", "p");
            for (int i = 0; i < 4; ++i)
            {
                AutomationPoint p;
                p.timeInSeconds = static_cast<double>(i);
                p.value = 0.25f * static_cast<float>(i);
                lane.curve.addPoint(p);
            }

            auto before = lane.curve.getPoints();
            lane.curve.clear();
            auto after = lane.curve.getPoints();

            juce::UndoManager undo;
            undo.beginNewTransaction("clear");
            undo.perform(new AutomationCurveUndoAction(mgr, 0, 0, before, after));

            expectEquals(static_cast<int>(readPoints(mgr, 0, 0).size()), 0);
            expect(undo.undo());
            expectEquals(static_cast<int>(readPoints(mgr, 0, 0).size()), 4,
                         "undo restored all 4 points");
        }

        //======================================================================
        // Multi-select / clipboard / multi-delete
        //======================================================================

        beginTest("Cmd-click toggles selection without affecting other points");
        {
            AutomationManager mgr;
            auto& lane = mgr.addLane(0, 0, "Plug", "Param", "p");
            for (int i = 0; i < 4; ++i)
            {
                AutomationPoint p; p.timeInSeconds = static_cast<double>(i); p.value = 0.5f;
                lane.curve.addPoint(p);
            }

            juce::UndoManager undo;
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr, &undo);
            auto* view = panel->testGetCurveView(0);
            expect(view != nullptr);

            view->testToggleSelectPoint(0);
            view->testToggleSelectPoint(2);
            const auto& sel = view->testGetSelectedIndices();
            expectEquals(static_cast<int>(sel.size()), 2);
            expect(sel.count(0) != 0);
            expect(sel.count(2) != 0);

            view->testToggleSelectPoint(0);  // toggle off
            expectEquals(static_cast<int>(view->testGetSelectedIndices().size()), 1);
            expect(view->testGetSelectedIndices().count(2) != 0,
                   "leftover selection is the un-toggled index");
        }

        beginTest("testSelectAll picks every point in the lane");
        {
            AutomationManager mgr;
            auto& lane = mgr.addLane(0, 0, "Plug", "Param", "p");
            for (int i = 0; i < 5; ++i)
            {
                AutomationPoint p; p.timeInSeconds = static_cast<double>(i); p.value = 0.0f;
                lane.curve.addPoint(p);
            }

            juce::UndoManager undo;
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr, &undo);
            auto* view = panel->testGetCurveView(0);
            view->testSelectAll();
            expectEquals(static_cast<int>(view->testGetSelectedIndices().size()), 5);
        }

        beginTest("Multi-delete removes all selected points and is undoable");
        {
            AutomationManager mgr;
            auto& lane = mgr.addLane(0, 0, "Plug", "Param", "p");
            for (int i = 0; i < 4; ++i)
            {
                AutomationPoint p; p.timeInSeconds = static_cast<double>(i); p.value = 0.0f;
                lane.curve.addPoint(p);
            }

            juce::UndoManager undo;
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr, &undo);
            auto* view = panel->testGetCurveView(0);
            view->testToggleSelectPoint(1);
            view->testToggleSelectPoint(3);

            view->testDeleteSelected();
            expectEquals(lane.curve.getNumPoints(), 2,
                         "two selected points removed");

            expect(undo.undo());
            expectEquals(lane.curve.getNumPoints(), 4,
                         "undo restored both points");
        }

        beginTest("Copy/paste round-trip preserves relative offsets");
        {
            AutomationClipboard::getInstance().clear();

            AutomationManager mgr;
            auto& src = mgr.addLane(0, 0, "Plug", "Src", "s");
            // Three points at t = 1.0, 2.5, 4.0.
            for (double t : { 1.0, 2.5, 4.0 })
            {
                AutomationPoint p; p.timeInSeconds = t; p.value = 0.7f;
                src.curve.addPoint(p);
            }

            juce::UndoManager undo;
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr, &undo);
            auto* view = panel->testGetCurveView(0);
            view->testSelectAll();
            view->testCopySelectionToClipboard();
            expectEquals(AutomationClipboard::getInstance().getNumPoints(), 3);

            // Paste at anchor 10.0 — original stays, copies appended at +10.
            view->testPasteFromClipboardAt(10.0);
            const auto pts = src.curve.getPoints();
            expectEquals(static_cast<int>(pts.size()), 6,
                         "3 original + 3 pasted");

            // Find the pasted ones by time and check relative offsets.
            std::vector<double> pastedTimes;
            for (const auto& p : pts)
                if (p.timeInSeconds >= 10.0)
                    pastedTimes.push_back(p.timeInSeconds);
            std::sort(pastedTimes.begin(), pastedTimes.end());
            expectEquals(static_cast<int>(pastedTimes.size()), 3);
            expectWithinAbsoluteError(pastedTimes[0], 10.0, 1.0e-9);  // 1.0-1.0 (earliest) + 10
            expectWithinAbsoluteError(pastedTimes[1], 11.5, 1.0e-9);  // 2.5-1.0 + 10
            expectWithinAbsoluteError(pastedTimes[2], 13.0, 1.0e-9);  // 4.0-1.0 + 10
        }

        beginTest("Cross-lane paste: copy from lane A, paste into lane B");
        {
            AutomationClipboard::getInstance().clear();

            AutomationManager mgr;
            (void) mgr.addLane(0, 0, "Plug", "Src",  "s");
            (void) mgr.addLane(0, 1, "Plug", "Dest", "d");

            // Look the lane up after BOTH lanes are added — addLane()
            // returns a reference that's invalidated by subsequent
            // vector growth.
            AutomationPoint p; p.timeInSeconds = 2.0; p.value = 0.5f;
            mgr.getLane(0)->curve.addPoint(p);

            juce::UndoManager undo;
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr, &undo);
            panel->testGetCurveView(0)->testSelectAll();
            panel->testGetCurveView(0)->testCopySelectionToClipboard();

            panel->testGetCurveView(1)->testPasteFromClipboardAt(5.0);
            expectEquals(mgr.getLane(0)->curve.getNumPoints(), 1,
                         "source lane untouched");
            expectEquals(mgr.getLane(1)->curve.getNumPoints(), 1,
                         "destination lane received the point");
            expectWithinAbsoluteError(
                mgr.getLane(1)->curve.getPoints()[0].timeInSeconds,
                5.0, 1.0e-9);
        }

        beginTest("Selection clears on AutomationManager structural change");
        {
            AutomationManager mgr;
            auto& lane = mgr.addLane(0, 0, "Plug", "Param", "p");
            AutomationPoint p; p.timeInSeconds = 1.0; p.value = 0.5f;
            lane.curve.addPoint(p);

            juce::UndoManager undo;
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr, &undo);
            auto* view = panel->testGetCurveView(0);
            view->testToggleSelectPoint(0);
            expectEquals(static_cast<int>(view->testGetSelectedIndices().size()), 1);

            // Adding a new lane fires the manager listener, which
            // rebuilds rows and clears each curve view's selection.
            mgr.addLane(0, 1, "Plug", "Other", "o");
            auto* viewAfter = panel->testGetCurveView(0);
            expect(viewAfter != nullptr);
            expectEquals(static_cast<int>(viewAfter->testGetSelectedIndices().size()), 0,
                         "selection cleared after lane-add fired the listener");
        }

        beginTest("Marquee select replaces selection with points inside the rect");
        {
            AutomationManager mgr;
            auto& lane = mgr.addLane(0, 0, "Plug", "Param", "p");
            // 4 points at t = 0, 1, 2, 3 (seconds), all at value 0.5.
            for (int i = 0; i < 4; ++i)
            {
                AutomationPoint p;
                p.timeInSeconds = static_cast<double>(i);
                p.value = 0.5f;
                lane.curve.addPoint(p);
            }

            juce::UndoManager undo;
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr, &undo);
            // Force a layout so timeToX/valueToY work.
            auto* view = panel->testGetCurveView(0);
            view->setSize(400, 80);

            // Pre-seed a selection that should be replaced (non-additive).
            view->testToggleSelectPoint(0);
            expectEquals(static_cast<int>(view->testGetSelectedIndices().size()), 1);

            // Marquee covering the entire view → all 4 points selected,
            // index 0 replaced (not added on top).
            view->testMarqueeSelect(juce::Rectangle<int>(0, 0, 400, 80), false);
            expectEquals(static_cast<int>(view->testGetSelectedIndices().size()), 4,
                         "non-additive marquee replaces selection with everything in rect");
        }

        beginTest("Shift-marquee extends the existing selection");
        {
            AutomationManager mgr;
            auto& lane = mgr.addLane(0, 0, "Plug", "Param", "p");
            for (int i = 0; i < 4; ++i)
            {
                AutomationPoint p;
                p.timeInSeconds = static_cast<double>(i);
                p.value = 0.5f;
                lane.curve.addPoint(p);
            }

            juce::UndoManager undo;
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr, &undo);
            auto* view = panel->testGetCurveView(0);
            view->setSize(400, 80);

            view->testToggleSelectPoint(0);  // seed selection
            view->testMarqueeSelect(juce::Rectangle<int>(0, 0, 400, 80), /*additive=*/true);
            expectEquals(static_cast<int>(view->testGetSelectedIndices().size()), 4,
                         "additive: union of seed + rect = all 4 points");
        }

        beginTest("UndoAction is a graceful no-op when lane has been removed");
        {
            AutomationManager mgr;
            auto& lane = mgr.addLane(0, 0, "Plug", "Param", "p");
            AutomationPoint p; p.timeInSeconds = 0.0; p.value = 0.5f;
            lane.curve.addPoint(p);

            std::vector<AutomationPoint> before{};
            std::vector<AutomationPoint> after = lane.curve.getPoints();

            juce::UndoManager undo;
            undo.beginNewTransaction("add");
            undo.perform(new AutomationCurveUndoAction(mgr, 0, 0, before, after));

            // Remove the lane that the undo entry references.
            mgr.removeLane(0);
            expectEquals(mgr.getNumLanes(), 0);

            // undo() must complete without crashing and must not resurrect
            // the lane (lane lifecycle isn't this action's job). JUCE's
            // UndoManager always returns true if a transaction exists, so
            // we don't assert the bool — we assert behavioral state.
            undo.undo();
            expectEquals(mgr.getNumLanes(), 0,
                         "undo did not resurrect the removed lane");
        }
    }
};

static AutomationLanesPanelTests automationLanesPanelTests;
