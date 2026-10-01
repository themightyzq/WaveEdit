/*
  ==============================================================================

    SaveIdentityTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    v0.9.1 regression guards for the document's save identity and format.

    - Cmd+S after Save As wrote to the ORIGINAL file: FileController took the
      target from AudioEngine::getCurrentFile(), which Save As never updates.
      Document::getFile() is now the one canonical identity.
    - In-place saves (Cmd+S, tab close, quit) must keep the format that is on
      disk. Tab close used Document::saveFile's old default of 16-bit.
    - A rate-converting Save As is repeated by later in-place saves, until the
      buffer's own rate changes.

    The in-place route through FileController shows no UI, so it runs headless.

  ==============================================================================
*/

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include "../../Source/Audio/AudioFileManager.h"
#include "../../Source/Controllers/FileController.h"
#include "../../Source/Utils/Document.h"
#include "../../Source/Utils/DocumentManager.h"
#include "../../Source/Utils/MarkerManager.h"
#include "../../Source/Utils/RegionManager.h"
#include "../TestUtils/TestAudioFiles.h"

class SaveIdentityTests : public juce::UnitTest
{
public:
    SaveIdentityTests() : juce::UnitTest("Save Identity (v0.9.1)", "Integration") {}

    void runTest() override
    {
        testSaveAfterSaveAsTargetsNewFile();
        testInPlaceSaveKeepsBitDepth();
        testRateConvertingSaveAsIsRepeatedUntilBufferRateChanges();
        testResolveSaveRoute();
        testUnmodifiedCloseNeedsNoPrompt();
    }

private:
    juce::File dir;

    void initialise() override
    {
        dir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                  .getChildFile("WaveEditSaveIdentityTests");
        dir.deleteRecursively();
        dir.createDirectory();
    }

    void shutdown() override { dir.deleteRecursively(); }

    juce::File makeWav(const juce::String& name, int bitDepth, double rate = 44100.0)
    {
        auto file = dir.getChildFile(name);
        auto buffer = TestAudio::createSineWave(440.0, 0.5f, rate, 0.5, 2);
        AudioFileManager fm;
        expect(fm.saveAsWav(file, buffer, rate, bitDepth), "fixture WAV should save");
        return file;
    }

    struct Info { double rate = 0.0; int bits = 0; juce::int64 length = 0; };

    Info readInfo(const juce::File& file)
    {
        juce::AudioFormatManager fmt;
        fmt.registerBasicFormats();
        Info info;
        if (std::unique_ptr<juce::AudioFormatReader> r { fmt.createReaderFor(file) })
        {
            info.rate = r->sampleRate;
            info.bits = static_cast<int>(r->bitsPerSample);
            info.length = r->lengthInSamples;
        }
        return info;
    }

    // Edits the document the way a DSP apply does, then marks it modified.
    void halveGain(Document& doc)
    {
        doc.getBufferManager().getMutableBuffer().applyGain(0.5f);
        doc.setModified(true);
    }

    void testSaveAfterSaveAsTargetsNewFile()
    {
        beginTest("Cmd+S after Save As writes the new file, never the original");

        const auto original = makeWav("original.wav", 24);
        const auto saveAsTarget = dir.getChildFile("renamed.wav");
        juce::MemoryBlock originalBytes;
        expect(original.loadFileAsData(originalBytes));

        DocumentManager docs;
        AudioFileManager fm;
        FileController files(docs, fm);

        auto* doc = docs.openDocument(original);
        expect(doc != nullptr, "document should open");
        if (doc == nullptr) return;

        // What Save As does after its dialog.
        expect(doc->saveFile(saveAsTarget, 24, 10, 0.0), "Save As write");
        expect(doc->getFile() == saveAsTarget, "Save As moves the document's identity");

        halveGain(*doc);
        files.saveFile(doc);

        juce::MemoryBlock afterBytes;
        expect(original.loadFileAsData(afterBytes));
        expect(afterBytes == originalBytes, "The original file must be byte-identical");
        expect(!doc->isModified(), "Cmd+S must have saved the document");
        expect(saveAsTarget.getLastModificationTime() >= original.getLastModificationTime(),
               "The Save As target received the edit");
    }

    void testInPlaceSaveKeepsBitDepth()
    {
        beginTest("In-place saves keep the file's bit depth (tab close used to force 16-bit)");

        const auto file = makeWav("depth24.wav", 24);
        DocumentManager docs;
        AudioFileManager fm;
        FileController files(docs, fm);

        auto* doc = docs.openDocument(file);
        expect(doc != nullptr);
        if (doc == nullptr) return;

        expectEquals(doc->getSaveBitDepth(), 24, "Seeded from the file on load");

        halveGain(*doc);
        files.saveFile(doc);
        expectEquals(readInfo(file).bits, 24, "Cmd+S keeps 24-bit");

        expect(doc->saveFile(dir.getChildFile("depth16.wav"), 16, 10, 0.0));
        expectEquals(doc->getSaveBitDepth(), 16, "A Save As sets the new on-disk format");
    }

    void testRateConvertingSaveAsIsRepeatedUntilBufferRateChanges()
    {
        beginTest("A rate-converting Save As is kept by Cmd+S until the buffer rate changes");

        const auto file = makeWav("rate44.wav", 24, 44100.0);
        const auto converted = dir.getChildFile("rate48.wav");

        DocumentManager docs;
        AudioFileManager fm;
        FileController files(docs, fm);

        auto* doc = docs.openDocument(file);
        expect(doc != nullptr);
        if (doc == nullptr) return;

        expect(doc->saveFile(converted, 24, 10, 48000.0), "Save As at 48 kHz");
        expectEquals(doc->getSaveTargetSampleRate(), 48000.0);

        halveGain(*doc);
        files.saveFile(doc);
        expectEquals(readInfo(converted).rate, 48000.0, "Cmd+S must not rewrite the 48k file at 44.1k");

        // Process > Resample replaces the buffer at a new rate: the old
        // conversion target no longer applies.
        auto buffer = doc->getBufferManager().getBuffer();
        doc->getBufferManager().setBuffer(buffer, 22050.0);
        expectEquals(doc->getSaveTargetSampleRate(), 0.0);
    }

    void testResolveSaveRoute()
    {
        beginTest("resolveSaveRoute decides from the document's own file");

        using Route = FileController::SaveRoute;

        Document untitled;
        expect(FileController::resolveSaveRoute(untitled) == Route::NeedsSaveAs, "Untitled needs Save As");

        const auto m4a = dir.getChildFile("song.m4a");
        expect(m4a.replaceWithText("not really audio"));
        Document readOnlyFormat(m4a);
        expect(FileController::resolveSaveRoute(readOnlyFormat) == Route::NeedsSaveAs,
               "Decode-only formats need Save As");

        const auto wav = makeWav("route.wav", 16);
        Document writable(wav);
        expect(FileController::resolveSaveRoute(writable) == Route::InPlace, "A writable WAV saves in place");

        expect(wav.setReadOnly(true));
        expect(FileController::resolveSaveRoute(writable) == Route::NoWriteAccess, "A locked file is reported");
        wav.setReadOnly(false);
    }

    void testUnmodifiedCloseNeedsNoPrompt()
    {
        beginTest("Closing an unmodified document closes it without a prompt");

        DocumentManager docs;
        AudioFileManager fm;
        FileController files(docs, fm);

        auto* doc = docs.openDocument(makeWav("close.wav", 16));
        expect(doc != nullptr);
        expectEquals(docs.getNumDocuments(), 1);
        expect(files.closeDocumentWithPrompt(doc), "An unmodified document closes");
        expectEquals(docs.getNumDocuments(), 0);

        // An empty (never loaded) document must be closable too; it used to be
        // skipped by an isFileLoaded() early return.
        docs.createDocument();
        expect(files.closeAllFiles(), "closeAllFiles closes unmodified documents");
        expectEquals(docs.getNumDocuments(), 0);
    }
};

static SaveIdentityTests saveIdentityTests;
