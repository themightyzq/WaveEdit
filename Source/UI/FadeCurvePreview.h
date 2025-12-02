/*
  ==============================================================================

    FadeCurvePreview.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Audio/AudioProcessor.h"

/**
 * Visual preview component for fade curves.
 *
 * Displays a graphical representation of fade curve shapes used in
 * fade in/out operations. Renders the curve mathematically using
 * the same formulas as AudioProcessor.
 *
 * Features:
 * - Real-time curve rendering based on FadeCurveType
 * - Grid overlay for reference (25%, 50%, 75% lines)
 * - Anti-aliased curve drawing
 * - Compact 100x60 pixel display
 * - Automatic redraw on curve type change
 *
 * Visual Design:
 * - X-axis: 0% to 100% (time/position)
 * - Y-axis: 0% to 100% (amplitude)
 * - Background: Dark grey with subtle grid
 * - Curve: Cyan with 2px stroke
 *
 * Thread Safety: All operations run on message thread.
 */
class FadeCurvePreview : public juce::Component
{
public:
    /**
     * Constructor.
     *
     * @param isFadeIn If true, shows fade in curve (0→1),
     *                 if false shows fade out curve (1→0)
     */
    FadeCurvePreview(bool isFadeIn = true);

    ~FadeCurvePreview() override = default;

    /**
     * Set the curve type to display.
     * Triggers a repaint to update the visualization.
     *
     * @param curveType The fade curve type to display
     */
    void setCurveType(FadeCurveType curveType);

    /**
     * Get the currently displayed curve type.
     *
     * @return Current curve type
     */
    FadeCurveType getCurveType() const { return m_curveType; }

    // Component overrides
    void paint(juce::Graphics& g) override;

private:
    /**
     * Calculate the curve value at a given normalized position.
     * Uses the same formulas as AudioProcessor for consistency.
     *
     * @param normalizedPosition Position from 0.0 to 1.0
     * @param curveType The curve type to calculate
     * @return Gain value from 0.0 to 1.0
     */
    float calculateCurveValue(float normalizedPosition, FadeCurveType curveType) const;

    FadeCurveType m_curveType { FadeCurveType::LINEAR };
    bool m_isFadeIn { true };  // true = fade in (0→1), false = fade out (1→0)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FadeCurvePreview)
};