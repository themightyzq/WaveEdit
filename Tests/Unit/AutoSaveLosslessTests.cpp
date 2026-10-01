/*
  ==============================================================================

    AutoSaveLosslessTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Regression tests for the v0.9.1 auto-save follow-up:

    1. Auto-saves were written at the engine bit depth (16/24-bit integer for
       most files), so a recovered take lost precision and clipped anything
       beyond full scale. AutoSaveRecovery::writeAutoSave now writes 32-bit
       float, which round-trips the float edit buffer exactly.

    2. Crash recovery only tried the newest auto-save. A crash during the
       auto-save write leaves that file truncated, and recovery then gave up
       although an older, intact auto-save existed. loadFirstReadable() falls
       back to the older files.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Utils/AutoSaveRecovery.h"
#include "Audio/AudioFileManager.h"

class AutoSaveLosslessTests : public juce::UnitTest
{
public:
    AutoSaveLosslessTests() : juce::UnitTest("Auto-Save Lossless + Fallback", "AutoSave") {}

    void runTest() override
    {
        dir().deleteRecursively();
        dir().createDirectory();

        testAutoSaveIsBitExact();
        testRecoveryFallsBackPastUnreadableNewest();
        testRecoveryReportsNothingReadable();

        dir().deleteRecursively();
    }

private:
    static juce::File dir()
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("WaveEditAutoSaveLosslessTests");
    }

    static juce::AudioBuffer<float> awkwardBuffer(float offset)
    {
        // Values a 16/24-bit integer file cannot hold: beyond full scale and
        // finer than one 24-bit step.
        juce::AudioBuffer<float> buffer(2, 1000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample(ch, i, offset + 1.5f * std::sin(0.01f * (float) i + (float) ch)
                                            + 1.0e-7f * (float) (i % 7));
        return buffer;
    }

    static bool load(const juce::File& f, juce::AudioBuffer<float>& out)
    {
        AudioFileManager fm;
        return fm.loadIntoBufferUnchecked(f, out);
    }

    void expectBitExact(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b,
                        const juce::String& what)
    {
        expectEquals(b.getNumChannels(), a.getNumChannels(), what + ": channel count");
        expectEquals(b.getNumSamples(), a.getNumSamples(), what + ": length");
        if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples())
            return;

        int mismatches = 0;
        for (int ch = 0; ch < a.getNumChannels(); ++ch)
            for (int i = 0; i < a.getNumSamples(); ++i)
                if (! juce::exactlyEqual(a.getSample(ch, i), b.getSample(ch, i)))
                    ++mismatches;
        expectEquals(mismatches, 0, what + ": every sample identical");
    }

    void testAutoSaveIsBitExact()
    {
        beginTest("Auto-save round-trips the float buffer exactly (incl. > 0 dBFS)");

        const auto source = awkwardBuffer(0.0f);
        const auto file = dir().getChildFile("autosave_exact.wav");
        juce::String error;
        expect(AutoSaveRecovery::writeAutoSave(file, source, 44100.0, error), error);

        juce::AudioBuffer<float> recovered;
        expect(load(file, recovered), "auto-save reads back");
        expectBitExact(source, recovered, "auto-save");
    }

    void testRecoveryFallsBackPastUnreadableNewest()
    {
        beginTest("Recovery falls back to an older auto-save when the newest is unreadable");

        const auto older = dir().getChildFile("autosave_older.wav");
        const auto newest = dir().getChildFile("autosave_newest.wav");
        const auto olderAudio = awkwardBuffer(0.25f);
        juce::String error;
        expect(AutoSaveRecovery::writeAutoSave(older, olderAudio, 48000.0, error), error);
        expect(AutoSaveRecovery::writeAutoSave(newest, awkwardBuffer(0.5f), 48000.0, error), error);

        // Simulate a crash mid-write: only the first bytes of the header made it.
        {
            juce::MemoryBlock head;
            newest.loadFileAsData(head);
            head.setSize(20);
            expect(newest.replaceWithData(head.getData(), head.getSize()), "truncate newest");
        }

        juce::Array<juce::File> newestFirst { newest, older };
        juce::AudioBuffer<float> recovered;
        const int used = AutoSaveRecovery::loadFirstReadable(newestFirst, &load, recovered);
        expectEquals(used, 1, "the older auto-save is used");
        expectBitExact(olderAudio, recovered, "fallback recovery");
    }

    void testRecoveryReportsNothingReadable()
    {
        beginTest("Recovery reports -1 (keep backups) when no auto-save can be read");

        const auto empty = dir().getChildFile("autosave_empty.wav");
        expect(empty.replaceWithText(""), "empty file");
        juce::Array<juce::File> list { empty, dir().getChildFile("missing.wav") };
        juce::AudioBuffer<float> recovered;
        expectEquals(AutoSaveRecovery::loadFirstReadable(list, &load, recovered), -1);
    }
};

static AutoSaveLosslessTests autoSaveLosslessTests;
