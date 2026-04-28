/*
  ==============================================================================

    FileController.h
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
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Utils/DocumentManager.h"
#include "../Audio/AudioFileManager.h"

/**
 * Controller for all file operations: open, save, save-as, close, drag-drop, auto-save.
 *
 * Owns the file chooser and auto-save thread pool. Delegates to DocumentManager
 * for document lifecycle and to AudioFileManager for file validation.
 */
class FileController
{
public:
    FileController(DocumentManager& docManager,
                   AudioFileManager& fileManager,
                   juce::ApplicationCommandManager& cmdManager);

    ~FileController();

    /**
     * Show open file dialog and load selected file(s).
     * @param parent Parent component for the dialog (typically MainComponent).
     */
    void openFile(juce::Component* parent);

    /**
     * Load a specific file (e.g. from recent files list or command line).
     * Validates path safety and file readability before loading.
     */
    void loadFile(const juce::File& file, juce::Component* parent);

    /**
     * Save the current document to its existing file.
     * Falls back to saveFileAs if file doesn't exist.
     */
    void saveFile(Document* doc, std::function<void()> onSaved = nullptr);

    /**
     * Show save-as dialog and save to a new file with format options.
     */
    void saveFileAs(Document* doc, juce::Component* parent);

    /**
     * Close a document, prompting to save if modified.
     * @param onClosed Called after successful close (for UI refresh).
     */
    void closeFile(Document* doc, juce::Component* parent,
                   std::function<void()> onClosed = nullptr);

    /**
     * Check if any open document has unsaved changes.
     */
    bool hasUnsavedChanges() const;

    /**
     * Save all modified documents. Returns true if all saves succeeded.
     */
    bool saveAllModifiedDocuments();

    /**
     * Show a confirmation dialog for discarding changes.
     */
    bool confirmDiscardChanges();

    /**
     * Handle files dropped onto the application window.
     */
    void handleFileDrop(const juce::StringArray& files, juce::Component* parent);

    /**
     * Perform auto-save of all modified documents to temp location.
     * Called periodically by timer. Thread-safe.
     */
    void performAutoSave();

    /**
     * Set callback for UI refresh after file operations (e.g. repaint).
     */
    void setOnUIRefreshNeeded(std::function<void()> callback) { m_onUIRefreshNeeded = std::move(callback); }

    /**
     * Set callback to get the save-file-as fallback working from saveFile().
     * Needed because saveFile() may need to call saveFileAs() which requires a parent.
     */
    void setSaveAsParent(juce::Component* parent) { m_saveAsParent = parent; }

private:
    /**
     * Validates that a file path is safe (no path traversal attacks).
     */
    bool isPathSafe(const juce::File& file) const;

    /**
     * Cleans up old auto-save files, keeping only the most recent 3 per original file.
     */
    void cleanupOldAutoSaves(const juce::File& autoSaveDir);

    /** Trigger UI refresh if callback is set */
    void requestUIRefresh();

    //==========================================================================
    DocumentManager& m_documentManager;
    AudioFileManager& m_fileManager;
    juce::ApplicationCommandManager& m_commandManager;
    std::unique_ptr<juce::FileChooser> m_fileChooser;
    juce::ThreadPool m_autoSaveThreadPool {1};
    std::function<void()> m_onUIRefreshNeeded;
    juce::Component* m_saveAsParent = nullptr;

    //==========================================================================
    /** Background job for auto-saving to avoid blocking the message thread. */
    struct AutoSaveJob : public juce::ThreadPoolJob
    {
        juce::AudioBuffer<float> bufferCopy;
        juce::File targetFile;
        juce::File originalFile;
        double sampleRate;
        int bitDepth;

        AutoSaveJob(const juce::AudioBuffer<float>& buffer,
                    const juce::File& target,
                    const juce::File& original,
                    double rate,
                    int depth);

        JobStatus runJob() override;

    private:
        void logFailure(const juce::String& reason);
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FileController)
};
