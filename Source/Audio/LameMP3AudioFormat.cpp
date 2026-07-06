/*
  ==============================================================================

    LameMP3AudioFormat.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "LameMP3AudioFormat.h"

// Entire implementation is a no-op when LAME is unavailable (see header).
#if WAVEEDIT_HAVE_LAME

#include <lame/lame.h>

namespace waveedit
{

namespace
{
const char* const kLameMp3FormatName = "MP3 file (LAME)";

// LAME-encodable sample rates: MPEG-1 (32/44.1/48k), MPEG-2 (16/22.05/24k),
// MPEG-2.5 (8/11.025/12k). 88.2/96/176.4/192 kHz are NOT supported.
const int kSupportedRates[] = { 8000, 11025, 12000, 16000, 22050,
                                24000, 32000, 44100, 48000 };
} // namespace

//==============================================================================
// Writer: streams float PCM through libmp3lame into the OutputStream.
//==============================================================================
class LameMP3Writer : public juce::AudioFormatWriter
{
public:
    /** Takes ownership of @p out only on successful construction. Check isOk()
        before use; on failure the caller keeps responsibility for the stream. */
    LameMP3Writer(std::unique_ptr<juce::OutputStream> out,
                  double sampleRateToUse,
                  unsigned int numChannelsToUse,
                  int bitrateKbps)
        : juce::AudioFormatWriter(out.get(), kLameMp3FormatName, sampleRateToUse,
                                  numChannelsToUse, 16)
    {
        // Base class now owns the stream; release our unique_ptr so it is not
        // double-freed. (On init failure the base dtor frees it.)
        out.release();

        // We feed LAME normalized float PCM directly, so writeFromFloatArrays()
        // hands us float data reinterpreted as int** (see JUCE AudioFormatWriter).
        usesFloatingPointData = true;

        m_lame = lame_init();
        if (m_lame == nullptr)
            return;

        lame_set_in_samplerate(m_lame, (int) sampleRateToUse);
        lame_set_out_samplerate(m_lame, (int) sampleRateToUse); // no internal resample
        lame_set_num_channels(m_lame, (int) numChannelsToUse);
        lame_set_mode(m_lame, numChannelsToUse == 1 ? MONO : JOINT_STEREO);

        // Constant bitrate: predictable size + matches the UI's bitrate labels.
        lame_set_VBR(m_lame, vbr_off);
        lame_set_brate(m_lame, bitrateKbps);

        // Algorithm quality: 0 = best/slowest, 9 = worst/fastest. 2 is the
        // standard high-quality setting.
        lame_set_quality(m_lame, 2);

        // Skip the Xing/Info VBR header frame. Writing it correctly requires
        // seeking back to the start of the stream after encoding to replace a
        // placeholder frame (lame_get_lametag_frame); an arbitrary OutputStream
        // is not reliably seekable. Omitting it yields a valid, decodable CBR
        // MP3 without needing a rewind. (Cost: players can't use the VBR TOC
        // for seeking -- irrelevant for CBR.)
        lame_set_bWriteVbrTag(m_lame, 0);

        if (lame_init_params(m_lame) < 0)
            return;

        m_initOk = true;
    }

    ~LameMP3Writer() override
    {
        if (m_lame != nullptr)
        {
            if (m_initOk && output != nullptr)
            {
                // Flush LAME's internal buffers -- returns the final frames.
                unsigned char flushBuffer[7200];
                const int bytes = lame_encode_flush(m_lame, flushBuffer,
                                                    (int) sizeof(flushBuffer));
                if (bytes > 0)
                    output->write(flushBuffer, (size_t) bytes);
            }

            lame_close(m_lame);
            m_lame = nullptr;
        }
    }

    /** True when lame_init + lame_init_params both succeeded. */
    bool isOk() const noexcept { return m_initOk; }

    bool write(const int** samplesToWrite, int numSamples) override
    {
        if (! m_initOk || m_lame == nullptr || output == nullptr)
            return false;

        if (numSamples <= 0)
            return true;

        // usesFloatingPointData == true, so these are actually float channels.
        const float* const* floatChannels =
            reinterpret_cast<const float* const*>(samplesToWrite);

        const float* left = floatChannels[0];
        const float* right = (numChannels >= 2) ? floatChannels[1] : floatChannels[0];

        // LAME worst case: 1.25 * samples + 7200 bytes.
        const size_t needed = (size_t) numSamples + (size_t) numSamples / 4 + 7200;
        if (m_mp3Buffer.size() < needed)
            m_mp3Buffer.resize(needed);

        const int bytes = lame_encode_buffer_ieee_float(
            m_lame, left, right, numSamples,
            m_mp3Buffer.data(), (int) m_mp3Buffer.size());

        if (bytes < 0)
        {
            juce::Logger::writeToLog("LameMP3Writer::write - lame_encode_buffer_ieee_float failed: "
                                     + juce::String(bytes));
            return false;
        }

        if (bytes > 0)
            return output->write(m_mp3Buffer.data(), (size_t) bytes);

        return true;
    }

private:
    lame_global_flags* m_lame = nullptr;
    bool m_initOk = false;
    std::vector<unsigned char> m_mp3Buffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LameMP3Writer)
};

//==============================================================================
// Format
//==============================================================================
LameMP3AudioFormat::LameMP3AudioFormat()
    : juce::AudioFormat(kLameMp3FormatName, ".mp3")
{
}

LameMP3AudioFormat::~LameMP3AudioFormat() = default;

juce::Array<int> LameMP3AudioFormat::getPossibleSampleRates()
{
    juce::Array<int> rates;
    for (int r : kSupportedRates)
        rates.add(r);
    return rates;
}

juce::Array<int> LameMP3AudioFormat::getPossibleBitDepths()
{
    // MP3 is not PCM; report 16 as the nominal source depth.
    return { 16 };
}

bool LameMP3AudioFormat::canDoStereo() { return true; }
bool LameMP3AudioFormat::canDoMono() { return true; }
bool LameMP3AudioFormat::isCompressed() { return true; }

juce::StringArray LameMP3AudioFormat::getQualityOptions()
{
    juce::StringArray options;
    for (int q = 0; q <= 10; ++q)
        options.add(juce::String(bitrateForQuality(q)) + " kbps");
    return options;
}

juce::AudioFormatReader* LameMP3AudioFormat::createReaderFor(juce::InputStream* sourceStream,
                                                             bool deleteStreamIfOpeningFails)
{
    // Encoder only -- decoding is handled by juce::MP3AudioFormat, which must be
    // registered alongside this format. Honour the delete contract.
    if (deleteStreamIfOpeningFails)
        delete sourceStream;

    return nullptr;
}

std::unique_ptr<juce::AudioFormatWriter>
LameMP3AudioFormat::createWriterFor(std::unique_ptr<juce::OutputStream>& streamToWriteTo,
                                    const juce::AudioFormatWriterOptions& options)
{
    if (streamToWriteTo == nullptr)
        return nullptr;

    const double sr = options.getSampleRate();
    const int channels = options.getNumChannels();

    // Validate BEFORE taking the stream so, for the common rejection paths, the
    // caller keeps its stream (per the createWriterFor contract). MP3 is
    // mono/stereo only and limited to LAME's sample-rate set.
    if (channels < 1 || channels > 2)
        return nullptr;

    if (! isSampleRateSupported(sr))
        return nullptr;

    const int bitrate = bitrateForQuality(options.getQualityOptionIndex());

    auto writer = std::make_unique<LameMP3Writer>(std::move(streamToWriteTo), sr,
                                                  (unsigned int) channels, bitrate);

    if (! writer->isOk())
    {
        // LAME init failed (e.g. out-of-memory) after we validated the params.
        // The stream was consumed by the writer and is freed as it destructs.
        // This is an extreme edge; the only caller does not reuse the stream.
        return nullptr;
    }

    return writer;
}

//==============================================================================
bool LameMP3AudioFormat::isSampleRateSupported(double sampleRate)
{
    for (int r : kSupportedRates)
        if (std::abs(sampleRate - (double) r) < 1.0)
            return true;

    return false;
}

juce::String LameMP3AudioFormat::getSupportedSampleRatesString()
{
    juce::StringArray parts;
    for (int r : kSupportedRates)
        parts.add(juce::String(r));

    return parts.joinIntoString(", ") + " Hz";
}

int LameMP3AudioFormat::bitrateForQuality(int qualityOptionIndex)
{
    // 0-10 slider -> CBR kbps. Thresholds match SaveAsOptionsPanel's MP3 labels
    // (<=2 ~96, <=4 ~128, <=6 ~192, <=8 ~256, else ~320).
    static const int table[] = { 96, 96, 96, 128, 128, 192, 192, 256, 256, 320, 320 };
    const int q = juce::jlimit(0, 10, qualityOptionIndex);
    return table[q];
}

} // namespace waveedit

#endif // WAVEEDIT_HAVE_LAME
