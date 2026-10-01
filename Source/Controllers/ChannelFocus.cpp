/*
  ==============================================================================

    ChannelFocus.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "ChannelFocus.h"
#include "../Audio/ChannelMask.h"
#include "../Utils/Document.h"
#include "../Utils/UndoActions/ChannelFocusUndoActions.h"

namespace ChannelFocus
{

int maskFor(Document& doc)
{
    return ChannelMask::normalise(doc.getWaveformDisplay().getFocusedChannels(),
                                  doc.getBufferManager().getNumChannels());
}

bool isPartial(Document& doc)
{
    return maskFor(doc) != ChannelMask::kAll;
}

juce::UndoableAction* wrap(Document& doc,
                           juce::UndoableAction* action,
                           const juce::AudioBuffer<float>& beforeRange,
                           int64_t startSample)
{
    const int mask = maskFor(doc);
    if (mask == ChannelMask::kAll || action == nullptr)
        return action;

    return new ChannelMaskedAction(action,
                                   doc.getBufferManager(),
                                   doc.getAudioEngine(),
                                   doc.getWaveformDisplay(),
                                   beforeRange,
                                   startSample,
                                   mask);
}

juce::AudioBuffer<float> snapshotIfPartial(Document& doc, int64_t startSample, int64_t numSamples)
{
    if (!isPartial(doc))
        return {};
    return doc.getBufferManager().getAudioRange(startSample, numSamples);
}

bool refuseIfPartial(Document& doc, const juce::String& operationName)
{
    if (!isPartial(doc))
        return false;

    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon,
        operationName,
        operationName + " changes the length of the file, so it cannot run on some "
        "channels only. Double-click the focused channel again to work on all "
        "channels, then try again.",
        "OK");
    return true;
}

ScopedPreviewMask::ScopedPreviewMask(Document& doc)
    : m_doc(doc)
{
    m_doc.getAudioEngine().setPreviewChannelMask(maskFor(m_doc));
}

ScopedPreviewMask::~ScopedPreviewMask()
{
    m_doc.getAudioEngine().setPreviewChannelMask(ChannelMask::kAll);
}

}
