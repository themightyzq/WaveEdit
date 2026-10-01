/*
  ==============================================================================

    PluginRenderCorrectnessTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    Regression tests for two Phase 1 plugin-apply defects:

    1. Recorded plugin parameter automation was never rendered by Apply
       Plugin Chain or batch processing: the offline renderer copied each
       plugin's static state and ignored the automation lanes. The renderer
       now takes a snapshot of the lanes (PluginChainRenderer::
       captureAutomation) and applies it per block at file time.

    2. Applying a plugin chain with an effect tail grew the buffer but left
       regions and markers after the selection at their old positions. The
       commit (DSPController::commitPluginRender) now moves them by the tail
       length in the same undo step.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Plugins/PluginChainRenderer.h"
#include "Automation/AutomationManager.h"
#include "Controllers/DSPController.h"
#include "Utils/DocumentManager.h"
#include "Utils/Document.h"
#include "Utils/Region.h"
#include "Utils/Marker.h"

namespace
{
/** A plain normalised parameter, defaulting to 0.5. */
class GainParameter : public juce::AudioPluginInstance::HostedParameter
{
public:
    float getValue() const override { return m_value.load(); }
    void setValue(float newValue) override { m_value.store(newValue); }
    float getDefaultValue() const override { return 0.5f; }
    juce::String getName(int) const override { return "Gain"; }
    juce::String getLabel() const override { return {}; }
    float getValueForText(const juce::String& text) const override { return text.getFloatValue(); }
    juce::String getParameterID() const override { return "gain"; }

private:
    std::atomic<float> m_value { 0.5f };
};

/** Multiplies its input by its single normalised parameter ("gain"). */
class GainParamPlugin : public juce::AudioPluginInstance
{
public:
    GainParamPlugin()
    {
        addHostedParameter(std::make_unique<GainParameter>());
    }

    const juce::String getName() const override { return "GainParamPlugin"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        buffer.applyGain(getParameters()[0]->getValue());
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
        d.name = "GainParamPlugin";
        d.pluginFormatName = "Mock";
    }
};

constexpr double kSR = 48000.0;

/** One-instance offline chain whose instance was built from chain slot @p slot. */
PluginChainRenderer::OfflineChain makeGainChain(int slot)
{
    PluginChainRenderer::OfflineChain chain;
    chain.instances.push_back(std::make_unique<GainParamPlugin>());
    chain.bypassed.push_back(false);
    chain.chainIndices.push_back(slot);
    chain.totalLatency = 0;
    return chain;
}

/** Lane on chain slot @p slot, param 0: 0 at t=0 rising linearly to 1 at t=1 s. */
void addRampLane(AutomationManager& automation, int slot)
{
    auto& lane = automation.addLane(slot, 0, "GainParamPlugin", "Gain", "gain");
    AutomationPoint a;
    a.timeInSeconds = 0.0;
    a.value = 0.0f;
    AutomationPoint b;
    b.timeInSeconds = 1.0;
    b.value = 1.0f;
    lane.curve.addPoint(a);
    lane.curve.addPoint(b);
}

juce::AudioBuffer<float> ones(int numSamples)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    for (int ch = 0; ch < 2; ++ch)
        juce::FloatVectorOperations::fill(buffer.getWritePointer(ch), 1.0f, numSamples);
    return buffer;
}

Document* makeDoc(DocumentManager& mgr, int numSamples)
{
    Document* doc = mgr.createDocument();
    juce::AudioBuffer<float> buffer(2, numSamples);
    for (int ch = 0; ch < 2; ++ch)
        juce::FloatVectorOperations::fill(buffer.getWritePointer(ch), 0.5f, numSamples);
    doc->getBufferManager().setBuffer(buffer, kSR);
    doc->getAudioEngine().loadFromBuffer(doc->getBufferManager().getBuffer(), kSR, 2);
    doc->getWaveformDisplay().reloadFromBuffer(doc->getBufferManager().getBuffer(), kSR,
                                               false, false);
    return doc;
}

const Region* findRegion(Document* doc, const juce::String& name)
{
    for (const auto& r : doc->getRegionManager().getAllRegions())
        if (r.getName() == name)
            return &r;
    return nullptr;
}

int64_t markerPosition(Document* doc, const juce::String& name)
{
    for (const auto& m : doc->getMarkerManager().getAllMarkers())
        if (m.getName() == name)
            return m.getPosition();
    return -1;
}
} // namespace

//==============================================================================
class PluginRenderCorrectnessTests : public juce::UnitTest
{
public:
    PluginRenderCorrectnessTests() : juce::UnitTest("Plugin Render Correctness", "Plugins") {}

    void runTest() override
    {
        testAutomationIsRendered();
        testAutomationUsesFileTimeOffset();
        testAutomationMapsThroughChainSlots();
        testTailShiftsRegionsAndMarkers();
        testNoTailLeavesTimelineAlone();
    }

private:
    void testAutomationIsRendered()
    {
        beginTest("Apply Plugin Chain renders recorded automation (ramp 0 -> 1 over 1 s)");

        auto chain = makeGainChain(0);
        AutomationManager automation;
        addRampLane(automation, 0);
        PluginChainRenderer::captureAutomation(chain, automation);
        expectEquals((int) chain.automation.size(), 1, "the lane was captured");

        const int n = (int) kSR;  // 1 s of ones
        auto source = ones(n);
        PluginChainRenderer renderer;
        auto result = renderer.renderWithOfflineChain(source, chain, kSR, 0, n, nullptr);
        expect(result.success, "render succeeds");

        // Automation is applied once per render block, so each sample carries
        // the ramp value at the start of its block.
        const float blockTime = (float) PluginChainRenderer::kAutomationBlockSize / (float) kSR;
        for (int i : { 0, 1000, 12000, 24000, 36000, 47999 })
        {
            const float expected = (float) i / (float) kSR;
            expectWithinAbsoluteError(result.processedBuffer.getSample(0, i), expected,
                                      blockTime + 1.0e-4f,
                                      "sample " + juce::String(i) + " follows the automation ramp");
        }
        expectWithinAbsoluteError(result.processedBuffer.getSample(0, 0), 0.0f, 1.0e-6f,
                                  "the first block uses the automation value, not the plugin default 0.5");
    }

    void testAutomationUsesFileTimeOffset()
    {
        beginTest("Automation is evaluated at file time for a copied-out selection");

        auto chain = makeGainChain(0);
        AutomationManager automation;
        addRampLane(automation, 0);
        PluginChainRenderer::captureAutomation(chain, automation);

        // A 0.1 s copy that starts 0.25 s into the file (the async apply path
        // renders a snapshot starting at index 0).
        chain.automationFileOffset = (int64_t) (0.25 * kSR);
        const int n = (int) (0.1 * kSR);
        auto source = ones(n);
        PluginChainRenderer renderer;
        auto result = renderer.renderWithOfflineChain(source, chain, kSR, 0, n, nullptr);
        expect(result.success);
        expectWithinAbsoluteError(result.processedBuffer.getSample(1, 0), 0.25f, 1.0e-4f,
                                  "first sample uses the value at 0.25 s, not at 0 s");
    }

    void testAutomationMapsThroughChainSlots()
    {
        beginTest("Automation lanes map to offline instances through the chain slot");

        // The instance was built from chain slot 1 (slot 0 failed to load).
        auto chain = makeGainChain(1);
        AutomationManager automation;
        addRampLane(automation, 0);  // targets the missing plugin: must be ignored
        PluginChainRenderer::captureAutomation(chain, automation);
        expectEquals((int) chain.automation.size(), 0, "lane for a missing slot is dropped");

        AutomationManager automation2;
        addRampLane(automation2, 1);
        PluginChainRenderer::captureAutomation(chain, automation2);
        expectEquals((int) chain.automation.size(), 1, "lane for slot 1 is kept");
        expectEquals(chain.automation[0].instanceIndex, 0, "slot 1 maps to instance 0");
    }

    void testTailShiftsRegionsAndMarkers()
    {
        beginTest("Plugin apply with a tail moves regions/markers after the selection; undo/redo");

        DocumentManager mgr;
        Document* doc = makeDoc(mgr, 48000);

        const int64_t selStart = 10000, selLen = 10000, tail = 5000;
        auto& regions = doc->getRegionManager();
        regions.addRegion(Region("before", 0, 5000));
        regions.addRegion(Region("spanning", 15000, 25000));
        regions.addRegion(Region("after", 30000, 40000));
        auto& markers = doc->getMarkerManager();
        markers.addMarker(Marker("m-before", 5000));
        markers.addMarker(Marker("m-at-end", selStart + selLen));
        markers.addMarker(Marker("m-after", 30000));

        juce::AudioBuffer<float> processed(2, (int) (selLen + tail));
        processed.clear();

        expect(DSPController::commitPluginRender(doc, selStart, selLen, processed,
                                                 "Apply Plugin Chain: test", "test"),
               "commit succeeds");
        expectEquals((int) doc->getBufferManager().getNumSamples(), 48000 + (int) tail,
                     "file grew by the tail");

        auto check = [this, doc](int64_t shift, const juce::String& when)
        {
            const auto* before = findRegion(doc, "before");
            const auto* spanning = findRegion(doc, "spanning");
            const auto* after = findRegion(doc, "after");
            expect(before != nullptr && spanning != nullptr && after != nullptr,
                   "all regions survive " + when);
            if (before == nullptr || spanning == nullptr || after == nullptr)
                return;
            expectEquals((int) before->getStartSample(), 0, "region before: start " + when);
            expectEquals((int) before->getEndSample(), 5000, "region before: end " + when);
            expectEquals((int) spanning->getStartSample(), 15000, "spanning: start fixed " + when);
            expectEquals((int) spanning->getEndSample(), 25000 + (int) shift, "spanning: end " + when);
            expectEquals((int) after->getStartSample(), 30000 + (int) shift, "after: start " + when);
            expectEquals((int) after->getEndSample(), 40000 + (int) shift, "after: end " + when);
            expectEquals((int) markerPosition(doc, "m-before"), 5000, "marker before " + when);
            expectEquals((int) markerPosition(doc, "m-at-end"), 20000 + (int) shift,
                         "marker at selection end " + when);
            expectEquals((int) markerPosition(doc, "m-after"), 30000 + (int) shift,
                         "marker after " + when);
        };

        check(tail, "after apply");

        expect(doc->getUndoManager().undo(), "undo");
        expectEquals((int) doc->getBufferManager().getNumSamples(), 48000, "undo restores length");
        check(0, "after undo");

        expect(doc->getUndoManager().redo(), "redo");
        expectEquals((int) doc->getBufferManager().getNumSamples(), 48000 + (int) tail,
                     "redo regrows");
        check(tail, "after redo");
    }

    void testNoTailLeavesTimelineAlone()
    {
        beginTest("Plugin apply without a tail does not move regions/markers");

        DocumentManager mgr;
        Document* doc = makeDoc(mgr, 48000);
        doc->getRegionManager().addRegion(Region("after", 30000, 40000));
        doc->getMarkerManager().addMarker(Marker("m-after", 30000));

        juce::AudioBuffer<float> processed(2, 10000);
        processed.clear();
        expect(DSPController::commitPluginRender(doc, 10000, 10000, processed, "t", "t"));

        const auto* after = findRegion(doc, "after");
        expect(after != nullptr && after->getStartSample() == 30000, "region unmoved");
        expectEquals((int) markerPosition(doc, "m-after"), 30000, "marker unmoved");
    }
};

static PluginRenderCorrectnessTests pluginRenderCorrectnessTests;
