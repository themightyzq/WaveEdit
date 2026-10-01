/*
  ==============================================================================

    AudioEngineLifetimeTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    v0.9.1 regression guards for two audio-engine defects:

    1. End of a one-shot selection called AudioTransportSource::stop() on the
       audio thread. JUCE's stop() sleeps until a LATER callback on that same
       thread sets its 'stopped' flag, so the device callback stalled for about
       one second at the end of every selection playback. The end is now
       flagged by the audio thread, silenced sample-exactly, and serviced on
       the message thread.

    2. ~AudioEngine never removed its device callback, and the device manager
       is destroyed after the members the callback reads (transport, plugin
       chain, automation pointer). Teardown now detaches first. Closing a
       playing document must also not wait on JUCE's 1 s stop() loop.

    The one-shot tests drive audioDeviceIOCallbackWithContext directly with a
    dummy device, so they need no audio hardware and run in CI. The device
    stress test opens real devices and is skipped under WAVEEDIT_CI.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_devices/juce_audio_devices.h>

#include "Audio/AudioEngine.h"
#include "Utils/Document.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace
{
    constexpr double kRate = 48000.0;
    constexpr int kBlock = 512;
    constexpr int kChannels = 2;

    // Minimal device: only the rate, block size and channel mask are read by
    // AudioEngine::audioDeviceAboutToStart.
    class DummyDevice : public juce::AudioIODevice
    {
    public:
        DummyDevice() : juce::AudioIODevice("Dummy", "Dummy") {}

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

    // Renders one device block; returns the wall time the callback took (ms).
    double renderBlock(AudioEngine& engine, juce::AudioBuffer<float>& out)
    {
        out.clear();
        const juce::AudioIODeviceCallbackContext context{};
        const auto t0 = std::chrono::steady_clock::now();
        engine.audioDeviceIOCallbackWithContext(nullptr, 0, out.getArrayOfWritePointers(),
                                                out.getNumChannels(), out.getNumSamples(), context);
        const auto t1 = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    juce::AudioBuffer<float> makeConstant(float value, double seconds)
    {
        juce::AudioBuffer<float> b(kChannels, static_cast<int>(seconds * kRate));
        for (int ch = 0; ch < kChannels; ++ch)
            juce::FloatVectorOperations::fill(b.getWritePointer(ch), value, b.getNumSamples());
        return b;
    }
}

//==============================================================================
class AudioEngineOneShotEndTests : public juce::UnitTest
{
public:
    AudioEngineOneShotEndTests() : juce::UnitTest("AudioEngine One-Shot End (v0.9.1)", "AudioEngine") {}

    void runTest() override
    {
        beginTest("Selection end is silent from loopEnd on and never blocks the callback");
        testSampleExactEndWithoutStall();

        beginTest("Message thread turns a pending end into a prompt STOPPED");
        testServiceStops();

        beginTest("Seeking during the pending window resumes audio");
        testSeekClearsPending();
    }

private:
    // Plays [0, 0.1 s) one-shot: output must carry audio up to sample 4800 and
    // exact silence after it, with no callback anywhere near the old ~1 s stall.
    void testSampleExactEndWithoutStall()
    {
        AudioEngine engine;
        DummyDevice device;
        engine.audioDeviceAboutToStart(&device);
        expect(engine.loadFromBuffer(makeConstant(0.5f, 1.0), kRate, kChannels));

        engine.setLoopPoints(0.0, 0.1);
        engine.setLooping(false);
        engine.setPosition(0.0);
        engine.play();

        const int loopEndSample = static_cast<int>(0.1 * kRate);   // 4800
        std::vector<float> rendered;
        juce::AudioBuffer<float> out(kChannels, kBlock);
        double worstMs = 0.0;

        for (int block = 0; block < 20; ++block)
        {
            worstMs = juce::jmax(worstMs, renderBlock(engine, out));
            for (int i = 0; i < kBlock; ++i)
                rendered.push_back(out.getSample(0, i));
        }

        expect(worstMs < 100.0, "Callback took " + juce::String(worstMs, 1)
                                  + " ms; the audio-thread stop() stall was ~1000 ms");

        expectWithinAbsoluteError(rendered[static_cast<size_t>(loopEndSample - 1)], 0.5f, 1.0e-4f,
                                  "Last sample before loopEnd must still be audio");

        bool silentAfterEnd = true;
        for (size_t i = static_cast<size_t>(loopEndSample); i < rendered.size(); ++i)
            silentAfterEnd = silentAfterEnd && rendered[i] == 0.0f;
        expect(silentAfterEnd, "Everything from loopEnd on must be exact silence");

        engine.shutdownAudio();
    }

    // serviceOneShotStop() runs the real message-thread stop(). A helper thread
    // stands in for the device so JUCE's stop() handshake completes normally.
    void testServiceStops()
    {
        AudioEngine engine;
        DummyDevice device;
        engine.audioDeviceAboutToStart(&device);
        expect(engine.loadFromBuffer(makeConstant(0.5f, 1.0), kRate, kChannels));

        engine.setLoopPoints(0.0, 0.05);
        engine.setLooping(false);
        engine.setPosition(0.0);
        engine.play();

        juce::AudioBuffer<float> out(kChannels, kBlock);
        for (int block = 0; block < 10; ++block)
            renderBlock(engine, out);

        expect(engine.isPlaying(), "Audio thread only flags the end; the state changes on the message thread");

        std::atomic<bool> pumping { true };
        std::thread device_thread([&]
        {
            juce::AudioBuffer<float> devOut(kChannels, kBlock);
            while (pumping.load())
            {
                renderBlock(engine, devOut);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

        const auto t0 = std::chrono::steady_clock::now();
        engine.serviceOneShotStop();
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

        pumping.store(false);
        device_thread.join();

        expect(engine.getPlaybackState() == PlaybackState::STOPPED, "serviceOneShotStop must stop the engine");
        expect(!engine.hasLoopPoints(), "stop() clears the one-shot loop points");
        expect(ms < 500.0, "Stopping took " + juce::String(ms, 1) + " ms");

        engine.shutdownAudio();
    }

    void testSeekClearsPending()
    {
        AudioEngine engine;
        DummyDevice device;
        engine.audioDeviceAboutToStart(&device);
        expect(engine.loadFromBuffer(makeConstant(0.5f, 1.0), kRate, kChannels));

        engine.setLoopPoints(0.0, 0.05);
        engine.setLooping(false);
        engine.setPosition(0.0);
        engine.play();

        juce::AudioBuffer<float> out(kChannels, kBlock);
        for (int block = 0; block < 10; ++block)
            renderBlock(engine, out);

        renderBlock(engine, out);
        expectEquals(out.getMagnitude(0, kBlock), 0.0f, "Pending end renders silence");

        engine.setPosition(0.0);
        renderBlock(engine, out);
        expect(out.getMagnitude(0, kBlock) > 0.4f, "A seek clears the pending end and audio resumes");

        engine.shutdownAudio();
    }
};

//==============================================================================
class AudioEngineTeardownTests : public juce::UnitTest
{
public:
    AudioEngineTeardownTests() : juce::UnitTest("AudioEngine Teardown (v0.9.1)", "AudioEngine") {}

    void runTest() override
    {
        beginTest("shutdownAudio is idempotent without a device");
        {
            AudioEngine engine;
            engine.shutdownAudio();
            engine.shutdownAudio();
            expect(engine.getPlaybackState() == PlaybackState::STOPPED);
        }

        beginTest("Closing a playing document returns promptly");
        {
            auto doc = std::make_unique<Document>();
            auto& engine = doc->getAudioEngine();
            expect(engine.loadFromBuffer(makeConstant(0.25f, 2.0), kRate, kChannels));
            engine.play();

            const auto t0 = std::chrono::steady_clock::now();
            doc.reset();
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

            // Before the fix, teardown ran AudioTransportSource::stop() with no
            // callback left to answer it: a fixed ~1 s wait per playing tab.
            expect(ms < 500.0, "Document teardown took " + juce::String(ms, 1) + " ms");
        }
    }
};

//==============================================================================
// Local-only: real devices, live callbacks. Skipped under WAVEEDIT_CI (see
// Tests/TestRunner.cpp). Run under AddressSanitizer to prove no callback
// touches a destroyed member.
class AudioEngineLifetimeDeviceStress : public juce::UnitTest
{
public:
    AudioEngineLifetimeDeviceStress() : juce::UnitTest("AudioEngine Lifetime Device Stress", "AudioEngine") {}

    void runTest() override
    {
        beginTest("Create, play, and destroy 50 documents with a live device");

        int played = 0;
        for (int i = 0; i < 50; ++i)
        {
            auto doc = std::make_unique<Document>();
            auto& engine = doc->getAudioEngine();
            if (!engine.loadFromBuffer(makeConstant(0.1f, 0.5), kRate, kChannels))
                continue;

            engine.setLoopPoints(0.0, 0.01);
            engine.setLooping(false);
            engine.play();
            ++played;

            // Let the callback run across the one-shot end and into the
            // pending window, then destroy mid-stream.
            juce::Thread::sleep(i % 3 == 0 ? 25 : 5);
        }

        expectEquals(played, 50);
    }
};

static AudioEngineOneShotEndTests audioEngineOneShotEndTests;
static AudioEngineTeardownTests audioEngineTeardownTests;
static AudioEngineLifetimeDeviceStress audioEngineLifetimeDeviceStress;
