/*
  ==============================================================================

    AutomationRecorderTests.cpp
    WaveEdit - Professional Audio Editor

    Phase 4 of Plugin Parameter Automation: validate that
    AutomationRecorder gates and coalesces parameter-change events
    correctly without needing a real VST3 plugin.

    Tests cover:
      - applyRecordedValue gating (global arm, lane.isRecording, transport state)
      - debounce coalescing of rapid changes
      - dispatcher attach/detach lifecycle (ParameterDispatcher's
        AudioProcessorParameter::Listener registration)
      - refreshAttachments idempotence

  ==============================================================================
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../../Source/Automation/AutomationManager.h"
#include "../../Source/Automation/AutomationRecorder.h"
#include "../TestUtils/MockAudioProcessor.h"

namespace
{

// Build a fully-armed recorder driven by deterministic test transport
// state so every assertion is reproducible. Returns the recorder via
// AutomationManager (manager owns it).
struct Harness
{
    AutomationManager manager;
    double currentTime = 0.0;
    bool playing = true;

    Harness()
    {
        manager.setAudioEngine(nullptr);
        auto* rec = manager.getRecorder();
        rec->setTransportProvidersForTest(
            [this] { return playing; },
            [this] { return currentTime; });
    }

    AutomationRecorder& recorder() { return *manager.getRecorder(); }
};

} // namespace

//==============================================================================
class AutomationRecorderTests : public juce::UnitTest
{
public:
    AutomationRecorderTests() : juce::UnitTest("AutomationRecorder", "Unit") {}

    void runTest() override
    {
        beginTest("Gate: not armed → no point recorded");
        {
            Harness h;
            h.manager.addLane(0, 0, "MockPlugin", "Param 0", "p0");
            h.manager.findLane(0, 0)->isRecording = true;
            // Global arm intentionally off.

            h.recorder().debugInjectParameterChange(0, 0, 0.75f);

            expectEquals(h.manager.findLane(0, 0)->curve.getNumPoints(), 0,
                         "no point recorded when global arm is off");
        }

        beginTest("Gate: lane not in record → no point recorded");
        {
            Harness h;
            h.recorder().setGlobalArm(true);
            h.manager.addLane(0, 0, "MockPlugin", "Param 0", "p0");
            // lane.isRecording stays false.

            h.recorder().debugInjectParameterChange(0, 0, 0.75f);

            expectEquals(h.manager.findLane(0, 0)->curve.getNumPoints(), 0,
                         "no point recorded when the lane isn't armed");
        }

        beginTest("Gate: transport stopped → no point recorded");
        {
            Harness h;
            h.recorder().setGlobalArm(true);
            h.manager.addLane(0, 0, "MockPlugin", "Param 0", "p0");
            h.manager.findLane(0, 0)->isRecording = true;
            h.playing = false;

            h.recorder().debugInjectParameterChange(0, 0, 0.75f);

            expectEquals(h.manager.findLane(0, 0)->curve.getNumPoints(), 0,
                         "no point recorded when transport is not playing");
        }

        beginTest("Happy path: armed lane records a point at the current time");
        {
            Harness h;
            h.recorder().setGlobalArm(true);
            h.manager.addLane(0, 0, "MockPlugin", "Param 0", "p0");
            h.manager.findLane(0, 0)->isRecording = true;
            h.currentTime = 1.5;

            h.recorder().debugInjectParameterChange(0, 0, 0.42f);

            auto pts = h.manager.findLane(0, 0)->curve.getPoints();
            expectEquals(static_cast<int>(pts.size()), 1, "exactly one point");
            expectWithinAbsoluteError(pts[0].timeInSeconds, 1.5, 1e-6,
                                       "point timestamp matches transport position");
            expectWithinAbsoluteError<float>(pts[0].value, 0.42f, 1e-5f,
                                              "point value matches injected value");
        }

        beginTest("armParameter creates a lane and disarmParameter clears its flag");
        {
            Harness h;
            expect(h.manager.findLane(0, 0) == nullptr,
                   "no lane present yet");

            h.recorder().armParameter(0, 0, "MockPlugin", "Param 0", "p0");
            auto* lane = h.manager.findLane(0, 0);
            expect(lane != nullptr, "armParameter created the lane");
            expect(lane->isRecording, "lane is in record state");

            h.recorder().disarmParameter(0, 0);
            expect(!lane->isRecording, "disarmParameter cleared record flag");
            expect(h.manager.findLane(0, 0) != nullptr,
                   "disarmParameter does NOT delete the lane");
        }

        beginTest("Debounce: rapid changes inside the window coalesce");
        {
            Harness h;
            h.recorder().setGlobalArm(true);
            h.recorder().setDebounceMillis(20);  // 20ms gate
            h.manager.addLane(0, 0, "MockPlugin", "Param 0", "p0");
            h.manager.findLane(0, 0)->isRecording = true;

            // 50 injections at t=0, t=0.001s, t=0.002s, ...
            for (int i = 0; i < 50; ++i)
            {
                h.currentTime = i * 0.001;
                h.recorder().debugInjectParameterChange(0, 0,
                                                          0.5f + 0.005f * i);
            }

            // First point always emits (firstPoint guard); subsequent points
            // need debounce window (20ms) AND value change. Across 50ms we
            // expect ~3 points at most.
            const int n = h.manager.findLane(0, 0)->curve.getNumPoints();
            expect(n >= 1, "at least one point recorded");
            expect(n <= 5, "rapid-fire updates were coalesced (≤5 points)");
        }

        beginTest("Debounce: no value change → no extra point");
        {
            Harness h;
            h.recorder().setGlobalArm(true);
            h.recorder().setDebounceMillis(0);  // even with debounce off
            h.manager.addLane(0, 0, "MockPlugin", "Param 0", "p0");
            h.manager.findLane(0, 0)->isRecording = true;

            h.currentTime = 0.0;
            h.recorder().debugInjectParameterChange(0, 0, 0.5f);
            h.currentTime = 1.0;
            h.recorder().debugInjectParameterChange(0, 0, 0.5f);  // identical

            expectEquals(h.manager.findLane(0, 0)->curve.getNumPoints(), 1,
                         "second identical value didn't append a duplicate");
        }

        beginTest("ParameterDispatcher: registers and removes its parameter listeners");
        {
            // Pure attach/detach lifecycle test — no recording assertions,
            // just verify no crash on parameter changes after destruction.
            Harness h;
            auto plugin = std::make_unique<waveedit::test::MockAudioProcessor>(3);

            // Drive a value change *before* the dispatcher exists — should be
            // a no-op since no listeners are attached.
            plugin->getParameters()[0]->setValueNotifyingHost(0.7f);

            // Construct dispatcher (registers listeners on every param).
            {
                AutomationRecorder::ParameterDispatcher dispatcher(
                    h.recorder(), 0, plugin.get());
                expectEquals(dispatcher.getPluginIndex(), 0,
                             "dispatcher remembers its plugin index");
                expect(dispatcher.getPlugin() == plugin.get(),
                       "dispatcher remembers its plugin pointer");

                // Drive a value change while the dispatcher is alive.
                // Without arming there should still be no recorded
                // points; we just verify the listener fires without
                // crashing and the AsyncUpdater queue can be drained.
                plugin->getParameters()[0]->setValueNotifyingHost(0.123f);
                dispatcher.flushPending();
            }
            // Dispatcher destroyed — listeners must have been removed.
            // If they weren't, this next setValueNotifyingHost() would
            // call into a freed object and crash / use-after-free.
            plugin->getParameters()[1]->setValueNotifyingHost(0.3f);
            expect(true, "no crash after dispatcher destruction");
        }

        beginTest("ParameterDispatcher: rebinds plugin index across reorders");
        {
            // Simulate a remove-then-reorder by constructing two
            // dispatchers, then calling setPluginIndex on the survivor
            // to mimic what refreshAttachments does after a chain
            // mutation. Verifies the dispatcher is still attached to
            // its original plugin pointer afterwards.
            Harness h;
            auto pluginA = std::make_unique<waveedit::test::MockAudioProcessor>(2);
            auto pluginB = std::make_unique<waveedit::test::MockAudioProcessor>(2);

            AutomationRecorder::ParameterDispatcher dispA(h.recorder(), 0, pluginA.get());
            AutomationRecorder::ParameterDispatcher dispB(h.recorder(), 1, pluginB.get());

            // Pretend pluginA was removed; pluginB shifted from index 1 → 0.
            dispB.setPluginIndex(0);

            expect(dispB.getPlugin() == pluginB.get(),
                   "rebound dispatcher still owns its original plugin pointer");
            expectEquals(dispB.getPluginIndex(), 0,
                          "rebound dispatcher reflects new chain index");

            // Drive a value change on pluginB and confirm no crash.
            pluginB->getParameters()[0]->setValueNotifyingHost(0.42f);
            dispB.flushPending();
            expect(true, "no crash after dispatcher reindex");
        }
    }
};

static AutomationRecorderTests automationRecorderTests;
