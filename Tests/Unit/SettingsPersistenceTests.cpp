/*
  ==============================================================================

    SettingsPersistenceTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Regression tests for UX finding 1: Settings::save() used to serialize only
    version + recentFiles, so every setSetting() key was silently dropped on
    relaunch. These tests exercise the generic round-trip: set representative
    keys of each var type, persist to a temp file, load into a fresh instance,
    and verify everything survives -- plus old-format (version + recentFiles
    only) load compatibility.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "../../Source/Utils/Settings.h"

//==============================================================================
class SettingsPersistenceTests : public juce::UnitTest
{
public:
    SettingsPersistenceTests()
        : juce::UnitTest("Settings Persistence", "Unit") {}

    void runTest() override
    {
        testAllVarTypesRoundTrip();
        testLeafAndParentNodeCoexist();
        testVersionAndRecentsSurvive();
        testOldFormatLoadCompatibility();
        testDefaultsWhenKeyAbsent();
    }

private:
    //==========================================================================
    // Each call gets its own temp settings file + parent dir, cleaned up on
    // scope exit. Never touches the shared per-user settings location.
    struct ScopedSettingsFile
    {
        juce::File dir;
        juce::File file;

        ScopedSettingsFile()
        {
            dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                      .getChildFile("WaveEditSettingsTest_"
                                    + juce::Uuid().toString());
            dir.createDirectory();
            file = dir.getChildFile("settings.json");
        }

        ~ScopedSettingsFile() { dir.deleteRecursively(); }
    };

    // Create a real (empty) file so addRecentFile()'s existsAsFile() guard
    // accepts it. Returns the created file.
    juce::File makeRealFile(const juce::File& dir, const juce::String& name)
    {
        auto f = dir.getChildFile(name);
        f.replaceWithText("x");
        return f;
    }

    //==========================================================================
    void testAllVarTypesRoundTrip()
    {
        beginTest("All var types survive save -> load into a fresh instance");

        ScopedSettingsFile tmp;

        {
            Settings s(tmp.file);
            s.setSetting("display.theme", juce::var("high-contrast"));   // String
            s.setSetting("autoSave.enabled", juce::var(false));          // bool (non-default)
            s.setSetting("autoSave.intervalMinutes", juce::var(2));      // int
            s.setSetting("dsp.previewGain", juce::var(3.5));             // double
            s.save();
        }

        Settings loaded(tmp.file);
        expectEquals(loaded.getSetting("display.theme").toString(),
                     juce::String("high-contrast"), "String key survives");
        expect(static_cast<bool>(loaded.getSetting("autoSave.enabled", true)) == false,
               "bool key survives (and keeps false, not the default true)");
        expectEquals(static_cast<int>(loaded.getSetting("autoSave.intervalMinutes", 5)),
                     2, "int key survives");
        expect(std::abs(static_cast<double>(loaded.getSetting("dsp.previewGain", 0.0)) - 3.5) < 1.0e-9,
               "double key survives");
    }

    //==========================================================================
    // Regression for the real collision case: "display.waveformColor" is a
    // leaf value AND the parent of "display.waveformColor.ch0". Both must
    // round-trip independently.
    void testLeafAndParentNodeCoexist()
    {
        beginTest("A node that is both a leaf value and a parent round-trips");

        ScopedSettingsFile tmp;

        {
            Settings s(tmp.file);
            s.setSetting("display.waveformColor", juce::var("ffabcdef"));
            s.setSetting("display.waveformColor.ch0", juce::var("ff112233"));
            s.setSetting("display.waveformColor.ch1", juce::var("ff445566"));
            s.save();
        }

        Settings loaded(tmp.file);
        expectEquals(loaded.getSetting("display.waveformColor").toString(),
                     juce::String("ffabcdef"), "Parent-node leaf value survives");
        expectEquals(loaded.getSetting("display.waveformColor.ch0").toString(),
                     juce::String("ff112233"), "Child ch0 survives");
        expectEquals(loaded.getSetting("display.waveformColor.ch1").toString(),
                     juce::String("ff445566"), "Child ch1 survives");
    }

    //==========================================================================
    void testVersionAndRecentsSurvive()
    {
        beginTest("version field and recent files survive the round-trip");

        ScopedSettingsFile tmp;
        auto a = makeRealFile(tmp.dir, "one.wav");
        auto b = makeRealFile(tmp.dir, "two.wav");

        {
            Settings s(tmp.file);
            s.addRecentFile(a);   // persists on its own
            s.addRecentFile(b);   // b is now most-recent
            s.setSetting("display.theme", juce::var("light"));
            s.save();
        }

        Settings loaded(tmp.file);

        // version is written as a top-level field, default "1.0".
        expectEquals(loaded.getSettingsFile().getFullPathName(),
                     tmp.file.getFullPathName(), "Bound to the temp file");

        auto recents = loaded.getRecentFiles();
        expectEquals(recents.size(), 2, "Both recent files survive");
        expectEquals(recents[0], b.getFullPathName(), "Most-recent ordering preserved");
        expectEquals(recents[1], a.getFullPathName(), "Second recent preserved");
        expectEquals(loaded.getSetting("display.theme").toString(),
                     juce::String("light"), "Setting persists alongside recents");
    }

    //==========================================================================
    // An old settings.json carrying only version + recentFiles must still load
    // (backward compatibility). Missing sections are simply absent.
    void testOldFormatLoadCompatibility()
    {
        beginTest("Old-format (version + recentFiles only) file loads");

        ScopedSettingsFile tmp;
        auto a = makeRealFile(tmp.dir, "legacy.wav");

        // Hand-write an old-format file exactly as pre-fix builds wrote it.
        auto obj = std::make_unique<juce::DynamicObject>();
        obj->setProperty("version", "1.0");
        juce::Array<juce::var> recents;
        recents.add(a.getFullPathName());
        obj->setProperty("recentFiles", recents);
        tmp.file.replaceWithText(juce::JSON::toString(juce::var(obj.release()), true));

        Settings loaded(tmp.file);

        auto r = loaded.getRecentFiles();
        expectEquals(r.size(), 1, "Legacy recent file loads");
        expectEquals(r[0], a.getFullPathName(), "Legacy recent path preserved");

        // No settings section -> getSetting returns caller defaults.
        expectEquals(loaded.getSetting("display.theme", "dark").toString(),
                     juce::String("dark"), "Absent key falls back to default");
    }

    //==========================================================================
    void testDefaultsWhenKeyAbsent()
    {
        beginTest("Absent keys return the caller-supplied default");

        ScopedSettingsFile tmp;
        Settings s(tmp.file);

        expect(static_cast<bool>(s.getSetting("autoSave.enabled", true)) == true,
               "Unset bool returns default true");
        expectEquals(static_cast<int>(s.getSetting("display.timeFormat", 2)),
                     2, "Unset int returns default 2");
        expectEquals(s.getSetting("currentKeymap", "Default").toString(),
                     juce::String("Default"), "Unset String returns default");
    }
};

static SettingsPersistenceTests settingsPersistenceTests;
