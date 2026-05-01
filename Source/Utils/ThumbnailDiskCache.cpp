/*
  ==============================================================================

    ThumbnailDiskCache.cpp
    Copyright (C) 2025 ZQ SFX

  ==============================================================================
*/

#include "ThumbnailDiskCache.h"
#include "Settings.h"

namespace ThumbnailDiskCache
{

juce::File getCacheDirectory()
{
    auto dir = Settings::getInstance().getSettingsDirectory().getChildFile("thumbnail_cache");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

juce::File getCacheEntryFor(const juce::File& audioFile)
{
    if (! audioFile.existsAsFile())
        return {};

    // Stable cache key: 64-bit hash of the absolute path collapses
    // long paths to a fixed-length filename prefix. Size + mtime are
    // appended unhashed so a stale cache is self-evicting (the
    // filename simply doesn't match anymore on a content change).
    const auto pathHash = juce::String::toHexString(
        static_cast<juce::int64>(audioFile.getFullPathName().hash()));

    const auto size  = audioFile.getSize();
    const auto mtime = audioFile.getLastModificationTime().toMilliseconds();
    const auto name  = pathHash
                          + "_" + juce::String(size)
                          + "_" + juce::String(mtime)
                          + ".thumb";
    return getCacheDirectory().getChildFile(name);
}

bool tryLoad(const juce::File& audioFile, juce::AudioThumbnail& thumb)
{
    auto entry = getCacheEntryFor(audioFile);
    if (entry == juce::File() || ! entry.existsAsFile())
        return false;

    juce::FileInputStream in(entry);
    if (! in.openedOk())
        return false;

    // AudioThumbnail::loadFrom returns true on success; the resulting
    // thumbnail is fully populated (no audio decode required).
    return thumb.loadFrom(in);
}

bool save(const juce::File& audioFile, const juce::AudioThumbnail& thumb)
{
    auto entry = getCacheEntryFor(audioFile);
    if (entry == juce::File())
        return false;

    // Don't bother caching incomplete thumbnails.
    if (! thumb.isFullyLoaded())
        return false;

    {
        juce::TemporaryFile tmp(entry, juce::TemporaryFile::useHiddenFile);
        {
            juce::FileOutputStream out(tmp.getFile());
            if (! out.openedOk())
                return false;
            thumb.saveTo(out);
        }
        if (! tmp.overwriteTargetFileWithTemporary())
            return false;
    }

    // Eviction: if we're over the cap, delete the oldest entries by
    // last-modification time (LRU-ish since juce updates mtime on
    // file write).
    juce::Array<juce::File> all;
    getCacheDirectory().findChildFiles(all, juce::File::findFiles, false, "*.thumb");
    if (all.size() > kMaxEntries)
    {
        struct ByMTimeAsc
        {
            static int compareElements(const juce::File& a, const juce::File& b)
            {
                const auto ta = a.getLastModificationTime();
                const auto tb = b.getLastModificationTime();
                if (ta < tb) return -1;
                if (ta > tb) return 1;
                return 0;
            }
        };
        ByMTimeAsc cmp;
        all.sort(cmp, false);

        const int toDelete = all.size() - kMaxEntries;
        for (int i = 0; i < toDelete; ++i)
            all.getReference(i).deleteFile();
    }

    return true;
}

}  // namespace ThumbnailDiskCache
