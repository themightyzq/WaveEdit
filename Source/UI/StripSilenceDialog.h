/*
  ==============================================================================

    StripSilenceDialog.h
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
#include "../Utils/RegionManager.h"

/**
 * Auto Region Dialog - Auto-create regions from non-silent audio sections.
 *
 * This dialog provides a UI for the "Auto Region" algorithm, which analyzes
 * an audio buffer and automatically creates regions around sections that exceed
 * a threshold level. Similar to Pro Tools "Auto Region" feature.
 *
 * Algorithm Parameters:
 * - Threshold (dB): Audio below this level is considered silence
 * - Min Region Length (ms): Regions shorter than this are discarded
 * - Min Silence Length (ms): Gaps shorter than this don't split regions
 * - Pre-Roll (ms): Margin added before each region
 * - Post-Roll (ms): Margin added after each region
 *
 * Use Cases:
 * - Podcast editing: Auto-create regions for each speaking section
 * - Dialog editing: Separate dialog takes automatically
 * - Sound effect organization: Group related sounds
 */
class StripSilenceDialog : public juce::Component
{
public:
    /**
     * Creates a Auto Region dialog.
     *
     * @param regionManager RegionManager to populate with auto-created regions
     * @param audioBuffer Audio buffer to analyze
     * @param sampleRate Sample rate for time calculations
     */
    StripSilenceDialog(RegionManager& regionManager,
                       const juce::AudioBuffer<float>& audioBuffer,
                       double sampleRate);

    ~StripSilenceDialog() override;

    //==============================================================================
    // Component overrides

    void paint(juce::Graphics& g) override;
    void resized() override;

    //==============================================================================
    // Callbacks

    /**
     * Callback invoked when user clicks "Apply" button.
     * Returns the number of regions created.
     */
    std::function<void(int numRegionsCreated)> onApply;

    /**
     * Callback invoked when user clicks "Cancel" button.
     */
    std::function<void()> onCancel;

private:
    //==============================================================================
    // Parameter controls

    juce::Label m_thresholdLabel;
    juce::Slider m_thresholdSlider;
    juce::Label m_thresholdValueLabel;

    juce::Label m_minRegionLabel;
    juce::Slider m_minRegionSlider;
    juce::Label m_minRegionValueLabel;

    juce::Label m_minSilenceLabel;
    juce::Slider m_minSilenceSlider;
    juce::Label m_minSilenceValueLabel;

    juce::Label m_preRollLabel;
    juce::Slider m_preRollSlider;
    juce::Label m_preRollValueLabel;

    juce::Label m_postRollLabel;
    juce::Slider m_postRollSlider;
    juce::Label m_postRollValueLabel;

    //==============================================================================
    // Buttons

    juce::TextButton m_previewButton;
    juce::TextButton m_applyButton;
    juce::TextButton m_cancelButton;

    //==============================================================================
    // Preview area

    juce::Label m_previewLabel;

    // Waveform preview component
    class WaveformPreview : public juce::Component
    {
    public:
        WaveformPreview(const juce::AudioBuffer<float>& audioBuffer)
            : m_audioBuffer(audioBuffer)
        {
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds();

            // Draw background
            g.fillAll(juce::Colour(0xff1a1a1a));

            // Draw border
            g.setColour(juce::Colours::grey);
            g.drawRect(bounds, 1);

            if (m_audioBuffer.getNumSamples() == 0)
            {
                g.setColour(juce::Colours::white);
                g.drawText("No audio loaded", bounds, juce::Justification::centred);
                return;
            }

            // Draw waveform
            drawWaveform(g, bounds.reduced(2));

            // Draw preview regions
            drawPreviewRegions(g, bounds.reduced(2));
        }

        void setPreviewRegions(const juce::Array<Region>& regions)
        {
            m_previewRegions = regions;
            repaint();
        }

    private:
        void drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds)
        {
            const int numChannels = m_audioBuffer.getNumChannels();
            const int numSamples = m_audioBuffer.getNumSamples();
            const int width = bounds.getWidth();
            const int height = bounds.getHeight();

            if (numSamples == 0 || width == 0)
                return;

            // Calculate samples per pixel
            const int samplesPerPixel = juce::jmax(1, numSamples / width);

            // Draw each channel
            const int channelHeight = height / juce::jmax(1, numChannels);

            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float* channelData = m_audioBuffer.getReadPointer(channel);
                const int channelY = bounds.getY() + channel * channelHeight;
                const int channelCenterY = channelY + channelHeight / 2;

                g.setColour(juce::Colour(0xff00ff00).withAlpha(0.6f));

                juce::Path waveformPath;
                bool firstPoint = true;

                for (int x = 0; x < width; ++x)
                {
                    int startSample = x * samplesPerPixel;
                    int endSample = juce::jmin(startSample + samplesPerPixel, numSamples);

                    float minVal = 0.0f;
                    float maxVal = 0.0f;

                    // Find min/max in this pixel range
                    for (int i = startSample; i < endSample; ++i)
                    {
                        float sample = channelData[i];
                        minVal = juce::jmin(minVal, sample);
                        maxVal = juce::jmax(maxVal, sample);
                    }

                    const int pixelX = bounds.getX() + x;
                    const int minY = channelCenterY + (int)(minVal * channelHeight / 2);
                    const int maxY = channelCenterY + (int)(maxVal * channelHeight / 2);

                    if (firstPoint)
                    {
                        waveformPath.startNewSubPath((float)pixelX, (float)minY);
                        firstPoint = false;
                    }

                    waveformPath.lineTo((float)pixelX, (float)maxY);
                    waveformPath.lineTo((float)pixelX, (float)minY);
                }

                g.strokePath(waveformPath, juce::PathStrokeType(1.0f));
            }
        }

        void drawPreviewRegions(juce::Graphics& g, juce::Rectangle<int> bounds)
        {
            if (m_previewRegions.isEmpty())
                return;

            const int numSamples = m_audioBuffer.getNumSamples();
            if (numSamples == 0)
                return;

            const int width = bounds.getWidth();

            // Color palette for regions
            const juce::Array<juce::Colour> colors = {
                juce::Colour(0xffff6b6b).withAlpha(0.3f),
                juce::Colour(0xff4ecdc4).withAlpha(0.3f),
                juce::Colour(0xfff7b731).withAlpha(0.3f),
                juce::Colour(0xff5f27cd).withAlpha(0.3f),
                juce::Colour(0xff00d2d3).withAlpha(0.3f)
            };

            for (int i = 0; i < m_previewRegions.size(); ++i)
            {
                const auto& region = m_previewRegions.getReference(i);

                // Calculate pixel positions
                int startX = bounds.getX() + (int)((region.getStartSample() / (double)numSamples) * width);
                int endX = bounds.getX() + (int)((region.getEndSample() / (double)numSamples) * width);
                int regionWidth = juce::jmax(1, endX - startX);

                // Draw region overlay
                g.setColour(colors[i % colors.size()]);
                g.fillRect(startX, bounds.getY(), regionWidth, bounds.getHeight());

                // Draw region border
                g.setColour(colors[i % colors.size()].withAlpha(0.8f));
                g.drawRect(startX, bounds.getY(), regionWidth, bounds.getHeight(), 1);
            }
        }

        const juce::AudioBuffer<float>& m_audioBuffer;
        juce::Array<Region> m_previewRegions;
    };

    std::unique_ptr<WaveformPreview> m_waveformPreview;
    juce::Label m_regionCountLabel;  // Shows count of regions

    //==============================================================================
    // State

    RegionManager& m_regionManager;
    const juce::AudioBuffer<float>& m_audioBuffer;
    double m_sampleRate;

    bool m_isPreviewMode;
    juce::Array<Region> m_previewRegions;

    //==============================================================================
    // Helper methods

    /**
     * Updates the value label for a slider (displays current value).
     */
    void updateValueLabel(juce::Slider& slider, juce::Label& label, const juce::String& suffix);

    /**
     * Applies the Auto Region algorithm with current parameter values.
     *
     * @param previewOnly If true, stores results in m_previewRegions without modifying RegionManager
     * @return Number of regions created
     */
    int applyStripSilence(bool previewOnly);

    /**
     * Validates parameters and shows error dialog if invalid.
     *
     * @return true if parameters are valid, false otherwise
     */
    bool validateParameters();

    /**
     * Updates the preview text editor to show the list of preview regions.
     */
    void updatePreviewDisplay();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StripSilenceDialog)
};
