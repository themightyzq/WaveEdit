/*
  ==============================================================================

    AutomationApplyGuardTests.cpp
    WaveEdit - Professional Audio Editor

    Pass-2 defect coverage for H23: when AutomationManager::applyAutomation
    pushes a value into a plugin parameter on the AUDIO thread, JUCE fires
    parameterValueChanged synchronously on that thread. Recording that echo
    would (a) feed automation back into its own curve and (b) take a
    CriticalSection + triggerAsyncUpdate on the audio thread (§6.4).

    The fix raises an atomic ScopedApplyGuard for the duration of the
    setValue loop; the ParameterDispatcher checks it and bails lock-free.
    These tests assert the dispatcher records nothing while the guard is
    held and resumes normally once it is released.

    Thread interleaving itself is reasoning-only (no real audio thread in
    the runner); here we verify the guard's gating logic deterministically.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Automation/AutomationManager.h"
#include "Automation/AutomationRecorder.h"
#include "MockAudioProcessor.h"

//==============================================================================
class AutomationApplyGuardTests : public juce::UnitTest
{
public:
    AutomationApplyGuardTests()
        : juce::UnitTest("AutomationApplyGuard", "Unit") {}

    void runTest() override
    {
        beginTest("H23: dispatcher records a change with no guard held");
        {
            AutomationManager mgr;
            mgr.setAudioEngine(nullptr);  // tests default to playing @ t=0
            auto& rec = *mgr.getRecorder();
            rec.setGlobalArm(true);

            mgr.addLane(0, 0, "Mock", "Param 0", "p0");
            mgr.findLane(0, 0)->isRecording = true;

            waveedit::test::MockAudioProcessor proc(2);
            AutomationRecorder::ParameterDispatcher disp(rec, 0, &proc);

            disp.parameterValueChanged(0, 0.8f);
            disp.flushPending();

            expectEquals(mgr.findLane(0, 0)->curve.getNumPoints(), 1,
                         "baseline: a normal parameter change is recorded");
        }

        beginTest("H23: ScopedApplyGuard suppresses parameter-change capture");
        {
            AutomationManager mgr;
            mgr.setAudioEngine(nullptr);
            auto& rec = *mgr.getRecorder();
            rec.setGlobalArm(true);

            mgr.addLane(0, 0, "Mock", "Param 0", "p0");
            mgr.findLane(0, 0)->isRecording = true;

            waveedit::test::MockAudioProcessor proc(2);
            AutomationRecorder::ParameterDispatcher disp(rec, 0, &proc);

            {
                AutomationRecorder::ScopedApplyGuard guard(&rec);
                expect(rec.isApplyingAutomation(),
                       "guard raises the applying-automation flag");

                // This mimics the echo JUCE fires from setValue() while
                // applyAutomation is running. It must be dropped.
                disp.parameterValueChanged(0, 0.8f);
                disp.flushPending();
            }

            expect(! rec.isApplyingAutomation(),
                   "guard lowers the flag on scope exit");
            expectEquals(mgr.findLane(0, 0)->curve.getNumPoints(), 0,
                         "no point recorded for an automation-driven echo");
        }

        beginTest("H23: recording resumes after the guard is released");
        {
            AutomationManager mgr;
            mgr.setAudioEngine(nullptr);
            auto& rec = *mgr.getRecorder();
            rec.setGlobalArm(true);

            mgr.addLane(0, 0, "Mock", "Param 0", "p0");
            mgr.findLane(0, 0)->isRecording = true;

            waveedit::test::MockAudioProcessor proc(2);
            AutomationRecorder::ParameterDispatcher disp(rec, 0, &proc);

            {
                AutomationRecorder::ScopedApplyGuard guard(&rec);
                disp.parameterValueChanged(0, 0.3f);
                disp.flushPending();
            }
            // Now a genuine user-driven change, guard released.
            disp.parameterValueChanged(0, 0.6f);
            disp.flushPending();

            expectEquals(mgr.findLane(0, 0)->curve.getNumPoints(), 1,
                         "exactly the post-guard user change is recorded");
        }

        beginTest("H23: ScopedApplyGuard(nullptr) is a safe no-op");
        {
            AutomationRecorder::ScopedApplyGuard guard(nullptr);
            // Just constructing/destroying must not crash.
            expect(true);
        }
    }
};

static AutomationApplyGuardTests automationApplyGuardTests;
