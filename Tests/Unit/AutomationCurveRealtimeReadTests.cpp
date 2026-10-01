/*
  ==============================================================================

    AutomationCurveRealtimeReadTests.cpp
    WaveEdit - Professional Audio Editor

    Coverage for H-H2: AutomationCurve::getValueAtRealtime() is the audio-thread
    read path. It must:
      - return the SAME interpolated value as the message-thread getValueAt()
        for any non-empty curve (single source of truth via evaluate());
      - return false (and leave the out-param untouched) on an empty curve;
      - never block, never allocate, and never touch a shared_ptr refcount -- so
        concurrent message-thread publishing (addPoint/movePoint/clear) can only
        ever cause a read to SKIP (return false), never crash or return garbage.

    The first two properties are checked deterministically. The third is checked
    with a writer thread hammering the curve while a reader thread hammers
    getValueAtRealtime(): every successful read must yield an in-range value and
    the run must complete without a data race fault.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "Automation/AutomationData.h"

#include <atomic>
#include <thread>

//==============================================================================
class AutomationCurveRealtimeReadTests : public juce::UnitTest
{
public:
    AutomationCurveRealtimeReadTests()
        : juce::UnitTest("AutomationCurveRealtimeRead", "Unit") {}

    void runTest() override
    {
        beginTest("Empty curve: realtime read returns false and leaves out untouched");
        {
            AutomationCurve curve;
            float out = -123.0f;
            expect(! curve.getValueAtRealtime(0.0, out),
                   "empty curve must report no value");
            expectEquals(out, -123.0f, "out-param must be untouched on skip");
        }

        beginTest("Realtime read matches message-thread getValueAt across the span");
        {
            AutomationCurve curve;
            addPt(curve, 0.0, 0.0f);
            addPt(curve, 1.0, 1.0f, AutomationPoint::CurveType::Linear);
            addPt(curve, 2.0, 0.25f, AutomationPoint::CurveType::SCurve);
            addPt(curve, 3.0, 0.75f);

            // Sample densely, including before-first / after-last / exact knots.
            for (double t = -0.5; t <= 3.5; t += 0.05)
            {
                float rt = -1.0f;
                const bool ok = curve.getValueAtRealtime(t, rt);
                expect(ok, "non-empty curve must produce a value");

                const float ui = curve.getValueAt(t);
                expectWithinAbsoluteError(rt, ui, 1.0e-6f,
                    "realtime read must equal getValueAt at t=" + juce::String(t));
            }
        }

        beginTest("Realtime read clamps at the endpoints (hold first / last value)");
        {
            AutomationCurve curve;
            addPt(curve, 1.0, 0.2f);
            addPt(curve, 2.0, 0.8f);

            float v = 0.0f;
            expect(curve.getValueAtRealtime(-10.0, v));
            expectWithinAbsoluteError(v, 0.2f, 1.0e-6f, "before first -> first value");
            expect(curve.getValueAtRealtime(100.0, v));
            expectWithinAbsoluteError(v, 0.8f, 1.0e-6f, "after last -> last value");
        }

        beginTest("Concurrent publish + realtime read: no crash, always in range");
        {
            AutomationCurve curve;
            addPt(curve, 0.0, 0.5f);

            std::atomic<bool> stop{false};
            std::atomic<int>  successfulReads{0};
            std::atomic<bool> outOfRange{false};

            // Writer (stands in for the message thread): continuously mutate and
            // occasionally clear, forcing frequent publish() pointer swaps.
            std::thread writer([&curve, &stop]()
            {
                juce::Random rng(0x51CE);
                int i = 0;
                while (! stop.load(std::memory_order_relaxed))
                {
                    const double t = rng.nextDouble() * 4.0;
                    const float  v = rng.nextFloat();  // [0,1)
                    curve.addPoint({ t, v, AutomationPoint::CurveType::Linear });

                    if ((++i % 64) == 0)
                        curve.clear();
                }
            });

            // Reader (stands in for the audio thread): only ever try-locks.
            std::thread reader([&curve, &stop, &successfulReads, &outOfRange]()
            {
                juce::Random rng(0xA0D10);
                while (! stop.load(std::memory_order_relaxed))
                {
                    float v = -999.0f;
                    if (curve.getValueAtRealtime(rng.nextDouble() * 4.0, v))
                    {
                        if (v < 0.0f || v > 1.0f)
                            outOfRange.store(true, std::memory_order_relaxed);
                        successfulReads.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            stop.store(true, std::memory_order_relaxed);
            writer.join();
            reader.join();

            expect(! outOfRange.load(), "no successful read may return an out-of-range value");
            expect(successfulReads.load() > 0,
                   "reader should have observed published points at least once");
        }
    }

private:
    static void addPt(AutomationCurve& c, double t, float v,
                      AutomationPoint::CurveType curve = AutomationPoint::CurveType::Linear)
    {
        c.addPoint({ t, v, curve });
    }
};

static AutomationCurveRealtimeReadTests automationCurveRealtimeReadTests;
