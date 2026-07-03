/*
  ==============================================================================

    PlaybackController.cpp
    Copyright (C) 2025 ZQ SFX

  ==============================================================================
*/

#include "PlaybackController.h"

#include "../Utils/Document.h"
#include "../Audio/AudioEngine.h"
#include "../UI/WaveformDisplay.h"
#include "../UI/TransportControls.h"
#include "../Utils/RegionManager.h"
#include "../Utils/Region.h"

void PlaybackController::togglePlayback(Document* doc)
{
    if (doc == nullptr) return;
    auto& engine = doc->getAudioEngine();
    if (! engine.isFileLoaded()) return;

    if (engine.isPlaying())
    {
        engine.stop();
        return;
    }

    // Clear any stale loop state from a previous session.
    engine.clearLoopPoints();
    engine.setLooping(false);

    auto& waveform = doc->getWaveformDisplay();

    if (waveform.hasSelection())
    {
        const double selStart = waveform.getSelectionStart();
        const double selEnd   = waveform.getSelectionEnd();

        engine.setPosition(selStart);
        // looping=false means stop at selEnd (don't loop back).
        engine.setLoopPoints(selStart, selEnd);
        engine.setLooping(false);
    }
    else if (waveform.hasEditCursor())
    {
        engine.setPosition(waveform.getEditCursorPosition());
    }
    else
    {
        engine.setPosition(0.0);
    }

    engine.play();
}

void PlaybackController::stopPlayback(Document* doc)
{
    if (doc == nullptr) return;
    auto& engine = doc->getAudioEngine();
    if (! engine.isFileLoaded()) return;

    engine.stop();
}

void PlaybackController::pausePlayback(Document* doc)
{
    if (doc == nullptr) return;
    auto& engine = doc->getAudioEngine();
    if (! engine.isFileLoaded()) return;

    const auto state = engine.getPlaybackState();
    if (state == PlaybackState::PLAYING)
        engine.pause();
    else if (state == PlaybackState::PAUSED)
        engine.play();
}

void PlaybackController::toggleLoop(Document* doc)
{
    if (doc == nullptr) return;
    auto& engine = doc->getAudioEngine();
    if (! engine.isFileLoaded()) return;

    auto& transport = doc->getTransportControls();
    transport.toggleLoop();
    engine.setLooping(transport.isLoopEnabled());
}

void PlaybackController::loopRegion(Document* doc)
{
    if (doc == nullptr) return;
    auto& regionMgr = doc->getRegionManager();

    const int selectedIndex = regionMgr.getSelectedRegionIndex();
    if (selectedIndex < 0 || selectedIndex >= regionMgr.getNumRegions())
        return;

    auto* region = regionMgr.getRegion(selectedIndex);
    if (region == nullptr) return;

    auto& engine = doc->getAudioEngine();
    const double sampleRate = engine.getSampleRate();
    const double startTime  = region->getStartSample() / sampleRate;
    const double endTime    = region->getEndSample()   / sampleRate;

    // H2: setSelection takes seconds, not raw samples. Use the same divided
    // times we pass to setLoopPoints below so the highlighted selection matches
    // the actual loop range instead of spanning the whole file.
    doc->getWaveformDisplay().setSelection(startTime, endTime);
    engine.setLoopPoints(startTime, endTime);
    engine.setLooping(true);
    engine.setPosition(startTime);
    engine.play();
}

double PlaybackController::currentCursorPosition(Document* doc) const
{
    auto& engine = doc->getAudioEngine();
    auto& waveform = doc->getWaveformDisplay();

    // While playing, mark against the live playhead so the I/O workflow
    // captures where you are hearing. When stopped, prefer the edit cursor.
    if (engine.isPlaying())
        return engine.getCurrentPosition();
    if (waveform.hasEditCursor())
        return waveform.getEditCursorPosition();
    return waveform.getPlaybackPosition();
}

void PlaybackController::markSelectionStart(Document* doc)
{
    if (doc == nullptr) return;
    auto& engine = doc->getAudioEngine();
    if (! engine.isFileLoaded()) return;

    auto& waveform = doc->getWaveformDisplay();
    const double pos = currentCursorPosition(doc);
    const double otherEnd = waveform.hasSelection() ? waveform.getSelectionEnd() : pos;
    // setSelection sorts start/end, so an in-point past the out-point is safe.
    waveform.setSelection(pos, otherEnd);
}

void PlaybackController::markSelectionEnd(Document* doc)
{
    if (doc == nullptr) return;
    auto& engine = doc->getAudioEngine();
    if (! engine.isFileLoaded()) return;

    auto& waveform = doc->getWaveformDisplay();
    const double pos = currentCursorPosition(doc);
    const double otherStart = waveform.hasSelection() ? waveform.getSelectionStart() : pos;
    waveform.setSelection(otherStart, pos);
}
