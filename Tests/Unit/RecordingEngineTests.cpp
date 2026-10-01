/*
  ==============================================================================

    RecordingEngineTests.cpp
    WaveEdit - Professional Audio Editor

    Pass-2 defect coverage for RecordingEngine:
      - M18: the (large) recording buffer is allocated lazily on
        startRecording(), NOT eagerly on every device start. With no
        device ever started, the buffer should be empty until recording
        begins.
      - M17: dropped-sample bookkeeping starts at zero and resets on
        startRecording()/clearRecording() so the UI can surface a gap.

    These tests drive RecordingEngine's public state machine directly;
    they do not require a real audio device. The audio-callback / device
    -teardown race (C9) is reasoning-only — see the agent summary.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include "Audio/RecordingEngine.h"

//==============================================================================
class RecordingEngineTests : public juce::UnitTest
{
public:
    RecordingEngineTests() : juce::UnitTest("RecordingEngine", "Unit") {}

    void runTest() override
    {
        beginTest("M18: no recording buffer is allocated before record start");
        {
            RecordingEngine engine;
            // Fresh engine, no device started, no recording: the heavy
            // 1-hour buffer must not have been allocated.
            expectEquals(engine.getRecordedAudio().getNumSamples(), 0,
                         "recording buffer is empty until startRecording()");
        }

        beginTest("M18: startRecording allocates the buffer; stop trims it");
        {
            RecordingEngine engine;
            expect(engine.getRecordingState() == RecordingState::IDLE);

            const bool started = engine.startRecording();
            expect(started, "startRecording succeeds and allocates lazily");
            expect(engine.isRecording(), "engine is in RECORDING state");

            // Buffer now sized for ~1 hour at the default 44.1k rate.
            expect(engine.getRecordedAudio().getNumSamples() > 0,
                   "recording buffer allocated on record start");

            const bool stopped = engine.stopRecording();
            expect(stopped, "stopRecording succeeds");
            expect(engine.getRecordingState() == RecordingState::IDLE);
        }

        beginTest("M17: dropped-sample count starts at zero");
        {
            RecordingEngine engine;
            expectEquals(engine.getDroppedSampleCount(), 0,
                         "no samples dropped on a fresh engine");
        }

        beginTest("M17: clearRecording resets the dropped-sample count");
        {
            RecordingEngine engine;
            engine.startRecording();
            engine.stopRecording();
            engine.clearRecording();
            expectEquals(engine.getDroppedSampleCount(), 0,
                         "dropped-sample count is zero after clearRecording");
        }

        beginTest("Channels: default request is stereo (2)");
        {
            RecordingEngine engine;
            expectEquals(engine.getRequestedChannelCount(), 2,
                         "engine defaults to stereo capture");
        }

        beginTest("Channels: mono request yields a 1-channel recorded buffer");
        {
            RecordingEngine engine;
            engine.setRequestedChannelCount(1);
            expectEquals(engine.getRequestedChannelCount(), 1, "request stored");

            expect(engine.startRecording(), "startRecording succeeds");
            // No device started -> resolved count == requested count.
            expectEquals(engine.getRecordedNumChannels(), 1,
                         "recorded buffer is mono");
            expectEquals(engine.getRecordedAudio().getNumChannels(), 1,
                         "allocated buffer has 1 channel");
            engine.stopRecording();
        }

        beginTest("Channels: stereo request yields a 2-channel recorded buffer");
        {
            RecordingEngine engine;
            engine.setRequestedChannelCount(2);
            expect(engine.startRecording(), "startRecording succeeds");
            expectEquals(engine.getRecordedNumChannels(), 2,
                         "recorded buffer is stereo");
            expectEquals(engine.getRecordedAudio().getNumChannels(), 2,
                         "allocated buffer has 2 channels");
            engine.stopRecording();
        }

        beginTest("Channels: out-of-range requests are clamped to [1, 2]");
        {
            RecordingEngine engine;
            engine.setRequestedChannelCount(5);
            expectEquals(engine.getRequestedChannelCount(), 2, "5 clamps to 2");
            engine.setRequestedChannelCount(0);
            expectEquals(engine.getRequestedChannelCount(), 1, "0 clamps to 1");
            engine.setRequestedChannelCount(-3);
            expectEquals(engine.getRequestedChannelCount(), 1, "negative clamps to 1");
        }

        beginTest("Channels: channel request is ignored while recording");
        {
            RecordingEngine engine;
            engine.setRequestedChannelCount(2);
            expect(engine.startRecording());
            engine.setRequestedChannelCount(1);  // must be ignored mid-take
            expectEquals(engine.getRequestedChannelCount(), 2,
                         "request unchanged while recording");
            engine.stopRecording();
            engine.setRequestedChannelCount(1);  // now allowed
            expectEquals(engine.getRequestedChannelCount(), 1,
                         "request applies once stopped");
        }

        beginTest("Cap: max record time is 1 hour; remaining starts full");
        {
            RecordingEngine engine;
            expectWithinAbsoluteError(engine.getMaxRecordSeconds(), 3600.0, 0.001,
                                      "cap is one hour");

            // Before allocation, remaining reports the full cap.
            expectWithinAbsoluteError(engine.getRemainingRecordSeconds(), 3600.0, 0.001,
                                      "remaining is full cap before record start");

            expect(engine.startRecording());
            // Freshly started, nothing captured yet -> ~full hour remaining.
            const double remaining = engine.getRemainingRecordSeconds();
            expect(remaining > 3599.0 && remaining <= 3600.0 + 0.001,
                   "remaining is ~1 hour immediately after start");
            engine.stopRecording();
        }
    }
};

static RecordingEngineTests recordingEngineTests;
