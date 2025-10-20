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

namespace
{
    // UI Constants
    constexpr int DIALOG_WIDTH = 400;
    constexpr int DIALOG_HEIGHT = 280;
    constexpr int PADDING = 20;
    constexpr int LABEL_HEIGHT = 24;
    constexpr int BUTTON_HEIGHT = 32;
    constexpr int BUTTON_WIDTH = 100;
    constexpr int EDITOR_HEIGHT = 32;
    constexpr int SPACING = 10;

    // Dark theme colors (matching SettingsPanel and FilePropertiesDialog)
    const juce::Colour BACKGROUND_COLOR = juce::Colour(0xff2a2a2a);
    const juce::Colour TEXT_COLOR = juce::Colour(0xffd0d0d0);
    const juce::Colour ACCENT_COLOR = juce::Colour(0xff4a9eff);
    const juce::Colour ERROR_COLOR = juce::Colour(0xffff5555);
    const juce::Colour SUCCESS_COLOR = juce::Colour(0xff55ff55);
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
    m_titleLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    m_titleLabel.setColour(juce::Label::textColourId, TEXT_COLOR);
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Instruction label
    m_instructionLabel.setText("Enter position to jump to:", juce::dontSendNotification);
    m_instructionLabel.setFont(juce::Font(14.0f));
    m_instructionLabel.setColour(juce::Label::textColourId, TEXT_COLOR);
    addAndMakeVisible(m_instructionLabel);

    // Format label ("Format:")
    m_formatLabel.setText("Format:", juce::dontSendNotification);
    m_formatLabel.setFont(juce::Font(14.0f));
    m_formatLabel.setColour(juce::Label::textColourId, TEXT_COLOR);
    addAndMakeVisible(m_formatLabel);

    // Format selection ComboBox
    m_formatComboBox.addItem("Samples", static_cast<int>(AudioUnits::TimeFormat::Samples) + 1);
    m_formatComboBox.addItem("Milliseconds", static_cast<int>(AudioUnits::TimeFormat::Milliseconds) + 1);
    m_formatComboBox.addItem("Seconds", static_cast<int>(AudioUnits::TimeFormat::Seconds) + 1);
    m_formatComboBox.addItem("Frames", static_cast<int>(AudioUnits::TimeFormat::Frames) + 1);
    m_formatComboBox.setSelectedId(static_cast<int>(m_timeFormat) + 1, juce::dontSendNotification);
    m_formatComboBox.addListener(this);
    m_formatComboBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1a1a));
    m_formatComboBox.setColour(juce::ComboBox::textColourId, TEXT_COLOR);
    m_formatComboBox.setColour(juce::ComboBox::outlineColourId, ACCENT_COLOR);
    addAndMakeVisible(m_formatComboBox);

    // Example label
    m_exampleLabel.setText(getFormatExample(), juce::dontSendNotification);
    m_exampleLabel.setFont(juce::Font(12.0f));
    m_exampleLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(m_exampleLabel);

    // Position text editor
    m_positionEditor.setMultiLine(false);
    m_positionEditor.setReturnKeyStartsNewLine(false);
    m_positionEditor.setScrollbarsShown(false);
    m_positionEditor.setCaretVisible(true);
    m_positionEditor.setPopupMenuEnabled(true);
    m_positionEditor.setFont(juce::Font(16.0f));
    m_positionEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1a1a));
    m_positionEditor.setColour(juce::TextEditor::textColourId, TEXT_COLOR);
    m_positionEditor.setColour(juce::TextEditor::outlineColourId, ACCENT_COLOR);
    m_positionEditor.setColour(juce::TextEditor::focusedOutlineColourId, ACCENT_COLOR.brighter());
    m_positionEditor.addListener(this);
    addAndMakeVisible(m_positionEditor);

    // Validation label (error/success messages)
    m_validationLabel.setText("", juce::dontSendNotification);
    m_validationLabel.setFont(juce::Font(12.0f, juce::Font::bold));
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
    g.fillAll(BACKGROUND_COLOR);

    // Draw border
    g.setColour(ACCENT_COLOR);
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

    // Buttons (right-aligned)
    auto buttonRow = bounds.removeFromTop(BUTTON_HEIGHT);
    m_cancelButton.setBounds(buttonRow.removeFromRight(BUTTON_WIDTH));
    buttonRow.removeFromRight(SPACING);
    m_goButton.setBounds(buttonRow.removeFromRight(BUTTON_WIDTH));
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
                // Parse as integer sample count
                int64_t samples = trimmed.getLargeIntValue();
                return (samples >= 0 && samples <= m_maxSamples) ? samples : -1;
            }

            case AudioUnits::TimeFormat::Milliseconds:
            {
                // Parse as decimal milliseconds
                double ms = trimmed.getDoubleValue();
                if (ms < 0.0)
                    return -1;
                int64_t samples = AudioUnits::millisecondsToSamples(ms, m_sampleRate);
                return (samples <= m_maxSamples) ? samples : -1;
            }

            case AudioUnits::TimeFormat::Seconds:
            {
                // Parse as decimal seconds
                double seconds = trimmed.getDoubleValue();
                if (seconds < 0.0)
                    return -1;
                int64_t samples = AudioUnits::secondsToSamples(seconds, m_sampleRate);
                return (samples <= m_maxSamples) ? samples : -1;
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
        m_validationLabel.setColour(juce::Label::textColourId, TEXT_COLOR);
        m_goButton.setEnabled(false);
    }
    else if (m_isValid)
    {
        // Valid input
        double timeInSeconds = AudioUnits::samplesToSeconds(m_cachedPosition, m_sampleRate);
        juce::String validMsg = juce::String::formatted("✓ Valid position: %.3f seconds", timeInSeconds);
        m_validationLabel.setText(validMsg, juce::dontSendNotification);
        m_validationLabel.setColour(juce::Label::textColourId, SUCCESS_COLOR);
        m_goButton.setEnabled(true);
    }
    else
    {
        // Invalid input
        m_validationLabel.setText("✗ Invalid format or out of range", juce::dontSendNotification);
        m_validationLabel.setColour(juce::Label::textColourId, ERROR_COLOR);
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
