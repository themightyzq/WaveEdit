/*
  ==============================================================================

    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 WaveEdit

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/

#include "Region.h"

Region::Region(const juce::String& name, int64_t startSample, int64_t endSample)
    : m_name(name)
    , m_startSample(startSample)
    , m_endSample(endSample)
    , m_color(juce::Colours::lightblue) // Default color
{
    // Ensure start <= end
    if (m_startSample > m_endSample)
    {
        std::swap(m_startSample, m_endSample);
    }
}

Region::Region()
    : m_name("Untitled Region")
    , m_startSample(0)
    , m_endSample(0)
    , m_color(juce::Colours::lightblue)
{
}

juce::String Region::getName() const
{
    return m_name;
}

void Region::setName(const juce::String& name)
{
    m_name = name;
}

int64_t Region::getStartSample() const
{
    return m_startSample;
}

void Region::setStartSample(int64_t sample)
{
    m_startSample = sample;

    // Maintain invariant: start <= end
    if (m_startSample > m_endSample)
    {
        m_endSample = m_startSample;
    }
}

int64_t Region::getEndSample() const
{
    return m_endSample;
}

void Region::setEndSample(int64_t sample)
{
    m_endSample = sample;

    // Maintain invariant: start <= end
    if (m_endSample < m_startSample)
    {
        m_startSample = m_endSample;
    }
}

int64_t Region::getLengthInSamples() const
{
    return m_endSample - m_startSample;
}

double Region::getLengthInSeconds(double sampleRate) const
{
    if (sampleRate <= 0.0)
        return 0.0;

    return static_cast<double>(getLengthInSamples()) / sampleRate;
}

juce::Colour Region::getColor() const
{
    return m_color;
}

void Region::setColor(const juce::Colour& color)
{
    m_color = color;
}

juce::var Region::toJSON() const
{
    auto* obj = new juce::DynamicObject();

    obj->setProperty("name", m_name);
    obj->setProperty("startSample", static_cast<juce::int64>(m_startSample));
    obj->setProperty("endSample", static_cast<juce::int64>(m_endSample));
    obj->setProperty("color", m_color.toString());

    return juce::var(obj);
}

Region Region::fromJSON(const juce::var& json)
{
    Region region;

    if (auto* obj = json.getDynamicObject())
    {
        region.m_name = obj->getProperty("name").toString();
        region.m_startSample = static_cast<int64_t>(obj->getProperty("startSample"));
        region.m_endSample = static_cast<int64_t>(obj->getProperty("endSample"));

        juce::String colorString = obj->getProperty("color").toString();
        region.m_color = juce::Colour::fromString(colorString);
    }

    return region;
}

bool Region::containsSample(int64_t sample) const
{
    return sample >= m_startSample && sample <= m_endSample;
}

bool Region::overlaps(const Region& other) const
{
    // Two regions overlap if one's start is within the other's range
    return (m_startSample >= other.m_startSample && m_startSample <= other.m_endSample) ||
           (other.m_startSample >= m_startSample && other.m_startSample <= m_endSample);
}
