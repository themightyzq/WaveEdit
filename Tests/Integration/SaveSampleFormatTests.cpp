/*
  ==============================================================================

    SaveSampleFormatTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Regression tests for H1: "32-bit Float" WAV export must produce IEEE-float
    audio, not 32-bit integer PCM. Both write paths are covered:
      - AudioFileManager::saveAsWav (File > Save As)
      - RegionExporter::exportSingleRegion (region batch export)

    Also verifies the 8-bit path stays integral, 16/24-bit are unaffected, and
    the FLAC bit-depth cap (M4) reports its effective bit depth.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Audio/AudioFileManager.h"
#include "Utils/Region.h"
#include "Utils/RegionExporter.h"

// ============================================================================
class SaveSampleFormatTests : public juce::UnitTest
{
public:
    SaveSampleFormatTests()
        : juce::UnitTest("Save Sample Format (H1)", "Integration") {}

    void runTest() override
    {
        beginTest("saveAsWav: 32-bit selection writes IEEE float, not int PCM");
        testSaveAsWavFloat();

        beginTest("saveAsWav: 8-bit stays integral");
        testSaveAsWav8Bit();

        beginTest("saveAsWav: 16/24-bit stay integer PCM at the right width");
        testSaveAsWav16And24();

        beginTest("RegionExporter: 32-bit WAV region writes IEEE float");
        testRegionExportFloat();

        beginTest("RegionExporter: FLAC caps 32-bit request at 24-bit (M4)");
        testRegionExportFlacCoercion();
    }

private:
    juce::File tempFile(const juce::String& tag, const juce::String& ext)
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getNonexistentChildFile("WaveEdit_H1_" + tag, ext, false);
    }

    juce::AudioBuffer<float> makeSine(int channels, int numSamples)
    {
        juce::AudioBuffer<float> buffer(channels, numSamples);
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* d = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = 0.5f * std::sin((float) i * 0.02f);
        }
        return buffer;
    }

    struct ReadBack
    {
        bool ok = false;
        bool usesFloat = false;
        int bits = 0;
    };

    ReadBack readBack(const juce::File& f)
    {
        ReadBack r;
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(f));
        if (reader != nullptr)
        {
            r.ok = true;
            r.usesFloat = reader->usesFloatingPointData;
            r.bits = (int) reader->bitsPerSample;
        }
        return r;
    }

    void testSaveAsWavFloat()
    {
        AudioFileManager mgr;
        auto buffer = makeSine(2, 2048);
        auto file = tempFile("float", ".wav");

        bool saved = mgr.saveAsWav(file, buffer, 44100.0, 32, juce::StringPairArray());
        expect(saved, "32-bit save succeeds: " + mgr.getLastError());

        auto rb = readBack(file);
        expect(rb.ok, "file reads back");
        expect(rb.usesFloat, "32-bit WAV must be IEEE float, not integer PCM");
        expectEquals(rb.bits, 32, "reported bit depth is 32");

        file.deleteFile();
    }

    void testSaveAsWav8Bit()
    {
        AudioFileManager mgr;
        auto buffer = makeSine(1, 1024);
        auto file = tempFile("8bit", ".wav");

        bool saved = mgr.saveAsWav(file, buffer, 44100.0, 8, juce::StringPairArray());
        expect(saved, "8-bit save succeeds: " + mgr.getLastError());

        auto rb = readBack(file);
        expect(rb.ok, "file reads back");
        expect(! rb.usesFloat, "8-bit WAV must be integral PCM");
        expectEquals(rb.bits, 8, "reported bit depth is 8");

        file.deleteFile();
    }

    void testSaveAsWav16And24()
    {
        for (int bits : { 16, 24 })
        {
            AudioFileManager mgr;
            auto buffer = makeSine(2, 1500);
            auto file = tempFile("pcm" + juce::String(bits), ".wav");

            bool saved = mgr.saveAsWav(file, buffer, 48000.0, bits, juce::StringPairArray());
            expect(saved, juce::String(bits) + "-bit save succeeds: " + mgr.getLastError());

            auto rb = readBack(file);
            expect(rb.ok, "file reads back");
            expect(! rb.usesFloat, juce::String(bits) + "-bit WAV must be integer PCM");
            expectEquals(rb.bits, bits, "reported bit depth matches request");

            file.deleteFile();
        }
    }

    void testRegionExportFloat()
    {
        auto buffer = makeSine(2, 4096);
        Region region("float_region", 0, 4096);
        auto file = tempFile("region_float", ".wav");

        juce::String err;
        int effective = 0;
        bool ok = RegionExporter::exportSingleRegion(buffer, 44100.0, region, file,
                                                     32, err, "wav", &effective);
        expect(ok, "32-bit region export succeeds: " + err);
        expectEquals(effective, 32, "WAV keeps 32-bit");

        auto rb = readBack(file);
        expect(rb.ok, "file reads back");
        expect(rb.usesFloat, "32-bit region WAV must be IEEE float");
        expectEquals(rb.bits, 32, "reported bit depth is 32");

        file.deleteFile();
    }

    void testRegionExportFlacCoercion()
    {
        auto buffer = makeSine(1, 2048);
        Region region("flac_region", 0, 2048);
        auto file = tempFile("region_flac", ".flac");

        juce::String err;
        int effective = 0;
        bool ok = RegionExporter::exportSingleRegion(buffer, 44100.0, region, file,
                                                     32, err, "flac", &effective);
        expect(ok, "FLAC region export succeeds: " + err);
        expectEquals(effective, 24, "FLAC caps a 32-bit request at 24-bit");

        auto rb = readBack(file);
        expect(rb.ok, "FLAC file reads back");
        expect(! rb.usesFloat, "FLAC is integer, not float");

        file.deleteFile();
    }
};

static SaveSampleFormatTests saveSampleFormatTests;
