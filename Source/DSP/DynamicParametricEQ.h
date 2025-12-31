/*
  ==============================================================================

    DynamicParametricEQ.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

/**
 * Dynamic Parametric EQ with up to 20 bands and multiple filter types.
 *
 * Thread Safety:
 * - Parameter updates are thread-safe via atomic flag exchange
 * - applyEQ() is real-time safe (no allocations)
 * - setParameters() may allocate (call from message thread only)
 *
 * Usage:
 * @code
 * DynamicParametricEQ eq;
 * eq.prepare(sampleRate, maxBlockSize);
 *
 * // Add bands dynamically
 * DynamicParametricEQ::BandParameters band;
 * band.frequency = 1000.0f;
 * band.gain = 3.0f;
 * band.q = 1.4f;
 * band.filterType = DynamicParametricEQ::FilterType::Bell;
 * band.enabled = true;
 *
 * DynamicParametricEQ::Parameters params;
 * params.bands.push_back(band);
 * eq.setParameters(params);
 *
 * // Process audio
 * eq.applyEQ(buffer);
 *
 * // Get frequency response for visualization
 * std::vector<float> magnitudes(512);
 * eq.getFrequencyResponse(magnitudes.data(), 512, 20.0, 20000.0);
 * @endcode
 */
class DynamicParametricEQ
{
public:
    //==============================================================================
    /** Maximum number of EQ bands supported */
    static constexpr int MAX_BANDS = 20;

    /** Minimum frequency (Hz) */
    static constexpr float MIN_FREQUENCY = 20.0f;

    /** Maximum frequency (Hz) */
    static constexpr float MAX_FREQUENCY = 20000.0f;

    /** Minimum gain (dB) */
    static constexpr float MIN_GAIN = -24.0f;

    /** Maximum gain (dB) */
    static constexpr float MAX_GAIN = 24.0f;

    /** Minimum Q factor */
    static constexpr float MIN_Q = 0.1f;

    /** Maximum Q factor */
    static constexpr float MAX_Q = 18.0f;

    /** Default Q factor (Butterworth) */
    static constexpr float DEFAULT_Q = 0.707f;

    //==============================================================================
    /**
     * Filter types available for each band.
     */
    enum class FilterType
    {
        Bell,           ///< Parametric peak/notch filter (gain affects boost/cut)
        LowShelf,       ///< Low shelf filter (boosts/cuts below frequency)
        HighShelf,      ///< High shelf filter (boosts/cuts above frequency)
        LowCut,         ///< High-pass filter (cuts below frequency, Q affects slope)
        HighCut,        ///< Low-pass filter (cuts above frequency, Q affects slope)
        Notch,          ///< Notch filter (narrow cut at frequency)
        Bandpass        ///< Bandpass filter (passes frequencies around center)
    };

    /**
     * Get human-readable name for a filter type.
     */
    static juce::String getFilterTypeName(FilterType type)
    {
        switch (type)
        {
            case FilterType::Bell:      return "Bell";
            case FilterType::LowShelf:  return "Low Shelf";
            case FilterType::HighShelf: return "High Shelf";
            case FilterType::LowCut:    return "Low Cut";
            case FilterType::HighCut:   return "High Cut";
            case FilterType::Notch:     return "Notch";
            case FilterType::Bandpass:  return "Bandpass";
            default:                    return "Unknown";
        }
    }

    /**
     * Get short name for a filter type (for UI).
     */
    static juce::String getFilterTypeShortName(FilterType type)
    {
        switch (type)
        {
            case FilterType::Bell:      return "BEL";
            case FilterType::LowShelf:  return "LSH";
            case FilterType::HighShelf: return "HSH";
            case FilterType::LowCut:    return "LCT";
            case FilterType::HighCut:   return "HCT";
            case FilterType::Notch:     return "NOT";
            case FilterType::Bandpass:  return "BPF";
            default:                    return "???";
        }
    }

    /**
     * Check if a filter type uses gain parameter.
     */
    static bool filterTypeUsesGain(FilterType type)
    {
        switch (type)
        {
            case FilterType::Bell:
            case FilterType::LowShelf:
            case FilterType::HighShelf:
                return true;
            case FilterType::LowCut:
            case FilterType::HighCut:
            case FilterType::Notch:
            case FilterType::Bandpass:
                return false;
            default:
                return false;
        }
    }

    //==============================================================================
    /**
     * Parameters for a single EQ band.
     */
    struct BandParameters
    {
        float frequency = 1000.0f;          ///< Center/corner frequency (Hz)
        float gain = 0.0f;                  ///< Gain in dB (-24 to +24)
        float q = DEFAULT_Q;                ///< Q factor (0.1 to 18.0)
        FilterType filterType = FilterType::Bell;  ///< Filter type
        bool enabled = true;                ///< Whether this band is active

        bool operator==(const BandParameters& other) const
        {
            return frequency == other.frequency
                && gain == other.gain
                && q == other.q
                && filterType == other.filterType
                && enabled == other.enabled;
        }

        bool operator!=(const BandParameters& other) const
        {
            return !(*this == other);
        }
    };

    /**
     * Complete EQ parameters (all bands).
     */
    struct Parameters
    {
        std::vector<BandParameters> bands;  ///< Active bands (up to MAX_BANDS)
        float outputGain = 0.0f;            ///< Output gain in dB

        /** Get number of active (enabled) bands */
        int getNumActiveBands() const
        {
            int count = 0;
            for (const auto& band : bands)
                if (band.enabled)
                    ++count;
            return count;
        }

        bool operator==(const Parameters& other) const
        {
            if (bands.size() != other.bands.size())
                return false;
            if (outputGain != other.outputGain)
                return false;
            for (size_t i = 0; i < bands.size(); ++i)
                if (bands[i] != other.bands[i])
                    return false;
            return true;
        }

        bool operator!=(const Parameters& other) const
        {
            return !(*this == other);
        }
    };

    //==============================================================================
    /** Constructor */
    DynamicParametricEQ();

    /** Destructor */
    ~DynamicParametricEQ();

    //==============================================================================
    /**
     * Prepare the EQ for processing.
     * Must be called before applyEQ().
     *
     * @param sampleRate The sample rate in Hz
     * @param maxBlockSize Maximum expected block size
     */
    void prepare(double sampleRate, int maxBlockSize);

    /**
     * Reset internal filter states (call when seeking or on discontinuity).
     */
    void reset();

    //==============================================================================
    /**
     * Set EQ parameters.
     * Thread-safe but may allocate - call from message thread only.
     *
     * @param newParams The new parameters to apply
     */
    void setParameters(const Parameters& newParams);

    /**
     * Get current parameters (thread-safe copy).
     */
    Parameters getParameters() const;

    /**
     * Check if parameters have changed since last applyEQ().
     */
    bool parametersChanged() const { return m_parametersChanged.load(); }

    //==============================================================================
    /**
     * Apply EQ to an audio buffer.
     * Real-time safe - no allocations.
     *
     * @param buffer The audio buffer to process in-place
     */
    void applyEQ(juce::AudioBuffer<float>& buffer);

    /**
     * Apply EQ to a JUCE AudioBlock.
     * Real-time safe - no allocations.
     *
     * @param block The audio block to process in-place
     */
    void applyEQ(juce::dsp::AudioBlock<float>& block);

    //==============================================================================
    /**
     * Calculate frequency response magnitude at multiple frequencies.
     * Used for visualizing the EQ curve.
     *
     * @param magnitudes Output array for magnitude values (in dB)
     * @param numPoints Number of frequency points
     * @param startFreq Start frequency in Hz (typically 20)
     * @param endFreq End frequency in Hz (typically 20000)
     * @param useLogScale If true, frequencies are logarithmically spaced
     */
    void getFrequencyResponse(float* magnitudes, int numPoints,
                              double startFreq, double endFreq,
                              bool useLogScale = true) const;

    /**
     * Calculate frequency response at a single frequency.
     *
     * @param frequency The frequency in Hz
     * @return Magnitude response in dB
     */
    float getFrequencyResponseAt(double frequency) const;

    //==============================================================================
    /**
     * Get the current sample rate.
     */
    double getSampleRate() const { return m_sampleRate; }

    /**
     * Check if the EQ is prepared for processing.
     */
    bool isPrepared() const { return m_sampleRate > 0; }

    /**
     * Get the total latency introduced by the EQ (always 0 for IIR).
     */
    int getLatencySamples() const { return 0; }

    /**
     * Force update of all filter coefficients for visualization purposes.
     * Call this after setParameters() when you need getFrequencyResponse()
     * to reflect the new parameters immediately, without calling applyEQ().
     */
    void updateCoefficientsForVisualization();

    //==============================================================================
    // Convenience methods for common operations

    /**
     * Add a new band with default parameters.
     * @param frequency Initial frequency
     * @param filterType Initial filter type
     * @return Index of new band, or -1 if MAX_BANDS reached
     */
    int addBand(float frequency = 1000.0f, FilterType filterType = FilterType::Bell);

    /**
     * Remove a band by index.
     * @param index Band index to remove
     * @return true if removed, false if index invalid
     */
    bool removeBand(int index);

    /**
     * Set parameters for a single band.
     * @param index Band index
     * @param params New parameters for the band
     * @return true if set, false if index invalid
     */
    bool setBandParameters(int index, const BandParameters& params);

    /**
     * Get parameters for a single band.
     * @param index Band index
     * @return Band parameters, or default if index invalid
     */
    BandParameters getBandParameters(int index) const;

    /**
     * Get number of bands.
     */
    int getNumBands() const;

    /**
     * Enable/disable a band.
     */
    bool setBandEnabled(int index, bool enabled);

    /**
     * Clear all bands.
     */
    void clearAllBands();

    //==============================================================================
    // Preset management

    /**
     * Create default preset with common starting points.
     * @return Parameters with 3 bands: low shelf, mid bell, high shelf
     */
    static Parameters createDefaultPreset();

    /**
     * Create flat preset (no bands, unity gain).
     */
    static Parameters createFlatPreset();

private:
    //==============================================================================
    // Internal filter representation
    using IIRFilter = juce::dsp::IIR::Filter<float>;
    using IIRCoefficients = juce::dsp::IIR::Coefficients<float>;

    /**
     * Internal band state with filter instances and cached coefficients.
     */
    struct BandState
    {
        BandParameters params;
        std::array<IIRFilter, 2> filters;  // One per channel (stereo max)
        IIRCoefficients::Ptr coefficients;
        bool needsUpdate = true;
    };

    //==============================================================================
    void updateCoefficients();
    void updateBandCoefficients(BandState& band);
    IIRCoefficients::Ptr createCoefficients(const BandParameters& params) const;
    std::complex<double> getFilterResponse(const BandState& band, double frequency) const;

    //==============================================================================
    double m_sampleRate = 0;
    int m_maxBlockSize = 0;

    // Thread-safe parameter storage
    Parameters m_parameters;
    mutable juce::CriticalSection m_parameterLock;
    std::atomic<bool> m_parametersChanged{false};

    // Filter states (audio thread only)
    std::vector<BandState> m_bandStates;
    float m_outputGainLinear = 1.0f;

    // Coefficient cache to avoid redundant calculations
    Parameters m_cachedParams;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicParametricEQ)
};
