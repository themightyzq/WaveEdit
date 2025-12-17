/*
  ==============================================================================

    ToolbarCustomizationDialog.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/ToolbarConfig.h"
#include "../Utils/ToolbarManager.h"

/**
 * Dialog for customizing the toolbar layout.
 *
 * Features:
 * - Two-list interface: Available buttons on left, Current toolbar on right
 * - Drag buttons between lists to add/remove
 * - Move up/down buttons for reordering
 * - Layout template selector dropdown
 * - Save current layout as new template
 * - Reset to default layout
 * - Import/Export layout files
 *
 * Workflow:
 * 1. User opens dialog (View -> Customize Toolbar... or right-click toolbar)
 * 2. Select a template from dropdown, or modify current layout
 * 3. Drag buttons between Available and Current lists
 * 4. Use Move Up/Down to reorder current toolbar buttons
 * 5. Optionally save as new template
 * 6. Click OK to apply, Cancel to discard changes
 *
 * Thread Safety: UI thread only. Must be shown from message thread.
 */
class ToolbarCustomizationDialog : public juce::Component,
                                   public juce::ListBoxModel,
                                   public juce::DragAndDropContainer,
                                   public juce::DragAndDropTarget
{
public:
    //==============================================================================

    /**
     * Creates the toolbar customization dialog.
     *
     * @param toolbarManager Reference to the toolbar manager
     * @param commandManager Reference to the application command manager (for command names)
     */
    ToolbarCustomizationDialog(ToolbarManager& toolbarManager,
                               juce::ApplicationCommandManager& commandManager);

    ~ToolbarCustomizationDialog() override;

    //==============================================================================
    // Static dialog launcher

    /**
     * Show the dialog modally.
     *
     * @param toolbarManager Reference to the toolbar manager
     * @param commandManager Reference to the application command manager
     * @return true if user clicked OK and layout was changed, false if cancelled
     */
    static bool showDialog(ToolbarManager& toolbarManager,
                           juce::ApplicationCommandManager& commandManager);

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // ListBoxModel overrides (shared between both list boxes)

    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                          bool rowIsSelected) override;
    juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;

    //==============================================================================
    // DragAndDropTarget overrides

    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

private:
    //==============================================================================
    // Internal list box model for available buttons
    class AvailableButtonsModel : public juce::ListBoxModel
    {
    public:
        AvailableButtonsModel(ToolbarCustomizationDialog& owner) : m_owner(owner) {}
        int getNumRows() override { return m_owner.m_availableButtons.size(); }
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                              bool rowIsSelected) override;
        juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;

    private:
        ToolbarCustomizationDialog& m_owner;
    };

    // Internal list box model for current toolbar buttons
    class CurrentButtonsModel : public juce::ListBoxModel
    {
    public:
        CurrentButtonsModel(ToolbarCustomizationDialog& owner) : m_owner(owner) {}
        int getNumRows() override { return m_owner.m_currentLayout.buttons.size(); }
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                              bool rowIsSelected) override;
        juce::var getDragSourceDescription(const juce::SparseSet<int>& selectedRows) override;

    private:
        ToolbarCustomizationDialog& m_owner;
    };

    //==============================================================================
    // Private methods

    void refreshAvailableButtons();
    void refreshCurrentButtons();
    void onLayoutSelectionChanged();
    void onMoveUpClicked();
    void onMoveDownClicked();
    void onAddButtonClicked();
    void onRemoveButtonClicked();
    void onAddSeparatorClicked();
    void onAddSpacerClicked();
    void onSaveAsClicked();
    void onResetClicked();
    void onImportClicked();
    void onExportClicked();
    void onOkClicked();
    void onCancelClicked();

    void addButtonToToolbar(const ToolbarButtonConfig& button, int insertIndex = -1);
    void removeButtonFromToolbar(int index);
    void moveButtonInToolbar(int fromIndex, int toIndex);

    juce::String getButtonDisplayName(const ToolbarButtonConfig& config) const;
    juce::String getButtonTypeLabel(ToolbarButtonType type) const;

    //==============================================================================
    // UI Components

    // Title and instructions
    juce::Label m_titleLabel;
    juce::Label m_instructionsLabel;

    // Layout template selector
    juce::Label m_layoutLabel;
    juce::ComboBox m_layoutSelector;

    // Available buttons list (left side)
    juce::Label m_availableLabel;
    juce::ListBox m_availableList;
    std::unique_ptr<AvailableButtonsModel> m_availableModel;

    // Current toolbar buttons list (right side)
    juce::Label m_currentLabel;
    juce::ListBox m_currentList;
    std::unique_ptr<CurrentButtonsModel> m_currentModel;

    // Transfer buttons
    juce::TextButton m_addButton;
    juce::TextButton m_removeButton;

    // Reorder buttons
    juce::TextButton m_moveUpButton;
    juce::TextButton m_moveDownButton;

    // Special item buttons
    juce::TextButton m_addSeparatorButton;
    juce::TextButton m_addSpacerButton;

    // Template management buttons
    juce::TextButton m_saveAsButton;
    juce::TextButton m_resetButton;
    juce::TextButton m_importButton;
    juce::TextButton m_exportButton;

    // Dialog buttons
    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;

    //==============================================================================
    // Data

    ToolbarManager& m_toolbarManager;
    juce::ApplicationCommandManager& m_commandManager;

    ToolbarLayout m_currentLayout;              // Working copy being edited
    ToolbarLayout m_originalLayout;             // Original layout for cancel/reset
    std::vector<ToolbarButtonConfig> m_availableButtons;  // Available buttons to add

    bool m_layoutChanged = false;
    bool m_cancelled = false;

    // Drag-drop state
    int m_dropInsertIndex = -1;
    bool m_isDraggingToAvailable = false;
    bool m_isDraggingToCurrent = false;

    //==============================================================================
    // Layout constants

    static const int kDialogWidth = 700;
    static const int kDialogHeight = 550;
    static const int kPadding = 15;
    static const int kSpacing = 10;
    static const int kButtonWidth = 100;
    static const int kButtonHeight = 28;
    static const int kListRowHeight = 24;
    static const int kLabelHeight = 20;
    static const int kComboHeight = 26;

    // Visual settings
    const juce::Colour m_backgroundColour { 0xff2b2b2b };
    const juce::Colour m_listBackgroundColour { 0xff1e1e1e };
    const juce::Colour m_selectedRowColour { 0xff3a5a8a };
    const juce::Colour m_textColour { 0xffe0e0e0 };
    const juce::Colour m_accentColour { 0xff4a90d9 };
    const juce::Colour m_separatorColour { 0xff404040 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolbarCustomizationDialog)
};
