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
// ITU-R BS.775 Standard Gain Coefficients for Downmix/Upmix
// These constants are used throughout the channel conversion system.
//==============================================================================

namespace ITUCoefficients
{
    constexpr float kUnityGain = 1.0f;            // 0 dB (full level)
    constexpr float kMinus3dB  = 0.70710678f;     // -3 dB = 1/sqrt(2)
    constexpr float kMinus6dB  = 0.50118723f;     // -6 dB = 10^(-6/20)
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
// Downmix preset types for professional workflows
//==============================================================================

enum class DownmixPreset
{
    ITU_Standard,    // ITU-R BS.775: Center -3dB, Surrounds -3dB, LFE muted
    Professional,    // Center -3dB, Surrounds -6dB, LFE muted
    FilmFoldDown,    // Center -3dB, Surrounds -3dB, LFE -6dB
    Custom           // User-defined coefficients
};

//==============================================================================
// LFE handling options
//==============================================================================

enum class LFEHandling
{
    Exclude,         // Do not include LFE in downmix (recommended)
    IncludeMinus3dB, // Include at -3dB
    IncludeMinus6dB  // Include at -6dB
};

//==============================================================================
// Upmix strategy options
//==============================================================================

enum class UpmixStrategy
{
    FrontOnly,       // L/R to front speakers only, silence elsewhere (Recommended)
    PhantomCenter,   // L/R front, derive center from L+R at -3dB
    FullSurround,    // L/R front, C = (L+R)*0.707, Ls = L*0.5, Rs = R*0.5
    Duplicate        // Current pan-based behavior, duplicate to all channels
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
        // Default to ITU Standard preset with LFE excluded
        return convert(source, targetChannels, targetLayout,
                       DownmixPreset::ITU_Standard, LFEHandling::Exclude);
    }

    /**
     * Convert audio buffer with custom downmix/upmix settings.
     *
     * @param source Source audio buffer
     * @param targetChannels Target number of channels
     * @param targetLayout Optional target layout for proper mapping
     * @param preset Downmix preset (ITU_Standard, Professional, FilmFoldDown, Custom)
     * @param lfeHandling How to handle LFE channel (Exclude, IncludeMinus3dB, IncludeMinus6dB)
     * @param upmixStrategy How to handle upmixing (FrontOnly, PhantomCenter, FullSurround, Duplicate)
     * @return Converted audio buffer
     */
    static juce::AudioBuffer<float> convert(const juce::AudioBuffer<float>& source,
                                             int targetChannels,
                                             ChannelLayoutType targetLayout,
                                             DownmixPreset preset,
                                             LFEHandling lfeHandling,
                                             UpmixStrategy upmixStrategy = UpmixStrategy::FrontOnly)
    {
        int sourceChannels = source.getNumChannels();
        int numSamples = source.getNumSamples();

        // Guard: empty or invalid buffer
        if (sourceChannels <= 0 || numSamples <= 0 || targetChannels <= 0)
        {
            // Return minimal valid buffer matching target channel count
            return juce::AudioBuffer<float>(juce::jmax(1, targetChannels), 0);
        }

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
            // Downmix to mono using ITU coefficients
            mixdownToMono(source, result, lfeHandling);
        }
        else if (sourceChannels == 1)
        {
            // Upmix from mono - copy to all channels
            upmixFromMono(source, result, targetChannels);
        }
        else if (targetChannels == 2)
        {
            // Downmix to stereo using ITU-R BS.775 coefficients
            mixdownToStereo(source, result, preset, lfeHandling);
        }
        else if (sourceChannels == 2)
        {
            // Upmix from stereo with selected strategy
            upmixFromStereo(source, result, targetChannels, targetLayout, upmixStrategy);
        }
        else
        {
            // General case: copy matching channels, pad with silence
            generalConvert(source, result);
        }

        return result;
    }

    /**
     * Extract specific channels from source buffer.
     *
     * @param source Source audio buffer
     * @param channelIndices Vector of channel indices to extract (0-based)
     * @return New buffer containing only the selected channels
     */
    static juce::AudioBuffer<float> extractChannels(const juce::AudioBuffer<float>& source,
                                                     const std::vector<int>& channelIndices)
    {
        int numSamples = source.getNumSamples();
        int outputChannels = static_cast<int>(channelIndices.size());

        // Guard: empty buffer
        if (numSamples <= 0 || source.getNumChannels() <= 0)
        {
            return juce::AudioBuffer<float>(juce::jmax(1, outputChannels), 0);
        }

        if (outputChannels == 0)
            return juce::AudioBuffer<float>(0, 0); // Return empty buffer for empty channel list

        juce::AudioBuffer<float> result(outputChannels, numSamples);

        for (int destCh = 0; destCh < outputChannels; ++destCh)
        {
            int srcCh = channelIndices[static_cast<size_t>(destCh)];
            if (srcCh >= 0 && srcCh < source.getNumChannels())
            {
                result.copyFrom(destCh, 0, source, srcCh, 0, numSamples);
            }
            else
            {
                result.clear(destCh, 0, numSamples);
            }
        }

        return result;
    }

    //==========================================================================
    // Mixdown to mono using ITU-R BS.775 coefficients
    //==========================================================================

    static void mixdownToMono(const juce::AudioBuffer<float>& source,
                               juce::AudioBuffer<float>& dest,
                               LFEHandling lfeHandling = LFEHandling::Exclude)
    {
        int numChannels = source.getNumChannels();
        int numSamples = source.getNumSamples();

        // Guard: empty or invalid buffers
        if (numChannels <= 0 || numSamples <= 0 || dest.getNumChannels() < 1 || dest.getNumSamples() < numSamples)
        {
            if (dest.getNumSamples() > 0)
                dest.clear();
            return;
        }

        // IMPORTANT: Clear destination FIRST, then get pointer
        // Getting the pointer before clear() can result in an invalid pointer
        dest.clear();
        float* monoData = dest.getWritePointer(0);

        // LFE gain based on handling option
        float lfeGain = 0.0f;
        switch (lfeHandling)
        {
            case LFEHandling::Exclude:         lfeGain = 0.0f; break;
            case LFEHandling::IncludeMinus3dB: lfeGain = ITUCoefficients::kMinus3dB; break;
            case LFEHandling::IncludeMinus6dB: lfeGain = ITUCoefficients::kMinus6dB; break;
        }

        // Get layout for speaker position mapping
        ChannelLayout layout = ChannelLayout::fromChannelCount(numChannels);

        // For stereo, use ITU standard: M = 0.707*L + 0.707*R
        // For multichannel, use speaker position-aware mixing
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float* channelData = source.getReadPointer(ch);
            const ChannelInfo& info = layout.getChannelInfo(ch);

            float gain = 0.0f;

            switch (info.speakerPosition)
            {
                case SpeakerPosition::FRONT_LEFT:
                case SpeakerPosition::FRONT_RIGHT:
                    // L/R at -3dB each for stereo-to-mono
                    gain = ITUCoefficients::kMinus3dB;
                    break;

                case SpeakerPosition::FRONT_CENTER:
                    // Center at unity (full level) in mono
                    gain = ITUCoefficients::kUnityGain;
                    break;

                case SpeakerPosition::LOW_FREQUENCY:
                    gain = lfeGain;
                    break;

                case SpeakerPosition::BACK_LEFT:
                case SpeakerPosition::BACK_RIGHT:
                case SpeakerPosition::SIDE_LEFT:
                case SpeakerPosition::SIDE_RIGHT:
                case SpeakerPosition::BACK_CENTER:
                    // Surrounds at -6dB in mono mix
                    gain = ITUCoefficients::kMinus6dB;
                    break;

                default:
                    // Unknown - use simple averaging fallback
                    gain = 1.0f / static_cast<float>(numChannels);
                    break;
            }

            for (int i = 0; i < numSamples; ++i)
            {
                monoData[i] += channelData[i] * gain;
            }
        }

        // Normalize to prevent clipping
        float maxLevel = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            maxLevel = juce::jmax(maxLevel, std::abs(monoData[i]));
        }

        if (maxLevel > 1.0f)
        {
            float scale = 1.0f / maxLevel;
            for (int i = 0; i < numSamples; ++i)
            {
                monoData[i] *= scale;
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
        int numSamples = source.getNumSamples();

        // Guard: empty or invalid buffers
        if (source.getNumChannels() < 1 || numSamples <= 0 || targetChannels <= 0 || dest.getNumSamples() < numSamples)
        {
            if (dest.getNumSamples() > 0)
                dest.clear();
            return;
        }

        const float* monoData = source.getReadPointer(0);

        // Copy mono to all target channels
        for (int ch = 0; ch < targetChannels && ch < dest.getNumChannels(); ++ch)
        {
            dest.copyFrom(ch, 0, monoData, numSamples);
        }
    }

    //==========================================================================
    // Mixdown to stereo using ITU-R BS.775 standard coefficients
    //==========================================================================

    static void mixdownToStereo(const juce::AudioBuffer<float>& source,
                                 juce::AudioBuffer<float>& dest,
                                 DownmixPreset preset = DownmixPreset::ITU_Standard,
                                 LFEHandling lfeHandling = LFEHandling::Exclude)
    {
        int sourceChannels = source.getNumChannels();
        int numSamples = source.getNumSamples();

        // Guard: empty or invalid buffers
        if (sourceChannels <= 0 || numSamples <= 0 || dest.getNumChannels() < 2 || dest.getNumSamples() < numSamples)
        {
            if (dest.getNumSamples() > 0)
                dest.clear();
            return;
        }

        dest.clear();

        float* leftData = dest.getWritePointer(0);
        float* rightData = dest.getWritePointer(1);

        // Select surround attenuation based on preset
        float surroundGain = (preset == DownmixPreset::Professional)
                             ? ITUCoefficients::kMinus6dB
                             : ITUCoefficients::kMinus3dB;
        float centerGain = ITUCoefficients::kMinus3dB;

        // LFE gain based on handling option
        float lfeGain = 0.0f;
        switch (lfeHandling)
        {
            case LFEHandling::Exclude:         lfeGain = 0.0f; break;
            case LFEHandling::IncludeMinus3dB: lfeGain = ITUCoefficients::kMinus3dB; break;
            case LFEHandling::IncludeMinus6dB: lfeGain = ITUCoefficients::kMinus6dB; break;
        }

        // Override LFE for Film Fold-Down preset
        if (preset == DownmixPreset::FilmFoldDown)
            lfeGain = ITUCoefficients::kMinus6dB;

        // Get source layout for speaker position mapping
        ChannelLayout layout = ChannelLayout::fromChannelCount(sourceChannels);

        // Apply ITU-R BS.775 downmix coefficients based on speaker position
        for (int ch = 0; ch < sourceChannels; ++ch)
        {
            const float* channelData = source.getReadPointer(ch);
            const ChannelInfo& info = layout.getChannelInfo(ch);

            float leftGain = 0.0f;
            float rightGain = 0.0f;

            // Map speaker position to stereo using ITU coefficients
            switch (info.speakerPosition)
            {
                case SpeakerPosition::FRONT_LEFT:
                    leftGain = ITUCoefficients::kUnityGain;
                    rightGain = 0.0f;
                    break;

                case SpeakerPosition::FRONT_RIGHT:
                    leftGain = 0.0f;
                    rightGain = ITUCoefficients::kUnityGain;
                    break;

                case SpeakerPosition::FRONT_CENTER:
                    // Center goes to both channels at -3dB
                    leftGain = centerGain;
                    rightGain = centerGain;
                    break;

                case SpeakerPosition::LOW_FREQUENCY:
                    // LFE based on handling option (default: excluded)
                    leftGain = lfeGain;
                    rightGain = lfeGain;
                    break;

                case SpeakerPosition::BACK_LEFT:
                case SpeakerPosition::SIDE_LEFT:
                    // Left surround goes to left channel only
                    leftGain = surroundGain;
                    rightGain = 0.0f;
                    break;

                case SpeakerPosition::BACK_RIGHT:
                case SpeakerPosition::SIDE_RIGHT:
                    // Right surround goes to right channel only
                    leftGain = 0.0f;
                    rightGain = surroundGain;
                    break;

                case SpeakerPosition::BACK_CENTER:
                    // Center surround splits to both at reduced level
                    leftGain = surroundGain * ITUCoefficients::kMinus3dB;
                    rightGain = surroundGain * ITUCoefficients::kMinus3dB;
                    break;

                default:
                    // Unknown speaker position - ignore channel
                    break;
            }

            // Apply gains
            for (int i = 0; i < numSamples; ++i)
            {
                leftData[i] += channelData[i] * leftGain;
                rightData[i] += channelData[i] * rightGain;
            }
        }

        // Normalize to prevent clipping (simple peak limiting)
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
    // Upmix from stereo with strategy selection
    //==========================================================================

    static void upmixFromStereo(const juce::AudioBuffer<float>& source,
                                 juce::AudioBuffer<float>& dest,
                                 int targetChannels,
                                 ChannelLayoutType targetLayout,
                                 UpmixStrategy strategy = UpmixStrategy::FrontOnly)
    {
        int numSamples = source.getNumSamples();

        // Guard: empty or invalid buffers
        if (source.getNumChannels() < 2 || numSamples <= 0 || targetChannels <= 0 || dest.getNumSamples() < numSamples)
        {
            if (dest.getNumSamples() > 0)
                dest.clear();
            return;
        }

        const float* leftData = source.getReadPointer(0);
        const float* rightData = source.getReadPointer(1);

        ChannelLayout layout(targetLayout != ChannelLayoutType::Unknown
                             ? targetLayout
                             : ChannelLayout::fromChannelCount(targetChannels).getType());

        // Use centralized ITU coefficients
        using ITUCoefficients::kMinus3dB;
        using ITUCoefficients::kMinus6dB;

        for (int ch = 0; ch < targetChannels && ch < layout.getNumChannels() && ch < dest.getNumChannels(); ++ch)
        {
            float* destData = dest.getWritePointer(ch);
            const ChannelInfo& info = layout.getChannelInfo(ch);

            float leftGain = 0.0f;
            float rightGain = 0.0f;

            switch (strategy)
            {
                case UpmixStrategy::FrontOnly:
                    // Only fill front L/R, silence everything else
                    if (info.speakerPosition == SpeakerPosition::FRONT_LEFT)
                    {
                        leftGain = 1.0f;
                        rightGain = 0.0f;
                    }
                    else if (info.speakerPosition == SpeakerPosition::FRONT_RIGHT)
                    {
                        leftGain = 0.0f;
                        rightGain = 1.0f;
                    }
                    // All other channels remain silent (0, 0)
                    break;

                case UpmixStrategy::PhantomCenter:
                    // L/R to front, derive center from L+R
                    if (info.speakerPosition == SpeakerPosition::FRONT_LEFT)
                    {
                        leftGain = 1.0f;
                        rightGain = 0.0f;
                    }
                    else if (info.speakerPosition == SpeakerPosition::FRONT_RIGHT)
                    {
                        leftGain = 0.0f;
                        rightGain = 1.0f;
                    }
                    else if (info.speakerPosition == SpeakerPosition::FRONT_CENTER)
                    {
                        // Center derived from L+R at -3dB each
                        leftGain = kMinus3dB;
                        rightGain = kMinus3dB;
                    }
                    // LFE and surrounds remain silent
                    break;

                case UpmixStrategy::FullSurround:
                    // L/R front, C from L+R, Ls/Rs derived from L/R
                    if (info.speakerPosition == SpeakerPosition::FRONT_LEFT)
                    {
                        leftGain = 1.0f;
                        rightGain = 0.0f;
                    }
                    else if (info.speakerPosition == SpeakerPosition::FRONT_RIGHT)
                    {
                        leftGain = 0.0f;
                        rightGain = 1.0f;
                    }
                    else if (info.speakerPosition == SpeakerPosition::FRONT_CENTER)
                    {
                        // Center derived from L+R at -3dB
                        leftGain = kMinus3dB;
                        rightGain = kMinus3dB;
                    }
                    else if (info.speakerPosition == SpeakerPosition::BACK_LEFT ||
                             info.speakerPosition == SpeakerPosition::SIDE_LEFT)
                    {
                        // Left surround derived from Left at -6dB
                        leftGain = kMinus6dB;
                        rightGain = 0.0f;
                    }
                    else if (info.speakerPosition == SpeakerPosition::BACK_RIGHT ||
                             info.speakerPosition == SpeakerPosition::SIDE_RIGHT)
                    {
                        // Right surround derived from Right at -6dB
                        leftGain = 0.0f;
                        rightGain = kMinus6dB;
                    }
                    // LFE remains silent
                    break;

                case UpmixStrategy::Duplicate:
                default:
                    // Pan-based distribution to all channels (original behavior)
                    leftGain = (1.0f - info.defaultPanPosition) * 0.5f;
                    rightGain = (1.0f + info.defaultPanPosition) * 0.5f;
                    break;
            }

            // Apply gains
            for (int i = 0; i < numSamples; ++i)
            {
                destData[i] = leftData[i] * leftGain + rightData[i] * rightGain;
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

        // Guard: empty or invalid buffers
        if (sourceChannels <= 0 || destChannels <= 0 || numSamples <= 0 || dest.getNumSamples() < numSamples)
        {
            if (dest.getNumSamples() > 0)
                dest.clear();
            return;
        }

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
