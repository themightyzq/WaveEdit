/*
  ==============================================================================

    RegionListPanelTests.cpp
    WaveEdit - Professional Audio Editor

    Integration tests for RegionListPanel: panel construction, region
    display, listener wiring, refresh stability, sample-rate updates,
    selection lifecycle.

    These tests do not exercise mouse/keyboard interaction (no
    MessageManager dispatch loop in the test runner) — they verify the
    state-side contract of the panel: after refresh(), getNumRows()
    matches the manager; selectRegion() handles invalid indices; the
    listener interface is satisfied by a non-abstract mock; etc.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "UI/RegionListPanel.h"
#include "Utils/RegionManager.h"
#include "Utils/Region.h"

namespace
{
    /**
     * Concrete RegionListPanel::Listener for tests. Records call counts and
     * the most recent arguments so tests can assert on them. Implements all
     * four pure-virtual methods so the class is instantiable.
     */
    class MockListener : public RegionListPanel::Listener
    {
    public:
        void regionListPanelJumpToRegion(int regionIndex) override
        {
            ++jumpCount;
            lastJumpIndex = regionIndex;
        }

        void regionListPanelRegionDeleted(int regionIndex) override
        {
            ++deleteCount;
            lastDeleteIndex = regionIndex;
        }

        void regionListPanelRegionRenamed(int regionIndex,
                                          const juce::String& newName) override
        {
            ++renameCount;
            lastRenameIndex = regionIndex;
            lastRenameName  = newName;
        }

        void regionListPanelRegionSelected(int regionIndex) override
        {
            ++selectCount;
            lastSelectIndex = regionIndex;
        }

        int jumpCount        = 0;
        int deleteCount      = 0;
        int renameCount      = 0;
        int selectCount      = 0;
        int lastJumpIndex    = -1;
        int lastDeleteIndex  = -1;
        int lastRenameIndex  = -1;
        int lastSelectIndex  = -1;
        juce::String lastRenameName;
    };
}

//==============================================================================
class RegionListPanelTests : public juce::UnitTest
{
public:
    RegionListPanelTests()
        : juce::UnitTest("RegionListPanel Integration", "Integration") {}

    void runTest() override
    {
        constexpr double kSampleRate = 44100.0;

        beginTest("Panel construction with empty manager");
        {
            RegionManager mgr;
            auto panel = std::make_unique<RegionListPanel>(&mgr, kSampleRate);
            panel->refresh();
            expectEquals(panel->getNumRows(), 0);
        }

        beginTest("getNumRows tracks region count after refresh");
        {
            RegionManager mgr;
            auto panel = std::make_unique<RegionListPanel>(&mgr, kSampleRate);

            panel->refresh();
            expectEquals(panel->getNumRows(), 0);

            mgr.addRegion(Region("Intro",  0,     44100));
            mgr.addRegion(Region("Verse",  44100, 88200));
            mgr.addRegion(Region("Chorus", 88200, 132300));

            panel->refresh();
            expectEquals(panel->getNumRows(), 3);
        }

        beginTest("Selecting a valid index does not crash");
        {
            RegionManager mgr;
            auto panel = std::make_unique<RegionListPanel>(&mgr, kSampleRate);

            mgr.addRegion(Region("R1", 0,     44100));
            mgr.addRegion(Region("R2", 44100, 88200));
            mgr.addRegion(Region("R3", 88200, 132300));
            panel->refresh();

            panel->selectRegion(0);
            panel->selectRegion(1);
            panel->selectRegion(2);
            // No expect() here — we're asserting "doesn't crash". A failure
            // would manifest as a JUCE leak detector hit at shutdown.
            expect(true, "selectRegion(valid) returned without crashing");
        }

        beginTest("Selecting out-of-range index is a no-op");
        {
            RegionManager mgr;
            auto panel = std::make_unique<RegionListPanel>(&mgr, kSampleRate);

            mgr.addRegion(Region("R1", 0, 44100));
            panel->refresh();

            panel->selectRegion(-1);
            panel->selectRegion(999);
            expect(true, "selectRegion(out-of-range) handled gracefully");
        }

        beginTest("Listener attaches without callback fire on setListener");
        {
            RegionManager mgr;
            auto panel = std::make_unique<RegionListPanel>(&mgr, kSampleRate);

            mgr.addRegion(Region("Sample", 0, 44100));
            panel->refresh();

            MockListener listener;
            panel->setListener(&listener);

            expectEquals(listener.jumpCount,   0);
            expectEquals(listener.deleteCount, 0);
            expectEquals(listener.renameCount, 0);
            expectEquals(listener.selectCount, 0);
        }

        beginTest("Sample rate change does not invalidate row count");
        {
            RegionManager mgr;
            auto panel = std::make_unique<RegionListPanel>(&mgr, kSampleRate);

            mgr.addRegion(Region("Test", 0, 44100));
            panel->refresh();
            expectEquals(panel->getNumRows(), 1);

            panel->setSampleRate(48000.0);
            panel->refresh();
            expectEquals(panel->getNumRows(), 1);
        }

        beginTest("Multiple refresh calls produce stable row count");
        {
            RegionManager mgr;
            auto panel = std::make_unique<RegionListPanel>(&mgr, kSampleRate);

            mgr.addRegion(Region("R1", 0,     44100));
            mgr.addRegion(Region("R2", 44100, 88200));

            for (int i = 0; i < 10; ++i)
            {
                panel->refresh();
                expectEquals(panel->getNumRows(), 2,
                             "Row count stable on refresh #" + juce::String(i + 1));
            }
        }

        beginTest("Adding regions after construction is reflected on refresh");
        {
            RegionManager mgr;
            auto panel = std::make_unique<RegionListPanel>(&mgr, kSampleRate);
            panel->refresh();
            expectEquals(panel->getNumRows(), 0);

            mgr.addRegion(Region("New", 0, 44100));
            panel->refresh();
            expectEquals(panel->getNumRows(), 1);
        }

        beginTest("Removing regions is reflected on refresh");
        {
            RegionManager mgr;
            auto panel = std::make_unique<RegionListPanel>(&mgr, kSampleRate);

            mgr.addRegion(Region("A", 0,     22050));
            mgr.addRegion(Region("B", 22050, 44100));
            mgr.addRegion(Region("C", 44100, 66150));
            panel->refresh();
            expectEquals(panel->getNumRows(), 3);

            mgr.removeRegion(1);
            panel->refresh();
            expectEquals(panel->getNumRows(), 2);
        }
    }
};

static RegionListPanelTests regionListPanelTests;
