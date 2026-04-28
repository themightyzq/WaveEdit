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

#include "MarkerController.h"
#include "../Utils/Document.h"
#include "../Utils/Marker.h"
#include "../Utils/UndoActions/MarkerUndoActions.h"
#include "../Utils/UndoActions/RegionUndoActions.h"
#include <set>

MarkerController::MarkerController()
{
}

void MarkerController::addMarkerAtCursor(Document* doc, double lastClickTimeInSeconds, bool hasLastClickPosition)
{
    if (!doc)
        return;

    if (!doc->getAudioEngine().isFileLoaded())
    {
        DBG("Cannot add marker: No file loaded");
        return;
    }

    // Get cursor position from tracked click position (most reliable)
    // This is set by WaveformClickTracker every time user clicks on waveform
    double cursorTime = 0.0;
    if (hasLastClickPosition)
    {
        // Use tracked click position (always accurate, never cleared)
        cursorTime = lastClickTimeInSeconds;
    }
    else if (doc->getWaveformDisplay().hasEditCursor())
    {
        // Fallback 1: Use edit cursor if available
        cursorTime = doc->getWaveformDisplay().getEditCursorPosition();
    }
    else
    {
        // Fallback 2: Use playback position as last resort
        cursorTime = doc->getAudioEngine().getCurrentPosition();
    }

    // Convert time to samples for marker storage
    int64_t cursorSample = doc->getBufferManager().timeToSample(cursorTime);

    // Generate default marker name (M1, M2, etc.)
    int markerCount = doc->getMarkerManager().getNumMarkers() + 1;
    juce::String markerName = juce::String("M") + juce::String(markerCount);

    // Create marker with default color (yellow)
    Marker marker(markerName, cursorSample, juce::Colours::yellow);

    // Create undo action
    doc->getUndoManager().beginNewTransaction("Add Marker");
    auto* undoAction = new AddMarkerUndoAction(
        doc->getMarkerManager(),
        &doc->getMarkerDisplay(),
        marker
    );
    doc->getUndoManager().perform(undoAction);

    DBG(juce::String::formatted(
        "Added marker '%s' at %.3fs (sample %lld)",
        markerName.toRawUTF8(),
        cursorTime,
        cursorSample
    ));
}

void MarkerController::deleteSelectedMarker(Document* doc)
{
    if (!doc)
        return;

    int selectedIndex = doc->getMarkerManager().getSelectedMarkerIndex();
    if (selectedIndex < 0)
    {
        DBG("Cannot delete marker: No marker selected");
        return;
    }

    const Marker* marker = doc->getMarkerManager().getMarker(selectedIndex);
    if (!marker)
    {
        DBG("Cannot delete marker: Invalid marker index");
        return;
    }

    // Save marker info for logging
    juce::String markerName = marker->getName();
    int64_t markerPosition = marker->getPosition();

    // Create undo action
    doc->getUndoManager().beginNewTransaction("Delete Marker");
    auto* undoAction = new DeleteMarkerUndoAction(
        doc->getMarkerManager(),
        &doc->getMarkerDisplay(),
        selectedIndex,
        *marker
    );
    doc->getUndoManager().perform(undoAction);

    DBG(juce::String::formatted(
        "Deleted marker '%s' at sample %lld",
        markerName.toRawUTF8(),
        markerPosition
    ));
}

void MarkerController::jumpToNextMarker(Document* doc)
{
    if (!doc)
        return;

    int64_t currentSample = doc->getAudioEngine().getCurrentPosition();
    int nextIndex = doc->getMarkerManager().getNextMarkerIndex(currentSample);

    if (nextIndex < 0)
    {
        DBG("No marker found after current position");
        return;
    }

    const Marker* marker = doc->getMarkerManager().getMarker(nextIndex);
    if (!marker)
    {
        DBG("Invalid marker index");
        return;
    }

    // Set playback position to marker
    doc->getAudioEngine().setPosition(marker->getPosition());

    // Select the marker
    doc->getMarkerManager().setSelectedMarkerIndex(nextIndex);

    DBG(juce::String::formatted(
        "Jumped to marker '%s' at sample %lld",
        marker->getName().toRawUTF8(),
        marker->getPosition()
    ));
}

void MarkerController::jumpToPreviousMarker(Document* doc)
{
    if (!doc)
        return;

    int64_t currentSample = doc->getAudioEngine().getCurrentPosition();
    int prevIndex = doc->getMarkerManager().getPreviousMarkerIndex(currentSample);

    if (prevIndex < 0)
    {
        DBG("No marker found before current position");
        return;
    }

    const Marker* marker = doc->getMarkerManager().getMarker(prevIndex);
    if (!marker)
    {
        DBG("Invalid marker index");
        return;
    }

    // Set playback position to marker
    doc->getAudioEngine().setPosition(marker->getPosition());

    // Select the marker
    doc->getMarkerManager().setSelectedMarkerIndex(prevIndex);

    DBG(juce::String::formatted(
        "Jumped to marker '%s' at sample %lld",
        marker->getName().toRawUTF8(),
        marker->getPosition()
    ));
}

void MarkerController::setupMarkerCallbacks(Document* doc)
{
    if (!doc)
        return;

    auto& markerDisplay = doc->getMarkerDisplay();

    // onMarkerClicked: Select marker and jump playback position
    markerDisplay.onMarkerClicked = [doc](int markerIndex)
    {
        if (!doc)
            return;

        const Marker* marker = doc->getMarkerManager().getMarker(markerIndex);
        if (!marker)
        {
            DBG("Cannot select marker: Invalid marker index");
            return;
        }

        // Set playback position to marker
        doc->getAudioEngine().setPosition(marker->getPosition());

        // Select the marker
        doc->getMarkerManager().setSelectedMarkerIndex(markerIndex);

        DBG(juce::String::formatted(
            "Jumped to marker '%s' at sample %lld",
            marker->getName().toRawUTF8(),
            marker->getPosition()
        ));

        // Repaint to show selection
        doc->getMarkerDisplay().repaint();
    };

    // onMarkerRenamed: Update marker name with undo support
    markerDisplay.onMarkerRenamed = [doc](int markerIndex, const juce::String& newName)
    {
        if (!doc)
            return;

        Marker* marker = doc->getMarkerManager().getMarker(markerIndex);
        if (!marker)
        {
            DBG("Cannot rename marker: Invalid marker index");
            return;
        }

        // Update marker name directly (no undo for rename in Phase 1)
        marker->setName(newName);

        // Save to sidecar JSON file
        doc->getMarkerManager().saveToFile(doc->getFile());

        // Repaint to show new name
        doc->getMarkerDisplay().repaint();

        DBG("Renamed marker to: " + newName);
    };

    // onMarkerColorChanged: Update marker color with undo support
    markerDisplay.onMarkerColorChanged = [doc](int markerIndex, const juce::Colour& newColor)
    {
        if (!doc)
            return;

        Marker* marker = doc->getMarkerManager().getMarker(markerIndex);
        if (!marker)
        {
            DBG("Cannot change marker color: Invalid marker index");
            return;
        }

        // Update marker color directly (no undo for color change in Phase 1)
        marker->setColor(newColor);

        // Save to sidecar JSON file
        doc->getMarkerManager().saveToFile(doc->getFile());

        // Repaint to show new color
        doc->getMarkerDisplay().repaint();

        DBG("Changed marker color");
    };

    // onMarkerDeleted: Remove marker from manager with undo support
    markerDisplay.onMarkerDeleted = [doc](int markerIndex)
    {
        if (!doc)
            return;

        Marker* marker = doc->getMarkerManager().getMarker(markerIndex);
        if (!marker)
        {
            DBG("Cannot delete marker: Invalid marker index");
            return;
        }

        juce::String markerName = marker->getName();

        // Start a new transaction
        juce::String transactionName = "Delete Marker";
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action (stores marker data before deletion)
        auto* undoAction = new DeleteMarkerUndoAction(
            doc->getMarkerManager(),
            &doc->getMarkerDisplay(),
            markerIndex,
            *marker
        );

        // perform() calls DeleteMarkerUndoAction::perform() which removes the marker
        doc->getUndoManager().perform(undoAction);

        DBG("Deleted marker: " + markerName);
    };

    // onMarkerMoved: Update marker position with undo support
    markerDisplay.onMarkerMoved = [doc](int markerIndex, int64_t oldPos, int64_t newPos)
    {
        if (!doc)
            return;

        Marker* marker = doc->getMarkerManager().getMarker(markerIndex);
        if (!marker)
        {
            DBG("Cannot move marker: Invalid marker index");
            return;
        }

        // Update marker position directly (no undo for move in Phase 1)
        marker->setPosition(newPos);

        // Re-sort markers (they must stay sorted by position)
        // Remove and re-add to maintain sort order
        Marker movedMarker = *marker;
        doc->getMarkerManager().removeMarker(markerIndex);
        int newIndex = doc->getMarkerManager().addMarker(movedMarker);

        // Update selection to new index
        doc->getMarkerManager().setSelectedMarkerIndex(newIndex);

        // Save to sidecar JSON file
        doc->getMarkerManager().saveToFile(doc->getFile());

        // Repaint to show new position
        doc->getMarkerDisplay().repaint();

        DBG(juce::String::formatted(
            "Moved marker '%s' from sample %lld to %lld",
            movedMarker.getName().toRawUTF8(),
            oldPos,
            newPos
        ));
    };

    // onMarkerDoubleClicked: Show rename dialog
    markerDisplay.onMarkerDoubleClicked = [doc](int markerIndex)
    {
        if (!doc)
            return;

        Marker* marker = doc->getMarkerManager().getMarker(markerIndex);
        if (!marker)
        {
            DBG("Cannot rename marker: Invalid marker index");
            return;
        }

        // Create an alert window with a text editor for proper input
        auto* alertWindow = new juce::AlertWindow("Rename Marker", "Enter new name:", juce::AlertWindow::NoIcon);
        alertWindow->addTextEditor("name", marker->getName(), "Name:");
        alertWindow->addButton("OK", 1);
        alertWindow->addButton("Cancel", 0);

        // Show modal dialog and handle result
        alertWindow->enterModalState(true, juce::ModalCallbackFunction::create([doc, markerIndex, alertWindow](int result)
        {
            if (result == 1)
            {
                auto newName = alertWindow->getTextEditorContents("name");
                if (newName.isNotEmpty())
                {
                    auto* marker = doc->getMarkerManager().getMarker(markerIndex);
                    if (marker)
                    {
                        marker->setName(newName);
                        doc->getMarkerManager().saveToFile(doc->getFile());
                        doc->getMarkerDisplay().repaint();
                        DBG("Marker renamed to: " + newName);
                    }
                }
            }
            delete alertWindow;
        }), true);
    };

    // Track mouse clicks on waveform for reliable marker placement
    // Create a custom MouseListener to capture click positions
    class WaveformClickTracker : public juce::MouseListener
    {
    public:
        WaveformClickTracker(Document* document, std::function<void(double)> onClickCallback)
            : m_document(document), m_onClickCallback(onClickCallback) {}

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (!m_document)
                return;

            // Get the WaveformDisplay component
            auto& waveform = m_document->getWaveformDisplay();

            // Ignore clicks on scrollbar or ruler (same logic as WaveformDisplay)
            if (event.y < 30 || event.y > waveform.getHeight() - 16)
                return;

            // Convert click position to time
            int clampedX = juce::jlimit(0, waveform.getWidth() - 1, event.x);

            // Calculate time from X position (matches WaveformDisplay::xToTime logic)
            double clickTime = waveform.getVisibleRangeStart() +
                (static_cast<double>(clampedX) / waveform.getWidth()) *
                (waveform.getVisibleRangeEnd() - waveform.getVisibleRangeStart());

            // Invoke callback to store the click position
            if (m_onClickCallback)
                m_onClickCallback(clickTime);

            DBG(juce::String::formatted(
                "Tracked waveform click at %.3fs", clickTime));
        }

    private:
        Document* m_document;
        std::function<void(double)> m_onClickCallback;
    };

    // Note: This listener is created but we can't store the click position directly here
    // The delegate (MainComponent) will need to provide a callback mechanism
    // For now, we'll create the listener but it won't be attached
    // The MainComponent will handle the click tracking separately
}

void MarkerController::convertMarkersToRegions(Document* doc)
{
    if (!doc) return;

    try
    {
        auto& markerManager = doc->getMarkerManager();
        auto& regionManager = doc->getRegionManager();

        int numMarkers = markerManager.getNumMarkers();
        if (numMarkers < 2)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Markers to Regions",
                "Need at least 2 markers to create regions.",
                "OK");
            return;
        }

        // Get all markers sorted by position
        auto allMarkers = markerManager.getAllMarkers();

        // Sort by position
        struct MarkerComparator
        {
            static int compareElements(const Marker& a, const Marker& b)
            {
                if (a.getPosition() < b.getPosition()) return -1;
                if (a.getPosition() > b.getPosition()) return 1;
                return 0;
            }
        };
        MarkerComparator comp;
        allMarkers.sort(comp, false);

        // Create regions between consecutive markers
        juce::Array<Region> newRegions;
        for (int i = 0; i < allMarkers.size() - 1; ++i)
        {
            const auto& startMarker = allMarkers.getReference(i);
            const auto& endMarker = allMarkers.getReference(i + 1);

            Region region(startMarker.getName(),
                         startMarker.getPosition(),
                         endMarker.getPosition());
            region.setColor(startMarker.getColor());
            newRegions.add(region);
        }

        if (newRegions.isEmpty()) return;

        // Register undo action
        doc->getUndoManager().beginNewTransaction("Markers to Regions");
        doc->getUndoManager().perform(new MarkersToRegionsUndoAction(
            regionManager, doc->getRegionDisplay(), doc->getFile(), newRegions));

        doc->setModified(true);

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Markers to Regions",
            "Created " + juce::String(newRegions.size()) + " region(s) from " + juce::String(numMarkers) + " markers.",
            "OK");
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("MarkerController::convertMarkersToRegions - " + juce::String(e.what()));
    }
}

void MarkerController::convertRegionsToMarkers(Document* doc)
{
    if (!doc) return;

    try
    {
        auto& regionManager = doc->getRegionManager();
        auto& markerManager = doc->getMarkerManager();

        int numRegions = regionManager.getNumRegions();
        if (numRegions < 1)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Regions to Markers",
                "No regions to convert.",
                "OK");
            return;
        }

        // Collect unique boundary positions to avoid duplicate markers
        std::set<int64_t> positions;
        struct MarkerInfo { int64_t pos; juce::String name; juce::Colour color; };
        std::vector<MarkerInfo> markerInfos;

        const auto& allRegions = regionManager.getAllRegions();
        for (const auto& region : allRegions)
        {
            int64_t startPos = region.getStartSample();
            int64_t endPos = region.getEndSample();

            if (positions.find(startPos) == positions.end())
            {
                positions.insert(startPos);
                markerInfos.push_back({startPos, region.getName() + " Start", region.getColor()});
            }
            if (positions.find(endPos) == positions.end())
            {
                positions.insert(endPos);
                markerInfos.push_back({endPos, region.getName() + " End", region.getColor()});
            }
        }

        // Sort by position
        std::sort(markerInfos.begin(), markerInfos.end(),
                  [](const MarkerInfo& a, const MarkerInfo& b) { return a.pos < b.pos; });

        // Create Marker objects
        juce::Array<Marker> newMarkers;
        for (const auto& info : markerInfos)
        {
            newMarkers.add(Marker(info.name, info.pos, info.color));
        }

        if (newMarkers.isEmpty()) return;

        // Register undo action
        doc->getUndoManager().beginNewTransaction("Regions to Markers");
        doc->getUndoManager().perform(new RegionsToMarkersUndoAction(
            markerManager, &doc->getMarkerDisplay(), doc->getFile(), newMarkers));

        doc->setModified(true);

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Regions to Markers",
            "Created " + juce::String(newMarkers.size()) + " marker(s) from " + juce::String(numRegions) + " region(s).",
            "OK");
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("MarkerController::convertRegionsToMarkers - " + juce::String(e.what()));
    }
}

//==============================================================================
// Marker List Panel Callbacks

void MarkerController::handleMarkerListJumpToMarker(Document* doc, int markerIndex)
{
    if (!doc)
        return;

    const auto* marker = doc->getMarkerManager().getMarker(markerIndex);
    if (marker)
    {
        // Set playback position to marker
        doc->getAudioEngine().setPosition(marker->getPosition());

        // Select the marker
        doc->getMarkerManager().setSelectedMarkerIndex(markerIndex);

        // Repaint to show visual feedback
        doc->getMarkerDisplay().repaint();

        DBG("Jumped to marker: " + marker->getName());
    }
}

void MarkerController::handleMarkerListMarkerDeleted(Document* doc)
{
    if (!doc)
        return;

    // Marker was deleted from panel - refresh main display
    doc->getMarkerDisplay().repaint();

    DBG("Marker deleted - display refreshed");
}

void MarkerController::handleMarkerListMarkerRenamed(Document* doc)
{
    if (!doc)
        return;

    // Marker was renamed - update display
    doc->setModified(true);
    doc->getMarkerDisplay().repaint();

    DBG("Marker renamed - display refreshed");
}

void MarkerController::handleMarkerListMarkerSelected(Document* doc, int markerIndex)
{
    if (!doc)
        return;

    // Marker was selected in panel - sync with marker display
    doc->getMarkerManager().setSelectedMarkerIndex(markerIndex);
    doc->getMarkerDisplay().repaint();

    DBG("Marker selected: " + juce::String(markerIndex));
}
