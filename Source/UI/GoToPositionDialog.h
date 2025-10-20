/*
  ==============================================================================

    GoToPositionDialog.h
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

/**
 * Go To Position dialog for WaveEdit.
 *
 * Allows users to jump to an exact position in the audio file by entering:
 * - Sample numbers (e.g., "44100")
 * - Milliseconds (e.g., "1000")
 * - Seconds (e.g., "1.5")
 * - Timecode HH:MM:SS (e.g., "00:00:01.500")
 * - SMPTE timecode (e.g., "00:00:01:15")
 * - Frame numbers (e.g., "30")
 *
 * The dialog respects the current time format setting and provides
 * appropriate input validation.
 *
 * Accessed via Cmd+G keyboard shortcut.
 */
class GoToPositionDialog : public juce::Component,
                           public juce::Button::Listener,
                           public juce::TextEditor::Listener,
                           public juce::ComboBox::Listener
{
public:
    /**
     * Constructor.
     *
     * @param currentFormat Current time format setting
     * @param sampleRate Sample rate of the audio file in Hz
     * @param fps Frames per second for SMPTE/Frames format
     * @param maxSamples Maximum valid position in samples
     */
    GoToPositionDialog(AudioUnits::TimeFormat currentFormat,
                       double sampleRate,
                       double fps,
                       int64_t maxSamples);

    ~GoToPositionDialog() override;

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
     * Shows the Go To Position dialog as a modal window.
     *
     * @param parentComponent Parent component to center the dialog over
     * @param currentFormat Current time format setting
     * @param sampleRate Sample rate of the audio file in Hz
     * @param fps Frames per second for SMPTE/Frames format
     * @param maxSamples Maximum valid position in samples
     * @param callback Function to call with the requested position (in samples)
     */
    static void showDialog(juce::Component* parentComponent,
                           AudioUnits::TimeFormat currentFormat,
                           double sampleRate,
                           double fps,
                           int64_t maxSamples,
                           std::function<void(int64_t)> callback);

    /**
     * Gets the entered position in samples (if valid).
     *
     * @return Position in samples, or -1 if invalid
     */
    int64_t getPositionInSamples() const;

    /**
     * Checks if the entered position is valid.
     *
     * @return true if valid, false otherwise
     */
    bool isPositionValid() const;

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

    //==============================================================================
    // Member variables

    AudioUnits::TimeFormat m_timeFormat;
    double m_sampleRate;
    double m_fps;
    int64_t m_maxSamples;
    std::function<void(int64_t)> m_callback;
    int64_t m_cachedPosition;  // Cached parsed position
    bool m_isValid;             // Cached validity state

    //==============================================================================
    // UI Components

    juce::Label m_titleLabel;
    juce::Label m_instructionLabel;
    juce::Label m_formatLabel;      // "Format:" label
    juce::ComboBox m_formatComboBox; // Format selection dropdown
    juce::Label m_exampleLabel;
    juce::TextEditor m_positionEditor;
    juce::Label m_validationLabel;  // Shows error messages
    juce::TextButton m_goButton;
    juce::TextButton m_cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GoToPositionDialog)
};
