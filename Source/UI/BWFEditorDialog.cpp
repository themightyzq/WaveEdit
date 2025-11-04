/*
  ==============================================================================

    BWFEditorDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "BWFEditorDialog.h"

// Dialog dimensions
namespace
{
    constexpr int DIALOG_WIDTH = 600;
    constexpr int DIALOG_HEIGHT = 620;
    constexpr int ROW_HEIGHT = 30;
    constexpr int MULTILINE_HEIGHT = 80;
    constexpr int LABEL_WIDTH = 150;
    constexpr int SPACING = 10;
    constexpr int BUTTON_HEIGHT = 30;
    constexpr int BUTTON_WIDTH = 80;
}

//==============================================================================
BWFEditorDialog::BWFEditorDialog(BWFMetadata& metadata, std::function<void()> onApply)
    : m_metadata(metadata), m_onApply(onApply)
{
    // Setup labels
    auto setupLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        label.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(label);
    };

    setupLabel(m_descriptionLabel, "Description:");
    setupLabel(m_originatorLabel, "Originator:");
    setupLabel(m_originatorRefLabel, "Originator Ref:");
    setupLabel(m_originationDateLabel, "Origination Date:");
    setupLabel(m_originationTimeLabel, "Origination Time:");
    setupLabel(m_timeReferenceLabel, "Time Reference:");
    setupLabel(m_codingHistoryLabel, "Coding History:");

    // Setup text editors
    auto setupEditor = [this](juce::TextEditor& editor, int maxChars = 0)
    {
        editor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff3a3a3a));
        editor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
        editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::grey);
        editor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::lightblue);
        if (maxChars > 0)
            editor.setInputRestrictions(maxChars);
        editor.addListener(this);
        addAndMakeVisible(editor);
    };

    setupEditor(m_descriptionEditor, 256);
    setupEditor(m_originatorEditor, 32);
    setupEditor(m_originatorRefEditor, 32);
    setupEditor(m_originationDateEditor, 10);
    setupEditor(m_originationTimeEditor, 8);
    setupEditor(m_timeReferenceEditor);
    setupEditor(m_codingHistoryEditor);

    // Coding history is multi-line
    m_codingHistoryEditor.setMultiLine(true, true);
    m_codingHistoryEditor.setReturnKeyStartsNewLine(true);

    // Setup character count labels
    auto setupCharCount = [this](juce::Label& label)
    {
        label.setColour(juce::Label::textColourId, juce::Colours::grey);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::Font(11.0f));
        addAndMakeVisible(label);
    };

    setupCharCount(m_descriptionCharCount);
    setupCharCount(m_originatorCharCount);
    setupCharCount(m_originatorRefCharCount);

    // Setup format hint labels
    auto setupHint = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colours::grey);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(juce::Font(11.0f));
        addAndMakeVisible(label);
    };

    setupHint(m_dateFormatLabel, "(yyyy-mm-dd)");
    setupHint(m_timeFormatLabel, "(hh:mm:ss)");
    setupHint(m_timeReferenceHint, "(samples from midnight, typically 0)");
    setupHint(m_codingHistoryHint, "(e.g., A=PCM,F=44100,W=16,M=stereo,T=WaveEdit)");

    // Setup "Set Current" button
    m_setCurrentButton.setButtonText("Set Current");
    m_setCurrentButton.addListener(this);
    addAndMakeVisible(m_setCurrentButton);

    // Setup action buttons
    m_okButton.setButtonText("OK");
    m_okButton.addListener(this);
    addAndMakeVisible(m_okButton);

    m_applyButton.setButtonText("Apply");
    m_applyButton.addListener(this);
    addAndMakeVisible(m_applyButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.addListener(this);
    addAndMakeVisible(m_cancelButton);

    // Load current metadata
    loadMetadata();
    updateCharacterCounts();

    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);
}

BWFEditorDialog::~BWFEditorDialog()
{
}

//==============================================================================
void BWFEditorDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));

    // Draw section header
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    g.drawText("Edit BWF (Broadcast Wave Format) Metadata", SPACING, SPACING,
               DIALOG_WIDTH - 2 * SPACING, 20, juce::Justification::centred, false);

    // Draw separator line
    g.setColour(juce::Colours::grey);
    g.drawLine(SPACING, ROW_HEIGHT + SPACING,
               DIALOG_WIDTH - SPACING, ROW_HEIGHT + SPACING, 1.0f);
}

void BWFEditorDialog::resized()
{
    auto bounds = getLocalBounds().reduced(SPACING);

    // Skip header
    bounds.removeFromTop(ROW_HEIGHT + SPACING);

    // Layout helper
    auto layoutRow = [&](juce::Label& label, juce::Component& editor, juce::Label* hint = nullptr, int height = ROW_HEIGHT)
    {
        auto row = bounds.removeFromTop(height);
        label.setBounds(row.removeFromLeft(LABEL_WIDTH));
        row.removeFromLeft(SPACING);

        if (hint != nullptr)
        {
            // Split remaining space between editor and hint
            int hintWidth = 200;
            auto editorBounds = row.removeFromLeft(row.getWidth() - hintWidth - SPACING);
            editor.setBounds(editorBounds);
            row.removeFromLeft(SPACING);
            hint->setBounds(row);
        }
        else
        {
            editor.setBounds(row);
        }

        bounds.removeFromTop(5); // Small gap between rows
    };

    // Description field with character count
    layoutRow(m_descriptionLabel, m_descriptionEditor, &m_descriptionCharCount);

    // Originator field with character count
    layoutRow(m_originatorLabel, m_originatorEditor, &m_originatorCharCount);

    // Originator Reference field with character count
    layoutRow(m_originatorRefLabel, m_originatorRefEditor, &m_originatorRefCharCount);

    // Origination Date with format hint
    layoutRow(m_originationDateLabel, m_originationDateEditor, &m_dateFormatLabel);

    // Origination Time with format hint
    layoutRow(m_originationTimeLabel, m_originationTimeEditor, &m_timeFormatLabel);

    // "Set Current" button (aligned with time field)
    auto buttonRow = bounds.removeFromTop(ROW_HEIGHT);
    buttonRow.removeFromLeft(LABEL_WIDTH + SPACING);
    m_setCurrentButton.setBounds(buttonRow.removeFromLeft(120));
    bounds.removeFromTop(5);

    // Time Reference with hint
    layoutRow(m_timeReferenceLabel, m_timeReferenceEditor, &m_timeReferenceHint);

    // Coding History with hint (multi-line)
    layoutRow(m_codingHistoryLabel, m_codingHistoryEditor, nullptr, MULTILINE_HEIGHT);
    auto hintRow = bounds.removeFromTop(20);
    hintRow.removeFromLeft(LABEL_WIDTH + SPACING);
    m_codingHistoryHint.setBounds(hintRow);

    // Action buttons at bottom
    bounds.removeFromTop(SPACING);
    auto buttonArea = getLocalBounds().removeFromBottom(BUTTON_HEIGHT + SPACING).reduced(SPACING);
    int totalButtonWidth = 3 * BUTTON_WIDTH + 2 * SPACING;
    auto buttonRow2 = buttonArea.withSizeKeepingCentre(totalButtonWidth, BUTTON_HEIGHT);

    m_okButton.setBounds(buttonRow2.removeFromLeft(BUTTON_WIDTH));
    buttonRow2.removeFromLeft(SPACING);
    m_applyButton.setBounds(buttonRow2.removeFromLeft(BUTTON_WIDTH));
    buttonRow2.removeFromLeft(SPACING);
    m_cancelButton.setBounds(buttonRow2);
}

//==============================================================================
void BWFEditorDialog::buttonClicked(juce::Button* button)
{
    if (button == &m_setCurrentButton)
    {
        setCurrentDateTime();
    }
    else if (button == &m_okButton)
    {
        saveMetadata();
        if (m_onApply)
            m_onApply();

        if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
            dialogWindow->exitModalState(1);
    }
    else if (button == &m_applyButton)
    {
        saveMetadata();
        if (m_onApply)
            m_onApply();
    }
    else if (button == &m_cancelButton)
    {
        if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
            dialogWindow->exitModalState(0);
    }
}

void BWFEditorDialog::textEditorTextChanged(juce::TextEditor& editor)
{
    updateCharacterCounts();
}

//==============================================================================
void BWFEditorDialog::loadMetadata()
{
    m_descriptionEditor.setText(m_metadata.getDescription(), false);
    m_originatorEditor.setText(m_metadata.getOriginator(), false);
    m_originatorRefEditor.setText(m_metadata.getOriginatorRef(), false);
    m_originationDateEditor.setText(m_metadata.getOriginationDate(), false);
    m_originationTimeEditor.setText(m_metadata.getOriginationTime(), false);
    m_timeReferenceEditor.setText(juce::String(m_metadata.getTimeReference()), false);
    m_codingHistoryEditor.setText(m_metadata.getCodingHistory(), false);
}

void BWFEditorDialog::saveMetadata()
{
    m_metadata.setDescription(m_descriptionEditor.getText());
    m_metadata.setOriginator(m_originatorEditor.getText());
    m_metadata.setOriginatorRef(m_originatorRefEditor.getText());
    m_metadata.setOriginationDate(m_originationDateEditor.getText());
    m_metadata.setOriginationTime(m_originationTimeEditor.getText());
    m_metadata.setTimeReference(m_timeReferenceEditor.getText().getLargeIntValue());
    m_metadata.setCodingHistory(m_codingHistoryEditor.getText());

    juce::Logger::writeToLog("BWFEditorDialog::saveMetadata() - BWF metadata updated");
}

void BWFEditorDialog::setCurrentDateTime()
{
    juce::Time currentTime = juce::Time::getCurrentTime();

    // Format date as yyyy-mm-dd
    juce::String date = juce::String::formatted("%04d-%02d-%02d",
                                                  currentTime.getYear(),
                                                  currentTime.getMonth() + 1,
                                                  currentTime.getDayOfMonth());

    // Format time as hh:mm:ss
    juce::String time = juce::String::formatted("%02d:%02d:%02d",
                                                  currentTime.getHours(),
                                                  currentTime.getMinutes(),
                                                  currentTime.getSeconds());

    m_originationDateEditor.setText(date, true);
    m_originationTimeEditor.setText(time, true);

    juce::Logger::writeToLog("BWFEditorDialog::setCurrentDateTime() - Set to current date/time");
}

void BWFEditorDialog::updateCharacterCounts()
{
    // Description (max 256)
    int descLen = m_descriptionEditor.getText().length();
    m_descriptionCharCount.setText(juce::String(descLen) + " / 256", juce::dontSendNotification);
    m_descriptionCharCount.setColour(juce::Label::textColourId,
                                      descLen > 256 ? juce::Colours::red : juce::Colours::grey);

    // Originator (max 32)
    int origLen = m_originatorEditor.getText().length();
    m_originatorCharCount.setText(juce::String(origLen) + " / 32", juce::dontSendNotification);
    m_originatorCharCount.setColour(juce::Label::textColourId,
                                     origLen > 32 ? juce::Colours::red : juce::Colours::grey);

    // Originator Reference (max 32)
    int refLen = m_originatorRefEditor.getText().length();
    m_originatorRefCharCount.setText(juce::String(refLen) + " / 32", juce::dontSendNotification);
    m_originatorRefCharCount.setColour(juce::Label::textColourId,
                                        refLen > 32 ? juce::Colours::red : juce::Colours::grey);
}

//==============================================================================
void BWFEditorDialog::showDialog(juce::Component* parentComponent,
                                  BWFMetadata& metadata,
                                  std::function<void()> onApply)
{
    auto* editorDialog = new BWFEditorDialog(metadata, onApply);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(editorDialog);
    options.dialogTitle = "Edit BWF Metadata";
    options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.useBottomRightCornerResizer = false;

    // Center over parent component
    if (parentComponent != nullptr)
    {
        auto parentBounds = parentComponent->getScreenBounds();
        auto dialogBounds = juce::Rectangle<int>(0, 0, DIALOG_WIDTH, DIALOG_HEIGHT);
        dialogBounds.setCentre(parentBounds.getCentre());
        options.content->setBounds(dialogBounds);
    }

    // Launch dialog
    options.launchAsync();
}
