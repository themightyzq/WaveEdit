/*
  ==============================================================================

    KeymapCommandResolutionTests.cpp
    WaveEdit - Professional Audio Editor

    Static cross-file consistency checks for the command/keymap system
    (REVIEW-QA H27). These guard three invariants that, when broken, silently
    drop keyboard shortcuts:

      (a) Every command name referenced in Default.json resolves to a real
          command ID (i.e. it appears in KeymapManager's forward name map AND
          its enum id is listed in the reverse-map vector).
      (b) Every command registered in CommandHandler::getAllCommands() has a
          name-map entry in KeymapManager (so getKeyPress() can find its
          shortcut).
      (c) No two commands in Default.json share the same key+modifier combo.

    The test parses the authoritative source files directly rather than
    instantiating KeymapManager/CommandHandler (which need a live
    ApplicationCommandManager / MainComponent). The repo source root is
    derived from the WAVEEDIT_TEMPLATES_DIR compile definition
    (<root>/Templates/Keymaps), so no new CMake wiring is required.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#ifndef WAVEEDIT_TEMPLATES_DIR
    #define WAVEEDIT_TEMPLATES_DIR ""
#endif

namespace
{
    juce::File repoRoot()
    {
        // WAVEEDIT_TEMPLATES_DIR == <root>/Templates/Keymaps
        return juce::File(juce::String(WAVEEDIT_TEMPLATES_DIR))
                   .getParentDirectory()    // Templates
                   .getParentDirectory();   // <root>
    }

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

    // Extract the forward name-map entries:
    //   commandNameMap[CommandIDs::<enumId>] = "<name>";
    // Returns enumId -> name.
    std::map<juce::String, juce::String> parseForwardNameMap(const juce::String& src)
    {
        std::map<juce::String, juce::String> out;
        // Regex-free scan: walk lines.
        juce::StringArray lines = juce::StringArray::fromLines(src);
        for (const auto& line : lines)
        {
            int idx = line.indexOf("commandNameMap[CommandIDs::");
            if (idx < 0) continue;
            juce::String rest = line.substring(idx + (int)juce::String("commandNameMap[CommandIDs::").length());
            int bracket = rest.indexOfChar(']');
            if (bracket < 0) continue;
            juce::String enumId = rest.substring(0, bracket).trim();
            int q1 = rest.indexOfChar(bracket, '"');
            if (q1 < 0) continue;
            int q2 = rest.indexOfChar(q1 + 1, '"');
            if (q2 < 0) continue;
            juce::String name = rest.substring(q1 + 1, q2);
            if (enumId.isNotEmpty() && name.isNotEmpty())
                out[enumId] = name;
        }
        return out;
    }

    // Extract the enum ids listed inside the reverse-map `allCommandIDs = { ... };`
    // vector. Returns the set of enum-id tokens.
    std::set<juce::String> parseReverseVectorIds(const juce::String& src)
    {
        std::set<juce::String> out;
        int start = src.indexOf("allCommandIDs");
        if (start < 0) return out;
        int brace = src.indexOfChar(start, '{');
        int end = src.indexOf(brace, "};");
        if (brace < 0 || end < 0) return out;
        juce::String block = src.substring(brace + 1, end);
        const juce::String marker = "CommandIDs::";
        int pos = 0;
        for (;;)
        {
            int idx = block.indexOf(pos, marker);
            if (idx < 0) break;
            int s = idx + marker.length();
            int e = s;
            while (e < block.length()
                   && (juce::CharacterFunctions::isLetterOrDigit(block[e]) || block[e] == '_'))
                ++e;
            out.insert(block.substring(s, e));
            pos = e;
        }
        return out;
    }

    // Generic helper: collect every `CommandIDs::<token>` inside [from, to).
    void collectCommandIds(const juce::String& block, std::set<juce::String>& out)
    {
        const juce::String marker = "CommandIDs::";
        int pos = 0;
        for (;;)
        {
            int idx = block.indexOf(pos, marker);
            if (idx < 0) break;
            int s = idx + marker.length();
            int e = s;
            while (e < block.length()
                   && (juce::CharacterFunctions::isLetterOrDigit(block[e]) || block[e] == '_'))
                ++e;
            out.insert(block.substring(s, e));
            pos = e;
        }
    }

    // Extract the enum ids registered in getAllCommands()'s `ids[] = { ... };`
    // array. Returns the set of enum-id tokens.
    std::set<juce::String> parseGetAllCommands(const juce::String& src)
    {
        std::set<juce::String> out;
        int fn = src.indexOf("getAllCommands");
        if (fn < 0) return out;
        int arr = src.indexOf(fn, "ids[]");
        if (arr < 0) return out;
        int brace = src.indexOfChar(arr, '{');
        int end = src.indexOf(brace, "};");
        if (brace < 0 || end < 0) return out;
        collectCommandIds(src.substring(brace + 1, end), out);
        return out;
    }

    // From CommandHandler_GetInfo.cpp, find the set of command IDs whose
    // getCommandInfo() case requests the keymap-derived shortcut (i.e. calls
    // addDefaultKeypress(keyPress, ...)). These are the commands that MUST have
    // a KeymapManager name-map entry, otherwise getKeyPress() returns empty and
    // no shortcut ever binds (the H27 defect class). Menu-only / radio-item
    // commands that never call addDefaultKeypress(keyPress...) are excluded.
    std::set<juce::String> parseKeymapBoundCommands(const juce::String& src)
    {
        std::set<juce::String> out;
        const juce::String caseMarker = "case CommandIDs::";
        int pos = 0;
        for (;;)
        {
            int idx = src.indexOf(pos, caseMarker);
            if (idx < 0) break;
            int s = idx + caseMarker.length();
            int e = s;
            while (e < src.length()
                   && (juce::CharacterFunctions::isLetterOrDigit(src[e]) || src[e] == '_'))
                ++e;
            const juce::String name = src.substring(s, e);

            // Body runs until the next case label (or end of file).
            int nextCase = src.indexOf(e, caseMarker);
            const juce::String body = (nextCase < 0) ? src.substring(e)
                                                     : src.substring(e, nextCase);
            if (body.contains("addDefaultKeypress(keyPress"))
                out.insert(name);

            pos = e;
        }
        return out;
    }
}

class KeymapCommandResolutionTests : public juce::UnitTest
{
public:
    KeymapCommandResolutionTests()
        : juce::UnitTest("Keymap Command Resolution", "Unit") {}

    void runTest() override
    {
        const juce::File root = repoRoot();

        beginTest("Source files are reachable");
        const juce::File defaultJson = root.getChildFile("Templates/Keymaps/Default.json");
        const juce::File keymapCpp   = root.getChildFile("Source/Utils/KeymapManager.cpp");
        const juce::File handlerCpp  = root.getChildFile("Source/Commands/CommandHandler.cpp");
        const juce::File getInfoCpp  = root.getChildFile("Source/Commands/CommandHandler_GetInfo.cpp");
        expect(defaultJson.existsAsFile(), "Missing: " + defaultJson.getFullPathName());
        expect(keymapCpp.existsAsFile(),   "Missing: " + keymapCpp.getFullPathName());
        expect(handlerCpp.existsAsFile(),  "Missing: " + handlerCpp.getFullPathName());
        expect(getInfoCpp.existsAsFile(),  "Missing: " + getInfoCpp.getFullPathName());
        if (! (defaultJson.existsAsFile() && keymapCpp.existsAsFile()
               && handlerCpp.existsAsFile() && getInfoCpp.existsAsFile()))
            return;

        const juce::String keymapSrc  = keymapCpp.loadFileAsString();
        const juce::String handlerSrc = handlerCpp.loadFileAsString();
        const juce::String getInfoSrc = getInfoCpp.loadFileAsString();

        // enumId -> name (forward map), and the set of names that getCommandID
        // can actually resolve (forward-mapped AND present in the reverse vector).
        const auto forwardMap     = parseForwardNameMap(keymapSrc);
        const auto reverseIds     = parseReverseVectorIds(keymapSrc);
        const auto registered     = parseGetAllCommands(handlerSrc);
        const auto keymapBound    = parseKeymapBoundCommands(getInfoSrc);

        expect(! forwardMap.empty(),  "Failed to parse KeymapManager forward name map");
        expect(! reverseIds.empty(),  "Failed to parse KeymapManager reverse-map vector");
        expect(! registered.empty(),  "Failed to parse getAllCommands()");
        expect(! keymapBound.empty(), "Failed to parse keymap-bound commands");

        // name -> resolvable?  (mirror of getCommandID(): reverseMap is built from
        // getCommandName(id) for each id in allCommandIDs)
        std::set<juce::String> resolvableNames;
        for (const auto& pair : forwardMap)
            if (reverseIds.count(pair.first) > 0)
                resolvableNames.insert(pair.second);

        // enumId -> name lookup for the registered-command assertion.
        std::map<juce::String, juce::String> idToName = forwardMap;

        //----------------------------------------------------------------------
        // (a) Every Default.json command name resolves to a real command ID.
        beginTest("Every Default.json command name resolves to a command ID");
        const juce::var parsed = juce::JSON::parse(defaultJson);
        auto* obj = parsed.getDynamicObject();
        expect(obj != nullptr, "Default.json did not parse to an object");
        if (obj == nullptr) return;

        const juce::var shortcuts = obj->getProperty("shortcuts");
        auto* shortcutsObj = shortcuts.getDynamicObject();
        expect(shortcutsObj != nullptr, "Default.json has no 'shortcuts' object");
        if (shortcutsObj == nullptr) return;

        const auto& props = shortcutsObj->getProperties();
        int unresolved = 0;
        for (int i = 0; i < props.size(); ++i)
        {
            const juce::String name = props.getName(i).toString();
            if (name.startsWith("_comment")) continue;
            if (resolvableNames.count(name) == 0)
            {
                ++unresolved;
                logMessage("  UNRESOLVED Default.json name: " + name);
            }
        }
        expectEquals(unresolved, 0, "Default.json names with no resolvable command ID");

        //----------------------------------------------------------------------
        // (b) Every registered command that requests a keymap shortcut has a
        // KeymapManager name-map entry. (Menu-only / radio-item commands that
        // never call addDefaultKeypress(keyPress...) are intentionally excluded:
        // they have no keyboard binding by design, e.g. the spectrum FFT-size
        // and window-function radio items.)
        beginTest("Every keymap-bound registered command has a name-map entry");
        int missingFromMap = 0;
        for (const auto& enumId : keymapBound)
        {
            // Only assert on commands that are actually registered.
            if (registered.count(enumId) == 0) continue;
            if (idToName.find(enumId) == idToName.end())
            {
                ++missingFromMap;
                logMessage("  KEYMAP-BOUND command missing from name map: CommandIDs::" + enumId);
            }
        }
        expectEquals(missingFromMap, 0,
                     "Keymap-bound commands absent from KeymapManager name maps");

        //----------------------------------------------------------------------
        // (b2) Symmetry: every forward-mapped name is also in the reverse vector,
        // otherwise getCommandID() silently fails to resolve it.
        beginTest("Forward name map and reverse-map vector are in sync");
        int notInReverse = 0;
        for (const auto& pair : forwardMap)
        {
            if (reverseIds.count(pair.first) == 0)
            {
                ++notInReverse;
                logMessage("  Forward-mapped but missing from reverse vector: CommandIDs::"
                           + pair.first);
            }
        }
        expectEquals(notInReverse, 0, "Forward name map / reverse vector mismatch");

        //----------------------------------------------------------------------
        // (c) No two Default.json commands share the same key+modifier combo.
        beginTest("Default.json has no duplicate key bindings");
        std::map<juce::String, juce::StringArray> comboToCommands;
        for (int i = 0; i < props.size(); ++i)
        {
            const juce::String name = props.getName(i).toString();
            const juce::var& binding = props.getValueAt(i);
            auto* bindObj = binding.getDynamicObject();
            if (bindObj == nullptr) continue;
            const juce::String key = bindObj->getProperty("key").toString();
            if (key.isEmpty()) continue;
            const juce::String mods = normaliseModifiers(bindObj->getProperty("modifiers"));
            comboToCommands[mods + ":" + key].add(name);
        }
        int conflicts = 0;
        for (const auto& pair : comboToCommands)
        {
            if (pair.second.size() > 1)
            {
                ++conflicts;
                logMessage("  CONFLICT [" + pair.first + "]: "
                           + pair.second.joinIntoString(", "));
            }
        }
        expectEquals(conflicts, 0, "Duplicate key bindings in Default.json");

        //----------------------------------------------------------------------
        // (d) Regression guard for H27: the 15 previously-unmapped commands all
        // resolve now. (The renamed processChannelConverter must be gone.)
        beginTest("H27: previously-unmapped commands resolve");
        const juce::StringArray h27 = {
            "toolsChannelConverter", "toolsChannelExtractor", "toolsHeadTail",
            "toolsLoopingTools", "processReverse", "processInvert", "processResample",
            "processTimeStretch", "processPitchShift", "markerConvertToRegions",
            "regionConvertToMarkers", "pluginOffline", "toolbarCustomize",
            "toolbarReset", "fileBatchProcessor"
        };
        for (const auto& name : h27)
            expect(resolvableNames.count(name) > 0,
                   "H27 command does not resolve: " + name);

        expect(resolvableNames.count("processChannelConverter") == 0,
               "Stale 'processChannelConverter' name should no longer resolve");
    }
};

static KeymapCommandResolutionTests keymapCommandResolutionTests;
