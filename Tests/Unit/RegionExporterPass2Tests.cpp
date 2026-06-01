/*
  ==============================================================================

    RegionExporterPass2Tests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Pass-2 regression tests for RegionExporter:
      - C17: long-region length is no longer narrowed to int; chunked write
             produces the exact sample count across a chunk boundary, and an
             invalid (zero/negative) length is rejected, not crashed.
      - H15: two regions whose names sanitize to the same filename do NOT
             overwrite each other - the second gets a _1 suffix.
      - H16: a region named after a Windows reserved device (CON/COM1/...) is
             escaped so createOutputStream cannot fail on Windows.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Utils/Region.h"
#include "Utils/RegionManager.h"
#include "Utils/RegionExporter.h"

// ============================================================================
class RegionExporterPass2Tests : public juce::UnitTest
{
public:
    RegionExporterPass2Tests()
        : juce::UnitTest("RegionExporter (Pass 2)", "RegionManager") {}

    void runTest() override
    {
        beginTest("C17: invalid (zero-length) region rejected with a message");
        testInvalidLengthRejected();

        beginTest("C17: chunked export writes exact sample count over a boundary");
        testChunkedExportSampleCount();

        beginTest("H15: colliding sanitized names get a dedup suffix");
        testFilenameDedup();

        beginTest("H16: Windows reserved names are escaped");
        testReservedNamesEscaped();
    }

private:
    juce::File makeOutputDir(const juce::String& tag)
    {
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                       .getNonexistentChildFile("WaveEdit_Export_" + tag, "", false);
        dir.createDirectory();
        return dir;
    }

    juce::AudioBuffer<float> makeRamp(int channels, int numSamples)
    {
        juce::AudioBuffer<float> buffer(channels, numSamples);
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* d = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = std::sin((float) i * 0.001f) * 0.5f;
        }
        return buffer;
    }

    int readWavSampleCount(const juce::File& f)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(f));
        return reader ? (int) reader->lengthInSamples : -1;
    }

    void testInvalidLengthRejected()
    {
        auto buffer = makeRamp(1, 1000);
        Region zeroLen("z", 500, 500);  // start == end -> length 0
        auto dir = makeOutputDir("invalid");
        auto out = dir.getChildFile("z.wav");

        juce::String err;
        bool ok = RegionExporter::exportSingleRegion(buffer, 44100.0, zeroLen, out, 24, err);
        expect(!ok, "zero-length region must not export");
        expect(err.isNotEmpty(), "an error message is provided");
        expect(!out.existsAsFile(), "no output file is left behind");

        dir.deleteRecursively();
    }

    void testChunkedExportSampleCount()
    {
        // The chunk size in RegionExporter is 1<<20. Use a region that crosses
        // that boundary so a single-chunk regression would be caught.
        const int total = (1 << 20) + 12345;
        auto buffer = makeRamp(2, total);

        Region full("full", 0, total);
        auto dir = makeOutputDir("chunked");
        auto out = dir.getChildFile("full.wav");

        juce::String err;
        bool ok = RegionExporter::exportSingleRegion(buffer, 48000.0, full, out, 24, err);
        expect(ok, "long region exports: " + err);
        expectEquals(readWavSampleCount(out), total,
                     "exported sample count matches the region length exactly");

        dir.deleteRecursively();
    }

    void testFilenameDedup()
    {
        // Two region names that sanitize to the same filename: "Take/1" and
        // "Take?1" both become "Take_1".
        RegionManager manager;
        manager.addRegion(Region("Take/1", 0,   100));
        manager.addRegion(Region("Take?1", 200, 300));

        auto buffer = makeRamp(1, 1000);
        auto dir = makeOutputDir("dedup");

        RegionExporter::ExportSettings settings;
        settings.outputDirectory = dir;
        settings.includeRegionName = true;
        settings.includeIndex = false;       // force the name-only collision
        settings.bitDepth = 16;

        int exported = RegionExporter::exportRegions(buffer, 44100.0, manager,
                                                     juce::File("src.wav"), settings);
        expectEquals(exported, 2, "both regions report exported");

        int wavCount = dir.getNumberOfChildFiles(juce::File::findFiles, "*.wav");
        expectEquals(wavCount, 2, "two distinct files exist (no overwrite)");

        dir.deleteRecursively();
    }

    void testReservedNamesEscaped()
    {
        RegionManager manager;
        manager.addRegion(Region("CON",  0,   100));
        manager.addRegion(Region("COM1", 200, 300));

        auto buffer = makeRamp(1, 1000);
        auto dir = makeOutputDir("reserved");

        RegionExporter::ExportSettings settings;
        settings.outputDirectory = dir;
        settings.includeRegionName = true;
        settings.includeIndex = false;
        settings.customTemplate = "{region}";  // filename stem == region name
        settings.bitDepth = 16;

        int exported = RegionExporter::exportRegions(buffer, 44100.0, manager,
                                                     juce::File("src.wav"), settings);
        expectEquals(exported, 2, "reserved-named regions still export");

        // No file may have a bare reserved stem; each is prefixed with '_'.
        expect(!dir.getChildFile("CON.wav").existsAsFile(),
               "bare reserved 'CON.wav' is not produced");
        expect(dir.getChildFile("_CON.wav").existsAsFile(),
               "reserved stem escaped to '_CON.wav'");
        expect(dir.getChildFile("_COM1.wav").existsAsFile(),
               "reserved stem escaped to '_COM1.wav'");

        dir.deleteRecursively();
    }
};

static RegionExporterPass2Tests regionExporterPass2Tests;
