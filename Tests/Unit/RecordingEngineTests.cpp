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
    }
};

static RecordingEngineTests recordingEngineTests;
