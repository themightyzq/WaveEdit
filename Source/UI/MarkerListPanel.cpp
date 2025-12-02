/*
  ==============================================================================

    MarkerListPanel.cpp
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

#include "MarkerListPanel.h"

//==============================================================================
// NameEditor implementation
//==============================================================================

MarkerListPanel::NameEditor::NameEditor(MarkerListPanel& owner, int row)
    : owner(owner), rowNumber(row)
{
    setMultiLine(false);
    setReturnKeyStartsNewLine(false);
    setPopupMenuEnabled(false);
    setSelectAllWhenFocused(true);
}

void MarkerListPanel::NameEditor::focusLost(FocusChangeType cause)
{
    (void)cause;
    owner.finishEditingName(true);
}

//==============================================================================
// MarkerListPanel implementation
//==============================================================================

MarkerListPanel::MarkerListPanel(MarkerManager* markerManager, double sampleRate)
    : m_markerManager(markerManager)
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
    m_table.setMultipleSelectionEnabled(false);  // Single selection for markers
    m_table.setModel(this);

    // Configure table columns
    auto& header = m_table.getHeader();
    header.addColumn("", ColorColumn, m_colorColumnWidth, m_colorColumnWidth, m_colorColumnWidth,
                    juce::TableHeaderComponent::notSortable);
    header.addColumn("Name", NameColumn, 200, 100, 400);
    header.addColumn("Position", PositionColumn, 150, 100, 250);

    header.setColour(juce::TableHeaderComponent::textColourId, m_textColour);
    header.setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour(0xff2a2a2a));
    header.setColour(juce::TableHeaderComponent::highlightColourId, juce::Colour(0xff3a3a3a));

    addAndMakeVisible(m_table);

    // Initialize filtered markers
    updateFilteredMarkers();
    if (m_markerManager)
        m_lastKnownMarkerCount = m_markerManager->getNumMarkers();

    // Set focus order
    setWantsKeyboardFocus(true);
    m_searchBox.setWantsKeyboardFocus(true);
    m_table.setWantsKeyboardFocus(true);

    // Start timer for periodic refresh (in case markers change externally)
    startTimer(500);
}

MarkerListPanel::~MarkerListPanel()
{
    stopTimer();
}

void MarkerListPanel::setListener(Listener* listener)
{
    m_listener = listener;
}

void MarkerListPanel::setCommandManager(juce::ApplicationCommandManager* commandManager)
{
    m_commandManager = commandManager;
}

void MarkerListPanel::setSampleRate(double sampleRate)
{
    m_sampleRate = sampleRate;
    updateFilteredMarkers();
}

void MarkerListPanel::refresh()
{
    updateFilteredMarkers();
    m_table.updateContent();
    m_table.repaint();
}

void MarkerListPanel::selectMarker(int markerIndex)
{
    // Find the filtered row for this original marker index
    for (int i = 0; i < m_filteredMarkers.size(); ++i)
    {
        if (m_filteredMarkers[i].originalIndex == markerIndex)
        {
            m_table.selectRow(i);
            return;
        }
    }

    m_table.deselectAllRows();
}

std::vector<int> MarkerListPanel::getSelectedMarkerIndices() const
{
    std::vector<int> indices;

    int selectedRow = m_table.getSelectedRow();
    if (selectedRow >= 0 && selectedRow < m_filteredMarkers.size())
    {
        indices.push_back(m_filteredMarkers[selectedRow].originalIndex);
    }

    return indices;
}

juce::DocumentWindow* MarkerListPanel::showInWindow(bool modal)
{
    // Create custom window that routes key commands to our command manager
    class MarkerListWindow : public juce::DocumentWindow
    {
    public:
        MarkerListWindow(MarkerListPanel* panel, juce::ApplicationCommandManager* cm)
            : juce::DocumentWindow("Marker List",
                                  juce::Colour(0xff2a2a2a),
                                  juce::DocumentWindow::allButtons)
            , markerPanel(panel)
            , commandManager(cm)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(panel, true);
            setResizable(true, false);
            centreWithSize(500, 400);
        }

        void closeButtonPressed() override
        {
            setVisible(false);
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            // Route keyboard shortcuts to command manager (for undo, redo, etc.)
            if (commandManager)
            {
                if (commandManager->invokeDirectly(key.getTextCharacter(), false))
                    return true;
            }

            return juce::DocumentWindow::keyPressed(key);
        }

    private:
        MarkerListPanel* markerPanel;
        juce::ApplicationCommandManager* commandManager;
    };

    auto* window = new MarkerListWindow(this, m_commandManager);
    window->setVisible(true);

    if (modal)
        window->enterModalState();

    return window;
}

//==============================================================================
// Component overrides
//==============================================================================

void MarkerListPanel::paint(juce::Graphics& g)
{
    g.fillAll(m_backgroundColour);
}

void MarkerListPanel::resized()
{
    auto bounds = getLocalBounds();

    // Search box at top
    auto searchArea = bounds.removeFromTop(30).reduced(5);
    m_searchLabel.setBounds(searchArea.removeFromLeft(60));
    m_searchBox.setBounds(searchArea);

    // Table takes remaining space
    m_table.setBounds(bounds);
}

bool MarkerListPanel::keyPressed(const juce::KeyPress& key)
{
    // Route to command manager for global shortcuts
    if (m_commandManager)
    {
        if (m_commandManager->invokeDirectly(key.getTextCharacter(), false))
            return true;
    }

    return Component::keyPressed(key);
}

//==============================================================================
// TableListBoxModel overrides
//==============================================================================

int MarkerListPanel::getNumRows()
{
    return m_filteredMarkers.size();
}

void MarkerListPanel::paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height,
                                        bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll(m_selectedRowColour);
    }
    else if (rowNumber % 2 == 0)
    {
        g.fillAll(m_alternateRowColour);
    }
}

void MarkerListPanel::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                               int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= m_filteredMarkers.size())
        return;

    const auto& filtered = m_filteredMarkers[rowNumber];
    const auto* marker = filtered.marker;

    if (!marker)
        return;

    g.setColour(m_textColour);

    switch (columnId)
    {
        case ColorColumn:
        {
            // Draw color swatch
            auto swatchBounds = juce::Rectangle<int>(4, 4, 32, 20);
            g.setColour(marker->getColor());
            g.fillRoundedRectangle(swatchBounds.toFloat(), 2.0f);

            // Draw border
            g.setColour(juce::Colour(0xff4a4a4a));
            g.drawRoundedRectangle(swatchBounds.toFloat(), 2.0f, 1.0f);
            break;
        }

        case NameColumn:
        {
            // Draw marker name
            g.setFont(14.0f);
            g.drawText(marker->getName(),
                      4, 0, width - 8, height,
                      juce::Justification::centredLeft, true);
            break;
        }

        case PositionColumn:
        {
            // Draw formatted position
            g.setFont(14.0f);
            g.drawText(filtered.formattedPosition,
                      4, 0, width - 8, height,
                      juce::Justification::centredLeft, true);
            break;
        }
    }
}

void MarkerListPanel::cellClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)event;

    if (rowNumber < 0 || rowNumber >= m_filteredMarkers.size())
        return;

    // Notify listener of selection
    if (m_listener)
    {
        int originalIndex = m_filteredMarkers[rowNumber].originalIndex;
        m_listener->markerListPanelMarkerSelected(originalIndex);
    }

    // Start editing name if clicked on name column
    if (columnId == NameColumn)
    {
        startEditingName(rowNumber);
    }
}

void MarkerListPanel::cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent& event)
{
    (void)columnId;
    (void)event;

    if (rowNumber < 0 || rowNumber >= m_filteredMarkers.size())
        return;

    jumpToSelectedMarker();
}

void MarkerListPanel::sortOrderChanged(int newSortColumnId, bool isForwards)
{
    if (newSortColumnId == PositionColumn && m_sortColumnId == PositionColumn)
    {
        // Cycle time format when clicking same column
        switch (m_timeFormat)
        {
            case AudioUnits::TimeFormat::Seconds:
                m_timeFormat = AudioUnits::TimeFormat::Samples;
                break;
            case AudioUnits::TimeFormat::Samples:
                m_timeFormat = AudioUnits::TimeFormat::Milliseconds;
                break;
            case AudioUnits::TimeFormat::Milliseconds:
                m_timeFormat = AudioUnits::TimeFormat::Frames;
                break;
            case AudioUnits::TimeFormat::Frames:
                m_timeFormat = AudioUnits::TimeFormat::Seconds;
                break;
            default:
                m_timeFormat = AudioUnits::TimeFormat::Seconds;
                break;
        }
        updateFilteredMarkers();
        m_table.updateContent();
        m_table.repaint();
    }
    else
    {
        m_sortColumnId = newSortColumnId;
        m_sortForwards = isForwards;
        sortMarkers();
        m_table.updateContent();
    }
}

juce::Component* MarkerListPanel::refreshComponentForCell(int rowNumber, int columnId,
                                                         bool isRowSelected,
                                                         juce::Component* existingComponentToUpdate)
{
    (void)rowNumber;
    (void)columnId;
    (void)isRowSelected;

    // We're not using components for cells, just painting
    delete existingComponentToUpdate;
    return nullptr;
}

void MarkerListPanel::selectedRowsChanged(int lastRowSelected)
{
    (void)lastRowSelected;
    // Selection handled in cellClicked
}

void MarkerListPanel::deleteKeyPressed(int lastRowSelected)
{
    (void)lastRowSelected;
    deleteSelectedMarker();
}

void MarkerListPanel::returnKeyPressed(int lastRowSelected)
{
    (void)lastRowSelected;

    // If editing, finish edit. Otherwise jump to marker
    if (m_nameEditor != nullptr)
    {
        finishEditingName(true);
    }
    else
    {
        jumpToSelectedMarker();
    }
}

//==============================================================================
// TextEditor::Listener overrides
//==============================================================================

void MarkerListPanel::textEditorTextChanged(juce::TextEditor& editor)
{
    // Search box changed - update filter
    if (&editor == &m_searchBox)
    {
        m_filterText = editor.getText();
        applyFilter();
        m_table.updateContent();
    }
}

void MarkerListPanel::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    // Name editor - finish editing
    if (&editor != &m_searchBox)
    {
        finishEditingName(true);
    }
}

void MarkerListPanel::textEditorEscapeKeyPressed(juce::TextEditor& editor)
{
    // Cancel editing
    if (&editor != &m_searchBox)
    {
        finishEditingName(false);
    }
}

void MarkerListPanel::textEditorFocusLost(juce::TextEditor& editor)
{
    // Name editor - save changes
    if (&editor != &m_searchBox)
    {
        finishEditingName(true);
    }
}

//==============================================================================
// Timer override
//==============================================================================

void MarkerListPanel::timerCallback()
{
    // Check if marker count changed (markers added/removed externally)
    if (m_markerManager)
    {
        int currentCount = m_markerManager->getNumMarkers();
        if (currentCount != m_lastKnownMarkerCount)
        {
            m_lastKnownMarkerCount = currentCount;
            refresh();
        }
    }
}

//==============================================================================
// Private methods
//==============================================================================

void MarkerListPanel::updateFilteredMarkers()
{
    m_filteredMarkers.clear();

    if (!m_markerManager)
        return;

    // Build filtered list
    for (int i = 0; i < m_markerManager->getNumMarkers(); ++i)
    {
        const auto* marker = m_markerManager->getMarker(i);
        if (!marker)
            continue;

        // Apply name filter
        if (m_filterText.isNotEmpty())
        {
            if (!marker->getName().containsIgnoreCase(m_filterText))
                continue;
        }

        FilteredMarker filtered;
        filtered.originalIndex = i;
        filtered.marker = marker;

        // Format position for display
        double timeInSeconds = marker->getPositionInSeconds(m_sampleRate);
        filtered.formattedPosition = formatTimeForDisplay(timeInSeconds);

        m_filteredMarkers.add(filtered);
    }

    // Sort
    sortMarkers();
}

void MarkerListPanel::applyFilter()
{
    updateFilteredMarkers();
}

void MarkerListPanel::sortMarkers()
{
    // Comparator class for sorting markers
    class MarkerComparator
    {
    public:
        MarkerComparator(int columnId, bool forwards, double sampleRate)
            : sortColumnId(columnId), sortForwards(forwards), sampleRate(sampleRate)
        {
        }

        int compareElements(const FilteredMarker& a, const FilteredMarker& b) const
        {
            if (sortColumnId == NameColumn)
            {
                // Sort by name
                int comparison = a.marker->getName().compareIgnoreCase(b.marker->getName());
                return sortForwards ? comparison : -comparison;
            }
            else if (sortColumnId == PositionColumn)
            {
                // Sort by position
                double posA = a.marker->getPosition();
                double posB = b.marker->getPosition();

                if (posA < posB)
                    return sortForwards ? -1 : 1;
                else if (posA > posB)
                    return sortForwards ? 1 : -1;
                else
                    return 0;
            }

            return 0;
        }

    private:
        int sortColumnId;
        bool sortForwards;
        double sampleRate;
    };

    MarkerComparator comparator(m_sortColumnId, m_sortForwards, m_sampleRate);
    m_filteredMarkers.sort(comparator);
}

void MarkerListPanel::jumpToSelectedMarker()
{
    int selectedRow = m_table.getSelectedRow();
    if (selectedRow < 0 || selectedRow >= m_filteredMarkers.size())
        return;

    int originalIndex = m_filteredMarkers[selectedRow].originalIndex;

    if (m_listener)
    {
        m_listener->markerListPanelJumpToMarker(originalIndex);
    }
}

void MarkerListPanel::deleteSelectedMarker()
{
    int selectedRow = m_table.getSelectedRow();
    if (selectedRow < 0 || selectedRow >= m_filteredMarkers.size())
        return;

    int originalIndex = m_filteredMarkers[selectedRow].originalIndex;

    if (m_listener)
    {
        m_listener->markerListPanelMarkerDeleted(originalIndex);
    }

    // Refresh to reflect deletion
    refresh();
}

void MarkerListPanel::startEditingName(int rowNumber)
{
    if (rowNumber < 0 || rowNumber >= m_filteredMarkers.size())
        return;

    // Finish any existing edit
    if (m_nameEditor != nullptr)
    {
        finishEditingName(false);
    }

    // Create new editor
    m_nameEditor = std::make_unique<NameEditor>(*this, rowNumber);
    m_editingRow = rowNumber;

    const auto* marker = m_filteredMarkers[rowNumber].marker;
    m_nameEditor->setText(marker->getName());

    // Position editor in name column
    auto cellBounds = m_table.getCellPosition(NameColumn, rowNumber, true);
    m_nameEditor->setBounds(cellBounds.getX(), cellBounds.getY(),
                           cellBounds.getWidth(), cellBounds.getHeight());

    m_nameEditor->addListener(this);
    m_table.addAndMakeVisible(m_nameEditor.get());
    m_nameEditor->grabKeyboardFocus();
}

void MarkerListPanel::finishEditingName(bool applyChanges)
{
    if (!m_nameEditor || m_editingRow < 0)
        return;

    if (applyChanges)
    {
        juce::String newName = m_nameEditor->getText().trim();

        if (newName.isNotEmpty() && m_editingRow < m_filteredMarkers.size())
        {
            int originalIndex = m_filteredMarkers[m_editingRow].originalIndex;

            if (m_listener)
            {
                m_listener->markerListPanelMarkerRenamed(originalIndex, newName);
            }

            refresh();
        }
    }

    m_nameEditor.reset();
    m_editingRow = -1;
    m_table.grabKeyboardFocus();
}

juce::String MarkerListPanel::formatTimeForDisplay(double timeInSeconds) const
{
    int64_t positionInSamples = static_cast<int64_t>(timeInSeconds * m_sampleRate);

    switch (m_timeFormat)
    {
        case AudioUnits::TimeFormat::Samples:
            return juce::String(positionInSamples);

        case AudioUnits::TimeFormat::Milliseconds:
            return juce::String(timeInSeconds * 1000.0, 3) + " ms";

        case AudioUnits::TimeFormat::Frames:
        {
            // Calculate frame number at 30fps
            int frame = static_cast<int>(timeInSeconds * 30.0);
            return juce::String(frame) + " frames";
        }

        case AudioUnits::TimeFormat::Seconds:
        default:
        {
            int minutes = static_cast<int>(timeInSeconds / 60.0);
            double remainingSeconds = timeInSeconds - (minutes * 60.0);

            return juce::String::formatted("%02d:%06.3f", minutes, remainingSeconds);
        }
    }
}
