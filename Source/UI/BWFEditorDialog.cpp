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
#include "ThemeManager.h"
#include "UIConstants.h"
#include <regex>

namespace ui = waveedit::ui;

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
        // Inherit themed default text colour so the dialog re-skins.
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
        // Rely on the themed LookAndFeel defaults for editor colours so the
        // dialog re-skins; only override the focus ring with the accent token.
        const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();
        editor.setColour(juce::TextEditor::focusedOutlineColourId, theme.accent);
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
        label.setColour(juce::Label::textColourId,
                        waveedit::ThemeManager::getInstance().getCurrent().textMuted);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(ui::smallFont());
        addAndMakeVisible(label);
    };

    setupCharCount(m_descriptionCharCount);
    setupCharCount(m_originatorCharCount);
    setupCharCount(m_originatorRefCharCount);

    // Setup format hint labels
    auto setupHint = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId,
                        waveedit::ThemeManager::getInstance().getCurrent().textMuted);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setFont(ui::smallFont());
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

    // Allow component-level Enter/Escape handling (REVIEW-DESIGN H8).
    setWantsKeyboardFocus(true);

    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);
}

BWFEditorDialog::~BWFEditorDialog()
{
}

//==============================================================================
void BWFEditorDialog::paint(juce::Graphics& g)
{
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();
    g.fillAll(theme.background);

    // Draw section header
    g.setColour(theme.text);
    g.setFont(ui::bodyFont());
    g.drawText("Edit BWF (Broadcast Wave Format) Metadata", SPACING, SPACING,
               DIALOG_WIDTH - 2 * SPACING, 20, juce::Justification::centred, false);

    // Draw separator line
    g.setColour(theme.border);
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
        if (!validateDateTime())
            return;  // Block save on malformed date/time (inline hints show why)

        saveMetadata();
        if (m_onApply)
            m_onApply();

        if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
            dialogWindow->exitModalState(1);
    }
    else if (button == &m_applyButton)
    {
        if (!validateDateTime())
            return;  // Block save on malformed date/time (inline hints show why)

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

void BWFEditorDialog::textEditorTextChanged(juce::TextEditor& /*editor*/)
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

    DBG("BWFEditorDialog::saveMetadata() - BWF metadata updated");
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

    DBG("BWFEditorDialog::setCurrentDateTime() - Set to current date/time");
}

void BWFEditorDialog::updateCharacterCounts()
{
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();

    // The editors hard-cap input via setInputRestrictions(), so overflow can
    // never occur. Instead, show used/limit in a normal colour and warn (amber)
    // when within ~10 chars of the limit so the counter is meaningful (H10).
    auto updateCount = [&theme](juce::Label& label, int used, int limit)
    {
        label.setText(juce::String(used) + " / " + juce::String(limit), juce::dontSendNotification);
        const bool nearLimit = (limit - used) <= 10;
        label.setColour(juce::Label::textColourId, nearLimit ? theme.warning : theme.textMuted);
    };

    updateCount(m_descriptionCharCount, m_descriptionEditor.getText().length(), 256);
    updateCount(m_originatorCharCount, m_originatorEditor.getText().length(), 32);
    updateCount(m_originatorRefCharCount, m_originatorRefEditor.getText().length(), 32);
}

bool BWFEditorDialog::validateDateTime()
{
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();
    bool valid = true;

    // Date: YYYY-MM-DD with sane month/day ranges. Empty is allowed.
    juce::String date = m_originationDateEditor.getText().trim();
    bool dateOk = date.isEmpty();
    if (!dateOk)
    {
        std::regex re(R"(^(\d{4})-(\d{2})-(\d{2})$)");
        std::smatch m;
        const std::string s = date.toStdString();
        if (std::regex_match(s, m, re))
        {
            int month = std::stoi(m[2].str());
            int day   = std::stoi(m[3].str());
            dateOk = (month >= 1 && month <= 12 && day >= 1 && day <= 31);
        }
    }
    m_dateFormatLabel.setText(dateOk ? "(yyyy-mm-dd)" : "Invalid - use yyyy-mm-dd",
                              juce::dontSendNotification);
    m_dateFormatLabel.setColour(juce::Label::textColourId, dateOk ? theme.textMuted : theme.error);
    valid = valid && dateOk;

    // Time: HH:MM:SS with sane ranges. Empty is allowed.
    juce::String time = m_originationTimeEditor.getText().trim();
    bool timeOk = time.isEmpty();
    if (!timeOk)
    {
        std::regex re(R"(^(\d{2}):(\d{2}):(\d{2})$)");
        std::smatch m;
        const std::string s = time.toStdString();
        if (std::regex_match(s, m, re))
        {
            int hh = std::stoi(m[1].str());
            int mm = std::stoi(m[2].str());
            int ss = std::stoi(m[3].str());
            timeOk = (hh <= 23 && mm <= 59 && ss <= 59);
        }
    }
    m_timeFormatLabel.setText(timeOk ? "(hh:mm:ss)" : "Invalid - use hh:mm:ss",
                              juce::dontSendNotification);
    m_timeFormatLabel.setColour(juce::Label::textColourId, timeOk ? theme.textMuted : theme.error);
    valid = valid && timeOk;

    return valid;
}

bool BWFEditorDialog::keyPressed(const juce::KeyPress& key)
{
    // Enter confirms (OK) when fields validate; Escape cancels (REVIEW-DESIGN H8).
    if (key == juce::KeyPress::returnKey)
    {
        if (validateDateTime())
            buttonClicked(&m_okButton);
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        buttonClicked(&m_cancelButton);
        return true;
    }
    return false;
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
    options.dialogBackgroundColour = waveedit::ThemeManager::getInstance().getCurrent().background;
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
