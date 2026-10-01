/*
  ==============================================================================

    AutoSaveRecoveryPass2Tests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Regression tests for Pass 2 latent-defect fixes in the auto-save /
    crash-recovery path and the file-drop predicate:
      - M4 : auto-save prefix disambiguates same-stem files in different dirs.
      - M5 : the prefix is robust for stems containing underscores.
      - H26: the file-drop predicate accepts WAV/FLAC/MP3/OGG, not just WAV.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "../../Source/Utils/AutoSaveRecovery.h"
#include "../../Source/Controllers/FileController.h"

//==============================================================================
class AutoSaveRecoveryPass2Tests : public juce::UnitTest
{
public:
    AutoSaveRecoveryPass2Tests()
        : juce::UnitTest("AutoSaveRecovery Pass 2", "AutoSave") {}

    void runTest() override
    {
        testPrefixDisambiguatesByPath();   // M4
        testPrefixRobustWithUnderscores(); // M5
        testDropPredicateAcceptsFormats(); // H26
    }

private:
    //==========================================================================
    // M4: /A/kick.wav and /B/kick.wav must get DISTINCT auto-save prefixes so
    // their recovery audio can't be confused.
    void testPrefixDisambiguatesByPath()
    {
        beginTest("M4 - auto-save prefix disambiguates same stem, different dirs");

        juce::File a("/projects/A/kick.wav");
        juce::File b("/projects/B/kick.wav");

        const auto pa = AutoSaveRecovery::autoSavePrefixFor(a);
        const auto pb = AutoSaveRecovery::autoSavePrefixFor(b);

        expect(pa != pb,
               "Same stem in different directories must yield different prefixes");

        // Both still start with the human-readable stem and end with '_'.
        expect(pa.startsWith("autosave_kick_"), "Prefix should keep the stem");
        expect(pa.endsWith("_"), "Prefix should end with separator before timestamp");

        // A given path is stable (deterministic) across calls.
        expect(AutoSaveRecovery::autoSavePrefixFor(a) == pa,
               "Prefix must be deterministic for the same path");
    }

    //==========================================================================
    // M5: a stem containing underscores ("my_kick_01") must still produce a
    // prefix whose timestamp can be unambiguously stripped. We verify by
    // reconstructing a full auto-save filename and confirming the grouping
    // key (everything before the trailing date_time) equals the prefix sans
    // its own trailing underscore.
    void testPrefixRobustWithUnderscores()
    {
        beginTest("M5 - prefix robust for stems with underscores");

        juce::File f("/sfx/my_kick_01.wav");
        const auto prefix = AutoSaveRecovery::autoSavePrefixFor(f);

        expect(prefix.startsWith("autosave_my_kick_01_"),
               "Stem with underscores must be preserved verbatim, got: " + prefix);

        // Simulate a written filename and the cleanup grouping logic: drop the
        // last two underscore tokens (date, time); the remainder is the group
        // key and must be identical for two timestamps of the same original.
        auto groupKeyOf = [](const juce::String& stemNoExt)
        {
            juce::StringArray parts = juce::StringArray::fromTokens(stemNoExt, "_", "");
            if (parts.size() < 5)
                return juce::String();
            parts.removeRange(parts.size() - 2, 2);
            return parts.joinIntoString("_");
        };

        const juce::String name1 = prefix + "20250529_120000";
        const juce::String name2 = prefix + "20250529_130000";

        const auto k1 = groupKeyOf(name1);
        const auto k2 = groupKeyOf(name2);

        expect(k1.isNotEmpty(), "Group key should be parseable");
        expect(k1 == k2,
               "Two timestamps of the same original must share a group key");

        // And a DIFFERENT original ("my.wav") must NOT collide with this one.
        juce::File other("/sfx/my.wav");
        const juce::String otherName =
            AutoSaveRecovery::autoSavePrefixFor(other) + "20250529_120000";
        expect(groupKeyOf(otherName) != k1,
               "Different original must not share the underscore-stem group key");
    }

    //==========================================================================
    // H26: drop predicate accepts every format the Open dialog advertises.
    void testDropPredicateAcceptsFormats()
    {
        beginTest("H26 - file-drop predicate accepts WAV/FLAC/MP3/OGG");

        expect(FileController::isDroppableAudioFile(juce::File("/x/a.wav")), "WAV accepted");
        expect(FileController::isDroppableAudioFile(juce::File("/x/a.flac")), "FLAC accepted");
        expect(FileController::isDroppableAudioFile(juce::File("/x/a.mp3")), "MP3 accepted");
        expect(FileController::isDroppableAudioFile(juce::File("/x/a.ogg")), "OGG accepted");

        // Case-insensitive.
        expect(FileController::isDroppableAudioFile(juce::File("/x/a.FLAC")),
               "Uppercase extension accepted");

        // Non-audio rejected.
        expect(! FileController::isDroppableAudioFile(juce::File("/x/a.txt")),
               "Non-audio extension rejected");
        expect(! FileController::isDroppableAudioFile(juce::File("/x/a.wavx")),
               "Near-miss extension rejected");
    }
};

static AutoSaveRecoveryPass2Tests autoSaveRecoveryPass2Tests;
