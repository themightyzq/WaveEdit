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
#include "../DSP/DynamicParametricEQ.h"
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
 * - Up to 20 dynamic EQ bands with multiple filter types
 * - Filter types: Bell, Low Shelf, High Shelf, Low Cut, High Cut, Notch, Bandpass
 * - Draggable control points (click and drag to adjust frequency/gain)
 * - Double-click to add new band at cursor position
 * - Right-click to delete a band
 * - Mouse wheel to adjust Q (bandwidth/resonance)
 * - Real-time spectrum analyzer visualization
 * - Accurate frequency response curve using IIR filter math
 * - Grid lines and axis labels for precision
 * - Color-coded bands by filter type
 *
 * Architecture:
 * - Uses DynamicParametricEQ for accurate frequency response calculation
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
     * @param initialParams Initial EQ parameters (defaults to default preset)
     */
    explicit GraphicalEQEditor(const DynamicParametricEQ::Parameters& initialParams = DynamicParametricEQ::createDefaultPreset());
    ~GraphicalEQEditor() override;

    //==============================================================================
    // Public API

    /**
     * Shows the graphical EQ editor as a modal dialog.
     * Returns the edited EQ parameters if Apply was clicked, otherwise std::nullopt.
     *
     * @param audioEngine Pointer to the audio engine for preview functionality (can be nullptr)
     * @param initialParams Initial EQ parameters
     * @return Edited parameters if applied, std::nullopt if cancelled
     */
    static std::optional<DynamicParametricEQ::Parameters> showDialog(
        AudioEngine* audioEngine,
        const DynamicParametricEQ::Parameters& initialParams = DynamicParametricEQ::createDefaultPreset()
    );

    /**
     * Gets the current EQ parameters.
     *
     * @return Current EQ parameter values
     */
    const DynamicParametricEQ::Parameters& getParameters() const { return m_params; }

    /**
     * Sets the audio engine to monitor for spectrum visualization.
     * Pass nullptr to disconnect from audio monitoring.
     *
     * @param audioEngine The audio engine to monitor, or nullptr
     */
    void setAudioEngine(AudioEngine* audioEngine);

    /**
     * Pushes audio samples for FFT analysis (thread-safe).
     * Called by AudioEngine to feed spectrum analyzer data.
     */
    void pushAudioData(const float* buffer, int numSamples);

    //==============================================================================
    // Component Overrides

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    //==============================================================================
    // Control Point Representation

    struct ControlPoint
    {
        int bandIndex;        // Index into m_params.bands
        float x;              // Screen X coordinate
        float y;              // Screen Y coordinate
        bool isDragging;

        ControlPoint(int idx = -1) : bandIndex(idx), x(0.0f), y(0.0f), isDragging(false) {}
    };

    //==============================================================================
    // EQ Parameters

    DynamicParametricEQ::Parameters m_params;
    std::optional<DynamicParametricEQ::Parameters> m_result;  // Result from modal dialog

    // Dynamic control points (one per band)
    std::vector<ControlPoint> m_controlPoints;

    // Track which band is being dragged
    int m_draggingBandIndex = -1;

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

    // Preview state
    bool m_previewActive = false;

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
     * Processes the FFT and updates visualization data.
     */
    void processFFT();

    //==============================================================================
    // Helper Methods - Mouse Interaction

    /**
     * Finds the control point index nearest to the given screen position.
     * Returns -1 if no point is within interaction threshold.
     */
    int findNearestControlPointIndex(float x, float y);

    /**
     * Updates all control point screen positions based on current EQ parameters.
     */
    void updateControlPointPositions();

    /**
     * Updates EQ parameters from control point screen position for specific band.
     */
    void updateParametersFromControlPoint(int bandIndex);

    /**
     * Add a new EQ band at the given screen position.
     */
    void addBandAtPosition(float x, float y);

    /**
     * Remove the band at the given index.
     */
    void removeBand(int bandIndex);

    /**
     * Get color for a filter type.
     */
    juce::Colour getFilterTypeColor(DynamicParametricEQ::FilterType type) const;

    /**
     * Show context menu for band editing.
     */
    void showBandContextMenu(int bandIndex);

    /**
     * Process preview audio with current EQ parameters.
     */
    void processPreviewAudio();

    /**
     * Start/stop preview playback.
     */
    void togglePreview();

    // DynamicParametricEQ instance for accurate frequency response calculation
    std::unique_ptr<DynamicParametricEQ> m_eqProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraphicalEQEditor)
};
