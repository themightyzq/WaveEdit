/*
  ==============================================================================

    AudioFileManager_Cues.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    WAV RIFF "cue " + LIST-adtl embedding for markers and regions. Split out of
    AudioFileManager.cpp to keep that file under the size cap (CLAUDE.md 7.5).

    RIFF is little-endian; all multi-byte fields are read with
    juce::ByteOrder::littleEndianInt / written with MemoryOutputStream (LE).
    Chunk surgery mirrors AudioFileManager::appendiXMLChunk: read the whole
    file, copy every chunk except a prior cue/adtl, append fresh chunks, fix the
    RIFF size, and swap the result onto the target atomically.

  ==============================================================================
*/

#include "AudioFileManager.h"
#include <limits>
#include <map>
#include <vector>

namespace
{
    //==============================================================================
    // Little-endian helpers

    /** Writes a fully-formed RIFF sub-chunk (id + LE size + payload + pad). */
    void writeRiffChunk(juce::MemoryOutputStream& out,
                        const char* fourCC,
                        const juce::MemoryBlock& payload)
    {
        out.write(fourCC, 4);
        out.writeInt(static_cast<int>(payload.getSize()));  // LE size
        out.write(payload.getData(), payload.getSize());

        if ((payload.getSize() % 2) != 0)
            out.writeByte(0);  // word-align odd-sized chunks
    }

    /** Appends an ASCII/Latin-1, null-terminated string. Non-ASCII -> '?'. */
    void writeAsciiZ(juce::MemoryOutputStream& out, const juce::String& s)
    {
        for (int i = 0; i < s.length(); ++i)
        {
            const juce::juce_wchar c = s[i];
            out.writeByte((c > 0 && c < 128) ? static_cast<char>(c) : '?');
        }
        out.writeByte(0);  // null terminator
    }

    /** Reads an ASCII string of at most maxLen bytes, stopping at the first NUL. */
    juce::String readAscii(const char* data, int maxLen)
    {
        int len = 0;
        while (len < maxLen && data[len] != 0)
            ++len;

        return juce::String::fromUTF8(data, len);
    }

    //==============================================================================
    // Combined cue-entry model (markers first, then regions; IDs 1..N)

    struct CueEntry
    {
        juce::uint32 id;
        juce::int64 position;
        bool isRegion;
        juce::int64 length;
        juce::String name;
    };

    /**
     * RIFF "cue " / "ltxt" fields are 32-bit (DWORD). A position or region
     * end beyond UINT32_MAX would silently truncate/wrap into a garbage
     * sample when cast to a 32-bit field on write. Reject those entries here
     * rather than embedding a corrupt cue point; the caller falls back to
     * the (int64-capable) JSON sidecar for anything dropped this way.
     */
    bool fitsIn32BitCueField(juce::int64 position, juce::int64 length)
    {
        return position >= 0
            && length >= 0
            && position <= static_cast<juce::int64>(std::numeric_limits<juce::uint32>::max())
            && (position + length) <= static_cast<juce::int64>(std::numeric_limits<juce::uint32>::max());
    }

    juce::Array<CueEntry> buildEntries(const WavCueData& data)
    {
        juce::Array<CueEntry> entries;
        juce::uint32 nextId = 1;

        for (const auto& m : data.markers)
        {
            if (!fitsIn32BitCueField(m.position, 0))
            {
                juce::Logger::writeToLog(
                    "AudioFileManager::buildEntries - marker '" + m.name
                    + "' position exceeds the 32-bit WAV cue-chunk range; skipped "
                      "(preserved in the JSON sidecar only)");
                continue;
            }
            entries.add({ nextId++, m.position, false, 0, m.name });
        }

        for (const auto& r : data.regions)
        {
            if (!fitsIn32BitCueField(r.start, r.length))
            {
                juce::Logger::writeToLog(
                    "AudioFileManager::buildEntries - region '" + r.name
                    + "' exceeds the 32-bit WAV cue-chunk range; skipped "
                      "(preserved in the JSON sidecar only)");
                continue;
            }
            entries.add({ nextId++, r.start, true, r.length, r.name });
        }

        return entries;
    }

    /** Builds the "cue " and LIST-adtl chunk block. Empty when no entries. */
    void buildCueChunks(const WavCueData& data, juce::MemoryOutputStream& out)
    {
        const auto entries = buildEntries(data);
        if (entries.isEmpty())
            return;

        // --- cue chunk: count + 24-byte cue points ---
        {
            juce::MemoryOutputStream cue;
            cue.writeInt(static_cast<int>(entries.size()));  // dwCuePoints

            for (const auto& e : entries)
            {
                cue.writeInt(static_cast<int>(e.id));                        // dwName
                cue.writeInt(static_cast<int>(e.position));                  // dwPosition
                cue.write("data", 4);                                        // fccChunk
                cue.writeInt(0);                                             // dwChunkStart
                cue.writeInt(0);                                             // dwBlockStart
                cue.writeInt(static_cast<int>(e.position));                  // dwSampleOffset
            }

            juce::MemoryBlock payload(cue.getData(), cue.getDataSize());
            writeRiffChunk(out, "cue ", payload);
        }

        // --- LIST-adtl: labl for every entry, ltxt for regions ---
        {
            juce::MemoryOutputStream adtl;
            adtl.write("adtl", 4);

            for (const auto& e : entries)
            {
                juce::MemoryOutputStream labl;
                labl.writeInt(static_cast<int>(e.id));  // dwName (cue id)
                writeAsciiZ(labl, e.name);
                {
                    juce::MemoryBlock lablBlock(labl.getData(), labl.getDataSize());
                    writeRiffChunk(adtl, "labl", lablBlock);
                }

                if (e.isRegion)
                {
                    juce::MemoryOutputStream ltxt;
                    ltxt.writeInt(static_cast<int>(e.id));       // dwName
                    ltxt.writeInt(static_cast<int>(e.length));   // dwSampleLength
                    ltxt.write("rgn ", 4);                       // dwPurposeID
                    ltxt.writeShort(0);                          // wCountry
                    ltxt.writeShort(0);                          // wLanguage
                    ltxt.writeShort(0);                          // wDialect
                    ltxt.writeShort(0);                          // wCodePage
                    // No inline text: the name is carried by the labl above.

                    juce::MemoryBlock ltxtBlock(ltxt.getData(), ltxt.getDataSize());
                    writeRiffChunk(adtl, "ltxt", ltxtBlock);
                }
            }

            juce::MemoryBlock adtlBlock(adtl.getData(), adtl.getDataSize());
            writeRiffChunk(out, "LIST", adtlBlock);
        }
    }
}  // namespace

//==============================================================================
bool AudioFileManager::writeCueChunks(const juce::File& file, const WavCueData& data)
{
    clearError();

    if (!file.existsAsFile())
    {
        setError("File does not exist: " + file.getFullPathName());
        return false;
    }

    juce::MemoryBlock fileData;
    {
        juce::FileInputStream inputStream(file);
        if (!inputStream.openedOk())
        {
            setError("Could not open file for reading: " + file.getFullPathName());
            return false;
        }
        inputStream.readIntoMemoryBlock(fileData);
    }

    if (fileData.getSize() < 12)
    {
        setError("File too small to be a valid WAV file");
        return false;
    }

    auto* dataPtr = static_cast<const char*>(fileData.getData());
    if (memcmp(dataPtr, "RIFF", 4) != 0 || memcmp(dataPtr + 8, "WAVE", 4) != 0)
    {
        setError("File is not a valid WAV/RIFF file");
        return false;
    }

    // Build the fresh cue + adtl block (may be empty).
    juce::MemoryOutputStream newChunks;
    buildCueChunks(data, newChunks);

    // Rebuild the file, copying every chunk except a prior cue/adtl. Preserves
    // bext, LIST-INFO, iXML, etc. Use uint64 arithmetic so a corrupt near-
    // UINT32_MAX size field cannot wrap the bounds check (mirrors M8).
    juce::MemoryOutputStream cleanFile;
    cleanFile.write("RIFF", 4);
    cleanFile.writeInt(0);  // placeholder RIFF size
    cleanFile.write("WAVE", 4);

    bool foundExisting = false;
    uint64_t offset = 12;
    const uint64_t fileSize = static_cast<uint64_t>(fileData.getSize());

    while (offset + 8 <= fileSize)
    {
        char chunkID[5] = { 0 };
        memcpy(chunkID, dataPtr + offset, 4);

        const uint64_t chunkSize = static_cast<uint64_t>(
            static_cast<juce::uint32>(juce::ByteOrder::littleEndianInt(dataPtr + offset + 4)));

        if (chunkSize > fileSize - offset - 8)
            break;  // corrupt chunk; stop parsing

        const bool isCue = memcmp(chunkID, "cue ", 4) == 0;
        bool isAdtl = false;
        if (memcmp(chunkID, "LIST", 4) == 0 && chunkSize >= 4)
            isAdtl = memcmp(dataPtr + offset + 8, "adtl", 4) == 0;

        if (isCue || isAdtl)
        {
            foundExisting = true;  // drop it; a fresh copy is appended below
        }
        else
        {
            cleanFile.write(dataPtr + offset, static_cast<size_t>(8 + chunkSize));
            if ((chunkSize % 2) != 0)
                cleanFile.writeByte(0);
        }

        offset += 8 + chunkSize;
        if ((chunkSize % 2) != 0)
            offset += 1;
    }

    // Nothing to add and nothing to strip: leave the file untouched.
    if (newChunks.getDataSize() == 0 && !foundExisting)
        return true;

    cleanFile.write(newChunks.getData(), newChunks.getDataSize());

    // Fix up the RIFF size (little-endian, independent of host byte order).
    auto* cleanData = static_cast<uint8_t*>(const_cast<void*>(cleanFile.getData()));
    const uint32_t riffSize = static_cast<uint32_t>(cleanFile.getDataSize()) - 8;
    cleanData[4] = static_cast<uint8_t>(riffSize & 0xff);
    cleanData[5] = static_cast<uint8_t>((riffSize >> 8) & 0xff);
    cleanData[6] = static_cast<uint8_t>((riffSize >> 16) & 0xff);
    cleanData[7] = static_cast<uint8_t>((riffSize >> 24) & 0xff);

    // Atomic replace via a sibling temp file (mirrors appendiXMLChunk / C10).
    {
        juce::TemporaryFile tmp(file, juce::TemporaryFile::useHiddenFile);
        {
            juce::FileOutputStream outputStream(tmp.getFile());
            if (!outputStream.openedOk())
            {
                setError("Could not open temp file for writing: " + file.getFullPathName());
                return false;
            }

            if (!outputStream.write(cleanFile.getData(), cleanFile.getDataSize()))
            {
                setError("Failed to write cue data to temp file: " + file.getFullPathName());
                return false;
            }
            outputStream.flush();
        }

        if (!tmp.overwriteTargetFileWithTemporary())
        {
            setError("Could not replace file with updated temp file: " + file.getFullPathName());
            return false;
        }
    }

    DBG("Cue/adtl chunks written (" + juce::String(data.markers.size()) + " markers, "
        + juce::String(data.regions.size()) + " regions)");
    return true;
}

//==============================================================================
bool AudioFileManager::readCueChunks(const juce::File& file, WavCueData& outData)
{
    clearError();

    outData.markers.clear();
    outData.regions.clear();

    if (!file.existsAsFile())
    {
        setError("File does not exist: " + file.getFullPathName());
        return false;
    }

    juce::MemoryBlock fileData;
    {
        juce::FileInputStream inputStream(file);
        if (!inputStream.openedOk())
        {
            setError("Could not open file for reading: " + file.getFullPathName());
            return false;
        }
        inputStream.readIntoMemoryBlock(fileData);
    }

    if (fileData.getSize() < 12)
        return false;

    auto* d = static_cast<const char*>(fileData.getData());
    if (memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0)
        return false;

    // Accumulate by cue id, preserving first-seen order.
    std::map<juce::uint32, juce::int64> cuePos;
    std::vector<juce::uint32> cueOrder;
    std::map<juce::uint32, juce::String> labels;
    std::map<juce::uint32, juce::int64> regionLen;
    // Fallback name source: a foreign WAV may store a region's name as
    // inline text inside "ltxt" instead of a separate "labl" (WaveEdit's own
    // writer always emits "labl", so this is only exercised on import).
    std::map<juce::uint32, juce::String> ltxtInlineText;

    uint64_t offset = 12;
    const uint64_t fileSize = static_cast<uint64_t>(fileData.getSize());

    while (offset + 8 <= fileSize)
    {
        char chunkID[5] = { 0 };
        memcpy(chunkID, d + offset, 4);

        const uint64_t chunkSize = static_cast<uint64_t>(
            static_cast<juce::uint32>(juce::ByteOrder::littleEndianInt(d + offset + 4)));

        if (chunkSize > fileSize - offset - 8)
            break;

        const char* body = d + offset + 8;

        if (memcmp(chunkID, "cue ", 4) == 0 && chunkSize >= 4)
        {
            const juce::uint32 n =
                static_cast<juce::uint32>(juce::ByteOrder::littleEndianInt(body));
            uint64_t p = 4;

            for (juce::uint32 i = 0; i < n && p + 24 <= chunkSize; ++i)
            {
                const juce::uint32 cid =
                    static_cast<juce::uint32>(juce::ByteOrder::littleEndianInt(body + p));
                const juce::uint32 dwChunkStart =
                    static_cast<juce::uint32>(juce::ByteOrder::littleEndianInt(body + p + 12));
                const juce::uint32 sampleOffset =
                    static_cast<juce::uint32>(juce::ByteOrder::littleEndianInt(body + p + 20));

                // dwSampleOffset is only a direct, file-relative sample position
                // when fccChunk names the single "data" chunk and dwChunkStart
                // is 0 (per the WAV cue-chunk spec). WaveEdit's own writer
                // always emits exactly that; a foreign file whose cue points at
                // a different/offset chunk cannot be interpreted as an absolute
                // sample without also resolving that chunk, which this reader
                // does not do. Skip rather than silently misplacing the cue.
                const bool isDirectDataOffset =
                    memcmp(body + p + 8, "data", 4) == 0 && dwChunkStart == 0;

                if (isDirectDataOffset)
                {
                    if (cuePos.find(cid) == cuePos.end())
                        cueOrder.push_back(cid);
                    cuePos[cid] = static_cast<juce::int64>(sampleOffset);
                }
                else
                {
                    juce::Logger::writeToLog(
                        "AudioFileManager::readCueChunks - cue id " + juce::String(cid)
                        + " does not reference the data chunk at offset 0; skipped "
                          "(non-WaveEdit cue layout)");
                }

                p += 24;
            }
        }
        else if (memcmp(chunkID, "LIST", 4) == 0 && chunkSize >= 4
                 && memcmp(body, "adtl", 4) == 0)
        {
            uint64_t p = 4;
            while (p + 8 <= chunkSize)
            {
                char subID[5] = { 0 };
                memcpy(subID, body + p, 4);

                const uint64_t subSize = static_cast<uint64_t>(
                    static_cast<juce::uint32>(juce::ByteOrder::littleEndianInt(body + p + 4)));

                if (subSize > chunkSize - p - 8)
                    break;

                const char* sub = body + p + 8;

                if (memcmp(subID, "labl", 4) == 0 && subSize >= 4)
                {
                    const juce::uint32 cid =
                        static_cast<juce::uint32>(juce::ByteOrder::littleEndianInt(sub));
                    labels[cid] = readAscii(sub + 4, static_cast<int>(subSize) - 4);
                }
                else if (memcmp(subID, "ltxt", 4) == 0 && subSize >= 20)
                {
                    const juce::uint32 cid =
                        static_cast<juce::uint32>(juce::ByteOrder::littleEndianInt(sub));
                    const juce::uint32 sampleLen =
                        static_cast<juce::uint32>(juce::ByteOrder::littleEndianInt(sub + 4));
                    char purpose[5] = { 0 };
                    memcpy(purpose, sub + 8, 4);

                    if (memcmp(purpose, "rgn ", 4) == 0 && sampleLen > 0)
                        regionLen[cid] = static_cast<juce::int64>(sampleLen);

                    // ltxt's own fixed header is 20 bytes (dwName, dwSampleLength,
                    // dwPurposeID, wCountry, wLanguage, wDialect, wCodePage);
                    // anything past that is the inline label text some tools use
                    // instead of a "labl" chunk.
                    if (subSize > 20)
                    {
                        const juce::String inlineText =
                            readAscii(sub + 20, static_cast<int>(subSize) - 20);
                        if (inlineText.isNotEmpty())
                            ltxtInlineText[cid] = inlineText;
                    }
                }

                p += 8 + subSize;
                if ((subSize % 2) != 0)
                    p += 1;
            }
        }

        offset += 8 + chunkSize;
        if ((chunkSize % 2) != 0)
            offset += 1;
    }

    // Combine: a cue with a matching nonzero-length 'rgn ' ltxt is a region.
    for (const juce::uint32 cid : cueOrder)
    {
        const juce::int64 pos = cuePos[cid];
        // Prefer the "labl" chunk; fall back to ltxt's inline text (some
        // foreign encoders store the name there instead) so an imported
        // region/marker isn't unnecessarily unnamed.
        const juce::String name = labels.count(cid)
                                 ? labels[cid]
                                 : (ltxtInlineText.count(cid) ? ltxtInlineText[cid] : juce::String());

        const auto rl = regionLen.find(cid);
        if (rl != regionLen.end())
        {
            WavCueRegion r;
            r.name = name;
            r.start = pos;
            r.length = rl->second;
            outData.regions.add(r);
        }
        else
        {
            WavCueMarker m;
            m.name = name;
            m.position = pos;
            outData.markers.add(m);
        }
    }

    return !outData.isEmpty();
}
