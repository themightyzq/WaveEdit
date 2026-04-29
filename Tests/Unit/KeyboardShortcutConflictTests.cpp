/*
  ==============================================================================

    KeyboardShortcutConflictTests.cpp
    WaveEdit - Professional Audio Editor

    Loads each keymap template under Templates/Keymaps/ and verifies that no
    two commands share the same key+modifier combination. Operates on the
    JSON directly so it does not depend on a fully-instantiated
    ApplicationCommandManager.

    Test file location is provided via the WAVEEDIT_TEMPLATES_DIR compile
    definition set by CMakeLists.txt.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#ifndef WAVEEDIT_TEMPLATES_DIR
    #define WAVEEDIT_TEMPLATES_DIR ""
#endif

namespace
{
    juce::String normaliseModifiers(const juce::var& mods)
    {
        if (auto* arr = mods.getArray())
        {
            juce::StringArray flags;
            for (const auto& m : *arr)
                flags.add(m.toString().toLowerCase());
            flags.sort(true);
            return flags.joinIntoString("+");
        }
        return {};
    }
}

class KeymapConflictTests : public juce::UnitTest
{
public:
    KeymapConflictTests() : juce::UnitTest("Keymap Conflict Detection", "Unit") {}

    void runTest() override
    {
        beginTest("Templates directory is configured");
        const juce::String templatesPath = WAVEEDIT_TEMPLATES_DIR;
        expect(templatesPath.isNotEmpty(),
               "WAVEEDIT_TEMPLATES_DIR compile definition not set");

        const juce::File templatesDir(templatesPath);
        expect(templatesDir.isDirectory(),
               "Templates directory missing: " + templatesDir.getFullPathName());

        beginTest("At least one keymap template ships");
        juce::Array<juce::File> jsonFiles;
        templatesDir.findChildFiles(jsonFiles, juce::File::findFiles, false, "*.json");
        expect(jsonFiles.size() > 0, "No keymap JSON files found");

        for (const auto& jsonFile : jsonFiles)
            checkSingleTemplate(jsonFile);
    }

private:
    void checkSingleTemplate(const juce::File& jsonFile)
    {
        beginTest("Template has no key conflicts: " + jsonFile.getFileName());

        const auto parsed = juce::JSON::parse(jsonFile);
        if (! parsed.isObject())
        {
            expect(false, "Failed to parse JSON: " + jsonFile.getFileName());
            return;
        }

        const auto* obj = parsed.getDynamicObject();
        if (obj == nullptr)
        {
            expect(false, "Parsed JSON has no object: " + jsonFile.getFileName());
            return;
        }

        const juce::var shortcuts = obj->getProperty("shortcuts");
        if (! shortcuts.isObject())
        {
            // Some templates may legitimately ship empty; skip without failing.
            logMessage("  no 'shortcuts' object — skipping " + jsonFile.getFileName());
            return;
        }

        std::map<juce::String, juce::StringArray> keyToCommands;
        const auto* shortcutsObj = shortcuts.getDynamicObject();
        const auto& props = shortcutsObj->getProperties();

        for (int i = 0; i < props.size(); ++i)
        {
            const juce::String commandName = props.getName(i).toString();
            const juce::var& binding = props.getValueAt(i);
            if (! binding.isObject()) continue;

            const auto* bindObj = binding.getDynamicObject();
            const juce::String key = bindObj->getProperty("key").toString();
            const juce::String mods = normaliseModifiers(bindObj->getProperty("modifiers"));
            if (key.isEmpty()) continue;

            const juce::String combo = mods + ":" + key;
            keyToCommands[combo].add(commandName);
        }

        int conflictCount = 0;
        for (const auto& pair : keyToCommands)
        {
            if (pair.second.size() > 1)
            {
                ++conflictCount;
                logMessage("  CONFLICT [" + pair.first + "]: "
                           + pair.second.joinIntoString(", "));
            }
        }

        expectEquals(conflictCount, 0,
                     "Conflicts in " + jsonFile.getFileName());
    }
};

static KeymapConflictTests keymapConflictTests;
