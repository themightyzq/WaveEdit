/*
  ==============================================================================

    AutoSaveRecovery.cpp
    Copyright (C) 2025 ZQ SFX

  ==============================================================================
*/

#include "AutoSaveRecovery.h"
#include "Settings.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace AutoSaveRecovery
{

juce::File getAutoSaveDirectory()
{
    return Settings::getInstance().getSettingsDirectory().getChildFile("autosave");
}

juce::String autoSavePrefixFor(const juce::File& originalFile)
{
    // Hash the ABSOLUTE path so same-stem files in different directories get
    // distinct prefixes (M4). Stem is kept for human-readability; the hash
    // is what actually disambiguates. The trailing "_" separates the prefix
    // from the timestamp the writer appends.
    const auto pathHash = juce::String::toHexString(
        static_cast<juce::int64>(originalFile.getFullPathName().hash()));

    return "autosave_" + originalFile.getFileNameWithoutExtension()
                       + "_" + pathHash + "_";
}

// Does an auto-save file belong to originalFile?
//
// Two on-disk layouts are recognised so that auto-saves written by older
// builds remain recoverable after the M4 path-hash change:
//   legacy : autosave_<stem>_<YYYYMMDD>_<HHMMSS>.wav         (2 trailing tokens)
//   current: autosave_<stem>_<pathHash>_<YYYYMMDD>_<HHMMSS>.wav (3 trailing tokens)
// We strip the known "autosave_<stem>_" prefix first, so a stem containing
// underscores is handled correctly (the M5 trap). For current-format files
// the embedded path hash must match, which disambiguates same-stem files in
// different directories; legacy files have no hash and are matched by stem
// only (best effort, preserving pre-M4 recovery behaviour).
static bool autoSaveMatchesOriginal(const juce::String& fileName,
                                    const juce::File& originalFile)
{
    const auto stemPrefix = "autosave_" + originalFile.getFileNameWithoutExtension() + "_";
    if (! fileName.startsWith(stemPrefix))
        return false;

    auto remainder = fileName.substring(stemPrefix.length());
    if (remainder.endsWithIgnoreCase(".wav"))
        remainder = remainder.dropLastCharacters(4);

    juce::StringArray tokens;
    tokens.addTokens(remainder, "_", "");

    if (tokens.size() == 2)
        return true;  // legacy: <date>_<time> — disambiguated by stem only

    if (tokens.size() == 3)
    {
        const auto expectedHash = juce::String::toHexString(
            static_cast<juce::int64>(originalFile.getFullPathName().hash()));
        return tokens[0] == expectedHash;  // current: <hash>_<date>_<time>
    }

    return false;
}

juce::Array<juce::File> findOrphanedAutoSaves(const juce::File& autoSaveDir,
                                               const juce::File& originalFile)
{
    juce::Array<juce::File> result;
    if (! autoSaveDir.isDirectory())
        return result;

    const auto originalMTime = originalFile.getLastModificationTime();

    juce::Array<juce::File> candidates;
    autoSaveDir.findChildFiles(candidates, juce::File::findFiles, false, "autosave_*.wav");

    for (const auto& f : candidates)
    {
        if (! autoSaveMatchesOriginal(f.getFileName(), originalFile))
            continue;
        if (f.getLastModificationTime() <= originalMTime)
            continue;  // stale: the original was saved after this auto-save
        result.add(f);
    }

    // Newest first.
    struct ByMTimeDesc
    {
        static int compareElements(const juce::File& a, const juce::File& b)
        {
            const auto ta = a.getLastModificationTime();
            const auto tb = b.getLastModificationTime();
            if (ta > tb) return -1;
            if (ta < tb) return 1;
            return 0;
        }
    };
    ByMTimeDesc cmp;
    result.sort(cmp, false);
    return result;
}

juce::Array<juce::File> findOrphanedAutoSaves(const juce::File& originalFile)
{
    return findOrphanedAutoSaves(getAutoSaveDirectory(), originalFile);
}

void deleteAutoSavesFor(const juce::File& autoSaveDir,
                        const juce::File& originalFile)
{
    if (! autoSaveDir.isDirectory())
        return;

    juce::Array<juce::File> candidates;
    autoSaveDir.findChildFiles(candidates, juce::File::findFiles, false, "autosave_*.wav");
    for (const auto& f : candidates)
        if (autoSaveMatchesOriginal(f.getFileName(), originalFile))
            f.deleteFile();
}

void deleteAutoSavesFor(const juce::File& originalFile)
{
    deleteAutoSavesFor(getAutoSaveDirectory(), originalFile);
}

bool writeAutoSave(const juce::File& target,
                   const juce::AudioBuffer<float>& buffer,
                   double sampleRate,
                   juce::String& error)
{
    std::unique_ptr<juce::OutputStream> outputStream = target.createOutputStream();
    if (outputStream == nullptr)
    {
        error = "Could not create output stream";
        return false;
    }

    // JUCE 8 API: takes the unique_ptr by reference and assumes ownership of
    // the stream only on success. 32-bit IEEE float (see
    // AudioFileManager::saveAsWav, H1, for why the format must be explicit).
    juce::WavAudioFormat wavFormat;
    auto writer = wavFormat.createWriterFor(
        outputStream,
        juce::AudioFormatWriterOptions()
            .withSampleRate(sampleRate)
            .withNumChannels(buffer.getNumChannels())
            .withBitsPerSample(32)
            .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));

    if (writer == nullptr)
    {
        error = "Could not create audio writer";
        return false;
    }

    const bool written = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    writer.reset();  // flush and close

    if (!written)
        error = "Write operation failed";
    return written;
}

int loadFirstReadable(const juce::Array<juce::File>& newestFirst,
                      const AutoSaveLoader& loader,
                      juce::AudioBuffer<float>& recovered)
{
    for (int i = 0; i < newestFirst.size(); ++i)
    {
        juce::AudioBuffer<float> candidate;
        if (loader && loader(newestFirst.getReference(i), candidate)
            && candidate.getNumSamples() > 0 && candidate.getNumChannels() > 0)
        {
            recovered = std::move(candidate);
            return i;
        }
    }
    return -1;
}

}  // namespace AutoSaveRecovery
