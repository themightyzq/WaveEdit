/*
  ==============================================================================

    PluginPresetManagerTests.cpp
    WaveEdit - Professional Audio Editor

    Format-extension tests for plugin chain presets that bundle
    AutomationManager state. The chain's plugin-state round-trip is
    not exercised here (would require a real VST3/AU plugin) — these
    tests focus on the new automation block and on backwards
    compatibility with the chain-only API.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Automation/AutomationData.h"
#include "Automation/AutomationManager.h"
#include "Plugins/PluginChain.h"
#include "Plugins/PluginPresetManager.h"

namespace
{
    // Build an AutomationManager seeded with two lanes containing a
    // few points each, suitable for round-trip equality checks.
    void seedManager(AutomationManager& mgr)
    {
        (void) mgr.addLane(0, 0, "Comp",   "Threshold", "thresh");
        (void) mgr.addLane(0, 1, "Comp",   "Ratio",     "ratio");

        // Repopulate via lookup since addLane can invalidate references.
        if (auto* lane0 = mgr.getLane(0))
        {
            AutomationPoint p0; p0.timeInSeconds = 0.5; p0.value = 0.25f;
            AutomationPoint p1; p1.timeInSeconds = 1.0; p1.value = 0.75f;
            p1.curve = AutomationPoint::CurveType::SCurve;
            lane0->curve.addPoint(p0);
            lane0->curve.addPoint(p1);
        }

        if (auto* lane1 = mgr.getLane(1))
        {
            AutomationPoint p0; p0.timeInSeconds = 2.0; p0.value = 0.4f;
            p0.curve = AutomationPoint::CurveType::Step;
            lane1->curve.addPoint(p0);
        }
    }

    // Tests need to write to disk; ensure each test uses a unique file.
    juce::File makeTempFile(const juce::String& tag)
    {
        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto path    = tempDir.getChildFile("WaveEditTest_" + tag + ".wepchain");
        path.deleteFile();
        return path;
    }
}

//==============================================================================
class PluginPresetManagerTests : public juce::UnitTest
{
public:
    PluginPresetManagerTests()
        : juce::UnitTest("PluginPresetManager", "Unit") {}

    void runTest() override
    {
        beginTest("Empty AutomationManager: no automation key in saved JSON");
        {
            PluginChain chain;
            AutomationManager mgr;  // empty

            auto file = makeTempFile("empty");
            expect(PluginPresetManager::exportPreset(chain, mgr, file));
            expect(file.existsAsFile());

            const auto raw = file.loadFileAsString();
            expect(! raw.contains("\"automation\""),
                   "empty manager: no automation key written");

            file.deleteFile();
        }

        beginTest("Round-trip: chain + 2 lanes survive export → import");
        {
            PluginChain chainOut;          // empty (no real plugins available in tests)
            AutomationManager mgrOut;
            seedManager(mgrOut);
            expectEquals(mgrOut.getNumLanes(), 2);

            auto file = makeTempFile("roundtrip");
            expect(PluginPresetManager::exportPreset(chainOut, mgrOut, file));

            PluginChain chainIn;
            AutomationManager mgrIn;
            expect(PluginPresetManager::importPreset(chainIn, mgrIn, file));

            expectEquals(mgrIn.getNumLanes(), 2,
                         "both lanes restored");

            // Inspect lane 0: 2 points with the seeded times+values+curves.
            const auto& lanes = mgrIn.getLanes();
            const auto pts0 = lanes[0].curve.getPoints();
            expectEquals(static_cast<int>(pts0.size()), 2);
            expectWithinAbsoluteError(pts0[0].timeInSeconds, 0.5, 1.0e-9);
            expectWithinAbsoluteError(static_cast<double>(pts0[0].value), 0.25, 1.0e-6);
            expect(pts0[1].curve == AutomationPoint::CurveType::SCurve);

            // Lane 1: single point with Step curve.
            const auto pts1 = lanes[1].curve.getPoints();
            expectEquals(static_cast<int>(pts1.size()), 1);
            expect(pts1[0].curve == AutomationPoint::CurveType::Step);

            file.deleteFile();
        }

        beginTest("Importing an old (chain-only) file clears the manager");
        {
            // Hand-craft a "v0" preset file with no automation block.
            auto* root = new juce::DynamicObject();
            root->setProperty("version", 1);
            root->setProperty("plugins", juce::Array<juce::var>{});
            const auto json = juce::JSON::toString(juce::var(root), true);

            auto file = makeTempFile("legacy");
            expect(file.replaceWithText(json));

            // Pre-populate the destination manager — the import should clear it.
            PluginChain chain;
            AutomationManager mgr;
            seedManager(mgr);
            expectEquals(mgr.getNumLanes(), 2);

            expect(PluginPresetManager::importPreset(chain, mgr, file));
            expectEquals(mgr.getNumLanes(), 0,
                         "load with no automation block clears the manager");

            file.deleteFile();
        }

        beginTest("Importing a new file with chain-only API ignores automation");
        {
            // Save a preset WITH automation.
            PluginChain chainOut;
            AutomationManager mgrOut;
            seedManager(mgrOut);
            auto file = makeTempFile("chainonly_compat");
            expect(PluginPresetManager::exportPreset(chainOut, mgrOut, file));

            // Load through the OLD chain-only API. This must succeed
            // and not require an AutomationManager.
            PluginChain chainIn;
            expect(PluginPresetManager::importPreset(chainIn, file),
                   "chain-only API ignores the automation block cleanly");

            file.deleteFile();
        }

        beginTest("File-not-found returns false (automation-aware path)");
        {
            PluginChain chain;
            AutomationManager mgr;
            juce::File missing(juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("WaveEditTest_doesnotexist.wepchain"));
            missing.deleteFile();  // just in case
            expect(! PluginPresetManager::importPreset(chain, mgr, missing));
        }

        beginTest("Malformed JSON returns false (automation-aware path)");
        {
            PluginChain chain;
            AutomationManager mgr;
            auto file = makeTempFile("malformed");
            file.replaceWithText("{ this is not valid json");
            expect(! PluginPresetManager::importPreset(chain, mgr, file));
            file.deleteFile();
        }

        beginTest("savePreset/loadPreset (named) round-trip via Application Support");
        {
            // savePreset writes to the user's Application Support
            // directory. We use an obviously-test-only name and clean
            // up after.
            const juce::String name = "WaveEditTest_AutomationRoundTrip";
            // Pre-clean any leftover from a prior run.
            PluginPresetManager::deletePreset(name);

            PluginChain chainOut;
            AutomationManager mgrOut;
            seedManager(mgrOut);
            expect(PluginPresetManager::savePreset(chainOut, mgrOut, name));
            expect(PluginPresetManager::presetExists(name));

            PluginChain chainIn;
            AutomationManager mgrIn;
            expect(PluginPresetManager::loadPreset(chainIn, mgrIn, name));
            expectEquals(mgrIn.getNumLanes(), 2);

            // Cleanup.
            expect(PluginPresetManager::deletePreset(name));
        }
    }
};

static PluginPresetManagerTests pluginPresetManagerTests;
