/*
  ==============================================================================

    BroadcastChunkPreserver.cpp
    Part of WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

// PROVENANCE: this file is a copy of
//   ZQSFX_WaveFoundry/AudioSplitterJUCE/Source/Utils/BroadcastChunkPreserver.cpp
// JUCE/CLAUDE.md section 1 forbids referencing a sibling project from a
// project's build files, so the implementation is duplicated here rather than
// shared. There is no shared module yet -- until one exists, keep this copy
// and the AudioSplitterJUCE original in sync by hand when either changes.

#include "BroadcastChunkPreserver.h"

namespace BroadcastChunkPreserver
{

using namespace juce;

namespace
{
    constexpr uint32 kRf64DataSentinel = 0xFFFFFFFFu; // RF64: real data size lives in ds64
    constexpr int64  kMaxRiffSize      = 0xFFFFFFFFll; // standard RIFF size field is uint32

    bool matchesId (const char* fourCC, const String& id)
    {
        const char* want = id.toRawUTF8();
        return strlen (want) >= 4 && memcmp (fourCC, want, 4) == 0;
    }

    // Reads the four-byte RIFF magic of a file ("RIFF", "RF64", ...). Empty on failure.
    String readMagic (const File& f)
    {
        FileInputStream in (f);
        if (! in.openedOk())
            return {};
        char magic[4];
        if (in.read (magic, 4) != 4)
            return {};
        return String (CharPointer_ASCII (magic), (size_t) 4);
    }

    // Streams the RIFF chunk directory of `wav` looking for `id`. Reads only chunk
    // headers and the matched chunk's payload -- never the (huge) audio data.
    // Returns true and fills `out` if found.
    bool extractChunk (const File& wav, const String& id, MemoryBlock& out)
    {
        FileInputStream in (wav);
        if (! in.openedOk())
            return false;

        char riff[4];
        if (in.read (riff, 4) != 4)
            return false;
        const bool isRiff = (memcmp (riff, "RIFF", 4) == 0)
                         || (memcmp (riff, "RF64", 4) == 0)
                         || (memcmp (riff, "BW64", 4) == 0);
        if (! isRiff)
            return false;

        in.skipNextBytes (4); // RIFF size
        char wave[4];
        if (in.read (wave, 4) != 4 || memcmp (wave, "WAVE", 4) != 0)
            return false;

        while (! in.isExhausted())
        {
            char cid[4];
            if (in.read (cid, 4) != 4)
                break;
            uint8 szb[4];
            if (in.read (szb, 4) != 4)
                break;
            const uint32 sz = (uint32) szb[0] | ((uint32) szb[1] << 8)
                            | ((uint32) szb[2] << 16) | ((uint32) szb[3] << 24);

            if (matchesId (cid, id))
            {
                out.setSize (sz);
                return (size_t) in.read (out.getData(), (int) sz) == (size_t) sz;
            }

            // An RF64 sentinel data size means we can no longer trust byte offsets to
            // skip forward; stop scanning. (Broadcast metadata normally precedes `data`.)
            if (sz == kRf64DataSentinel)
                break;

            const int64 advance = (int64) sz + (sz & 1); // chunks are word-aligned
            in.setPosition (in.getPosition() + advance);
        }
        return false;
    }

    bool hasChunk (const File& wav, const String& id)
    {
        MemoryBlock ignored;
        return extractChunk (wav, id, ignored);
    }

    void writeU32LE (FileOutputStream& out, uint32 v)
    {
        const uint8 b[4] = { (uint8) v, (uint8) (v >> 8), (uint8) (v >> 16), (uint8) (v >> 24) };
        out.write (b, 4);
    }

    void writeU64LE (FileOutputStream& out, uint64 v)
    {
        uint8 b[8];
        for (int i = 0; i < 8; ++i) b[i] = (uint8) (v >> (8 * i));
        out.write (b, 8);
    }

    uint64 readU64LE (const uint8* p)
    {
        uint64 v = 0;
        for (int i = 0; i < 8; ++i) v |= (uint64) p[i] << (8 * i);
        return v;
    }

    // RF64 (EBU Tech 3306 / BW64) keeps the real RIFF size in a mandatory `ds64` chunk that
    // immediately follows 'WAVE'. Reads the dest header, verifies the ds64 layout, and returns
    // the absolute file offset of the 64-bit riffSize field plus its current value.
    // Returns false if the file is not a well-formed RF64 with ds64 first.
    bool locateRf64RiffSize (const File& dest, int64& fieldOffset, uint64& currentValue)
    {
        FileInputStream in (dest);
        if (! in.openedOk())
            return false;

        uint8 header[28];
        if (in.read (header, sizeof (header)) != (int) sizeof (header))
            return false;

        // [0..3]="RF64" [4..7]=0xFFFFFFFF [8..11]="WAVE" [12..15]="ds64" [16..19]=ds64Size
        // [20..27]=riffSize(uint64)
        if (memcmp (header,      "RF64", 4) != 0) return false;
        if (memcmp (header + 8,  "WAVE", 4) != 0) return false;
        if (memcmp (header + 12, "ds64", 4) != 0) return false;

        fieldOffset  = 20;
        currentValue = readU64LE (header + 20);
        return true;
    }
}

StringArray defaultChunkIds()
{
    return { "iXML", "axml" };
}

bool preserve (const File& source, const File& dest,
               const StringArray& chunkIds, String& message)
{
    if (! source.existsAsFile() || ! dest.existsAsFile())
    {
        message = "source or destination file missing";
        return false;
    }

    // Collect the chunks that exist in source but not yet in dest.
    struct Pending { String id; MemoryBlock data; };
    Array<Pending> pending;
    StringArray skippedExisting;

    for (const auto& id : chunkIds)
    {
        MemoryBlock block;
        if (! extractChunk (source, id, block))
            continue; // source doesn't have it -> nothing to preserve
        if (hasChunk (dest, id))
        {
            skippedExisting.add (id); // JUCE already wrote one; don't duplicate
            continue;
        }
        pending.add ({ id, std::move (block) });
    }

    if (pending.isEmpty())
    {
        message = skippedExisting.isEmpty()
                    ? "no broadcast chunks to preserve"
                    : "chunks already present in output, left untouched: " + skippedExisting.joinIntoString (", ");
        return true; // nothing needed -> success
    }

    // The dest is either standard RIFF (32-bit size at bytes 4..7) or RF64/BW64 (sentinel
    // size at bytes 4..7, real 64-bit size in the leading `ds64` chunk).
    const String magic = readMagic (dest);
    const bool isRiff = (magic == "RIFF");
    const bool isRf64 = (magic == "RF64") || (magic == "BW64");

    int64  ds64FieldOffset = -1;   // RF64 only: file offset of the 64-bit riffSize field
    uint64 ds64RiffSize    = 0;
    if (isRf64 && ! locateRf64RiffSize (dest, ds64FieldOffset, ds64RiffSize))
    {
        message = "output is '" + magic + "' but its ds64 chunk is missing/misplaced; not appending to avoid corruption";
        return false;
    }
    if (! isRiff && ! isRf64)
    {
        message = "output is '" + magic + "' (unrecognised RIFF variant); cannot append "
                + String (pending.size()) + " chunk(s)";
        return false;
    }

    const int64 destSizeBefore = dest.getSize();
    if (destSizeBefore < 12)
    {
        message = "output too small to be a valid WAV";
        return false;
    }

    // Compute total appended size.
    int64 appended = 0;
    for (const auto& p : pending)
    {
        const int64 payload = (int64) p.data.getSize();
        appended += 8 + payload + (payload & 1);
    }
    // Standard RIFF caps its size field at 4 GB; RF64 uses a 64-bit field and has no such limit.
    if (isRiff && destSizeBefore - 8 + appended > kMaxRiffSize)
    {
        message = "appending chunks would exceed the 4 GB RIFF limit; output must be RF64 for this to be supported";
        return false;
    }

    FileOutputStream out (dest); // opens existing file, positioned at end, no truncation
    if (! out.openedOk())
    {
        message = "could not open output for append: " + out.getStatus().getErrorMessage();
        return false;
    }

    // Defensive: a valid WAV ends word-aligned, but never append at an odd offset.
    if ((out.getPosition() & 1) != 0)
    {
        const uint8 pad = 0;
        out.write (&pad, 1);
        appended += 1;
    }

    StringArray copied;
    for (const auto& p : pending)
    {
        const int64 payload = (int64) p.data.getSize();
        out.write (p.id.toRawUTF8(), 4);
        writeU32LE (out, (uint32) payload);
        if (payload > 0)
            out.write (p.data.getData(), (size_t) payload);
        if ((payload & 1) != 0)
        {
            const uint8 pad = 0;
            out.write (&pad, 1);
        }
        copied.add (p.id);
    }

    // Patch the container size to include the appended chunks. For RIFF that is the uint32
    // field at bytes 4..7; for RF64 it is the uint64 riffSize inside the ds64 chunk (the
    // bytes 4..7 sentinel stays 0xFFFFFFFF). The data chunk size is unchanged either way.
    bool seekOk = false;
    if (isRiff)
    {
        seekOk = out.setPosition (4);
        if (seekOk) writeU32LE (out, (uint32) ((destSizeBefore - 8) + appended));
    }
    else // RF64
    {
        seekOk = out.setPosition (ds64FieldOffset);
        if (seekOk) writeU64LE (out, ds64RiffSize + (uint64) appended);
    }
    if (! seekOk)
    {
        message = "wrote chunks but failed to seek to patch the container size (file may be corrupt)";
        return false;
    }
    out.flush();

    message = "preserved chunk(s): " + copied.joinIntoString (", ");
    if (! skippedExisting.isEmpty())
        message += "; already present (left untouched): " + skippedExisting.joinIntoString (", ");
    return true;
}

bool preserve (const File& source, const File& dest, String& message)
{
    return preserve (source, dest, defaultChunkIds(), message);
}

} // namespace BroadcastChunkPreserver
