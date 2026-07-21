/*
  ==============================================================================

    InsertSilenceDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "InsertSilenceDialog.h"
#include "ThemeManager.h"
#include "UIConstants.h"

namespace
{
    constexpr int DIALOG_WIDTH = 400;
    constexpr int DIALOG_HEIGHT = 230;
    constexpr int PADDING = waveedit::ui::kDialogPadding;
    constexpr int ROW = 28;
    constexpr int SPACING = 10;

    inline waveedit::Theme theme()
        { return waveedit::ThemeManager::getInstance().getCurrent(); }
}

//==============================================================================
InsertSilenceDialog::InsertSilenceDialog(double defaultSeconds, bool hasSelection)
    : m_hasSelection(hasSelection)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);

    m_titleLabel.setText("Insert Silence", juce::dontSendNotification);
    m_titleLabel.setFont(waveedit::ui::dialogTitleFont());
    m_titleLabel.setColour(juce::Label::textColourId, theme().text);
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    m_durationLabel.setText("Duration (seconds):", juce::dontSendNotification);
    m_durationLabel.setFont(waveedit::ui::bodyFont());
    m_durationLabel.setColour(juce::Label::textColourId, theme().text);
    addAndMakeVisible(m_durationLabel);

    m_durationEditor.setMultiLine(false);
    m_durationEditor.setReturnKeyStartsNewLine(false);
    m_durationEditor.setScrollbarsShown(false);
    m_durationEditor.setFont(waveedit::ui::bodyFont());
    m_durationEditor.setColour(juce::TextEditor::backgroundColourId, theme().panelAlternate);
    m_durationEditor.setColour(juce::TextEditor::textColourId, theme().text);
    m_durationEditor.setColour(juce::TextEditor::outlineColourId, theme().accent);
    m_durationEditor.setText(juce::String(defaultSeconds, 3), juce::dontSendNotification);
    m_durationEditor.setEnabled(!m_hasSelection);
    m_durationEditor.addListener(this);
    addAndMakeVisible(m_durationEditor);

    m_noteLabel.setFont(waveedit::ui::smallFont());
    m_noteLabel.setColour(juce::Label::textColourId, theme().textMuted);
    m_noteLabel.setText(m_hasSelection
                            ? "A selection is active - it will be silenced (duration ignored)."
                            : "Silence is inserted at the edit cursor.",
                        juce::dontSendNotification);
    addAndMakeVisible(m_noteLabel);

    m_validationLabel.setFont(waveedit::ui::smallFont());
    m_validationLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_validationLabel);

    m_insertButton.setButtonText(m_hasSelection ? "Silence" : "Insert");
    m_insertButton.addListener(this);
    addAndMakeVisible(m_insertButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.addListener(this);
    addAndMakeVisible(m_cancelButton);

    validateInput();
    m_durationEditor.grabKeyboardFocus();
}

InsertSilenceDialog::~InsertSilenceDialog()
{
    m_durationEditor.removeListener(this);
}

void InsertSilenceDialog::paint(juce::Graphics& g)
{
    g.fillAll(theme().panel);
    g.setColour(theme().accent);
    g.drawRect(getLocalBounds(), 2);
}

void InsertSilenceDialog::resized()
{
    auto bounds = getLocalBounds().reduced(PADDING);

    m_titleLabel.setBounds(bounds.removeFromTop(ROW));
    bounds.removeFromTop(SPACING);

    m_durationLabel.setBounds(bounds.removeFromTop(ROW));
    m_durationEditor.setBounds(bounds.removeFromTop(ROW));
    bounds.removeFromTop(SPACING / 2);

    m_noteLabel.setBounds(bounds.removeFromTop(ROW));
    bounds.removeFromTop(SPACING / 2);
    m_validationLabel.setBounds(bounds.removeFromTop(ROW));

    auto buttonRow = getLocalBounds().reduced(PADDING).removeFromBottom(waveedit::ui::kButtonHeight);
    m_insertButton.setBounds(buttonRow.removeFromRight(waveedit::ui::kButtonWidth));
    buttonRow.removeFromRight(SPACING);
    m_cancelButton.setBounds(buttonRow.removeFromRight(waveedit::ui::kButtonWidth));
}

void InsertSilenceDialog::buttonClicked(juce::Button* button)
{
    if (button == &m_insertButton)
    {
        confirmDialog();
    }
    else if (button == &m_cancelButton)
    {
        if (auto* modal = juce::ModalComponentManager::getInstance()->getModalComponent(0))
            modal->exitModalState(0);
    }
}

void InsertSilenceDialog::textEditorReturnKeyPressed(juce::TextEditor&)
{
    if (m_isValid)
        confirmDialog();
}

void InsertSilenceDialog::textEditorTextChanged(juce::TextEditor&)
{
    validateInput();
}

void InsertSilenceDialog::validateInput()
{
    if (m_hasSelection)
    {
        m_isValid = true;
        m_cachedSeconds = 0.0;
        m_validationLabel.setText("", juce::dontSendNotification);
        m_insertButton.setEnabled(true);
        return;
    }

    const double seconds = m_durationEditor.getText().trim().getDoubleValue();
    m_isValid = (seconds > 0.0 && seconds <= 3600.0);
    m_cachedSeconds = seconds;

    m_validationLabel.setColour(juce::Label::textColourId,
                                m_isValid ? theme().success : theme().error);
    m_validationLabel.setText(m_isValid ? "" : "Enter a duration between 0 and 3600 seconds.",
                              juce::dontSendNotification);
    m_insertButton.setEnabled(m_isValid);
}

void InsertSilenceDialog::confirmDialog()
{
    if (!m_isValid)
        return;

    if (m_callback)
        m_callback(m_cachedSeconds);

    if (auto* modal = juce::ModalComponentManager::getInstance()->getModalComponent(0))
        modal->exitModalState(0);
}

//==============================================================================
void InsertSilenceDialog::showDialog(juce::Component* /*parent*/, double defaultSeconds,
                                     bool hasSelection, std::function<void(double)> callback)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto* dialog = new InsertSilenceDialog(defaultSeconds, hasSelection);
    dialog->m_callback = std::move(callback);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = "Insert Silence";
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;
    options.launchAsync();
}
