/*
  ==============================================================================

    NavigationPreferences.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    User preferences for navigation and snapping behavior.

  ==============================================================================
*/

#pragma once

#include "AudioUnits.h"
#include <juce_data_structures/juce_data_structures.h>

/**
 * Stores user preferences for navigation and snapping.
 * These settings persist across application sessions.
 */
class NavigationPreferences
{
public:
    //==============================================================================
    // Constructor

    NavigationPreferences()
        : m_defaultSnapMode(AudioUnits::SnapMode::Off)
        , m_defaultSnapIncrement(100)  // 100 samples or 100ms depending on mode
        , m_navigationUnit(AudioUnits::UnitType::Milliseconds)
        , m_navigationIncrement(10)    // 10ms for arrow keys
        , m_navigationIncrementLarge(100)  // 100ms for Shift+arrow
        , m_navigationIncrementPage(1000)  // 1000ms (1s) for Page Up/Down
        , m_frameRate(30.0)  // Default to 30 fps for frame-based navigation
        , m_zeroCrossingSearchRadius(1000)  // Search up to 1000 samples
        , m_snapToZeroCrossingWhenNavigating(false)  // Don't auto-snap to zero by default
        , m_showNavigationFeedback(true)  // Show tooltip during navigation
    {
    }

    //==============================================================================
    // Snap mode preferences

    AudioUnits::SnapMode getDefaultSnapMode() const { return m_defaultSnapMode; }
    void setDefaultSnapMode(AudioUnits::SnapMode mode) { m_defaultSnapMode = mode; }

    int getDefaultSnapIncrement() const { return m_defaultSnapIncrement; }
    void setDefaultSnapIncrement(int increment) { m_defaultSnapIncrement = increment; }

    //==============================================================================
    // Navigation preferences

    AudioUnits::UnitType getNavigationUnit() const { return m_navigationUnit; }
    void setNavigationUnit(AudioUnits::UnitType unit) { m_navigationUnit = unit; }

    int getNavigationIncrement() const { return m_navigationIncrement; }
    void setNavigationIncrement(int increment) { m_navigationIncrement = increment; }

    int getNavigationIncrementLarge() const { return m_navigationIncrementLarge; }
    void setNavigationIncrementLarge(int increment) { m_navigationIncrementLarge = increment; }

    int getNavigationIncrementPage() const { return m_navigationIncrementPage; }
    void setNavigationIncrementPage(int increment) { m_navigationIncrementPage = increment; }

    //==============================================================================
    // Frame rate for frame-based operations

    double getFrameRate() const { return m_frameRate; }
    void setFrameRate(double fps) { m_frameRate = fps; }

    //==============================================================================
    // Zero-crossing preferences

    int getZeroCrossingSearchRadius() const { return m_zeroCrossingSearchRadius; }
    void setZeroCrossingSearchRadius(int radius) { m_zeroCrossingSearchRadius = radius; }

    bool getSnapToZeroCrossingWhenNavigating() const { return m_snapToZeroCrossingWhenNavigating; }
    void setSnapToZeroCrossingWhenNavigating(bool enabled) { m_snapToZeroCrossingWhenNavigating = enabled; }

    //==============================================================================
    // UI feedback preferences

    bool getShowNavigationFeedback() const { return m_showNavigationFeedback; }
    void setShowNavigationFeedback(bool show) { m_showNavigationFeedback = show; }

    //==============================================================================
    // Serialization

    /**
     * Converts preferences to a juce::var for JSON serialization.
     */
    juce::var toVar() const
    {
        auto obj = new juce::DynamicObject();

        obj->setProperty("defaultSnapMode", static_cast<int>(m_defaultSnapMode));
        obj->setProperty("defaultSnapIncrement", m_defaultSnapIncrement);
        obj->setProperty("navigationUnit", static_cast<int>(m_navigationUnit));
        obj->setProperty("navigationIncrement", m_navigationIncrement);
        obj->setProperty("navigationIncrementLarge", m_navigationIncrementLarge);
        obj->setProperty("navigationIncrementPage", m_navigationIncrementPage);
        obj->setProperty("frameRate", m_frameRate);
        obj->setProperty("zeroCrossingSearchRadius", m_zeroCrossingSearchRadius);
        obj->setProperty("showNavigationFeedback", m_showNavigationFeedback);
        obj->setProperty("snapToZeroCrossingWhenNavigating", m_snapToZeroCrossingWhenNavigating);

        return juce::var(obj);
    }

    /**
     * Loads preferences from a juce::var (deserialization from JSON).
     */
    void fromVar(const juce::var& v)
    {
        if (!v.isObject())
            return;

        auto obj = v.getDynamicObject();
        if (obj == nullptr)
            return;

        if (obj->hasProperty("defaultSnapMode"))
            m_defaultSnapMode = static_cast<AudioUnits::SnapMode>(static_cast<int>(obj->getProperty("defaultSnapMode")));

        if (obj->hasProperty("defaultSnapIncrement"))
            m_defaultSnapIncrement = obj->getProperty("defaultSnapIncrement");

        if (obj->hasProperty("navigationUnit"))
            m_navigationUnit = static_cast<AudioUnits::UnitType>(static_cast<int>(obj->getProperty("navigationUnit")));

        if (obj->hasProperty("navigationIncrement"))
            m_navigationIncrement = obj->getProperty("navigationIncrement");

        if (obj->hasProperty("navigationIncrementLarge"))
            m_navigationIncrementLarge = obj->getProperty("navigationIncrementLarge");

        if (obj->hasProperty("navigationIncrementPage"))
            m_navigationIncrementPage = obj->getProperty("navigationIncrementPage");

        if (obj->hasProperty("frameRate"))
            m_frameRate = obj->getProperty("frameRate");

        if (obj->hasProperty("zeroCrossingSearchRadius"))
            m_zeroCrossingSearchRadius = obj->getProperty("zeroCrossingSearchRadius");

        if (obj->hasProperty("showNavigationFeedback"))
            m_showNavigationFeedback = obj->getProperty("showNavigationFeedback");

        if (obj->hasProperty("snapToZeroCrossingWhenNavigating"))
            m_snapToZeroCrossingWhenNavigating = obj->getProperty("snapToZeroCrossingWhenNavigating");
    }

    /**
     * Resets all preferences to default values.
     */
    void resetToDefaults()
    {
        m_defaultSnapMode = AudioUnits::SnapMode::Off;
        m_defaultSnapIncrement = 100;
        m_navigationUnit = AudioUnits::UnitType::Milliseconds;
        m_navigationIncrement = 10;
        m_navigationIncrementLarge = 100;
        m_navigationIncrementPage = 1000;
        m_frameRate = 30.0;
        m_zeroCrossingSearchRadius = 1000;
        m_showNavigationFeedback = true;
        m_snapToZeroCrossingWhenNavigating = false;
    }

private:
    //==============================================================================
    // Snap preferences
    AudioUnits::SnapMode m_defaultSnapMode;
    int m_defaultSnapIncrement;

    // Navigation preferences
    AudioUnits::UnitType m_navigationUnit;
    int m_navigationIncrement;       // Arrow keys
    int m_navigationIncrementLarge;  // Shift+Arrow keys
    int m_navigationIncrementPage;   // Page Up/Down

    // Frame rate for frame-based navigation
    double m_frameRate;

    // Zero-crossing preferences
    int m_zeroCrossingSearchRadius;
    bool m_snapToZeroCrossingWhenNavigating;

    // UI feedback
    bool m_showNavigationFeedback;
};
