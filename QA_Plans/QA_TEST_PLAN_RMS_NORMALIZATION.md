# QA Test Plan: RMS Normalization Feature

## Feature Overview
Added RMS (Root Mean Square) normalization mode to NormalizeDialog as an alternative to Peak normalization.
RMS normalization adjusts audio based on perceived loudness rather than absolute peaks.

## Implementation Details
- **Mode Selector**: ComboBox to choose between "Peak Level" and "RMS Level"
- **RMS Calculation**: Accurate RMS level measurement using AudioProcessor::getRMSLevelDB()
- **UI Updates**: Shows both current Peak and RMS levels
- **Settings Persistence**: Mode preference saved to settings.json ("dsp.normalizeMode")
- **Backward Compatibility**: Existing normalize operations still work, defaults to Peak mode

## Test Scenarios

### 1. UI Display and Layout
- [ ] Open Normalize dialog (Process > Normalize)
- [ ] Verify Mode selector appears above Target Level slider
- [ ] Verify both "Peak Level" and "RMS Level" options are available
- [ ] Verify current Peak level is displayed
- [ ] Verify current RMS level is displayed
- [ ] Verify Required Gain updates when mode changes
- [ ] Verify dialog height accommodates all controls without overlap

### 2. Peak Mode Normalization (Default)
- [ ] Select "Peak Level" mode
- [ ] Verify Target label shows "Target Peak Level:"
- [ ] Set target to -0.1 dB
- [ ] Click Preview - verify audio plays with peak normalization applied
- [ ] Click Apply - verify audio is normalized to target peak level
- [ ] Verify undo shows "Normalize Peak to -0.1 dB"
- [ ] Verify redo works correctly

### 3. RMS Mode Normalization
- [ ] Select "RMS Level" mode
- [ ] Verify Target label changes to "Target RMS Level:"
- [ ] Verify Required Gain recalculates based on RMS level
- [ ] Set target to -12 dB (typical RMS target)
- [ ] Click Preview - verify audio plays with RMS normalization applied
- [ ] Click Apply - verify audio is normalized to target RMS level
- [ ] Verify undo shows "Normalize RMS to -12.0 dB"
- [ ] Verify redo works correctly

### 4. Mode Switching
- [ ] Start in Peak mode, note Required Gain value
- [ ] Switch to RMS mode
- [ ] Verify Required Gain updates to reflect RMS calculation
- [ ] Switch back to Peak mode
- [ ] Verify Required Gain returns to original value

### 5. Settings Persistence
- [ ] Set mode to RMS
- [ ] Apply normalization and close dialog
- [ ] Reopen dialog - verify RMS mode is still selected
- [ ] Set mode to Peak
- [ ] Apply normalization and close dialog
- [ ] Reopen dialog - verify Peak mode is selected
- [ ] Restart application
- [ ] Open dialog - verify last selected mode is restored

### 6. Preview with Loop
- [ ] Enable "Loop Preview" toggle
- [ ] Test preview in Peak mode - verify looping works
- [ ] Test preview in RMS mode - verify looping works
- [ ] Stop preview with button - verify stops correctly

### 7. Selection vs Entire File
- [ ] Test Peak normalization on selection
- [ ] Test RMS normalization on selection
- [ ] Test Peak normalization on entire file
- [ ] Test RMS normalization on entire file

### 8. Edge Cases
- [ ] Test with very quiet audio (< -60 dB)
- [ ] Test with already normalized audio
- [ ] Test with DC offset present
- [ ] Test with mono file
- [ ] Test with stereo file
- [ ] Cancel dialog - verify no changes applied

### 9. Accuracy Verification
- [ ] Generate 1kHz sine wave at -20 dB
- [ ] RMS and Peak should be nearly identical for sine wave
- [ ] Generate white noise
- [ ] RMS should be ~3 dB lower than peak for white noise
- [ ] Generate music/speech content
- [ ] RMS should be 6-20 dB lower than peak typically

### 10. Performance
- [ ] Test with large file (1+ hour)
- [ ] Verify RMS calculation completes quickly
- [ ] Verify preview starts without delay
- [ ] Verify Apply completes in reasonable time

## Expected Results

### RMS Calculation Formula
```
RMS = sqrt(mean(x²))
RMS_dB = 20 * log10(RMS)
```

### Typical RMS vs Peak Differences
- Sine wave: RMS ≈ Peak
- Square wave: RMS = Peak
- White noise: RMS ≈ Peak - 3 dB
- Music: RMS ≈ Peak - 12 to -18 dB
- Speech: RMS ≈ Peak - 15 to -20 dB

## Known Limitations
- RMS calculation is performed on the entire selection/file (no windowing)
- Mode preference is global (not per-document)

## Regression Testing
- [ ] Gain adjustment still works
- [ ] Peak normalization unchanged for existing users
- [ ] Other DSP operations unaffected
- [ ] Undo/Redo system intact
- [ ] Preview system works for all DSP dialogs

## Build Info
- Added: AudioProcessor::getRMSLevelDB()
- Modified: NormalizeDialog UI
- Settings key: "dsp.normalizeMode" (0=Peak, 1=RMS)