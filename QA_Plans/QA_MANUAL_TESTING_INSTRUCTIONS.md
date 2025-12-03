# Manual Testing Instructions: ParametricEQ Preview & RMS Normalization
## WaveEdit by ZQ SFX - User Guide for QA Testing

**Prepared By**: audio-qa-specialist (Claude Code)
**Date**: 2025-12-02
**Testing Duration**: 30-45 minutes
**Difficulty**: Easy (no technical knowledge required)

---

## Quick Start

### 1. Launch the Application
```bash
cd /Users/zacharylquarles/PROJECTS_Apps/Project_WaveEditor
open ./build/WaveEdit_artefacts/Release/WaveEdit.app
```

### 2. Prepare Test Files
You already have test files in `TestFiles/`:
- `test_clean.wav` - Small mono file (good for quick tests)
- `MAGIC_fairy_dust_group_designed_01.wav` - Stereo with tonal content (best for EQ testing)
- `MAGEvil_DESIGNED airMagic explosion 01_PGN_DM.wav` - Loud transient peaks (best for RMS testing)

---

## Test Session 1: ParametricEQ Preview (15 minutes)

### What You're Testing
**Feature**: Real-time parameter updates during EQ preview
**Expected Behavior**: When you adjust EQ sliders during preview playback, you should hear the changes IMMEDIATELY (no need to stop/restart preview)

---

### Test 1A: Basic Preview Functionality (5 min)

**Steps**:
1. Open `MAGIC_fairy_dust_group_designed_01.wav` (drag & drop into WaveEdit)
2. Select a 5-10 second region (click and drag on waveform)
3. Press `Shift+E` to open ParametricEQ dialog
4. Adjust sliders to extreme values:
   - Low Band Gain: +10 dB
   - Mid Band Frequency: 2000 Hz
   - High Band Gain: +5 dB
5. Click "Preview" button
6. Check "Loop Preview" checkbox
7. **LISTEN**: Audio should play with EQ applied

**Pass Criteria**:
- [ ] Preview starts playing immediately
- [ ] Audio plays from selection start (NOT file start)
- [ ] Loop repeats selection continuously
- [ ] EQ effect audible (bass boosted, highs boosted)

**If FAIL**: Note what happened and report in test results

---

### Test 1B: Real-Time Parameter Updates (5 min)

**Steps** (continuing from Test 1A):
1. While preview is still looping, slowly adjust sliders:
   - Move Low Band Gain from +10 dB to -10 dB
   - Move Mid Band Frequency from 1000 Hz to 5000 Hz
   - Move High Band Gain from +5 dB to -5 dB
2. **LISTEN CAREFULLY**: Do you hear the audio change WHILE moving the slider?

**Pass Criteria**:
- [ ] Low Band Gain: Hear bass get quieter as you move slider down
- [ ] Mid Band Frequency: Hear tonal shift as you sweep frequency
- [ ] High Band Gain: Hear highs get quieter as you move slider down
- [ ] Changes happen DURING slider movement (not after release)

**Expected Behavior**: Audio updates in real-time as you drag slider
**Known Issue**: You MAY hear brief clicks/pops during RAPID slider movement (this is expected, documented as P2)

---

### Test 1C: Audio Artifact Check (5 min)

**Steps**:
1. With preview still looping, RAPIDLY move sliders back and forth:
   - Low Band Gain: -20 dB ↔ +20 dB (fast!)
   - Mid Band Frequency: 100 Hz ↔ 5000 Hz (fast!)
2. **LISTEN FOR**:
   - Clicks or pops when slider moves
   - Audio dropouts (silence)
   - Distortion or clipping
   - Zipper noise (rapid stepping sound)

**Document Severity**:
- [ ] NO artifacts heard (better than expected!)
- [ ] Minor clicks/pops ONLY during RAPID movement (acceptable P2)
- [ ] Frequent artifacts during normal adjustment (MAJOR ISSUE - report as P1)
- [ ] Constant artifacts or audio corruption (CRITICAL - report as P0)

**Notes**: Write down exactly what you hear and when it happens

---

### Test 1D: Apply and Undo (2 min)

**Steps**:
1. Click "Stop Preview" button
2. Click "Apply" button
3. Play the audio normally (press Space)
4. **VERIFY**: Audio matches what you heard in preview
5. Press `Cmd+Z` (Undo)
6. **VERIFY**: Waveform returns to original (no EQ)
7. Press `Cmd+Shift+Z` (Redo)
8. **VERIFY**: EQ re-applied (matches original apply)

**Pass Criteria**:
- [ ] Applied audio matches preview exactly
- [ ] Undo restores original audio perfectly
- [ ] Redo matches original apply perfectly
- [ ] Waveform visually changes (amplitude shifts for boosted bands)

---

## Test Session 2: RMS Normalization (15 minutes)

### What You're Testing
**Feature**: RMS normalization mode (alternative to Peak normalization)
**Key Difference**:
- Peak mode: Brings loudest sample to target level
- RMS mode: Brings average loudness to target level

---

### Test 2A: UI and Level Display (5 min)

**Steps**:
1. Open `MAGEvil_DESIGNED airMagic explosion 01_PGN_DM.wav`
2. Press `Cmd+A` to select entire file
3. Press `Ctrl+Shift+N` to open Normalize dialog
4. **OBSERVE UI**:
   - Mode dropdown (should show "Peak Level" by default)
   - Current Peak: [some negative dB value, e.g., -8.5 dB]
   - Current RMS: [some negative dB value, e.g., -15.2 dB]
   - Required Gain: [calculated value]

**Pass Criteria**:
- [ ] Mode dropdown visible with 2 options
- [ ] Both "Current Peak" and "Current RMS" display values (not "Analyzing..." forever)
- [ ] RMS value is LOWER (more negative) than Peak value (e.g., -15 dB vs -8 dB)
- [ ] Values update within 1 second

**Record Values Here**:
- Current Peak: _______ dB
- Current RMS: _______ dB
- Required Gain (Peak mode, target -0.1 dB): _______ dB

---

### Test 2B: Mode Switching (3 min)

**Steps**:
1. With Normalize dialog still open, change mode dropdown to "RMS Level"
2. **OBSERVE**: Required Gain value changes
3. Switch back to "Peak Level"
4. **OBSERVE**: Required Gain changes back

**Pass Criteria**:
- [ ] Switching mode immediately updates "Required Gain"
- [ ] Peak mode: Required Gain = (Target - Current Peak)
- [ ] RMS mode: Required Gain = (Target - Current RMS)
- [ ] UI responds instantly (no lag)

**Example Calculation** (verify manually):
- If Current Peak = -8.5 dB, Target = -0.1 dB → Required Gain should be +8.4 dB
- If Current RMS = -15.2 dB, Target = -0.1 dB → Required Gain should be +15.1 dB

---

### Test 2C: RMS Normalization Apply (5 min)

**Steps**:
1. Select mode: "Peak Level"
2. Set Target Level: -0.1 dB
3. Click "Apply"
4. Press `Ctrl+Shift+N` again to re-open Normalize dialog
5. **RECORD**: New Current Peak = _______ dB (should be ≈ -0.1 dB)
6. Click "Cancel"
7. Press `Cmd+Z` to undo
8. Press `Ctrl+Shift+N` again
9. Select mode: "RMS Level"
10. Set Target Level: -12.0 dB
11. Click "Apply"
12. Press `Ctrl+Shift+N` again
13. **RECORD**: New Current RMS = _______ dB (should be ≈ -12.0 dB)

**Pass Criteria**:
- [ ] Peak normalize: New peak within 0.2 dB of target (-0.1 dB)
- [ ] RMS normalize: New RMS within 0.2 dB of target (-12.0 dB)
- [ ] Undo works correctly between tests
- [ ] Waveform amplitude visibly changes

---

### Test 2D: Preview Comparison (5 min)

**Steps**:
1. Undo all previous normalizations (press `Cmd+Z` until back to original)
2. Select a 5-second region with loud transient peaks
3. Press `Ctrl+Shift+N`
4. Set mode: "Peak Level", Target: -0.1 dB
5. Click "Preview" + check "Loop Preview"
6. **LISTEN**: Note how loud the audio sounds
7. Click "Stop Preview"
8. Change mode to "RMS Level", Target: -6.0 dB
9. Click "Preview" again
10. **LISTEN**: Compare loudness to Peak mode

**Expected Difference**:
- Peak mode at -0.1 dB: Very loud, may sound aggressive on transients
- RMS mode at -6.0 dB: More balanced, consistent perceived loudness

**Pass Criteria**:
- [ ] Both modes preview correctly
- [ ] Audio difference is audible
- [ ] RMS mode sounds "smoother" or "more balanced" than Peak mode
- [ ] No crashes or audio corruption

---

## Test Session 3: Integration Tests (10 minutes)

### Test 3A: Settings Persistence (3 min)

**Steps**:
1. Open Normalize dialog
2. Change mode to "RMS Level"
3. Click "Apply"
4. Quit WaveEdit (Cmd+Q)
5. Relaunch WaveEdit
6. Open any audio file
7. Open Normalize dialog
8. **VERIFY**: Mode selector shows "RMS Level" (last used)

**Pass Criteria**:
- [ ] Last used mode persists after app restart
- [ ] Setting stored in `~/Library/Application Support/WaveEdit/settings.json`

---

### Test 3B: Multi-File State (4 min)

**Steps**:
1. Open File A (test_clean.wav) - Tab 1
2. Open ParametricEQ dialog, adjust parameters, click "Preview" (DO NOT APPLY)
3. Open File B (MAGIC_fairy_dust) in new tab (Cmd+O) - Tab 2
4. Switch to Tab 2, press Space to play
5. **VERIFY**: No EQ applied (audio sounds normal)
6. Open ParametricEQ in Tab 2
7. **VERIFY**: Default parameters (0 dB gains, default frequencies)
8. Switch back to Tab 1
9. Click "Apply" to commit EQ
10. Switch to Tab 2 again
11. **VERIFY**: Still no EQ (tabs are independent)

**Pass Criteria**:
- [ ] Tab 2 unaffected by Tab 1 preview
- [ ] Tab 2 EQ dialog shows defaults
- [ ] Tab 1 applied EQ persists
- [ ] No audio artifacts when switching tabs

---

### Test 3C: Undo/Redo Chain (3 min)

**Steps**:
1. Open test file, select region
2. Apply ParametricEQ (+5 dB low band)
3. Apply Normalize (RMS mode, -12 dB)
4. Apply Gain (+3 dB)
5. Verify waveform changed significantly
6. Press `Cmd+Z` three times
7. **VERIFY**: Back to original audio
8. Press `Cmd+Shift+Z` three times
9. **VERIFY**: All three operations re-applied in correct order

**Pass Criteria**:
- [ ] Undo reverses operations in reverse order
- [ ] Redo re-applies in original order
- [ ] Audio matches original applies exactly
- [ ] No phantom undo entries from preview

---

## Reporting Results

### Option 1: Quick Report (5 min)
Simply copy this checklist and mark each item:
```
Test 1A: Basic Preview - [PASS/FAIL]
Test 1B: Real-Time Updates - [PASS/FAIL]
Test 1C: Audio Artifacts - [ACCEPTABLE P2 / MAJOR ISSUE]
Test 1D: Apply & Undo - [PASS/FAIL]
Test 2A: UI & Level Display - [PASS/FAIL]
Test 2B: Mode Switching - [PASS/FAIL]
Test 2C: RMS Apply - [PASS/FAIL]
Test 2D: Preview Comparison - [PASS/FAIL]
Test 3A: Settings Persistence - [PASS/FAIL]
Test 3B: Multi-File State - [PASS/FAIL]
Test 3C: Undo/Redo Chain - [PASS/FAIL]

Notes: [Any issues or observations]
```

### Option 2: Detailed Report (15 min)
Fill out the complete test report in `QA_TEST_REPORT_PARAMETRIC_EQ_RMS_NORMALIZE.md`

---

## Critical Questions to Answer

1. **ParametricEQ Preview**: Do parameter changes update audio in real-time during playback?
   - YES: Feature works as designed ✅
   - NO: Critical bug, needs investigation ❌

2. **Audio Artifacts**: How severe are clicks/pops during rapid slider movement?
   - None/Minimal: Better than expected ✅
   - Only during RAPID movement: Acceptable P2 ⚠️
   - During normal use: Major issue P1 ❌
   - Constant/Severe: Critical P0 ❌

3. **RMS Calculation**: Does RMS normalize produce correct loudness?
   - Peak ≈ -0.1 dB after Peak normalize: Correct ✅
   - RMS ≈ -12.0 dB after RMS normalize: Correct ✅
   - Significant error (>0.5 dB): Calculation bug ❌

4. **Settings Persistence**: Does RMS mode selection persist?
   - YES: Feature complete ✅
   - NO: Minor bug, needs fix ⚠️

---

## Success Criteria

**Minimum Requirements for QA Sign-Off**:
- [ ] ParametricEQ preview updates in real-time (Questions 1 = YES)
- [ ] Audio artifacts acceptable (Question 2 = Minimal or P2)
- [ ] RMS calculation accurate (Question 3 = Correct)
- [ ] No P0/P1 regressions in existing features
- [ ] Undo/redo works correctly

**If ALL criteria met**: ✅ **FEATURES APPROVED FOR RELEASE**
**If ANY P0/P1 issues found**: ❌ **HOLD RELEASE, FIX CRITICAL BUGS**

---

## Need Help?

**Console Logs**: Look for errors in Console.app (filter for "WaveEdit")
**Crash Reports**: `~/Library/Logs/DiagnosticReports/WaveEdit*.crash`
**Settings File**: `~/Library/Application Support/WaveEdit/settings.json`

**Common Issues**:
- App won't launch: Check build output, run `./build-and-run.command clean` and rebuild
- Audio device errors: Check System Preferences → Sound → Input/Output
- File won't load: Check file format (WAV, FLAC, OGG supported)

---

**Testing Instructions Prepared By**: audio-qa-specialist (Claude Code)
**Last Updated**: 2025-12-02
**Estimated Testing Time**: 30-45 minutes total
