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

    juce::StringArray categories;
    categories.add("Starter");       // Infrastructure tests
    categories.add("AudioEngine");   // Week 2 ✅
    categories.add("BufferManager"); // Week 2 ✅
    categories.add("Processor");     // Week 2 ✅
    categories.add("Integration");   // Week 3 ✅ (File I/O Integration)
    // categories.add("UndoRedo");      // Week 3
    // categories.add("EndToEnd");      // Week 5
    // categories.add("Performance");   // Week 6

    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         WaveEdit Automated Test Suite by ZQ SFX             ║\n";
    std::cout << "║                    Version 0.1.0                             ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    // Run all tests in registered categories
    testRunner.runTestsInCategory("Starter");
    testRunner.runTestsInCategory("AudioEngine");
    testRunner.runTestsInCategory("BufferManager");
    testRunner.runTestsInCategory("Processor");
    testRunner.runTestsInCategory("BWF");               // BWF Metadata tests (Phase 4)
    testRunner.runTestsInCategory("Unit");              // UndoRedo data integrity tests
    testRunner.runTestsInCategory("Integration");       // File I/O integration tests
    testRunner.runTestsInCategory("MultiDocument");     // Multi-file architecture tests
    testRunner.runTestsInCategory("InterFileClipboard"); // Inter-file clipboard tests
    testRunner.runTestsInCategory("Batch");             // Batch processor tests
    testRunner.runTestsInCategory("ChannelSystem");     // Channel system tests

    // Print results
    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    int totalTests = testRunner.getNumResults();
    if (totalTests > 0)
    {
        // Sum up passes and failures from all test results
        int totalPasses = 0;
        int totalFailures = 0;

        for (int i = 0; i < totalTests; ++i)
        {
            const juce::UnitTestRunner::TestResult* result = testRunner.getResult(i);
            if (result != nullptr)
            {
                totalPasses += result->passes;
                totalFailures += result->failures;

                // Log failures
                if (result->failures > 0)
                {
                    std::cout << "\n❌ " << result->unitTestName.toStdString()
                              << " :: " << result->subcategoryName.toStdString()
                              << " - " << result->failures << " failures\n";

                    for (const auto& message : result->messages)
                    {
                        std::cout << "   " << message.toStdString() << "\n";
                    }
                }
            }
        }

        std::cout << "\nTotal test groups: " << totalTests << "\n";
        std::cout << "Total assertions: " << (totalPasses + totalFailures) << "\n";
        std::cout << "Passed: " << totalPasses << "\n";
        std::cout << "Failed: " << totalFailures << "\n";

        if (totalFailures == 0)
        {
            std::cout << "\n✅ All tests PASSED\n";
            return 0;
        }
        else
        {
            std::cout << "\n❌ Some tests FAILED\n";
            return 1;
        }
    }
    else
    {
        std::cout << "⚠️  No tests were run\n";
        return 1;
    }
}
