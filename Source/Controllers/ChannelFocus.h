/*
  ==============================================================================

    ChannelFocus.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    How DSP operations honour per-channel focus (the channels picked in the
    waveform). Cut/Copy/Delete already work on the focused channels only;
    every DSP operation now does too:
      - length-preserving operations change only the focused channels
        (wrap() their undo action);
      - operations that change the file length or channel layout cannot be
        limited to some channels, so they are refused with a message while
        a partial focus is active (refuseIfPartial()). Resample and the
        channel converter/extractor are whole-file format operations and
        are not affected by focus.

  ==============================================================================
*/

#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_audio_basics/juce_audio_basics.h>

class Document;

namespace ChannelFocus
{
    /** The document's focus as a mask over its channels: ChannelMask::kAll
        unless some, but not all, channels are focused. */
    int maskFor(Document& doc);

    /** True if some, but not all, channels are focused. */
    bool isPartial(Document& doc);

    /**
     * The action to hand to the UndoManager: @p action itself when every
     * channel is focused, otherwise a ChannelMaskedAction that owns it.
     * @p beforeRange is the range [startSample, startSample + length) as it
     * was before the DSP ran (all channels). For length-preserving actions only.
     */
    juce::UndoableAction* wrap(Document& doc,
                               juce::UndoableAction* action,
                               const juce::AudioBuffer<float>& beforeRange,
                               int64_t startSample);

    /**
     * The range [startSample, startSample + numSamples) of every channel, for
     * wrap(), when a partial focus is active; an empty buffer otherwise (no
     * copy is needed when every channel is processed).
     */
    juce::AudioBuffer<float> snapshotIfPartial(Document& doc, int64_t startSample, int64_t numSamples);

    /**
     * For operations that change the file length or channel layout: when a
     * partial focus is active, tell the user why the operation cannot run
     * on some channels only and return true (the caller then does nothing).
     */
    bool refuseIfPartial(Document& doc, const juce::String& operationName);

    /** For the life of a (modal) DSP dialog: its realtime preview changes
        only the focused channels, like Apply. */
    class ScopedPreviewMask
    {
    public:
        explicit ScopedPreviewMask(Document& doc);
        ~ScopedPreviewMask();

    private:
        Document& m_doc;
        JUCE_DECLARE_NON_COPYABLE(ScopedPreviewMask)
    };
}
