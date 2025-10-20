/*
  ==============================================================================

    RegionListPanel.h
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

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/RegionManager.h"
#include "../Utils/AudioUnits.h"

/**
 * A panel that displays a list of regions in a tabular format.
 *
 * Features:
 * - Sortable columns (name, start, end, duration)
 * - Inline editing of region names
 * - Color swatches for each region
 * - Search/filter by name
 * - Keyboard navigation (arrows, Enter to jump, Delete to remove)
 * - Mouse interaction (click to select, double-click to jump)
 *
 * This panel can be shown as a modal or non-modal window and provides
 * an organized view of all regions in the current document.
 */
class RegionListPanel : public juce::Component,
                        public juce::TableListBoxModel,
                        private juce::TextEditor::Listener,
                        private juce::Timer
{
public:
    /**
     * Listener interface for region list events.
     */
    class Listener
    {
    public:
        virtual ~Listener() = default;

        /**
         * Called when the user wants to jump to a region.
         *
         * @param regionIndex Index of the region to jump to
         */
        virtual void regionListPanelJumpToRegion(int regionIndex) = 0;

        /**
         * Called when the user deletes a region.
         *
         * @param regionIndex Index of the region that was deleted
         */
        virtual void regionListPanelRegionDeleted(int regionIndex) = 0;

        /**
         * Called when the user renames a region.
         *
         * @param regionIndex Index of the region
         * @param newName New name for the region
         */
        virtual void regionListPanelRegionRenamed(int regionIndex, const juce::String& newName) = 0;

        /**
         * Called when the user selects a region (single-click).
         *
         * @param regionIndex Index of the region that was selected
         */
        virtual void regionListPanelRegionSelected(int regionIndex) = 0;
    };

    /**
     * Creates a region list panel.
     *
     * @param regionManager The region manager to display and modify
     * @param sampleRate Sample rate for time formatting
     */
    RegionListPanel(RegionManager* regionManager, double sampleRate);

    ~RegionListPanel() override;

    /**
     * Sets the listener for region list events.
     */
    void setListener(Listener* listener);

    /**
     * Updates the sample rate for time formatting.
     */
    void setSampleRate(double sampleRate);

    /**
     * Refreshes the list to reflect current regions.
     */
    void refresh();

    /**
     * Sets the selected region.
     *
     * @param regionIndex Index of region to select
     */
    void selectRegion(int regionIndex);

    /**
     * Shows this panel in a window.
     *
     * @param modal If true, shows as a modal window
     * @return The created window (caller owns this)
     */
    juce::DocumentWindow* showInWindow(bool modal = false);

    //==============================================================================
    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

    //==============================================================================
    // TableListBoxModel overrides
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height,
                           bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId,
                   int width, int height, bool rowIsSelected) override;
    void cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event) override;
    void sortOrderChanged(int newSortColumnId, bool isForwards) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected,
                                            juce::Component* existingComponentToUpdate) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void deleteKeyPressed(int lastRowSelected) override;
    void returnKeyPressed(int lastRowSelected) override;

private:
    /**
     * Column IDs for the table.
     */
    enum ColumnId
    {
        ColorColumn = 1,
        NameColumn = 2,
        StartColumn = 3,
        EndColumn = 4,
        DurationColumn = 5
    };

    /**
     * Struct to hold filtered region data.
     */
    struct FilteredRegion
    {
        int originalIndex;
        const Region* region;

        // Cached formatted strings for performance
        juce::String formattedStart;
        juce::String formattedEnd;
        juce::String formattedDuration;
    };

    /**
     * Custom text editor for inline name editing.
     */
    class NameEditor : public juce::TextEditor
    {
    public:
        NameEditor(RegionListPanel& owner, int row);
        void focusLost(FocusChangeType cause) override;

    private:
        RegionListPanel& owner;
        int rowNumber;
    };

    //==============================================================================
    // TextEditor::Listener override
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorEscapeKeyPressed(juce::TextEditor& editor) override;
    void textEditorFocusLost(juce::TextEditor& editor) override;

    //==============================================================================
    // Timer override
    void timerCallback() override;

    //==============================================================================
    // Private methods
    void updateFilteredRegions();
    void applyFilter();
    void sortRegions();
    void jumpToSelectedRegion();
    void deleteSelectedRegion();
    void startEditingName(int rowNumber);
    void finishEditingName(bool applyChanges);
    juce::String formatTimeForDisplay(double timeInSeconds) const;

    //==============================================================================
    // Member variables
    RegionManager* m_regionManager;
    double m_sampleRate;
    Listener* m_listener = nullptr;

    // UI Components
    juce::Label m_searchLabel;
    juce::TextEditor m_searchBox;
    juce::TableListBox m_table;

    // Filtered and sorted regions
    juce::Array<FilteredRegion> m_filteredRegions;
    juce::String m_filterText;
    int m_lastKnownRegionCount = 0;  // For detecting changes in region count

    // Sorting state
    int m_sortColumnId = StartColumn;
    bool m_sortForwards = true;

    // Name editing
    std::unique_ptr<NameEditor> m_nameEditor;
    int m_editingRow = -1;

    // Time format (cycles through different formats on click)
    AudioUnits::TimeFormat m_timeFormat = AudioUnits::TimeFormat::Seconds;

    // Visual settings
    const int m_rowHeight = 28;
    const int m_colorColumnWidth = 40;
    const juce::Colour m_backgroundColour { 0xff1e1e1e };
    const juce::Colour m_alternateRowColour { 0xff252525 };
    const juce::Colour m_selectedRowColour { 0xff3a3a3a };
    const juce::Colour m_textColour { 0xffe0e0e0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RegionListPanel)
};