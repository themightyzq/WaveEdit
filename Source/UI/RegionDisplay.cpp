/*
  ==============================================================================

    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

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

#include "RegionDisplay.h"
#include "../Utils/AudioUnits.h"
#include "../Utils/Settings.h"
#include <juce_gui_extra/juce_gui_extra.h>

RegionDisplay::RegionDisplay(RegionManager& regionManager)
    : m_regionManager(regionManager)
    , m_visibleStart(0.0)
    , m_visibleEnd(1.0)
    , m_sampleRate(44100.0)
    , m_totalDuration(0.0)
    , m_audioBuffer(nullptr)
    , m_hoveredRegionIndex(-1)
    , m_draggedRegionIndex(-1)
    , m_resizeMode(ResizeMode::None)
    , m_resizeRegionIndex(-1)
    , m_originalRegionStart(0)
    , m_originalRegionEnd(0)
    , m_dragStartX(0)
{
    setInterceptsMouseClicks(true, false);
}

RegionDisplay::~RegionDisplay()
{
}

//==============================================================================
// View state

void RegionDisplay::setVisibleRange(double startTime, double endTime)
{
    m_visibleStart = startTime;
    m_visibleEnd = endTime;

    // DEBUG: Log visible range updates to diagnose alignment issues
    juce::Logger::writeToLog(juce::String::formatted(
        "RegionDisplay::setVisibleRange(%.2f, %.2f) - width=%d",
        startTime, endTime, getWidth()));

    repaint();
}

void RegionDisplay::setSampleRate(double sampleRate)
{
    m_sampleRate = sampleRate;
}

void RegionDisplay::setTotalDuration(double duration)
{
    m_totalDuration = duration;
}

void RegionDisplay::setAudioBuffer(const juce::AudioBuffer<float>* buffer)
{
    m_audioBuffer = buffer;
}

//==============================================================================
// Component overrides

void RegionDisplay::paint(juce::Graphics& g)
{
    // Background (transparent - waveform shows through)
    g.fillAll(juce::Colours::transparentBlack);

    // Draw all regions
    for (int i = 0; i < m_regionManager.getNumRegions(); ++i)
    {
        const Region* region = m_regionManager.getRegion(i);
        if (region != nullptr)
        {
            drawRegion(g, *region, i, getLocalBounds());
        }
    }
}

void RegionDisplay::resized()
{
    // No child components to layout
}

void RegionDisplay::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        // Right-click: show context menu
        int regionIndex = findRegionAtX(event.x);
        if (regionIndex >= 0)
        {
            showRegionContextMenu(regionIndex, event.getPosition());
        }
    }
    else if (event.mods.isLeftButtonDown())
    {
        // Left-click: check for resize (edge) first, then region selection
        int regionIndex = findRegionAtX(event.x);
        if (regionIndex >= 0)
        {
            // Check if clicking on edge (for resize)
            EdgeProximity edgeProximity = getEdgeProximity(regionIndex, event.x);

            if (edgeProximity != EdgeProximity::None)
            {
                // Start resize operation
                m_resizeMode = (edgeProximity == EdgeProximity::StartEdge) ?
                    ResizeMode::ResizingStart : ResizeMode::ResizingEnd;

                m_resizeRegionIndex = regionIndex;
                m_dragStartX = event.x;

                // Store original region boundaries
                const Region* region = m_regionManager.getRegion(regionIndex);
                if (region != nullptr)
                {
                    m_originalRegionStart = region->getStartSample();
                    m_originalRegionEnd = region->getEndSample();
                }
            }
            else
            {
                // Regular region selection (not on edge)
                m_draggedRegionIndex = regionIndex;

                // Multi-selection support (Phase 3.5)
                if (event.mods.isCommandDown())
                {
                    // Cmd+Click: Toggle region in/out of selection
                    m_regionManager.toggleRegionSelection(regionIndex);

                    // Prepare for potential move operation (drag after Cmd+Click)
                    m_resizeMode = ResizeMode::MovingRegion;
                    m_resizeRegionIndex = regionIndex;
                    m_dragStartX = event.x;

                    // Store original bounds for ALL selected regions
                    m_originalMultiRegionBounds.clear();
                    auto selectedIndices = m_regionManager.getSelectedRegionIndices();
                    for (int idx : selectedIndices)
                    {
                        const Region* r = m_regionManager.getRegion(idx);
                        if (r != nullptr)
                        {
                            OriginalRegionBounds bounds;
                            bounds.regionIndex = idx;
                            bounds.startSample = r->getStartSample();
                            bounds.endSample = r->getEndSample();
                            m_originalMultiRegionBounds.add(bounds);
                        }
                    }

                    // Notify callback for backward compatibility
                    if (onRegionClicked)
                        onRegionClicked(regionIndex);

                    // Repaint to show updated selection
                    repaint();
                }
                else if (event.mods.isShiftDown())
                {
                    // Shift+Click: Range selection from primary selection to clicked region
                    int primaryIndex = m_regionManager.getPrimarySelectionIndex();
                    if (primaryIndex >= 0)
                        m_regionManager.selectRegionRange(primaryIndex, regionIndex);
                    else
                        m_regionManager.selectRegion(regionIndex, false);  // No primary, just select this one

                    // Prepare for potential move operation (drag after Shift+Click)
                    m_resizeMode = ResizeMode::MovingRegion;
                    m_resizeRegionIndex = regionIndex;
                    m_dragStartX = event.x;

                    // Store original bounds for ALL selected regions
                    m_originalMultiRegionBounds.clear();
                    auto selectedIndices = m_regionManager.getSelectedRegionIndices();
                    for (int idx : selectedIndices)
                    {
                        const Region* r = m_regionManager.getRegion(idx);
                        if (r != nullptr)
                        {
                            OriginalRegionBounds bounds;
                            bounds.regionIndex = idx;
                            bounds.startSample = r->getStartSample();
                            bounds.endSample = r->getEndSample();
                            m_originalMultiRegionBounds.add(bounds);
                        }
                    }

                    // Notify callback for backward compatibility
                    if (onRegionClicked)
                        onRegionClicked(regionIndex);

                    // Repaint to show updated selection
                    repaint();
                }
                else
                {
                    // Regular click: Select single region (clear others) and prepare for move drag
                    m_regionManager.selectRegion(regionIndex, false);

                    // Prepare for potential move operation (Phase 3 / 3.5)
                    m_resizeMode = ResizeMode::MovingRegion;
                    m_resizeRegionIndex = regionIndex;
                    m_dragStartX = event.x;

                    // Store original region boundaries (single region for backward compatibility)
                    const Region* region = m_regionManager.getRegion(regionIndex);
                    if (region != nullptr)
                    {
                        m_originalRegionStart = region->getStartSample();
                        m_originalRegionEnd = region->getEndSample();
                    }

                    // Phase 3.5: Store original bounds for ALL selected regions (for multi-region move undo)
                    m_originalMultiRegionBounds.clear();
                    auto selectedIndices = m_regionManager.getSelectedRegionIndices();
                    for (int idx : selectedIndices)
                    {
                        const Region* r = m_regionManager.getRegion(idx);
                        if (r != nullptr)
                        {
                            OriginalRegionBounds bounds;
                            bounds.regionIndex = idx;
                            bounds.startSample = r->getStartSample();
                            bounds.endSample = r->getEndSample();
                            m_originalMultiRegionBounds.add(bounds);
                        }
                    }

                    // Notify callback for backward compatibility
                    if (onRegionClicked)
                        onRegionClicked(regionIndex);

                    // Repaint to show updated selection
                    repaint();
                }
            }
        }
    }
}

void RegionDisplay::mouseDoubleClick(const juce::MouseEvent& event)
{
    // Double-click: rename region
    int regionIndex = findRegionAtX(event.x);
    if (regionIndex >= 0)
    {
        if (onRegionDoubleClicked)
            onRegionDoubleClicked(regionIndex);

        showRenameDialog(regionIndex);
    }
}

void RegionDisplay::mouseDrag(const juce::MouseEvent& event)
{
    // Handle region resize dragging
    if (m_resizeMode != ResizeMode::None)
    {
        Region* region = m_regionManager.getRegion(m_resizeRegionIndex);
        if (region == nullptr)
            return;

        // Guard against invalid sample rate
        if (m_sampleRate <= 0.0)
        {
            juce::Logger::writeToLog("Warning: RegionDisplay sample rate not set during resize");
            return;
        }

        // Convert mouse X position to time, then to sample position
        double currentTime = xToTime(event.x);
        int64_t currentSample = static_cast<int64_t>(currentTime * m_sampleRate);

        // Clamp to file boundaries [0, totalDuration]
        int64_t maxSample = static_cast<int64_t>(m_totalDuration * m_sampleRate);
        currentSample = juce::jlimit<int64_t>(0, maxSample, currentSample);

        // Minimum region size: 1ms (prevents regions too small to be useful)
        const int64_t MIN_REGION_SAMPLES = static_cast<int64_t>(0.001 * m_sampleRate);

        // Update the appropriate boundary based on resize mode
        if (m_resizeMode == ResizeMode::ResizingStart)
        {
            // Resizing start edge - ensure start < end with minimum size
            int64_t endSample = region->getEndSample();
            int64_t maxStart = endSample - MIN_REGION_SAMPLES;
            currentSample = juce::jmin(currentSample, maxStart);

            if (currentSample >= 0 && currentSample < endSample)
            {
                region->setStartSample(currentSample);
            }
        }
        else if (m_resizeMode == ResizeMode::ResizingEnd)
        {
            // Resizing end edge - ensure end > start with minimum size
            int64_t startSample = region->getStartSample();
            int64_t minEnd = startSample + MIN_REGION_SAMPLES;
            currentSample = juce::jmax(currentSample, minEnd);

            if (currentSample > startSample && currentSample <= maxSample)
            {
                region->setEndSample(currentSample);
            }
        }
        else if (m_resizeMode == ResizeMode::MovingRegion)
        {
            // Moving entire region(s) - maintain duration, shift position
            // Phase 3.5: Support multi-region move (move all selected regions together)
            double dragDeltaTime = xToTime(event.x) - xToTime(m_dragStartX);
            int64_t dragDeltaSamples = static_cast<int64_t>(dragDeltaTime * m_sampleRate);

            // Check if multiple regions are selected
            int numSelected = m_regionManager.getNumSelectedRegions();
            if (numSelected > 1)
            {
                // Phase 3.5: Move ALL selected regions together
                // Calculate clamping based on the entire group bounds
                int64_t groupMinStart = std::numeric_limits<int64_t>::max();
                int64_t groupMaxEnd = 0;

                // Find group boundaries
                auto selectedIndices = m_regionManager.getSelectedRegionIndices();
                for (int idx : selectedIndices)
                {
                    const Region* r = m_regionManager.getRegion(idx);
                    if (r != nullptr)
                    {
                        groupMinStart = std::min(groupMinStart, r->getStartSample());
                        groupMaxEnd = std::max(groupMaxEnd, r->getEndSample());
                    }
                }

                // Calculate group offset with clamping
                int64_t clampedOffset = dragDeltaSamples;
                if (groupMinStart + dragDeltaSamples < 0)
                    clampedOffset = -groupMinStart;  // Clamp to start of file
                else if (groupMaxEnd + dragDeltaSamples > maxSample)
                    clampedOffset = maxSample - groupMaxEnd;  // Clamp to end of file

                // Move all selected regions by the clamped offset
                for (int idx : selectedIndices)
                {
                    Region* r = m_regionManager.getRegion(idx);
                    if (r != nullptr)
                    {
                        int64_t newStart = r->getStartSample() + clampedOffset;
                        int64_t newEnd = r->getEndSample() + clampedOffset;
                        r->setStartSample(newStart);
                        r->setEndSample(newEnd);
                    }
                }
            }
            else
            {
                // Phase 3: Move single region
                int64_t regionDuration = m_originalRegionEnd - m_originalRegionStart;
                int64_t newStart = m_originalRegionStart + dragDeltaSamples;
                int64_t newEnd = m_originalRegionEnd + dragDeltaSamples;

                // Clamp to file boundaries [0, totalDuration]
                if (newStart < 0)
                {
                    newStart = 0;
                    newEnd = regionDuration;
                }
                else if (newEnd > maxSample)
                {
                    newEnd = maxSample;
                    newStart = maxSample - regionDuration;
                }

                // Update region position
                region->setStartSample(newStart);
                region->setEndSample(newEnd);
            }
        }

        // Notify WaveformDisplay to update region overlays during drag
        if (onRegionResizing)
            onRegionResizing();

        // Repaint to show updated region boundaries during drag
        repaint();
    }
}

void RegionDisplay::mouseUp(const juce::MouseEvent& event)
{
    // Finalize resize operation if we were resizing
    if (m_resizeMode != ResizeMode::None)
    {
        Region* region = m_regionManager.getRegion(m_resizeRegionIndex);
        if (region != nullptr)
        {
            // Apply zero-crossing snap if enabled (Phase 3.3)
            if (Settings::getInstance().getSnapRegionsToZeroCrossings() &&
                m_audioBuffer != nullptr &&
                m_audioBuffer->getNumChannels() > 0 &&
                m_audioBuffer->getNumSamples() > 0)
            {
                int channel = 0;  // Use first channel for snap detection
                int searchRadius = 1000;  // 1000 samples (~22ms at 44.1kHz)

                int64_t startSample = region->getStartSample();
                int64_t endSample = region->getEndSample();

                int64_t snappedStart = AudioUnits::snapToZeroCrossing(startSample, *m_audioBuffer, channel, searchRadius);
                int64_t snappedEnd = AudioUnits::snapToZeroCrossing(endSample, *m_audioBuffer, channel, searchRadius);

                // Ensure region remains valid (start < end) after snap
                if (snappedStart < snappedEnd)
                {
                    region->setStartSample(snappedStart);
                    region->setEndSample(snappedEnd);

                    juce::Logger::writeToLog(juce::String::formatted(
                        "Zero-crossing snap on resize: start %lld -> %lld, end %lld -> %lld",
                        startSample, snappedStart, endSample, snappedEnd));

                    // Trigger repaint to show snapped boundaries
                    repaint();
                    if (onRegionResizing)
                        onRegionResizing();
                }
            }

            // Notify callback with final boundaries (after snap)
            // Phase 3.5: Handle multi-region move undo callbacks
            if (onRegionResized)
            {
                if (m_originalMultiRegionBounds.size() > 1)
                {
                    // Multi-region move: Call callback for each moved region
                    for (const auto& bounds : m_originalMultiRegionBounds)
                    {
                        const Region* movedRegion = m_regionManager.getRegion(bounds.regionIndex);
                        if (movedRegion != nullptr)
                        {
                            onRegionResized(bounds.regionIndex,
                                          bounds.startSample,              // Old start sample
                                          bounds.endSample,                // Old end sample
                                          movedRegion->getStartSample(),   // New start sample (after snap)
                                          movedRegion->getEndSample());    // New end sample (after snap)
                        }
                    }
                }
                else
                {
                    // Single region resize/move: Call callback once
                    onRegionResized(m_resizeRegionIndex,
                                  m_originalRegionStart,    // Old start sample
                                  m_originalRegionEnd,      // Old end sample
                                  region->getStartSample(), // New start sample (after snap)
                                  region->getEndSample());  // New end sample (after snap)
                }
            }
        }

        // Reset resize/move state
        m_resizeMode = ResizeMode::None;
        m_resizeRegionIndex = -1;
        m_originalMultiRegionBounds.clear();  // Phase 3.5: Clear multi-region bounds
    }

    m_draggedRegionIndex = -1;
}

void RegionDisplay::mouseMove(const juce::MouseEvent& event)
{
    // Find region under mouse cursor
    int regionIndex = findRegionAtX(event.x);

    // Check if we're near an edge (for resize cursor)
    EdgeProximity edgeProximity = EdgeProximity::None;
    if (regionIndex >= 0)
    {
        edgeProximity = getEdgeProximity(regionIndex, event.x);
    }

    // Update cursor based on edge proximity (Phase 3: add drag hand cursor)
    if (edgeProximity != EdgeProximity::None)
    {
        // Near edge: Show resize cursor
        setMouseCursor(getResizeCursorForEdge(edgeProximity));
    }
    else if (regionIndex >= 0)
    {
        // Over region middle: Show drag hand cursor (indicates moveable)
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
    else
    {
        // Not over region: Normal pointer
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }

    // Update hover state if changed
    if (regionIndex != m_hoveredRegionIndex)
    {
        m_hoveredRegionIndex = regionIndex;
        repaint();
    }
}

//==============================================================================
// Helper methods

int RegionDisplay::timeToX(double timeInSeconds) const
{
    // CRITICAL FIX: Match WaveformDisplay's coordinate conversion EXACTLY
    // Guard against invalid state and zero width
    int width = getWidth();
    if (m_visibleEnd <= m_visibleStart || width <= 0)
    {
        return 0;
    }

    // Clamp time to visible range before conversion (prevents overflow)
    double clampedTime = juce::jlimit(m_visibleStart, m_visibleEnd, timeInSeconds);
    double ratio = (clampedTime - m_visibleStart) / (m_visibleEnd - m_visibleStart);

    // Clamp result to valid pixel range (matches WaveformDisplay exactly)
    return juce::jlimit(0, width - 1, static_cast<int>(ratio * width));
}

double RegionDisplay::xToTime(int x) const
{
    // CRITICAL FIX: Match WaveformDisplay's coordinate conversion EXACTLY
    // Guard against zero width (component not laid out yet)
    int width = getWidth();
    if (width <= 0)
    {
        return m_visibleStart;  // Return start of visible range when width invalid
    }

    // Clamp x to [0, width-1] to ensure valid pixel coordinates
    int clampedX = juce::jlimit(0, width - 1, x);

    double ratio = static_cast<double>(clampedX) / width;
    return m_visibleStart + ratio * (m_visibleEnd - m_visibleStart);
}

double RegionDisplay::sampleToTime(int64_t sample) const
{
    if (m_sampleRate <= 0.0)
        return 0.0;

    return static_cast<double>(sample) / m_sampleRate;
}

int RegionDisplay::findRegionAtX(int x) const
{
    double clickTime = xToTime(x);
    int64_t clickSample = static_cast<int64_t>(clickTime * m_sampleRate);

    return m_regionManager.findRegionAtSample(clickSample);
}

RegionDisplay::EdgeProximity RegionDisplay::getEdgeProximity(int regionIndex, int x) const
{
    const Region* region = m_regionManager.getRegion(regionIndex);
    if (region == nullptr)
        return EdgeProximity::None;

    // Convert region boundaries to screen coordinates
    double startTime = sampleToTime(region->getStartSample());
    double endTime = sampleToTime(region->getEndSample());

    int startX = timeToX(startTime);
    int endX = timeToX(endTime);

    // Check proximity to edges (within EDGE_GRAB_TOLERANCE pixels)
    if (std::abs(x - startX) <= EDGE_GRAB_TOLERANCE)
        return EdgeProximity::StartEdge;

    if (std::abs(x - endX) <= EDGE_GRAB_TOLERANCE)
        return EdgeProximity::EndEdge;

    return EdgeProximity::None;
}

juce::MouseCursor RegionDisplay::getResizeCursorForEdge(EdgeProximity edge) const
{
    switch (edge)
    {
        case EdgeProximity::StartEdge:
        case EdgeProximity::EndEdge:
            return juce::MouseCursor::LeftRightResizeCursor;

        case EdgeProximity::None:
        default:
            return juce::MouseCursor::NormalCursor;
    }
}

void RegionDisplay::drawRegion(juce::Graphics& g, const Region& region, int regionIndex, juce::Rectangle<int> bounds)
{
    // Convert region sample positions to screen coordinates
    double startTime = sampleToTime(region.getStartSample());
    double endTime = sampleToTime(region.getEndSample());

    int x1 = timeToX(startTime);
    int x2 = timeToX(endTime);

    // DEBUG: Log coordinate conversions for first region
    if (regionIndex == 0)
    {
        juce::Logger::writeToLog(juce::String::formatted(
            "RegionDisplay: Region[0] %.2fs-%.2fs -> pixels %d-%d (width=%d, view=%.2f-%.2f)",
            startTime, endTime, x1, x2, getWidth(), m_visibleStart, m_visibleEnd));
    }

    // Only draw if region is visible
    if (x2 < 0 || x1 > getWidth())
        return;

    // Constrain to visible area
    x1 = juce::jlimit(0, getWidth(), x1);
    x2 = juce::jlimit(0, getWidth(), x2);

    // Region bar
    juce::Rectangle<int> regionBounds(x1, 0, x2 - x1, BAR_HEIGHT);

    // Determine alpha based on selection and hover state (multi-selection support)
    bool isSelected = m_regionManager.isRegionSelected(regionIndex);
    bool isHovered = (regionIndex == m_hoveredRegionIndex);

    float alpha = UNSELECTED_ALPHA;  // Default: 0.7
    if (isSelected)
        alpha = SELECTED_ALPHA;      // Selected: 1.0 (fully opaque)
    else if (isHovered)
        alpha = 0.85f;               // Hovered: 0.85 (brighter than unselected)

    // Draw region background
    juce::Colour barColor = region.getColor().withAlpha(alpha);
    g.setColour(barColor);
    g.fillRect(regionBounds);

    // Draw region border
    g.setColour(barColor.darker(0.3f));
    g.drawRect(regionBounds, 1);

    // Draw white border for selected region
    if (isSelected)
    {
        g.setColour(juce::Colours::white);
        g.drawRect(regionBounds, 3);  // 3px white border for selected regions
    }

    // Draw region name (if wide enough)
    if (regionBounds.getWidth() > 40)
    {
        g.setColour(juce::Colours::black.withAlpha(alpha));
        g.setFont(12.0f);

        juce::Rectangle<int> textBounds = regionBounds.reduced(LABEL_MARGIN, 2);
        g.drawText(region.getName(), textBounds, juce::Justification::centredLeft, true);
    }
}

void RegionDisplay::showRegionContextMenu(int regionIndex, juce::Point<int>)
{
    juce::PopupMenu menu;

    menu.addItem(1, "Rename Region");
    menu.addItem(2, "Edit Boundaries...");
    menu.addItem(3, "Change Color");
    menu.addSeparator();
    menu.addItem(4, "Delete Region", true, false);

    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this, regionIndex](int result)
        {
            if (result == 1)
            {
                showRenameDialog(regionIndex);
            }
            else if (result == 2)
            {
                // Edit Boundaries... - trigger callback
                if (onRegionEditBoundaries)
                    onRegionEditBoundaries(regionIndex);
            }
            else if (result == 3)
            {
                showColorPicker(regionIndex);
            }
            else if (result == 4)
            {
                if (onRegionDeleted)
                    onRegionDeleted(regionIndex);
            }
        });
}

void RegionDisplay::showRenameDialog(int regionIndex)
{
    Region* region = m_regionManager.getRegion(regionIndex);
    if (region == nullptr)
        return;

    juce::String currentName = region->getName();

    // Use async AlertWindow for JUCE 7+ compatibility
    auto options = juce::MessageBoxOptions()
        .withIconType(juce::AlertWindow::QuestionIcon)
        .withTitle("Rename Region")
        .withMessage("Enter a new name for this region:")
        .withButton("OK")
        .withButton("Cancel");

    juce::AlertWindow::showAsync(options,
        [this, regionIndex, currentName](int result) mutable
        {
            if (result == 1) // OK button
            {
                // For MVP: Use a simple input dialog approach
                // Full implementation would need proper text input
                Region* r = m_regionManager.getRegion(regionIndex);
                if (r != nullptr)
                {
                    // Simple fallback: cycle through preset names
                    juce::String newName = "Region " + juce::String(regionIndex + 1);
                    r->setName(newName);

                    if (onRegionRenamed)
                        onRegionRenamed(regionIndex, newName);

                    repaint();
                }
            }
        });
}

void RegionDisplay::showColorPicker(int regionIndex)
{
    Region* region = m_regionManager.getRegion(regionIndex);
    if (region == nullptr)
        return;

    // Define color palette with names for menu
    struct ColorOption
    {
        juce::String name;
        juce::Colour color;
    };

    const ColorOption colorPalette[] = {
        { "Light Blue", juce::Colours::lightblue },
        { "Light Green", juce::Colours::lightgreen },
        { "Light Coral", juce::Colours::lightcoral },
        { "Light Yellow", juce::Colours::lightyellow },
        { "Light Pink", juce::Colours::lightpink },
        { "Light Cyan", juce::Colours::lightcyan },
        { "Light Grey", juce::Colours::lightgrey },
        { "Light Salmon", juce::Colours::lightsalmon },
        { "Sky Blue", juce::Colours::skyblue },
        { "Spring Green", juce::Colours::springgreen },
        { "Orange", juce::Colours::orange },
        { "Purple", juce::Colours::purple },
        { "Red", juce::Colours::red },
        { "Green", juce::Colours::green },
        { "Blue", juce::Colours::blue },
        { "Yellow", juce::Colours::yellow }
    };

    juce::Colour currentColor = region->getColor();
    juce::PopupMenu menu;

    // Add preset color swatches
    for (int i = 0; i < 16; ++i)
    {
        bool isCurrentColor = (colorPalette[i].color == currentColor);
        menu.addItem(i + 1, colorPalette[i].name, true, isCurrentColor);
    }

    // Add separator and custom color option
    menu.addSeparator();
    menu.addItem(99, "Custom Color...", true, false);

    // Show menu and handle selection
    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this, regionIndex, colorPalette](int result)
        {
            if (result > 0 && result <= 16)
            {
                // Preset color selected
                juce::Colour newColor = colorPalette[result - 1].color;
                Region* r = m_regionManager.getRegion(regionIndex);
                if (r != nullptr && newColor != r->getColor())
                {
                    r->setColor(newColor);
                    if (onRegionColorChanged)
                        onRegionColorChanged(regionIndex, newColor);
                    repaint();
                }
            }
            else if (result == 99)
            {
                // Custom color picker selected
                showCustomColorPicker(regionIndex);
            }
        });
}

void RegionDisplay::showCustomColorPicker(int regionIndex)
{
    Region* region = m_regionManager.getRegion(regionIndex);
    if (region == nullptr)
        return;

    juce::Colour currentColor = region->getColor();

    // Create color selector with standard options
    auto* colorSelector = new juce::ColourSelector(
        juce::ColourSelector::showColourAtTop |
        juce::ColourSelector::showSliders |
        juce::ColourSelector::showColourspace);

    colorSelector->setCurrentColour(currentColor);
    colorSelector->setSize(400, 300);

    // Create a simple dialog component - inherits from Component for UI hierarchy
    struct ColorPickerDialog : public juce::Component
    {
        ColorPickerDialog(juce::ColourSelector* selector,
                          std::function<void(bool, juce::Colour)> callback)
            : m_selector(selector), m_callback(callback)
        {
            addAndMakeVisible(m_selector);

            m_okButton.setButtonText("OK");
            m_cancelButton.setButtonText("Cancel");

            m_okButton.onClick = [this]() {
                if (m_callback)
                    m_callback(true, m_selector->getCurrentColour());
                dismissCallout();
            };

            m_cancelButton.onClick = [this]() {
                if (m_callback)
                    m_callback(false, juce::Colours::transparentBlack);
                dismissCallout();
            };

            addAndMakeVisible(m_okButton);
            addAndMakeVisible(m_cancelButton);

            setSize(420, 380);
        }

        void dismissCallout()
        {
            // Find parent CallOutBox and dismiss it
            if (auto* parent = getParentComponent())
            {
                if (auto* callout = parent->findParentComponentOfClass<juce::CallOutBox>())
                    callout->exitModalState(0);
            }
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced(10);
            auto buttonArea = bounds.removeFromBottom(40);

            m_selector->setBounds(bounds);

            auto buttonWidth = 80;
            m_okButton.setBounds(buttonArea.removeFromRight(buttonWidth));
            buttonArea.removeFromRight(10);
            m_cancelButton.setBounds(buttonArea.removeFromRight(buttonWidth));
        }

        juce::ColourSelector* m_selector;
        juce::TextButton m_okButton;
        juce::TextButton m_cancelButton;
        std::function<void(bool, juce::Colour)> m_callback;
    };

    // Create dialog with callback - use weak reference pattern to avoid capture issues
    auto dialogPtr = std::make_unique<ColorPickerDialog>(colorSelector,
        [this, regionIndex, currentColor](bool ok, juce::Colour newColor)
        {
            if (ok && newColor != currentColor)
            {
                Region* r = m_regionManager.getRegion(regionIndex);
                if (r != nullptr)
                {
                    r->setColor(newColor);
                    if (onRegionColorChanged)
                        onRegionColorChanged(regionIndex, newColor);
                    repaint();
                }
            }

            // CallOutBox will be dismissed automatically when OK/Cancel is clicked
            // We rely on JUCE's component hierarchy for cleanup
        });

    // Show in callout box - ownership transferred to JUCE
    juce::CallOutBox::launchAsynchronously(
        std::move(dialogPtr),
        getScreenBounds(),
        nullptr);
}
