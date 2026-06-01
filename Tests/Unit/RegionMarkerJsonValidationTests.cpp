/*
  ==============================================================================

    RegionMarkerJsonValidationTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Pass-2 regression tests for H22: region/marker JSON deserialization must
    validate fields rather than silently coerce missing/wrong-typed data to 0.

      - Missing/non-numeric startSample/endSample -> region stays the [0,0]
        sentinel (rejected on load), not a bogus mid-buffer range.
      - start > end is unswapped by fromJSON (matches the constructor).
      - RegionManager::loadFromFile drops zero-length / malformed entries and,
        when given a buffer length, clamps out-of-range offsets.
      - Marker with a non-numeric position falls back to the default marker.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "Utils/Region.h"
#include "Utils/RegionManager.h"
#include "Utils/Marker.h"

// ============================================================================
class RegionMarkerJsonValidationTests : public juce::UnitTest
{
public:
    RegionMarkerJsonValidationTests()
        : juce::UnitTest("Region/Marker JSON Validation (Pass 2)", "RegionManager") {}

    void runTest() override
    {
        beginTest("H22: region with start > end is unswapped");
        testRegionSwapOnLoad();

        beginTest("H22: region missing endSample is rejected (sentinel)");
        testRegionMissingField();

        beginTest("H22: region with non-numeric fields is rejected");
        testRegionNonNumericField();

        beginTest("H22: loadFromFile drops malformed and clamps out-of-range");
        testLoadFromFileValidation();

        beginTest("H22: marker with non-numeric position falls back to default");
        testMarkerNonNumericPosition();
    }

private:
    juce::var makeObj(std::initializer_list<std::pair<const char*, juce::var>> props)
    {
        auto* obj = new juce::DynamicObject();
        for (const auto& p : props)
            obj->setProperty(p.first, p.second);
        return juce::var(obj);
    }

    void testRegionSwapOnLoad()
    {
        auto json = makeObj({
            {"name", "swapped"},
            {"startSample", (juce::int64) 5000},
            {"endSample",   (juce::int64) 1000},   // start > end
            {"color", juce::Colours::red.toString()}
        });

        Region r = Region::fromJSON(json);
        expect(r.getStartSample() <= r.getEndSample(), "fromJSON unswaps start/end");
        expectEquals(r.getStartSample(), (int64_t) 1000);
        expectEquals(r.getEndSample(),   (int64_t) 5000);
    }

    void testRegionMissingField()
    {
        auto json = makeObj({
            {"name", "incomplete"},
            {"startSample", (juce::int64) 1234}
            // endSample missing
        });

        Region r = Region::fromJSON(json);
        // Both fields must be numeric; otherwise the [0,0] sentinel is kept.
        expectEquals(r.getStartSample(), (int64_t) 0, "missing field -> sentinel start");
        expectEquals(r.getEndSample(),   (int64_t) 0, "missing field -> sentinel end");
    }

    void testRegionNonNumericField()
    {
        auto json = makeObj({
            {"name", "bad"},
            {"startSample", juce::var("not a number")},
            {"endSample",   juce::var("nope")}
        });

        Region r = Region::fromJSON(json);
        expectEquals(r.getStartSample(), (int64_t) 0);
        expectEquals(r.getEndSample(),   (int64_t) 0);
    }

    void testLoadFromFileValidation()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory);
        auto audioFile = dir.getNonexistentChildFile("WaveEdit_JsonValidate", ".wav", false);
        audioFile.create();
        auto sidecar = RegionManager::getRegionFilePath(audioFile);

        // Build a sidecar with: one good region, one malformed (missing end),
        // one start>end, and one out-of-range (end beyond the buffer).
        juce::Array<juce::var> regions;
        regions.add(makeObj({
            {"name", "good"}, {"startSample", (juce::int64) 100},
            {"endSample", (juce::int64) 500}, {"color", juce::Colours::blue.toString()}}));
        regions.add(makeObj({
            {"name", "missingEnd"}, {"startSample", (juce::int64) 100}}));
        regions.add(makeObj({
            {"name", "swapped"}, {"startSample", (juce::int64) 900},
            {"endSample", (juce::int64) 200}}));
        regions.add(makeObj({
            {"name", "outOfRange"}, {"startSample", (juce::int64) 100},
            {"endSample", (juce::int64) 1000000}}));

        auto* root = new juce::DynamicObject();
        root->setProperty("version", "1.0");
        root->setProperty("regions", regions);
        sidecar.replaceWithText(juce::JSON::toString(juce::var(root), true));

        const int64_t bufferLen = 2000;
        RegionManager manager;
        bool loaded = manager.loadFromFile(audioFile, bufferLen);
        expect(loaded, "sidecar parsed");

        // Expected survivors:
        //  - "good" [100,500]                              -> kept
        //  - "missingEnd" -> [0,0] sentinel, zero-length   -> dropped
        //  - "swapped" -> [200,900] after unswap           -> kept
        //  - "outOfRange" -> end clamped to 2000           -> kept [100,2000]
        expectEquals(manager.getNumRegions(), 3, "malformed zero-length region dropped");

        bool sawOutOfRangeClamped = false;
        bool sawSwapped = false;
        for (int i = 0; i < manager.getNumRegions(); ++i)
        {
            const Region* r = manager.getRegion(i);
            expect(r->getStartSample() < r->getEndSample(), "no negative-length region survives");
            expect(r->getEndSample() <= bufferLen, "no region exceeds the buffer length");

            if (r->getName() == "outOfRange")
            {
                sawOutOfRangeClamped = (r->getEndSample() == bufferLen);
            }
            if (r->getName() == "swapped")
            {
                sawSwapped = (r->getStartSample() == 200 && r->getEndSample() == 900);
            }
        }
        expect(sawOutOfRangeClamped, "out-of-range end clamped to buffer length");
        expect(sawSwapped, "swapped region unswapped to [200,900]");

        sidecar.deleteFile();
        audioFile.deleteFile();
    }

    void testMarkerNonNumericPosition()
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty("name", "bad");
        obj->setProperty("position", juce::var("not a number"));
        obj->setProperty("color", juce::Colours::green.toString());

        Marker m = Marker::fromJSON(juce::var(obj));
        // Falls back to a default marker (position 0, default name) rather than
        // silently parsing the string as 0 with the caller-supplied name.
        expectEquals(m.getPosition(), (int64_t) 0);
        expectEquals(m.getName(), juce::String("Marker"), "default marker returned on bad position");
    }
};

static RegionMarkerJsonValidationTests regionMarkerJsonValidationTests;
