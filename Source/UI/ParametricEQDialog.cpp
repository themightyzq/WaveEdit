/*
  ==============================================================================

    ParametricEQDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "ParametricEQDialog.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioBufferManager.h"
#include "ThemeManager.h"
#include "UIConstants.h"
#include <cmath>

namespace ui = waveedit::ui;

// Static state persistence for dialog reopens (Phase 6 finalization)
// These persist bypass and loop toggle states across dialog instances
static bool s_lastBypassState = false;
static bool s_lastLoopState = true;  // Default ON per CLAUDE.md Protocol

//==============================================================================
// BandControl Implementation

ParametricEQDialog::BandControl::BandControl(const juce::String& bandName)
    : m_bandName(bandName),
      m_titleLabel("titleLabel", bandName),
      m_freqLabel("freqLabel", "Frequency:"),
      m_freqValueLabel("freqValueLabel", "1000 Hz"),
      m_gainLabel("gainLabel", "Gain:"),
      m_gainValueLabel("gainValueLabel", "0.0 dB"),
      m_qLabel("qLabel", "Q:"),
      m_qValueLabel("qValueLabel", "0.71")
{
    // Title
    m_titleLabel.setFont(ui::boldValueFont());
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Frequency controls
    m_freqLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_freqLabel);

    m_freqSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_freqSlider.setRange(20.0, 20000.0, 1.0);
    m_freqSlider.setSkewFactorFromMidPoint(1000.0);  // Logarithmic scale
    m_freqSlider.setValue(1000.0);
    // UX20: editable text box so a value can be typed (e.g. 3150 Hz), matching
    // the app-wide TextBoxRight slider convention.
    m_freqSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    m_freqSlider.setTextValueSuffix(" Hz");
    m_freqSlider.onValueChange = [this]() {
        updateValueLabels();
        if (auto* parent = findParentComponentOfClass<ParametricEQDialog>())
            parent->onParameterChanged();
    };
    addAndMakeVisible(m_freqSlider);

    m_freqValueLabel.setJustificationType(juce::Justification::centredLeft);
    m_freqValueLabel.setFont(ui::monospaceFont());
    // UX20: the editable slider text box now shows the value; keep the label as
    // an invisible child so updateValueLabels() stays valid without a second
    // (redundant) number in the row.
    addChildComponent(m_freqValueLabel);

    // Gain controls
    m_gainLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_gainLabel);

    m_gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_gainSlider.setRange(-20.0, 20.0, 0.1);
    m_gainSlider.setValue(0.0);
    // UX20: editable text box (type a dB value directly).
    m_gainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    m_gainSlider.setTextValueSuffix(" dB");
    m_gainSlider.onValueChange = [this]() {
        updateValueLabels();
        if (auto* parent = findParentComponentOfClass<ParametricEQDialog>())
            parent->onParameterChanged();
    };
    addAndMakeVisible(m_gainSlider);

    m_gainValueLabel.setJustificationType(juce::Justification::centredLeft);
    m_gainValueLabel.setFont(ui::monospaceFont());
    addChildComponent(m_gainValueLabel);  // UX20: invisible (slider text box shows value)

    // Q controls
    m_qLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_qLabel);

    m_qSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_qSlider.setRange(0.1, 10.0, 0.01);
    m_qSlider.setValue(0.707);
    m_qSlider.setSkewFactorFromMidPoint(0.707);  // Logarithmic scale centered on 0.707
    // UX20: editable text box (type a Q value directly).
    m_qSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 20);
    m_qSlider.onValueChange = [this]() {
        updateValueLabels();
        if (auto* parent = findParentComponentOfClass<ParametricEQDialog>())
            parent->onParameterChanged();
    };
    addAndMakeVisible(m_qSlider);

    m_qValueLabel.setJustificationType(juce::Justification::centredLeft);
    m_qValueLabel.setFont(ui::monospaceFont());
    addChildComponent(m_qValueLabel);  // UX20: invisible (slider text box shows value)

    // Initialize value labels
    updateValueLabels();

    setSize(400, 120);
}

void ParametricEQDialog::BandControl::paint(juce::Graphics& g)
{
    g.setColour(waveedit::ThemeManager::getInstance().getCurrent().border);
    g.drawRect(getLocalBounds(), 1);
}

void ParametricEQDialog::BandControl::resized()
{
    auto area = getLocalBounds().reduced(10);

    // Title
    m_titleLabel.setBounds(area.removeFromTop(20));
    area.removeFromTop(5);

    // Frequency row (slider carries its own editable text box on the right).
    auto freqRow = area.removeFromTop(24);
    m_freqLabel.setBounds(freqRow.removeFromLeft(70));
    freqRow.removeFromLeft(5);
    m_freqSlider.setBounds(freqRow);

    area.removeFromTop(3);

    // Gain row
    auto gainRow = area.removeFromTop(24);
    m_gainLabel.setBounds(gainRow.removeFromLeft(70));
    gainRow.removeFromLeft(5);
    m_gainSlider.setBounds(gainRow);

    area.removeFromTop(3);

    // Q row
    auto qRow = area.removeFromTop(24);
    m_qLabel.setBounds(qRow.removeFromLeft(70));
    qRow.removeFromLeft(5);
    m_qSlider.setBounds(qRow);
}

void ParametricEQDialog::BandControl::setParameters(const ParametricEQ::BandParameters& params)
{
    m_freqSlider.setValue(params.frequency, juce::dontSendNotification);
    m_gainSlider.setValue(params.gain, juce::dontSendNotification);
    m_qSlider.setValue(params.q, juce::dontSendNotification);
    updateValueLabels();
}

ParametricEQ::BandParameters ParametricEQDialog::BandControl::getParameters() const
{
    ParametricEQ::BandParameters params;
    params.frequency = static_cast<float>(m_freqSlider.getValue());
    params.gain = static_cast<float>(m_gainSlider.getValue());
    params.q = static_cast<float>(m_qSlider.getValue());
    return params;
}

void ParametricEQDialog::BandControl::resetToNeutral()
{
    m_gainSlider.setValue(0.0, juce::sendNotification);
}

void ParametricEQDialog::BandControl::updateValueLabels()
{
    // Frequency
    {
        float freq = static_cast<float>(m_freqSlider.getValue());
        if (freq >= 1000.0f)
        {
            m_freqValueLabel.setText(juce::String(freq / 1000.0f, 2) + " kHz", juce::dontSendNotification);
        }
        else
        {
            m_freqValueLabel.setText(juce::String(static_cast<int>(freq)) + " Hz", juce::dontSendNotification);
        }
    }

    // Gain
    {
        float gain = static_cast<float>(m_gainSlider.getValue());
        juce::String gainText = juce::String(gain, 1) + " dB";
        if (gain > 0.0f)
            gainText = "+" + gainText;
        m_gainValueLabel.setText(gainText, juce::dontSendNotification);
    }

    // Q
    {
        float q = static_cast<float>(m_qSlider.getValue());
        m_qValueLabel.setText(juce::String(q, 2), juce::dontSendNotification);
    }
}

//==============================================================================
// ParametricEQDialog Implementation

ParametricEQDialog::ParametricEQDialog(AudioEngine* audioEngine,
                                       AudioBufferManager* bufferManager,
                                       int64_t selectionStart,
                                       int64_t selectionEnd)
    : m_titleLabel("titleLabel", "3-Band Parametric EQ"),
      m_lowBand("Low Shelf"),
      m_midBand("Mid Peak"),
      m_highBand("High Shelf"),
      m_previewButton("Preview"),
      m_loopToggle("Loop"),
      m_resetButton("Reset"),
      m_applyButton("Apply"),
      m_cancelButton("Cancel"),
      m_audioEngine(audioEngine),
      m_bufferManager(bufferManager),
      m_selectionStart(selectionStart),
      m_selectionEnd(selectionEnd),
      m_isPreviewPlaying(false)
{
    // Title
    m_titleLabel.setFont(ui::dialogTitleFont());
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Band controls
    addAndMakeVisible(m_lowBand);
    addAndMakeVisible(m_midBand);
    addAndMakeVisible(m_highBand);

    // Preview controls - §6.8 footer: always present at the correct
    // position/width so the layout doesn't change shape depending on
    // whether an engine was supplied (REVIEW-DESIGN.md High Impact finding
    // 5, third bullet); disabled outright when there's no engine/buffer to
    // preview against instead of vanishing from the footer entirely.
    const bool hasPreviewEngine = (m_audioEngine != nullptr && m_bufferManager != nullptr);

    m_previewButton.onClick = [this]() { onPreviewClicked(); };
    m_previewButton.setEnabled(hasPreviewEngine);
    addAndMakeVisible(m_previewButton);

    // Loop toggle - restore persisted state
    m_loopToggle.setToggleState(s_lastLoopState, juce::dontSendNotification);  // Restore persisted state
    m_loopToggle.onStateChange = [this]() {
        s_lastLoopState = m_loopToggle.getToggleState();  // Save state on change
    };
    m_loopToggle.setEnabled(hasPreviewEngine);
    addAndMakeVisible(m_loopToggle);

    // Bypass button (starts disabled, enabled only during preview)
    m_bypassButton.setButtonText("Bypass");
    m_bypassButton.onClick = [this]() { onBypassClicked(); };
    m_bypassButton.setEnabled(false);  // Disabled until preview starts (or permanently, with no engine)
    addAndMakeVisible(m_bypassButton);

    if (hasPreviewEngine)
    {
        // Create EQ processor for preview
        m_parametricEQ = std::make_unique<ParametricEQ>();
    }
    else
    {
        m_previewButton.setTooltip("Not available - no audio loaded to preview against");
        m_bypassButton.setTooltip("Not available - no audio loaded to preview against");
        m_loopToggle.setTooltip("Not available - no audio loaded to preview against");
    }

    // Buttons
    m_resetButton.onClick = [this]() { onResetClicked(); };
    addAndMakeVisible(m_resetButton);

    m_applyButton.onClick = [this]() { onApplyClicked(); };
    addAndMakeVisible(m_applyButton);

    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    // Dialog size: 3 bands (120px each) + spacing + title + buttons.
    // Widened from 450 to fit the full §6.8 footer (Preview+Bypass+Loop+Reset
    // on the left, Cancel+Apply on the right = 610px of buttons/gaps at
    // kDialogPadding=15 each side) now that group is always laid out instead
    // of only when an engine is present -- 450 could only ever fit the
    // 3-button no-engine footer (needs visual QA to confirm final proportions).
    setSize(660, 490);

    // Keyboard-first: grab focus on the primary control after construction
    setWantsKeyboardFocus(true);
    juce::Component::SafePointer<ParametricEQDialog> safeThis(this);
    juce::MessageManager::callAsync([safeThis]()
    {
        if (safeThis != nullptr)
            safeThis->grabKeyboardFocus();
    });
}

bool ParametricEQDialog::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::returnKey)
    {
        onApplyClicked();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        onCancelClicked();
        return true;
    }
    return false;
}

ParametricEQDialog::~ParametricEQDialog()
{
    // Stop preview when dialog is destroyed and reset bypass
    if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
        m_audioEngine->stopSelectionPreview();
}

std::optional<ParametricEQ::Parameters> ParametricEQDialog::showDialog(
    AudioEngine* audioEngine,
    AudioBufferManager* bufferManager,
    int64_t selectionStart,
    int64_t selectionEnd,
    const ParametricEQ::Parameters& currentParams)
{
    ParametricEQDialog dialog(audioEngine, bufferManager, selectionStart, selectionEnd);

    // Initialize with current parameters
    dialog.m_lowBand.setParameters(currentParams.low);
    dialog.m_midBand.setParameters(currentParams.mid);
    dialog.m_highBand.setParameters(currentParams.high);

    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(&dialog);
    options.dialogTitle = "Parametric EQ";
    options.dialogBackgroundColour = waveedit::ThemeManager::getInstance().getCurrent().panel;
    options.escapeKeyTriggersCloseButton = false;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = nullptr;

    #if JUCE_MODAL_LOOPS_PERMITTED
        int result = options.runModal();
        return (result == 1) ? dialog.m_result : std::nullopt;
    #else
        jassertfalse;  // Modal loops required
        return std::nullopt;
    #endif
}

void ParametricEQDialog::paint(juce::Graphics& g)
{
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();
    g.fillAll(theme.panel);

    g.setColour(theme.border);
    g.drawRect(getLocalBounds(), 1);
}

void ParametricEQDialog::resized()
{
    auto area = getLocalBounds().reduced(ui::kDialogPadding);

    // Title
    m_titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10);

    // Low band
    m_lowBand.setBounds(area.removeFromTop(120));
    area.removeFromTop(5);

    // Mid band
    m_midBand.setBounds(area.removeFromTop(120));
    area.removeFromTop(5);

    // High band
    m_highBand.setBounds(area.removeFromTop(120));
    area.removeFromTop(15);

    // Button row - standardized §6.8 layout.
    // Left group: Preview + Bypass + Loop + Reset | Right group: Cancel + Apply.
    // Reset lives in the left group (not centered) so the right side always stays
    // Cancel/Apply per protocol. Preview/Bypass/Loop are always laid out (just
    // disabled when there's no audio engine) so the footer shape never changes.
    auto buttonRow = area.removeFromTop(ui::kButtonHeight);
    const int buttonWidth   = ui::kButtonWidth;
    const int buttonSpacing = ui::kButtonGap;

    // Left side: Preview, Bypass, Loop, then Reset
    m_previewButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(buttonSpacing);
    m_bypassButton.setBounds(buttonRow.removeFromLeft(ui::kButtonWidthNarrow));
    buttonRow.removeFromLeft(buttonSpacing);
    m_loopToggle.setBounds(buttonRow.removeFromLeft(60));
    buttonRow.removeFromLeft(buttonSpacing);

    m_resetButton.setBounds(buttonRow.removeFromLeft(buttonWidth));

    // Right side: Apply (rightmost) then Cancel
    m_applyButton.setBounds(buttonRow.removeFromRight(buttonWidth));
    buttonRow.removeFromRight(buttonSpacing);
    m_cancelButton.setBounds(buttonRow.removeFromRight(buttonWidth));
}

void ParametricEQDialog::visibilityChanged()
{
    if (!isVisible())
    {
        // Stop preview when dialog is hidden
        if (m_audioEngine && m_audioEngine->getPreviewMode() != PreviewMode::DISABLED)
            m_audioEngine->stopSelectionPreview();
    }
}

void ParametricEQDialog::onApplyClicked()
{
    m_result = getCurrentParameters();

    // Disable EQ preview when applying
    if (m_audioEngine)
    {
        m_audioEngine->setParametricEQPreview(ParametricEQ::Parameters::createNeutral(), false);
    }

    if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
    {
        parent->exitModalState(1);
    }
}

void ParametricEQDialog::onCancelClicked()
{
    m_result = std::nullopt;

    // Disable EQ preview when cancelling
    if (m_audioEngine)
    {
        m_audioEngine->setParametricEQPreview(ParametricEQ::Parameters::createNeutral(), false);
    }

    if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
    {
        parent->exitModalState(0);
    }
}

void ParametricEQDialog::onResetClicked()
{
    m_lowBand.resetToNeutral();
    m_midBand.resetToNeutral();
    m_highBand.resetToNeutral();
}

void ParametricEQDialog::onPreviewClicked()
{
    if (!m_audioEngine || !m_bufferManager || !m_parametricEQ)
        return;

    // Toggle behavior: if preview is playing, stop it
    if (m_isPreviewPlaying && m_audioEngine->isPlaying())
    {
        m_audioEngine->stopSelectionPreview();
        m_isPreviewPlaying = false;
        m_previewButton.setButtonText("Preview");
        m_previewButton.setColour(juce::TextButton::buttonColourId, getLookAndFeel().findColour(juce::TextButton::buttonColourId));
        // Disable and reset bypass button
        m_bypassButton.setEnabled(false);
        m_bypassButton.setButtonText("Bypass");
        m_bypassButton.setColour(juce::TextButton::buttonColourId, getLookAndFeel().findColour(juce::TextButton::buttonColourId));
        return;
    }

    // Configure the effect, then hand selection/loop/transport to the shared helper.
    m_audioEngine->setPreviewMode(PreviewMode::REALTIME_DSP);
    m_audioEngine->setParametricEQPreview(getCurrentParameters(), true);
    m_audioEngine->startSelectionPreview(m_selectionStart, m_selectionEnd,
                                         m_loopToggle.getToggleState());

    // 8. Update button state for toggle
    m_isPreviewPlaying = true;
    m_previewButton.setButtonText("Stop Preview");
    m_previewButton.setColour(juce::TextButton::buttonColourId, ui::colour(ui::kButtonPreviewActive));

    // 9. Enable bypass button during preview
    m_bypassButton.setEnabled(true);
}

void ParametricEQDialog::onParameterChanged()
{
    // Only update preview if it's currently playing
    if (!m_isPreviewPlaying || !m_audioEngine)
        return;

    // Update EQ parameters atomically - no buffer reload needed!
    ParametricEQ::Parameters params = getCurrentParameters();
    m_audioEngine->setParametricEQPreview(params, true);

    // That's it! The audio thread will pick up the new parameters
    // on the next audio block. No artifacts, instant response.
}

ParametricEQ::Parameters ParametricEQDialog::getCurrentParameters() const
{
    ParametricEQ::Parameters params;
    params.low = m_lowBand.getParameters();
    params.mid = m_midBand.getParameters();
    params.high = m_highBand.getParameters();
    return params;
}

void ParametricEQDialog::onBypassClicked()
{
    if (!m_audioEngine)
        return;

    bool bypassed = m_audioEngine->isPreviewBypassed();
    bool newBypassState = !bypassed;
    m_audioEngine->setPreviewBypassed(newBypassState);

    // Save bypass state for persistence across dialog reopens
    s_lastBypassState = newBypassState;

    // Update button appearance
    // H5 FIX: bypass-active colour follows the active theme's warning token
    // (was a fixed 0xffff8c00 orange). Read at click-time, matching
    // PluginChainPanel.cpp's updateBypassButtonAppearance() pattern.
    if (!bypassed)
    {
        const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();
        const auto bypassTextColour = theme.warning.getPerceivedBrightness() > 0.5f
                                           ? juce::Colours::black
                                           : juce::Colours::white;
        m_bypassButton.setButtonText("Bypassed");
        m_bypassButton.setColour(juce::TextButton::buttonColourId, theme.warning);
        m_bypassButton.setColour(juce::TextButton::textColourOffId, bypassTextColour);
    }
    else
    {
        m_bypassButton.setButtonText("Bypass");
        m_bypassButton.setColour(juce::TextButton::buttonColourId,
                                 getLookAndFeel().findColour(juce::TextButton::buttonColourId));
        m_bypassButton.setColour(juce::TextButton::textColourOffId,
                                 getLookAndFeel().findColour(juce::TextButton::textColourOffId));
    }
}
