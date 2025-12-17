/*
  ==============================================================================

    PluginManagerDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "PluginManagerDialog.h"

//==============================================================================
PluginManagerDialog::PluginManagerDialog()
    : m_scanProgressBar(m_scanProgress)
{
    // Title
    m_titleLabel.setText("Plugin Manager", juce::dontSendNotification);
    m_titleLabel.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Search box
    m_searchLabel.setText("Search:", juce::dontSendNotification);
    addAndMakeVisible(m_searchLabel);

    m_searchBox.setTextToShowWhenEmpty("Search plugins...", juce::Colours::grey);
    m_searchBox.onTextChange = [this]() { onSearchTextChanged(); };
    addAndMakeVisible(m_searchBox);

    // Category filter
    m_categoryLabel.setText("Category:", juce::dontSendNotification);
    addAndMakeVisible(m_categoryLabel);

    m_categoryComboBox.addItem("All Categories", 1);
    m_categoryComboBox.setSelectedId(1);
    m_categoryComboBox.onChange = [this]() { onCategoryFilterChanged(); };
    addAndMakeVisible(m_categoryComboBox);

    // Manufacturer filter
    m_manufacturerLabel.setText("Manufacturer:", juce::dontSendNotification);
    addAndMakeVisible(m_manufacturerLabel);

    m_manufacturerComboBox.addItem("All Manufacturers", 1);
    m_manufacturerComboBox.setSelectedId(1);
    m_manufacturerComboBox.onChange = [this]() { onManufacturerFilterChanged(); };
    addAndMakeVisible(m_manufacturerComboBox);

    // Plugin table
    m_table.setModel(this);
    m_table.setColour(juce::ListBox::backgroundColourId, m_backgroundColour);
    m_table.setRowHeight(m_rowHeight);
    m_table.setMultipleSelectionEnabled(false);
    m_table.getHeader().setStretchToFitActive(true);

    // Add columns
    m_table.getHeader().addColumn("Name", NameColumn, 200, 100, 400,
                                   juce::TableHeaderComponent::defaultFlags);
    m_table.getHeader().addColumn("Manufacturer", ManufacturerColumn, 150, 80, 300,
                                   juce::TableHeaderComponent::defaultFlags);
    m_table.getHeader().addColumn("Category", CategoryColumn, 100, 60, 200,
                                   juce::TableHeaderComponent::defaultFlags);
    m_table.getHeader().addColumn("Format", FormatColumn, 60, 50, 100,
                                   juce::TableHeaderComponent::defaultFlags);
    m_table.getHeader().addColumn("Latency", LatencyColumn, 60, 50, 100,
                                   juce::TableHeaderComponent::defaultFlags);

    m_table.getHeader().setSortColumnId(NameColumn, true);
    addAndMakeVisible(m_table);

    // Plugin info panel
    m_infoTitleLabel.setText("Plugin Details", juce::dontSendNotification);
    m_infoTitleLabel.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    addAndMakeVisible(m_infoTitleLabel);

    m_infoNameLabel.setText("Name: -", juce::dontSendNotification);
    addAndMakeVisible(m_infoNameLabel);

    m_infoManufacturerLabel.setText("Manufacturer: -", juce::dontSendNotification);
    addAndMakeVisible(m_infoManufacturerLabel);

    m_infoCategoryLabel.setText("Category: -", juce::dontSendNotification);
    addAndMakeVisible(m_infoCategoryLabel);

    m_infoVersionLabel.setText("Version: -", juce::dontSendNotification);
    addAndMakeVisible(m_infoVersionLabel);

    m_infoFormatLabel.setText("Format: -", juce::dontSendNotification);
    addAndMakeVisible(m_infoFormatLabel);

    m_infoLatencyLabel.setText("Latency: -", juce::dontSendNotification);
    addAndMakeVisible(m_infoLatencyLabel);

    m_infoFileLabel.setText("File: -", juce::dontSendNotification);
    addAndMakeVisible(m_infoFileLabel);

    // Scan progress
    m_scanStatusLabel.setText("", juce::dontSendNotification);
    addAndMakeVisible(m_scanStatusLabel);

    m_scanProgressBar.setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xff333333));
    m_scanProgressBar.setColour(juce::ProgressBar::foregroundColourId, juce::Colour(0xff4a9eff));
    addAndMakeVisible(m_scanProgressBar);
    m_scanProgressBar.setVisible(false);

    // Buttons
    m_rescanButton.setButtonText("Rescan Plugins");
    m_rescanButton.onClick = [this]() { onRescanClicked(); };
    addAndMakeVisible(m_rescanButton);

    m_addButton.setButtonText("Add to Chain");
    m_addButton.onClick = [this]() { onAddClicked(); };
    m_addButton.setEnabled(false);
    addAndMakeVisible(m_addButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    // Load initial plugin list
    refresh();

    setSize(700, 600);
}

PluginManagerDialog::~PluginManagerDialog()
{
    stopTimer();
}

//==============================================================================
void PluginManagerDialog::refresh()
{
    auto& pluginManager = PluginManager::getInstance();

    // Get all plugins and filter out instruments (effects only)
    auto allAvailable = pluginManager.getAvailablePlugins();
    m_allPlugins.clear();

    for (const auto& desc : allAvailable)
    {
        // Filter out instruments - we only want effects
        if (!desc.isInstrument)
        {
            m_allPlugins.add(desc);
        }
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

    // Update filter combo boxes
    m_categoryComboBox.clear();
    m_categoryComboBox.addItem("All Categories", 1);
    int id = 2;
    for (const auto& cat : categories)
        m_categoryComboBox.addItem(cat, id++);
    m_categoryComboBox.setSelectedId(1, juce::dontSendNotification);

    m_manufacturerComboBox.clear();
    m_manufacturerComboBox.addItem("All Manufacturers", 1);
    id = 2;
    for (const auto& mfr : manufacturers)
        m_manufacturerComboBox.addItem(mfr, id++);
    m_manufacturerComboBox.setSelectedId(1, juce::dontSendNotification);

    // Reset filters
    m_categoryFilter.clear();
    m_manufacturerFilter.clear();
    m_filterText.clear();
    m_searchBox.setText("", juce::dontSendNotification);

    // Update filtered list
    updateFilteredPlugins();
}

void PluginManagerDialog::updateFilteredPlugins()
{
    m_filteredPlugins.clear();

    for (int i = 0; i < m_allPlugins.size(); ++i)
    {
        const auto& desc = m_allPlugins[i];

        // Apply search filter
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

        FilteredPlugin fp;
        fp.originalIndex = i;
        fp.description = &m_allPlugins.getReference(i);
        m_filteredPlugins.add(fp);
    }

    sortPlugins();
    m_table.updateContent();
    m_table.repaint();
}

void PluginManagerDialog::sortPlugins()
{
    auto compareFunc = [this](const FilteredPlugin& a, const FilteredPlugin& b) -> bool
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
            case LatencyColumn:
                // Can't sort by latency without loading the plugin
                result = a.description->name.compareIgnoreCase(b.description->name);
                break;
            default:
                result = 0;
        }

        return m_sortForwards ? (result < 0) : (result > 0);
    };

    std::sort(m_filteredPlugins.begin(), m_filteredPlugins.end(), compareFunc);
}

void PluginManagerDialog::updatePluginInfo()
{
    int selectedRow = m_table.getSelectedRow();

    if (selectedRow >= 0 && selectedRow < m_filteredPlugins.size())
    {
        const auto* desc = m_filteredPlugins[selectedRow].description;

        m_infoNameLabel.setText("Name: " + desc->name, juce::dontSendNotification);
        m_infoManufacturerLabel.setText("Manufacturer: " + desc->manufacturerName, juce::dontSendNotification);
        m_infoCategoryLabel.setText("Category: " + desc->category, juce::dontSendNotification);
        m_infoVersionLabel.setText("Version: " + desc->version, juce::dontSendNotification);
        m_infoFormatLabel.setText("Format: " + desc->pluginFormatName, juce::dontSendNotification);
        m_infoLatencyLabel.setText("Latency: (load to check)", juce::dontSendNotification);

        juce::File pluginFile(desc->fileOrIdentifier);
        m_infoFileLabel.setText("File: " + pluginFile.getFileName(), juce::dontSendNotification);

        m_addButton.setEnabled(true);
    }
    else
    {
        m_infoNameLabel.setText("Name: -", juce::dontSendNotification);
        m_infoManufacturerLabel.setText("Manufacturer: -", juce::dontSendNotification);
        m_infoCategoryLabel.setText("Category: -", juce::dontSendNotification);
        m_infoVersionLabel.setText("Version: -", juce::dontSendNotification);
        m_infoFormatLabel.setText("Format: -", juce::dontSendNotification);
        m_infoLatencyLabel.setText("Latency: -", juce::dontSendNotification);
        m_infoFileLabel.setText("File: -", juce::dontSendNotification);

        m_addButton.setEnabled(false);
    }
}

//==============================================================================
void PluginManagerDialog::onSearchTextChanged()
{
    m_filterText = m_searchBox.getText();
    updateFilteredPlugins();
}

void PluginManagerDialog::onCategoryFilterChanged()
{
    int selectedId = m_categoryComboBox.getSelectedId();
    if (selectedId == 1)
        m_categoryFilter.clear();
    else
        m_categoryFilter = m_categoryComboBox.getText();

    updateFilteredPlugins();
}

void PluginManagerDialog::onManufacturerFilterChanged()
{
    int selectedId = m_manufacturerComboBox.getSelectedId();
    if (selectedId == 1)
        m_manufacturerFilter.clear();
    else
        m_manufacturerFilter = m_manufacturerComboBox.getText();

    updateFilteredPlugins();
}

void PluginManagerDialog::onRescanClicked()
{
    auto& pluginManager = PluginManager::getInstance();

    if (pluginManager.isScanInProgress())
    {
        // Already scanning
        return;
    }

    m_rescanButton.setEnabled(false);
    m_scanStatusLabel.setText("Scanning...", juce::dontSendNotification);
    m_scanProgressBar.setVisible(true);
    m_scanProgress = 0.0;

    // Start async scan with progress and completion callbacks
    pluginManager.forceRescan(
        // Progress callback (called on message thread)
        [this](float progress, const juce::String& currentPlugin)
        {
            m_scanProgress = static_cast<double>(progress);
            if (currentPlugin.isNotEmpty())
            {
                m_scanStatusLabel.setText("Scanning: " + currentPlugin, juce::dontSendNotification);
            }
            m_scanProgressBar.repaint();
        },
        // Completion callback (called on message thread)
        [this](bool success, int numPluginsFound)
        {
            juce::ignoreUnused(success);
            m_scanStatusLabel.setText("Found " + juce::String(numPluginsFound) + " plugins", juce::dontSendNotification);
            m_scanProgressBar.setVisible(false);
            m_rescanButton.setEnabled(true);
            refresh();
        }
    );
}

void PluginManagerDialog::onAddClicked()
{
    const juce::PluginDescription* desc = getSelectedPlugin();
    if (desc != nullptr)
    {
        m_wasAddClicked = true;

        // Notify listener if set
        if (m_listener != nullptr)
        {
            m_listener->pluginManagerDialogAddPlugin(*desc);
        }

        // Close the dialog window (exit modal state)
        if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
        {
            dialogWindow->exitModalState(1);
        }
    }
}

void PluginManagerDialog::onCancelClicked()
{
    // Notify listener if set
    if (m_listener != nullptr)
    {
        m_listener->pluginManagerDialogCancelled();
    }

    // Close the dialog window (exit modal state)
    if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
    {
        dialogWindow->exitModalState(0);
    }
}

const juce::PluginDescription* PluginManagerDialog::getSelectedPlugin() const
{
    int selectedRow = m_table.getSelectedRow();
    if (selectedRow >= 0 && selectedRow < m_filteredPlugins.size())
    {
        return m_filteredPlugins[selectedRow].description;
    }
    return nullptr;
}

//==============================================================================
void PluginManagerDialog::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    // Not used - we use callbacks directly via startScanAsync/forceRescan
}

void PluginManagerDialog::timerCallback()
{
    // Not used - progress is updated via callback
}

//==============================================================================
juce::DocumentWindow* PluginManagerDialog::showInWindow(bool modal)
{
    auto* window = new juce::DocumentWindow(
        "Plugin Manager",
        juce::Colours::darkgrey,
        juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton,
        true);

    window->setContentOwned(this, true);
    window->setResizable(true, true);
    window->setUsingNativeTitleBar(true);
    window->centreWithSize(getWidth(), getHeight());

    if (modal)
    {
        window->enterModalState(true, nullptr, true);
    }
    else
    {
        window->setVisible(true);
    }

    return window;
}

//==============================================================================
void PluginManagerDialog::paint(juce::Graphics& g)
{
    g.fillAll(m_backgroundColour);

    // Draw separator line above info panel
    auto bounds = getLocalBounds();
    int infoPanelY = bounds.getHeight() - 200;

    g.setColour(juce::Colour(0xff444444));
    g.drawHorizontalLine(infoPanelY, 10.0f, static_cast<float>(bounds.getWidth() - 10));
}

void PluginManagerDialog::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Title
    m_titleLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(10);

    // Search and filters row
    auto filterRow = bounds.removeFromTop(30);
    m_searchLabel.setBounds(filterRow.removeFromLeft(50));
    m_searchBox.setBounds(filterRow.removeFromLeft(200));
    filterRow.removeFromLeft(20);

    m_categoryLabel.setBounds(filterRow.removeFromLeft(60));
    m_categoryComboBox.setBounds(filterRow.removeFromLeft(150));
    filterRow.removeFromLeft(20);

    m_manufacturerLabel.setBounds(filterRow.removeFromLeft(85));
    m_manufacturerComboBox.setBounds(filterRow.removeFromLeft(150));

    bounds.removeFromTop(10);

    // Buttons row at bottom
    auto buttonRow = bounds.removeFromBottom(30);
    m_cancelButton.setBounds(buttonRow.removeFromRight(100));
    buttonRow.removeFromRight(10);
    m_addButton.setBounds(buttonRow.removeFromRight(120));
    buttonRow.removeFromRight(20);
    m_rescanButton.setBounds(buttonRow.removeFromRight(120));

    bounds.removeFromBottom(10);

    // Scan progress row
    auto scanRow = bounds.removeFromBottom(25);
    m_scanStatusLabel.setBounds(scanRow.removeFromLeft(200));
    m_scanProgressBar.setBounds(scanRow);

    bounds.removeFromBottom(10);

    // Info panel at bottom
    auto infoPanel = bounds.removeFromBottom(120);
    m_infoTitleLabel.setBounds(infoPanel.removeFromTop(20));
    m_infoNameLabel.setBounds(infoPanel.removeFromTop(18));
    m_infoManufacturerLabel.setBounds(infoPanel.removeFromTop(18));
    m_infoCategoryLabel.setBounds(infoPanel.removeFromTop(18));
    m_infoVersionLabel.setBounds(infoPanel.removeFromTop(18));
    m_infoFormatLabel.setBounds(infoPanel.removeFromTop(18));
    m_infoFileLabel.setBounds(infoPanel.removeFromTop(18));

    bounds.removeFromBottom(10);

    // Table takes remaining space
    m_table.setBounds(bounds);
}

bool PluginManagerDialog::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        onCancelClicked();
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        if (getSelectedPlugin() != nullptr)
        {
            onAddClicked();
            return true;
        }
    }

    return false;
}

//==============================================================================
// TableListBoxModel overrides
int PluginManagerDialog::getNumRows()
{
    return m_filteredPlugins.size();
}

void PluginManagerDialog::paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/,
                                              bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll(m_selectedRowColour);
    }
    else if (rowNumber % 2 == 1)
    {
        g.fillAll(m_alternateRowColour);
    }
    else
    {
        g.fillAll(m_backgroundColour);
    }
}

void PluginManagerDialog::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                                     int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= m_filteredPlugins.size())
        return;

    const auto* desc = m_filteredPlugins[rowNumber].description;
    if (desc == nullptr)
        return;

    g.setColour(m_textColour);
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
        case LatencyColumn:
            text = "-"; // Would need to load plugin to get actual latency
            break;
        default:
            break;
    }

    g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
}

void PluginManagerDialog::cellClicked(int /*rowNumber*/, int /*columnId*/, const juce::MouseEvent& /*event*/)
{
    updatePluginInfo();
}

void PluginManagerDialog::cellDoubleClicked(int /*rowNumber*/, int /*columnId*/, const juce::MouseEvent& /*event*/)
{
    // Double-click adds the plugin
    onAddClicked();
}

void PluginManagerDialog::sortOrderChanged(int newSortColumnId, bool isForwards)
{
    m_sortColumnId = newSortColumnId;
    m_sortForwards = isForwards;
    sortPlugins();
    m_table.updateContent();
    m_table.repaint();
}

void PluginManagerDialog::selectedRowsChanged(int /*lastRowSelected*/)
{
    updatePluginInfo();
}

void PluginManagerDialog::returnKeyPressed(int /*lastRowSelected*/)
{
    if (getSelectedPlugin() != nullptr)
    {
        onAddClicked();
    }
}
