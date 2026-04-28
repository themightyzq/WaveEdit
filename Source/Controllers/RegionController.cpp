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

#include "RegionController.h"
#include "../Utils/Document.h"
#include "../Utils/Region.h"
#include "../Utils/Settings.h"
#include "../Utils/AudioUnits.h"
#include "../Utils/RegionExporter.h"
#include "../Utils/UndoableEdits.h"
#include "../Utils/UndoActions/RegionUndoActions.h"
#include "../UI/StripSilenceDialog.h"
#include "../UI/BatchExportDialog.h"
#include "../UI/EditRegionBoundariesDialog.h"
#include <algorithm>

RegionController::RegionController()
{
}

void RegionController::addRegionFromSelection(Document* doc)
{
    if (!doc)
        return;

    if (!doc->getWaveformDisplay().hasSelection())
    {
        DBG("Cannot create region: No selection");
        return;
    }

    // Get selection in samples
    double startTime = doc->getWaveformDisplay().getSelectionStart();
    double endTime = doc->getWaveformDisplay().getSelectionEnd();
    int64_t startSample = doc->getBufferManager().timeToSample(startTime);
    int64_t endSample = doc->getBufferManager().timeToSample(endTime);

    // Apply zero-crossing snap if enabled (Phase 3.3)
    if (Settings::getInstance().getSnapRegionsToZeroCrossings())
    {
        const auto& buffer = doc->getBufferManager().getBuffer();
        if (buffer.getNumChannels() > 0 && buffer.getNumSamples() > 0)
        {
            int channel = 0;  // Use first channel for snap detection
            int searchRadius = 1000;  // 1000 samples (~22ms at 44.1kHz)

            int64_t originalStart = startSample;
            int64_t originalEnd = endSample;

            startSample = AudioUnits::snapToZeroCrossing(startSample, buffer, channel, searchRadius);
            endSample = AudioUnits::snapToZeroCrossing(endSample, buffer, channel, searchRadius);

            DBG(juce::String::formatted(
                "Zero-crossing snap: start %lld -> %lld, end %lld -> %lld",
                originalStart, startSample, originalEnd, endSample));
        }
    }

    // Create region with auto-generated numerical name (001, 002, etc.)
    int regionNum = doc->getRegionManager().getNumRegions() + 1;
    juce::String regionName = juce::String(regionNum).paddedLeft('0', 3);  // Zero-padded to 3 digits
    Region newRegion(regionName, startSample, endSample);

    // Start a new transaction
    juce::String transactionName = "Add Region: " + regionName;
    doc->getUndoManager().beginNewTransaction(transactionName);

    // Create undo action (perform() will add the region)
    auto* undoAction = new AddRegionUndoAction(
        doc->getRegionManager(),
        doc->getRegionDisplay(),
        doc->getFile(),
        newRegion
    );

    // perform() calls AddRegionUndoAction::perform() which adds the region
    doc->getUndoManager().perform(undoAction);

    DBG("Added region: " + regionName);
}

void RegionController::deleteSelectedRegion(Document* doc)
{
    if (!doc)
        return;

    // Get selected region index
    int regionIndex = doc->getRegionManager().getSelectedRegionIndex();
    if (regionIndex < 0)
    {
        DBG("Cannot delete region: No region selected");
        return;
    }

    // Get region before deleting (for logging)
    const Region* region = doc->getRegionManager().getRegion(regionIndex);
    if (!region)
    {
        DBG("Cannot delete region: Invalid region index");
        return;
    }

    juce::String regionName = region->getName();

    // Start a new transaction
    juce::String transactionName = "Delete Region: " + regionName;
    doc->getUndoManager().beginNewTransaction(transactionName);

    // Create undo action (perform() will delete the region)
    auto* undoAction = new DeleteRegionUndoAction(
        doc->getRegionManager(),
        doc->getRegionDisplay(),
        doc->getFile(),
        regionIndex
    );

    // perform() calls DeleteRegionUndoAction::perform() which deletes the region
    doc->getUndoManager().perform(undoAction);

    DBG("Deleted region: " + regionName);
}

void RegionController::jumpToNextRegion(Document* doc)
{
    if (!doc)
        return;

    int numRegions = doc->getRegionManager().getNumRegions();
    if (numRegions == 0)
    {
        DBG("No regions to navigate");
        return;
    }

    // Create sorted list of region indices by start sample (timeline order)
    juce::Array<int> sortedIndices;
    for (int i = 0; i < numRegions; ++i)
        sortedIndices.add(i);

    // Sort by timeline order (start sample position) using std::sort
    std::sort(sortedIndices.begin(), sortedIndices.end(),
        [&](int a, int b)
        {
            const Region* regionA = doc->getRegionManager().getRegion(a);
            const Region* regionB = doc->getRegionManager().getRegion(b);
            if (!regionA || !regionB)
                return false;
            return regionA->getStartSample() < regionB->getStartSample();
        });

    // Get current selected region index
    int currentIndex = doc->getRegionManager().getSelectedRegionIndex();

    // Find position in sorted list
    int nextIndex;
    if (currentIndex < 0)
    {
        // No region selected, start from first in timeline
        nextIndex = sortedIndices[0];
    }
    else
    {
        // Find current position in sorted list
        int currentPos = sortedIndices.indexOf(currentIndex);
        if (currentPos < 0)
            currentPos = 0; // Fallback if not found

        // Move to next in timeline order with wraparound
        int nextPos = (currentPos + 1) % sortedIndices.size();
        nextIndex = sortedIndices[nextPos];
    }

    const Region* nextRegion = doc->getRegionManager().getRegion(nextIndex);
    if (nextRegion)
    {
        // Set selection to region bounds
        double startTime = doc->getBufferManager().sampleToTime(nextRegion->getStartSample());
        double endTime = doc->getBufferManager().sampleToTime(nextRegion->getEndSample());
        doc->getWaveformDisplay().setSelection(startTime, endTime);

        // Select region in manager
        doc->getRegionManager().setSelectedRegionIndex(nextIndex);

        DBG("Jumped to next region (timeline order): " + nextRegion->getName());
    }
}

void RegionController::jumpToPreviousRegion(Document* doc)
{
    if (!doc)
        return;

    int numRegions = doc->getRegionManager().getNumRegions();
    if (numRegions == 0)
    {
        DBG("No regions to navigate");
        return;
    }

    // Create sorted list of region indices by start sample (timeline order)
    juce::Array<int> sortedIndices;
    for (int i = 0; i < numRegions; ++i)
        sortedIndices.add(i);

    // Sort by timeline order (start sample position) using std::sort
    std::sort(sortedIndices.begin(), sortedIndices.end(),
        [&](int a, int b)
        {
            const Region* regionA = doc->getRegionManager().getRegion(a);
            const Region* regionB = doc->getRegionManager().getRegion(b);
            if (!regionA || !regionB)
                return false;
            return regionA->getStartSample() < regionB->getStartSample();
        });

    // Get current selected region index
    int currentIndex = doc->getRegionManager().getSelectedRegionIndex();

    // Find position in sorted list
    int prevIndex;
    if (currentIndex < 0)
    {
        // No region selected, start from last in timeline
        prevIndex = sortedIndices[sortedIndices.size() - 1];
    }
    else
    {
        // Find current position in sorted list
        int currentPos = sortedIndices.indexOf(currentIndex);
        if (currentPos < 0)
            currentPos = 0; // Fallback if not found

        // Move to previous in timeline order with wraparound
        int prevPos = (currentPos - 1 + sortedIndices.size()) % sortedIndices.size();
        prevIndex = sortedIndices[prevPos];
    }

    const Region* prevRegion = doc->getRegionManager().getRegion(prevIndex);
    if (prevRegion)
    {
        // Set selection to region bounds
        double startTime = doc->getBufferManager().sampleToTime(prevRegion->getStartSample());
        double endTime = doc->getBufferManager().sampleToTime(prevRegion->getEndSample());
        doc->getWaveformDisplay().setSelection(startTime, endTime);

        // Select region in manager
        doc->getRegionManager().setSelectedRegionIndex(prevIndex);

        DBG("Jumped to previous region (timeline order): " + prevRegion->getName());
    }
}

void RegionController::selectInverseOfRegions(Document* doc)
{
    if (!doc)
        return;

    // Get inverse ranges (everything NOT in regions)
    int64_t totalSamples = doc->getBufferManager().getNumSamples();
    auto inverseRanges = doc->getRegionManager().getInverseRanges(totalSamples);

    if (inverseRanges.isEmpty())
    {
        DBG("No inverse selection: All audio covered by regions");
        return;
    }

    // For MVP: Select the first inverse range
    // Future: Support multi-range selection for advanced editing workflows
    auto firstRange = inverseRanges[0];
    double startTime = doc->getBufferManager().sampleToTime(firstRange.first);
    double endTime = doc->getBufferManager().sampleToTime(firstRange.second);

    doc->getWaveformDisplay().setSelection(startTime, endTime);

    DBG(juce::String::formatted(
        "Selected inverse of regions (%d gap%s found, showing first)",
        inverseRanges.size(),
        inverseRanges.size() == 1 ? "" : "s"));
}

void RegionController::selectAllRegions(Document* doc)
{
    if (!doc)
        return;

    if (doc->getRegionManager().getNumRegions() == 0)
    {
        DBG("No regions to select");
        return;
    }

    // Find union of all regions (earliest start to latest end)
    int64_t earliestStart = std::numeric_limits<int64_t>::max();
    int64_t latestEnd = 0;

    for (int i = 0; i < doc->getRegionManager().getNumRegions(); ++i)
    {
        const Region* region = doc->getRegionManager().getRegion(i);
        if (region)
        {
            earliestStart = std::min(earliestStart, region->getStartSample());
            latestEnd = std::max(latestEnd, region->getEndSample());
        }
    }

    double startTime = doc->getBufferManager().sampleToTime(earliestStart);
    double endTime = doc->getBufferManager().sampleToTime(latestEnd);

    doc->getWaveformDisplay().setSelection(startTime, endTime);

    DBG("Selected union of all regions");
}

void RegionController::showStripSilenceDialog(Document* doc, juce::Component* parent)
{
    if (!doc)
        return;

    // Get audio buffer, sample rate, and current file from current document
    const auto& buffer = doc->getBufferManager().getBuffer();
    double sampleRate = doc->getBufferManager().getSampleRate();
    juce::File currentFile = doc->getAudioEngine().getCurrentFile();

    // CRITICAL: Capture old regions BEFORE showing dialog
    // This enables undo support since the dialog modifies regions directly
    juce::Array<Region> oldRegions;
    const auto& allRegions = doc->getRegionManager().getAllRegions();
    for (const auto& region : allRegions)
    {
        oldRegions.add(region);
    }

    // Create dialog
    auto* dialog = new StripSilenceDialog(doc->getRegionManager(), buffer, sampleRate);

    // Set up Apply callback with retrospective undo support
    dialog->onApply = [this, doc, currentFile, oldRegions = oldRegions](int numRegionsCreated) mutable
    {
        // Get current region display from document
        auto& regionDisplay = doc->getRegionDisplay();

        // NOTE: The dialog already applied Auto Region in applyStripSilence(false)
        // So the regions are already changed. We create a retrospective undo action.

        // Capture new regions (after Auto Region)
        juce::Array<Region> newRegions;
        const auto& currentRegions = doc->getRegionManager().getAllRegions();
        for (const auto& region : currentRegions)
        {
            newRegions.add(region);
        }

        // Create retrospective undo action
        // This action doesn't need to call perform() since changes are already applied
        auto* undoAction = new RetrospectiveStripSilenceUndoAction(
            doc->getRegionManager(),
            regionDisplay,
            currentFile,
            oldRegions,
            newRegions
        );

        // Add to undo manager (don't call perform() since changes already applied)
        doc->getUndoManager().perform(undoAction, "Auto Region");

        // Save regions to JSON file
        doc->getRegionManager().saveToFile(currentFile);

        // Request repaint to show created regions
        regionDisplay.repaint();

        DBG("Auto Region: Created " + juce::String(numRegionsCreated) + " regions with undo support");
    };

    // Set up Cancel callback
    dialog->onCancel = []()
    {
        DBG("Auto Region: Cancelled by user");
    };

    // Show dialog modally
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = "Auto Region";
    options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.useBottomRightCornerResizer = false;

    // Center over parent window
    auto parentBounds = parent->getScreenBounds();
    auto dialogBounds = juce::Rectangle<int>(0, 0, 650, 580);  // Size for waveform preview
    dialogBounds.setCentre(parentBounds.getCentre());
    options.content->setBounds(dialogBounds);

    // Launch dialog
    options.launchAsync();
}

bool RegionController::canMergeRegions(Document* doc) const
{
    if (!doc)
        return false;

    auto& regionManager = doc->getRegionManager();
    int numSelected = regionManager.getNumSelectedRegions();

    // Phase 3.5: Multi-selection merge support
    if (numSelected >= 2)
    {
        // Multiple regions selected: Can merge them all
        return true;
    }
    else if (numSelected == 1)
    {
        // Single region selected: Can merge if there's a next region (legacy behavior)
        int selectedIndex = regionManager.getPrimarySelectionIndex();
        return (selectedIndex >= 0 && selectedIndex < regionManager.getNumRegions() - 1);
    }

    // No regions selected: Cannot merge
    return false;
}

bool RegionController::canSplitRegion(Document* doc) const
{
    if (!doc)
        return false;

    auto& regionManager = doc->getRegionManager();
    auto& waveformDisplay = doc->getWaveformDisplay();
    auto& bufferManager = doc->getBufferManager();

    int64_t cursorSample = bufferManager.timeToSample(waveformDisplay.getEditCursorPosition());
    int regionIndex = regionManager.findRegionAtSample(cursorSample);

    if (regionIndex < 0)
        return false;

    const Region* region = regionManager.getRegion(regionIndex);
    if (!region)
        return false;

    // Can split if cursor is within region and both resulting regions have >= 1 sample
    return cursorSample > region->getStartSample() &&
           cursorSample < region->getEndSample();
}

void RegionController::mergeSelectedRegions(Document* doc)
{
    if (!doc || !canMergeRegions(doc))
        return;

    auto& regionManager = doc->getRegionManager();

    // Save original regions and indices for undo
    juce::Array<int> originalIndices;
    juce::Array<Region> originalRegions;
    auto selectedIndices = regionManager.getSelectedRegionIndices();

    for (int idx : selectedIndices)
    {
        const Region* region = regionManager.getRegion(idx);
        if (region != nullptr)
        {
            originalIndices.add(idx);
            originalRegions.add(*region);
        }
    }

    // Create and perform undo action
    auto* undoAction = new MultiMergeRegionsUndoAction(
        regionManager,
        doc->getRegionDisplay(),
        doc->getFile(),
        originalIndices,
        originalRegions
    );

    doc->getUndoManager().perform(undoAction);

    // Repaint displays
    doc->getWaveformDisplay().repaint();

    juce::String mergedNames;
    for (int i = 0; i < originalRegions.size(); ++i)
    {
        if (i > 0)
            mergedNames += " + ";
        mergedNames += originalRegions[i].getName();
    }
    DBG("Merged " + juce::String(originalRegions.size()) + " regions: " + mergedNames);
}

void RegionController::splitRegionAtCursor(Document* doc)
{
    if (!doc || !canSplitRegion(doc))
        return;

    auto& regionManager = doc->getRegionManager();
    auto& waveformDisplay = doc->getWaveformDisplay();
    auto& bufferManager = doc->getBufferManager();

    int64_t splitSample = bufferManager.timeToSample(waveformDisplay.getEditCursorPosition());
    int regionIndex = regionManager.findRegionAtSample(splitSample);

    // Save original region for undo
    Region originalRegion = *regionManager.getRegion(regionIndex);

    // Create undo action
    auto* undoAction = new SplitRegionUndoAction(
        regionManager,
        &doc->getRegionDisplay(),
        regionIndex,
        splitSample,
        originalRegion
    );

    doc->getUndoManager().perform(undoAction);

    DBG("Split region: " + originalRegion.getName());
}

void RegionController::copyRegionsToClipboard(Document* doc)
{
    if (!doc)
        return;

    auto& regionManager = doc->getRegionManager();

    // Copy only the selected region to clipboard
    m_regionClipboard.clear();

    int selectedIndex = regionManager.getSelectedRegionIndex();
    if (selectedIndex >= 0)
    {
        if (const auto* region = regionManager.getRegion(selectedIndex))
        {
            m_regionClipboard.push_back(*region);
            DBG("Copied region '" + region->getName() + "' to clipboard");
        }
    }

    m_hasRegionClipboard = !m_regionClipboard.empty();
}

void RegionController::pasteRegionsFromClipboard(Document* doc)
{
    if (!doc || !m_hasRegionClipboard)
        return;

    auto& regionManager = doc->getRegionManager();
    auto& waveformDisplay = doc->getWaveformDisplay();
    auto& bufferManager = doc->getBufferManager();

    // Get cursor position as paste offset
    int64_t cursorSample = bufferManager.timeToSample(waveformDisplay.getEditCursorPosition());

    // Calculate offset (align first region's start to cursor)
    if (m_regionClipboard.empty())
        return;

    int64_t firstRegionStart = m_regionClipboard[0].getStartSample();
    int64_t offset = cursorSample - firstRegionStart;

    // Paste all regions with offset
    int numPasted = 0;
    for (const auto& region : m_regionClipboard)
    {
        int64_t newStart = region.getStartSample() + offset;
        int64_t newEnd = region.getEndSample() + offset;

        // Check if fits within audio duration (both negative and exceeding boundaries)
        int64_t maxSample = bufferManager.getNumSamples();
        if (newStart < 0 || newEnd > maxSample)
        {
            DBG(juce::String::formatted(
                "Stopped pasting: Region '%s' would be outside file bounds (start=%lld, end=%lld, max=%lld)",
                region.getName().toRawUTF8(), newStart, newEnd, maxSample));
            break;  // Stop pasting if region would be out of bounds
        }

        Region newRegion(region.getName(), newStart, newEnd);
        newRegion.setColor(region.getColor());
        regionManager.addRegion(newRegion);
        numPasted++;
    }

    doc->getRegionDisplay().repaint();

    DBG(juce::String::formatted(
        "Pasted %d region%s at sample %lld",
        numPasted,
        numPasted == 1 ? "" : "s",
        cursorSample));
}

void RegionController::showBatchExportDialog(Document* doc, juce::Component* parent)
{
    if (!doc)
        return;

    // Validate preconditions
    if (!doc->getAudioEngine().isFileLoaded())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Audio File",
            "Please load an audio file before exporting regions.",
            "OK"
        );
        return;
    }

    // Check if we have regions to export
    if (doc->getRegionManager().getNumRegions() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "No Regions to Export",
            "There are no regions defined in this file.\n\n"
            "Create regions first using:\n"
            "  • R - Create region from selection\n"
            "  • Cmd+Shift+R - Auto-create regions (Strip Silence)",
            "OK"
        );
        return;
    }

    // Show the batch export dialog
    auto exportSettings = BatchExportDialog::showDialog(
        doc->getFile(),
        doc->getRegionManager()
    );

    if (!exportSettings.has_value())
    {
        // User cancelled
        return;
    }

    // Prepare export settings for RegionExporter
    RegionExporter::ExportSettings settings;
    settings.outputDirectory = exportSettings->outputDirectory;
    settings.includeRegionName = exportSettings->includeRegionName;
    settings.includeIndex = exportSettings->includeIndex;
    settings.bitDepth = 24; // Default to 24-bit for professional quality

    // Copy advanced naming options (Phase 4 enhancements)
    settings.customTemplate = exportSettings->customTemplate;
    settings.prefix = exportSettings->prefix;
    settings.suffix = exportSettings->suffix;
    settings.usePaddedIndex = exportSettings->usePaddedIndex;
    settings.suffixBeforeIndex = exportSettings->suffixBeforeIndex;

    // Create progress dialog with progress value
    double progressValue = 0.0;
    auto progressDialog = std::make_unique<juce::AlertWindow>(
        "Exporting Regions",
        "Exporting regions to separate files...",
        juce::AlertWindow::NoIcon
    );
    progressDialog->addProgressBarComponent(progressValue);
    progressDialog->enterModalState();

    // Export regions with progress callback
    int totalRegions = doc->getRegionManager().getNumRegions();
    int exportedCount = RegionExporter::exportRegions(
        doc->getBufferManager().getBuffer(),
        doc->getAudioEngine().getSampleRate(),
        doc->getRegionManager(),
        doc->getFile(),
        settings,
        [dlg = progressDialog.get(), totalRegions, &progressValue](int current, int total, const juce::String& regionName)
        {
            // Update progress value (referenced by AlertWindow)
            progressValue = static_cast<double>(current + 1) / static_cast<double>(total);

            // Update message
            dlg->setMessage("Exporting: " + regionName +
                           " (" + juce::String(current + 1) + "/" +
                           juce::String(totalRegions) + ")");

            return true; // Continue export
        }
    );

    // Close progress dialog
    progressDialog->exitModalState(0);
    progressDialog.reset();

    // Show result message
    if (exportedCount == totalRegions)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Export Complete",
            "Successfully exported " + juce::String(exportedCount) +
            " region" + (exportedCount > 1 ? "s" : "") + " to:\n\n" +
            settings.outputDirectory.getFullPathName(),
            "OK"
        );
    }
    else if (exportedCount > 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Partial Export",
            "Exported " + juce::String(exportedCount) + " of " +
            juce::String(totalRegions) + " regions.\n\n" +
            "Check the console log for details about failed exports.",
            "OK"
        );
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Export Failed",
            "Failed to export any regions.\n\n"
            "Check the console log for error details.",
            "OK"
        );
    }
}

void RegionController::nudgeRegionBoundary(Document* doc, bool nudgeStart, bool moveLeft)
{
    if (!doc)
        return;

    // Get selected region
    int regionIndex = doc->getRegionManager().getSelectedRegionIndex();
    if (regionIndex < 0)
    {
        DBG("Cannot nudge region: No region selected");
        return;
    }

    Region* region = doc->getRegionManager().getRegion(regionIndex);
    if (!region)
    {
        DBG("Cannot nudge region: Invalid region index");
        return;
    }

    // Get current snap increment in samples
    int64_t increment = doc->getWaveformDisplay().getSnapIncrementInSamples();
    if (moveLeft)
        increment = -increment;

    // Calculate new boundary position
    int64_t oldPosition = nudgeStart ? region->getStartSample() : region->getEndSample();
    int64_t newPosition = oldPosition + increment;

    // Validate boundaries
    int64_t totalSamples = doc->getBufferManager().getNumSamples();

    if (nudgeStart)
    {
        // Start boundary constraints:
        // - Can't go below 0
        // - Can't exceed end boundary (must maintain at least 1 sample region)
        int64_t endSample = region->getEndSample();
        newPosition = juce::jlimit((int64_t)0, endSample - 1, newPosition);
    }
    else
    {
        // End boundary constraints:
        // - Can't go below start boundary (must maintain at least 1 sample region)
        // - Can't exceed total duration
        int64_t startSample = region->getStartSample();
        newPosition = juce::jlimit(startSample + 1, totalSamples, newPosition);
    }

    // Check if position actually changed
    if (newPosition == oldPosition)
    {
        DBG("Cannot nudge region: Boundary already at limit");
        return;
    }

    // Start a new transaction
    juce::String transactionName = juce::String::formatted(
        "Nudge Region %s %s: %s",
        nudgeStart ? "Start" : "End",
        moveLeft ? "Left" : "Right",
        region->getName().toRawUTF8()
    );
    doc->getUndoManager().beginNewTransaction(transactionName);

    // Create undo action (perform() will apply the nudge)
    auto* undoAction = new NudgeRegionUndoAction(
        doc->getRegionManager(),
        &doc->getRegionDisplay(),
        regionIndex,
        nudgeStart,
        oldPosition,
        newPosition
    );

    // perform() calls NudgeRegionUndoAction::perform() which updates the boundary
    doc->getUndoManager().perform(undoAction);

    // Log the nudge
    double oldTime = doc->getBufferManager().sampleToTime(oldPosition);
    double newTime = doc->getBufferManager().sampleToTime(newPosition);
    DBG(juce::String::formatted(
        "Nudged region '%s' %s: %.3fs -> %.3fs (delta: %lld samples)",
        region->getName().toRawUTF8(),
        nudgeStart ? "start" : "end",
        oldTime,
        newTime,
        increment
    ));
}

void RegionController::setupRegionCallbacks(Document* doc)
{
    if (!doc)
        return;

    auto& regionDisplay = doc->getRegionDisplay();

    // onRegionClicked: Select region and update waveform selection
    regionDisplay.onRegionClicked = [this, doc](int regionIndex)
    {
        if (!doc)
            return;

        const Region* region = doc->getRegionManager().getRegion(regionIndex);
        if (!region)
        {
            DBG("Cannot select region: Invalid region index");
            return;
        }

        // CRITICAL FIX: Do NOT modify selection state here!
        // RegionDisplay has already handled multi-selection correctly in mouseDown()
        // This callback should ONLY update the waveform display, not override selection

        // Update waveform selection to match the clicked region bounds
        double startTime = doc->getBufferManager().sampleToTime(region->getStartSample());
        double endTime = doc->getBufferManager().sampleToTime(region->getEndSample());
        doc->getWaveformDisplay().setSelection(startTime, endTime);

        // Repaint to show visual feedback
        doc->getRegionDisplay().repaint();

        // Auto-preview: Automatically play region if enabled
        if (Settings::getInstance().getAutoPreviewRegions())
        {
            // Stop any current playback first
            if (doc->getAudioEngine().isPlaying())
            {
                doc->getAudioEngine().stop();
            }

            // Start playing from region start
            // Note: Playback will continue to end of file unless stopped manually
            // Future enhancement: Add auto-stop at region end using timer
            doc->getAudioEngine().setPosition(startTime);
            doc->getAudioEngine().play();

            DBG("Auto-previewing region: " + region->getName() +
                                     " (" + juce::String(startTime, 3) + "s - " +
                                     juce::String(endTime, 3) + "s)");
        }

        DBG("Selected region: " + region->getName() +
                                " (" + juce::String(startTime, 3) + "s - " +
                                juce::String(endTime, 3) + "s)");
    };

    // onRegionDoubleClicked: Zoom to fit the double-clicked region
    regionDisplay.onRegionDoubleClicked = [doc](int regionIndex)
    {
        if (!doc)
            return;

        // Zoom to the double-clicked region
        doc->getWaveformDisplay().zoomToRegion(regionIndex);

        DBG("Zoomed to region " + juce::String(regionIndex));
    };

    // onRegionRenamed: Update region name with undo support
    regionDisplay.onRegionRenamed = [doc](int regionIndex, const juce::String& newName)
    {
        if (!doc)
            return;

        Region* region = doc->getRegionManager().getRegion(regionIndex);
        if (!region)
        {
            DBG("Cannot rename region: Invalid region index");
            return;
        }

        juce::String oldName = region->getName();

        // Start a new transaction
        juce::String transactionName = "Rename Region: " + oldName + " → " + newName;
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action
        auto* undoAction = new RenameRegionUndoAction(
            doc->getRegionManager(),
            doc->getRegionDisplay(),
            doc->getFile(),
            regionIndex,
            oldName,
            newName
        );

        // perform() calls RenameRegionUndoAction::perform() which renames the region
        doc->getUndoManager().perform(undoAction);

        DBG("Renamed region from '" + oldName + "' to '" + newName + "'");
    };

    // onRegionColorChanged: Update region color with undo support
    regionDisplay.onRegionColorChanged = [doc](int regionIndex, const juce::Colour& newColor)
    {
        if (!doc)
            return;

        Region* region = doc->getRegionManager().getRegion(regionIndex);
        if (!region)
        {
            DBG("Cannot change region color: Invalid region index");
            return;
        }

        juce::Colour oldColor = region->getColor();

        // Start a new transaction
        juce::String transactionName = "Change Region Color";
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action
        auto* undoAction = new ChangeRegionColorUndoAction(
            doc->getRegionManager(),
            doc->getRegionDisplay(),
            doc->getFile(),
            regionIndex,
            oldColor,
            newColor
        );

        // perform() calls ChangeRegionColorUndoAction::perform() which changes the color
        doc->getUndoManager().perform(undoAction);

        DBG("Changed region color");
    };

    // onRegionDeleted: Remove region from manager with undo support
    regionDisplay.onRegionDeleted = [doc](int regionIndex)
    {
        if (!doc)
            return;

        Region* region = doc->getRegionManager().getRegion(regionIndex);
        if (!region)
        {
            DBG("Cannot delete region: Invalid region index");
            return;
        }

        juce::String regionName = region->getName();

        // Start a new transaction
        juce::String transactionName = "Delete Region: " + regionName;
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action
        auto* undoAction = new DeleteRegionUndoAction(
            doc->getRegionManager(),
            doc->getRegionDisplay(),
            doc->getFile(),
            regionIndex
        );

        // perform() calls DeleteRegionUndoAction::perform() which deletes the region
        doc->getUndoManager().perform(undoAction);

        DBG("Deleted region: " + regionName);
    };

    // onRegionResized: Update region boundaries with undo support
    regionDisplay.onRegionResized = [doc](int regionIndex, int64_t oldStart, int64_t oldEnd, int64_t newStart, int64_t newEnd)
    {
        if (!doc)
            return;

        Region* region = doc->getRegionManager().getRegion(regionIndex);
        if (!region)
        {
            DBG("Cannot resize region: Invalid region index");
            return;
        }

        // Skip if boundaries haven't changed (using parameters, not region)
        if (oldStart == newStart && oldEnd == newEnd)
            return;

        // Start a new transaction
        juce::String transactionName = "Resize Region: " + region->getName();
        doc->getUndoManager().beginNewTransaction(transactionName);

        // Create undo action with original boundaries from parameters
        auto* undoAction = new ResizeRegionUndoAction(
            doc->getRegionManager(),
            doc->getRegionDisplay(),
            doc->getFile(),
            regionIndex,
            oldStart,
            oldEnd,
            newStart,
            newEnd
        );

        // perform() calls ResizeRegionUndoAction::perform() which resizes the region
        doc->getUndoManager().perform(undoAction);

        DBG(juce::String::formatted(
            "Resized region '%s': %lld-%lld → %lld-%lld samples",
            region->getName().toRawUTF8(),
            oldStart, oldEnd, newStart, newEnd));
    };

    // onRegionResizing: Real-time visual feedback during region resize drag
    regionDisplay.onRegionResizing = [doc]()
    {
        if (!doc)
            return;

        // Repaint WaveformDisplay to update region overlays in real-time
        doc->getWaveformDisplay().repaint();
    };

    // onRegionEditBoundaries: Show dialog to edit region boundaries numerically
    // Note: This captures 'this' to access m_timeFormat - delegate will provide it via callback
    regionDisplay.onRegionEditBoundaries = [doc](int regionIndex)
    {
        if (!doc)
            return;

        Region* region = doc->getRegionManager().getRegion(regionIndex);
        if (!region)
        {
            DBG("Cannot edit region boundaries: Invalid region index");
            return;
        }

        // Get audio context
        double sampleRate = doc->getBufferManager().getSampleRate();
        double fps = 30.0;  // Default FPS (future: user-configurable via settings)
        int64_t totalSamples = doc->getBufferManager().getNumSamples();
        AudioUnits::TimeFormat currentFormat = AudioUnits::TimeFormat::Seconds;  // Default format

        // Show dialog
        EditRegionBoundariesDialog::showDialog(
            nullptr,  // parent - may be nullptr
            *region,
            currentFormat,
            sampleRate,
            fps,
            totalSamples,
            [doc, regionIndex](int64_t newStart, int64_t newEnd)
            {
                // Callback when user clicks OK
                Region* region = doc->getRegionManager().getRegion(regionIndex);
                if (!region)
                    return;

                // Get old boundaries for undo
                int64_t oldStart = region->getStartSample();
                int64_t oldEnd = region->getEndSample();

                // Skip if boundaries haven't changed
                if (oldStart == newStart && oldEnd == newEnd)
                {
                    DBG("No changes to region boundaries");
                    return;
                }

                // Start a new transaction
                juce::String transactionName = "Edit Region Boundaries: " + region->getName();
                doc->getUndoManager().beginNewTransaction(transactionName);

                // Create undo action
                auto* undoAction = new ResizeRegionUndoAction(
                    doc->getRegionManager(),
                    doc->getRegionDisplay(),
                    doc->getFile(),
                    regionIndex,
                    oldStart,
                    oldEnd,
                    newStart,
                    newEnd
                );

                // Perform action
                doc->getUndoManager().perform(undoAction);

                // Mark document as modified
                doc->setModified(true);

                // Repaint to show updated region
                doc->getRegionDisplay().repaint();
                doc->getWaveformDisplay().repaint();

                DBG(juce::String::formatted(
                    "Edited region '%s' boundaries: %lld-%lld → %lld-%lld samples",
                    region->getName().toRawUTF8(),
                    oldStart, oldEnd, newStart, newEnd));
            }
        );
    };
}

//==============================================================================
// Region List Panel Callbacks

void RegionController::handleRegionListJumpToRegion(Document* doc, int regionIndex)
{
    if (!doc)
        return;

    if (auto* region = doc->getRegionManager().getRegion(regionIndex))
    {
        // Jump to region and select its range
        int64_t startSample = region->getStartSample();
        int64_t endSample = region->getEndSample();

        // Convert to time for selection
        double sampleRate = doc->getBufferManager().getSampleRate();
        double startTime = static_cast<double>(startSample) / sampleRate;
        double endTime = static_cast<double>(endSample) / sampleRate;

        // Set selection
        doc->getWaveformDisplay().setSelection(startTime, endTime);

        // Center view on region start
        doc->getWaveformDisplay().setVisibleRange(startTime, endTime);
        doc->getWaveformDisplay().zoomToSelection();

        DBG("Jumped to region: " + region->getName());
    }
}

void RegionController::handleRegionListRegionDeleted(Document* doc)
{
    if (!doc)
        return;

    // Region already deleted from RegionManager
    // Just refresh the waveform display
    doc->getWaveformDisplay().repaint();

    DBG("Region deleted - display refreshed");
}

void RegionController::handleRegionListRegionRenamed(Document* doc)
{
    if (!doc)
        return;

    // Region already renamed in RegionManager
    // Refresh both the waveform display and the region display
    doc->getWaveformDisplay().repaint();
    doc->getRegionDisplay().repaint();  // This ensures region names update in the waveform

    DBG("Region renamed - display refreshed");
}

void RegionController::handleRegionListRegionSelected(Document* doc, int regionIndex)
{
    if (!doc)
        return;

    if (auto* region = doc->getRegionManager().getRegion(regionIndex))
    {
        // Single-click: Select the region's range and zoom out to show full waveform
        // Get region boundaries
        int64_t startSample = region->getStartSample();
        int64_t endSample = region->getEndSample();

        // Convert to time for selection
        double sampleRate = doc->getBufferManager().getSampleRate();
        double startTime = static_cast<double>(startSample) / sampleRate;
        double endTime = static_cast<double>(endSample) / sampleRate;

        // Zoom out to show full waveform first
        doc->getWaveformDisplay().zoomToFit();

        // Then set selection to the region
        doc->getWaveformDisplay().setSelection(startTime, endTime);
        doc->getWaveformDisplay().repaint();

        DBG("Selected region: " + region->getName());
    }
}

void RegionController::handleRegionListBatchRenameStart()
{
    // Called when user wants to start batch rename workflow (e.g., from Cmd+Shift+N shortcut)
    // Callback to refresh the region list panel is called via m_onRegionListRefreshNeeded
    if (m_onRegionListRefreshNeeded)
    {
        m_onRegionListRefreshNeeded();
    }

    DBG("Batch rename workflow started");
}

void RegionController::handleRegionListBatchRenameApply(Document* doc, const std::vector<int>& regionIndices,
                                                        const std::vector<juce::String>& newNames)
{
    if (!doc || regionIndices.empty() || newNames.empty())
        return;

    // Ensure indices and names match
    if (regionIndices.size() != newNames.size())
    {
        jassertfalse;  // Programming error - should never happen
        return;
    }

    // Collect old names for undo
    std::vector<juce::String> oldNames;
    oldNames.reserve(regionIndices.size());

    for (int index : regionIndices)
    {
        if (auto* region = doc->getRegionManager().getRegion(index))
        {
            oldNames.push_back(region->getName());
        }
        else
        {
            // Region no longer exists - shouldn't happen
            jassertfalse;
            return;
        }
    }

    // Create undo action with old and new names
    auto* undoAction = new BatchRenameRegionUndoAction(
        doc->getRegionManager(),
        &doc->getRegionDisplay(),
        regionIndices,
        oldNames,
        newNames
    );

    // Add to undo manager and perform
    doc->getUndoManager().perform(undoAction);

    // Debug: Log undo/redo state
    DBG("Batch rename action added to undo manager. Can undo: " +
        juce::String(doc->getUndoManager().canUndo() ? "YES" : "NO"));

    // Refresh displays
    doc->getWaveformDisplay().repaint();
    doc->getRegionDisplay().repaint();

    // Refresh region list panel
    if (m_onRegionListRefreshNeeded)
    {
        m_onRegionListRefreshNeeded();
    }
}
