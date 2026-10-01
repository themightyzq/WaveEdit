/*
  ==============================================================================

    AtomicSaveTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Integration tests for atomic saving in AudioFileManager::saveAudioFile
    (v0.9.1). Non-WAV saves (FLAC, AIFF, OGG, MP3) used to delete the user's
    existing file before writing the replacement; a writer-creation failure,
    an encode failure, or a full disk left the user with no file at all. The
    fix writes to a sibling temp file and only swaps it onto the target after
    a fully successful write, mirroring the pattern already used by
    saveAsWav().

    These tests cover:
      - Overwrite-in-place still works for FLAC, AIFF, OGG (and MP3 when the
        LAME encoder is compiled in): saving new audio over an existing file
        succeeds and the new content reads back.
      - No leftover hidden temp file remains in the directory after a
        successful save.
      - A save that fails AFTER the point where the old code deleted the
        original (writer creation) leaves the pre-existing file byte-for-byte
        untouched. A 9-channel FLAC save is used to trigger this: FLAC's
        encoder rejects more than 8 channels at stream-encoder init time
        (FLAC__MAX_CHANNELS, see juce_FlacAudioFormat.cpp / libFLAC
        stream_encoder.c), and AudioFileManager::saveAudioFile does not
        validate channel count for FLAC before reaching that point, so the
        failure surfaces exactly where the old delete-then-write code would
        already have removed the original file.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Audio/AudioFileManager.h"
#include "TestAudioFiles.h"

namespace
{
juce::File getTestDir()
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("WaveEditAtomicSaveTests");
}

// Every visible AND hidden entry directly inside a directory, so a leaked
// hidden TemporaryFile (useHiddenFile prefixes the name with ".") is caught.
int countAllEntries(const juce::File& dir)
{
    juce::Array<juce::File> entries;
    dir.findChildFiles(entries, juce::File::findFilesAndDirectories, false, "*");
    return entries.size();
}
} // namespace

//==============================================================================
class AtomicSaveTests : public juce::UnitTest
{
public:
    AtomicSaveTests() : juce::UnitTest("Atomic Save (v0.9.1)", "Integration") {}

    void runTest() override
    {
        auto testDir = getTestDir();
        if (testDir.exists())
            testDir.deleteRecursively();
        testDir.createDirectory();

        testOverwriteInPlace(".flac", "atomic_overwrite.flac");
        testOverwriteInPlace(".aiff", "atomic_overwrite.aiff");
        testOverwriteInPlace(".ogg", "atomic_overwrite.ogg");
#if WAVEEDIT_HAVE_LAME
        testOverwriteInPlace(".mp3", "atomic_overwrite.mp3");
#endif

        testFailedSaveLeavesOriginalUntouched();

        testDir.deleteRecursively();
    }

private:
    // Saves audio A, then saves different audio B over the same path. The
    // save must succeed, the file must read back with B's length, and no
    // hidden temp file must be left behind in the directory.
    void testOverwriteInPlace(const juce::String& extension, const juce::String& fileName)
    {
        beginTest("Overwrite in place: " + extension);

        auto testDir = getTestDir();
        auto file = testDir.getChildFile(fileName);
        if (file.existsAsFile())
            file.deleteFile();

        const double sampleRate = 44100.0;
        auto bufferA = TestAudio::createSineWave(440.0, 0.5f, sampleRate, 1.0, 2);
        auto bufferB = TestAudio::createSineWave(220.0, 0.4f, sampleRate, 0.5, 2);

        AudioFileManager mgr;

        expect(mgr.saveAudioFile(file, bufferA, sampleRate, 16, 5),
               "initial save (A) succeeds: " + mgr.getLastError());
        expect(file.existsAsFile(), "file exists after initial save");

        const int entriesAfterA = countAllEntries(testDir);

        expect(mgr.saveAudioFile(file, bufferB, sampleRate, 16, 5),
               "overwrite save (B) succeeds: " + mgr.getLastError());
        expect(file.existsAsFile(), "file exists after overwrite save");

        std::unique_ptr<juce::AudioFormatReader> reader(mgr.createReaderFor(file));
        expect(reader != nullptr, "file reads back after overwrite: " + mgr.getLastError());

        if (reader != nullptr)
        {
            // Compressed formats can pad/trim by a small number of samples;
            // check the new (shorter) length is what landed, not A's length.
            const juce::int64 lengthA = (juce::int64) bufferA.getNumSamples();
            const juce::int64 lengthB = (juce::int64) bufferB.getNumSamples();
            expect(reader->lengthInSamples < lengthA,
                   "readback length reflects B's shorter content, not A's");
            expect(std::abs(reader->lengthInSamples - lengthB) < lengthB,
                   "readback length is in the right ballpark for B");
        }

        const int entriesAfterB = countAllEntries(testDir);
        expectEquals(entriesAfterB, entriesAfterA,
                     "no leftover hidden temp file after overwrite");
    }

    // A FLAC save with 9 channels fails at writer creation (FLAC caps at 8
    // channels), which is AFTER the point where the old code deleted the
    // pre-existing target file. On the old code this test would fail: the
    // delete-existing-file step ran unconditionally before
    // format->createWriterFor() was ever called, so the pre-existing file
    // would already be gone by the time writer creation failed, leaving no
    // file at all instead of the original. With the fix, the write happens
    // on a sibling temp file and the target is only replaced after a fully
    // successful write, so a writer-creation failure leaves the original
    // file's bytes completely unchanged.
    void testFailedSaveLeavesOriginalUntouched()
    {
        beginTest("Failed FLAC save (9 channels) leaves original untouched");

        auto testDir = getTestDir();
        auto file = testDir.getChildFile("atomic_failure.flac");
        if (file.existsAsFile())
            file.deleteFile();

        const double sampleRate = 44100.0;
        auto original = TestAudio::createSineWave(440.0, 0.5f, sampleRate, 1.0, 2);

        AudioFileManager mgr;
        expect(mgr.saveAudioFile(file, original, sampleRate, 16, 5),
               "pre-existing FLAC file is created: " + mgr.getLastError());
        expect(file.existsAsFile(), "pre-existing file exists before the failing save");

        juce::MemoryBlock originalBytes;
        expect(file.loadFileAsData(originalBytes), "can read pre-existing file bytes");

        // 9 channels: passes AudioFileManager::saveAudioFile's own checks
        // (it does not cap channel count for FLAC) but is rejected by
        // libFLAC's stream encoder at init (FLAC__MAX_CHANNELS == 8), so
        // format->createWriterFor() returns nullptr.
        juce::AudioBuffer<float> nineChannelBuffer(9, 1000);
        nineChannelBuffer.clear();
        for (int ch = 0; ch < nineChannelBuffer.getNumChannels(); ++ch)
        {
            float* data = nineChannelBuffer.getWritePointer(ch);
            for (int i = 0; i < nineChannelBuffer.getNumSamples(); ++i)
                data[i] = 0.1f;
        }

        const bool saveResult = mgr.saveAudioFile(file, nineChannelBuffer, sampleRate, 16, 5);
        expect(! saveResult, "9-channel FLAC save reports failure");

        expect(file.existsAsFile(), "original file still exists after the failed save");

        juce::MemoryBlock bytesAfterFailure;
        expect(file.loadFileAsData(bytesAfterFailure), "can read file bytes after the failed save");

        expect(originalBytes.getSize() == bytesAfterFailure.getSize()
                   && originalBytes == bytesAfterFailure,
               "original file bytes are unchanged by the failed save");

        juce::Array<juce::File> hiddenEntries;
        testDir.findChildFiles(hiddenEntries, juce::File::findFiles, false, ".*");
        expect(hiddenEntries.isEmpty(), "no hidden temp file left after the failed save");
    }
};

static AtomicSaveTests atomicSaveTests;
