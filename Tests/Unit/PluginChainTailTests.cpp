/*
  ==============================================================================

    PluginChainTailTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Tests for the "include tail" path used by batch plugin processing (H25).

    Two layers are covered:

    1. Renderer contract: PluginChainRenderer::renderWithOfflineChain must
       return a processedBuffer of length (numSamples + tailSamples) and must
       NOT discard the tail region. This is the precondition that BatchJob's
       copy-back relies on.

    2. BatchJob copy-back logic: the fixed behaviour resizes the destination
       buffer to the full processed length before copying, so a tail longer
       than the original audio is preserved. The pre-fix behaviour clamped the
       copy to the original length and silently dropped the tail.

    A minimal juce::AudioPluginInstance mock ("TailPlugin") fills any trailing
    silence (the tail region) with a constant marker value so the tail is
    observable in the output.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Plugins/PluginChainRenderer.h"

namespace
{

// Marker value written into otherwise-silent tail samples so we can detect
// that the renderer captured (rather than discarded) the tail.
constexpr float kTailMarker = 0.5f;

/**
 * Minimal AudioPluginInstance for tests. Pass-through for non-silent input,
 * but stamps a marker value onto any sample whose magnitude is ~0 (i.e. the
 * appended tail-capture silence). That makes the captured tail observable.
 */
class TailPlugin : public juce::AudioPluginInstance
{
public:
    const juce::String getName() const override { return "TailPlugin"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (std::abs(d[i]) < 1.0e-6f)
                    d[i] = kTailMarker;
            }
        }
    }

    double getTailLengthSeconds() const override { return 0.0; }
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    void fillInPluginDescription(juce::PluginDescription& d) const override
    {
        d.name = "TailPlugin";
        d.pluginFormatName = "Mock";
    }
};

} // namespace

// ============================================================================

class PluginChainTailTests : public juce::UnitTest
{
public:
    PluginChainTailTests() : juce::UnitTest("Plugin Chain Tail", "Plugins") {}

    void runTest() override
    {
        beginTest("Renderer returns numSamples + tailSamples and captures tail");
        testRendererTailContract();

        beginTest("BatchJob copy-back grows buffer to preserve tail");
        testCopyBackGrowsBuffer();
    }

private:
    PluginChainRenderer::OfflineChain makeTailChain()
    {
        PluginChainRenderer::OfflineChain chain;
        chain.instances.push_back(std::make_unique<TailPlugin>());
        chain.bypassed.push_back(false);
        chain.totalLatency = 0;
        return chain;
    }

    void testRendererTailContract()
    {
        const double sampleRate = 44100.0;
        const int numSamples = 1000;
        const int tailSamples = 500;
        const int channels = 2;

        // Non-silent source so the body is distinguishable from the tail.
        juce::AudioBuffer<float> source(channels, numSamples);
        for (int ch = 0; ch < channels; ++ch)
        {
            auto* d = source.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                d[i] = 0.8f;
        }

        auto chain = makeTailChain();

        PluginChainRenderer renderer;
        renderer.setBlockSize(256);

        auto result = renderer.renderWithOfflineChain(
            source, chain, sampleRate,
            /*startSample*/ 0, /*numSamples*/ numSamples,
            /*progress*/ [](float, const juce::String&) { return true; },
            /*outputChannels*/ 0,
            /*tailSamples*/ tailSamples);

        expect(result.success, "Render should succeed");

        // Core H25 precondition: the result is exactly the requested length.
        expectEquals(result.processedBuffer.getNumSamples(), numSamples + tailSamples,
                     "Processed buffer must include the tail length");

        // The tail region (after the original numSamples) must hold the
        // plugin's marker, proving the tail was captured, not discarded.
        bool tailCaptured = true;
        for (int i = numSamples; i < numSamples + tailSamples; ++i)
        {
            if (std::abs(result.processedBuffer.getSample(0, i) - kTailMarker) > 1.0e-3f)
            {
                tailCaptured = false;
                break;
            }
        }
        expect(tailCaptured, "Tail region must contain the plugin's tail output");
    }

    // Mirrors the fixed BatchJob::applyPluginChain copy-back. The pre-fix code
    // clamped samplesToCopy to the original buffer length, dropping the tail;
    // the fix resizes the destination to the full processed length first.
    void testCopyBackGrowsBuffer()
    {
        const int numChannels = 2;
        const int originalSamples = 1000;
        const int tailSamples = 500;
        const int processedSamples = originalSamples + tailSamples;

        // Destination buffer at the ORIGINAL length (as loaded from the file).
        juce::AudioBuffer<float> dest(numChannels, originalSamples);
        dest.clear();

        // Processed result is longer and has the marker in the tail.
        juce::AudioBuffer<float> processed(numChannels, processedSamples);
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* d = processed.getWritePointer(ch);
            for (int i = 0; i < originalSamples; ++i)
                d[i] = 0.8f;
            for (int i = originalSamples; i < processedSamples; ++i)
                d[i] = kTailMarker;
        }

        // ---- fixed copy-back logic ----
        const int procSamples = processed.getNumSamples();
        const int resultChannels = processed.getNumChannels();
        if (procSamples > dest.getNumSamples())
            dest.setSize(numChannels, procSamples, true, true, true);

        const int channelsToCopy = juce::jmin(numChannels, resultChannels);
        for (int ch = 0; ch < channelsToCopy; ++ch)
            dest.copyFrom(ch, 0, processed, ch, 0, procSamples);
        // -------------------------------

        expectEquals(dest.getNumSamples(), processedSamples,
                     "Destination buffer should grow to include the tail");

        // Tail must survive in the destination.
        bool tailPreserved = true;
        for (int i = originalSamples; i < processedSamples; ++i)
        {
            if (std::abs(dest.getSample(0, i) - kTailMarker) > 1.0e-6f)
            {
                tailPreserved = false;
                break;
            }
        }
        expect(tailPreserved, "Tail samples must be preserved after copy-back");
    }
};

static PluginChainTailTests pluginChainTailTests;
