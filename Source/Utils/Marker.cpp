/*
 * WaveEdit by ZQ SFX
 * Copyright (C) 2025 ZQ SFX
 * Licensed under GPL v3
 *
 * Marker.cpp
 */

#include "Marker.h"

Marker::Marker()
    : m_name("Marker")
    , m_position(0)
    , m_color(juce::Colours::yellow)
{
}

Marker::Marker(const juce::String& name, int64_t position, juce::Colour color)
    : m_name(name)
    , m_position(juce::jmax((int64_t)0, position))  // Clamp to non-negative
    , m_color(color)
{
}

void Marker::setPosition(int64_t position)
{
    m_position = juce::jmax((int64_t)0, position);  // Clamp to non-negative
}

double Marker::getPositionInSeconds(double sampleRate) const
{
    if (sampleRate <= 0.0)
        return 0.0;

    return static_cast<double>(m_position) / sampleRate;
}

bool Marker::isNear(int64_t sample, int64_t tolerance) const
{
    // Safe absolute difference to avoid integer overflow
    int64_t diff = (m_position > sample) ? (m_position - sample) : (sample - m_position);
    return diff <= tolerance;
}

juce::var Marker::toJSON() const
{
    auto* obj = new juce::DynamicObject();

    obj->setProperty("name", m_name);
    obj->setProperty("position", static_cast<juce::int64>(m_position));
    obj->setProperty("color", m_color.toString());  // ARGB hex string

    return juce::var(obj);
}

Marker Marker::fromJSON(const juce::var& json)
{
    if (!json.isObject())
        return Marker();  // Return default marker on parse error

    auto* obj = json.getDynamicObject();
    if (!obj)
        return Marker();

    juce::String name = obj->getProperty("name").toString();
    int64_t position = static_cast<int64_t>(obj->getProperty("position"));
    juce::String colorStr = obj->getProperty("color").toString();
    juce::Colour color = juce::Colour::fromString(colorStr);

    return Marker(name, position, color);
}

bool Marker::operator==(const Marker& other) const
{
    return m_position == other.m_position &&
           m_name == other.m_name &&
           m_color == other.m_color;
}
