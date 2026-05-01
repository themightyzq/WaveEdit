/*
  ==============================================================================

    AutomationData.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Core data structures for plugin parameter automation.
    AutomationPoint, AutomationCurve, AutomationLane.

    Thread safety: AutomationCurve uses copy-on-write with a SpinLock
    to protect the shared_ptr swap. The SpinLock is held for only
    nanoseconds (pointer copy), making it real-time safe.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <memory>
#include <mutex>
#include <cmath>
#include <algorithm>

//==============================================================================
/**
 * A single automation point on the timeline.
 */
struct AutomationPoint
{
    double timeInSeconds = 0.0;
    float value = 0.0f;           // Normalized [0..1]

    /** Interpolation curve TO the next point. */
    enum class CurveType
    {
        Linear,       // Straight line (default)
        Step,         // Jump immediately at next point
        SCurve,       // Smooth ease-in/ease-out
        Exponential   // Accelerating curve
    };
    CurveType curve = CurveType::Linear;

    bool operator<(const AutomationPoint& other) const
    {
        return timeInSeconds < other.timeInSeconds;
    }
};

//==============================================================================
/**
 * Thread-safe automation curve for one parameter.
 *
 * Write operations (add/remove/edit points) happen on the message thread.
 * Read operations (getValueAt) happen on the audio thread.
 *
 * Thread safety: copy-on-write with SpinLock. The SpinLock is only held
 * for the duration of a shared_ptr copy (nanoseconds), so it is real-time
 * safe. Message thread builds a new sorted vector, takes the lock to swap
 * the pointer, then releases.
 */
class AutomationCurve
{
public:
    AutomationCurve()
        : m_points(std::make_shared<PointList>())
    {
    }

    // Move constructor (needed because std::mutex and SpinLock are non-movable)
    AutomationCurve(AutomationCurve&& other) noexcept
        : m_points(std::make_shared<PointList>())
    {
        const juce::SpinLock::ScopedLockType lock(other.m_ptrLock);
        m_points = other.m_points;
    }

    // Move assignment
    AutomationCurve& operator=(AutomationCurve&& other) noexcept
    {
        if (this != &other)
        {
            std::shared_ptr<const PointList> otherPoints;
            {
                const juce::SpinLock::ScopedLockType lock(other.m_ptrLock);
                otherPoints = other.m_points;
            }
            const juce::SpinLock::ScopedLockType lock(m_ptrLock);
            m_points = otherPoints;
        }
        return *this;
    }

    // Non-copyable (mutex + SpinLock)
    AutomationCurve(const AutomationCurve&) = delete;
    AutomationCurve& operator=(const AutomationCurve&) = delete;

    //==========================================================================
    // Audio thread (SpinLock-protected reads — nanosecond hold time)

    /** Get interpolated value at a given time. */
    float getValueAt(double timeInSeconds) const
    {
        // Copy shared_ptr under SpinLock (nanoseconds)
        std::shared_ptr<const PointList> points;
        {
            const juce::SpinLock::ScopedLockType lock(m_ptrLock);
            points = m_points;
        }

        if (points->empty())
            return 0.5f;

        if (timeInSeconds <= points->front().timeInSeconds)
            return points->front().value;

        if (timeInSeconds >= points->back().timeInSeconds)
            return points->back().value;

        // Binary search for surrounding points
        auto it = std::lower_bound(points->begin(), points->end(),
            timeInSeconds,
            [](const AutomationPoint& pt, double t) { return pt.timeInSeconds < t; });

        if (it == points->begin())
            return it->value;

        const auto& b = *it;
        const auto& a = *(it - 1);

        return interpolate(a, b, timeInSeconds);
    }

    /** Check if this curve has any points. */
    bool hasPoints() const
    {
        const juce::SpinLock::ScopedLockType lock(m_ptrLock);
        return !m_points->empty();
    }

    /** Get the time range covered by this curve. */
    std::pair<double, double> getTimeRange() const
    {
        std::shared_ptr<const PointList> points;
        {
            const juce::SpinLock::ScopedLockType lock(m_ptrLock);
            points = m_points;
        }
        if (points->empty())
            return { 0.0, 0.0 };
        return { points->front().timeInSeconds, points->back().timeInSeconds };
    }

    //==========================================================================
    // Message thread (write operations)

    /** Add a point. Maintains sorted order. */
    void addPoint(const AutomationPoint& point)
    {
        std::lock_guard<std::mutex> writeLock(m_writeMutex);
        auto newList = std::make_shared<PointList>(getPointsCopy());
        auto it = std::lower_bound(newList->begin(), newList->end(), point);
        newList->insert(it, point);
        publish(std::move(newList));
    }

    /** Remove the point nearest to the given time (within tolerance). */
    bool removePointAt(double timeInSeconds, double toleranceSeconds = 0.01)
    {
        std::lock_guard<std::mutex> writeLock(m_writeMutex);
        auto current = getPointsCopy();
        if (current.empty())
            return false;

        int bestIdx = -1;
        double bestDist = toleranceSeconds;
        for (int i = 0; i < static_cast<int>(current.size()); ++i)
        {
            double dist = std::abs(current[static_cast<size_t>(i)].timeInSeconds
                                   - timeInSeconds);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestIdx = i;
            }
        }

        if (bestIdx < 0)
            return false;

        auto newList = std::make_shared<PointList>(std::move(current));
        newList->erase(newList->begin() + bestIdx);
        publish(std::move(newList));
        return true;
    }

    /** Move a point to a new time/value. */
    bool movePoint(int index, double newTime, float newValue)
    {
        std::lock_guard<std::mutex> writeLock(m_writeMutex);
        auto current = getPointsCopy();
        if (index < 0 || index >= static_cast<int>(current.size()))
            return false;

        AutomationPoint pt = current[static_cast<size_t>(index)];
        pt.timeInSeconds = newTime;
        pt.value = juce::jlimit(0.0f, 1.0f, newValue);

        auto newList = std::make_shared<PointList>(std::move(current));
        newList->erase(newList->begin() + index);
        auto it = std::lower_bound(newList->begin(), newList->end(), pt);
        newList->insert(it, pt);
        publish(std::move(newList));
        return true;
    }

    /** Change the interpolation curve of a single point. Index is unchanged. */
    bool setPointCurve(int index, AutomationPoint::CurveType curve)
    {
        std::lock_guard<std::mutex> writeLock(m_writeMutex);
        auto current = getPointsCopy();
        if (index < 0 || index >= static_cast<int>(current.size()))
            return false;

        if (current[static_cast<size_t>(index)].curve == curve)
            return false;

        current[static_cast<size_t>(index)].curve = curve;
        publish(std::make_shared<PointList>(std::move(current)));
        return true;
    }

    /** Clear all points. */
    void clear()
    {
        std::lock_guard<std::mutex> writeLock(m_writeMutex);
        publish(std::make_shared<PointList>());
    }

    /** Get all points (snapshot for UI drawing). */
    std::vector<AutomationPoint> getPoints() const
    {
        return getPointsCopy();
    }

    /** Get number of points. */
    int getNumPoints() const
    {
        const juce::SpinLock::ScopedLockType lock(m_ptrLock);
        return static_cast<int>(m_points->size());
    }

    //==========================================================================
    // Serialization

    juce::var toVar() const
    {
        auto pts = getPointsCopy();
        auto arr = juce::Array<juce::var>();
        for (const auto& pt : pts)
        {
            auto obj = new juce::DynamicObject();
            obj->setProperty("time", pt.timeInSeconds);
            obj->setProperty("value", static_cast<double>(pt.value));
            obj->setProperty("curve", static_cast<int>(pt.curve));
            arr.add(juce::var(obj));
        }
        return juce::var(arr);
    }

    void fromVar(const juce::var& data)
    {
        std::lock_guard<std::mutex> writeLock(m_writeMutex);
        auto newList = std::make_shared<PointList>();

        if (auto* arr = data.getArray())
        {
            for (const auto& item : *arr)
            {
                AutomationPoint pt;
                pt.timeInSeconds = static_cast<double>(item.getProperty("time", 0.0));
                pt.value = static_cast<float>(static_cast<double>(
                    item.getProperty("value", 0.5)));
                pt.curve = static_cast<AutomationPoint::CurveType>(
                    static_cast<int>(item.getProperty("curve", 0)));
                newList->push_back(pt);
            }
            std::sort(newList->begin(), newList->end());
        }

        publish(std::move(newList));
    }

private:
    using PointList = std::vector<AutomationPoint>;

    // The shared point list — read by audio thread, swapped by message thread
    std::shared_ptr<const PointList> m_points;

    // SpinLock protects the shared_ptr read/write (nanosecond hold time)
    mutable juce::SpinLock m_ptrLock;

    // Mutex serializes write operations (message thread only)
    // NOTE: Not in m_ptrLock scope — only used by message thread.
    // Marked mutable so const methods on AutomationCurve can compile,
    // though write methods are non-const.
    mutable std::mutex m_writeMutex;

    /** Get a copy of the current point list (for message thread modifications). */
    PointList getPointsCopy() const
    {
        const juce::SpinLock::ScopedLockType lock(m_ptrLock);
        return *m_points;
    }

    /** Publish a new point list (swap under SpinLock). */
    void publish(std::shared_ptr<PointList> newList)
    {
        auto constList = std::shared_ptr<const PointList>(std::move(newList));
        const juce::SpinLock::ScopedLockType lock(m_ptrLock);
        m_points = constList;
    }

    static float interpolate(const AutomationPoint& a,
                             const AutomationPoint& b,
                             double time)
    {
        double duration = b.timeInSeconds - a.timeInSeconds;
        if (duration <= 0.0)
            return a.value;

        float t = static_cast<float>((time - a.timeInSeconds) / duration);
        t = juce::jlimit(0.0f, 1.0f, t);

        switch (a.curve)
        {
            case AutomationPoint::CurveType::Step:
                return a.value;

            case AutomationPoint::CurveType::SCurve:
            {
                float s = t * t * (3.0f - 2.0f * t);  // smoothstep
                return a.value + s * (b.value - a.value);
            }

            case AutomationPoint::CurveType::Exponential:
            {
                float e = t * t;  // quadratic ease-in
                return a.value + e * (b.value - a.value);
            }

            case AutomationPoint::CurveType::Linear:
            default:
                return a.value + t * (b.value - a.value);
        }
    }
};

//==============================================================================
/**
 * Represents automation for one parameter of one plugin in the chain.
 */
struct AutomationLane
{
    int pluginIndex = -1;
    int parameterIndex = -1;
    juce::String pluginName;
    juce::String parameterName;
    juce::String parameterID;

    AutomationCurve curve;

    bool enabled = true;
    bool isRecording = false;

    AutomationLane() = default;
    AutomationLane(AutomationLane&&) noexcept = default;
    AutomationLane& operator=(AutomationLane&&) noexcept = default;
    AutomationLane(const AutomationLane&) = delete;
    AutomationLane& operator=(const AutomationLane&) = delete;

    bool matches(int plugIdx, int paramIdx) const
    {
        return pluginIndex == plugIdx && parameterIndex == paramIdx;
    }
};
