/*
  ==============================================================================

    PluginBlacklistTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Regression tests for PluginManager::isBlacklisted (M21).

    Before the fix, isBlacklisted used substring containment in BOTH
    directions:
        fileOrIdentifier.contains(entry) || entry.contains(fileOrIdentifier)
    which meant blacklisting one plugin path also blacklisted every sibling
    in the same directory and every plugin whose path shared a manufacturer
    substring. The fix matches by exact identifier/path equality only.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "Plugins/PluginManager.h"

// ============================================================================
// PluginManager Blacklist Tests
// ============================================================================

class PluginBlacklistTests : public juce::UnitTest
{
public:
    PluginBlacklistTests() : juce::UnitTest("PluginManager Blacklist", "Plugins") {}

    void runTest() override
    {
        beginTest("Exact blacklist match");
        testExactMatch();

        beginTest("Sibling in same directory is NOT blacklisted");
        testSiblingNotBlacklisted();

        beginTest("Manufacturer-substring path is NOT blacklisted");
        testManufacturerSubstringNotBlacklisted();

        beginTest("Parent directory is NOT blacklisted by a child entry");
        testParentDirNotBlacklisted();
    }

private:
    // Each test owns its blacklist state: clear before, blacklist what it
    // needs, then clear after so tests do not interfere with one another or
    // with the user's real blacklist file content during the run.
    void resetBlacklist(PluginManager& pm)
    {
        pm.clearBlacklist();
    }

    void testExactMatch()
    {
        auto& pm = PluginManager::getInstance();
        resetBlacklist(pm);

        const juce::String pathA = "/Library/Audio/Plug-Ins/VST3/AcmeReverb.vst3";
        pm.addToBlacklist(pathA);

        expect(pm.isBlacklisted(pathA), "Exact path must be reported blacklisted");

        resetBlacklist(pm);
    }

    void testSiblingNotBlacklisted()
    {
        auto& pm = PluginManager::getInstance();
        resetBlacklist(pm);

        const juce::String badPlugin  = "/Library/Audio/Plug-Ins/VST3/AcmeReverb.vst3";
        const juce::String goodPlugin = "/Library/Audio/Plug-Ins/VST3/AcmeDelay.vst3";

        pm.addToBlacklist(badPlugin);

        expect(pm.isBlacklisted(badPlugin), "Blacklisted plugin should match");
        // Failing-before / passing-after: under the old contains() logic the
        // shared directory prefix made the sibling match too.
        expect(!pm.isBlacklisted(goodPlugin),
               "A sibling plugin in the same directory must NOT be blacklisted");

        resetBlacklist(pm);
    }

    void testManufacturerSubstringNotBlacklisted()
    {
        auto& pm = PluginManager::getInstance();
        resetBlacklist(pm);

        // Blacklist a short manufacturer-ish token. Old code: entry.contains()
        // / contains(entry) would catch any path mentioning "Acme".
        const juce::String shortEntry = "Acme";
        const juce::String unrelated  = "/Library/Audio/Plug-Ins/VST3/AcmeWidget.vst3";

        pm.addToBlacklist(shortEntry);

        expect(pm.isBlacklisted(shortEntry), "Exact entry should match itself");
        expect(!pm.isBlacklisted(unrelated),
               "A path merely containing the manufacturer substring must NOT match");

        resetBlacklist(pm);
    }

    void testParentDirNotBlacklisted()
    {
        auto& pm = PluginManager::getInstance();
        resetBlacklist(pm);

        const juce::String child = "/Library/Audio/Plug-Ins/VST3/AcmeReverb.vst3";
        const juce::String parentDir = "/Library/Audio/Plug-Ins/VST3";

        pm.addToBlacklist(child);

        // Old code: parentDir.contains(...) is false, but child.contains(parentDir)
        // was TRUE, so querying the parent directory could be reported blacklisted.
        expect(!pm.isBlacklisted(parentDir),
               "A parent directory of a blacklisted plugin must NOT be blacklisted");

        resetBlacklist(pm);
    }
};

static PluginBlacklistTests pluginBlacklistTests;
