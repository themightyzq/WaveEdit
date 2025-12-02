/*
  ==============================================================================

    FadeCurvePreview.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "FadeCurvePreview.h"

FadeCurvePreview::FadeCurvePreview(bool isFadeIn)
    : m_isFadeIn(isFadeIn)
{
    // Set preferred size
    setSize(100, 60);
}

void FadeCurvePreview::setCurveType(FadeCurveType curveType)
{
    if (m_curveType != curveType)
    {
        m_curveType = curveType;
        repaint();
    }
}

void FadeCurvePreview::paint(juce::Graphics& g)
{
    // Draw background
    g.fillAll(juce::Colour(0xff2a2a2a));  // Dark grey background

    // Get drawing bounds with small padding
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    const float width = bounds.getWidth();
    const float height = bounds.getHeight();

    // Draw grid lines (subtle)
    g.setColour(juce::Colour(0xff404040));  // Subtle grey for grid
    const float gridAlpha = 0.3f;
    g.setOpacity(gridAlpha);

    // Horizontal grid lines at 25%, 50%, 75%
    for (float yPercent : {0.25f, 0.5f, 0.75f})
    {
        float y = bounds.getY() + height * (1.0f - yPercent);
        g.drawHorizontalLine(static_cast<int>(y),
                           bounds.getX(), bounds.getRight());
    }

    // Vertical grid lines at 25%, 50%, 75%
    for (float xPercent : {0.25f, 0.5f, 0.75f})
    {
        float x = bounds.getX() + width * xPercent;
        g.drawVerticalLine(static_cast<int>(x),
                         bounds.getY(), bounds.getBottom());
    }

    g.setOpacity(1.0f);

    // Draw border
    g.setColour(juce::Colour(0xff505050));
    g.drawRect(bounds, 1.0f);

    // Draw curve
    juce::Path curvePath;

    // Calculate curve points
    const int numPoints = static_cast<int>(width);
    for (int x = 0; x <= numPoints; ++x)
    {
        float normalizedX = x / width;

        // calculateCurveValue() handles fade in/out differences internally
        float curveValue = calculateCurveValue(normalizedX, m_curveType);

        // Convert to screen coordinates (Y is inverted in screen space)
        float screenX = bounds.getX() + x;
        float screenY = bounds.getBottom() - (curveValue * height);

        if (x == 0)
        {
            curvePath.startNewSubPath(screenX, screenY);
        }
        else
        {
            curvePath.lineTo(screenX, screenY);
        }
    }

    // Draw the curve with anti-aliasing
    g.setColour(juce::Colours::cyan);
    g.strokePath(curvePath, juce::PathStrokeType(2.0f));

    // Add subtle glow effect
    g.setColour(juce::Colours::cyan.withAlpha(0.3f));
    g.strokePath(curvePath, juce::PathStrokeType(4.0f));

    // Draw curve type label (small text at bottom)
    const char* curveNames[] = { "Linear", "Exponential", "Logarithmic", "S-Curve" };
    g.setColour(juce::Colour(0xff808080));  // Grey text
    g.setFont(10.0f);
    g.drawText(curveNames[static_cast<int>(m_curveType)],
               bounds.reduced(2.0f),
               juce::Justification::bottomRight, false);
}

float FadeCurvePreview::calculateCurveValue(float normalizedPosition, FadeCurveType curveType) const
{
    // Clamp input to valid range
    normalizedPosition = juce::jlimit(0.0f, 1.0f, normalizedPosition);

    float gain = 0.0f;

    // Use the exact same formulas as AudioProcessor for consistency
    // CRITICAL: Fade In and Fade Out use different formulas for EXPONENTIAL/LOGARITHMIC
    if (m_isFadeIn)
    {
        // Fade In formulas (0% → 100% amplitude)
        switch (curveType)
        {
            case FadeCurveType::LINEAR:
                gain = normalizedPosition;
                break;

            case FadeCurveType::EXPONENTIAL:
                // Slow start, fast end (x²)
                gain = normalizedPosition * normalizedPosition;
                break;

            case FadeCurveType::LOGARITHMIC:
                // Fast start, slow end (1 - (1-x)²)
                gain = 1.0f - (1.0f - normalizedPosition) * (1.0f - normalizedPosition);
                break;

            case FadeCurveType::S_CURVE:
                // Smooth start and end (smoothstep: 3x² - 2x³)
                gain = normalizedPosition * normalizedPosition * (3.0f - 2.0f * normalizedPosition);
                break;
        }
    }
    else
    {
        // Fade Out formulas (100% → 0% amplitude)
        // NOTE: EXPONENTIAL/LOGARITHMIC curves are swapped to maintain perceptual characteristics
        switch (curveType)
        {
            case FadeCurveType::LINEAR:
                gain = 1.0f - normalizedPosition;
                break;

            case FadeCurveType::EXPONENTIAL:
                // Fast start, slow end for fade out (inverted logarithmic shape)
                gain = (1.0f - normalizedPosition) * (1.0f - normalizedPosition);
                break;

            case FadeCurveType::LOGARITHMIC:
                // Slow start, fast end for fade out (inverted exponential shape)
                gain = 1.0f - normalizedPosition * normalizedPosition;
                break;

            case FadeCurveType::S_CURVE:
            {
                // S-Curve is symmetric, so invert the result
                float fadeInValue = normalizedPosition * normalizedPosition * (3.0f - 2.0f * normalizedPosition);
                gain = 1.0f - fadeInValue;
                break;
            }
        }
    }

    return gain;
}