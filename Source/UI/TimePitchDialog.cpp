/*
  ==============================================================================

    TimePitchDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "TimePitchDialog.h"
#include "../DSP/TimePitchEngine.h"
#include "../Audio/AudioEngine.h"
#include "ThemeManager.h"
#include "UIConstants.h"

#include <cmath>

namespace ui = waveedit::ui;

//==============================================================================
// Construction
//==============================================================================

TimePitchDialog::TimePitchDialog(Mode mode,
                                 const juce::AudioBuffer<float>& audioBuffer,
                                 double sampleRate,
                                 bool hasSelection,
                                 double selectionStartSeconds,
                                 double cursorSeconds,
                                 AudioEngine* audioEngine,
                                 juce::Component* documentLifeline)
    : m_mode(mode)
    , m_sampleRate(sampleRate)
    , m_audioEngine(audioEngine)
    , m_documentLifeline(documentLifeline)
{
    const bool isStretch = (m_mode == Mode::TimeStretch);

    if (sampleRate > 0.0)
        m_fullDurationSeconds = (double) audioBuffer.getNumSamples() / sampleRate;

    // Own a copy of the preview excerpt (not the live document buffer). Rendering
    // the whole file through SoundTouch is slow, so we only render this excerpt.
    const auto range = computePreviewExcerpt(audioBuffer.getNumSamples(), sampleRate,
                                             hasSelection, selectionStartSeconds,
                                             cursorSeconds);
    const int excerptLen = (int) juce::jmax((juce::int64) 0, range.getLength());
    if (excerptLen > 0 && audioBuffer.getNumChannels() > 0)
    {
        m_originalExcerpt.setSize(audioBuffer.getNumChannels(), excerptLen);
        for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
            m_originalExcerpt.copyFrom(ch, 0, audioBuffer, ch,
                                       (int) range.getStart(), excerptLen);
    }

    //--------------------------------------------------------------------------
    // Help line -- wording mirrors the previous AlertWindow prompts.

    m_helpLabel.setText(isStretch
        ? "Stretch or compress the file's tempo without changing pitch. "
          "Range: -50% (half speed) to +500% (6x speed)."
        : "Shift the file's pitch without changing length. Use whole semitones "
          "(e.g. 12 = 1 octave up) or fractions for cents. Range: -24 to +24.",
        juce::dontSendNotification);
    m_helpLabel.setJustificationType(juce::Justification::topLeft);
    m_helpLabel.setColour(juce::Label::textColourId,
                          waveedit::ThemeManager::getInstance().getCurrent().textMuted);
    m_helpLabel.setFont(ui::smallFont());
    addAndMakeVisible(m_helpLabel);

    // The help label caches a theme token, so re-apply it on theme switches
    // (Sec 6.11) for the case where the theme changes while the dialog is open.
    waveedit::ThemeManager::getInstance().addChangeListener(this);

    //--------------------------------------------------------------------------
    // Parameter row.

    m_paramLabel.setText(isStretch ? "Tempo change (%):" : "Semitones:",
                         juce::dontSendNotification);
    m_paramLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(m_paramLabel);

    m_paramSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    if (isStretch)
    {
        m_paramSlider.setRange(-50.0, 500.0, 0.1);
        m_paramSlider.setTextValueSuffix(" %");
    }
    else
    {
        m_paramSlider.setRange(-24.0, 24.0, 0.01);
        m_paramSlider.setTextValueSuffix(" st");
    }
    m_paramSlider.setValue(0.0, juce::dontSendNotification);
    m_paramSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false,
                                  ui::kSliderTextWidth + 20, ui::kInputHeight);
    m_paramSlider.onValueChange = [this]() { updateSummary(); };
    addAndMakeVisible(m_paramSlider);

    //--------------------------------------------------------------------------
    // Summary line.

    m_summaryLabel.setJustificationType(juce::Justification::centredLeft);
    m_summaryLabel.setFont(ui::smallFont());
    addAndMakeVisible(m_summaryLabel);

    //--------------------------------------------------------------------------
    // Footer buttons (Sec 6.8).

    m_previewButton.setButtonText("Preview");
    m_previewButton.onClick = [this]()
    {
        if (m_previewActive)
            stopPreview();
        else
            startPreview();
    };
    addAndMakeVisible(m_previewButton);

    m_bypassButton.setButtonText("Bypass");
    m_bypassButton.setEnabled(false);   // Enabled only while previewing.
    m_bypassButton.onClick = [this]()
    {
        if (! m_previewActive || ! engineUsable())
            return;
        m_bypassActive = ! m_bypassActive;
        m_bypassButton.setButtonText(m_bypassActive ? "Bypassed" : "Bypass");
        if (m_bypassActive)
            m_bypassButton.setColour(juce::TextButton::buttonColourId,
                                     ui::colour(ui::kButtonBypassActive));
        else
            m_bypassButton.removeColour(juce::TextButton::buttonColourId);

        reloadActiveBuffer();   // startBufferPreview restarts playback itself
    };
    addAndMakeVisible(m_bypassButton);

    m_loopToggle.setButtonText("Loop");
    m_loopToggle.setToggleState(true, juce::dontSendNotification);  // Default ON.
    m_loopToggle.onClick = [this]()
    {
        if (m_previewActive && engineUsable())
            reloadActiveBuffer();
    };
    addAndMakeVisible(m_loopToggle);

    m_applyButton.setButtonText("Apply");
    m_applyButton.onClick = [this]()
    {
        stopPreview();   // tear down before applying so the doc isn't left previewing
        if (onApply)
            onApply(m_paramSlider.getValue());

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(1);
    };
    addAndMakeVisible(m_applyButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]()
    {
        stopPreview();
        if (onCancel)
            onCancel();

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    };
    addAndMakeVisible(m_cancelButton);

    //--------------------------------------------------------------------------
    // Initial state.

    updateSummary();
    setSize(490, 220);

    setWantsKeyboardFocus(true);
    juce::Component::SafePointer<juce::Slider> safeSlider(&m_paramSlider);
    juce::MessageManager::callAsync([safeSlider]()
    {
        if (safeSlider != nullptr)
            safeSlider->grabKeyboardFocus();
    });
}

TimePitchDialog::~TimePitchDialog()
{
    waveedit::ThemeManager::getInstance().removeChangeListener(this);

    // Safety net for every close path: never leave the engine in preview mode.
    stopTimer();
    stopPreview();
}

void TimePitchDialog::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &waveedit::ThemeManager::getInstance())
    {
        m_helpLabel.setColour(juce::Label::textColourId,
                              waveedit::ThemeManager::getInstance().getCurrent().textMuted);
        repaint();
    }
}

//==============================================================================
// Pure helpers
//==============================================================================

juce::Range<juce::int64> TimePitchDialog::computePreviewExcerpt(
    juce::int64 totalSamples, double sampleRate,
    bool hasSelection, double selectionStartSeconds,
    double cursorSeconds) noexcept
{
    if (totalSamples <= 0 || sampleRate <= 0.0)
        return { 0, 0 };

    const double startSeconds = hasSelection ? selectionStartSeconds : cursorSeconds;
    juce::int64 start = (juce::int64) std::llround(startSeconds * sampleRate);
    start = juce::jlimit((juce::int64) 0, totalSamples, start);

    const juce::int64 maxLen =
        (juce::int64) std::llround(kPreviewSeconds * sampleRate);
    const juce::int64 end = juce::jmin(totalSamples, start + maxLen);

    return { start, end };
}

bool TimePitchDialog::previewIsPlayable(int numSamples, int numChannels) noexcept
{
    return numSamples > 0 && numChannels > 0;
}

//==============================================================================
// Audio preview (Sec 6.8)
//==============================================================================

bool TimePitchDialog::engineUsable() const noexcept
{
    return m_audioEngine != nullptr && m_documentLifeline.getComponent() != nullptr;
}

void TimePitchDialog::renderProcessed()
{
    try
    {
        TimePitchEngine::Recipe recipe;
        if (m_mode == Mode::TimeStretch)
            recipe.tempoPercent = m_paramSlider.getValue();
        else
            recipe.pitchSemitones = m_paramSlider.getValue();

        if (TimePitchEngine::isIdentity(recipe))
            m_processedBuffer.makeCopyOf(m_originalExcerpt);   // preview original
        else
            m_processedBuffer =
                TimePitchEngine::apply(m_originalExcerpt, m_sampleRate, recipe);
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("TimePitchDialog::renderProcessed - "
                                 + juce::String(e.what()));
        m_summaryLabel.setText("Preview failed: " + juce::String(e.what()),
                               juce::dontSendNotification);
        m_processedBuffer.setSize(0, 0);
    }

    m_processedPlayable = previewIsPlayable(m_processedBuffer.getNumSamples(),
                                            m_processedBuffer.getNumChannels());
}

bool TimePitchDialog::reloadActiveBuffer()
{
    if (! engineUsable() || ! m_previewActive)
        return false;

    const auto& buf = m_bypassActive ? m_originalExcerpt : m_processedBuffer;
    return m_audioEngine->startBufferPreview(buf, m_sampleRate, buf.getNumChannels(),
                                             m_loopToggle.getToggleState());
}

void TimePitchDialog::startPreview()
{
    if (! engineUsable())
        return;

    if (! previewIsPlayable(m_originalExcerpt.getNumSamples(),
                            m_originalExcerpt.getNumChannels()))
    {
        m_summaryLabel.setText("Nothing to preview at the cursor / selection.",
                               juce::dontSendNotification);
        return;
    }

    renderProcessed();
    if (! m_processedPlayable)
    {
        m_summaryLabel.setText("Result is empty - nothing to preview.",
                               juce::dontSendNotification);
        return;
    }

    m_previewActive = true;
    m_bypassActive  = false;
    if (! reloadActiveBuffer())
    {
        stopPreview();
        return;
    }

    m_previewButton.setButtonText("STOP");
    m_previewButton.setColour(juce::TextButton::buttonColourId,
                              ui::colour(ui::kButtonPreviewActive));
    m_bypassButton.setEnabled(true);
}

void TimePitchDialog::stopPreview()
{
    if (engineUsable())
        m_audioEngine->stopSelectionPreview();
    stopTimer();

    m_previewActive = false;
    m_bypassActive  = false;
    m_previewButton.setButtonText("Preview");
    m_previewButton.removeColour(juce::TextButton::buttonColourId);
    m_bypassButton.setButtonText("Bypass");
    m_bypassButton.removeColour(juce::TextButton::buttonColourId);
    m_bypassButton.setEnabled(false);
}

void TimePitchDialog::timerCallback()
{
    stopTimer();
    if (! m_previewActive)
        return;

    if (! engineUsable())   // document closed while a re-render was pending
    {
        stopPreview();
        return;
    }

    renderProcessed();
    if (! m_processedPlayable)
    {
        m_summaryLabel.setText("Result is empty - nothing to preview.",
                               juce::dontSendNotification);
        stopPreview();
        return;
    }

    reloadActiveBuffer();   // startBufferPreview restarts playback itself
}

//==============================================================================
// UI helpers
//==============================================================================

void TimePitchDialog::updateSummary()
{
    if (m_mode == Mode::TimeStretch)
    {
        const double tempoPercent = m_paramSlider.getValue();
        // Slower tempo -> longer output: scale by 100 / (100 + tempoPercent).
        // tempoPercent is clamped to [-50, +500], so the denominator stays >= 50.
        const double denom = 100.0 + tempoPercent;
        if (m_fullDurationSeconds > 0.0 && denom > 0.0)
        {
            const double projected = m_fullDurationSeconds * (100.0 / denom);
            m_summaryLabel.setText(
                "Projected duration: " + juce::String(projected, 3) + " s"
                + "  (was " + juce::String(m_fullDurationSeconds, 3) + " s)",
                juce::dontSendNotification);
        }
        else
        {
            m_summaryLabel.setText(juce::String(), juce::dontSendNotification);
        }
    }
    else
    {
        const double semis = m_paramSlider.getValue();
        m_summaryLabel.setText(
            "Length is preserved. Shift: " + juce::String(semis, 2)
            + " semitones (" + juce::String(semis * 100.0, 0) + " cents).",
            juce::dontSendNotification);
    }

    // Debounced preview re-render: a single hook covers the slider.
    if (m_previewActive)
        startTimer(kPreviewDebounceMs);
}

//==============================================================================
// Component overrides
//==============================================================================

bool TimePitchDialog::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey)
    {
        m_applyButton.triggerClick();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        m_cancelButton.triggerClick();
        return true;
    }
    return false;
}

void TimePitchDialog::paint(juce::Graphics& g)
{
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();
    g.fillAll(theme.panel);

    g.setColour(theme.text);
    g.setFont(ui::sectionHeaderFont());
    g.drawText(m_mode == Mode::TimeStretch ? "Time Stretch" : "Pitch Shift",
               getLocalBounds().removeFromTop(36),
               juce::Justification::centred, true);
}

void TimePitchDialog::resized()
{
    auto area = getLocalBounds().reduced(ui::kDialogPadding);
    area.removeFromTop(36);   // Title space (painted in paint()).

    m_helpLabel.setBounds(area.removeFromTop(40));
    area.removeFromTop(ui::kSectionGap);

    {
        auto row = area.removeFromTop(ui::kInputHeight + 6);
        m_paramLabel.setBounds(row.removeFromLeft(120));
        m_paramSlider.setBounds(row);
    }

    area.removeFromTop(ui::kRowGap);
    m_summaryLabel.setBounds(area.removeFromTop(20));

    //--------------------------------------------------------------------------
    // Footer -- Sec 6.8: Left Preview | Bypass | Loop   Right Cancel | Apply.

    area.removeFromTop(area.getHeight() - 40);   // Push to bottom.
    auto buttonRow = area.removeFromTop(40);

    const int kBtnW = ui::kButtonWidth;
    const int kGapBtn = ui::kButtonGap;

    m_previewButton.setBounds(buttonRow.removeFromLeft(kBtnW));
    buttonRow.removeFromLeft(kGapBtn);
    m_bypassButton.setBounds(buttonRow.removeFromLeft(ui::kButtonWidthNarrow));
    buttonRow.removeFromLeft(kGapBtn);
    m_loopToggle.setBounds(buttonRow.removeFromLeft(60));

    m_applyButton.setBounds(buttonRow.removeFromRight(kBtnW));
    buttonRow.removeFromRight(kGapBtn);
    m_cancelButton.setBounds(buttonRow.removeFromRight(kBtnW));
}
