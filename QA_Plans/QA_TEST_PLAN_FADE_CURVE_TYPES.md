# QA Test Plan: Fade Curve Types Feature

**Feature Version:** 1.0
**Date Created:** 2025-12-01
**Application:** WaveEdit - Professional Audio Editor
**Test Type:** Manual Functional Testing

---

## Executive Summary

This test plan covers the comprehensive validation of the new fade curve types feature added to Fade In and Fade Out dialogs. The feature introduces 4 curve types (Linear, Exponential, Logarithmic, S-Curve) with independent persistence, real-time preview updates, and full undo/redo support.

**Estimated Total Test Time:** 3-4 hours (experienced tester)

---

## Test Environment Setup

### Required Test Files

Create/obtain the following audio files for testing:

1. **mono_1khz_10sec.wav** - Mono, 44.1kHz, 16-bit, 10s, 1kHz sine wave
2. **stereo_music_60sec.wav** - Stereo, 48kHz, 24-bit, 60s, music with transients
3. **5.1_ambience_30sec.wav** - 6-channel (5.1), 48kHz, 24-bit, 30s, ambient noise
4. **tiny_100samples.wav** - Stereo, 44.1kHz, 16-bit, 100 samples (~2.3ms)
5. **hires_96k_stereo.wav** - Stereo, 96kHz, 24-bit, 10s, pink noise
6. **speech_mono_30sec.wav** - Mono, 44.1kHz, 16-bit, 30s, spoken voice

### Build and Launch

```bash
cd /Users/zacharylquarles/PROJECTS_Apps/Project_WaveEditor
./build-and-run.command clean
./build-and-run.command
```

### Test Data Preparation

Before starting, ensure:
- All test files load without errors
- Audio device is configured (check Settings)
- Volume is at comfortable listening level
- Preferences are at default state (delete `~/Library/Application Support/WaveEdit/settings.json` if needed)

---

## Test Scenarios

---

### CATEGORY 1: Dialog UI and Controls (30 minutes)

#### Test 1.1: Fade In Dialog - Curve Type ComboBox

**Priority:** P1
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Open `stereo_music_60sec.wav`
2. Select region from 0:00 to 0:05 (click and drag on waveform)
3. Press `Cmd+Shift+I` (macOS) or `Ctrl+Shift+I` (Windows/Linux)
4. Verify Fade In dialog appears
5. Locate "Curve Type:" ComboBox
6. Click ComboBox dropdown

**Expected Results:**
- ComboBox displays exactly 4 items:
  - Linear
  - Exponential
  - Logarithmic
  - S-Curve
- Default selection is "Linear" (first launch)
- ComboBox is enabled and clickable
- Text is clearly readable

**Pass/Fail Criteria:**
- PASS: All 4 curve types visible, Linear selected by default
- FAIL: Missing curve types, ComboBox disabled, or wrong default

---

#### Test 1.2: Fade Out Dialog - Curve Type ComboBox

**Priority:** P1
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Open `stereo_music_60sec.wav`
2. Select region from 0:55 to 1:00 (last 5 seconds)
3. Press `Cmd+Shift+O` (macOS) or `Ctrl+Shift+O` (Windows/Linux)
4. Verify Fade Out dialog appears
5. Locate "Curve Type:" ComboBox
6. Click ComboBox dropdown

**Expected Results:**
- ComboBox displays exactly 4 items (same as Fade In)
- Default selection is "Linear" (first launch)
- ComboBox is independent from Fade In (doesn't share state)

**Pass/Fail Criteria:**
- PASS: Identical behavior to Test 1.1, independent state
- FAIL: Missing items or shared state with Fade In dialog

---

#### Test 1.3: Dialog Layout and Sizing

**Priority:** P2
**Test File:** `mono_1khz_10sec.wav`

**Steps:**
1. Open Fade In dialog (`Cmd+Shift+I` on selection)
2. Verify dialog dimensions are 400x270 pixels
3. Check visual layout:
   - Title: "Fade In" centered at top
   - Instruction text below title
   - "Curve Type:" label and ComboBox on same row
   - Preview button (left), Loop Preview toggle, Cancel/Apply buttons (right)
4. Repeat for Fade Out dialog (`Cmd+Shift+O`)

**Expected Results:**
- Both dialogs are 400x270 pixels
- All controls visible without scrolling
- ComboBox has adequate width (~180px) to display "Logarithmic"
- No overlapping controls
- Consistent spacing between elements

**Pass/Fail Criteria:**
- PASS: Clean layout, all text readable, no overlaps
- FAIL: Controls overlap, text truncated, or layout broken

---

#### Test 1.4: Preview Button Toggle Behavior

**Priority:** P1
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Open Fade In dialog on a 5-second selection
2. Click "Preview" button
3. Observe button text and color change
4. Wait for preview playback to start
5. Click "Preview" button again (now labeled "Stop Preview")
6. Verify playback stops

**Expected Results:**
- First click: Button text changes to "Stop Preview", color changes to dark red
- Playback starts immediately
- Second click: Button text reverts to "Preview", color reverts to default
- Playback stops immediately

**Pass/Fail Criteria:**
- PASS: Toggle works, button updates instantly, playback starts/stops
- FAIL: Button doesn't toggle, playback doesn't stop, or visual state incorrect

---

#### Test 1.5: Loop Preview Toggle

**Priority:** P2
**Test File:** `tiny_100samples.wav` (very short for loop testing)

**Steps:**
1. Open Fade In dialog on entire file selection
2. Enable "Loop Preview" toggle
3. Click "Preview" button
4. Wait for audio to loop at least 3 times (should be very fast)
5. Click "Stop Preview"
6. Disable "Loop Preview" toggle
7. Click "Preview" again
8. Verify playback stops after single pass

**Expected Results:**
- With loop enabled: Audio repeats seamlessly
- With loop disabled: Audio plays once and stops
- Toggle changes behavior immediately on next preview

**Pass/Fail Criteria:**
- PASS: Looping works as expected, toggle controls behavior
- FAIL: Looping doesn't work, or plays once when loop enabled

---

### CATEGORY 2: Audio Quality - Curve Accuracy (60 minutes)

#### Test 2.1: LINEAR Fade In - Perceptual Test

**Priority:** P0 (Critical)
**Test File:** `mono_1khz_10sec.wav`

**Steps:**
1. Open file, select first 2 seconds
2. Open Fade In dialog, select "Linear" curve
3. Click "Preview" with loop enabled
4. Listen carefully to the fade envelope
5. Apply fade and listen to main playback

**Expected Perceptual Result:**
- Constant rate of volume increase
- No sudden jumps or steps
- Volume increases at steady pace from silence to full
- Mathematically: gain = x (straight line from 0 to 1)

**Objective Verification:**
- Waveform envelope should show straight diagonal line from 0% to 100% amplitude

**Pass/Fail Criteria:**
- PASS: Sounds like constant, steady volume increase
- FAIL: Audible steps, pops, or non-linear volume change

---

#### Test 2.2: EXPONENTIAL Fade In - Perceptual Test

**Priority:** P0 (Critical)
**Test File:** `speech_mono_30sec.wav`

**Steps:**
1. Open file, select first 3 seconds
2. Open Fade In dialog, select "Exponential" curve
3. Click "Preview" with loop enabled
4. Listen for slow start, rapid end characteristic
5. Apply and verify on main playback

**Expected Perceptual Result:**
- Volume stays low for longer at the beginning
- Volume increases rapidly near the end
- Gradual transition, then sudden arrival at full volume
- Mathematically: gain = x² (quadratic curve)

**Objective Verification:**
- Waveform envelope should show gentle curve, steeper at end
- First 50% of fade should be quieter than linear
- Last 25% should approach full volume quickly

**Pass/Fail Criteria:**
- PASS: Clear slow-start, fast-end behavior
- FAIL: Sounds linear or has reversed characteristic

---

#### Test 2.3: LOGARITHMIC Fade In - Perceptual Test

**Priority:** P0 (Critical)
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Open file, select first 4 seconds
2. Open Fade In dialog, select "Logarithmic" curve
3. Click "Preview" with loop enabled
4. Listen for fast start, slow end characteristic
5. Apply and verify on main playback

**Expected Perceptual Result:**
- Volume increases rapidly at the beginning
- Volume approaches full level gradually at the end
- Quick arrival, then gentle leveling off
- Mathematically: gain = 1 - (1-x)² (inverted quadratic)

**Objective Verification:**
- Waveform envelope should show steep curve at start, gentler at end
- First 25% of fade should reach significant volume quickly
- Last 50% should gradually approach 100%

**Pass/Fail Criteria:**
- PASS: Clear fast-start, slow-end behavior (opposite of exponential)
- FAIL: Sounds linear or exponential

---

#### Test 2.4: S_CURVE Fade In - Perceptual Test

**Priority:** P0 (Critical)
**Test File:** `hires_96k_stereo.wav`

**Steps:**
1. Open file, select first 5 seconds
2. Open Fade In dialog, select "S-Curve" curve
3. Click "Preview" with loop enabled
4. Listen for smooth start AND smooth end
5. Apply and verify on main playback

**Expected Perceptual Result:**
- Gentle start (slower than linear)
- Accelerates in the middle
- Gentle arrival at full volume (slower than linear)
- Most natural-sounding fade for crossfades
- Mathematically: gain = 3x² - 2x³ (smoothstep function)

**Objective Verification:**
- Waveform envelope should show S-shaped curve
- Symmetrical smoothness at both ends
- Steeper slope in middle 50%

**Pass/Fail Criteria:**
- PASS: Smooth, natural-sounding fade with gentle start and end
- FAIL: Sounds linear or has harsh transitions at boundaries

---

#### Test 2.5: LINEAR Fade Out - Perceptual Test

**Priority:** P0 (Critical)
**Test File:** `mono_1khz_10sec.wav`

**Steps:**
1. Open file, select last 2 seconds
2. Open Fade Out dialog, select "Linear" curve
3. Click "Preview" with loop enabled
4. Listen for constant rate of volume decrease
5. Apply and verify on main playback

**Expected Perceptual Result:**
- Constant rate of volume decrease
- No sudden drops or steps
- Volume decreases at steady pace from full to silence
- Mathematically: gain = 1 - x (straight line from 1 to 0)

**Objective Verification:**
- Waveform envelope should show straight diagonal line from 100% to 0% amplitude

**Pass/Fail Criteria:**
- PASS: Sounds like constant, steady volume decrease
- FAIL: Audible steps, clicks, or non-linear volume change

---

#### Test 2.6: EXPONENTIAL Fade Out - Perceptual Test

**Priority:** P0 (Critical)
**Test File:** `speech_mono_30sec.wav`

**Steps:**
1. Open file, select last 3 seconds
2. Open Fade Out dialog, select "Exponential" curve
3. Click "Preview" with loop enabled
4. Listen for fast start, slow end characteristic (rapid drop, then gradual)
5. Apply and verify on main playback

**Expected Perceptual Result:**
- Volume drops quickly at the beginning of fade
- Volume decreases slowly near the end, lingering longer
- Rapid departure, then gentle approach to silence
- Mathematically: gain = (1-x)² (inverted quadratic)

**Objective Verification:**
- Waveform envelope should show steep drop at start, gentler at end
- First 25% should drop significantly
- Last 50% should gradually approach 0%

**Pass/Fail Criteria:**
- PASS: Clear fast-start, slow-end decay
- FAIL: Sounds linear or has reversed characteristic

---

#### Test 2.7: LOGARITHMIC Fade Out - Perceptual Test

**Priority:** P0 (Critical)
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Open file, select last 4 seconds
2. Open Fade Out dialog, select "Logarithmic" curve
3. Click "Preview" with loop enabled
4. Listen for slow start, fast end characteristic (gradual drop, then rapid)
5. Apply and verify on main playback

**Expected Perceptual Result:**
- Volume stays high for longer at the beginning
- Volume drops rapidly near the end
- Gradual departure, then sudden silence
- Mathematically: gain = 1 - x² (quadratic decay)

**Objective Verification:**
- Waveform envelope should show gentle curve at start, steeper at end
- First 50% should remain at higher volume
- Last 25% should drop to silence quickly

**Pass/Fail Criteria:**
- PASS: Clear slow-start, fast-end decay (opposite of exponential)
- FAIL: Sounds linear or exponential

---

#### Test 2.8: S_CURVE Fade Out - Perceptual Test

**Priority:** P0 (Critical)
**Test File:** `hires_96k_stereo.wav`

**Steps:**
1. Open file, select last 5 seconds
2. Open Fade Out dialog, select "S-Curve" curve
3. Click "Preview" with loop enabled
4. Listen for smooth start AND smooth end
5. Apply and verify on main playback

**Expected Perceptual Result:**
- Gentle start (slower drop than linear)
- Accelerates in the middle
- Gentle arrival at silence (slower than linear)
- Most natural-sounding fade out for music endings
- Mathematically: inverted smoothstep

**Objective Verification:**
- Waveform envelope should show S-shaped curve (inverted from fade in)
- Symmetrical smoothness at both ends
- Steeper slope in middle 50%

**Pass/Fail Criteria:**
- PASS: Smooth, natural-sounding fade with gentle start and end
- FAIL: Sounds linear or has harsh transitions

---

#### Test 2.9: Click/Pop Detection at Fade Boundaries

**Priority:** P0 (Critical)
**Test File:** All test files

**Steps:**
1. For EACH test file and EACH curve type:
   - Apply Fade In (first 1 second)
   - Listen carefully at start and end of fade region with headphones
   - Apply Fade Out (last 1 second)
   - Listen carefully at start and end of fade region
2. Repeat at high zoom (1000% or sample-level view)
3. Check for discontinuities in waveform

**Expected Results:**
- No audible clicks or pops at any fade boundary
- Smooth waveform transitions (no vertical jumps between samples)
- Zero-amplitude at fade start (Fade In) and fade end (Fade Out)

**Pass/Fail Criteria:**
- PASS: Zero clicks/pops across ALL files and ALL curve types
- FAIL: ANY audible artifact at ANY fade boundary = CRITICAL BUG

---

#### Test 2.10: Multi-Channel Uniform Fading

**Priority:** P1
**Test File:** `5.1_ambience_30sec.wav`

**Steps:**
1. Open 6-channel file
2. Select first 3 seconds
3. Apply Fade In with Exponential curve
4. Open Audio File Properties dialog to verify channel count
5. Play back and listen to all speakers
6. Visually inspect waveform for all 6 channels

**Expected Results:**
- All 6 channels fade identically
- No channel is louder/quieter than others during fade
- Waveform envelopes are identical across all channels
- No phase issues or cancellation

**Pass/Fail Criteria:**
- PASS: Perfect uniformity across all channels
- FAIL: Any channel has different fade curve or timing

---

### CATEGORY 3: Settings Persistence (20 minutes)

#### Test 3.1: Fade In Curve Preference Persistence

**Priority:** P1
**Test File:** `mono_1khz_10sec.wav`

**Steps:**
1. Open Fade In dialog, select "Exponential" curve
2. Click "Preview" (to trigger preference save)
3. Click "Cancel" to close dialog
4. Re-open Fade In dialog on same or different selection
5. Check ComboBox selection

**Expected Results:**
- ComboBox shows "Exponential" (last used)
- Preference persisted to `dsp.lastFadeInCurve`

**Pass/Fail Criteria:**
- PASS: Last-used curve is remembered
- FAIL: Reverts to Linear or wrong curve

---

#### Test 3.2: Fade Out Curve Preference Persistence

**Priority:** P1
**Test File:** `mono_1khz_10sec.wav`

**Steps:**
1. Open Fade Out dialog, select "S-Curve" curve
2. Click "Preview" (to trigger preference save)
3. Click "Cancel" to close dialog
4. Re-open Fade Out dialog on same or different selection
5. Check ComboBox selection

**Expected Results:**
- ComboBox shows "S-Curve" (last used)
- Preference persisted to `dsp.lastFadeOutCurve`

**Pass/Fail Criteria:**
- PASS: Last-used curve is remembered
- FAIL: Reverts to Linear or wrong curve

---

#### Test 3.3: Independent Preference Storage

**Priority:** P1
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Open Fade In dialog, select "Exponential", preview, cancel
2. Open Fade Out dialog, select "Logarithmic", preview, cancel
3. Re-open Fade In dialog → verify "Exponential" is selected
4. Re-open Fade Out dialog → verify "Logarithmic" is selected

**Expected Results:**
- Fade In preference: `dsp.lastFadeInCurve = 1` (Exponential)
- Fade Out preference: `dsp.lastFadeOutCurve = 2` (Logarithmic)
- Each dialog maintains independent state

**Pass/Fail Criteria:**
- PASS: Both dialogs remember their own curve independently
- FAIL: One dialog's curve affects the other

---

#### Test 3.4: Persistence Across Application Restart

**Priority:** P1
**Test File:** Any file

**Steps:**
1. Set Fade In to "S-Curve", preview
2. Set Fade Out to "Exponential", preview
3. Quit WaveEdit completely
4. Relaunch WaveEdit
5. Open any file, open Fade In dialog → check curve
6. Open Fade Out dialog → check curve

**Expected Results:**
- Fade In dialog shows "S-Curve"
- Fade Out dialog shows "Exponential"
- Preferences loaded from `settings.json`

**Pass/Fail Criteria:**
- PASS: Curves persist across restart
- FAIL: Revert to default after restart

---

#### Test 3.5: Curve Change During Preview

**Priority:** P2
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Open Fade In dialog on 5-second selection
2. Enable "Loop Preview"
3. Click "Preview" with "Linear" selected
4. While preview is playing, change ComboBox to "Exponential"
5. Observe playback behavior

**Expected Results:**
- Playback stops briefly
- Playback restarts with new Exponential curve
- No crash or audio glitches
- Preference is saved to "Exponential"

**Pass/Fail Criteria:**
- PASS: Smooth transition to new curve during preview
- FAIL: Crash, audio glitches, or preview doesn't update

---

### CATEGORY 4: Keyboard Shortcut Integration (15 minutes)

#### Test 4.1: Fade In Shortcut with Last-Used Curve

**Priority:** P1
**Test File:** `speech_mono_30sec.wav`

**Steps:**
1. Open Fade In dialog, select "S-Curve", preview, apply
2. Select a NEW region (e.g., 5-10 seconds)
3. Press `Cmd+Shift+I` (macOS) or `Ctrl+Shift+I` (Windows/Linux)
4. Verify dialog opens with "S-Curve" pre-selected

**Expected Results:**
- Dialog opens with last-used curve ("S-Curve")
- Shortcut respects preference

**Pass/Fail Criteria:**
- PASS: Shortcut opens dialog with correct curve
- FAIL: Wrong curve or dialog doesn't respect preference

---

#### Test 4.2: Fade Out Shortcut with Last-Used Curve

**Priority:** P1
**Test File:** `speech_mono_30sec.wav`

**Steps:**
1. Open Fade Out dialog, select "Logarithmic", preview, apply
2. Select a NEW region (e.g., 20-25 seconds)
3. Press `Cmd+Shift+O` (macOS) or `Ctrl+Shift+O` (Windows/Linux)
4. Verify dialog opens with "Logarithmic" pre-selected

**Expected Results:**
- Dialog opens with last-used curve ("Logarithmic")
- Shortcut respects preference

**Pass/Fail Criteria:**
- PASS: Shortcut opens dialog with correct curve
- FAIL: Wrong curve or dialog doesn't respect preference

---

### CATEGORY 5: Undo/Redo Integration (30 minutes)

#### Test 5.1: Undo Single Fade In

**Priority:** P1
**Test File:** `mono_1khz_10sec.wav`

**Steps:**
1. Open file, select first 2 seconds
2. Apply Fade In with "Exponential" curve
3. Verify waveform shows fade envelope
4. Press `Cmd+Z` (Undo)
5. Verify waveform reverts to original

**Expected Results:**
- Undo completely reverses the fade
- Waveform matches original state
- Audio playback matches original

**Pass/Fail Criteria:**
- PASS: Perfect restoration of original audio
- FAIL: Audio remains faded or partially faded

---

#### Test 5.2: Redo Single Fade In

**Priority:** P1
**Test File:** `mono_1khz_10sec.wav`

**Steps:**
1. Continue from Test 5.1 (after undo)
2. Press `Cmd+Shift+Z` (Redo)
3. Verify waveform shows Exponential fade again
4. Play back to verify audio

**Expected Results:**
- Redo re-applies the exact same fade
- Waveform matches previous faded state
- Audio playback has Exponential fade

**Pass/Fail Criteria:**
- PASS: Perfect re-application of fade
- FAIL: Fade not restored or wrong curve

---

#### Test 5.3: Undo/Redo Chain with Multiple Curve Types

**Priority:** P1
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Select 0-5s, apply Fade In with "Linear"
2. Select 5-10s, apply Fade In with "Exponential"
3. Select 10-15s, apply Fade In with "Logarithmic"
4. Select 15-20s, apply Fade In with "S-Curve"
5. Press `Cmd+Z` four times (undo all)
6. Press `Cmd+Shift+Z` four times (redo all)
7. Verify each fade region has correct curve

**Expected Results:**
- Each undo reverses the most recent fade
- Each redo re-applies the correct fade with correct curve
- No mixing of curve types

**Pass/Fail Criteria:**
- PASS: All undos and redos apply correct curves
- FAIL: Wrong curve applied or undo/redo breaks

---

#### Test 5.4: Undo Preserves Curve Type (Not Just Audio)

**Priority:** P2
**Test File:** `mono_1khz_10sec.wav`

**Steps:**
1. Apply Fade In with "S-Curve" to selection
2. Undo the fade
3. Open Fade In dialog again
4. Check ComboBox selection

**Expected Results:**
- ComboBox still shows "S-Curve" (last used)
- Undo only reverses audio, not preference

**Pass/Fail Criteria:**
- PASS: Preference unchanged by undo
- FAIL: Undo resets curve to Linear

---

#### Test 5.5: Undo/Redo Across Document Switch

**Priority:** P2
**Test File:** `mono_1khz_10sec.wav` + `stereo_music_60sec.wav`

**Steps:**
1. Open both files in separate tabs
2. In File 1: Apply Fade In "Exponential" to selection
3. Switch to File 2 tab
4. In File 2: Apply Fade Out "Logarithmic" to selection
5. Switch back to File 1 tab
6. Press `Cmd+Z` → verify File 1 undo works
7. Switch to File 2 tab
8. Press `Cmd+Z` → verify File 2 undo works independently

**Expected Results:**
- Each document maintains independent undo/redo stack
- Undo in File 1 doesn't affect File 2
- Curve types remain correct after undo

**Pass/Fail Criteria:**
- PASS: Independent undo/redo per document
- FAIL: Undo affects wrong document or shared undo stack

---

### CATEGORY 6: Edge Cases and Error Handling (30 minutes)

#### Test 6.1: Very Short Selection (< 100 samples)

**Priority:** P1
**Test File:** `mono_1khz_10sec.wav`

**Steps:**
1. Zoom in to sample-level view (1000%+)
2. Select exactly 50 samples (~1.1ms at 44.1kHz)
3. Apply Fade In with "Exponential" curve
4. Visually inspect waveform at sample level
5. Apply Fade Out with "S-Curve" to another 50-sample region

**Expected Results:**
- Fade applies correctly even on tiny selections
- Smooth curve visible even at 50 samples
- No clicks or discontinuities
- Preview playback works (though very brief)

**Pass/Fail Criteria:**
- PASS: Fade works smoothly on short selections
- FAIL: Clicks, pops, or refusal to apply fade

---

#### Test 6.2: Very Long Selection (> 1 minute)

**Priority:** P2
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Select entire 60-second file
2. Apply Fade In with "Linear" curve
3. Monitor memory usage (Task Manager / Activity Monitor)
4. Play back first 10 seconds
5. Undo and apply Fade Out with "S-Curve"
6. Play back last 10 seconds

**Expected Results:**
- Fade applies without memory issues
- No performance degradation
- Smooth playback at both ends
- Undo/redo works normally

**Pass/Fail Criteria:**
- PASS: Handles long durations without issues
- FAIL: Crash, hang, or memory leak

---

#### Test 6.3: Different Sample Rates

**Priority:** P1
**Test File:** `stereo_music_60sec.wav` (48kHz) + `hires_96k_stereo.wav` (96kHz)

**Steps:**
1. Open 48kHz file, apply Fade In "Exponential" (2 seconds)
2. Open 96kHz file, apply Fade In "Exponential" (2 seconds)
3. Compare visual fade curves in waveform
4. Compare perceptual fade duration (both should sound like 2 seconds)

**Expected Results:**
- Fade duration is time-based, not sample-based
- Both fades sound identical (2 seconds each)
- 96kHz fade uses 2x more samples but same curve shape

**Pass/Fail Criteria:**
- PASS: Time-accurate fades regardless of sample rate
- FAIL: Different fade durations or incorrect curve

---

#### Test 6.4: No Selection (Full File)

**Priority:** P2
**Test File:** `mono_1khz_10sec.wav`

**Steps:**
1. Open file, ensure no selection (click waveform to deselect)
2. Press `Cmd+Shift+I` (Fade In shortcut)
3. Observe behavior

**Expected Results:**
- Dialog may refuse to open (fade requires selection)
- OR dialog opens but applies to entire file (check docs for intended behavior)

**Pass/Fail Criteria:**
- PASS: Behavior matches documented specification
- FAIL: Crash or undefined behavior

---

#### Test 6.5: Mono, Stereo, and Multi-Channel Consistency

**Priority:** P1
**Test Files:** `mono_1khz_10sec.wav`, `stereo_music_60sec.wav`, `5.1_ambience_30sec.wav`

**Steps:**
1. For EACH file:
   - Select first 3 seconds
   - Apply Fade In "S-Curve"
   - Visually verify curve shape is identical (relative to amplitude)
   - Play back to verify perceptual consistency

**Expected Results:**
- Curve shape is identical across all channel counts
- All channels in multi-channel files fade uniformly
- No difference in behavior based on channel count

**Pass/Fail Criteria:**
- PASS: Consistent curve across all channel configurations
- FAIL: Different curve shapes or channel imbalance

---

#### Test 6.6: Apply Without Preview

**Priority:** P2
**Test File:** `speech_mono_30sec.wav`

**Steps:**
1. Select region, open Fade In dialog
2. Select "Logarithmic" curve
3. Click "Apply" WITHOUT clicking "Preview" first
4. Verify fade is applied correctly
5. Play back to verify audio

**Expected Results:**
- Apply works without requiring preview first
- Correct curve is applied
- Preference is saved

**Pass/Fail Criteria:**
- PASS: Apply works independently of preview
- FAIL: Apply requires preview or applies wrong curve

---

#### Test 6.7: Cancel During Preview

**Priority:** P2
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Select 10-second region, open Fade In dialog
2. Enable "Loop Preview"
3. Click "Preview" to start looping playback
4. While playback is looping, click "Cancel"
5. Verify playback stops
6. Verify no fade was applied to audio

**Expected Results:**
- Playback stops immediately on Cancel
- No changes to audio buffer
- Preview mode is disabled

**Pass/Fail Criteria:**
- PASS: Clean cancel with no audio modification
- FAIL: Playback continues or fade is applied

---

#### Test 6.8: Rapid Curve Switching During Preview

**Priority:** P3 (Low - stress test)
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Select 5-second region, open Fade In dialog
2. Enable "Loop Preview", click "Preview"
3. Rapidly change curve type: Linear → Exponential → Logarithmic → S-Curve → Linear (repeat)
4. Cycle through all curves 10 times quickly
5. Click "Apply"

**Expected Results:**
- No crashes or hangs
- Each curve change triggers preview update
- Final applied curve matches last selected curve
- No audio glitches or buffer corruption

**Pass/Fail Criteria:**
- PASS: Stable operation under rapid switching
- FAIL: Crash, hang, or audio corruption

---

### CATEGORY 7: Workflow Integration (20 minutes)

#### Test 7.1: Complete Fade In Workflow

**Priority:** P0 (Critical Path)
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Open file
2. Select region (0-5 seconds)
3. Press `Cmd+Shift+I`
4. Dialog opens with last-used curve
5. Change to "S-Curve"
6. Click "Preview" and listen
7. Enable "Loop Preview" and listen again
8. Click "Apply"
9. Dialog closes
10. Verify waveform shows fade
11. Play back to verify audio
12. Press `Cmd+Z` to undo
13. Press `Cmd+Shift+Z` to redo
14. Verify fade restored

**Expected Results:**
- Entire workflow is smooth and intuitive
- Each step works as expected
- Audio quality is perfect

**Pass/Fail Criteria:**
- PASS: Complete workflow succeeds without issues
- FAIL: ANY step fails or produces incorrect results

---

#### Test 7.2: Complete Fade Out Workflow

**Priority:** P0 (Critical Path)
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Open file
2. Select region (55-60 seconds)
3. Press `Cmd+Shift+O`
4. Dialog opens with last-used curve
5. Change to "Logarithmic"
6. Click "Preview" and listen
7. Enable "Loop Preview" and listen again
8. Click "Apply"
9. Dialog closes
10. Verify waveform shows fade
11. Play back to verify audio
12. Press `Cmd+Z` to undo
13. Press `Cmd+Shift+Z` to redo
14. Verify fade restored

**Expected Results:**
- Entire workflow is smooth and intuitive
- Each step works as expected
- Audio quality is perfect

**Pass/Fail Criteria:**
- PASS: Complete workflow succeeds without issues
- FAIL: ANY step fails or produces incorrect results

---

#### Test 7.3: Crossfade Creation (Fade Out + Fade In)

**Priority:** P2
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Select 10-15 seconds
2. Apply Fade Out "S-Curve" to this region
3. Select 15-20 seconds (overlapping or adjacent)
4. Apply Fade In "S-Curve" to this region
5. Play back the crossfade region (10-20 seconds)

**Expected Results:**
- Smooth crossfade between the two regions
- No volume dip or spike in the middle
- Natural-sounding transition

**Pass/Fail Criteria:**
- PASS: Professional-quality crossfade
- FAIL: Audible artifacts or volume issues

---

#### Test 7.4: Save After Fade Application

**Priority:** P1
**Test File:** `mono_1khz_10sec.wav`

**Steps:**
1. Apply Fade In "Exponential" to selection
2. Press `Cmd+S` (Save)
3. Close file (do not save again)
4. Re-open the saved file
5. Verify fade is permanently saved

**Expected Results:**
- Fade is written to disk
- Re-opened file contains the fade
- No data loss or corruption

**Pass/Fail Criteria:**
- PASS: Fade persists after save/reload
- FAIL: Fade lost or file corrupted

---

#### Test 7.5: Export Region with Fade

**Priority:** P2
**Test File:** `stereo_music_60sec.wav`

**Steps:**
1. Create region from 10-15 seconds (use Region Manager)
2. Apply Fade In "Linear" to this region
3. Apply Fade Out "Linear" to same region
4. Export this region as WAV (use Export Region command)
5. Open exported file in new tab
6. Verify fades are present in exported file

**Expected Results:**
- Exported file contains the fades
- Fades are sample-accurate
- No extra audio before/after fade regions

**Pass/Fail Criteria:**
- PASS: Exported file has correct fades
- FAIL: Fades missing or incorrect in export

---

## Summary Metrics

### Test Coverage

- **Total Test Cases:** 45
- **P0 (Critical):** 12 tests
- **P1 (High):** 21 tests
- **P2 (Medium):** 11 tests
- **P3 (Low):** 1 test

### Time Allocation

| Category | Test Count | Estimated Time |
|----------|------------|----------------|
| Dialog UI and Controls | 5 | 30 minutes |
| Audio Quality - Curve Accuracy | 10 | 60 minutes |
| Settings Persistence | 5 | 20 minutes |
| Keyboard Shortcut Integration | 2 | 15 minutes |
| Undo/Redo Integration | 5 | 30 minutes |
| Edge Cases and Error Handling | 8 | 30 minutes |
| Workflow Integration | 5 | 20 minutes |
| **TOTAL** | **40** | **205 minutes (~3.5 hours)** |

### Pass/Fail Criteria

**Feature is READY FOR RELEASE if:**
- All P0 tests PASS
- At least 90% of P1 tests PASS
- At least 70% of P2 tests PASS
- Zero crashes or data corruption bugs

**Feature REQUIRES FIXES if:**
- Any P0 test FAILS
- More than 2 P1 tests FAIL
- Any crash or data corruption occurs

---

## Bug Reporting Template

When you find an issue, report it using this format:

```
BUG #XXX: [Short Description]

Severity: P0 / P1 / P2 / P3
Category: [UI / Audio Quality / Persistence / Keyboard / Undo / Edge Case / Workflow]
Test Case: [Reference test number, e.g., Test 2.2]

Reproduction Steps:
1. [Exact step-by-step instructions]
2. [Include file names and settings]
3. [Include selections and curve types]

Expected Behavior:
[What should happen]

Actual Behavior:
[What actually happened]

Evidence:
- Screenshot: [attach if relevant]
- Audio recording: [attach if audio artifact]
- Console logs: [paste if crash/error]

Workaround:
[If you found a temporary solution, describe it here]

Root Cause Hypothesis:
[Your best guess at the technical cause]
```

---

## Test Completion Checklist

After completing all tests, verify:

- [ ] All P0 tests executed and documented
- [ ] All P1 tests executed and documented
- [ ] All P2 tests executed and documented
- [ ] All bugs reported with complete information
- [ ] Regression tests passed (no old bugs resurfaced)
- [ ] Performance is acceptable (no UI lag or audio glitches)
- [ ] Documentation is accurate (README updated if needed)
- [ ] Test files archived for future regression testing

---

## Notes for Tester

### Listening Environment
- Use high-quality headphones for perceptual tests
- Test in quiet environment
- Volume should be comfortable but loud enough to hear details

### Common Pitfalls to Watch For
1. **Curve shape confusion:** Exponential fade-out should NOT sound like exponential fade-in reversed
2. **Preference bleeding:** Fade In and Fade Out curves should be independent
3. **Sample-accurate boundaries:** Check for clicks at 0% and 100% points
4. **Multi-channel uniformity:** All channels must fade identically
5. **Undo/redo integrity:** Each undo must perfectly reverse the previous action

### Performance Benchmarks
- Dialog open time: < 100ms
- Preview start time: < 200ms
- Apply time (5 seconds @ 48kHz): < 100ms
- Undo time: < 50ms

---

**Document Version:** 1.0
**Last Updated:** 2025-12-01
**Prepared by:** Audio QA Specialist (Claude Code)
