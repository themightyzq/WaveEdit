/*
  ==============================================================================

    BWFEditorDialog.h
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
#include "../Utils/BWFMetadata.h"

/**
 * BWF Metadata Editor dialog for WaveEdit.
 *
 * Provides editable form for all BWF (Broadcast Wave Format) metadata fields:
 * - Description: Free text description (max 256 chars)
 * - Originator: Organization name (max 32 chars)
 * - Originator Reference: Reference identifier (max 32 chars)
 * - Origination Date/Time: Timestamp with "Set Current" button
 * - Time Reference: Sample offset from midnight
 * - Coding History: Multi-line processing history
 *
 * Accessed via File menu or keyboard shortcut.
 * Changes are applied immediately to the Document's BWFMetadata,
 * and saved to file when user saves the document.
 */
class BWFEditorDialog : public juce::Component,
                        public juce::Button::Listener,
                        public juce::TextEditor::Listener
{
public:
    /**
     * Constructor.
     *
     * @param metadata Reference to the BWFMetadata to edit
     * @param onApply Callback invoked when user clicks Apply or OK
     */
    explicit BWFEditorDialog(BWFMetadata& metadata, std::function<void()> onApply = nullptr);
    ~BWFEditorDialog() override;

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // Button::Listener overrides

    void buttonClicked(juce::Button* button) override;

    //==============================================================================
    // TextEditor::Listener overrides

    void textEditorTextChanged(juce::TextEditor& editor) override;

    //==============================================================================
    // Static factory method

    /**
     * Shows the BWF editor dialog as a modal window.
     *
     * @param parentComponent Parent component to center the dialog over
     * @param metadata Reference to the BWFMetadata to edit
     * @param onApply Callback invoked when user applies changes
     */
    static void showDialog(juce::Component* parentComponent,
                          BWFMetadata& metadata,
                          std::function<void()> onApply = nullptr);

private:
    //==============================================================================
    // Helper methods

    /**
     * Loads current metadata values into UI controls.
     */
    void loadMetadata();

    /**
     * Saves UI control values back to metadata object.
     */
    void saveMetadata();

    /**
     * Sets the origination date/time to current time.
     */
    void setCurrentDateTime();

    /**
     * Validates and updates character count labels.
     */
    void updateCharacterCounts();

    //==============================================================================
    // UI Components

    // Description field (max 256 chars)
    juce::Label m_descriptionLabel;
    juce::TextEditor m_descriptionEditor;
    juce::Label m_descriptionCharCount;

    // Originator field (max 32 chars)
    juce::Label m_originatorLabel;
    juce::TextEditor m_originatorEditor;
    juce::Label m_originatorCharCount;

    // Originator Reference field (max 32 chars)
    juce::Label m_originatorRefLabel;
    juce::TextEditor m_originatorRefEditor;
    juce::Label m_originatorRefCharCount;

    // Origination Date (yyyy-mm-dd)
    juce::Label m_originationDateLabel;
    juce::TextEditor m_originationDateEditor;
    juce::Label m_dateFormatLabel;

    // Origination Time (hh:mm:ss)
    juce::Label m_originationTimeLabel;
    juce::TextEditor m_originationTimeEditor;
    juce::Label m_timeFormatLabel;

    // Set current date/time button
    juce::TextButton m_setCurrentButton;

    // Time Reference field
    juce::Label m_timeReferenceLabel;
    juce::TextEditor m_timeReferenceEditor;
    juce::Label m_timeReferenceHint;

    // Coding History field (multi-line)
    juce::Label m_codingHistoryLabel;
    juce::TextEditor m_codingHistoryEditor;
    juce::Label m_codingHistoryHint;

    // Action buttons
    juce::TextButton m_okButton;
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;

    //==============================================================================
    // Data members

    BWFMetadata& m_metadata;
    std::function<void()> m_onApply;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BWFEditorDialog)
};
