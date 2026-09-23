/*
  ==============================================================================

    NamingTokens.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "NamingTokens.h"

namespace waveedit
{

juce::String NamingTokens::substitute(const juce::String& templateStr,
                                       const std::vector<Token>& tokens)
{
    juce::String result = templateStr;

    for (const auto& t : tokens)
        result = result.replace(t.token, t.value);

    return result;
}

void NamingTokens::addAudioFormatTokens(std::vector<Token>& tokens,
                                         int sampleRate,
                                         int bitDepth,
                                         int numChannels)
{
    tokens.push_back({ "{samplerate}", juce::String(sampleRate) });
    tokens.push_back({ "{bitdepth}", juce::String(bitDepth) });
    tokens.push_back({ "{channels}", juce::String(numChannels) });
}

} // namespace waveedit
