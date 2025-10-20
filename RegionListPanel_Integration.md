# RegionListPanel Integration Guide

## Overview
The RegionListPanel has been successfully implemented with all requested features:
- Sortable table with columns (Name, Start, End, Duration, Color)
- Inline name editing
- Search/filter functionality
- Keyboard navigation (arrows, Enter to jump, Delete to remove)
- Mouse interaction (click to select, double-click to jump)
- Professional dark theme matching WaveEdit's design

## Integration Steps for Main.cpp

### 1. Include the header
```cpp
#include "Source/UI/RegionListPanel.h"
```

### 2. Add member variable to your main component
```cpp
std::unique_ptr<RegionListPanel> m_regionListPanel;
juce::DocumentWindow* m_regionListWindow = nullptr;
```

### 3. Implement the Listener interface
Your main component or document manager should implement `RegionListPanel::Listener`:

```cpp
class MainComponent : public juce::Component,
                     public RegionListPanel::Listener
{
    // ... existing interfaces ...

    // RegionListPanel::Listener overrides
    void regionListPanelJumpToRegion(int regionIndex) override
    {
        if (auto* doc = m_documentManager->getCurrentDocument())
        {
            if (auto* region = doc->getRegionManager().getRegion(regionIndex))
            {
                // Jump to region and select its range
                int64_t startSample = region->getStartSample();
                int64_t endSample = region->getEndSample();

                // Convert to time for selection
                double startTime = AudioUnits::samplesToSeconds(startSample, doc->getSampleRate());
                double endTime = AudioUnits::samplesToSeconds(endSample, doc->getSampleRate());

                // Set selection
                doc->setSelection(startTime, endTime);

                // Center view on region (optional)
                doc->getWaveformDisplay().centerViewOnTime(startTime);
            }
        }
    }

    void regionListPanelRegionDeleted(int regionIndex) override
    {
        // Region already deleted from RegionManager
        // Just refresh the waveform display
        if (auto* doc = m_documentManager->getCurrentDocument())
        {
            doc->getWaveformDisplay().repaint();
        }
    }

    void regionListPanelRegionRenamed(int regionIndex, const juce::String& newName) override
    {
        // Region already renamed in RegionManager
        // Just refresh the waveform display
        if (auto* doc = m_documentManager->getCurrentDocument())
        {
            doc->getWaveformDisplay().repaint();
        }
    }
};
```

### 4. Handle the command in perform()
In your `perform()` method, add the case for `regionShowList`:

```cpp
case CommandIDs::regionShowList:
{
    if (auto* doc = getCurrentDocument())
    {
        // Create the panel if it doesn't exist
        if (!m_regionListPanel)
        {
            m_regionListPanel = std::make_unique<RegionListPanel>(
                &doc->getRegionManager(),
                doc->getSampleRate()
            );
            m_regionListPanel->setListener(this);
        }
        else
        {
            // Update with current document's data
            m_regionListPanel->setSampleRate(doc->getSampleRate());
        }

        // Show in a window
        if (!m_regionListWindow || !m_regionListWindow->isVisible())
        {
            m_regionListWindow = m_regionListPanel->showInWindow(false); // non-modal

            // Set up close handler
            m_regionListWindow->setCloseButtonCallback([this]()
            {
                m_regionListWindow = nullptr;
                m_regionListPanel.reset();
            });
        }
        else
        {
            // Bring existing window to front
            m_regionListWindow->toFront(true);
        }
    }
    break;
}
```

### 5. Add keyboard shortcut in getCommandInfo()
```cpp
case CommandIDs::regionShowList:
    result.setInfo("Show Region List", "Display list of all regions", "Region", 0);
    result.addDefaultKeypress('M', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
    break;
```

### 6. Clean up on document change or close
When switching documents or closing:

```cpp
void documentChanged()
{
    // Close region list panel if open
    if (m_regionListWindow)
    {
        m_regionListWindow->closeButtonPressed();
        m_regionListWindow = nullptr;
        m_regionListPanel.reset();
    }
}
```

## Features Implemented

### Table Features
- **Sortable Columns**: Click any column header to sort ascending/descending
- **Color Display**: Visual color swatches for each region
- **Time Formatting**: Times displayed in seconds with 3 decimal places (uses AudioUnits helpers)
- **Inline Editing**: Click on region name to edit inline
- **Search Filter**: Real-time filtering as you type

### Keyboard Shortcuts (when panel has focus)
- **Arrow Keys**: Navigate rows
- **Enter**: Jump to selected region
- **Delete/Backspace**: Delete selected region
- **Escape** (in search): Clear search filter
- **Tab**: Move between search box and table

### Mouse Interaction
- **Click row**: Select region
- **Double-click row**: Jump to region
- **Click column header**: Sort by that column
- **Click name cell**: Start editing region name

### Thread Safety
- Uses locks when accessing RegionManager (if needed in your implementation)
- Periodic timer refresh (500ms) to catch external changes
- Safe handling of deleted regions

## Styling
The panel uses WaveEdit's professional dark theme:
- Background: #1e1e1e
- Alternate rows: #252525
- Selected row: #3a3a3a
- Text: #e0e0e0
- Matches the existing UI components

## Notes
- The panel is resizable (min: 600x400, max: 1200x800)
- Window position/size could be saved to settings (future enhancement)
- Time format can be changed by modifying the `m_timeFormat` member
- The panel automatically refreshes when regions are added/removed externally