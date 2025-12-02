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
    , m_renameTabs(*this, juce::TabbedButtonBar::TabsAtTop)
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
    m_table.setMultipleSelectionEnabled(true);  // Enable multi-selection for batch operations
    m_table.setModel(this);

    // Configure table columns
    auto& header = m_table.getHeader();
    header.addColumn("", ColorColumn, m_colorColumnWidth, m_colorColumnWidth, m_colorColumnWidth,
                    juce::TableHeaderComponent::notSortable);
    header.addColumn("Name", NameColumn, 200, 100, 400);
    header.addColumn("Start", StartColumn, 120, 80, 200);
    header.addColumn("End", EndColumn, 120, 80, 200);
    header.addColumn("Duration", DurationColumn, 120, 80, 200);
    header.addColumn("New Name", NewNameColumn, 200, 100, 400,
                    juce::TableHeaderComponent::notSortable | juce::TableHeaderComponent::visible);
    header.setColumnVisible(NewNameColumn, false);  // Initially hidden, shown when batch rename is active

    header.setColour(juce::TableHeaderComponent::textColourId, m_textColour);
    header.setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour(0xff2a2a2a));
    header.setColour(juce::TableHeaderComponent::highlightColourId, juce::Colour(0xff3a3a3a));

    addAndMakeVisible(m_table);

    // Set up batch rename UI
    m_batchRenameToggleButton.setButtonText("Batch Rename");
    m_batchRenameToggleButton.onClick = [this]()
    {
        expandBatchRenameSection(!m_batchRenameSectionExpanded);
    };
    addAndMakeVisible(m_batchRenameToggleButton);

    // Configure batch rename section
    m_batchRenameSection.setVisible(false);  // Initially collapsed

    // Set up Pattern mode UI
    m_patternLabel.setText("Pattern:", juce::dontSendNotification);
    m_patternLabel.setColour(juce::Label::textColourId, m_textColour);
    m_batchRenameSection.addAndMakeVisible(m_patternLabel);

    m_patternComboBox.addItem("Region {n}", 1);
    m_patternComboBox.addItem("Region {N}", 2);
    m_patternComboBox.addItem("{original} {n}", 3);
    m_patternComboBox.addItem("Custom...", 4);
    m_patternComboBox.setSelectedId(1);
    m_patternComboBox.onChange = [this]()
    {
        if (m_patternComboBox.getSelectedId() == 4)
        {
            m_customPatternEditor.setVisible(true);
            m_patternHelpLabel.setVisible(true);
        }
        else
        {
            m_customPatternEditor.setVisible(false);
            m_patternHelpLabel.setVisible(false);
            m_customPattern = m_patternComboBox.getText();
        }
        updateBatchRenamePreview();
    };
    m_batchRenameSection.addAndMakeVisible(m_patternComboBox);

    m_startNumberLabel.setText("Start:", juce::dontSendNotification);
    m_startNumberLabel.setColour(juce::Label::textColourId, m_textColour);
    m_batchRenameSection.addAndMakeVisible(m_startNumberLabel);

    m_decrementButton.setButtonText("-");
    m_decrementButton.onClick = [this]()
    {
        if (m_startNumber > 0)
        {
            m_startNumber--;
            m_startNumberValue.setText(juce::String(m_startNumber), juce::dontSendNotification);
            updateBatchRenamePreview();
        }
    };
    m_batchRenameSection.addAndMakeVisible(m_decrementButton);

    m_incrementButton.setButtonText("+");
    m_incrementButton.onClick = [this]()
    {
        m_startNumber++;
        m_startNumberValue.setText(juce::String(m_startNumber), juce::dontSendNotification);
        updateBatchRenamePreview();
    };
    m_batchRenameSection.addAndMakeVisible(m_incrementButton);

    m_startNumberValue.setText("1", juce::dontSendNotification);
    m_startNumberValue.setColour(juce::Label::textColourId, m_textColour);
    m_startNumberValue.setJustificationType(juce::Justification::centred);
    m_batchRenameSection.addAndMakeVisible(m_startNumberValue);

    m_customPatternEditor.setMultiLine(false);
    m_customPatternEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    m_customPatternEditor.setColour(juce::TextEditor::textColourId, m_textColour);
    m_customPatternEditor.onTextChange = [this]()
    {
        m_customPattern = m_customPatternEditor.getText();
        updateBatchRenamePreview();
    };
    m_customPatternEditor.setVisible(false);
    m_batchRenameSection.addAndMakeVisible(m_customPatternEditor);

    m_patternHelpLabel.setText("Use {n} for numbers, {N} for zero-padded, {original} for original name",
                               juce::dontSendNotification);
    m_patternHelpLabel.setColour(juce::Label::textColourId, juce::Colour(0xff888888));
    m_patternHelpLabel.setFont(juce::Font(11.0f));
    m_patternHelpLabel.setVisible(false);
    m_batchRenameSection.addAndMakeVisible(m_patternHelpLabel);

    // Set up Find/Replace mode UI
    m_findLabel.setText("Find:", juce::dontSendNotification);
    m_findLabel.setColour(juce::Label::textColourId, m_textColour);
    m_batchRenameSection.addAndMakeVisible(m_findLabel);

    m_findEditor.setMultiLine(false);
    m_findEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    m_findEditor.setColour(juce::TextEditor::textColourId, m_textColour);
    m_findEditor.onTextChange = [this]()
    {
        m_findText = m_findEditor.getText();
        updateBatchRenamePreview();
    };
    m_batchRenameSection.addAndMakeVisible(m_findEditor);

    m_replaceLabel.setText("Replace:", juce::dontSendNotification);
    m_replaceLabel.setColour(juce::Label::textColourId, m_textColour);
    m_batchRenameSection.addAndMakeVisible(m_replaceLabel);

    m_replaceEditor.setMultiLine(false);
    m_replaceEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    m_replaceEditor.setColour(juce::TextEditor::textColourId, m_textColour);
    m_replaceEditor.onTextChange = [this]()
    {
        m_replaceText = m_replaceEditor.getText();
        updateBatchRenamePreview();
    };
    m_batchRenameSection.addAndMakeVisible(m_replaceEditor);

    m_caseSensitiveToggle.setButtonText("Case Sensitive");
    m_caseSensitiveToggle.setColour(juce::ToggleButton::textColourId, m_textColour);
    m_caseSensitiveToggle.onClick = [this]()
    {
        m_caseSensitive = m_caseSensitiveToggle.getToggleState();
        updateBatchRenamePreview();
    };
    m_batchRenameSection.addAndMakeVisible(m_caseSensitiveToggle);

    m_replaceAllToggle.setButtonText("Replace All Occurrences");
    m_replaceAllToggle.setColour(juce::ToggleButton::textColourId, m_textColour);
    m_replaceAllToggle.setToggleState(true, juce::dontSendNotification);
    m_replaceAllToggle.onClick = [this]()
    {
        m_replaceAll = m_replaceAllToggle.getToggleState();
        updateBatchRenamePreview();
    };
    m_batchRenameSection.addAndMakeVisible(m_replaceAllToggle);

    // Set up Prefix/Suffix mode UI
    m_prefixLabel.setText("Prefix:", juce::dontSendNotification);
    m_prefixLabel.setColour(juce::Label::textColourId, m_textColour);
    m_batchRenameSection.addAndMakeVisible(m_prefixLabel);

    m_prefixEditor.setMultiLine(false);
    m_prefixEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    m_prefixEditor.setColour(juce::TextEditor::textColourId, m_textColour);
    m_prefixEditor.onTextChange = [this]()
    {
        m_prefixText = m_prefixEditor.getText();
        updateBatchRenamePreview();
    };
    m_batchRenameSection.addAndMakeVisible(m_prefixEditor);

    m_suffixLabel.setText("Suffix:", juce::dontSendNotification);
    m_suffixLabel.setColour(juce::Label::textColourId, m_textColour);
    m_batchRenameSection.addAndMakeVisible(m_suffixLabel);

    m_suffixEditor.setMultiLine(false);
    m_suffixEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    m_suffixEditor.setColour(juce::TextEditor::textColourId, m_textColour);
    m_suffixEditor.onTextChange = [this]()
    {
        m_suffixText = m_suffixEditor.getText();
        updateBatchRenamePreview();
    };
    m_batchRenameSection.addAndMakeVisible(m_suffixEditor);

    m_addNumberingToggle.setButtonText("Add Sequential Numbering");
    m_addNumberingToggle.setColour(juce::ToggleButton::textColourId, m_textColour);
    m_addNumberingToggle.onClick = [this]()
    {
        m_addNumbering = m_addNumberingToggle.getToggleState();
        updateBatchRenamePreview();
    };
    m_batchRenameSection.addAndMakeVisible(m_addNumberingToggle);

    // OLD PREVIEW (no longer used - preview now shown in "New Name" column)
    // m_previewLabel.setText("Preview:", juce::dontSendNotification);
    // m_previewLabel.setColour(juce::Label::textColourId, m_textColour);
    // m_batchRenameSection.addAndMakeVisible(m_previewLabel);

    // m_previewList.setMultiLine(true);
    // m_previewList.setReadOnly(true);
    // m_previewList.setScrollbarsShown(true);
    // m_previewList.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1a1a));
    // m_previewList.setColour(juce::TextEditor::textColourId, m_textColour);
    // m_previewList.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    // m_batchRenameSection.addAndMakeVisible(m_previewList);

    // Set up action buttons

    m_applyButton.setButtonText("Apply");
    m_applyButton.onClick = [this]() { applyBatchRename(); };
    m_batchRenameSection.addAndMakeVisible(m_applyButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]() { cancelBatchRename(); };
    m_batchRenameSection.addAndMakeVisible(m_cancelButton);

    // Set up tabbed component with three mode tabs
    auto* patternTab = new juce::Component();
    patternTab->addAndMakeVisible(m_patternLabel);
    patternTab->addAndMakeVisible(m_patternComboBox);
    patternTab->addAndMakeVisible(m_startNumberLabel);
    patternTab->addAndMakeVisible(m_decrementButton);
    patternTab->addAndMakeVisible(m_incrementButton);
    patternTab->addAndMakeVisible(m_startNumberValue);
    patternTab->addAndMakeVisible(m_customPatternEditor);
    patternTab->addAndMakeVisible(m_patternHelpLabel);

    auto* findReplaceTab = new juce::Component();
    findReplaceTab->addAndMakeVisible(m_findLabel);
    findReplaceTab->addAndMakeVisible(m_findEditor);
    findReplaceTab->addAndMakeVisible(m_replaceLabel);
    findReplaceTab->addAndMakeVisible(m_replaceEditor);
    findReplaceTab->addAndMakeVisible(m_caseSensitiveToggle);
    findReplaceTab->addAndMakeVisible(m_replaceAllToggle);

    auto* prefixSuffixTab = new juce::Component();
    prefixSuffixTab->addAndMakeVisible(m_prefixLabel);
    prefixSuffixTab->addAndMakeVisible(m_prefixEditor);
    prefixSuffixTab->addAndMakeVisible(m_suffixLabel);
    prefixSuffixTab->addAndMakeVisible(m_suffixEditor);
    prefixSuffixTab->addAndMakeVisible(m_addNumberingToggle);

    m_renameTabs.addTab("Pattern", juce::Colour(0xff2a2a2a), patternTab, true);
    m_renameTabs.addTab("Find/Replace", juce::Colour(0xff2a2a2a), findReplaceTab, true);
    m_renameTabs.addTab("Prefix/Suffix", juce::Colour(0xff2a2a2a), prefixSuffixTab, true);
    m_renameTabs.setCurrentTabIndex(0);
    m_batchRenameSection.addAndMakeVisible(m_renameTabs);

    addChildComponent(m_batchRenameSection);  // addChildComponent = not visible by default

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

void RegionListPanel::setCommandManager(juce::ApplicationCommandManager* commandManager)
{
    m_commandManager = commandManager;
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
    m_table.repaint();  // Force immediate visual update
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

std::vector<int> RegionListPanel::getSelectedRegionIndices() const
{
    std::vector<int> indices;

    // Get all selected rows from the table
    const auto selectedRows = m_table.getSelectedRows();

    // Convert filtered row indices to original region indices
    for (int i = 0; i < selectedRows.size(); ++i)
    {
        int row = selectedRows[i];
        if (row >= 0 && row < m_filteredRegions.size())
        {
            indices.push_back(m_filteredRegions[row].originalIndex);
        }
    }

    return indices;
}

juce::DocumentWindow* RegionListPanel::showInWindow(bool modal)
{
    // Custom window class that properly handles the close button and global keyboard shortcuts
    // Uses JUCE's ApplicationCommandTarget chain for proper command routing
    class RegionListWindow : public juce::DocumentWindow,
                             public juce::ApplicationCommandTarget
    {
    public:
        RegionListWindow(const juce::String& name, juce::Colour backgroundColour, int requiredButtons,
                        juce::ApplicationCommandManager* cmdManager)
            : DocumentWindow(name, backgroundColour, requiredButtons)
            , m_commandManager(cmdManager)
            , m_mainCommandTarget(nullptr)
        {
            // CRITICAL: Add KeyListener to enable keyboard shortcuts in this window
            // This connects keyboard events → KeyPressMappingSet → Commands
            if (m_commandManager != nullptr)
            {
                addKeyListener(m_commandManager->getKeyMappings());

                // Store the main command target for command chain routing
                // Use a dummy command ID to get the first target (MainComponent)
                m_mainCommandTarget = m_commandManager->getFirstCommandTarget(0);
            }
        }

        ~RegionListWindow() override
        {
            // Clean up the key listener on destruction
            if (m_commandManager != nullptr)
                removeKeyListener(m_commandManager->getKeyMappings());
        }

        void closeButtonPressed() override
        {
            // Hide the window instead of deleting it (can be reopened)
            setVisible(false);
        }

        //==============================================================================
        // ApplicationCommandTarget overrides - forward all commands to MainComponent

        juce::ApplicationCommandTarget* getNextCommandTarget() override
        {
            // CRITICAL: Chain to MainComponent so it can handle all commands
            // When JUCE can't find a handler in this window, it follows this chain
            // Return the stored main target (captured in constructor)
            return m_mainCommandTarget;
        }

        void getAllCommands(juce::Array<juce::CommandID>& commands) override
        {
            // We don't define our own commands - they're all in MainComponent
            (void)commands;
        }

        void getCommandInfo(juce::CommandID /*commandID*/, juce::ApplicationCommandInfo& /*result*/) override
        {
            // We don't define command info - MainComponent does
        }

        bool perform(const InvocationInfo& /*info*/) override
        {
            // We don't handle any commands ourselves
            // Return false so JUCE calls getNextCommandTarget() and tries MainComponent
            return false;
        }

    private:
        juce::ApplicationCommandManager* m_commandManager;
        juce::ApplicationCommandTarget* m_mainCommandTarget;  // Main command target for routing
    };

    auto* window = new RegionListWindow("Region List",
                                        juce::Colour(0xff2a2a2a),
                                        juce::DocumentWindow::allButtons,
                                        m_commandManager);

    window->setContentOwned(this, true);
    window->setResizable(true, true);
    window->setResizeLimits(700, 700, 1400, 1000);  // Increased minimums to accommodate batch rename section
    window->centreWithSize(900, 800);  // Larger default size for better visibility

    // Note: We DON'T call setFirstCommandTarget() here!
    // JUCE automatically routes commands based on which window has focus.
    // Calling setFirstCommandTarget() would break main window shortcuts.

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

    // Top toolbar: Search bar + Batch Rename button
    auto toolbarBounds = bounds.removeFromTop(40).reduced(8);
    m_batchRenameToggleButton.setBounds(toolbarBounds.removeFromRight(120));
    toolbarBounds.removeFromRight(8);  // Spacing

    m_searchLabel.setBounds(toolbarBounds.removeFromLeft(60));
    toolbarBounds.removeFromLeft(4);
    m_searchBox.setBounds(toolbarBounds);

    bounds.removeFromTop(4);  // Spacing after toolbar

    // Batch rename section (if expanded)
    if (m_batchRenameSectionExpanded && m_batchRenameSection.isVisible())
    {
        auto batchRenameBounds = bounds.removeFromTop(280);  // Height for tabs + buttons (preview now in table column)
        m_batchRenameSection.setBounds(batchRenameBounds);

        // Layout batch rename section components
        auto sectionBounds = batchRenameBounds.reduced(8);

        // Tabs at top (takes ~200px - reduced to give more space to preview)
        auto tabsBounds = sectionBounds.removeFromTop(200);
        m_renameTabs.setBounds(tabsBounds);

        // Layout tab contents
        auto* patternTab = m_renameTabs.getTabContentComponent(0);
        if (patternTab)
        {
            auto patternBounds = patternTab->getLocalBounds().reduced(8);

            // Pattern combo box row
            auto comboRow = patternBounds.removeFromTop(28);
            m_patternLabel.setBounds(comboRow.removeFromLeft(60));
            comboRow.removeFromLeft(4);
            m_patternComboBox.setBounds(comboRow.removeFromLeft(200));

            patternBounds.removeFromTop(8);  // Spacing

            // Start number controls row
            auto numberRow = patternBounds.removeFromTop(28);
            m_startNumberLabel.setBounds(numberRow.removeFromLeft(60));
            numberRow.removeFromLeft(4);
            m_decrementButton.setBounds(numberRow.removeFromLeft(30));
            numberRow.removeFromLeft(4);
            m_startNumberValue.setBounds(numberRow.removeFromLeft(60));
            numberRow.removeFromLeft(4);
            m_incrementButton.setBounds(numberRow.removeFromLeft(30));

            patternBounds.removeFromTop(8);  // Spacing

            // Custom pattern editor (if visible)
            if (m_customPatternEditor.isVisible())
            {
                m_customPatternEditor.setBounds(patternBounds.removeFromTop(28));
                patternBounds.removeFromTop(4);
                m_patternHelpLabel.setBounds(patternBounds.removeFromTop(20));
            }
        }

        auto* findReplaceTab = m_renameTabs.getTabContentComponent(1);
        if (findReplaceTab)
        {
            auto findReplaceBounds = findReplaceTab->getLocalBounds().reduced(8);

            // Find row
            auto findRow = findReplaceBounds.removeFromTop(28);
            m_findLabel.setBounds(findRow.removeFromLeft(60));
            findRow.removeFromLeft(4);
            m_findEditor.setBounds(findRow);

            findReplaceBounds.removeFromTop(8);  // Spacing

            // Replace row
            auto replaceRow = findReplaceBounds.removeFromTop(28);
            m_replaceLabel.setBounds(replaceRow.removeFromLeft(60));
            replaceRow.removeFromLeft(4);
            m_replaceEditor.setBounds(replaceRow);

            findReplaceBounds.removeFromTop(8);  // Spacing

            // Toggles
            m_caseSensitiveToggle.setBounds(findReplaceBounds.removeFromTop(24));
            findReplaceBounds.removeFromTop(4);
            m_replaceAllToggle.setBounds(findReplaceBounds.removeFromTop(24));
        }

        auto* prefixSuffixTab = m_renameTabs.getTabContentComponent(2);
        if (prefixSuffixTab)
        {
            auto prefixSuffixBounds = prefixSuffixTab->getLocalBounds().reduced(8);

            // Prefix row
            auto prefixRow = prefixSuffixBounds.removeFromTop(28);
            m_prefixLabel.setBounds(prefixRow.removeFromLeft(60));
            prefixRow.removeFromLeft(4);
            m_prefixEditor.setBounds(prefixRow);

            prefixSuffixBounds.removeFromTop(8);  // Spacing

            // Suffix row
            auto suffixRow = prefixSuffixBounds.removeFromTop(28);
            m_suffixLabel.setBounds(suffixRow.removeFromLeft(60));
            suffixRow.removeFromLeft(4);
            m_suffixEditor.setBounds(suffixRow);

            prefixSuffixBounds.removeFromTop(8);  // Spacing

            // Add numbering toggle
            m_addNumberingToggle.setBounds(prefixSuffixBounds.removeFromTop(24));
        }

        sectionBounds.removeFromTop(8);  // Spacing after tabs

        // OLD PREVIEW (no longer used - preview now shown in "New Name" column)
        // m_previewLabel.setBounds(sectionBounds.removeFromTop(20));
        // sectionBounds.removeFromTop(4);
        // m_previewList.setBounds(sectionBounds);

        // Apply/Cancel buttons at bottom
        auto buttonHeight = 32;
        auto buttonRow = sectionBounds.removeFromTop(buttonHeight);

        // Layout buttons
        m_cancelButton.setBounds(buttonRow.removeFromRight(80));
        buttonRow.removeFromRight(8);
        m_applyButton.setBounds(buttonRow.removeFromRight(80));

        bounds.removeFromTop(8);  // Spacing after batch rename section
    }

    // Table fills remaining space
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

        case NewNameColumn:
        {
            // Show the preview of the new name after all batch operations are applied
            juce::String newName = generateNewName(filteredRegion.originalIndex, *region);
            g.setColour(juce::Colours::lightgreen);  // Distinguish preview text
            g.drawText(newName, 4, 0, width - 8, height,
                      juce::Justification::centredLeft, true);
            break;
        }
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

void RegionListPanel::backgroundClicked(const juce::MouseEvent& event)
{
    // Show context menu on right-click when multiple regions are selected
    if (event.mods.isRightButtonDown())
    {
        const int numSelected = m_table.getNumSelectedRows();

        if (numSelected >= 2)
        {
            juce::PopupMenu menu;
            menu.addItem(1, juce::String::formatted("Batch Rename... (%d regions)", numSelected));

            menu.showMenuAsync(juce::PopupMenu::Options(),
                [this](int result)
                {
                    if (result == 1 && m_listener)
                    {
                        // Get selected region indices
                        auto selectedIndices = getSelectedRegionIndices();
                        m_listener->regionListPanelBatchRename(selectedIndices);
                    }
                });
        }
    }
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
// Tab change callback
//==============================================================================

void RegionListPanel::onTabChanged(int newTabIndex)
{
    // Update current rename mode based on selected tab
    switch (newTabIndex)
    {
        case 0: m_currentRenameMode = RenameMode::Pattern; break;
        case 1: m_currentRenameMode = RenameMode::FindReplace; break;
        case 2: m_currentRenameMode = RenameMode::PrefixSuffix; break;
        default: m_currentRenameMode = RenameMode::Pattern; break;
    }

    // Update preview to reflect the new mode
    updateBatchRenamePreview();
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
        int regionIndex = selectedIndices[i];
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