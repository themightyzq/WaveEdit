/*
  ==============================================================================

    ChannelMask.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Channel-focus bitmask helpers (the WaveformDisplay focus mask: -1 = all
    channels, otherwise bit N = channel N). Pure, header-only, any thread.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace ChannelMask
{
    /** Every channel. */
    constexpr int kAll = -1;

    /** True if channel @p channel is in @p mask. */
    inline bool includes(int mask, int channel) noexcept
    {
        return mask == kAll || (channel >= 0 && channel < 31 && (mask & (1 << channel)) != 0);
    }

    /**
     * The mask restricted to channels that exist, or kAll when it selects
     * every channel (or none, which is not a usable focus) -- so a result
     * other than kAll always means "some, but not all, channels".
     */
    inline int normalise(int mask, int numChannels) noexcept
    {
        if (mask == kAll || numChannels <= 0 || numChannels > 31)
            return kAll;

        const int existing = (1 << numChannels) - 1;
        const int restricted = mask & existing;
        return (restricted == 0 || restricted == existing) ? kAll : restricted;
    }

    /** A copy of just the channels of @p source that are in @p mask. */
    inline juce::AudioBuffer<float> selectChannels(const juce::AudioBuffer<float>& source, int mask)
    {
        int count = 0;
        for (int ch = 0; ch < source.getNumChannels(); ++ch)
            if (includes(mask, ch))
                ++count;

        juce::AudioBuffer<float> result(count, source.getNumSamples());
        int dest = 0;
        for (int ch = 0; ch < source.getNumChannels(); ++ch)
            if (includes(mask, ch))
                result.copyFrom(dest++, 0, source, ch, 0, source.getNumSamples());
        return result;
    }
}
