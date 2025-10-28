/*
  ==============================================================================

    EditRegionBoundariesDialog.h
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
#include "../Utils/AudioUnits.h"
#include "../Utils/Region.h"

/**
 * Edit Region Boundaries dialog for WaveEdit.
 *
 * Allows users to precisely edit region start/end positions by entering:
 * - Sample numbers (e.g., "44100")
 * - Milliseconds (e.g., "1000")
 * - Seconds (e.g., "1.5")
 * - Frame numbers (e.g., "30")
 *
 * The dialog provides:
 * - Two separate fields for start and end positions
 * - Time format selection (Samples, Milliseconds, Seconds, Frames)
 * - Real-time validation (start < end, within file duration)
 * - Visual feedback for valid/invalid input
 *
 * Accessed via right-click context menu on a region → "Edit Boundaries..."
 */
class EditRegionBoundariesDialog : public juce::Component,
                                    public juce::Button::Listener,
                                    public juce::TextEditor::Listener,
                                    public juce::ComboBox::Listener
{
public:
    /**
     * Constructor.
     *
     * @param region Current region to edit
     * @param currentFormat Current time format setting
     * @param sampleRate Sample rate of the audio file in Hz
     * @param fps Frames per second for Frames format
     * @param totalSamples Total file duration in samples (for validation)
     */
    EditRegionBoundariesDialog(const Region& region,
                                AudioUnits::TimeFormat currentFormat,
                                double sampleRate,
                                double fps,
                                int64_t totalSamples);

    ~EditRegionBoundariesDialog() override;

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // Button::Listener overrides

    void buttonClicked(juce::Button* button) override;

    //==============================================================================
    // TextEditor::Listener overrides

    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorTextChanged(juce::TextEditor& editor) override;

    //==============================================================================
    // ComboBox::Listener overrides

    void comboBoxChanged(juce::ComboBox* comboBox) override;

    //==============================================================================
    // Static factory method

    /**
     * Shows the Edit Region Boundaries dialog as a modal window.
     *
     * @param parentComponent Parent component to center the dialog over
     * @param region Current region to edit
     * @param currentFormat Current time format setting
     * @param sampleRate Sample rate of the audio file in Hz
     * @param fps Frames per second for Frames format
     * @param totalSamples Total file duration in samples
     * @param callback Function to call with new boundaries (startSample, endSample)
     */
    static void showDialog(juce::Component* parentComponent,
                           const Region& region,
                           AudioUnits::TimeFormat currentFormat,
                           double sampleRate,
                           double fps,
                           int64_t totalSamples,
                           std::function<void(int64_t, int64_t)> callback);

    /**
     * Gets the new start position in samples (if valid).
     *
     * @return Start position in samples, or -1 if invalid
     */
    int64_t getNewStartSample() const;

    /**
     * Gets the new end position in samples (if valid).
     *
     * @return End position in samples, or -1 if invalid
     */
    int64_t getNewEndSample() const;

    /**
     * Checks if the entered boundaries are valid.
     *
     * @return true if valid (start < end, both within file duration), false otherwise
     */
    bool areBoundariesValid() const;

private:
    //==============================================================================
    // Helper methods

    /**
     * Parses user input and converts to sample position.
     *
     * @param input User input string
     * @return Position in samples, or -1 if invalid
     */
    int64_t parseInput(const juce::String& input) const;

    /**
     * Validates the current input and updates UI feedback.
     */
    void validateInput();

    /**
     * Gets format example string for user guidance.
     *
     * @return Example string (e.g., "Example: 44100" for Samples)
     */
    juce::String getFormatExample() const;

    /**
     * Confirms the dialog and calls the callback.
     */
    void confirmDialog();

    /**
     * Formats a sample position as a string in the current time format.
     *
     * @param samples Sample position
     * @return Formatted string (e.g., "1000.0" for 1000ms)
     */
    juce::String formatSamplePosition(int64_t samples) const;

    //==============================================================================
    // Member variables

    // Audio context
    double m_sampleRate;
    double m_fps;
    int64_t m_totalSamples;
    AudioUnits::TimeFormat m_timeFormat;

    // Original region boundaries
    int64_t m_originalStartSample;
    int64_t m_originalEndSample;
    juce::String m_regionName;

    // Callback
    std::function<void(int64_t, int64_t)> m_callback;

    // Cached parsed values
    int64_t m_cachedStartSample;
    int64_t m_cachedEndSample;
    bool m_isStartValid;
    bool m_isEndValid;
    bool m_areBothValid;  // true if start < end and both within range

    //==============================================================================
    // UI Components

    juce::Label m_titleLabel;
    juce::Label m_instructionLabel;

    // Format selection
    juce::Label m_formatLabel;
    juce::ComboBox m_formatComboBox;
    juce::Label m_exampleLabel;

    // Start position
    juce::Label m_startLabel;
    juce::TextEditor m_startEditor;

    // End position
    juce::Label m_endLabel;
    juce::TextEditor m_endEditor;

    // Validation feedback
    juce::Label m_validationLabel;

    // Buttons
    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EditRegionBoundariesDialog)
};
