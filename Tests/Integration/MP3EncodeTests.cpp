/*
  ==============================================================================

    MP3EncodeTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Integration tests for the LAME-backed MP3 encoder
    (Source/Audio/LameMP3AudioFormat). MP3 is lossy, so the round-trip checks
    are tolerance-based (channel count, sample rate, duration within a couple of
    frames, level/correlation within a tolerance) -- never sample-exact.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "Audio/AudioFileManager.h"
#include "TestAudioFiles.h"

#if WAVEEDIT_HAVE_LAME
#include "Audio/LameMP3AudioFormat.h"
#endif

namespace
{
// Decodes an .mp3 file with JUCE's decoder into a float buffer. Returns false
// if a reader could not be created.
bool decodeMp3(const juce::File& file, juce::AudioBuffer<float>& out, double& sampleRate,
               int& numChannels)
{
    juce::MP3AudioFormat mp3;

    auto stream = file.createInputStream();
    if (stream == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(
        mp3.createReaderFor(stream.release(), true));

    if (reader == nullptr)
        return false;

    sampleRate = reader->sampleRate;
    numChannels = static_cast<int>(reader->numChannels);

    out.setSize(numChannels, static_cast<int>(reader->lengthInSamples));
    reader->read(&out, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
    return true;
}

// RMS over [start, start+count) of channel 0.
float rmsOf(const juce::AudioBuffer<float>& buffer, int start, int count)
{
    start = juce::jmax(0, start);
    count = juce::jmin(count, buffer.getNumSamples() - start);
    if (count <= 0)
        return 0.0f;

    double sum = 0.0;
    const float* d = buffer.getReadPointer(0);
    for (int i = start; i < start + count; ++i)
        sum += (double) d[i] * (double) d[i];

    return (float) std::sqrt(sum / count);
}

// Best normalized cross-correlation of a decoded window against the original
// reference sine, searching a small lag range to absorb MP3 encoder delay.
float bestCorrelation(const juce::AudioBuffer<float>& reference,
                      const juce::AudioBuffer<float>& decoded,
                      int windowLen, int maxLag)
{
    const int refStart = 4096; // a stable region well past the start
    if (reference.getNumSamples() < refStart + windowLen)
        return 0.0f;

    const float* ref = reference.getReadPointer(0) + refStart;

    // Precompute reference energy.
    double refEnergy = 0.0;
    for (int i = 0; i < windowLen; ++i)
        refEnergy += (double) ref[i] * ref[i];
    if (refEnergy <= 0.0)
        return 0.0f;

    float best = 0.0f;
    const float* dec = decoded.getReadPointer(0);

    for (int lag = 0; lag <= maxLag; ++lag)
    {
        const int decStart = refStart + lag;
        if (decStart + windowLen > decoded.getNumSamples())
            break;

        double dot = 0.0, decEnergy = 0.0;
        for (int i = 0; i < windowLen; ++i)
        {
            const double a = ref[i];
            const double b = dec[decStart + i];
            dot += a * b;
            decEnergy += b * b;
        }

        if (decEnergy > 0.0)
        {
            const float corr = (float) (dot / std::sqrt(refEnergy * decEnergy));
            best = juce::jmax(best, corr);
        }
    }

    return best;
}
} // namespace

class MP3EncodeTests : public juce::UnitTest
{
public:
    MP3EncodeTests() : juce::UnitTest("MP3 Encode (LAME)", "Integration") {}

    void runTest() override
    {
#if WAVEEDIT_HAVE_LAME
        runStereoRoundTrip();
        runMonoRoundTrip();
        runUnsupportedSampleRateRejected();
        runUnsupportedChannelCountRejected();
#else
        beginTest("MP3 encoder not compiled in (WAVEEDIT_HAVE_LAME undefined)");
        logMessage("Skipping LAME MP3 tests - built without the LAME encoder.");
        expect(true);
#endif
    }

#if WAVEEDIT_HAVE_LAME
private:
    juce::File makeTempMp3(const juce::String& name)
    {
        return juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("WaveEditMP3Tests")
            .getChildFile(name);
    }

    void runStereoRoundTrip()
    {
        beginTest("Stereo 44.1kHz sine encodes to a valid, decodable MP3");

        const double sr = 44100.0;
        const float amp = 0.5f;
        const double durationSec = 2.0;
        auto original = TestAudio::createSineWave(1000.0, amp, sr, durationSec, 2);

        auto file = makeTempMp3("stereo_1k.mp3");
        file.getParentDirectory().createDirectory();
        file.deleteFile();

        AudioFileManager mgr;
        const bool saved = mgr.saveAudioFile(file, original, sr, 16, /*quality*/ 9);
        expect(saved, "saveAudioFile returned true (" + mgr.getLastError() + ")");
        expect(file.existsAsFile() && file.getSize() > 0, "MP3 file exists and is non-empty");

        juce::AudioBuffer<float> decoded;
        double decSr = 0.0;
        int decCh = 0;
        const bool ok = decodeMp3(file, decoded, decSr, decCh);
        expect(ok, "Decoded the encoded MP3 with juce::MP3AudioFormat");

        expectEquals(decCh, 2, "Decoded channel count matches");
        expect(std::abs(decSr - sr) < 1.0, "Decoded sample rate matches");

        // Duration within a few MP3 frames (encoder delay + padding ~ 1-2 frames
        // of 1152 samples). Allow 4000 samples of slack.
        const int expectedSamples = (int) (durationSec * sr);
        expect(std::abs(decoded.getNumSamples() - expectedSamples) < 4000,
               "Decoded duration within a few frames of the source");

        // Level: RMS of a stable mid-section approximates the sine's RMS
        // (amp / sqrt(2)). MP3 preserves level well at high bitrate.
        const float expectedRms = amp / std::sqrt(2.0f);
        const float midRms = rmsOf(decoded, decoded.getNumSamples() / 4,
                                   decoded.getNumSamples() / 2);
        expect(std::abs(midRms - expectedRms) < 0.08f,
               "Decoded RMS ~ source RMS (got " + juce::String(midRms)
                   + ", expected " + juce::String(expectedRms) + ")");

        // Shape: high correlation with the original sine after delay alignment.
        const float corr = bestCorrelation(original, decoded, 8192, 2048);
        expect(corr > 0.9f, "Decoded waveform correlates with source sine (corr="
                                + juce::String(corr) + ")");

        file.deleteFile();
    }

    void runMonoRoundTrip()
    {
        beginTest("Mono 48kHz sine encodes and decodes with correct channel count");

        const double sr = 48000.0;
        const float amp = 0.5f;
        auto original = TestAudio::createSineWave(440.0, amp, sr, 1.5, 1);

        auto file = makeTempMp3("mono_440.mp3");
        file.getParentDirectory().createDirectory();
        file.deleteFile();

        AudioFileManager mgr;
        const bool saved = mgr.saveAudioFile(file, original, sr, 16, /*quality*/ 7);
        expect(saved, "saveAudioFile (mono) returned true (" + mgr.getLastError() + ")");

        juce::AudioBuffer<float> decoded;
        double decSr = 0.0;
        int decCh = 0;
        const bool ok = decodeMp3(file, decoded, decSr, decCh);
        expect(ok, "Decoded the mono MP3");
        expect(decCh == 1 || decCh == 2, "Mono decodes to 1 (or a duplicated 2) channels");
        expect(std::abs(decSr - sr) < 1.0, "Mono decoded sample rate matches");

        const float expectedRms = amp / std::sqrt(2.0f);
        const float midRms = rmsOf(decoded, decoded.getNumSamples() / 4,
                                   decoded.getNumSamples() / 2);
        expect(std::abs(midRms - expectedRms) < 0.08f,
               "Mono decoded RMS ~ source RMS (got " + juce::String(midRms) + ")");

        file.deleteFile();
    }

    void runUnsupportedSampleRateRejected()
    {
        beginTest("Unsupported sample rate (96kHz) is rejected with a clear error");

        const double sr = 96000.0; // above LAME's 48kHz max
        auto original = TestAudio::createSineWave(1000.0, 0.5f, sr, 0.5, 2);

        auto file = makeTempMp3("reject_96k.mp3");
        file.getParentDirectory().createDirectory();
        file.deleteFile();

        AudioFileManager mgr;
        const bool saved = mgr.saveAudioFile(file, original, sr, 16, 9);
        expect(! saved, "saveAudioFile refuses 96kHz for MP3");

        const juce::String err = mgr.getLastError();
        expect(err.containsIgnoreCase("sample rate")
                   || err.containsIgnoreCase("48000")
                   || err.containsIgnoreCase("does not support"),
               "Error names the sample-rate limitation: " + err);

        // No corrupt file should be left behind on rejection.
        expect(! file.existsAsFile(), "No MP3 written when the rate is rejected");

        // The static policy helper agrees.
        expect(! waveedit::LameMP3AudioFormat::isSampleRateSupported(96000.0),
               "isSampleRateSupported(96000) is false");
        expect(waveedit::LameMP3AudioFormat::isSampleRateSupported(44100.0),
               "isSampleRateSupported(44100) is true");
    }

    void runUnsupportedChannelCountRejected()
    {
        beginTest("More than 2 channels is rejected for MP3");

        const double sr = 44100.0;
        auto original = TestAudio::createSineWave(1000.0, 0.5f, sr, 0.5, 1);
        // Expand to 4 channels (MP3 supports mono/stereo only).
        juce::AudioBuffer<float> quad(4, original.getNumSamples());
        for (int ch = 0; ch < 4; ++ch)
            quad.copyFrom(ch, 0, original, 0, 0, original.getNumSamples());

        auto file = makeTempMp3("reject_quad.mp3");
        file.getParentDirectory().createDirectory();
        file.deleteFile();

        AudioFileManager mgr;
        const bool saved = mgr.saveAudioFile(file, quad, sr, 16, 9);
        expect(! saved, "saveAudioFile refuses 4-channel MP3");
        expect(mgr.getLastError().containsIgnoreCase("mono or stereo")
                   || mgr.getLastError().containsIgnoreCase("channels"),
               "Error names the channel limitation: " + mgr.getLastError());
    }
#endif // WAVEEDIT_HAVE_LAME
};

static MP3EncodeTests mp3EncodeTests;
