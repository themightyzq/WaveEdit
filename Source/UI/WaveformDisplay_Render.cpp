/*
  ==============================================================================

    WaveformDisplay_Render.cpp
    WaveformEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Split out from WaveformDisplay.cpp under CLAUDE.md §7.5 (file-size
    cap). Hosts the paint pipeline: the top-level paint/resized
    overrides plus every drawXxx helper (time ruler, channel waveforms,
    selection box, playback / preview / edit cursors, region overlays).
    State, lifecycle, mouse, navigation, and zoom all stay in the main
    WaveformDisplay.cpp.

  ==============================================================================
*/

#include "WaveformDisplay.h"
#include "../Utils/AudioBufferInputSource.h"
#include "../Utils/RegionManager.h"
#include "../Utils/Region.h"
#include "../Utils/Settings.h"
#include "ThemeManager.h"
#include "../Audio/ChannelLayout.h"
#include <cmath>
#include <juce_gui_extra/juce_gui_extra.h>

void WaveformDisplay::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(waveedit::ThemeManager::getInstance().getCurrent().background);

    auto bounds = getLocalBounds();

    // Reserve space for scrollbar
    bounds.removeFromBottom(SCROLLBAR_HEIGHT);

    // Draw time ruler
    auto rulerBounds = bounds.removeFromTop(RULER_HEIGHT);
    drawTimeRuler(g, rulerBounds);

    if (m_isLoading)
    {
        // Loading in progress
        g.setColour(juce::Colours::white);
        g.setFont(16.0f);
        g.drawText("Loading waveform...", bounds, juce::Justification::centred, true);
        return;
    }

    if (!m_fileLoaded)
    {
        // No file loaded
        g.setColour(juce::Colours::grey);
        g.setFont(14.0f);
        g.drawText("No audio file loaded", bounds, juce::Justification::centred, true);
        return;
    }

    // Save the full waveform area bounds BEFORE we start modifying it
    auto waveformArea = bounds;

    // Draw waveforms for each channel FIRST
    if (m_numChannels == 1)
    {
        // Mono - use full height
        drawChannelWaveform(g, bounds, 0);
    }
    else if (m_numChannels >= 2)
    {
        // Stereo or multichannel - split vertically
        int channelHeight = bounds.getHeight() / m_numChannels;

        for (int ch = 0; ch < m_numChannels; ++ch)
        {
            auto channelBounds = bounds.removeFromTop(channelHeight);
            if (ch < m_numChannels - 1)
            {
                channelBounds.removeFromBottom(CHANNEL_GAP);
            }
            drawChannelWaveform(g, channelBounds, ch);
        }
    }

    // Draw region overlays ON TOP of waveform (semi-transparent colored bands)
    drawRegionOverlays(g, waveformArea);

    // Draw selection highlight ON TOP of waveform (so it's visible!)
    if (m_hasSelection)
    {
        drawSelection(g, waveformArea);
    }

    // Draw playback cursor (green)
    drawPlaybackCursor(g, waveformArea);

    // Draw preview cursor (orange) - shows position during DSP preview mode
    drawPreviewCursor(g, waveformArea);

    // Draw edit cursor on top of everything (yellow, shows paste position)
    drawEditCursor(g, waveformArea);
}

void WaveformDisplay::resized()
{
    auto bounds = getLocalBounds();

    // Position scrollbar at bottom
    m_scrollbar.setBounds(bounds.removeFromBottom(SCROLLBAR_HEIGHT));

    // Update scrollbar after layout
    updateScrollbar();
}

void WaveformDisplay::drawTimeRuler(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();

    // Background for ruler
    g.setColour(theme.ruler);
    g.fillRect(bounds);

    // Border
    g.setColour(theme.border);
    g.drawLine(bounds.getX(), bounds.getBottom(), bounds.getRight(), bounds.getBottom(), 1.0f);

    if (!m_fileLoaded)
    {
        return;
    }

    // Calculate time markers
    double visibleDuration = m_visibleEnd - m_visibleStart;
    double pixelsPerSecond = bounds.getWidth() / visibleDuration;

    // Determine marker interval based on zoom level
    double markerInterval = 1.0; // Default: 1 second
    if (pixelsPerSecond < 10)
    {
        markerInterval = 10.0;
    }
    else if (pixelsPerSecond < 50)
    {
        markerInterval = 5.0;
    }
    else if (pixelsPerSecond > 200)
    {
        markerInterval = 0.1;
    }
    else if (pixelsPerSecond > 100)
    {
        markerInterval = 0.5;
    }

    // Draw time markers
    g.setColour(waveedit::ThemeManager::getInstance().getCurrent().rulerText);
    g.setFont(12.0f);

    double firstMarker = std::ceil(m_visibleStart / markerInterval) * markerInterval;
    for (double time = firstMarker; time <= m_visibleEnd; time += markerInterval)
    {
        int x = timeToX(time);

        // Draw tick mark
        g.drawLine(x, bounds.getBottom() - 8, x, bounds.getBottom(), 1.0f);

        // Draw time label
        juce::String timeLabel;
        if (time >= 60.0)
        {
            int minutes = static_cast<int>(time / 60.0);
            double seconds = time - minutes * 60.0;
            timeLabel = juce::String::formatted("%d:%05.2f", minutes, seconds);
        }
        else
        {
            timeLabel = juce::String::formatted("%.2fs", time);
        }

        g.drawText(timeLabel, x - 30, bounds.getY() + 2, 60, bounds.getHeight() - 4,
                   juce::Justification::centred, true);
    }
}

void WaveformDisplay::drawChannelWaveform(juce::Graphics& g, juce::Rectangle<int> bounds, int channelNum)
{
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();

    // Channel background - slightly different shade for unfocused channels.
    // The "dimmer" variant is the theme's panel surface darkened by
    // ~25% so the relationship holds whether we're on Dark or Light.
    bool isFocused = isChannelFocused(channelNum);
    bool showFocusIndicator = (m_focusedChannels != -1);  // Only show when not all focused

    if (!isFocused && showFocusIndicator)
    {
        g.setColour(theme.waveformBackground.darker(0.4f));
    }
    else
    {
        g.setColour(theme.waveformBackground);
    }
    g.fillRect(bounds);

    // Draw focus indicator border for focused channels (when not all are focused)
    if (isFocused && showFocusIndicator)
    {
        g.setColour(theme.waveformBorder.withAlpha(0.6f));
        g.drawRect(bounds.reduced(1), 2);
    }

    // Center line
    g.setColour(theme.gridLine);
    g.drawLine(bounds.getX(), bounds.getCentreY(),
               bounds.getRight(), bounds.getCentreY(), 1.0f);

    // Draw waveform - use fast direct rendering if enabled, otherwise use thumbnail
    // Dim unfocused channels when single channel is focused
    float waveformAlpha = (isFocused || !showFocusIndicator) ? 1.0f : 0.4f;

    if (m_useDirectRendering)
    {
        // For direct rendering, we'll handle alpha in drawChannelWaveformDirect
        drawChannelWaveformDirect(g, bounds, channelNum);

        // Apply dimming overlay for unfocused channels
        if (!isFocused && showFocusIndicator)
        {
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.fillRect(bounds);
        }
    }
    else
    {
        juce::Colour waveformColor = getChannelWaveformColour(channelNum).withAlpha(waveformAlpha);
        g.setColour(waveformColor);
        m_thumbnail.drawChannel(g, bounds, m_visibleStart, m_visibleEnd, channelNum, 1.0f);
    }

    // Channel label with solo/mute indicators (for all channel counts)
    if (m_numChannels >= 1 && channelNum < 8)
    {
        // Store label bounds for hit testing
        juce::Rectangle<int> labelBounds(bounds.getX() + 2, bounds.getY() + 2,
                                          CHANNEL_LABEL_WIDTH, CHANNEL_LABEL_HEIGHT);
        m_channelLabelBounds[static_cast<size_t>(channelNum)] = labelBounds;

        // Check solo/mute/focus state
        bool isSolo = getChannelSolo ? getChannelSolo(channelNum) : false;
        bool isMute = getChannelMute ? getChannelMute(channelNum) : false;
        bool isChannelCurrentlyFocused = isChannelFocused(channelNum);
        bool showingFocusMode = (m_focusedChannels != -1);  // In per-channel focus mode

        // Draw background based on state priority: Solo > Mute > Focused > Default
        juce::Colour labelBgColor(0x00000000);  // Transparent by default
        juce::Colour labelTextColor = juce::Colours::grey;

        const auto& laneTheme = waveedit::ThemeManager::getInstance().getCurrent();
        if (isSolo)
        {
            // Solo — warning token (yellow / amber across themes)
            labelBgColor = laneTheme.warning.withAlpha(0.9f);
            labelTextColor = juce::Colours::white;
        }
        else if (isMute)
        {
            // Mute — error token (red across themes)
            labelBgColor = laneTheme.error.withAlpha(0.9f);
            labelTextColor = juce::Colours::white;
        }
        else if (showingFocusMode && isChannelCurrentlyFocused)
        {
            // Focused channel — accent token (blue-ish across themes)
            labelBgColor = laneTheme.accent.withAlpha(0.9f);
            labelTextColor = juce::Colours::white;
        }
        else if (showingFocusMode && !isChannelCurrentlyFocused)
        {
            // Dimmed label for unfocused channels
            labelTextColor = juce::Colours::grey.withAlpha(0.5f);
        }

        // Draw label background if not transparent
        if (labelBgColor.getAlpha() > 0)
        {
            g.setColour(labelBgColor);
            g.fillRoundedRectangle(labelBounds.toFloat(), 3.0f);
        }

        // Use ChannelLayout for proper labels
        waveedit::ChannelLayout layout = waveedit::ChannelLayout::fromChannelCount(m_numChannels);
        juce::String label = layout.getShortLabel(channelNum);

        // Fallback to generic numbering if no label defined
        if (label.isEmpty() || label == "?")
            label = "Ch " + juce::String(channelNum + 1);

        // Note: Channel state is indicated by background color:
        // - Yellow = Solo, Red = Mute, Blue = Focused, None = Default
        // No text suffix needed to avoid confusion with surround labels.

        // Draw label text
        g.setColour(labelTextColor);
        g.setFont(10.0f);
        g.drawText(label, labelBounds.reduced(3, 0),
                   juce::Justification::centredLeft, true);
    }
}

void WaveformDisplay::drawChannelWaveformDirect(juce::Graphics& g, juce::Rectangle<int> bounds, int channelNum)
{
    // PERFORMANCE-CRITICAL: Fast direct rendering from cached buffer
    // Achieves <10ms redraw by bypassing AudioThumbnail regeneration

    juce::ScopedLock lock(m_bufferLock);

    // Validate buffer
    if (m_cachedBuffer.getNumSamples() == 0 || channelNum >= m_cachedBuffer.getNumChannels())
    {
        return;
    }

    const float* channelData = m_cachedBuffer.getReadPointer(channelNum);
    const int totalSamples = m_cachedBuffer.getNumSamples();

    // Calculate visible sample range
    const int64_t startSample = static_cast<int64_t>(m_visibleStart * m_sampleRate);
    const int64_t endSample = static_cast<int64_t>(m_visibleEnd * m_sampleRate);
    const int64_t visibleSamples = endSample - startSample;

    if (visibleSamples <= 0)
    {
        return;
    }

    // Rendering parameters
    const int width = bounds.getWidth();
    const int height = bounds.getHeight();
    const float centerY = bounds.getCentreY();
    const float halfHeight = height * 0.5f;

    // Set waveform color (per-channel override falls back to global setting)
    g.setColour(getChannelWaveformColour(channelNum));

    // Calculate samples per pixel
    const double samplesPerPixel = static_cast<double>(visibleSamples) / width;

    // Fast rendering: for each pixel column, find min/max and draw vertical line
    for (int x = 0; x < width; ++x)
    {
        // Calculate sample range for this pixel
        const int64_t pixelStartSample = startSample + static_cast<int64_t>(x * samplesPerPixel);
        const int64_t pixelEndSample = startSample + static_cast<int64_t>((x + 1) * samplesPerPixel);

        // Clamp to buffer bounds
        const int64_t clampedStart = juce::jlimit<int64_t>(0, totalSamples - 1, pixelStartSample);
        const int64_t clampedEnd = juce::jlimit<int64_t>(clampedStart + 1, totalSamples, pixelEndSample);

        // Find min/max in this pixel's sample range
        float minSample = 0.0f;
        float maxSample = 0.0f;

        for (int64_t i = clampedStart; i < clampedEnd; ++i)
        {
            const float sample = channelData[i];
            minSample = juce::jmin(minSample, sample);
            maxSample = juce::jmax(maxSample, sample);
        }

        // Convert to pixel coordinates (flipped Y axis, -1.0 is top, +1.0 is bottom)
        const float minY = centerY - (maxSample * halfHeight);
        const float maxY = centerY - (minSample * halfHeight);

        // Draw vertical line for this pixel column
        g.drawLine(bounds.getX() + x, minY,
                   bounds.getX() + x, maxY,
                   1.0f);
    }
}

void WaveformDisplay::drawSelection(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    int startX = timeToX(m_selectionStart);
    int endX = timeToX(m_selectionEnd);

    // Clamp to visible area
    startX = juce::jmax(0, startX);
    endX = juce::jmin(getWidth(), endX);

    // Debug logging to verify coordinates
    if (endX <= startX)
    {
        // Selection is too small or outside visible range
        return;
    }

    // Create a clipped graphics context to ensure we only draw within bounds
    g.saveState();
    g.reduceClipRegion(bounds);

    // METHOD 1: Animated pulsing overlay for better visibility
    // Use animated alpha for subtle pulsing effect (Phase 1.2 requirement)
    g.setColour(juce::Colours::black.withAlpha(m_selectionAlpha)); // Black overlay with animated alpha
    g.fillRect(startX, bounds.getY(), endX - startX, bounds.getHeight());

    // METHOD 2: Inverted/brightened area using blend mode
    // Draw a bright accent color on the edges for clear boundaries
    g.setColour(juce::Colour(0x88FFFF00)); // Semi-transparent yellow highlight
    g.fillRect(startX, bounds.getY(), 3, bounds.getHeight()); // Left edge highlight
    g.fillRect(endX - 3, bounds.getY(), 3, bounds.getHeight()); // Right edge highlight

    // METHOD 3: Strong border lines for clear delineation (2.5px as per Phase 1.2)
    // Draw thick white borders with drop shadow effect
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawLine(startX - 1, bounds.getY(), startX - 1, bounds.getBottom(), 2.5f); // Shadow left
    g.drawLine(endX + 1, bounds.getY(), endX + 1, bounds.getBottom(), 2.5f); // Shadow right

    g.setColour(juce::Colours::white);
    g.drawLine(startX, bounds.getY(), startX, bounds.getBottom(), 2.5f); // Bright left edge
    g.drawLine(endX, bounds.getY(), endX, bounds.getBottom(), 2.5f); // Bright right edge

    // METHOD 4: Yellow corner handles for visual anchoring (Phase 1.2)
    g.setColour(juce::Colour(0xFFFFFF00)); // Bright yellow
    g.fillRect(startX - 3, bounds.getY(), 6, 12); // Top left handle
    g.fillRect(startX - 3, bounds.getBottom() - 12, 6, 12); // Bottom left handle
    g.fillRect(endX - 3, bounds.getY(), 6, 12); // Top right handle
    g.fillRect(endX - 3, bounds.getBottom() - 12, 6, 12); // Bottom right handle

    // METHOD 5: Duration label at center of selection (Phase 1.2)
    double duration = getSelectionDuration();
    juce::String durationLabel;
    int hours = static_cast<int>(duration / 3600.0);
    int minutes = static_cast<int>((duration - hours * 3600.0) / 60.0);
    double seconds = duration - hours * 3600.0 - minutes * 60.0;

    if (hours > 0)
    {
        durationLabel = juce::String::formatted("%dh %dm %.1fs", hours, minutes, seconds);
    }
    else if (minutes > 0)
    {
        durationLabel = juce::String::formatted("%dm %.1fs", minutes, seconds);
    }
    else
    {
        durationLabel = juce::String::formatted("%.2fs", duration);
    }

    // Draw label at center of selection
    int centerX = (startX + endX) / 2;
    int centerY = bounds.getCentreY();
    int labelWidth = 100;
    int labelHeight = 24;

    // Only draw label if selection is wide enough
    if (endX - startX > labelWidth)
    {
        // Draw label background with semi-transparent black
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.fillRoundedRectangle(centerX - labelWidth / 2.0f, centerY - labelHeight / 2.0f,
                               static_cast<float>(labelWidth), static_cast<float>(labelHeight), 4.0f);

        // Draw label text in bright yellow
        g.setColour(juce::Colour(0xFFFFFF00));
        g.setFont(13.0f);
        g.drawText(durationLabel, centerX - labelWidth / 2, centerY - labelHeight / 2,
                   labelWidth, labelHeight, juce::Justification::centred, true);
    }

    // Restore graphics state
    g.restoreState();
}

void WaveformDisplay::drawPlaybackCursor(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (m_playbackPosition < m_visibleStart || m_playbackPosition > m_visibleEnd)
    {
        return; // Cursor not visible
    }

    int x = timeToX(m_playbackPosition);

    // Draw playback cursor in GREEN (distinct from yellow edit cursor)
    g.setColour(juce::Colour(0xFF00FF00)); // Bright green for playback
    g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 2.0f);

    // Draw triangle at top
    juce::Path triangle;
    triangle.addTriangle(x - 5, bounds.getY(),
                        x + 5, bounds.getY(),
                        x, bounds.getY() + 8);
    g.fillPath(triangle);
}

void WaveformDisplay::drawPreviewCursor(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (!m_hasPreviewPosition)
    {
        return; // No preview position active
    }

    if (m_previewPosition < m_visibleStart || m_previewPosition > m_visibleEnd)
    {
        return; // Cursor not visible
    }

    int x = timeToX(m_previewPosition);

    // Draw preview cursor in ORANGE (distinct from green playback and yellow edit cursor)
    // This indicates DSP preview mode is active during processing dialog preview
    g.setColour(juce::Colour(0xFFFF8000)); // Bright orange for preview
    g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 2.0f);

    // Draw triangle at top (pointing down to indicate preview position)
    juce::Path triangle;
    triangle.addTriangle(x - 5, bounds.getY(),
                        x + 5, bounds.getY(),
                        x, bounds.getY() + 8);
    g.fillPath(triangle);
}

void WaveformDisplay::drawEditCursor(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (!m_hasEditCursor)
    {
        return; // No edit cursor active
    }

    if (m_editCursorPosition < m_visibleStart || m_editCursorPosition > m_visibleEnd)
    {
        return; // Cursor not visible
    }

    int x = timeToX(m_editCursorPosition);

    // Draw edit cursor in YELLOW (distinct from green playback cursor)
    g.setColour(juce::Colour(0xFFFFFF00)); // Bright yellow for edit cursor
    g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 2.5f); // Slightly thicker than playback

    // Draw triangle at top
    juce::Path triangle;
    triangle.addTriangle(x - 6, bounds.getY(),
                        x + 6, bounds.getY(),
                        x, bounds.getY() + 10);
    g.fillPath(triangle);

    // Draw time label above cursor
    juce::String timeLabel;
    double time = m_editCursorPosition;
    int hours = static_cast<int>(time / 3600.0);
    int minutes = static_cast<int>((time - hours * 3600.0) / 60.0);
    double seconds = time - hours * 3600.0 - minutes * 60.0;

    if (hours > 0)
    {
        timeLabel = juce::String::formatted("%02d:%02d:%06.3f", hours, minutes, seconds);
    }
    else if (minutes > 0)
    {
        timeLabel = juce::String::formatted("%02d:%06.3f", minutes, seconds);
    }
    else
    {
        timeLabel = juce::String::formatted("%.3fs", time);
    }

    // Draw label background
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    int labelWidth = 80;
    int labelHeight = 20;
    int labelX = x - labelWidth / 2;
    int labelY = bounds.getY() + 12; // Just below the triangle
    g.fillRect(labelX, labelY, labelWidth, labelHeight);

    // Draw label text
    g.setColour(juce::Colours::yellow);
    g.setFont(12.0f);
    g.drawText(timeLabel, labelX, labelY, labelWidth, labelHeight,
               juce::Justification::centred, true);
}

void WaveformDisplay::drawRegionOverlays(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    // Early exit if no RegionManager set
    if (m_regionManager == nullptr)
        return;

    // Early exit if no regions
    int numRegions = m_regionManager->getNumRegions();
    if (numRegions == 0)
        return;

    // Draw each region overlay
    for (int i = 0; i < numRegions; ++i)
    {
        const Region* region = m_regionManager->getRegion(i);
        if (region == nullptr)
            continue;

        // Convert region sample positions to time
        double startTime = static_cast<double>(region->getStartSample()) / m_sampleRate;
        double endTime = static_cast<double>(region->getEndSample()) / m_sampleRate;

        // Calculate pixel positions
        int startX = timeToX(startTime);
        int endX = timeToX(endTime);

        // Skip if region is completely outside visible range
        if (endX < 0 || startX > bounds.getWidth())
            continue;

        // Constrain to visible area
        startX = juce::jlimit(bounds.getX(), bounds.getRight(), startX);
        endX = juce::jlimit(bounds.getX(), bounds.getRight(), endX);

        int regionWidth = juce::jmax(1, endX - startX);

        // CRITICAL FIX: Use region's actual color (matches RegionDisplay rendering)
        // This ensures consistent color display across RegionDisplay bars and waveform overlays
        juce::Colour regionColor = region->getColor();

        // Draw region overlay (30% alpha fill)
        g.setColour(regionColor.withAlpha(0.3f));
        g.fillRect(startX, bounds.getY(), regionWidth, bounds.getHeight());

        // Draw region border (80% alpha for stronger outline)
        g.setColour(regionColor.withAlpha(0.8f));
        g.drawRect(startX, bounds.getY(), regionWidth, bounds.getHeight(), 1);
    }
}

