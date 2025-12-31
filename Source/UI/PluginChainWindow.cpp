/*
  ==============================================================================

    PluginChainWindow.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "PluginChainWindow.h"

//==============================================================================
// PluginRowComponent implementation
//==============================================================================
PluginChainWindow::PluginRowComponent::PluginRowComponent(PluginChainWindow& owner)
    : m_owner(owner)
{
    // Move Up button
    m_moveUpButton.setButtonText("^");
    m_moveUpButton.setTooltip("Move plugin up in chain");
    m_moveUpButton.setMouseClickGrabsKeyboardFocus(false);
    m_moveUpButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff505050));
    m_moveUpButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    m_moveUpButton.onClick = [this]()
    {
        if (m_index > 0 && m_owner.m_listener != nullptr)
        {
            m_owner.m_listener->pluginChainWindowPluginMoved(m_index, m_index - 1);
        }
    };
    addAndMakeVisible(m_moveUpButton);

    // Move Down button
    m_moveDownButton.setButtonText("v");
    m_moveDownButton.setTooltip("Move plugin down in chain");
    m_moveDownButton.setMouseClickGrabsKeyboardFocus(false);
    m_moveDownButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff505050));
    m_moveDownButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    m_moveDownButton.onClick = [this]()
    {
        // Simply check if button is enabled - updateMoveButtonStates() handles the logic
        // Note: We pass m_index + 2 because movePlugin() uses "insert position" semantics
        // where it decrements toIndex when toIndex > fromIndex to account for the removed element.
        // So to move from position 0 to position 1, we pass (0, 2) which becomes (0, 1) after adjustment.
        if (m_moveDownButton.isEnabled() && m_index >= 0 && m_owner.m_listener != nullptr)
        {
            m_owner.m_listener->pluginChainWindowPluginMoved(m_index, m_index + 2);
        }
    };
    addAndMakeVisible(m_moveDownButton);

    m_bypassButton.setButtonText("Bypass");
    m_bypassButton.setTooltip("Bypass this plugin (disable effect processing)");
    m_bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff505050));
    m_bypassButton.onClick = [this]()
    {
        if (m_node != nullptr && m_index >= 0)
        {
            bool newBypassed = !m_node->isBypassed();
            m_node->setBypassed(newBypassed);
            updateBypassButtonAppearance(newBypassed);
            if (m_owner.m_listener != nullptr)
                m_owner.m_listener->pluginChainWindowPluginBypassed(m_index, newBypassed);
        }
    };
    addAndMakeVisible(m_bypassButton);

    m_editButton.setButtonText("Edit");
    m_editButton.setTooltip("Open plugin editor");
    m_editButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff404040));
    m_editButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff505050));
    m_editButton.onClick = [this]()
    {
        if (m_index >= 0 && m_owner.m_listener != nullptr)
            m_owner.m_listener->pluginChainWindowEditPlugin(m_index);
    };
    addAndMakeVisible(m_editButton);

    m_removeButton.setButtonText("X");
    m_removeButton.setTooltip("Remove plugin from chain");
    m_removeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff404040));
    m_removeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff605050));
    m_removeButton.onClick = [this]()
    {
        if (m_index >= 0 && m_owner.m_listener != nullptr)
            m_owner.m_listener->pluginChainWindowPluginRemoved(m_index);
    };
    addAndMakeVisible(m_removeButton);

    m_nameLabel.setFont(juce::FontOptions(14.0f));
    m_nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe0e0e0));
    m_nameLabel.setInterceptsMouseClicks(false, false);  // Allow drag-through
    addAndMakeVisible(m_nameLabel);

    m_latencyLabel.setFont(juce::FontOptions(11.0f));
    m_latencyLabel.setColour(juce::Label::textColourId, juce::Colour(0xff909090));
    m_latencyLabel.setInterceptsMouseClicks(false, false);  // Allow drag-through
    addAndMakeVisible(m_latencyLabel);
}

void PluginChainWindow::PluginRowComponent::update(int index, PluginChainNode* node, int totalCount)
{
    m_index = index;
    m_node = node;

    if (node != nullptr)
    {
        m_nameLabel.setText(node->getName(), juce::dontSendNotification);
        int latency = node->getLatencySamples();
        if (latency > 0)
            m_latencyLabel.setText(juce::String(latency) + " samples latency", juce::dontSendNotification);
        else
            m_latencyLabel.setText("", juce::dontSendNotification);

        updateBypassButtonAppearance(node->isBypassed());
        updateMoveButtonStates(index, totalCount);
    }
    else
    {
        m_nameLabel.setText("", juce::dontSendNotification);
        m_latencyLabel.setText("", juce::dontSendNotification);
    }
}

void PluginChainWindow::PluginRowComponent::updateMoveButtonStates(int index, int totalCount)
{
    // First plugin can't move up
    m_moveUpButton.setEnabled(index > 0);

    // Last plugin can't move down
    m_moveDownButton.setEnabled(index < totalCount - 1);
}

void PluginChainWindow::PluginRowComponent::updateBypassButtonAppearance(bool isBypassed)
{
    if (isBypassed)
    {
        // Bypassed state: orange background (consistent with PluginChainPanel)
        m_bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffcc8800));
        m_bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffdd9900));
        m_bypassButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
    else
    {
        // Active state: normal button appearance
        m_bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff404040));
        m_bypassButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff505050));
        m_bypassButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe0e0e0));
    }
}

void PluginChainWindow::PluginRowComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));
}

void PluginChainWindow::PluginRowComponent::resized()
{
    auto bounds = getLocalBounds().reduced(4);

    // Move Up/Down buttons on left
    auto moveButtonArea = bounds.removeFromLeft(56);
    m_moveUpButton.setBounds(moveButtonArea.removeFromLeft(26).reduced(2));
    m_moveDownButton.setBounds(moveButtonArea.reduced(2));

    // Buttons on right
    auto buttonArea = bounds.removeFromRight(200);
    m_removeButton.setBounds(buttonArea.removeFromRight(30).reduced(2));
    m_editButton.setBounds(buttonArea.removeFromRight(50).reduced(2));
    m_bypassButton.setBounds(buttonArea.removeFromRight(70).reduced(2));

    // Name and latency in middle
    auto labelArea = bounds;
    m_nameLabel.setBounds(labelArea.removeFromTop(22));
    m_latencyLabel.setBounds(labelArea);
}

void PluginChainWindow::PluginRowComponent::mouseDown(const juce::MouseEvent& e)
{
    m_dragStarted = false;
    m_dragStartPos = e.getPosition();
}

void PluginChainWindow::PluginRowComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!m_dragStarted && e.getDistanceFromDragStart() > 5)
    {
        m_dragStarted = true;
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        {
            container->startDragging(juce::var(m_index), this);
        }
    }
}

//==============================================================================
// DraggableListBox implementation
//==============================================================================
PluginChainWindow::DraggableListBox::DraggableListBox(PluginChainWindow& owner)
    : juce::ListBox("Chain", &owner), m_owner(owner)
{
}

bool PluginChainWindow::DraggableListBox::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.isInt();
}

void PluginChainWindow::DraggableListBox::itemDragEnter(const SourceDetails& details)
{
    m_dropInsertIndex = getInsertIndexForPosition(details.localPosition.y);
    repaint();
}

void PluginChainWindow::DraggableListBox::itemDragMove(const SourceDetails& details)
{
    m_dropInsertIndex = getInsertIndexForPosition(details.localPosition.y);
    repaint();
}

void PluginChainWindow::DraggableListBox::itemDragExit(const SourceDetails&)
{
    m_dropInsertIndex = -1;
    repaint();
}

void PluginChainWindow::DraggableListBox::itemDropped(const SourceDetails& details)
{
    int fromIndex = static_cast<int>(details.description);
    int toIndex = getInsertIndexForPosition(details.localPosition.y);

    // PluginChain::movePlugin() uses "insert before" semantics:
    // - It removes the item from fromIndex
    // - Then internally does --toIndex if toIndex > fromIndex (to account for shift)
    // - Then inserts at toIndex
    //
    // For drag-and-drop, we want to pass the raw insert position.
    // The no-op conditions are:
    // - toIndex == fromIndex (dropping on self, same position)
    // - toIndex == fromIndex + 1 (dropping just below self, same position after adjustment)

    if (toIndex != fromIndex && toIndex != fromIndex + 1)
    {
        if (m_owner.m_listener != nullptr)
            m_owner.m_listener->pluginChainWindowPluginMoved(fromIndex, toIndex);
    }

    m_dropInsertIndex = -1;
    repaint();
}

void PluginChainWindow::DraggableListBox::paint(juce::Graphics& g)
{
    juce::ListBox::paint(g);
}

void PluginChainWindow::DraggableListBox::paintOverChildren(juce::Graphics& g)
{
    // Draw drop indicator on top of all children (including viewport content)
    if (m_dropInsertIndex >= 0)
    {
        // Get the viewport to account for scroll position
        auto* viewport = getViewport();
        int scrollOffset = viewport != nullptr ? viewport->getViewPositionY() : 0;

        int y = (m_dropInsertIndex * m_owner.m_chainRowHeight) - scrollOffset;

        // Only draw if visible
        if (y >= 0 && y <= getHeight())
        {
            g.setColour(m_owner.m_accentColour);
            g.fillRect(0, y - 2, getWidth(), 4);
        }
    }
}

int PluginChainWindow::DraggableListBox::getInsertIndexForPosition(int y) const
{
    int row = y / m_owner.m_chainRowHeight;
    int offsetInRow = y % m_owner.m_chainRowHeight;

    if (offsetInRow > m_owner.m_chainRowHeight / 2)
        row++;

    return juce::jlimit(0, m_owner.getNumRows(), row);
}

//==============================================================================
// PluginChainWindow implementation
//==============================================================================
PluginChainWindow::PluginChainWindow(PluginChain* chain)
    : m_chain(chain),
      m_chainListBox(*this),
      m_browserTableModel(*this),
      m_scanProgressBar(m_scanProgress)
{
    // Chain panel title
    m_chainTitleLabel.setText("Plugin Chain", juce::dontSendNotification);
    m_chainTitleLabel.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    m_chainTitleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_chainTitleLabel);

    // Chain list box
    m_chainListBox.setModel(this);
    m_chainListBox.setColour(juce::ListBox::backgroundColourId, m_backgroundColour);
    m_chainListBox.setRowHeight(m_chainRowHeight);
    m_chainListBox.setMultipleSelectionEnabled(false);
    addAndMakeVisible(m_chainListBox);

    // Empty chain label
    m_emptyChainLabel.setText("No plugins in chain.\nDouble-click a plugin in the browser\nto add it to the chain.",
                               juce::dontSendNotification);
    m_emptyChainLabel.setJustificationType(juce::Justification::centred);
    m_emptyChainLabel.setColour(juce::Label::textColourId, juce::Colour(0xff909090));
    addAndMakeVisible(m_emptyChainLabel);

    // Latency display
    m_latencyLabel.setText("Total Latency: 0 samples", juce::dontSendNotification);
    m_latencyLabel.setFont(juce::FontOptions(12.0f));
    m_latencyLabel.setColour(juce::Label::textColourId, juce::Colour(0xff909090));
    addAndMakeVisible(m_latencyLabel);

    // Bypass all toggle
    m_bypassAllButton.setButtonText("Bypass All");
    m_bypassAllButton.setTooltip("Bypass all plugins in the chain");
    m_bypassAllButton.setColour(juce::ToggleButton::tickColourId, m_accentColour);
    m_bypassAllButton.onClick = [this]() { onBypassAllClicked(); };
    addAndMakeVisible(m_bypassAllButton);

    // Apply to Selection button
    m_applyToSelectionButton.setButtonText("Apply to Selection (Cmd+P)");
    m_applyToSelectionButton.setTooltip("Permanently apply plugin chain effects to selected audio");
    m_applyToSelectionButton.onClick = [this]() { onApplyToSelectionClicked(); };
    m_applyToSelectionButton.setColour(juce::TextButton::buttonColourId, m_accentColour);
    m_applyToSelectionButton.setColour(juce::TextButton::buttonOnColourId, m_accentColour.brighter(0.2f));
    addAndMakeVisible(m_applyToSelectionButton);

    // Render Options group
    m_renderOptionsGroup.setText("Render Options");
    m_renderOptionsGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xff555555));
    m_renderOptionsGroup.setColour(juce::GroupComponent::textColourId, m_textColour);
    addAndMakeVisible(m_renderOptionsGroup);

    m_convertToStereoCheckbox.setButtonText("Convert to Stereo");
    m_convertToStereoCheckbox.setTooltip("Convert mono audio to stereo before processing (preserves stereo effects from plugins)");
    m_convertToStereoCheckbox.setEnabled(false);  // Disabled until we know if source is mono
    addAndMakeVisible(m_convertToStereoCheckbox);

    m_includeTailCheckbox.setButtonText("Include Effect Tail");
    m_includeTailCheckbox.setTooltip("Extend the selection to capture reverb/delay tails");
    m_includeTailCheckbox.onClick = [this]()
    {
        m_tailLengthSlider.setEnabled(m_includeTailCheckbox.getToggleState());
        m_tailLengthLabel.setEnabled(m_includeTailCheckbox.getToggleState());
    };
    addAndMakeVisible(m_includeTailCheckbox);

    m_tailLengthLabel.setText("Tail:", juce::dontSendNotification);
    m_tailLengthLabel.setEnabled(false);
    addAndMakeVisible(m_tailLengthLabel);

    m_tailLengthSlider.setRange(0.5, 10.0, 0.5);
    m_tailLengthSlider.setValue(2.0);
    m_tailLengthSlider.setTextValueSuffix(" sec");
    m_tailLengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_tailLengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    m_tailLengthSlider.setEnabled(false);
    addAndMakeVisible(m_tailLengthSlider);

    // Browser panel title
    m_browserTitleLabel.setText("Available Plugins", juce::dontSendNotification);
    m_browserTitleLabel.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    m_browserTitleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_browserTitleLabel);

    // Search box
    m_searchLabel.setText("Search:", juce::dontSendNotification);
    addAndMakeVisible(m_searchLabel);

    m_searchBox.setTextToShowWhenEmpty("Filter plugins...", juce::Colours::grey);
    m_searchBox.onTextChange = [this]() { onSearchTextChanged(); };
    addAndMakeVisible(m_searchBox);

    // Category filter
    m_categoryLabel.setText("Category:", juce::dontSendNotification);
    addAndMakeVisible(m_categoryLabel);

    m_categoryComboBox.addItem("All", 1);
    m_categoryComboBox.setSelectedId(1);
    m_categoryComboBox.onChange = [this]() { onCategoryFilterChanged(); };
    addAndMakeVisible(m_categoryComboBox);

    // Manufacturer filter
    m_manufacturerLabel.setText("Manufacturer:", juce::dontSendNotification);
    addAndMakeVisible(m_manufacturerLabel);

    m_manufacturerComboBox.addItem("All", 1);
    m_manufacturerComboBox.setSelectedId(1);
    m_manufacturerComboBox.onChange = [this]() { onManufacturerFilterChanged(); };
    addAndMakeVisible(m_manufacturerComboBox);

    // Browser table
    m_browserTable.setModel(&m_browserTableModel);
    m_browserTable.setColour(juce::ListBox::backgroundColourId, m_backgroundColour);
    m_browserTable.setRowHeight(m_browserRowHeight);
    m_browserTable.setMultipleSelectionEnabled(false);
    m_browserTable.getHeader().setStretchToFitActive(true);

    m_browserTable.getHeader().addColumn("Name", NameColumn, 200, 100, 300,
                                          juce::TableHeaderComponent::defaultFlags);
    m_browserTable.getHeader().addColumn("Manufacturer", ManufacturerColumn, 120, 80, 200,
                                          juce::TableHeaderComponent::defaultFlags);
    m_browserTable.getHeader().addColumn("Category", CategoryColumn, 80, 60, 150,
                                          juce::TableHeaderComponent::defaultFlags);
    m_browserTable.getHeader().addColumn("Type", FormatColumn, 60, 40, 80,
                                          juce::TableHeaderComponent::defaultFlags);
    m_browserTable.getHeader().setSortColumnId(NameColumn, true);
    addAndMakeVisible(m_browserTable);

    // Empty search results label
    m_emptySearchLabel.setText("No plugins match your search.\nTry adjusting your filters.",
                                juce::dontSendNotification);
    m_emptySearchLabel.setJustificationType(juce::Justification::centred);
    m_emptySearchLabel.setColour(juce::Label::textColourId, juce::Colour(0xff909090));
    m_emptySearchLabel.setVisible(false);
    addAndMakeVisible(m_emptySearchLabel);

    // Rescan button
    m_rescanButton.setButtonText("Rescan Plugins");
    m_rescanButton.setTooltip("Scan for new or updated plugins");
    m_rescanButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff404040));
    m_rescanButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff505050));
    m_rescanButton.onClick = [this]() { onRescanClicked(); };
    addAndMakeVisible(m_rescanButton);

    // Scan progress
    m_scanStatusLabel.setText("", juce::dontSendNotification);
    addAndMakeVisible(m_scanStatusLabel);

    m_scanProgressBar.setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xff333333));
    m_scanProgressBar.setColour(juce::ProgressBar::foregroundColourId, m_accentColour);
    addAndMakeVisible(m_scanProgressBar);
    m_scanProgressBar.setVisible(false);

    // Listen for chain changes
    if (m_chain != nullptr)
        m_chain->addChangeListener(this);

    // Initial refresh
    refresh();

    setSize(900, 600);
}

PluginChainWindow::~PluginChainWindow()
{
    stopTimer();

    if (m_chain != nullptr)
        m_chain->removeChangeListener(this);
}

void PluginChainWindow::refresh()
{
    m_chainListBox.updateContent();
    updateLatencyDisplay();

    bool isEmpty = (m_chain == nullptr || m_chain->isEmpty());
    m_emptyChainLabel.setVisible(isEmpty);

    refreshBrowser();
}

void PluginChainWindow::refreshBrowser()
{
    auto& pluginManager = PluginManager::getInstance();

    // Get all effect plugins (filter out instruments)
    auto allAvailable = pluginManager.getAvailablePlugins();
    m_allPlugins.clear();

    for (const auto& desc : allAvailable)
    {
        if (!desc.isInstrument)
            m_allPlugins.add(desc);
    }

    // Build category and manufacturer lists
    juce::StringArray categories;
    juce::StringArray manufacturers;

    for (const auto& desc : m_allPlugins)
    {
        if (desc.category.isNotEmpty() && !categories.contains(desc.category))
            categories.add(desc.category);

        if (desc.manufacturerName.isNotEmpty() && !manufacturers.contains(desc.manufacturerName))
            manufacturers.add(desc.manufacturerName);
    }

    categories.sort(true);
    manufacturers.sort(true);

    // Update combo boxes
    m_categoryComboBox.clear();
    m_categoryComboBox.addItem("All", 1);
    int id = 2;
    for (const auto& cat : categories)
        m_categoryComboBox.addItem(cat, id++);
    m_categoryComboBox.setSelectedId(1, juce::dontSendNotification);

    m_manufacturerComboBox.clear();
    m_manufacturerComboBox.addItem("All", 1);
    id = 2;
    for (const auto& mfr : manufacturers)
        m_manufacturerComboBox.addItem(mfr, id++);
    m_manufacturerComboBox.setSelectedId(1, juce::dontSendNotification);

    m_categoryFilter.clear();
    m_manufacturerFilter.clear();
    m_filterText.clear();

    updateFilteredPlugins();
}

juce::DocumentWindow* PluginChainWindow::showInWindow(juce::ApplicationCommandManager* commandManager)
{
    /**
     * Custom DocumentWindow that routes keyboard shortcuts to the main application.
     * This allows shortcuts like Space (play/stop), Cmd+P (apply chain), etc.
     * to work even when focus is in the Plugin Chain window.
     */
    class PluginChainDocumentWindow : public juce::DocumentWindow,
                                       public juce::ApplicationCommandTarget
    {
    public:
        PluginChainDocumentWindow(const juce::String& name, juce::Colour backgroundColour,
                                   int requiredButtons, juce::ApplicationCommandManager* cmdManager)
            : DocumentWindow(name, backgroundColour, requiredButtons)
            , m_commandManager(cmdManager)
            , m_mainCommandTarget(nullptr)
        {
            // CRITICAL: Add KeyListener to enable keyboard shortcuts in this window
            // This connects keyboard events -> KeyPressMappingSet -> Commands
            if (m_commandManager != nullptr)
            {
                addKeyListener(m_commandManager->getKeyMappings());

                // Store the main command target for command chain routing
                // Use a dummy command ID to get the first target (MainComponent)
                m_mainCommandTarget = m_commandManager->getFirstCommandTarget(0);
            }
        }

        ~PluginChainDocumentWindow() override
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
            return m_mainCommandTarget;
        }

        void getAllCommands(juce::Array<juce::CommandID>& commands) override
        {
            // We don't define our own commands - they're all in MainComponent
            juce::ignoreUnused(commands);
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
        juce::ApplicationCommandTarget* m_mainCommandTarget;
    };

    auto* window = new PluginChainDocumentWindow(
        "Plugin Chain",
        juce::Colour(0xff1e1e1e),
        juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton,
        commandManager);

    window->setUsingNativeTitleBar(true);
    window->setContentOwned(this, true);
    window->centreWithSize(getWidth(), getHeight());
    window->setVisible(true);
    window->setResizable(true, true);
    window->setResizeLimits(700, 400, 1600, 1200);

    return window;
}

//==============================================================================
// Component overrides
//==============================================================================
void PluginChainWindow::paint(juce::Graphics& g)
{
    g.fillAll(m_backgroundColour);

    // Draw divider
    g.setColour(m_dividerColour);
    g.fillRect(m_dividerX - 1, 0, 2, getHeight());
}

void PluginChainWindow::resized()
{
    auto bounds = getLocalBounds();
    const int padding = 10;

    // Calculate divider position (proportional)
    int dividerX = static_cast<int>(bounds.getWidth() * 0.5);

    // Chain panel (left side)
    auto chainArea = bounds.removeFromLeft(dividerX - 1).reduced(padding);

    m_chainTitleLabel.setBounds(chainArea.removeFromTop(30));

    // Bottom buttons row
    auto chainBottomButtons = chainArea.removeFromBottom(36);
    m_bypassAllButton.setBounds(chainBottomButtons.removeFromLeft(100).reduced(2));
    m_applyToSelectionButton.setBounds(chainBottomButtons.reduced(2));

    // Render options area (above bottom buttons)
    const int renderOptionsHeight = 70;
    auto renderOptionsArea = chainArea.removeFromBottom(renderOptionsHeight);
    m_renderOptionsGroup.setBounds(renderOptionsArea);

    auto renderInner = renderOptionsArea.reduced(10, 18);
    auto row1 = renderInner.removeFromTop(22);
    m_convertToStereoCheckbox.setBounds(row1.removeFromLeft(150));
    m_includeTailCheckbox.setBounds(row1.removeFromLeft(150));

    auto row2 = renderInner.removeFromTop(24);
    row2.removeFromLeft(10);  // Indent
    m_tailLengthLabel.setBounds(row2.removeFromLeft(35));
    m_tailLengthSlider.setBounds(row2.removeFromLeft(180));

    m_latencyLabel.setBounds(chainArea.removeFromBottom(20));

    m_chainListBox.setBounds(chainArea);
    m_emptyChainLabel.setBounds(chainArea);

    // Browser panel (right side)
    bounds.removeFromLeft(2);  // Divider space
    auto browserArea = bounds.reduced(padding);

    m_browserTitleLabel.setBounds(browserArea.removeFromTop(30));

    // Search row
    auto searchRow = browserArea.removeFromTop(28);
    m_searchLabel.setBounds(searchRow.removeFromLeft(55));
    m_searchBox.setBounds(searchRow);

    browserArea.removeFromTop(6);

    // Filter row
    auto filterRow = browserArea.removeFromTop(28);
    m_categoryLabel.setBounds(filterRow.removeFromLeft(60));
    m_categoryComboBox.setBounds(filterRow.removeFromLeft(120));
    filterRow.removeFromLeft(10);
    m_manufacturerLabel.setBounds(filterRow.removeFromLeft(80));
    m_manufacturerComboBox.setBounds(filterRow.removeFromLeft(120));

    browserArea.removeFromTop(6);

    // Bottom controls
    auto browserBottom = browserArea.removeFromBottom(36);
    m_rescanButton.setBounds(browserBottom.removeFromRight(120).reduced(2));

    auto scanArea = browserBottom.reduced(2);
    if (m_scanProgressBar.isVisible())
    {
        m_scanStatusLabel.setBounds(scanArea.removeFromTop(16));
        m_scanProgressBar.setBounds(scanArea);
    }
    else
    {
        m_scanStatusLabel.setBounds(scanArea);
        m_scanProgressBar.setBounds(0, 0, 0, 0);
    }

    // Browser table
    m_browserTable.setBounds(browserArea);
    m_emptySearchLabel.setBounds(browserArea);
}

bool PluginChainWindow::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            window->closeButtonPressed();
        return true;
    }

    return false;
}

//==============================================================================
// ListBoxModel overrides (for plugin chain)
//==============================================================================
int PluginChainWindow::getNumRows()
{
    return m_chain != nullptr ? m_chain->getNumPlugins() : 0;
}

void PluginChainWindow::paintListBoxItem(int, juce::Graphics&, int, int, bool)
{
    // Handled by custom row component
}

void PluginChainWindow::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    m_chainListBox.selectRow(row);
}

void PluginChainWindow::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (m_listener != nullptr && row >= 0 && row < getNumRows())
        m_listener->pluginChainWindowEditPlugin(row);
}

void PluginChainWindow::deleteKeyPressed(int lastRowSelected)
{
    if (m_listener != nullptr && lastRowSelected >= 0 && lastRowSelected < getNumRows())
        m_listener->pluginChainWindowPluginRemoved(lastRowSelected);
}

void PluginChainWindow::returnKeyPressed(int lastRowSelected)
{
    if (m_listener != nullptr && lastRowSelected >= 0 && lastRowSelected < getNumRows())
        m_listener->pluginChainWindowEditPlugin(lastRowSelected);
}

juce::var PluginChainWindow::getDragSourceDescription(const juce::SparseSet<int>& rowsToDescribe)
{
    if (rowsToDescribe.size() == 1)
        return juce::var(rowsToDescribe[0]);
    return {};
}

juce::Component* PluginChainWindow::refreshComponentForRow(int rowNumber, bool,
                                                            juce::Component* existingComponentToUpdate)
{
    auto* row = static_cast<PluginRowComponent*>(existingComponentToUpdate);

    if (row == nullptr)
        row = new PluginRowComponent(*this);

    PluginChainNode* node = nullptr;
    int totalCount = 0;
    if (m_chain != nullptr)
    {
        totalCount = m_chain->getNumPlugins();
        if (rowNumber >= 0 && rowNumber < totalCount)
            node = m_chain->getPlugin(rowNumber);
    }

    row->update(rowNumber, node, totalCount);
    return row;
}

//==============================================================================
// BrowserTableModel implementation
//==============================================================================
int PluginChainWindow::BrowserTableModel::getNumRows()
{
    return m_owner.m_filteredPlugins.size();
}

void PluginChainWindow::BrowserTableModel::paintRowBackground(juce::Graphics& g, int rowNumber, int, int,
                                                               bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(m_owner.m_selectedRowColour);
    else if (rowNumber % 2 == 1)
        g.fillAll(m_owner.m_alternateRowColour);
    else
        g.fillAll(m_owner.m_backgroundColour);
}

void PluginChainWindow::BrowserTableModel::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                                                      int width, int height, bool)
{
    if (rowNumber < 0 || rowNumber >= m_owner.m_filteredPlugins.size())
        return;

    const auto* desc = m_owner.m_filteredPlugins[rowNumber].description;
    if (desc == nullptr)
        return;

    g.setColour(m_owner.m_textColour);
    g.setFont(juce::FontOptions(13.0f));

    juce::String text;
    switch (columnId)
    {
        case NameColumn:
            text = desc->name;
            break;
        case ManufacturerColumn:
            text = desc->manufacturerName;
            break;
        case CategoryColumn:
            text = desc->category;
            break;
        case FormatColumn:
            text = desc->pluginFormatName;
            break;
        default:
            break;
    }

    g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
}

void PluginChainWindow::BrowserTableModel::cellClicked(int, int, const juce::MouseEvent&)
{
    // Selection handled automatically
}

void PluginChainWindow::BrowserTableModel::cellDoubleClicked(int rowNumber, int, const juce::MouseEvent&)
{
    if (rowNumber >= 0 && rowNumber < m_owner.m_filteredPlugins.size())
    {
        m_owner.addSelectedPluginToChain();
    }
}

void PluginChainWindow::BrowserTableModel::sortOrderChanged(int newSortColumnId, bool isForwards)
{
    m_owner.m_sortColumnId = newSortColumnId;
    m_owner.m_sortForwards = isForwards;
    m_owner.sortPlugins();
    m_owner.m_browserTable.updateContent();
}

void PluginChainWindow::BrowserTableModel::selectedRowsChanged(int)
{
    // No action needed
}

//==============================================================================
// ChangeListener override
//==============================================================================
void PluginChainWindow::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == m_chain)
    {
        m_chainListBox.updateContent();
        updateLatencyDisplay();

        bool isEmpty = (m_chain == nullptr || m_chain->isEmpty());
        m_emptyChainLabel.setVisible(isEmpty);
    }
}

//==============================================================================
// Timer override (for scan progress)
//==============================================================================
void PluginChainWindow::timerCallback()
{
    auto& pluginManager = PluginManager::getInstance();

    if (pluginManager.isScanInProgress())
    {
        // Progress is updated by the callback passed to startScanAsync
        m_scanProgressBar.setVisible(true);
        repaint();
    }
    else
    {
        stopTimer();
        m_scanProgressBar.setVisible(false);
        m_scanStatusLabel.setText(juce::String(m_filteredPlugins.size()) + " plugins available",
                                   juce::dontSendNotification);
        refreshBrowser();
    }
}

//==============================================================================
// Private methods - Chain panel
//==============================================================================
void PluginChainWindow::updateLatencyDisplay()
{
    if (m_chain != nullptr)
    {
        int totalLatency = m_chain->getTotalLatency();
        m_latencyLabel.setText("Total Latency: " + juce::String(totalLatency) + " samples",
                                juce::dontSendNotification);
    }
    else
    {
        m_latencyLabel.setText("Total Latency: 0 samples", juce::dontSendNotification);
    }
}

void PluginChainWindow::onBypassAllClicked()
{
    if (m_listener != nullptr)
        m_listener->pluginChainWindowBypassAll(m_bypassAllButton.getToggleState());
}

void PluginChainWindow::onApplyToSelectionClicked()
{
    if (m_listener != nullptr)
        m_listener->pluginChainWindowApplyToSelection(getRenderOptions());
}

PluginChainWindow::RenderOptions PluginChainWindow::getRenderOptions() const
{
    RenderOptions options;
    options.convertToStereo = m_convertToStereoCheckbox.getToggleState();
    options.includeTail = m_includeTailCheckbox.getToggleState();
    options.tailLengthSeconds = m_tailLengthSlider.getValue();
    return options;
}

void PluginChainWindow::setSourceIsMono(bool isMono)
{
    m_isSourceMono = isMono;
    m_convertToStereoCheckbox.setEnabled(isMono);

    if (!isMono)
    {
        // If source is already stereo, uncheck and disable the checkbox
        m_convertToStereoCheckbox.setToggleState(false, juce::dontSendNotification);
    }
}

//==============================================================================
// Private methods - Browser panel
//==============================================================================
void PluginChainWindow::updateFilteredPlugins()
{
    m_filteredPlugins.clear();

    for (int i = 0; i < m_allPlugins.size(); ++i)
    {
        const auto& desc = m_allPlugins.getReference(i);

        // Apply text filter
        if (m_filterText.isNotEmpty())
        {
            bool matches = desc.name.containsIgnoreCase(m_filterText) ||
                          desc.manufacturerName.containsIgnoreCase(m_filterText) ||
                          desc.category.containsIgnoreCase(m_filterText);
            if (!matches)
                continue;
        }

        // Apply category filter
        if (m_categoryFilter.isNotEmpty() && desc.category != m_categoryFilter)
            continue;

        // Apply manufacturer filter
        if (m_manufacturerFilter.isNotEmpty() && desc.manufacturerName != m_manufacturerFilter)
            continue;

        m_filteredPlugins.add({ i, &desc });
    }

    sortPlugins();
    m_browserTable.updateContent();

    m_scanStatusLabel.setText(juce::String(m_filteredPlugins.size()) + " plugins available",
                               juce::dontSendNotification);

    // Show empty search label when filters return 0 results (but only if a filter is active)
    bool hasActiveFilter = m_filterText.isNotEmpty() ||
                           m_categoryFilter.isNotEmpty() ||
                           m_manufacturerFilter.isNotEmpty();
    bool showEmptyMessage = m_filteredPlugins.isEmpty() && hasActiveFilter;
    m_emptySearchLabel.setVisible(showEmptyMessage);
}

void PluginChainWindow::sortPlugins()
{
    std::stable_sort(m_filteredPlugins.begin(), m_filteredPlugins.end(),
        [this](const FilteredPlugin& a, const FilteredPlugin& b)
        {
            int result = 0;

            switch (m_sortColumnId)
            {
                case NameColumn:
                    result = a.description->name.compareIgnoreCase(b.description->name);
                    break;
                case ManufacturerColumn:
                    result = a.description->manufacturerName.compareIgnoreCase(b.description->manufacturerName);
                    break;
                case CategoryColumn:
                    result = a.description->category.compareIgnoreCase(b.description->category);
                    break;
                case FormatColumn:
                    result = a.description->pluginFormatName.compareIgnoreCase(b.description->pluginFormatName);
                    break;
                default:
                    break;
            }

            return m_sortForwards ? (result < 0) : (result > 0);
        });
}

void PluginChainWindow::onSearchTextChanged()
{
    m_filterText = m_searchBox.getText();
    updateFilteredPlugins();
}

void PluginChainWindow::onCategoryFilterChanged()
{
    int selectedId = m_categoryComboBox.getSelectedId();
    if (selectedId == 1)
        m_categoryFilter.clear();
    else
        m_categoryFilter = m_categoryComboBox.getText();

    updateFilteredPlugins();
}

void PluginChainWindow::onManufacturerFilterChanged()
{
    int selectedId = m_manufacturerComboBox.getSelectedId();
    if (selectedId == 1)
        m_manufacturerFilter.clear();
    else
        m_manufacturerFilter = m_manufacturerComboBox.getText();

    updateFilteredPlugins();
}

void PluginChainWindow::onRescanClicked()
{
    auto& pluginManager = PluginManager::getInstance();

    if (!pluginManager.isScanInProgress())
    {
        // Use a weak reference to safely update UI from callbacks
        juce::Component::SafePointer<PluginChainWindow> safeThis(this);

        pluginManager.startScanAsync(
            // Progress callback
            [safeThis](float progress, const juce::String& currentPlugin)
            {
                if (safeThis != nullptr)
                {
                    safeThis->m_scanProgress = static_cast<double>(progress);
                    safeThis->m_scanStatusLabel.setText("Scanning: " + currentPlugin,
                                                         juce::dontSendNotification);
                }
            },
            // Completion callback
            [safeThis](bool /*success*/, int numPluginsFound)
            {
                if (safeThis != nullptr)
                {
                    safeThis->stopTimer();
                    safeThis->m_scanProgressBar.setVisible(false);
                    safeThis->m_scanStatusLabel.setText(juce::String(numPluginsFound) + " plugins found",
                                                         juce::dontSendNotification);
                    safeThis->refreshBrowser();
                }
            }
        );

        startTimerHz(10);
        m_scanProgressBar.setVisible(true);
        m_scanStatusLabel.setText("Starting scan...", juce::dontSendNotification);
    }
}

void PluginChainWindow::addSelectedPluginToChain()
{
    int selectedRow = m_browserTable.getSelectedRow();
    if (selectedRow >= 0 && selectedRow < m_filteredPlugins.size())
    {
        const auto* desc = m_filteredPlugins[selectedRow].description;
        if (desc != nullptr && m_listener != nullptr)
        {
            m_listener->pluginChainWindowPluginAdded(*desc);
        }
    }
}
