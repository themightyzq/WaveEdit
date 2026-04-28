/*
  ==============================================================================

    FileController.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "FileController.h"
#include "../Utils/Settings.h"
#include "../UI/ErrorDialog.h"
#include "../UI/SaveAsOptionsPanel.h"

//==============================================================================
FileController::FileController(DocumentManager& docManager,
                               AudioFileManager& fileManager,
                               juce::ApplicationCommandManager& cmdManager)
    : m_documentManager(docManager),
      m_fileManager(fileManager),
      m_commandManager(cmdManager)
{
}

FileController::~FileController()
{
    m_autoSaveThreadPool.removeAllJobs(true, 5000);
}

//==============================================================================
void FileController::openFile(juce::Component* /*parent*/)
{
    // Don't create new file chooser if one is already active
    if (m_fileChooser != nullptr)
    {
        juce::Logger::writeToLog("FileController::openFile - File chooser already active");
        return;
    }

    // Create file chooser - supports multiple file selection
    // Support WAV, FLAC, MP3, and OGG formats
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Open Audio File(s)",
        Settings::getInstance().getLastFileDirectory(),
        "*.wav;*.flac;*.mp3;*.ogg",
        true);

    auto folderChooserFlags = juce::FileBrowserComponent::openMode |
                              juce::FileBrowserComponent::canSelectFiles |
                              juce::FileBrowserComponent::canSelectMultipleItems;

    m_fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& chooser)
    {
        auto files = chooser.getResults();

        if (!files.isEmpty())
        {
            // Open each selected file in its own document tab
            for (const auto& file : files)
            {
                if (file != juce::File())
                {
                    // Remember the directory for next time
                    Settings::getInstance().setLastFileDirectory(file.getParentDirectory());

                    // Use DocumentManager to open files (supports multiple tabs)
                    auto* newDoc = m_documentManager.openDocument(file);
                    if (newDoc)
                    {
                        // Add to recent files
                        Settings::getInstance().addRecentFile(file);

                        DBG("FileController::openFile - Opened: " + file.getFileName());
                    }
                    else
                    {
                        // Show user-friendly error dialog
                        ErrorDialog::showFileError(
                            "open",
                            file.getFullPathName(),
                            "Unsupported format or corrupted data"
                        );
                    }
                }
            }
        }

        // CRITICAL: Always clear the file chooser after use, regardless of outcome
        m_fileChooser.reset();
    });
}

//==============================================================================
bool FileController::isPathSafe(const juce::File& file) const
{
    // Get normalized absolute path
    auto path = file.getFullPathName();

    // Check for path traversal attempts
    if (path.contains(".."))
    {
        juce::Logger::writeToLog("FileController::isPathSafe - Path traversal attempt detected: " + path);
        return false;
    }

    // File must exist
    if (!file.existsAsFile())
    {
        return false;
    }

    return true;
}

//==============================================================================
void FileController::loadFile(const juce::File& file, juce::Component* /*parent*/)
{
    // Validate path safety
    if (!isPathSafe(file))
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Security Error",
            "Invalid or unsafe file path: " + file.getFileName(),
            "OK");
        return;
    }

    // Validate the file first
    AudioFileInfo info;
    if (!m_fileManager.getFileInfo(file, info))
    {
        ErrorDialog::showFileError(
            "open",
            file.getFullPathName(),
            m_fileManager.getLastError()
        );
        return;
    }

    // Check file permissions
    if (!file.hasReadAccess())
    {
        ErrorDialog::showFileError(
            "open",
            file.getFullPathName(),
            "No read permission for this file"
        );
        return;
    }

    // Remember the directory for next time
    Settings::getInstance().setLastFileDirectory(file.getParentDirectory());

    // Use DocumentManager to open the file
    auto* doc = m_documentManager.openDocument(file);
    if (doc != nullptr)
    {
        // Add to recent files
        Settings::getInstance().addRecentFile(file);

        // Document is now active, UI will update via listener
        DBG("FileController::loadFile - Opened: " + file.getFileName());
    }
    else
    {
        ErrorDialog::showFileError(
            "open",
            file.getFullPathName(),
            "Could not load audio data from this file"
        );
    }
}

//==============================================================================
void FileController::saveFile(Document* doc, std::function<void()> onSaved)
{
    if (!doc || !doc->getAudioEngine().isFileLoaded())
    {
        return;
    }

    auto currentFile = doc->getAudioEngine().getCurrentFile();

    // Check if file exists and is writable
    if (!currentFile.existsAsFile())
    {
        // File doesn't exist, do Save As instead
        if (m_saveAsParent != nullptr)
            saveFileAs(doc, m_saveAsParent);
        return;
    }

    if (!currentFile.hasWriteAccess())
    {
        juce::String message = "No write permission for this file.";
        message += "\n\nUse 'Save As' to save to a different location.";
        ErrorDialog::show("Permission Error", message, ErrorDialog::Severity::Error);
        return;
    }

    // Save using Document::saveFile() which includes BWF and iXML metadata
    bool saveSuccess = doc->saveFile(currentFile, doc->getBufferManager().getBitDepth());

    if (saveSuccess)
    {
        requestUIRefresh();
        DBG("FileController::saveFile - Saved: " + currentFile.getFullPathName());

        if (onSaved)
            onSaved();
    }
    else
    {
        // Show error dialog
        ErrorDialog::showWithDetails(
            "Save Failed",
            "Could not save file: " + currentFile.getFileName(),
            "Failed to write file with metadata",
            ErrorDialog::Severity::Error
        );
    }
}

//==============================================================================
void FileController::saveFileAs(Document* doc, juce::Component* /*parent*/)
{
    if (!doc) return;

    // Get current file, or provide default for unsaved documents
    juce::File currentFile = doc->getAudioEngine().getCurrentFile();
    if (!currentFile.existsAsFile())
    {
        // Provide sensible default: Documents/Untitled.wav
        currentFile = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                           .getChildFile("Untitled.wav");
    }

    // Show Save As dialog to get format and encoding settings
    double sourceSampleRate = doc->getAudioEngine().getSampleRate();
    int sourceChannels = doc->getBufferManager().getBuffer().getNumChannels();

    auto result = SaveAsOptionsPanel::showDialog(sourceSampleRate, sourceChannels, currentFile);

    // If user cancelled, exit
    if (!result.has_value())
        return;

    auto settings = result.value();

    DBG("FileController::saveFileAs - Saving as " + settings.format.toUpperCase() +
                             " - Bit depth: " + juce::String(settings.bitDepth) +
                             ", Quality: " + juce::String(settings.quality) +
                             ", Sample rate: " + juce::String(settings.targetSampleRate > 0.0 ? settings.targetSampleRate : sourceSampleRate, 0) + " Hz");

    // Save using Document::saveFile() with all settings
    bool saveSuccess = doc->saveFile(settings.targetFile, settings.bitDepth, settings.quality, settings.targetSampleRate);

    if (saveSuccess)
    {
        // Remember the directory for next time
        Settings::getInstance().setLastFileDirectory(settings.targetFile.getParentDirectory());

        // Add to recent files
        Settings::getInstance().addRecentFile(settings.targetFile);

        requestUIRefresh();

        DBG("FileController::saveFileAs - Saved: " + settings.targetFile.getFullPathName());
    }
    else
    {
        // Show error dialog
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Save As Failed",
            "Could not save file as: " + settings.targetFile.getFileName() + "\n\n" +
            "Failed to write file. Check console for details.",
            "OK");
    }
}

//==============================================================================
bool FileController::hasUnsavedChanges() const
{
    // Check if any document has unsaved changes
    for (int i = 0; i < m_documentManager.getNumDocuments(); ++i)
    {
        if (auto* doc = m_documentManager.getDocument(i))
        {
            if (doc->isModified())
            {
                return true;
            }
        }
    }
    return false;
}

//==============================================================================
bool FileController::saveAllModifiedDocuments()
{
    // Save all modified documents
    for (int i = 0; i < m_documentManager.getNumDocuments(); ++i)
    {
        if (auto* doc = m_documentManager.getDocument(i))
        {
            if (doc->isModified())
            {
                // Switch to this document's tab
                m_documentManager.setCurrentDocumentIndex(i);

                // Save it
                auto currentFile = doc->getAudioEngine().getCurrentFile();
                if (!currentFile.existsAsFile())
                {
                    // File doesn't exist, would need Save As - abort for now
                    DBG("FileController::saveAllModifiedDocuments - Cannot auto-save untitled document - skipping");
                    continue;
                }

                bool success = doc->saveFile(currentFile, doc->getBufferManager().getBitDepth());
                if (!success)
                {
                    juce::Logger::writeToLog("FileController::saveAllModifiedDocuments - Failed to save: " + doc->getFilename());
                    return false;  // Abort on first failure
                }
            }
        }
    }
    return true;  // All saves successful
}

//==============================================================================
void FileController::closeFile(Document* doc, juce::Component* /*parent*/,
                               std::function<void()> onClosed)
{
    if (!doc) return;

    if (!doc->getAudioEngine().isFileLoaded())
    {
        return;
    }

    // Check if document has unsaved changes
    if (doc->isModified())
    {
        auto result = juce::AlertWindow::showYesNoCancelBox(
            juce::AlertWindow::WarningIcon,
            "Unsaved Changes",
            "Do you want to save changes to \"" + doc->getFilename() + "\" before closing?",
            "Save",
            "Don't Save",
            "Cancel",
            nullptr,
            nullptr);

        if (result == 0) // Cancel
        {
            return;
        }
        else if (result == 1) // Save
        {
            saveFile(doc);  // Save before closing
            if (doc->isModified())  // If save failed or was cancelled
            {
                return;  // Don't close
            }
        }
        // result == 2 means "Don't Save" - proceed with close
    }

    // Close the document (which removes the tab and cleans up)
    m_documentManager.closeDocument(doc);
    requestUIRefresh();

    if (onClosed)
        onClosed();
}

//==============================================================================
bool FileController::confirmDiscardChanges()
{
    return juce::NativeMessageBox::showOkCancelBox(
        juce::MessageBoxIconType::WarningIcon,
        "Unsaved Changes",
        "The current file has unsaved changes.\n\n"
        "Do you want to discard these changes?",
        nullptr,
        nullptr);
}

//==============================================================================
void FileController::handleFileDrop(const juce::StringArray& files, juce::Component* /*parent*/)
{
    if (files.isEmpty())
    {
        return;
    }

    // Open each dropped file in its own tab
    for (const auto& filePath : files)
    {
        juce::File file(filePath);
        if (file.existsAsFile() && file.hasFileExtension(".wav"))
        {
            // Remember the directory for next time
            Settings::getInstance().setLastFileDirectory(file.getParentDirectory());

            auto* newDoc = m_documentManager.openDocument(file);
            if (newDoc)
            {
                // Add to recent files
                Settings::getInstance().addRecentFile(file);

                DBG("FileController::handleFileDrop - Opened: " + file.getFileName());
            }
        }
    }
}

//==============================================================================
void FileController::performAutoSave()
{
    // Check if auto-save is enabled (thread-safe read from Settings singleton)
    bool autoSaveEnabled = Settings::getInstance().getSetting("autoSave.enabled", true);
    if (!autoSaveEnabled)
        return;

    // Get auto-save directory
    juce::File autoSaveDir = Settings::getInstance().getSettingsDirectory().getChildFile("autosave");
    if (!autoSaveDir.exists())
        autoSaveDir.createDirectory();

    // Check each document for auto-save
    for (int i = 0; i < m_documentManager.getNumDocuments(); ++i)
    {
        auto* doc = m_documentManager.getDocument(i);
        if (!doc || !doc->isModified())
            continue;  // Skip unmodified documents

        // Check if document has a file (skip untitled documents for now)
        if (!doc->getAudioEngine().isFileLoaded())
            continue;

        // Create auto-save filename: autosave_[originalname]_[timestamp].wav
        juce::File originalFile = doc->getAudioEngine().getCurrentFile();
        juce::String timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        juce::String autoSaveFilename = "autosave_" + originalFile.getFileNameWithoutExtension() + "_" + timestamp + ".wav";
        juce::File autoSaveFile = autoSaveDir.getChildFile(autoSaveFilename);

        // Get audio data (on message thread - safe)
        const auto& buffer = doc->getBufferManager().getBuffer();
        double sampleRate = doc->getAudioEngine().getSampleRate();
        int bitDepth = doc->getAudioEngine().getBitDepth();

        // Create auto-save job (makes buffer copy for thread safety)
        auto* job = new AutoSaveJob(buffer, autoSaveFile, originalFile, sampleRate, bitDepth);

        // Add job to thread pool (will run on background thread)
        m_autoSaveThreadPool.addJob(job, true);  // deleteJobWhenFinished = true
    }

    // Clean up old auto-save files (fast operation, OK on message thread)
    cleanupOldAutoSaves(autoSaveDir);
}

//==============================================================================
void FileController::cleanupOldAutoSaves(const juce::File& autoSaveDir)
{
    if (!autoSaveDir.exists())
        return;

    // Get all auto-save files
    juce::Array<juce::File> autoSaveFiles;
    autoSaveDir.findChildFiles(autoSaveFiles, juce::File::findFiles, false, "autosave_*.wav");

    // Group files by original name
    std::map<juce::String, juce::Array<juce::File>> filesByOriginal;
    for (auto& file : autoSaveFiles)
    {
        // Extract original name from autosave_[originalname]_[timestamp].wav
        juce::String filename = file.getFileNameWithoutExtension();
        juce::StringArray parts = juce::StringArray::fromTokens(filename, "_", "");
        if (parts.size() >= 2)
        {
            juce::String originalName = parts[1];
            filesByOriginal[originalName].add(file);
        }
    }

    // For each original file, keep only the newest 3 auto-saves
    for (auto& pair : filesByOriginal)
    {
        auto& files = pair.second;
        if (files.size() <= 3)
            continue;

        // Sort by modification time (newest first)
        struct FileComparator
        {
            static int compareElements(const juce::File& first, const juce::File& second)
            {
                auto timeA = first.getLastModificationTime();
                auto timeB = second.getLastModificationTime();
                if (timeA > timeB) return -1;
                if (timeA < timeB) return 1;
                return 0;
            }
        };
        FileComparator comp;
        files.sort(comp, false);

        // Delete all but the newest 3
        for (int i = 3; i < files.size(); ++i)
        {
            files[i].deleteFile();
            juce::Logger::writeToLog("FileController::cleanupOldAutoSaves - Deleted: " + files[i].getFullPathName());
        }
    }
}

//==============================================================================
void FileController::requestUIRefresh()
{
    if (m_onUIRefreshNeeded)
        m_onUIRefreshNeeded();
}

//==============================================================================
// AutoSaveJob implementation
//==============================================================================

FileController::AutoSaveJob::AutoSaveJob(const juce::AudioBuffer<float>& buffer,
                                         const juce::File& target,
                                         const juce::File& original,
                                         double rate,
                                         int depth)
    : juce::ThreadPoolJob("AutoSave"),
      targetFile(target),
      originalFile(original),
      sampleRate(rate),
      bitDepth(depth)
{
    // Make a copy of the buffer for thread safety
    bufferCopy.makeCopyOf(buffer);
}

juce::ThreadPoolJob::JobStatus FileController::AutoSaveJob::runJob()
{
    try
    {
        // Create output stream
        auto outputStream = targetFile.createOutputStream();
        if (!outputStream)
        {
            logFailure("Could not create output stream");
            return jobHasFinished;
        }

        // Ensure valid bit depth (WAV supports 8, 16, 24, 32)
        int safeBitDepth = bitDepth;
        if (safeBitDepth <= 0) safeBitDepth = 16;
        else if (safeBitDepth > 32) safeBitDepth = 32;

        // IMPORTANT: Release ownership BEFORE createWriterFor
        // JUCE's createWriterFor may delete the stream on failure,
        // which would cause double-free if unique_ptr also tries to delete
        auto* rawStream = outputStream.release();

        // Create WAV writer
        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
            rawStream,
            sampleRate,
            static_cast<unsigned int>(bufferCopy.getNumChannels()),
            safeBitDepth,
            {},
            0));

        if (!writer)
        {
            // Note: stream was deleted by createWriterFor on failure
            logFailure("Could not create audio writer");
            return jobHasFinished;
        }

        // Write buffer to file
        bool success = writer->writeFromAudioSampleBuffer(bufferCopy, 0, bufferCopy.getNumSamples());
        writer.reset();  // Flush and close

        if (success)
        {
            // Log success on message thread
            juce::MessageManager::callAsync([file = targetFile]()
            {
                juce::Logger::writeToLog("Auto-saved: " + file.getFullPathName());
            });
        }
        else
        {
            logFailure("Write operation failed");
        }
    }
    catch (const std::exception& e)
    {
        logFailure(juce::String("Exception: ") + e.what());
    }
    catch (...)
    {
        logFailure("Unknown exception");
    }

    return jobHasFinished;
}

void FileController::AutoSaveJob::logFailure(const juce::String& reason)
{
    juce::MessageManager::callAsync([file = originalFile, reason]()
    {
        juce::Logger::writeToLog("Auto-save failed for " + file.getFullPathName() + ": " + reason);
    });
}
