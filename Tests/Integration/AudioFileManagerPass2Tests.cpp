/*
  ==============================================================================

    AudioFileManagerPass2Tests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Regression tests for Pass 2 latent-defect fixes in the file I/O path:
      - C11: iXML save must preserve the BWF bext chunk (and vice versa).
      - C15: resampleBuffer must produce correct audio past the first chunk.
      - M7 : iXML field text must be XML-entity-escaped (round-trip with &<>).
      - M8 : appendiXMLChunk must reject a crafted oversized chunk-size field.

    Each test fails on the pre-fix code and passes after the fix.

  ==============================================================================
*/

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>
#include "../../Source/Audio/AudioFileManager.h"
#include "../../Source/Utils/BWFMetadata.h"
#include "../../Source/Utils/iXMLMetadata.h"
#include "../TestUtils/TestAudioFiles.h"

//==============================================================================
class AudioFileManagerPass2Tests : public juce::UnitTest
{
public:
    AudioFileManagerPass2Tests()
        : juce::UnitTest("AudioFileManager Pass 2", "Integration") {}

    void runTest() override
    {
        testBextSurvivesiXMLSave();        // C11
        testResampleProducesValidAudio();  // C15
        testiXMLEntityEscapingRoundTrip(); // M7
        testOversizedChunkRejected();      // M8
    }

private:
    juce::File tempFile(const juce::String& name)
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile(name);
    }

    //==========================================================================
    // C11: BWF bext AND iXML must both survive an iXML-embedding save.
    void testBextSurvivesiXMLSave()
    {
        beginTest("C11 - bext survives iXML save round-trip");

        auto file = tempFile("pass2_bext_ixml.wav");
        file.deleteFile();

        auto buffer = TestAudio::createSineWave(440.0, 0.5f, 44100.0, 0.25, 2);

        // BWF metadata.
        BWFMetadata bwf;
        bwf.setDescription("Pass2 BWF description");
        bwf.setOriginator("ZQ SFX WaveEdit");
        bwf.setOriginatorRef("REF42");
        bwf.setOriginationDate("2025-05-29");
        bwf.setOriginationTime("12:00:00");
        bwf.setTimeReference(44100);

        // iXML metadata.
        iXMLMetadata ixml;
        ixml.setProject("Pass2Project");
        ixml.setCategory("DOOR");
        ixml.setSubcategory("Wood");

        AudioFileManager fm;

        // Mirror Document::saveFile: write WAV with bext metadata, then append
        // the iXML chunk over the top of the just-written file.
        bool saved = fm.saveAsWav(file, buffer, 44100.0, 16, bwf.toJUCEMetadata());
        expect(saved, "Save with BWF metadata should succeed");

        bool appended = fm.appendiXMLChunk(file, ixml.toXMLString());
        expect(appended, "Appending iXML chunk should succeed");

        // Reload BOTH metadata kinds.
        AudioFileInfo info;
        expect(fm.getFileInfo(file, info), "Should read file info back");

        BWFMetadata loadedBwf;
        loadedBwf.fromJUCEMetadata(info.metadata);
        expect(loadedBwf.getDescription() == "Pass2 BWF description",
               "BWF description must survive the iXML append (bext not stripped)");
        expect(loadedBwf.getOriginatorRef() == "REF42",
               "BWF originator ref must survive the iXML append");

        iXMLMetadata loadedIxml;
        expect(loadedIxml.loadFromFile(file), "iXML chunk should be readable");
        expect(loadedIxml.getProject() == "Pass2Project",
               "iXML project must survive");
        expect(loadedIxml.getCategory() == "DOOR",
               "iXML category must survive");

        file.deleteFile();
    }

    //==========================================================================
    // C15: resample a ramp and verify it stays monotonic / non-zero past the
    // first 4096-sample chunk (the old code re-read input from 0 each chunk
    // and garbled everything after the first chunk).
    void testResampleProducesValidAudio()
    {
        beginTest("C15 - resampleBuffer produces valid audio past first chunk");

        // ~1 second mono ramp at 44100 -> ~48000 output samples (>> 4096).
        auto ramp = TestAudio::createLinearRamp(-0.9f, 0.9f, 44100.0, 1.0, 1);

        auto out = AudioFileManager::resampleBuffer(ramp, 44100.0, 48000.0);

        const int n = out.getNumSamples();
        expect(n > 4096 * 3, "Output should span multiple processing chunks");

        const float* d = out.getReadPointer(0);

        // The ramp is monotonically increasing; a correct resample preserves
        // that across the INTERIOR. The pre-fix bug re-read input from sample 0
        // every 4096-output chunk, producing big backward jumps at each chunk
        // boundary (sample ~4096, ~8192, ...) and garbage thereafter -- those
        // land squarely in the interior and would show up below.
        //
        // We skip the first/last few samples: LagrangeInterpolator is a 5-point
        // FIR whose zero-initialised history makes the first handful of outputs
        // a startup transient (here the ramp starts at -0.9, so outputs briefly
        // settle from ~0 down to -0.9 -- a real, expected edge artifact of any
        // FIR resampler, not the C15 corruption).
        const int prime = 8;  // skip interpolator startup/teardown priming
        int regressions = 0;
        for (int i = prime + 1; i < n - prime; ++i)
            if (d[i] < d[i - 1] - 0.01f)
                ++regressions;

        expect(regressions == 0,
               "Resampled ramp interior must be monotonic (no chunk-boundary "
               "jumps); found " + juce::String(regressions) + " regressions");

        // The tail (well past the first chunk) must not be all zeros, and
        // should be near the ramp's end value (~0.9).
        const float lastVal = d[n - 1];
        expect(lastVal > 0.5f,
               "Tail of resampled ramp should approach end value, got "
               + juce::String(lastVal));

        // Also verify a sample deep into the buffer (chunk 4) is non-trivial.
        const float midLate = d[4096 * 4];
        expect(std::abs(midLate) > 0.0001f,
               "Sample past the first chunks must be real data, not zero");
    }

    //==========================================================================
    // M7: a field value with & < > must round-trip (escaped on write, parsed
    // on read). Pre-fix, the raw '&' broke XML parsing and ALL iXML was lost.
    void testiXMLEntityEscapingRoundTrip()
    {
        beginTest("M7 - iXML entity escaping round-trip");

        iXMLMetadata ixml;
        ixml.setProject("Test & Demo <v2>");
        ixml.setDescription("a < b && c > d");

        const juce::String xml = ixml.toXMLString();

        // Raw special chars must NOT appear unescaped inside the value.
        expect(xml.contains("&amp;"), "Ampersand should be escaped to &amp;");
        expect(xml.contains("&lt;"), "Less-than should be escaped to &lt;");
        expect(xml.contains("&gt;"), "Greater-than should be escaped to &gt;");

        // And the XML must actually parse + round-trip the values.
        iXMLMetadata parsed;
        expect(parsed.fromXMLString(xml),
               "Escaped iXML must parse (pre-fix this failed -> all iXML lost)");
        expect(parsed.getProject() == "Test & Demo <v2>",
               "Project value must round-trip exactly, got: " + parsed.getProject());
        expect(parsed.getDescription() == "a < b && c > d",
               "Description value must round-trip exactly");
    }

    //==========================================================================
    // M8: a WAV whose chunk declares a near-UINT32_MAX size must not pass the
    // bounds check and trigger an out-of-range read; appendiXMLChunk should
    // simply stop parsing at the bad chunk (and not crash).
    void testOversizedChunkRejected()
    {
        beginTest("M8 - oversized chunk-size field rejected");

        auto file = tempFile("pass2_oversized_chunk.wav");
        file.deleteFile();

        // Build a minimal valid RIFF/WAVE with a fmt chunk, then a bogus
        // chunk whose declared size is 0xFFFFFFF0 (way past EOF).
        juce::MemoryOutputStream mos;
        mos.write("RIFF", 4);
        mos.writeInt(0);            // riff size placeholder
        mos.write("WAVE", 4);

        // fmt chunk (16 bytes of PCM fmt).
        mos.write("fmt ", 4);
        mos.writeInt(16);
        mos.writeShort(1);          // PCM
        mos.writeShort(1);          // mono
        mos.writeInt(44100);        // sample rate
        mos.writeInt(88200);        // byte rate
        mos.writeShort(2);          // block align
        mos.writeShort(16);         // bits per sample

        // Bogus chunk with an enormous declared size but no data.
        mos.write("junk", 4);
        mos.writeInt(static_cast<int>(0xFFFFFFF0u));  // huge size, overflows naive math

        // Patch RIFF size.
        auto* raw = static_cast<char*>(const_cast<void*>(mos.getData()));
        uint32_t riffSize = static_cast<uint32_t>(mos.getDataSize()) - 8;
        memcpy(raw + 4, &riffSize, 4);

        file.replaceWithData(mos.getData(), mos.getDataSize());

        AudioFileManager fm;

        // Should not crash, should not over-read. The crafted chunk is
        // skipped (its size exceeds remaining bytes) so the rebuilt file is
        // produced from the valid chunks only. Just assert it returns without
        // UB and the file still exists / is non-empty.
        bool result = fm.appendiXMLChunk(file, "<BWFXML><PROJECT>X</PROJECT></BWFXML>");
        expect(result, "appendiXMLChunk should complete on a crafted oversized chunk");
        expect(file.getSize() > 12, "Resulting file should still be a valid RIFF container");

        file.deleteFile();
    }
};

static AudioFileManagerPass2Tests audioFileManagerPass2Tests;
