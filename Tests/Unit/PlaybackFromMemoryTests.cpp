/*
  ==============================================================================

    PlaybackFromMemoryTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Regression test for the Phase 1 finding "file reads/decoding happen on
    the audio thread until the first edit": Document::loadFile wired the
    transport to an AudioFormatReaderSource with no read-ahead, so each
    playback block was read and decoded from disk inside the audio callback.
    A freshly opened document now plays from its decoded in-memory buffer,
    and that buffer playback is sample-identical to the old file playback.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>

#include "../../Source/Audio/AudioEngine.h"
#include "../../Source/Audio/AudioFileManager.h"
#include "../../Source/Utils/Document.h"
#include "../TestUtils/TestAudioFiles.h"

namespace
{
    constexpr double kRate = 48000.0;
    constexpr int kBlock = 512;

    // Minimal device: AudioEngine::audioDeviceAboutToStart reads only the
    // rate, block size and channel mask.
    class BlockDevice : public juce::AudioIODevice
    {
    public:
        BlockDevice() : juce::AudioIODevice("Block", "Block") {}
        juce::StringArray getOutputChannelNames() override { return { "L", "R" }; }
        juce::StringArray getInputChannelNames() override { return {}; }
        juce::Array<double> getAvailableSampleRates() override { return { kRate }; }
        juce::Array<int> getAvailableBufferSizes() override { return { kBlock }; }
        int getDefaultBufferSize() override { return kBlock; }
        juce::String open(const juce::BigInteger&, const juce::BigInteger&, double, int) override { return {}; }
        void close() override {}
        bool isOpen() override { return true; }
        void start(juce::AudioIODeviceCallback*) override {}
        void stop() override {}
        bool isPlaying() override { return true; }
        juce::String getLastError() override { return {}; }
        int getCurrentBufferSizeSamples() override { return kBlock; }
        double getCurrentSampleRate() override { return kRate; }
        int getCurrentBitDepth() override { return 32; }
        juce::BigInteger getActiveOutputChannels() const override { return juce::BigInteger(0b11); }
        juce::BigInteger getActiveInputChannels() const override { return {}; }
        int getOutputLatencyInSamples() override { return 0; }
        int getInputLatencyInSamples() override { return 0; }
    };

    juce::AudioBuffer<float> render(AudioEngine& engine, int numBlocks)
    {
        juce::AudioBuffer<float> all(2, numBlocks * kBlock);
        juce::AudioBuffer<float> block(2, kBlock);
        const juce::AudioIODeviceCallbackContext context{};
        for (int b = 0; b < numBlocks; ++b)
        {
            block.clear();
            engine.audioDeviceIOCallbackWithContext(nullptr, 0, block.getArrayOfWritePointers(),
                                                    2, kBlock, context);
            for (int ch = 0; ch < 2; ++ch)
                all.copyFrom(ch, b * kBlock, block, ch, 0, kBlock);
        }
        return all;
    }
}

class PlaybackFromMemoryTests : public juce::UnitTest
{
public:
    PlaybackFromMemoryTests() : juce::UnitTest("Playback From Memory After Open", "AudioEngine") {}

    void runTest() override
    {
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("WaveEditPlaybackFromMemory.wav");
        auto source = TestAudio::createSineWave(997.0, 0.6f, kRate, 0.5, 2);
        AudioFileManager fm;
        expect(fm.saveAsWav(file, source, kRate, 24), "fixture written");

        beginTest("A freshly opened document plays from memory, not from the file");
        {
            Document doc;
            expect(doc.loadFile(file), "document loads");
            expect(doc.getAudioEngine().isPlayingFromBuffer(),
                   "playback source is the decoded buffer (no disk reads in the callback)");
            expect(!doc.isModified(), "opening does not mark the document modified");
            expect(doc.getAudioEngine().getCurrentFile() == file, "engine still knows its file");
        }

        beginTest("Buffer playback is sample-identical to the old file playback");
        {
            AudioEngine fromFile;
            AudioEngine fromMemory;
            BlockDevice deviceA, deviceB;
            fromFile.audioDeviceAboutToStart(&deviceA);
            fromMemory.audioDeviceAboutToStart(&deviceB);

            expect(fromFile.loadAudioFile(file), "file source");
            AudioBufferManager decoded;
            expect(decoded.loadFromFile(file, fromMemory.getFormatManager()), "decode");
            expect(fromMemory.loadFromBuffer(decoded.getBuffer(), decoded.getSampleRate(),
                                             decoded.getNumChannels()), "memory source");

            for (auto* e : { &fromFile, &fromMemory })
            {
                e->setPosition(0.1);
                e->play();
            }

            const auto a = render(fromFile, 20);
            const auto b = render(fromMemory, 20);
            int mismatches = 0;
            float peak = 0.0f;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < a.getNumSamples(); ++i)
                {
                    if (! juce::exactlyEqual(a.getSample(ch, i), b.getSample(ch, i)))
                        ++mismatches;
                    peak = juce::jmax(peak, std::abs(b.getSample(ch, i)));
                }
            expect(peak > 0.1f, "rendered real audio, not silence");
            expectEquals(mismatches, 0, "every output sample identical");

            fromFile.stop();
            fromMemory.stop();
        }

        file.deleteFile();
    }
};

static PlaybackFromMemoryTests playbackFromMemoryTests;
