/*
  ==============================================================================

    HeadTailDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "HeadTailDialog.h"
#include "../DSP/HeadTailEngine.h"

//==============================================================================
// WaveformPreview implementation
//==============================================================================

HeadTailDialog::WaveformPreview::WaveformPreview(const juce::AudioBuffer<float>& audioBuffer)
    : m_audioBuffer(audioBuffer)
{
}

void HeadTailDialog::WaveformPreview::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.fillAll(juce::Colour(0xff1a1a1a));

    g.setColour(juce::Colours::grey);
    g.drawRect(bounds, 1);

    if (m_audioBuffer.getNumSamples() == 0)
    {
        g.setColour(juce::Colours::grey);
        g.drawText("No audio loaded", bounds, juce::Justification::centred);
        return;
    }

    auto inner = bounds.reduced(2);
    drawWaveform(g, inner);
    drawTrimOverlay(g, inner);
    drawBoundaryLines(g, inner);
}

void HeadTailDialog::WaveformPreview::setTrimRegion(int64_t keepStart, int64_t keepEnd)
{
    m_keepStart  = keepStart;
    m_keepEnd    = keepEnd;
    m_hasMarkers = true;
    repaint();
}

void HeadTailDialog::WaveformPreview::setDetectionBoundaries(int64_t start, int64_t end)
{
    m_boundaryStart = start;
    m_boundaryEnd   = end;
    m_hasMarkers    = true;
    repaint();
}

void HeadTailDialog::WaveformPreview::clearMarkers()
{
    m_keepStart     = -1;
    m_keepEnd       = -1;
    m_boundaryStart = -1;
    m_boundaryEnd   = -1;
    m_hasMarkers    = false;
    repaint();
}

void HeadTailDialog::WaveformPreview::drawWaveform(juce::Graphics& g,
                                                    juce::Rectangle<int> bounds)
{
    const int numChannels = m_audioBuffer.getNumChannels();
    const int numSamples  = m_audioBuffer.getNumSamples();
    const int width       = bounds.getWidth();
    const int height      = bounds.getHeight();

    if (numSamples == 0 || width == 0)
        return;

    const int samplesPerPixel  = juce::jmax(1, numSamples / width);
    const int channelHeight    = height / juce::jmax(1, numChannels);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* data       = m_audioBuffer.getReadPointer(channel);
        const int    channelY   = bounds.getY() + channel * channelHeight;
        const int    centerY    = channelY + channelHeight / 2;

        g.setColour(juce::Colour(0xff00cc44).withAlpha(0.7f));

        juce::Path path;
        bool firstPoint = true;

        for (int x = 0; x < width; ++x)
        {
            const int startSample = x * samplesPerPixel;
            const int endSample   = juce::jmin(startSample + samplesPerPixel, numSamples);

            float minVal = 0.0f;
            float maxVal = 0.0f;

            for (int i = startSample; i < endSample; ++i)
            {
                minVal = juce::jmin(minVal, data[i]);
                maxVal = juce::jmax(maxVal, data[i]);
            }

            const int pixelX = bounds.getX() + x;
            const int minY   = centerY + (int)(minVal * channelHeight / 2);
            const int maxY   = centerY + (int)(maxVal * channelHeight / 2);  // Note: maxVal negative → higher on screen

            if (firstPoint)
            {
                path.startNewSubPath((float)pixelX, (float)minY);
                firstPoint = false;
            }

            path.lineTo((float)pixelX, (float)maxY);
            path.lineTo((float)pixelX, (float)minY);
        }

        g.strokePath(path, juce::PathStrokeType(1.0f));
    }
}

void HeadTailDialog::WaveformPreview::drawTrimOverlay(juce::Graphics& g,
                                                       juce::Rectangle<int> bounds)
{
    if (!m_hasMarkers || m_keepStart < 0 || m_keepEnd < 0)
        return;

    const int64_t totalSamples = m_audioBuffer.getNumSamples();
    if (totalSamples == 0)
        return;

    const int width = bounds.getWidth();
    const auto toX = [&](int64_t sample) -> int
    {
        return bounds.getX() + (int)((double)sample / (double)totalSamples * width);
    };

    const juce::Colour overlay = juce::Colour(0x660a0a0a);

    // Head trim region (left of keepStart)
    const int headEndX = toX(m_keepStart);
    if (headEndX > bounds.getX())
    {
        g.setColour(overlay);
        g.fillRect(bounds.getX(), bounds.getY(), headEndX - bounds.getX(), bounds.getHeight());
    }

    // Tail trim region (right of keepEnd)
    const int tailStartX = toX(m_keepEnd);
    if (tailStartX < bounds.getRight())
    {
        g.setColour(overlay);
        g.fillRect(tailStartX, bounds.getY(), bounds.getRight() - tailStartX, bounds.getHeight());
    }
}

void HeadTailDialog::WaveformPreview::drawBoundaryLines(juce::Graphics& g,
                                                         juce::Rectangle<int> bounds)
{
    if (!m_hasMarkers)
        return;

    const int64_t totalSamples = m_audioBuffer.getNumSamples();
    if (totalSamples == 0)
        return;

    const int width = bounds.getWidth();
    const auto toX = [&](int64_t sample) -> int
    {
        return bounds.getX() + (int)((double)sample / (double)totalSamples * width);
    };

    g.setColour(juce::Colour(0xff00e060));

    if (m_boundaryStart >= 0)
    {
        const int x = toX(m_boundaryStart);
        g.drawLine((float)x, (float)bounds.getY(),
                   (float)x, (float)bounds.getBottom(), 1.5f);
    }

    if (m_boundaryEnd >= 0)
    {
        const int x = toX(m_boundaryEnd);
        g.drawLine((float)x, (float)bounds.getY(),
                   (float)x, (float)bounds.getBottom(), 1.5f);
    }
}

//==============================================================================
// HeadTailDialog implementation
//==============================================================================

HeadTailDialog::HeadTailDialog(const juce::AudioBuffer<float>& audioBuffer,
                                double sampleRate)
    : m_audioBuffer(audioBuffer)
    , m_sampleRate(sampleRate)
{
    //--------------------------------------------------------------------------
    // Section 1: Intelligent Trim

    m_detectEnableToggle.setButtonText("Enable Detection");
    m_detectEnableToggle.setToggleState(false, juce::dontSendNotification);
    m_detectEnableToggle.onStateChange = [this]()
    {
        updateDetectionControlsEnabled();
        updateSummary();
    };
    addAndMakeVisible(m_detectEnableToggle);

    m_detectModeLabel.setText("Mode:", juce::dontSendNotification);
    m_detectModeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_detectModeLabel);

    m_detectModeCombo.addItem("Peak", 1);
    m_detectModeCombo.addItem("RMS",  2);
    m_detectModeCombo.setSelectedId(1, juce::dontSendNotification);
    m_detectModeCombo.onChange = [this]() { updateSummary(); };
    addAndMakeVisible(m_detectModeCombo);

    addSliderRow(m_thresholdLabel,   "Threshold (dB):",
                 m_thresholdSlider,  -80.0, 0.0, 0.1, -40.0,
                 m_thresholdValueLabel, " dB");

    addSliderRow(m_holdTimeLabel,    "Hold Time (ms):",
                 m_holdTimeSlider,   0.0, 500.0, 1.0, 10.0,
                 m_holdTimeValueLabel, " ms");

    addSliderRow(m_leadingPadLabel,  "Leading Pad (ms):",
                 m_leadingPadSlider, 0.0, 2000.0, 1.0, 0.0,
                 m_leadingPadValueLabel, " ms");

    addSliderRow(m_trailingPadLabel, "Trailing Pad (ms):",
                 m_trailingPadSlider, 0.0, 2000.0, 1.0, 0.0,
                 m_trailingPadValueLabel, " ms");

    addSliderRow(m_minKeptLabel,     "Min Kept (ms):",
                 m_minKeptSlider,    0.0, 10000.0, 10.0, 100.0,
                 m_minKeptValueLabel, " ms");

    // Max Trim — special: shows "no limit" when 0
    m_maxTrimLabel.setText("Max Trim (ms):", juce::dontSendNotification);
    m_maxTrimLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_maxTrimLabel);

    m_maxTrimSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_maxTrimSlider.setRange(0.0, 30000.0, 10.0);
    m_maxTrimSlider.setValue(0.0);
    m_maxTrimSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_maxTrimSlider.onValueChange = [this]()
    {
        double v = m_maxTrimSlider.getValue();
        if (v <= 0.0)
            m_maxTrimValueLabel.setText("no limit", juce::dontSendNotification);
        else
            m_maxTrimValueLabel.setText(juce::String(v, 0) + " ms", juce::dontSendNotification);
        updateSummary();
    };
    addAndMakeVisible(m_maxTrimSlider);

    m_maxTrimValueLabel.setText("no limit", juce::dontSendNotification);
    m_maxTrimValueLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_maxTrimValueLabel);

    //--------------------------------------------------------------------------
    // Section 2: Time-Based Edits

    addSliderRow(m_headTrimLabel,  "Head Trim (ms):",
                 m_headTrimSlider, 0.0, 10000.0, 1.0, 0.0,
                 m_headTrimValueLabel, " ms");

    addSliderRow(m_tailTrimLabel,  "Tail Trim (ms):",
                 m_tailTrimSlider, 0.0, 10000.0, 1.0, 0.0,
                 m_tailTrimValueLabel, " ms");

    addSliderRow(m_prependLabel,   "Prepend Silence (ms):",
                 m_prependSlider,  0.0, 5000.0, 1.0, 0.0,
                 m_prependValueLabel, " ms");

    addSliderRow(m_appendLabel,    "Append Silence (ms):",
                 m_appendSlider,   0.0, 5000.0, 1.0, 0.0,
                 m_appendValueLabel, " ms");

    // Fade In — with curve combo appended after value label
    addSliderRow(m_fadeInLabel,    "Fade In (ms):",
                 m_fadeInSlider,   0.0, 5000.0, 1.0, 0.0,
                 m_fadeInValueLabel, " ms");

    m_fadeInCurveLabel.setText("Curve:", juce::dontSendNotification);
    m_fadeInCurveLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_fadeInCurveLabel);

    m_fadeInCurveCombo.addItem("Linear",      1);
    m_fadeInCurveCombo.addItem("Exponential", 2);
    m_fadeInCurveCombo.addItem("Logarithmic", 3);
    m_fadeInCurveCombo.addItem("S-Curve",     4);
    m_fadeInCurveCombo.setSelectedId(1, juce::dontSendNotification);
    m_fadeInCurveCombo.onChange = [this]() { updateSummary(); };
    addAndMakeVisible(m_fadeInCurveCombo);

    // Fade Out — with curve combo
    addSliderRow(m_fadeOutLabel,   "Fade Out (ms):",
                 m_fadeOutSlider,  0.0, 5000.0, 1.0, 0.0,
                 m_fadeOutValueLabel, " ms");

    m_fadeOutCurveLabel.setText("Curve:", juce::dontSendNotification);
    m_fadeOutCurveLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_fadeOutCurveLabel);

    m_fadeOutCurveCombo.addItem("Linear",      1);
    m_fadeOutCurveCombo.addItem("Exponential", 2);
    m_fadeOutCurveCombo.addItem("Logarithmic", 3);
    m_fadeOutCurveCombo.addItem("S-Curve",     4);
    m_fadeOutCurveCombo.setSelectedId(1, juce::dontSendNotification);
    m_fadeOutCurveCombo.onChange = [this]() { updateSummary(); };
    addAndMakeVisible(m_fadeOutCurveCombo);

    //--------------------------------------------------------------------------
    // Waveform preview

    m_waveformPreview = std::make_unique<WaveformPreview>(m_audioBuffer);
    addAndMakeVisible(m_waveformPreview.get());

    //--------------------------------------------------------------------------
    // Recipe summary

    m_summaryEditor.setMultiLine(true);
    m_summaryEditor.setReadOnly(true);
    m_summaryEditor.setScrollbarsShown(true);
    m_summaryEditor.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(m_summaryEditor);

    //--------------------------------------------------------------------------
    // Buttons

    m_previewButton.setButtonText("Preview");
    m_previewButton.onClick = [this]()
    {
        auto recipe = buildRecipe();

        // Run detection to get visual boundaries
        if (recipe.detectEnabled && m_audioBuffer.getNumSamples() > 0)
        {
            auto [boundaryStart, boundaryEnd] =
                HeadTailEngine::detectBoundaries(m_audioBuffer, m_sampleRate, recipe);

            m_waveformPreview->setDetectionBoundaries(boundaryStart, boundaryEnd);

            // Estimate final keep region after pads
            auto padSamples = [&](float ms) -> int64_t
            {
                return static_cast<int64_t>(ms * m_sampleRate / 1000.0);
            };

            int64_t keepStart = juce::jmax((int64_t)0,
                                           boundaryStart - padSamples(recipe.leadingPadMs));
            int64_t keepEnd   = juce::jmin((int64_t)m_audioBuffer.getNumSamples(),
                                            boundaryEnd + padSamples(recipe.trailingPadMs));

            m_waveformPreview->setTrimRegion(keepStart, keepEnd);
        }
        else
        {
            // Show trim based on fixed head/tail sliders only
            auto padSamples = [&](float ms) -> int64_t
            {
                return static_cast<int64_t>(ms * m_sampleRate / 1000.0);
            };

            int64_t keepStart = padSamples(recipe.headTrimMs);
            int64_t totalSamples = m_audioBuffer.getNumSamples();
            int64_t keepEnd   = totalSamples - padSamples(recipe.tailTrimMs);
            keepEnd = juce::jmax(keepStart, keepEnd);

            m_waveformPreview->setTrimRegion(keepStart, keepEnd);
        }

        updateSummary();
    };
    addAndMakeVisible(m_previewButton);

    m_applyButton.setButtonText("Apply");
    m_applyButton.onClick = [this]()
    {
        if (onApply)
            onApply(buildRecipe());

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(1);
    };
    addAndMakeVisible(m_applyButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]()
    {
        if (onCancel)
            onCancel();

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(m_cancelButton);

    //--------------------------------------------------------------------------
    // Initial state

    updateDetectionControlsEnabled();
    updateSummary();

    setSize(700, 680);
}

HeadTailDialog::~HeadTailDialog()
{
}

//==============================================================================
// Component overrides

void HeadTailDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.drawText("Head & Tail Processing", getLocalBounds().removeFromTop(36),
               juce::Justification::centred, true);

    // Section dividers (drawn behind controls using a fixed Y based on layout constants)
    // The actual divider lines are drawn as thin rectangles relative to getLocalBounds.
    // Positions are derived from the same constants used in resized().
    const int kMargin     = 16;
    const int kRowH       = 32;
    const int kHeaderH    = 36;
    const int kSection1Y  = kHeaderH + kMargin;
    // Section 1 header rect
    g.setColour(juce::Colour(0xff333333));
    g.fillRoundedRectangle((float)kMargin, (float)kSection1Y,
                            (float)(getWidth() - kMargin * 2), 20.0f, 3.0f);

    // Section 2 starts after 1 header row + 6 slider rows (each kRowH)
    const int kSection2Y = kSection1Y + 20 + 8 + kRowH + 6 * kRowH + 8;
    g.fillRoundedRectangle((float)kMargin, (float)kSection2Y,
                            (float)(getWidth() - kMargin * 2), 20.0f, 3.0f);

    // Section labels
    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("TIME-BASED EDITS",
               juce::Rectangle<int>(kMargin + 4, kSection2Y, 200, 20),
               juce::Justification::centredLeft, true);
}

void HeadTailDialog::resized()
{
    const int kMargin   = 16;
    const int kLabelW   = 180;
    const int kSliderW  = 280;
    const int kValueW   = 80;
    const int kRowH     = 32;
    const int kHeaderH  = 36;
    const int kGap      = 8;

    auto area = getLocalBounds().reduced(kMargin);
    area.removeFromTop(kHeaderH);   // Title space

    //--------------------------------------------------------------------------
    // Section 1 header + detection enable row

    // Section header band (painted in paint(), just advance bounds)
    area.removeFromTop(20);
    area.removeFromTop(kGap);

    // Detection enable toggle + mode combo row
    {
        auto row = area.removeFromTop(kRowH);
        m_detectEnableToggle.setBounds(row.removeFromLeft(160));
        row.removeFromLeft(kGap);
        m_detectModeLabel.setBounds(row.removeFromLeft(50));
        row.removeFromLeft(4);
        m_detectModeCombo.setBounds(row.removeFromLeft(100));
    }

    // Detection slider rows
    auto layoutRow = [&](juce::Label& lbl, juce::Slider& slider, juce::Label& val)
    {
        auto row = area.removeFromTop(kRowH);
        lbl.setBounds(row.removeFromLeft(kLabelW));
        slider.setBounds(row.removeFromLeft(kSliderW));
        val.setBounds(row.removeFromLeft(kValueW));
    };

    layoutRow(m_thresholdLabel,   m_thresholdSlider,   m_thresholdValueLabel);
    layoutRow(m_holdTimeLabel,    m_holdTimeSlider,     m_holdTimeValueLabel);
    layoutRow(m_leadingPadLabel,  m_leadingPadSlider,   m_leadingPadValueLabel);
    layoutRow(m_trailingPadLabel, m_trailingPadSlider,  m_trailingPadValueLabel);
    layoutRow(m_minKeptLabel,     m_minKeptSlider,      m_minKeptValueLabel);
    layoutRow(m_maxTrimLabel,     m_maxTrimSlider,      m_maxTrimValueLabel);

    //--------------------------------------------------------------------------
    // Section 2 header

    area.removeFromTop(kGap);
    area.removeFromTop(20);   // Section header band height (painted in paint())
    area.removeFromTop(kGap);

    // Time-based edit rows
    layoutRow(m_headTrimLabel, m_headTrimSlider, m_headTrimValueLabel);
    layoutRow(m_tailTrimLabel, m_tailTrimSlider, m_tailTrimValueLabel);
    layoutRow(m_prependLabel,  m_prependSlider,  m_prependValueLabel);
    layoutRow(m_appendLabel,   m_appendSlider,   m_appendValueLabel);

    // Fade In row — slider + value + curve label + curve combo
    {
        auto row = area.removeFromTop(kRowH);
        m_fadeInLabel.setBounds(row.removeFromLeft(kLabelW));
        m_fadeInSlider.setBounds(row.removeFromLeft(kSliderW));
        m_fadeInValueLabel.setBounds(row.removeFromLeft(kValueW));
        row.removeFromLeft(4);
        m_fadeInCurveLabel.setBounds(row.removeFromLeft(46));
        m_fadeInCurveCombo.setBounds(row.removeFromLeft(100));
    }

    // Fade Out row
    {
        auto row = area.removeFromTop(kRowH);
        m_fadeOutLabel.setBounds(row.removeFromLeft(kLabelW));
        m_fadeOutSlider.setBounds(row.removeFromLeft(kSliderW));
        m_fadeOutValueLabel.setBounds(row.removeFromLeft(kValueW));
        row.removeFromLeft(4);
        m_fadeOutCurveLabel.setBounds(row.removeFromLeft(46));
        m_fadeOutCurveCombo.setBounds(row.removeFromLeft(100));
    }

    //--------------------------------------------------------------------------
    // Waveform preview (140px)

    area.removeFromTop(kGap);
    m_waveformPreview->setBounds(area.removeFromTop(140));

    //--------------------------------------------------------------------------
    // Recipe summary (80px)

    area.removeFromTop(kGap);
    m_summaryEditor.setBounds(area.removeFromTop(80));

    //--------------------------------------------------------------------------
    // Button row — three centred buttons

    area.removeFromTop(kGap);
    auto buttonRow = area.removeFromTop(30);

    const int kBtnW    = 100;
    const int kBtnGap  = 10;
    const int totalBW  = kBtnW * 3 + kBtnGap * 2;
    const int startX   = buttonRow.getX() + (buttonRow.getWidth() - totalBW) / 2;

    m_previewButton.setBounds(startX,                        buttonRow.getY(), kBtnW, 30);
    m_applyButton.setBounds  (startX + kBtnW + kBtnGap,     buttonRow.getY(), kBtnW, 30);
    m_cancelButton.setBounds (startX + (kBtnW + kBtnGap)*2, buttonRow.getY(), kBtnW, 30);
}

//==============================================================================
// Helper methods

void HeadTailDialog::addSliderRow(juce::Label& label, const juce::String& text,
                                   juce::Slider& slider, double min, double max,
                                   double step, double defaultVal,
                                   juce::Label& valueLabel, const juce::String& suffix)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setRange(min, max, step);
    slider.setValue(defaultVal);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.onValueChange = [this, &slider, &valueLabel, suffix]()
    {
        updateValueLabel(slider, valueLabel, suffix);
        updateSummary();
    };
    addAndMakeVisible(slider);

    // Initialise value label text from default
    juce::String initText = (step < 1.0)
        ? juce::String(defaultVal, 1) + suffix
        : juce::String(defaultVal, 0) + suffix;
    valueLabel.setText(initText, juce::dontSendNotification);
    valueLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(valueLabel);
}

void HeadTailDialog::updateValueLabel(juce::Slider& slider, juce::Label& label,
                                       const juce::String& suffix)
{
    double v = slider.getValue();
    // Use one decimal for sub-unit steps, zero decimals otherwise
    juce::String text = (slider.getInterval() < 1.0)
        ? juce::String(v, 1) + suffix
        : juce::String(v, 0) + suffix;
    label.setText(text, juce::dontSendNotification);
}

void HeadTailDialog::updateDetectionControlsEnabled()
{
    const bool enabled = m_detectEnableToggle.getToggleState();

    m_detectModeLabel.setEnabled(enabled);
    m_detectModeCombo.setEnabled(enabled);
    m_thresholdLabel.setEnabled(enabled);
    m_thresholdSlider.setEnabled(enabled);
    m_thresholdValueLabel.setEnabled(enabled);
    m_holdTimeLabel.setEnabled(enabled);
    m_holdTimeSlider.setEnabled(enabled);
    m_holdTimeValueLabel.setEnabled(enabled);
    m_leadingPadLabel.setEnabled(enabled);
    m_leadingPadSlider.setEnabled(enabled);
    m_leadingPadValueLabel.setEnabled(enabled);
    m_trailingPadLabel.setEnabled(enabled);
    m_trailingPadSlider.setEnabled(enabled);
    m_trailingPadValueLabel.setEnabled(enabled);
    m_minKeptLabel.setEnabled(enabled);
    m_minKeptSlider.setEnabled(enabled);
    m_minKeptValueLabel.setEnabled(enabled);
    m_maxTrimLabel.setEnabled(enabled);
    m_maxTrimSlider.setEnabled(enabled);
    m_maxTrimValueLabel.setEnabled(enabled);
}

HeadTailRecipe HeadTailDialog::buildRecipe() const
{
    HeadTailRecipe recipe;

    // Detection section
    recipe.detectEnabled = m_detectEnableToggle.getToggleState();
    recipe.detectionMode = (m_detectModeCombo.getSelectedId() == 2)
        ? HeadTailRecipe::DetectionMode::RMS
        : HeadTailRecipe::DetectionMode::Peak;
    recipe.thresholdDB       = (float)m_thresholdSlider.getValue();
    recipe.holdTimeMs        = (float)m_holdTimeSlider.getValue();
    recipe.leadingPadMs      = (float)m_leadingPadSlider.getValue();
    recipe.trailingPadMs     = (float)m_trailingPadSlider.getValue();
    recipe.minKeptDurationMs = (float)m_minKeptSlider.getValue();
    recipe.maxTrimMs         = (float)m_maxTrimSlider.getValue();

    // Head trim
    float headTrim = (float)m_headTrimSlider.getValue();
    recipe.headTrimEnabled = (headTrim > 0.0f);
    recipe.headTrimMs      = headTrim;

    // Tail trim
    float tailTrim = (float)m_tailTrimSlider.getValue();
    recipe.tailTrimEnabled = (tailTrim > 0.0f);
    recipe.tailTrimMs      = tailTrim;

    // Prepend silence
    float prepend = (float)m_prependSlider.getValue();
    recipe.prependSilenceEnabled = (prepend > 0.0f);
    recipe.prependSilenceMs      = prepend;

    // Append silence
    float append = (float)m_appendSlider.getValue();
    recipe.appendSilenceEnabled = (append > 0.0f);
    recipe.appendSilenceMs      = append;

    // Fade in
    float fadeIn = (float)m_fadeInSlider.getValue();
    recipe.fadeInEnabled = (fadeIn > 0.0f);
    recipe.fadeInMs      = fadeIn;
    recipe.fadeInCurve   = static_cast<FadeCurveType>(m_fadeInCurveCombo.getSelectedId() - 1);

    // Fade out
    float fadeOut = (float)m_fadeOutSlider.getValue();
    recipe.fadeOutEnabled = (fadeOut > 0.0f);
    recipe.fadeOutMs      = fadeOut;
    recipe.fadeOutCurve   = static_cast<FadeCurveType>(m_fadeOutCurveCombo.getSelectedId() - 1);

    return recipe;
}

void HeadTailDialog::updateSummary()
{
    auto recipe = buildRecipe();
    juce::String summary;
    int step = 1;

    if (recipe.detectEnabled)
    {
        juce::String mode = (recipe.detectionMode == HeadTailRecipe::DetectionMode::Peak)
            ? "peak" : "RMS";
        summary += juce::String(step++) + ". Detect: trim to content (threshold "
                   + juce::String(recipe.thresholdDB, 1) + " dB, " + mode + " mode)\n";
    }

    if (recipe.headTrimEnabled && recipe.headTrimMs > 0.0f)
        summary += juce::String(step++) + ". Head trim: remove "
                   + juce::String(recipe.headTrimMs, 0) + " ms\n";

    if (recipe.tailTrimEnabled && recipe.tailTrimMs > 0.0f)
        summary += juce::String(step++) + ". Tail trim: remove "
                   + juce::String(recipe.tailTrimMs, 0) + " ms\n";

    if (recipe.prependSilenceEnabled && recipe.prependSilenceMs > 0.0f)
        summary += juce::String(step++) + ". Prepend silence: add "
                   + juce::String(recipe.prependSilenceMs, 0) + " ms\n";

    if (recipe.appendSilenceEnabled && recipe.appendSilenceMs > 0.0f)
        summary += juce::String(step++) + ". Append silence: add "
                   + juce::String(recipe.appendSilenceMs, 0) + " ms\n";

    if (recipe.fadeInEnabled && recipe.fadeInMs > 0.0f)
        summary += juce::String(step++) + ". Fade in: "
                   + juce::String(recipe.fadeInMs, 0) + " ms\n";

    if (recipe.fadeOutEnabled && recipe.fadeOutMs > 0.0f)
        summary += juce::String(step++) + ". Fade out: "
                   + juce::String(recipe.fadeOutMs, 0) + " ms\n";

    if (step == 1)
        summary = "(No operations configured)";

    if (m_sampleRate > 0.0 && m_audioBuffer.getNumSamples() > 0)
    {
        double originalMs = ((double)m_audioBuffer.getNumSamples() / m_sampleRate) * 1000.0;
        summary += "\nDuration: " + juce::String(originalMs, 1) + " ms";
    }

    m_summaryEditor.setText(summary, juce::dontSendNotification);
}
