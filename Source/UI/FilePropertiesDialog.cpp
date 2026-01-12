/*
  ==============================================================================

    FilePropertiesDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "FilePropertiesDialog.h"
#include "BWFEditorDialog.h"
#include "iXMLEditorDialog.h"
#include "../Audio/ChannelLayout.h"

// Dialog dimensions
namespace
{
    constexpr int DIALOG_WIDTH = 700;   // Increased width for better readability
    constexpr int DIALOG_HEIGHT = 700;  // Fixed height with scrolling viewport
    constexpr int ROW_HEIGHT = 30;
    constexpr int ROW_HEIGHT_MULTILINE = 60;  // For multi-line fields
    constexpr int LABEL_WIDTH = 180;    // Increased for longer labels
    constexpr int SPACING = 10;
    constexpr int BUTTON_HEIGHT = 30;
    constexpr int BUTTON_WIDTH = 100;
    constexpr int EDIT_BUTTON_WIDTH = 80;
    constexpr int EDIT_BUTTON_HEIGHT = 25;
}

//==============================================================================
FilePropertiesDialog::FilePropertiesDialog(Document& document)
    : m_document(document)
{
    // Setup property name labels (left column)
    auto setupLabel = [this](juce::Label& label, const juce::String& text)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        label.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(label);
    };

    setupLabel(m_filenameLabel, "Filename:");
    setupLabel(m_filePathLabel, "File Path:");
    setupLabel(m_fileSizeLabel, "File Size:");
    setupLabel(m_dateCreatedLabel, "Date Created:");
    setupLabel(m_dateModifiedLabel, "Date Modified:");
    setupLabel(m_sampleRateLabel, "Sample Rate:");
    setupLabel(m_bitDepthLabel, "Bit Depth:");
    setupLabel(m_channelsLabel, "Channels:");
    setupLabel(m_durationLabel, "Duration:");
    setupLabel(m_codecLabel, "Format:");
    setupLabel(m_bwfDescriptionLabel, "BWF Description:");
    setupLabel(m_bwfOriginatorLabel, "BWF Originator:");
    setupLabel(m_bwfOriginationDateLabel, "BWF Date:");
    setupLabel(m_ixmlCategoryLabel, "Category:");
    setupLabel(m_ixmlSubcategoryLabel, "Subcategory:");
    setupLabel(m_ixmlCategoryFullLabel, "CategoryFull:");
    setupLabel(m_ixmlFXNameLabel, "FX Name:");
    setupLabel(m_ixmlTrackTitleLabel, "Track Title:");
    setupLabel(m_ixmlDescriptionLabel, "Description:");
    setupLabel(m_ixmlKeywordsLabel, "Keywords:");
    setupLabel(m_ixmlDesignerLabel, "Designer:");
    setupLabel(m_ixmlProjectLabel, "Project:");
    setupLabel(m_ixmlTapeLabel, "Library:");

    // Setup value labels (right column)
    auto setupValueLabel = [this](juce::Label& label)
    {
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        label.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(label);
    };

    setupValueLabel(m_filenameValue);
    setupValueLabel(m_filePathValue);
    setupValueLabel(m_fileSizeValue);
    setupValueLabel(m_dateCreatedValue);
    setupValueLabel(m_dateModifiedValue);
    setupValueLabel(m_sampleRateValue);
    setupValueLabel(m_bitDepthValue);
    setupValueLabel(m_channelsValue);
    setupValueLabel(m_durationValue);
    setupValueLabel(m_codecValue);
    setupValueLabel(m_bwfDescriptionValue);
    setupValueLabel(m_bwfOriginatorValue);
    setupValueLabel(m_bwfOriginationDateValue);
    setupValueLabel(m_ixmlCategoryValue);
    setupValueLabel(m_ixmlSubcategoryValue);
    setupValueLabel(m_ixmlCategoryFullValue);
    setupValueLabel(m_ixmlFXNameValue);
    setupValueLabel(m_ixmlTrackTitleValue);
    setupValueLabel(m_ixmlDescriptionValue);
    setupValueLabel(m_ixmlKeywordsValue);
    setupValueLabel(m_ixmlDesignerValue);
    setupValueLabel(m_ixmlProjectValue);
    setupValueLabel(m_ixmlTapeValue);

    // Make file path value label support text selection for copying
    m_filePathValue.setEditable(false, false, true);

    // Enable multi-line display for long fields
    m_ixmlFXNameValue.setMinimumHorizontalScale(1.0f);  // Allow word wrap
    m_ixmlDescriptionValue.setMinimumHorizontalScale(1.0f);
    m_ixmlKeywordsValue.setMinimumHorizontalScale(1.0f);

    // Setup viewport for scrolling
    m_viewport.setViewedComponent(&m_contentComponent, false);
    m_viewport.setScrollBarsShown(true, false);  // Vertical scrollbar only
    addAndMakeVisible(m_viewport);

    // Add all labels to content component (not directly to dialog)
    // This allows scrolling while keeping Close button fixed
    auto addToContent = [this](juce::Component& comp) {
        m_contentComponent.addAndMakeVisible(&comp);
    };

    // Add all labels to scrollable content
    addToContent(m_filenameLabel); addToContent(m_filenameValue);
    addToContent(m_filePathLabel); addToContent(m_filePathValue);
    addToContent(m_fileSizeLabel); addToContent(m_fileSizeValue);
    addToContent(m_dateCreatedLabel); addToContent(m_dateCreatedValue);
    addToContent(m_dateModifiedLabel); addToContent(m_dateModifiedValue);
    addToContent(m_sampleRateLabel); addToContent(m_sampleRateValue);
    addToContent(m_bitDepthLabel); addToContent(m_bitDepthValue);
    addToContent(m_channelsLabel); addToContent(m_channelsValue);
    addToContent(m_durationLabel); addToContent(m_durationValue);
    addToContent(m_codecLabel); addToContent(m_codecValue);
    addToContent(m_bwfDescriptionLabel); addToContent(m_bwfDescriptionValue);
    addToContent(m_bwfOriginatorLabel); addToContent(m_bwfOriginatorValue);
    addToContent(m_bwfOriginationDateLabel); addToContent(m_bwfOriginationDateValue);
    addToContent(m_ixmlCategoryLabel); addToContent(m_ixmlCategoryValue);
    addToContent(m_ixmlSubcategoryLabel); addToContent(m_ixmlSubcategoryValue);
    addToContent(m_ixmlCategoryFullLabel); addToContent(m_ixmlCategoryFullValue);
    addToContent(m_ixmlFXNameLabel); addToContent(m_ixmlFXNameValue);
    addToContent(m_ixmlTrackTitleLabel); addToContent(m_ixmlTrackTitleValue);
    addToContent(m_ixmlDescriptionLabel); addToContent(m_ixmlDescriptionValue);
    addToContent(m_ixmlKeywordsLabel); addToContent(m_ixmlKeywordsValue);
    addToContent(m_ixmlDesignerLabel); addToContent(m_ixmlDesignerValue);
    addToContent(m_ixmlProjectLabel); addToContent(m_ixmlProjectValue);
    addToContent(m_ixmlTapeLabel); addToContent(m_ixmlTapeValue);

    // Setup Edit buttons (fixed at section headers, not in viewport)
    m_editBWFButton.setButtonText("Edit...");
    m_editBWFButton.addListener(this);
    m_contentComponent.addAndMakeVisible(&m_editBWFButton);

    m_editiXMLButton.setButtonText("Edit...");
    m_editiXMLButton.addListener(this);
    m_contentComponent.addAndMakeVisible(&m_editiXMLButton);

    // Setup Close button (fixed at bottom, not in viewport)
    m_closeButton.setButtonText("Close");
    m_closeButton.addListener(this);
    addAndMakeVisible(m_closeButton);

    // Load properties from document
    loadProperties();

    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);
}

FilePropertiesDialog::~FilePropertiesDialog()
{
}

//==============================================================================
void FilePropertiesDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));
    // Note: Section headers and separators are now drawn on m_contentComponent
}

void FilePropertiesDialog::resized()
{
    auto bounds = getLocalBounds().reduced(SPACING);

    // Reserve space for Close button at bottom (outside viewport)
    auto buttonArea = bounds.removeFromBottom(BUTTON_HEIGHT + SPACING);
    m_closeButton.setBounds(buttonArea.withSizeKeepingCentre(BUTTON_WIDTH, BUTTON_HEIGHT));

    // Viewport takes remaining space
    m_viewport.setBounds(bounds);

    // Layout content component with all fields
    int contentWidth = bounds.getWidth() - m_viewport.getScrollBarThickness();
    int yPos = 0;

    auto layoutRow = [&](juce::Label& nameLabel, juce::Label& valueLabel, int rowHeight = ROW_HEIGHT)
    {
        nameLabel.setBounds(SPACING, yPos, LABEL_WIDTH, rowHeight);
        valueLabel.setBounds(LABEL_WIDTH + 2 * SPACING, yPos,
                            contentWidth - LABEL_WIDTH - 3 * SPACING, rowHeight);
        yPos += rowHeight;
    };

    // File Information section
    yPos += ROW_HEIGHT + SPACING;  // Section header space
    layoutRow(m_filenameLabel, m_filenameValue);
    layoutRow(m_filePathLabel, m_filePathValue);
    layoutRow(m_fileSizeLabel, m_fileSizeValue);
    layoutRow(m_dateCreatedLabel, m_dateCreatedValue);
    layoutRow(m_dateModifiedLabel, m_dateModifiedValue);
    yPos += SPACING;  // Section separator

    // Audio Information section
    yPos += ROW_HEIGHT + SPACING;  // Section header space
    layoutRow(m_sampleRateLabel, m_sampleRateValue);
    layoutRow(m_bitDepthLabel, m_bitDepthValue);
    layoutRow(m_channelsLabel, m_channelsValue);
    layoutRow(m_durationLabel, m_durationValue);
    layoutRow(m_codecLabel, m_codecValue);
    yPos += SPACING;  // Section separator

    // BWF Metadata section
    yPos += ROW_HEIGHT + SPACING;  // Section header space
    // Position Edit button for BWF section
    m_editBWFButton.setBounds(contentWidth - EDIT_BUTTON_WIDTH - SPACING, yPos - ROW_HEIGHT - SPACING / 2,
                              EDIT_BUTTON_WIDTH, EDIT_BUTTON_HEIGHT);
    layoutRow(m_bwfDescriptionLabel, m_bwfDescriptionValue);
    layoutRow(m_bwfOriginatorLabel, m_bwfOriginatorValue);
    layoutRow(m_bwfOriginationDateLabel, m_bwfOriginationDateValue);
    yPos += SPACING;  // Section separator

    // SoundMiner / iXML Metadata section
    yPos += ROW_HEIGHT + SPACING;  // Section header space
    // Position Edit button for iXML section
    m_editiXMLButton.setBounds(contentWidth - EDIT_BUTTON_WIDTH - SPACING, yPos - ROW_HEIGHT - SPACING / 2,
                               EDIT_BUTTON_WIDTH, EDIT_BUTTON_HEIGHT);
    layoutRow(m_ixmlCategoryLabel, m_ixmlCategoryValue);
    layoutRow(m_ixmlSubcategoryLabel, m_ixmlSubcategoryValue);
    layoutRow(m_ixmlCategoryFullLabel, m_ixmlCategoryFullValue);
    layoutRow(m_ixmlFXNameLabel, m_ixmlFXNameValue, ROW_HEIGHT_MULTILINE);  // Multi-line
    layoutRow(m_ixmlTrackTitleLabel, m_ixmlTrackTitleValue);
    layoutRow(m_ixmlDescriptionLabel, m_ixmlDescriptionValue, ROW_HEIGHT_MULTILINE);  // Multi-line
    layoutRow(m_ixmlKeywordsLabel, m_ixmlKeywordsValue, ROW_HEIGHT_MULTILINE);  // Multi-line
    layoutRow(m_ixmlDesignerLabel, m_ixmlDesignerValue);
    layoutRow(m_ixmlProjectLabel, m_ixmlProjectValue);
    layoutRow(m_ixmlTapeLabel, m_ixmlTapeValue);
    yPos += SPACING;

    // Set content component size to total height needed
    m_contentComponent.setSize(contentWidth, yPos);
}

//==============================================================================
void FilePropertiesDialog::buttonClicked(juce::Button* button)
{
    if (button == &m_editBWFButton)
    {
        // Open BWF Editor Dialog
        BWFEditorDialog::showDialog(this, m_document.getBWFMetadata(), [this]()
        {
            // Mark document as modified when BWF metadata changes
            m_document.setModified(true);
            // Reload properties to show updated values
            loadProperties();
            juce::Logger::writeToLog("BWF metadata updated from File Properties dialog");
        });
    }
    else if (button == &m_editiXMLButton)
    {
        // Open iXML Editor Dialog with filename for UCS automation
        iXMLEditorDialog::showDialog(this, m_document.getiXMLMetadata(),
                                     m_document.getFilename(), [this]()
        {
            // Mark document as modified when iXML metadata changes
            m_document.setModified(true);
            // Reload properties to show updated values
            loadProperties();
            juce::Logger::writeToLog("iXML metadata updated from File Properties dialog");
        });
    }
    else if (button == &m_closeButton)
    {
        // Close dialog
        if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
        {
            dialogWindow->exitModalState(0);
        }
    }
}

//==============================================================================
void FilePropertiesDialog::loadProperties()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    const juce::File& file = m_document.getFile();

    // Check if audio engine has a file loaded
    if (!m_document.getAudioEngine().isFileLoaded())
    {
        m_filenameValue.setText("(No file loaded)", juce::dontSendNotification);
        m_filePathValue.setText("", juce::dontSendNotification);
        m_fileSizeValue.setText("N/A", juce::dontSendNotification);
        m_dateCreatedValue.setText("N/A", juce::dontSendNotification);
        m_dateModifiedValue.setText("N/A", juce::dontSendNotification);
        m_sampleRateValue.setText("N/A", juce::dontSendNotification);
        m_bitDepthValue.setText("N/A", juce::dontSendNotification);
        m_channelsValue.setText("N/A", juce::dontSendNotification);
        m_durationValue.setText("N/A", juce::dontSendNotification);
        m_codecValue.setText("N/A", juce::dontSendNotification);
        m_bwfDescriptionValue.setText("N/A", juce::dontSendNotification);
        m_bwfOriginatorValue.setText("N/A", juce::dontSendNotification);
        m_bwfOriginationDateValue.setText("N/A", juce::dontSendNotification);
        m_ixmlCategoryValue.setText("N/A", juce::dontSendNotification);
        m_ixmlSubcategoryValue.setText("N/A", juce::dontSendNotification);
        m_ixmlCategoryFullValue.setText("N/A", juce::dontSendNotification);
        m_ixmlFXNameValue.setText("N/A", juce::dontSendNotification);
        m_ixmlTrackTitleValue.setText("N/A", juce::dontSendNotification);
        m_ixmlDescriptionValue.setText("N/A", juce::dontSendNotification);
        m_ixmlKeywordsValue.setText("N/A", juce::dontSendNotification);
        m_ixmlDesignerValue.setText("N/A", juce::dontSendNotification);
        m_ixmlProjectValue.setText("N/A", juce::dontSendNotification);
        m_ixmlTapeValue.setText("N/A", juce::dontSendNotification);
        return;
    }

    // File information
    m_filenameValue.setText(m_document.getFilename(), juce::dontSendNotification);
    m_filePathValue.setText(file.getFullPathName(), juce::dontSendNotification);

    // Check if file exists before accessing file properties
    if (file.existsAsFile())
    {
        m_fileSizeValue.setText(formatFileSize(file.getSize()), juce::dontSendNotification);
        m_dateCreatedValue.setText(formatDateTime(file.getCreationTime()), juce::dontSendNotification);
        m_dateModifiedValue.setText(formatDateTime(file.getLastModificationTime()), juce::dontSendNotification);
    }
    else
    {
        m_fileSizeValue.setText("File not found", juce::dontSendNotification);
        m_dateCreatedValue.setText("N/A", juce::dontSendNotification);
        m_dateModifiedValue.setText("N/A", juce::dontSendNotification);
    }

    // Audio information
    const AudioEngine& audioEngine = m_document.getAudioEngine();
    const AudioBufferManager& bufferManager = m_document.getBufferManager();

    double sampleRate = audioEngine.getSampleRate();
    int bitDepth = audioEngine.getBitDepth();
    int numChannels = audioEngine.getNumChannels();
    int64_t numSamples = bufferManager.getNumSamples();

    m_sampleRateValue.setText(juce::String(sampleRate, 1) + " Hz", juce::dontSendNotification);
    m_bitDepthValue.setText(juce::String(bitDepth) + " bit", juce::dontSendNotification);

    // Format channels with proper layout name
    waveedit::ChannelLayout layout = waveedit::ChannelLayout::fromChannelCount(numChannels);
    juce::String channelsStr = juce::String(numChannels) + " (" + layout.getLayoutName() + ")";
    m_channelsValue.setText(channelsStr, juce::dontSendNotification);

    // Calculate and format duration
    double durationSeconds = (sampleRate > 0) ? numSamples / sampleRate : 0.0;
    m_durationValue.setText(formatDuration(durationSeconds), juce::dontSendNotification);

    // Determine codec
    m_codecValue.setText(determineCodec(file, bitDepth), juce::dontSendNotification);

    // BWF Metadata
    const BWFMetadata& bwf = m_document.getBWFMetadata();
    if (bwf.hasMetadata())
    {
        m_bwfDescriptionValue.setText(bwf.getDescription(), juce::dontSendNotification);
        m_bwfOriginatorValue.setText(bwf.getOriginator(), juce::dontSendNotification);

        // Format BWF origination date/time
        juce::String dateTimeStr = bwf.getOriginationDate() + " " + bwf.getOriginationTime();
        if (dateTimeStr.trim().isEmpty())
            dateTimeStr = "(Not set)";
        m_bwfOriginationDateValue.setText(dateTimeStr, juce::dontSendNotification);
    }
    else
    {
        m_bwfDescriptionValue.setText("(No BWF metadata)", juce::dontSendNotification);
        m_bwfOriginatorValue.setText("(Not set)", juce::dontSendNotification);
        m_bwfOriginationDateValue.setText("(Not set)", juce::dontSendNotification);
    }

    // SoundMiner / iXML Metadata
    const iXMLMetadata& ixml = m_document.getiXMLMetadata();
    if (ixml.hasMetadata())
    {
        // Display all SoundMiner fields with "(Not set)" for empty values
        juce::String category = ixml.getCategory();
        m_ixmlCategoryValue.setText(category.isEmpty() ? "(Not set)" : category, juce::dontSendNotification);

        juce::String subcategory = ixml.getSubcategory();
        m_ixmlSubcategoryValue.setText(subcategory.isEmpty() ? "(Not set)" : subcategory, juce::dontSendNotification);

        // CategoryFull (computed)
        juce::String categoryFull = ixml.getCategoryFull();
        m_ixmlCategoryFullValue.setText(categoryFull.isEmpty() ? "(Not set)" : categoryFull, juce::dontSendNotification);

        // FXName (can be long, multi-line)
        juce::String fxName = ixml.getFXName();
        m_ixmlFXNameValue.setText(fxName.isEmpty() ? "(Not set)" : fxName, juce::dontSendNotification);

        juce::String trackTitle = ixml.getTrackTitle();
        m_ixmlTrackTitleValue.setText(trackTitle.isEmpty() ? "(Not set)" : trackTitle, juce::dontSendNotification);

        // Description (long multi-line text)
        juce::String description = ixml.getDescription();
        m_ixmlDescriptionValue.setText(description.isEmpty() ? "(Not set)" : description, juce::dontSendNotification);

        // Keywords (comma-separated, multi-line)
        juce::String keywords = ixml.getKeywords();
        m_ixmlKeywordsValue.setText(keywords.isEmpty() ? "(Not set)" : keywords, juce::dontSendNotification);

        // Designer (short text)
        juce::String designer = ixml.getDesigner();
        m_ixmlDesignerValue.setText(designer.isEmpty() ? "(Not set)" : designer, juce::dontSendNotification);

        juce::String project = ixml.getProject();
        m_ixmlProjectValue.setText(project.isEmpty() ? "(Not set)" : project, juce::dontSendNotification);

        juce::String tape = ixml.getTape();
        m_ixmlTapeValue.setText(tape.isEmpty() ? "(Not set)" : tape, juce::dontSendNotification);
    }
    else
    {
        m_ixmlCategoryValue.setText("(No iXML metadata)", juce::dontSendNotification);
        m_ixmlSubcategoryValue.setText("(Not set)", juce::dontSendNotification);
        m_ixmlCategoryFullValue.setText("(Not set)", juce::dontSendNotification);
        m_ixmlFXNameValue.setText("(Not set)", juce::dontSendNotification);
        m_ixmlTrackTitleValue.setText("(Not set)", juce::dontSendNotification);
        m_ixmlDescriptionValue.setText("(Not set)", juce::dontSendNotification);
        m_ixmlKeywordsValue.setText("(Not set)", juce::dontSendNotification);
        m_ixmlDesignerValue.setText("(Not set)", juce::dontSendNotification);
        m_ixmlProjectValue.setText("(Not set)", juce::dontSendNotification);
        m_ixmlTapeValue.setText("(Not set)", juce::dontSendNotification);
    }
}

juce::String FilePropertiesDialog::formatDuration(double durationSeconds)
{
    int hours = static_cast<int>(durationSeconds / 3600.0);
    int minutes = static_cast<int>((durationSeconds - hours * 3600) / 60.0);
    int seconds = static_cast<int>(durationSeconds) % 60;
    int milliseconds = static_cast<int>((durationSeconds - static_cast<int>(durationSeconds)) * 1000);

    return juce::String::formatted("%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
}

juce::String FilePropertiesDialog::formatFileSize(int64_t bytes)
{
    if (bytes < 1024)
        return juce::String(bytes) + " bytes";
    else if (bytes < 1024 * 1024)
        return juce::String(bytes / 1024.0, 2) + " KB";
    else if (bytes < 1024 * 1024 * 1024)
        return juce::String(bytes / (1024.0 * 1024.0), 2) + " MB";
    else
        return juce::String(bytes / (1024.0 * 1024.0 * 1024.0), 2) + " GB";
}

juce::String FilePropertiesDialog::formatDateTime(const juce::Time& time)
{
    return time.formatted("%Y-%m-%d %H:%M:%S");
}

juce::String FilePropertiesDialog::determineCodec(const juce::File& file, int bitDepth)
{
    juce::String extension = file.getFileExtension().toLowerCase();

    // WAV files can be PCM or IEEE float
    if (extension == ".wav")
    {
        if (bitDepth == 32)
            return "WAV (IEEE Float 32-bit)";
        else if (bitDepth == 24)
            return "WAV (PCM 24-bit)";
        else if (bitDepth == 16)
            return "WAV (PCM 16-bit)";
        else if (bitDepth == 8)
            return "WAV (PCM 8-bit)";
        else
            return "WAV (PCM)";
    }
    else if (extension == ".aiff" || extension == ".aif")
    {
        return "AIFF (PCM " + juce::String(bitDepth) + "-bit)";
    }
    else if (extension == ".flac")
    {
        return "FLAC (Lossless " + juce::String(bitDepth) + "-bit)";
    }
    else if (extension == ".mp3")
    {
        return "MP3 (Lossy)";
    }
    else if (extension == ".ogg")
    {
        return "Ogg Vorbis (Lossy)";
    }
    else
    {
        return "Unknown Format";
    }
}

//==============================================================================
void FilePropertiesDialog::showDialog(juce::Component* parentComponent, Document& document)
{
    auto* propertiesDialog = new FilePropertiesDialog(document);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(propertiesDialog);
    options.dialogTitle = "File Properties";
    options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.useBottomRightCornerResizer = false;

    // Center over parent component
    if (parentComponent != nullptr)
    {
        auto parentBounds = parentComponent->getScreenBounds();
        auto dialogBounds = juce::Rectangle<int>(0, 0, DIALOG_WIDTH, DIALOG_HEIGHT);
        dialogBounds.setCentre(parentBounds.getCentre());
        options.content->setBounds(dialogBounds);
    }

    // Launch dialog
    options.launchAsync();
}
