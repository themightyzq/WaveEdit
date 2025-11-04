/*
  ==============================================================================

    iXMLEditorDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "iXMLEditorDialog.h"
#include "../Utils/UCSCategorySuggester.h"

// Dialog dimensions
namespace
{
    constexpr int DIALOG_WIDTH = 700;
    constexpr int DIALOG_HEIGHT = 750;  // Tall dialog with viewport scrolling
    constexpr int ROW_HEIGHT = 30;
    constexpr int ROW_HEIGHT_MULTILINE = 80;  // For Description editor
    constexpr int LABEL_WIDTH = 150;
    constexpr int SPACING = 10;
    constexpr int BUTTON_HEIGHT = 30;
    constexpr int BUTTON_WIDTH = 100;
}

//==============================================================================
iXMLEditorDialog::iXMLEditorDialog(iXMLMetadata& metadata,
                                   const juce::String& filename,
                                   std::function<void()> onApply)
    : m_metadata(metadata),
      m_filename(filename),
      m_onApply(onApply)
{
    // Setup labels
    auto setupLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        label.setJustificationType(juce::Justification::centredRight);
        m_contentComponent.addAndMakeVisible(&label);
    };

    setupLabel(m_categoryLabel, "Category:");
    setupLabel(m_subcategoryLabel, "Subcategory:");
    setupLabel(m_categoryFullLabel, "CategoryFull:");
    setupLabel(m_fxNameLabel, "FX Name:");
    setupLabel(m_descriptionLabel, "Description:");
    setupLabel(m_keywordsLabel, "Keywords:");
    setupLabel(m_designerLabel, "Designer:");
    setupLabel(m_trackTitleLabel, "Track Title:");
    setupLabel(m_projectLabel, "Project:");
    setupLabel(m_tapeLabel, "Library:");

    // Setup text editors
    auto setupEditor = [this](juce::TextEditor& editor, int maxChars = 0, bool multiline = false)
    {
        editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff3a3a3a));
        editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::grey);
        editor.setMultiLine(multiline);
        editor.setReturnKeyStartsNewLine(multiline);
        editor.setScrollbarsShown(multiline);
        if (maxChars > 0)
            editor.setInputRestrictions(maxChars);
        editor.addListener(this);
        m_contentComponent.addAndMakeVisible(&editor);
    };

    // UCS Category fields (ALL CAPS for Category, Title Case for Subcategory)
    setupEditor(m_categoryEditor, 32);
    setupEditor(m_subcategoryEditor, 32);

    // SoundMiner Extended fields
    setupEditor(m_fxNameEditor, 256);
    setupEditor(m_descriptionEditor, 0, true);  // Multi-line, no char limit
    setupEditor(m_keywordsEditor, 0, true);     // Multi-line, comma-separated

    // Standard fields
    setupEditor(m_designerEditor, 32);
    setupEditor(m_trackTitleEditor, 256);
    setupEditor(m_projectEditor, 256);
    setupEditor(m_tapeEditor, 256);

    // Setup hint/count labels
    auto setupHintLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colours::grey);
        label.setFont(juce::Font(11.0f));
        label.setJustificationType(juce::Justification::centredLeft);
        m_contentComponent.addAndMakeVisible(&label);
    };

    setupHintLabel(m_categoryHint, "ALL CAPS (e.g., DOOR, AMBIENCE)");
    setupHintLabel(m_subcategoryHint, "Title Case (e.g., Wood, Birdsong)");
    setupHintLabel(m_keywordsHint, "Comma-separated (e.g., door, wood, creak)");
    setupHintLabel(m_fxNameCount, "0 / 256");
    setupHintLabel(m_descriptionCount, "0 chars");

    // CategoryFull value (read-only, computed)
    m_categoryFullValue.setColour(juce::Label::textColourId, juce::Colours::white);
    m_categoryFullValue.setJustificationType(juce::Justification::centredLeft);
    m_categoryFullValue.setFont(juce::Font(13.0f, juce::Font::bold));
    m_contentComponent.addAndMakeVisible(&m_categoryFullValue);

    // Setup buttons
    m_autoPopulateButton.setButtonText("Auto-Populate from Filename");
    m_autoPopulateButton.addListener(this);
    m_contentComponent.addAndMakeVisible(&m_autoPopulateButton);

    m_suggestCategoryButton.setButtonText("Suggest Category from Keywords");
    m_suggestCategoryButton.addListener(this);
    m_contentComponent.addAndMakeVisible(&m_suggestCategoryButton);

    m_applyButton.setButtonText("Apply");
    m_applyButton.addListener(this);
    addAndMakeVisible(&m_applyButton);

    m_okButton.setButtonText("OK");
    m_okButton.addListener(this);
    addAndMakeVisible(&m_okButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.addListener(this);
    addAndMakeVisible(&m_cancelButton);

    // Setup viewport
    m_viewport.setViewedComponent(&m_contentComponent, false);
    m_viewport.setScrollBarsShown(true, false);  // Vertical scrollbar only
    addAndMakeVisible(&m_viewport);

    // Load metadata into UI
    loadMetadata();

    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);
}

iXMLEditorDialog::~iXMLEditorDialog()
{
}

//==============================================================================
void iXMLEditorDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));

    // Draw section header at top (above viewport)
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("SoundMiner / iXML Metadata Editor",
               SPACING, SPACING, DIALOG_WIDTH - 2 * SPACING, 25,
               juce::Justification::centred, false);
}

void iXMLEditorDialog::resized()
{
    auto bounds = getLocalBounds().reduced(SPACING);

    // Skip title header
    bounds.removeFromTop(35);

    // Reserve space for buttons at bottom (outside viewport)
    auto buttonArea = bounds.removeFromBottom(BUTTON_HEIGHT + SPACING);
    int buttonSpacing = SPACING;
    int totalButtonWidth = 3 * BUTTON_WIDTH + 2 * buttonSpacing;
    auto centeredButtons = buttonArea.withSizeKeepingCentre(totalButtonWidth, BUTTON_HEIGHT);

    m_cancelButton.setBounds(centeredButtons.removeFromLeft(BUTTON_WIDTH));
    centeredButtons.removeFromLeft(buttonSpacing);
    m_applyButton.setBounds(centeredButtons.removeFromLeft(BUTTON_WIDTH));
    centeredButtons.removeFromLeft(buttonSpacing);
    m_okButton.setBounds(centeredButtons);

    // Viewport takes remaining space
    m_viewport.setBounds(bounds);

    // Layout content component with all fields
    int contentWidth = bounds.getWidth() - m_viewport.getScrollBarThickness();
    int yPos = SPACING;

    auto layoutRow = [&](juce::Label& label, juce::Component& editor, int editorHeight = ROW_HEIGHT)
    {
        label.setBounds(SPACING, yPos, LABEL_WIDTH, ROW_HEIGHT);
        editor.setBounds(LABEL_WIDTH + 2 * SPACING, yPos,
                        contentWidth - LABEL_WIDTH - 3 * SPACING, editorHeight);
        yPos += editorHeight + SPACING / 2;
    };

    auto layoutHint = [&](juce::Label& hint)
    {
        hint.setBounds(LABEL_WIDTH + 2 * SPACING, yPos,
                      contentWidth - LABEL_WIDTH - 3 * SPACING, 15);
        yPos += 20;
    };

    // UCS Category Section
    layoutRow(m_categoryLabel, m_categoryEditor);
    layoutHint(m_categoryHint);

    layoutRow(m_subcategoryLabel, m_subcategoryEditor);
    layoutHint(m_subcategoryHint);

    // CategoryFull (computed, read-only)
    layoutRow(m_categoryFullLabel, m_categoryFullValue);
    yPos += SPACING;

    // SoundMiner Extended Section
    layoutRow(m_fxNameLabel, m_fxNameEditor);
    layoutHint(m_fxNameCount);

    layoutRow(m_descriptionLabel, m_descriptionEditor, ROW_HEIGHT_MULTILINE);
    layoutHint(m_descriptionCount);

    layoutRow(m_keywordsLabel, m_keywordsEditor, ROW_HEIGHT_MULTILINE);
    layoutHint(m_keywordsHint);

    layoutRow(m_designerLabel, m_designerEditor);
    yPos += SPACING;

    // Standard iXML Section
    layoutRow(m_trackTitleLabel, m_trackTitleEditor);
    layoutRow(m_projectLabel, m_projectEditor);
    layoutRow(m_tapeLabel, m_tapeEditor);
    yPos += SPACING;

    // Auto-Populate and Suggest Category buttons (side by side)
    int buttonWidth = (contentWidth - LABEL_WIDTH - 4 * SPACING) / 2;
    m_autoPopulateButton.setBounds(LABEL_WIDTH + 2 * SPACING, yPos,
                                   buttonWidth, BUTTON_HEIGHT);
    m_suggestCategoryButton.setBounds(LABEL_WIDTH + 3 * SPACING + buttonWidth, yPos,
                                      buttonWidth, BUTTON_HEIGHT);
    yPos += BUTTON_HEIGHT + SPACING * 2;

    // Set content component size
    m_contentComponent.setSize(contentWidth, yPos);
}

//==============================================================================
void iXMLEditorDialog::buttonClicked(juce::Button* button)
{
    if (button == &m_autoPopulateButton)
    {
        autoPopulateFromFilename();
    }
    else if (button == &m_suggestCategoryButton)
    {
        suggestCategory();
    }
    else if (button == &m_applyButton)
    {
        saveMetadata();
        if (m_onApply)
            m_onApply();
    }
    else if (button == &m_okButton)
    {
        saveMetadata();
        if (m_onApply)
            m_onApply();

        // Close dialog
        if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
            dialogWindow->exitModalState(1);
    }
    else if (button == &m_cancelButton)
    {
        // Close dialog without saving
        if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
            dialogWindow->exitModalState(0);
    }
}

void iXMLEditorDialog::textEditorTextChanged(juce::TextEditor& editor)
{
    updateCharacterCounts();

    // Update CategoryFull when Category or Subcategory changes
    if (&editor == &m_categoryEditor || &editor == &m_subcategoryEditor)
    {
        juce::String category = m_categoryEditor.getText().trim();
        juce::String subcategory = m_subcategoryEditor.getText().trim();

        juce::String categoryFull;
        if (category.isNotEmpty())
        {
            categoryFull = category;
            if (subcategory.isNotEmpty())
                categoryFull += "-" + subcategory;
        }
        m_categoryFullValue.setText(categoryFull.isEmpty() ? "(Not set)" : categoryFull,
                                    juce::dontSendNotification);
    }
}

//==============================================================================
void iXMLEditorDialog::loadMetadata()
{
    m_categoryEditor.setText(m_metadata.getCategory(), false);
    m_subcategoryEditor.setText(m_metadata.getSubcategory(), false);
    m_fxNameEditor.setText(m_metadata.getFXName(), false);
    m_descriptionEditor.setText(m_metadata.getDescription(), false);
    m_keywordsEditor.setText(m_metadata.getKeywords(), false);
    m_designerEditor.setText(m_metadata.getDesigner(), false);
    m_trackTitleEditor.setText(m_metadata.getTrackTitle(), false);
    m_projectEditor.setText(m_metadata.getProject(), false);
    m_tapeEditor.setText(m_metadata.getTape(), false);

    // Update CategoryFull display
    juce::String categoryFull = m_metadata.getCategoryFull();
    m_categoryFullValue.setText(categoryFull.isEmpty() ? "(Not set)" : categoryFull,
                                juce::dontSendNotification);

    updateCharacterCounts();
}

void iXMLEditorDialog::saveMetadata()
{
    m_metadata.setCategory(m_categoryEditor.getText().trim());
    m_metadata.setSubcategory(m_subcategoryEditor.getText().trim());
    m_metadata.setFXName(m_fxNameEditor.getText().trim());
    m_metadata.setDescription(m_descriptionEditor.getText().trim());
    m_metadata.setKeywords(m_keywordsEditor.getText().trim());
    m_metadata.setDesigner(m_designerEditor.getText().trim());
    m_metadata.setTrackTitle(m_trackTitleEditor.getText().trim());
    m_metadata.setProject(m_projectEditor.getText().trim());
    m_metadata.setTape(m_tapeEditor.getText().trim());
}

void iXMLEditorDialog::autoPopulateFromFilename()
{
    if (m_filename.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Filename Available",
            "Cannot auto-populate: No filename provided.\n\n"
            "Please use 'Suggest Category from Keywords' instead, which analyzes "
            "the current FX Name and Description fields.",
            "OK"
        );
        return;
    }

    // Parse UCS-compliant filename
    iXMLMetadata tempMetadata = iXMLMetadata::fromUCSFilename(m_filename);

    if (tempMetadata.hasMetadata())
    {
        // Populate fields from UCS filename
        if (tempMetadata.getCategory().isNotEmpty())
            m_categoryEditor.setText(tempMetadata.getCategory(), false);

        if (tempMetadata.getSubcategory().isNotEmpty())
            m_subcategoryEditor.setText(tempMetadata.getSubcategory(), false);

        if (tempMetadata.getFXName().isNotEmpty())
            m_fxNameEditor.setText(tempMetadata.getFXName(), false);

        if (tempMetadata.getDesigner().isNotEmpty())
            m_designerEditor.setText(tempMetadata.getDesigner(), false);

        if (tempMetadata.getProject().isNotEmpty())
            m_projectEditor.setText(tempMetadata.getProject(), false);

        // Update CategoryFull display
        textEditorTextChanged(m_categoryEditor);

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Auto-Populate Successful",
            "Metadata populated from UCS filename:\n\n" +
            m_filename + "\n\n" +
            "Category: " + tempMetadata.getCategory() + "\n" +
            "FX Name: " + tempMetadata.getFXName() + "\n" +
            "Designer: " + tempMetadata.getDesigner(),
            "OK"
        );
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Not a UCS Filename",
            "Filename does not match UCS format:\n\n" +
            m_filename + "\n\n" +
            "Expected format: CatID_FXName_CreatorID_SourceID.wav\n" +
            "Example: MAGEvil_DESIGNED airMagic explosion 01_PGN_DM.wav\n\n" +
            "Try using 'Suggest Category from Keywords' instead.",
            "OK"
        );
    }
}

void iXMLEditorDialog::suggestCategory()
{
    // Use UCS suggester to analyze current metadata
    UCSCategorySuggester suggester;

    juce::String currentFXName = m_fxNameEditor.getText();
    juce::String currentDescription = m_descriptionEditor.getText();
    juce::String currentKeywords = m_keywordsEditor.getText();

    // Get top 3 suggestions
    auto suggestions = suggester.suggestCategories(
        m_filename.isNotEmpty() ? m_filename : currentFXName,
        currentDescription,
        currentKeywords,
        3
    );

    if (suggestions.empty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Suggestions Found",
            "Could not find matching UCS categories.\n\n"
            "Try adding more descriptive keywords to the FX Name, Description, or Keywords fields.",
            "OK"
        );
        return;
    }

    // Build suggestion message
    juce::String message = "Based on your filename and metadata, here are the top category suggestions:\n\n";
    for (size_t i = 0; i < suggestions.size(); ++i)
    {
        message += juce::String(i + 1) + ". " + suggestions[i].category;
        if (suggestions[i].subcategory.isNotEmpty())
            message += " - " + suggestions[i].subcategory;
        message += juce::String::formatted(" (%.0f%% confidence)\n", suggestions[i].confidence * 100.0f);
    }
    message += "\nApply the top suggestion?";

    // Show confirmation dialog
    bool apply = juce::NativeMessageBox::showOkCancelBox(
        juce::MessageBoxIconType::QuestionIcon,
        "Category Suggestions",
        message,
        nullptr,  // parent component
        juce::ModalCallbackFunction::create([this, suggestions](int result)
        {
            if (result == 1 && !suggestions.empty())
            {
                // Apply top suggestion
                m_categoryEditor.setText(suggestions[0].category, false);
                m_subcategoryEditor.setText(suggestions[0].subcategory, false);

                // Update CategoryFull display
                textEditorTextChanged(m_categoryEditor);

                juce::Logger::writeToLog("Applied UCS category suggestion: " +
                                        suggestions[0].category + " - " +
                                        suggestions[0].subcategory);
            }
        })
    );

    // Note: The callback above handles the apply logic
    (void)apply;  // Suppress unused warning
}

void iXMLEditorDialog::updateCharacterCounts()
{
    // FXName character count
    int fxNameLen = m_fxNameEditor.getText().length();
    m_fxNameCount.setText(juce::String(fxNameLen) + " / 256", juce::dontSendNotification);
    m_fxNameCount.setColour(juce::Label::textColourId,
                           fxNameLen > 256 ? juce::Colours::red : juce::Colours::grey);

    // Description character count
    int descLen = m_descriptionEditor.getText().length();
    m_descriptionCount.setText(juce::String(descLen) + " chars", juce::dontSendNotification);
}

//==============================================================================
void iXMLEditorDialog::showDialog(juce::Component* parentComponent,
                                  iXMLMetadata& metadata,
                                  const juce::String& filename,
                                  std::function<void()> onApply)
{
    auto* dialog = new iXMLEditorDialog(metadata, filename, onApply);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = "Edit SoundMiner / iXML Metadata";
    options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;
    options.componentToCentreAround = parentComponent;

    options.launchAsync();
}
