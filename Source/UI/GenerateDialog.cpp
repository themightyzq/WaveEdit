/*
  ==============================================================================

    GenerateDialog.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2026 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "GenerateDialog.h"
#include "ThemeManager.h"
#include "UIConstants.h"
#include "WaveformDisplay.h"
#include "../Audio/AudioEngine.h"

namespace
{
    constexpr int DIALOG_WIDTH = 440;
    constexpr int PADDING = waveedit::ui::kDialogPadding;
    constexpr int ROW = 28;
    constexpr int SPACING = 10;

    inline waveedit::Theme theme()
        { return waveedit::ThemeManager::getInstance().getCurrent(); }

    void styleSlider(juce::Slider& s)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, ROW);
        s.setColour(juce::Slider::textBoxBackgroundColourId, theme().panelAlternate);
        s.setColour(juce::Slider::textBoxTextColourId, theme().text);
        s.setColour(juce::Slider::textBoxOutlineColourId, theme().accent);
        s.setColour(juce::Slider::trackColourId, theme().accent);
        s.setColour(juce::Slider::thumbColourId, theme().accent);
    }
}

//==============================================================================
GenerateDialog::GenerateDialog(Mode mode,
                               AudioEngine* engine,
                               juce::Component::SafePointer<WaveformDisplay> lifeline,
                               bool hasSelection,
                               double selectionSeconds,
                               double sampleRate,
                               int numChannels)
    : m_mode(mode),
      m_engine(engine),
      m_lifeline(lifeline),
      m_hasSelection(hasSelection),
      m_selectionSeconds(selectionSeconds),
      m_sampleRate(sampleRate > 0.0 ? sampleRate : 44100.0),
      m_numChannels(juce::jmax(1, numChannels))
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    const bool isTone = (m_mode == Mode::Tone);
    setSize(DIALOG_WIDTH, isTone ? 330 : 300);

    m_titleLabel.setText(isTone ? "Generate Tone" : "Generate Noise", juce::dontSendNotification);
    m_titleLabel.setFont(waveedit::ui::dialogTitleFont());
    m_titleLabel.setColour(juce::Label::textColourId, theme().text);
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Type combo (waveform for Tone, noise colour for Noise).
    m_typeLabel.setText(isTone ? "Waveform:" : "Type:", juce::dontSendNotification);
    m_typeLabel.setFont(waveedit::ui::bodyFont());
    m_typeLabel.setColour(juce::Label::textColourId, theme().text);
    addAndMakeVisible(m_typeLabel);

    if (isTone)
    {
        m_typeCombo.addItem("Sine", 1);
        m_typeCombo.addItem("Square", 2);
        m_typeCombo.addItem("Saw", 3);
        m_typeCombo.addItem("Triangle", 4);
    }
    else
    {
        m_typeCombo.addItem("White", 1);
        m_typeCombo.addItem("Pink", 2);
    }
    m_typeCombo.setSelectedId(1, juce::dontSendNotification);
    m_typeCombo.setColour(juce::ComboBox::backgroundColourId, theme().panelAlternate);
    m_typeCombo.setColour(juce::ComboBox::textColourId, theme().text);
    m_typeCombo.setColour(juce::ComboBox::outlineColourId, theme().accent);
    m_typeCombo.onChange = [this] { restartPreviewIfActive(); };
    addAndMakeVisible(m_typeCombo);

    // Frequency (Tone only).
    m_freqLabel.setText("Frequency (Hz):", juce::dontSendNotification);
    m_freqLabel.setFont(waveedit::ui::bodyFont());
    m_freqLabel.setColour(juce::Label::textColourId, theme().text);
    m_freqSlider.setRange(20.0, 20000.0, 1.0);
    m_freqSlider.setSkewFactorFromMidPoint(1000.0);   // log-ish feel
    m_freqSlider.setValue(1000.0, juce::dontSendNotification);
    styleSlider(m_freqSlider);
    m_freqSlider.onValueChange = [this] { restartPreviewIfActive(); };
    m_freqLabel.setVisible(isTone);
    m_freqSlider.setVisible(isTone);
    if (isTone)
    {
        addAndMakeVisible(m_freqLabel);
        addAndMakeVisible(m_freqSlider);
    }

    // Amplitude in dBFS.
    m_ampLabel.setText("Amplitude (dBFS):", juce::dontSendNotification);
    m_ampLabel.setFont(waveedit::ui::bodyFont());
    m_ampLabel.setColour(juce::Label::textColourId, theme().text);
    m_ampSlider.setRange(-60.0, 0.0, 0.1);
    m_ampSlider.setValue(isTone ? -12.0 : -18.0, juce::dontSendNotification);
    styleSlider(m_ampSlider);
    m_ampSlider.onValueChange = [this] { restartPreviewIfActive(); };
    addAndMakeVisible(m_ampLabel);
    addAndMakeVisible(m_ampSlider);

    // Duration.
    m_durationLabel.setText("Duration (seconds):", juce::dontSendNotification);
    m_durationLabel.setFont(waveedit::ui::bodyFont());
    m_durationLabel.setColour(juce::Label::textColourId, theme().text);
    addAndMakeVisible(m_durationLabel);

    m_durationEditor.setMultiLine(false);
    m_durationEditor.setFont(waveedit::ui::bodyFont());
    m_durationEditor.setColour(juce::TextEditor::backgroundColourId, theme().panelAlternate);
    m_durationEditor.setColour(juce::TextEditor::textColourId, theme().text);
    m_durationEditor.setColour(juce::TextEditor::outlineColourId, theme().accent);
    m_durationEditor.setText("1.000", juce::dontSendNotification);
    m_durationEditor.setEnabled(!m_hasSelection);
    m_durationEditor.onTextChange = [this] { updateValidity(); };
    addAndMakeVisible(m_durationEditor);

    m_validationLabel.setFont(waveedit::ui::smallFont());
    m_validationLabel.setColour(juce::Label::textColourId, theme().error);
    m_validationLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_validationLabel);

    m_noteLabel.setFont(waveedit::ui::smallFont());
    m_noteLabel.setColour(juce::Label::textColourId, theme().textMuted);
    m_noteLabel.setText(m_hasSelection
                            ? "A selection is active - it will be overwritten (using selection length)."
                            : "Inserted at the edit cursor.",
                        juce::dontSendNotification);
    addAndMakeVisible(m_noteLabel);

    // Footer: Preview (left) | Cancel + Generate (right).
    m_previewButton.setButtonText("Preview");
    m_previewButton.onClick = [this]
    {
        if (m_previewActive) stopPreview();
        else                 startPreview();
    };
    m_previewButton.setEnabled(engineUsable());
    addAndMakeVisible(m_previewButton);

    m_generateButton.setButtonText("Generate");
    m_generateButton.onClick = [this] { confirmDialog(); };
    addAndMakeVisible(m_generateButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]
    {
        stopPreview();
        if (auto* modal = juce::ModalComponentManager::getInstance()->getModalComponent(0))
            modal->exitModalState(0);
    };
    addAndMakeVisible(m_cancelButton);

    updateValidity();
}

GenerateDialog::~GenerateDialog()
{
    // Safety net: never leave the engine auditioning after the dialog closes.
    stopPreview();
}

//==============================================================================
void GenerateDialog::paint(juce::Graphics& g)
{
    g.fillAll(theme().panel);
    g.setColour(theme().accent);
    g.drawRect(getLocalBounds(), 2);
}

void GenerateDialog::resized()
{
    auto bounds = getLocalBounds().reduced(PADDING);
    const bool isTone = (m_mode == Mode::Tone);

    m_titleLabel.setBounds(bounds.removeFromTop(ROW));
    bounds.removeFromTop(SPACING);

    auto typeRow = bounds.removeFromTop(ROW);
    m_typeLabel.setBounds(typeRow.removeFromLeft(130));
    m_typeCombo.setBounds(typeRow);
    bounds.removeFromTop(SPACING / 2);

    if (isTone)
    {
        auto freqRow = bounds.removeFromTop(ROW);
        m_freqLabel.setBounds(freqRow.removeFromLeft(130));
        m_freqSlider.setBounds(freqRow);
        bounds.removeFromTop(SPACING / 2);
    }

    auto ampRow = bounds.removeFromTop(ROW);
    m_ampLabel.setBounds(ampRow.removeFromLeft(130));
    m_ampSlider.setBounds(ampRow);
    bounds.removeFromTop(SPACING / 2);

    auto durRow = bounds.removeFromTop(ROW);
    m_durationLabel.setBounds(durRow.removeFromLeft(130));
    m_durationEditor.setBounds(durRow);
    bounds.removeFromTop(SPACING / 2);

    m_noteLabel.setBounds(bounds.removeFromTop(ROW));
    m_validationLabel.setBounds(bounds.removeFromTop(ROW));

    auto buttonRow = getLocalBounds().reduced(PADDING).removeFromBottom(waveedit::ui::kButtonHeight);
    m_previewButton.setBounds(buttonRow.removeFromLeft(waveedit::ui::kButtonWidth));
    m_generateButton.setBounds(buttonRow.removeFromRight(waveedit::ui::kButtonWidth));
    buttonRow.removeFromRight(SPACING);
    m_cancelButton.setBounds(buttonRow.removeFromRight(waveedit::ui::kButtonWidth));
}

//==============================================================================
GenerateDialog::Result GenerateDialog::readControls() const
{
    Result r;
    r.mode = m_mode;
    if (m_mode == Mode::Tone)
        r.waveform = static_cast<AudioGenerator::Waveform>(m_typeCombo.getSelectedId() - 1);
    else
        r.noiseType = static_cast<AudioGenerator::NoiseType>(m_typeCombo.getSelectedId() - 1);
    r.frequencyHz = m_freqSlider.getValue();
    r.amplitudeDb = (float) m_ampSlider.getValue();
    r.durationSeconds = m_durationEditor.getText().trim().getDoubleValue();
    return r;
}

double GenerateDialog::effectiveDurationSeconds() const
{
    if (m_hasSelection)
        return m_selectionSeconds;
    const double d = m_durationEditor.getText().trim().getDoubleValue();
    return (d > 0.0) ? d : 1.0;
}

bool GenerateDialog::isDurationValid() const
{
    // With a selection the duration field is ignored (selection length is used).
    if (m_hasSelection)
        return true;
    const double d = m_durationEditor.getText().trim().getDoubleValue();
    return d > 0.0 && d <= 3600.0;
}

void GenerateDialog::updateValidity()
{
    const bool valid = isDurationValid();
    m_generateButton.setEnabled(valid);
    m_previewButton.setEnabled(engineUsable() && valid);
    m_validationLabel.setText(valid ? "" : "Enter a duration between 0 and 3600 seconds.",
                              juce::dontSendNotification);
    if (!valid && m_previewActive)
        stopPreview();
}

void GenerateDialog::fillBuffer(juce::AudioBuffer<float>& buffer) const
{
    const Result r = readControls();
    const float amp = std::pow(10.0f, r.amplitudeDb / 20.0f);   // dBFS -> linear

    AudioGenerator gen;
    gen.prepare(m_sampleRate);
    if (m_mode == Mode::Tone)
        gen.generateTone(buffer, r.waveform, r.frequencyHz, amp);
    else
        gen.generateNoise(buffer, r.noiseType, amp);
}

//==============================================================================
bool GenerateDialog::engineUsable() const noexcept
{
    return m_engine != nullptr && m_lifeline.getComponent() != nullptr;
}

void GenerateDialog::startPreview()
{
    if (!engineUsable())
        return;

    // Audition up to 2 s (looped) so a long duration doesn't allocate a huge
    // preview buffer.
    const double dur = juce::jlimit(0.25, 2.0, effectiveDurationSeconds());
    const int numSamples = (int) std::llround(dur * m_sampleRate);
    if (numSamples <= 0)
        return;

    juce::AudioBuffer<float> preview(m_numChannels, numSamples);
    preview.clear();
    fillBuffer(preview);

    if (!m_engine->startBufferPreview(preview, m_sampleRate, m_numChannels, /*loop*/ true))
        return;

    m_previewActive = true;
    m_previewButton.setButtonText("STOP");
    m_previewButton.setColour(juce::TextButton::buttonColourId,
                              waveedit::ui::colour(waveedit::ui::kButtonPreviewActive));
}

void GenerateDialog::stopPreview()
{
    if (engineUsable())
        m_engine->stopSelectionPreview();

    m_previewActive = false;
    m_previewButton.setButtonText("Preview");
    m_previewButton.removeColour(juce::TextButton::buttonColourId);
}

void GenerateDialog::restartPreviewIfActive()
{
    if (m_previewActive)
        startPreview();   // re-render + restart with the new parameters
}

void GenerateDialog::confirmDialog()
{
    if (!isDurationValid())
        return;

    stopPreview();

    if (m_callback)
        m_callback(readControls());

    if (auto* modal = juce::ModalComponentManager::getInstance()->getModalComponent(0))
        modal->exitModalState(0);
}

//==============================================================================
void GenerateDialog::showDialog(juce::Component* /*parent*/,
                                Mode mode,
                                AudioEngine* engine,
                                juce::Component::SafePointer<WaveformDisplay> lifeline,
                                bool hasSelection,
                                double selectionSeconds,
                                double sampleRate,
                                int numChannels,
                                std::function<void(const Result&)> callback)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto* dialog = new GenerateDialog(mode, engine, lifeline, hasSelection,
                                      selectionSeconds, sampleRate, numChannels);
    dialog->m_callback = std::move(callback);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = (mode == Mode::Tone) ? "Generate Tone" : "Generate Noise";
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;
    options.launchAsync();
}
