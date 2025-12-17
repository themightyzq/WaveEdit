/*
  ==============================================================================

    ToolbarCustomizationDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "ToolbarCustomizationDialog.h"
#include "../Commands/CommandIDs.h"

//==============================================================================
// AvailableButtonsModel implementation

void ToolbarCustomizationDialog::AvailableButtonsModel::paintListBoxItem(
    int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(m_owner.m_availableButtons.size()))
        return;

    const auto& button = m_owner.m_availableButtons[static_cast<size_t>(rowNumber)];

    // Background
    if (rowIsSelected)
        g.fillAll(m_owner.m_selectedRowColour);
    else if (rowNumber % 2 == 1)
        g.fillAll(m_owner.m_listBackgroundColour.brighter(0.05f));

    // Type indicator
    g.setColour(m_owner.m_accentColour.withAlpha(0.6f));
    g.fillRoundedRectangle(4, 4, 60, height - 8, 3);

    g.setColour(m_owner.m_textColour);
    g.setFont(juce::Font(11.0f));
    g.drawText(m_owner.getButtonTypeLabel(button.type), 4, 0, 60, height,
               juce::Justification::centred);

    // Button name
    g.setFont(juce::Font(13.0f));
    g.drawText(m_owner.getButtonDisplayName(button), 70, 0, width - 75, height,
               juce::Justification::centredLeft);
}

juce::var ToolbarCustomizationDialog::AvailableButtonsModel::getDragSourceDescription(
    const juce::SparseSet<int>& selectedRows)
{
    if (selectedRows.isEmpty())
        return {};

    int row = selectedRows[0];
    if (row < 0 || row >= static_cast<int>(m_owner.m_availableButtons.size()))
        return {};

    auto* obj = new juce::DynamicObject();
    obj->setProperty("source", "available");
    obj->setProperty("index", row);
    return juce::var(obj);
}

//==============================================================================
// CurrentButtonsModel implementation

void ToolbarCustomizationDialog::CurrentButtonsModel::paintListBoxItem(
    int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(m_owner.m_currentLayout.buttons.size()))
        return;

    const auto& button = m_owner.m_currentLayout.buttons[static_cast<size_t>(rowNumber)];

    // Background
    if (rowIsSelected)
        g.fillAll(m_owner.m_selectedRowColour);
    else if (rowNumber % 2 == 1)
        g.fillAll(m_owner.m_listBackgroundColour.brighter(0.05f));

    // Index number
    g.setColour(m_owner.m_textColour.withAlpha(0.5f));
    g.setFont(juce::Font(11.0f));
    g.drawText(juce::String(rowNumber + 1) + ".", 4, 0, 24, height,
               juce::Justification::centredRight);

    // Type indicator
    g.setColour(m_owner.m_accentColour.withAlpha(0.6f));
    g.fillRoundedRectangle(32, 4, 60, height - 8, 3);

    g.setColour(m_owner.m_textColour);
    g.setFont(juce::Font(11.0f));
    g.drawText(m_owner.getButtonTypeLabel(button.type), 32, 0, 60, height,
               juce::Justification::centred);

    // Button name
    g.setFont(juce::Font(13.0f));
    g.drawText(m_owner.getButtonDisplayName(button), 98, 0, width - 103, height,
               juce::Justification::centredLeft);
}

juce::var ToolbarCustomizationDialog::CurrentButtonsModel::getDragSourceDescription(
    const juce::SparseSet<int>& selectedRows)
{
    if (selectedRows.isEmpty())
        return {};

    int row = selectedRows[0];
    if (row < 0 || row >= static_cast<int>(m_owner.m_currentLayout.buttons.size()))
        return {};

    auto* obj = new juce::DynamicObject();
    obj->setProperty("source", "current");
    obj->setProperty("index", row);
    return juce::var(obj);
}

//==============================================================================
// ToolbarCustomizationDialog implementation

ToolbarCustomizationDialog::ToolbarCustomizationDialog(
    ToolbarManager& toolbarManager,
    juce::ApplicationCommandManager& commandManager)
    : m_toolbarManager(toolbarManager)
    , m_commandManager(commandManager)
{
    // Store original layout for cancel/reset
    m_originalLayout = m_toolbarManager.getCurrentLayout();
    m_currentLayout = m_originalLayout;

    // Create list models
    m_availableModel = std::make_unique<AvailableButtonsModel>(*this);
    m_currentModel = std::make_unique<CurrentButtonsModel>(*this);

    // Title
    m_titleLabel.setText("Customize Toolbar", juce::dontSendNotification);
    m_titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    m_titleLabel.setColour(juce::Label::textColourId, m_textColour);
    addAndMakeVisible(m_titleLabel);

    // Instructions
    m_instructionsLabel.setText(
        "Drag buttons between lists to add or remove. Use Move Up/Down to reorder.",
        juce::dontSendNotification);
    m_instructionsLabel.setFont(juce::Font(12.0f));
    m_instructionsLabel.setColour(juce::Label::textColourId, m_textColour.withAlpha(0.7f));
    addAndMakeVisible(m_instructionsLabel);

    // Layout selector
    m_layoutLabel.setText("Template:", juce::dontSendNotification);
    m_layoutLabel.setFont(juce::Font(13.0f));
    m_layoutLabel.setColour(juce::Label::textColourId, m_textColour);
    addAndMakeVisible(m_layoutLabel);

    m_layoutSelector.setColour(juce::ComboBox::backgroundColourId, m_listBackgroundColour);
    m_layoutSelector.setColour(juce::ComboBox::textColourId, m_textColour);
    m_layoutSelector.setColour(juce::ComboBox::outlineColourId, m_separatorColour);

    auto layouts = m_toolbarManager.getAvailableLayouts();
    int currentIndex = 0;
    for (int i = 0; i < layouts.size(); ++i)
    {
        m_layoutSelector.addItem(layouts[i], i + 1);
        if (layouts[i] == m_toolbarManager.getCurrentLayoutName())
            currentIndex = i + 1;
    }
    m_layoutSelector.setSelectedId(currentIndex, juce::dontSendNotification);
    m_layoutSelector.onChange = [this]() { onLayoutSelectionChanged(); };
    addAndMakeVisible(m_layoutSelector);

    // Available buttons label
    m_availableLabel.setText("Available Buttons:", juce::dontSendNotification);
    m_availableLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    m_availableLabel.setColour(juce::Label::textColourId, m_textColour);
    addAndMakeVisible(m_availableLabel);

    // Available buttons list
    m_availableList.setModel(m_availableModel.get());
    m_availableList.setRowHeight(kListRowHeight);
    m_availableList.setColour(juce::ListBox::backgroundColourId, m_listBackgroundColour);
    m_availableList.setColour(juce::ListBox::outlineColourId, m_separatorColour);
    m_availableList.setOutlineThickness(1);
    addAndMakeVisible(m_availableList);

    // Current toolbar label
    m_currentLabel.setText("Current Toolbar:", juce::dontSendNotification);
    m_currentLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    m_currentLabel.setColour(juce::Label::textColourId, m_textColour);
    addAndMakeVisible(m_currentLabel);

    // Current toolbar list
    m_currentList.setModel(m_currentModel.get());
    m_currentList.setRowHeight(kListRowHeight);
    m_currentList.setColour(juce::ListBox::backgroundColourId, m_listBackgroundColour);
    m_currentList.setColour(juce::ListBox::outlineColourId, m_separatorColour);
    m_currentList.setOutlineThickness(1);
    addAndMakeVisible(m_currentList);

    // Transfer buttons
    m_addButton.setButtonText("Add >");
    m_addButton.onClick = [this]() { onAddButtonClicked(); };
    addAndMakeVisible(m_addButton);

    m_removeButton.setButtonText("< Remove");
    m_removeButton.onClick = [this]() { onRemoveButtonClicked(); };
    addAndMakeVisible(m_removeButton);

    // Reorder buttons
    m_moveUpButton.setButtonText("Move Up");
    m_moveUpButton.onClick = [this]() { onMoveUpClicked(); };
    addAndMakeVisible(m_moveUpButton);

    m_moveDownButton.setButtonText("Move Down");
    m_moveDownButton.onClick = [this]() { onMoveDownClicked(); };
    addAndMakeVisible(m_moveDownButton);

    // Special item buttons
    m_addSeparatorButton.setButtonText("+ Separator");
    m_addSeparatorButton.onClick = [this]() { onAddSeparatorClicked(); };
    addAndMakeVisible(m_addSeparatorButton);

    m_addSpacerButton.setButtonText("+ Spacer");
    m_addSpacerButton.onClick = [this]() { onAddSpacerClicked(); };
    addAndMakeVisible(m_addSpacerButton);

    // Template management buttons
    m_saveAsButton.setButtonText("Save As...");
    m_saveAsButton.onClick = [this]() { onSaveAsClicked(); };
    addAndMakeVisible(m_saveAsButton);

    m_resetButton.setButtonText("Reset");
    m_resetButton.onClick = [this]() { onResetClicked(); };
    addAndMakeVisible(m_resetButton);

    m_importButton.setButtonText("Import...");
    m_importButton.onClick = [this]() { onImportClicked(); };
    addAndMakeVisible(m_importButton);

    m_exportButton.setButtonText("Export...");
    m_exportButton.onClick = [this]() { onExportClicked(); };
    addAndMakeVisible(m_exportButton);

    // Dialog buttons
    m_okButton.setButtonText("OK");
    m_okButton.onClick = [this]() { onOkClicked(); };
    addAndMakeVisible(m_okButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    // Initialize available buttons list
    refreshAvailableButtons();

    setSize(kDialogWidth, kDialogHeight);
}

ToolbarCustomizationDialog::~ToolbarCustomizationDialog()
{
}

//==============================================================================
// Static dialog launcher

bool ToolbarCustomizationDialog::showDialog(
    ToolbarManager& toolbarManager,
    juce::ApplicationCommandManager& commandManager)
{
    ToolbarCustomizationDialog dialog(toolbarManager, commandManager);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Customize Toolbar";
    options.dialogBackgroundColour = dialog.m_backgroundColour;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.content.setNonOwned(&dialog);

    // Create and show dialog modally
    std::unique_ptr<juce::DialogWindow> dialogWindow(options.create());
    dialogWindow->setVisible(true);
    dialogWindow->centreWithSize(kDialogWidth, kDialogHeight);

#if JUCE_MODAL_LOOPS_PERMITTED
    dialogWindow->enterModalState(true);
    dialogWindow->runModalLoop();
#else
    // Fallback for non-modal platforms
    dialogWindow->setAlwaysOnTop(true);
#endif

    return dialog.m_layoutChanged && !dialog.m_cancelled;
}

//==============================================================================
// Component overrides

void ToolbarCustomizationDialog::paint(juce::Graphics& g)
{
    g.fillAll(m_backgroundColour);

    // Draw divider between transfer buttons
    auto bounds = getLocalBounds().reduced(kPadding);
    int centerX = bounds.getCentreX();
    g.setColour(m_separatorColour);
    g.drawLine(static_cast<float>(centerX), static_cast<float>(bounds.getY() + 100),
               static_cast<float>(centerX), static_cast<float>(bounds.getBottom() - 100), 1.0f);
}

void ToolbarCustomizationDialog::resized()
{
    auto bounds = getLocalBounds().reduced(kPadding);

    // Title and instructions at top
    m_titleLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(kSpacing / 2);
    m_instructionsLabel.setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(kSpacing);

    // Layout selector row
    auto layoutRow = bounds.removeFromTop(kComboHeight);
    m_layoutLabel.setBounds(layoutRow.removeFromLeft(70));
    layoutRow.removeFromLeft(kSpacing);
    m_layoutSelector.setBounds(layoutRow.removeFromLeft(200));
    bounds.removeFromTop(kSpacing);

    // Labels for lists
    auto labelRow = bounds.removeFromTop(kLabelHeight);
    int listWidth = (bounds.getWidth() - 120) / 2;  // 120 for center buttons
    m_availableLabel.setBounds(labelRow.removeFromLeft(listWidth));
    labelRow.removeFromLeft(120);  // Center space for buttons
    m_currentLabel.setBounds(labelRow);
    bounds.removeFromTop(4);

    // Bottom buttons (OK, Cancel, Save As, etc.)
    auto bottomRow = bounds.removeFromBottom(kButtonHeight);
    m_cancelButton.setBounds(bottomRow.removeFromRight(80));
    bottomRow.removeFromRight(kSpacing);
    m_okButton.setBounds(bottomRow.removeFromRight(80));
    bottomRow.removeFromRight(kSpacing * 3);
    m_exportButton.setBounds(bottomRow.removeFromRight(80));
    bottomRow.removeFromRight(kSpacing);
    m_importButton.setBounds(bottomRow.removeFromRight(80));
    bottomRow.removeFromRight(kSpacing);
    m_resetButton.setBounds(bottomRow.removeFromRight(60));
    bottomRow.removeFromRight(kSpacing);
    m_saveAsButton.setBounds(bottomRow.removeFromRight(80));

    bounds.removeFromBottom(kSpacing);

    // Main content area: Lists and center buttons
    int centerButtonsWidth = 100;
    int centerX = bounds.getCentreX();

    // Available list (left)
    auto availableBounds = bounds.withWidth(listWidth).withTrimmedRight(kSpacing);
    m_availableList.setBounds(availableBounds);

    // Current list (right)
    auto currentBounds = bounds.withLeft(centerX + centerButtonsWidth / 2 + kSpacing);
    m_currentList.setBounds(currentBounds);

    // Center buttons
    auto centerBounds = juce::Rectangle<int>(
        centerX - centerButtonsWidth / 2,
        bounds.getY(),
        centerButtonsWidth,
        bounds.getHeight()
    );

    int buttonY = centerBounds.getY() + 30;
    int buttonSpacing = 35;

    m_addButton.setBounds(centerBounds.getX(), buttonY, centerButtonsWidth, kButtonHeight);
    buttonY += buttonSpacing;
    m_removeButton.setBounds(centerBounds.getX(), buttonY, centerButtonsWidth, kButtonHeight);
    buttonY += buttonSpacing + 20;

    m_moveUpButton.setBounds(centerBounds.getX(), buttonY, centerButtonsWidth, kButtonHeight);
    buttonY += buttonSpacing;
    m_moveDownButton.setBounds(centerBounds.getX(), buttonY, centerButtonsWidth, kButtonHeight);
    buttonY += buttonSpacing + 20;

    m_addSeparatorButton.setBounds(centerBounds.getX(), buttonY, centerButtonsWidth, kButtonHeight);
    buttonY += buttonSpacing;
    m_addSpacerButton.setBounds(centerBounds.getX(), buttonY, centerButtonsWidth, kButtonHeight);
}

//==============================================================================
// ListBoxModel overrides (unused - we use nested models)

int ToolbarCustomizationDialog::getNumRows()
{
    return 0;
}

void ToolbarCustomizationDialog::paintListBoxItem(int, juce::Graphics&, int, int, bool)
{
}

juce::var ToolbarCustomizationDialog::getDragSourceDescription(const juce::SparseSet<int>&)
{
    return {};
}

//==============================================================================
// DragAndDropTarget overrides

bool ToolbarCustomizationDialog::isInterestedInDragSource(const SourceDetails& details)
{
    auto* obj = details.description.getDynamicObject();
    if (obj == nullptr)
        return false;

    return obj->hasProperty("source") && obj->hasProperty("index");
}

void ToolbarCustomizationDialog::itemDragEnter(const SourceDetails&)
{
}

void ToolbarCustomizationDialog::itemDragMove(const SourceDetails&)
{
}

void ToolbarCustomizationDialog::itemDragExit(const SourceDetails&)
{
    m_dropInsertIndex = -1;
    m_isDraggingToAvailable = false;
    m_isDraggingToCurrent = false;
}

void ToolbarCustomizationDialog::itemDropped(const SourceDetails& details)
{
    auto* obj = details.description.getDynamicObject();
    if (obj == nullptr)
        return;

    juce::String source = obj->getProperty("source").toString();
    int index = static_cast<int>(obj->getProperty("index"));

    // Determine drop target
    bool droppedOnCurrent = m_currentList.getBounds().contains(details.localPosition.toInt());
    bool droppedOnAvailable = m_availableList.getBounds().contains(details.localPosition.toInt());

    if (source == "available" && droppedOnCurrent)
    {
        // Add from available to current
        if (index >= 0 && index < static_cast<int>(m_availableButtons.size()))
        {
            addButtonToToolbar(m_availableButtons[static_cast<size_t>(index)]);
        }
    }
    else if (source == "current" && droppedOnAvailable)
    {
        // Remove from current
        if (index >= 0 && index < static_cast<int>(m_currentLayout.buttons.size()))
        {
            removeButtonFromToolbar(index);
        }
    }
    else if (source == "current" && droppedOnCurrent)
    {
        // Reorder within current list
        auto localPos = m_currentList.getLocalPoint(this, details.localPosition);
        int targetRow = m_currentList.getRowContainingPosition(
            static_cast<int>(localPos.x), static_cast<int>(localPos.y));

        if (targetRow >= 0 && targetRow != index)
        {
            moveButtonInToolbar(index, targetRow);
        }
    }

    m_dropInsertIndex = -1;
}

//==============================================================================
// Private methods

void ToolbarCustomizationDialog::refreshAvailableButtons()
{
    m_availableButtons.clear();

    // Add all available command buttons
    // These are the commands that can be added to the toolbar
    struct CommandEntry
    {
        juce::CommandID id;
        const char* commandName;
        const char* iconName;
    };

    static const CommandEntry availableCommands[] = {
        // File operations
        { CommandIDs::fileNew, "fileNew", "new" },
        { CommandIDs::fileOpen, "fileOpen", "open" },
        { CommandIDs::fileSave, "fileSave", "save" },
        { CommandIDs::fileSaveAs, "fileSaveAs", "saveAs" },

        // Edit operations
        { CommandIDs::editUndo, "editUndo", "undo" },
        { CommandIDs::editRedo, "editRedo", "redo" },
        { CommandIDs::editCut, "editCut", "cut" },
        { CommandIDs::editCopy, "editCopy", "copy" },
        { CommandIDs::editPaste, "editPaste", "paste" },
        { CommandIDs::editDelete, "editDelete", "delete" },
        { CommandIDs::editSelectAll, "editSelectAll", "selectAll" },

        // View operations
        { CommandIDs::viewZoomIn, "viewZoomIn", "zoomIn" },
        { CommandIDs::viewZoomOut, "viewZoomOut", "zoomOut" },
        { CommandIDs::viewZoomFit, "viewZoomFit", "zoomFit" },
        { CommandIDs::viewZoomSelection, "viewZoomSelection", "zoomSelection" },

        // Processing operations
        { CommandIDs::processFadeIn, "processFadeIn", "fadeIn" },
        { CommandIDs::processFadeOut, "processFadeOut", "fadeOut" },
        { CommandIDs::processNormalize, "processNormalize", "normalize" },
        { CommandIDs::processDCOffset, "processDCOffset", "dcOffset" },
        { CommandIDs::processGain, "processGain", "gain" },
        { CommandIDs::processGraphicalEQ, "processGraphicalEQ", "eq" },

        // Plugin operations
        { CommandIDs::pluginShowChain, "pluginShowChain", "plugin" },
        { CommandIDs::pluginApplyChain, "pluginApplyChain", "apply" },
        { CommandIDs::pluginOffline, "pluginOffline", "offline" },

        // Region operations
        { CommandIDs::regionAdd, "regionAdd", "regionAdd" },
        { CommandIDs::regionExportAll, "regionExportAll", "regionExport" },

        // Marker operations
        { CommandIDs::markerAdd, "markerAdd", "markerAdd" },
    };

    for (const auto& cmd : availableCommands)
    {
        auto* commandInfo = m_commandManager.getCommandForID(cmd.id);
        if (commandInfo != nullptr)
        {
            ToolbarButtonConfig config;
            config.id = juce::String(cmd.id);
            config.type = ToolbarButtonType::COMMAND;
            config.commandName = cmd.commandName;  // Use proper commandName for icon matching
            config.iconName = cmd.iconName;
            config.tooltip = commandInfo->description;
            config.width = 28;

            m_availableButtons.push_back(config);
        }
    }

    // Add transport as an option
    ToolbarButtonConfig transportConfig;
    transportConfig.id = "transport";
    transportConfig.type = ToolbarButtonType::TRANSPORT;
    transportConfig.tooltip = "Transport Controls";
    transportConfig.width = 200;
    m_availableButtons.push_back(transportConfig);

    m_availableList.updateContent();
}

void ToolbarCustomizationDialog::refreshCurrentButtons()
{
    m_currentList.updateContent();
}

void ToolbarCustomizationDialog::onLayoutSelectionChanged()
{
    int selectedId = m_layoutSelector.getSelectedId();
    if (selectedId <= 0)
        return;

    juce::String layoutName = m_layoutSelector.getItemText(selectedId - 1);
    if (m_toolbarManager.loadLayout(layoutName))
    {
        m_currentLayout = m_toolbarManager.getCurrentLayout();
        refreshCurrentButtons();
        m_layoutChanged = true;
    }
}

void ToolbarCustomizationDialog::onMoveUpClicked()
{
    int selectedRow = m_currentList.getSelectedRow();
    if (selectedRow > 0)
    {
        moveButtonInToolbar(selectedRow, selectedRow - 1);
        m_currentList.selectRow(selectedRow - 1);
    }
}

void ToolbarCustomizationDialog::onMoveDownClicked()
{
    int selectedRow = m_currentList.getSelectedRow();
    if (selectedRow >= 0 && selectedRow < static_cast<int>(m_currentLayout.buttons.size()) - 1)
    {
        moveButtonInToolbar(selectedRow, selectedRow + 1);
        m_currentList.selectRow(selectedRow + 1);
    }
}

void ToolbarCustomizationDialog::onAddButtonClicked()
{
    int selectedRow = m_availableList.getSelectedRow();
    if (selectedRow >= 0 && selectedRow < static_cast<int>(m_availableButtons.size()))
    {
        // Calculate insert position: after selection in current list, or before first spacer
        int insertIndex = m_currentList.getSelectedRow();
        if (insertIndex >= 0)
        {
            // Insert after the selected item
            insertIndex++;
        }
        else
        {
            // No selection - insert BEFORE the first spacer (so items appear in the content area,
            // not after the flexible spacer which would push them to the far right edge)
            insertIndex = static_cast<int>(m_currentLayout.buttons.size()); // Default: end
            for (size_t i = 0; i < m_currentLayout.buttons.size(); ++i)
            {
                if (m_currentLayout.buttons[i].type == ToolbarButtonType::SPACER)
                {
                    insertIndex = static_cast<int>(i);
                    break;
                }
            }
        }
        addButtonToToolbar(m_availableButtons[static_cast<size_t>(selectedRow)], insertIndex);

        // Auto-select and scroll to new item for better UX
        m_currentList.selectRow(insertIndex);
        m_currentList.scrollToEnsureRowIsOnscreen(insertIndex);
    }
}

void ToolbarCustomizationDialog::onRemoveButtonClicked()
{
    int selectedRow = m_currentList.getSelectedRow();
    if (selectedRow >= 0 && selectedRow < static_cast<int>(m_currentLayout.buttons.size()))
    {
        removeButtonFromToolbar(selectedRow);
    }
}

void ToolbarCustomizationDialog::onAddSeparatorClicked()
{
    static int separatorCount = 1;
    ToolbarButtonConfig sep = ToolbarButtonConfig::separator(
        "sep" + juce::String(separatorCount++), 8);

    // Calculate insert position: after selection in current list, or before first spacer
    int insertIndex = m_currentList.getSelectedRow();
    if (insertIndex >= 0)
    {
        insertIndex++;
    }
    else
    {
        // No selection - insert BEFORE the first spacer (so items appear in the content area,
        // not after the flexible spacer which would push them to the far right edge)
        insertIndex = static_cast<int>(m_currentLayout.buttons.size()); // Default: end
        for (size_t i = 0; i < m_currentLayout.buttons.size(); ++i)
        {
            if (m_currentLayout.buttons[i].type == ToolbarButtonType::SPACER)
            {
                insertIndex = static_cast<int>(i);
                break;
            }
        }
    }
    addButtonToToolbar(sep, insertIndex);

    // Auto-select and scroll to new item for better UX
    m_currentList.selectRow(insertIndex);
    m_currentList.scrollToEnsureRowIsOnscreen(insertIndex);
}

void ToolbarCustomizationDialog::onAddSpacerClicked()
{
    static int spacerCount = 1;
    ToolbarButtonConfig spacer = ToolbarButtonConfig::spacer(
        "spacer" + juce::String(spacerCount++), 16);

    // Calculate insert position: after selection in current list, or before first spacer
    int insertIndex = m_currentList.getSelectedRow();
    if (insertIndex >= 0)
    {
        insertIndex++;
    }
    else
    {
        // No selection - insert BEFORE the first spacer (so items appear in the content area,
        // not after the flexible spacer which would push them to the far right edge)
        insertIndex = static_cast<int>(m_currentLayout.buttons.size()); // Default: end
        for (size_t i = 0; i < m_currentLayout.buttons.size(); ++i)
        {
            if (m_currentLayout.buttons[i].type == ToolbarButtonType::SPACER)
            {
                insertIndex = static_cast<int>(i);
                break;
            }
        }
    }
    addButtonToToolbar(spacer, insertIndex);

    // Auto-select and scroll to new item for better UX
    m_currentList.selectRow(insertIndex);
    m_currentList.scrollToEnsureRowIsOnscreen(insertIndex);
}

void ToolbarCustomizationDialog::onSaveAsClicked()
{
    auto alertWindow = std::make_shared<juce::AlertWindow>(
        "Save Layout As",
        "Enter a name for this toolbar layout:",
        juce::MessageBoxIconType::QuestionIcon);

    alertWindow->addTextEditor("layoutName", m_currentLayout.name, "Layout name:");
    alertWindow->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey, 0, 0));
    alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey, 0, 0));

    alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, alertWindow](int result)
        {
            if (result == 1)
            {
                auto name = alertWindow->getTextEditorContents("layoutName");
                if (name.isNotEmpty())
                {
                    // Update the current layout name and save
                    m_currentLayout.name = name;
                    m_toolbarManager.saveCurrentLayoutAs(name);

                    // Refresh layout selector
                    m_layoutSelector.clear();
                    auto layouts = m_toolbarManager.getAvailableLayouts();
                    for (int i = 0; i < layouts.size(); ++i)
                    {
                        m_layoutSelector.addItem(layouts[i], i + 1);
                        if (layouts[i] == name)
                            m_layoutSelector.setSelectedId(i + 1, juce::dontSendNotification);
                    }
                }
            }
        }), true);
}

void ToolbarCustomizationDialog::onResetClicked()
{
    m_currentLayout = m_originalLayout;
    refreshCurrentButtons();
    m_layoutChanged = false;
}

void ToolbarCustomizationDialog::onImportClicked()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Import Toolbar Layout",
        ToolbarManager::getToolbarsDirectory(),
        "*.json");

    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                         juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                if (m_toolbarManager.importLayout(file, true))
                {
                    m_currentLayout = m_toolbarManager.getCurrentLayout();
                    refreshCurrentButtons();
                    m_layoutChanged = true;

                    // Refresh layout selector
                    m_layoutSelector.clear();
                    auto layouts = m_toolbarManager.getAvailableLayouts();
                    for (int i = 0; i < layouts.size(); ++i)
                    {
                        m_layoutSelector.addItem(layouts[i], i + 1);
                        if (layouts[i] == m_currentLayout.name)
                            m_layoutSelector.setSelectedId(i + 1, juce::dontSendNotification);
                    }
                }
            }
        });
}

void ToolbarCustomizationDialog::onExportClicked()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Export Toolbar Layout",
        ToolbarManager::getToolbarsDirectory().getChildFile(m_currentLayout.name + ".json"),
        "*.json");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                         juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File())
            {
                m_currentLayout.saveToJSON(file);
            }
        });
}

void ToolbarCustomizationDialog::onOkClicked()
{
    m_layoutChanged = true;
    m_cancelled = false;

    // Apply the current layout
    // The ToolbarManager should already have the layout from onLayoutSelectionChanged
    // But we might have modified it, so we should update it
    m_toolbarManager.saveToSettings();

    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
        dialog->exitModalState(1);
}

void ToolbarCustomizationDialog::onCancelClicked()
{
    m_cancelled = true;

    // Restore original layout
    m_toolbarManager.loadLayout(m_originalLayout.name);

    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
        dialog->exitModalState(0);
}

void ToolbarCustomizationDialog::addButtonToToolbar(const ToolbarButtonConfig& button, int insertIndex)
{
    if (insertIndex < 0 || insertIndex > static_cast<int>(m_currentLayout.buttons.size()))
        insertIndex = static_cast<int>(m_currentLayout.buttons.size());

    m_currentLayout.buttons.insert(
        m_currentLayout.buttons.begin() + insertIndex, button);

    refreshCurrentButtons();
    m_layoutChanged = true;

    // Update the toolbar manager with our modified layout
    // This updates internal state, saves to disk, and notifies listeners
    m_toolbarManager.updateCurrentLayout(m_currentLayout);
}

void ToolbarCustomizationDialog::removeButtonFromToolbar(int index)
{
    if (index >= 0 && index < static_cast<int>(m_currentLayout.buttons.size()))
    {
        m_currentLayout.buttons.erase(m_currentLayout.buttons.begin() + index);
        refreshCurrentButtons();
        m_layoutChanged = true;

        // Update the toolbar manager
        m_toolbarManager.updateCurrentLayout(m_currentLayout);
    }
}

void ToolbarCustomizationDialog::moveButtonInToolbar(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= static_cast<int>(m_currentLayout.buttons.size()))
        return;
    if (toIndex < 0 || toIndex >= static_cast<int>(m_currentLayout.buttons.size()))
        return;
    if (fromIndex == toIndex)
        return;

    auto button = m_currentLayout.buttons[static_cast<size_t>(fromIndex)];
    m_currentLayout.buttons.erase(m_currentLayout.buttons.begin() + fromIndex);
    m_currentLayout.buttons.insert(m_currentLayout.buttons.begin() + toIndex, button);

    refreshCurrentButtons();
    m_layoutChanged = true;

    // Update the toolbar manager
    m_toolbarManager.updateCurrentLayout(m_currentLayout);
}

juce::String ToolbarCustomizationDialog::getButtonDisplayName(const ToolbarButtonConfig& config) const
{
    switch (config.type)
    {
        case ToolbarButtonType::COMMAND:
            return config.commandName.isNotEmpty() ? config.commandName : config.id;

        case ToolbarButtonType::PLUGIN:
            return config.pluginIdentifier.isNotEmpty() ? config.pluginIdentifier : "Plugin";

        case ToolbarButtonType::TRANSPORT:
            return "Transport Controls";

        case ToolbarButtonType::SEPARATOR:
            return "---";

        case ToolbarButtonType::SPACER:
            return "(Spacer)";

        default:
            return config.id;
    }
}

juce::String ToolbarCustomizationDialog::getButtonTypeLabel(ToolbarButtonType type) const
{
    switch (type)
    {
        case ToolbarButtonType::COMMAND:   return "CMD";
        case ToolbarButtonType::PLUGIN:    return "PLG";
        case ToolbarButtonType::TRANSPORT: return "XPORT";
        case ToolbarButtonType::SEPARATOR: return "SEP";
        case ToolbarButtonType::SPACER:    return "SPC";
        default:                           return "???";
    }
}
