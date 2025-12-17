/*
  ==============================================================================

    CompactTransport.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "CompactTransport.h"
#include "WaveformDisplay.h"

//==============================================================================
// Icon Creation - Compact 16x16 icons for 24px buttons

std::unique_ptr<juce::Drawable> CompactTransport::createRecordIcon()
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    juce::Path path;

    // Record icon: Filled circle
    path.addEllipse(4.0f, 4.0f, 8.0f, 8.0f);

    drawable->setPath(path);
    drawable->setFill(juce::Colours::red);
    return drawable;
}

std::unique_ptr<juce::Drawable> CompactTransport::createRewindIcon()
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    juce::Path path;

    // Rewind to start: |<< (bar + two triangles)
    path.addRectangle(2.0f, 3.0f, 2.0f, 10.0f);  // Bar
    path.startNewSubPath(14.0f, 8.0f);
    path.lineTo(8.0f, 3.0f);
    path.lineTo(8.0f, 13.0f);
    path.closeSubPath();
    path.startNewSubPath(9.0f, 8.0f);
    path.lineTo(4.0f, 3.0f);
    path.lineTo(4.0f, 13.0f);
    path.closeSubPath();

    drawable->setPath(path);
    drawable->setFill(juce::Colours::white);
    return drawable;
}

std::unique_ptr<juce::Drawable> CompactTransport::createStopIcon()
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    juce::Path path;

    // Stop icon: Square
    path.addRectangle(4.0f, 4.0f, 8.0f, 8.0f);

    drawable->setPath(path);
    drawable->setFill(juce::Colours::white);
    return drawable;
}

std::unique_ptr<juce::Drawable> CompactTransport::createPlayIcon()
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    juce::Path path;

    // Play icon: Right-pointing triangle
    path.startNewSubPath(5.0f, 3.0f);
    path.lineTo(13.0f, 8.0f);
    path.lineTo(5.0f, 13.0f);
    path.closeSubPath();

    drawable->setPath(path);
    drawable->setFill(juce::Colours::white);
    return drawable;
}

std::unique_ptr<juce::Drawable> CompactTransport::createPauseIcon()
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    juce::Path path;

    // Pause icon: Two vertical bars
    path.addRectangle(4.0f, 3.0f, 3.0f, 10.0f);
    path.addRectangle(9.0f, 3.0f, 3.0f, 10.0f);

    drawable->setPath(path);
    drawable->setFill(juce::Colours::white);
    return drawable;
}

std::unique_ptr<juce::Drawable> CompactTransport::createForwardIcon()
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    juce::Path path;

    // Forward to end: >>| (two triangles + bar)
    path.startNewSubPath(2.0f, 8.0f);
    path.lineTo(8.0f, 3.0f);
    path.lineTo(8.0f, 13.0f);
    path.closeSubPath();
    path.startNewSubPath(7.0f, 8.0f);
    path.lineTo(12.0f, 3.0f);
    path.lineTo(12.0f, 13.0f);
    path.closeSubPath();
    path.addRectangle(12.0f, 3.0f, 2.0f, 10.0f);  // Bar

    drawable->setPath(path);
    drawable->setFill(juce::Colours::white);
    return drawable;
}

std::unique_ptr<juce::Drawable> CompactTransport::createLoopIcon()
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    juce::Path path;

    // Compact loop icon: circular arrows
    path.startNewSubPath(4.0f, 8.0f);
    path.cubicTo(4.0f, 5.0f, 6.0f, 3.0f, 8.0f, 3.0f);
    path.cubicTo(10.0f, 3.0f, 12.0f, 5.0f, 12.0f, 8.0f);
    path.cubicTo(12.0f, 11.0f, 10.0f, 13.0f, 8.0f, 13.0f);
    path.cubicTo(7.0f, 13.0f, 6.0f, 12.5f, 5.5f, 12.0f);

    // Arrowheads
    path.startNewSubPath(12.0f, 8.0f);
    path.lineTo(10.5f, 6.5f);
    path.lineTo(13.5f, 6.5f);
    path.closeSubPath();

    path.startNewSubPath(4.0f, 8.0f);
    path.lineTo(5.5f, 9.5f);
    path.lineTo(2.5f, 9.5f);
    path.closeSubPath();

    drawable->setPath(path);
    drawable->setFill(juce::FillType());
    drawable->setStrokeFill(juce::Colours::white);
    drawable->setStrokeThickness(1.5f);
    return drawable;
}

//==============================================================================
CompactTransport::CompactTransport()
    : m_audioEngine(nullptr),
      m_waveformDisplay(nullptr),
      m_loopEnabled(false),
      m_timeFormat(TimeFormat::CompactTime),
      m_lastState(PlaybackState::STOPPED),
      m_lastPosition(-1.0),
      m_recordPulse(false)
{
    // Create Record button
    m_recordButton = std::make_unique<juce::DrawableButton>("Record", juce::DrawableButton::ImageFitted);
    m_recordButton->setImages(createRecordIcon().get());
    m_recordButton->onClick = [this] { onRecordClicked(); };
    m_recordButton->setTooltip("Record (R)");
    addAndMakeVisible(m_recordButton.get());

    // Create Rewind button
    m_rewindButton = std::make_unique<juce::DrawableButton>("Rewind", juce::DrawableButton::ImageFitted);
    m_rewindButton->setImages(createRewindIcon().get());
    m_rewindButton->onClick = [this] { onRewindClicked(); };
    m_rewindButton->setTooltip("Go to Start (Home)");
    addAndMakeVisible(m_rewindButton.get());

    // Create Stop button
    m_stopButton = std::make_unique<juce::DrawableButton>("Stop", juce::DrawableButton::ImageFitted);
    m_stopButton->setImages(createStopIcon().get());
    m_stopButton->onClick = [this] { onStopClicked(); };
    m_stopButton->setTooltip("Stop");
    addAndMakeVisible(m_stopButton.get());

    // Create Play/Pause toggle button (starts showing Play icon)
    m_playPauseButton = std::make_unique<juce::DrawableButton>("PlayPause", juce::DrawableButton::ImageFitted);
    m_playPauseButton->setImages(createPlayIcon().get());
    m_playPauseButton->onClick = [this] { onPlayPauseClicked(); };
    m_playPauseButton->setTooltip("Play/Pause (Space)");
    addAndMakeVisible(m_playPauseButton.get());

    // Create Forward button
    m_forwardButton = std::make_unique<juce::DrawableButton>("Forward", juce::DrawableButton::ImageFitted);
    m_forwardButton->setImages(createForwardIcon().get());
    m_forwardButton->onClick = [this] { onForwardClicked(); };
    m_forwardButton->setTooltip("Go to End (End)");
    addAndMakeVisible(m_forwardButton.get());

    // Create Loop button
    m_loopButton = std::make_unique<juce::DrawableButton>("Loop", juce::DrawableButton::ImageFitted);
    m_loopButton->setImages(createLoopIcon().get());
    m_loopButton->onClick = [this] { onLoopClicked(); };
    m_loopButton->setTooltip("Toggle Loop (L)");
    m_loopButton->setClickingTogglesState(true);
    addAndMakeVisible(m_loopButton.get());

    // Create compact time display
    m_timeLabel = std::make_unique<juce::Label>("Time", "00:00.00");
    m_timeLabel->setJustificationType(juce::Justification::centred);
    m_timeLabel->setFont(juce::FontOptions(11.0f, juce::Font::bold));
    m_timeLabel->setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    m_timeLabel->setColour(juce::Label::backgroundColourId, juce::Colour(0xff1a1a1a));
    m_timeLabel->setEditable(false, false, false);
    // Make label clickable to cycle time format
    m_timeLabel->addMouseListener(this, false);
    addAndMakeVisible(m_timeLabel.get());

    // Start timer for position updates
    startTimer(50);

    // Initial state update
    updateButtonStates();
}

CompactTransport::~CompactTransport()
{
    stopTimer();
}

//==============================================================================
void CompactTransport::setAudioEngine(AudioEngine* audioEngine)
{
    m_audioEngine = audioEngine;
    updateButtonStates();
    updatePositionDisplay();
    updatePlayPauseIcon();
}

void CompactTransport::setWaveformDisplay(WaveformDisplay* waveformDisplay)
{
    m_waveformDisplay = waveformDisplay;
}

//==============================================================================
void CompactTransport::paint(juce::Graphics& g)
{
    // Subtle dark background
    g.fillAll(juce::Colour(0xff2a2a2a));

    // Border on bottom only to separate from content below
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawLine(0.0f, static_cast<float>(getHeight() - 1),
               static_cast<float>(getWidth()), static_cast<float>(getHeight() - 1), 1.0f);

    // Pulsing record indicator when recording
    // Note: Recording integration with RecordingEngine is TODO
    // if (m_audioEngine != nullptr && m_audioEngine->isRecording() && m_recordPulse)
    // {
    //     g.setColour(juce::Colours::red.withAlpha(0.3f));
    //     g.fillRect(m_recordButton->getBounds().expanded(2));
    // }
}

void CompactTransport::resized()
{
    auto bounds = getLocalBounds().reduced(4, 2);

    const int buttonSize = kButtonSize;
    const int buttonSpacing = 2;

    // Layout buttons left to right
    m_recordButton->setBounds(bounds.removeFromLeft(buttonSize));
    bounds.removeFromLeft(buttonSpacing);

    m_rewindButton->setBounds(bounds.removeFromLeft(buttonSize));
    bounds.removeFromLeft(buttonSpacing);

    m_stopButton->setBounds(bounds.removeFromLeft(buttonSize));
    bounds.removeFromLeft(buttonSpacing);

    m_playPauseButton->setBounds(bounds.removeFromLeft(buttonSize));
    bounds.removeFromLeft(buttonSpacing);

    m_forwardButton->setBounds(bounds.removeFromLeft(buttonSize));
    bounds.removeFromLeft(buttonSpacing);

    m_loopButton->setBounds(bounds.removeFromLeft(buttonSize));
    bounds.removeFromLeft(buttonSpacing + 4);  // Extra spacing before time display

    // Time display takes remaining width
    m_timeLabel->setBounds(bounds.reduced(0, 4));
}

void CompactTransport::mouseDown(const juce::MouseEvent& event)
{
    // Forward right-click to parent (toolbar) for context menu
    if (event.mods.isPopupMenu())
    {
        if (auto* parent = getParentComponent())
        {
            auto parentEvent = event.getEventRelativeTo(parent);
            parent->mouseDown(parentEvent);
        }
    }
}

//==============================================================================
void CompactTransport::timerCallback()
{
    // Early return if no audio engine
    if (m_audioEngine == nullptr)
        return;

    auto currentState = m_audioEngine->getPlaybackState();
    double currentPosition = m_audioEngine->getCurrentPosition();
    double totalLength = m_audioEngine->getTotalLength();

    // Toggle record pulse for animation
    // Note: Recording integration with RecordingEngine is TODO
    // if (m_audioEngine->isRecording())
    //     m_recordPulse = !m_recordPulse;

    // Selection-bounded playback handling
    if (m_audioEngine->isPlaying() && m_waveformDisplay != nullptr && m_waveformDisplay->hasSelection() &&
        m_audioEngine->getPreviewMode() == PreviewMode::DISABLED)
    {
        double selectionStart = m_waveformDisplay->getSelectionStart();
        double selectionEnd = m_waveformDisplay->getSelectionEnd();

        if (currentPosition >= (selectionEnd - 0.05))
        {
            if (m_loopEnabled)
            {
                m_audioEngine->setPosition(selectionStart);
            }
            else
            {
                m_audioEngine->stop();
            }
        }
    }
    else if (m_loopEnabled && m_audioEngine->isPlaying())
    {
        if (totalLength > 0.0 && currentPosition >= (totalLength - 0.05))
        {
            m_audioEngine->setPosition(0.0);
        }
    }

    // Check if state or position changed
    bool stateChanged = (currentState != m_lastState);
    bool positionChanged = (m_audioEngine->isPlaying() &&
                            std::abs(currentPosition - m_lastPosition) > 0.01);

    if (stateChanged || positionChanged)
    {
        updateButtonStates();
        updatePositionDisplay();
        updatePlayPauseIcon();

        m_lastState = currentState;
        m_lastPosition = currentPosition;

        repaint();
    }
}

//==============================================================================
bool CompactTransport::isLoopEnabled() const
{
    return m_loopEnabled;
}

void CompactTransport::setLoopEnabled(bool shouldLoop)
{
    m_loopEnabled = shouldLoop;
    m_loopButton->setToggleState(m_loopEnabled, juce::dontSendNotification);
}

void CompactTransport::toggleLoop()
{
    setLoopEnabled(!m_loopEnabled);
}

void CompactTransport::setTimeFormat(TimeFormat format)
{
    m_timeFormat = format;
    updatePositionDisplay();
}

void CompactTransport::cycleTimeFormat()
{
    switch (m_timeFormat)
    {
        case TimeFormat::Time:
            m_timeFormat = TimeFormat::CompactTime;
            break;
        case TimeFormat::CompactTime:
            m_timeFormat = TimeFormat::Samples;
            break;
        case TimeFormat::Samples:
            m_timeFormat = TimeFormat::Time;
            break;
    }
    updatePositionDisplay();
}

//==============================================================================
void CompactTransport::updateButtonStates()
{
    bool fileLoaded = (m_audioEngine != nullptr) && m_audioEngine->isFileLoaded();

    m_recordButton->setEnabled(true);  // Record always available
    m_rewindButton->setEnabled(fileLoaded);
    m_stopButton->setEnabled(fileLoaded);
    m_playPauseButton->setEnabled(fileLoaded);
    m_forwardButton->setEnabled(fileLoaded);
    m_loopButton->setEnabled(fileLoaded);
}

void CompactTransport::updatePositionDisplay()
{
    if (m_audioEngine == nullptr || !m_audioEngine->isFileLoaded())
    {
        m_timeLabel->setText("--:--.--", juce::dontSendNotification);
        return;
    }

    double currentPos = m_audioEngine->getCurrentPosition();
    double sampleRate = m_audioEngine->getSampleRate();
    juce::String timeText;

    switch (m_timeFormat)
    {
        case TimeFormat::Time:
            timeText = formatTime(currentPos);
            break;
        case TimeFormat::CompactTime:
            timeText = formatCompactTime(currentPos);
            break;
        case TimeFormat::Samples:
            timeText = formatSamples(static_cast<int64_t>(currentPos * sampleRate));
            break;
    }

    m_timeLabel->setText(timeText, juce::dontSendNotification);
}

void CompactTransport::updatePlayPauseIcon()
{
    if (m_audioEngine == nullptr)
    {
        m_playPauseButton->setImages(createPlayIcon().get());
        return;
    }

    auto state = m_audioEngine->getPlaybackState();

    if (state == PlaybackState::PLAYING)
    {
        // Show pause icon when playing
        m_playPauseButton->setImages(createPauseIcon().get());
    }
    else
    {
        // Show play icon when stopped or paused
        m_playPauseButton->setImages(createPlayIcon().get());
    }
}

//==============================================================================
juce::String CompactTransport::formatTime(double timeInSeconds) const
{
    int hours = static_cast<int>(timeInSeconds / 3600.0);
    int minutes = static_cast<int>((timeInSeconds - (hours * 3600.0)) / 60.0);
    double seconds = timeInSeconds - (hours * 3600.0) - (minutes * 60.0);

    int wholeSecs = static_cast<int>(seconds);
    int milliseconds = static_cast<int>((seconds - wholeSecs) * 1000.0);

    return juce::String::formatted("%02d:%02d:%02d.%03d",
                                   hours, minutes, wholeSecs, milliseconds);
}

juce::String CompactTransport::formatCompactTime(double timeInSeconds) const
{
    int minutes = static_cast<int>(timeInSeconds / 60.0);
    double seconds = timeInSeconds - (minutes * 60.0);
    int wholeSecs = static_cast<int>(seconds);
    int centiseconds = static_cast<int>((seconds - wholeSecs) * 100.0);

    return juce::String::formatted("%02d:%02d.%02d", minutes, wholeSecs, centiseconds);
}

juce::String CompactTransport::formatSamples(int64_t samples) const
{
    // Format with thousands separators
    juce::String sampleStr = juce::String(samples);
    juce::String formatted;

    int digitCount = 0;
    for (int i = sampleStr.length() - 1; i >= 0; --i)
    {
        if (digitCount > 0 && digitCount % 3 == 0)
            formatted = "," + formatted;
        formatted = juce::String::charToString(sampleStr[i]) + formatted;
        digitCount++;
    }

    return formatted;
}

//==============================================================================
void CompactTransport::onRecordClicked()
{
    // Trigger record command (handled by command system)
    juce::Logger::writeToLog("CompactTransport: Record clicked");
    // Note: Recording is handled by the main application's command system
}

void CompactTransport::onRewindClicked()
{
    if (m_audioEngine == nullptr || !m_audioEngine->isFileLoaded())
        return;

    m_audioEngine->setPosition(0.0);
    updatePositionDisplay();
}

void CompactTransport::onStopClicked()
{
    if (m_audioEngine == nullptr || !m_audioEngine->isFileLoaded())
        return;

    m_audioEngine->stop();
    updateButtonStates();
    updatePlayPauseIcon();
}

void CompactTransport::onPlayPauseClicked()
{
    if (m_audioEngine == nullptr || !m_audioEngine->isFileLoaded())
        return;

    auto state = m_audioEngine->getPlaybackState();

    if (state == PlaybackState::PLAYING)
    {
        m_audioEngine->pause();
    }
    else
    {
        m_audioEngine->play();
    }

    updateButtonStates();
    updatePlayPauseIcon();
}

void CompactTransport::onForwardClicked()
{
    if (m_audioEngine == nullptr || !m_audioEngine->isFileLoaded())
        return;

    double totalLength = m_audioEngine->getTotalLength();
    m_audioEngine->setPosition(totalLength);
    updatePositionDisplay();
}

void CompactTransport::onLoopClicked()
{
    toggleLoop();
    updateButtonStates();
    juce::Logger::writeToLog("Loop " + juce::String(m_loopEnabled ? "enabled" : "disabled"));
}

void CompactTransport::onTimeLabelClicked()
{
    cycleTimeFormat();
}
