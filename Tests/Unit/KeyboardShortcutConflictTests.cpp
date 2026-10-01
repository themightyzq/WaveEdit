/*
  ==============================================================================

    KeyboardShortcutConflictTests.cpp
    WaveEdit - Professional Audio Editor

    Loads each keymap template under Templates/Keymaps/ and verifies that
      (1) no two commands share the same key+modifier combination, and
      (2) no command binds a macOS system-reserved combo (Cmd+Space =
          Spotlight, Cmd+Tab / Cmd+Shift+Tab = App Switcher) that the OS
          would swallow before the app ever saw it (REVIEW-UX finding 7).
    Operates on the JSON directly so it does not depend on a
    fully-instantiated ApplicationCommandManager.

    Test file location is provided via the WAVEEDIT_TEMPLATES_DIR compile
    definition set by CMakeLists.txt.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <map>
#include <set>

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

    // Build the canonical "mods:key" combo string used for both duplicate and
    // reserved-key comparison. `mods` is normaliseModifiers()'s sorted output.
    juce::String comboString(const juce::String& normalisedMods, const juce::String& key)
    {
        return normalisedMods + ":" + key;
    }

    // macOS system-reserved combos, expressed in the same normalised
    // "sorted-lowercase-mods:key" form. These are intercepted by the OS
    // (Spotlight / App Switcher) and must never be bound by a template.
    // Note: fileExit's Cmd+Q (Quit) is a legitimate app command, not listed.
    const std::set<juce::String>& systemReservedCombos()
    {
        static const std::set<juce::String> reserved = {
            "cmd:Space",      // Spotlight
            "cmd:Tab",        // App Switcher (forward)
            "cmd+shift:Tab"   // App Switcher (reverse)
        };
        return reserved;
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

            const juce::String combo = comboString(mods, key);
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

        // (2) No binding may claim a macOS system-reserved combo.
        beginTest("Template avoids system-reserved keys: " + jsonFile.getFileName());
        int reservedHits = 0;
        for (const auto& pair : keyToCommands)
        {
            if (systemReservedCombos().count(pair.first) > 0)
            {
                ++reservedHits;
                logMessage("  SYSTEM-RESERVED [" + pair.first + "]: "
                           + pair.second.joinIntoString(", "));
            }
        }
        expectEquals(reservedHits, 0,
                     "System-reserved bindings in " + jsonFile.getFileName());
    }
};

static KeymapConflictTests keymapConflictTests;
