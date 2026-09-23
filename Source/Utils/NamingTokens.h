/*
  ==============================================================================

    NamingTokens.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace waveedit
{

/**
 * Shared filename-template substitution mechanism for WaveEdit's two
 * independent naming-template systems (region export, batch processing).
 * Neither system knows the audio format of what it is naming; this class
 * lets both share one substitution mechanism and one set of audio-format
 * tokens ({samplerate}, {bitdepth}, {channels}) instead of maintaining
 * duplicate token lists and duplicate .replace() chains.
 *
 * This is a pure, stateless helper: it does not know or care which tokens
 * a given call site supports, so each call site still owns its own list of
 * literal token spellings.
 */
class NamingTokens
{
public:
    /** A single template placeholder and the text it expands to. */
    struct Token
    {
        juce::String token;   ///< Exact literal including braces, e.g. "{basename}" or "{index:03}"
        juce::String value;   ///< Replacement text
    };

    /**
     * Substitutes every token present in @p tokens against @p templateStr, in
     * list order. Any {placeholder} in @p templateStr that does not match one
     * of the given tokens is left untouched verbatim -- this is how an
     * unknown/typo'd token, or a token this call site did not supply, survives
     * unchanged in the output.
     *
     * @param templateStr The filename template containing {placeholder} tokens.
     * @param tokens The tokens to substitute, in the order they should be applied.
     * @return The template with every matching token replaced.
     */
    static juce::String substitute(const juce::String& templateStr,
                                    const std::vector<Token>& tokens);

    /**
     * Appends the three audio-format tokens common to every naming system to
     * @p tokens: {samplerate} (integer Hz), {bitdepth} (integer bits), and
     * {channels} (integer channel count).
     *
     * Pass 0 for a value the call site cannot determine (e.g. an unreadable
     * file) -- it renders as the literal "0" rather than leaving the token
     * unexpanded, since these are genuinely known tokens, just with an
     * unknown value in that edge case.
     *
     * @param tokens Token list to append to.
     * @param sampleRate Sample rate in Hz, or 0 if unknown.
     * @param bitDepth Bit depth in bits, or 0 if unknown.
     * @param numChannels Channel count, or 0 if unknown.
     */
    static void addAudioFormatTokens(std::vector<Token>& tokens,
                                      int sampleRate,
                                      int bitDepth,
                                      int numChannels);
};

} // namespace waveedit
