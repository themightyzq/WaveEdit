/*
  ==============================================================================

    SpectrumAnalyzer.h
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
#include <atomic>
#include <array>

// Forward declarations
class AudioEngine;

/**
 * Real-time FFT-based spectrum analyzer component.
 *
 * Features:
 * - Real-time frequency domain visualization during playback
 * - Configurable FFT size (512, 1024, 2048, 4096, 8192)
 * - Multiple windowing functions (Hann, Hamming, Blackman, Rectangular)
 * - Logarithmic frequency scale for natural perception
 * - Peak hold visualization
 * - Thread-safe audio data transfer (audio thread → UI thread)
 *
 * Design Philosophy:
 * - Inspired by professional tools (Sound Forge, Adobe Audition, iZotope RX)
 * - Clean, minimal design with clear frequency axis
 * - Real-time performance with configurable quality/CPU trade-off
 * - Color gradient visualization (low = blue, mid = green, high = yellow/red)
 */
class SpectrumAnalyzer : public juce::Component,
                         public juce::Timer
{
public:
    SpectrumAnalyzer();
    ~SpectrumAnalyzer() override;

    //==============================================================================
    // FFT Configuration

    enum class FFTSize
    {
        SIZE_512 = 512,
        SIZE_1024 = 1024,
        SIZE_2048 = 2048,
        SIZE_4096 = 4096,
        SIZE_8192 = 8192
    };

    enum class WindowFunction
    {
        HANN,
        HAMMING,
        BLACKMAN,
        RECTANGULAR
    };

    /**
     * Sets the FFT size for frequency resolution.
     * Larger sizes provide better frequency resolution but higher latency.
     *
     * @param size FFT size (512, 1024, 2048, 4096, or 8192)
     */
    void setFFTSize(FFTSize size);

    /**
     * Sets the windowing function applied to audio data before FFT.
     *
     * @param window Window function type
     */
    void setWindowFunction(WindowFunction window);

    /**
     * Gets the current FFT size.
     *
     * @return Current FFT size
     */
    FFTSize getFFTSize() const { return m_currentFFTSize; }

    /**
     * Gets the current windowing function.
     *
     * @return Current window function
     */
    WindowFunction getWindowFunction() const { return m_currentWindow; }

    //==============================================================================
    // Audio Data Input (called from audio thread)

    /**
     * Pushes audio samples for FFT analysis (thread-safe).
     * Called from audio thread during playback.
     *
     * @param buffer Audio buffer containing samples
     * @param numSamples Number of samples in buffer
     */
    void pushAudioData(const float* buffer, int numSamples);

    /**
     * Resets the spectrum analyzer to zero state.
     */
    void reset();

    /**
     * Sets the audio engine to monitor for spectrum updates.
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
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    //==============================================================================
    // FFT State

    static constexpr int MAX_FFT_SIZE = 8192;
    static constexpr int MAX_FFT_SIZE_ORDER = 13; // 2^13 = 8192

    FFTSize m_currentFFTSize;
    WindowFunction m_currentWindow;
    int m_fftOrder;

    std::unique_ptr<juce::dsp::FFT> m_fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> m_window;

    // FFT input/output buffers (protected by spin lock for thread safety)
    std::array<float, MAX_FFT_SIZE * 2> m_fftData; // Input data (time domain) + output (frequency domain)
    std::array<float, MAX_FFT_SIZE> m_scopeData;   // Visualisation data for UI thread
    std::array<float, MAX_FFT_SIZE> m_peakHold;    // Peak hold values

    int m_fifoIndex;                               // Current write position in FIFO
    std::atomic<bool> m_nextFFTBlockReady;        // Flag for new FFT data ready
    juce::SpinLock m_fftLock;                     // Protects m_fftData during audio thread writes

    // Audio source (not owned)
    AudioEngine* m_audioEngine;

    //==============================================================================
    // Visualization State

    float m_minFrequency;   // Minimum frequency to display (default: 20 Hz)
    float m_maxFrequency;   // Maximum frequency to display (default: 20 kHz)
    float m_minDB;          // Minimum dB level (default: -80 dB)
    float m_maxDB;          // Maximum dB level (default: 0 dB)

    int m_peakHoldTime[MAX_FFT_SIZE];  // Peak hold timer per frequency bin
    static constexpr int PEAK_HOLD_TIME_MS = 1500;
    static constexpr int UPDATE_RATE_HZ = 30;

    // Sample rate (for frequency calculations)
    std::atomic<double> m_sampleRate;

    //==============================================================================
    // Helper Methods

    /**
     * Updates the FFT with new configuration (size, window function).
     */
    void updateFFTConfiguration();

    /**
     * Applies the selected window function to the time-domain data.
     */
    void applyWindow();

    /**
     * Processes the FFT and updates visualization data.
     */
    void processFFT();

    /**
     * Converts frequency to X position on screen (logarithmic scale).
     *
     * @param frequency Frequency in Hz
     * @param bounds Drawing bounds
     * @return X coordinate
     */
    float frequencyToX(float frequency, juce::Rectangle<float> bounds) const;

    /**
     * Converts X position to frequency (logarithmic scale).
     *
     * @param x X coordinate
     * @param bounds Drawing bounds
     * @return Frequency in Hz
     */
    float xToFrequency(float x, juce::Rectangle<float> bounds) const;

    /**
     * Converts dB magnitude to Y position on screen.
     *
     * @param dB Magnitude in decibels
     * @param bounds Drawing bounds
     * @return Y coordinate
     */
    float dBToY(float dB, juce::Rectangle<float> bounds) const;

    /**
     * Gets color for magnitude visualization (gradient from blue to red).
     *
     * @param dB Magnitude in decibels
     * @return Color for this magnitude
     */
    juce::Colour getColorForMagnitude(float dB) const;

    /**
     * Draws the frequency axis with logarithmic scale markings.
     *
     * @param g Graphics context
     * @param bounds Drawing bounds
     */
    void drawFrequencyAxis(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draws the magnitude axis with dB scale markings.
     *
     * @param g Graphics context
     * @param bounds Drawing bounds
     */
    void drawMagnitudeAxis(juce::Graphics& g, juce::Rectangle<float> bounds);

    /**
     * Draws the spectrum visualization.
     *
     * @param g Graphics context
     * @param bounds Drawing bounds
     */
    void drawSpectrum(juce::Graphics& g, juce::Rectangle<float> bounds);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzer)
};
