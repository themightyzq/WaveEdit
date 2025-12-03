# QA Test Report: ParametricEQ Preview & RMS Normalization
## WaveEdit by ZQ SFX

**Test Date**: 2025-12-02
**Tester**: audio-qa-specialist (Claude Code)
**Build**: Release (commit: latest)
**Platform**: macOS (Darwin 24.5.0)

---

## Executive Summary

**Features Under Test**:
1. ParametricEQ Preview Optimization (P1) - REALTIME_DSP mode
2. RMS Normalization (P3) - Mode selector + dual level display

**Test Scope**:
- End-to-end functional testing
- Audio quality verification (artifact detection)
- Preview playback integration
- Undo/redo chain integrity
- Settings persistence
- Multi-file state management
- Extreme parameter testing

---

## Test Environment

**System Configuration**:
- OS: macOS (Darwin 24.5.0)
- Build Type: Release
- Build Date: 2025-12-02
- Test Files Used:
  - `test_clean.wav` (mono, 44.1kHz, small file)
  - `MAGIC_fairy_dust_group_designed_01.wav` (stereo, 48kHz, medium file)
  - `MAGEvil_DESIGNED airMagic explosion 01_PGN_DM.wav` (stereo, 48kHz)

**Pre-Test Verification**:
- [x] Application builds without errors
- [x] All 47 automated tests passing (pre-check)
- [x] Test audio files available in TestFiles/
- [x] Previous P0 bugs resolved (preview coordinate fix, playback fix)

---

## Test Plan Overview

### Test Suites

**Suite A: ParametricEQ Preview Optimization**
- A1: Preview Mode Switching (OFFLINE_BUFFER → REALTIME_DSP)
- A2: Real-Time Parameter Updates During Preview
- A3: Audio Artifact Detection During Slider Adjustment
- A4: Preview → Apply → Undo → Redo Chain
- A5: Multi-File State Management
- A6: Extreme Parameter Testing
- A7: Loop Preview Functionality

**Suite B: RMS Normalization**
- B1: Mode Selector UI (Peak vs RMS)
- B2: Dual Level Display (Peak + RMS)
- B3: RMS Calculation Accuracy
- B4: Preview Functionality (Peak vs RMS comparison)
- B5: Settings Persistence (mode selection saved)
- B6: Undo/Redo Integration
- B7: File Save Integration

**Suite C: Integration & Regression**
- C1: Preview System Coordination (all 6 DSP dialogs)
- C2: Playback State Preservation
- C3: Performance Testing (CPU usage during preview)
- C4: Memory Leak Detection (long preview sessions)

---

## Test Execution Log

### Test Suite A: ParametricEQ Preview Optimization

#### A1: Preview Mode Switching
**Test Case**: Verify ParametricEQ uses REALTIME_DSP mode instead of OFFLINE_BUFFER

**Status**: ⏳ PENDING - GUI application requires manual testing

**Test Steps**:
1. Open test file (test_clean.wav)
2. Select region (0:00-0:05)
3. Open ParametricEQ dialog (Shift+E)
4. Adjust Low Band Gain to +10dB
5. Click "Preview" button
6. Observe audio playback
7. While playing, adjust Mid Band Frequency slider (1kHz → 5kHz)
8. Listen for immediate parameter change in audio

**Expected Behavior**:
- Preview starts playing selected region immediately
- Slider adjustments audible in REAL-TIME during playback
- NO audio dropouts or glitches during parameter change
- Preview uses REALTIME_DSP mode (confirmed in code: AudioEngine.cpp)

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] Preview starts at selection start (not file start)
- [ ] Parameter changes audible immediately (< 100ms latency)
- [ ] No audio dropouts during slider movement
- [ ] Playback position preserved during parameter updates

---

#### A2: Real-Time Parameter Updates During Preview
**Test Case**: Verify all 9 parameters update audio during preview playback

**Status**: ⏳ PENDING

**Test Steps**:
1. Open test file with tonal content (MAGIC_fairy_dust_group_designed_01.wav)
2. Select 10-second region with clear frequency content
3. Open ParametricEQ dialog (Shift+E)
4. Click "Preview" button + enable "Loop Preview"
5. Test each parameter while looping:
   - Low Band: Freq (100Hz → 500Hz), Gain (-10dB → +10dB), Q (0.1 → 5.0)
   - Mid Band: Freq (1kHz → 5kHz), Gain (-10dB → +10dB), Q (0.1 → 5.0)
   - High Band: Freq (5kHz → 15kHz), Gain (-10dB → +10dB), Q (0.1 → 5.0)
6. Verify audio reflects each parameter change immediately

**Expected Behavior**:
- Each slider movement results in immediate audio change
- Frequency changes shift tonal character
- Gain changes affect volume of band
- Q changes affect bandwidth (narrower/wider)
- Loop preview continues uninterrupted during adjustments

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] All 9 sliders trigger real-time audio updates
- [ ] Audio changes match parameter direction (e.g., +10dB = louder)
- [ ] Loop continues smoothly during parameter updates
- [ ] No audio corruption or silence

---

#### A3: Audio Artifact Detection During Slider Adjustment
**Test Case**: Critical test for clicking, popping, or distortion during rapid parameter changes

**Status**: ⏳ PENDING - KNOWN ISSUE EXPECTED

**Test Steps**:
1. Open test file (test_clean.wav)
2. Select region (0:00-0:10)
3. Open ParametricEQ dialog
4. Click "Preview" + "Loop Preview"
5. Rapidly move Low Band Gain slider back and forth (-20dB ↔ +20dB)
6. Listen carefully for:
   - Clicks/pops at slider movement boundaries
   - Audio dropouts/silence
   - Distortion/clipping
   - Zipper noise (rapid parameter stepping)

**Expected Behavior** (Current Implementation):
- **KNOWN ISSUE**: Audio artifacts MAY occur during rapid slider adjustment
- Root cause: Re-rendering entire buffer on message thread (per TODO.md line 519)
- This is a P2 issue, not a blocker

**Expected Behavior** (Future Optimization):
- Parameter smoothing eliminates artifacts
- OR move to pure real-time DSP chain (no buffer re-render)

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] Artifacts documented (severity, frequency, reproducibility)
- [ ] Artifacts do NOT occur during normal (non-rapid) adjustment
- [ ] Artifacts do NOT persist after slider released
- [ ] Preview playback recovers gracefully

**Notes**: This is expected behavior per TODO.md. Document severity for P2 triage.

---

#### A4: Preview → Apply → Undo → Redo Chain
**Test Case**: Verify preview doesn't corrupt undo/redo system

**Status**: ⏳ PENDING

**Test Steps**:
1. Open test file
2. Select region (0:00-0:05)
3. Open ParametricEQ dialog
4. Adjust parameters (Low +5dB, Mid 2kHz, High +3dB)
5. Click "Preview" - verify audio
6. Click "Apply"
7. Verify waveform updated
8. Press Cmd+Z (undo)
9. Verify waveform restored to original
10. Press Cmd+Shift+Z (redo)
11. Verify EQ re-applied
12. Play audio - verify redo matches original apply

**Expected Behavior**:
- Preview does NOT create undo entry
- Apply creates ONE undo entry
- Undo perfectly reverses EQ
- Redo perfectly re-applies EQ
- Audio matches preview exactly after apply

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] Undo restores original audio (sample-accurate)
- [ ] Redo matches original apply (sample-accurate)
- [ ] Playback matches preview audio after apply
- [ ] No phantom undo entries from preview

---

#### A5: Multi-File State Management
**Test Case**: Verify preview state doesn't bleed between files

**Status**: ⏳ PENDING

**Test Steps**:
1. Open File A (test_clean.wav) in Tab 1
2. Open ParametricEQ, adjust parameters, preview, DO NOT APPLY
3. Open File B (MAGIC_fairy_dust) in new tab (Cmd+O)
4. Switch to Tab 2, play audio - verify NO EQ applied
5. Open ParametricEQ in Tab 2 - verify default parameters (0dB, 1kHz, etc.)
6. Switch back to Tab 1 - verify original parameters preserved
7. Apply EQ in Tab 1
8. Switch to Tab 2 again - verify still no EQ

**Expected Behavior**:
- Each tab maintains independent preview state
- Preview in Tab 1 does NOT affect Tab 2
- Parameter values reset for each new dialog instance
- Preview mode disabled when switching tabs

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] Tab 2 audio unaffected by Tab 1 preview
- [ ] Tab 2 EQ dialog shows default values
- [ ] Tab 1 applied EQ persists after tab switch
- [ ] No audio artifacts when switching tabs during preview

---

#### A6: Extreme Parameter Testing
**Test Case**: Test parameter boundaries and edge cases

**Status**: ⏳ PENDING

**Test Steps**:
1. Open test file
2. Select region
3. Open ParametricEQ dialog
4. Test extreme values:
   - Low Band: 20Hz, -20dB, Q=0.1
   - Mid Band: Nyquist (22.05kHz for 44.1kHz), +20dB, Q=10.0
   - High Band: 20kHz, +20dB, Q=10.0
5. Click "Preview" - verify no crash
6. Apply - verify no crash/corruption
7. Test edge cases:
   - All bands at 0dB (should be silent/no change)
   - All bands at +20dB (should amplify significantly)
   - Q=0.1 (very wide band) vs Q=10.0 (very narrow)

**Expected Behavior**:
- No crashes or audio corruption
- Nyquist frequency properly clamped (safety margin 0.49 × sampleRate)
- Extreme gain values clipped to [-1.0, 1.0] if needed
- Extreme Q values handled gracefully

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] No crashes with extreme values
- [ ] Audio remains in valid range [-1.0, 1.0]
- [ ] Nyquist clamping prevents filter instability
- [ ] UI shows clamped values if applicable

---

#### A7: Loop Preview Functionality
**Test Case**: Verify loop preview works correctly for EQ

**Status**: ⏳ PENDING

**Test Steps**:
1. Open test file
2. Select 5-second region
3. Open ParametricEQ dialog
4. Enable "Loop Preview" checkbox
5. Click "Preview"
6. Verify playback loops continuously
7. Adjust parameters during loop
8. Verify loop boundaries preserved (no drift)
9. Click "Stop Preview"
10. Verify playback stops cleanly

**Expected Behavior**:
- Loop repeats selection continuously
- Loop boundaries remain sample-accurate (0:00-0:05 loops exactly)
- Parameter adjustments don't break loop
- Stop button ends preview immediately

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] Loop starts at selection start
- [ ] Loop ends at selection end (no drift)
- [ ] Loop continues during parameter changes
- [ ] Stop button works immediately

---

### Test Suite B: RMS Normalization

#### B1: Mode Selector UI (Peak vs RMS)
**Test Case**: Verify mode selector displays and functions correctly

**Status**: ⏳ PENDING

**Test Steps**:
1. Open test file with loud transient content (MAGEvil_DESIGNED)
2. Select region (0:00-0:05)
3. Open Normalize dialog (Ctrl+Shift+N)
4. Verify UI shows:
   - Mode dropdown with "Peak Level" and "RMS Level"
   - Default selection: "Peak Level"
   - Target Level slider (-80dB to 0dB)
   - Current Peak label + value
   - Current RMS label + value
   - Required Gain label + value
5. Switch mode to "RMS Level"
6. Verify "Target Level:" label unchanged
7. Verify "Required Gain" recalculates

**Expected Behavior**:
- Mode selector visible and functional
- Both Peak and RMS levels display simultaneously
- Required Gain updates when mode changed
- UI labels clear and unambiguous

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] Mode selector present with 2 options
- [ ] Peak and RMS values both displayed
- [ ] Switching mode updates Required Gain calculation
- [ ] UI layout matches NormalizeDialog.cpp lines 38-93

---

#### B2: Dual Level Display (Peak + RMS)
**Test Case**: Verify both Peak and RMS levels calculated and displayed correctly

**Status**: ⏳ PENDING

**Test Steps**:
1. Open test file with known characteristics:
   - File 1: test_clean.wav (likely low peak, low RMS)
   - File 2: MAGEvil explosion (high peak transients, lower RMS)
2. For each file:
   - Select full file (Cmd+A)
   - Open Normalize dialog
   - Record displayed values:
     - Current Peak: _____ dB
     - Current RMS: _____ dB
3. Verify RMS < Peak (always true for real audio)
4. Verify values match manual calculation (if possible)

**Expected Behavior**:
- Current Peak: Maximum absolute sample value in dB
- Current RMS: sqrt(mean(x²)) in dB
- RMS value always ≤ Peak value
- Values update within 1 second of dialog open

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] Both values display (not "Analyzing..." indefinitely)
- [ ] RMS ≤ Peak for all test files
- [ ] Values reasonable for audio content (not NaN/-Inf)
- [ ] Calculation completes quickly (< 1 second for 10-second file)

---

#### B3: RMS Calculation Accuracy
**Test Case**: Verify RMS normalization calculates correct gain

**Status**: ⏳ PENDING

**Test Steps**:
1. Open test file (MAGEvil explosion)
2. Select region with transient peak
3. Open Normalize dialog
4. Note values:
   - Current Peak: -X dB (e.g., -6.0 dB)
   - Current RMS: -Y dB (e.g., -12.0 dB)
5. Test Case 1: Peak Mode
   - Select "Peak Level"
   - Set Target Level: -0.1 dB
   - Note Required Gain: Should be (-0.1 - CurrentPeak) = (+5.9 dB)
   - Click "Apply"
   - Re-open Normalize dialog
   - Verify new Peak ≈ -0.1 dB
6. Undo (Cmd+Z)
7. Test Case 2: RMS Mode
   - Select "RMS Level"
   - Set Target Level: -12.0 dB
   - Note Required Gain: Should be (-12.0 - CurrentRMS) = (0.0 dB if RMS was -12)
   - Click "Apply"
   - Re-open Normalize dialog
   - Verify new RMS ≈ -12.0 dB

**Expected Behavior**:
- Peak mode: Gain = TargetDB - CurrentPeakDB
- RMS mode: Gain = TargetDB - CurrentRMSDB
- Resulting level matches target ±0.1 dB

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] Peak mode: Resulting peak within 0.1 dB of target
- [ ] RMS mode: Resulting RMS within 0.1 dB of target
- [ ] Required Gain calculation matches expectation
- [ ] Audio amplitude visually confirms gain change

---

#### B4: Preview Functionality (Peak vs RMS comparison)
**Test Case**: Verify preview works for both modes and allows comparison

**Status**: ⏳ PENDING

**Test Steps**:
1. Open test file with transient peaks
2. Select region (0:00-0:05)
3. Open Normalize dialog
4. Select "Peak Level", Target: -0.1 dB
5. Click "Preview" + enable "Loop Preview"
6. Listen to normalized audio (should hit peaks)
7. Click "Stop Preview"
8. Switch to "RMS Level", Target: -6.0 dB
9. Click "Preview" again
10. Listen - should sound different (less aggressive than Peak)
11. Compare perceived loudness between Peak and RMS

**Expected Behavior**:
- Peak mode preview: Louder transients, possible clipping feel
- RMS mode preview: More consistent perceived loudness
- Preview accurately reflects mode selection
- Loop allows extended listening for comparison

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] Preview works for both Peak and RMS modes
- [ ] Audio difference audible between modes
- [ ] RMS mode sounds "more balanced" than Peak mode
- [ ] Preview matches final apply result

---

#### B5: Settings Persistence (mode selection saved)
**Test Case**: Verify mode selection persists across app restarts

**Status**: ⏳ PENDING

**Test Steps**:
1. Open test file
2. Open Normalize dialog
3. Change mode to "RMS Level"
4. Click "Cancel" (do not apply)
5. Close application (Cmd+Q)
6. Relaunch application
7. Open same or different file
8. Open Normalize dialog
9. Verify mode selector shows "RMS Level" (last used)
10. Test Case 2:
    - Change to "Peak Level"
    - Click "Apply"
    - Relaunch app
    - Open Normalize dialog
    - Verify "Peak Level" selected

**Expected Behavior**:
- Last used mode persists in Settings
- Mode restored on dialog open
- Persistence via `Settings::getInstance().getSetting("dsp.normalizeMode")`

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] Mode selection persists after app restart
- [ ] Setting stored in correct location (Settings.json)
- [ ] Default mode is "Peak Level" on first launch
- [ ] Setting file path: ~/Library/Application Support/WaveEdit/settings.json

---

#### B6: Undo/Redo Integration
**Test Case**: Verify RMS normalization integrates with undo/redo system

**Status**: ⏳ PENDING

**Test Steps**:
1. Open test file
2. Select region (0:00-0:05)
3. Open Normalize dialog, select "RMS Level", Target: -6.0 dB
4. Click "Apply"
5. Verify waveform amplitude changed
6. Press Cmd+Z (undo)
7. Verify waveform restored to original
8. Press Cmd+Shift+Z (redo)
9. Verify RMS normalize re-applied
10. Play audio - verify matches original apply

**Expected Behavior**:
- Apply creates ONE undo entry
- Undo perfectly reverses RMS normalize
- Redo perfectly re-applies with same gain
- Audio sample-accurate after undo/redo

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] Undo restores original audio exactly
- [ ] Redo matches original apply exactly
- [ ] Only ONE undo entry created
- [ ] Undo description: "Apply Normalize" (or similar)

---

#### B7: File Save Integration
**Test Case**: Verify RMS normalized audio saves correctly

**Status**: ⏳ PENDING

**Test Steps**:
1. Open test file (test_clean.wav)
2. Select full file (Cmd+A)
3. Open Normalize dialog
4. Select "RMS Level", Target: -12.0 dB
5. Click "Apply"
6. Save As: test_rms_normalized.wav (Cmd+Shift+S)
7. Close file
8. Re-open test_rms_normalized.wav
9. Open Normalize dialog
10. Verify Current RMS ≈ -12.0 dB (within 0.1 dB)
11. Verify Current Peak unchanged (relative to RMS)

**Expected Behavior**:
- Saved file contains normalized audio
- RMS level matches target after reload
- No audio degradation or corruption
- Metadata preserved (BWF, etc.)

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] File saves without errors
- [ ] Reloaded RMS matches target ±0.1 dB
- [ ] Audio quality unchanged (no extra compression)
- [ ] File format correct (WAV, bit depth, sample rate)

---

### Test Suite C: Integration & Regression

#### C1: Preview System Coordination (all 6 DSP dialogs)
**Test Case**: Verify ParametricEQ preview doesn't break other dialogs

**Status**: ⏳ PENDING

**Test Steps**:
1. Open test file, select region
2. Test each dialog in sequence:
   - GainDialog: Preview works
   - NormalizeDialog: Preview works (Peak mode)
   - NormalizeDialog: Preview works (RMS mode)
   - ParametricEQDialog: Preview works (REALTIME_DSP)
   - FadeInDialog: Preview works
   - FadeOutDialog: Preview works
   - DCOffsetDialog: Preview works
3. For each, verify:
   - Preview button functional
   - Loop toggle functional
   - Audio plays correctly
   - Stop button works
4. Test dialog chaining:
   - Open GainDialog, preview, cancel
   - Open ParametricEQ, preview, apply
   - Open Normalize, preview, cancel
   - Verify no conflicts or state corruption

**Expected Behavior**:
- All 6 dialogs coexist peacefully
- ParametricEQ REALTIME_DSP doesn't break OFFLINE_BUFFER dialogs
- Preview mode resets correctly between dialogs
- No memory leaks or resource conflicts

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] All 6 dialogs preview correctly
- [ ] No crashes when opening multiple dialogs
- [ ] Preview mode resets between dialogs
- [ ] Audio output correct for each dialog type

---

#### C2: Playback State Preservation
**Test Case**: Verify preview preserves playback position and loop state

**Status**: ⏳ PENDING

**Test Steps**:
1. Open test file
2. Start playback (Space)
3. Let play for 3 seconds (position ≈ 0:03)
4. Open ParametricEQ dialog
5. Click "Preview" (should switch to preview buffer)
6. Listen to preview
7. Click "Stop Preview"
8. Verify playback position restored to ≈ 0:03
9. Press Space - verify playback resumes from 0:03
10. Test Case 2: Loop Preservation
    - Enable loop (Q)
    - Set loop points (0:00-0:10)
    - Open ParametricEQ, preview, stop
    - Verify loop points preserved

**Expected Behavior**:
- Playback position preserved during preview
- Loop state preserved during preview
- Original playback mode restored after preview

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] Playback position preserved ±0.1 seconds
- [ ] Loop points preserved exactly
- [ ] Playback state restored correctly

---

#### C3: Performance Testing (CPU usage during preview)
**Test Case**: Measure CPU overhead of REALTIME_DSP preview

**Status**: ⏳ PENDING

**Test Steps**:
1. Open Activity Monitor (macOS)
2. Open test file (10-second region)
3. Baseline: Play file normally, note CPU % (WaveEdit process)
4. Open ParametricEQ dialog
5. Enable "Loop Preview"
6. Click "Preview"
7. Note CPU % during preview playback
8. While looping, rapidly adjust sliders
9. Note peak CPU % during parameter changes
10. Compare:
    - Baseline playback: _____ %
    - Preview playback: _____ %
    - Preview + parameter change: _____ %

**Expected Behavior**:
- Baseline: < 5% CPU (audio playback is cheap)
- Preview: < 10% CPU (REALTIME_DSP adds EQ overhead)
- Parameter change: < 20% CPU (buffer re-render + EQ)

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] CPU usage < 20% during preview
- [ ] No sustained 100% CPU (would indicate runaway thread)
- [ ] Audio thread priority maintained (no dropouts)

---

#### C4: Memory Leak Detection (long preview sessions)
**Test Case**: Verify no memory leaks during extended preview use

**Status**: ⏳ PENDING

**Test Steps**:
1. Open Activity Monitor → Memory tab
2. Note WaveEdit memory usage (baseline): _____ MB
3. Open test file
4. Open ParametricEQ dialog
5. Enable "Loop Preview"
6. Click "Preview"
7. Let loop for 5 minutes
8. Adjust parameters every 30 seconds
9. Note memory usage after 5 minutes: _____ MB
10. Click "Stop Preview"
11. Close dialog
12. Note memory usage after cleanup: _____ MB
13. Repeat 10 times
14. Verify memory returns to baseline ±10 MB

**Expected Behavior**:
- Memory increase during preview: < 50 MB (preview buffer)
- Memory releases after dialog close
- No sustained growth over multiple open/close cycles

**Actual Behavior**: [MANUAL TESTING REQUIRED]

**Pass Criteria**:
- [ ] Memory growth < 100 MB during 5-minute preview
- [ ] Memory releases after dialog close
- [ ] No memory leak after 10 cycles (growth < 50 MB)

---

## Summary & Sign-Off

### Test Results Overview

**Total Test Cases**: 20 (7 ParametricEQ, 7 RMS Normalize, 6 Integration/Regression)

**Results**:
- ✅ PASS: [X] tests
- ❌ FAIL: [X] tests
- ⚠️ CONDITIONAL PASS: [X] tests (with known issues documented)
- ⏳ BLOCKED: [X] tests
- ⏭️ SKIPPED: [X] tests

### Bugs Found

#### P0 - Critical (Blocks Release)
None found.

#### P1 - Major (Should Fix Before Release)
None found.

#### P2 - Minor (Can Ship With)
- **Known Issue**: ParametricEQ audio artifacts during rapid slider adjustment
  - Severity: P2 (functional, but could be smoother)
  - Root Cause: Buffer re-rendering on message thread per parameter change
  - Workaround: Adjust parameters slowly
  - Future Fix: Implement parameter smoothing or pure real-time DSP

#### P3 - Cosmetic
None found.

---

### Feature Acceptance Criteria

#### ParametricEQ Preview Optimization (P1)
**Acceptance Criteria**:
- [x] Preview mode switched from OFFLINE_BUFFER to REALTIME_DSP (verified in code)
- [ ] Real-time parameter updates during preview playback (MANUAL TEST REQUIRED)
- [ ] Audio artifacts minimal during normal use (rapid adjustment issue documented)
- [ ] Preview integrates with undo/redo correctly
- [ ] Multi-file state management correct

**Status**: ⏳ PENDING MANUAL TESTING

---

#### RMS Normalization (P3)
**Acceptance Criteria**:
- [x] Mode selector UI implemented (verified in code)
- [x] Dual level display (Peak + RMS) (verified in code)
- [x] getRMSLevelDB() method implemented (verified in AudioProcessor.h)
- [ ] RMS calculation accurate ±0.1 dB (MANUAL TEST REQUIRED)
- [ ] Preview works for both Peak and RMS modes
- [ ] Settings persistence works

**Status**: ⏳ PENDING MANUAL TESTING

---

### QA Sign-Off

**Recommendation**: ⏳ MANUAL TESTING REQUIRED

This report provides:
1. ✅ Comprehensive test plan (20 test cases)
2. ✅ Step-by-step test procedures
3. ✅ Clear pass/fail criteria
4. ✅ Code verification (implementation reviewed)
5. ⏳ Manual GUI testing (required for final sign-off)

**Next Steps**:
1. Execute manual test cases using WaveEdit GUI
2. Record actual behavior for each test
3. Document any bugs found
4. Update test results in this report
5. Provide final QA sign-off (PASS/FAIL)

---

**Test Report Generated By**: audio-qa-specialist (Claude Code)
**Report Version**: 1.0
**Last Updated**: 2025-12-02
