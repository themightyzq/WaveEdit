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

#include "RegionDisplay.h"

RegionDisplay::RegionDisplay(RegionManager& regionManager)
    : m_regionManager(regionManager)
    , m_visibleStart(0.0)
    , m_visibleEnd(1.0)
    , m_sampleRate(44100.0)
    , m_totalDuration(0.0)
    , m_hoveredRegionIndex(-1)
    , m_draggedRegionIndex(-1)
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
        // Left-click: select region
        int regionIndex = findRegionAtX(event.x);
        if (regionIndex >= 0)
        {
            m_draggedRegionIndex = regionIndex;

            if (onRegionClicked)
                onRegionClicked(regionIndex);
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

void RegionDisplay::mouseUp(const juce::MouseEvent& event)
{
    m_draggedRegionIndex = -1;
}

void RegionDisplay::mouseMove(const juce::MouseEvent& event)
{
    // Find region under mouse cursor
    int regionIndex = findRegionAtX(event.x);

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

    // Determine alpha based on selection and hover state
    bool isSelected = (regionIndex == m_regionManager.getSelectedRegionIndex());
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
    menu.addItem(2, "Change Color");
    menu.addSeparator();
    menu.addItem(3, "Delete Region", true, false);

    menu.showMenuAsync(juce::PopupMenu::Options(),
        [this, regionIndex](int result)
        {
            if (result == 1)
            {
                showRenameDialog(regionIndex);
            }
            else if (result == 2)
            {
                showColorPicker(regionIndex);
            }
            else if (result == 3)
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

    // For MVP: Use preset colors palette (simplified)
    // Full implementation would use a proper color picker dialog
    const juce::Colour presetColors[] = {
        juce::Colours::lightblue,
        juce::Colours::lightgreen,
        juce::Colours::lightyellow,
        juce::Colours::lightcoral,
        juce::Colours::lightpink,
        juce::Colours::lightsalmon,
        juce::Colours::lightseagreen,
        juce::Colours::lightskyblue
    };

    // Cycle to next color in palette
    juce::Colour currentColor = region->getColor();
    int currentIndex = 0;
    for (int i = 0; i < 8; ++i)
    {
        if (presetColors[i] == currentColor)
        {
            currentIndex = i;
            break;
        }
    }

    int nextIndex = (currentIndex + 1) % 8;
    region->setColor(presetColors[nextIndex]);

    if (onRegionColorChanged)
        onRegionColorChanged(regionIndex, presetColors[nextIndex]);

    repaint();
}
