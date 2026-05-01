/*
  ==============================================================================

    ThumbnailDiskCache.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Persistent on-disk cache for `juce::AudioThumbnail` data. The first
    time the user opens a file, the thumbnail is generated normally;
    when generation finishes, the binary thumbnail is dumped to
    `<settings>/thumbnail_cache/<hash>_<size>_<mtime>.thumb`. On
    subsequent opens of the same file, the cache is loaded directly,
    skipping audio decoding entirely.

    Cache invalidation is automatic: the file size + mtime are encoded
    in the cache filename, so any change to the source audio produces a
    different key and a stale entry is simply ignored. Eviction caps
    the total entry count to a fixed maximum (oldest entries by mtime
    are deleted on the next write).

    Lifetime: pure functions. No global state held outside the actual
    cache directory.

  ==============================================================================
*/

#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

namespace ThumbnailDiskCache
{
    /** Cache directory under the per-user settings root. */
    juce::File getCacheDirectory();

    /** Filename for a given audio file's cached thumbnail. The
        filename embeds the source's size + mtime so a stale cache
        is automatically a different file. Returns juce::File() if
        @p audioFile is not on disk. */
    juce::File getCacheEntryFor(const juce::File& audioFile);

    /** Load the cached thumbnail for @p audioFile into @p thumb.
        Returns true if the cache hit and produced a fully-loaded
        thumbnail; false otherwise (caller should generate fresh). */
    bool tryLoad(const juce::File& audioFile, juce::AudioThumbnail& thumb);

    /** Save @p thumb to disk for later retrieval. Cleans up older
        entries if the cache exceeds the configured cap. Returns
        true on success. */
    bool save(const juce::File& audioFile, const juce::AudioThumbnail& thumb);

    /** Maximum number of cache entries kept on disk. Older entries
        (by mtime) are evicted on save. */
    constexpr int kMaxEntries = 100;
}
