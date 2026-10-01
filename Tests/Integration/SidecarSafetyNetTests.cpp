/*
  ==============================================================================

    SidecarSafetyNetTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Integration tests for the v0.9.1 sidecar safety net: regions/markers must
    survive a save even when the destination format cannot embed WAV cue/adtl
    chunks (FLAC, AIFF, OGG, MP3) or when a WAV's cue write does not fully
    embed every entry (32-bit position overflow, or RF64). Before this fix,
    RegionManager::saveToFile / MarkerManager::saveToFile only wrote a sidecar
    when the data was flagged non-embeddable (custom color / non-ASCII name),
    so a save to any non-WAV format silently dropped every plain region and
    marker:
      - SidecarPolicy::mustForceSidecar truth table (all 8 combinations)
      - FLAC / AIFF / OGG saves force a sidecar and round-trip sample-exact
      - MP3 save forces a sidecar and round-trips within an encoder-delay
        tolerance
      - a plain WAV save with default-color/ASCII data still writes NO
        sidecar (embedding covers it) -- guards against sidecar spam
      - writeCueChunks reports allEntriesEmbedded=false when an entry is
        dropped for exceeding the 32-bit cue field, true otherwise

  ==============================================================================
*/

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include <limits>
#include "../../Source/Audio/AudioFileManager.h"
#include "../../Source/Utils/RegionManager.h"
#include "../../Source/Utils/MarkerManager.h"
#include "../../Source/Utils/SidecarPolicy.h"
#include "../../Source/Utils/Document.h"
#include "../TestUtils/TestAudioFiles.h"

//==============================================================================
class SidecarSafetyNetTests : public juce::UnitTest
{
public:
    SidecarSafetyNetTests() : juce::UnitTest("Sidecar Safety Net (v0.9.1)", "Integration") {}

    void runTest() override
    {
        tempDir().deleteRecursively();
        tempDir().createDirectory();

        testMustForceSidecarTable();
        testNonWavFormatForcesSidecar("flac");
        testNonWavFormatForcesSidecar("aiff");
        testNonWavFormatForcesSidecar("ogg");
        testMp3ForcesSidecar();
        testPlainWavStillWritesNoSidecar();
        testWriteCueChunksReportsAllEntriesEmbedded();

        tempDir().deleteRecursively();
    }

private:
    juce::File tempDir()
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("WaveEditSidecarSafetyNetTests");
    }

    juce::File tempFile(const juce::String& name)
    {
        return tempDir().getChildFile(name);
    }

    // Write a valid 1-second mono WAV fixture so a Document has real audio to
    // load, edit and re-save.
    juce::File makeWav(const juce::String& name)
    {
        auto file = tempFile(name);
        cleanup(file);

        auto buffer = TestAudio::createSineWave(440.0, 0.5f, 44100.0, 1.0, 1);
        AudioFileManager fm;
        expect(fm.saveAsWav(file, buffer, 44100.0, 16), "WAV fixture should save");
        return file;
    }

    void cleanup(const juce::File& file)
    {
        file.deleteFile();
        RegionManager::getRegionFilePath(file).deleteFile();
        MarkerManager::getMarkerFilePath(file).deleteFile();
    }

    //==========================================================================
    void testMustForceSidecarTable()
    {
        beginTest("SidecarPolicy::mustForceSidecar - all 8 combinations");

        // mustForceSidecar returns true UNLESS (isWav && cueWriteOk && allEmbedded).
        struct Case
        {
            bool isWav;
            bool cueOk;
            bool allEmbedded;
            bool expected;
        };

        const Case cases[] = {
            { false, false, false, true },
            { false, false, true,  true },
            { false, true,  false, true },
            { false, true,  true,  true },
            { true,  false, false, true },
            { true,  false, true,  true },
            { true,  true,  false, true },
            { true,  true,  true,  false },
        };

        for (const auto& c : cases)
        {
            const juce::String label = "isWav=" + juce::String((int) c.isWav)
                                      + " cueOk=" + juce::String((int) c.cueOk)
                                      + " allEmbedded=" + juce::String((int) c.allEmbedded);
            expectEquals((int) SidecarPolicy::mustForceSidecar(c.isWav, c.cueOk, c.allEmbedded),
                        (int) c.expected, label);
        }
    }

    //==========================================================================
    // Adds 2 default-color ASCII regions + 2 default markers to a Document
    // loaded from a fresh WAV fixture, saves to the given non-WAV extension,
    // and verifies the sidecar exists and the data round-trips exactly.
    void testNonWavFormatForcesSidecar(const juce::String& extension)
    {
        beginTest("Save to ." + extension + " forces a sidecar and round-trips regions/markers");

        auto sourceFile = makeWav("safetynet_source_" + extension + ".wav");
        auto targetFile = tempFile("safetynet_target." + extension);
        cleanup(targetFile);

        Document doc;
        expect(doc.loadFile(sourceFile), "document should load the WAV fixture");

        doc.getRegionManager().addRegion(Region("Verse", 1000, 4000));
        doc.getRegionManager().addRegion(Region("Chorus", 5000, 9000));
        doc.getMarkerManager().addMarker(Marker("Intro", 500));
        doc.getMarkerManager().addMarker(Marker("Outro", 15000));

        expect(doc.saveFile(targetFile, 16, 10, 0.0), "save to ." + extension + " should succeed");

        expect(RegionManager::getRegionFilePath(targetFile).existsAsFile(),
              "a .regions.json sidecar must exist for a ." + extension + " save");
        expect(MarkerManager::getMarkerFilePath(targetFile).existsAsFile(),
              "a .markers.json sidecar must exist for a ." + extension + " save");

        Document reopened;
        expect(reopened.loadFile(targetFile), "reopened document should load the ." + extension + " file");

        expectEquals(reopened.getRegionManager().getNumRegions(), 2, "region count survives");
        if (auto* r0 = reopened.getRegionManager().getRegion(0))
        {
            expect(r0->getName() == "Verse", "region 0 name");
            expectEquals((int) r0->getStartSample(), 1000, "region 0 start");
            expectEquals((int) r0->getEndSample(), 4000, "region 0 end");
        }
        if (auto* r1 = reopened.getRegionManager().getRegion(1))
        {
            expect(r1->getName() == "Chorus", "region 1 name");
            expectEquals((int) r1->getStartSample(), 5000, "region 1 start");
            expectEquals((int) r1->getEndSample(), 9000, "region 1 end");
        }

        expectEquals(reopened.getMarkerManager().getNumMarkers(), 2, "marker count survives");
        if (auto* m0 = reopened.getMarkerManager().getMarker(0))
        {
            expect(m0->getName() == "Intro", "marker 0 name");
            expectEquals((int) m0->getPosition(), 500, "marker 0 position");
        }
        if (auto* m1 = reopened.getMarkerManager().getMarker(1))
        {
            expect(m1->getName() == "Outro", "marker 1 name");
            expectEquals((int) m1->getPosition(), 15000, "marker 1 position");
        }

        cleanup(sourceFile);
        cleanup(targetFile);
    }

    //==========================================================================
    void testMp3ForcesSidecar()
    {
#if WAVEEDIT_HAVE_LAME
        beginTest("Save to .mp3 forces a sidecar and round-trips regions/markers (within tolerance)");

        auto sourceFile = makeWav("safetynet_source_mp3.wav");
        auto targetFile = tempFile("safetynet_target.mp3");
        cleanup(targetFile);

        Document doc;
        expect(doc.loadFile(sourceFile), "document should load the WAV fixture");

        doc.getRegionManager().addRegion(Region("Verse", 1000, 4000));
        doc.getRegionManager().addRegion(Region("Chorus", 5000, 9000));
        doc.getMarkerManager().addMarker(Marker("Intro", 500));
        doc.getMarkerManager().addMarker(Marker("Outro", 15000));

        expect(doc.saveFile(targetFile, 16, 9, 0.0), "save to .mp3 should succeed");

        expect(RegionManager::getRegionFilePath(targetFile).existsAsFile(),
              "a .regions.json sidecar must exist for an .mp3 save");
        expect(MarkerManager::getMarkerFilePath(targetFile).existsAsFile(),
              "a .markers.json sidecar must exist for an .mp3 save");

        Document reopened;
        expect(reopened.loadFile(targetFile), "reopened document should load the .mp3 file");

        // The sidecar JSON stores positions verbatim -- MP3 encoder delay/
        // padding shifts the DECODED buffer's length, not the sidecar's
        // numbers, so positions are expected to come back exact. We still
        // compare with a small tolerance (2000 samples, per spec) in case
        // reopen-time clamping against the (delay-shifted) buffer length
        // moves anything; observed behavior in this build is an EXACT
        // match (tolerance was not needed).
        constexpr int tolerance = 2000;

        expectEquals(reopened.getRegionManager().getNumRegions(), 2, "region count survives");
        if (auto* r0 = reopened.getRegionManager().getRegion(0))
        {
            expect(r0->getName() == "Verse", "region 0 name");
            expect(std::abs((int) r0->getStartSample() - 1000) <= tolerance,
                  "region 0 start within tolerance");
            expect(std::abs((int) r0->getEndSample() - 4000) <= tolerance,
                  "region 0 end within tolerance");
        }
        if (auto* r1 = reopened.getRegionManager().getRegion(1))
        {
            expect(r1->getName() == "Chorus", "region 1 name");
            expect(std::abs((int) r1->getStartSample() - 5000) <= tolerance,
                  "region 1 start within tolerance");
            expect(std::abs((int) r1->getEndSample() - 9000) <= tolerance,
                  "region 1 end within tolerance");
        }

        expectEquals(reopened.getMarkerManager().getNumMarkers(), 2, "marker count survives");
        if (auto* m0 = reopened.getMarkerManager().getMarker(0))
        {
            expect(m0->getName() == "Intro", "marker 0 name");
            expect(std::abs((int) m0->getPosition() - 500) <= tolerance,
                  "marker 0 position within tolerance");
        }
        if (auto* m1 = reopened.getMarkerManager().getMarker(1))
        {
            expect(m1->getName() == "Outro", "marker 1 name");
            expect(std::abs((int) m1->getPosition() - 15000) <= tolerance,
                  "marker 1 position within tolerance");
        }

        cleanup(sourceFile);
        cleanup(targetFile);
#else
        beginTest("MP3 encoder not compiled in (WAVEEDIT_HAVE_LAME undefined)");
        logMessage("Skipping MP3 sidecar safety-net test - built without the LAME encoder.");
        expect(true);
#endif
    }

    //==========================================================================
    void testPlainWavStillWritesNoSidecar()
    {
        beginTest("Plain WAV save with default regions still writes NO sidecar");

        auto file = makeWav("safetynet_plain_wav.wav");

        Document doc;
        expect(doc.loadFile(file), "document should load the WAV fixture");

        doc.getRegionManager().addRegion(Region("Verse", 1000, 4000));
        doc.getMarkerManager().addMarker(Marker("Intro", 500));

        expect(doc.saveFile(file, 16, 10, 0.0), "re-save as WAV should succeed");

        expect(!RegionManager::getRegionFilePath(file).existsAsFile(),
              "embedding covers a plain WAV save -- no .regions.json expected");
        expect(!MarkerManager::getMarkerFilePath(file).existsAsFile(),
              "embedding covers a plain WAV save -- no .markers.json expected");

        Document reopened;
        expect(reopened.loadFile(file), "reopened document should load the WAV");

        expectEquals(reopened.getRegionManager().getNumRegions(), 1,
                    "region round-trips from embedded cues");
        if (auto* r = reopened.getRegionManager().getRegion(0))
        {
            expect(r->getName() == "Verse", "region name from embedded cue");
            expectEquals((int) r->getStartSample(), 1000, "region start from embedded cue");
            expectEquals((int) r->getEndSample(), 4000, "region end from embedded cue");
        }

        expectEquals(reopened.getMarkerManager().getNumMarkers(), 1,
                    "marker round-trips from embedded cue");
        if (auto* m = reopened.getMarkerManager().getMarker(0))
        {
            expect(m->getName() == "Intro", "marker name from embedded cue");
            expectEquals((int) m->getPosition(), 500, "marker position from embedded cue");
        }

        cleanup(file);
    }

    //==========================================================================
    void testWriteCueChunksReportsAllEntriesEmbedded()
    {
        beginTest("writeCueChunks reports allEntriesEmbedded correctly");

        auto file = makeWav("safetynet_allembedded.wav");

        // Normal, in-range data: allEntriesEmbedded must come back true.
        {
            WavCueData cues;
            cues.markers.add({ "InRange", 1000 });
            WavCueRegion r;
            r.name = "Region";
            r.start = 2000;
            r.length = 3000;
            cues.regions.add(r);

            AudioFileManager fm;
            bool allEmbedded = false;
            expect(fm.writeCueChunks(file, cues, &allEmbedded),
                  "writeCueChunks should succeed for in-range data");
            expect(allEmbedded, "allEntriesEmbedded must be true when every entry fits");
        }

        // An oversized position: the call still succeeds (in-range entries
        // embed), but allEntriesEmbedded must come back false.
        {
            constexpr juce::int64 oversizedPosition =
                static_cast<juce::int64>(std::numeric_limits<juce::uint32>::max()) + 1000;

            WavCueData cues;
            cues.markers.add({ "InRange", 1000 });
            cues.markers.add({ "TooFar", oversizedPosition });

            AudioFileManager fm;
            bool allEmbedded = true;
            expect(fm.writeCueChunks(file, cues, &allEmbedded),
                  "writeCueChunks must still succeed even though one entry is out of range");
            expect(!allEmbedded, "allEntriesEmbedded must be false when an entry is skipped");
        }

        cleanup(file);
    }
};

static SidecarSafetyNetTests sidecarSafetyNetTests;
