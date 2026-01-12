# Multichannel Support Discovery Report

**Date**: 2026-01-12
**Phase**: A - Discovery
**Author**: Claude Code

---

## Executive Summary

WaveEdit has **strong multichannel foundations** already in place. The codebase largely uses dynamic channel counts via `getNumChannels()` loops rather than hardcoded stereo assumptions. Key areas requiring work are channel labeling, conversion utilities, and WAVEFORMATEXTENSIBLE support for >2ch WAV files.

---

## 1. Current Multichannel Support (Already Working)

### 1.1 Audio I/O
- **File loading**: `AudioFileManager.cpp:111-113` validates 1-8 channels
- **File saving**: Uses `buffer.getNumChannels()` dynamically
- **Buffer creation**: `AudioBufferManager.cpp:55` - `setSize(numChannels, numSamples)` from reader

### 1.2 DSP Processing
- **AudioProcessor.cpp**: All 15+ DSP operations use `for (ch = 0; ch < buffer.getNumChannels(); ++ch)` pattern
- Operations verified multichannel-safe:
  - Gain, Normalize, DC Offset, Fade In/Out
  - Peak/RMS analysis
  - Insert/delete operations

### 1.3 Waveform Display
- **WaveformDisplay.cpp:1381-1393**: Already handles N channels
  ```cpp
  int channelHeight = bounds.getHeight() / m_numChannels;
  for (int ch = 0; ch < m_numChannels; ++ch)
  {
      auto channelBounds = bounds.removeFromTop(channelHeight);
      drawChannelWaveform(g, channelBounds, ch);
  }
  ```

### 1.4 Audio Engine
- **AudioEngine.h/cpp**: Uses atomic `m_numChannels` and handles N channels throughout
- **Transport source**: Properly configured with channel count

### 1.5 Clipboard/Paste
- **DocumentManager.cpp:391-407**: General case handles mismatched channel counts by copying available channels and zero-padding

---

## 2. Areas Requiring Changes

### 2.1 High Priority - Channel Labels

**Location**: `WaveformDisplay.cpp:1839-1846`
```cpp
// Current: Only labels stereo as "L"/"R"
if (m_numChannels == 2)
{
    juce::String label = (channelNum == 0) ? "L" : "R";
    // ...
}
```

**Required Change**: Add labels for all channel counts:
- Mono: "M" or no label
- Stereo: "L", "R"
- 5.1: "L", "R", "C", "LFE", "Ls", "Rs"
- 7.1: Add "Lrs", "Rrs"
- Generic: "Ch 1", "Ch 2", etc. for non-standard layouts

### 2.2 High Priority - Channel Conversion Utilities

**Location**: `AudioBufferManager.cpp:364-438`

| Function | Current Behavior | Required Change |
|----------|------------------|-----------------|
| `convertToStereo()` | Only works with mono | Generalize to N→2 (take first 2 or mix) |
| `convertToMono()` | Only works with stereo | Generalize to N→1 (mix all channels) |

**New Functions Needed**:
- `convertChannelCount(int targetChannels, ChannelLayout layout)`
- `remapChannels(const ChannelMapping& mapping)`

### 2.3 Medium Priority - WAVEFORMATEXTENSIBLE

**Location**: `AudioFileManager.cpp` save functions

**Issue**: WAV files >2 channels require WAVEFORMATEXTENSIBLE format with `dwChannelMask` to specify speaker positions. Current code uses basic WAV format.

**JUCE Support**: Check if JUCE's WavAudioFormat handles this automatically or if we need to pass channel mask metadata.

### 2.4 Medium Priority - FilePropertiesDialog

**Location**: `FilePropertiesDialog.cpp:359`
```cpp
else if (numChannels == 2)
    // Shows "Stereo"
```

**Required Change**: Show proper layout names for all channel counts:
- 1: "Mono"
- 2: "Stereo"
- 6: "5.1 Surround"
- 8: "7.1 Surround"
- Other: "N channels"

### 2.5 Low Priority - RecordingEngine Default

**Location**: `RecordingEngine.cpp:24`
```cpp
m_numChannels(2),  // Default to stereo
```

**Note**: This is fine - it's updated dynamically when recording starts. No change needed.

---

## 3. Impact Assessment

| Component | Current State | Effort to Fix |
|-----------|---------------|---------------|
| File I/O | N-channel ready | Low (add WAVEFORMATEXTENSIBLE) |
| DSP Operations | N-channel safe | None |
| Waveform Display | N-channel rendering works | Low (add labels) |
| Audio Engine | N-channel ready | None |
| Clipboard | Works with N channels | None |
| Channel Conversion | Mono/Stereo only | Medium |
| Properties Dialog | Stereo labels only | Low |

---

## 4. Recommended Implementation Order

### Phase B - Research & Spec
1. Document Film vs SMPTE channel ordering
2. Define `ChannelLayout` enum and mappings
3. Research JUCE WAVEFORMATEXTENSIBLE support

### Phase C - Multichannel Foundation
1. Add channel labels in WaveformDisplay (1-2 hours)
2. Update FilePropertiesDialog (30 min)
3. Test with existing 5.1/7.1 files

### Phase D - Channel Converter
1. Generalize `convertToMono()` for N channels
2. Generalize `convertToStereo()` for N channels
3. Add Channel Converter dialog with presets
4. Implement channel remapping matrix

### Phase E - Testing
1. Unit tests for channel conversions
2. Roundtrip I/O tests for multichannel WAV
3. Manual QA with various channel layouts

---

## 5. Files to Modify

| File | Changes |
|------|---------|
| `Source/UI/WaveformDisplay.cpp` | Add N-channel labels |
| `Source/UI/FilePropertiesDialog.cpp` | Add layout name display |
| `Source/Audio/AudioBufferManager.cpp` | Generalize conversion functions |
| `Source/Audio/AudioFileManager.cpp` | Add WAVEFORMATEXTENSIBLE support |
| `Source/UI/ChannelConverterDialog.cpp` | New file - converter UI |
| `Source/Audio/ChannelLayout.h` | New file - layout definitions |

---

## 6. Conclusion

The codebase is **well-prepared for multichannel support**. The primary work is:
1. **UI Polish**: Channel labels and layout names
2. **New Feature**: Channel Converter dialog
3. **Format Support**: WAVEFORMATEXTENSIBLE for >2ch WAV

Estimated total effort: **Medium** (most infrastructure already exists)

---

*Discovery Phase Complete - Ready for Phase B (Research & Spec)*
