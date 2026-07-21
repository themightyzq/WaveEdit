/*
  ==============================================================================

    InsertSilenceDialog.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

/**
 * Insert Silence dialog (Generate menu). Collects a duration in seconds and
 * calls back with it. When a selection exists the controller ignores the
 * duration and silences the selection instead, so the dialog notes that.
 */
class InsertSilenceDialog : public juce::Component,
                            public juce::Button::Listener,
                            public juce::TextEditor::Listener
{
public:
    InsertSilenceDialog(double defaultSeconds, bool hasSelection);
    ~InsertSilenceDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorTextChanged(juce::TextEditor& editor) override;

    /** Shows the dialog. The callback receives the requested duration (s). */
    static void showDialog(juce::Component* parent, double defaultSeconds,
                           bool hasSelection, std::function<void(double)> callback);

private:
    void validateInput();
    void confirmDialog();

    bool m_hasSelection = false;
    double m_cachedSeconds = 0.0;
    bool m_isValid = false;
    std::function<void(double)> m_callback;

    juce::Label m_titleLabel;
    juce::Label m_durationLabel;
    juce::TextEditor m_durationEditor;
    juce::Label m_noteLabel;
    juce::Label m_validationLabel;
    juce::TextButton m_insertButton;
    juce::TextButton m_cancelButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InsertSilenceDialog)
};
