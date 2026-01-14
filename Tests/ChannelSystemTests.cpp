/*
  ==============================================================================

    ChannelSystemTests.cpp
    Created: 2026-01-13
    Author:  ZQ SFX
    Copyright (C) 2026 ZQ SFX - All Rights Reserved

    Comprehensive tests for the multichannel audio system:
    - ChannelLayout type detection
    - ITU-R BS.775 downmix coefficient verification
    - Channel extraction buffer integrity
    - Upmix strategy correctness

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "../Source/Audio/ChannelLayout.h"
#include "TestUtils/TestAudioFiles.h"
#include "TestUtils/AudioAssertions.h"

// ============================================================================
// ITU-R BS.775 Constants for verification
// ============================================================================
namespace ITU
{
    constexpr float kUnity = 1.0f;              // 0 dB
    constexpr float kMinus3dB = 0.70710678f;   // 1/sqrt(2) = -3 dB
    constexpr float kMinus6dB = 0.50118723f;   // 10^(-6/20) = -6 dB
    constexpr float kTolerance = 0.001f;        // Test tolerance
}

// ============================================================================
// Channel Layout Type Detection Tests
// ============================================================================

class ChannelLayoutTypeTests : public juce::UnitTest
{
public:
    ChannelLayoutTypeTests() : juce::UnitTest("ChannelLayout Type Detection", "ChannelSystem") {}

    void runTest() override
    {
        beginTest("Channel count to layout type mapping");
        {
            // Standard channel counts should map to expected layouts
            auto mono = waveedit::ChannelLayout::fromChannelCount(1);
            expect(mono.getType() == waveedit::ChannelLayoutType::Mono,
                   "1 channel should be Mono");

            auto stereo = waveedit::ChannelLayout::fromChannelCount(2);
            expect(stereo.getType() == waveedit::ChannelLayoutType::Stereo,
                   "2 channels should be Stereo");

            auto surround51 = waveedit::ChannelLayout::fromChannelCount(6);
            expect(surround51.getType() == waveedit::ChannelLayoutType::Surround_5_1,
                   "6 channels should be 5.1 Surround");

            auto surround71 = waveedit::ChannelLayout::fromChannelCount(8);
            expect(surround71.getType() == waveedit::ChannelLayoutType::Surround_7_1,
                   "8 channels should be 7.1 Surround");
        }

        beginTest("Layout preserves channel count");
        {
            for (int ch = 1; ch <= 8; ++ch)
            {
                auto layout = waveedit::ChannelLayout::fromChannelCount(ch);
                expect(layout.getNumChannels() == ch,
                       "Layout for " + juce::String(ch) + " channels should preserve count");
            }
        }

        beginTest("Speaker position labels are non-empty");
        {
            auto layout = waveedit::ChannelLayout::fromChannelCount(6); // 5.1
            for (int ch = 0; ch < layout.getNumChannels(); ++ch)
            {
                juce::String label = layout.getShortLabel(ch);
                expect(label.isNotEmpty(),
                       "Channel " + juce::String(ch) + " should have a label");
            }
        }
    }
};

static ChannelLayoutTypeTests channelLayoutTypeTests;

// ============================================================================
// Stereo to Mono Downmix Tests (ITU-R BS.775)
// ============================================================================

class StereoToMonoDownmixTests : public juce::UnitTest
{
public:
    StereoToMonoDownmixTests() : juce::UnitTest("Stereo to Mono Downmix", "ChannelSystem") {}

    void runTest() override
    {
        beginTest("Left-only stereo to mono uses -3dB coefficient");
        {
            // Create stereo buffer: L=1.0, R=0.0
            juce::AudioBuffer<float> stereo(2, 1000);
            stereo.clear();
            for (int i = 0; i < stereo.getNumSamples(); ++i)
            {
                stereo.setSample(0, i, 1.0f);  // Left = 1.0
                stereo.setSample(1, i, 0.0f);  // Right = 0.0
            }

            auto mono = waveedit::ChannelConverter::convert(
                stereo, 1, waveedit::ChannelLayoutType::Mono);

            expect(mono.getNumChannels() == 1, "Should produce mono");

            // L at -3dB = 0.707
            float expectedValue = ITU::kMinus3dB;
            float actualValue = mono.getSample(0, 500);

            expect(std::abs(actualValue - expectedValue) < ITU::kTolerance,
                   "Left-only should result in -3dB in mono (expected: " +
                   juce::String(expectedValue) + ", got: " + juce::String(actualValue) + ")");
        }

        beginTest("Right-only stereo to mono uses -3dB coefficient");
        {
            juce::AudioBuffer<float> stereo(2, 1000);
            stereo.clear();
            for (int i = 0; i < stereo.getNumSamples(); ++i)
            {
                stereo.setSample(0, i, 0.0f);  // Left = 0.0
                stereo.setSample(1, i, 1.0f);  // Right = 1.0
            }

            auto mono = waveedit::ChannelConverter::convert(
                stereo, 1, waveedit::ChannelLayoutType::Mono);

            float expectedValue = ITU::kMinus3dB;
            float actualValue = mono.getSample(0, 500);

            expect(std::abs(actualValue - expectedValue) < ITU::kTolerance,
                   "Right-only should result in -3dB in mono");
        }

        beginTest("Centered stereo (L=R) to mono sums correctly");
        {
            // L=1.0, R=1.0 should sum to approximately 1.414 before normalization
            juce::AudioBuffer<float> stereo(2, 1000);
            for (int i = 0; i < stereo.getNumSamples(); ++i)
            {
                stereo.setSample(0, i, 0.5f);  // Left = 0.5
                stereo.setSample(1, i, 0.5f);  // Right = 0.5
            }

            auto mono = waveedit::ChannelConverter::convert(
                stereo, 1, waveedit::ChannelLayoutType::Mono);

            // 0.5 * 0.707 + 0.5 * 0.707 = 0.707
            float expectedValue = 0.5f * ITU::kMinus3dB + 0.5f * ITU::kMinus3dB;
            float actualValue = mono.getSample(0, 500);

            expect(std::abs(actualValue - expectedValue) < ITU::kTolerance,
                   "Centered stereo should sum L+R with -3dB coefficients");
        }

        beginTest("Sample count preserved in mono conversion");
        {
            auto stereo = TestAudio::createSineWave(440.0, 0.5, 44100.0, 1.0, 2);

            auto mono = waveedit::ChannelConverter::convert(
                stereo, 1, waveedit::ChannelLayoutType::Mono);

            expect(mono.getNumSamples() == stereo.getNumSamples(),
                   "Sample count should be preserved");
        }
    }
};

static StereoToMonoDownmixTests stereoToMonoDownmixTests;

// ============================================================================
// 5.1 Surround to Stereo Downmix Tests (ITU-R BS.775)
// ============================================================================

class SurroundToStereoDownmixTests : public juce::UnitTest
{
public:
    SurroundToStereoDownmixTests() : juce::UnitTest("5.1 to Stereo Downmix", "ChannelSystem") {}

    void runTest() override
    {
        // 5.1 channel order (Film/SMPTE): L, R, C, LFE, Ls, Rs
        beginTest("Center channel goes to both L and R at -3dB");
        {
            juce::AudioBuffer<float> surround51(6, 1000);
            surround51.clear();

            // Set only center channel (index 2) to 1.0
            for (int i = 0; i < surround51.getNumSamples(); ++i)
            {
                surround51.setSample(2, i, 1.0f);  // Center = 1.0
            }

            auto stereo = waveedit::ChannelConverter::convert(
                surround51, 2, waveedit::ChannelLayoutType::Stereo,
                waveedit::DownmixPreset::ITU_Standard,
                waveedit::LFEHandling::Exclude);

            expect(stereo.getNumChannels() == 2, "Should produce stereo");

            float leftValue = stereo.getSample(0, 500);
            float rightValue = stereo.getSample(1, 500);

            // Center should appear at -3dB in both channels
            expect(std::abs(leftValue - ITU::kMinus3dB) < ITU::kTolerance,
                   "Center should appear in Left at -3dB");
            expect(std::abs(rightValue - ITU::kMinus3dB) < ITU::kTolerance,
                   "Center should appear in Right at -3dB");
        }

        beginTest("Front Left goes only to Left at unity");
        {
            juce::AudioBuffer<float> surround51(6, 1000);
            surround51.clear();

            for (int i = 0; i < surround51.getNumSamples(); ++i)
            {
                surround51.setSample(0, i, 1.0f);  // Front Left = 1.0
            }

            auto stereo = waveedit::ChannelConverter::convert(
                surround51, 2, waveedit::ChannelLayoutType::Stereo);

            float leftValue = stereo.getSample(0, 500);
            float rightValue = stereo.getSample(1, 500);

            expect(std::abs(leftValue - ITU::kUnity) < ITU::kTolerance,
                   "Front Left should appear in Left at unity (0dB)");
            expect(std::abs(rightValue) < ITU::kTolerance,
                   "Front Left should NOT appear in Right");
        }

        beginTest("LFE excluded by default");
        {
            juce::AudioBuffer<float> surround51(6, 1000);
            surround51.clear();

            // Set only LFE channel (index 3) to 1.0
            for (int i = 0; i < surround51.getNumSamples(); ++i)
            {
                surround51.setSample(3, i, 1.0f);  // LFE = 1.0
            }

            auto stereo = waveedit::ChannelConverter::convert(
                surround51, 2, waveedit::ChannelLayoutType::Stereo,
                waveedit::DownmixPreset::ITU_Standard,
                waveedit::LFEHandling::Exclude);

            float leftValue = stereo.getSample(0, 500);
            float rightValue = stereo.getSample(1, 500);

            expect(std::abs(leftValue) < ITU::kTolerance,
                   "LFE should be excluded from Left");
            expect(std::abs(rightValue) < ITU::kTolerance,
                   "LFE should be excluded from Right");
        }

        beginTest("LFE included at -3dB when requested");
        {
            juce::AudioBuffer<float> surround51(6, 1000);
            surround51.clear();

            for (int i = 0; i < surround51.getNumSamples(); ++i)
            {
                surround51.setSample(3, i, 1.0f);  // LFE = 1.0
            }

            auto stereo = waveedit::ChannelConverter::convert(
                surround51, 2, waveedit::ChannelLayoutType::Stereo,
                waveedit::DownmixPreset::ITU_Standard,
                waveedit::LFEHandling::IncludeMinus3dB);

            float leftValue = stereo.getSample(0, 500);

            expect(std::abs(leftValue - ITU::kMinus3dB) < ITU::kTolerance,
                   "LFE should appear at -3dB when IncludeMinus3dB is set");
        }

        beginTest("Left Surround goes only to Left");
        {
            juce::AudioBuffer<float> surround51(6, 1000);
            surround51.clear();

            // Ls is index 4 in Film/SMPTE order
            for (int i = 0; i < surround51.getNumSamples(); ++i)
            {
                surround51.setSample(4, i, 1.0f);  // Left Surround = 1.0
            }

            auto stereo = waveedit::ChannelConverter::convert(
                surround51, 2, waveedit::ChannelLayoutType::Stereo,
                waveedit::DownmixPreset::ITU_Standard,
                waveedit::LFEHandling::Exclude);

            float leftValue = stereo.getSample(0, 500);
            float rightValue = stereo.getSample(1, 500);

            // Surrounds at -3dB in ITU_Standard preset
            expect(std::abs(leftValue - ITU::kMinus3dB) < ITU::kTolerance,
                   "Left Surround should appear in Left at -3dB");
            expect(std::abs(rightValue) < ITU::kTolerance,
                   "Left Surround should NOT appear in Right");
        }
    }
};

static SurroundToStereoDownmixTests surroundToStereoDownmixTests;

// ============================================================================
// Channel Extraction Tests
// ============================================================================

class ChannelExtractionTests : public juce::UnitTest
{
public:
    ChannelExtractionTests() : juce::UnitTest("Channel Extraction", "ChannelSystem") {}

    void runTest() override
    {
        beginTest("Extract single channel preserves samples exactly");
        {
            // Create stereo with different content on each channel
            juce::AudioBuffer<float> stereo(2, 1000);
            for (int i = 0; i < stereo.getNumSamples(); ++i)
            {
                stereo.setSample(0, i, 0.5f);   // Left = 0.5
                stereo.setSample(1, i, -0.25f); // Right = -0.25
            }

            // Extract right channel only
            std::vector<int> channels = {1};
            auto extracted = waveedit::ChannelConverter::extractChannels(stereo, channels);

            expect(extracted.getNumChannels() == 1, "Should extract 1 channel");
            expect(extracted.getNumSamples() == stereo.getNumSamples(),
                   "Sample count should be preserved");

            // Verify samples are exact copy
            for (int i = 0; i < extracted.getNumSamples(); ++i)
            {
                expect(extracted.getSample(0, i) == stereo.getSample(1, i),
                       "Extracted samples should be bit-identical");
            }
        }

        beginTest("Extract multiple channels preserves order");
        {
            juce::AudioBuffer<float> surround51(6, 1000);
            for (int ch = 0; ch < 6; ++ch)
            {
                float value = static_cast<float>(ch) / 10.0f;
                for (int i = 0; i < surround51.getNumSamples(); ++i)
                {
                    surround51.setSample(ch, i, value);
                }
            }

            // Extract channels 2 and 4 (Center and Left Surround)
            std::vector<int> channels = {2, 4};
            auto extracted = waveedit::ChannelConverter::extractChannels(surround51, channels);

            expect(extracted.getNumChannels() == 2, "Should extract 2 channels");

            // First extracted channel should have channel 2's content
            expect(std::abs(extracted.getSample(0, 500) - 0.2f) < 0.001f,
                   "First extracted should be from channel 2");

            // Second extracted channel should have channel 4's content
            expect(std::abs(extracted.getSample(1, 500) - 0.4f) < 0.001f,
                   "Second extracted should be from channel 4");
        }

        beginTest("Extract with audio content preserves waveform");
        {
            // Create 4-channel buffer with unique sine waves per channel
            juce::AudioBuffer<float> quad(4, 4410);
            for (int ch = 0; ch < 4; ++ch)
            {
                double freq = 440.0 * (ch + 1); // 440, 880, 1320, 1760 Hz
                for (int i = 0; i < quad.getNumSamples(); ++i)
                {
                    float t = static_cast<float>(i) / 44100.0f;
                    quad.setSample(ch, i, std::sin(2.0f * juce::MathConstants<float>::pi * freq * t));
                }
            }

            // Extract channel 2 (1320 Hz)
            std::vector<int> channels = {2};
            auto extracted = waveedit::ChannelConverter::extractChannels(quad, channels);

            // Verify exact sample match
            expect(AudioAssertions::expectBuffersEqual(
                extracted,
                [&]() {
                    juce::AudioBuffer<float> expected(1, quad.getNumSamples());
                    expected.copyFrom(0, 0, quad, 2, 0, quad.getNumSamples());
                    return expected;
                }()),
                "Extracted audio should be bit-identical to source channel");
        }
    }
};

static ChannelExtractionTests channelExtractionTests;

// ============================================================================
// Upmix Strategy Tests
// ============================================================================

class UpmixStrategyTests : public juce::UnitTest
{
public:
    UpmixStrategyTests() : juce::UnitTest("Upmix Strategies", "ChannelSystem") {}

    void runTest() override
    {
        beginTest("FrontOnly: Stereo to 5.1 places audio only in L/R");
        {
            juce::AudioBuffer<float> stereo(2, 1000);
            for (int i = 0; i < stereo.getNumSamples(); ++i)
            {
                stereo.setSample(0, i, 0.8f);  // Left
                stereo.setSample(1, i, 0.6f);  // Right
            }

            auto surround = waveedit::ChannelConverter::convert(
                stereo, 6, waveedit::ChannelLayoutType::Surround_5_1,
                waveedit::DownmixPreset::ITU_Standard,
                waveedit::LFEHandling::Exclude,
                waveedit::UpmixStrategy::FrontOnly);

            expect(surround.getNumChannels() == 6, "Should produce 6 channels");

            // Check Front L/R have content
            expect(std::abs(surround.getSample(0, 500) - 0.8f) < ITU::kTolerance,
                   "Front Left should have stereo Left content");
            expect(std::abs(surround.getSample(1, 500) - 0.6f) < ITU::kTolerance,
                   "Front Right should have stereo Right content");

            // Check C, LFE, Ls, Rs are silent
            expect(std::abs(surround.getSample(2, 500)) < ITU::kTolerance,
                   "Center should be silent in FrontOnly");
            expect(std::abs(surround.getSample(3, 500)) < ITU::kTolerance,
                   "LFE should be silent in FrontOnly");
            expect(std::abs(surround.getSample(4, 500)) < ITU::kTolerance,
                   "Left Surround should be silent in FrontOnly");
            expect(std::abs(surround.getSample(5, 500)) < ITU::kTolerance,
                   "Right Surround should be silent in FrontOnly");
        }

        beginTest("PhantomCenter: Derives center from L+R");
        {
            juce::AudioBuffer<float> stereo(2, 1000);
            for (int i = 0; i < stereo.getNumSamples(); ++i)
            {
                stereo.setSample(0, i, 1.0f);  // Left = 1.0
                stereo.setSample(1, i, 1.0f);  // Right = 1.0
            }

            auto surround = waveedit::ChannelConverter::convert(
                stereo, 6, waveedit::ChannelLayoutType::Surround_5_1,
                waveedit::DownmixPreset::ITU_Standard,
                waveedit::LFEHandling::Exclude,
                waveedit::UpmixStrategy::PhantomCenter);

            // Center should be L*0.707 + R*0.707 = 1.414
            float centerValue = surround.getSample(2, 500);
            float expectedCenter = 1.0f * ITU::kMinus3dB + 1.0f * ITU::kMinus3dB;

            expect(std::abs(centerValue - expectedCenter) < ITU::kTolerance,
                   "Center should be derived from L+R at -3dB each");
        }

        beginTest("FullSurround: Derives surrounds from L/R");
        {
            juce::AudioBuffer<float> stereo(2, 1000);
            for (int i = 0; i < stereo.getNumSamples(); ++i)
            {
                stereo.setSample(0, i, 1.0f);  // Left = 1.0
                stereo.setSample(1, i, 0.0f);  // Right = 0.0
            }

            auto surround = waveedit::ChannelConverter::convert(
                stereo, 6, waveedit::ChannelLayoutType::Surround_5_1,
                waveedit::DownmixPreset::ITU_Standard,
                waveedit::LFEHandling::Exclude,
                waveedit::UpmixStrategy::FullSurround);

            // Left Surround should derive from Left at -6dB
            float lsValue = surround.getSample(4, 500);
            expect(std::abs(lsValue - ITU::kMinus6dB) < ITU::kTolerance,
                   "Left Surround should derive from Left at -6dB");

            // Right Surround should be silent (Right input was 0)
            float rsValue = surround.getSample(5, 500);
            expect(std::abs(rsValue) < ITU::kTolerance,
                   "Right Surround should be silent when Right input is silent");
        }

        beginTest("Mono to stereo duplicates to both channels");
        {
            juce::AudioBuffer<float> mono(1, 1000);
            for (int i = 0; i < mono.getNumSamples(); ++i)
            {
                mono.setSample(0, i, 0.75f);
            }

            auto stereo = waveedit::ChannelConverter::convert(
                mono, 2, waveedit::ChannelLayoutType::Stereo);

            expect(stereo.getNumChannels() == 2, "Should produce stereo");

            float leftValue = stereo.getSample(0, 500);
            float rightValue = stereo.getSample(1, 500);

            expect(std::abs(leftValue - 0.75f) < ITU::kTolerance,
                   "Left should have mono content");
            expect(std::abs(rightValue - 0.75f) < ITU::kTolerance,
                   "Right should have mono content");
        }
    }
};

static UpmixStrategyTests upmixStrategyTests;

// ============================================================================
// Edge Case Tests
// ============================================================================

class ChannelEdgeCaseTests : public juce::UnitTest
{
public:
    ChannelEdgeCaseTests() : juce::UnitTest("Channel Edge Cases", "ChannelSystem") {}

    void runTest() override
    {
        beginTest("Converting to same channel count returns equivalent buffer");
        {
            auto stereo = TestAudio::createSineWave(440.0, 0.5, 44100.0, 0.1, 2);

            auto result = waveedit::ChannelConverter::convert(
                stereo, 2, waveedit::ChannelLayoutType::Stereo);

            expect(result.getNumChannels() == 2, "Should remain stereo");
            expect(result.getNumSamples() == stereo.getNumSamples(),
                   "Sample count should be preserved");
        }

        beginTest("Empty channel extraction returns empty buffer");
        {
            juce::AudioBuffer<float> stereo(2, 1000);
            std::vector<int> noChannels;

            auto extracted = waveedit::ChannelConverter::extractChannels(stereo, noChannels);

            expect(extracted.getNumChannels() == 0 || extracted.getNumSamples() == 0,
                   "Empty channel list should return empty buffer");
        }

        beginTest("Single sample buffer converts correctly");
        {
            // Use values that won't trigger clipping prevention
            juce::AudioBuffer<float> stereo(2, 1);
            stereo.setSample(0, 0, 0.5f);   // L = 0.5
            stereo.setSample(1, 0, 0.5f);   // R = 0.5

            auto mono = waveedit::ChannelConverter::convert(
                stereo, 1, waveedit::ChannelLayoutType::Mono);

            expect(mono.getNumChannels() == 1, "Should produce mono");
            expect(mono.getNumSamples() == 1, "Should have 1 sample");

            // L*0.707 + R*0.707 = 0.5*0.707 + 0.5*0.707 = 0.707 (no clipping)
            float expected = 0.5f * ITU::kMinus3dB + 0.5f * ITU::kMinus3dB;
            expect(std::abs(mono.getSample(0, 0) - expected) < ITU::kTolerance,
                   "Single sample should convert correctly");
        }

        beginTest("Clipping prevention in loud downmix");
        {
            // Create stereo with max amplitude in both channels
            juce::AudioBuffer<float> stereo(2, 1000);
            for (int i = 0; i < stereo.getNumSamples(); ++i)
            {
                stereo.setSample(0, i, 1.0f);
                stereo.setSample(1, i, 1.0f);
            }

            auto mono = waveedit::ChannelConverter::convert(
                stereo, 1, waveedit::ChannelLayoutType::Mono);

            // Result should be normalized to prevent clipping
            float peakValue = mono.getMagnitude(0, 0, mono.getNumSamples());
            expect(peakValue <= 1.0f + ITU::kTolerance,
                   "Downmix should not exceed 0dBFS (peak: " + juce::String(peakValue) + ")");
        }
    }
};

static ChannelEdgeCaseTests channelEdgeCaseTests;

// ============================================================================
// Downmix Preset Tests
// ============================================================================

class DownmixPresetTests : public juce::UnitTest
{
public:
    DownmixPresetTests() : juce::UnitTest("Downmix Presets", "ChannelSystem") {}

    void runTest() override
    {
        beginTest("Professional preset uses -6dB for surrounds");
        {
            juce::AudioBuffer<float> surround51(6, 1000);
            surround51.clear();

            // Set Left Surround to 1.0
            for (int i = 0; i < surround51.getNumSamples(); ++i)
            {
                surround51.setSample(4, i, 1.0f);  // Ls = 1.0
            }

            auto stereo = waveedit::ChannelConverter::convert(
                surround51, 2, waveedit::ChannelLayoutType::Stereo,
                waveedit::DownmixPreset::Professional,  // Uses -6dB for surrounds
                waveedit::LFEHandling::Exclude);

            float leftValue = stereo.getSample(0, 500);

            // Professional preset uses -6dB for surrounds
            expect(std::abs(leftValue - ITU::kMinus6dB) < ITU::kTolerance,
                   "Professional preset should use -6dB for surrounds");
        }

        beginTest("FilmFoldDown preset includes LFE at -6dB");
        {
            juce::AudioBuffer<float> surround51(6, 1000);
            surround51.clear();

            // Set LFE to 1.0
            for (int i = 0; i < surround51.getNumSamples(); ++i)
            {
                surround51.setSample(3, i, 1.0f);  // LFE = 1.0
            }

            auto stereo = waveedit::ChannelConverter::convert(
                surround51, 2, waveedit::ChannelLayoutType::Stereo,
                waveedit::DownmixPreset::FilmFoldDown,
                waveedit::LFEHandling::Exclude);  // FilmFoldDown overrides this

            float leftValue = stereo.getSample(0, 500);

            // FilmFoldDown always includes LFE at -6dB
            expect(std::abs(leftValue - ITU::kMinus6dB) < ITU::kTolerance,
                   "FilmFoldDown preset should include LFE at -6dB");
        }
    }
};

static DownmixPresetTests downmixPresetTests;
