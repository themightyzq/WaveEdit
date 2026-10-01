/*
  ==============================================================================

    CueChunkTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Integration tests for WAV cue/adtl embedding of markers + regions and the
    opt-in sidecar policy:
      - markers + regions embed and re-read sample-exact (positions, names,
        region lengths)
      - bext + iXML survive alongside cue/adtl
      - re-save replaces (does not duplicate) cue chunks
      - non-ASCII name -> ASCII fallback embedded + sidecar required
      - palette/default-color doc -> NO sidecar written
      - custom color -> sidecar written
      - old-format sidecar (no fingerprint) still loads and is treated as fresh

  ==============================================================================
*/

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <limits>
#include "../../Source/Audio/AudioFileManager.h"
#include "../../Source/Utils/BWFMetadata.h"
#include "../../Source/Utils/iXMLMetadata.h"
#include "../../Source/Utils/RegionManager.h"
#include "../../Source/Utils/MarkerManager.h"
#include "../../Source/Utils/SidecarPolicy.h"
#include "../../Source/Utils/Document.h"
#include "../TestUtils/TestAudioFiles.h"

//==============================================================================
class CueChunkTests : public juce::UnitTest
{
public:
    CueChunkTests() : juce::UnitTest("Cue Chunk Embedding", "Integration") {}

    void runTest() override
    {
        testEmbedRoundTripSampleExact();
        testBextAndIxmlSurviveCues();
        testReSaveReplacesNotDuplicates();
        testNonAsciiFallbackAndSidecarRequired();
        testPaletteOnlyDocWritesNoSidecar();
        testCustomColorWritesSidecar();
        testOldFormatSidecarStillLoads();
        testSampleRateConversionRescalesEmbeddedCues();
        testSampleRateConversionRescalesSidecar();
        testOversizedCuePositionSkippedNotCorrupted();
    }

private:
    juce::File tempFile(const juce::String& name)
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(name);
    }

    // Write a valid 1-second mono WAV so cue surgery has a real file to edit.
    juce::File makeWav(const juce::String& name)
    {
        auto file = tempFile(name);
        file.deleteFile();
        RegionManager::getRegionFilePath(file).deleteFile();
        MarkerManager::getMarkerFilePath(file).deleteFile();

        auto buffer = TestAudio::createSineWave(440.0, 0.5f, 44100.0, 1.0, 1);
        AudioFileManager fm;
        expect(fm.saveAsWav(file, buffer, 44100.0, 16), "WAV should save");
        return file;
    }

    void cleanup(const juce::File& file)
    {
        file.deleteFile();
        RegionManager::getRegionFilePath(file).deleteFile();
        MarkerManager::getMarkerFilePath(file).deleteFile();
    }

    //==========================================================================
    void testEmbedRoundTripSampleExact()
    {
        beginTest("Embed + re-read markers/regions sample-exact");

        auto file = makeWav("cue_roundtrip.wav");

        WavCueData in;
        in.markers.add({ "Intro", 1000 });
        in.markers.add({ "Hit", 5000 });
        WavCueRegion r1; r1.name = "Verse"; r1.start = 2000; r1.length = 3000;
        WavCueRegion r2; r2.name = "Chorus"; r2.start = 10000; r2.length = 7500;
        in.regions.add(r1);
        in.regions.add(r2);

        AudioFileManager fm;
        expect(fm.writeCueChunks(file, in), "writeCueChunks should succeed");

        WavCueData out;
        expect(fm.readCueChunks(file, out), "readCueChunks should find data");

        expectEquals(out.markers.size(), 2, "marker count");
        expectEquals(out.regions.size(), 2, "region count");

        expectEquals((int) out.markers[0].position, 1000, "marker 0 position");
        expect(out.markers[0].name == "Intro", "marker 0 name");
        expectEquals((int) out.markers[1].position, 5000, "marker 1 position");
        expect(out.markers[1].name == "Hit", "marker 1 name");

        expectEquals((int) out.regions[0].start, 2000, "region 0 start");
        expectEquals((int) out.regions[0].length, 3000, "region 0 length");
        expect(out.regions[0].name == "Verse", "region 0 name");
        expectEquals((int) out.regions[1].start, 10000, "region 1 start");
        expectEquals((int) out.regions[1].length, 7500, "region 1 length");
        expect(out.regions[1].name == "Chorus", "region 1 name");

        cleanup(file);
    }

    //==========================================================================
    void testBextAndIxmlSurviveCues()
    {
        beginTest("bext + iXML survive alongside cue/adtl");

        auto file = tempFile("cue_bext_ixml.wav");
        file.deleteFile();

        auto buffer = TestAudio::createSineWave(440.0, 0.5f, 44100.0, 0.5, 2);

        BWFMetadata bwf;
        bwf.setDescription("Cue test description");
        bwf.setOriginatorRef("CUEREF");

        iXMLMetadata ixml;
        ixml.setProject("CueProject");
        ixml.setCategory("DOOR");

        AudioFileManager fm;
        expect(fm.saveAsWav(file, buffer, 44100.0, 16, bwf.toJUCEMetadata()), "save w/ bext");
        expect(fm.appendiXMLChunk(file, ixml.toXMLString()), "append iXML");

        WavCueData cues;
        cues.markers.add({ "M1", 4410 });
        WavCueRegion r; r.name = "R1"; r.start = 8820; r.length = 4410;
        cues.regions.add(r);
        expect(fm.writeCueChunks(file, cues), "embed cues over bext+iXML");

        // bext survives
        AudioFileInfo info;
        expect(fm.getFileInfo(file, info), "read info");
        BWFMetadata loadedBwf;
        loadedBwf.fromJUCEMetadata(info.metadata);
        expect(loadedBwf.getDescription() == "Cue test description", "bext description survives");
        expect(loadedBwf.getOriginatorRef() == "CUEREF", "bext ref survives");

        // iXML survives
        iXMLMetadata loadedIxml;
        expect(loadedIxml.loadFromFile(file), "iXML readable");
        expect(loadedIxml.getProject() == "CueProject", "iXML project survives");

        // cues survive
        WavCueData readBack;
        expect(fm.readCueChunks(file, readBack), "cues readable");
        expectEquals(readBack.markers.size(), 1, "marker survived");
        expectEquals(readBack.regions.size(), 1, "region survived");

        cleanup(file);
    }

    //==========================================================================
    void testReSaveReplacesNotDuplicates()
    {
        beginTest("Re-save replaces (not duplicates) cue chunks");

        auto file = makeWav("cue_resave.wav");
        AudioFileManager fm;

        WavCueData first;
        first.markers.add({ "A", 1000 });
        first.markers.add({ "B", 2000 });
        expect(fm.writeCueChunks(file, first), "first embed");

        WavCueData second;
        second.markers.add({ "C", 3000 });  // different, single marker
        expect(fm.writeCueChunks(file, second), "second embed");

        WavCueData out;
        expect(fm.readCueChunks(file, out), "read after re-save");
        expectEquals(out.markers.size(), 1, "only the last write's markers remain");
        expectEquals(out.regions.size(), 0, "no leftover regions");
        expect(out.markers[0].name == "C", "content is the second write");
        expectEquals((int) out.markers[0].position, 3000, "position is the second write");

        cleanup(file);
    }

    //==========================================================================
    void testNonAsciiFallbackAndSidecarRequired()
    {
        beginTest("Non-ASCII name -> ASCII fallback embedded + sidecar required");

        auto file = makeWav("cue_nonascii.wav");

        const juce::String fancy = juce::CharPointer_UTF8("Caf\xc3\xa9");  // "Cafe" with accent

        // Region with a non-ASCII name requires a sidecar.
        RegionManager regions;
        Region region(fancy, 1000, 4000);
        regions.addRegion(region);
        expect(SidecarPolicy::regionsNeedSidecar(regions),
               "non-ASCII region name must require a sidecar");

        // The embedded labl is a lossy ASCII transliteration.
        WavCueData cues;
        WavCueRegion cr;
        cr.name = SidecarPolicy::toAsciiLabel(fancy);
        cr.start = 1000; cr.length = 3000;
        cues.regions.add(cr);

        AudioFileManager fm;
        expect(fm.writeCueChunks(file, cues), "embed ascii-fallback region");

        WavCueData out;
        expect(fm.readCueChunks(file, out), "read back");
        expectEquals(out.regions.size(), 1, "region present");
        expect(SidecarPolicy::isPureAscii(out.regions[0].name),
               "embedded labl must be pure ASCII");
        expect(out.regions[0].name != fancy, "embedded name is the lossy fallback, not the original");

        cleanup(file);
    }

    //==========================================================================
    void testPaletteOnlyDocWritesNoSidecar()
    {
        beginTest("Palette/default-color doc writes NO sidecar");

        auto file = makeWav("cue_no_sidecar.wav");

        RegionManager regions;
        // Region 0: flat default (manual Cmd+M) color.
        regions.addRegion(Region("One", 100, 500));
        // Region 1: palette color for its index (Strip Silence / import).
        Region two("Two", 600, 900);
        two.setColor(SidecarPolicy::autoRegionColor(1));
        regions.addRegion(two);

        expect(!SidecarPolicy::regionsNeedSidecar(regions),
               "default + palette colors are embeddable");
        expect(regions.saveToFile(file), "saveToFile returns true (no-op write)");
        expect(!RegionManager::getRegionFilePath(file).existsAsFile(),
               "no .regions.json should be created for a fully embeddable doc");

        // Markers with default color likewise create no sidecar.
        MarkerManager markers;
        markers.addMarker(Marker("Mk", 200));  // default yellow
        expect(!SidecarPolicy::markersNeedSidecar(markers), "default marker embeddable");
        expect(markers.saveToFile(file), "marker saveToFile no-op");
        expect(!MarkerManager::getMarkerFilePath(file).existsAsFile(),
               "no .markers.json for embeddable markers");

        cleanup(file);
    }

    //==========================================================================
    void testCustomColorWritesSidecar()
    {
        beginTest("Custom color writes a sidecar");

        auto file = makeWav("cue_custom_sidecar.wav");

        RegionManager regions;
        Region r("Red", 100, 500);
        r.setColor(juce::Colours::red);  // not default, not palette(index 0)
        regions.addRegion(r);

        expect(SidecarPolicy::regionsNeedSidecar(regions), "custom color requires sidecar");
        expect(regions.saveToFile(file), "saveToFile succeeds");
        expect(RegionManager::getRegionFilePath(file).existsAsFile(),
               "a .regions.json must be created for a custom-color doc");

        // Custom marker color writes a .markers.json too.
        MarkerManager markers;
        markers.addMarker(Marker("Blue", 200, juce::Colours::blue));
        expect(SidecarPolicy::markersNeedSidecar(markers), "custom marker color requires sidecar");
        expect(markers.saveToFile(file), "marker saveToFile succeeds");
        expect(MarkerManager::getMarkerFilePath(file).existsAsFile(),
               "a .markers.json must be created for a custom marker color");

        cleanup(file);
    }

    //==========================================================================
    void testOldFormatSidecarStillLoads()
    {
        beginTest("Old-format sidecar (no fingerprint) still loads and is fresh");

        auto file = makeWav("cue_oldformat.wav");
        auto sidecar = RegionManager::getRegionFilePath(file);

        // Hand-write an old-style sidecar with no audioLength / audioModTime.
        const juce::String json =
            "{ \"version\": \"1.0\", \"audioFile\": \"" + file.getFileName() + "\", "
            "\"regions\": [ { \"name\": \"Legacy\", \"startSample\": 1000, "
            "\"endSample\": 4000, \"color\": \"ffff0000\" } ] }";
        expect(sidecar.replaceWithText(json), "write legacy sidecar");

        RegionManager regions;
        expect(regions.loadFromFile(file), "legacy sidecar loads");
        expectEquals(regions.getNumRegions(), 1, "one region loaded");
        if (const Region* r = regions.getRegion(0))
        {
            expect(r->getName() == "Legacy", "legacy region name");
            expectEquals((int) r->getStartSample(), 1000, "legacy start");
            expectEquals((int) r->getEndSample(), 4000, "legacy end");
        }

        // A sidecar with no fingerprint fields is treated as fresh (not stale).
        expect(!SidecarPolicy::sidecarStaleAgainst(sidecar, file),
               "old-format sidecar must be treated as fresh (back-compat)");

        cleanup(file);
    }

    //==========================================================================
    // Regression test (Tier 0 data-integrity finding): a Save-As that converts
    // the sample rate resamples the audio but, before this fix, wrote cue and
    // sidecar positions straight through in source-rate samples -- every
    // marker/region silently drifted on reopen. Positions below are multiples
    // of 147 so the 44100 -> 48000 ratio (160/147) lands on an exact integer,
    // letting the assertions check exact equality rather than a tolerance.
    void testSampleRateConversionRescalesEmbeddedCues()
    {
        beginTest("Save As with SR conversion rescales embedded cue positions");

        auto sourceFile = tempFile("cue_srconvert_embedded.wav");
        auto targetFile = tempFile("cue_srconvert_embedded_48k.wav");
        cleanup(sourceFile);
        cleanup(targetFile);

        auto buffer = TestAudio::createSineWave(440.0, 0.5f, 44100.0, 1.0, 1);
        AudioFileManager fm;
        expect(fm.saveAsWav(sourceFile, buffer, 44100.0, 16), "source WAV should save");

        Document doc;
        expect(doc.loadFile(sourceFile), "document should load the source WAV");
        expectEquals(doc.getAudioEngine().getSampleRate(), 44100.0, "source sample rate");

        // Default colors/names -> no sidecar; these round-trip purely through
        // embedded WAV cue/adtl chunks.
        doc.getRegionManager().addRegion(Region("Verse", 1470, 4410));  // 147*10 .. 147*30
        doc.getMarkerManager().addMarker(Marker("Hit", 2940));          // 147*20

        expect(doc.saveFile(targetFile, 16, 10, 48000.0), "Save As @ 48kHz should succeed");
        expect(!RegionManager::getRegionFilePath(targetFile).existsAsFile(),
               "default-color regions stay embed-only -- no sidecar expected");

        Document reopened;
        expect(reopened.loadFile(targetFile), "reopened document should load the converted WAV");
        expectEquals(reopened.getAudioEngine().getSampleRate(), 48000.0, "target sample rate");

        expectEquals(reopened.getRegionManager().getNumRegions(), 1, "one region round-tripped");
        if (auto* r = reopened.getRegionManager().getRegion(0))
        {
            expectEquals((int) r->getStartSample(), 1600, "region start rescaled to target rate");
            expectEquals((int) r->getEndSample(), 4800, "region end rescaled to target rate");
        }

        expectEquals(reopened.getMarkerManager().getNumMarkers(), 1, "one marker round-tripped");
        if (auto* m = reopened.getMarkerManager().getMarker(0))
            expectEquals((int) m->getPosition(), 3200, "marker position rescaled to target rate");

        cleanup(sourceFile);
        cleanup(targetFile);
    }

    //==========================================================================
    void testSampleRateConversionRescalesSidecar()
    {
        beginTest("Save As with SR conversion rescales sidecar positions");

        auto sourceFile = tempFile("cue_srconvert_sidecar.wav");
        auto targetFile = tempFile("cue_srconvert_sidecar_48k.wav");
        cleanup(sourceFile);
        cleanup(targetFile);

        auto buffer = TestAudio::createSineWave(440.0, 0.5f, 44100.0, 1.0, 1);
        AudioFileManager fm;
        expect(fm.saveAsWav(sourceFile, buffer, 44100.0, 16), "source WAV should save");

        Document doc;
        expect(doc.loadFile(sourceFile), "document should load the source WAV");

        // Custom colors force a JSON sidecar (the WAV cue chunk cannot carry
        // color) -- the other position store that must be rescaled.
        Region region("Red", 1470, 4410);
        region.setColor(juce::Colours::red);
        doc.getRegionManager().addRegion(region);

        Marker marker("Blue", 2940, juce::Colours::blue);
        doc.getMarkerManager().addMarker(marker);

        expect(doc.saveFile(targetFile, 16, 10, 48000.0), "Save As @ 48kHz should succeed");
        expect(RegionManager::getRegionFilePath(targetFile).existsAsFile(),
               "custom-color region must still force a sidecar");
        expect(MarkerManager::getMarkerFilePath(targetFile).existsAsFile(),
               "custom-color marker must still force a sidecar");

        Document reopened;
        expect(reopened.loadFile(targetFile), "reopened document should load the converted WAV");

        expectEquals(reopened.getRegionManager().getNumRegions(), 1, "one region round-tripped");
        if (auto* r = reopened.getRegionManager().getRegion(0))
        {
            expectEquals((int) r->getStartSample(), 1600, "sidecar region start rescaled to target rate");
            expectEquals((int) r->getEndSample(), 4800, "sidecar region end rescaled to target rate");
        }

        expectEquals(reopened.getMarkerManager().getNumMarkers(), 1, "one marker round-tripped");
        if (auto* m = reopened.getMarkerManager().getMarker(0))
            expectEquals((int) m->getPosition(), 3200, "sidecar marker position rescaled to target rate");

        cleanup(sourceFile);
        cleanup(targetFile);
    }

    //==========================================================================
    // Regression test (M-H2 hardening finding): RIFF "cue "/"ltxt" fields are
    // 32-bit. Before this fix, a position/length beyond UINT32_MAX was cast
    // straight into the 32-bit field with no guard, silently truncating to a
    // garbage sample on read. Entries that don't fit must be skipped on
    // write (and a warning logged) rather than embedding a corrupt cue --
    // the entry is still preserved via the (int64-capable) JSON sidecar,
    // which is a separate, already-covered path.
    void testOversizedCuePositionSkippedNotCorrupted()
    {
        beginTest("Cue position beyond the 32-bit WAV field is skipped, not corrupted");

        auto file = makeWav("cue_oversized_position.wav");

        constexpr juce::int64 oversizedPosition =
            static_cast<juce::int64>(std::numeric_limits<juce::uint32>::max()) + 1000;

        WavCueData cues;
        cues.markers.add({ "InRange", 1000 });          // fits -- must survive
        cues.markers.add({ "TooFar", oversizedPosition }); // does not fit -- must be dropped, not wrapped

        AudioFileManager fm;
        expect(fm.writeCueChunks(file, cues),
              "writeCueChunks must still succeed even though one entry is out of range");

        WavCueData out;
        expect(fm.readCueChunks(file, out), "readCueChunks should find the in-range marker");

        expectEquals(out.markers.size(), 1,
                     "the oversized marker must be dropped, not embedded as a wrapped/garbage value");
        if (out.markers.size() == 1)
        {
            expect(out.markers[0].name == "InRange", "the surviving marker is the in-range one");
            expectEquals((int) out.markers[0].position, 1000, "in-range marker position is untouched");
        }

        cleanup(file);
    }
};

static CueChunkTests cueChunkTests;
