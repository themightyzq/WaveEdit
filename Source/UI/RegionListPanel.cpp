/*
  ==============================================================================

    RegionListPanel.cpp
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

#include "RegionListPanel.h"

//==============================================================================
// NameEditor implementation
//==============================================================================

RegionListPanel::NameEditor::NameEditor(RegionListPanel& owner, int row)
    : owner(owner), rowNumber(row)
{
    setMultiLine(false);
    setReturnKeyStartsNewLine(false);
    setPopupMenuEnabled(false);
    setSelectAllWhenFocused(true);
}

void RegionListPanel::NameEditor::focusLost(FocusChangeType cause)
{
    (void)cause;  // Suppress unused parameter warning
    owner.finishEditingName(true);
}

//==============================================================================
// RegionListPanel implementation
//==============================================================================

RegionListPanel::RegionListPanel(RegionManager* regionManager, double sampleRate)
    : m_regionManager(regionManager)
    , m_sampleRate(sampleRate)
{
    // Set up search box
    m_searchLabel.setText("Search:", juce::dontSendNotification);
    m_searchLabel.setColour(juce::Label::textColourId, m_textColour);
    addAndMakeVisible(m_searchLabel);

    m_searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    m_searchBox.setColour(juce::TextEditor::textColourId, m_textColour);
    m_searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff3a3a3a));
    m_searchBox.addListener(this);
    addAndMakeVisible(m_searchBox);

    // Set up table
    m_table.setColour(juce::ListBox::backgroundColourId, m_backgroundColour);
    m_table.setColour(juce::ListBox::textColourId, m_textColour);
    m_table.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff3a3a3a));
    m_table.setOutlineThickness(1);
    m_table.setRowHeight(m_rowHeight);
    m_table.setModel(this);

    // Configure table columns
    auto& header = m_table.getHeader();
    header.addColumn("", ColorColumn, m_colorColumnWidth, m_colorColumnWidth, m_colorColumnWidth,
                    juce::TableHeaderComponent::notSortable);
    header.addColumn("Name", NameColumn, 200, 100, 400);
    header.addColumn("Start", StartColumn, 120, 80, 200);
    header.addColumn("End", EndColumn, 120, 80, 200);
    header.addColumn("Duration", DurationColumn, 120, 80, 200);

    header.setColour(juce::TableHeaderComponent::textColourId, m_textColour);
    header.setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour(0xff2a2a2a));
    header.setColour(juce::TableHeaderComponent::highlightColourId, juce::Colour(0xff3a3a3a));

    addAndMakeVisible(m_table);

    // Initialize filtered regions
    updateFilteredRegions();
    if (m_regionManager)
        m_lastKnownRegionCount = m_regionManager->getNumRegions();

    // Set focus order
    setWantsKeyboardFocus(true);
    m_searchBox.setWantsKeyboardFocus(true);
    m_table.setWantsKeyboardFocus(true);

    // Start timer for periodic refresh (in case regions change externally)
    startTimer(500);
}

RegionListPanel::~RegionListPanel()
{
    stopTimer();
}

void RegionListPanel::setListener(Listener* listener)
{
    m_listener = listener;
}

void RegionListPanel::setSampleRate(double sampleRate)
{
    m_sampleRate = sampleRate;
    updateFilteredRegions();
    m_table.updateContent();
}

void RegionListPanel::refresh()
{
    updateFilteredRegions();
    m_table.updateContent();
}

void RegionListPanel::selectRegion(int regionIndex)
{
    // Find the row for this region index in the filtered list
    for (int i = 0; i < m_filteredRegions.size(); ++i)
    {
        if (m_filteredRegions[i].originalIndex == regionIndex)
        {
            m_table.selectRow(i);
            m_table.scrollToEnsureRowIsOnscreen(i);
            break;
        }
    }
}

juce::DocumentWindow* RegionListPanel::showInWindow(bool modal)
{
    // Custom window class that properly handles the close button
    class RegionListWindow : public juce::DocumentWindow
    {
    public:
        RegionListWindow(const juce::String& name, juce::Colour backgroundColour, int requiredButtons)
            : DocumentWindow(name, backgroundColour, requiredButtons)
        {
        }

        void closeButtonPressed() override
        {
            // Hide the window instead of deleting it (can be reopened)
            setVisible(false);
        }
    };

    auto* window = new RegionListWindow("Region List",
                                        juce::Colour(0xff2a2a2a),
                                        juce::DocumentWindow::allButtons);

    window->setContentOwned(this, true);
    window->setResizable(true, true);
    window->setResizeLimits(600, 400, 1200, 800);
    window->centreWithSize(800, 600);

    if (modal)
    {
        window->setVisible(true);
        window->runModalLoop();
    }
    else
    {
        window->setVisible(true);
    }

    return window;
}

//==============================================================================
// Component overrides
//==============================================================================

void RegionListPanel::paint(juce::Graphics& g)
{
    g.fillAll(m_backgroundColour);
}

void RegionListPanel::resized()
{
    auto bounds = getLocalBounds();

    // Search bar at top
    auto searchBounds = bounds.removeFromTop(40).reduced(8);
    m_searchLabel.setBounds(searchBounds.removeFromLeft(60));
    searchBounds.removeFromLeft(4);
    m_searchBox.setBounds(searchBounds);

    // Table fills remaining space
    bounds.removeFromTop(4);
    m_table.setBounds(bounds.reduced(8));
}

bool RegionListPanel::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        // Let the table handle arrow navigation
        return m_table.keyPressed(key);
    }
    else if (key == juce::KeyPress::returnKey)
    {
        jumpToSelectedRegion();
        return true;
    }
    else if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelectedRegion();
        return true;
    }

    return false;
}

//==============================================================================
// TableListBoxModel overrides
//==============================================================================

int RegionListPanel::getNumRows()
{
    return m_filteredRegions.size();
}

void RegionListPanel::paintRowBackground(juce::Graphics& g, int rowNumber,
                                        int width, int height, bool rowIsSelected)
{
    (void)width;   // Suppress unused parameter warning
    (void)height;  // Suppress unused parameter warning

    if (rowIsSelected)
    {
        g.fillAll(m_selectedRowColour);
    }
    else if (rowNumber % 2 == 1)
    {
        g.fillAll(m_alternateRowColour);
    }
}

void RegionListPanel::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                               int width, int height, bool rowIsSelected)
{
    if (rowNumber >= m_filteredRegions.size())
        return;

    const auto& filteredRegion = m_filteredRegions[rowNumber];
    const auto* region = filteredRegion.region;

    if (!region)
        return;

    g.setColour(rowIsSelected ? juce::Colours::white : m_textColour);

    switch (columnId)
    {
        case ColorColumn:
        {
            // Draw color swatch
            auto swatchBounds = juce::Rectangle<int>(4, 4, width - 8, height - 8);
            g.setColour(region->getColor());
            g.fillRoundedRectangle(swatchBounds.toFloat(), 2.0f);

            // Draw outline
            g.setColour(juce::Colour(0xff4a4a4a));
            g.drawRoundedRectangle(swatchBounds.toFloat(), 2.0f, 1.0f);
            break;
        }

        case NameColumn:
            // Name is handled by the editable component
            if (m_editingRow != rowNumber)
            {
                g.drawText(region->getName(), 4, 0, width - 8, height,
                          juce::Justification::centredLeft, true);
            }
            break;

        case StartColumn:
            g.drawText(filteredRegion.formattedStart, 4, 0, width - 8, height,
                      juce::Justification::centredLeft, true);
            break;

        case EndColumn:
            g.drawText(filteredRegion.formattedEnd, 4, 0, width - 8, height,
                      juce::Justification::centredLeft, true);
            break;

        case DurationColumn:
            g.drawText(filteredRegion.formattedDuration, 4, 0, width - 8, height,
                      juce::Justification::centredLeft, true);
            break;
    }
}

void RegionListPanel::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    if (columnId == NameColumn && event.mods.isLeftButtonDown())
    {
        // Start editing on click in name column
        startEditingName(rowNumber);
    }
}

void RegionListPanel::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)columnId;  // Suppress unused parameter warning
    (void)event;     // Suppress unused parameter warning

    if (rowNumber >= 0 && rowNumber < m_filteredRegions.size())
    {
        jumpToSelectedRegion();
    }
}

void RegionListPanel::sortOrderChanged(int newSortColumnId, bool isForwards)
{
    m_sortColumnId = newSortColumnId;
    m_sortForwards = isForwards;
    sortRegions();
    m_table.updateContent();
}

juce::Component* RegionListPanel::refreshComponentForCell(int rowNumber, int columnId,
                                                         bool isRowSelected,
                                                         juce::Component* existingComponentToUpdate)
{
    if (columnId == NameColumn && m_editingRow == rowNumber)
    {
        if (!m_nameEditor)
        {
            m_nameEditor = std::make_unique<NameEditor>(*this, rowNumber);
            m_nameEditor->addListener(this);  // Register for Return/Escape key events

            if (rowNumber < m_filteredRegions.size())
            {
                const auto* region = m_filteredRegions[rowNumber].region;
                if (region)
                {
                    m_nameEditor->setText(region->getName(), false);
                }
            }

            m_nameEditor->setColour(juce::TextEditor::backgroundColourId,
                                   isRowSelected ? m_selectedRowColour : m_backgroundColour);
            m_nameEditor->setColour(juce::TextEditor::textColourId, m_textColour);
            m_nameEditor->selectAll();
        }

        return m_nameEditor.get();
    }

    // Clean up editor if we're not editing this cell
    if (existingComponentToUpdate == m_nameEditor.get())
    {
        m_nameEditor.reset();
    }

    return nullptr;
}

void RegionListPanel::selectedRowsChanged(int lastRowSelected)
{
    // Notify listener that a region was selected (for updating main waveform selection)
    if (lastRowSelected >= 0 && lastRowSelected < m_filteredRegions.size() && m_listener)
    {
        const int regionIndex = m_filteredRegions[lastRowSelected].originalIndex;
        m_listener->regionListPanelRegionSelected(regionIndex);
    }
}

void RegionListPanel::deleteKeyPressed(int lastRowSelected)
{
    (void)lastRowSelected;  // Suppress unused parameter warning
    deleteSelectedRegion();
}

void RegionListPanel::returnKeyPressed(int lastRowSelected)
{
    (void)lastRowSelected;  // Suppress unused parameter warning
    jumpToSelectedRegion();
}

//==============================================================================
// TextEditor::Listener overrides
//==============================================================================

void RegionListPanel::textEditorTextChanged(juce::TextEditor& editor)
{
    if (&editor == &m_searchBox)
    {
        m_filterText = editor.getText();
        applyFilter();
    }
}

void RegionListPanel::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &m_searchBox)
    {
        // Move focus to table when pressing Enter in search box
        m_table.grabKeyboardFocus();
    }
    else if (m_nameEditor && &editor == m_nameEditor.get())
    {
        // Apply changes when pressing Enter in name editor
        finishEditingName(true);
    }
}

void RegionListPanel::textEditorEscapeKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &m_searchBox)
    {
        // Clear search on Escape
        editor.clear();
        m_filterText.clear();
        applyFilter();
    }
    else if (m_nameEditor && &editor == m_nameEditor.get())
    {
        // Discard changes when pressing Escape in name editor
        finishEditingName(false);
    }
}

void RegionListPanel::textEditorFocusLost(juce::TextEditor& editor)
{
    (void)editor;  // Suppress unused parameter warning
    // Nothing special to do when search box loses focus
}

//==============================================================================
// Timer override
//==============================================================================

void RegionListPanel::timerCallback()
{
    // Check if the actual region count has changed (not filtered count)
    if (m_regionManager)
    {
        int currentRegionCount = m_regionManager->getNumRegions();
        if (currentRegionCount != m_lastKnownRegionCount)
        {
            refresh();
            m_lastKnownRegionCount = currentRegionCount;
        }
    }
}

//==============================================================================
// Private methods
//==============================================================================

void RegionListPanel::updateFilteredRegions()
{
    if (!m_regionManager)
    {
        m_filteredRegions.clear();
        m_table.updateContent();
        return;
    }

    // Clear filter text to ensure we get all regions
    m_filterText.clear();
    applyFilter();  // Safe now - no recursion possible since filter is cleared
}

void RegionListPanel::applyFilter()
{
    if (!m_regionManager)
    {
        m_table.updateContent();
        return;
    }

    // Helper lambda to create a FilteredRegion (eliminates code duplication)
    auto createFilteredRegion = [this](int index, const Region* region) -> FilteredRegion
    {
        FilteredRegion fr;
        fr.originalIndex = index;
        fr.region = region;

        // Pre-format time strings for performance
        double startTime = AudioUnits::samplesToSeconds(region->getStartSample(), m_sampleRate);
        double endTime = AudioUnits::samplesToSeconds(region->getEndSample(), m_sampleRate);
        double duration = region->getLengthInSeconds(m_sampleRate);

        fr.formattedStart = formatTimeForDisplay(startTime);
        fr.formattedEnd = formatTimeForDisplay(endTime);
        fr.formattedDuration = formatTimeForDisplay(duration);

        return fr;
    };

    // Clear and rebuild filtered list
    m_filteredRegions.clear();

    const int numRegions = m_regionManager->getNumRegions();
    const bool hasFilter = !m_filterText.isEmpty();
    const auto searchText = hasFilter ? m_filterText.toLowerCase() : juce::String();

    for (int i = 0; i < numRegions; ++i)
    {
        const auto* region = m_regionManager->getRegion(i);
        if (!region)
            continue;

        // Apply filter if present
        if (hasFilter && !region->getName().toLowerCase().contains(searchText))
            continue;

        m_filteredRegions.add(createFilteredRegion(i, region));
    }

    sortRegions();
    m_table.updateContent();
}

void RegionListPanel::sortRegions()
{
    if (m_sortColumnId == 0)
        return;

    // JUCE comparator struct - must have compareElements method
    struct RegionComparator
    {
        RegionComparator(int columnId, bool forwards)
            : sortColumnId(columnId), sortForwards(forwards) {}

        int compareElements(const FilteredRegion& a, const FilteredRegion& b) const
        {
            if (!a.region || !b.region)
                return 0;

            int result = 0;

            switch (sortColumnId)
            {
                case NameColumn:
                    result = a.region->getName().compareNatural(b.region->getName());
                    break;

                case StartColumn:
                    if (a.region->getStartSample() < b.region->getStartSample())
                        result = -1;
                    else if (a.region->getStartSample() > b.region->getStartSample())
                        result = 1;
                    break;

                case EndColumn:
                    if (a.region->getEndSample() < b.region->getEndSample())
                        result = -1;
                    else if (a.region->getEndSample() > b.region->getEndSample())
                        result = 1;
                    break;

                case DurationColumn:
                    if (a.region->getLengthInSamples() < b.region->getLengthInSamples())
                        result = -1;
                    else if (a.region->getLengthInSamples() > b.region->getLengthInSamples())
                        result = 1;
                    break;
            }

            return sortForwards ? result : -result;
        }

    private:
        int sortColumnId;
        bool sortForwards;
    };

    RegionComparator comparator(m_sortColumnId, m_sortForwards);
    m_filteredRegions.sort(comparator);
}

void RegionListPanel::jumpToSelectedRegion()
{
    int selectedRow = m_table.getSelectedRow();

    if (selectedRow >= 0 && selectedRow < m_filteredRegions.size())
    {
        int regionIndex = m_filteredRegions[selectedRow].originalIndex;

        if (m_listener)
        {
            m_listener->regionListPanelJumpToRegion(regionIndex);
        }
    }
}

void RegionListPanel::deleteSelectedRegion()
{
    int selectedRow = m_table.getSelectedRow();

    if (selectedRow >= 0 && selectedRow < m_filteredRegions.size())
    {
        int regionIndex = m_filteredRegions[selectedRow].originalIndex;

        if (m_regionManager)
        {
            m_regionManager->removeRegion(regionIndex);

            if (m_listener)
            {
                m_listener->regionListPanelRegionDeleted(regionIndex);
            }

            refresh();
        }
    }
}

void RegionListPanel::startEditingName(int rowNumber)
{
    if (rowNumber >= 0 && rowNumber < m_filteredRegions.size())
    {
        // If we're already editing a different row, finish that edit first
        // This prevents component lifecycle issues when switching between edits
        if (m_editingRow >= 0 && m_editingRow != rowNumber)
        {
            finishEditingName(false);  // Discard changes, just clean up
        }

        m_editingRow = rowNumber;
        m_table.updateContent();

        // Focus will be grabbed by the editor component when it's created
        if (m_nameEditor)
        {
            m_nameEditor->grabKeyboardFocus();
        }
    }
}

void RegionListPanel::finishEditingName(bool applyChanges)
{
    if (m_editingRow >= 0 && m_editingRow < m_filteredRegions.size() &&
        m_nameEditor && applyChanges)
    {
        const auto newName = m_nameEditor->getText();
        const int regionIndex = m_filteredRegions[m_editingRow].originalIndex;

        if (m_regionManager)
        {
            auto* region = m_regionManager->getRegion(regionIndex);
            if (region && region->getName() != newName)
            {
                region->setName(newName);

                if (m_listener)
                {
                    m_listener->regionListPanelRegionRenamed(regionIndex, newName);
                }
            }
        }
    }

    m_editingRow = -1;
    // Let updateContent() handle cleanup through refreshComponentForCell()
    // Don't manually reset m_nameEditor here - JUCE's ListBox still has internal
    // pointers to it, and destroying it before updateContent() causes a crash.
    m_table.updateContent();
}

juce::String RegionListPanel::formatTimeForDisplay(double timeInSeconds) const
{
    return AudioUnits::formatTime(timeInSeconds, m_sampleRate, m_timeFormat);
}