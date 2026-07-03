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
#include "../Utils/AutoSaveRecovery.h"
#include "../Utils/Settings.h"
#include "../UI/ErrorDialog.h"
#include "../UI/SaveAsOptionsPanel.h"
#include "../UI/NewFileDialog.h"
#include "../UI/WaveformDisplay.h"
#include "../UI/RegionDisplay.h"
#include "../UI/MarkerDisplay.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioBufferManager.h"

//==============================================================================
FileController::FileController(DocumentManager& docManager,
                               AudioFileManager& fileManager)
    : m_documentManager(docManager),
      m_fileManager(fileManager)
{
}

FileController::~FileController()
{
    m_autoSaveThreadPool.removeAllJobs(true, 5000);
}

void FileController::createNewFile()
{
    auto settings = NewFileDialog::showDialog();
    if (! settings.has_value()) return;

    auto* newDoc = m_documentManager.createDocument();
    if (newDoc == nullptr) return;

    const int64_t numSamples = static_cast<int64_t>(settings->durationSeconds * settings->sampleRate);
    juce::AudioBuffer<float> emptyBuffer(settings->numChannels, static_cast<int>(numSamples));
    emptyBuffer.clear();

    auto& buffer = newDoc->getBufferManager().getMutableBuffer();
    buffer.setSize(settings->numChannels, static_cast<int>(numSamples));
    buffer.clear();

    newDoc->getAudioEngine().loadFromBuffer(emptyBuffer, settings->sampleRate, settings->numChannels);
    newDoc->getWaveformDisplay().reloadFromBuffer(emptyBuffer, settings->sampleRate, false, false);

    newDoc->getRegionDisplay().setSampleRate(settings->sampleRate);
    newDoc->getRegionDisplay().setTotalDuration(settings->durationSeconds);
    newDoc->getRegionDisplay().setVisibleRange(0.0, settings->durationSeconds);
    newDoc->getRegionDisplay().setAudioBuffer(&buffer);

    newDoc->getMarkerDisplay().setSampleRate(settings->sampleRate);
    newDoc->getMarkerDisplay().setTotalDuration(settings->durationSeconds);

    newDoc->setModified(true);
    m_documentManager.setCurrentDocument(newDoc);
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

                        // Crash recovery: offer to restore an unsaved auto-save.
                        offerCrashRecovery(newDoc, file);
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

        // Crash recovery: offer to restore an unsaved auto-save.
        offerCrashRecovery(doc, file);
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
        // The on-disk file is now the canonical version — drop any
        // crash-recovery auto-saves for this file (they're superseded).
        deleteAutoSavesFor(currentFile);

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
    saveDocumentAs(doc);
}

//==============================================================================
bool FileController::saveDocumentAs(Document* doc)
{
    if (!doc) return false;

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
        return false;

    auto settings = result.value();

    DBG("FileController::saveDocumentAs - Saving as " + settings.format.toUpperCase() +
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

        // The on-disk file is now the canonical version -- drop any auto-saves
        // (including untitled recovery takes) now superseded by this save.
        deleteAutoSavesFor(settings.targetFile);

        requestUIRefresh();

        DBG("FileController::saveDocumentAs - Saved: " + settings.targetFile.getFullPathName());
        return true;
    }

    // Show error dialog
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::WarningIcon,
        "Save As Failed",
        "Could not save file as: " + settings.targetFile.getFileName() + "\n\n" +
        "Failed to write file. Check console for details.",
        "OK");
    return false;
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
    // Save all modified documents ahead of quitting.
    for (int i = 0; i < m_documentManager.getNumDocuments(); ++i)
    {
        auto* doc = m_documentManager.getDocument(i);
        if (!doc || !doc->isModified())
            continue;

        // Switch to this document's tab so the user sees what they are being
        // prompted about.
        m_documentManager.setCurrentDocumentIndex(i);

        auto currentFile = doc->getAudioEngine().getCurrentFile();
        if (currentFile.existsAsFile())
        {
            // Titled document: save in place.
            bool success = doc->saveFile(currentFile, doc->getBufferManager().getBitDepth());
            if (!success)
            {
                juce::Logger::writeToLog("FileController::saveAllModifiedDocuments - Failed to save: "
                                         + doc->getFilename());
                return false;  // Abort quit on first failure
            }

            // On-disk file is canonical now -- clear any superseded auto-saves.
            deleteAutoSavesFor(currentFile);
            continue;
        }

        // Untitled document (e.g. a fresh recording): previously skipped here,
        // silently discarding the take on quit (UX finding 2). Prompt the user
        // per document instead. Uses the same synchronous modal pattern as
        // closeFile()'s per-document close prompt.
        int choice = juce::AlertWindow::showYesNoCancelBox(
            juce::AlertWindow::WarningIcon,
            "Save Changes",
            "Save changes to \"" + doc->getFilename() + "\" before quitting?",
            "Save As...",
            "Discard",
            "Cancel",
            nullptr,
            nullptr);

        if (choice == 0)          // Cancel: abort the whole quit.
            return false;
        if (choice == 2)          // Discard: leave this take unsaved, keep going.
            continue;

        // Save As... -- run the modal chooser. A cancelled chooser or a failed
        // write must NOT silently drop the take, so treat it as "cancel quit".
        if (!saveDocumentAs(doc))
            return false;
    }

    return true;  // Every modified document saved or explicitly discarded.
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
        // Accept every format the Open dialog advertises (and that
        // AudioFileManager can read) -- previously only .wav was accepted,
        // so dropped FLAC/MP3/OGG were silently ignored (H26).
        if (file.existsAsFile() && isDroppableAudioFile(file))
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
bool FileController::isDroppableAudioFile(const juce::File& file)
{
    // Keep in sync with the Open dialog's filter ("*.wav;*.flac;*.mp3;*.ogg").
    return file.hasFileExtension("wav")
        || file.hasFileExtension("flac")
        || file.hasFileExtension("mp3")
        || file.hasFileExtension("ogg");
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

        // Nothing to recover if there's no audio yet.
        const auto& buffer = doc->getBufferManager().getBuffer();
        if (buffer.getNumSamples() <= 0)
            continue;

        // Key the auto-save on the document's source file when it has one.
        // UNTITLED documents (e.g. a fresh recording) have no file on disk and
        // used to be skipped entirely -- so a crash lost the take (UX finding
        // 2). Synthesize a stable per-document recovery identity so untitled
        // takes are auto-saved too. The token is derived from the document's
        // address: stable for the life of the document (successive auto-saves
        // therefore group + clean up together) and unique across concurrently
        // open untitled documents. The synthetic path never exists on disk, so
        // a recovery scan treats every such auto-save as newer than its
        // (absent) original and lists it.
        //
        // NOTE: offerCrashRecovery() only fires when a file is OPENED, and an
        // untitled take is never re-opened by path on the next launch. These
        // takes are instead surfaced by offerUntitledCrashRecovery(), a
        // launch-time scan the app host calls once after the window exists.
        juce::File originalFile = doc->getAudioEngine().getCurrentFile();
        if (!originalFile.existsAsFile())
        {
            const auto token = juce::String::toHexString(
                static_cast<juce::int64>(reinterpret_cast<juce::pointer_sized_int>(doc)));
            originalFile = autoSaveDir.getChildFile("Untitled-" + token + ".wav");
        }

        // Create auto-save filename:
        //   autosave_[stem]_[pathHash]_[timestamp].wav
        // The pathHash (from AutoSaveRecovery::autoSavePrefixFor) keeps two
        // same-named files in different directories from colliding (M4), and
        // gives cleanup a robust grouping token (M5).
        juce::String timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        juce::String autoSaveFilename = AutoSaveRecovery::autoSavePrefixFor(originalFile)
                                            + timestamp + ".wav";
        juce::File autoSaveFile = autoSaveDir.getChildFile(autoSaveFilename);

        // Get audio data (on message thread - safe)
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

    // Group files by their per-original prefix.
    //
    // Filename layout is: autosave_<stem>_<pathHash>_<YYYYMMDD>_<HHMMSS>.wav
    // The timestamp is always the LAST TWO underscore-separated tokens
    // (date, time). Everything before them is the stable group key
    // (prefix incl. pathHash). Splitting on '_' and taking parts[1] as the
    // stem mis-grouped stems containing underscores like "my_kick_01" (M5);
    // dropping the trailing timestamp tokens is robust to any stem.
    std::map<juce::String, juce::Array<juce::File>> filesByOriginal;
    for (auto& file : autoSaveFiles)
    {
        juce::String filename = file.getFileNameWithoutExtension();
        juce::StringArray parts = juce::StringArray::fromTokens(filename, "_", "");

        // Need at least: "autosave", stem, hash, date, time => 5 tokens.
        if (parts.size() < 5)
            continue;

        // Drop the last two tokens (date + time); rejoin the rest as the key.
        parts.removeRange(parts.size() - 2, 2);
        juce::String groupKey = parts.joinIntoString("_");
        filesByOriginal[groupKey].add(file);
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
        std::unique_ptr<juce::OutputStream> outputStream = targetFile.createOutputStream();
        if (!outputStream)
        {
            logFailure("Could not create output stream");
            return jobHasFinished;
        }

        // Ensure valid bit depth (WAV supports 8, 16, 24, 32)
        int safeBitDepth = bitDepth;
        if (safeBitDepth <= 0) safeBitDepth = 16;
        else if (safeBitDepth > 32) safeBitDepth = 32;

        // Create WAV writer (JUCE 8 API: takes the unique_ptr by reference
        // and only assumes ownership of the stream on success)
        juce::WavAudioFormat wavFormat;
        auto writer = wavFormat.createWriterFor(
            outputStream,
            juce::AudioFormatWriterOptions()
                .withSampleRate(sampleRate)
                .withNumChannels(bufferCopy.getNumChannels())
                .withBitsPerSample(safeBitDepth));

        if (!writer)
        {
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

//==============================================================================
// Crash recovery
//
// The static helpers (find/delete/dir) live in Source/Utils/AutoSaveRecovery
// — they're UI-free so unit tests can link them without dragging the rest
// of FileController's dialog dependencies. The FileController shim methods
// just forward, keeping the public API surface familiar.
//==============================================================================

juce::File FileController::getAutoSaveDirectory()
{
    return AutoSaveRecovery::getAutoSaveDirectory();
}

juce::Array<juce::File> FileController::findOrphanedAutoSaves(const juce::File& originalFile)
{
    return AutoSaveRecovery::findOrphanedAutoSaves(originalFile);
}

void FileController::deleteAutoSavesFor(const juce::File& originalFile)
{
    AutoSaveRecovery::deleteAutoSavesFor(originalFile);
}

juce::Array<juce::File> FileController::findUntitledAutoSaves()
{
    juce::Array<juce::File> result;

    auto dir = AutoSaveRecovery::getAutoSaveDirectory();
    if (!dir.isDirectory())
        return result;

    // Untitled takes are written as "autosave_Untitled-<token>_...wav" (see
    // performAutoSave). Match that prefix; there is no real source file to key
    // on, so every match is a candidate recovery take.
    dir.findChildFiles(result, juce::File::findFiles, false, "autosave_Untitled-*.wav");

    // Newest first, so callers can offer the latest take.
    struct ByMTimeDesc
    {
        static int compareElements(const juce::File& a, const juce::File& b)
        {
            const auto ta = a.getLastModificationTime();
            const auto tb = b.getLastModificationTime();
            if (ta > tb) return -1;
            if (ta < tb) return 1;
            return 0;
        }
    };
    ByMTimeDesc cmp;
    result.sort(cmp, false);
    return result;
}

//==============================================================================
// Load one untitled auto-save take into a brand-new document tab. Mirrors the
// document setup in createNewFile() (buffer + engine + waveform/region/marker
// displays) but seeds it from the recovered audio. Reads the take's own sample
// rate/channel count from its header (getFileInfo) -- there is no source
// document to inherit them from. Returns true if a tab was created.
//
// Free function (not capturing FileController::this) so the async recovery
// callback stays valid even if the controller's owner is torn down mid-dialog,
// matching offerCrashRecovery()'s capture discipline.
static bool recoverUntitledTakeInto(DocumentManager& docManager,
                                    AudioFileManager& fileMgr,
                                    const juce::File& take)
{
    AudioFileInfo info;
    if (!fileMgr.getFileInfo(take, info) || info.sampleRate <= 0.0)
    {
        juce::Logger::writeToLog("FileController::offerUntitledCrashRecovery - could not read "
                                 + take.getFullPathName() + " -- keeping it on disk");
        return false;
    }

    juce::AudioBuffer<float> recovered;
    if (!fileMgr.loadIntoBufferUnchecked(take, recovered) || recovered.getNumSamples() <= 0)
    {
        juce::Logger::writeToLog("FileController::offerUntitledCrashRecovery - empty/failed load "
                                 + take.getFullPathName() + " -- keeping it on disk");
        return false;
    }

    auto* newDoc = docManager.createDocument();
    if (newDoc == nullptr)
        return false;

    const double sampleRate    = info.sampleRate;
    const int    numChannels   = recovered.getNumChannels();
    const double durationSecs  = recovered.getNumSamples() / sampleRate;

    auto& buffer = newDoc->getBufferManager().getMutableBuffer();
    buffer = recovered;

    newDoc->getAudioEngine().loadFromBuffer(recovered, sampleRate, numChannels);
    newDoc->getWaveformDisplay().reloadFromBuffer(recovered, sampleRate, false, false);

    newDoc->getRegionDisplay().setSampleRate(sampleRate);
    newDoc->getRegionDisplay().setTotalDuration(durationSecs);
    newDoc->getRegionDisplay().setVisibleRange(0.0, durationSecs);
    newDoc->getRegionDisplay().setAudioBuffer(&buffer);

    newDoc->getMarkerDisplay().setSampleRate(sampleRate);
    newDoc->getMarkerDisplay().setTotalDuration(durationSecs);

    // Recovered but never committed to a real file: mark modified so the
    // quit-time untitled-save prompt (saveAllModifiedDocuments) covers it.
    newDoc->setModified(true);
    docManager.setCurrentDocument(newDoc);
    return true;
}

void FileController::offerUntitledCrashRecovery()
{
    auto takes = findUntitledAutoSaves();
    if (takes.isEmpty())
        return;

    const auto& newest = takes.getReference(0);
    const auto when     = newest.getLastModificationTime().toString(true, true, true, true);

    juce::String body;
    body << "WaveEdit found " << takes.size() << " unsaved recording"
         << (takes.size() == 1 ? "" : "s") << " from a previous session.\n\n"
         << "The most recent is from " << when << ".\n\n"
         << "Recover into new tabs? You can then Save As to keep them.";

    auto opts = juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::QuestionIcon)
                    .withTitle("Recover Unsaved Recordings?")
                    .withMessage(body)
                    .withButton("Recover")
                    .withButton("Discard")
                    .withButton("Keep & Continue");

    auto* docManager = &m_documentManager;
    auto* fileMgr    = &m_fileManager;
    auto takesCopy   = takes;
    auto refreshUI   = m_onUIRefreshNeeded;

    juce::AlertWindow::showAsync(opts,
        [docManager, fileMgr, takesCopy, refreshUI](int result)
        {
            // 1 = Recover, 2 = Discard, anything else = Keep & Continue.
            if (result == 2)
            {
                for (const auto& f : takesCopy)
                    f.deleteFile();
                return;
            }
            if (result != 1)
                return;  // Keep & Continue: leave takes on disk for next launch.

            for (const auto& take : takesCopy)
            {
                // Only delete a take once it is safely loaded into a tab, so a
                // failed recovery keeps the backup for a later attempt.
                if (recoverUntitledTakeInto(*docManager, *fileMgr, take))
                    take.deleteFile();
            }

            if (refreshUI)
                refreshUI();
        });
}

void FileController::offerCrashRecovery(Document* doc, const juce::File& originalFile)
{
    if (doc == nullptr)
        return;

    auto orphans = AutoSaveRecovery::findOrphanedAutoSaves(originalFile);
    if (orphans.isEmpty())
        return;

    const auto& newest = orphans.getReference(0);
    const auto when    = newest.getLastModificationTime().toString(true, true, true, true);

    juce::String body;
    body << "WaveEdit found unsaved changes for \"" << originalFile.getFileName() << "\".\n\n"
         << "The most recent auto-save is from " << when << ".\n\n"
         << "What would you like to do?";

    auto opts = juce::MessageBoxOptions()
                    .withIconType(juce::MessageBoxIconType::QuestionIcon)
                    .withTitle("Recover Unsaved Changes?")
                    .withMessage(body)
                    .withButton("Recover")
                    .withButton("Discard")
                    .withButton("Keep & Continue");

    auto* docManager = &m_documentManager;
    auto* fileMgr    = &m_fileManager;
    auto orphanList  = orphans;
    auto refreshUI   = m_onUIRefreshNeeded;
    Document* docPtr = doc;

    juce::AlertWindow::showAsync(opts,
        [docManager, fileMgr, orphanList, refreshUI, docPtr](int result)
        {
            // result 1 = Recover, 2 = Discard, 3 = Keep & Continue.
            // Anything else (including the user closing via the
            // window's close button) we treat as Keep & Continue.
            if (result == 2)
            {
                for (const auto& f : orphanList)
                    f.deleteFile();
                return;
            }
            if (result != 1)
                return;  // Keep & Continue: do nothing.

            // Verify the document still exists in the manager — the
            // user may have closed it while the modal was open. We
            // use pointer-equality against the manager's owned list
            // since Document doesn't expose a stable identity.
            bool stillOpen = false;
            for (int i = 0; i < docManager->getNumDocuments(); ++i)
            {
                if (docManager->getDocument(i) == docPtr)
                {
                    stillOpen = true;
                    break;
                }
            }
            if (! stillOpen)
                return;

            const auto& newestAS = orphanList.getReference(0);

            // Load the auto-save's audio into a fresh buffer. Use the
            // unchecked loader so a nonstandard-sample-rate auto-save the
            // app itself wrote isn't rejected by the open-validation
            // whitelist (M6).
            juce::AudioBuffer<float> recovered;
            if (! fileMgr->loadIntoBufferUnchecked(newestAS, recovered))
            {
                // Keep the backups: the user may still want to recover by
                // other means, and we have not yet replaced anything.
                juce::Logger::writeToLog(
                    "Crash recovery: failed to load " + newestAS.getFullPathName()
                    + " -- keeping auto-saves.");
                ErrorDialog::show(
                    "Recovery Failed",
                    "Could not read the auto-saved file. Your backups have been kept.",
                    ErrorDialog::Severity::Error);
                return;
            }

            // Guard against a truncated / zero-sample recovery: loading can
            // "succeed" on a 0-sample file, which would silently replace the
            // user's audio with silence AND delete the backups (M6). Validate
            // before touching anything.
            if (recovered.getNumSamples() <= 0)
            {
                juce::Logger::writeToLog(
                    "Crash recovery: recovered buffer is empty for "
                    + newestAS.getFullPathName() + " -- keeping auto-saves.");
                ErrorDialog::show(
                    "Recovery Failed",
                    "The auto-saved file contained no audio. Your backups have been kept.",
                    ErrorDialog::Severity::Error);
                return;
            }

            // Replace the document's buffer in place. Mark it modified
            // so the user must Save (or Save As) to commit, and reload
            // the audio engine so playback uses the recovered audio.
            const double sr = docPtr->getAudioEngine().getSampleRate();
            docPtr->getBufferManager().setBuffer(recovered, sr);
            docPtr->getAudioEngine().reloadBufferPreservingPlayback(
                docPtr->getBufferManager().getBuffer(),
                sr,
                docPtr->getBufferManager().getBuffer().getNumChannels());
            docPtr->setModified(true);

            // Only now that recovery succeeded, discard the consumed
            // auto-saves.
            for (const auto& f : orphanList)
                f.deleteFile();

            if (refreshUI)
                refreshUI();
        });
}
