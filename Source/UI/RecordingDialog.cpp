/*
  ==============================================================================

    RecordingDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "RecordingDialog.h"
#include "ThemeManager.h"
#include "UIConstants.h"

namespace
{
    //==========================================================================
    // VU-meter domain-visualisation colours (§6.11 carve-out). These are the
    // meter's green/yellow/red bar segments and clip latch — intentionally
    // specific, kept here as named constants rather than scattered literals.
    const juce::Colour kMeterGreen  { 0xff2ecc40 };  // safe zone
    const juce::Colour kMeterYellow { 0xffffdc00 };  // caution zone
    const juce::Colour kMeterRed    { 0xffff4136 };  // hot zone
    const juce::Colour kMeterClip   { 0xffff0000 };  // latched clip segment

    // dBFS scale and standard colour breakpoints.
    constexpr float kMeterFloorDb    = -60.0f;  // left edge of the meter
    constexpr float kMeterYellowDb   = -18.0f;  // green -> yellow
    constexpr float kMeterRedDb      =  -6.0f;  // yellow -> red
    constexpr int   kClipSegmentPx   = 6;       // width of the clip latch
}

//==============================================================================
// LevelMeter Implementation

RecordingDialog::LevelMeter::LevelMeter()
    : m_level(0.0f)
{
}

void RecordingDialog::LevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Chrome (background + border) from the active theme (§6.11 / M5).
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();
    g.setColour(theme.panelAlternate);
    g.fillRect(bounds);

    // Map the linear peak to dBFS, then to a 0..1 fill across [-60 dB .. 0 dB].
    const float db = juce::Decibels::gainToDecibels(m_level, kMeterFloorDb);
    const float norm = juce::jlimit(0.0f, 1.0f,
        juce::jmap(db, kMeterFloorDb, 0.0f, 0.0f, 1.0f));

    const float levelWidth = bounds.getWidth() * norm;
    juce::Rectangle<float> levelBar(bounds.getX(), bounds.getY(), levelWidth, bounds.getHeight());

    // Colour the fill by the zone the current peak sits in.
    if (db < kMeterYellowDb)
        g.setColour(kMeterGreen);
    else if (db < kMeterRedDb)
        g.setColour(kMeterYellow);
    else
        g.setColour(kMeterRed);

    g.fillRect(levelBar);

    // Latching clip indicator: a red segment pinned to the right edge that
    // stays lit after any >= 0 dBFS peak until cleared (click / record start).
    if (m_clipped)
    {
        g.setColour(kMeterClip);
        g.fillRect(bounds.removeFromRight(static_cast<float>(kClipSegmentPx)));
    }

    // Border from the active theme.
    g.setColour(theme.border);
    g.drawRect(getLocalBounds().toFloat(), 1.0f);
}

void RecordingDialog::LevelMeter::setLevel(float linearPeak)
{
    // Detect clip BEFORE clamping: >= 0 dBFS means linear peak >= 1.0.
    if (linearPeak >= 1.0f)
        m_clipped = true;

    m_level = juce::jlimit(0.0f, 1.0f, linearPeak);
    repaint();
}

void RecordingDialog::LevelMeter::resetClip()
{
    m_clipped = false;
    repaint();
}

void RecordingDialog::LevelMeter::mouseDown(const juce::MouseEvent&)
{
    resetClip();
}

//==============================================================================
// RecordingDialog Implementation

RecordingDialog::RecordingDialog(juce::AudioDeviceManager& deviceManager)
    : m_deviceManager(deviceManager),
      m_recordingEngine(std::make_unique<RecordingEngine>())
{
    // Input device selection
    m_inputDeviceLabel.setText("Input Device:", juce::dontSendNotification);
    addAndMakeVisible(m_inputDeviceLabel);

    m_inputDeviceSelector.addListener(this);
    addAndMakeVisible(m_inputDeviceSelector);
    populateInputDevices();
    m_currentInputDeviceId = m_inputDeviceSelector.getSelectedId();

    // Sample rate selection
    m_sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
    addAndMakeVisible(m_sampleRateLabel);

    m_sampleRateSelector.addListener(this);
    addAndMakeVisible(m_sampleRateSelector);
    populateSampleRates();
    m_currentSampleRateId = m_sampleRateSelector.getSelectedId();

    // Channel configuration
    m_channelConfigLabel.setText("Channels:", juce::dontSendNotification);
    addAndMakeVisible(m_channelConfigLabel);

    m_channelConfigSelector.addListener(this);
    addAndMakeVisible(m_channelConfigSelector);
    populateChannelConfigurations();

    // Recording controls
    m_recordButton.setButtonText("Record");
    m_recordButton.addListener(this);
    m_recordButton.setColour(juce::TextButton::buttonColourId,
        waveedit::ThemeManager::getInstance().getCurrent().error);
    addAndMakeVisible(m_recordButton);

    m_stopButton.setButtonText("Stop");
    m_stopButton.addListener(this);
    m_stopButton.setEnabled(false);
    addAndMakeVisible(m_stopButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.addListener(this);
    addAndMakeVisible(m_cancelButton);

    // Status display
    m_statusLabel.setText("Ready to record", juce::dontSendNotification);
    m_statusLabel.setFont(waveedit::ui::legacyFont(16.0f, juce::Font::bold));
    m_statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_statusLabel);

    m_elapsedTimeLabel.setText("00:00.000", juce::dontSendNotification);
    m_elapsedTimeLabel.setFont(waveedit::ui::legacyFont(24.0f, juce::Font::bold));
    m_elapsedTimeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_elapsedTimeLabel);

    // Level labels
    m_leftLevelLabel.setText("L:", juce::dontSendNotification);
    addAndMakeVisible(m_leftLevelLabel);

    m_rightLevelLabel.setText("R:", juce::dontSendNotification);
    addAndMakeVisible(m_rightLevelLabel);

    // Level meters
    addAndMakeVisible(m_leftLevelMeter);
    addAndMakeVisible(m_rightLevelMeter);

    // Sync the engine with the default channel choice and set the right
    // meter/label visibility. Done AFTER the meters are added so its
    // setVisible() isn't clobbered by the addAndMakeVisible() calls above.
    applyChannelSelection();

    // Listen to recording engine state changes
    m_recordingEngine->addChangeListener(this);

    // This dialog can stay open for the duration of a recording session
    // (up to the 1-hour buffer cap), so cached colours below need to be
    // re-applied on a live theme switch, not just read once at construction.
    waveedit::ThemeManager::getInstance().addChangeListener(this);

    // Add audio callback for input monitoring IMMEDIATELY
    // This allows level meters to show input before recording starts
    m_deviceManager.addAudioCallback(m_recordingEngine.get());

    // Start timer for UI updates (30 FPS)
    startTimer(33);

    // Set size
    setSize(500, 350);
}

RecordingDialog::~RecordingDialog()
{
    stopTimer();
    m_recordingEngine->removeChangeListener(this);
    waveedit::ThemeManager::getInstance().removeChangeListener(this);

    // Ensure recording is stopped
    if (m_recordingEngine->isRecording())
    {
        m_recordingEngine->stopRecording();
    }

    // Clear any external recording indicator if the window was closed mid-capture
    // (idempotent if stopRecording() already fired it).
    if (m_recordingStateCallback)
        m_recordingStateCallback(false);

    // Remove callback from device manager
    m_deviceManager.removeAudioCallback(m_recordingEngine.get());
}

//==============================================================================
// Component overrides

void RecordingDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void RecordingDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // Title area
    auto statusArea = bounds.removeFromTop(60);
    m_statusLabel.setBounds(statusArea.removeFromTop(25));
    m_elapsedTimeLabel.setBounds(statusArea);

    bounds.removeFromTop(10);  // Spacing

    // Configuration area
    auto configArea = bounds.removeFromTop(120);

    auto inputDeviceRow = configArea.removeFromTop(30);
    m_inputDeviceLabel.setBounds(inputDeviceRow.removeFromLeft(100));
    m_inputDeviceSelector.setBounds(inputDeviceRow);

    configArea.removeFromTop(10);  // Spacing

    auto sampleRateRow = configArea.removeFromTop(30);
    m_sampleRateLabel.setBounds(sampleRateRow.removeFromLeft(100));
    m_sampleRateSelector.setBounds(sampleRateRow);

    configArea.removeFromTop(10);  // Spacing

    auto channelConfigRow = configArea.removeFromTop(30);
    m_channelConfigLabel.setBounds(channelConfigRow.removeFromLeft(100));
    m_channelConfigSelector.setBounds(channelConfigRow);

    bounds.removeFromTop(20);  // Spacing

    // Level meters
    auto levelArea = bounds.removeFromTop(60);

    auto leftLevelRow = levelArea.removeFromTop(25);
    m_leftLevelLabel.setBounds(leftLevelRow.removeFromLeft(25));
    m_leftLevelMeter.setBounds(leftLevelRow);

    levelArea.removeFromTop(10);  // Spacing

    auto rightLevelRow = levelArea.removeFromTop(25);
    m_rightLevelLabel.setBounds(rightLevelRow.removeFromLeft(25));
    m_rightLevelMeter.setBounds(rightLevelRow);

    bounds.removeFromTop(20);  // Spacing

    // Control buttons
    auto buttonArea = bounds.removeFromBottom(40);
    int buttonWidth = 120;
    int buttonSpacing = 10;

    m_cancelButton.setBounds(buttonArea.removeFromRight(buttonWidth));
    buttonArea.removeFromRight(buttonSpacing);

    m_stopButton.setBounds(buttonArea.removeFromRight(buttonWidth));
    buttonArea.removeFromRight(buttonSpacing);

    m_recordButton.setBounds(buttonArea.removeFromRight(buttonWidth));
}

//==============================================================================
// Button::Listener overrides

void RecordingDialog::buttonClicked(juce::Button* button)
{
    if (button == &m_recordButton)
    {
        startRecording();
    }
    else if (button == &m_stopButton)
    {
        stopRecording();
    }
    else if (button == &m_cancelButton)
    {
        // Close dialog without saving
        if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
        {
            window->exitModalState(0);
        }
    }
}

//==============================================================================
// ComboBox::Listener overrides

void RecordingDialog::comboBoxChanged(juce::ComboBox* comboBox)
{
    // Never reconfigure the device mid-take (data loss). The combos are
    // disabled while recording, so this is belt-and-braces.
    if (m_recordingEngine->isRecording())
        return;

    if (comboBox == &m_inputDeviceSelector)
        handleInputDeviceChange();
    else if (comboBox == &m_sampleRateSelector)
        handleSampleRateChange();
    else if (comboBox == &m_channelConfigSelector)
        applyChannelSelection();
}

//==============================================================================
// Device / rate / channel switching

bool RecordingDialog::applyDeviceSetup(const juce::AudioDeviceManager::AudioDeviceSetup& newSetup,
                                       juce::String& error)
{
    // Stop the monitoring feed across the reconfigure so the level meters
    // aren't reading a half-torn-down device, then restart it afterward.
    m_deviceManager.removeAudioCallback(m_recordingEngine.get());
    error = m_deviceManager.setAudioDeviceSetup(newSetup, true /* treatAsChosenDevice */);
    m_deviceManager.addAudioCallback(m_recordingEngine.get());

    return error.isEmpty();
}

void RecordingDialog::handleInputDeviceChange()
{
    const int newId = m_inputDeviceSelector.getSelectedId();
    if (newId == m_currentInputDeviceId)
        return;

    const juce::String chosenDevice = m_inputDeviceSelector.getText();

    auto setup = m_deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = chosenDevice;
    setup.useDefaultInputChannels = true;

    juce::String error;
    if (! applyDeviceSetup(setup, error))
    {
        // Roll back to the last good selection and tell the user why.
        m_inputDeviceSelector.setSelectedId(m_currentInputDeviceId, juce::dontSendNotification);
        showStatusError("Could not switch input device: " + error);
        return;
    }

    m_currentInputDeviceId = newId;

    // A new device may expose different rates / channel counts.
    populateSampleRates();
    m_currentSampleRateId = m_sampleRateSelector.getSelectedId();
    populateChannelConfigurations();
    applyChannelSelection();

    updateUIState();  // refresh status text back to "Ready to record"
}

void RecordingDialog::handleSampleRateChange()
{
    const int newId = m_sampleRateSelector.getSelectedId();
    if (newId == 0 || newId == m_currentSampleRateId)
        return;

    // The combo item ID is the rate in Hz (see populateSampleRates()).
    auto setup = m_deviceManager.getAudioDeviceSetup();
    setup.sampleRate = static_cast<double>(newId);

    juce::String error;
    if (! applyDeviceSetup(setup, error))
    {
        m_sampleRateSelector.setSelectedId(m_currentSampleRateId, juce::dontSendNotification);
        showStatusError("Could not switch sample rate: " + error);
        return;
    }

    m_currentSampleRateId = newId;
    updateUIState();
}

void RecordingDialog::applyChannelSelection()
{
    // The channel-combo item ID IS the recorded channel count (1 or 2).
    const int channelCount = juce::jmax(1, m_channelConfigSelector.getSelectedId());
    m_recordingEngine->setRequestedChannelCount(channelCount);

    // Mono captures a single channel; hide the right meter/label so the UI
    // doesn't imply a second channel is being recorded.
    const bool stereo = (channelCount >= 2);
    m_rightLevelLabel.setVisible(stereo);
    m_rightLevelMeter.setVisible(stereo);
    if (! stereo)
        m_rightLevelMeter.setLevel(0.0f);
}

void RecordingDialog::showStatusError(const juce::String& message)
{
    m_statusLabel.setText(message, juce::dontSendNotification);
    m_statusLabel.setColour(juce::Label::textColourId,
        waveedit::ThemeManager::getInstance().getCurrent().error);
}

//==============================================================================
// ChangeListener overrides

void RecordingDialog::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == m_recordingEngine.get())
    {
        updateUIState();
    }
    else if (source == &waveedit::ThemeManager::getInstance())
    {
        // Re-apply themed colours cached on child components (§6.11).
        // Deliberately does NOT call updateUIState(): that method also
        // rewrites the status label's TEXT based on isRecording(), which
        // would clobber the "No audio input device available" message
        // (set once in populateInputDevices() and not otherwise tracked
        // in state). Only colours are touched here, text is left alone.
        const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();

        m_recordButton.setColour(juce::TextButton::buttonColourId, theme.error);

        const bool isRecording = m_recordingEngine->isRecording();
        const bool isNoDeviceMessage =
            (m_statusLabel.getText() == "No audio input device available");
        m_statusLabel.setColour(juce::Label::textColourId,
            (isRecording || isNoDeviceMessage) ? theme.error : theme.text);

        // The meters read theme tokens at paint() time (no cached colours),
        // but repaint them explicitly so the chrome updates immediately on a
        // theme switch rather than waiting for the next level tick.
        m_leftLevelMeter.repaint();
        m_rightLevelMeter.repaint();

        repaint();
    }
}

//==============================================================================
// Timer overrides

void RecordingDialog::timerCallback()
{
    // Always update level meters for input monitoring (before AND during recording)
    updateLevelMeters();

    // Check if buffer became full (polled on UI thread, not audio thread).
    // Latch so the warning is shown AT MOST ONCE per recording session,
    // otherwise the modal stacks on every timer tick (~33ms).
    if (m_recordingEngine->isBufferFull() && !m_bufferFullHandled)
    {
        m_bufferFullHandled = true;

        juce::Logger::writeToLog("RecordingDialog::timerCallback: recording buffer full, stopping recording");

        // Prevent an immediate re-record refilling the buffer.
        m_recordButton.setEnabled(false);

        // Notify the user once.
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Recording Buffer Full",
            "Maximum recording duration reached (1 hour). Recording has been stopped.",
            this);

        // Finalize the recording via the existing Stop code path.
        stopRecording();
        return;
    }

    // Only update elapsed time during recording
    if (m_recordingEngine->isRecording())
    {
        updateElapsedTime();

        // Surface the RAM cap honestly: show how much record time is left
        // before the 1-hour buffer fills (derived from real buffer capacity).
        const double remaining = m_recordingEngine->getRemainingRecordSeconds();
        const int totalSecs = juce::jmax(0, static_cast<int>(remaining));
        const juce::String remainingText =
            juce::String::formatted("Recording - %02d:%02d remaining",
                                    totalSecs / 60, totalSecs % 60);
        m_statusLabel.setText(remainingText, juce::dontSendNotification);
    }
}

//==============================================================================
// Listener Management

void RecordingDialog::addListener(Listener* listener)
{
    m_listeners.add(listener);
}

void RecordingDialog::removeListener(Listener* listener)
{
    m_listeners.remove(listener);
}

//==============================================================================
// Static Helper

void RecordingDialog::showDialog(juce::Component* parentComponent,
                                 juce::AudioDeviceManager& deviceManager,
                                 Listener* listener,
                                 std::function<void(bool)> recordingStateCallback)
{
    auto* dialog = new RecordingDialog(deviceManager);

    if (listener != nullptr)
    {
        dialog->addListener(listener);
    }

    dialog->m_recordingStateCallback = std::move(recordingStateCallback);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = "Record Audio";
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = parentComponent;

    options.launchAsync();
}

//==============================================================================
// Private Methods

void RecordingDialog::populateInputDevices()
{
    m_inputDeviceSelector.clear(juce::dontSendNotification);

    // Get available input devices from device manager
    auto* deviceType = m_deviceManager.getCurrentDeviceTypeObject();
    if (deviceType != nullptr)
    {
        auto inputDevices = deviceType->getDeviceNames(true);  // true = input devices

        for (int i = 0; i < inputDevices.size(); ++i)
        {
            m_inputDeviceSelector.addItem(inputDevices[i], i + 1);
        }

        // Select current input device
        auto currentDevice = m_deviceManager.getCurrentAudioDevice();
        if (currentDevice != nullptr)
        {
            m_inputDeviceSelector.setText(currentDevice->getName(), juce::dontSendNotification);
        }
    }

    if (m_inputDeviceSelector.getNumItems() == 0)
    {
        m_hasInputDevice = false;

        m_inputDeviceSelector.addItem("No input devices available", 1);
        m_inputDeviceSelector.setSelectedId(1, juce::dontSendNotification);
        m_inputDeviceSelector.setEnabled(false);

        // CRITICAL: Disable record button + config combos when no audio
        // device is available (there is nothing to configure or capture).
        m_recordButton.setEnabled(false);
        m_sampleRateSelector.setEnabled(false);
        m_channelConfigSelector.setEnabled(false);
        m_statusLabel.setText("No audio input device available", juce::dontSendNotification);
        m_statusLabel.setColour(juce::Label::textColourId,
            waveedit::ThemeManager::getInstance().getCurrent().error);
    }
    else
    {
        m_hasInputDevice = true;
        m_inputDeviceSelector.setEnabled(true);
    }
}

void RecordingDialog::populateSampleRates()
{
    m_sampleRateSelector.clear(juce::dontSendNotification);

    auto* currentDevice = m_deviceManager.getCurrentAudioDevice();
    if (currentDevice == nullptr)
    {
        // No device -> nothing meaningful to offer.
        m_sampleRateSelector.setEnabled(false);
        return;
    }

    // The combo item ID encodes the rate directly (rounded Hz), so a switch
    // handler can recover the exact rate without a parallel lookup table.
    const auto availableRates = currentDevice->getAvailableSampleRates();
    const double currentRate = currentDevice->getCurrentSampleRate();

    for (auto rate : availableRates)
    {
        const int id = static_cast<int>(rate);  // e.g. 48000
        m_sampleRateSelector.addItem(juce::String(id) + " Hz", id);

        if (std::abs(rate - currentRate) < 1.0)
            m_sampleRateSelector.setSelectedId(id, juce::dontSendNotification);
    }

    // Fall back to the first available rate if the current one wasn't listed.
    if (m_sampleRateSelector.getSelectedId() == 0 && m_sampleRateSelector.getNumItems() > 0)
        m_sampleRateSelector.setSelectedItemIndex(0, juce::dontSendNotification);

    m_sampleRateSelector.setEnabled(m_hasInputDevice && m_sampleRateSelector.getNumItems() > 0);
}

void RecordingDialog::populateChannelConfigurations()
{
    m_channelConfigSelector.clear(juce::dontSendNotification);

    // Simple, honest semantics (documented inline in the item text):
    //   Mono   -> records the first device input channel (input 1).
    //   Stereo -> records the first two device input channels (inputs 1-2).
    // No downmix is performed. The item ID IS the recorded channel count.
    m_channelConfigSelector.addItem("Mono (records input 1)", 1);

    // Only offer stereo if the device actually exposes >= 2 input channels.
    int availableInputs = 2;
    if (auto* currentDevice = m_deviceManager.getCurrentAudioDevice())
        availableInputs = currentDevice->getActiveInputChannels().countNumberOfSetBits();

    if (availableInputs >= 2)
        m_channelConfigSelector.addItem("Stereo (records inputs 1-2)", 2);

    // Default to stereo when available, otherwise mono.
    m_channelConfigSelector.setSelectedId(availableInputs >= 2 ? 2 : 1, juce::dontSendNotification);

    m_channelConfigSelector.setEnabled(m_hasInputDevice);
}

void RecordingDialog::startRecording()
{
    // Verify audio device is available before attempting to record
    auto* currentDevice = m_deviceManager.getCurrentAudioDevice();
    if (currentDevice == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "No Audio Device",
                                               "No audio input device is available for recording.",
                                               "OK");
        return;
    }

    // Audio callback already added in constructor for input monitoring
    // Just start the recording state
    if (m_recordingEngine->startRecording())
    {
        m_bufferFullHandled = false;  // Reset buffer-full latch for the new session
        m_recordingStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;

        // Fresh take -> clear any latched clip from monitoring.
        m_leftLevelMeter.resetClip();
        m_rightLevelMeter.resetClip();

        updateUIState();

        if (m_recordingStateCallback)
            m_recordingStateCallback(true);
    }
}

void RecordingDialog::stopRecording()
{
    // Stop recording
    m_recordingEngine->stopRecording();

    // Remove callback from device manager
    m_deviceManager.removeAudioCallback(m_recordingEngine.get());

    // Get recorded audio
    const auto& audioBuffer = m_recordingEngine->getRecordedAudio();
    double sampleRate = m_recordingEngine->getRecordedSampleRate();
    int numChannels = m_recordingEngine->getRecordedNumChannels();

    // Notify listeners
    m_listeners.call([&](Listener& listener)
    {
        listener.recordingCompleted(audioBuffer, sampleRate, numChannels);
    });

    if (m_recordingStateCallback)
        m_recordingStateCallback(false);

    // Close dialog
    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
    {
        window->exitModalState(1);
    }
}

void RecordingDialog::updateUIState()
{
    bool isRecording = m_recordingEngine->isRecording();

    m_recordButton.setEnabled(!isRecording && m_hasInputDevice);
    m_stopButton.setEnabled(isRecording);

    // Device churn mid-take is data loss: lock the config combos while
    // recording, re-enable them (if a device exists) once stopped.
    const bool configEnabled = (!isRecording && m_hasInputDevice);
    m_inputDeviceSelector.setEnabled(configEnabled);
    m_sampleRateSelector.setEnabled(configEnabled && m_sampleRateSelector.getNumItems() > 0);
    m_channelConfigSelector.setEnabled(configEnabled);

    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();
    if (isRecording)
    {
        m_statusLabel.setText("Recording...", juce::dontSendNotification);
        m_statusLabel.setColour(juce::Label::textColourId, theme.error);
    }
    else
    {
        m_statusLabel.setText("Ready to record", juce::dontSendNotification);
        m_statusLabel.setColour(juce::Label::textColourId, theme.text);
    }
}

void RecordingDialog::updateElapsedTime()
{
    double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    double elapsed = currentTime - m_recordingStartTime;

    m_elapsedTimeLabel.setText(formatTime(elapsed), juce::dontSendNotification);
}

void RecordingDialog::updateLevelMeters()
{
    m_leftLevelMeter.setLevel(m_recordingEngine->getInputPeakLevel(0));

    // Only feed the right meter in stereo (it is hidden in mono).
    if (m_rightLevelMeter.isVisible())
        m_rightLevelMeter.setLevel(m_recordingEngine->getInputPeakLevel(1));
}

juce::String RecordingDialog::formatTime(double seconds) const
{
    int minutes = static_cast<int>(seconds) / 60;
    double remainingSeconds = seconds - (minutes * 60);

    return juce::String::formatted("%02d:%06.3f", minutes, remainingSeconds);
}
