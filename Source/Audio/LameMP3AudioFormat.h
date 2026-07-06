/*
  ==============================================================================

    LameMP3AudioFormat.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

// The entire translation unit compiles to nothing when LAME is not available
// (see CMake: WAVEEDIT_HAVE_LAME is defined only when the header + library were
// found). Callers must guard use of this class with the same macro. When LAME
// is absent MP3 export cleanly reports unavailable at runtime rather than
// failing the build.
#if WAVEEDIT_HAVE_LAME

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

namespace waveedit
{

/**
 * A REAL, write-capable MP3 AudioFormat backed by libmp3lame.
 *
 * JUCE's own juce::MP3AudioFormat is DECODE-ONLY (its createWriterFor asserts
 * and returns nullptr), so this class provides the missing encoder. It is a
 * WRITER only: createReaderFor() returns nullptr so that the AudioFormatManager
 * falls through to juce::MP3AudioFormat for DECODING .mp3 files.
 *
 * Registration order matters: register this format BEFORE juce::MP3AudioFormat
 * so findFormatForFileExtension("mp3") resolves to the encoder for writing,
 * while reading still falls through to JUCE's decoder.
 *
 * LGPL note: libmp3lame is LGPL-2.1 and is DYNAMICALLY linked so the library
 * can be swapped by the end user. Source: https://lame.sourceforge.io/
 */
class LameMP3AudioFormat : public juce::AudioFormat
{
public:
    LameMP3AudioFormat();
    ~LameMP3AudioFormat() override;

    //==============================================================================
    // AudioFormat interface
    juce::Array<int> getPossibleSampleRates() override;
    juce::Array<int> getPossibleBitDepths() override;
    bool canDoStereo() override;
    bool canDoMono() override;
    bool isCompressed() override;
    juce::StringArray getQualityOptions() override;

    /** Decode is delegated to juce::MP3AudioFormat, so this always returns
        nullptr (and deletes the stream when asked to). */
    juce::AudioFormatReader* createReaderFor(juce::InputStream* sourceStream,
                                             bool deleteStreamIfOpeningFails) override;

    std::unique_ptr<juce::AudioFormatWriter>
    createWriterFor(std::unique_ptr<juce::OutputStream>& streamToWriteTo,
                    const juce::AudioFormatWriterOptions& options) override;

    // Keep the deprecated overloads reachable.
    using juce::AudioFormat::createWriterFor;

    //==============================================================================
    // Sample-rate policy helpers (LAME supports MPEG-1/2/2.5 rates only; the
    // max is 48 kHz -- 88.2/96/176.4/192 kHz are rejected).

    /** True if @p sampleRate is one LAME can encode. */
    static bool isSampleRateSupported(double sampleRate);

    /** Human-readable, ASCII list of supported MP3 sample rates (for errors). */
    static juce::String getSupportedSampleRatesString();

    /** Maps WaveEdit's 0-10 quality slider to a CBR bitrate in kbps. */
    static int bitrateForQuality(int qualityOptionIndex);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LameMP3AudioFormat)
};

} // namespace waveedit

#endif // WAVEEDIT_HAVE_LAME
