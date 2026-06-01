/*
  ==============================================================================

    GoToPositionDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "GoToPositionDialog.h"
#include "ThemeManager.h"
#include "UIConstants.h"
#include <cctype>
#include <cmath>
#include <string>

namespace
{
    // UI Constants
    constexpr int DIALOG_WIDTH = 400;
    constexpr int DIALOG_HEIGHT = 280;
    constexpr int PADDING = waveedit::ui::kDialogPadding;
    constexpr int LABEL_HEIGHT = 24;
    constexpr int BUTTON_HEIGHT = 32;
    constexpr int BUTTON_WIDTH = 100;
    constexpr int EDITOR_HEIGHT = 32;
    constexpr int SPACING = 10;

    // Theme-driven colour accessors (resolved at construction time;
    // dialog is short-lived so live re-skin isn't needed).
    inline juce::Colour bgFn()
        { return waveedit::ThemeManager::getInstance().getCurrent().panel; }
    inline juce::Colour textFn()
        { return waveedit::ThemeManager::getInstance().getCurrent().text; }
    inline juce::Colour accentFn()
        { return waveedit::ThemeManager::getInstance().getCurrent().accent; }
    inline juce::Colour errorFn()
        { return waveedit::ThemeManager::getInstance().getCurrent().error; }
    inline juce::Colour successFn()
        { return waveedit::ThemeManager::getInstance().getCurrent().success; }
}

//==============================================================================
GoToPositionDialog::GoToPositionDialog(AudioUnits::TimeFormat currentFormat,
                                       double sampleRate,
                                       double fps,
                                       int64_t maxSamples)
    : m_timeFormat(currentFormat),
      m_sampleRate(sampleRate),
      m_fps(fps),
      m_maxSamples(maxSamples),
      m_cachedPosition(-1),
      m_isValid(false)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);

    // Title label
    m_titleLabel.setText("Go To Position", juce::dontSendNotification);
    m_titleLabel.setFont(waveedit::ui::dialogTitleFont());
    m_titleLabel.setColour(juce::Label::textColourId, textFn());
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Instruction label
    m_instructionLabel.setText("Enter position to jump to:", juce::dontSendNotification);
    m_instructionLabel.setFont(waveedit::ui::bodyFont());
    m_instructionLabel.setColour(juce::Label::textColourId, textFn());
    addAndMakeVisible(m_instructionLabel);

    // Format label ("Format:")
    m_formatLabel.setText("Format:", juce::dontSendNotification);
    m_formatLabel.setFont(waveedit::ui::bodyFont());
    m_formatLabel.setColour(juce::Label::textColourId, textFn());
    addAndMakeVisible(m_formatLabel);

    // Format selection ComboBox
    m_formatComboBox.addItem("Samples", static_cast<int>(AudioUnits::TimeFormat::Samples) + 1);
    m_formatComboBox.addItem("Milliseconds", static_cast<int>(AudioUnits::TimeFormat::Milliseconds) + 1);
    m_formatComboBox.addItem("Seconds", static_cast<int>(AudioUnits::TimeFormat::Seconds) + 1);
    m_formatComboBox.addItem("Frames", static_cast<int>(AudioUnits::TimeFormat::Frames) + 1);
    m_formatComboBox.setSelectedId(static_cast<int>(m_timeFormat) + 1, juce::dontSendNotification);
    m_formatComboBox.addListener(this);
    m_formatComboBox.setColour(juce::ComboBox::backgroundColourId,
                               waveedit::ThemeManager::getInstance().getCurrent().panelAlternate);
    m_formatComboBox.setColour(juce::ComboBox::textColourId, textFn());
    m_formatComboBox.setColour(juce::ComboBox::outlineColourId, accentFn());
    addAndMakeVisible(m_formatComboBox);

    // Example label
    m_exampleLabel.setText(getFormatExample(), juce::dontSendNotification);
    m_exampleLabel.setFont(waveedit::ui::smallFont());
    m_exampleLabel.setColour(juce::Label::textColourId,
                             waveedit::ThemeManager::getInstance().getCurrent().textMuted);
    addAndMakeVisible(m_exampleLabel);

    // Position text editor
    m_positionEditor.setMultiLine(false);
    m_positionEditor.setReturnKeyStartsNewLine(false);
    m_positionEditor.setScrollbarsShown(false);
    m_positionEditor.setCaretVisible(true);
    m_positionEditor.setPopupMenuEnabled(true);
    m_positionEditor.setFont(waveedit::ui::bodyFont());
    m_positionEditor.setColour(juce::TextEditor::backgroundColourId,
                               waveedit::ThemeManager::getInstance().getCurrent().panelAlternate);
    m_positionEditor.setColour(juce::TextEditor::textColourId, textFn());
    m_positionEditor.setColour(juce::TextEditor::outlineColourId, accentFn());
    m_positionEditor.setColour(juce::TextEditor::focusedOutlineColourId, accentFn().brighter());
    m_positionEditor.addListener(this);
    addAndMakeVisible(m_positionEditor);

    // Validation label (error/success messages)
    m_validationLabel.setText("", juce::dontSendNotification);
    m_validationLabel.setFont(waveedit::ui::smallFont());
    m_validationLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_validationLabel);

    // Go button
    m_goButton.setButtonText("Go");
    m_goButton.addListener(this);
    m_goButton.setEnabled(false);  // Disabled until valid input
    addAndMakeVisible(m_goButton);

    // Cancel button
    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.addListener(this);
    addAndMakeVisible(m_cancelButton);

    // Give focus to text editor
    m_positionEditor.grabKeyboardFocus();
}

GoToPositionDialog::~GoToPositionDialog()
{
    m_positionEditor.removeListener(this);
    m_formatComboBox.removeListener(this);
}

//==============================================================================
void GoToPositionDialog::paint(juce::Graphics& g)
{
    g.fillAll(bgFn());

    // Draw border
    g.setColour(accentFn());
    g.drawRect(getLocalBounds(), 2);
}

void GoToPositionDialog::resized()
{
    auto bounds = getLocalBounds().reduced(PADDING);

    // Title
    m_titleLabel.setBounds(bounds.removeFromTop(LABEL_HEIGHT + SPACING));
    bounds.removeFromTop(SPACING);

    // Instruction
    m_instructionLabel.setBounds(bounds.removeFromTop(LABEL_HEIGHT));
    bounds.removeFromTop(SPACING / 2);

    // Format label and ComboBox (side by side)
    auto formatRow = bounds.removeFromTop(EDITOR_HEIGHT);
    m_formatLabel.setBounds(formatRow.removeFromLeft(70));
    m_formatComboBox.setBounds(formatRow);
    bounds.removeFromTop(SPACING / 2);

    // Example label
    m_exampleLabel.setBounds(bounds.removeFromTop(LABEL_HEIGHT));
    bounds.removeFromTop(SPACING);

    // Position editor
    m_positionEditor.setBounds(bounds.removeFromTop(EDITOR_HEIGHT));
    bounds.removeFromTop(SPACING);

    // Validation label
    m_validationLabel.setBounds(bounds.removeFromTop(LABEL_HEIGHT));
    bounds.removeFromTop(SPACING * 2);

    // Buttons (right-aligned) - §6.8: Cancel left of primary action (Go rightmost)
    auto buttonRow = bounds.removeFromTop(BUTTON_HEIGHT);
    m_goButton.setBounds(buttonRow.removeFromRight(BUTTON_WIDTH));
    buttonRow.removeFromRight(SPACING);
    m_cancelButton.setBounds(buttonRow.removeFromRight(BUTTON_WIDTH));
}

//==============================================================================
void GoToPositionDialog::buttonClicked(juce::Button* button)
{
    if (button == &m_goButton)
    {
        confirmDialog();
    }
    else if (button == &m_cancelButton)
    {
        // Close dialog without action
        if (auto* modalHandler = juce::ModalComponentManager::getInstance()->getModalComponent(0))
        {
            modalHandler->exitModalState(0);
        }
    }
}

void GoToPositionDialog::textEditorReturnKeyPressed(juce::TextEditor&)
{
    if (m_isValid)
    {
        confirmDialog();
    }
}

void GoToPositionDialog::textEditorTextChanged(juce::TextEditor&)
{
    validateInput();
}

void GoToPositionDialog::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &m_formatComboBox)
    {
        // Update time format based on selected item
        int selectedId = m_formatComboBox.getSelectedId();
        if (selectedId > 0)
        {
            m_timeFormat = static_cast<AudioUnits::TimeFormat>(selectedId - 1);

            // Update example text
            m_exampleLabel.setText(getFormatExample(), juce::dontSendNotification);

            // Clear input and re-validate (format changed, old input may be invalid)
            m_positionEditor.clear();
            validateInput();

            // Give focus back to text editor
            m_positionEditor.grabKeyboardFocus();
        }
    }
}

//==============================================================================
void GoToPositionDialog::showDialog(juce::Component* parentComponent,
                                    AudioUnits::TimeFormat currentFormat,
                                    double sampleRate,
                                    double fps,
                                    int64_t maxSamples,
                                    std::function<void(int64_t)> callback)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto* dialog = new GoToPositionDialog(currentFormat, sampleRate, fps, maxSamples);
    dialog->m_callback = std::move(callback);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = "Go To Position";
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;
    options.useBottomRightCornerResizer = false;

    options.launchAsync();
}

int64_t GoToPositionDialog::getPositionInSamples() const
{
    return m_cachedPosition;
}

bool GoToPositionDialog::isPositionValid() const
{
    return m_isValid;
}

//==============================================================================
int64_t GoToPositionDialog::parseInput(const juce::String& input) const
{
    if (input.trim().isEmpty())
        return -1;

    juce::String trimmed = input.trim();

    try
    {
        switch (m_timeFormat)
        {
            case AudioUnits::TimeFormat::Samples:
            {
                // H17: Reject non-integer input ("1e5" silently becomes 1 via
                // getLargeIntValue). Accept only an optional leading minus
                // followed by digits. This also blocks "NaN"/"Inf"/float notation.
                const std::string stdStr = trimmed.toStdString();
                if (stdStr.empty())
                    return -1;
                const size_t start = (stdStr[0] == '-') ? 1 : 0;
                if (stdStr.size() <= start)
                    return -1;
                for (size_t k = start; k < stdStr.size(); ++k)
                    if (!std::isdigit(static_cast<unsigned char>(stdStr[k])))
                        return -1;
                int64_t samples = trimmed.getLargeIntValue();
                return (samples >= 0 && samples <= m_maxSamples) ? samples : -1;
            }

            case AudioUnits::TimeFormat::Milliseconds:
            {
                // H17: Guard against NaN/Inf from std parsing before any cast.
                double ms = trimmed.getDoubleValue();
                if (!std::isfinite(ms) || ms < 0.0)
                    return -1;
                int64_t samples = AudioUnits::millisecondsToSamples(ms, m_sampleRate);
                return (samples >= 0 && samples <= m_maxSamples) ? samples : -1;
            }

            case AudioUnits::TimeFormat::Seconds:
            {
                // H17: Guard against NaN/Inf from std parsing before any cast.
                double seconds = trimmed.getDoubleValue();
                if (!std::isfinite(seconds) || seconds < 0.0)
                    return -1;
                int64_t samples = AudioUnits::secondsToSamples(seconds, m_sampleRate);
                return (samples >= 0 && samples <= m_maxSamples) ? samples : -1;
            }

            case AudioUnits::TimeFormat::Frames:
            {
                // Parse as integer frame number
                int frame = trimmed.getIntValue();
                if (frame < 0)
                    return -1;
                int64_t samples = AudioUnits::framesToSamples(frame, m_fps, m_sampleRate);
                return (samples <= m_maxSamples) ? samples : -1;
            }

            default:
                return -1;
        }
    }
    catch (...)
    {
        return -1;
    }
}

void GoToPositionDialog::validateInput()
{
    juce::String input = m_positionEditor.getText();
    m_cachedPosition = parseInput(input);
    m_isValid = (m_cachedPosition >= 0);

    if (input.trim().isEmpty())
    {
        // No input yet - neutral state
        m_validationLabel.setText("", juce::dontSendNotification);
        m_validationLabel.setColour(juce::Label::textColourId, textFn());
        m_goButton.setEnabled(false);
    }
    else if (m_isValid)
    {
        // Valid input
        double timeInSeconds = AudioUnits::samplesToSeconds(m_cachedPosition, m_sampleRate);
        juce::String validMsg = juce::String::formatted("Valid position: %.3f seconds", timeInSeconds);
        m_validationLabel.setText(validMsg, juce::dontSendNotification);
        m_validationLabel.setColour(juce::Label::textColourId, successFn());
        m_goButton.setEnabled(true);
    }
    else
    {
        // Invalid input
        m_validationLabel.setText("Invalid format or out of range", juce::dontSendNotification);
        m_validationLabel.setColour(juce::Label::textColourId, errorFn());
        m_goButton.setEnabled(false);
    }

    repaint();
}

juce::String GoToPositionDialog::getFormatExample() const
{
    switch (m_timeFormat)
    {
        case AudioUnits::TimeFormat::Samples:
            return "Example: 44100";

        case AudioUnits::TimeFormat::Milliseconds:
            return "Example: 1000.0";

        case AudioUnits::TimeFormat::Seconds:
            return "Example: 1.5";

        case AudioUnits::TimeFormat::Frames:
            return juce::String::formatted("Example: 30 (at %.1f fps)", m_fps);

        default:
            return "Example: 0";
    }
}

void GoToPositionDialog::confirmDialog()
{
    if (m_isValid && m_callback)
    {
        m_callback(m_cachedPosition);
    }

    // Close dialog
    if (auto* modalHandler = juce::ModalComponentManager::getInstance()->getModalComponent(0))
    {
        modalHandler->exitModalState(m_isValid ? 1 : 0);
    }
}
