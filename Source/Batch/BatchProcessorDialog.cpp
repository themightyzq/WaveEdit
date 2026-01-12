/*
  ==============================================================================

    BatchProcessorDialog.cpp
    Created: 2025
    Author:  ZQ SFX

  ==============================================================================
*/

#include "BatchProcessorDialog.h"

namespace waveedit
{

// =============================================================================
// DSPOperationComponent
// =============================================================================

DSPOperationComponent::DSPOperationComponent(int index)
    : m_index(index)
    , m_enabledToggle("Enabled")
    , m_paramLabel("paramLabel", "Value:")
    , m_removeButton("X")
{
    // Enabled toggle
    m_enabledToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(m_enabledToggle);

    // Operation combo
    m_operationCombo.addItem("Gain", static_cast<int>(BatchDSPOperation::GAIN) + 1);
    m_operationCombo.addItem("Normalize", static_cast<int>(BatchDSPOperation::NORMALIZE) + 1);
    m_operationCombo.addItem("DC Offset", static_cast<int>(BatchDSPOperation::DC_OFFSET) + 1);
    m_operationCombo.addItem("Fade In", static_cast<int>(BatchDSPOperation::FADE_IN) + 1);
    m_operationCombo.addItem("Fade Out", static_cast<int>(BatchDSPOperation::FADE_OUT) + 1);
    m_operationCombo.setSelectedId(1);
    m_operationCombo.addListener(this);
    addAndMakeVisible(m_operationCombo);

    // Parameter label
    addAndMakeVisible(m_paramLabel);

    // Parameter slider
    m_paramSlider.setRange(-24.0, 24.0, 0.1);
    m_paramSlider.setValue(0.0);
    m_paramSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    m_paramSlider.onValueChange = [this]()
    {
        if (onSettingsChanged)
            onSettingsChanged();
    };
    addAndMakeVisible(m_paramSlider);

    // Curve combo (for fades)
    m_curveCombo.addItem("Linear", 1);
    m_curveCombo.addItem("Exponential", 2);
    m_curveCombo.addItem("Logarithmic", 3);
    m_curveCombo.addItem("S-Curve", 4);
    m_curveCombo.setSelectedId(1);
    m_curveCombo.onChange = [this]()
    {
        if (onSettingsChanged)
            onSettingsChanged();
    };
    addChildComponent(m_curveCombo);

    // Remove button
    m_removeButton.onClick = [this]()
    {
        if (onRemoveClicked)
            onRemoveClicked();
    };
    addAndMakeVisible(m_removeButton);

    updateParameterVisibility();
}

void DSPOperationComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff383838));
    g.setColour(juce::Colour(0xff505050));
    g.drawRect(getLocalBounds(), 1);
}

void DSPOperationComponent::resized()
{
    auto area = getLocalBounds().reduced(5);

    m_enabledToggle.setBounds(area.removeFromLeft(70));
    area.removeFromLeft(5);

    m_operationCombo.setBounds(area.removeFromLeft(100));
    area.removeFromLeft(10);

    m_removeButton.setBounds(area.removeFromRight(30));
    area.removeFromRight(5);

    if (m_curveCombo.isVisible())
    {
        m_curveCombo.setBounds(area.removeFromRight(100));
        area.removeFromRight(5);
    }

    m_paramLabel.setBounds(area.removeFromLeft(50));
    m_paramSlider.setBounds(area);
}

void DSPOperationComponent::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &m_operationCombo)
    {
        updateParameterVisibility();
        if (onSettingsChanged)
            onSettingsChanged();
    }
}

BatchDSPSettings DSPOperationComponent::getSettings() const
{
    BatchDSPSettings settings;
    settings.enabled = m_enabledToggle.getToggleState();
    settings.operation = static_cast<BatchDSPOperation>(m_operationCombo.getSelectedId() - 1);

    switch (settings.operation)
    {
        case BatchDSPOperation::GAIN:
            settings.gainDb = static_cast<float>(m_paramSlider.getValue());
            break;
        case BatchDSPOperation::NORMALIZE:
            settings.normalizeTargetDb = static_cast<float>(m_paramSlider.getValue());
            break;
        case BatchDSPOperation::FADE_IN:
        case BatchDSPOperation::FADE_OUT:
            settings.fadeDurationMs = static_cast<float>(m_paramSlider.getValue());
            settings.fadeType = m_curveCombo.getSelectedId() - 1;
            break;
        default:
            break;
    }

    return settings;
}

void DSPOperationComponent::setSettings(const BatchDSPSettings& settings)
{
    m_enabledToggle.setToggleState(settings.enabled, juce::dontSendNotification);
    m_operationCombo.setSelectedId(static_cast<int>(settings.operation) + 1, juce::dontSendNotification);

    switch (settings.operation)
    {
        case BatchDSPOperation::GAIN:
            m_paramSlider.setValue(settings.gainDb, juce::dontSendNotification);
            break;
        case BatchDSPOperation::NORMALIZE:
            m_paramSlider.setValue(settings.normalizeTargetDb, juce::dontSendNotification);
            break;
        case BatchDSPOperation::FADE_IN:
        case BatchDSPOperation::FADE_OUT:
            m_paramSlider.setValue(settings.fadeDurationMs, juce::dontSendNotification);
            m_curveCombo.setSelectedId(settings.fadeType + 1, juce::dontSendNotification);
            break;
        default:
            break;
    }

    updateParameterVisibility();
}

void DSPOperationComponent::updateParameterVisibility()
{
    auto op = static_cast<BatchDSPOperation>(m_operationCombo.getSelectedId() - 1);

    bool showSlider = true;
    bool showCurve = false;

    switch (op)
    {
        case BatchDSPOperation::GAIN:
            m_paramLabel.setText("Gain (dB):", juce::dontSendNotification);
            m_paramSlider.setRange(-24.0, 24.0, 0.1);
            break;

        case BatchDSPOperation::NORMALIZE:
            m_paramLabel.setText("Target (dB):", juce::dontSendNotification);
            m_paramSlider.setRange(-24.0, 0.0, 0.1);
            m_paramSlider.setValue(-0.3, juce::dontSendNotification);
            break;

        case BatchDSPOperation::DC_OFFSET:
            showSlider = false;
            m_paramLabel.setText("", juce::dontSendNotification);
            break;

        case BatchDSPOperation::FADE_IN:
        case BatchDSPOperation::FADE_OUT:
            m_paramLabel.setText("Duration (ms):", juce::dontSendNotification);
            m_paramSlider.setRange(1.0, 5000.0, 1.0);
            m_paramSlider.setValue(100.0, juce::dontSendNotification);
            showCurve = true;
            break;

        default:
            showSlider = false;
            break;
    }

    m_paramSlider.setVisible(showSlider);
    m_paramLabel.setVisible(showSlider);
    m_curveCombo.setVisible(showCurve);
    resized();
}

// =============================================================================
// DSPChainPanel
// =============================================================================

DSPChainPanel::DSPChainPanel()
    : m_titleLabel("titleLabel", "DSP Processing Chain")
    , m_addButton("+ Add Operation")
{
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_titleLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    #pragma GCC diagnostic pop
    addAndMakeVisible(m_titleLabel);

    m_addButton.onClick = [this]() { addOperation(); };
    addAndMakeVisible(m_addButton);

    m_contentComponent = std::make_unique<juce::Component>();
    m_viewport.setViewedComponent(m_contentComponent.get(), false);
    m_viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(m_viewport);
}

void DSPChainPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawRect(getLocalBounds(), 1);
}

void DSPChainPanel::resized()
{
    auto area = getLocalBounds().reduced(5);

    auto headerArea = area.removeFromTop(25);
    m_titleLabel.setBounds(headerArea.removeFromLeft(200));
    m_addButton.setBounds(headerArea.removeFromRight(120));

    area.removeFromTop(5);
    m_viewport.setBounds(area);

    rebuildLayout();
}

std::vector<BatchDSPSettings> DSPChainPanel::getDSPChain() const
{
    std::vector<BatchDSPSettings> chain;
    for (auto* op : m_operations)
        chain.push_back(op->getSettings());
    return chain;
}

void DSPChainPanel::setDSPChain(const std::vector<BatchDSPSettings>& chain)
{
    m_operations.clear();

    for (size_t i = 0; i < chain.size(); ++i)
    {
        auto* op = new DSPOperationComponent(static_cast<int>(i));
        op->setSettings(chain[i]);

        int index = static_cast<int>(i);
        op->onRemoveClicked = [this, index]() { removeOperation(index); };
        op->onSettingsChanged = [this]()
        {
            if (onChainChanged)
                onChainChanged();
        };

        m_operations.add(op);
        m_contentComponent->addAndMakeVisible(op);
    }

    rebuildLayout();
}

void DSPChainPanel::addOperation()
{
    int index = m_operations.size();
    auto* op = new DSPOperationComponent(index);

    op->onRemoveClicked = [this, index]() { removeOperation(index); };
    op->onSettingsChanged = [this]()
    {
        if (onChainChanged)
            onChainChanged();
    };

    m_operations.add(op);
    m_contentComponent->addAndMakeVisible(op);

    rebuildLayout();

    if (onChainChanged)
        onChainChanged();
}

void DSPChainPanel::removeOperation(int index)
{
    if (index >= 0 && index < m_operations.size())
    {
        m_operations.remove(index);
        rebuildLayout();

        // Update indices for remaining operations
        for (int i = 0; i < m_operations.size(); ++i)
        {
            int newIndex = i;
            m_operations[i]->onRemoveClicked = [this, newIndex]() { removeOperation(newIndex); };
        }

        if (onChainChanged)
            onChainChanged();
    }
}

void DSPChainPanel::rebuildLayout()
{
    int yPos = 0;
    int rowHeight = 40;
    int width = m_viewport.getWidth() - 20; // Account for scrollbar

    for (auto* op : m_operations)
    {
        op->setBounds(0, yPos, width, rowHeight);
        yPos += rowHeight + 2;
    }

    m_contentComponent->setSize(width, juce::jmax(yPos, m_viewport.getHeight()));
}

// =============================================================================
// BatchProcessorDialog
// =============================================================================

BatchProcessorDialog::BatchProcessorDialog()
    : m_filesLabel("filesLabel", "Input Files:")
    , m_addFilesButton("Add Files...")
    , m_addFolderButton("Add Folder...")
    , m_removeFilesButton("Remove")
    , m_clearFilesButton("Clear All")
    , m_fileCountLabel("fileCountLabel", "0 files selected")
    , m_outputLabel("outputLabel", "Output Settings")
    , m_outputDirLabel("outputDirLabel", "Output Directory:")
    , m_browseOutputButton("Browse...")
    , m_patternLabel("patternLabel", "Naming Pattern:")
    , m_patternHelpLabel("patternHelpLabel", "Tokens: {filename}, {index}, {index:03}, {date}, {time}, {preset}")
    , m_overwriteToggle("Overwrite existing files")
    , m_formatLabel("formatLabel", "Format:")
    , m_bitDepthLabel("bitDepthLabel", "Bit Depth:")
    , m_sampleRateLabel("sampleRateLabel", "Sample Rate:")
    , m_presetLabel("presetLabel", "Preset:")
    , m_savePresetButton("Save...")
    , m_deletePresetButton("Delete")
    , m_progressBar(m_progress)
    , m_statusLabel("statusLabel", "Ready")
    , m_startButton("Start Processing")
    , m_cancelButton("Cancel")
    , m_closeButton("Close")
{
    // Initialize managers
    m_presetManager = std::make_unique<BatchPresetManager>();
    m_engine = std::make_unique<BatchProcessorEngine>();
    m_engine->addListener(this);

    // File list model
    m_fileListModel = std::make_unique<FileListModel>(m_inputFiles);

    // =========================================================================
    // File Selection Section
    // =========================================================================
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_filesLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    #pragma GCC diagnostic pop
    addAndMakeVisible(m_filesLabel);

    m_fileListBox.setModel(m_fileListModel.get());
    m_fileListBox.setMultipleSelectionEnabled(true);
    m_fileListBox.setRowHeight(20);
    addAndMakeVisible(m_fileListBox);

    m_addFilesButton.onClick = [this]() { onAddFilesClicked(); };
    addAndMakeVisible(m_addFilesButton);

    m_addFolderButton.onClick = [this]() { onAddFolderClicked(); };
    addAndMakeVisible(m_addFolderButton);

    m_removeFilesButton.onClick = [this]() { onRemoveFilesClicked(); };
    addAndMakeVisible(m_removeFilesButton);

    m_clearFilesButton.onClick = [this]() { onClearFilesClicked(); };
    addAndMakeVisible(m_clearFilesButton);

    addAndMakeVisible(m_fileCountLabel);

    // =========================================================================
    // DSP Chain Section
    // =========================================================================
    m_dspChainPanel = std::make_unique<DSPChainPanel>();
    m_dspChainPanel->onChainChanged = [this]() { updatePreview(); };
    addAndMakeVisible(m_dspChainPanel.get());

    // =========================================================================
    // Output Settings Section
    // =========================================================================
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_outputLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    #pragma GCC diagnostic pop
    addAndMakeVisible(m_outputLabel);

    addAndMakeVisible(m_outputDirLabel);

    m_outputDirEditor.onTextChange = [this]() { updatePreview(); };
    addAndMakeVisible(m_outputDirEditor);

    m_browseOutputButton.onClick = [this]() { onBrowseOutputClicked(); };
    addAndMakeVisible(m_browseOutputButton);

    addAndMakeVisible(m_patternLabel);

    m_patternEditor.setText("{filename}_processed");
    m_patternEditor.onTextChange = [this]() { updatePreview(); };
    addAndMakeVisible(m_patternEditor);

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_patternHelpLabel.setFont(juce::Font(10.0f));
    #pragma GCC diagnostic pop
    m_patternHelpLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible(m_patternHelpLabel);

    addAndMakeVisible(m_overwriteToggle);

    addAndMakeVisible(m_formatLabel);
    m_formatCombo.addItem("WAV", 1);
    m_formatCombo.addItem("FLAC", 2);
    m_formatCombo.addItem("OGG", 3);
    m_formatCombo.setSelectedId(1);
    addAndMakeVisible(m_formatCombo);

    addAndMakeVisible(m_bitDepthLabel);
    m_bitDepthCombo.addItem("16-bit", 16);
    m_bitDepthCombo.addItem("24-bit", 24);
    m_bitDepthCombo.addItem("32-bit", 32);
    m_bitDepthCombo.setSelectedId(16);
    addAndMakeVisible(m_bitDepthCombo);

    addAndMakeVisible(m_sampleRateLabel);
    m_sampleRateCombo.addItem("Keep Original", 1);
    m_sampleRateCombo.addItem("44100 Hz", 44100);
    m_sampleRateCombo.addItem("48000 Hz", 48000);
    m_sampleRateCombo.addItem("96000 Hz", 96000);
    m_sampleRateCombo.setSelectedId(1);
    addAndMakeVisible(m_sampleRateCombo);

    // =========================================================================
    // Preset Section
    // =========================================================================
    addAndMakeVisible(m_presetLabel);

    m_presetCombo.onChange = [this]() { onPresetChanged(); };
    addAndMakeVisible(m_presetCombo);

    m_savePresetButton.onClick = [this]() { onSavePresetClicked(); };
    addAndMakeVisible(m_savePresetButton);

    m_deletePresetButton.onClick = [this]() { onDeletePresetClicked(); };
    addAndMakeVisible(m_deletePresetButton);

    refreshPresetList();

    // =========================================================================
    // Progress Section
    // =========================================================================
    addAndMakeVisible(m_progressBar);
    addAndMakeVisible(m_statusLabel);

    m_logEditor.setMultiLine(true);
    m_logEditor.setReadOnly(true);
    m_logEditor.setScrollbarsShown(true);
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_logEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    #pragma GCC diagnostic pop
    addAndMakeVisible(m_logEditor);

    // =========================================================================
    // Action Buttons
    // =========================================================================
    m_startButton.onClick = [this]() { onStartClicked(); };
    addAndMakeVisible(m_startButton);

    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    m_cancelButton.setEnabled(false);
    addAndMakeVisible(m_cancelButton);

    m_closeButton.onClick = [this]() { onCloseClicked(); };
    addAndMakeVisible(m_closeButton);

    // Set initial size
    setSize(800, 700);
}

BatchProcessorDialog::~BatchProcessorDialog()
{
    if (m_engine)
    {
        m_engine->removeListener(this);
        m_engine->cancelProcessing();
        m_engine->waitForCompletion(5000);
    }
}

void BatchProcessorDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
}

void BatchProcessorDialog::resized()
{
    auto area = getLocalBounds().reduced(10);

    // =========================================================================
    // Left column (file list)
    // =========================================================================
    auto leftColumn = area.removeFromLeft(300);

    m_filesLabel.setBounds(leftColumn.removeFromTop(25));
    leftColumn.removeFromTop(5);

    auto fileButtonRow = leftColumn.removeFromTop(25);
    int btnWidth = 70;
    m_addFilesButton.setBounds(fileButtonRow.removeFromLeft(btnWidth));
    fileButtonRow.removeFromLeft(5);
    m_addFolderButton.setBounds(fileButtonRow.removeFromLeft(btnWidth + 10));
    fileButtonRow.removeFromLeft(5);
    m_removeFilesButton.setBounds(fileButtonRow.removeFromLeft(btnWidth));
    fileButtonRow.removeFromLeft(5);
    m_clearFilesButton.setBounds(fileButtonRow.removeFromLeft(btnWidth));

    leftColumn.removeFromTop(5);
    m_fileCountLabel.setBounds(leftColumn.removeFromTop(20));

    leftColumn.removeFromTop(5);
    m_fileListBox.setBounds(leftColumn);

    area.removeFromLeft(10);

    // =========================================================================
    // Right column (settings)
    // =========================================================================
    auto rightColumn = area;

    // DSP Chain
    m_dspChainPanel->setBounds(rightColumn.removeFromTop(180));
    rightColumn.removeFromTop(10);

    // Output Settings
    m_outputLabel.setBounds(rightColumn.removeFromTop(25));
    rightColumn.removeFromTop(5);

    auto outputDirRow = rightColumn.removeFromTop(25);
    m_outputDirLabel.setBounds(outputDirRow.removeFromLeft(110));
    m_browseOutputButton.setBounds(outputDirRow.removeFromRight(80));
    outputDirRow.removeFromRight(5);
    m_outputDirEditor.setBounds(outputDirRow);

    rightColumn.removeFromTop(5);

    auto patternRow = rightColumn.removeFromTop(25);
    m_patternLabel.setBounds(patternRow.removeFromLeft(110));
    m_patternEditor.setBounds(patternRow);

    m_patternHelpLabel.setBounds(rightColumn.removeFromTop(15).withTrimmedLeft(110));
    rightColumn.removeFromTop(5);

    m_overwriteToggle.setBounds(rightColumn.removeFromTop(25).withTrimmedLeft(110));
    rightColumn.removeFromTop(5);

    auto formatRow = rightColumn.removeFromTop(25);
    m_formatLabel.setBounds(formatRow.removeFromLeft(50));
    m_formatCombo.setBounds(formatRow.removeFromLeft(80));
    formatRow.removeFromLeft(20);
    m_bitDepthLabel.setBounds(formatRow.removeFromLeft(60));
    m_bitDepthCombo.setBounds(formatRow.removeFromLeft(80));
    formatRow.removeFromLeft(20);
    m_sampleRateLabel.setBounds(formatRow.removeFromLeft(80));
    m_sampleRateCombo.setBounds(formatRow.removeFromLeft(100));

    rightColumn.removeFromTop(10);

    // Preset Row
    auto presetRow = rightColumn.removeFromTop(25);
    m_presetLabel.setBounds(presetRow.removeFromLeft(50));
    m_deletePresetButton.setBounds(presetRow.removeFromRight(60));
    presetRow.removeFromRight(5);
    m_savePresetButton.setBounds(presetRow.removeFromRight(60));
    presetRow.removeFromRight(5);
    m_presetCombo.setBounds(presetRow);

    rightColumn.removeFromTop(10);

    // Progress
    m_progressBar.setBounds(rightColumn.removeFromTop(20));
    rightColumn.removeFromTop(5);
    m_statusLabel.setBounds(rightColumn.removeFromTop(20));
    rightColumn.removeFromTop(5);

    // Log
    auto logHeight = rightColumn.getHeight() - 40;
    m_logEditor.setBounds(rightColumn.removeFromTop(logHeight));
    rightColumn.removeFromTop(10);

    // Buttons
    auto buttonRow = rightColumn.removeFromTop(30);
    m_closeButton.setBounds(buttonRow.removeFromRight(80));
    buttonRow.removeFromRight(10);
    m_cancelButton.setBounds(buttonRow.removeFromRight(80));
    buttonRow.removeFromRight(10);
    m_startButton.setBounds(buttonRow.removeFromRight(120));
}

bool BatchProcessorDialog::showDialog()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    std::unique_ptr<BatchProcessorDialog> dialog(new BatchProcessorDialog());

    juce::DialogWindow dlg("Batch Processor", juce::Colours::darkgrey, true, false);
    dlg.setContentOwned(dialog.release(), true);
    dlg.centreWithSize(800, 700);
    dlg.setResizable(true, true);
    dlg.setUsingNativeTitleBar(true);

    dlg.addToDesktop(juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowHasCloseButton);
    dlg.setVisible(true);
    dlg.toFront(true);

    dlg.enterModalState(true);

    #if JUCE_MODAL_LOOPS_PERMITTED
        int result = dlg.runModalLoop();
        return result == 1;
    #else
        return false;
    #endif
}

// =============================================================================
// BatchProcessorListener
// =============================================================================

void BatchProcessorDialog::batchProgressChanged(float progress, int currentFile, int totalFiles,
                                                  const juce::String& statusMessage)
{
    m_progress = progress;

    juce::String status = "Processing file " + juce::String(currentFile) + " of "
                         + juce::String(totalFiles) + ": " + statusMessage;
    m_statusLabel.setText(status, juce::dontSendNotification);

    repaint();
}

void BatchProcessorDialog::jobCompleted(int jobIndex, const BatchJobResult& result)
{
    juce::String logLine;

    if (result.status == BatchJobStatus::COMPLETED)
    {
        logLine = "[OK] " + result.outputFile.getFileName()
                + " (" + juce::String(result.durationSeconds, 1) + "s)";
    }
    else if (result.status == BatchJobStatus::FAILED)
    {
        logLine = "[FAIL] Job " + juce::String(jobIndex + 1) + ": " + result.errorMessage;
    }
    else if (result.status == BatchJobStatus::SKIPPED)
    {
        logLine = "[SKIP] Job " + juce::String(jobIndex + 1) + ": " + result.errorMessage;
    }

    m_logEditor.moveCaretToEnd();
    m_logEditor.insertTextAtCaret(logLine + "\n");
}

void BatchProcessorDialog::batchCompleted(bool cancelled, int successCount,
                                           int failedCount, int skippedCount)
{
    setProcessingMode(false);

    juce::String summary;
    if (cancelled)
    {
        summary = "Processing cancelled. ";
    }
    else
    {
        summary = "Processing complete. ";
    }

    summary += juce::String(successCount) + " succeeded, "
             + juce::String(failedCount) + " failed, "
             + juce::String(skippedCount) + " skipped.";

    m_statusLabel.setText(summary, juce::dontSendNotification);

    m_logEditor.moveCaretToEnd();
    m_logEditor.insertTextAtCaret("\n" + summary + "\n");

    // Show completion message
    if (!cancelled && failedCount == 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Batch Processing Complete",
            "Successfully processed " + juce::String(successCount) + " files.",
            "OK"
        );
    }
    else if (failedCount > 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Batch Processing Complete",
            summary + "\n\nCheck the log for details about failed files.",
            "OK"
        );
    }
}

// =============================================================================
// FileDragAndDropTarget
// =============================================================================

bool BatchProcessorDialog::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& file : files)
    {
        juce::File f(file);
        juce::String ext = f.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aif" || ext == ".aiff" ||
            ext == ".flac" || ext == ".mp3" || ext == ".ogg")
        {
            return true;
        }
        if (f.isDirectory())
            return true;
    }
    return false;
}

void BatchProcessorDialog::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    for (const auto& file : files)
    {
        juce::File f(file);

        if (f.isDirectory())
        {
            auto audioFiles = f.findChildFiles(
                juce::File::findFiles, true,
                "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg"
            );

            for (const auto& audioFile : audioFiles)
            {
                if (!m_inputFiles.contains(audioFile.getFullPathName()))
                    m_inputFiles.add(audioFile.getFullPathName());
            }
        }
        else
        {
            juce::String ext = f.getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".aif" || ext == ".aiff" ||
                ext == ".flac" || ext == ".mp3" || ext == ".ogg")
            {
                if (!m_inputFiles.contains(f.getFullPathName()))
                    m_inputFiles.add(f.getFullPathName());
            }
        }
    }

    updateFileList();
}

// =============================================================================
// File List Management
// =============================================================================

void BatchProcessorDialog::onAddFilesClicked()
{
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Select Audio Files",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg"
    );

    auto flags = juce::FileBrowserComponent::openMode |
                 juce::FileBrowserComponent::canSelectFiles |
                 juce::FileBrowserComponent::canSelectMultipleItems;

    m_fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto results = fc.getResults();
        for (const auto& file : results)
        {
            if (!m_inputFiles.contains(file.getFullPathName()))
                m_inputFiles.add(file.getFullPathName());
        }
        updateFileList();
    });
}

void BatchProcessorDialog::onAddFolderClicked()
{
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Select Folder",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory)
    );

    auto flags = juce::FileBrowserComponent::openMode |
                 juce::FileBrowserComponent::canSelectDirectories;

    m_fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result.isDirectory())
        {
            auto audioFiles = result.findChildFiles(
                juce::File::findFiles, true,
                "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg"
            );

            for (const auto& file : audioFiles)
            {
                if (!m_inputFiles.contains(file.getFullPathName()))
                    m_inputFiles.add(file.getFullPathName());
            }
            updateFileList();
        }
    });
}

void BatchProcessorDialog::onRemoveFilesClicked()
{
    auto selectedRows = m_fileListBox.getSelectedRows();
    juce::StringArray newFiles;

    for (int i = 0; i < m_inputFiles.size(); ++i)
    {
        if (!selectedRows.contains(i))
            newFiles.add(m_inputFiles[i]);
    }

    m_inputFiles = newFiles;
    updateFileList();
}

void BatchProcessorDialog::onClearFilesClicked()
{
    m_inputFiles.clear();
    updateFileList();
}

void BatchProcessorDialog::updateFileList()
{
    m_fileListBox.updateContent();
    m_fileCountLabel.setText(juce::String(m_inputFiles.size()) + " files selected",
                              juce::dontSendNotification);
}

// =============================================================================
// Output Settings
// =============================================================================

void BatchProcessorDialog::onBrowseOutputClicked()
{
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Select Output Directory",
        juce::File(m_outputDirEditor.getText())
    );

    auto flags = juce::FileBrowserComponent::openMode |
                 juce::FileBrowserComponent::canSelectDirectories;

    m_fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result.isDirectory())
        {
            m_outputDirEditor.setText(result.getFullPathName());
        }
    });
}

void BatchProcessorDialog::updateOutputPattern()
{
    // Pattern is stored in the text editor
}

void BatchProcessorDialog::updatePreview()
{
    // Could show preview of output filenames here
}

// =============================================================================
// Preset Management
// =============================================================================

void BatchProcessorDialog::onPresetChanged()
{
    int selectedId = m_presetCombo.getSelectedId();
    if (selectedId > 0)
    {
        juce::String presetName = m_presetCombo.getText();
        loadPreset(presetName);
    }
}

void BatchProcessorDialog::onSavePresetClicked()
{
    juce::AlertWindow dialog("Save Preset",
                              "Enter a name for the preset:",
                              juce::AlertWindow::QuestionIcon);

    dialog.addTextEditor("presetName", "", "Preset Name:");
    dialog.addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (dialog.runModalLoop() == 1)
    {
        juce::String presetName = dialog.getTextEditor("presetName")->getText().trim();

        if (presetName.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Invalid Name",
                "Please enter a name for the preset."
            );
            return;
        }

        if (m_presetManager->isFactoryPreset(presetName))
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Cannot Overwrite",
                "Cannot overwrite factory presets."
            );
            return;
        }

        auto settings = gatherSettings();
        if (m_presetManager->savePreset(presetName, "", settings))
        {
            refreshPresetList();
            m_presetCombo.setText(presetName);
        }
    }
}

void BatchProcessorDialog::onDeletePresetClicked()
{
    juce::String presetName = m_presetCombo.getText();

    if (presetName.isEmpty())
        return;

    if (m_presetManager->isFactoryPreset(presetName))
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Cannot Delete",
            "Cannot delete factory presets."
        );
        return;
    }

    bool confirm = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::QuestionIcon,
        "Delete Preset",
        "Are you sure you want to delete the preset '" + presetName + "'?",
        "Delete",
        "Cancel"
    );

    if (confirm)
    {
        if (m_presetManager->deletePreset(presetName))
        {
            refreshPresetList();
        }
    }
}

void BatchProcessorDialog::loadPreset(const juce::String& name)
{
    auto* preset = m_presetManager->getPreset(name);
    if (preset == nullptr)
        return;

    // Load DSP chain
    m_dspChainPanel->setDSPChain(preset->settings.dspChain);

    // Load output settings
    m_patternEditor.setText(preset->settings.outputPattern);
    m_overwriteToggle.setToggleState(preset->settings.overwriteExisting, juce::dontSendNotification);

    // Load format settings
    const auto& fmt = preset->settings.outputFormat.format;
    if (fmt == "wav")
        m_formatCombo.setSelectedId(1);
    else if (fmt == "flac")
        m_formatCombo.setSelectedId(2);
    else if (fmt == "ogg")
        m_formatCombo.setSelectedId(3);
    else
        m_formatCombo.setSelectedId(1);

    if (preset->settings.outputFormat.bitDepth > 0)
        m_bitDepthCombo.setSelectedId(preset->settings.outputFormat.bitDepth);

    if (preset->settings.outputFormat.sampleRate > 0)
        m_sampleRateCombo.setSelectedId(preset->settings.outputFormat.sampleRate);
    else
        m_sampleRateCombo.setSelectedId(1);
}

void BatchProcessorDialog::refreshPresetList()
{
    m_presetCombo.clear();

    auto names = m_presetManager->getPresetNames();
    int id = 1;
    for (const auto& name : names)
    {
        m_presetCombo.addItem(name, id++);
    }
}

// =============================================================================
// Processing
// =============================================================================

void BatchProcessorDialog::onStartClicked()
{
    if (!validateSettings())
        return;

    auto settings = gatherSettings();

    m_engine->setSettings(settings);

    // Clear log
    m_logEditor.clear();
    m_logEditor.insertTextAtCaret("Starting batch processing...\n\n");

    setProcessingMode(true);

    if (!m_engine->startProcessing())
    {
        setProcessingMode(false);
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Processing Failed",
            "Failed to start batch processing. Check settings and try again."
        );
    }
}

void BatchProcessorDialog::onCancelClicked()
{
    if (m_engine && m_engine->isProcessing())
    {
        m_engine->cancelProcessing();
        m_statusLabel.setText("Cancelling...", juce::dontSendNotification);
    }
}

void BatchProcessorDialog::onCloseClicked()
{
    if (m_engine && m_engine->isProcessing())
    {
        bool confirm = juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::QuestionIcon,
            "Processing in Progress",
            "Batch processing is still running. Cancel and close?",
            "Cancel & Close",
            "Keep Running"
        );

        if (!confirm)
            return;

        m_engine->cancelProcessing();
        m_engine->waitForCompletion(5000);
    }

    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
    {
        dw->exitModalState(0);
    }
}

bool BatchProcessorDialog::validateSettings()
{
    if (m_inputFiles.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Input Files",
            "Please add at least one audio file to process."
        );
        return false;
    }

    juce::String outputDir = m_outputDirEditor.getText().trim();
    if (outputDir.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Output Directory",
            "Please select an output directory."
        );
        return false;
    }

    juce::File outDir(outputDir);
    if (!outDir.exists())
    {
        bool create = juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::QuestionIcon,
            "Create Directory",
            "Output directory does not exist. Create it?",
            "Create",
            "Cancel"
        );

        if (create)
        {
            if (!outDir.createDirectory())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Error",
                    "Failed to create output directory."
                );
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    juce::String pattern = m_patternEditor.getText().trim();
    if (pattern.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Output Pattern",
            "Please enter an output naming pattern."
        );
        return false;
    }

    return true;
}

BatchProcessorSettings BatchProcessorDialog::gatherSettings()
{
    BatchProcessorSettings settings;

    // Input files
    settings.inputFiles = m_inputFiles;

    // Output directory
    settings.outputDirectory = juce::File(m_outputDirEditor.getText().trim());

    // Naming pattern
    settings.outputPattern = m_patternEditor.getText().trim();

    // Overwrite
    settings.overwriteExisting = m_overwriteToggle.getToggleState();

    // DSP chain
    settings.dspChain = m_dspChainPanel->getDSPChain();

    // Output format
    switch (m_formatCombo.getSelectedId())
    {
        case 1: settings.outputFormat.format = "wav"; break;
        case 2: settings.outputFormat.format = "flac"; break;
        case 3: settings.outputFormat.format = "ogg"; break;
        default: settings.outputFormat.format = "wav"; break;
    }

    settings.outputFormat.bitDepth = m_bitDepthCombo.getSelectedId();

    int sampleRateId = m_sampleRateCombo.getSelectedId();
    settings.outputFormat.sampleRate = (sampleRateId == 1) ? 0 : sampleRateId;

    return settings;
}

void BatchProcessorDialog::setProcessingMode(bool processing)
{
    m_isProcessing = processing;

    m_startButton.setEnabled(!processing);
    m_cancelButton.setEnabled(processing);
    m_closeButton.setEnabled(!processing);

    m_addFilesButton.setEnabled(!processing);
    m_addFolderButton.setEnabled(!processing);
    m_removeFilesButton.setEnabled(!processing);
    m_clearFilesButton.setEnabled(!processing);
    m_browseOutputButton.setEnabled(!processing);
    m_presetCombo.setEnabled(!processing);
    m_savePresetButton.setEnabled(!processing);
    m_deletePresetButton.setEnabled(!processing);
}

} // namespace waveedit
