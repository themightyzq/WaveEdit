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
#include <atomic>

int64_t Region::nextId()
{
    // Process-unique, monotonic. Starts at 1 so 0 can mark "no id yet".
    static std::atomic<int64_t> counter{1};
    return counter.fetch_add(1, std::memory_order_relaxed);
}

Region::Region(const juce::String& name, int64_t startSample, int64_t endSample)
    : m_id(nextId())
    , m_name(name)
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
    : m_id(nextId())
    , m_name("Untitled Region")
    , m_startSample(0)
    , m_endSample(0)
    , m_color(juce::Colours::lightblue)
{
}

int64_t Region::getId() const
{
    return m_id;
}

void Region::setId(int64_t id)
{
    m_id = id;
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

    auto* obj = json.getDynamicObject();
    if (obj == nullptr)
    {
        // Malformed entry: leave the default (sentinel) region. Callers that
        // care about validity should reject zero-length regions on load.
        return region;
    }

    region.m_name = obj->getProperty("name").toString();

    // H22: validate that the sample fields are present and numeric. A missing
    // or wrong-typed field would otherwise silently coerce to 0, producing a
    // bogus [0,0] region. var::isInt/isInt64/isDouble lets us tell "the field
    // is a real number" from "the field is absent / a string".
    const juce::var startVar = obj->getProperty("startSample");
    const juce::var endVar   = obj->getProperty("endSample");

    auto isNumeric = [](const juce::var& v)
    {
        return v.isInt() || v.isInt64() || v.isDouble();
    };

    if (isNumeric(startVar) && isNumeric(endVar))
    {
        region.m_startSample = static_cast<int64_t>(static_cast<juce::int64>(startVar));
        region.m_endSample   = static_cast<int64_t>(static_cast<juce::int64>(endVar));

        // Apply the same invariant the constructor enforces: start <= end.
        // A sidecar with start > end must NOT bypass this and create a
        // negative-length region (which breaks getInverseRanges()).
        if (region.m_startSample > region.m_endSample)
            std::swap(region.m_startSample, region.m_endSample);
    }
    // else: fields invalid -> leave [0,0] sentinel so the caller rejects it.

    juce::String colorString = obj->getProperty("color").toString();
    juce::Colour parsedColor = juce::Colour::fromString(colorString);
    if (colorString.isNotEmpty())
        region.m_color = parsedColor;

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
