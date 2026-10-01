/*
  ==============================================================================

    KeymapPersistenceTests.cpp
    Copyright (C) 2026 ZQ SFX

    Regression tests for keyboard-template persistence.

    The bug these guard against: MainComponent read a settings key that nothing ever wrote
    ("keyboard.activeTemplate"), fell back to "Default", and called loadTemplate("Default"),
    which also saved "Default" over the user's real choice ("currentKeymap"). The keymap
    reverted to Default on every launch.

    Everything here runs against a temp settings file and temp template folder through
    KeymapManager's test constructor, so the user's real settings and keymaps are never
    read or written.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Source/Utils/Settings.h"
#include "../../Source/Utils/KeymapManager.h"

class KeymapPersistenceTests : public juce::UnitTest
{
public:
    KeymapPersistenceTests() : juce::UnitTest("Keymap Persistence", "Unit") {}

    void runTest() override
    {
        testChosenTemplateSurvivesRestart();
        testLegacyTemplateIdResolves();
        testUnknownTemplateFallsBackToDefault();
        testNoDeadKeymapSettingKey();
    }

private:
    struct ScopedEnv
    {
        juce::File dir;
        juce::File settingsFile;
        juce::File userTemplates;

        ScopedEnv()
        {
            dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                      .getChildFile("WaveEditKeymapTest_" + juce::Uuid().toString());
            dir.createDirectory();
            settingsFile  = dir.getChildFile("settings.json");
            userTemplates = dir.getChildFile("Keymaps");
        }

        ~ScopedEnv() { dir.deleteRecursively(); }
    };

    static juce::File bundledTemplates() { return juce::File(juce::String(WAVEEDIT_TEMPLATES_DIR)); }

    static juce::File repoRoot()
    {
        return bundledTemplates().getParentDirectory().getParentDirectory();
    }

    //==========================================================================
    void testChosenTemplateSurvivesRestart()
    {
        beginTest("A chosen template survives a fresh KeymapManager on the same settings file");

        ScopedEnv env;
        juce::ApplicationCommandManager commands;

        {
            Settings settings(env.settingsFile);
            KeymapManager first(commands, settings, bundledTemplates(), env.userTemplates);

            expectEquals(first.getCurrentTemplateName(), juce::String("Default"),
                         "A fresh settings file starts on Default");
            expect(first.templateExists("Classic Editor"), "Classic Editor template is bundled");
            expect(first.loadTemplate("Classic Editor"), "Switching to Classic Editor succeeds");
            expectEquals(settings.getSetting("currentKeymap", "").toString(),
                         juce::String("Classic Editor"), "The choice is written to settings");
        }

        // A new Settings object reads the file from disk: this is what a relaunch sees.
        Settings reloaded(env.settingsFile);
        KeymapManager second(commands, reloaded, bundledTemplates(), env.userTemplates);

        expectEquals(second.getCurrentTemplateName(), juce::String("Classic Editor"),
                     "The chosen template is active after a restart");
        expectEquals(reloaded.getSetting("currentKeymap", "").toString(),
                     juce::String("Classic Editor"), "Loading does not overwrite the saved choice");
    }

    //==========================================================================
    void testLegacyTemplateIdResolves()
    {
        beginTest("A settings file naming an old template id resolves to its current name");

        ScopedEnv env;
        juce::ApplicationCommandManager commands;

        {
            Settings seed(env.settingsFile);
            seed.setSetting("currentKeymap", "Sound Forge");
        }

        Settings settings(env.settingsFile);
        KeymapManager manager(commands, settings, bundledTemplates(), env.userTemplates);
        expectEquals(manager.getCurrentTemplateName(), juce::String("Classic Editor"),
                     "Old id maps to Classic Editor");
    }

    //==========================================================================
    void testUnknownTemplateFallsBackToDefault()
    {
        beginTest("An unknown saved template falls back to Default");

        ScopedEnv env;
        juce::ApplicationCommandManager commands;

        {
            Settings seed(env.settingsFile);
            seed.setSetting("currentKeymap", "No Such Template");
        }

        Settings settings(env.settingsFile);
        KeymapManager manager(commands, settings, bundledTemplates(), env.userTemplates);
        expectEquals(manager.getCurrentTemplateName(), juce::String("Default"),
                     "Unknown template falls back to Default");
    }

    //==========================================================================
    void testNoDeadKeymapSettingKey()
    {
        beginTest("No source file reads a keymap setting other than through KeymapManager");

        // The dead key, assembled so this file does not match its own search.
        const juce::String deadKey = juce::String("keyboard") + "." + "activeTemplate";

        const auto sourceDir = repoRoot().getChildFile("Source");
        expect(sourceDir.isDirectory(), "Source directory reachable: " + sourceDir.getFullPathName());

        int hits = 0;
        for (const auto& f : sourceDir.findChildFiles(juce::File::findFiles, true, "*.h;*.cpp"))
        {
            if (f.loadFileAsString().contains(deadKey))
            {
                ++hits;
                logMessage("  reads the dead key: " + f.getFullPathName());
            }
        }
        expectEquals(hits, 0, "Nothing reads the never-written keymap key");
    }
};

static KeymapPersistenceTests keymapPersistenceTests;
