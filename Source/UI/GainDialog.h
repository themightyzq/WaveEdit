#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <optional>

/**
 * Modal dialog for entering precise gain adjustment values.
 *
 * Allows user to enter any decimal gain value (positive or negative)
 * and apply it to the audio buffer. Used for precise gain control
 * beyond the keyboard shortcuts (Shift+Up/Down).
 */
class GainDialog : public juce::Component
{
public:
    GainDialog();
    ~GainDialog() override = default;

    /**
     * Show the dialog modally and return the user's gain input.
     *
     * @return std::optional<float> containing gain in dB if user clicked Apply,
     *         std::nullopt if user clicked Cancel or closed the dialog
     */
    static std::optional<float> showDialog();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    /**
     * Validates the text input and returns the gain value if valid.
     *
     * @return std::optional<float> containing validated gain value,
     *         std::nullopt if input is invalid
     */
    std::optional<float> getValidatedGain() const;

    void onApplyClicked();
    void onCancelClicked();

    juce::Label m_titleLabel;
    juce::Label m_gainLabel;
    juce::TextEditor m_gainInput;
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;

    std::optional<float> m_result;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GainDialog)
};
