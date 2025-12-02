/*
  ==============================================================================

    GraphicalEQEditor.h
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
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/ParametricEQ.h"
#include <optional>

// Forward declarations
class AudioEngine;

/**
 * Modern graphical parametric EQ editor with real-time spectrum visualization.
 *
 * Design Philosophy:
 * - Inspired by FabFilter Pro-Q, iZotope Ozone, Fabfilter
 * - Interactive draggable control points for frequency/gain/Q
 * - Real-time spectrum analyzer background
 * - EQ frequency response curve overlay
 * - Professional visual feedback with color-coded bands
 *
 * Features:
 * - 3-band parametric EQ (Low Shelf, Mid Peak, High Shelf)
 * - Draggable control points (click and drag to adjust frequency/gain)
 * - Mouse wheel to adjust Q (bandwidth/resonance)
 * - Real-time spectrum analyzer visualization
 * - Combined frequency response curve display
 * - Grid lines and axis labels for precision
 * - Color-coded bands (Low=blue, Mid=green, High=red)
 *
 * Architecture:
 * - Reuses SpectrumAnalyzer FFT visualization logic
 * - Calculates combined EQ frequency response in real-time
 * - Interactive mouse/keyboard controls for parameter adjustment
 * - Thread-safe audio data transfer for spectrum display
 */
class GraphicalEQEditor : public juce::Component,
                          public juce::Timer
{
public:
    /**
     * Creates a graphical EQ editor with optional initial parameters.
     *
     * @param initialParams Initial EQ parameters (defaults to neutral if not provided)
     */
    explicit GraphicalEQEditor(const ParametricEQ::Parameters& initialParams = ParametricEQ::Parameters::createNeutral());
    ~GraphicalEQEditor() override;

    //==============================================================================
    // Public API

    /**
     * Shows the graphical EQ editor as a modal dialog.
     * Returns the edited EQ parameters if Apply was clicked, otherwise std::nullopt.
     *
     * NOTE: Preview functionality requires AudioEngine refactoring to support
     * temporary buffer playback. Currently preview is disabled.
     *
     * @param initialParams Initial EQ parameters
     * @return Edited parameters if applied, std::nullopt if cancelled
     */
    static std::optional<ParametricEQ::Parameters> showDialog(
        const ParametricEQ::Parameters& initialParams = ParametricEQ::Parameters::createNeutral()
    );

    /**
     * Gets the current EQ parameters.
     *
     * @return Current EQ parameter values
     */
    const ParametricEQ::Parameters& getParameters() const { return m_params; }

    /**
     * Sets the audio engine to monitor for spectrum visualization.
     * Pass nullptr to disconnect from audio monitoring.
     *
     * @param audioEngine The audio engine to monitor, or nullptr
     */
    void setAudioEngine(AudioEngine* audioEngine);

    //==============================================================================
    // Component Overrides

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    //==============================================================================
    // Control Point Representation

    enum class BandType
    {
        LOW,
        MID,
        HIGH
    };

    struct ControlPoint
    {
        BandType band;
        float x;              // Screen X coordinate
        float y;              // Screen Y coordinate
        bool isDragging;

        ControlPoint(BandType b) : band(b), x(0.0f), y(0.0f), isDragging(false) {}
    };

    //==============================================================================
    // EQ Parameters

    ParametricEQ::Parameters m_params;
    std::optional<ParametricEQ::Parameters> m_result;  // Result from modal dialog

    // Control points for each band
    ControlPoint m_lowBandPoint;
    ControlPoint m_midBandPoint;
    ControlPoint m_highBandPoint;

    //==============================================================================
    // Spectrum Analyzer State (reused from SpectrumAnalyzer)

    static constexpr int FFT_SIZE = 2048;
    static constexpr int FFT_ORDER = 11;  // 2^11 = 2048

    std::unique_ptr<juce::dsp::FFT> m_fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> m_window;

    // FFT buffers
    std::array<float, FFT_SIZE * 2> m_fftData;   // Time + frequency domain
    std::array<float, FFT_SIZE> m_scopeData;     // Visualization data

    int m_fifoIndex;
    std::atomic<bool> m_nextFFTBlockReady;
    juce::SpinLock m_fftLock;

    // Audio source (not owned)
    AudioEngine* m_audioEngine;
    std::atomic<double> m_sampleRate;

    //==============================================================================
    // Visualization Bounds

    float m_minFrequency;   // 20 Hz
    float m_maxFrequency;   // 20 kHz
    float m_minDB;          // -60 dB
    float m_maxDB;          // +20 dB

    static constexpr int UPDATE_RATE_HZ = 30;

    //==============================================================================
    // Dialog Controls

    juce::TextButton m_previewButton;
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;

    //==============================================================================
    // Helper Methods - Coordinate Conversions

    /**
     * Converts frequency to X position (logarithmic scale).
     */
    float frequencyToX(float frequency, juce::Rectangle<float> bounds) const;

    /**
     * Converts X position to frequency (logarithmic scale).
     */
    float xToFrequency(float x, juce::Rectangle<float> bounds) const;

    /**
     * Converts dB gain to Y position.
     */
    float gainToY(float gainDB, juce::Rectangle<float> bounds) const;

    /**
     * Converts Y position to dB gain.
     */
    float yToGain(float y, juce::Rectangle<float> bounds) const;

    //==============================================================================
    // Helper Methods - Drawing

    /**
     * Draws the spectrum analyzer background.
     */
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draws the combined EQ frequency response curve.
     */
    void drawEQCurve(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draws the control points for each band.
     */
    void drawControlPoints(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draws the frequency/gain grid and axis labels.
     */
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> bounds);

    //==============================================================================
    // Helper Methods - Audio Processing

    /**
     * Pushes audio samples for FFT analysis (thread-safe).
     */
    void pushAudioData(const float* buffer, int numSamples);

    /**
     * Processes the FFT and updates visualization data.
     */
    void processFFT();

    //==============================================================================
    // Helper Methods - Mouse Interaction

    /**
     * Finds the control point nearest to the given screen position.
     * Returns nullptr if no point is within interaction threshold.
     */
    ControlPoint* findNearestControlPoint(float x, float y);

    /**
     * Updates control point positions based on current EQ parameters.
     */
    void updateControlPointPositions();

    /**
     * Updates EQ parameters based on control point screen positions.
     */
    void updateParametersFromControlPoints();

    /**
     * Process preview audio with current EQ parameters.
     */
    void processPreviewAudio();

    /**
     * Start/stop preview playback.
     */
    void togglePreview();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphicalEQEditor)
};
