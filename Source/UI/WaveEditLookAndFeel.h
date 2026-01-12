/*
  ==============================================================================

    WaveEditLookAndFeel.h
    Created: 2026-01-12
    Author:  ZQ SFX

    Custom LookAndFeel for WaveEdit with WCAG-compliant focus indicators.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "UIConstants.h"

namespace waveedit
{

/**
 * @brief Custom LookAndFeel for WaveEdit
 *
 * Extends JUCE's LookAndFeel_V4 with:
 * - Visible focus rings on buttons and other focusable components
 * - Dark theme consistent with WaveEdit UI
 * - WCAG-compliant color contrast
 */
class WaveEditLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WaveEditLookAndFeel();
    ~WaveEditLookAndFeel() override = default;

    // =========================================================================
    // Button Drawing
    // =========================================================================

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    // =========================================================================
    // Toggle Button Drawing
    // =========================================================================

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    // =========================================================================
    // ComboBox Drawing
    // =========================================================================

    void drawComboBox(juce::Graphics& g,
                      int width,
                      int height,
                      bool isButtonDown,
                      int buttonX,
                      int buttonY,
                      int buttonW,
                      int buttonH,
                      juce::ComboBox& box) override;

    // =========================================================================
    // Slider Drawing
    // =========================================================================

    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    // =========================================================================
    // TextEditor Drawing
    // =========================================================================

    void fillTextEditorBackground(juce::Graphics& g,
                                  int width,
                                  int height,
                                  juce::TextEditor& textEditor) override;

    void drawTextEditorOutline(juce::Graphics& g,
                               int width,
                               int height,
                               juce::TextEditor& textEditor) override;

private:
    /**
     * @brief Draw a focus ring around the given bounds
     */
    void drawFocusRing(juce::Graphics& g, juce::Rectangle<int> bounds);

    /**
     * @brief Check if a component has keyboard focus
     */
    static bool hasFocus(juce::Component& component);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveEditLookAndFeel)
};

} // namespace waveedit
