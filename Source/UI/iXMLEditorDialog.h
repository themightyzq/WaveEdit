/*
  ==============================================================================

    iXMLEditorDialog.h
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
#include "../Utils/iXMLMetadata.h"
#include <functional>

/**
 * iXML Metadata Editor Dialog for WaveEdit.
 *
 * Provides a UI for editing all SoundMiner-compatible iXML metadata fields:
 * - Category / Subcategory (UCS fields)
 * - FXName (sound effect name)
 * - Description (long descriptive text)
 * - Keywords (comma-separated searchable terms)
 * - Designer (creator/recordist)
 * - Track Title (user-editable title)
 * - Project (source ID)
 * - Library (tape/manufacturer)
 *
 * Compatible with SoundMiner, Steinberg Nuendo/WaveLab, iZotope RX, BaseHead.
 */
class iXMLEditorDialog : public juce::Component,
                         public juce::Button::Listener,
                         public juce::TextEditor::Listener
{
public:
    /**
     * Constructor.
     *
     * @param metadata Reference to iXMLMetadata object to edit
     * @param filename Optional current filename for UCS parsing
     * @param onApply Optional callback invoked when Apply/OK is clicked
     */
    explicit iXMLEditorDialog(iXMLMetadata& metadata,
                              const juce::String& filename = juce::String(),
                              std::function<void()> onApply = nullptr);

    ~iXMLEditorDialog() override;

    //==========================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==========================================================================
    // Button::Listener overrides

    void buttonClicked(juce::Button* button) override;

    //==========================================================================
    // TextEditor::Listener overrides

    void textEditorTextChanged(juce::TextEditor& editor) override;

    //==========================================================================
    // Static factory method

    /**
     * Shows the iXML metadata editor dialog as a modal window.
     *
     * @param parentComponent Parent component to center dialog over
     * @param metadata Reference to iXMLMetadata object to edit
     * @param filename Optional current filename for UCS parsing/auto-populate
     * @param onApply Optional callback invoked when metadata is modified
     */
    static void showDialog(juce::Component* parentComponent,
                           iXMLMetadata& metadata,
                           const juce::String& filename = juce::String(),
                           std::function<void()> onApply = nullptr);

private:
    //==========================================================================
    // Helper methods

    /**
     * Loads metadata values into UI fields.
     */
    void loadMetadata();

    /**
     * Saves UI field values back to metadata object.
     */
    void saveMetadata();

    /**
     * Auto-populates FXName and Designer from current filename (if UCS-compliant).
     */
    void autoPopulateFromFilename();

    /**
     * Suggests UCS category/subcategory based on filename and current metadata.
     */
    void suggestCategory();

    /**
     * Updates character count labels for text editors.
     */
    void updateCharacterCounts();

    //==========================================================================
    // UI Components

    // Viewport for scrolling (dialog has many fields)
    juce::Viewport m_viewport;
    juce::Component m_contentComponent;

    // UCS Category fields
    juce::Label m_categoryLabel;
    juce::TextEditor m_categoryEditor;
    juce::Label m_categoryHint;

    juce::Label m_subcategoryLabel;
    juce::TextEditor m_subcategoryEditor;
    juce::Label m_subcategoryHint;

    // CategoryFull (read-only, computed)
    juce::Label m_categoryFullLabel;
    juce::Label m_categoryFullValue;

    // SoundMiner Extended fields
    juce::Label m_fxNameLabel;
    juce::TextEditor m_fxNameEditor;
    juce::Label m_fxNameCount;

    juce::Label m_descriptionLabel;
    juce::TextEditor m_descriptionEditor;
    juce::Label m_descriptionCount;

    juce::Label m_keywordsLabel;
    juce::TextEditor m_keywordsEditor;
    juce::Label m_keywordsHint;

    juce::Label m_designerLabel;
    juce::TextEditor m_designerEditor;

    // Standard iXML fields
    juce::Label m_trackTitleLabel;
    juce::TextEditor m_trackTitleEditor;

    juce::Label m_projectLabel;
    juce::TextEditor m_projectEditor;

    juce::Label m_tapeLabel;
    juce::TextEditor m_tapeEditor;

    // Buttons
    juce::TextButton m_autoPopulateButton;
    juce::TextButton m_suggestCategoryButton;
    juce::TextButton m_applyButton;
    juce::TextButton m_okButton;
    juce::TextButton m_cancelButton;

    // Data
    iXMLMetadata& m_metadata;
    juce::String m_filename;  // Current filename for UCS parsing
    std::function<void()> m_onApply;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(iXMLEditorDialog)
};
