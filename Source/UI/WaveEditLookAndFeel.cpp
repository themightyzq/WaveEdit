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
    applyThemeColours();

    // Re-skin live when the user switches Dark / Light / High Contrast.
    ThemeManager::getInstance().addChangeListener(this);
}

WaveEditLookAndFeel::~WaveEditLookAndFeel()
{
    ThemeManager::getInstance().removeChangeListener(this);
}

void WaveEditLookAndFeel::applyThemeColours()
{
    const auto& t = theme();

    setColour(juce::ResizableWindow::backgroundColourId, t.background);

    setColour(juce::TextButton::buttonColourId, t.panel);
    setColour(juce::TextButton::textColourOffId, t.text);
    setColour(juce::TextButton::textColourOnId, t.text);

    setColour(juce::ComboBox::backgroundColourId, t.panelAlternate);
    setColour(juce::ComboBox::textColourId, t.text);
    setColour(juce::ComboBox::outlineColourId, t.border);
    setColour(juce::ComboBox::arrowColourId, t.textMuted);

    setColour(juce::TextEditor::backgroundColourId, t.panelAlternate);
    setColour(juce::TextEditor::textColourId, t.text);
    setColour(juce::TextEditor::outlineColourId, t.border);
    setColour(juce::TextEditor::focusedOutlineColourId, t.accent);
    setColour(juce::TextEditor::highlightColourId, t.accent.withAlpha(0.30f));
    setColour(juce::TextEditor::highlightedTextColourId, t.text);

    setColour(juce::Label::textColourId, t.text);

    setColour(juce::Slider::thumbColourId, t.accent);
    setColour(juce::Slider::trackColourId, t.panel);
    setColour(juce::Slider::backgroundColourId, t.panelAlternate);

    // Menus inherit the theme so right-click / dropdown surfaces re-skin too.
    setColour(juce::PopupMenu::backgroundColourId, t.panel);
    setColour(juce::PopupMenu::textColourId, t.text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, t.accent);
    setColour(juce::PopupMenu::highlightedTextColourId, t.background);
}

void WaveEditLookAndFeel::changeListenerCallback(juce::ChangeBroadcaster* /*source*/)
{
    applyThemeColours();

    // As the default LookAndFeel, refreshing every top-level window cascades a
    // repaint to all children, re-skinning all standard controls at once.
    for (int i = juce::TopLevelWindow::getNumTopLevelWindows(); --i >= 0;)
    {
        if (auto* w = juce::TopLevelWindow::getTopLevelWindow(i))
            w->repaint();
    }
}

void WaveEditLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto baseColour = backgroundColour;

    // Luminance-aware so hover/press stay visible on both dark and light
    // surfaces (brighter() is a no-op on a white button).
    const bool isLightSurface = baseColour.getPerceivedBrightness() > 0.5f;

    if (shouldDrawButtonAsDown)
        baseColour = isLightSurface ? baseColour.darker(0.18f) : baseColour.brighter(0.30f);
    else if (shouldDrawButtonAsHighlighted)
        baseColour = isLightSurface ? baseColour.darker(0.08f) : baseColour.brighter(0.18f);

    // Draw button background
    g.setColour(baseColour);
    g.fillRoundedRectangle(bounds, 4.0f);

    // Draw border
    g.setColour(theme().border);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // Draw focus ring if button has keyboard focus
    if (hasFocus(button))
    {
        drawFocusRing(g, button.getLocalBounds());
    }
}

void WaveEditLookAndFeel::drawButtonText(juce::Graphics& g,
                                          juce::TextButton& button,
                                          bool /*shouldDrawButtonAsHighlighted*/,
                                          bool /*shouldDrawButtonAsDown*/)
{
    auto font = getTextButtonFont(button, button.getHeight());
    g.setFont(font);

    auto textColour = button.findColour(button.getToggleState()
                                         ? juce::TextButton::textColourOnId
                                         : juce::TextButton::textColourOffId);

    if (!button.isEnabled())
        textColour = theme().textMuted;

    g.setColour(textColour);

    auto bounds = button.getLocalBounds().reduced(4, 2);
    g.drawText(button.getButtonText(), bounds, juce::Justification::centred, true);
}

void WaveEditLookAndFeel::drawToggleButton(juce::Graphics& g,
                                            juce::ToggleButton& button,
                                            bool /*shouldDrawButtonAsHighlighted*/,
                                            bool /*shouldDrawButtonAsDown*/)
{
    const auto& t = theme();

    auto fontSize = juce::jmin(15.0f, static_cast<float>(button.getHeight()) * 0.75f);
    auto tickWidth = fontSize * 1.1f;

    // Draw checkbox box
    auto tickBounds = juce::Rectangle<float>(4.0f, (static_cast<float>(button.getHeight()) - tickWidth) * 0.5f,
                                              tickWidth, tickWidth);

    g.setColour(t.panelAlternate);
    g.fillRoundedRectangle(tickBounds, 3.0f);

    g.setColour(t.border);
    g.drawRoundedRectangle(tickBounds, 3.0f, 1.0f);

    // Draw checkmark if toggled
    if (button.getToggleState())
    {
        g.setColour(t.accent);
        auto tick = tickBounds.reduced(4.0f);
        g.drawLine(tick.getX(), tick.getCentreY(),
                   tick.getCentreX(), tick.getBottom(), 2.0f);
        g.drawLine(tick.getCentreX(), tick.getBottom(),
                   tick.getRight(), tick.getY(), 2.0f);
    }

    // Draw text
    g.setColour(button.isEnabled() ? t.text : t.textMuted);

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
                                        bool /*isButtonDown*/,
                                        int buttonX,
                                        int buttonY,
                                        int buttonW,
                                        int buttonH,
                                        juce::ComboBox& box)
{
    const auto& t = theme();
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);

    // Background
    g.setColour(t.panelAlternate);
    g.fillRoundedRectangle(bounds, 4.0f);

    // Border
    g.setColour(t.border);
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

    g.setColour(t.textMuted);
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
        g.setColour(theme().accent);
        g.drawRoundedRectangle(bounds, 4.0f, 2.0f);
    }
    else
    {
        g.setColour(theme().border);
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    }
}

void WaveEditLookAndFeel::drawFocusRing(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(theme().focus);
    g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 4.0f,
                           static_cast<float>(ui::kFocusRingWidth));
}

bool WaveEditLookAndFeel::hasFocus(juce::Component& component)
{
    return component.hasKeyboardFocus(false);
}

} // namespace waveedit
