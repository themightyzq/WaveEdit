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
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "BatchProcessorSettings.h"
#include "BatchProcessorEngine.h"
#include "BatchPresetManager.h"

namespace waveedit
{

/**
 * @brief Info about a file in the batch list
 */
struct BatchFileInfo
{
    juce::String fullPath;
    juce::String fileName;
    juce::int64 sizeBytes = 0;
    double durationSeconds = 0.0;
    int numChannels = 0;
    double sampleRate = 0.0;
    BatchJobStatus status = BatchJobStatus::PENDING;
    float progress = 0.0f;  ///< 0.0 to 1.0 progress for current file

    juce::String getFormattedSize() const
    {
        if (sizeBytes < 1024)
            return juce::String(sizeBytes) + " B";
        else if (sizeBytes < 1024 * 1024)
            return juce::String(sizeBytes / 1024.0, 1) + " KB";
        else
            return juce::String(sizeBytes / (1024.0 * 1024.0), 1) + " MB";
    }

    juce::String getFormattedDuration() const
    {
        int totalSeconds = static_cast<int>(durationSeconds);
        int minutes = totalSeconds / 60;
        int seconds = totalSeconds % 60;
        return juce::String::formatted("%d:%02d", minutes, seconds);
    }
};

/**
 * @brief Component for configuring a single DSP operation
 * Supports drag-and-drop reordering within the DSP chain.
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

    // Drag-and-drop support
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    BatchDSPSettings getSettings() const;
    void setSettings(const BatchDSPSettings& settings);

    int getIndex() const { return m_index; }
    void setIndex(int index) { m_index = index; }

    void setDragHighlight(bool highlighted);

    std::function<void()> onRemoveClicked;
    std::function<void()> onMoveUpClicked;
    std::function<void()> onMoveDownClicked;
    std::function<void()> onSettingsChanged;
    std::function<void(int fromIndex, int toIndex)> onDragReorder;

    void setMoveButtonsEnabled(bool canMoveUp, bool canMoveDown);

private:
    void updateParameterVisibility();
    void showDetailsPopup();

    int m_index;
    bool m_isDragging = false;
    bool m_dragHighlight = false;
    juce::Point<int> m_dragStartPos;

    juce::ToggleButton m_enabledToggle;
    juce::ComboBox m_operationCombo;
    juce::Label m_paramLabel;
    juce::Slider m_paramSlider;
    juce::ComboBox m_curveCombo;
    juce::TextButton m_detailsButton;
    juce::TextButton m_moveUpButton;
    juce::TextButton m_moveDownButton;
    juce::TextButton m_removeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DSPOperationComponent)
};

/**
 * @brief Panel for managing DSP chain operations
 * Supports drag-and-drop reordering of operations.
 */
class DSPChainPanel : public juce::Component,
                      public juce::DragAndDropContainer,
                      public juce::DragAndDropTarget
{
public:
    DSPChainPanel();
    ~DSPChainPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::vector<BatchDSPSettings> getDSPChain() const;
    void setDSPChain(const std::vector<BatchDSPSettings>& chain);

    // DragAndDropTarget interface
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails& details) override;
    void itemDragMove(const SourceDetails& details) override;
    void itemDragExit(const SourceDetails& details) override;
    void itemDropped(const SourceDetails& details) override;

    void moveOperationTo(int fromIndex, int toIndex);

    std::function<void()> onChainChanged;

private:
    void addOperation();
    void removeOperation(int index);
    void moveOperation(int index, int direction);  // direction: -1 = up, +1 = down
    void rebuildLayout();
    void updateOperationCallbacks();
    int getDropIndexFromY(int y) const;
    void clearDropIndicator();

    juce::Label m_titleLabel;
    juce::TextButton m_addButton;
    juce::OwnedArray<DSPOperationComponent> m_operations;
    juce::Viewport m_viewport;
    std::unique_ptr<juce::Component> m_contentComponent;

    int m_dropIndicatorIndex = -1;  ///< Index where drop would occur, -1 = none

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
    void addFileWithInfo(const juce::File& file);
    BatchFileInfo getFileInfo(const juce::File& file);
    juce::String getTotalFileSummary() const;

    // Output settings
    void onBrowseOutputClicked();
    void onSameAsSourceToggled();
    void onPatternHelpClicked();
    void updateOutputPattern();
    void updatePreview();
    void updateOutputPreview();

    // Preset management
    void onPresetChanged();
    void onSavePresetClicked();
    void onDeletePresetClicked();
    void onExportPresetClicked();
    void onImportPresetClicked();
    void loadPreset(const juce::String& name);
    void refreshPresetList();

    // Processing
    void onStartClicked();
    void onCancelClicked();
    void onCloseClicked();
    bool validateSettings();
    BatchProcessorSettings gatherSettings();

    // Audio Preview
    void onPreviewClicked();
    void stopPreview();
    void startPreviewPlayback(const juce::File& audioFile);
    void cleanupPreview();

    // Plugin Chain
    void onUsePluginChainToggled();
    void onBrowsePluginPresetClicked();
    void updatePluginChainUI();

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
    // UI Components - Plugin Chain
    // =========================================================================
    juce::ToggleButton m_usePluginChainToggle;
    juce::Label m_pluginPresetLabel;
    juce::TextEditor m_pluginPresetEditor;
    juce::TextButton m_browsePluginPresetButton;
    juce::Label m_pluginTailLabel;
    juce::Slider m_pluginTailSlider;

    // =========================================================================
    // UI Components - Output Settings
    // =========================================================================
    juce::Label m_outputLabel;
    juce::Label m_outputDirLabel;
    juce::TextEditor m_outputDirEditor;
    juce::TextButton m_browseOutputButton;
    juce::ToggleButton m_sameAsSourceToggle;  // Output to same folder as source
    juce::Label m_patternLabel;
    juce::TextEditor m_patternEditor;
    juce::Label m_patternHelpLabel;
    juce::TextButton m_patternHelpButton;     // "?" button for full pattern docs
    juce::ToggleButton m_overwriteToggle;
    juce::Label m_formatLabel;
    juce::ComboBox m_formatCombo;
    juce::Label m_bitDepthLabel;
    juce::ComboBox m_bitDepthCombo;
    juce::Label m_sampleRateLabel;
    juce::ComboBox m_sampleRateCombo;

    // =========================================================================
    // UI Components - Output Preview
    // =========================================================================
    juce::Label m_previewLabel;
    juce::TextEditor m_outputPreviewEditor;   // Shows input -> output mapping

    // =========================================================================
    // UI Components - Presets
    // =========================================================================
    juce::Label m_presetLabel;
    juce::ComboBox m_presetCombo;
    juce::TextButton m_savePresetButton;
    juce::TextButton m_deletePresetButton;
    juce::TextButton m_exportPresetButton;
    juce::TextButton m_importPresetButton;

    // =========================================================================
    // UI Components - Progress
    // =========================================================================
    juce::ProgressBar m_progressBar;
    juce::Label m_statusLabel;
    juce::TextEditor m_logEditor;

    // =========================================================================
    // UI Components - Buttons
    // =========================================================================
    juce::TextButton m_previewButton;   // Preview selected file with DSP chain
    juce::TextButton m_startButton;
    juce::TextButton m_cancelButton;
    juce::TextButton m_closeButton;

    // =========================================================================
    // State
    // =========================================================================
    juce::StringArray m_inputFiles;
    double m_progress = 0.0;
    bool m_isProcessing = false;
    bool m_isPreviewing = false;

    // =========================================================================
    // Processing
    // =========================================================================
    std::unique_ptr<BatchProcessorEngine> m_engine;
    std::unique_ptr<BatchPresetManager> m_presetManager;
    std::unique_ptr<juce::FileChooser> m_fileChooser;

    // =========================================================================
    // Audio Preview
    // =========================================================================
    juce::AudioDeviceManager m_previewDeviceManager;
    juce::AudioSourcePlayer m_previewSourcePlayer;
    std::unique_ptr<juce::AudioTransportSource> m_previewTransport;
    std::unique_ptr<juce::AudioFormatReaderSource> m_previewReaderSource;
    std::unique_ptr<juce::AudioBuffer<float>> m_previewBuffer;
    std::unique_ptr<juce::MemoryAudioSource> m_previewMemorySource;

    // Enhanced file list model with size, duration, path, and status
    class FileListModel : public juce::ListBoxModel,
                           public juce::TooltipClient
    {
    public:
        FileListModel(std::vector<BatchFileInfo>& files) : m_files(files) {}

        int getNumRows() override { return static_cast<int>(m_files.size()); }

        void paintListBoxItem(int rowNumber, juce::Graphics& g,
                               int width, int height, bool rowIsSelected) override;

        juce::String getTooltipForRow(int row);
        juce::String getTooltip() override { return {}; }

        void setHoveredRow(int row) { m_hoveredRow = row; }
        int getHoveredRow() const { return m_hoveredRow; }

    private:
        std::vector<BatchFileInfo>& m_files;
        int m_hoveredRow = -1;
    };

    std::unique_ptr<FileListModel> m_fileListModel;
    std::vector<BatchFileInfo> m_fileInfos;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BatchProcessorDialog)
};

} // namespace waveedit
