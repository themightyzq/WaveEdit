/*
  ==============================================================================

    ChannelLayout.h
    Created: 2026-01-12
    Author:  WaveEdit

    Channel layout definitions for multichannel audio support.
    Supports mono, stereo, and surround formats up to 7.1.

    References:
    - SMPTE ST 2067-8:2013 (IMF channel ordering)
    - Film industry conventions
    - Microsoft WAVEFORMATEXTENSIBLE speaker positions

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace waveedit
{

//==============================================================================
// Speaker Position flags (compatible with WAVEFORMATEXTENSIBLE dwChannelMask)
//==============================================================================

namespace SpeakerPosition
{
    constexpr uint32_t FRONT_LEFT            = 0x00000001;
    constexpr uint32_t FRONT_RIGHT           = 0x00000002;
    constexpr uint32_t FRONT_CENTER          = 0x00000004;
    constexpr uint32_t LOW_FREQUENCY         = 0x00000008;
    constexpr uint32_t BACK_LEFT             = 0x00000010;
    constexpr uint32_t BACK_RIGHT            = 0x00000020;
    constexpr uint32_t FRONT_LEFT_OF_CENTER  = 0x00000040;
    constexpr uint32_t FRONT_RIGHT_OF_CENTER = 0x00000080;
    constexpr uint32_t BACK_CENTER           = 0x00000100;
    constexpr uint32_t SIDE_LEFT             = 0x00000200;
    constexpr uint32_t SIDE_RIGHT            = 0x00000400;
    constexpr uint32_t TOP_CENTER            = 0x00000800;
    constexpr uint32_t TOP_FRONT_LEFT        = 0x00001000;
    constexpr uint32_t TOP_FRONT_CENTER      = 0x00002000;
    constexpr uint32_t TOP_FRONT_RIGHT       = 0x00004000;
    constexpr uint32_t TOP_BACK_LEFT         = 0x00008000;
    constexpr uint32_t TOP_BACK_CENTER       = 0x00010000;
    constexpr uint32_t TOP_BACK_RIGHT        = 0x00020000;
}

//==============================================================================
// Channel Layout Presets
//==============================================================================

enum class ChannelLayoutType
{
    Unknown,
    Mono,
    Stereo,
    LCR,            // 3.0 Left, Center, Right
    Quad,           // 4.0 Quadraphonic
    Surround_5_0,   // 5.0 (no LFE)
    Surround_5_1,   // 5.1 (with LFE)
    Surround_6_1,   // 6.1
    Surround_7_1,   // 7.1
    Custom          // User-defined or unknown layout
};

//==============================================================================
// Channel ordering standards
//==============================================================================

enum class ChannelOrderStandard
{
    // Film/SMPTE standard (most common for broadcast/film):
    // 5.1: L, R, C, LFE, Ls, Rs
    // 7.1: L, R, C, LFE, Ls, Rs, Lrs, Rrs
    Film_SMPTE,

    // DTS/Logic Pro ordering:
    // 5.1: L, R, Ls, Rs, C, LFE
    DTS_LogicPro,

    // Pro Tools ordering (AAF):
    // 5.1: L, C, R, Ls, Rs, LFE
    ProTools_AAF,

    // Microsoft WAVE ordering (based on speaker position bit order):
    // 5.1: L, R, C, LFE, Ls, Rs (same as Film/SMPTE for 5.1)
    Wave
};

//==============================================================================
// Individual channel info
//==============================================================================

struct ChannelInfo
{
    juce::String shortLabel;    // "L", "R", "C", "LFE", etc.
    juce::String fullName;      // "Left", "Right", "Center", "Low Frequency Effects"
    uint32_t speakerPosition;   // WAVEFORMATEXTENSIBLE speaker flag
    float defaultPanPosition;   // -1.0 (left) to 1.0 (right), 0.0 center
};

//==============================================================================
// Channel Layout class
//==============================================================================

class ChannelLayout
{
public:
    //==========================================================================
    // Construction
    //==========================================================================

    ChannelLayout() : m_type(ChannelLayoutType::Unknown), m_channelMask(0) {}

    explicit ChannelLayout(int numChannels)
        : m_type(getDefaultTypeForChannelCount(numChannels))
        , m_channelMask(getDefaultMaskForChannelCount(numChannels))
    {
        initializeChannelInfo();
    }

    ChannelLayout(ChannelLayoutType type)
        : m_type(type)
        , m_channelMask(getMaskForType(type))
    {
        initializeChannelInfo();
    }

    //==========================================================================
    // Accessors
    //==========================================================================

    ChannelLayoutType getType() const { return m_type; }
    int getNumChannels() const { return static_cast<int>(m_channels.size()); }
    uint32_t getChannelMask() const { return m_channelMask; }

    const ChannelInfo& getChannelInfo(int channelIndex) const
    {
        static const ChannelInfo unknown = { "?", "Unknown", 0, 0.0f };
        if (channelIndex >= 0 && channelIndex < static_cast<int>(m_channels.size()))
            return m_channels[static_cast<size_t>(channelIndex)];
        return unknown;
    }

    juce::String getShortLabel(int channelIndex) const
    {
        return getChannelInfo(channelIndex).shortLabel;
    }

    juce::String getFullName(int channelIndex) const
    {
        return getChannelInfo(channelIndex).fullName;
    }

    //==========================================================================
    // Layout name for display
    //==========================================================================

    juce::String getLayoutName() const
    {
        switch (m_type)
        {
            case ChannelLayoutType::Mono:        return "Mono";
            case ChannelLayoutType::Stereo:      return "Stereo";
            case ChannelLayoutType::LCR:         return "3.0 (L-C-R)";
            case ChannelLayoutType::Quad:        return "4.0 Quad";
            case ChannelLayoutType::Surround_5_0: return "5.0 Surround";
            case ChannelLayoutType::Surround_5_1: return "5.1 Surround";
            case ChannelLayoutType::Surround_6_1: return "6.1 Surround";
            case ChannelLayoutType::Surround_7_1: return "7.1 Surround";
            case ChannelLayoutType::Custom:
            case ChannelLayoutType::Unknown:
            default:
                return juce::String(getNumChannels()) + " Channels";
        }
    }

    //==========================================================================
    // Static factory methods
    //==========================================================================

    static ChannelLayout mono()    { return ChannelLayout(ChannelLayoutType::Mono); }
    static ChannelLayout stereo()  { return ChannelLayout(ChannelLayoutType::Stereo); }
    static ChannelLayout surround51() { return ChannelLayout(ChannelLayoutType::Surround_5_1); }
    static ChannelLayout surround71() { return ChannelLayout(ChannelLayoutType::Surround_7_1); }

    static ChannelLayout fromChannelCount(int numChannels)
    {
        return ChannelLayout(numChannels);
    }

    //==========================================================================
    // Channel count for layout type
    //==========================================================================

    static int getChannelCountForType(ChannelLayoutType type)
    {
        switch (type)
        {
            case ChannelLayoutType::Mono:         return 1;
            case ChannelLayoutType::Stereo:       return 2;
            case ChannelLayoutType::LCR:          return 3;
            case ChannelLayoutType::Quad:         return 4;
            case ChannelLayoutType::Surround_5_0: return 5;
            case ChannelLayoutType::Surround_5_1: return 6;
            case ChannelLayoutType::Surround_6_1: return 7;
            case ChannelLayoutType::Surround_7_1: return 8;
            default: return 0;
        }
    }

    //==========================================================================
    // Available presets for a given channel count
    //==========================================================================

    static std::vector<ChannelLayoutType> getAvailableLayoutsForChannelCount(int numChannels)
    {
        std::vector<ChannelLayoutType> layouts;

        switch (numChannels)
        {
            case 1:
                layouts.push_back(ChannelLayoutType::Mono);
                break;
            case 2:
                layouts.push_back(ChannelLayoutType::Stereo);
                break;
            case 3:
                layouts.push_back(ChannelLayoutType::LCR);
                break;
            case 4:
                layouts.push_back(ChannelLayoutType::Quad);
                break;
            case 5:
                layouts.push_back(ChannelLayoutType::Surround_5_0);
                break;
            case 6:
                layouts.push_back(ChannelLayoutType::Surround_5_1);
                break;
            case 7:
                layouts.push_back(ChannelLayoutType::Surround_6_1);
                break;
            case 8:
                layouts.push_back(ChannelLayoutType::Surround_7_1);
                break;
            default:
                break;
        }

        layouts.push_back(ChannelLayoutType::Custom);
        return layouts;
    }

private:
    ChannelLayoutType m_type;
    uint32_t m_channelMask;
    std::vector<ChannelInfo> m_channels;

    //==========================================================================
    // Default type for channel count
    //==========================================================================

    static ChannelLayoutType getDefaultTypeForChannelCount(int numChannels)
    {
        switch (numChannels)
        {
            case 1: return ChannelLayoutType::Mono;
            case 2: return ChannelLayoutType::Stereo;
            case 3: return ChannelLayoutType::LCR;
            case 4: return ChannelLayoutType::Quad;
            case 5: return ChannelLayoutType::Surround_5_0;
            case 6: return ChannelLayoutType::Surround_5_1;
            case 7: return ChannelLayoutType::Surround_6_1;
            case 8: return ChannelLayoutType::Surround_7_1;
            default: return ChannelLayoutType::Custom;
        }
    }

    //==========================================================================
    // WAVEFORMATEXTENSIBLE channel masks
    //==========================================================================

    static uint32_t getDefaultMaskForChannelCount(int numChannels)
    {
        return getMaskForType(getDefaultTypeForChannelCount(numChannels));
    }

    static uint32_t getMaskForType(ChannelLayoutType type)
    {
        using namespace SpeakerPosition;

        switch (type)
        {
            case ChannelLayoutType::Mono:
                return FRONT_CENTER;

            case ChannelLayoutType::Stereo:
                return FRONT_LEFT | FRONT_RIGHT;

            case ChannelLayoutType::LCR:
                return FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER;

            case ChannelLayoutType::Quad:
                return FRONT_LEFT | FRONT_RIGHT | BACK_LEFT | BACK_RIGHT;

            case ChannelLayoutType::Surround_5_0:
                return FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | BACK_LEFT | BACK_RIGHT;

            case ChannelLayoutType::Surround_5_1:
                return FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | LOW_FREQUENCY | BACK_LEFT | BACK_RIGHT;

            case ChannelLayoutType::Surround_6_1:
                return FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | LOW_FREQUENCY |
                       BACK_LEFT | BACK_RIGHT | BACK_CENTER;

            case ChannelLayoutType::Surround_7_1:
                return FRONT_LEFT | FRONT_RIGHT | FRONT_CENTER | LOW_FREQUENCY |
                       BACK_LEFT | BACK_RIGHT | SIDE_LEFT | SIDE_RIGHT;

            default:
                return 0;
        }
    }

    //==========================================================================
    // Initialize channel info based on type
    //==========================================================================

    void initializeChannelInfo()
    {
        m_channels.clear();

        // Film/SMPTE ordering is the default
        switch (m_type)
        {
            case ChannelLayoutType::Mono:
                m_channels.push_back({ "M", "Mono", SpeakerPosition::FRONT_CENTER, 0.0f });
                break;

            case ChannelLayoutType::Stereo:
                m_channels.push_back({ "L", "Left", SpeakerPosition::FRONT_LEFT, -1.0f });
                m_channels.push_back({ "R", "Right", SpeakerPosition::FRONT_RIGHT, 1.0f });
                break;

            case ChannelLayoutType::LCR:
                m_channels.push_back({ "L", "Left", SpeakerPosition::FRONT_LEFT, -1.0f });
                m_channels.push_back({ "R", "Right", SpeakerPosition::FRONT_RIGHT, 1.0f });
                m_channels.push_back({ "C", "Center", SpeakerPosition::FRONT_CENTER, 0.0f });
                break;

            case ChannelLayoutType::Quad:
                m_channels.push_back({ "L", "Left", SpeakerPosition::FRONT_LEFT, -1.0f });
                m_channels.push_back({ "R", "Right", SpeakerPosition::FRONT_RIGHT, 1.0f });
                m_channels.push_back({ "Ls", "Left Surround", SpeakerPosition::BACK_LEFT, -0.7f });
                m_channels.push_back({ "Rs", "Right Surround", SpeakerPosition::BACK_RIGHT, 0.7f });
                break;

            case ChannelLayoutType::Surround_5_0:
                m_channels.push_back({ "L", "Left", SpeakerPosition::FRONT_LEFT, -1.0f });
                m_channels.push_back({ "R", "Right", SpeakerPosition::FRONT_RIGHT, 1.0f });
                m_channels.push_back({ "C", "Center", SpeakerPosition::FRONT_CENTER, 0.0f });
                m_channels.push_back({ "Ls", "Left Surround", SpeakerPosition::BACK_LEFT, -0.7f });
                m_channels.push_back({ "Rs", "Right Surround", SpeakerPosition::BACK_RIGHT, 0.7f });
                break;

            case ChannelLayoutType::Surround_5_1:
                // Film/SMPTE order: L, R, C, LFE, Ls, Rs
                m_channels.push_back({ "L", "Left", SpeakerPosition::FRONT_LEFT, -1.0f });
                m_channels.push_back({ "R", "Right", SpeakerPosition::FRONT_RIGHT, 1.0f });
                m_channels.push_back({ "C", "Center", SpeakerPosition::FRONT_CENTER, 0.0f });
                m_channels.push_back({ "LFE", "Low Frequency Effects", SpeakerPosition::LOW_FREQUENCY, 0.0f });
                m_channels.push_back({ "Ls", "Left Surround", SpeakerPosition::BACK_LEFT, -0.7f });
                m_channels.push_back({ "Rs", "Right Surround", SpeakerPosition::BACK_RIGHT, 0.7f });
                break;

            case ChannelLayoutType::Surround_6_1:
                m_channels.push_back({ "L", "Left", SpeakerPosition::FRONT_LEFT, -1.0f });
                m_channels.push_back({ "R", "Right", SpeakerPosition::FRONT_RIGHT, 1.0f });
                m_channels.push_back({ "C", "Center", SpeakerPosition::FRONT_CENTER, 0.0f });
                m_channels.push_back({ "LFE", "Low Frequency Effects", SpeakerPosition::LOW_FREQUENCY, 0.0f });
                m_channels.push_back({ "Ls", "Left Surround", SpeakerPosition::BACK_LEFT, -0.7f });
                m_channels.push_back({ "Rs", "Right Surround", SpeakerPosition::BACK_RIGHT, 0.7f });
                m_channels.push_back({ "Cs", "Center Surround", SpeakerPosition::BACK_CENTER, 0.0f });
                break;

            case ChannelLayoutType::Surround_7_1:
                // 7.1 Film/SMPTE: L, R, C, LFE, Ls, Rs, Lrs, Rrs
                m_channels.push_back({ "L", "Left", SpeakerPosition::FRONT_LEFT, -1.0f });
                m_channels.push_back({ "R", "Right", SpeakerPosition::FRONT_RIGHT, 1.0f });
                m_channels.push_back({ "C", "Center", SpeakerPosition::FRONT_CENTER, 0.0f });
                m_channels.push_back({ "LFE", "Low Frequency Effects", SpeakerPosition::LOW_FREQUENCY, 0.0f });
                m_channels.push_back({ "Lss", "Left Side Surround", SpeakerPosition::SIDE_LEFT, -0.5f });
                m_channels.push_back({ "Rss", "Right Side Surround", SpeakerPosition::SIDE_RIGHT, 0.5f });
                m_channels.push_back({ "Lrs", "Left Rear Surround", SpeakerPosition::BACK_LEFT, -0.7f });
                m_channels.push_back({ "Rrs", "Right Rear Surround", SpeakerPosition::BACK_RIGHT, 0.7f });
                break;

            case ChannelLayoutType::Custom:
            case ChannelLayoutType::Unknown:
            default:
                // Generic numbered channels
                break;
        }
    }
};

//==============================================================================
// Channel Converter utilities
//==============================================================================

class ChannelConverter
{
public:
    //==========================================================================
    // Convert buffer to target channel count
    //==========================================================================

    /**
     * Convert audio buffer to a different channel count.
     *
     * @param source Source audio buffer
     * @param targetChannels Target number of channels
     * @param targetLayout Optional target layout for proper mapping
     * @return Converted audio buffer
     */
    static juce::AudioBuffer<float> convert(const juce::AudioBuffer<float>& source,
                                             int targetChannels,
                                             ChannelLayoutType targetLayout = ChannelLayoutType::Unknown)
    {
        int sourceChannels = source.getNumChannels();
        int numSamples = source.getNumSamples();

        if (sourceChannels == targetChannels)
        {
            // Same channel count - just copy
            juce::AudioBuffer<float> result(targetChannels, numSamples);
            for (int ch = 0; ch < targetChannels; ++ch)
                result.copyFrom(ch, 0, source, ch, 0, numSamples);
            return result;
        }

        juce::AudioBuffer<float> result(targetChannels, numSamples);
        result.clear();

        if (targetChannels == 1)
        {
            // Downmix to mono - average all channels
            mixdownToMono(source, result);
        }
        else if (sourceChannels == 1)
        {
            // Upmix from mono - copy to all channels
            upmixFromMono(source, result, targetChannels);
        }
        else if (targetChannels == 2)
        {
            // Downmix to stereo
            mixdownToStereo(source, result);
        }
        else if (sourceChannels == 2)
        {
            // Upmix from stereo
            upmixFromStereo(source, result, targetChannels, targetLayout);
        }
        else
        {
            // General case: copy matching channels, pad with silence
            generalConvert(source, result);
        }

        return result;
    }

    //==========================================================================
    // Mixdown to mono
    //==========================================================================

    static void mixdownToMono(const juce::AudioBuffer<float>& source,
                               juce::AudioBuffer<float>& dest)
    {
        int numChannels = source.getNumChannels();
        int numSamples = source.getNumSamples();
        float* monoData = dest.getWritePointer(0);
        float scale = 1.0f / static_cast<float>(numChannels);

        // Clear destination
        dest.clear();

        // Sum all channels with equal weighting
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* channelData = source.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                monoData[i] += channelData[i] * scale;
            }
        }
    }

    //==========================================================================
    // Upmix from mono
    //==========================================================================

    static void upmixFromMono(const juce::AudioBuffer<float>& source,
                               juce::AudioBuffer<float>& dest,
                               int targetChannels)
    {
        const float* monoData = source.getReadPointer(0);
        int numSamples = source.getNumSamples();

        // Copy mono to all target channels
        for (int ch = 0; ch < targetChannels; ++ch)
        {
            dest.copyFrom(ch, 0, monoData, numSamples);
        }
    }

    //==========================================================================
    // Mixdown to stereo
    //==========================================================================

    static void mixdownToStereo(const juce::AudioBuffer<float>& source,
                                 juce::AudioBuffer<float>& dest)
    {
        int sourceChannels = source.getNumChannels();
        int numSamples = source.getNumSamples();

        dest.clear();

        float* leftData = dest.getWritePointer(0);
        float* rightData = dest.getWritePointer(1);

        // Get source layout for proper mixing
        ChannelLayout layout = ChannelLayout::fromChannelCount(sourceChannels);

        // Standard downmix coefficients
        for (int ch = 0; ch < sourceChannels; ++ch)
        {
            const float* channelData = source.getReadPointer(ch);
            const ChannelInfo& info = layout.getChannelInfo(ch);

            // Pan position determines L/R contribution
            float leftGain = (1.0f - info.defaultPanPosition) * 0.5f;
            float rightGain = (1.0f + info.defaultPanPosition) * 0.5f;

            // LFE gets reduced level in stereo downmix
            if (info.speakerPosition == SpeakerPosition::LOW_FREQUENCY)
            {
                leftGain *= 0.707f;  // -3dB
                rightGain *= 0.707f;
            }

            for (int i = 0; i < numSamples; ++i)
            {
                leftData[i] += channelData[i] * leftGain;
                rightData[i] += channelData[i] * rightGain;
            }
        }

        // Normalize to prevent clipping (simple limiting)
        float maxLevel = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            maxLevel = juce::jmax(maxLevel, std::abs(leftData[i]), std::abs(rightData[i]));
        }

        if (maxLevel > 1.0f)
        {
            float scale = 1.0f / maxLevel;
            for (int i = 0; i < numSamples; ++i)
            {
                leftData[i] *= scale;
                rightData[i] *= scale;
            }
        }
    }

    //==========================================================================
    // Upmix from stereo
    //==========================================================================

    static void upmixFromStereo(const juce::AudioBuffer<float>& source,
                                 juce::AudioBuffer<float>& dest,
                                 int targetChannels,
                                 ChannelLayoutType targetLayout)
    {
        const float* leftData = source.getReadPointer(0);
        const float* rightData = source.getReadPointer(1);
        int numSamples = source.getNumSamples();

        ChannelLayout layout(targetLayout != ChannelLayoutType::Unknown
                             ? targetLayout
                             : ChannelLayout::fromChannelCount(targetChannels).getType());

        for (int ch = 0; ch < targetChannels && ch < layout.getNumChannels(); ++ch)
        {
            float* destData = dest.getWritePointer(ch);
            const ChannelInfo& info = layout.getChannelInfo(ch);

            // Derive from stereo based on pan position
            float leftContrib = (1.0f - info.defaultPanPosition) * 0.5f;
            float rightContrib = (1.0f + info.defaultPanPosition) * 0.5f;

            for (int i = 0; i < numSamples; ++i)
            {
                destData[i] = leftData[i] * leftContrib + rightData[i] * rightContrib;
            }
        }
    }

    //==========================================================================
    // General conversion
    //==========================================================================

    static void generalConvert(const juce::AudioBuffer<float>& source,
                                juce::AudioBuffer<float>& dest)
    {
        int sourceChannels = source.getNumChannels();
        int destChannels = dest.getNumChannels();
        int numSamples = source.getNumSamples();

        // Copy matching channels
        int channelsToCopy = juce::jmin(sourceChannels, destChannels);
        for (int ch = 0; ch < channelsToCopy; ++ch)
        {
            dest.copyFrom(ch, 0, source, ch, 0, numSamples);
        }

        // Remaining channels are already cleared
    }
};

} // namespace waveedit
