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
     * Show the New File dialog and create a fresh empty document with the
     * chosen sample rate / channel count / duration. No-op if the user
     * cancels the dialog.
     */
    void createNewFile();

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

    //==========================================================================
    // Crash recovery (per-file).
    //
    // The auto-save mechanism only writes when a document is modified, so
    // an auto-save existing for a given original file means changes that
    // were never persisted by the user — exactly the recovery condition.

    /** The directory where auto-saves are written. */
    static juce::File getAutoSaveDirectory();

    /**
     * Return every auto-save file that targets @p originalFile and is
     * newer than the original on disk. Empty result = no recovery needed.
     */
    static juce::Array<juce::File> findOrphanedAutoSaves(const juce::File& originalFile);

    /**
     * Delete every auto-save file that targets @p originalFile. Called
     * after a successful save (the on-disk file is now the canonical
     * version) and after the user picks "Discard" in the recovery
     * dialog.
     */
    static void deleteAutoSavesFor(const juce::File& originalFile);

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

    /** Show the recovery dialog and act on the user's choice. */
    void offerCrashRecovery(Document* doc, const juce::File& originalFile);

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
