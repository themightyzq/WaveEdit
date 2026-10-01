/*
  ==============================================================================

    ChannelFocusDSPTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Regression tests for the Phase 1 finding "DSP ignores per-channel
    focus": Cut/Copy/Delete only touched the focused channels, but Gain,
    Normalize, fades, silence, invert/reverse, EQ, plugin apply and
    generate processed every channel. They now change only the focused
    channels (ChannelFocus::wrap + ChannelMaskedAction), undo/redo
    included, and the realtime DSP preview does the same.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>

#include "../../Source/Controllers/DSPController.h"
#include "../../Source/Controllers/ChannelFocus.h"
#include "../../Source/Audio/ChannelMask.h"
#include "../../Source/Utils/DocumentManager.h"
#include "../../Source/Utils/Document.h"
#include "../../Source/Utils/UndoActions/AudioUndoActions.h"

namespace
{
    constexpr double kRate = 48000.0;
    constexpr int kLength = 48000;
    constexpr float kLeft = 0.5f;
    constexpr float kRight = -0.25f;

    Document* makeStereoDoc(DocumentManager& mgr)
    {
        Document* doc = mgr.createDocument();
        juce::AudioBuffer<float> audio(2, kLength);
        juce::FloatVectorOperations::fill(audio.getWritePointer(0), kLeft, kLength);
        juce::FloatVectorOperations::fill(audio.getWritePointer(1), kRight, kLength);
        doc->getBufferManager().setBuffer(audio, kRate);
        doc->getAudioEngine().loadFromBuffer(doc->getBufferManager().getBuffer(), kRate, 2);
        doc->getWaveformDisplay().reloadFromBuffer(doc->getBufferManager().getBuffer(), kRate, false, false);
        return doc;
    }

    float at(Document* doc, int ch, int i)
    {
        return doc->getBufferManager().getBuffer().getSample(ch, i);
    }

    class BlockDevice : public juce::AudioIODevice
    {
    public:
        BlockDevice() : juce::AudioIODevice("Block", "Block") {}
        juce::StringArray getOutputChannelNames() override { return { "L", "R" }; }
        juce::StringArray getInputChannelNames() override { return {}; }
        juce::Array<double> getAvailableSampleRates() override { return { kRate }; }
        juce::Array<int> getAvailableBufferSizes() override { return { 512 }; }
        int getDefaultBufferSize() override { return 512; }
        juce::String open(const juce::BigInteger&, const juce::BigInteger&, double, int) override { return {}; }
        void close() override {}
        bool isOpen() override { return true; }
        void start(juce::AudioIODeviceCallback*) override {}
        void stop() override {}
        bool isPlaying() override { return true; }
        juce::String getLastError() override { return {}; }
        int getCurrentBufferSizeSamples() override { return 512; }
        double getCurrentSampleRate() override { return kRate; }
        int getCurrentBitDepth() override { return 32; }
        juce::BigInteger getActiveOutputChannels() const override { return juce::BigInteger(0b11); }
        juce::BigInteger getActiveInputChannels() const override { return {}; }
        int getOutputLatencyInSamples() override { return 0; }
        int getInputLatencyInSamples() override { return 0; }
    };
}

class ChannelFocusDSPTests : public juce::UnitTest
{
public:
    ChannelFocusDSPTests() : juce::UnitTest("DSP Respects Channel Focus", "Integration") {}

    void runTest() override
    {
        testGainUndoRedo();
        testNormalizeMeasuresFocusedChannels();
        testSelectionOperations();
        testPluginAndGenerate();
        testAlreadyPerformedPath();
        testMaskRules();
        testRealtimePreview();
    }

private:
    void testGainUndoRedo()
    {
        beginTest("Gain changes only the focused channel; undo and redo keep it that way");

        DocumentManager mgr;
        DSPController dsp;
        Document* doc = makeStereoDoc(mgr);
        doc->getWaveformDisplay().clearSelection();
        doc->getWaveformDisplay().setFocusedChannels(1 << 0);

        const float gain = juce::Decibels::decibelsToGain(6.0f);
        dsp.applyGainAdjustment(doc, 6.0f);
        expectWithinAbsoluteError(at(doc, 0, 100), kLeft * gain, 1.0e-5f, "left gained");
        expectEquals(at(doc, 1, 100), kRight, "right untouched");

        doc->getUndoManager().undo();
        expectEquals(at(doc, 0, 100), kLeft, "undo restores left");
        expectEquals(at(doc, 1, 100), kRight, "undo keeps right");

        doc->getUndoManager().redo();
        expectWithinAbsoluteError(at(doc, 0, 100), kLeft * gain, 1.0e-5f, "redo gains left");
        expectEquals(at(doc, 1, 100), kRight, "redo still leaves right alone");
    }

    void testNormalizeMeasuresFocusedChannels()
    {
        beginTest("Normalize measures and changes only the focused channel");

        DocumentManager mgr;
        DSPController dsp;
        Document* doc = makeStereoDoc(mgr);
        doc->getWaveformDisplay().clearSelection();
        doc->getWaveformDisplay().setFocusedChannels(1 << 1);

        dsp.applyNormalize(doc);  // to 0 dBFS
        expectWithinAbsoluteError(at(doc, 1, 10), -1.0f, 1.0e-4f, "right normalized from its own peak");
        expectEquals(at(doc, 0, 10), kLeft, "left untouched");
    }

    void testSelectionOperations()
    {
        beginTest("Invert, silence and fade in change only the focused channel");

        DocumentManager mgr;
        DSPController dsp;
        Document* doc = makeStereoDoc(mgr);
        doc->getWaveformDisplay().setSelection(0.25, 0.5);  // samples 12000..24000

        doc->getWaveformDisplay().setFocusedChannels(1 << 0);
        dsp.invertSelection(doc);
        expectEquals(at(doc, 0, 15000), -kLeft, "left inverted");
        expectEquals(at(doc, 1, 15000), kRight, "right not inverted");

        doc->getWaveformDisplay().setFocusedChannels(1 << 1);
        dsp.silenceSelection(doc);
        expectEquals(at(doc, 1, 15000), 0.0f, "right silenced");
        expectEquals(at(doc, 0, 15000), -kLeft, "left not silenced");

        doc->getWaveformDisplay().setFocusedChannels(1 << 0);
        dsp.applyFadeIn(doc);
        expectWithinAbsoluteError(at(doc, 0, 12000), 0.0f, 1.0e-3f, "left faded at the start");
        expectEquals(at(doc, 1, 12000), 0.0f, "right keeps its silence (not re-processed)");
        expectEquals(at(doc, 1, 30000), kRight, "right outside the selection untouched");
    }

    void testPluginAndGenerate()
    {
        beginTest("Plugin apply and generate-over-selection change only the focused channel");

        DocumentManager mgr;
        DSPController dsp;
        Document* doc = makeStereoDoc(mgr);
        doc->getWaveformDisplay().setFocusedChannels(1 << 0);

        juce::AudioBuffer<float> rendered(2, 1000);
        rendered.clear();
        expect(DSPController::commitPluginRender(doc, 2000, 1000, rendered, "Apply Plugin Chain: t", "t"));
        expectEquals(at(doc, 0, 2500), 0.0f, "left takes the render");
        expectEquals(at(doc, 1, 2500), kRight, "right keeps its audio");

        doc->getWaveformDisplay().setFocusedChannels(1 << 1);
        doc->getWaveformDisplay().setSelection(0.5, 0.75);
        dsp.generateAndPlace(doc, 0.0, "Generate Tone",
                             [](juce::AudioBuffer<float>& b, double)
                             {
                                 for (int ch = 0; ch < b.getNumChannels(); ++ch)
                                     juce::FloatVectorOperations::fill(b.getWritePointer(ch), 0.125f,
                                                                       b.getNumSamples());
                             });
        expectEquals(doc->getBufferManager().getNumSamples(), (int64_t) kLength, "same length");
        expectEquals(at(doc, 1, 30000), 0.125f, "right generated");
        expectEquals(at(doc, 0, 30000), kLeft, "left not generated");
    }

    void testAlreadyPerformedPath()
    {
        beginTest("Progress-dialog path (DSP applied before the action is registered) is masked too");

        DocumentManager mgr;
        Document* doc = makeStereoDoc(mgr);
        doc->getWaveformDisplay().setFocusedChannels(1 << 1);

        auto& buffer = doc->getBufferManager().getMutableBuffer();
        const auto before = doc->getBufferManager().getAudioRange(0, kLength);
        buffer.applyGain(2.0f);  // the background job processed every channel

        auto* gainAction = new GainUndoAction(doc->getBufferManager(), doc->getWaveformDisplay(),
                                              doc->getAudioEngine(), before, 0, kLength,
                                              juce::Decibels::gainToDecibels(2.0f), false);
        gainAction->markAsAlreadyPerformed();
        doc->getUndoManager().beginNewTransaction("Gain");
        doc->getUndoManager().perform(ChannelFocus::wrap(*doc, gainAction, before, 0));

        expectEquals(at(doc, 1, 5), kRight * 2.0f, "focused channel processed");
        expectEquals(at(doc, 0, 5), kLeft, "unfocused channel restored");
        doc->getUndoManager().undo();
        expectEquals(at(doc, 1, 5), kRight, "undo restores");
    }

    void testMaskRules()
    {
        beginTest("Focus covering every channel, or a mono file, is not a partial focus");

        expectEquals(ChannelMask::normalise(ChannelMask::kAll, 2), ChannelMask::kAll);
        expectEquals(ChannelMask::normalise(0b11, 2), ChannelMask::kAll, "both of two channels");
        expectEquals(ChannelMask::normalise(0b01, 1), ChannelMask::kAll, "mono");
        expectEquals(ChannelMask::normalise(0b10, 2), 0b10);
        expectEquals(ChannelMask::normalise(0b100, 2), ChannelMask::kAll, "no existing channel");

        DocumentManager mgr;
        Document* doc = makeStereoDoc(mgr);
        expect(!ChannelFocus::isPartial(*doc), "default focus is all channels");
        doc->getWaveformDisplay().setFocusedChannels(1 << 0);
        expect(ChannelFocus::isPartial(*doc), "one of two channels is partial");
    }

    void testRealtimePreview()
    {
        beginTest("Realtime DSP preview changes only the focused channels");

        AudioEngine engine;
        BlockDevice device;
        engine.audioDeviceAboutToStart(&device);
        juce::AudioBuffer<float> audio(2, kLength);
        juce::FloatVectorOperations::fill(audio.getWritePointer(0), kLeft, kLength);
        juce::FloatVectorOperations::fill(audio.getWritePointer(1), kRight, kLength);
        expect(engine.loadFromBuffer(audio, kRate, 2));

        engine.setPreviewMode(PreviewMode::REALTIME_DSP);
        engine.setGainPreview(6.0f, true);
        engine.setPreviewChannelMask(1 << 1);
        engine.setPosition(0.0);
        engine.play();

        juce::AudioBuffer<float> out(2, 512);
        const juce::AudioIODeviceCallbackContext context{};
        for (int block = 0; block < 4; ++block)
        {
            out.clear();
            engine.audioDeviceIOCallbackWithContext(nullptr, 0, out.getArrayOfWritePointers(), 2, 512,
                                                    context);
        }
        const float gain = juce::Decibels::decibelsToGain(6.0f);
        expectWithinAbsoluteError(out.getSample(1, 256), kRight * gain, 1.0e-3f, "focused channel previewed");
        expectWithinAbsoluteError(out.getSample(0, 256), kLeft, 1.0e-6f, "unfocused channel dry");

        engine.stop();
        engine.setPreviewMode(PreviewMode::DISABLED);
    }
};

static ChannelFocusDSPTests channelFocusDSPTests;
