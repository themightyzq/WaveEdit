/*
  ==============================================================================

    AutomationLanesPanelRowIndexTests.cpp
    WaveEdit - Professional Audio Editor

    Pass-2 defect coverage for M16: pooled AutomationLaneRow reuse must
    re-point row N at lane N after a NON-TAIL lane removal. Before the
    fix, a row kept its construction-time m_laneIndex, so after the
    vector shifted, rows read the wrong lane (off-by-one) — wrong names
    and, worse, record-arming the wrong parameter.

    The panel rebuilds its rows from the AutomationManager::Listener
    callback that addLane/removeLane fire, so these tests exercise the
    real rebuild path without any mouse/keyboard events.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Automation/AutomationManager.h"
#include "UI/AutomationLanesPanel.h"

namespace
{
    // Each lane gets a unique (pluginIndex, parameterIndex) so we can
    // assert exactly which lane a given row is bound to.
    void addLane(AutomationManager& mgr, int pluginIndex, int parameterIndex)
    {
        mgr.addLane(pluginIndex, parameterIndex,
                    "Plugin" + juce::String(pluginIndex),
                    "Param" + juce::String(parameterIndex),
                    "id" + juce::String(parameterIndex));
    }
}

//==============================================================================
class AutomationLanesPanelRowIndexTests : public juce::UnitTest
{
public:
    AutomationLanesPanelRowIndexTests()
        : juce::UnitTest("AutomationLanesPanelRowIndex", "Unit") {}

    void runTest() override
    {
        beginTest("M16: row N maps to lane N after a tail-only removal");
        {
            AutomationManager mgr;
            mgr.setAudioEngine(nullptr);
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr);

            addLane(mgr, 0, 10);
            addLane(mgr, 0, 11);
            addLane(mgr, 0, 12);

            mgr.removeLane(2);  // remove the tail lane

            // Two rows remain, bound to lanes 0 and 1 in order.
            expect(panel->testGetCurveView(0) != nullptr);
            expect(panel->testGetCurveView(1) != nullptr);
            expect(panel->testGetCurveView(2) == nullptr, "tail row removed");

            const auto* l0 = panel->testGetCurveView(0)->testGetLane();
            const auto* l1 = panel->testGetCurveView(1)->testGetLane();
            expect(l0 != nullptr && l0->parameterIndex == 10);
            expect(l1 != nullptr && l1->parameterIndex == 11);
        }

        beginTest("M16: row N maps to lane N after a NON-TAIL removal");
        {
            AutomationManager mgr;
            mgr.setAudioEngine(nullptr);
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr);

            addLane(mgr, 0, 10);
            addLane(mgr, 0, 11);
            addLane(mgr, 0, 12);

            // Remove the MIDDLE lane. The manager collapses indices:
            // remaining lanes are param 10 (lane 0) and param 12 (lane 1).
            mgr.removeLane(1);

            const auto* l0 = panel->testGetCurveView(0)->testGetLane();
            const auto* l1 = panel->testGetCurveView(1)->testGetLane();

            expect(l0 != nullptr, "row 0 still bound");
            expect(l1 != nullptr, "row 1 still bound");
            expectEquals(l0->parameterIndex, 10,
                         "row 0 still maps to the first surviving lane");
            expectEquals(l1->parameterIndex, 12,
                         "row 1 maps to the lane that shifted DOWN, not stale lane");
        }

        beginTest("M16: removing the first lane re-points every row");
        {
            AutomationManager mgr;
            mgr.setAudioEngine(nullptr);
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr);

            addLane(mgr, 0, 10);
            addLane(mgr, 0, 11);
            addLane(mgr, 0, 12);

            mgr.removeLane(0);  // remove the head; all lanes shift down

            const auto* l0 = panel->testGetCurveView(0)->testGetLane();
            const auto* l1 = panel->testGetCurveView(1)->testGetLane();
            expect(l0 != nullptr && l1 != nullptr);
            expectEquals(l0->parameterIndex, 11, "row 0 now shows old lane 1");
            expectEquals(l1->parameterIndex, 12, "row 1 now shows old lane 2");
        }

        beginTest("M16: re-growing the pool after a shrink keeps mapping correct");
        {
            AutomationManager mgr;
            mgr.setAudioEngine(nullptr);
            auto panel = std::make_unique<AutomationLanesPanel>(&mgr, nullptr);

            addLane(mgr, 0, 10);
            addLane(mgr, 0, 11);
            addLane(mgr, 0, 12);
            mgr.removeLane(0);           // shrink to 2 (rows reused)
            addLane(mgr, 0, 13);         // grow back to 3

            const auto* l0 = panel->testGetCurveView(0)->testGetLane();
            const auto* l1 = panel->testGetCurveView(1)->testGetLane();
            const auto* l2 = panel->testGetCurveView(2)->testGetLane();
            expect(l0 != nullptr && l1 != nullptr && l2 != nullptr);
            expectEquals(l0->parameterIndex, 11);
            expectEquals(l1->parameterIndex, 12);
            expectEquals(l2->parameterIndex, 13);
        }
    }
};

static AutomationLanesPanelRowIndexTests automationLanesPanelRowIndexTests;
