/*
  ==============================================================================

    GraphicalEQEditor.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "GraphicalEQEditor.h"
#include "../Audio/AudioEngine.h"
#include <cmath>

//==============================================================================
// Display Configuration Constants

namespace
{
    /**
     * FFT Display Scaling Factor (reused from SpectrumAnalyzer pattern)
     */
    constexpr float FFT_DISPLAY_SCALE = 4.0f;

    /**
     * Exponential Smoothing Factor for FFT visualization
     */
    constexpr float SMOOTHING_FACTOR = 0.75f;

    /**
     * Control point interaction threshold (pixels)
     */
    constexpr float CONTROL_POINT_RADIUS = 8.0f;
    constexpr float CONTROL_POINT_CLICK_THRESHOLD = 15.0f;

    /**
     * Dialog window dimensions
     */
    constexpr int DIALOG_WIDTH = 800;
    constexpr int DIALOG_HEIGHT = 600;
    constexpr int BUTTON_HEIGHT = 30;
    constexpr int BUTTON_WIDTH = 100;
    constexpr int MARGIN = 10;
}

//==============================================================================
// Constructor / Destructor

GraphicalEQEditor::GraphicalEQEditor(const ParametricEQ::Parameters& initialParams)
    : m_params(initialParams)
    , m_lowBandPoint(BandType::LOW)
    , m_midBandPoint(BandType::MID)
    , m_highBandPoint(BandType::HIGH)
    , m_fft(std::make_unique<juce::dsp::FFT>(FFT_ORDER))
    , m_window(std::make_unique<juce::dsp::WindowingFunction<float>>(
        FFT_SIZE, juce::dsp::WindowingFunction<float>::hann))
    , m_fifoIndex(0)
    , m_nextFFTBlockReady(false)
    , m_audioEngine(nullptr)
    , m_minFrequency(20.0f)
    , m_maxFrequency(20000.0f)
    , m_minDB(-60.0f)
    , m_maxDB(20.0f)
    , m_previewButton("Preview")
    , m_applyButton("Apply")
    , m_cancelButton("Cancel")
{
    m_sampleRate.store(44100.0);

    // Initialize FFT buffers
    m_fftData.fill(0.0f);
    m_scopeData.fill(0.0f);

    // Setup buttons
    m_previewButton.onClick = [this]()
    {
        togglePreview();
    };
    m_previewButton.setEnabled(true); // Enabled but shows info message about preview
    addAndMakeVisible(m_previewButton);

    m_applyButton.onClick = [this]()
    {
        // DON'T re-sync from screen coords - m_params already updated during drag
        // Parameters are already current from mouseDrag() operations
        m_result = m_params;

        // Use Logger for Release builds (DBG is compiled out)
        juce::Logger::writeToLog("GraphicalEQ Apply clicked:");
        juce::Logger::writeToLog("  Low: " + juce::String(m_params.low.frequency) + " Hz, " +
            juce::String(m_params.low.gain) + " dB, Q=" + juce::String(m_params.low.q));
        juce::Logger::writeToLog("  Mid: " + juce::String(m_params.mid.frequency) + " Hz, " +
            juce::String(m_params.mid.gain) + " dB, Q=" + juce::String(m_params.mid.q));
        juce::Logger::writeToLog("  High: " + juce::String(m_params.high.frequency) + " Hz, " +
            juce::String(m_params.high.gain) + " dB, Q=" + juce::String(m_params.high.q));

        if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
        {
            dialogWindow->exitModalState(1);
        }
    };
    addAndMakeVisible(m_applyButton);

    m_cancelButton.onClick = [this]()
    {
        m_result = std::nullopt;
        if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
        {
            dialogWindow->exitModalState(0);
        }
    };
    addAndMakeVisible(m_cancelButton);

    // Set size FIRST to establish valid bounds
    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);

    // Initialize control points AFTER size is set
    updateControlPointPositions();

    // Start timer for UI updates (30fps)
    startTimer(1000 / UPDATE_RATE_HZ);
}

GraphicalEQEditor::~GraphicalEQEditor()
{
    stopTimer();
}

//==============================================================================
// Public API

std::optional<ParametricEQ::Parameters> GraphicalEQEditor::showDialog(
    const ParametricEQ::Parameters& initialParams)
{
    // Create dialog on stack (like ParametricEQDialog does)
    GraphicalEQEditor editor(initialParams);

    // TODO: Preview functionality requires AudioEngine refactoring
    // to support temporary buffer playback without modifying the main buffer.
    // For now, preview is disabled.

    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(&editor);  // Dialog stays alive on stack
    options.dialogTitle = "Graphical Parametric EQ";
    options.dialogBackgroundColour = juce::Colour(0xff2d2d2d);
    options.escapeKeyTriggersCloseButton = false;
    options.useNativeTitleBar = false;
    options.resizable = false;
    options.componentToCentreAround = nullptr;

    #if JUCE_MODAL_LOOPS_PERMITTED
        int result = options.runModal();
        return (result == 1) ? editor.m_result : std::nullopt;
    #else
        // Non-modal mode not supported for this dialog
        jassertfalse;
        return std::nullopt;
    #endif
}

void GraphicalEQEditor::setAudioEngine(AudioEngine* audioEngine)
{
    // Note: Currently unused as spectrum analyzer audio feed is disabled
    // Future: Add thread-safe audio data access when implementing real-time visualization
    m_audioEngine = audioEngine;
    if (audioEngine)
    {
        m_sampleRate.store(audioEngine->getSampleRate());
    }
}

//==============================================================================
// Component Overrides

void GraphicalEQEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));

    // Calculate visualization bounds (reserve space for buttons)
    auto bounds = getLocalBounds().reduced(MARGIN);
    bounds.removeFromBottom(BUTTON_HEIGHT + MARGIN);

    auto vizBounds = bounds.toFloat();

    // Draw grid and axes first
    drawGrid(g, vizBounds);

    // Draw spectrum analyzer background
    drawSpectrum(g, vizBounds);

    // Draw EQ frequency response curve
    drawEQCurve(g, vizBounds);

    // Draw control points on top
    drawControlPoints(g, vizBounds);
}

void GraphicalEQEditor::resized()
{
    auto bounds = getLocalBounds().reduced(MARGIN);

    // Position buttons at bottom
    auto buttonArea = bounds.removeFromBottom(BUTTON_HEIGHT);
    m_cancelButton.setBounds(buttonArea.removeFromLeft(BUTTON_WIDTH));
    buttonArea.removeFromLeft(MARGIN);
    m_previewButton.setBounds(buttonArea.removeFromLeft(BUTTON_WIDTH));
    buttonArea.removeFromLeft(MARGIN);
    m_applyButton.setBounds(buttonArea.removeFromLeft(BUTTON_WIDTH));

    // Update control point positions when window resizes
    updateControlPointPositions();
}

void GraphicalEQEditor::timerCallback()
{
    // Process FFT if new block is ready
    if (m_nextFFTBlockReady.load())
    {
        processFFT();
        m_nextFFTBlockReady.store(false);
    }

    // Future: Feed audio data from engine for real-time spectrum visualization
    // This would require adding getAudioData() API to AudioEngine
    // For now, spectrum analyzer is disabled (shows only EQ curve)

    repaint();
}

//==============================================================================
// Mouse Interaction

void GraphicalEQEditor::mouseDown(const juce::MouseEvent& event)
{
    auto bounds = getLocalBounds().reduced(MARGIN);
    bounds.removeFromBottom(BUTTON_HEIGHT + MARGIN);

    float x = static_cast<float>(event.x);
    float y = static_cast<float>(event.y);

    // Find nearest control point
    ControlPoint* point = findNearestControlPoint(x, y);
    if (point)
    {
        point->isDragging = true;
    }
}

void GraphicalEQEditor::mouseDrag(const juce::MouseEvent& event)
{
    auto bounds = getLocalBounds().reduced(MARGIN);
    bounds.removeFromBottom(BUTTON_HEIGHT + MARGIN);
    auto vizBounds = bounds.toFloat();

    float x = juce::jlimit(vizBounds.getX(), vizBounds.getRight(), static_cast<float>(event.x));
    float y = juce::jlimit(vizBounds.getY(), vizBounds.getBottom(), static_cast<float>(event.y));

    // Update dragging point position
    bool parametersChanged = false;

    if (m_lowBandPoint.isDragging)
    {
        m_lowBandPoint.x = x;
        m_lowBandPoint.y = y;
        updateParametersFromControlPoints();
        parametersChanged = true;
    }
    else if (m_midBandPoint.isDragging)
    {
        m_midBandPoint.x = x;
        m_midBandPoint.y = y;
        updateParametersFromControlPoints();
        parametersChanged = true;
    }
    else if (m_highBandPoint.isDragging)
    {
        m_highBandPoint.x = x;
        m_highBandPoint.y = y;
        updateParametersFromControlPoints();
        parametersChanged = true;
    }

    // Preview would update here if enabled
    // TODO: Requires AudioEngine refactoring to support temp buffer playback
    (void)parametersChanged;

    repaint();
}

void GraphicalEQEditor::mouseUp(const juce::MouseEvent& /*event*/)
{
    m_lowBandPoint.isDragging = false;
    m_midBandPoint.isDragging = false;
    m_highBandPoint.isDragging = false;
}

void GraphicalEQEditor::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    auto bounds = getLocalBounds().reduced(MARGIN);
    bounds.removeFromBottom(BUTTON_HEIGHT + MARGIN);

    float x = static_cast<float>(event.x);
    float y = static_cast<float>(event.y);

    // Find nearest control point
    ControlPoint* point = findNearestControlPoint(x, y);
    if (point)
    {
        // Adjust Q value with mouse wheel
        float qDelta = wheel.deltaY * 0.5f;

        if (point->band == BandType::LOW)
        {
            m_params.low.q = juce::jlimit(0.1f, 10.0f, m_params.low.q + qDelta);
        }
        else if (point->band == BandType::MID)
        {
            m_params.mid.q = juce::jlimit(0.1f, 10.0f, m_params.mid.q + qDelta);
        }
        else if (point->band == BandType::HIGH)
        {
            m_params.high.q = juce::jlimit(0.1f, 10.0f, m_params.high.q + qDelta);
        }

        repaint();
    }
}

//==============================================================================
// Coordinate Conversion Helpers

float GraphicalEQEditor::frequencyToX(float frequency, juce::Rectangle<float> bounds) const
{
    // Logarithmic scale
    float logMin = std::log10(m_minFrequency);
    float logMax = std::log10(m_maxFrequency);
    float logFreq = std::log10(juce::jlimit(m_minFrequency, m_maxFrequency, frequency));

    float normalized = (logFreq - logMin) / (logMax - logMin);
    return bounds.getX() + normalized * bounds.getWidth();
}

float GraphicalEQEditor::xToFrequency(float x, juce::Rectangle<float> bounds) const
{
    // Logarithmic scale
    float normalized = (x - bounds.getX()) / bounds.getWidth();
    normalized = juce::jlimit(0.0f, 1.0f, normalized);

    float logMin = std::log10(m_minFrequency);
    float logMax = std::log10(m_maxFrequency);
    float logFreq = logMin + normalized * (logMax - logMin);

    return std::pow(10.0f, logFreq);
}

float GraphicalEQEditor::gainToY(float gainDB, juce::Rectangle<float> bounds) const
{
    // Linear scale
    float normalized = (gainDB - m_minDB) / (m_maxDB - m_minDB);
    normalized = 1.0f - normalized; // Flip Y axis
    return bounds.getY() + normalized * bounds.getHeight();
}

float GraphicalEQEditor::yToGain(float y, juce::Rectangle<float> bounds) const
{
    // Linear scale
    float normalized = (y - bounds.getY()) / bounds.getHeight();
    normalized = 1.0f - normalized; // Flip Y axis
    return m_minDB + normalized * (m_maxDB - m_minDB);
}

//==============================================================================
// Drawing Helpers

void GraphicalEQEditor::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Spectrum visualization is currently disabled (no audio feed)
    // This will be enabled when real-time audio monitoring is implemented
    // For now, we skip drawing the spectrum to avoid visual clutter

    // Future: Re-enable when m_audioEngine provides real-time audio data
    (void)g;
    (void)bounds;
}

void GraphicalEQEditor::drawEQCurve(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Calculate combined frequency response curve with smooth interpolation
    juce::Path curvePath;
    bool firstPoint = true;

    const int numPoints = 1024; // Increased resolution for smoother curves
    std::vector<juce::Point<float>> curvePoints;

    for (int i = 0; i < numPoints; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(numPoints - 1);
        float freq = m_minFrequency * std::pow(m_maxFrequency / m_minFrequency, t);

        // Calculate magnitude response using bell-curve approximation for each band
        float totalGainDB = 0.0f;

        // Low shelf contribution (smooth transition)
        {
            float logDist = std::log10(freq / m_params.low.frequency);
            float bandwidth = 1.0f / m_params.low.q;
            float lowResponse = m_params.low.gain / (1.0f + std::pow(logDist / bandwidth, 2.0f));
            totalGainDB += lowResponse;
        }

        // Mid peak contribution (bell curve)
        {
            float logDist = std::log10(freq / m_params.mid.frequency);
            float bandwidth = 0.5f / m_params.mid.q;
            float midResponse = m_params.mid.gain * std::exp(-std::pow(logDist / bandwidth, 2.0f));
            totalGainDB += midResponse;
        }

        // High shelf contribution (smooth transition)
        {
            float logDist = std::log10(freq / m_params.high.frequency);
            float bandwidth = 1.0f / m_params.high.q;
            float highResponse = m_params.high.gain / (1.0f + std::pow(logDist / bandwidth, 2.0f));
            totalGainDB += highResponse;
        }

        // Convert to screen coordinates
        float x = frequencyToX(freq, bounds);
        float y = gainToY(totalGainDB, bounds);

        curvePoints.push_back(juce::Point<float>(x, y));

        if (firstPoint)
        {
            curvePath.startNewSubPath(x, y);
            firstPoint = false;
        }
        else
        {
            // Use quadratic curves for smoother appearance
            if (i > 0 && i < numPoints - 1)
            {
                // Calculate midpoint for smooth curve
                auto& prev = curvePoints[curvePoints.size() - 2];
                auto& curr = curvePoints[curvePoints.size() - 1];
                curvePath.quadraticTo(prev.x, prev.y, (prev.x + curr.x) * 0.5f, (prev.y + curr.y) * 0.5f);
            }
            curvePath.lineTo(x, y);
        }
    }

    // Draw filled area under curve (subtle)
    juce::Path fillPath = curvePath;
    float zeroY = gainToY(0.0f, bounds);
    fillPath.lineTo(bounds.getRight(), zeroY);
    fillPath.lineTo(bounds.getX(), zeroY);
    fillPath.closeSubPath();

    g.setColour(juce::Colours::cyan.withAlpha(0.15f));
    g.fillPath(fillPath);

    // Draw curve outline
    g.setColour(juce::Colours::cyan.withAlpha(0.9f));
    g.strokePath(curvePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void GraphicalEQEditor::drawControlPoints(juce::Graphics& g, juce::Rectangle<float> /*bounds*/)
{
    // Control point positions are updated in updateControlPointPositions()
    // and during mouse drag. Don't update them here to avoid feedback loops.

    // Draw control points
    auto drawPoint = [&g](const ControlPoint& point, const juce::Colour& colour)
    {
        g.setColour(colour.withAlpha(0.5f));
        g.fillEllipse(point.x - CONTROL_POINT_RADIUS, point.y - CONTROL_POINT_RADIUS,
                      CONTROL_POINT_RADIUS * 2, CONTROL_POINT_RADIUS * 2);

        g.setColour(colour);
        g.drawEllipse(point.x - CONTROL_POINT_RADIUS, point.y - CONTROL_POINT_RADIUS,
                      CONTROL_POINT_RADIUS * 2, CONTROL_POINT_RADIUS * 2, 2.0f);
    };

    drawPoint(m_lowBandPoint, juce::Colours::blue);
    drawPoint(m_midBandPoint, juce::Colours::green);
    drawPoint(m_highBandPoint, juce::Colours::red);
}

void GraphicalEQEditor::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(juce::Colour(0xff3d3d3d));

    // Draw frequency grid lines (logarithmic)
    const std::array<float, 9> frequencies = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000};
    for (float freq : frequencies)
    {
        if (freq >= m_minFrequency && freq <= m_maxFrequency)
        {
            float x = frequencyToX(freq, bounds);
            g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 1.0f);

            // Draw frequency label
            juce::String label = freq < 1000 ? juce::String(static_cast<int>(freq)) + " Hz"
                                             : juce::String(static_cast<int>(freq / 1000)) + " kHz";
            g.setColour(juce::Colours::grey);
            g.drawText(label, static_cast<int>(x) - 30, static_cast<int>(bounds.getBottom()) + 5, 60, 20,
                      juce::Justification::centred);
            g.setColour(juce::Colour(0xff3d3d3d));
        }
    }

    // Draw gain grid lines (linear)
    for (float gainDB = m_minDB; gainDB <= m_maxDB; gainDB += 10.0f)
    {
        float y = gainToY(gainDB, bounds);
        g.drawLine(bounds.getX(), y, bounds.getRight(), y, 1.0f);

        // Draw gain label
        g.setColour(juce::Colours::grey);
        g.drawText(juce::String(static_cast<int>(gainDB)) + " dB",
                  static_cast<int>(bounds.getX()) - 50, static_cast<int>(y) - 10, 45, 20,
                  juce::Justification::right);
        g.setColour(juce::Colour(0xff3d3d3d));
    }

    // Draw 0 dB reference line (highlighted)
    g.setColour(juce::Colour(0xff5d5d5d));
    float zeroY = gainToY(0.0f, bounds);
    g.drawLine(bounds.getX(), zeroY, bounds.getRight(), zeroY, 2.0f);
}

//==============================================================================
// Audio Processing Helpers

void GraphicalEQEditor::pushAudioData(const float* buffer, int numSamples)
{
    juce::SpinLock::ScopedLockType lock(m_fftLock);

    for (int i = 0; i < numSamples; ++i)
    {
        m_fftData[static_cast<size_t>(m_fifoIndex)] = buffer[i];
        m_fifoIndex++;

        if (m_fifoIndex >= FFT_SIZE)
        {
            m_nextFFTBlockReady.store(true);
            m_fifoIndex = 0;
        }
    }
}

void GraphicalEQEditor::processFFT()
{
    juce::SpinLock::ScopedLockType lock(m_fftLock);

    // Apply windowing function
    m_window->multiplyWithWindowingTable(m_fftData.data(), FFT_SIZE);

    // Perform forward FFT for real-valued input
    // Note: This is currently unused as spectrum visualization is disabled
    m_fft->performRealOnlyForwardTransform(m_fftData.data());

    // Convert to dB and smooth
    const int numBins = FFT_SIZE / 2;
    for (int i = 0; i < numBins; ++i)
    {
        float magnitude = m_fftData[static_cast<size_t>(i)];
        float magnitudeDB = juce::Decibels::gainToDecibels(magnitude * FFT_DISPLAY_SCALE, m_minDB);

        // Exponential smoothing
        m_scopeData[static_cast<size_t>(i)] = SMOOTHING_FACTOR * m_scopeData[static_cast<size_t>(i)]
                                            + (1.0f - SMOOTHING_FACTOR) * magnitudeDB;
    }
}

//==============================================================================
// Mouse Interaction Helpers

GraphicalEQEditor::ControlPoint* GraphicalEQEditor::findNearestControlPoint(float x, float y)
{
    float minDistance = std::numeric_limits<float>::max();
    ControlPoint* nearest = nullptr;

    auto checkPoint = [&](ControlPoint& point)
    {
        float dx = x - point.x;
        float dy = y - point.y;
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < CONTROL_POINT_CLICK_THRESHOLD && distance < minDistance)
        {
            minDistance = distance;
            nearest = &point;
        }
    };

    checkPoint(m_lowBandPoint);
    checkPoint(m_midBandPoint);
    checkPoint(m_highBandPoint);

    return nearest;
}

void GraphicalEQEditor::updateControlPointPositions()
{
    auto bounds = getLocalBounds().reduced(MARGIN);
    bounds.removeFromBottom(BUTTON_HEIGHT + MARGIN);
    auto vizBounds = bounds.toFloat();

    m_lowBandPoint.x = frequencyToX(m_params.low.frequency, vizBounds);
    m_lowBandPoint.y = gainToY(m_params.low.gain, vizBounds);

    m_midBandPoint.x = frequencyToX(m_params.mid.frequency, vizBounds);
    m_midBandPoint.y = gainToY(m_params.mid.gain, vizBounds);

    m_highBandPoint.x = frequencyToX(m_params.high.frequency, vizBounds);
    m_highBandPoint.y = gainToY(m_params.high.gain, vizBounds);
}

void GraphicalEQEditor::updateParametersFromControlPoints()
{
    auto bounds = getLocalBounds().reduced(MARGIN);
    bounds.removeFromBottom(BUTTON_HEIGHT + MARGIN);
    auto vizBounds = bounds.toFloat();

    // Update parameters from control point positions
    m_params.low.frequency = xToFrequency(m_lowBandPoint.x, vizBounds);
    m_params.low.gain = yToGain(m_lowBandPoint.y, vizBounds);

    m_params.mid.frequency = xToFrequency(m_midBandPoint.x, vizBounds);
    m_params.mid.gain = yToGain(m_midBandPoint.y, vizBounds);

    m_params.high.frequency = xToFrequency(m_highBandPoint.x, vizBounds);
    m_params.high.gain = yToGain(m_highBandPoint.y, vizBounds);

    // Clamp to valid ranges
    double sampleRate = m_sampleRate.load();
    double nyquist = sampleRate / 2.0;

    m_params.low.frequency = juce::jlimit(20.0f, static_cast<float>(nyquist * 0.49), m_params.low.frequency);
    m_params.low.gain = juce::jlimit(-20.0f, 20.0f, m_params.low.gain);

    m_params.mid.frequency = juce::jlimit(20.0f, static_cast<float>(nyquist * 0.49), m_params.mid.frequency);
    m_params.mid.gain = juce::jlimit(-20.0f, 20.0f, m_params.mid.gain);

    m_params.high.frequency = juce::jlimit(20.0f, static_cast<float>(nyquist * 0.49), m_params.high.frequency);
    m_params.high.gain = juce::jlimit(-20.0f, 20.0f, m_params.high.gain);
}

//==============================================================================
// Preview Methods

void GraphicalEQEditor::processPreviewAudio()
{
    // TODO: Implement preview audio processing
    // Requires AudioEngine refactoring to support temporary buffer playback
}

void GraphicalEQEditor::togglePreview()
{
    // TODO: Implement preview playback
    // Requires AudioEngine refactoring to support temporary buffer playback
    // For now, show a message explaining this
    juce::AlertWindow::showMessageBoxAsync(
        juce::MessageBoxIconType::InfoIcon,
        "Preview Not Available",
        "Real-time preview functionality requires AudioEngine refactoring\n"
        "to support temporary buffer playback without modifying the main buffer.\n\n"
        "For now, you can:\n"
        "1. Adjust the EQ curve visually\n"
        "2. Click Apply to hear the result\n"
        "3. Use Undo (Cmd+Z) if you don't like it",
        "OK"
    );
}
