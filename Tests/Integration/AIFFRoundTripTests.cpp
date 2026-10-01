/*
  ==============================================================================

    AIFFRoundTripTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Integration tests for AIFF (.aif/.aiff) open + save support. AIFF is
    lossless PCM, so round-trips are checked sample-by-sample within
    quantization tolerance. JUCE's AIFF writer supports 8/16/24-bit only;
    a 32-bit(-float) request must be written as 24-bit, not fail.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Audio/AudioFileManager.h"
#include "TestAudioFiles.h"

namespace
{
juce::File makeTempFile(const juce::String& name)
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("WaveEditAIFFTests")
        .getChildFile(name);
}

// Reports the bit depth a reader sees for the given file, or -1 on failure.
int readBitDepth(const juce::File& file)
{
    juce::AudioFormatManager manager;
    manager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(manager.createReaderFor(file));
    return reader != nullptr ? static_cast<int>(reader->bitsPerSample) : -1;
}
} // namespace

//==============================================================================
class AIFFRoundTripTests : public juce::UnitTest
{
public:
    AIFFRoundTripTests() : juce::UnitTest("AIFF Round Trip", "Integration") {}

    void runTest() override
    {
        testGates();
        testLosslessRoundTrip(16, 2.0f / 32767.0f);
        testLosslessRoundTrip(24, 2.0f / 8388607.0f);
        testLosslessRoundTrip(8, 2.0f / 127.0f);
        test32FloatClamp();
        testAifSpelling();
    }

private:
    void testGates()
    {
        beginTest("AIFF gates: openable and writable on all platforms");

        const auto wildcard = AudioFileManager::getSupportedExtensions();
        expect(wildcard.contains("*.aif"),  "wildcard has *.aif: " + wildcard);
        expect(wildcard.contains("*.aiff"), "wildcard has *.aiff: " + wildcard);

        const juce::File base("/nonexistent/dir");
        expect(AudioFileManager::isSupportedAudioFile(base.getChildFile("a.aif")));
        expect(AudioFileManager::isSupportedAudioFile(base.getChildFile("a.aiff")));
        expect(AudioFileManager::isSupportedAudioFile(base.getChildFile("A.AIFF")),
               "extension check is case-insensitive");
        expect(! AudioFileManager::isSupportedAudioFile(base.getChildFile("a.aifc")),
               "compressed AIFF-C is intentionally not advertised");

        expect(AudioFileManager::canWriteFormat(".aif"));
        expect(AudioFileManager::canWriteFormat("aiff"));
    }

    void testLosslessRoundTrip(int bitDepth, float tolerance)
    {
        beginTest("AIFF " + juce::String(bitDepth) + "-bit save/load round trip");

        const double sampleRate = 48000.0;
        auto original = TestAudio::createSineWave(440.0, 0.5f, sampleRate, 1.0, 2);

        auto file = makeTempFile("roundtrip_" + juce::String(bitDepth) + ".aiff");
        file.getParentDirectory().createDirectory();
        file.deleteFile();

        AudioFileManager mgr;
        expect(mgr.saveAudioFile(file, original, sampleRate, bitDepth, 0),
               "save succeeds: " + mgr.getLastError());

        expect(mgr.isValidAudioFile(file), "file validates: " + mgr.getLastError());
        expectEquals(readBitDepth(file), bitDepth, "file has requested bit depth");

        juce::AudioBuffer<float> decoded;
        expect(mgr.loadIntoBuffer(file, decoded), "load succeeds: " + mgr.getLastError());
        expectEquals(decoded.getNumChannels(), original.getNumChannels());
        expectEquals(decoded.getNumSamples(), original.getNumSamples(),
                     "AIFF is PCM: sample count is exact");

        // Sample-by-sample within quantization error of the write depth.
        float maxDiff = 0.0f;
        for (int ch = 0; ch < original.getNumChannels(); ++ch)
        {
            const float* a = original.getReadPointer(ch);
            const float* b = decoded.getReadPointer(ch);
            for (int i = 0; i < original.getNumSamples(); ++i)
                maxDiff = juce::jmax(maxDiff, std::abs(a[i] - b[i]));
        }
        expect(maxDiff <= tolerance,
               "max sample error " + juce::String(maxDiff, 8)
                   + " within quantization tolerance " + juce::String(tolerance, 8));
    }

    void test32FloatClamp()
    {
        beginTest("AIFF 32-bit request is written as 24-bit, not rejected");

        const double sampleRate = 44100.0;
        auto original = TestAudio::createSineWave(440.0, 0.5f, sampleRate, 0.5, 1);

        auto file = makeTempFile("clamp_32.aiff");
        file.getParentDirectory().createDirectory();
        file.deleteFile();

        AudioFileManager mgr;
        expect(mgr.saveAudioFile(file, original, sampleRate, 32, 0),
               "32-bit request succeeds via clamp: " + mgr.getLastError());
        expectEquals(readBitDepth(file), 24, "written file is 24-bit");
    }

    void testAifSpelling()
    {
        beginTest("Short .aif spelling round trip");

        const double sampleRate = 44100.0;
        auto original = TestAudio::createSineWave(440.0, 0.5f, sampleRate, 0.5, 2);

        auto file = makeTempFile("spelling.aif");
        file.getParentDirectory().createDirectory();
        file.deleteFile();

        AudioFileManager mgr;
        expect(mgr.saveAudioFile(file, original, sampleRate, 16, 0),
               ".aif save succeeds: " + mgr.getLastError());

        juce::AudioBuffer<float> decoded;
        expect(mgr.loadIntoBuffer(file, decoded), ".aif load succeeds: " + mgr.getLastError());
        expectEquals(decoded.getNumSamples(), original.getNumSamples());
    }
};

static AIFFRoundTripTests aiffRoundTripTests;
