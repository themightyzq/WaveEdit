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
#include <iostream>

// Static state persistence for dialog reopens (Phase 6 finalization)
// These persist bypass and loop toggle states across dialog instances
static bool s_lastBypassState = false;
static bool s_lastLoopState = true;  // Default ON per CLAUDE.md Protocol

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
    constexpr float CONTROL_POINT_RADIUS = 10.0f;
    constexpr float CONTROL_POINT_CLICK_THRESHOLD = 18.0f;

    /**
     * Dialog window dimensions
     */
    constexpr int DIALOG_WIDTH = 900;
    constexpr int DIALOG_HEIGHT = 650;
    constexpr int BUTTON_HEIGHT = 30;
    constexpr int BUTTON_WIDTH = 100;
    constexpr int MARGIN = 10;
}

//==============================================================================
// Constructor / Destructor

GraphicalEQEditor::GraphicalEQEditor(const DynamicParametricEQ::Parameters& initialParams)
    : m_params(initialParams)
    , m_fft(std::make_unique<juce::dsp::FFT>(FFT_ORDER))
    , m_window(std::make_unique<juce::dsp::WindowingFunction<float>>(
        FFT_SIZE, juce::dsp::WindowingFunction<float>::hann))
    , m_fifoIndex(0)
    , m_nextFFTBlockReady(false)
    , m_audioEngine(nullptr)
    , m_minFrequency(20.0f)
    , m_maxFrequency(20000.0f)
    , m_minDB(-30.0f)
    , m_maxDB(30.0f)
    , m_previewButton("Preview")
    , m_applyButton("Apply")
    , m_cancelButton("Cancel")
{
    m_sampleRate.store(44100.0);

    // Initialize FFT buffers
    m_fftData.fill(0.0f);
    m_scopeData.fill(0.0f);

    // Create the EQ processor for accurate frequency response calculation
    m_eqProcessor = std::make_unique<DynamicParametricEQ>();
    m_eqProcessor->prepare(44100.0, 512);
    m_eqProcessor->setParameters(m_params);
    m_eqProcessor->updateCoefficientsForVisualization();  // Force coefficient creation for initial curve display

    // Setup buttons
    m_previewButton.onClick = [this]()
    {
        togglePreview();
    };
    m_previewButton.setEnabled(true);
    addAndMakeVisible(m_previewButton);

    // Bypass button (starts disabled, enabled only during preview)
    m_bypassButton.setButtonText("Bypass");
    m_bypassButton.onClick = [this]() { onBypassClicked(); };
    m_bypassButton.setEnabled(false);  // Disabled until preview starts
    addAndMakeVisible(m_bypassButton);

    // Loop toggle - restore persisted state
    m_loopToggle.setButtonText("Loop");
    m_loopToggle.setToggleState(s_lastLoopState, juce::dontSendNotification);  // Restore persisted state
    m_loopToggle.onStateChange = [this]() {
        s_lastLoopState = m_loopToggle.getToggleState();  // Save state on change
    };
    addAndMakeVisible(m_loopToggle);

    m_applyButton.onClick = [this]()
    {
        m_result = m_params;

        juce::Logger::writeToLog("GraphicalEQ Apply clicked:");
        juce::Logger::writeToLog("  " + juce::String(m_params.bands.size()) + " bands");
        for (size_t i = 0; i < m_params.bands.size(); ++i)
        {
            const auto& band = m_params.bands[i];
            juce::Logger::writeToLog("  Band " + juce::String(i) + ": " +
                juce::String(band.frequency) + " Hz, " +
                juce::String(band.gain) + " dB, Q=" + juce::String(band.q) +
                ", Type=" + DynamicParametricEQ::getFilterTypeName(band.filterType));
        }

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

    // Setup preset controls
    m_presetComboBox.setTextWhenNothingSelected("Select Preset...");
    m_presetComboBox.onChange = [this]() { presetSelected(); };
    addAndMakeVisible(m_presetComboBox);

    m_savePresetButton.setButtonText("Save");
    m_savePresetButton.onClick = [this]() { savePreset(); };
    addAndMakeVisible(m_savePresetButton);

    m_deletePresetButton.setButtonText("Delete");
    m_deletePresetButton.onClick = [this]() { deletePreset(); };
    addAndMakeVisible(m_deletePresetButton);

    m_resetButton.setButtonText("Reset");
    m_resetButton.onClick = [this]() { resetToFlat(); };
    addAndMakeVisible(m_resetButton);

    // Populate preset list
    refreshPresetList();

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

    // Ensure preview is disabled and unregister from AudioEngine when dialog closes
    // CRITICAL: Must stop playback and disable preview mode - same pattern as GainDialog/ParametricEQDialog
    if (m_audioEngine)
    {
        // Unregister from receiving audio data for spectrum visualization
        m_audioEngine->setGraphicalEQEditor(nullptr);

        if (m_previewActive)
        {
            // FIX: Must stop audio playback when closing dialog (was missing before)
            m_audioEngine->stop();
            m_audioEngine->setDynamicEQPreview(DynamicParametricEQ::Parameters(), false);
            m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
            m_audioEngine->setPreviewBypassed(false);  // Reset bypass state
            m_previewActive = false;
        }
    }
}

//==============================================================================
// Public API

std::optional<DynamicParametricEQ::Parameters> GraphicalEQEditor::showDialog(
    AudioEngine* audioEngine,
    const DynamicParametricEQ::Parameters& initialParams,
    int64_t selectionStart,
    int64_t selectionEnd)
{
    GraphicalEQEditor editor(initialParams);
    editor.setAudioEngine(audioEngine);

    // Store selection bounds for preview positioning
    editor.m_selectionStart = selectionStart;
    editor.m_selectionEnd = selectionEnd;

    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(&editor);
    options.dialogTitle = "Graphical Parametric EQ (20-Band)";
    options.dialogBackgroundColour = juce::Colour(0xff2d2d2d);
    options.escapeKeyTriggersCloseButton = false;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = nullptr;

    #if JUCE_MODAL_LOOPS_PERMITTED
        int result = options.runModal();
        return (result == 1) ? editor.m_result : std::nullopt;
    #else
        jassertfalse;
        return std::nullopt;
    #endif
}

void GraphicalEQEditor::setAudioEngine(AudioEngine* audioEngine)
{
    m_audioEngine = audioEngine;
    if (audioEngine)
    {
        double sr = audioEngine->getSampleRate();
        // Only update sample rate if valid - AudioEngine may return 0 if no file is loaded
        if (sr > 0)
        {
            m_sampleRate.store(sr);
            if (m_eqProcessor)
                m_eqProcessor->prepare(sr, 512);
        }
    }
}

//==============================================================================
// Component Overrides

void GraphicalEQEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));

    // Calculate visualization bounds (reserve space for preset controls, axis labels, and buttons)
    // Must match the layout in resized()
    constexpr int AXIS_LABEL_HEIGHT = 20;
    constexpr int FOOTER_SPACING = 10;

    auto bounds = getLocalBounds().reduced(MARGIN);
    bounds.removeFromTop(BUTTON_HEIGHT);  // Preset controls
    bounds.removeFromTop(MARGIN);         // Spacing
    bounds.removeFromBottom(BUTTON_HEIGHT + FOOTER_SPACING + AXIS_LABEL_HEIGHT);  // Footer area

    auto vizBounds = bounds.toFloat();

    // Draw grid and axes first
    drawGrid(g, vizBounds);

    // Draw spectrum analyzer background
    drawSpectrum(g, vizBounds);

    // Draw EQ frequency response curve (accurate using DynamicParametricEQ)
    drawEQCurve(g, vizBounds);

    // Draw control points on top
    drawControlPoints(g, vizBounds);

    // Draw instructions
    g.setColour(juce::Colours::grey);
    g.setFont(11.0f);
    g.drawText("Double-click to add band | Right-click band to delete | Scroll wheel to adjust Q",
               vizBounds.getX(), vizBounds.getBottom() - 20, vizBounds.getWidth(), 20,
               juce::Justification::centred);
}

void GraphicalEQEditor::resized()
{
    auto bounds = getLocalBounds().reduced(MARGIN);

    // Position preset controls at top-right
    auto presetArea = bounds.removeFromTop(BUTTON_HEIGHT);
    presetArea.removeFromLeft(presetArea.getWidth() - 400);  // Leave 400px on the right for presets

    m_presetComboBox.setBounds(presetArea.removeFromLeft(180));
    presetArea.removeFromLeft(MARGIN / 2);
    m_savePresetButton.setBounds(presetArea.removeFromLeft(60));
    presetArea.removeFromLeft(MARGIN / 2);
    m_deletePresetButton.setBounds(presetArea.removeFromLeft(60));
    presetArea.removeFromLeft(MARGIN / 2);
    m_resetButton.setBounds(presetArea.removeFromLeft(60));

    bounds.removeFromTop(MARGIN);  // Spacing between preset controls and EQ display

    // Reserve space at bottom for:
    // 1. Frequency axis labels (20px)
    // 2. Spacing (10px)
    // 3. Buttons (30px)
    // Total footer height: 60px
    constexpr int AXIS_LABEL_HEIGHT = 20;
    constexpr int FOOTER_SPACING = 10;

    auto footerArea = bounds.removeFromBottom(BUTTON_HEIGHT + FOOTER_SPACING + AXIS_LABEL_HEIGHT);

    // Skip the axis label area (visualization will draw labels here)
    footerArea.removeFromTop(AXIS_LABEL_HEIGHT);
    footerArea.removeFromTop(FOOTER_SPACING);

    // Position buttons at bottom - standardized layout
    // Left: Preview + Bypass + Loop | Right: Cancel + Apply
    auto buttonArea = footerArea;
    const int buttonSpacing = 10;

    // Left side: Preview, Bypass, and Loop toggle (standardized order)
    m_previewButton.setBounds(buttonArea.removeFromLeft(90));  // Standard width
    buttonArea.removeFromLeft(buttonSpacing);
    m_bypassButton.setBounds(buttonArea.removeFromLeft(70));   // Slightly narrower for bypass
    buttonArea.removeFromLeft(buttonSpacing);
    m_loopToggle.setBounds(buttonArea.removeFromLeft(60));     // Loop toggle

    // Right side: Cancel and Apply buttons
    m_applyButton.setBounds(buttonArea.removeFromRight(90));   // Standard width
    buttonArea.removeFromRight(buttonSpacing);
    m_cancelButton.setBounds(buttonArea.removeFromRight(90));  // Standard width

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

    // Handle right-click for context menu
    if (event.mods.isRightButtonDown())
    {
        int bandIndex = findNearestControlPointIndex(x, y);
        if (bandIndex >= 0)
        {
            showBandContextMenu(bandIndex);
        }
        return;
    }

    // Find nearest control point for dragging
    int bandIndex = findNearestControlPointIndex(x, y);
    if (bandIndex >= 0)
    {
        m_draggingBandIndex = bandIndex;
        if (static_cast<size_t>(bandIndex) < m_controlPoints.size())
        {
            m_controlPoints[static_cast<size_t>(bandIndex)].isDragging = true;
        }
    }
}

void GraphicalEQEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (m_draggingBandIndex < 0 || static_cast<size_t>(m_draggingBandIndex) >= m_controlPoints.size())
        return;

    auto bounds = getLocalBounds().reduced(MARGIN);
    bounds.removeFromBottom(BUTTON_HEIGHT + MARGIN);
    auto vizBounds = bounds.toFloat();

    float x = juce::jlimit(vizBounds.getX(), vizBounds.getRight(), static_cast<float>(event.x));
    float y = juce::jlimit(vizBounds.getY(), vizBounds.getBottom(), static_cast<float>(event.y));

    // Update control point position
    m_controlPoints[static_cast<size_t>(m_draggingBandIndex)].x = x;
    m_controlPoints[static_cast<size_t>(m_draggingBandIndex)].y = y;

    // Update parameters from control point
    updateParametersFromControlPoint(m_draggingBandIndex);

    // Update EQ processor for accurate frequency response
    if (m_eqProcessor)
    {
        m_eqProcessor->setParameters(m_params);
        m_eqProcessor->updateCoefficientsForVisualization();
    }

    // Update preview in real-time if active
    if (m_previewActive && m_audioEngine)
    {
        m_audioEngine->setDynamicEQPreview(m_params, true);
    }

    repaint();
}

void GraphicalEQEditor::mouseUp(const juce::MouseEvent& /*event*/)
{
    if (m_draggingBandIndex >= 0 && static_cast<size_t>(m_draggingBandIndex) < m_controlPoints.size())
    {
        m_controlPoints[static_cast<size_t>(m_draggingBandIndex)].isDragging = false;
    }
    m_draggingBandIndex = -1;
}

void GraphicalEQEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    auto bounds = getLocalBounds().reduced(MARGIN);
    bounds.removeFromBottom(BUTTON_HEIGHT + MARGIN);

    float x = static_cast<float>(event.x);
    float y = static_cast<float>(event.y);

    // Check if clicking on existing control point - if so, toggle enabled
    int bandIndex = findNearestControlPointIndex(x, y);
    if (bandIndex >= 0 && static_cast<size_t>(bandIndex) < m_params.bands.size())
    {
        m_params.bands[static_cast<size_t>(bandIndex)].enabled =
            !m_params.bands[static_cast<size_t>(bandIndex)].enabled;

        // Update EQ processor
        if (m_eqProcessor)
        {
            m_eqProcessor->setParameters(m_params);
            m_eqProcessor->updateCoefficientsForVisualization();
        }

        // Update preview if active
        if (m_previewActive && m_audioEngine)
        {
            m_audioEngine->setDynamicEQPreview(m_params, true);
        }

        repaint();
        return;
    }

    // Add new band at click position
    if (bounds.contains(event.x, event.y))
    {
        addBandAtPosition(x, y);
    }
}

void GraphicalEQEditor::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    float x = static_cast<float>(event.x);
    float y = static_cast<float>(event.y);

    // Find nearest control point
    int bandIndex = findNearestControlPointIndex(x, y);
    if (bandIndex >= 0 && static_cast<size_t>(bandIndex) < m_params.bands.size())
    {
        // Adjust Q value with mouse wheel
        float qDelta = wheel.deltaY * 0.5f;
        auto& band = m_params.bands[static_cast<size_t>(bandIndex)];
        band.q = juce::jlimit(DynamicParametricEQ::MIN_Q, DynamicParametricEQ::MAX_Q, band.q + qDelta);

        // Update EQ processor
        if (m_eqProcessor)
        {
            m_eqProcessor->setParameters(m_params);
            m_eqProcessor->updateCoefficientsForVisualization();
        }

        // Update preview if active
        if (m_previewActive && m_audioEngine)
        {
            m_audioEngine->setDynamicEQPreview(m_params, true);
        }

        repaint();
    }
}

//==============================================================================
// Coordinate Conversion Helpers

float GraphicalEQEditor::frequencyToX(float frequency, juce::Rectangle<float> bounds) const
{
    float logMin = std::log10(m_minFrequency);
    float logMax = std::log10(m_maxFrequency);
    float logFreq = std::log10(juce::jlimit(m_minFrequency, m_maxFrequency, frequency));

    float normalized = (logFreq - logMin) / (logMax - logMin);
    return bounds.getX() + normalized * bounds.getWidth();
}

float GraphicalEQEditor::xToFrequency(float x, juce::Rectangle<float> bounds) const
{
    float normalized = (x - bounds.getX()) / bounds.getWidth();
    normalized = juce::jlimit(0.0f, 1.0f, normalized);

    float logMin = std::log10(m_minFrequency);
    float logMax = std::log10(m_maxFrequency);
    float logFreq = logMin + normalized * (logMax - logMin);

    return std::pow(10.0f, logFreq);
}

float GraphicalEQEditor::gainToY(float gainDB, juce::Rectangle<float> bounds) const
{
    float normalized = (gainDB - m_minDB) / (m_maxDB - m_minDB);
    normalized = 1.0f - normalized; // Flip Y axis
    return bounds.getY() + normalized * bounds.getHeight();
}

float GraphicalEQEditor::yToGain(float y, juce::Rectangle<float> bounds) const
{
    float normalized = (y - bounds.getY()) / bounds.getHeight();
    normalized = 1.0f - normalized; // Flip Y axis
    return m_minDB + normalized * (m_maxDB - m_minDB);
}

//==============================================================================
// Drawing Helpers

void GraphicalEQEditor::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    // Only draw spectrum if we have FFT data ready
    if (!m_nextFFTBlockReady.load() && m_scopeData[0] == 0.0f)
        return;

    // Draw spectrum as a semi-transparent filled area
    juce::Path spectrumPath;
    bool firstPoint = true;

    const int numBins = FFT_SIZE / 2;
    const double sampleRate = m_sampleRate.load();
    const double nyquist = sampleRate / 2.0;

    for (int i = 1; i < numBins; ++i)
    {
        // Calculate frequency for this bin
        double freq = static_cast<double>(i) * nyquist / static_cast<double>(numBins);

        // Skip frequencies outside our display range
        if (freq < static_cast<double>(m_minFrequency) || freq > static_cast<double>(m_maxFrequency))
            continue;

        // Get magnitude in dB (already computed by processFFT)
        float magnitudeDB = m_scopeData[static_cast<size_t>(i)];

        // Convert to screen coordinates
        float x = frequencyToX(static_cast<float>(freq), bounds);
        float y = gainToY(magnitudeDB, bounds);

        // Clamp Y to bounds
        y = juce::jlimit(bounds.getY(), bounds.getBottom(), y);

        if (firstPoint)
        {
            spectrumPath.startNewSubPath(x, y);
            firstPoint = false;
        }
        else
        {
            spectrumPath.lineTo(x, y);
        }
    }

    if (!firstPoint)
    {
        // Close the path to create a filled area
        juce::Path fillPath = spectrumPath;
        float bottomY = bounds.getBottom();
        fillPath.lineTo(bounds.getRight(), bottomY);
        fillPath.lineTo(bounds.getX(), bottomY);
        fillPath.closeSubPath();

        // Draw filled spectrum (subtle purple/blue gradient)
        g.setColour(juce::Colour(0x20a040c0));  // Semi-transparent purple-blue
        g.fillPath(fillPath);

        // Draw spectrum outline
        g.setColour(juce::Colour(0x60a060ff));  // Brighter purple-blue
        g.strokePath(spectrumPath, juce::PathStrokeType(1.0f));
    }
}

void GraphicalEQEditor::drawEQCurve(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (m_params.bands.empty())
        return;

    // Use valid sample rate - fall back to 44100 if m_sampleRate is invalid
    double sampleRate = m_sampleRate.load();
    if (sampleRate <= 0)
        sampleRate = 44100.0;

    // CRITICAL FIX: Create a FRESH temporary processor each paint cycle.
    // This ensures the processor always has the exact current parameters
    // and properly calculates coefficients with a valid sample rate.
    // The member m_eqProcessor was causing issues because:
    // 1. setParameters() has an early return if params haven't changed
    // 2. But the UI may have changed m_params directly during drag operations
    // 3. Creating a fresh processor guarantees all state is properly initialized
    auto tempProcessor = std::make_unique<DynamicParametricEQ>();
    tempProcessor->prepare(sampleRate, 512);
    tempProcessor->setParameters(m_params);
    tempProcessor->updateCoefficientsForVisualization();

    // Get frequency response from the temporary processor
    const int numPoints = 512;
    std::vector<float> magnitudes(static_cast<size_t>(numPoints));

    tempProcessor->getFrequencyResponse(magnitudes.data(), numPoints,
        static_cast<double>(m_minFrequency), static_cast<double>(m_maxFrequency), true);

    // Build the path
    juce::Path curvePath;
    bool firstPoint = true;

    for (int i = 0; i < numPoints; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(numPoints - 1);
        float freq = m_minFrequency * std::pow(m_maxFrequency / m_minFrequency, t);

        float x = frequencyToX(freq, bounds);
        float y = gainToY(magnitudes[static_cast<size_t>(i)], bounds);

        // Clamp Y to bounds
        y = juce::jlimit(bounds.getY(), bounds.getBottom(), y);

        if (firstPoint)
        {
            curvePath.startNewSubPath(x, y);
            firstPoint = false;
        }
        else
        {
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
    for (size_t i = 0; i < m_controlPoints.size() && i < m_params.bands.size(); ++i)
    {
        const auto& point = m_controlPoints[i];
        const auto& band = m_params.bands[i];

        juce::Colour colour = getFilterTypeColor(band.filterType);

        // Dim if disabled
        if (!band.enabled)
            colour = colour.withAlpha(0.3f);

        // Draw control point
        float radius = point.isDragging ? CONTROL_POINT_RADIUS * 1.3f : CONTROL_POINT_RADIUS;

        g.setColour(colour.withAlpha(0.5f));
        g.fillEllipse(point.x - radius, point.y - radius, radius * 2, radius * 2);

        g.setColour(colour);
        g.drawEllipse(point.x - radius, point.y - radius, radius * 2, radius * 2, 2.0f);

        // Draw filter type label above the control point
        g.setColour(juce::Colours::white);
        g.setFont(9.0f);
        g.drawText(DynamicParametricEQ::getFilterTypeShortName(band.filterType),
                   static_cast<int>(point.x) - 12, static_cast<int>(point.y) - 25, 24, 14,
                   juce::Justification::centred);

        // Draw info label showing frequency and gain below the control point
        // Format frequency: show Hz for < 1kHz, kHz for >= 1kHz
        juce::String freqStr;
        if (band.frequency >= 1000.0f)
            freqStr = juce::String(band.frequency / 1000.0f, 1) + "k";
        else if (band.frequency >= 100.0f)
            freqStr = juce::String(static_cast<int>(band.frequency));
        else
            freqStr = juce::String(band.frequency, 1);

        // Format gain with sign and 1 decimal
        juce::String gainStr = (band.gain >= 0 ? "+" : "") + juce::String(band.gain, 1) + "dB";

        // Combine into info label
        juce::String infoLabel = freqStr + " Hz\n" + gainStr;

        // Draw info label in a semi-transparent background box below control point
        g.setFont(10.0f);
        int labelWidth = 60;
        int labelHeight = 28;
        int labelX = static_cast<int>(point.x) - labelWidth / 2;
        int labelY = static_cast<int>(point.y) + static_cast<int>(radius) + 4;

        // Background box
        g.setColour(juce::Colour(0xcc1e1e1e));  // Semi-transparent dark background
        g.fillRoundedRectangle(static_cast<float>(labelX), static_cast<float>(labelY),
                               static_cast<float>(labelWidth), static_cast<float>(labelHeight), 3.0f);

        // Border
        g.setColour(colour.withAlpha(0.6f));
        g.drawRoundedRectangle(static_cast<float>(labelX), static_cast<float>(labelY),
                               static_cast<float>(labelWidth), static_cast<float>(labelHeight), 3.0f, 1.0f);

        // Text - frequency on first line
        g.setColour(juce::Colours::white);
        g.setFont(9.0f);
        g.drawText(freqStr + " Hz", labelX, labelY + 2, labelWidth, 12, juce::Justification::centred);

        // Text - gain on second line
        g.setColour(band.gain > 0 ? juce::Colours::lightgreen : (band.gain < 0 ? juce::Colours::lightsalmon : juce::Colours::grey));
        g.drawText(gainStr, labelX, labelY + 14, labelWidth, 12, juce::Justification::centred);
    }
}

void GraphicalEQEditor::drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour(juce::Colour(0xff3d3d3d));

    // Draw frequency grid lines (logarithmic)
    const std::array<float, 10> frequencies = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    for (float freq : frequencies)
    {
        if (freq >= m_minFrequency && freq <= m_maxFrequency)
        {
            float x = frequencyToX(freq, bounds);
            g.drawLine(x, bounds.getY(), x, bounds.getBottom(), 1.0f);

            // Draw frequency label
            juce::String label = freq < 1000 ? juce::String(static_cast<int>(freq)) + " Hz"
                                             : juce::String(static_cast<int>(freq / 1000)) + "k";
            g.setColour(juce::Colours::grey);
            g.setFont(10.0f);
            g.drawText(label, static_cast<int>(x) - 20, static_cast<int>(bounds.getBottom()) + 2, 40, 16,
                      juce::Justification::centred);
            g.setColour(juce::Colour(0xff3d3d3d));
        }
    }

    // Draw gain grid lines (linear, every 6 dB)
    for (float gainDB = m_minDB; gainDB <= m_maxDB; gainDB += 6.0f)
    {
        float y = gainToY(gainDB, bounds);
        g.drawLine(bounds.getX(), y, bounds.getRight(), y, 1.0f);

        // Draw gain label
        g.setColour(juce::Colours::grey);
        g.setFont(10.0f);
        g.drawText(juce::String(static_cast<int>(gainDB)) + " dB",
                  static_cast<int>(bounds.getX()) - 45, static_cast<int>(y) - 8, 40, 16,
                  juce::Justification::right);
        g.setColour(juce::Colour(0xff3d3d3d));
    }

    // Draw 0 dB reference line (highlighted)
    g.setColour(juce::Colour(0xff6d6d6d));
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

    m_window->multiplyWithWindowingTable(m_fftData.data(), FFT_SIZE);
    m_fft->performRealOnlyForwardTransform(m_fftData.data());

    const int numBins = FFT_SIZE / 2;
    for (int i = 0; i < numBins; ++i)
    {
        float magnitude = m_fftData[static_cast<size_t>(i)];
        float magnitudeDB = juce::Decibels::gainToDecibels(magnitude * FFT_DISPLAY_SCALE, m_minDB);

        m_scopeData[static_cast<size_t>(i)] = SMOOTHING_FACTOR * m_scopeData[static_cast<size_t>(i)]
                                            + (1.0f - SMOOTHING_FACTOR) * magnitudeDB;
    }
}

//==============================================================================
// Mouse Interaction Helpers

int GraphicalEQEditor::findNearestControlPointIndex(float x, float y)
{
    float minDistance = std::numeric_limits<float>::max();
    int nearestIndex = -1;

    for (size_t i = 0; i < m_controlPoints.size(); ++i)
    {
        const auto& point = m_controlPoints[i];
        float dx = x - point.x;
        float dy = y - point.y;
        float distance = std::sqrt(dx * dx + dy * dy);

        if (distance < CONTROL_POINT_CLICK_THRESHOLD && distance < minDistance)
        {
            minDistance = distance;
            nearestIndex = static_cast<int>(i);
        }
    }

    return nearestIndex;
}

void GraphicalEQEditor::updateControlPointPositions()
{
    auto bounds = getLocalBounds().reduced(MARGIN);
    bounds.removeFromBottom(BUTTON_HEIGHT + MARGIN);
    auto vizBounds = bounds.toFloat();

    // Resize control points vector to match bands
    m_controlPoints.resize(m_params.bands.size());

    // Update each control point position
    for (size_t i = 0; i < m_params.bands.size(); ++i)
    {
        const auto& band = m_params.bands[i];
        m_controlPoints[i].bandIndex = static_cast<int>(i);
        m_controlPoints[i].x = frequencyToX(band.frequency, vizBounds);
        m_controlPoints[i].y = gainToY(band.gain, vizBounds);
    }
}

void GraphicalEQEditor::updateParametersFromControlPoint(int bandIndex)
{
    if (bandIndex < 0 || static_cast<size_t>(bandIndex) >= m_params.bands.size())
        return;

    auto bounds = getLocalBounds().reduced(MARGIN);
    bounds.removeFromBottom(BUTTON_HEIGHT + MARGIN);
    auto vizBounds = bounds.toFloat();

    const auto& point = m_controlPoints[static_cast<size_t>(bandIndex)];
    auto& band = m_params.bands[static_cast<size_t>(bandIndex)];

    // Update frequency and gain from control point position
    band.frequency = xToFrequency(point.x, vizBounds);
    band.gain = yToGain(point.y, vizBounds);

    // Clamp to valid ranges
    double sampleRate = m_sampleRate.load();
    double nyquist = sampleRate / 2.0;

    band.frequency = juce::jlimit(DynamicParametricEQ::MIN_FREQUENCY,
                                   static_cast<float>(nyquist * 0.49f),
                                   band.frequency);
    band.gain = juce::jlimit(DynamicParametricEQ::MIN_GAIN,
                              DynamicParametricEQ::MAX_GAIN,
                              band.gain);
}

void GraphicalEQEditor::addBandAtPosition(float x, float y)
{
    if (static_cast<int>(m_params.bands.size()) >= DynamicParametricEQ::MAX_BANDS)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Maximum Bands Reached",
            "Cannot add more than " + juce::String(DynamicParametricEQ::MAX_BANDS) + " bands.",
            "OK"
        );
        return;
    }

    auto bounds = getLocalBounds().reduced(MARGIN);
    bounds.removeFromBottom(BUTTON_HEIGHT + MARGIN);
    auto vizBounds = bounds.toFloat();

    // Create new band at click position
    DynamicParametricEQ::BandParameters newBand;
    newBand.frequency = xToFrequency(x, vizBounds);
    newBand.gain = yToGain(y, vizBounds);
    newBand.q = DynamicParametricEQ::DEFAULT_Q;
    newBand.filterType = DynamicParametricEQ::FilterType::Bell;
    newBand.enabled = true;

    // Clamp frequency
    double sampleRate = m_sampleRate.load();
    double nyquist = sampleRate / 2.0;
    newBand.frequency = juce::jlimit(DynamicParametricEQ::MIN_FREQUENCY,
                                      static_cast<float>(nyquist * 0.49f),
                                      newBand.frequency);

    // Add the band
    m_params.bands.push_back(newBand);

    // Update control points
    updateControlPointPositions();

    // Update EQ processor
    if (m_eqProcessor)
    {
        m_eqProcessor->setParameters(m_params);
        m_eqProcessor->updateCoefficientsForVisualization();
    }

    // Update preview if active
    if (m_previewActive && m_audioEngine)
    {
        m_audioEngine->setDynamicEQPreview(m_params, true);
    }

    repaint();
}

void GraphicalEQEditor::removeBand(int bandIndex)
{
    if (bandIndex < 0 || static_cast<size_t>(bandIndex) >= m_params.bands.size())
        return;

    m_params.bands.erase(m_params.bands.begin() + bandIndex);

    // Update control points
    updateControlPointPositions();

    // Update EQ processor
    if (m_eqProcessor)
    {
        m_eqProcessor->setParameters(m_params);
        m_eqProcessor->updateCoefficientsForVisualization();
    }

    // Update preview if active
    if (m_previewActive && m_audioEngine)
    {
        m_audioEngine->setDynamicEQPreview(m_params, true);
    }

    repaint();
}

juce::Colour GraphicalEQEditor::getFilterTypeColor(DynamicParametricEQ::FilterType type) const
{
    switch (type)
    {
        case DynamicParametricEQ::FilterType::Bell:      return juce::Colours::green;
        case DynamicParametricEQ::FilterType::LowShelf:  return juce::Colours::blue;
        case DynamicParametricEQ::FilterType::HighShelf: return juce::Colours::red;
        case DynamicParametricEQ::FilterType::LowCut:    return juce::Colours::purple;
        case DynamicParametricEQ::FilterType::HighCut:   return juce::Colours::orange;
        case DynamicParametricEQ::FilterType::Notch:     return juce::Colours::cyan;
        case DynamicParametricEQ::FilterType::Bandpass:  return juce::Colours::yellow;
        default:                                          return juce::Colours::white;
    }
}

void GraphicalEQEditor::showBandContextMenu(int bandIndex)
{
    if (bandIndex < 0 || static_cast<size_t>(bandIndex) >= m_params.bands.size())
        return;

    auto& band = m_params.bands[static_cast<size_t>(bandIndex)];

    juce::PopupMenu menu;

    // Filter type submenu
    juce::PopupMenu typeMenu;
    typeMenu.addItem(1, "Bell", true, band.filterType == DynamicParametricEQ::FilterType::Bell);
    typeMenu.addItem(2, "Low Shelf", true, band.filterType == DynamicParametricEQ::FilterType::LowShelf);
    typeMenu.addItem(3, "High Shelf", true, band.filterType == DynamicParametricEQ::FilterType::HighShelf);
    typeMenu.addItem(4, "Low Cut (HP)", true, band.filterType == DynamicParametricEQ::FilterType::LowCut);
    typeMenu.addItem(5, "High Cut (LP)", true, band.filterType == DynamicParametricEQ::FilterType::HighCut);
    typeMenu.addItem(6, "Notch", true, band.filterType == DynamicParametricEQ::FilterType::Notch);
    typeMenu.addItem(7, "Bandpass", true, band.filterType == DynamicParametricEQ::FilterType::Bandpass);
    menu.addSubMenu("Filter Type", typeMenu);

    menu.addSeparator();

    menu.addItem(10, band.enabled ? "Disable Band" : "Enable Band");
    menu.addItem(11, "Reset to 0 dB");
    menu.addItem(12, "Set Gain...");

    menu.addSeparator();

    menu.addItem(20, "Delete Band");

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, bandIndex](int result)
    {
        if (result == 0 || static_cast<size_t>(bandIndex) >= m_params.bands.size())
            return;

        auto& band = m_params.bands[static_cast<size_t>(bandIndex)];

        if (result >= 1 && result <= 7)
        {
            // Filter type selection
            band.filterType = static_cast<DynamicParametricEQ::FilterType>(result - 1);
        }
        else if (result == 10)
        {
            band.enabled = !band.enabled;
        }
        else if (result == 11)
        {
            band.gain = 0.0f;
            updateControlPointPositions();
        }
        else if (result == 12)
        {
            // Set Gain... - show dialog to input specific gain value
            auto alertWindow = std::make_unique<juce::AlertWindow>(
                "Set Gain",
                "Enter gain value in dB (range: " + juce::String(DynamicParametricEQ::MIN_GAIN, 1) + " to " + juce::String(DynamicParametricEQ::MAX_GAIN, 1) + " dB):",
                juce::MessageBoxIconType::QuestionIcon);

            alertWindow->addTextEditor("gain", juce::String(band.gain, 1), "Gain (dB):");
            alertWindow->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
            alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

            auto* gainEditor = alertWindow->getTextEditor("gain");
            if (gainEditor)
            {
                gainEditor->setInputRestrictions(10, "-0123456789.");
            }

            alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
                [this, bandIndex, alertWindow = alertWindow.release()](int returnValue)
                {
                    std::unique_ptr<juce::AlertWindow> aw(alertWindow);
                    if (returnValue == 1 && static_cast<size_t>(bandIndex) < m_params.bands.size())
                    {
                        auto* editor = aw->getTextEditor("gain");
                        if (editor)
                        {
                            float newGain = editor->getText().getFloatValue();
                            // Clamp to valid range
                            newGain = juce::jlimit(DynamicParametricEQ::MIN_GAIN, DynamicParametricEQ::MAX_GAIN, newGain);
                            m_params.bands[static_cast<size_t>(bandIndex)].gain = newGain;
                            updateControlPointPositions();

                            // Update EQ processor and repaint
                            if (m_eqProcessor)
                            {
                                m_eqProcessor->setParameters(m_params);
                                m_eqProcessor->updateCoefficientsForVisualization();
                            }

                            // Update preview if active
                            if (m_previewActive && m_audioEngine)
                            {
                                m_audioEngine->setDynamicEQPreview(m_params, true);
                            }

                            repaint();
                        }
                    }
                }), true);
            return;  // Don't fall through to the common update code below
        }
        else if (result == 20)
        {
            removeBand(bandIndex);
            return; // removeBand already calls repaint
        }

        // Update EQ processor and repaint
        if (m_eqProcessor)
        {
            m_eqProcessor->setParameters(m_params);
            m_eqProcessor->updateCoefficientsForVisualization();
        }

        // Update preview if active
        if (m_previewActive && m_audioEngine)
        {
            m_audioEngine->setDynamicEQPreview(m_params, true);
        }

        repaint();
    });
}

//==============================================================================
// Preview Methods

void GraphicalEQEditor::processPreviewAudio()
{
    // TODO: Implement preview audio processing
}

void GraphicalEQEditor::togglePreview()
{
    // Check if we have an audio engine to work with
    if (!m_audioEngine)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Preview Not Available",
            "No audio engine connected. Preview requires an active audio document.",
            "OK"
        );
        return;
    }

    // FIX: Match the toggle pattern from GainDialog/ParametricEQDialog
    // Check if we're currently previewing AND playing - if so, stop
    if (m_previewActive && m_audioEngine->isPlaying())
    {
        // FIX: Must stop audio playback when clicking "Stop Preview" (was missing before)
        m_audioEngine->stop();
        m_audioEngine->setGraphicalEQEditor(nullptr);
        m_audioEngine->setDynamicEQPreview(DynamicParametricEQ::Parameters(), false);
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
        m_audioEngine->setPreviewBypassed(false);  // Reset bypass state
        m_previewActive = false;
        m_previewButton.setButtonText("Preview");
        m_previewButton.setColour(juce::TextButton::buttonColourId,
            getLookAndFeel().findColour(juce::TextButton::buttonColourId));
        // Disable and reset bypass button
        m_bypassButton.setEnabled(false);
        m_bypassButton.setButtonText("Bypass");
        m_bypassButton.setColour(juce::TextButton::buttonColourId,
            getLookAndFeel().findColour(juce::TextButton::buttonColourId));
        return;
    }

    // Start preview
    m_previewActive = true;

    // Register this editor to receive audio data for spectrum visualization
    m_audioEngine->setGraphicalEQEditor(this);

    // Clear any existing loop points before starting (matches GainDialog pattern)
    m_audioEngine->clearLoopPoints();

    // Enable real-time EQ preview
    m_audioEngine->setPreviewMode(PreviewMode::REALTIME_DSP);
    m_audioEngine->setDynamicEQPreview(m_params, true);

    // FIX: Position playback at selection start (like GainDialog pattern)
    // If there's a valid selection, start preview from selection start
    if (m_selectionEnd > m_selectionStart)
    {
        // Set the preview selection offset for DSP preview to work correctly
        m_audioEngine->setPreviewSelectionOffset(m_selectionStart);

        // Calculate selection bounds in seconds
        const double sampleRate = m_audioEngine->getSampleRate();
        double selectionStartSec = static_cast<double>(m_selectionStart) / sampleRate;
        double selectionEndSec = static_cast<double>(m_selectionEnd) / sampleRate;

        // Position playhead at selection start
        m_audioEngine->setPosition(selectionStartSec);

        // Set loop points if loop toggle is enabled
        bool shouldLoop = m_loopToggle.getToggleState();
        if (shouldLoop)
        {
            m_audioEngine->setLoopPoints(selectionStartSec, selectionEndSec);
        }
    }

    // Start playback if not already playing
    if (!m_audioEngine->isPlaying())
    {
        m_audioEngine->play();
    }

    // Update button text and color to indicate active state (matches GainDialog pattern)
    m_previewButton.setButtonText("Stop Preview");
    m_previewButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);

    // Enable bypass button during preview
    m_bypassButton.setEnabled(true);
}

//==============================================================================
// Preset Helper Methods

void GraphicalEQEditor::refreshPresetList()
{
    m_presetComboBox.clear(juce::dontSendNotification);

    int itemId = 1;

    // Add factory presets section
    auto factoryPresets = EQPresetManager::getFactoryPresetNames();
    if (!factoryPresets.isEmpty())
    {
        m_presetComboBox.addSectionHeading("Factory Presets");

        for (const auto& name : factoryPresets)
        {
            m_presetComboBox.addItem(name, itemId++);
        }

        m_presetComboBox.addSeparator();
    }

    // Add user presets section
    auto userPresets = EQPresetManager::getAvailablePresets();
    if (!userPresets.isEmpty())
    {
        m_presetComboBox.addSectionHeading("User Presets");

        for (const auto& name : userPresets)
        {
            m_presetComboBox.addItem(name, itemId++);
        }
    }

    // Update delete button state - only enabled for user presets
    m_deletePresetButton.setEnabled(false);
}

void GraphicalEQEditor::presetSelected()
{
    juce::String selectedName = m_presetComboBox.getText();

    if (selectedName.isEmpty())
        return;

    // Check if it's a factory preset
    if (EQPresetManager::isFactoryPreset(selectedName))
    {
        m_params = EQPresetManager::getFactoryPreset(selectedName);
        m_deletePresetButton.setEnabled(false);
    }
    else
    {
        // Try to load user preset
        if (EQPresetManager::loadPreset(m_params, selectedName))
        {
            m_deletePresetButton.setEnabled(true);
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Load Failed",
                "Could not load preset: " + selectedName,
                "OK"
            );
            return;
        }
    }

    // Update control points
    updateControlPointPositions();

    // Update EQ processor for visualization
    if (m_eqProcessor)
    {
        m_eqProcessor->setParameters(m_params);
        m_eqProcessor->updateCoefficientsForVisualization();
    }

    // Update preview if active
    if (m_previewActive && m_audioEngine)
    {
        m_audioEngine->setDynamicEQPreview(m_params, true);
    }

    repaint();
}

void GraphicalEQEditor::savePreset()
{
    // Show dialog to get preset name
    auto* alertWindow = new juce::AlertWindow(
        "Save EQ Preset",
        "Enter a name for this preset:",
        juce::MessageBoxIconType::QuestionIcon
    );

    alertWindow->addTextEditor("presetName", "", "Preset Name:");
    alertWindow->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, alertWindow](int result)
        {
            if (result == 1)
            {
                juce::String presetName = alertWindow->getTextEditorContents("presetName").trim();

                if (presetName.isEmpty())
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Invalid Name",
                        "Please enter a valid preset name.",
                        "OK"
                    );
                }
                else if (EQPresetManager::isFactoryPreset(presetName))
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Reserved Name",
                        "Cannot use a factory preset name. Please choose a different name.",
                        "OK"
                    );
                }
                else
                {
                    // Check if preset already exists
                    if (EQPresetManager::presetExists(presetName))
                    {
                        juce::AlertWindow::showOkCancelBox(
                            juce::MessageBoxIconType::QuestionIcon,
                            "Overwrite Preset?",
                            "A preset named \"" + presetName + "\" already exists. Overwrite it?",
                            "Overwrite",
                            "Cancel",
                            nullptr,
                            juce::ModalCallbackFunction::create(
                                [this, presetName](int overwriteResult)
                                {
                                    if (overwriteResult == 1)
                                    {
                                        doSavePreset(presetName);
                                    }
                                }
                            )
                        );
                    }
                    else
                    {
                        doSavePreset(presetName);
                    }
                }
            }

            delete alertWindow;
        }
    ), true);
}

void GraphicalEQEditor::doSavePreset(const juce::String& presetName)
{
    if (EQPresetManager::savePreset(m_params, presetName))
    {
        refreshPresetList();

        // Select the newly saved preset
        for (int i = 0; i < m_presetComboBox.getNumItems(); ++i)
        {
            if (m_presetComboBox.getItemText(i) == presetName)
            {
                m_presetComboBox.setSelectedItemIndex(i, juce::dontSendNotification);
                m_deletePresetButton.setEnabled(true);
                break;
            }
        }

        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Preset Saved",
            "Preset \"" + presetName + "\" saved successfully.",
            "OK"
        );
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Save Failed",
            "Could not save preset. Please check disk permissions.",
            "OK"
        );
    }
}

void GraphicalEQEditor::deletePreset()
{
    juce::String selectedName = m_presetComboBox.getText();

    if (selectedName.isEmpty())
        return;

    // Cannot delete factory presets
    if (EQPresetManager::isFactoryPreset(selectedName))
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Cannot Delete",
            "Factory presets cannot be deleted.",
            "OK"
        );
        return;
    }

    // Confirm deletion
    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::QuestionIcon,
        "Delete Preset?",
        "Are you sure you want to delete the preset \"" + selectedName + "\"?",
        "Delete",
        "Cancel",
        nullptr,
        juce::ModalCallbackFunction::create(
            [this, selectedName](int result)
            {
                if (result == 1)
                {
                    if (EQPresetManager::deletePreset(selectedName))
                    {
                        refreshPresetList();
                        m_deletePresetButton.setEnabled(false);
                    }
                    else
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Delete Failed",
                            "Could not delete preset.",
                            "OK"
                        );
                    }
                }
            }
        )
    );
}

void GraphicalEQEditor::resetToFlat()
{
    // Create a flat EQ with no bands
    m_params = EQPresetManager::getFactoryPreset("Flat");

    // Update control points
    updateControlPointPositions();

    // Update EQ processor for visualization
    if (m_eqProcessor)
    {
        m_eqProcessor->setParameters(m_params);
        m_eqProcessor->updateCoefficientsForVisualization();
    }

    // Update preview if active
    if (m_previewActive && m_audioEngine)
    {
        m_audioEngine->setDynamicEQPreview(m_params, true);
    }

    // Clear preset selection
    m_presetComboBox.setSelectedId(0, juce::dontSendNotification);
    m_deletePresetButton.setEnabled(false);

    repaint();
}

void GraphicalEQEditor::onBypassClicked()
{
    if (!m_audioEngine)
        return;

    bool bypassed = m_audioEngine->isPreviewBypassed();
    bool newBypassState = !bypassed;
    m_audioEngine->setPreviewBypassed(newBypassState);

    // Save bypass state for persistence across dialog reopens
    s_lastBypassState = newBypassState;

    // Update button appearance
    if (!bypassed)
    {
        m_bypassButton.setButtonText("Bypassed");
        m_bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff8c00));
    }
    else
    {
        m_bypassButton.setButtonText("Bypass");
        m_bypassButton.setColour(juce::TextButton::buttonColourId,
                                 getLookAndFeel().findColour(juce::TextButton::buttonColourId));
    }
}
