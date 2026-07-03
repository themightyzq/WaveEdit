/*
  ==============================================================================

    RegionListPanel_BatchRename.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Batch-rename implementation for RegionListPanel (pattern, find/replace,
    prefix/suffix pipeline plus the expand/apply/cancel section handling).
    Split out of RegionListPanel.cpp to stay under the Sec 7.5 file-size cap.

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

#include "RegionListPanel.h"
#include <juce_gui_extra/juce_gui_extra.h>

void RegionListPanel::expandBatchRenameSection(bool expand)
{
    m_batchRenameSectionExpanded = expand;
    m_batchRenameSection.setVisible(expand);

    // Show/hide the "New Name" preview column
    m_table.getHeader().setColumnVisible(NewNameColumn, expand);

    // Update button text to show current state
    m_batchRenameToggleButton.setButtonText(expand ? "Hide Batch Rename" : "Batch Rename");

    // Trigger relayout
    resized();

    // If expanding, update preview to show current selection
    if (expand)
    {
        updateBatchRenamePreview();
    }
}

//==============================================================================
// Batch rename helper methods (stubs for now - will be implemented in Phase 2.4-2.8)
//==============================================================================

void RegionListPanel::updateBatchRenameMode()
{
    // Future: Update UI based on current rename mode (not currently called)
}

void RegionListPanel::updateBatchRenamePreview()
{
    // Trigger table repaint to update the "New Name" column preview
    // The preview is now shown directly in the table's NewNameColumn
    m_table.repaint();
}

juce::String RegionListPanel::generateNewName(int index, const Region& region) const
{
    // Apply ALL rename operations cumulatively (Pattern → Find/Replace → Prefix/Suffix)
    // This allows users to preview the combined effect of all operations

    juce::String newName = region.getName();

    // STEP 1: Apply Pattern operation (if pattern has been customized from default)
    {
        // Pattern mode: Replace {n}, {N}, {original} placeholders
        juce::String patternName = m_customPattern;

        // Calculate the region number (1-based index from m_startNumber)
        int regionNumber = m_startNumber + index;

        // Replace {n} with sequential number
        patternName = patternName.replace("{n}", juce::String(regionNumber));

        // Replace {N} with zero-padded number
        // Determine padding width based on total number of regions
        auto selectedIndices = getSelectedRegionIndices();
        int maxNumber = m_startNumber + static_cast<int>(selectedIndices.size()) - 1;
        int paddingWidth = juce::String(maxNumber).length();
        juce::String paddedNumber = juce::String(regionNumber).paddedLeft('0', paddingWidth);
        patternName = patternName.replace("{N}", paddedNumber);

        // Replace {original} with original region name
        patternName = patternName.replace("{original}", newName);

        // Use pattern result as new base name
        newName = patternName;
    }

    // STEP 2: Apply Find/Replace operation to the result from step 1
    {
        if (!m_findText.isEmpty())
        {
            if (m_replaceAll)
            {
                // Replace all occurrences
                if (m_caseSensitive)
                {
                    newName = newName.replace(m_findText, m_replaceText);
                }
                else
                {
                    // JUCE doesn't have replaceIgnoreCase, so implement manually
                    juce::String result;
                    juce::String remaining = newName;
                    juce::String findLower = m_findText.toLowerCase();

                    while (true)
                    {
                        int pos = remaining.toLowerCase().indexOf(findLower);
                        if (pos < 0)
                        {
                            // No more matches - append remaining text
                            result += remaining;
                            break;
                        }

                        // Append text before match + replacement
                        result += remaining.substring(0, pos);
                        result += m_replaceText;

                        // Continue with text after match
                        remaining = remaining.substring(pos + m_findText.length());
                    }

                    newName = result;
                }
            }
            else
            {
                // Replace first occurrence only
                if (m_caseSensitive)
                {
                    newName = newName.replaceFirstOccurrenceOf(m_findText, m_replaceText);
                }
                else
                {
                    // JUCE doesn't have replaceFirstOccurrenceOfIgnoreCase, so we need to implement it
                    // Find the first occurrence (case-insensitive) and replace it
                    int pos = newName.toLowerCase().indexOf(m_findText.toLowerCase());
                    if (pos >= 0)
                    {
                        newName = newName.substring(0, pos) + m_replaceText +
                                 newName.substring(pos + m_findText.length());
                    }
                }
            }
        }
    }

    // STEP 3: Apply Prefix/Suffix operation to the result from step 2
    {
        // Add prefix
        if (!m_prefixText.isEmpty())
        {
            newName = m_prefixText + newName;
        }

        // Add sequential numbering (before suffix)
        if (m_addNumbering)
        {
            int regionNumber = m_startNumber + index;
            newName = newName + " " + juce::String(regionNumber);
        }

        // Add suffix
        if (!m_suffixText.isEmpty())
        {
            newName = newName + m_suffixText;
        }
    }

    return newName;
}

void RegionListPanel::applyBatchRename()
{
    if (!m_regionManager || !m_listener)
        return;

    // Get selected region indices
    auto selectedIndices = getSelectedRegionIndices();
    if (selectedIndices.empty())
    {
        // No regions selected - show message
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                "Batch Rename",
                                                "No regions selected.\n\nSelect one or more regions to rename.");
        return;
    }

    // Generate new names for all selected regions
    std::vector<juce::String> newNames;
    newNames.reserve(selectedIndices.size());

    for (int i = 0; i < static_cast<int>(selectedIndices.size()); ++i)
    {
        int regionIndex = selectedIndices[static_cast<size_t>(i)];
        const auto* region = m_regionManager->getRegion(regionIndex);

        if (region)
        {
            juce::String newName = generateNewName(i, *region);
            newNames.push_back(newName);
        }
        else
        {
            // Region invalid - this shouldn't happen
            newNames.push_back(juce::String());
        }
    }

    // Call listener to create undo action and apply renames
    // The listener (Main.cpp) will:
    // 1. Collect old names
    // 2. Create BatchRenameRegionUndoAction
    // 3. Add to undo manager
    // 4. Perform the action (applies renames)
    // 5. Refresh RegionDisplay
    m_listener->regionListPanelBatchRenameApply(selectedIndices, newNames);

    // Collapse batch rename section
    expandBatchRenameSection(false);

    // Refresh table to show updated names
    m_table.updateContent();
    m_table.repaint();
}

void RegionListPanel::cancelBatchRename()
{
    // Collapse section without applying changes
    expandBatchRenameSection(false);
}
