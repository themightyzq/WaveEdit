#include "GainDialog.h"

GainDialog::GainDialog()
    : m_titleLabel("titleLabel", "Apply Gain"),
      m_gainLabel("gainLabel", "Gain (dB):"),
      m_applyButton("Apply"),
      m_cancelButton("Cancel")
{
    // Title label
    m_titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Gain label
    m_gainLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_gainLabel);

    // Gain input
    m_gainInput.setInputRestrictions(0, "-0123456789.");
    m_gainInput.setJustification(juce::Justification::centredLeft);
    m_gainInput.setText("0.0");
    m_gainInput.setSelectAllWhenFocused(true);
    m_gainInput.onReturnKey = [this]() { onApplyClicked(); };
    m_gainInput.onEscapeKey = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_gainInput);

    // Apply button
    m_applyButton.onClick = [this]() { onApplyClicked(); };
    addAndMakeVisible(m_applyButton);

    // Cancel button
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    // Set initial focus to text input
    m_gainInput.setWantsKeyboardFocus(true);

    setSize(300, 150);
}

std::optional<float> GainDialog::showDialog()
{
    // Show helpful message directing users to working shortcuts
    // Full interactive dialog with custom input will be implemented in Phase 2
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Gain Adjustment",
        "Quick Gain Shortcuts:\n\n"
        "  • Shift+Up: Increase gain by 1 dB\n"
        "  • Shift+Down: Decrease gain by 1 dB\n\n"
        "These shortcuts work on the entire file or selected region.\n\n"
        "Full gain dialog with custom values coming in Phase 2.",
        "OK"
    );

    return std::nullopt;
}

void GainDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));

    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawRect(getLocalBounds(), 1);
}

void GainDialog::resized()
{
    auto area = getLocalBounds().reduced(15);

    // Title
    m_titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10); // Spacing

    // Gain input row
    auto inputRow = area.removeFromTop(30);
    m_gainLabel.setBounds(inputRow.removeFromLeft(80));
    inputRow.removeFromLeft(10); // Spacing
    m_gainInput.setBounds(inputRow);

    area.removeFromTop(20); // Spacing before buttons

    // Buttons
    auto buttonRow = area.removeFromTop(30);
    auto buttonWidth = 80;
    auto buttonSpacing = 10;

    m_cancelButton.setBounds(buttonRow.removeFromRight(buttonWidth));
    buttonRow.removeFromRight(buttonSpacing);
    m_applyButton.setBounds(buttonRow.removeFromRight(buttonWidth));
}

std::optional<float> GainDialog::getValidatedGain() const
{
    auto text = m_gainInput.getText().trim();

    if (text.isEmpty())
    {
        return std::nullopt;
    }

    try
    {
        float value = std::stof(text.toStdString());
        return value;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

void GainDialog::onApplyClicked()
{
    auto gain = getValidatedGain();

    if (!gain.has_value())
    {
        // MVP: Simple validation feedback via logger
        // Proper async error dialog deferred to Phase 2
        juce::Logger::writeToLog("GainDialog: Invalid gain value entered");
        return;
    }

    m_result = gain;

    // Close the dialog
    if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
    {
        parent->exitModalState(1);
    }
}

void GainDialog::onCancelClicked()
{
    m_result = std::nullopt;

    // Close the dialog
    if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
    {
        parent->exitModalState(0);
    }
}
