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
#include <cmath>

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
        // Reset stateful DSP so filter history doesn't carry between selections.
        // (FadeProcessor is position-driven and stateless, so it needs no reset.)
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

void AudioEngine::setDynamicEQPreview(const DynamicParametricEQ::Parameters& params, bool enabled)
{
    // Thread-safe: message thread only. C1 FIX: DynamicParametricEQ builds
    // coefficients inside setParameters() under its own lock (which its
    // applyEQ() try-locks), so the audio thread never allocates.
    if (m_dynamicEQ)
        m_dynamicEQ->setParameters(params);
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
    // Thread-safe: can be called from the message thread. The FadeProcessor is
    // position-driven, so there is no counter to reset -- the audio callback
    // derives the fade position from the transport each block.

    m_fadeProcessor.fadeIn.store(fadeIn);
    m_fadeProcessor.curveType.store(juce::jlimit(0, 3, curveType)); // 0=Lin,1=Log,2=Exp,3=S-Curve

    // Fade span (file samples) = duration converted at the file sample rate.
    const double sampleRate = m_sampleRate.load();
    if (sampleRate > 0.0)
        m_fadeProcessor.fadeDurationSamples.store((durationMs / 1000.0f) * static_cast<float>(sampleRate));

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
    m_dcOffsetProcessor.reset();
    // Note: GainProcessor, NormalizeProcessor and the (position-driven)
    // FadeProcessor are stateless and need no reset.
}

void AudioEngine::disableAllPreviewProcessors()
{
    // Thread-safe: Uses atomic stores
    // Disables all preview processors for clean state on dialog close
    m_gainProcessor.enabled.store(false);
    m_normalizeProcessor.enabled.store(false);
    m_fadeProcessor.enabled.store(false);
    m_dcOffsetProcessor.enabled.store(false);
    m_dynamicEQEnabled.set(false);
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
    info.eqActive = m_dynamicEQEnabled.get();
    return info;
}

void AudioEngine::setPreviewPluginInstance(juce::AudioPluginInstance* instance)
{
    // C-H1 FIX: take m_previewPluginLock (blocking, message thread) around the
    // store. The audio callback try-locks the same lock around its
    // load+processBlock, so this setter cannot complete while the callback is
    // mid-dereference. After this returns, the audio thread is guaranteed not to
    // be inside the guarded region and will observe the new pointer next block --
    // so a caller that stores nullptr here may then safely destroy the instance
    // (OfflinePluginDialog::disablePreview does exactly this before freeing its
    // unique_ptr). A plain atomic store is NOT a barrier and would race the
    // in-flight callback.
    const juce::ScopedLock sl(m_previewPluginLock);
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

//==============================================================================
// Shared preview helpers (pure) + session control
//==============================================================================

AudioEngine::PreviewRegion AudioEngine::computePreviewRegion(int64_t selectionStart,
                                                             int64_t selectionEnd,
                                                             double sampleRate,
                                                             int64_t totalSamples,
                                                             bool loop) noexcept
{
    PreviewRegion region;

    if (sampleRate <= 0.0 || totalSamples <= 0)
        return region; // invalid: nothing to preview

    // Clamp the selection into [0, totalSamples].
    int64_t start = juce::jlimit<int64_t>(0, totalSamples, selectionStart);
    int64_t end   = juce::jlimit<int64_t>(0, totalSamples, selectionEnd);

    // Empty or inverted selection => preview the whole buffer.
    if (end <= start)
    {
        start = 0;
        end   = totalSamples;
    }

    region.valid    = true;
    region.startSec = static_cast<double>(start) / sampleRate;
    region.endSec   = static_cast<double>(end)   / sampleRate;
    region.loop     = loop;
    return region;
}

float AudioEngine::fadeGainAt(double posInSpanSamples, double fadeDurationSamples,
                              bool fadeIn, int curveType) noexcept
{
    if (fadeDurationSamples <= 0.0)
        return 1.0f; // instantaneous fade -> unity

    const float progress = static_cast<float>(
        juce::jlimit(0.0, 1.0, posInSpanSamples / fadeDurationSamples));

    float gain = progress; // linear (curveType 0)
    switch (curveType)
    {
        case 1: gain = std::log10(1.0f + progress * 9.0f); break;                       // logarithmic
        case 2: gain = (std::exp(progress * 2.0f) - 1.0f) / (std::exp(2.0f) - 1.0f); break; // exponential
        case 3: gain = 0.5f * (1.0f + std::tanh(6.0f * (progress - 0.5f))); break;       // S-curve
        default: break;
    }

    return fadeIn ? gain : (1.0f - gain);
}

void AudioEngine::startSelectionPreview(int64_t selectionStart, int64_t selectionEnd, bool loop)
{
    const double sr = m_sampleRate.load();
    const auto total = static_cast<int64_t>(std::llround(getTotalLength() * sr)); // getTotalLength() is seconds

    const PreviewRegion region = computePreviewRegion(selectionStart, selectionEnd, sr, total, loop);
    if (!region.valid)
        return;

    // Cursor/visualisation offset + the fade's position reference (file seconds).
    setPreviewSelectionOffset(static_cast<int64_t>(std::llround(region.startSec * sr)));
    m_previewRegionStartSec.store(region.startSec);

    clearLoopPoints();
    setLooping(region.loop);
    if (region.loop)
        setLoopPoints(region.startSec, region.endSec);

    setPosition(region.startSec);
    play();
}

bool AudioEngine::startBufferPreview(const juce::AudioBuffer<float>& buffer, double sampleRate,
                                     int numChannels, bool loop, double fileOffsetSeconds)
{
    if (buffer.getNumSamples() <= 0 || numChannels <= 0 || sampleRate <= 0.0)
        return false;

    setPreviewMode(PreviewMode::OFFLINE_BUFFER);
    if (!loadPreviewBuffer(buffer, sampleRate, numChannels))
        return false;

    const double durationSec = buffer.getNumSamples() / sampleRate;
    // Offline buffer plays from 0; map its playback position back onto the
    // source timeline so the on-screen playhead lands at the selection, not
    // the file start (getCurrentPosition() adds this for OFFLINE_BUFFER).
    m_previewRegionStartSec.store(fileOffsetSeconds);

    clearLoopPoints();
    setLooping(loop);
    if (loop)
        setLoopPoints(0.0, durationSec);

    setPosition(0.0);
    play();
    return true;
}

void AudioEngine::stopSelectionPreview()
{
    // Order matters: stop() also clears loop state, so clear/disable afterwards.
    stop();
    clearLoopPoints();
    setLooping(false);

    // Disable every preview effect so the next preview starts from a clean slate.
    setGainPreview(0.0f, false);
    setNormalizePreview(0.0f, false);
    setFadePreview(true, 0, 0.0f, false);
    setDCOffsetPreview(false);
    setDynamicEQPreview(DynamicParametricEQ::Parameters{}, false);

    setPreviewBypassed(false);
    setPreviewMode(PreviewMode::DISABLED);
    m_previewRegionStartSec.store(0.0);
}
