/*
  ==============================================================================

    NamingTokensTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Tests for the shared filename-template substitution helper (Finding #24):
    both RegionExporter::generateFilename() and
    BatchProcessorSettings::applyNamingPattern() call through NamingTokens
    instead of maintaining independent .replace() chains. These tests cover
    the shared helper's substitution logic directly; end-to-end coverage for
    generateFilename()/applyNamingPattern() themselves already lives in
    RegionExporterPass2Tests.cpp and BatchJobTests.cpp.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "Utils/NamingTokens.h"

// ============================================================================
class NamingTokensTests : public juce::UnitTest
{
public:
    NamingTokensTests()
        : juce::UnitTest("NamingTokens", "RegionManager") {}

    void runTest() override
    {
        beginTest("Legacy region-export tokens substitute unchanged");
        testRegionExportTokens();

        beginTest("Legacy batch tokens substitute unchanged");
        testBatchTokens();

        beginTest("New audio-format tokens substitute via addAudioFormatTokens");
        testAudioFormatTokens();

        beginTest("Unknown/unsupplied tokens are left untouched");
        testUnknownTokensUntouched();

        beginTest("{index:03} zero-pads correctly");
        testPaddedIndex();
    }

private:
    using Token = waveedit::NamingTokens::Token;

    void testRegionExportTokens()
    {
        // The four tokens RegionExporter::generateFilename() supports.
        std::vector<Token> tokens = {
            { "{basename}", "drums" },
            { "{region}", "Verse" },
            { "{index}", "3" },
            { "{N}", "003" },
        };

        expectEquals(waveedit::NamingTokens::substitute("{basename}", tokens), juce::String("drums"));
        expectEquals(waveedit::NamingTokens::substitute("{region}", tokens), juce::String("Verse"));
        expectEquals(waveedit::NamingTokens::substitute("{index}", tokens), juce::String("3"));
        expectEquals(waveedit::NamingTokens::substitute("{N}", tokens), juce::String("003"));
        expectEquals(waveedit::NamingTokens::substitute("{basename}_{region}_{N}", tokens),
                     juce::String("drums_Verse_003"));
    }

    void testBatchTokens()
    {
        // The seven tokens BatchProcessorSettings::applyNamingPattern() supports
        // (literal spellings from BatchNamingTokens).
        std::vector<Token> tokens = {
            { "{filename}", "song" },
            { "{ext}", "wav" },
            { "{date}", "2026-01-12" },
            { "{time}", "14-30-45" },
            { "{index:03}", "007" },
            { "{index}", "7" },
            { "{preset}", "Broadcast Ready" },
        };

        expectEquals(waveedit::NamingTokens::substitute("{filename}", tokens), juce::String("song"));
        expectEquals(waveedit::NamingTokens::substitute("{ext}", tokens), juce::String("wav"));
        expectEquals(waveedit::NamingTokens::substitute("{date}", tokens), juce::String("2026-01-12"));
        expectEquals(waveedit::NamingTokens::substitute("{time}", tokens), juce::String("14-30-45"));
        expectEquals(waveedit::NamingTokens::substitute("{index:03}", tokens), juce::String("007"));
        expectEquals(waveedit::NamingTokens::substitute("{index}", tokens), juce::String("7"));
        expectEquals(waveedit::NamingTokens::substitute("{preset}", tokens), juce::String("Broadcast Ready"));
        expectEquals(waveedit::NamingTokens::substitute("{preset}_{filename}_{index:03}", tokens),
                     juce::String("Broadcast Ready_song_007"));
    }

    void testAudioFormatTokens()
    {
        std::vector<Token> tokens;
        waveedit::NamingTokens::addAudioFormatTokens(tokens, 48000, 24, 2);

        expectEquals(waveedit::NamingTokens::substitute("{samplerate}", tokens), juce::String("48000"));
        expectEquals(waveedit::NamingTokens::substitute("{bitdepth}", tokens), juce::String("24"));
        expectEquals(waveedit::NamingTokens::substitute("{channels}", tokens), juce::String("2"));
        expectEquals(waveedit::NamingTokens::substitute("{samplerate}_{bitdepth}_{channels}", tokens),
                     juce::String("48000_24_2"));

        // Unknown/unreadable-file fallback: 0 renders as the literal "0", not
        // left unexpanded.
        std::vector<Token> zeroTokens;
        waveedit::NamingTokens::addAudioFormatTokens(zeroTokens, 0, 0, 0);
        expectEquals(waveedit::NamingTokens::substitute("{samplerate}_{bitdepth}_{channels}", zeroTokens),
                     juce::String("0_0_0"));
    }

    void testUnknownTokensUntouched()
    {
        std::vector<Token> tokens = {
            { "{basename}", "drums" },
        };

        // A token literally not recognised by any system.
        expectEquals(waveedit::NamingTokens::substitute("{basename}_{nonsense}", tokens),
                     juce::String("drums_{nonsense}"));

        // A token this call site simply did not supply (e.g. {region} omitted).
        expectEquals(waveedit::NamingTokens::substitute("{basename}_{region}", tokens),
                     juce::String("drums_{region}"));
    }

    void testPaddedIndex()
    {
        auto padded = [](int index) -> juce::String
        {
            return juce::String(index).paddedLeft('0', 3);
        };

        std::vector<Token> tokens7 = { { "{index:03}", padded(7) } };
        expectEquals(waveedit::NamingTokens::substitute("{index:03}", tokens7), juce::String("007"));

        std::vector<Token> tokens123 = { { "{index:03}", padded(123) } };
        expectEquals(waveedit::NamingTokens::substitute("{index:03}", tokens123), juce::String("123"));

        std::vector<Token> tokens5 = { { "{index:03}", padded(5) } };
        expectEquals(waveedit::NamingTokens::substitute("{index:03}", tokens5), juce::String("005"));
    }
};

static NamingTokensTests namingTokensTests;
