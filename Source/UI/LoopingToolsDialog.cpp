/*
  ==============================================================================

    LoopingToolsDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "LoopingToolsDialog.h"

//==============================================================================
// WaveformPreview implementation
//==============================================================================

void LoopingToolsDialog::WaveformPreview::setBuffer(const juce::AudioBuffer<float>& buffer)
{
    m_buffer = &buffer;
    repaint();
}

void LoopingToolsDialog::WaveformPreview::setCrossfadeLength(int64_t samples)
{
    m_crossfadeLen = samples;
    repaint();
}

void LoopingToolsDialog::WaveformPreview::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff1a1a1a));
    g.setColour(juce::Colours::grey);
    g.drawRect(getLocalBounds(), 1);

    if (m_buffer == nullptr || m_buffer->getNumSamples() == 0)
    {
        g.setColour(juce::Colour(0xff888888));
        g.drawText("Click Preview to see loop waveform",
                   getLocalBounds(), juce::Justification::centred);
        return;
    }

    auto bounds = getLocalBounds().reduced(2);

    // Crossfade zone overlay at the start of the loop (where tail blends into head)
    if (m_crossfadeLen > 0 && m_buffer->getNumSamples() > 0)
    {
        float xfadeWidth = (float)bounds.getWidth()
                           * (float)m_crossfadeLen
                           / (float)m_buffer->getNumSamples();

        g.setColour(juce::Colour(0xff4488ff).withAlpha(0.15f));
        g.fillRect((float)bounds.getX(), (float)bounds.getY(),
                   xfadeWidth, (float)bounds.getHeight());
    }

    drawWaveform(g, bounds);

    // Crossfade zone border line (left edge marks loop point)
    if (m_crossfadeLen > 0 && m_buffer->getNumSamples() > 0)
    {
        float xfadeWidth = (float)bounds.getWidth()
                           * (float)m_crossfadeLen
                           / (float)m_buffer->getNumSamples();

        g.setColour(juce::Colour(0xff4488ff).withAlpha(0.6f));
        float lineX = (float)bounds.getX() + xfadeWidth;
        g.drawLine(lineX, (float)bounds.getY(),
                   lineX, (float)bounds.getBottom(), 1.0f);
    }
}

void LoopingToolsDialog::WaveformPreview::drawWaveform(juce::Graphics& g,
                                                        juce::Rectangle<int> bounds)
{
    const int numChannels = m_buffer->getNumChannels();
    const int numSamples  = m_buffer->getNumSamples();
    const int width       = bounds.getWidth();
    const int height      = bounds.getHeight();

    if (numSamples == 0 || width == 0)
        return;

    const int samplesPerPixel = juce::jmax(1, numSamples / width);
    const int channelHeight   = height / juce::jmax(1, numChannels);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float* data     = m_buffer->getReadPointer(channel);
        const int    chanY    = bounds.getY() + channel * channelHeight;
        const int    centerY  = chanY + channelHeight / 2;

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
            const int minY   = centerY + (int)(minVal * (float)channelHeight / 2.0f);
            const int maxY   = centerY - (int)(maxVal * (float)channelHeight / 2.0f);

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

//==============================================================================
// LoopingToolsDialog constructor
//==============================================================================

LoopingToolsDialog::LoopingToolsDialog(const juce::AudioBuffer<float>& buffer,
                                        double sampleRate,
                                        int64_t selectionStart,
                                        int64_t selectionEnd,
                                        const juce::File& sourceFile)
    : m_audioBuffer(buffer)
    , m_sampleRate(sampleRate)
    , m_selectionStart(selectionStart)
    , m_selectionEnd(selectionEnd)
    , m_sourceFile(sourceFile)
    , m_outputDirectory(sourceFile.getParentDirectory())
{
    //--------------------------------------------------------------------------
    // Section 1: Loop Settings

    // Loop Count
    addSliderRow(m_loopCountLabel,      "Loop Count:",
                 m_loopCountSlider,     1.0, 100.0, 1.0, 1.0,
                 m_loopCountValueLabel, "");
    m_loopCountSlider.onValueChange = [this]()
    {
        updateValueLabel(m_loopCountSlider, m_loopCountValueLabel, "");
        updateVariationControlsEnabled();
        updateFilePreview();
    };

    // Crossfade Length + mode combo
    addSliderRow(m_crossfadeLenLabel,      "Crossfade Length:",
                 m_crossfadeLenSlider,     1.0, 500.0, 1.0, 50.0,
                 m_crossfadeLenValueLabel, " ms");
    m_crossfadeLenSlider.onValueChange = [this]()
    {
        int modeId = m_crossfadeModeCombo.getSelectedId();
        juce::String suffix = (modeId == 2) ? " %" : " ms";
        updateValueLabel(m_crossfadeLenSlider, m_crossfadeLenValueLabel, suffix);
        schedulePreviewRebuild();
    };

    m_crossfadeModeCombo.addItem("ms", 1);
    m_crossfadeModeCombo.addItem("%",  2);
    m_crossfadeModeCombo.setSelectedId(1, juce::dontSendNotification);
    m_crossfadeModeCombo.onChange = [this]()
    {
        // When mode switches to % update range/default and suffix
        if (m_crossfadeModeCombo.getSelectedId() == 2)
        {
            m_crossfadeLenSlider.setRange(1.0, 100.0, 1.0);
            m_crossfadeLenSlider.setValue(10.0, juce::dontSendNotification);
            updateValueLabel(m_crossfadeLenSlider, m_crossfadeLenValueLabel, " %");
        }
        else
        {
            m_crossfadeLenSlider.setRange(1.0, 500.0, 1.0);
            m_crossfadeLenSlider.setValue(50.0, juce::dontSendNotification);
            updateValueLabel(m_crossfadeLenSlider, m_crossfadeLenValueLabel, " ms");
        }
        schedulePreviewRebuild();
    };
    addAndMakeVisible(m_crossfadeModeCombo);

    // Max Crossfade
    addSliderRow(m_maxCrossfadeLabel,      "Max Crossfade:",
                 m_maxCrossfadeSlider,     10.0, 2000.0, 10.0, 500.0,
                 m_maxCrossfadeValueLabel, " ms");

    // Crossfade Curve
    m_crossfadeCurveLabel.setText("Crossfade Curve:", juce::dontSendNotification);
    m_crossfadeCurveLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_crossfadeCurveLabel);

    m_crossfadeCurveCombo.addItem("Linear",       1);
    m_crossfadeCurveCombo.addItem("Equal Power",  2);
    m_crossfadeCurveCombo.addItem("S-Curve",      3);
    m_crossfadeCurveCombo.setSelectedId(2, juce::dontSendNotification);
    m_crossfadeCurveCombo.onChange = [this]() { schedulePreviewRebuild(); };
    addAndMakeVisible(m_crossfadeCurveCombo);

    // Zero-Crossing toggle + Search Window slider
    m_zeroCrossingToggle.setButtonText("Zero-Crossing Snap");
    m_zeroCrossingToggle.setToggleState(true, juce::dontSendNotification);
    m_zeroCrossingToggle.onStateChange = [this]()
    {
        updateZeroCrossingControlsEnabled();
        schedulePreviewRebuild();
    };
    addAndMakeVisible(m_zeroCrossingToggle);

    addSliderRow(m_searchWindowLabel,      "Search Window:",
                 m_searchWindowSlider,     100.0, 5000.0, 100.0, 1000.0,
                 m_searchWindowValueLabel, " smp");

    //--------------------------------------------------------------------------
    // Section 2: Variation Settings

    addSliderRow(m_offsetStepLabel,      "Offset Step:",
                 m_offsetStepSlider,     0.0, 10000.0, 1.0, 0.0,
                 m_offsetStepValueLabel, " ms");
    m_offsetStepSlider.onValueChange = [this]()
    {
        updateValueLabel(m_offsetStepSlider, m_offsetStepValueLabel, " ms");
    };

    addSliderRow(m_shuffleSeedLabel,      "Shuffle Seed:",
                 m_shuffleSeedSlider,     0.0, 9999.0, 1.0, 0.0,
                 m_shuffleSeedValueLabel, "");
    m_shuffleSeedSlider.onValueChange = [this]()
    {
        updateValueLabel(m_shuffleSeedSlider, m_shuffleSeedValueLabel, "");
    };

    //--------------------------------------------------------------------------
    // Section 2.5: Shepard Tone

    m_shepardEnable.setButtonText("Enable Shepard Tone");
    m_shepardEnable.setToggleState(false, juce::dontSendNotification);
    m_shepardEnable.onStateChange = [this]()
    {
        updateShepardControlsEnabled();
        updateShepardConstraint();
        updatePreview();
        updateFilePreview();
        schedulePreviewRebuild();
    };
    addAndMakeVisible(m_shepardEnable);

    addSliderRow(m_shepardSemitonesLabel, "Semitones:",
                 m_shepardSemitonesSlider, 0.1, 12.0, 0.1, 1.0,
                 m_shepardSemitonesValueLabel, " st");
    m_shepardSemitonesSlider.onValueChange = [this]()
    {
        updateValueLabel(m_shepardSemitonesSlider, m_shepardSemitonesValueLabel, " st");
        updateShepardConstraint();
        updatePreview();
        updateFilePreview();
        schedulePreviewRebuild();
    };

    m_shepardDirectionLabel.setText("Direction:", juce::dontSendNotification);
    m_shepardDirectionLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_shepardDirectionLabel);

    m_shepardDirectionCombo.addItem("Up", 1);
    m_shepardDirectionCombo.addItem("Down", 2);
    m_shepardDirectionCombo.setSelectedId(1, juce::dontSendNotification);
    m_shepardDirectionCombo.onChange = [this]()
    {
        updatePreview();
        updateFilePreview();
        schedulePreviewRebuild();
    };
    addAndMakeVisible(m_shepardDirectionCombo);

    m_shepardRampLabel.setText("Ramp Shape:", juce::dontSendNotification);
    m_shepardRampLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_shepardRampLabel);

    m_shepardRampCombo.addItem("Linear", 1);
    m_shepardRampCombo.addItem("Exponential", 2);
    m_shepardRampCombo.setSelectedId(1, juce::dontSendNotification);
    m_shepardRampCombo.onChange = [this]()
    {
        updatePreview();
        updateFilePreview();
        schedulePreviewRebuild();
    };
    addAndMakeVisible(m_shepardRampCombo);

    m_shepardConstraintLabel.setFont(juce::FontOptions(11.0f));
    m_shepardConstraintLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_shepardConstraintLabel);

    //--------------------------------------------------------------------------
    // Section 3: Output

    m_outputDirLabel.setText("Output Directory:", juce::dontSendNotification);
    m_outputDirLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_outputDirLabel);

    m_outputDirPathLabel.setText(m_outputDirectory.getFullPathName(),
                                  juce::dontSendNotification);
    m_outputDirPathLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_outputDirPathLabel);

    m_browseButton.setButtonText("Browse...");
    m_browseButton.onClick = [this]()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Output Directory",
            m_outputDirectory,
            "");

        chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto results = fc.getResults();
                if (results.size() > 0)
                {
                    m_outputDirectory = results[0];
                    m_outputDirPathLabel.setText(m_outputDirectory.getFullPathName(),
                                                  juce::dontSendNotification);
                }
            });
    };
    addAndMakeVisible(m_browseButton);

    m_suffixLabel.setText("Suffix:", juce::dontSendNotification);
    m_suffixLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_suffixLabel);

    m_suffixEditor.setText("_loop");
    m_suffixEditor.onTextChange = [this]() { updateFilePreview(); };
    addAndMakeVisible(m_suffixEditor);

    m_bitDepthLabel.setText("Bit Depth:", juce::dontSendNotification);
    m_bitDepthLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_bitDepthLabel);

    m_bitDepthCombo.addItem("16 bit", 1);
    m_bitDepthCombo.addItem("24 bit", 2);
    m_bitDepthCombo.addItem("32 bit", 3);
    m_bitDepthCombo.setSelectedId(2, juce::dontSendNotification);  // Default: 24-bit
    addAndMakeVisible(m_bitDepthCombo);

    m_filePreviewHeaderLabel.setText("Output Files:", juce::dontSendNotification);
    m_filePreviewHeaderLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_filePreviewHeaderLabel);

    m_filePreviewLabel.setJustificationType(juce::Justification::centredLeft);
    m_filePreviewLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(m_filePreviewLabel);

    //--------------------------------------------------------------------------
    // Section 4: Preview

    m_waveformPreview = std::make_unique<WaveformPreview>();
    addAndMakeVisible(m_waveformPreview.get());

    m_diagnosticsLabel.setJustificationType(juce::Justification::topLeft);
    m_diagnosticsLabel.setFont(juce::FontOptions(11.0f));
    m_diagnosticsLabel.setText("(no preview yet)", juce::dontSendNotification);
    addAndMakeVisible(m_diagnosticsLabel);

    //--------------------------------------------------------------------------
    // Buttons

    m_previewPlaybackButton.setButtonText("\xe2\x96\xb6 Preview");
    m_previewPlaybackButton.onClick = [this]() { togglePreviewPlayback(); };
    addAndMakeVisible(m_previewPlaybackButton);

    m_previewButton.setButtonText("Preview");
    m_previewButton.onClick = [this]() { updatePreview(); };
    addAndMakeVisible(m_previewButton);

    m_exportButton.setButtonText("Export");
    m_exportButton.onClick = [this]() { exportLoops(); };
    addAndMakeVisible(m_exportButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]()
    {
        // Stop preview playback if active
        if (m_isPreviewPlaying)
            togglePreviewPlayback();

        if (onCancel)
            onCancel();

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(m_cancelButton);

    //--------------------------------------------------------------------------
    // Initial state

    updateVariationControlsEnabled();
    updateZeroCrossingControlsEnabled();
    updateShepardControlsEnabled();
    updateShepardConstraint();
    updateFilePreview();

    setSize(760, 800);
}

LoopingToolsDialog::~LoopingToolsDialog()
{
    stopTimer();

    if (m_isPreviewPlaying && onPreviewStopped)
        onPreviewStopped();
}

//==============================================================================
// Component overrides
//==============================================================================

void LoopingToolsDialog::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    // Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.drawText("Looping Tools", getLocalBounds().removeFromTop(36),
               juce::Justification::centred, true);

    // Section header backgrounds
    const int kMargin    = 16;
    const int kHeaderH   = 36;
    const int kRowH      = 32;
    const int kGap       = 8;
    const int kSectionBandH = 20;

    // Section 1 header Y
    const int sec1Y = kHeaderH + kGap;

    // Section 1 content: 1 toggle row + 1 search window row + crossfade len row +
    //                     max crossfade row + curve row = 5 rows after header
    const int sec2Y = sec1Y + kSectionBandH + kGap
                    + kRowH          // loop count
                    + kRowH          // crossfade length
                    + kRowH          // max crossfade
                    + kRowH          // crossfade curve
                    + kRowH          // zero-crossing toggle
                    + kRowH          // search window
                    + kGap;

    // Shepard Tone section starts after Section 2
    const int secShepY = sec2Y + kSectionBandH + kGap
                    + kRowH          // offset step
                    + kRowH          // shuffle seed
                    + kGap;

    // Section 3 (Output) starts after Shepard section
    const int sec3Y = secShepY + kSectionBandH + kGap
                    + kRowH          // enable toggle
                    + kRowH          // semitones
                    + kRowH          // direction
                    + kRowH          // ramp shape
                    + kRowH          // constraint label
                    + kGap;

    const int sectionW = getWidth() - kMargin * 2;

    g.setColour(juce::Colour(0xff333333));
    g.fillRoundedRectangle((float)kMargin, (float)sec1Y,
                            (float)sectionW, (float)kSectionBandH, 3.0f);
    g.fillRoundedRectangle((float)kMargin, (float)sec2Y,
                            (float)sectionW, (float)kSectionBandH, 3.0f);
    g.fillRoundedRectangle((float)kMargin, (float)secShepY,
                            (float)sectionW, (float)kSectionBandH, 3.0f);
    g.fillRoundedRectangle((float)kMargin, (float)sec3Y,
                            (float)sectionW, (float)kSectionBandH, 3.0f);

    g.setColour(juce::Colour(0xffaaaaaa));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("LOOP SETTINGS",
               juce::Rectangle<int>(kMargin + 4, sec1Y, 200, kSectionBandH),
               juce::Justification::centredLeft, true);
    g.drawText("VARIATION SETTINGS",
               juce::Rectangle<int>(kMargin + 4, sec2Y, 200, kSectionBandH),
               juce::Justification::centredLeft, true);
    g.drawText("SHEPARD TONE",
               juce::Rectangle<int>(kMargin + 4, secShepY, 200, kSectionBandH),
               juce::Justification::centredLeft, true);
    g.drawText("OUTPUT",
               juce::Rectangle<int>(kMargin + 4, sec3Y, 200, kSectionBandH),
               juce::Justification::centredLeft, true);
}

void LoopingToolsDialog::resized()
{
    const int kMargin  = 16;
    const int kLabelW  = 180;
    const int kSliderW = 260;
    const int kValueW  = 72;
    const int kRowH    = 32;
    const int kHeaderH = 36;
    const int kGap     = 8;
    const int kBandH   = 20;   // Section header band height

    auto area = getLocalBounds().reduced(kMargin);
    area.removeFromTop(kHeaderH);   // Title

    // Convenience lambda: lay out a standard slider row
    auto layoutRow = [&](juce::Label& lbl, juce::Slider& slider, juce::Label& val)
    {
        auto row = area.removeFromTop(kRowH);
        lbl.setBounds(row.removeFromLeft(kLabelW));
        slider.setBounds(row.removeFromLeft(kSliderW));
        val.setBounds(row.removeFromLeft(kValueW));
    };

    //--------------------------------------------------------------------------
    // Section 1: Loop Settings

    area.removeFromTop(kGap);
    area.removeFromTop(kBandH);   // Section header (painted in paint())
    area.removeFromTop(kGap);

    // Loop Count
    layoutRow(m_loopCountLabel, m_loopCountSlider, m_loopCountValueLabel);

    // Crossfade Length + mode combo
    {
        auto row = area.removeFromTop(kRowH);
        m_crossfadeLenLabel.setBounds(row.removeFromLeft(kLabelW));
        m_crossfadeLenSlider.setBounds(row.removeFromLeft(kSliderW));
        m_crossfadeLenValueLabel.setBounds(row.removeFromLeft(kValueW));
        row.removeFromLeft(kGap);
        m_crossfadeModeCombo.setBounds(row.removeFromLeft(60));
    }

    // Max Crossfade
    layoutRow(m_maxCrossfadeLabel, m_maxCrossfadeSlider, m_maxCrossfadeValueLabel);

    // Crossfade Curve
    {
        auto row = area.removeFromTop(kRowH);
        m_crossfadeCurveLabel.setBounds(row.removeFromLeft(kLabelW));
        m_crossfadeCurveCombo.setBounds(row.removeFromLeft(140));
    }

    // Zero-Crossing toggle
    {
        auto row = area.removeFromTop(kRowH);
        m_zeroCrossingToggle.setBounds(row.removeFromLeft(200));
    }

    // Search Window
    layoutRow(m_searchWindowLabel, m_searchWindowSlider, m_searchWindowValueLabel);

    //--------------------------------------------------------------------------
    // Section 2: Variation Settings

    area.removeFromTop(kGap);
    area.removeFromTop(kBandH);
    area.removeFromTop(kGap);

    layoutRow(m_offsetStepLabel, m_offsetStepSlider, m_offsetStepValueLabel);
    layoutRow(m_shuffleSeedLabel, m_shuffleSeedSlider, m_shuffleSeedValueLabel);

    //--------------------------------------------------------------------------
    // Section 2.5: Shepard Tone

    area.removeFromTop(kGap);
    area.removeFromTop(kBandH);
    area.removeFromTop(kGap);

    // Enable toggle
    {
        auto row = area.removeFromTop(kRowH);
        m_shepardEnable.setBounds(row.removeFromLeft(200));
    }

    // Semitones
    layoutRow(m_shepardSemitonesLabel, m_shepardSemitonesSlider, m_shepardSemitonesValueLabel);

    // Direction
    {
        auto row = area.removeFromTop(kRowH);
        m_shepardDirectionLabel.setBounds(row.removeFromLeft(kLabelW));
        m_shepardDirectionCombo.setBounds(row.removeFromLeft(140));
    }

    // Ramp Shape
    {
        auto row = area.removeFromTop(kRowH);
        m_shepardRampLabel.setBounds(row.removeFromLeft(kLabelW));
        m_shepardRampCombo.setBounds(row.removeFromLeft(140));
    }

    // Constraint label
    {
        auto row = area.removeFromTop(kRowH);
        row.removeFromLeft(kLabelW);
        m_shepardConstraintLabel.setBounds(row);
    }

    //--------------------------------------------------------------------------
    // Section 3: Output

    area.removeFromTop(kGap);
    area.removeFromTop(kBandH);
    area.removeFromTop(kGap);

    // Output Directory row
    {
        auto row = area.removeFromTop(kRowH);
        m_outputDirLabel.setBounds(row.removeFromLeft(kLabelW));
        row.removeFromRight(0);
        // Reserve space for Browse button at the right
        m_browseButton.setBounds(row.removeFromRight(80));
        row.removeFromRight(kGap);
        m_outputDirPathLabel.setBounds(row);
    }

    // Suffix row
    {
        auto row = area.removeFromTop(kRowH);
        m_suffixLabel.setBounds(row.removeFromLeft(kLabelW));
        m_suffixEditor.setBounds(row.removeFromLeft(160));
    }

    // Bit Depth row
    {
        auto row = area.removeFromTop(kRowH);
        m_bitDepthLabel.setBounds(row.removeFromLeft(kLabelW));
        m_bitDepthCombo.setBounds(row.removeFromLeft(100));
    }

    // File preview row
    {
        auto row = area.removeFromTop(kRowH);
        m_filePreviewHeaderLabel.setBounds(row.removeFromLeft(kLabelW));
        m_filePreviewLabel.setBounds(row);
    }

    //--------------------------------------------------------------------------
    // Section 4: Waveform preview (140px) + Diagnostics (60px)

    area.removeFromTop(kGap);
    m_waveformPreview->setBounds(area.removeFromTop(140));

    area.removeFromTop(4);
    m_diagnosticsLabel.setBounds(area.removeFromTop(60));

    //--------------------------------------------------------------------------
    // Button row — four centred buttons

    area.removeFromTop(kGap);
    auto buttonRow = area.removeFromTop(32);

    const int kBtnW   = 100;
    const int kBtnGap = 10;
    const int totalBW = kBtnW * 4 + kBtnGap * 3;
    const int startX  = buttonRow.getX() + (buttonRow.getWidth() - totalBW) / 2;

    m_previewPlaybackButton.setBounds(startX,                          buttonRow.getY(), kBtnW, 32);
    m_previewButton.setBounds        (startX + (kBtnW + kBtnGap),     buttonRow.getY(), kBtnW, 32);
    m_exportButton.setBounds         (startX + (kBtnW + kBtnGap) * 2, buttonRow.getY(), kBtnW, 32);
    m_cancelButton.setBounds         (startX + (kBtnW + kBtnGap) * 3, buttonRow.getY(), kBtnW, 32);
}

//==============================================================================
// Core logic
//==============================================================================

LoopRecipe LoopingToolsDialog::buildRecipe() const
{
    LoopRecipe recipe;

    // Loop count
    recipe.loopCount = (int)m_loopCountSlider.getValue();

    // Crossfade length and mode
    if (m_crossfadeModeCombo.getSelectedId() == 2)
    {
        recipe.crossfadeMode    = LoopRecipe::CrossfadeMode::Percentage;
        recipe.crossfadePercent = (float)m_crossfadeLenSlider.getValue();
    }
    else
    {
        recipe.crossfadeMode       = LoopRecipe::CrossfadeMode::Milliseconds;
        recipe.crossfadeLengthMs   = (float)m_crossfadeLenSlider.getValue();
    }

    recipe.maxCrossfadeMs = (float)m_maxCrossfadeSlider.getValue();

    // Crossfade curve: combo id 1=Linear, 2=EqualPower, 3=SCurve
    switch (m_crossfadeCurveCombo.getSelectedId())
    {
        case 1:  recipe.crossfadeCurve = LoopRecipe::CrossfadeCurve::Linear;      break;
        case 3:  recipe.crossfadeCurve = LoopRecipe::CrossfadeCurve::SCurve;       break;
        default: recipe.crossfadeCurve = LoopRecipe::CrossfadeCurve::EqualPower;  break;
    }

    // Zero-crossing
    recipe.zeroCrossingEnabled               = m_zeroCrossingToggle.getToggleState();
    recipe.zeroCrossingSearchWindowSamples   = (int)m_searchWindowSlider.getValue();

    // Variation settings
    recipe.offsetStepMs = (float)m_offsetStepSlider.getValue();
    recipe.shuffleSeed  = (int)m_shuffleSeedSlider.getValue();

    // Shepard Tone settings
    recipe.shepardEnabled = m_shepardEnable.getToggleState();
    recipe.shepardSemitones = static_cast<float>(m_shepardSemitonesSlider.getValue());
    recipe.shepardDirection = (m_shepardDirectionCombo.getSelectedId() == 2) ?
        LoopRecipe::ShepardDirection::Down : LoopRecipe::ShepardDirection::Up;
    recipe.shepardRampShape = (m_shepardRampCombo.getSelectedId() == 2) ?
        LoopRecipe::ShepardRampShape::Exponential : LoopRecipe::ShepardRampShape::Linear;

    return recipe;
}

void LoopingToolsDialog::updatePreview()
{
    auto recipe = buildRecipe();

    LoopResult result;
    if (recipe.shepardEnabled)
        result = LoopEngine::createShepardLoop(m_audioBuffer, m_sampleRate,
                                                m_selectionStart, m_selectionEnd, recipe);
    else
        result = LoopEngine::createLoop(m_audioBuffer, m_sampleRate,
                                         m_selectionStart, m_selectionEnd, recipe);

    if (result.success)
    {
        m_previewResult = result;
        m_waveformPreview->setBuffer(m_previewResult.loopBuffer);
        m_waveformPreview->setCrossfadeLength(result.crossfadeLengthSamples);

        // Build detailed diagnostics
        juce::String diag;

        // Loop length
        double loopSec = result.loopBuffer.getNumSamples() / m_sampleRate;
        diag += "Loop length: " + juce::String(loopSec, 3) + "s ("
              + juce::String(result.loopBuffer.getNumSamples()) + " samples)\n";

        // Crossfade details
        double xfadeMs = result.crossfadeLengthSamples * 1000.0 / m_sampleRate;
        juce::String curveNames[] = { "linear", "equal power", "S-curve" };
        int curveIdx = static_cast<int>(recipe.crossfadeCurve);
        juce::String curveName = (curveIdx >= 0 && curveIdx < 3) ? curveNames[curveIdx] : "unknown";
        diag += "Crossfade: " + juce::String(xfadeMs, 0) + "ms ("
              + juce::String(result.crossfadeLengthSamples) + " samples), " + curveName + "\n";

        // Zero-crossing
        if (recipe.zeroCrossingEnabled)
        {
            if (result.zeroCrossingFound)
            {
                int64_t startDelta = result.loopStartSample - m_selectionStart;
                int64_t endDelta = result.loopEndSample - m_selectionEnd;
                diag += "Zero-crossing: found (start: "
                      + juce::String((startDelta >= 0 ? "+" : "")) + juce::String(startDelta)
                      + " samples, end: "
                      + juce::String((endDelta >= 0 ? "+" : "")) + juce::String(endDelta)
                      + " samples)\n";
            }
            else
            {
                diag += "Zero-crossing: not found (using original boundaries)\n";
            }
        }

        // Discontinuity with color-coded quality rating
        diag += "Discontinuity: "
              + juce::String(result.discontinuityBefore, 4)
              + " \xe2\x86\x92 "
              + juce::String(result.discontinuityAfter, 4);

        if (result.discontinuityBefore > 0.0001f)
        {
            float reduction = (1.0f - result.discontinuityAfter / result.discontinuityBefore) * 100.0f;
            diag += " (" + juce::String(reduction, 1) + "% reduction)";
        }

        // Quality rating
        if (result.discontinuityAfter < 0.001f)
            diag += " - Excellent";
        else if (result.discontinuityAfter < 0.01f)
            diag += " - Good";
        else if (result.discontinuityAfter < 0.05f)
            diag += " - Acceptable";
        else
            diag += " - May have audible click";

        // Shepard info
        if (recipe.shepardEnabled)
        {
            juce::String dir = (recipe.shepardDirection == LoopRecipe::ShepardDirection::Up) ? "up" : "down";
            diag += "\nShepard: " + juce::String(recipe.shepardSemitones, 1)
                  + " semitone " + dir + ", constrained to "
                  + juce::String(loopSec, 3) + "s";
        }

        m_diagnosticsLabel.setText(diag, juce::dontSendNotification);

        // Color-code the diagnostics label based on discontinuity quality
        if (result.discontinuityAfter < 0.001f)
            m_diagnosticsLabel.setColour(juce::Label::textColourId, juce::Colour(0xff44cc44));  // green
        else if (result.discontinuityAfter < 0.01f)
            m_diagnosticsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcccc44));  // yellow
        else if (result.discontinuityAfter < 0.05f)
            m_diagnosticsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcc8844));  // orange
        else
            m_diagnosticsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcc4444));  // red
    }
    else
    {
        m_diagnosticsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcc4444));
        m_diagnosticsLabel.setText("Preview failed: " + result.errorMessage,
                                    juce::dontSendNotification);
    }
}

//==============================================================================
// Preview playback
//==============================================================================

void LoopingToolsDialog::togglePreviewPlayback()
{
    if (m_isPreviewPlaying)
    {
        // Stop playback
        stopTimer();
        m_isPreviewPlaying = false;
        m_previewPlaybackButton.setButtonText("\xe2\x96\xb6 Preview");
        m_previewPlaybackButton.setColour(juce::TextButton::buttonColourId,
            getLookAndFeel().findColour(juce::TextButton::buttonColourId));

        if (onPreviewStopped)
            onPreviewStopped();
    }
    else
    {
        // Build loop and start playback
        updatePreview();

        if (!m_previewResult.success || m_previewResult.loopBuffer.getNumSamples() == 0)
            return;

        m_isPreviewPlaying = true;
        m_previewPlaybackButton.setButtonText("\xe2\x96\xa0 Stop Preview");
        m_previewPlaybackButton.setColour(juce::TextButton::buttonColourId,
            juce::Colour(0xff882222));

        if (onPreviewRequested)
            onPreviewRequested(m_previewResult.loopBuffer, m_sampleRate);
    }
}

void LoopingToolsDialog::rebuildPreviewPlayback()
{
    if (!m_isPreviewPlaying)
        return;

    updatePreview();

    if (m_previewResult.success && m_previewResult.loopBuffer.getNumSamples() > 0)
    {
        if (onPreviewRequested)
            onPreviewRequested(m_previewResult.loopBuffer, m_sampleRate);
    }
}

void LoopingToolsDialog::schedulePreviewRebuild()
{
    if (m_isPreviewPlaying)
        startTimer(200);  // 200ms debounce
}

void LoopingToolsDialog::timerCallback()
{
    stopTimer();
    rebuildPreviewPlayback();
}

void LoopingToolsDialog::updateFilePreview()
{
    auto recipe = buildRecipe();
    juce::String baseName = m_sourceFile.getFileNameWithoutExtension();
    juce::String suffix   = m_suffixEditor.getText();

    juce::String preview;

    if (recipe.loopCount <= 1)
    {
        preview = baseName + suffix + ".wav";
    }
    else
    {
        int limit = std::min(recipe.loopCount, 3);
        for (int i = 0; i < limit; ++i)
        {
            juce::String numStr = juce::String(1 + i).paddedLeft('0', 3);
            if (i > 0) preview += ", ";
            preview += baseName + suffix + "_" + numStr + ".wav";
        }
        if (recipe.loopCount > 3)
            preview += ", ...";
    }

    m_filePreviewLabel.setText(preview, juce::dontSendNotification);
}

void LoopingToolsDialog::exportLoops()
{
    auto recipe = buildRecipe();

    if (!m_outputDirectory.isDirectory())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Export Error",
            "Please select a valid output directory.",
            "OK");
        return;
    }

    juce::String baseName = m_sourceFile.getFileNameWithoutExtension();
    juce::String suffix   = m_suffixEditor.getText();

    // Bit depth from combo selection index → actual value
    int bitDepth = 24;
    switch (m_bitDepthCombo.getSelectedId())
    {
        case 1: bitDepth = 16; break;
        case 2: bitDepth = 24; break;
        case 3: bitDepth = 32; break;
        default: break;
    }

    if (recipe.loopCount <= 1)
    {
        // Single loop export
        LoopResult result;
        if (recipe.shepardEnabled)
            result = LoopEngine::createShepardLoop(m_audioBuffer, m_sampleRate,
                                                    m_selectionStart, m_selectionEnd, recipe);
        else
            result = LoopEngine::createLoop(m_audioBuffer, m_sampleRate,
                                             m_selectionStart, m_selectionEnd, recipe);
        if (!result.success)
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Export Error",
                result.errorMessage,
                "OK");
            return;
        }

        juce::File outputFile = m_outputDirectory.getChildFile(baseName + suffix + ".wav");

        if (writeWavFile(outputFile, result.loopBuffer, m_sampleRate, bitDepth))
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Export Complete",
                "Exported loop to:\n" + outputFile.getFullPathName(),
                "OK");
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Export Error",
                "Failed to write file:\n" + outputFile.getFullPathName(),
                "OK");
            return;
        }
    }
    else
    {
        // Multiple variation export
        auto results = LoopEngine::createVariations(m_audioBuffer, m_sampleRate,
                                                     m_selectionStart, m_selectionEnd,
                                                     recipe);
        int exported = 0;

        for (size_t i = 0; i < results.size(); ++i)
        {
            if (!results[i].success)
                continue;

            juce::String numStr = juce::String((int)i + 1).paddedLeft('0', 3);
            juce::File outputFile = m_outputDirectory.getChildFile(
                baseName + suffix + "_" + numStr + ".wav");

            if (writeWavFile(outputFile, results[i].loopBuffer, m_sampleRate, bitDepth))
                exported++;
        }

        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Export Complete",
            "Exported " + juce::String(exported) + " loop(s) to:\n"
                + m_outputDirectory.getFullPathName(),
            "OK");
    }

    // Close the dialog after successful export
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(1);
}

bool LoopingToolsDialog::writeWavFile(const juce::File& file,
                                       const juce::AudioBuffer<float>& buffer,
                                       double sampleRate,
                                       int bitDepth)
{
    // Delete existing file to avoid append mode issues
    if (file.exists())
        file.deleteFile();

    // JUCE 8 API requires unique_ptr<OutputStream> (not FileOutputStream)
    std::unique_ptr<juce::OutputStream> outputStream(file.createOutputStream());
    if (!outputStream)
        return false;

    juce::WavAudioFormat wavFormat;

    // Use the modern JUCE 8 AudioFormatWriterOptions API to avoid deprecation warning
    auto options = juce::AudioFormatWriterOptions()
        .withSampleRate(sampleRate)
        .withNumChannels(buffer.getNumChannels())
        .withBitsPerSample(bitDepth)
        .withQualityOptionIndex(0);

    std::unique_ptr<juce::AudioFormatWriter> writer =
        wavFormat.createWriterFor(outputStream, options);

    if (!writer)
        return false;

    return writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
}

//==============================================================================
// Layout helpers
//==============================================================================

void LoopingToolsDialog::addSliderRow(juce::Label& label, const juce::String& text,
                                       juce::Slider& slider,
                                       double min, double max, double step, double defaultVal,
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
    };
    addAndMakeVisible(slider);

    juce::String initText = (step < 1.0)
        ? juce::String(defaultVal, 1) + suffix
        : juce::String(defaultVal, 0) + suffix;
    valueLabel.setText(initText, juce::dontSendNotification);
    valueLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(valueLabel);
}

void LoopingToolsDialog::updateValueLabel(juce::Slider& slider, juce::Label& label,
                                           const juce::String& suffix)
{
    double v = slider.getValue();
    juce::String text = (slider.getInterval() < 1.0)
        ? juce::String(v, 1) + suffix
        : juce::String(v, 0) + suffix;
    label.setText(text, juce::dontSendNotification);
}

void LoopingToolsDialog::updateVariationControlsEnabled()
{
    const bool multiLoop = (m_loopCountSlider.getValue() > 1);

    m_offsetStepLabel.setEnabled(multiLoop);
    m_offsetStepSlider.setEnabled(multiLoop);
    m_offsetStepValueLabel.setEnabled(multiLoop);
    m_shuffleSeedLabel.setEnabled(multiLoop);
    m_shuffleSeedSlider.setEnabled(multiLoop);
    m_shuffleSeedValueLabel.setEnabled(multiLoop);
}

void LoopingToolsDialog::updateZeroCrossingControlsEnabled()
{
    const bool enabled = m_zeroCrossingToggle.getToggleState();

    m_searchWindowLabel.setEnabled(enabled);
    m_searchWindowSlider.setEnabled(enabled);
    m_searchWindowValueLabel.setEnabled(enabled);
}

void LoopingToolsDialog::updateShepardControlsEnabled()
{
    const bool enabled = m_shepardEnable.getToggleState();

    m_shepardSemitonesLabel.setEnabled(enabled);
    m_shepardSemitonesSlider.setEnabled(enabled);
    m_shepardSemitonesValueLabel.setEnabled(enabled);
    m_shepardDirectionLabel.setEnabled(enabled);
    m_shepardDirectionCombo.setEnabled(enabled);
    m_shepardRampLabel.setEnabled(enabled);
    m_shepardRampCombo.setEnabled(enabled);
    m_shepardConstraintLabel.setEnabled(enabled);
}

void LoopingToolsDialog::updateShepardConstraint()
{
    if (m_shepardEnable.getToggleState())
    {
        float semitones = static_cast<float>(m_shepardSemitonesSlider.getValue());
        float pitchRatio = std::pow(2.0f, semitones / 12.0f);
        int64_t selLen = m_selectionEnd - m_selectionStart;
        int64_t constrainedLen = static_cast<int64_t>(selLen / pitchRatio);
        double constrainedSec = constrainedLen / m_sampleRate;
        m_shepardConstraintLabel.setText(
            "Loop constrained to " + juce::String(constrainedSec, 3) + "s for seamless pitch loop",
            juce::dontSendNotification);
    }
    else
    {
        m_shepardConstraintLabel.setText("", juce::dontSendNotification);
    }
}
