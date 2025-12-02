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

//==============================================================================
// LevelMeter Implementation

RecordingDialog::LevelMeter::LevelMeter()
    : m_level(0.0f)
{
}

void RecordingDialog::LevelMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(bounds);

    // Level bar (green → yellow → red gradient)
    float levelWidth = bounds.getWidth() * m_level;
    juce::Rectangle<float> levelBar(bounds.getX(), bounds.getY(), levelWidth, bounds.getHeight());

    if (m_level < 0.7f)
    {
        g.setColour(juce::Colours::green);
    }
    else if (m_level < 0.9f)
    {
        g.setColour(juce::Colours::yellow);
    }
    else
    {
        g.setColour(juce::Colours::red);
    }

    g.fillRect(levelBar);

    // Border
    g.setColour(juce::Colours::black);
    g.drawRect(bounds, 1.0f);
}

void RecordingDialog::LevelMeter::setLevel(float level)
{
    m_level = juce::jlimit(0.0f, 1.0f, level);
    repaint();
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

    // Sample rate selection
    m_sampleRateLabel.setText("Sample Rate:", juce::dontSendNotification);
    addAndMakeVisible(m_sampleRateLabel);

    m_sampleRateSelector.addListener(this);
    addAndMakeVisible(m_sampleRateSelector);
    populateSampleRates();

    // Channel configuration
    m_channelConfigLabel.setText("Channels:", juce::dontSendNotification);
    addAndMakeVisible(m_channelConfigLabel);

    m_channelConfigSelector.addListener(this);
    addAndMakeVisible(m_channelConfigSelector);
    populateChannelConfigurations();

    // Recording controls
    m_recordButton.setButtonText("Record");
    m_recordButton.addListener(this);
    m_recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
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
    m_statusLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    m_statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_statusLabel);

    m_elapsedTimeLabel.setText("00:00.000", juce::dontSendNotification);
    m_elapsedTimeLabel.setFont(juce::Font(24.0f, juce::Font::bold));
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

    // Listen to recording engine state changes
    m_recordingEngine->addChangeListener(this);

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

    // Ensure recording is stopped
    if (m_recordingEngine->isRecording())
    {
        m_recordingEngine->stopRecording();
    }

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
    if (comboBox == &m_inputDeviceSelector)
    {
        // Future: Update input device configuration
    }
    else if (comboBox == &m_sampleRateSelector)
    {
        // Future: Update sample rate configuration
    }
    else if (comboBox == &m_channelConfigSelector)
    {
        // Future: Update channel configuration
    }
}

//==============================================================================
// ChangeListener overrides

void RecordingDialog::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == m_recordingEngine.get())
    {
        updateUIState();
    }
}

//==============================================================================
// Timer overrides

void RecordingDialog::timerCallback()
{
    // Always update level meters for input monitoring (before AND during recording)
    updateLevelMeters();

    // Check if buffer became full (polled on UI thread, not audio thread)
    if (m_recordingEngine->isBufferFull())
    {
        // Stop recording gracefully and notify user
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Recording Buffer Full",
            "Maximum recording duration reached (1 hour). Recording has been stopped.",
            this);
    }

    // Only update elapsed time during recording
    if (m_recordingEngine->isRecording())
    {
        updateElapsedTime();
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
                                 Listener* listener)
{
    auto* dialog = new RecordingDialog(deviceManager);

    if (listener != nullptr)
    {
        dialog->addListener(listener);
    }

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
        m_inputDeviceSelector.addItem("No input devices available", 1);
        m_inputDeviceSelector.setSelectedId(1, juce::dontSendNotification);
        m_inputDeviceSelector.setEnabled(false);

        // CRITICAL: Disable record button when no audio device available
        m_recordButton.setEnabled(false);
        m_statusLabel.setText("No audio input device available", juce::dontSendNotification);
        m_statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    }
}

void RecordingDialog::populateSampleRates()
{
    m_sampleRateSelector.clear(juce::dontSendNotification);

    // Common sample rates
    juce::StringArray sampleRates = { "44100 Hz", "48000 Hz", "88200 Hz", "96000 Hz" };
    for (int i = 0; i < sampleRates.size(); ++i)
    {
        m_sampleRateSelector.addItem(sampleRates[i], i + 1);
    }

    // Select current sample rate
    auto currentDevice = m_deviceManager.getCurrentAudioDevice();
    if (currentDevice != nullptr)
    {
        double currentSampleRate = currentDevice->getCurrentSampleRate();
        juce::String currentRateString = juce::String(static_cast<int>(currentSampleRate)) + " Hz";

        for (int i = 0; i < sampleRates.size(); ++i)
        {
            if (sampleRates[i] == currentRateString)
            {
                m_sampleRateSelector.setSelectedId(i + 1, juce::dontSendNotification);
                break;
            }
        }
    }

    if (m_sampleRateSelector.getSelectedId() == 0)
    {
        m_sampleRateSelector.setSelectedId(1, juce::dontSendNotification);  // Default to 44.1kHz
    }
}

void RecordingDialog::populateChannelConfigurations()
{
    m_channelConfigSelector.clear(juce::dontSendNotification);

    // Get current audio device
    auto* currentDevice = m_deviceManager.getCurrentAudioDevice();
    if (currentDevice != nullptr)
    {
        // Get input channel names
        auto inputChannelNames = currentDevice->getInputChannelNames();

        // Add specific input channel options
        if (inputChannelNames.size() > 0)
        {
            // Add mono options for each available input channel
            for (int i = 0; i < inputChannelNames.size(); ++i)
            {
                juce::String channelName = inputChannelNames[i];
                m_channelConfigSelector.addItem("Mono: " + channelName, i + 1);
            }

            // Add stereo pairs if we have multiple inputs
            if (inputChannelNames.size() >= 2)
            {
                int stereoBaseId = inputChannelNames.size() + 1;
                for (int i = 0; i < inputChannelNames.size() - 1; i += 2)
                {
                    juce::String pairName = inputChannelNames[i] + " + " + inputChannelNames[i + 1];
                    m_channelConfigSelector.addItem("Stereo: " + pairName, stereoBaseId + (i / 2));
                }
            }

            // Default to first stereo pair if available, otherwise first mono channel
            if (inputChannelNames.size() >= 2)
            {
                int defaultStereoId = inputChannelNames.size() + 1;
                m_channelConfigSelector.setSelectedId(defaultStereoId, juce::dontSendNotification);
            }
            else
            {
                m_channelConfigSelector.setSelectedId(1, juce::dontSendNotification);
            }
        }
        else
        {
            // Fallback if we can't get channel names
            m_channelConfigSelector.addItem("Mono (1 channel)", 1);
            m_channelConfigSelector.addItem("Stereo (2 channels)", 2);
            m_channelConfigSelector.setSelectedId(2, juce::dontSendNotification);
        }
    }
    else
    {
        // No device - show generic options
        m_channelConfigSelector.addItem("Mono (1 channel)", 1);
        m_channelConfigSelector.addItem("Stereo (2 channels)", 2);
        m_channelConfigSelector.setSelectedId(2, juce::dontSendNotification);
    }
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
        m_recordingStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        updateUIState();
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

    // Close dialog
    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
    {
        window->exitModalState(1);
    }
}

void RecordingDialog::updateUIState()
{
    bool isRecording = m_recordingEngine->isRecording();

    m_recordButton.setEnabled(!isRecording);
    m_stopButton.setEnabled(isRecording);

    m_inputDeviceSelector.setEnabled(!isRecording);
    m_sampleRateSelector.setEnabled(!isRecording);
    m_channelConfigSelector.setEnabled(!isRecording);

    if (isRecording)
    {
        m_statusLabel.setText("Recording...", juce::dontSendNotification);
        m_statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    }
    else
    {
        m_statusLabel.setText("Ready to record", juce::dontSendNotification);
        m_statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
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
    float leftLevel = m_recordingEngine->getInputPeakLevel(0);
    float rightLevel = m_recordingEngine->getInputPeakLevel(1);

    m_leftLevelMeter.setLevel(leftLevel);
    m_rightLevelMeter.setLevel(rightLevel);
}

juce::String RecordingDialog::formatTime(double seconds) const
{
    int minutes = static_cast<int>(seconds) / 60;
    double remainingSeconds = seconds - (minutes * 60);

    return juce::String::formatted("%02d:%06.3f", minutes, remainingSeconds);
}
