/*
  ==============================================================================

    PluginChainPersistenceTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Closes the "plugin chain state persistence" gap noted in TODO.md.

    PluginPresetManagerTests.cpp already covers the automation-bundle
    behaviour of PluginPresetManager's file-based export/import API. It does
    NOT cover:
      - PluginChain::saveToXml()/loadFromXml() directly (the XML format is
        untested anywhere else in the suite).
      - PluginChain::saveToJson()/loadFromJson() directly (only exercised
        indirectly through PluginPresetManager).
      - The chain-only (no AutomationManager) named-preset API
        (savePreset/loadPreset via the Application Support directory),
        which PluginPresetManagerTests only exercises for the
        automation-bundle overload.
      - getAvailablePresets() list semantics.

    Like PluginPresetManagerTests, these tests cannot instantiate a real
    VST3/AU plugin (none are scanned/registered in a headless test run), so
    PluginChain::loadFromXml()/loadFromJson() always take the "identifier not
    found -> skip" path (PluginManager::getPluginByIdentifier() returns
    nullopt). That is itself a real code path worth locking in: the chain
    must still return success and end up empty rather than crashing or
    partially mutating state.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Plugins/PluginChain.h"
#include "Plugins/PluginPresetManager.h"

//==============================================================================
// PluginChain direct serialization (saveToXml/loadFromXml, saveToJson/loadFromJson)
//==============================================================================
class PluginChainSerializationTests : public juce::UnitTest
{
public:
    PluginChainSerializationTests() : juce::UnitTest("PluginChain Serialization", "Plugins") {}

    void runTest() override
    {
        beginTest("Empty chain saveToXml() shape: tag, version, no children");
        {
            PluginChain chain;
            auto xml = chain.saveToXml();
            expect(xml != nullptr, "saveToXml should always return a valid element");
            expect(xml->getTagName() == "PluginChain", "root tag should be PluginChain");
            expectEquals(xml->getIntAttribute("version"), 1);
            expectEquals(xml->getNumChildElements(), 0, "empty chain should have no Plugin children");
        }

        beginTest("Empty chain XML round-trip: saveToXml -> loadFromXml");
        {
            PluginChain chainOut;
            auto xml = chainOut.saveToXml();

            PluginChain chainIn;
            expect(chainIn.loadFromXml(*xml), "loadFromXml should succeed for a well-formed empty chain");
            expectEquals(chainIn.getNumPlugins(), 0);
        }

        beginTest("loadFromXml rejects a document with the wrong root tag");
        {
            juce::XmlElement wrongXml("NotAPluginChain");
            PluginChain chain;
            expect(! chain.loadFromXml(wrongXml), "wrong root tag must be rejected");
            expectEquals(chain.getNumPlugins(), 0);
        }

        beginTest("loadFromXml skips an unresolved plugin identifier but still succeeds");
        {
            // No VST3/AU plugins are scanned in a headless test run, so
            // PluginManager::getPluginByIdentifier() will not resolve this.
            // The method's documented contract is "load what it can" -- it
            // must not fail the whole load or leave partial state.
            juce::XmlElement xml("PluginChain");
            xml.setAttribute("version", 1);
            auto* pluginXml = xml.createNewChildElement("Plugin");
            pluginXml->setAttribute("name", "Nonexistent Plugin");
            pluginXml->setAttribute("identifier", "VST3-Nonexistent-00000000");
            pluginXml->setAttribute("format", "VST3");
            pluginXml->setAttribute("bypassed", true);

            PluginChain chain;
            expect(chain.loadFromXml(xml), "unresolved identifiers should be skipped, not fail the load");
            expectEquals(chain.getNumPlugins(), 0, "unresolved plugin must not appear in the chain");
        }

        beginTest("Empty chain saveToJson() shape: version, empty plugins array");
        {
            PluginChain chain;
            auto json = chain.saveToJson();
            expect(json.isObject(), "saveToJson should produce a JSON object");

            auto* obj = json.getDynamicObject();
            expect(obj != nullptr);
            expectEquals(static_cast<int>(obj->getProperty("version")), 1);

            auto pluginsVar = obj->getProperty("plugins");
            expect(pluginsVar.isArray(), "plugins property should be an array");
            expectEquals(pluginsVar.getArray()->size(), 0);
        }

        beginTest("Empty chain JSON round-trip: saveToJson -> loadFromJson");
        {
            PluginChain chainOut;
            auto json = chainOut.saveToJson();

            PluginChain chainIn;
            expect(chainIn.loadFromJson(json), "loadFromJson should succeed for a well-formed empty chain");
            expectEquals(chainIn.getNumPlugins(), 0);
        }

        beginTest("loadFromJson rejects a non-object var");
        {
            PluginChain chain;
            juce::var notAnObject(42);
            expect(! chain.loadFromJson(notAnObject));
        }

        beginTest("loadFromJson rejects an object with no plugins property");
        {
            auto* root = new juce::DynamicObject();
            root->setProperty("version", 1);
            juce::var json(root);

            PluginChain chain;
            expect(! chain.loadFromJson(json), "missing plugins array must be rejected");
        }

        beginTest("loadFromJson rejects a plugins property that isn't an array");
        {
            auto* root = new juce::DynamicObject();
            root->setProperty("version", 1);
            root->setProperty("plugins", juce::var(5));
            juce::var json(root);

            PluginChain chain;
            expect(! chain.loadFromJson(json), "non-array plugins property must be rejected");
        }

        beginTest("loadFromJson skips an unresolved plugin identifier but still succeeds");
        {
            auto* pluginObj = new juce::DynamicObject();
            pluginObj->setProperty("name", "Nonexistent Plugin");
            pluginObj->setProperty("identifier", "VST3-Nonexistent-00000000");
            pluginObj->setProperty("bypassed", true);

            juce::Array<juce::var> pluginsArray;
            pluginsArray.add(juce::var(pluginObj));

            auto* root = new juce::DynamicObject();
            root->setProperty("version", 1);
            root->setProperty("plugins", pluginsArray);
            juce::var json(root);

            PluginChain chain;
            expect(chain.loadFromJson(json), "unresolved identifiers should be skipped, not fail the load");
            expectEquals(chain.getNumPlugins(), 0, "unresolved plugin must not appear in the chain");
        }
    }
};

static PluginChainSerializationTests pluginChainSerializationTests;

//==============================================================================
// PluginPresetManager named (Application Support) presets -- chain-only API
// (no AutomationManager). PluginPresetManagerTests.cpp only exercises the
// automation-bundle overload of savePreset/loadPreset for the named-preset
// path; this covers the plain chain-only overload plus list/delete/failure
// semantics that neither existing file touches.
//==============================================================================
class PluginPresetManagerNamedPresetTests : public juce::UnitTest
{
public:
    PluginPresetManagerNamedPresetTests()
        : juce::UnitTest("PluginPresetManager Named Chain-Only Presets", "Plugins") {}

    void runTest() override
    {
        beginTest("savePreset/loadPreset (chain-only, named) round trip via Application Support");
        {
            const juce::String name = "WaveEditTest_ChainPersistence_RoundTrip";
            PluginPresetManager::deletePreset(name);  // pre-clean from a prior failed run

            PluginChain chainOut;
            expect(PluginPresetManager::savePreset(chainOut, name));
            expect(PluginPresetManager::presetExists(name));

            PluginChain chainIn;
            expect(PluginPresetManager::loadPreset(chainIn, name));
            expectEquals(chainIn.getNumPlugins(), 0);

            expect(PluginPresetManager::deletePreset(name));
            expect(! PluginPresetManager::presetExists(name));
        }

        beginTest("savePreset (named) stamps presetName/createdAt and writes no automation key");
        {
            const juce::String name = "WaveEditTest_ChainPersistence_Metadata";
            PluginPresetManager::deletePreset(name);

            PluginChain chain;
            expect(PluginPresetManager::savePreset(chain, name));

            // getPresetFile() is private, but getPresetDirectory() + the
            // documented ".wepchain" extension (PluginPresetManager.h) let us
            // reconstruct the same path without reaching into internals.
            auto file = PluginPresetManager::getPresetDirectory().getChildFile(name + ".wepchain");
            expect(file.existsAsFile(), "named save should land in the standard preset directory");

            const auto raw = file.loadFileAsString();
            expect(raw.contains("\"presetName\""), "named save should stamp a presetName field");
            expect(raw.contains("\"createdAt\""), "named save should stamp a createdAt field");
            expect(! raw.contains("\"automation\""), "chain-only save must not write an automation block");

            expect(PluginPresetManager::deletePreset(name));
        }

        beginTest("getAvailablePresets lists saved chain-only presets and drops deleted ones");
        {
            const juce::String nameA = "WaveEditTest_ChainPersistence_ListA";
            const juce::String nameB = "WaveEditTest_ChainPersistence_ListB";
            PluginPresetManager::deletePreset(nameA);
            PluginPresetManager::deletePreset(nameB);

            PluginChain chain;
            expect(PluginPresetManager::savePreset(chain, nameA));
            expect(PluginPresetManager::savePreset(chain, nameB));

            auto presets = PluginPresetManager::getAvailablePresets();
            expect(presets.contains(nameA), "getAvailablePresets should list the first saved preset");
            expect(presets.contains(nameB), "getAvailablePresets should list the second saved preset");

            expect(PluginPresetManager::deletePreset(nameA));
            presets = PluginPresetManager::getAvailablePresets();
            expect(! presets.contains(nameA), "deleted preset should drop out of the listing");
            expect(presets.contains(nameB), "the untouched preset should remain listed");

            expect(PluginPresetManager::deletePreset(nameB));
        }

        beginTest("loadPreset for a nonexistent name fails cleanly");
        {
            const juce::String name = "WaveEditTest_ChainPersistence_DoesNotExist";
            PluginPresetManager::deletePreset(name);  // ensure absent

            PluginChain chain;
            expect(! PluginPresetManager::loadPreset(chain, name));
        }

        beginTest("savePreset with an empty preset name fails and writes nothing");
        {
            PluginChain chain;
            expect(! PluginPresetManager::savePreset(chain, juce::String()));
        }
    }
};

static PluginPresetManagerNamedPresetTests pluginPresetManagerNamedPresetTests;
