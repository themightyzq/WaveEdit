#include "GainDialog.h"
#include "../Audio/AudioEngine.h"
#include "../Audio/AudioBufferManager.h"

GainDialog::GainDialog(AudioEngine* audioEngine, AudioBufferManager* bufferManager, int64_t selectionStart, int64_t selectionEnd)
    : m_titleLabel("titleLabel", "Apply Gain"),
      m_gainLabel("gainLabel", "Gain (dB):"),
      m_gainValueLabel("gainValueLabel", "0.0 dB"),
      m_previewButton("Preview"),
      m_applyButton("Apply"),
      m_cancelButton("Cancel"),
      m_audioEngine(audioEngine),
      m_bufferManager(bufferManager),
      m_isPreviewActive(false),
      m_isPreviewPlaying(false),
      m_selectionStart(selectionStart),
      m_selectionEnd(selectionEnd)
{
    // Title label
    m_titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Gain label
    m_gainLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(m_gainLabel);

    // Gain input (text field)
    m_gainInput.setInputRestrictions(0, "-0123456789.");
    m_gainInput.setJustification(juce::Justification::centredLeft);
    m_gainInput.setText("0.0");
    m_gainInput.setSelectAllWhenFocused(true);
    m_gainInput.onReturnKey = [this]() { onApplyClicked(); };
    m_gainInput.onEscapeKey = [this]() { onCancelClicked(); };
    m_gainInput.onTextChange = [this]()
    {
        // Sync slider with text input
        auto gain = getValidatedGain();
        if (gain.has_value())
        {
            m_gainSlider.setValue(gain.value(), juce::dontSendNotification);
            m_gainValueLabel.setText(juce::String(gain.value(), 1) + " dB", juce::dontSendNotification);

            // Update preview in real-time if active
            if (m_isPreviewActive && m_audioEngine)
            {
                PreviewMode currentMode = m_audioEngine->getPreviewMode();

                if (currentMode == PreviewMode::REALTIME_DSP)
                {
                    // Real-time DSP mode - just update the gain parameter
                    m_audioEngine->setGainPreview(gain.value(), true);
                }
                else if (currentMode == PreviewMode::OFFLINE_BUFFER && m_bufferManager &&
                         m_selectionEnd > m_selectionStart)
                {
                    // OFFLINE_BUFFER mode with selection - need to re-render the preview buffer
                    bool wasPlaying = m_audioEngine->isPlaying();
                    double currentPos = m_audioEngine->getCurrentPosition();

                    // Extract and process the selection with new gain value
                    int64_t numSamples = m_selectionEnd - m_selectionStart;
                    juce::AudioBuffer<float> selectionBuffer = m_bufferManager->getAudioRange(m_selectionStart, numSamples);

                    float gainLinear = juce::Decibels::decibelsToGain(gain.value());
                    selectionBuffer.applyGain(gainLinear);

                    // Reload the preview buffer
                    m_audioEngine->loadPreviewBuffer(selectionBuffer,
                                                    m_bufferManager->getSampleRate(),
                                                    m_bufferManager->getNumChannels());

                    // Restore loop points in preview buffer coordinates if looping is enabled
                    if (m_loopCheckbox.getToggleState())
                    {
                        double selectionLengthSec = numSamples / m_bufferManager->getSampleRate();
                        m_audioEngine->setLoopPoints(0.0, selectionLengthSec);
                    }

                    // Restore playback state
                    if (wasPlaying)
                    {
                        m_audioEngine->setPosition(currentPos);
                        m_audioEngine->play();
                    }
                }
            }
        }
    };
    addAndMakeVisible(m_gainInput);

    // Gain slider for real-time adjustment
    m_gainSlider.setRange(-60.0, 40.0, 0.1);  // -60dB to +40dB in 0.1dB steps
    m_gainSlider.setValue(0.0);
    m_gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    m_gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    m_gainSlider.onValueChange = [this]() { onSliderValueChanged(); };
    addAndMakeVisible(m_gainSlider);

    // Gain value label (shows current slider value)
    m_gainValueLabel.setJustificationType(juce::Justification::centred);
    m_gainValueLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    addAndMakeVisible(m_gainValueLabel);

    // Loop checkbox for preview
    m_loopCheckbox.setButtonText("Loop");
    m_loopCheckbox.setToggleState(true, juce::dontSendNotification);  // Default ON
    addAndMakeVisible(m_loopCheckbox);

    // Preview button (non-toggle - starts looped playback with preview)
    m_previewButton.onClick = [this]() { onPreviewClicked(); };
    m_previewButton.setEnabled(m_audioEngine != nullptr);
    addAndMakeVisible(m_previewButton);

    // Bypass button for A/B comparison (only enabled when preview is active)
    m_bypassButton.setButtonText("Bypass");
    m_bypassButton.onClick = [this]() { onBypassClicked(); };
    m_bypassButton.setEnabled(false);  // Disabled until preview starts
    addAndMakeVisible(m_bypassButton);

    // Apply button
    m_applyButton.onClick = [this]() { onApplyClicked(); };
    addAndMakeVisible(m_applyButton);

    // Cancel button
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    // Set initial focus to text input
    m_gainInput.setWantsKeyboardFocus(true);

    setSize(450, 260);  // Width matches ParametricEQDialog standard for button layout
}

std::optional<float> GainDialog::showDialog(AudioEngine* audioEngine, AudioBufferManager* bufferManager, int64_t selectionStart, int64_t selectionEnd)
{
    GainDialog dialog(audioEngine, bufferManager, selectionStart, selectionEnd);  // Stack allocation - no memory leak

    juce::DialogWindow::LaunchOptions options;
    options.content.setNonOwned(&dialog);  // Use setNonOwned for stack object
    options.dialogTitle = "Apply Gain";
    options.dialogBackgroundColour = juce::Colour(0xff2b2b2b);
    options.escapeKeyTriggersCloseButton = false;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.componentToCentreAround = nullptr;

    #if JUCE_MODAL_LOOPS_PERMITTED
        // Show modal dialog and wait for result
        int result = options.runModal();

        // Ensure preview is disabled when dialog closes
        dialog.disablePreview();

        // Return the gain value only if Apply was clicked (result == 1)
        return (result == 1) ? dialog.m_result : std::nullopt;
    #else
        jassertfalse; // Modal loops required for this dialog
        return std::nullopt;
    #endif
}

void GainDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));

    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawRect(getLocalBounds(), 1);
}

void GainDialog::resized()
{
    auto area = getLocalBounds().reduced(15);

    // Title
    m_titleLabel.setBounds(area.removeFromTop(30));
    area.removeFromTop(10); // Spacing

    // Gain input row (text field)
    auto inputRow = area.removeFromTop(30);
    m_gainLabel.setBounds(inputRow.removeFromLeft(80));
    inputRow.removeFromLeft(10); // Spacing
    m_gainInput.setBounds(inputRow);

    area.removeFromTop(10); // Spacing

    // Gain slider row
    auto sliderRow = area.removeFromTop(40);
    m_gainSlider.setBounds(sliderRow.removeFromTop(25));
    m_gainValueLabel.setBounds(sliderRow);

    area.removeFromTop(10); // Spacing before buttons

    // Buttons (bottom) - standardized layout
    // Left: Preview + Bypass + Loop | Right: Cancel + Apply
    auto buttonRow = area.removeFromTop(30);
    const int buttonWidth = 90;
    const int buttonSpacing = 10;

    // Left side: Preview, Bypass, and Loop toggle
    m_previewButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(buttonSpacing);
    m_bypassButton.setBounds(buttonRow.removeFromLeft(70));  // Slightly narrower for bypass
    buttonRow.removeFromLeft(buttonSpacing);
    m_loopCheckbox.setBounds(buttonRow.removeFromLeft(60));  // Reduced width for just "Loop"
    buttonRow.removeFromLeft(buttonSpacing);

    // Right side: Cancel and Apply buttons
    m_applyButton.setBounds(buttonRow.removeFromRight(buttonWidth));
    buttonRow.removeFromRight(buttonSpacing);
    m_cancelButton.setBounds(buttonRow.removeFromRight(buttonWidth));
}

std::optional<float> GainDialog::getValidatedGain() const
{
    auto text = m_gainInput.getText().trim();

    if (text.isEmpty())
    {
        return std::nullopt;
    }

    try
    {
        size_t pos = 0;
        float value = std::stof(text.toStdString(), &pos);

        // Check if entire string was parsed
        if (pos != text.toStdString().length())
        {
            return std::nullopt;  // Partial parse - invalid input
        }

        // Validate reasonable gain range (-100dB to +100dB)
        // Prevents equipment damage and hearing damage from extreme values
        if (value < -100.0f || value > 100.0f)
        {
            return std::nullopt;  // Out of reasonable range
        }

        return value;
    }
    catch (const std::invalid_argument&)
    {
        return std::nullopt;  // Invalid number format
    }
    catch (const std::out_of_range&)
    {
        return std::nullopt;  // Number too large for float
    }
}

void GainDialog::onApplyClicked()
{
    auto gain = getValidatedGain();

    if (!gain.has_value())
    {
        // Show validation error
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Invalid Input",
            "Please enter a valid numeric gain value in dB.\n\n"
            "Valid range: -100.0 to +100.0 dB\n"
            "Example: -6.0 or 3.5",
            "OK"
        );
        return;
    }

    // Disable preview before applying (cleanup)
    disablePreview();

    m_result = gain;

    // Close the dialog
    if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
    {
        parent->exitModalState(1);
    }
}

void GainDialog::onCancelClicked()
{
    // Disable preview before closing
    disablePreview();

    m_result = std::nullopt;

    // Close the dialog
    if (auto* parent = findParentComponentOfClass<juce::DialogWindow>())
    {
        parent->exitModalState(0);
    }
}

void GainDialog::onPreviewClicked()
{
    if (!m_audioEngine)
    {
        return;  // No audio engine available
    }

    // Toggle behavior: if preview is playing, stop it
    if (m_isPreviewPlaying && m_audioEngine->isPlaying())
    {
        m_audioEngine->stop();
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);
        m_audioEngine->setPreviewBypassed(false);  // Clear bypass state
        m_isPreviewPlaying = false;
        m_isPreviewActive = false;
        m_previewButton.setButtonText("Preview");
        m_previewButton.setColour(juce::TextButton::buttonColourId, getLookAndFeel().findColour(juce::TextButton::buttonColourId));

        // Disable bypass button when preview stops
        m_bypassButton.setEnabled(false);
        m_bypassButton.setButtonText("Bypass");
        m_bypassButton.setColour(juce::TextButton::buttonColourId, getLookAndFeel().findColour(juce::TextButton::buttonColourId));
        return;
    }

    // Get current gain value
    auto gain = getValidatedGain();
    if (!gain.has_value())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Invalid Gain Value",
            "Please enter a valid gain value before previewing.",
            "OK"
        );
        return;
    }

    // CRITICAL: Clear any stale loop points from previous playback sessions
    // This prevents coordinate system mismatch between old main buffer loop points
    // and new preview buffer playback (which uses 0-based coordinates)
    m_audioEngine->clearLoopPoints();

    // Enable looping based on checkbox state
    bool shouldLoop = m_loopCheckbox.getToggleState();
    m_audioEngine->setLooping(shouldLoop);
    m_isPreviewActive = true;

    // Use REALTIME_DSP mode for instant parameter updates
    m_audioEngine->setPreviewMode(PreviewMode::REALTIME_DSP);
    m_audioEngine->setGainPreview(gain.value(), true);

    // If we have a selection, set up playback for that range
    if (m_bufferManager && m_selectionEnd > m_selectionStart)
    {
        // Set preview selection offset for accurate cursor positioning
        m_audioEngine->setPreviewSelectionOffset(m_selectionStart);

        // Set position and loop points in FILE coordinates
        const double sampleRate = m_bufferManager->getSampleRate();
        double selectionStartSec = m_selectionStart / sampleRate;
        double selectionEndSec = m_selectionEnd / sampleRate;

        m_audioEngine->setPosition(selectionStartSec);

        if (shouldLoop)
        {
            m_audioEngine->setLoopPoints(selectionStartSec, selectionEndSec);
        }
    }
    else
    {
        // No selection - play from beginning of file
        m_audioEngine->setPreviewSelectionOffset(0);
        m_audioEngine->setPosition(0.0);
    }

    m_audioEngine->play();

    // Update button state for toggle
    m_isPreviewPlaying = true;
    m_previewButton.setButtonText("Stop Preview");
    m_previewButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);

    // Enable bypass button now that preview is active
    m_bypassButton.setEnabled(true);
}

void GainDialog::onSliderValueChanged()
{
    // Update text input to match slider
    float value = static_cast<float>(m_gainSlider.getValue());
    m_gainInput.setText(juce::String(value, 1), juce::dontSendNotification);

    // Update value label
    m_gainValueLabel.setText(juce::String(value, 1) + " dB", juce::dontSendNotification);

    // Update preview in real-time if preview is active
    if (m_isPreviewActive && m_audioEngine)
    {
        PreviewMode currentMode = m_audioEngine->getPreviewMode();

        if (currentMode == PreviewMode::REALTIME_DSP)
        {
            // Real-time DSP mode - just update the gain parameter
            m_audioEngine->setGainPreview(value, true);
        }
        else if (currentMode == PreviewMode::OFFLINE_BUFFER && m_bufferManager &&
                 m_selectionEnd > m_selectionStart)
        {
            // OFFLINE_BUFFER mode with selection - need to re-render the preview buffer
            bool wasPlaying = m_audioEngine->isPlaying();
            double currentPos = m_audioEngine->getCurrentPosition();

            // Extract and process the selection with new gain value
            int64_t numSamples = m_selectionEnd - m_selectionStart;
            juce::AudioBuffer<float> selectionBuffer = m_bufferManager->getAudioRange(m_selectionStart, numSamples);

            float gainLinear = juce::Decibels::decibelsToGain(value);
            selectionBuffer.applyGain(gainLinear);

            // Update preview selection offset
            m_audioEngine->setPreviewSelectionOffset(m_selectionStart);

            // Reload the preview buffer
            m_audioEngine->loadPreviewBuffer(selectionBuffer,
                                            m_bufferManager->getSampleRate(),
                                            m_bufferManager->getNumChannels());

            // Restore loop points in preview buffer coordinates if looping is enabled
            if (m_loopCheckbox.getToggleState())
            {
                double selectionLengthSec = numSamples / m_bufferManager->getSampleRate();
                m_audioEngine->setLoopPoints(0.0, selectionLengthSec);
            }

            // Restore playback state
            if (wasPlaying)
            {
                m_audioEngine->setPosition(currentPos);
                m_audioEngine->play();
            }
        }
    }
}

void GainDialog::disablePreview()
{
    if (m_audioEngine && m_isPreviewActive)
    {
        // CRITICAL: Use pause() instead of stop() to preserve loop state for next preview
        // stop() calls clearLoopPoints() and setLooping(false), which breaks looping
        // on subsequent preview sessions. pause() only stops playback without clearing state.
        m_audioEngine->pause();

        // CRITICAL: Clear loop points and looping AFTER pausing
        // This ensures clean state for main playback while preserving the ability
        // to loop correctly on the next preview session
        m_audioEngine->clearLoopPoints();
        m_audioEngine->setLooping(false);

        // Clear preview mode and effects
        m_audioEngine->setGainPreview(0.0f, false);
        m_audioEngine->setPreviewMode(PreviewMode::DISABLED);

        // Clear bypass state
        m_audioEngine->setPreviewBypassed(false);

        m_isPreviewActive = false;
    }

    // Reset bypass button state
    m_bypassButton.setEnabled(false);
    m_bypassButton.setButtonText("Bypass");
    m_bypassButton.setColour(juce::TextButton::buttonColourId, getLookAndFeel().findColour(juce::TextButton::buttonColourId));
}

void GainDialog::onBypassClicked()
{
    if (!m_audioEngine || !m_isPreviewActive)
    {
        return;  // Bypass only works when preview is active
    }

    // Toggle bypass state
    bool newBypassState = !m_audioEngine->isPreviewBypassed();
    m_audioEngine->setPreviewBypassed(newBypassState);

    // Update button visual state
    if (newBypassState)
    {
        m_bypassButton.setButtonText("Bypassed");
        m_bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffff8c00));  // Orange for bypassed
    }
    else
    {
        m_bypassButton.setButtonText("Bypass");
        m_bypassButton.setColour(juce::TextButton::buttonColourId, getLookAndFeel().findColour(juce::TextButton::buttonColourId));
    }
}
