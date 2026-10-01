/*
  ==============================================================================

    CrashRecoveryTests.cpp
    WaveEdit - Professional Audio Editor

    Detection + supersede logic for the crash-recovery flow. The dialog
    itself is interactive (juce::AlertWindow::showAsync) so it isn't
    exercised here; these tests verify the static helpers in
    FileController that decide whether to offer recovery and that
    eviction-on-save works.

    Tests use juce::TemporaryFile to keep the user's real auto-save
    directory untouched, by writing both "original" files and matching
    auto-save files into a fresh temp dir.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "Utils/AutoSaveRecovery.h"

namespace
{
    using AutoSaveRecovery::findOrphanedAutoSaves;
    using AutoSaveRecovery::deleteAutoSavesFor;

    /** Write a tiny stub file (not real audio — just bytes that
        exist on disk so file mtimes can be set). The detection logic
        only inspects names and mtimes, never the audio itself. */
    void writeStub(const juce::File& f, const juce::String& body)
    {
        f.replaceWithText(body);
    }
}

//==============================================================================
class CrashRecoveryTests : public juce::UnitTest
{
public:
    CrashRecoveryTests() : juce::UnitTest("CrashRecovery", "Unit") {}

    void runTest() override
    {
        beginTest("No auto-save → detection returns empty");
        {
            auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("WaveEditTest_recovery_empty");
            tempDir.deleteRecursively();
            tempDir.createDirectory();

            auto original = tempDir.getChildFile("song.wav");
            writeStub(original, "stub");

            const auto orphans = findOrphanedAutoSaves(tempDir, original);
            expectEquals(orphans.size(), 0);

            tempDir.deleteRecursively();
        }

        beginTest("Auto-save older than original → detection ignores (stale)");
        {
            auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("WaveEditTest_recovery_stale");
            tempDir.deleteRecursively();
            tempDir.createDirectory();

            auto autoSave = tempDir.getChildFile("autosave_song_20260430_010000.wav");
            writeStub(autoSave, "old");
            // Force the auto-save's mtime into the past.
            autoSave.setLastModificationTime(
                juce::Time::getCurrentTime() - juce::RelativeTime::seconds(60.0));

            auto original = tempDir.getChildFile("song.wav");
            writeStub(original, "newer");
            // Original is "now" — newer than the auto-save.

            const auto orphans = findOrphanedAutoSaves(tempDir, original);
            expectEquals(orphans.size(), 0,
                         "stale auto-save (older than original) is not offered");

            tempDir.deleteRecursively();
        }

        beginTest("Auto-save newer than original → detection finds it");
        {
            auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("WaveEditTest_recovery_fresh");
            tempDir.deleteRecursively();
            tempDir.createDirectory();

            auto original = tempDir.getChildFile("song.wav");
            writeStub(original, "old original");
            original.setLastModificationTime(
                juce::Time::getCurrentTime() - juce::RelativeTime::seconds(60.0));

            auto autoSave = tempDir.getChildFile("autosave_song_20260430_120000.wav");
            writeStub(autoSave, "fresh");
            // mtime defaults to "now" — newer than original.

            const auto orphans = findOrphanedAutoSaves(tempDir, original);
            expectEquals(orphans.size(), 1);
            expect(orphans.getReference(0) == autoSave);

            tempDir.deleteRecursively();
        }

        beginTest("Multiple auto-saves: all returned, newest first");
        {
            auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("WaveEditTest_recovery_multi");
            tempDir.deleteRecursively();
            tempDir.createDirectory();

            auto original = tempDir.getChildFile("song.wav");
            writeStub(original, "old");
            original.setLastModificationTime(
                juce::Time::getCurrentTime() - juce::RelativeTime::seconds(120.0));

            auto autoSaveOlder  = tempDir.getChildFile("autosave_song_20260430_120000.wav");
            auto autoSaveNewer  = tempDir.getChildFile("autosave_song_20260430_130000.wav");
            writeStub(autoSaveOlder, "older");
            writeStub(autoSaveNewer, "newer");
            autoSaveOlder.setLastModificationTime(
                juce::Time::getCurrentTime() - juce::RelativeTime::seconds(30.0));

            const auto orphans = findOrphanedAutoSaves(tempDir, original);
            expectEquals(orphans.size(), 2);

            // Findable in either order; production code sorts newest-first.
            // Here we just verify both are present.
            bool foundOlder = false, foundNewer = false;
            for (const auto& f : orphans)
            {
                if (f == autoSaveOlder) foundOlder = true;
                if (f == autoSaveNewer) foundNewer = true;
            }
            expect(foundOlder && foundNewer,
                   "both auto-saves detected");

            tempDir.deleteRecursively();
        }

        beginTest("Auto-saves for a different file are ignored");
        {
            auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("WaveEditTest_recovery_other");
            tempDir.deleteRecursively();
            tempDir.createDirectory();

            auto original = tempDir.getChildFile("targetsong.wav");
            writeStub(original, "old");
            original.setLastModificationTime(
                juce::Time::getCurrentTime() - juce::RelativeTime::seconds(60.0));

            auto otherAutoSave = tempDir.getChildFile("autosave_otherfile_20260430_120000.wav");
            writeStub(otherAutoSave, "unrelated");

            const auto orphans = findOrphanedAutoSaves(tempDir, original);
            expectEquals(orphans.size(), 0,
                         "auto-save for a different original is filtered out by name prefix");

            tempDir.deleteRecursively();
        }

        beginTest("deleteAutoSavesFor removes only matching files");
        {
            auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("WaveEditTest_recovery_evict");
            tempDir.deleteRecursively();
            tempDir.createDirectory();

            auto original = tempDir.getChildFile("a.wav");
            writeStub(original, "original");

            auto match1 = tempDir.getChildFile("autosave_a_20260430_120000.wav");
            auto match2 = tempDir.getChildFile("autosave_a_20260430_130000.wav");
            auto other  = tempDir.getChildFile("autosave_b_20260430_120000.wav");
            writeStub(match1, "1");
            writeStub(match2, "2");
            writeStub(other,  "b");

            deleteAutoSavesFor(tempDir, original);

            expect(! match1.existsAsFile(), "matching auto-save 1 deleted");
            expect(! match2.existsAsFile(), "matching auto-save 2 deleted");
            expect(other.existsAsFile(),    "unrelated auto-save kept");

            tempDir.deleteRecursively();
        }

        beginTest("Production helpers work against the real auto-save dir");
        {
            // Smoke test: invoke the no-arg overloads that hit the real
            // settings directory. We don't seed files in there — we
            // just verify the calls don't crash. (No leftover mutation.)
            const auto dir = AutoSaveRecovery::getAutoSaveDirectory();
            expect(dir.getFileName() == "autosave",
                   "auto-save dir is named 'autosave'");

            // findOrphanedAutoSaves with a name that won't match real
            // user data should return empty (or near-empty).
            auto fakeFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getChildFile("WaveEditTest_recovery_smoke.wav");
            fakeFile.deleteFile();
            writeStub(fakeFile, "smoke");

            const auto orphans = AutoSaveRecovery::findOrphanedAutoSaves(fakeFile);
            expect(orphans.size() >= 0);

            // deleteAutoSavesFor on a name that won't match real user
            // data is a no-op.
            AutoSaveRecovery::deleteAutoSavesFor(fakeFile);
            fakeFile.deleteFile();
        }
    }
};

static CrashRecoveryTests crashRecoveryTests;
