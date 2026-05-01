/*
  ==============================================================================

    TimePitchEngine.cpp
    Copyright (C) 2025 ZQ SFX

  ==============================================================================
*/

#include "TimePitchEngine.h"

#include <SoundTouch.h>
#include <vector>

namespace TimePitchEngine
{

juce::AudioBuffer<float> apply(const juce::AudioBuffer<float>& input,
                                double sampleRate,
                                const Recipe& recipe)
{
    const int numChannels = input.getNumChannels();
    const int numSamples  = input.getNumSamples();

    if (numChannels <= 0 || numSamples <= 0 || sampleRate <= 0.0)
        return {};

    // Identity → return a copy (saves a SoundTouch round-trip).
    if (isIdentity(recipe))
    {
        juce::AudioBuffer<float> out(numChannels, numSamples);
        out.makeCopyOf(input, true);
        return out;
    }

    soundtouch::SoundTouch st;
    st.setSampleRate(static_cast<unsigned int>(sampleRate));
    st.setChannels(static_cast<unsigned int>(numChannels));
    st.setTempoChange(recipe.tempoPercent);
    st.setPitchSemiTones(recipe.pitchSemitones);

    // Per-channel output collectors. Conservative initial capacity of
    // numSamples * 2 covers both speed-up (smaller) and 2x slowdown.
    std::vector<std::vector<float>> outChannels(static_cast<size_t>(numChannels));
    for (auto& c : outChannels)
        c.reserve(static_cast<size_t>(numSamples) * 2);

    constexpr int kChunkFrames = 4096;
    std::vector<float> interleavedIn (static_cast<size_t>(kChunkFrames) * numChannels);
    std::vector<float> interleavedOut(static_cast<size_t>(kChunkFrames) * numChannels);

    auto drainOutput = [&]()
    {
        for (;;)
        {
            const auto received = st.receiveSamples(
                interleavedOut.data(),
                static_cast<unsigned int>(kChunkFrames));
            if (received == 0)
                break;

            for (unsigned int i = 0; i < received; ++i)
                for (int ch = 0; ch < numChannels; ++ch)
                    outChannels[static_cast<size_t>(ch)].push_back(
                        interleavedOut[i * static_cast<size_t>(numChannels)
                                          + static_cast<size_t>(ch)]);
        }
    };

    // Push the source through in chunks, draining as we go.
    int srcPos = 0;
    while (srcPos < numSamples)
    {
        const int chunk = std::min(kChunkFrames, numSamples - srcPos);

        for (int i = 0; i < chunk; ++i)
            for (int ch = 0; ch < numChannels; ++ch)
                interleavedIn[static_cast<size_t>(i) * static_cast<size_t>(numChannels)
                              + static_cast<size_t>(ch)]
                    = input.getSample(ch, srcPos + i);

        st.putSamples(interleavedIn.data(), static_cast<unsigned int>(chunk));
        drainOutput();
        srcPos += chunk;
    }

    // Flush internal state and drain whatever's left.
    st.flush();
    drainOutput();

    // Build the result buffer from the collected channel data.
    const int outNumSamples = static_cast<int>(outChannels[0].size());
    juce::AudioBuffer<float> result(numChannels, outNumSamples);
    for (int ch = 0; ch < numChannels; ++ch)
    {
        const int chFrames = std::min(outNumSamples,
                                       static_cast<int>(outChannels[static_cast<size_t>(ch)].size()));
        if (chFrames > 0)
        {
            std::memcpy(result.getWritePointer(ch),
                        outChannels[static_cast<size_t>(ch)].data(),
                        static_cast<size_t>(chFrames) * sizeof(float));
        }
    }

    return result;
}

}  // namespace TimePitchEngine
