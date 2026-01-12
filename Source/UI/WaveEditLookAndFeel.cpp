/*
  ==============================================================================

    WaveEditLookAndFeel.cpp
    Created: 2026-01-12
    Author:  ZQ SFX

  ==============================================================================
*/

#include "WaveEditLookAndFeel.h"

namespace waveedit
{

WaveEditLookAndFeel::WaveEditLookAndFeel()
{
    // Set default dark theme colors
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(ui::kBackgroundPrimary));
    setColour(juce::TextButton::buttonColourId, juce::Colour(ui::kSurface));
    setColour(juce::TextButton::textColourOffId, juce::Colour(ui::kTextPrimary));
    setColour(juce::TextButton::textColourOnId, juce::Colour(ui::kTextPrimary));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(ui::kBackgroundSecondary));
    setColour(juce::ComboBox::textColourId, juce::Colour(ui::kTextPrimary));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(ui::kBorder));
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(ui::kBackgroundSecondary));
    setColour(juce::TextEditor::textColourId, juce::Colour(ui::kTextPrimary));
    setColour(juce::TextEditor::outlineColourId, juce::Colour(ui::kBorder));
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(ui::kAccentPrimary));
    setColour(juce::Label::textColourId, juce::Colour(ui::kTextPrimary));
    setColour(juce::Slider::thumbColourId, juce::Colour(ui::kTextPrimary));
    setColour(juce::Slider::trackColourId, juce::Colour(ui::kSurface));
    setColour(juce::Slider::backgroundColourId, juce::Colour(ui::kBackgroundSecondary));
}

void WaveEditLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto baseColour = backgroundColour;

    if (shouldDrawButtonAsDown)
        baseColour = baseColour.darker(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        baseColour = baseColour.brighter(0.1f);

    // Draw button background
    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, 4.0f);

    // Draw border
    g.setColour(juce::Colour(ui::kBorder));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // Draw focus ring if button has keyboard focus
    if (hasFocus(button))
    {
        drawFocusRing(g, button.getLocalBounds());
    }
}

void WaveEditLookAndFeel::drawButtonText(juce::Graphics& g,
                                          juce::TextButton& button,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool shouldDrawButtonAsDown)
{
    auto font = getTextButtonFont(button, button.getHeight());
    g.setFont(font);

    auto textColour = button.findColour(button.getToggleState()
                                         ? juce::TextButton::textColourOnId
                                         : juce::TextButton::textColourOffId);

    if (!button.isEnabled())
        textColour = juce::Colour(ui::kTextDisabled);

    g.setColour(textColour);

    auto bounds = button.getLocalBounds().reduced(4, 2);
    g.drawText(button.getButtonText(), bounds, juce::Justification::centred, true);
}

void WaveEditLookAndFeel::drawToggleButton(juce::Graphics& g,
                                            juce::ToggleButton& button,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    auto fontSize = juce::jmin(15.0f, static_cast<float>(button.getHeight()) * 0.75f);
    auto tickWidth = fontSize * 1.1f;

    // Draw checkbox box
    auto tickBounds = juce::Rectangle<float>(4.0f, (static_cast<float>(button.getHeight()) - tickWidth) * 0.5f,
                                              tickWidth, tickWidth);

    g.setColour(juce::Colour(ui::kBackgroundSecondary));
    g.fillRoundedRectangle(tickBounds, 3.0f);

    g.setColour(juce::Colour(ui::kBorder));
    g.drawRoundedRectangle(tickBounds, 3.0f, 1.0f);

    // Draw checkmark if toggled
    if (button.getToggleState())
    {
        g.setColour(juce::Colour(ui::kAccentPrimary));
        auto tick = tickBounds.reduced(4.0f);
        g.drawLine(tick.getX(), tick.getCentreY(),
                   tick.getCentreX(), tick.getBottom(), 2.0f);
        g.drawLine(tick.getCentreX(), tick.getBottom(),
                   tick.getRight(), tick.getY(), 2.0f);
    }

    // Draw text
    g.setColour(button.isEnabled() ? juce::Colour(ui::kTextPrimary)
                                    : juce::Colour(ui::kTextDisabled));

    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    g.setFont(juce::Font(fontSize));
    #pragma GCC diagnostic pop

    auto textBounds = button.getLocalBounds().withTrimmedLeft(static_cast<int>(tickWidth) + 10);
    g.drawText(button.getButtonText(), textBounds, juce::Justification::centredLeft, true);

    // Draw focus ring around entire button if focused
    if (hasFocus(button))
    {
        drawFocusRing(g, button.getLocalBounds());
    }
}

void WaveEditLookAndFeel::drawComboBox(juce::Graphics& g,
                                        int width,
                                        int height,
                                        bool isButtonDown,
                                        int buttonX,
                                        int buttonY,
                                        int buttonW,
                                        int buttonH,
                                        juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);

    // Background
    g.setColour(juce::Colour(ui::kBackgroundSecondary));
    g.fillRoundedRectangle(bounds, 4.0f);

    // Border
    g.setColour(juce::Colour(ui::kBorder));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // Arrow
    juce::Path path;
    auto arrowBounds = juce::Rectangle<float>(static_cast<float>(buttonX), static_cast<float>(buttonY),
                                               static_cast<float>(buttonW), static_cast<float>(buttonH));
    auto arrowX = arrowBounds.getCentreX();
    auto arrowY = arrowBounds.getCentreY();

    path.startNewSubPath(arrowX - 4.0f, arrowY - 2.0f);
    path.lineTo(arrowX, arrowY + 2.0f);
    path.lineTo(arrowX + 4.0f, arrowY - 2.0f);

    g.setColour(juce::Colour(ui::kTextSecondary));
    g.strokePath(path, juce::PathStrokeType(2.0f));

    // Draw focus ring if focused
    if (hasFocus(box))
    {
        drawFocusRing(g, box.getLocalBounds());
    }
}

void WaveEditLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                            int x, int y, int width, int height,
                                            float sliderPos,
                                            float minSliderPos,
                                            float maxSliderPos,
                                            const juce::Slider::SliderStyle style,
                                            juce::Slider& slider)
{
    // Call parent implementation for the actual slider drawing
    LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos,
                                      minSliderPos, maxSliderPos, style, slider);

    // Add focus ring if focused
    if (hasFocus(slider))
    {
        drawFocusRing(g, slider.getLocalBounds());
    }
}

void WaveEditLookAndFeel::fillTextEditorBackground(juce::Graphics& g,
                                                    int width,
                                                    int height,
                                                    juce::TextEditor& textEditor)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();

    g.setColour(textEditor.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(bounds, 4.0f);
}

void WaveEditLookAndFeel::drawTextEditorOutline(juce::Graphics& g,
                                                 int width,
                                                 int height,
                                                 juce::TextEditor& textEditor)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);

    if (hasFocus(textEditor))
    {
        // Use accent color for focused state
        g.setColour(juce::Colour(ui::kAccentPrimary));
        g.drawRoundedRectangle(bounds, 4.0f, 2.0f);
    }
    else
    {
        g.setColour(juce::Colour(ui::kBorder));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }
}

void WaveEditLookAndFeel::drawFocusRing(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(juce::Colour(ui::kAccentPrimary));
    g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 4.0f,
                           static_cast<float>(ui::kFocusRingWidth));
}

bool WaveEditLookAndFeel::hasFocus(juce::Component& component)
{
    return component.hasKeyboardFocus(false);
}

} // namespace waveedit
