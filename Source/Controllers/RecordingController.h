/*
  ==============================================================================

    RecordingController.h - Recording workflow controller
    Part of WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Per CLAUDE.md §8.1: recording workflow / RecordingDialog::Listener
    inline class belongs out of Main.cpp.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <atomic>
#include <functional>
#include <memory>

class DocumentManager;
class Document;

/**
 * RecordingController owns the recording flow: prompts for a destination
 * (insert at cursor of the current document, or create a new document),
 * launches the RecordingDialog, and applies the captured buffer when the
 * dialog completes.
 */
class RecordingController
{
public:
    RecordingController() = default;
    ~RecordingController() = default;

    /**
     * Entry point for the Record command.
     *
     * If a document is open, asks the user whether to insert at cursor
     * or create a new document. With no document open, always creates
     * a new one. Then opens the RecordingDialog with the appropriate
     * listener.
     *
     * @param parent            Component used as dialog parent
     * @param documentManager   The application's document manager
     * @param audioDeviceManager Device manager that the dialog will record from
     * @param recordingStateCallback Optional; forwarded to the dialog and
     *        called with the live capture state so the host can mirror it on a
     *        persistent transport indicator.
     */
    void handleRecordCommand(juce::Component* parent,
                             DocumentManager& documentManager,
                             juce::AudioDeviceManager& audioDeviceManager,
                             std::function<void(bool)> recordingStateCallback = {});

    /**
     * Inserts a recorded take into an existing document as one undoable
     * step, exposed as a static so it can be exercised headlessly by tests
     * (no RecordingDialog/UI involved).
     *
     * If the document has no audio yet, this defers to populateNewDocument()
     * instead of inserting into an empty buffer.
     *
     * Otherwise the take is resampled to the document's own sample rate
     * (AudioBufferManager::getSampleRate(), NOT the engine's rate) when the
     * rates differ by more than 0.01 Hz, and its channel count is conformed
     * to the document's via ClipboardController::conformChannels(). If the
     * channel counts cannot be reconciled (e.g. 3-channel take into a
     * stereo file), the document is left completely unmodified, `error` is
     * set to a user-facing message, and false is returned.
     *
     * On success the insert is applied via a single InsertAction pushed
     * through the document's UndoManager (same mechanism as the Generate
     * placement feature), so regions/markers shift and one undo restores
     * the document exactly.
     *
     * @param doc            Target document (must already exist).
     * @param take           The recorded audio.
     * @param takeSampleRate The device/take's sample rate.
     * @param insertSeconds  Where to insert, in seconds (edit cursor or
     *                       playback position -- chosen by the caller).
     * @param error          Set to a user-facing message on failure.
     * @return true on success, false if the take could not be inserted.
     */
    static bool insertTake(Document& doc,
                           const juce::AudioBuffer<float>& take,
                           double takeSampleRate,
                           double insertSeconds,
                           juce::String& error);

    /**
     * Populates a brand-new, empty Document with a recorded take: records
     * the take's own sample rate on the AudioBufferManager (via setBuffer(),
     * not getMutableBuffer(), so the rate isn't silently left at the 44.1kHz
     * default) and sets up the engine/waveform/region/marker displays to
     * match.
     */
    static void populateNewDocument(Document& doc,
                                    const juce::AudioBuffer<float>& take,
                                    double sampleRate);

    /** True while a take is being captured (between the dialog's record
        start and stop/close). Editing commands are disabled meanwhile. */
    bool isRecording() const { return m_recordingState->load(); }

private:
    // Shared with the dialog's state callback, which can outlive this
    // controller if the main window closes while the dialog is still open.
    std::shared_ptr<std::atomic<bool>> m_recordingState =
        std::make_shared<std::atomic<bool>>(false);
};
