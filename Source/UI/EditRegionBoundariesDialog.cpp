/*
  ==============================================================================

    EditRegionBoundariesDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "EditRegionBoundariesDialog.h"

namespace
{
    // UI Constants (matching GoToPositionDialog)
    constexpr int DIALOG_WIDTH = 400;
    constexpr int DIALOG_HEIGHT = 360;
    constexpr int PADDING = 20;
    constexpr int LABEL_HEIGHT = 24;
    constexpr int BUTTON_HEIGHT = 32;
    constexpr int BUTTON_WIDTH = 100;
    constexpr int EDITOR_HEIGHT = 32;
    constexpr int SPACING = 10;

    // Dark theme colors (matching GoToPositionDialog)
    const juce::Colour BACKGROUND_COLOR = juce::Colour(0xff2a2a2a);
    const juce::Colour TEXT_COLOR = juce::Colour(0xffd0d0d0);
    const juce::Colour ACCENT_COLOR = juce::Colour(0xff4a9eff);
    const juce::Colour ERROR_COLOR = juce::Colour(0xffff5555);
    const juce::Colour SUCCESS_COLOR = juce::Colour(0xff55ff55);
}

//==============================================================================
EditRegionBoundariesDialog::EditRegionBoundariesDialog(const Region& region,
                                                       AudioUnits::TimeFormat currentFormat,
                                                       double sampleRate,
                                                       double fps,
                                                       int64_t totalSamples)
    : m_sampleRate(sampleRate),
      m_fps(fps),
      m_totalSamples(totalSamples),
      m_timeFormat(currentFormat),
      m_originalStartSample(region.getStartSample()),
      m_originalEndSample(region.getEndSample()),
      m_regionName(region.getName()),
      m_cachedStartSample(-1),
      m_cachedEndSample(-1),
      m_isStartValid(false),
      m_isEndValid(false),
      m_areBothValid(false)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);

    // Title label
    m_titleLabel.setText("Edit Region Boundaries: " + m_regionName, juce::dontSendNotification);
    m_titleLabel.setFont(juce::Font(20.0f, juce::Font::bold));
    m_titleLabel.setColour(juce::Label::textColourId, TEXT_COLOR);
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Instruction label
    m_instructionLabel.setText("Enter new start and end positions:", juce::dontSendNotification);
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

    // Start position label
    m_startLabel.setText("Start:", juce::dontSendNotification);
    m_startLabel.setFont(juce::Font(14.0f));
    m_startLabel.setColour(juce::Label::textColourId, TEXT_COLOR);
    addAndMakeVisible(m_startLabel);

    // Start position text editor
    m_startEditor.setMultiLine(false);
    m_startEditor.setReturnKeyStartsNewLine(false);
    m_startEditor.setScrollbarsShown(false);
    m_startEditor.setCaretVisible(true);
    m_startEditor.setPopupMenuEnabled(true);
    m_startEditor.setFont(juce::Font(16.0f));
    m_startEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1a1a));
    m_startEditor.setColour(juce::TextEditor::textColourId, TEXT_COLOR);
    m_startEditor.setColour(juce::TextEditor::outlineColourId, ACCENT_COLOR);
    m_startEditor.setColour(juce::TextEditor::focusedOutlineColourId, ACCENT_COLOR.brighter());
    m_startEditor.setText(formatSamplePosition(m_originalStartSample));
    m_startEditor.addListener(this);
    addAndMakeVisible(m_startEditor);

    // End position label
    m_endLabel.setText("End:", juce::dontSendNotification);
    m_endLabel.setFont(juce::Font(14.0f));
    m_endLabel.setColour(juce::Label::textColourId, TEXT_COLOR);
    addAndMakeVisible(m_endLabel);

    // End position text editor
    m_endEditor.setMultiLine(false);
    m_endEditor.setReturnKeyStartsNewLine(false);
    m_endEditor.setScrollbarsShown(false);
    m_endEditor.setCaretVisible(true);
    m_endEditor.setPopupMenuEnabled(true);
    m_endEditor.setFont(juce::Font(16.0f));
    m_endEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1a1a));
    m_endEditor.setColour(juce::TextEditor::textColourId, TEXT_COLOR);
    m_endEditor.setColour(juce::TextEditor::outlineColourId, ACCENT_COLOR);
    m_endEditor.setColour(juce::TextEditor::focusedOutlineColourId, ACCENT_COLOR.brighter());
    m_endEditor.setText(formatSamplePosition(m_originalEndSample));
    m_endEditor.addListener(this);
    addAndMakeVisible(m_endEditor);

    // Validation label (error/success messages)
    m_validationLabel.setText("", juce::dontSendNotification);
    m_validationLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    m_validationLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_validationLabel);

    // OK button
    m_okButton.setButtonText("OK");
    m_okButton.addListener(this);
    m_okButton.setEnabled(false);  // Disabled until valid input
    addAndMakeVisible(m_okButton);

    // Cancel button
    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.addListener(this);
    addAndMakeVisible(m_cancelButton);

    // Give focus to start editor
    m_startEditor.grabKeyboardFocus();

    // Validate initial values
    validateInput();
}

EditRegionBoundariesDialog::~EditRegionBoundariesDialog()
{
    m_startEditor.removeListener(this);
    m_endEditor.removeListener(this);
    m_formatComboBox.removeListener(this);
}

//==============================================================================
void EditRegionBoundariesDialog::paint(juce::Graphics& g)
{
    g.fillAll(BACKGROUND_COLOR);

    // Draw border
    g.setColour(ACCENT_COLOR);
    g.drawRect(getLocalBounds(), 2);
}

void EditRegionBoundariesDialog::resized()
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

    // Start position (label + editor)
    auto startRow = bounds.removeFromTop(EDITOR_HEIGHT);
    m_startLabel.setBounds(startRow.removeFromLeft(70));
    m_startEditor.setBounds(startRow);
    bounds.removeFromTop(SPACING);

    // End position (label + editor)
    auto endRow = bounds.removeFromTop(EDITOR_HEIGHT);
    m_endLabel.setBounds(endRow.removeFromLeft(70));
    m_endEditor.setBounds(endRow);
    bounds.removeFromTop(SPACING);

    // Validation label
    m_validationLabel.setBounds(bounds.removeFromTop(LABEL_HEIGHT));
    bounds.removeFromTop(SPACING * 2);

    // Buttons (right-aligned)
    auto buttonRow = bounds.removeFromTop(BUTTON_HEIGHT);
    m_cancelButton.setBounds(buttonRow.removeFromRight(BUTTON_WIDTH));
    buttonRow.removeFromRight(SPACING);
    m_okButton.setBounds(buttonRow.removeFromRight(BUTTON_WIDTH));
}

//==============================================================================
void EditRegionBoundariesDialog::buttonClicked(juce::Button* button)
{
    if (button == &m_okButton)
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

void EditRegionBoundariesDialog::textEditorReturnKeyPressed(juce::TextEditor&)
{
    if (m_areBothValid)
    {
        confirmDialog();
    }
}

void EditRegionBoundariesDialog::textEditorTextChanged(juce::TextEditor&)
{
    validateInput();
}

void EditRegionBoundariesDialog::comboBoxChanged(juce::ComboBox* comboBox)
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

            // Update text editors to show current values in new format
            m_startEditor.setText(formatSamplePosition(m_originalStartSample), false);
            m_endEditor.setText(formatSamplePosition(m_originalEndSample), false);

            // Re-validate with new format
            validateInput();

            // Give focus back to start editor
            m_startEditor.grabKeyboardFocus();
        }
    }
}

//==============================================================================
void EditRegionBoundariesDialog::showDialog(juce::Component* parentComponent,
                                            const Region& region,
                                            AudioUnits::TimeFormat currentFormat,
                                            double sampleRate,
                                            double fps,
                                            int64_t totalSamples,
                                            std::function<void(int64_t, int64_t)> callback)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto* dialog = new EditRegionBoundariesDialog(region, currentFormat, sampleRate, fps, totalSamples);
    dialog->m_callback = std::move(callback);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = "Edit Region Boundaries";
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;
    options.useBottomRightCornerResizer = false;

    options.launchAsync();
}

int64_t EditRegionBoundariesDialog::getNewStartSample() const
{
    return m_cachedStartSample;
}

int64_t EditRegionBoundariesDialog::getNewEndSample() const
{
    return m_cachedEndSample;
}

bool EditRegionBoundariesDialog::areBoundariesValid() const
{
    return m_areBothValid;
}

//==============================================================================
int64_t EditRegionBoundariesDialog::parseInput(const juce::String& input) const
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
                return (samples >= 0 && samples <= m_totalSamples) ? samples : -1;
            }

            case AudioUnits::TimeFormat::Milliseconds:
            {
                // Parse as decimal milliseconds
                double ms = trimmed.getDoubleValue();
                if (ms < 0.0)
                    return -1;
                int64_t samples = AudioUnits::millisecondsToSamples(ms, m_sampleRate);
                return (samples <= m_totalSamples) ? samples : -1;
            }

            case AudioUnits::TimeFormat::Seconds:
            {
                // Parse as decimal seconds
                double seconds = trimmed.getDoubleValue();
                if (seconds < 0.0)
                    return -1;
                int64_t samples = AudioUnits::secondsToSamples(seconds, m_sampleRate);
                return (samples <= m_totalSamples) ? samples : -1;
            }

            case AudioUnits::TimeFormat::Frames:
            {
                // Parse as integer frame number
                int frame = trimmed.getIntValue();
                if (frame < 0)
                    return -1;
                int64_t samples = AudioUnits::framesToSamples(frame, m_fps, m_sampleRate);
                return (samples <= m_totalSamples) ? samples : -1;
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

void EditRegionBoundariesDialog::validateInput()
{
    // Parse both inputs
    juce::String startInput = m_startEditor.getText();
    juce::String endInput = m_endEditor.getText();

    m_cachedStartSample = parseInput(startInput);
    m_cachedEndSample = parseInput(endInput);

    m_isStartValid = (m_cachedStartSample >= 0);
    m_isEndValid = (m_cachedEndSample >= 0);

    // Both must be valid AND start < end
    m_areBothValid = m_isStartValid && m_isEndValid && (m_cachedStartSample < m_cachedEndSample);

    // Update validation message
    if (startInput.trim().isEmpty() && endInput.trim().isEmpty())
    {
        // No input yet - neutral state
        m_validationLabel.setText("", juce::dontSendNotification);
        m_validationLabel.setColour(juce::Label::textColourId, TEXT_COLOR);
        m_okButton.setEnabled(false);
    }
    else if (!m_isStartValid && !m_isEndValid)
    {
        m_validationLabel.setText("✗ Both start and end are invalid", juce::dontSendNotification);
        m_validationLabel.setColour(juce::Label::textColourId, ERROR_COLOR);
        m_okButton.setEnabled(false);
    }
    else if (!m_isStartValid)
    {
        m_validationLabel.setText("✗ Start position is invalid", juce::dontSendNotification);
        m_validationLabel.setColour(juce::Label::textColourId, ERROR_COLOR);
        m_okButton.setEnabled(false);
    }
    else if (!m_isEndValid)
    {
        m_validationLabel.setText("✗ End position is invalid", juce::dontSendNotification);
        m_validationLabel.setColour(juce::Label::textColourId, ERROR_COLOR);
        m_okButton.setEnabled(false);
    }
    else if (m_cachedStartSample >= m_cachedEndSample)
    {
        m_validationLabel.setText("✗ Start must be before end", juce::dontSendNotification);
        m_validationLabel.setColour(juce::Label::textColourId, ERROR_COLOR);
        m_okButton.setEnabled(false);
    }
    else if (m_areBothValid)
    {
        // Valid boundaries
        double startSeconds = AudioUnits::samplesToSeconds(m_cachedStartSample, m_sampleRate);
        double endSeconds = AudioUnits::samplesToSeconds(m_cachedEndSample, m_sampleRate);
        double durationSeconds = endSeconds - startSeconds;

        juce::String validMsg = juce::String::formatted(
            "✓ Valid region: %.3f - %.3f sec (%.3f sec duration)",
            startSeconds, endSeconds, durationSeconds);
        m_validationLabel.setText(validMsg, juce::dontSendNotification);
        m_validationLabel.setColour(juce::Label::textColourId, SUCCESS_COLOR);
        m_okButton.setEnabled(true);
    }

    repaint();
}

juce::String EditRegionBoundariesDialog::getFormatExample() const
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

void EditRegionBoundariesDialog::confirmDialog()
{
    if (m_areBothValid && m_callback)
    {
        juce::Logger::writeToLog(juce::String::formatted(
            "EditRegionBoundariesDialog: Confirming new boundaries: start=%lld, end=%lld",
            m_cachedStartSample, m_cachedEndSample));

        m_callback(m_cachedStartSample, m_cachedEndSample);
    }

    // Close dialog
    if (auto* modalHandler = juce::ModalComponentManager::getInstance()->getModalComponent(0))
    {
        modalHandler->exitModalState(m_areBothValid ? 1 : 0);
    }
}

juce::String EditRegionBoundariesDialog::formatSamplePosition(int64_t samples) const
{
    switch (m_timeFormat)
    {
        case AudioUnits::TimeFormat::Samples:
            return juce::String(samples);

        case AudioUnits::TimeFormat::Milliseconds:
        {
            double ms = AudioUnits::samplesToMilliseconds(samples, m_sampleRate);
            return juce::String(ms, 2);
        }

        case AudioUnits::TimeFormat::Seconds:
        {
            double seconds = AudioUnits::samplesToSeconds(samples, m_sampleRate);
            return juce::String(seconds, 3);
        }

        case AudioUnits::TimeFormat::Frames:
        {
            int frame = AudioUnits::samplesToFrames(samples, m_fps, m_sampleRate);
            return juce::String(frame);
        }

        default:
            return juce::String(samples);
    }
}
