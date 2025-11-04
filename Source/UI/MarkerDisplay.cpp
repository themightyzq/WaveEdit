/*
 * WaveEdit by ZQ SFX
 * Copyright (C) 2025 ZQ SFX
 * Licensed under GPL v3
 *
 * MarkerDisplay.cpp
 */

#include "MarkerDisplay.h"

MarkerDisplay::MarkerDisplay(MarkerManager& markerManager)
    : m_markerManager(markerManager)
    , m_visibleStart(0.0)
    , m_visibleEnd(1.0)
    , m_sampleRate(44100.0)
    , m_totalDuration(1.0)
    , m_isDragging(false)
    , m_draggedMarkerIndex(-1)
    , m_originalMarkerPosition(0)
{
    setInterceptsMouseClicks(true, false);
}

void MarkerDisplay::paint(juce::Graphics& g)
{
    const int numMarkers = m_markerManager.getNumMarkers();
    if (numMarkers == 0)
        return;

    const int selectedIndex = m_markerManager.getSelectedMarkerIndex();

    // Draw all markers
    for (int i = 0; i < numMarkers; ++i)
    {
        const Marker* marker = m_markerManager.getMarker(i);
        if (!marker)
            continue;

        double markerTime = marker->getPositionInSeconds(m_sampleRate);

        // Skip if marker is outside visible range
        if (markerTime < m_visibleStart || markerTime > m_visibleEnd)
            continue;

        int x = timeToX(markerTime);
        bool isSelected = (i == selectedIndex);

        // Draw vertical line
        g.setColour(marker->getColor());
        int lineWidth = isSelected ? LINE_WIDTH_SELECTED : LINE_WIDTH_NORMAL;
        g.drawLine(static_cast<float>(x), 0.0f,
                   static_cast<float>(x), static_cast<float>(getHeight()),
                   static_cast<float>(lineWidth));

        // Draw label at top
        juce::String labelText = marker->getName();
        int labelWidth = g.getCurrentFont().getStringWidth(labelText) + 8;
        int labelX = x - labelWidth / 2;

        // Clamp label to visible area
        labelX = juce::jmax(0, juce::jmin(labelX, getWidth() - labelWidth));

        juce::Rectangle<int> labelBounds(labelX, 2, labelWidth, LABEL_HEIGHT);

        // Draw label background
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRect(labelBounds);

        // Draw label border if selected
        if (isSelected)
        {
            g.setColour(juce::Colours::white);
            g.drawRect(labelBounds, 1);
        }

        // Draw label text
        g.setColour(marker->getColor());
        g.drawText(labelText, labelBounds, juce::Justification::centred);
    }
}

void MarkerDisplay::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        // Right-click: Show context menu
        int markerIndex = findMarkerAtX(event.x, GRAB_TOLERANCE);
        if (markerIndex >= 0)
        {
            m_markerManager.setSelectedMarkerIndex(markerIndex);
            showContextMenu(markerIndex);
            repaint();
        }
        return;
    }

    // Left-click: Select and prepare for drag
    int markerIndex = findMarkerAtX(event.x, GRAB_TOLERANCE);

    if (markerIndex >= 0)
    {
        m_markerManager.setSelectedMarkerIndex(markerIndex);

        // Prepare for drag
        m_isDragging = false;  // Not dragging yet (wait for mouseDrag)
        m_draggedMarkerIndex = markerIndex;

        const Marker* marker = m_markerManager.getMarker(markerIndex);
        if (marker)
            m_originalMarkerPosition = marker->getPosition();

        // Notify callback
        if (onMarkerClicked)
            onMarkerClicked(markerIndex);

        repaint();
    }
    else
    {
        // Click on empty space: Clear selection
        m_markerManager.clearSelection();
        repaint();
    }
}

void MarkerDisplay::mouseDoubleClick(const juce::MouseEvent& event)
{
    int markerIndex = findMarkerAtX(event.x, GRAB_TOLERANCE);

    if (markerIndex >= 0)
    {
        m_markerManager.setSelectedMarkerIndex(markerIndex);

        // Notify callback
        if (onMarkerDoubleClicked)
            onMarkerDoubleClicked(markerIndex);

        repaint();
    }
}

void MarkerDisplay::mouseDrag(const juce::MouseEvent& event)
{
    if (m_draggedMarkerIndex < 0)
        return;

    m_isDragging = true;

    // Convert mouse X to sample position
    int64_t newPosition = xToSample(event.x);

    // Clamp to valid range [0, totalDuration]
    int64_t maxSample = static_cast<int64_t>(m_totalDuration * m_sampleRate);
    newPosition = juce::jlimit((int64_t)0, maxSample, newPosition);

    // Update marker position
    Marker* marker = m_markerManager.getMarker(m_draggedMarkerIndex);
    if (marker)
    {
        marker->setPosition(newPosition);

        // Notify callback for real-time feedback
        if (onMarkerMoving)
            onMarkerMoving();

        repaint();
    }
}

void MarkerDisplay::mouseUp(const juce::MouseEvent& event)
{
    if (m_isDragging && m_draggedMarkerIndex >= 0)
    {
        // Drag complete: Notify callback with final position
        const Marker* marker = m_markerManager.getMarker(m_draggedMarkerIndex);
        if (marker && onMarkerMoved)
        {
            int64_t newPosition = marker->getPosition();
            if (newPosition != m_originalMarkerPosition)
            {
                onMarkerMoved(m_draggedMarkerIndex, m_originalMarkerPosition, newPosition);
            }
        }

        m_isDragging = false;
        m_draggedMarkerIndex = -1;
        repaint();
    }
}

void MarkerDisplay::mouseMove(const juce::MouseEvent& event)
{
    // Change cursor when hovering over marker
    int markerIndex = findMarkerAtX(event.x, GRAB_TOLERANCE);

    if (markerIndex >= 0)
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);
}

void MarkerDisplay::setVisibleRange(double startTime, double endTime)
{
    m_visibleStart = startTime;
    m_visibleEnd = endTime;
    repaint();
}

void MarkerDisplay::setSampleRate(double sampleRate)
{
    m_sampleRate = sampleRate;
    repaint();
}

void MarkerDisplay::setTotalDuration(double duration)
{
    m_totalDuration = duration;
}

int MarkerDisplay::timeToX(double timeInSeconds) const
{
    if (m_visibleEnd <= m_visibleStart)
        return 0;

    double ratio = (timeInSeconds - m_visibleStart) / (m_visibleEnd - m_visibleStart);
    return static_cast<int>(ratio * getWidth());
}

double MarkerDisplay::xToTime(int x) const
{
    if (getWidth() <= 0)
        return 0.0;

    double ratio = static_cast<double>(x) / getWidth();
    return m_visibleStart + ratio * (m_visibleEnd - m_visibleStart);
}

int64_t MarkerDisplay::xToSample(int x) const
{
    double timeInSeconds = xToTime(x);
    return static_cast<int64_t>(timeInSeconds * m_sampleRate);
}

int MarkerDisplay::findMarkerAtX(int x, int tolerance) const
{
    const int numMarkers = m_markerManager.getNumMarkers();

    for (int i = 0; i < numMarkers; ++i)
    {
        const Marker* marker = m_markerManager.getMarker(i);
        if (!marker)
            continue;

        double markerTime = marker->getPositionInSeconds(m_sampleRate);
        int markerX = timeToX(markerTime);

        if (std::abs(x - markerX) <= tolerance)
            return i;
    }

    return -1;
}

void MarkerDisplay::showContextMenu(int markerIndex)
{
    const Marker* marker = m_markerManager.getMarker(markerIndex);
    if (!marker)
        return;

    juce::PopupMenu menu;

    menu.addItem(1, "Rename Marker...");
    menu.addItem(2, "Change Color...");
    menu.addSeparator();
    menu.addItem(3, "Delete Marker", true);

    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this, markerIndex](int result)
        {
            switch (result)
            {
                case 1:  // Rename
                    if (onMarkerDoubleClicked)
                        onMarkerDoubleClicked(markerIndex);
                    break;

                case 2:  // Change color
                    showColorPicker(markerIndex);
                    break;

                case 3:  // Delete
                    if (onMarkerDeleted)
                        onMarkerDeleted(markerIndex);
                    break;

                default:
                    break;
            }
        });
}

void MarkerDisplay::showColorPicker(int markerIndex)
{
    Marker* marker = m_markerManager.getMarker(markerIndex);
    if (!marker)
        return;

    // Show color picker with preset colors
    juce::PopupMenu menu;

    // Preset colors (16 common marker colors) - CRITICAL FIX: Made static to avoid dangling reference in async callback
    struct ColorOption
    {
        juce::String name;
        juce::Colour color;
    };

    static const ColorOption colors[] = {
        {"Yellow", juce::Colours::yellow},
        {"Light Blue", juce::Colour(0xff87ceeb)},
        {"Light Green", juce::Colour(0xff90ee90)},
        {"Light Coral", juce::Colour(0xfff08080)},
        {"Light Pink", juce::Colour(0xffffb6c1)},
        {"Light Cyan", juce::Colour(0xffe0ffff)},
        {"Light Orange", juce::Colours::orange.brighter()},
        {"Light Purple", juce::Colour(0xffdda0dd)},
        {"Red", juce::Colours::red},
        {"Green", juce::Colours::green},
        {"Blue", juce::Colours::blue},
        {"Orange", juce::Colours::orange},
        {"Purple", juce::Colours::purple},
        {"Cyan", juce::Colours::cyan},
        {"Magenta", juce::Colours::magenta},
        {"White", juce::Colours::white}
    };

    juce::Colour currentColor = marker->getColor();

    for (int i = 0; i < 16; ++i)
    {
        bool isCurrentColor = (colors[i].color == currentColor);
        menu.addItem(i + 1, colors[i].name, true, isCurrentColor);
    }

    // Note: Custom color picker removed for Phase 1 - can be added later with proper juce_gui_extra module integration

    // CRITICAL FIX: No need to capture static variable - it's always available
    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this, markerIndex, currentColor](int result)
        {
            if (result > 0 && result <= 16)
            {
                // Preset color selected - safe to access static colors array
                juce::Colour newColor = colors[result - 1].color;
                if (newColor != currentColor && onMarkerColorChanged)
                {
                    onMarkerColorChanged(markerIndex, newColor);
                }
            }
        });
}
