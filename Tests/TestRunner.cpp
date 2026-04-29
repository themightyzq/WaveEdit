/*
  ==============================================================================

    TestRunner.cpp
    Created: 2025-10-15
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

    Main entry point for WaveEdit automated test suite.
    Uses JUCE UnitTestRunner for console-based test execution.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

// Include test utility headers
#include "TestAudioFiles.h"
#include "AudioAssertions.h"

// ============================================================================
// Starter Test - Verifies test infrastructure is working
// ============================================================================

class InfrastructureTest : public juce::UnitTest
{
public:
    InfrastructureTest() : juce::UnitTest("Infrastructure", "Starter") {}

    void runTest() override
    {
        beginTest("Test infrastructure is operational");

        // Verify test utilities are available
        expect(true, "Test framework operational");

        // Test audio generator
        auto sineWave = TestAudio::createSineWave(1000.0, 1.0, 44100.0, 1.0, 2);
        expect(sineWave.getNumChannels() == 2, "Sine wave generator creates stereo");
        expect(sineWave.getNumSamples() == 44100, "Sine wave generator creates 1 second");

        // Test silence generator
        auto silence = TestAudio::createSilence(44100.0, 0.5, 1);
        expect(AudioAssertions::expectSilence(silence), "Silence generator creates zeros");

        // Test sample comparison
        auto buffer1 = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.1);
        auto buffer2 = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.1);
        expect(AudioAssertions::expectBuffersEqual(buffer1, buffer2), "Identical buffers compare equal");

        logMessage("✅ Test infrastructure verified - ready for comprehensive testing");
    }
};

// Register the starter test
static InfrastructureTest infrastructureTest;

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int, char**)
{
    juce::ConsoleApplication app;
    app.addVersionCommand("--version", "WaveEdit Test Suite v0.1.0");

    juce::UnitTestRunner testRunner;

    // CI-aware test selection.
    //
    // Some tests require a real audio device or a live message-thread
    // event loop, and assert/segfault when run on a headless GitHub
    // Actions runner. They pass locally on a developer machine. When
    // running in CI, set WAVEEDIT_CI=1 to skip them and keep the
    // suite green for everything that does not depend on the host
    // audio stack.
    const bool isCI = juce::SystemStats::getEnvironmentVariable("WAVEEDIT_CI", "")
                          .isNotEmpty();

    // Test names that depend on a live audio device. Matched against
    // juce::UnitTest::getName().
    const juce::StringArray ciSkipNames {
        // Whole-engine — initialize/playback assumes a device.
        "AudioEngine Thread Safety",
        "AudioEngine Functional",
        "AudioEngine Edge Cases",
        "AudioEngine Preview System",
        // Integration tests that drive the engine for playback.
        "Basic Playback+Editing",
        "Real-Time Buffer Updates",
        "Edit Workflows",
    };

    auto shouldSkip = [&](juce::UnitTest* t) -> bool
    {
        return isCI && ciSkipNames.contains(t->getName());
    };

    // NOTE on counting: juce::UnitTestRunner::runTests clears the results
    // list at the start of every call. To get a true overall summary we
    // accumulate per-call results into our own counters *between* calls —
    // a single getNumResults() at the end would only reflect the last run.
    int totalGroups   = 0;
    int totalPasses   = 0;
    int totalFailures = 0;
    int totalSkipped  = 0;
    juce::StringArray failureBlocks;

    auto runCategory = [&](const char* category)
    {
        juce::Array<juce::UnitTest*> tests;
        for (auto* t : juce::UnitTest::getTestsInCategory(category))
        {
            if (shouldSkip(t))
            {
                ++totalSkipped;
                std::cout << "⏭  Skipping (CI): " << t->getName() << "\n";
                continue;
            }
            tests.add(t);
        }

        if (tests.isEmpty())
            return;

        testRunner.runTests(tests);

        const int n = testRunner.getNumResults();
        for (int i = 0; i < n; ++i)
        {
            const auto* result = testRunner.getResult(i);
            if (result == nullptr) continue;

            ++totalGroups;
            totalPasses   += result->passes;
            totalFailures += result->failures;

            if (result->failures > 0)
            {
                juce::String block;
                block << "\n❌ " << result->unitTestName
                      << " :: " << result->subcategoryName
                      << " - " << result->failures << " failures";
                for (const auto& message : result->messages)
                    block << "\n   " << message;
                failureBlocks.add(block);
            }
        }
    };

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         WaveEdit Automated Test Suite by ZQ SFX             ║\n";
    std::cout << "║                    Version 0.1.0                             ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    if (isCI)
        std::cout << "Mode: CI (device-dependent tests will be skipped)\n";
    std::cout << "\n";

    runCategory("Starter");
    runCategory("AudioEngine");
    runCategory("BufferManager");
    runCategory("Processor");
    runCategory("BWF");               // BWF Metadata tests (Phase 4)
    runCategory("Unit");              // UndoRedo data integrity + keymap conflicts
    runCategory("Integration");       // File I/O integration tests
    runCategory("MultiDocument");     // Multi-file architecture tests
    runCategory("InterFileClipboard"); // Inter-file clipboard tests
    runCategory("Batch");             // Batch processor tests
    runCategory("ChannelSystem");     // Channel system tests
    runCategory("DSP");               // DSP engine tests (HeadTailEngine)
    runCategory("RegionManager");     // Region/auto-region/multi-drag tests
    runCategory("MarkerSystem");      // Marker + MarkerManager tests

    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    if (totalGroups <= 0)
    {
        std::cout << "⚠️  No tests were run\n";
        return 1;
    }

    for (const auto& block : failureBlocks)
        std::cout << block << "\n";

    std::cout << "\nTotal test groups: " << totalGroups << "\n";
    std::cout << "Total assertions: "    << (totalPasses + totalFailures) << "\n";
    std::cout << "Passed: "               << totalPasses   << "\n";
    std::cout << "Failed: "               << totalFailures << "\n";
    if (totalSkipped > 0)
        std::cout << "Skipped: "          << totalSkipped  << " (CI mode)\n";

    if (totalFailures == 0)
    {
        std::cout << "\n✅ All tests PASSED\n";
        return 0;
    }

    std::cout << "\n❌ Some tests FAILED\n";
    return 1;
}
