/*
  ==============================================================================

    TransportControls.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "TransportControls.h"
#include "WaveformDisplay.h"

//==============================================================================
// Icon Creation Helper Functions

std::unique_ptr<juce::Drawable> TransportControls::createPlayIcon()
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    juce::Path path;

    // Play icon: Right-pointing triangle
    path.startNewSubPath(8.0f, 4.0f);
    path.lineTo(20.0f, 12.0f);
    path.lineTo(8.0f, 20.0f);
    path.closeSubPath();

    drawable->setPath(path);
    drawable->setFill(juce::Colours::white);
    drawable->setStrokeFill(juce::Colours::white);
    drawable->setStrokeThickness(2.0f);

    return drawable;
}

std::unique_ptr<juce::Drawable> TransportControls::createPauseIcon()
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    juce::Path path;

    // Pause icon: Two vertical bars
    path.addRectangle(7.0f, 4.0f, 3.5f, 16.0f);
    path.addRectangle(13.5f, 4.0f, 3.5f, 16.0f);

    drawable->setPath(path);
    drawable->setFill(juce::Colours::white);
    drawable->setStrokeFill(juce::Colours::white);

    return drawable;
}

std::unique_ptr<juce::Drawable> TransportControls::createStopIcon()
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    juce::Path path;

    // Stop icon: Square
    path.addRectangle(6.0f, 6.0f, 12.0f, 12.0f);

    drawable->setPath(path);
    drawable->setFill(juce::Colours::white);
    drawable->setStrokeFill(juce::Colours::white);

    return drawable;
}

std::unique_ptr<juce::Drawable> TransportControls::createLoopIcon()
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    juce::Path path;

    // Loop icon: Circular arrows
    // Draw two curved arrows forming a loop
    path.startNewSubPath(6.0f, 12.0f);
    path.cubicTo(6.0f, 8.0f, 9.0f, 5.0f, 12.0f, 5.0f);
    path.cubicTo(15.0f, 5.0f, 18.0f, 8.0f, 18.0f, 12.0f);
    path.cubicTo(18.0f, 16.0f, 15.0f, 19.0f, 12.0f, 19.0f);
    path.cubicTo(10.5f, 19.0f, 9.0f, 18.0f, 8.0f, 17.0f);

    // Add arrowheads
    path.startNewSubPath(18.0f, 12.0f);
    path.lineTo(16.0f, 10.0f);
    path.lineTo(20.0f, 10.0f);
    path.closeSubPath();

    path.startNewSubPath(6.0f, 12.0f);
    path.lineTo(8.0f, 14.0f);
    path.lineTo(4.0f, 14.0f);
    path.closeSubPath();

    drawable->setPath(path);
    drawable->setFill(juce::FillType());
    drawable->setStrokeFill(juce::Colours::white);
    drawable->setStrokeThickness(2.0f);

    return drawable;
}

//==============================================================================
TransportControls::TransportControls(AudioEngine& audioEngine, WaveformDisplay& waveformDisplay)
    : m_audioEngine(audioEngine),
      m_waveformDisplay(waveformDisplay),
      m_loopEnabled(false),
      m_lastState(PlaybackState::STOPPED),
      m_lastPosition(-1.0)
{
    // Create Play button with icon
    m_playButton = std::make_unique<juce::DrawableButton>("Play", juce::DrawableButton::ImageFitted);
    m_playButton->setImages(createPlayIcon().get());
    m_playButton->onClick = [this] { onPlayClicked(); };
    m_playButton->setTooltip("Play (Space or F12)");
    addAndMakeVisible(m_playButton.get());

    // Create Pause button with icon
    m_pauseButton = std::make_unique<juce::DrawableButton>("Pause", juce::DrawableButton::ImageFitted);
    m_pauseButton->setImages(createPauseIcon().get());
    m_pauseButton->onClick = [this] { onPauseClicked(); };
    m_pauseButton->setTooltip("Pause (Enter or Ctrl+F12)");
    addAndMakeVisible(m_pauseButton.get());

    // Create Stop button with icon
    m_stopButton = std::make_unique<juce::DrawableButton>("Stop", juce::DrawableButton::ImageFitted);
    m_stopButton->setImages(createStopIcon().get());
    m_stopButton->onClick = [this] { onStopClicked(); };
    m_stopButton->setTooltip("Stop playback");
    addAndMakeVisible(m_stopButton.get());

    // Create Loop button with icon
    m_loopButton = std::make_unique<juce::DrawableButton>("Loop", juce::DrawableButton::ImageFitted);
    m_loopButton->setImages(createLoopIcon().get());
    m_loopButton->onClick = [this] { onLoopClicked(); };
    m_loopButton->setTooltip("Toggle Loop (Q)");
    m_loopButton->setClickingTogglesState(true);
    addAndMakeVisible(m_loopButton.get());

    // Create time display label
    m_timeLabel = std::make_unique<juce::Label>("Time", "00:00:00.000");
    m_timeLabel->setJustificationType(juce::Justification::centred);
    m_timeLabel->setFont(juce::FontOptions(16.0f, juce::Font::bold));
    m_timeLabel->setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible(m_timeLabel.get());

    // Start timer for position updates (20 times per second = 50ms)
    startTimer(50);

    // Initial button state update
    updateButtonStates();
}

TransportControls::~TransportControls()
{
    stopTimer();
}

//==============================================================================
// Component Overrides

void TransportControls::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff2a2a2a));

    // Border
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRect(getLocalBounds(), 1);

}

void TransportControls::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Split into two rows
    auto topRow = bounds.removeFromTop(bounds.getHeight() / 2);
    auto bottomRow = bounds;

    // Top row: Transport buttons
    auto buttonWidth = topRow.getWidth() / 4;
    m_playButton->setBounds(topRow.removeFromLeft(buttonWidth).reduced(5));
    m_pauseButton->setBounds(topRow.removeFromLeft(buttonWidth).reduced(5));
    m_stopButton->setBounds(topRow.removeFromLeft(buttonWidth).reduced(5));
    m_loopButton->setBounds(topRow.removeFromLeft(buttonWidth).reduced(5));

    // Bottom row: Position display (centered)
    m_timeLabel->setBounds(bottomRow.reduced(5));
}

//==============================================================================
// Timer Callback

void TransportControls::timerCallback()
{
    auto currentState = m_audioEngine.getPlaybackState();
    double currentPosition = m_audioEngine.getCurrentPosition();
    double totalLength = m_audioEngine.getTotalLength();

    // Selection-bounded playback: stop at selection end or loop within selection
    if (m_audioEngine.isPlaying() && m_waveformDisplay.hasSelection())
    {
        double selectionStart = m_waveformDisplay.getSelectionStart();
        double selectionEnd = m_waveformDisplay.getSelectionEnd();

        // Check if playback has reached selection end (within 50ms tolerance)
        if (currentPosition >= (selectionEnd - 0.05))
        {
            if (m_loopEnabled)
            {
                // Loop: restart from selection start
                m_audioEngine.setPosition(selectionStart);
                juce::Logger::writeToLog(juce::String::formatted(
                    "Selection loop: Restarting from %.3f s", selectionStart));
            }
            else
            {
                // No loop: stop at selection end
                m_audioEngine.stop();
                juce::Logger::writeToLog("Selection playback complete, stopped at end");
            }
        }
    }
    // Full file loop mode - restart playback when it reaches the end
    else if (m_loopEnabled && m_audioEngine.isPlaying())
    {
        // Check if playback has reached the end (within 50ms tolerance)
        if (totalLength > 0.0 && currentPosition >= (totalLength - 0.05))
        {
            // Restart playback from the beginning
            m_audioEngine.setPosition(0.0);
            juce::Logger::writeToLog("Loop: Restarting playback from beginning");
        }
    }

    // Check if state or position changed
    bool stateChanged = (currentState != m_lastState);
    bool positionChanged = (m_audioEngine.isPlaying() &&
                           std::abs(currentPosition - m_lastPosition) > 0.01);

    // Only update if something changed
    if (stateChanged || positionChanged)
    {
        updateButtonStates();
        updatePositionDisplay();

        // Update last known values
        m_lastState = currentState;
        m_lastPosition = currentPosition;

        repaint();
    }
}

//==============================================================================
// Loop Control

bool TransportControls::isLoopEnabled() const
{
    return m_loopEnabled;
}

void TransportControls::setLoopEnabled(bool shouldLoop)
{
    m_loopEnabled = shouldLoop;
    m_loopButton->setToggleState(m_loopEnabled, juce::dontSendNotification);
}

void TransportControls::toggleLoop()
{
    setLoopEnabled(!m_loopEnabled);
}

//==============================================================================
// Private Methods

void TransportControls::updateButtonStates()
{
    bool fileLoaded = m_audioEngine.isFileLoaded();

    // Enable/disable buttons based on file loaded state
    m_playButton->setEnabled(fileLoaded);
    m_pauseButton->setEnabled(fileLoaded);
    m_stopButton->setEnabled(fileLoaded);
    m_loopButton->setEnabled(fileLoaded);

    // Note: Visual state feedback is now provided by:
    // 1. The colored LED indicator in the paint() method
    // 2. Button enabled/disabled states
    // DrawableButtons will use default JUCE button styling
}

void TransportControls::updatePositionDisplay()
{
    if (!m_audioEngine.isFileLoaded())
    {
        m_timeLabel->setText("--:--:--.---", juce::dontSendNotification);
        return;
    }

    // Get current position and total length
    double currentPos = m_audioEngine.getCurrentPosition();
    double totalLength = m_audioEngine.getTotalLength();

    // Format and display time
    juce::String timeText = formatTime(currentPos) + " / " + formatTime(totalLength);
    m_timeLabel->setText(timeText, juce::dontSendNotification);
}

juce::String TransportControls::formatTime(double timeInSeconds) const
{
    int hours = static_cast<int>(timeInSeconds / 3600.0);
    int minutes = static_cast<int>((timeInSeconds - (hours * 3600.0)) / 60.0);
    double seconds = timeInSeconds - (hours * 3600.0) - (minutes * 60.0);

    int wholeSecs = static_cast<int>(seconds);
    int milliseconds = static_cast<int>((seconds - wholeSecs) * 1000.0);

    return juce::String::formatted("%02d:%02d:%02d.%03d",
                                   hours,
                                   minutes,
                                   wholeSecs,
                                   milliseconds);
}

juce::String TransportControls::formatSample(int64_t sample) const
{
    // Format with thousands separators
    juce::String sampleStr = juce::String(sample);
    juce::String formatted;

    int digitCount = 0;
    for (int i = sampleStr.length() - 1; i >= 0; --i)
    {
        if (digitCount > 0 && digitCount % 3 == 0)
        {
            formatted = "," + formatted;
        }
        formatted = juce::String::charToString(sampleStr[i]) + formatted;
        digitCount++;
    }

    return formatted;
}

//==============================================================================
// Button Callbacks

void TransportControls::onPlayClicked()
{
    if (!m_audioEngine.isFileLoaded())
    {
        return;
    }

    auto state = m_audioEngine.getPlaybackState();

    if (state == PlaybackState::STOPPED || state == PlaybackState::PAUSED)
    {
        m_audioEngine.play();
    }
    else if (state == PlaybackState::PLAYING)
    {
        // Play button acts as play/stop toggle when already playing
        m_audioEngine.stop();
    }

    updateButtonStates();
}

void TransportControls::onPauseClicked()
{
    if (!m_audioEngine.isFileLoaded())
    {
        return;
    }

    auto state = m_audioEngine.getPlaybackState();

    if (state == PlaybackState::PLAYING)
    {
        m_audioEngine.pause();
    }
    else if (state == PlaybackState::PAUSED)
    {
        // Unpause - resume playback
        m_audioEngine.play();
    }

    updateButtonStates();
}

void TransportControls::onStopClicked()
{
    if (!m_audioEngine.isFileLoaded())
    {
        return;
    }

    m_audioEngine.stop();
    updateButtonStates();
}

void TransportControls::onLoopClicked()
{
    toggleLoop();
    updateButtonStates();

    // Log loop state for debugging
    juce::Logger::writeToLog("Loop " + juce::String(m_loopEnabled ? "enabled" : "disabled"));

    // Note: Loop state is connected to AudioEngine in Main.cpp via setLooping() callback
}
