/*
  ==============================================================================

    AudioEngine_Preview.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Split out from AudioEngine.cpp under CLAUDE.md §7.5 (file-size cap).
    Hosts the preview-system surface — entering / leaving preview mode,
    loading the preview buffer, atomic param-exchange setters for each
    processor (gain / normalize / fade / DC offset / parametric EQ /
    dynamic EQ), the bypass + reset + disable helpers, and the
    selection-offset accounting used by previewing dialogs to map the
    file timeline back into the preview buffer.

  ==============================================================================
*/

#include "AudioEngine.h"

//==============================================================================
// Preview System Implementation

void AudioEngine::setPreviewMode(PreviewMode mode)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    PreviewMode oldMode = m_previewMode.load();

    m_previewMode.store(mode);

    // CRITICAL FIX: Track which AudioEngine is in preview mode
    // This allows other engines to auto-mute themselves during preview
    if (mode != PreviewMode::DISABLED && oldMode == PreviewMode::DISABLED)
    {
        // P2 FIX: Reset DSP processor state for clean preview
        // Prevents filter state carryover between different selections
        m_fadeProcessor.reset();
        m_dcOffsetProcessor.reset();

        // Entering preview mode - register this engine as the previewing one
        s_previewingEngine.store(this);
    }
    else if (mode == PreviewMode::DISABLED && oldMode != PreviewMode::DISABLED)
    {
        // Leaving preview mode - clear the previewing engine
        // Unconditionally clear if we are the current previewing engine
        AudioEngine* current = s_previewingEngine.load();
        if (current == this)
        {
            s_previewingEngine.store(nullptr);
        }
    }

    // If switching from OFFLINE_BUFFER to DISABLED, restore the original audio source
    if (oldMode == PreviewMode::OFFLINE_BUFFER && mode == PreviewMode::DISABLED)
    {

        // Save current position before disconnecting transport (in case user wants to resume)
        double savedPosition = getCurrentPosition();

        // Stop playback to safely disconnect transport
        if (isPlaying())
        {
            m_transportSource.stop();
        }

        // Disconnect preview buffer
        m_transportSource.setSource(nullptr);

        // Reconnect to the original audio source (file or buffer)
        if (m_bufferSource && m_isPlayingFromBuffer.load())
        {
            // Playing from edited buffer
            m_transportSource.setSource(m_bufferSource.get(), 0, nullptr,
                                       m_sampleRate.load(), m_numChannels.load());
        }
        else if (m_readerSource)
        {
            // Playing from original file
            m_transportSource.setSource(m_readerSource.get(), 0, nullptr,
                                       m_sampleRate.load(), m_numChannels.load());
        }

        // CRITICAL: Clear preview-related state (defense in depth)
        // GainDialog should have already cleared these, but ensure clean state
        clearLoopPoints();
        setLooping(false);
        m_previewSelectionStartSamples.store(0);  // Clear preview selection offset

        // Restore position (user may have been previewing in middle of file)
        m_transportSource.setPosition(savedPosition);

        // DO NOT auto-restart playback - user explicitly stopped preview
        // by closing/canceling dialog. Respect user intent.
    }
}

PreviewMode AudioEngine::getPreviewMode() const
{
    return m_previewMode.load();
}

bool AudioEngine::loadPreviewBuffer(const juce::AudioBuffer<float>& previewBuffer, double sampleRate, int numChannels)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (previewBuffer.getNumSamples() == 0 || previewBuffer.getNumChannels() == 0)
    {
        return false;
    }

    // CRITICAL: Stop any playback FIRST to clear audio device buffers
    // Without this, the audio device continues playing buffered samples from the old source
    bool wasPlaying = m_transportSource.isPlaying();
    if (wasPlaying)
    {
        m_transportSource.stop();
    }

    // CRITICAL: Release resources before switching sources
    // This flushes any cached audio data from the old source
    m_transportSource.releaseResources();

    // Disconnect transport to flush internal buffers
    m_transportSource.setSource(nullptr);

    // Load preview buffer
    m_previewBufferSource->setBuffer(previewBuffer, sampleRate, false);

    // Connect preview buffer as transport source
    m_transportSource.setSource(m_previewBufferSource.get(), 0, nullptr, sampleRate, numChannels);

    // CRITICAL: Call prepareToPlay() after changing the source
    // This is REQUIRED by JUCE's AudioTransportSource - without it, the transport
    // continues reading from the old source despite the setSource() call!
    auto* device = m_deviceManager.getCurrentAudioDevice();
    if (device != nullptr)
    {
        m_transportSource.prepareToPlay(device->getCurrentBufferSizeSamples(),
                                         device->getCurrentSampleRate());
    }

    // Apply current loop state to preview buffer source
    m_previewBufferSource->setLooping(m_isLooping.load());

    // Update audio properties (but NOT m_isPlayingFromBuffer!)
    // CRITICAL: Do NOT set m_isPlayingFromBuffer here! That flag indicates whether
    // the MAIN audio source is buffer-based or file-based. Preview is temporary.
    // If we set it to true here, setPreviewMode(DISABLED) will try to restore
    // m_bufferSource, which may be empty if the main file was loaded via loadAudioFile().
    m_sampleRate.store(sampleRate);
    m_numChannels.store(numChannels);

    // NOTE: Don't restore position/playback state - let caller control this.
    // Preview buffers are different content than main buffer, so restoring
    // the old position (e.g., 5.0 seconds into main file) would skip past
    // the intended preview audio. Caller will set position explicitly.

    return true;
}

void AudioEngine::setGainPreview(float gainDB, bool enabled)
{
    // Thread-safe: Can be called from UI thread
    // Atomic operations ensure safe updates from message thread while audio thread reads

    m_gainProcessor.gainDB.store(gainDB);
    m_gainProcessor.enabled.store(enabled);
}

void AudioEngine::setParametricEQPreview(const ParametricEQ::Parameters& params, bool enabled)
{
    // Thread-safe: Can be called from UI thread
    // Use atomic flag to safely exchange parameters between threads
    m_pendingParametricEQParams = params;
    m_parametricEQParamsChanged.set(true);
    m_parametricEQEnabled.set(enabled);
}

void AudioEngine::setDynamicEQPreview(const DynamicParametricEQ::Parameters& params, bool enabled)
{
    // Thread-safe: Can be called from UI thread
    // Use atomic flag to safely exchange parameters between threads
    m_pendingDynamicEQParams = params;
    m_dynamicEQParamsChanged.set(true);
    m_dynamicEQEnabled.set(enabled);
}

void AudioEngine::setNormalizePreview(float gainDB, bool enabled)
{
    // Thread-safe: Can be called from UI thread
    m_normalizeProcessor.gainDB.store(gainDB);
    m_normalizeProcessor.enabled.store(enabled);
}

void AudioEngine::setFadePreview(bool fadeIn, int curveType, float durationMs, bool enabled)
{
    // Thread-safe: Can be called from UI thread

    // Set fade type
    m_fadeProcessor.fadeType.store(fadeIn ?
        FadeProcessor::FadeType::FADE_IN :
        FadeProcessor::FadeType::FADE_OUT);

    // Set curve type (validate range)
    FadeProcessor::CurveType curve = FadeProcessor::CurveType::LINEAR;
    switch (curveType)
    {
        case 0: curve = FadeProcessor::CurveType::LINEAR; break;
        case 1: curve = FadeProcessor::CurveType::LOGARITHMIC; break;
        case 2: curve = FadeProcessor::CurveType::EXPONENTIAL; break;
        case 3: curve = FadeProcessor::CurveType::S_CURVE; break;
        default: curve = FadeProcessor::CurveType::LINEAR; break;
    }
    m_fadeProcessor.curveType.store(curve);

    // Convert duration from ms to samples
    double sampleRate = m_sampleRate.load();
    if (sampleRate > 0)
    {
        float durationSamples = (durationMs / 1000.0f) * static_cast<float>(sampleRate);
        m_fadeProcessor.fadeDurationSamples.store(durationSamples);
    }

    // Reset sample counter for fresh fade
    if (!m_fadeProcessor.enabled.load() && enabled)
    {
        m_fadeProcessor.reset();
    }

    m_fadeProcessor.enabled.store(enabled);
}

void AudioEngine::setDCOffsetPreview(bool enabled)
{
    // Thread-safe: Can be called from UI thread

    // Reset DC blockers when enabling to avoid clicks
    if (!m_dcOffsetProcessor.enabled.load() && enabled)
    {
        m_dcOffsetProcessor.reset();
    }

    m_dcOffsetProcessor.enabled.store(enabled);
}

//==============================================================================
// Preview Processor Unified API (Phase 2)

void AudioEngine::resetAllPreviewProcessors()
{
    // Thread-safe: Can be called from message thread
    // Resets processors with internal state (filter history, etc.)
    m_fadeProcessor.reset();
    m_dcOffsetProcessor.reset();
    // Note: GainProcessor and NormalizeProcessor are stateless (pure gain)
    // Note: ParametricEQ reset is handled by setParametricEQBands when reconfigured
}

void AudioEngine::disableAllPreviewProcessors()
{
    // Thread-safe: Uses atomic stores
    // Disables all preview processors for clean state on dialog close
    m_gainProcessor.enabled.store(false);
    m_normalizeProcessor.enabled.store(false);
    m_fadeProcessor.enabled.store(false);
    m_dcOffsetProcessor.enabled.store(false);
    m_parametricEQEnabled.set(false);
}

void AudioEngine::setPreviewBypassed(bool bypassed)
{
    // Thread-safe: Uses atomic store
    // When bypassed, all preview DSP processing is skipped for A/B comparison
    m_previewBypassed.store(bypassed);
}

bool AudioEngine::isPreviewBypassed() const
{
    // Thread-safe: Uses atomic load
    return m_previewBypassed.load();
}

AudioEngine::PreviewProcessorInfo AudioEngine::getPreviewProcessorInfo() const
{
    // Thread-safe: Uses atomic loads
    PreviewProcessorInfo info;
    info.gainActive = m_gainProcessor.enabled.load();
    info.normalizeActive = m_normalizeProcessor.enabled.load();
    info.fadeActive = m_fadeProcessor.enabled.load();
    info.dcOffsetActive = m_dcOffsetProcessor.enabled.load();
    info.eqActive = m_parametricEQEnabled.get();
    return info;
}

void AudioEngine::setPreviewPluginInstance(juce::AudioPluginInstance* instance)
{
    // Thread-safe: Can be called from UI thread
    // The atomic pointer ensures safe exchange between threads
    m_previewPluginInstance.store(instance);
}

void AudioEngine::setPreviewSelectionOffset(int64_t selectionStartSamples)
{
    // Validate offset is non-negative
    if (selectionStartSamples < 0)
    {
        jassert(false);  // Debug assertion for negative offset
        selectionStartSamples = 0;
    }

    // Thread-safe: Uses atomic store
    m_previewSelectionStartSamples.store(selectionStartSamples);
}

double AudioEngine::getPreviewSelectionOffsetSeconds() const
{
    // Only return offset if in preview mode (prevents race conditions)
    if (m_previewMode.load() == PreviewMode::DISABLED)
    {
        return 0.0;
    }

    // Thread-safe: Uses atomic load and current sample rate
    int64_t offsetSamples = m_previewSelectionStartSamples.load();
    double currentSampleRate = m_sampleRate.load();

    // Guard against invalid sample rates
    if (currentSampleRate <= 0.0)
    {
        return 0.0;
    }

    return static_cast<double>(offsetSamples) / currentSampleRate;
}
