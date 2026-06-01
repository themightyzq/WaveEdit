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
#include "ThemeManager.h"

namespace waveedit
{

/**
 * @brief Custom LookAndFeel for WaveEdit
 *
 * Extends JUCE's LookAndFeel_V4 with:
 * - Visible focus rings on buttons and other focusable components
 * - Colours sourced from the active ThemeManager theme (Dark / Light /
 *   High Contrast), refreshed live when the user switches themes
 * - WCAG-compliant color contrast
 *
 * This is the app-wide default LookAndFeel, so re-applying its colours on a
 * theme change and repainting every top-level window re-skins all standard
 * JUCE controls without per-component listeners.
 */
class WaveEditLookAndFeel : public juce::LookAndFeel_V4,
                            public juce::ChangeListener
{
public:
    WaveEditLookAndFeel();
    ~WaveEditLookAndFeel() override;

    /** ThemeManager broadcast: re-map colours and repaint all windows. */
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

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
     * @brief Map the active theme's tokens onto JUCE colour IDs
     */
    void applyThemeColours();

    /** @brief The currently-active theme. */
    static const Theme& theme() { return ThemeManager::getInstance().getCurrent(); }

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
