/*
  ==============================================================================

    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/

#include "WaveformDisplay.h"
#include "../Utils/AudioBufferInputSource.h"
#include "../Utils/RegionManager.h"
#include "../Utils/Region.h"
#include "../Utils/Settings.h"
#include "../Utils/ThumbnailDiskCache.h"
#include "ThemeManager.h"
#include "../Audio/ChannelLayout.h"
#include <cmath>
#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
WaveformDisplay::WaveformDisplay(juce::AudioFormatManager& formatManager)
    : m_thumbnailCache(5),
      m_thumbnail(512, formatManager, m_thumbnailCache),
      m_scrollbar(false),
      m_fileLoaded(false),
      m_isLoading(false),
      m_numChannels(0),
      m_sampleRate(44100.0),
      m_totalDuration(0.0),
      m_visibleStart(0.0),
      m_visibleEnd(10.0),
      m_zoomLevel(1.0),
      m_playbackPosition(0.0),
      m_followPlayback(true),  // Auto-scroll enabled by default
      m_lastPlaybackPosition(0.0),
      m_lastUserScrollTime(0.0),
      m_isScrollingProgrammatically(false),
      m_hasSelection(false),
      m_selectionStart(0.0),
      m_selectionEnd(0.0),
      m_isDraggingSelection(false),
      m_dragStartTime(0.0),
      m_focusedChannels(-1),  // All channels focused by default
      m_isExtendingSelection(false),
      m_selectionAnchor(0.0),
      m_hasEditCursor(false),
      m_editCursorPosition(0.0),
      m_selectionAlpha(0.25f),
      m_selectionAlphaIncreasing(true),
      m_snapUnitType(AudioUnits::UnitType::Milliseconds),  // Default unit
      m_snapIncrementIndex(0),  // Default to OFF (index 0 = snap disabled)
      m_snapEnabled(false),  // G key toggle: snap starts disabled
      m_lastSnapIncrementIndex(1),  // Remember first increment (10ms default)
      m_zeroCrossingEnabled(false),
      m_audioBufferRef(nullptr),
      m_useDirectRendering(false),
      m_regionManager(nullptr),
      m_previewPosition(0.0),
      m_hasPreviewPosition(false)
{
    // Set up thumbnail
    m_thumbnail.addChangeListener(this);

    // Subscribe to theme switches so we re-skin live without a restart.
    waveedit::ThemeManager::getInstance().addChangeListener(this);

    // Set up scrollbar
    m_scrollbar.addListener(this);
    m_scrollbar.setAutoHide(false);
    addAndMakeVisible(m_scrollbar);

    // Enable 60fps repaint for smooth cursor movement
    setOpaque(true);

    // Start animation timer for pulsing selection (30fps)
    startTimer(33);
}

WaveformDisplay::~WaveformDisplay()
{
    stopTimer();
    m_thumbnail.removeChangeListener(this);
    waveedit::ThemeManager::getInstance().removeChangeListener(this);
    m_scrollbar.removeListener(this);
}

//==============================================================================
// File loading

bool WaveformDisplay::loadFile(const juce::File& file, double sampleRate, int numChannels)
{
    // Validate file
    if (!file.existsAsFile())
    {
        m_lastError = "File does not exist: " + file.getFileName();
        return false;
    }

    if (!file.hasReadAccess())
    {
        m_lastError = "No read permission for file: " + file.getFileName();
        return false;
    }

    clear();

    // Store audio properties
    m_sampleRate = sampleRate;
    m_numChannels = numChannels;

    // Bookkeeping for the disk cache.
    m_loadedFile = file;
    m_thumbnailFromCache = false;
    m_thumbnailCacheSaved = false;
    m_isLoading = true;

    // Try to populate the thumbnail from the on-disk cache. If the
    // load succeeds, AudioThumbnail::loadFrom() fires its change
    // message and our changeListenerCallback finishes the rest of
    // the setup — no audio decoding required.
    if (ThumbnailDiskCache::tryLoad(file, m_thumbnail))
    {
        m_thumbnailFromCache = true;
        repaint();
        return true;
    }

    // Cache miss: kick off the normal async generation. AudioThumbnail
    // takes ownership of the source.
    auto source = std::make_unique<juce::FileInputSource>(file);
    m_thumbnail.setSource(source.release());

    // The thumbnail will call changeListenerCallback when data is ready
    repaint();
    return true;
}

bool WaveformDisplay::reloadFromBuffer(const juce::AudioBuffer<float>& buffer,
                                       double sampleRate,
                                       bool preserveView,
                                       bool preserveEditCursor)
{
    // IMPORTANT: Must be called from message thread only
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Validate buffer
    if (buffer.getNumSamples() == 0 || buffer.getNumChannels() == 0)
    {
        m_lastError = "Cannot reload from empty buffer";
        DBG("WaveformDisplay::reloadFromBuffer - Empty buffer");
        return false;
    }

    if (sampleRate <= 0.0)
    {
        m_lastError = "Invalid sample rate";
        DBG("WaveformDisplay::reloadFromBuffer - Invalid sample rate: " + juce::String(sampleRate));
        return false;
    }

    // CRITICAL: Save view state BEFORE any changes
    double savedVisibleStart = m_visibleStart;
    double savedVisibleEnd = m_visibleEnd;
    double savedZoomLevel = m_zoomLevel;
    bool savedHasEditCursor = m_hasEditCursor;
    double savedEditCursorPos = m_editCursorPosition;

    DBG(juce::String::formatted(
        "reloadFromBuffer BEFORE - view: %.2f-%.2f, cursor: %s at %.2f, preserveView=%d, preserveEditCursor=%d",
        savedVisibleStart, savedVisibleEnd,
        savedHasEditCursor ? "YES" : "NO", savedEditCursorPos,
        preserveView ? 1 : 0, preserveEditCursor ? 1 : 0));

    // Store audio properties
    m_sampleRate = sampleRate;
    m_numChannels = buffer.getNumChannels();
    m_totalDuration = buffer.getNumSamples() / sampleRate;

    // PERFORMANCE OPTIMIZATION (2025-10-08):
    // =======================================
    // Cache buffer for fast direct rendering (<10ms visual feedback)
    {
        juce::ScopedLock lock(m_bufferLock);

        // Make a deep copy of the buffer for direct rendering
        m_cachedBuffer.setSize(buffer.getNumChannels(), buffer.getNumSamples(), false, false, false);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            m_cachedBuffer.copyFrom(ch, 0, buffer, ch, 0, buffer.getNumSamples());
        }

        // Enable fast direct rendering mode - STAY in this mode permanently after edits
        m_useDirectRendering = true;

        DBG(juce::String::formatted(
            "PERFORMANCE: Fast rendering enabled - %d samples cached",
            buffer.getNumSamples()));
    }

    // CRITICAL OPTIMIZATION: DO NOT regenerate entire AudioThumbnail after edits!
    // This was causing the slow redraw - regenerating entire thumbnail is wasteful.
    // Fast direct rendering is sufficient for all zoom levels.
    // Thumbnail is only needed for initial file load (handled in loadFile()).

    // Mark as ready immediately - no waiting for thumbnail regeneration
    m_fileLoaded = true;
    m_isLoading = false;

    // CRITICAL: Restore view state if requested
    if (preserveView && m_totalDuration > 0)
    {
        // Constrain saved view to new file bounds
        m_visibleStart = juce::jlimit(0.0, m_totalDuration, savedVisibleStart);
        m_visibleEnd = juce::jlimit(m_visibleStart, m_totalDuration, savedVisibleEnd);
        m_zoomLevel = savedZoomLevel;

        DBG(juce::String::formatted(
            "WaveformDisplay::reloadFromBuffer - View preserved: %.2f-%.2f, zoom: %.2f",
            m_visibleStart, m_visibleEnd, m_zoomLevel));
    }
    else
    {
        // Default behavior: fit first 10 seconds to view
        m_visibleStart = 0.0;
        m_visibleEnd = juce::jmin(10.0, m_totalDuration);
        m_zoomLevel = 1.0;
    }

    updateScrollbar();

    // CRITICAL: Restore edit cursor if requested
    if (preserveEditCursor && savedHasEditCursor)
    {
        if (savedEditCursorPos <= m_totalDuration)
        {
            m_editCursorPosition = savedEditCursorPos;
            m_hasEditCursor = true;
            DBG(juce::String::formatted(
                "Edit cursor RESTORED at %.2f", m_editCursorPosition));
        }
        else
        {
            m_editCursorPosition = m_totalDuration;
            m_hasEditCursor = true;
            DBG(juce::String::formatted(
                "Edit cursor MOVED to file end: %.2f", m_editCursorPosition));
        }
    }
    else if (!preserveEditCursor)
    {
        // Clear selection and cursor if not preserving
        clearSelection();
        m_hasEditCursor = false;
        m_playbackPosition = 0.0;
        DBG("Edit cursor CLEARED (not preserving)");
    }
    else
    {
        DBG("Edit cursor NOT restored (savedHasEditCursor=false)");
    }

    DBG("WaveformDisplay: Reloaded from buffer - " +
                             juce::String(buffer.getNumSamples()) + " samples, " +
                             juce::String(m_numChannels) + " channels, " +
                             juce::String(m_totalDuration, 2) + " seconds");

    repaint();
    return true;
}

void WaveformDisplay::clear()
{
    m_thumbnail.clear();
    m_fileLoaded = false;
    m_isLoading = false;
    m_numChannels = 0;
    m_totalDuration = 0.0;
    m_visibleStart = 0.0;
    m_visibleEnd = 10.0;
    m_playbackPosition = 0.0;
    m_lastError.clear();
    clearSelection();
    clearEditCursor();
    updateScrollbar();

    // Clear fast rendering cache
    {
        juce::ScopedLock lock(m_bufferLock);
        m_cachedBuffer.setSize(0, 0);
        m_useDirectRendering = false;
    }

    repaint();
}

//==============================================================================
// Playback control

void WaveformDisplay::setPlaybackPosition(double positionInSeconds)
{
    juce::ScopedLock lock(m_playbackLock);  // Thread safety for playback state

    // Use epsilon comparison for floating point
    if (std::abs(m_playbackPosition - positionInSeconds) > TIME_EPSILON)
    {
        m_lastPlaybackPosition = m_playbackPosition;
        m_playbackPosition = positionInSeconds;

        // Auto-scroll to keep playback cursor visible (only if follow mode enabled)
        // EDGE CASE PROTECTION (2025-10-17): Don't auto-scroll while user is dragging selection
        // This prevents jarring view jumps while user is making precise selections
        if (m_followPlayback && !m_isDraggingSelection)
        {
            double visibleDuration = m_visibleEnd - m_visibleStart;

            // Use smooth look-ahead scrolling: scroll before cursor reaches edge
            // Keep cursor in the range [SCROLL_TRIGGER_LEFT to SCROLL_TRIGGER_RIGHT] of the visible area
            double cursorPositionInView = (m_playbackPosition - m_visibleStart) / visibleDuration;

            // Scroll if cursor is approaching right edge or past left edge
            if (cursorPositionInView > SCROLL_TRIGGER_RIGHT || cursorPositionInView < SCROLL_TRIGGER_LEFT ||
                m_playbackPosition < m_visibleStart || m_playbackPosition > m_visibleEnd)
            {
                // Position playback cursor at CURSOR_POSITION_RATIO from left edge (gives room to see ahead)
                m_visibleStart = m_playbackPosition - visibleDuration * CURSOR_POSITION_RATIO;
                m_visibleEnd = m_visibleStart + visibleDuration;
                constrainVisibleRange();

                // Update scrollbar without triggering callback (to prevent disabling follow mode)
                updateScrollbar(false);
            }
        }

        repaint();
    }
}

void WaveformDisplay::setPreviewPosition(double positionInSeconds)
{
    juce::ScopedLock lock(m_playbackLock);  // Thread safety for preview position

    // Minimal epsilon comparison for performance
    constexpr double epsilon = 0.00001;
    if (std::abs(m_previewPosition - positionInSeconds) < epsilon && m_hasPreviewPosition)
    {
        return;  // No significant change
    }

    m_previewPosition = positionInSeconds;
    m_hasPreviewPosition = true;

    repaint();
}

void WaveformDisplay::clearPreviewPosition()
{
    juce::ScopedLock lock(m_playbackLock);  // Thread safety for preview position

    if (!m_hasPreviewPosition)
    {
        return;  // Already cleared
    }

    m_hasPreviewPosition = false;
    m_previewPosition = 0.0;

    repaint();
}

void WaveformDisplay::setFollowPlayback(bool shouldFollow)
{
    juce::ScopedLock lock(m_playbackLock);  // Thread safety for playback state

    // No change - nothing to do
    if (m_followPlayback == shouldFollow)
    {
        return;
    }

    m_followPlayback = shouldFollow;

    // ENHANCEMENT (2025-10-17): If enabling follow mode during playback, immediately scroll to show playback cursor
    // This provides instant feedback when user toggles auto-scroll back on
    if (shouldFollow && m_playbackPosition > 0.0 && m_fileLoaded)
    {
        double visibleDuration = m_visibleEnd - m_visibleStart;

        // Check if playback cursor is already visible - only scroll if it's outside current view
        bool cursorVisible = (m_playbackPosition >= m_visibleStart && m_playbackPosition <= m_visibleEnd);

        if (!cursorVisible)
        {
            // Position playback cursor at CURSOR_POSITION_RATIO from left edge (gives room to see ahead)
            m_visibleStart = m_playbackPosition - visibleDuration * CURSOR_POSITION_RATIO;
            m_visibleEnd = m_visibleStart + visibleDuration;
            constrainVisibleRange();

            // Update scrollbar without sending notification (this is programmatic)
            updateScrollbar(false);
            repaint();
        }
    }
}

//==============================================================================
// Selection

void WaveformDisplay::setSelection(double startInSeconds, double endInSeconds)
{
    m_hasSelection = true;
    m_selectionStart = juce::jmin(startInSeconds, endInSeconds);
    m_selectionEnd = juce::jmax(startInSeconds, endInSeconds);

    // Force immediate repaint to show selection
    repaint();
}

void WaveformDisplay::clearSelection()
{
    m_hasSelection = false;
    m_selectionStart = 0.0;
    m_selectionEnd = 0.0;
    m_isExtendingSelection = false;  // Clear extending flag when selection is cleared
    repaint();
}

double WaveformDisplay::getSelectionDuration() const
{
    if (!m_hasSelection)
    {
        return 0.0;
    }
    return std::abs(m_selectionEnd - m_selectionStart);
}

juce::String WaveformDisplay::getSelectionStartString() const
{
    if (!m_hasSelection)
    {
        return "--:--:--.---";
    }

    double time = m_selectionStart;
    int hours = static_cast<int>(time / 3600.0);
    int minutes = static_cast<int>((time - hours * 3600.0) / 60.0);
    double seconds = time - hours * 3600.0 - minutes * 60.0;

    if (hours > 0)
    {
        return juce::String::formatted("%02d:%02d:%06.3f", hours, minutes, seconds);
    }
    else
    {
        return juce::String::formatted("%02d:%06.3f", minutes, seconds);
    }
}

juce::String WaveformDisplay::getSelectionEndString() const
{
    if (!m_hasSelection)
    {
        return "--:--:--.---";
    }

    double time = m_selectionEnd;
    int hours = static_cast<int>(time / 3600.0);
    int minutes = static_cast<int>((time - hours * 3600.0) / 60.0);
    double seconds = time - hours * 3600.0 - minutes * 60.0;

    if (hours > 0)
    {
        return juce::String::formatted("%02d:%02d:%06.3f", hours, minutes, seconds);
    }
    else
    {
        return juce::String::formatted("%02d:%06.3f", minutes, seconds);
    }
}

juce::String WaveformDisplay::getSelectionDurationString() const
{
    if (!m_hasSelection)
    {
        return "--:--:--.---";
    }

    double duration = getSelectionDuration();
    int hours = static_cast<int>(duration / 3600.0);
    int minutes = static_cast<int>((duration - hours * 3600.0) / 60.0);
    double seconds = duration - hours * 3600.0 - minutes * 60.0;

    if (hours > 0)
    {
        return juce::String::formatted("%02d:%02d:%06.3f", hours, minutes, seconds);
    }
    else
    {
        return juce::String::formatted("%02d:%06.3f", minutes, seconds);
    }
}

//==============================================================================
// Channel Focus (for per-channel editing)

void WaveformDisplay::setFocusedChannels(int channelMask)
{
    if (m_focusedChannels != channelMask)
    {
        m_focusedChannels = channelMask;
        repaint();

        if (onChannelFocusChanged)
            onChannelFocusChanged(channelMask);
    }
}

bool WaveformDisplay::isChannelFocused(int channel) const
{
    if (m_focusedChannels == -1)
        return true;  // All channels focused

    return (m_focusedChannels & (1 << channel)) != 0;
}

void WaveformDisplay::focusChannel(int channel)
{
    if (channel < 0)
        setFocusedChannels(-1);  // Focus all
    else
        setFocusedChannels(1 << channel);  // Focus single channel
}

int WaveformDisplay::getChannelAtY(int y) const
{
    // Check if y is in the waveform area
    int waveformTop = RULER_HEIGHT;
    int waveformBottom = getHeight() - SCROLLBAR_HEIGHT;

    if (y < waveformTop || y >= waveformBottom)
        return -1;  // Outside waveform area

    if (m_numChannels <= 1)
        return 0;  // Mono - always channel 0

    // Calculate channel positions
    int waveformHeight = waveformBottom - waveformTop;
    int channelHeight = waveformHeight / m_numChannels;
    int relativeY = y - waveformTop;

    for (int ch = 0; ch < m_numChannels; ++ch)
    {
        int channelTop = ch * channelHeight;
        int channelBottom = (ch + 1) * channelHeight;

        // Account for gap between channels
        if (ch < m_numChannels - 1)
            channelBottom -= CHANNEL_GAP;

        if (relativeY >= channelTop && relativeY < channelBottom)
            return ch;

        // Check if in the gap between this channel and the next
        if (ch < m_numChannels - 1)
        {
            int gapTop = channelBottom;
            int gapBottom = (ch + 1) * channelHeight;
            if (relativeY >= gapTop && relativeY < gapBottom)
                return -1;  // In gap - means "all channels"
        }
    }

    return -1;  // Shouldn't reach here, but fallback to all channels
}

//==============================================================================
// Edit cursor

void WaveformDisplay::setEditCursor(double positionInSeconds)
{
    // Clamp position to valid range
    m_editCursorPosition = juce::jlimit(0.0, m_totalDuration, positionInSeconds);
    m_hasEditCursor = true;

    // Auto-scroll to show cursor if it's outside visible range
    if (m_editCursorPosition < m_visibleStart || m_editCursorPosition > m_visibleEnd)
    {
        double visibleDuration = m_visibleEnd - m_visibleStart;
        m_visibleStart = m_editCursorPosition - visibleDuration * 0.25; // Position at 25% from left
        m_visibleEnd = m_visibleStart + visibleDuration;
        constrainVisibleRange();
        updateScrollbar();
    }

    repaint();
}

void WaveformDisplay::clearEditCursor()
{
    m_hasEditCursor = false;
    m_editCursorPosition = 0.0;
    repaint();
}

void WaveformDisplay::moveEditCursor(double deltaInSeconds)
{
    if (!m_hasEditCursor)
    {
        // If no cursor, create one at the current playback position
        setEditCursor(m_playbackPosition);
        return;
    }

    double newPosition = m_editCursorPosition + deltaInSeconds;
    setEditCursor(newPosition);
}

//==============================================================================
// Zoom and navigation

void WaveformDisplay::setSnapUnit(AudioUnits::UnitType unitType)
{
    juce::ScopedLock lock(m_snapLock);
    m_snapUnitType = unitType;
    // Reset to first increment (off) when changing unit
    m_snapIncrementIndex = 0;
    repaint();
}

void WaveformDisplay::cycleSnapIncrement()
{
    juce::ScopedLock lock(m_snapLock);
    // Get increments for current unit
    const auto& increments = AudioUnits::getIncrementsForUnit(m_snapUnitType);

    // Cycle to next increment (wrap around)
    m_snapIncrementIndex = (m_snapIncrementIndex + 1) % increments.size();

    repaint();
}

void WaveformDisplay::toggleSnap()
{
    juce::ScopedLock lock(m_snapLock);

    if (m_snapEnabled)
    {
        // Disabling snap: remember current increment and set to off
        if (m_snapIncrementIndex > 0)
        {
            m_lastSnapIncrementIndex = m_snapIncrementIndex;
        }
        m_snapIncrementIndex = 0;  // 0 = snap off
        m_snapEnabled = false;
    }
    else
    {
        // Enabling snap: restore last used increment
        m_snapIncrementIndex = m_lastSnapIncrementIndex;
        m_snapEnabled = true;
    }

    repaint();
}

int WaveformDisplay::getSnapIncrement() const
{
    juce::ScopedLock lock(m_snapLock);
    const auto& increments = AudioUnits::getIncrementsForUnit(m_snapUnitType);
    if (m_snapIncrementIndex < increments.size())
        return increments[m_snapIncrementIndex];
    return 0; // fallback: snap off
}

void WaveformDisplay::toggleZeroCrossing()
{
    juce::ScopedLock lock(m_snapLock);
    m_zeroCrossingEnabled = !m_zeroCrossingEnabled;
    repaint();
}

void WaveformDisplay::setNavigationPreferences(const NavigationPreferences& prefs)
{
    m_navigationPrefs = prefs;
}

void WaveformDisplay::setAudioBufferReference(const juce::AudioBuffer<float>* buffer)
{
    m_audioBufferRef = buffer;
}

void WaveformDisplay::setRegionManager(RegionManager* regionManager)
{
    m_regionManager = regionManager;
    repaint();  // Redraw with or without region overlays
}

double WaveformDisplay::getSnapIncrementInSeconds() const
{
    int increment = getSnapIncrement();

    // If increment is 0, snap is off - return default 10ms for navigation
    if (increment == 0)
        return 0.01;

    // Convert increment to seconds based on unit type
    switch (m_snapUnitType)
    {
        case AudioUnits::UnitType::Samples:
            return AudioUnits::samplesToSeconds(increment, m_sampleRate);

        case AudioUnits::UnitType::Milliseconds:
            return increment / 1000.0;

        case AudioUnits::UnitType::Seconds:
            // Increment is in tenths of seconds (e.g., 1 = 0.1s, 10 = 1.0s)
            return increment / 10.0;

        case AudioUnits::UnitType::Frames:
            return AudioUnits::samplesToSeconds(
                AudioUnits::framesToSamples(increment, m_navigationPrefs.getFrameRate(), m_sampleRate),
                m_sampleRate);

        case AudioUnits::UnitType::Custom:
            return AudioUnits::samplesToSeconds(increment, m_sampleRate);

        default:
            return 0.01; // 10ms default
    }
}

int64_t WaveformDisplay::getSnapIncrementInSamples() const
{
    int increment = getSnapIncrement();

    // If increment is 0, snap is off - return default 10ms in samples
    if (increment == 0)
    {
        return static_cast<int64_t>(m_sampleRate * 0.01); // 10ms default
    }

    // Convert increment to samples based on unit type
    switch (m_snapUnitType)
    {
        case AudioUnits::UnitType::Samples:
            return increment;

        case AudioUnits::UnitType::Milliseconds:
            return static_cast<int64_t>((increment / 1000.0) * m_sampleRate);

        case AudioUnits::UnitType::Seconds:
            // Increment is in tenths of seconds (e.g., 1 = 0.1s, 10 = 1.0s)
            return static_cast<int64_t>((increment / 10.0) * m_sampleRate);

        case AudioUnits::UnitType::Frames:
            return AudioUnits::framesToSamples(increment, m_navigationPrefs.getFrameRate(), m_sampleRate);

        case AudioUnits::UnitType::Custom:
            return increment;

        default:
            return static_cast<int64_t>(m_sampleRate * 0.01); // 10ms default
    }
}

double WaveformDisplay::snapTimeToUnit(double time) const
{
    int increment = getSnapIncrement();

    // If snap is off (increment = 0) and zero-crossing is disabled, no snapping
    if (increment == 0 && !m_zeroCrossingEnabled)
        return time;

    if (m_sampleRate <= 0.0)
        return time;

    // Apply zero-crossing snap if enabled (can combine with unit snapping)
    if (m_zeroCrossingEnabled && m_audioBufferRef != nullptr)
    {
        int64_t sample = AudioUnits::secondsToSamples(time, m_sampleRate);
        int64_t snappedSample = AudioUnits::snapToZeroCrossing(
            sample, *m_audioBufferRef, 0, m_navigationPrefs.getZeroCrossingSearchRadius());
        return AudioUnits::samplesToSeconds(snappedSample, m_sampleRate);
    }

    // If increment is 0, no unit snapping
    if (increment == 0)
        return time;

    // Convert to SnapMode for compatibility with existing AudioUnits functions
    AudioUnits::SnapMode mode;
    switch (m_snapUnitType)
    {
        case AudioUnits::UnitType::Samples:       mode = AudioUnits::SnapMode::Samples; break;
        case AudioUnits::UnitType::Milliseconds:  mode = AudioUnits::SnapMode::Milliseconds; break;
        case AudioUnits::UnitType::Seconds:       mode = AudioUnits::SnapMode::Seconds; break;
        case AudioUnits::UnitType::Frames:        mode = AudioUnits::SnapMode::Frames; break;
        default:                                  mode = AudioUnits::SnapMode::Off; break;
    }

    // Standard unit snapping
    return AudioUnits::snapTimeToUnit(time, mode, increment,
                                     m_sampleRate, m_navigationPrefs.getFrameRate());
}

void WaveformDisplay::timerCallback()
{
    if (!m_hasSelection)
    {
        return;
    }

    // Animate selection overlay alpha between 0.25 and 0.35 for subtle pulsing effect
    const float alphaStep = 0.01f; // Smooth animation
    const float minAlpha = 0.25f;
    const float maxAlpha = 0.35f;

    if (m_selectionAlphaIncreasing)
    {
        m_selectionAlpha += alphaStep;
        if (m_selectionAlpha >= maxAlpha)
        {
            m_selectionAlpha = maxAlpha;
            m_selectionAlphaIncreasing = false;
        }
    }
    else
    {
        m_selectionAlpha -= alphaStep;
        if (m_selectionAlpha <= minAlpha)
        {
            m_selectionAlpha = minAlpha;
            m_selectionAlphaIncreasing = true;
        }
    }

    // Trigger repaint for animation
    repaint();
}

//==============================================================================
// ChangeListener override

void WaveformDisplay::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &waveedit::ThemeManager::getInstance())
    {
        repaint();
        return;
    }

    if (source == &m_thumbnail)
    {
        // Check if thumbnail is ready (only used for initial file load)
        if (m_isLoading && m_thumbnail.getTotalLength() > 0.0)
        {
            // Thumbnail loaded successfully from initial file load
            m_fileLoaded = true;
            m_isLoading = false;
            m_totalDuration = m_thumbnail.getTotalLength();

            // Set visible range to show entire file initially
            m_visibleStart = 0.0;
            m_visibleEnd = m_totalDuration;
            m_zoomLevel = 1.0;

            // Reset playback position
            m_playbackPosition = 0.0;

            // Clear selection
            clearSelection();

            // Update scrollbar
            updateScrollbar();

            // Notify listeners (RegionDisplay) of visible range change
            if (onVisibleRangeChanged)
                onVisibleRangeChanged(m_visibleStart, m_visibleEnd);

            // NOTE: We do NOT switch rendering modes here anymore
            // Once a file has been edited, we stay in fast rendering mode permanently
            // Fast direct rendering is sufficient for all use cases and much faster
        }

        // Save freshly-generated thumbnails to the on-disk cache so the
        // next open of this file is instant. Skip if we already loaded
        // from cache (no point re-saving the same data) or if we've
        // already saved in this session.
        if (! m_thumbnailFromCache
            && ! m_thumbnailCacheSaved
            && m_thumbnail.isFullyLoaded()
            && m_loadedFile != juce::File()
            && m_loadedFile.existsAsFile())
        {
            ThumbnailDiskCache::save(m_loadedFile, m_thumbnail);
            m_thumbnailCacheSaved = true;
        }

        // Repaint for loading progress or completion
        repaint();
    }
}

//==============================================================================
// ScrollBar::Listener override

void WaveformDisplay::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (!m_fileLoaded || scrollBarThatHasMoved != &m_scrollbar)
    {
        return;
    }

    // Disable follow mode if user manually scrolled (not programmatic auto-scroll)
    if (!m_isScrollingProgrammatically && m_followPlayback)
    {
        m_followPlayback = false;
        m_lastUserScrollTime = juce::Time::getMillisecondCounterHiRes();
    }

    double visibleDuration = m_visibleEnd - m_visibleStart;
    m_visibleStart = newRangeStart;
    m_visibleEnd = m_visibleStart + visibleDuration;

    constrainVisibleRange();
    repaint();

    // Notify listeners (RegionDisplay) of visible range change
    if (onVisibleRangeChanged)
        onVisibleRangeChanged(m_visibleStart, m_visibleEnd);
}

//==============================================================================
// Helper methods

int WaveformDisplay::timeToX(double timeInSeconds) const
{
    // CRITICAL FIX (2025-10-07): Guard against invalid state and zero width
    int width = getWidth();
    if (m_visibleEnd <= m_visibleStart || width <= 0)
    {
        return 0;
    }

    // Clamp time to visible range before conversion
    double clampedTime = juce::jlimit(m_visibleStart, m_visibleEnd, timeInSeconds);
    double ratio = (clampedTime - m_visibleStart) / (m_visibleEnd - m_visibleStart);

    // Clamp result to valid pixel range
    return juce::jlimit(0, width - 1, static_cast<int>(ratio * width));
}

double WaveformDisplay::xToTime(int x) const
{
    // CRITICAL FIX (2025-10-07): Guard against zero width (component not laid out yet)
    int width = getWidth();
    if (width <= 0)
    {
        return m_visibleStart;  // Return start of visible range when width invalid
    }

    // CRITICAL FIX (2025-10-07): Prevent negative time when mouse is outside widget bounds
    // Clamp x to [0, width-1] to ensure valid pixel coordinates
    int clampedX = juce::jlimit(0, width - 1, x);

    double ratio = static_cast<double>(clampedX) / width;
    double time = m_visibleStart + ratio * (m_visibleEnd - m_visibleStart);

    // Clamp time to [0, m_totalDuration] to ensure valid time range
    return juce::jlimit(0.0, m_totalDuration, time);
}

void WaveformDisplay::updateScrollbar(bool sendNotification)
{
    if (!m_fileLoaded)
    {
        m_scrollbar.setRangeLimits(0.0, 1.0);
        m_scrollbar.setCurrentRange(0.0, 1.0, juce::dontSendNotification);
        return;
    }

    // CRITICAL FIX (2025-10-17): Set flag to prevent disabling auto-scroll during programmatic scrolling
    // When sendNotification is false, this is a programmatic scroll (auto-scroll triggered by playback)
    // We must set this flag BEFORE updating the scrollbar to prevent scrollBarMoved() from disabling follow mode
    bool wasProgrammatic = m_isScrollingProgrammatically;
    if (!sendNotification)
    {
        m_isScrollingProgrammatically = true;
    }

    // Set scrollbar range
    m_scrollbar.setRangeLimits(0.0, m_totalDuration);

    // Set visible range (with optional notification control)
    double visibleDuration = m_visibleEnd - m_visibleStart;
    auto notificationType = sendNotification ? juce::sendNotificationSync : juce::dontSendNotification;
    m_scrollbar.setCurrentRange(m_visibleStart, visibleDuration, notificationType);

    // Restore flag state
    if (!sendNotification)
    {
        m_isScrollingProgrammatically = wasProgrammatic;
    }
}

void WaveformDisplay::constrainVisibleRange()
{
    if (!m_fileLoaded)
    {
        return;
    }

    // Ensure visible range doesn't exceed file bounds
    double visibleDuration = m_visibleEnd - m_visibleStart;

    if (visibleDuration > m_totalDuration)
    {
        // View is wider than file
        m_visibleStart = 0.0;
        m_visibleEnd = m_totalDuration;
    }
    else
    {
        // Constrain start
        if (m_visibleStart < 0.0)
        {
            m_visibleStart = 0.0;
            m_visibleEnd = visibleDuration;
        }

        // Constrain end
        if (m_visibleEnd > m_totalDuration)
        {
            m_visibleEnd = m_totalDuration;
            m_visibleStart = m_totalDuration - visibleDuration;
        }
    }
}

//==============================================================================
// Channel Context Menu
//==============================================================================

void WaveformDisplay::showChannelContextMenu(int channel, juce::Point<int> screenPos)
{
    juce::PopupMenu menu;

    bool isSolo = getChannelSolo ? getChannelSolo(channel) : false;
    bool isMute = getChannelMute ? getChannelMute(channel) : false;

    // Get channel label for display
    waveedit::ChannelLayout layout = waveedit::ChannelLayout::fromChannelCount(m_numChannels);
    juce::String channelName = layout.getFullName(channel);
    if (channelName.isEmpty())
        channelName = "Channel " + juce::String(channel + 1);

    // Solo/Mute toggles
    menu.addItem(1, isSolo ? "Unsolo " + channelName : "Solo " + channelName, true, isSolo);
    menu.addItem(2, isMute ? "Unmute " + channelName : "Mute " + channelName, true, isMute);
    menu.addSeparator();

    // Bulk operations
    menu.addItem(3, "Solo This Only");
    menu.addItem(4, "Unsolo All Channels");
    menu.addItem(5, "Unmute All Channels");

    menu.addSeparator();
    menu.addItem(6, "Set Channel Color...");
    const bool hasOverride = getChannelWaveformOverride(channel).getARGB() != 0u;
    menu.addItem(7, "Reset Channel Color", hasOverride);

    menu.showMenuAsync(juce::PopupMenu::Options()
        .withTargetScreenArea(juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1)),
        [this, channel](int result) { handleChannelMenuResult(channel, result); });
}

void WaveformDisplay::handleChannelMenuResult(int channel, int menuResult)
{
    switch (menuResult)
    {
        case 1:  // Toggle Solo
            if (onChannelSoloChanged && getChannelSolo)
            {
                bool currentSolo = getChannelSolo(channel);
                onChannelSoloChanged(channel, !currentSolo);
                repaint();
            }
            break;

        case 2:  // Toggle Mute
            if (onChannelMuteChanged && getChannelMute)
            {
                bool currentMute = getChannelMute(channel);
                onChannelMuteChanged(channel, !currentMute);
                repaint();
            }
            break;

        case 3:  // Solo This Only
            if (onChannelSoloChanged)
            {
                // Unsolo all others, solo this one
                for (int ch = 0; ch < m_numChannels && ch < 8; ++ch)
                {
                    onChannelSoloChanged(ch, ch == channel);
                }
                repaint();
            }
            break;

        case 4:  // Unsolo All
            if (onChannelSoloChanged)
            {
                for (int ch = 0; ch < m_numChannels && ch < 8; ++ch)
                {
                    onChannelSoloChanged(ch, false);
                }
                repaint();
            }
            break;

        case 5:  // Unmute All
            if (onChannelMuteChanged)
            {
                for (int ch = 0; ch < m_numChannels && ch < 8; ++ch)
                {
                    onChannelMuteChanged(ch, false);
                }
                repaint();
            }
            break;

        case 6:  // Set Channel Color
            {
                struct PickerWrapper : public juce::Component, private juce::ChangeListener
                {
                    PickerWrapper(WaveformDisplay& d, int ch, juce::Colour startC)
                        : display(d), channel(ch),
                          m_sel(juce::ColourSelector::showColourAtTop
                                | juce::ColourSelector::showSliders
                                | juce::ColourSelector::showColourspace)
                    {
                        m_sel.setCurrentColour(startC, juce::dontSendNotification);
                        m_sel.addChangeListener(this);
                        addAndMakeVisible(m_sel);
                        setSize(300, 280);
                    }
                    ~PickerWrapper() override { m_sel.removeChangeListener(this); }
                    void resized() override { m_sel.setBounds(getLocalBounds()); }
                    void changeListenerCallback(juce::ChangeBroadcaster*) override
                    {
                        display.setChannelWaveformOverride(channel, m_sel.getCurrentColour());
                    }
                    WaveformDisplay& display;
                    int channel;
                    juce::ColourSelector m_sel;
                };

                const auto bounds = m_channelLabelBounds[static_cast<size_t>(channel)];
                const auto screenBounds = localAreaToGlobal(bounds);
                auto wrapper = std::make_unique<PickerWrapper>(*this, channel,
                                                                getChannelWaveformColour(channel));
                juce::CallOutBox::launchAsynchronously(std::move(wrapper),
                                                        screenBounds, nullptr);
            }
            break;

        case 7:  // Reset Channel Color (clear override)
            setChannelWaveformOverride(channel, juce::Colour());
            break;

        default:
            break;
    }
}

//==============================================================================
// Per-channel waveform colours
//==============================================================================

namespace
{
    constexpr juce::uint32 kNoOverride = 0u;  // transparent black sentinel

    juce::String channelColourSettingsKey(int channel)
    {
        return "display.waveformColor.ch" + juce::String(channel);
    }

    juce::Colour resolveGlobalWaveformColour()
    {
        // Resolution order: explicit `display.waveformColor` setting
        // (legacy from before the theme system) → active theme's
        // waveformLine token. Most installs will have no setting, in
        // which case the theme drives the colour and switching themes
        // re-skins the waveform.
        auto raw = Settings::getInstance()
                       .getSetting("display.waveformColor", "")
                       .toString();
        if (raw.isNotEmpty())
            return juce::Colour::fromString(raw);
        return waveedit::ThemeManager::getInstance().getCurrent().waveformLine;
    }
}

juce::Colour WaveformDisplay::getChannelWaveformOverride(int channel) const
{
    if (channel < 0 || channel >= 8)
        return juce::Colour();

    if (! m_channelOverridesLoaded)
    {
        // const-cast: lazy load from Settings.
        auto* self = const_cast<WaveformDisplay*>(this);
        for (int i = 0; i < 8; ++i)
        {
            auto raw = Settings::getInstance()
                           .getSetting(channelColourSettingsKey(i), "")
                           .toString();
            self->m_channelWaveformOverride[static_cast<size_t>(i)] =
                raw.isEmpty() ? juce::Colour() : juce::Colour::fromString(raw);
        }
        self->m_channelOverridesLoaded = true;
    }

    return m_channelWaveformOverride[static_cast<size_t>(channel)];
}

juce::Colour WaveformDisplay::getChannelWaveformColour(int channel) const
{
    const auto override_ = getChannelWaveformOverride(channel);
    if (override_.getARGB() != kNoOverride)
        return override_;
    return resolveGlobalWaveformColour();
}

void WaveformDisplay::setChannelWaveformOverride(int channel, juce::Colour colour)
{
    if (channel < 0 || channel >= 8)
        return;

    // Force lazy-load before write so we don't race with it.
    (void) getChannelWaveformOverride(0);

    m_channelWaveformOverride[static_cast<size_t>(channel)] = colour;

    if (colour.getARGB() == kNoOverride)
        Settings::getInstance().setSetting(channelColourSettingsKey(channel), "");
    else
        Settings::getInstance().setSetting(channelColourSettingsKey(channel),
                                            colour.toString());
    repaint();
}

void WaveformDisplay::clearAllChannelWaveformOverrides()
{
    for (int i = 0; i < 8; ++i)
    {
        m_channelWaveformOverride[static_cast<size_t>(i)] = juce::Colour();
        Settings::getInstance().setSetting(channelColourSettingsKey(i), "");
    }
    m_channelOverridesLoaded = true;
    repaint();
}
