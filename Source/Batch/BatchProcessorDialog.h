/*
  ==============================================================================

    BatchProcessorDialog.h
    Created: 2025
    Author:  ZQ SFX

    Main dialog for batch audio processing.
    Allows selecting multiple files, configuring DSP chain, output settings,
    and monitoring processing progress.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "BatchProcessorSettings.h"
#include "BatchProcessorEngine.h"
#include "BatchPresetManager.h"

namespace waveedit
{

/**
 * @brief Component for configuring a single DSP operation
 */
class DSPOperationComponent : public juce::Component,
                               public juce::ComboBox::Listener
{
public:
    DSPOperationComponent(int index);
    ~DSPOperationComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void comboBoxChanged(juce::ComboBox* comboBox) override;

    BatchDSPSettings getSettings() const;
    void setSettings(const BatchDSPSettings& settings);

    std::function<void()> onRemoveClicked;
    std::function<void()> onSettingsChanged;

private:
    void updateParameterVisibility();

    int m_index;
    juce::ToggleButton m_enabledToggle;
    juce::ComboBox m_operationCombo;
    juce::Label m_paramLabel;
    juce::Slider m_paramSlider;
    juce::ComboBox m_curveCombo;
    juce::TextButton m_removeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DSPOperationComponent)
};

/**
 * @brief Panel for managing DSP chain operations
 */
class DSPChainPanel : public juce::Component
{
public:
    DSPChainPanel();
    ~DSPChainPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::vector<BatchDSPSettings> getDSPChain() const;
    void setDSPChain(const std::vector<BatchDSPSettings>& chain);

    std::function<void()> onChainChanged;

private:
    void addOperation();
    void removeOperation(int index);
    void rebuildLayout();

    juce::Label m_titleLabel;
    juce::TextButton m_addButton;
    juce::OwnedArray<DSPOperationComponent> m_operations;
    juce::Viewport m_viewport;
    std::unique_ptr<juce::Component> m_contentComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DSPChainPanel)
};

/**
 * @brief Main batch processor dialog
 */
class BatchProcessorDialog : public juce::Component,
                              public BatchProcessorListener,
                              public juce::FileDragAndDropTarget
{
public:
    BatchProcessorDialog();
    ~BatchProcessorDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /**
     * @brief Show the dialog modally
     * @return true if processing was started
     */
    static bool showDialog();

    // BatchProcessorListener
    void batchProgressChanged(float progress, int currentFile, int totalFiles,
                               const juce::String& statusMessage) override;
    void jobCompleted(int jobIndex, const BatchJobResult& result) override;
    void batchCompleted(bool cancelled, int successCount,
                        int failedCount, int skippedCount) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    // File list management
    void onAddFilesClicked();
    void onAddFolderClicked();
    void onRemoveFilesClicked();
    void onClearFilesClicked();
    void updateFileList();

    // Output settings
    void onBrowseOutputClicked();
    void updateOutputPattern();
    void updatePreview();

    // Preset management
    void onPresetChanged();
    void onSavePresetClicked();
    void onDeletePresetClicked();
    void loadPreset(const juce::String& name);
    void refreshPresetList();

    // Processing
    void onStartClicked();
    void onCancelClicked();
    void onCloseClicked();
    bool validateSettings();
    BatchProcessorSettings gatherSettings();

    // UI state
    void setProcessingMode(bool processing);

    // =========================================================================
    // UI Components - File Selection
    // =========================================================================
    juce::Label m_filesLabel;
    juce::ListBox m_fileListBox;
    juce::TextButton m_addFilesButton;
    juce::TextButton m_addFolderButton;
    juce::TextButton m_removeFilesButton;
    juce::TextButton m_clearFilesButton;
    juce::Label m_fileCountLabel;

    // =========================================================================
    // UI Components - DSP Chain
    // =========================================================================
    std::unique_ptr<DSPChainPanel> m_dspChainPanel;

    // =========================================================================
    // UI Components - Output Settings
    // =========================================================================
    juce::Label m_outputLabel;
    juce::Label m_outputDirLabel;
    juce::TextEditor m_outputDirEditor;
    juce::TextButton m_browseOutputButton;
    juce::Label m_patternLabel;
    juce::TextEditor m_patternEditor;
    juce::Label m_patternHelpLabel;
    juce::ToggleButton m_overwriteToggle;
    juce::Label m_formatLabel;
    juce::ComboBox m_formatCombo;
    juce::Label m_bitDepthLabel;
    juce::ComboBox m_bitDepthCombo;
    juce::Label m_sampleRateLabel;
    juce::ComboBox m_sampleRateCombo;

    // =========================================================================
    // UI Components - Presets
    // =========================================================================
    juce::Label m_presetLabel;
    juce::ComboBox m_presetCombo;
    juce::TextButton m_savePresetButton;
    juce::TextButton m_deletePresetButton;

    // =========================================================================
    // UI Components - Progress
    // =========================================================================
    juce::ProgressBar m_progressBar;
    juce::Label m_statusLabel;
    juce::TextEditor m_logEditor;

    // =========================================================================
    // UI Components - Buttons
    // =========================================================================
    juce::TextButton m_startButton;
    juce::TextButton m_cancelButton;
    juce::TextButton m_closeButton;

    // =========================================================================
    // State
    // =========================================================================
    juce::StringArray m_inputFiles;
    double m_progress = 0.0;
    bool m_isProcessing = false;

    // =========================================================================
    // Processing
    // =========================================================================
    std::unique_ptr<BatchProcessorEngine> m_engine;
    std::unique_ptr<BatchPresetManager> m_presetManager;
    std::unique_ptr<juce::FileChooser> m_fileChooser;

    // File list model
    class FileListModel : public juce::ListBoxModel
    {
    public:
        FileListModel(juce::StringArray& files) : m_files(files) {}

        int getNumRows() override { return m_files.size(); }

        void paintListBoxItem(int rowNumber, juce::Graphics& g,
                               int width, int height, bool rowIsSelected) override
        {
            if (rowIsSelected)
                g.fillAll(juce::Colour(0xff3d6ea5));
            else if (rowNumber % 2 == 0)
                g.fillAll(juce::Colour(0xff2b2b2b));
            else
                g.fillAll(juce::Colour(0xff323232));

            g.setColour(juce::Colours::white);
            if (rowNumber < m_files.size())
            {
                juce::File f(m_files[rowNumber]);
                g.drawText(f.getFileName(), 5, 0, width - 10, height,
                           juce::Justification::centredLeft);
            }
        }

    private:
        juce::StringArray& m_files;
    };

    std::unique_ptr<FileListModel> m_fileListModel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BatchProcessorDialog)
};

} // namespace waveedit
