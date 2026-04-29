/*
  ==============================================================================

    MockAudioProcessor.h
    WaveEdit - Professional Audio Editor

    Minimal juce::AudioProcessor for unit tests that need to drive
    parameter listeners (e.g. AutomationRecorderTests). Adds N
    AudioParameterFloat parameters at construction; passes audio
    through unmodified.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace waveedit::test
{

class MockAudioProcessor : public juce::AudioProcessor
{
public:
    explicit MockAudioProcessor(int numParameters = 3)
    {
        for (int i = 0; i < numParameters; ++i)
        {
            auto* p = new juce::AudioParameterFloat(
                juce::ParameterID("p" + juce::String(i), 1),
                "Param " + juce::String(i),
                juce::NormalisableRange<float>(0.0f, 1.0f),
                0.5f);
            addParameter(p);
        }
    }

    // ----- juce::AudioProcessor pure virtuals -----

    const juce::String getName() const override { return "MockAudioProcessor"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

} // namespace waveedit::test
