# QA Test Plan: ParametricEQ Preview Optimization & RMS Normalization

**Feature Release:** v0.5.0
**Test Date:** December 2, 2025
**QA Specialist:** Audio Editor QA
**Severity Levels:** P0 (Blocker), P1 (Major), P2 (Minor), P3 (Cosmetic)

---

## Executive Summary

This document outlines comprehensive QA testing for two critical features:

1. **ParametricEQ Preview Optimization** - Real-time parameter updates during preview playback
2. **RMS Normalization Mode** - RMS level mode added to NormalizeDialog with persistence

Both features integrate deeply with the AudioEngine preview system and require careful testing of:
- Real-time DSP parameter updates (zero artifacts)
- Coordinate system integrity during preview
- Mode persistence and UI state consistency
- Playback + preview interactions across multiple documents

---

## Part 1: ParametricEQ Preview Optimization Testing

### Feature Overview

**User Workflow:**
1. Open audio file, select region
2. Open ParametricEQ dialog with preview enabled
3. Adjust sliders (Freq, Gain, Q) during playback
4. Hear changes in real-time (no buffer reload)
5. Apply changes → Undo/Redo works
6. Loop mode available for repeated listening

**Architecture Notes:**
- Preview Mode: `PreviewMode::REALTIME_DSP` (instant, no latency)
- Parameter updates via `AudioEngine::setParametricEQPreview()`
- Atomic parameter exchange (no locks, real-time safe)
- Filter coefficient updates only when parameters change
- Audio thread picks up new coefficients on next block

---

### 1.1 Rapid Slider Movement Testing

**Test Case: 1.1.1 - Gain Slider Rapid Adjustments**

Objective: Verify no audio artifacts during rapid gain changes

Steps:
1. Open `/TestFiles/moans.wav`
2. Select entire file (Cmd+A)
3. Open ParametricEQ dialog
4. Click "Preview" → playback starts from selection
5. Rapidly drag Low Shelf Gain slider from -20dB to +20dB and back (10 cycles, ~2 seconds)
6. Observe playback during adjustment

Expected Results:
- Audio level changes smoothly
- No clicks, pops, or discontinuities
- Playback remains uninterrupted
- Waveform display stays responsive (60fps)

Failure Criteria (P0):
- Audible artifacts at slider boundaries
- Playback stutters or buffers
- Audio drops out during adjustment

---

**Test Case: 1.1.2 - Frequency Slider Extreme Range**

Objective: Verify smooth frequency transitions across full range

Steps:
1. Open `/TestFiles/moans.wav`
2. Select middle section (5-10 seconds)
3. Open ParametricEQ dialog
4. Click "Preview"
5. Mid Peak Frequency slider: 20 Hz → 20 kHz (logarithmic sweep, 5 seconds)
6. Listen for resonance artifacts or filter instability

Expected Results:
- Smooth frequency sweep with no resonance peaks
- No filter instability at extreme frequencies (20 Hz, 20 kHz)
- Cutoff behavior matches standard parametric EQ
- Playback stable throughout

Failure Criteria (P1):
- Filter instability (ringing, resonance)
- Frequency jumps instead of smooth transition
- Audio distortion at filter boundaries

---

**Test Case: 1.1.3 - Q Factor Rapid Adjustment**

Objective: Verify Q changes don't cause filter state artifacts

Steps:
1. Open `/TestFiles/moans.wav` (speech/music with harmonic content)
2. Select region with clear harmonics
3. Open ParametricEQ dialog
4. Set Mid Peak to 1000 Hz, +6dB gain
5. Click "Preview"
6. Rapidly toggle Q slider between 0.1 and 10.0 (5 cycles)
7. Listen for resonance changes

Expected Results:
- Sharp Q (10.0) produces narrow peak
- Low Q (0.1) produces broad peak
- Transition smooth, no phase artifacts
- Playback continuous

Failure Criteria (P1):
- Filter coefficients cause instability (ringing)
- Audible pops at Q transitions
- Playback interruption

---

### 1.2 All Three Bands Parameter Testing

**Test Case: 1.2.1 - Independent Band Control**

Objective: Verify each band operates independently without crosstalk

Setup: Use test signal with discrete frequency components (or use sweep file)

Steps:
1. Open audio file with multi-frequency content
2. Select entire file
3. Open ParametricEQ dialog
4. Preview playback
5. Adjust only Low Shelf gain (+12 dB) → listen
6. Reset Low, adjust Mid Peak gain (-6 dB) → listen
7. Reset Mid, adjust High Shelf gain (+12 dB) → listen
8. Verify each band's effect is isolated

Expected Results:
- Low band (100 Hz default) affects bass only
- Mid band (1000 Hz default) affects midrange only
- High band (10000 Hz default) affects treble only
- No crosstalk between bands
- Gain changes proportional to slider position

Failure Criteria (P1):
- Band affects wrong frequency range
- Gain not applied correctly
- Crosstalk between bands

---

**Test Case: 1.2.2 - Multi-Band Simultaneous Adjustment**

Objective: Verify all bands can be adjusted together during playback

Steps:
1. Open `/TestFiles/moans.wav`
2. Select region
3. Open ParametricEQ dialog → Preview
4. While playback running:
   - Low Shelf: +8 dB (boost bass)
   - Mid Peak: +4 dB at 2 kHz (boost vocals)
   - High Shelf: +6 dB (boost presence)
5. Listen to cumulative effect
6. Adjust sliders slightly to fine-tune
7. Apply changes

Expected Results:
- All bands apply simultaneously
- Cumulative effect audible
- No audio glitches during multi-band adjustment
- Filter stability maintained

Failure Criteria (P1):
- One band doesn't apply when others are active
- Audio quality degrades with multiple bands
- Playback stutters during multi-adjustment

---

### 1.3 Extreme Settings Testing

**Test Case: 1.3.1 - Maximum Boost (+20 dB All Bands)**

Objective: Verify extreme gain doesn't cause clipping or instability

Steps:
1. Open `/TestFiles/moans.wav` (verify peak level < -3dB before test)
2. Select region
3. Open ParametricEQ → Preview
4. Set all bands to +20 dB gain
5. Play entire selection
6. Watch meters for clipping
7. Listen for distortion or artifacts

Expected Results:
- Audio level increases significantly
- No filter instability
- Meters show increase (may show clipping if file was loud)
- Playback clear (no distortion from EQ itself)

Failure Criteria (P1):
- EQ introduces distortion independent of gain
- Filter becomes unstable at high gains
- Playback interrupts

---

**Test Case: 1.3.2 - Maximum Cut (-20 dB All Bands)**

Objective: Verify extreme attenuation doesn't cause problems

Steps:
1. Open `/TestFiles/moans.wav`
2. Select region
3. Open ParametricEQ → Preview
4. Set all bands to -20 dB
5. Play entire selection
6. Listen for audio quality changes

Expected Results:
- Audio becomes very quiet but clear
- No increased noise floor
- Filter stable at extreme attenuation
- Playback uninterrupted

Failure Criteria (P1):
- Noise floor increases
- Filter introduces artifacts
- Playback issues

---

**Test Case: 1.3.3 - Extreme Frequency Values (20 Hz, 20 kHz)**

Objective: Verify filter stability at frequency extremes

Steps:
1. Open `/TestFiles/moans.wav`
2. Select region
3. Open ParametricEQ → Preview
4. Set Mid Peak frequency to 20 Hz, gain +6dB, Q 0.707
5. Play → listen
6. Change frequency to 20 kHz → listen
7. Verify no filter ringing or instability

Expected Results:
- Filter operates correctly at both extremes
- No ringing or instability
- Coefficients clamp properly (internal check)
- Playback stable

Failure Criteria (P1):
- Filter instability (ringing, feedback)
- Frequency values not clamped to Nyquist
- Audio quality issues

---

### 1.4 Loop Preview Mode Testing

**Test Case: 1.4.1 - Loop Toggle During Preview**

Objective: Verify loop behavior for repeated audition

Steps:
1. Open `/TestFiles/moans.wav`
2. Select middle section (5-10 seconds)
3. Open ParametricEQ dialog
4. Uncheck "Loop"
5. Click "Preview" → playback starts, plays to end, stops
6. Click "Preview" again → stops playback
7. Check "Loop"
8. Click "Preview" → playback starts
9. Wait for playback to reach end → should restart from beginning
10. Adjust EQ slider while looping
11. Click "Preview" again → stops

Expected Results:
- Loop toggle OFF: Playback stops at end
- Loop toggle ON: Playback restarts at selection start
- Parameter changes apply during loop playback
- Stop button works correctly

Failure Criteria (P1):
- Loop doesn't restart at selection boundary
- Loop points inaccurate (off by samples)
- Can't adjust parameters during loop

---

**Test Case: 1.4.2 - Loop Boundaries Accuracy**

Objective: Verify selection bounds are honored during loop

Setup: Use audio with distinctive markers at 5s and 10s positions

Steps:
1. Open `/TestFiles/moans.wav`
2. Select precisely 5.000s to 10.000s (selection range tool)
3. Open ParametricEQ dialog
4. Preview with Loop enabled
5. Listen to ensure:
   - Playback starts exactly at 5.000s
   - Playback stops exactly at 10.000s
   - Loop restart is seamless
   - Cursor position reflects selection offset

Expected Results:
- Selection start/end honored (sample-accurate)
- Loop restart seamless (no clicks)
- Cursor position correct during preview
- No audio dropout at loop boundary

Failure Criteria (P1):
- Loop boundary off by >100ms
- Audible click at loop restart
- Cursor position wrong

---

### 1.5 Preview → Apply → Undo Workflow Testing

**Test Case: 1.5.1 - Apply Changes Persist After Dialog Close**

Objective: Verify parameter changes apply correctly to audio buffer

Steps:
1. Open `/TestFiles/moans.wav`
2. Select region (0:00-0:10)
3. Open ParametricEQ dialog
4. Set Mid Peak: frequency=2000Hz, gain=+6dB, Q=1.0
5. Click "Preview" → hear changes
6. Stop preview (click Preview button again)
7. Click "Apply"
8. Dialog closes
9. Play file from beginning → hear EQ applied

Expected Results:
- EQ effect visible in waveform (subtle change)
- Playback reflects EQ applied
- Undo history shows "Apply ParametricEQ" operation

Failure Criteria (P0):
- Changes don't apply to buffer
- Playback doesn't reflect changes
- Undo history missing entry

---

**Test Case: 1.5.2 - Undo Reverses EQ Application**

Objective: Verify undo/redo chain works correctly

Steps:
1. Apply EQ from Test 1.5.1
2. Play file → hear EQ
3. Press Cmd+Z (Undo)
4. Play file → should hear original audio (no EQ)
5. Press Cmd+Shift+Z (Redo)
6. Play file → should hear EQ again

Expected Results:
- Undo perfectly reverses EQ (audio identical to original)
- Redo re-applies EQ
- Waveform display updates
- Multiple undo/redo works correctly

Failure Criteria (P0):
- Undo doesn't reverse changes
- Audio quality differs after undo/redo
- Undo history corrupted

---

**Test Case: 1.5.3 - Cancel Dialog Doesn't Apply Changes**

Objective: Verify cancel preserves original audio

Steps:
1. Open `/TestFiles/moans.wav`
2. Select region
3. Open ParametricEQ dialog
4. Set extreme EQ (all bands +20dB)
5. Preview → hear changes
6. Click "Cancel"
7. Play file → should hear original (no EQ applied)

Expected Results:
- Original audio preserved
- No undo history entry created
- Waveform unchanged

Failure Criteria (P0):
- EQ applied despite Cancel
- Audio changed

---

### 1.6 Multi-File Preview Interaction Testing

**Test Case: 1.6.1 - Preview Mode Mutes Other Documents**

Objective: Verify only one document can preview at a time

Steps:
1. Open `/TestFiles/moans.wav` in Tab 1
2. Open `/TestFiles/moans.wav` in Tab 2 (same or different file)
3. Switch to Tab 1
4. Open ParametricEQ dialog → Click Preview
5. Playback starts from Tab 1
6. Switch to Tab 2
7. Open Gain dialog → Click Preview
8. Playback should start from Tab 2, Tab 1 should be muted

Expected Results:
- Only active preview plays audio
- Other documents muted during preview
- Switching tabs updates which preview is audible
- No audio mixing between documents

Failure Criteria (P1):
- Multiple documents play simultaneously
- Audio from non-preview documents audible
- Can't distinguish which document is previewing

---

**Test Case: 1.6.2 - Preview Selection Offset Correct Across Tabs**

Objective: Verify cursor position correct during preview when switching tabs

Steps:
1. Tab 1: Open file, select 5s-10s region
2. Open ParametricEQ → Preview
3. Let playback run for 2 seconds
4. Tab 2: Open different file, select 10s-15s region
5. Open ParametricEQ → Preview
6. Observe cursor position in Tab 2 (should be at 10s + elapsed time)
7. Switch back to Tab 1
8. Observe cursor position in Tab 1 (preview stopped)

Expected Results:
- Tab 2 cursor shows correct position (10s + 2s = 12s)
- Tab 1 preview stopped
- Switching tabs stops active preview
- Coordinate system correct across documents

Failure Criteria (P1):
- Cursor position wrong
- Preview doesn't stop when switching tabs
- Selection offset not applied

---

### 1.7 Parameter Persistence Testing

**Test Case: 1.7.1 - Dialog Initialization with Current Settings**

Objective: Verify dialog shows last-used parameter values

Steps:
1. Open file
2. Open ParametricEQ dialog
3. Set: Low +6dB, Mid frequency 1500Hz, High +3dB
4. Click Cancel
5. Open ParametricEQ dialog again
6. Verify sliders show saved values

Expected Results:
- Sliders initialized to last values
- No reset to defaults
- UI matches last state

Failure Criteria (P2):
- Sliders reset to defaults
- Previous values lost

---

### 1.8 UI State and Responsiveness Testing

**Test Case: 1.8.1 - Button State Reflects Preview Playback**

Objective: Verify UI accurately reflects playback state

Steps:
1. Open file → Select region
2. Open ParametricEQ dialog
3. Click "Preview" → button text should change to "Stop Preview"
4. Button color should change to dark red
5. While preview running, adjust slider → button stays red
6. Click "Preview" again → stops, button text changes back to "Preview"
7. Button color reverts to default

Expected Results:
- Preview button text toggles correctly
- Button color indicates active preview
- State consistent with actual playback

Failure Criteria (P2):
- Button text doesn't update
- Color doesn't change
- UI state doesn't match actual playback

---

**Test Case: 1.8.2 - Value Label Updates in Real-Time**

Objective: Verify slider value displays update instantly

Steps:
1. Open ParametricEQ dialog
2. Drag Gain slider slowly from 0 to +10dB
3. Observe value label updates continuously
4. Label should show: "+0.1 dB", "+0.2 dB", ..., "+10.0 dB"

Expected Results:
- Value label updates every frame
- No lag or stuttering
- Format consistent (" dB" suffix, sign prefix)

Failure Criteria (P2):
- Label updates with lag
- Updates skip values
- Incorrect formatting

---

**Test Case: 1.8.3 - Dialog Responsiveness During Preview**

Objective: Verify UI doesn't stutter during real-time processing

Steps:
1. Open large audio file (>30 minutes)
2. Open ParametricEQ dialog
3. Click Preview
4. Drag sliders smoothly across range
5. Measure UI frame rate (should be 60fps)

Expected Results:
- Slider dragging smooth and responsive
- Dialog doesn't freeze or stutter
- Value labels update smoothly
- No perceptible lag

Failure Criteria (P1):
- UI stutters during slider adjustment
- Frame drops below 30fps
- Dialog becomes unresponsive

---

## Part 2: RMS Normalization Testing

### Feature Overview

**User Workflow:**
1. Open audio file, select region
2. Open Normalize dialog
3. Switch mode from "Peak Level" to "RMS Level"
4. Dialog calculates both Peak and RMS values
5. Adjust target RMS level
6. Preview normalized audio
7. Apply normalization (calculates required gain, applies, undo works)

**Architecture Notes:**
- Mode selection stored in Settings ("dsp.normalizeMode", 0=Peak, 1=RMS)
- RMS calculated via `AudioProcessor::getRMSLevelDB()`
- Preview mode: `PreviewMode::OFFLINE_BUFFER` (pre-rendered copy)
- Supports loop during preview
- Mode persists across dialog instances

---

### 2.1 Mode Switching Testing

**Test Case: 2.1.1 - Peak to RMS Mode Switch**

Objective: Verify mode selector changes UI and calculations

Steps:
1. Open `/TestFiles/moans.wav`
2. Select entire file
3. Open Normalize dialog
4. Verify default is "Peak Level" selected
5. Verify "Current Peak" label visible, "Current RMS" also visible
6. Click mode dropdown → select "RMS Level"
7. Verify UI updates:
   - "Target Peak Level:" → "Target RMS Level:"
   - Required gain recalculated
   - Current RMS value highlighted

Expected Results:
- Mode changes instantly
- Labels update correctly
- Required gain shows difference between current RMS and target
- No calculation errors

Failure Criteria (P1):
- Mode doesn't change
- Labels don't update
- Calculation incorrect

---

**Test Case: 2.1.2 - RMS to Peak Mode Switch**

Objective: Verify reverse mode switch works

Steps:
1. From Test 2.1.1, mode is RMS
2. Click mode dropdown → select "Peak Level"
3. Verify UI reverts:
   - "Target RMS Level:" → "Target Peak Level:"
   - Required gain recalculates based on current Peak

Expected Results:
- Mode switches smoothly
- Labels revert to Peak terminology
- Required gain correct for Peak normalization

Failure Criteria (P1):
- Mode doesn't switch back
- Calculations incorrect

---

**Test Case: 2.1.3 - Mode Switch Updates All Values**

Objective: Verify all dependent values recalculate on mode switch

Steps:
1. Open file with known Peak: -6dB, RMS: -12dB
2. Open Normalize dialog
3. Set Target: -0.1dB (Peak mode)
4. Verify Required Gain = -0.1 - (-6) = +5.9dB
5. Switch to RMS mode
6. Verify Required Gain = -0.1 - (-12) = +11.9dB
7. Verify label shows "Target RMS Level:"

Expected Results:
- All values recalculate correctly
- Math is accurate (Target - Current = Gain)
- Labels match selected mode

Failure Criteria (P1):
- Values don't recalculate
- Math incorrect
- Labels inconsistent

---

### 2.2 RMS Calculation Accuracy Testing

**Test Case: 2.2.1 - RMS Calculation on Known Signal**

Objective: Verify RMS calculation is mathematically correct

Setup: Generate or use test signal with known RMS (e.g., -3dB sine wave)

Steps:
1. Create test signal: 1 kHz sine wave at 0dB peak (RMS should be -3.01dB)
2. Load into WaveEdit
3. Select entire file
4. Open Normalize dialog, switch to RMS mode
5. Verify "Current RMS:" shows approximately -3 dB

Expected Results:
- RMS calculated correctly (-3.01 dB ± 0.1 dB)
- Calculation matches reference implementation
- No rounding errors

Failure Criteria (P1):
- RMS incorrect by >0.5 dB
- Calculation inconsistent

---

**Test Case: 2.2.2 - RMS vs Peak on Various Audio Types**

Objective: Verify RMS > Peak is never true (RMS always lower)

Test with multiple file types:
1. Speech (dynamic, RMS should be -15 to -12 dB below Peak)
2. Music (steady, RMS should be -6 to -10 dB below Peak)
3. Bass tone (sustained, RMS close to Peak, -3 to -6 dB)
4. Pink noise (balanced, RMS should be -3 to -5 dB below Peak)

Steps for each:
1. Load file
2. Select entire file
3. Open Normalize dialog
4. Note Peak and RMS values
5. Verify RMS < Peak (always)
6. Verify RMS/Peak ratio is reasonable

Expected Results:
- RMS always lower than Peak
- Ratios within expected ranges
- Values consistent across multiple tests

Failure Criteria (P1):
- RMS >= Peak (mathematically impossible)
- RMS unreasonably high or low

---

**Test Case: 2.2.3 - RMS on Silent Selection**

Objective: Verify RMS calculation on silence

Steps:
1. Create 10-second selection of silence
2. Open Normalize dialog, switch to RMS mode
3. Verify "Current RMS:" shows very low value (e.g., -100dB or -INF)

Expected Results:
- RMS shows -INF or very low dB value
- Not NaN or undefined
- Doesn't crash dialog

Failure Criteria (P0):
- RMS shows NaN or invalid
- Dialog crashes
- Calculation error

---

**Test Case: 2.2.4 - RMS on Partial Selection**

Objective: Verify RMS calculation respects selection bounds

Steps:
1. Load `/TestFiles/moans.wav`
2. Select first 5 seconds
3. Note RMS value
4. Select entire file
5. Note RMS value (should be different)
6. Select last 5 seconds
7. Note RMS value
8. Verify partial selections give different values than full file

Expected Results:
- RMS changes based on selection
- Different selections give different values
- Values reflect actual audio in selection

Failure Criteria (P1):
- RMS always same regardless of selection
- Calculation ignores selection bounds

---

### 2.3 RMS Normalization Preview Testing

**Test Case: 2.3.1 - Preview RMS Normalization**

Objective: Verify preview applies correct gain for RMS mode

Steps:
1. Load audio with RMS = -12dB
2. Select entire file
3. Open Normalize dialog → RMS mode
4. Set Target RMS = -6dB
5. Verify Required Gain = -6 - (-12) = +6dB
6. Click "Preview"
7. Playback starts
8. Listen to audio (should be louder by 6dB)
9. Compare to original peak level (should not exceed 0dB if original peak was -6dB)

Expected Results:
- Preview audio is louder (correct gain applied)
- No distortion from excessive gain
- Playback smooth
- Loop button works if checked

Failure Criteria (P1):
- Preview gain incorrect
- Preview audio distorted/clipped
- Playback interrupts

---

**Test Case: 2.3.2 - Preview Gain Calculation Accuracy**

Objective: Verify preview applies exact calculated gain

Steps:
1. Load audio with known RMS = -15dB, Peak = -3dB
2. Select region
3. Open Normalize → RMS mode
4. Target RMS = -3dB
5. Required Gain = -3 - (-15) = +12dB
6. Verify meter during preview shows +12dB increase
7. Peak should reach approximately 0dB (from -3dB + 12dB)

Expected Results:
- Preview gain = +12dB ± 0.1dB
- Peak level increases correctly
- No rounding errors in calculation

Failure Criteria (P1):
- Gain incorrect by >0.5 dB
- Meter doesn't match calculation

---

**Test Case: 2.3.3 - RMS Preview with Loop Mode**

Objective: Verify loop preview works correctly with RMS mode

Steps:
1. Load file, select middle section (5-10 seconds)
2. Open Normalize dialog → RMS mode
3. Enable "Loop Preview"
4. Click "Preview"
5. Listen as playback loops from 5s to 10s repeatedly
6. Adjust Target RMS slider → listen to change
7. Verify loop restart is seamless

Expected Results:
- Preview loops at selection boundary
- No click at loop restart
- Parameter changes audible during loop
- Stop button works

Failure Criteria (P1):
- Loop doesn't restart
- Audible click at boundary
- Can't adjust parameters during loop

---

### 2.4 Settings Persistence Testing

**Test Case: 2.4.1 - Mode Selection Persists Across Dialog Instances**

Objective: Verify mode setting saved and restored

Steps:
1. Open file 1, select region
2. Open Normalize dialog
3. Switch to "RMS Level" mode
4. Set Target RMS = -4dB
5. Close dialog (Cancel)
6. Reopen Normalize dialog
7. Verify "RMS Level" is selected (not Peak)
8. Verify Target RMS value preserved

Expected Results:
- Mode setting persists
- Dropdown shows RMS as selected
- Target value preserved

Failure Criteria (P2):
- Mode resets to Peak
- Setting not saved

---

**Test Case: 2.4.2 - Mode Persistence Across Application Restart**

Objective: Verify mode saved to settings file

Steps:
1. Open WaveEdit
2. Open any file
3. Open Normalize dialog → switch to RMS mode
4. Close dialog
5. Close WaveEdit completely
6. Relaunch WaveEdit
7. Open file, open Normalize dialog
8. Verify RMS mode is pre-selected

Expected Results:
- RMS mode persists across restart
- Settings file includes mode

Failure Criteria (P1):
- Mode resets to Peak on restart
- Settings not persisted

---

**Test Case: 2.4.3 - Target Level Value Not Persisted (Should Reset)**

Objective: Verify target level resets but mode persists

Steps:
1. Open file 1, Normalize dialog, set Target RMS = -6dB
2. Close dialog
3. Open file 2, Normalize dialog
4. Verify Target shows default (-0.1dB)
5. Verify mode is RMS (persisted from step 1)

Expected Results:
- Mode persists (RMS)
- Target value resets to default
- No carry-over of settings between different files

Failure Criteria (P2):
- Target value carries over
- Unexpected defaults

---

### 2.5 Multi-Mode Workflow Testing

**Test Case: 2.5.1 - Switch Between Peak and RMS During Editing Session**

Objective: Verify user can freely switch modes

Steps:
1. Load file, select region
2. Open Normalize → Peak mode, set Target -0.1dB
3. Click "Preview" → listen to peak-normalized audio
4. Switch to RMS mode → Required Gain updates
5. Adjust Target RMS to -6dB
6. Click "Preview" → listen to RMS-normalized audio
7. Compare: RMS normalized should sound louder (more aggressive normalization for speech)
8. Switch back to Peak mode
9. Verify Required Gain shows Peak calculation again

Expected Results:
- Switching modes updates all calculations
- Preview reflects correct mode
- User can A/B compare both normalizations
- Required Gain values make sense for each mode

Failure Criteria (P1):
- Mode switch doesn't update preview
- Calculations incorrect
- Can't compare modes side-by-side

---

**Test Case: 2.5.2 - Apply After Mode Switch**

Objective: Verify applying normalization uses correct mode

Steps:
1. Load file with RMS = -12dB
2. Open Normalize → Peak mode
3. Set Target Peak = -0.1dB (but this is wrong mode for speech)
4. Switch to RMS mode
5. Set Target RMS = -6dB
6. Click "Apply"
7. Verify undo history shows "RMS Normalization" (not Peak)

Expected Results:
- Apply uses current mode (RMS)
- Correct gain applied
- Undo message reflects RMS operation

Failure Criteria (P0):
- Wrong mode applied
- Undo message shows wrong operation

---

### 2.6 Edge Case Testing

**Test Case: 2.6.1 - RMS Normalization to Same Level (No Change)**

Objective: Verify no gain applied when current = target

Steps:
1. Load file with RMS = -6dB
2. Open Normalize → RMS mode
3. Set Target RMS = -6dB
4. Verify Required Gain = 0.0dB
5. Click "Preview" → audio should sound identical
6. Click "Apply" → undo should show operation (even with 0dB gain)

Expected Results:
- Required Gain shows 0.0dB
- No gain applied (or 0dB gain applied)
- Playback identical
- Undo still works (idempotent operation)

Failure Criteria (P2):
- Gain calculation wrong
- Operation doesn't appear in undo

---

**Test Case: 2.6.2 - RMS with Extreme Target (-60 dB)**

Objective: Verify extreme normalization doesn't cause issues

Steps:
1. Load normal audio
2. Open Normalize → RMS mode
3. Set Target RMS = -60dB
4. Verify Required Gain is large negative number (-40 to -50dB)
5. Click "Preview" → audio should be extremely quiet but not silent
6. Required Gain label should be orange/red (warning color)

Expected Results:
- Extreme targets handled gracefully
- Gain displayed with warning color
- Preview audio reflects extreme attenuation
- No crashes

Failure Criteria (P1):
- Calculation error
- No warning color
- Preview broken

---

**Test Case: 2.6.3 - RMS with Extreme Target (+20 dB)**

Objective: Verify extreme boost doesn't cause issues

Steps:
1. Load quiet audio (RMS = -20dB, Peak = -10dB)
2. Open Normalize → RMS mode
3. Set Target RMS = 0dB (invalid, but test robustness)
4. Verify Required Gain = 0 - (-20) = +20dB
5. Required Gain label should be RED (excessive gain warning)
6. Click "Preview" → audio should be boosted but clipping at +10dB peak

Expected Results:
- Extreme gain shown with red warning
- Preview applies gain (may show clipping meters)
- No crash
- User warned of excessive gain

Failure Criteria (P1):
- No warning color
- Calculation error
- Preview fails

---

**Test Case: 2.6.4 - RMS with Very Small Selection (< 100ms)**

Objective: Verify RMS calculates correctly on short audio

Steps:
1. Load file
2. Select tiny region (50ms)
3. Open Normalize → RMS mode
4. Verify "Current RMS" calculates (not blank or error)
5. Value should be valid

Expected Results:
- RMS calculates on tiny selection
- Value reasonable (not NaN or error)
- Dialog doesn't freeze

Failure Criteria (P1):
- RMS error or invalid
- Dialog freezes
- Calculation skipped

---

### 2.7 UI Display and Layout Testing

**Test Case: 2.7.1 - Both Peak and RMS Values Always Displayed**

Objective: Verify both levels shown regardless of mode

Steps:
1. Open Normalize dialog
2. Verify layout shows:
   - "Current Peak:" label + value
   - "Current RMS:" label + value
   - "Target Peak Level:" or "Target RMS Level:" (changes with mode)
   - "Required Gain:" label + value
3. Switch mode
4. Verify both Peak and RMS values still visible

Expected Results:
- Both Peak and RMS always displayed
- Prevents user confusion
- Layout clean and organized

Failure Criteria (P2):
- One value hidden based on mode
- Layout cramped or unclear

---

**Test Case: 2.7.2 - Target Level Label Changes with Mode**

Objective: Verify label accurately reflects selected mode

Steps:
1. Open Normalize → Peak mode
2. Verify label says "Target Peak Level:"
3. Switch to RMS mode
4. Verify label changes to "Target RMS Level:"
5. Switch back to Peak
6. Verify label reverts to "Target Peak Level:"

Expected Results:
- Label updates instantly with mode
- No confusion about which parameter being adjusted
- Grammar correct ("Peak" vs "RMS")

Failure Criteria (P2):
- Label doesn't update
- Grammatical errors

---

**Test Case: 2.7.3 - Dialog Sizing with Mode Change**

Objective: Verify layout doesn't break when mode changes

Steps:
1. Open Normalize dialog (standard size)
2. Resize window (drag corner to make larger/smaller)
3. Switch mode
4. Verify all controls still visible and aligned
5. Text not cut off
6. Labels and values line up

Expected Results:
- Layout robust to size changes
- All controls visible at all sizes
- No overlapping or misalignment

Failure Criteria (P2):
- Controls hidden at certain sizes
- Text cut off
- Misalignment

---

### 2.8 Gain Warning Color Testing

**Test Case: 2.8.1 - Required Gain Color Indicators**

Objective: Verify visual warnings for excessive gain

Steps:
1. Open file with RMS = -20dB
2. Open Normalize → RMS mode
3. Target RMS = -6dB
4. Required Gain = +14dB → should show ORANGE (warning)
5. Adjust Target to 0dB
6. Required Gain = +20dB → should show RED (critical)
7. Adjust Target to -18dB
8. Required Gain = +2dB → should show DEFAULT color (safe)

Expected Results:
- Colors: Default (safe) < Orange (+12-24dB) < Red (>+24dB)
- Colors update instantly
- Warnings help user avoid excessive gain
- User knows when normalization is risky

Failure Criteria (P2):
- Colors don't change
- Incorrect threshold
- Color doesn't reflect risk level

---

**Test Case: 2.8.2 - Negative Gain Has No Warning**

Objective: Verify reduction doesn't trigger warnings

Steps:
1. Open file with RMS = -6dB
2. Open Normalize → RMS mode
3. Target RMS = -12dB
4. Required Gain = -6dB (reduction)
5. Verify label stays DEFAULT color (no warning)

Expected Results:
- Reduction (negative gain) safe, no warning needed
- Default color shows no risk
- Only boost gets warnings

Failure Criteria (P2):
- Warning shown for reduction
- User confused about risk

---

## Part 3: Cross-Feature Integration Testing

### 3.1 ParametricEQ + RMS Normalization Workflow

**Test Case: 3.1.1 - Normalize Then EQ**

Objective: Verify operations don't interfere

Steps:
1. Load `/TestFiles/moans.wav`, select region
2. Open Normalize → RMS mode, Target -6dB → Apply
3. Verify audio normalized (waveform slightly larger)
4. Open ParametricEQ → Preview
5. Adjust EQ sliders
6. Apply EQ

Expected Results:
- Both operations apply independently
- Undo history shows both operations
- Audio is normalized then EQ'd
- Ctrl+Z twice undoes both

Failure Criteria (P1):
- Operations interfere
- One operation lost
- Undo chain broken

---

**Test Case: 3.1.2 - EQ Then Normalize**

Objective: Verify order independence

Steps:
1. Load `/TestFiles/moans.wav`, select region
2. Open ParametricEQ, set Mid +6dB → Apply
3. Open Normalize → RMS mode, Target -6dB → Apply
4. Audio should be EQ'd then normalized

Expected Results:
- Both operations apply
- Final audio is EQ'd and normalized
- Different from Test 3.1.1 (order matters for audio quality)
- Both in undo history

Failure Criteria (P1):
- One operation lost
- Undo fails

---

### 3.2 Multi-Document Preview State Testing

**Test Case: 3.2.1 - Preview Muting Consistency**

Objective: Verify muting works across multiple dialog types

Steps:
1. Tab 1: File A → ParametricEQ → Preview (playing)
2. Tab 2: File B → NormalizeDialog → Click Preview (should stop Tab 1)
3. Verify Tab 1 audio muted
4. Tab 2 audio playing
5. Stop Tab 2 preview
6. Tab 1 unmutes but not playing (no auto-resume)

Expected Results:
- Only one preview audible at a time
- Switching dialogs respects mute rules
- Manual stop works correctly

Failure Criteria (P1):
- Multiple previews play simultaneously
- Audio mixing occurs

---

## Part 4: Performance & Stress Testing

### 4.1 Large File Testing

**Test Case: 4.1.1 - ParametricEQ Preview on 1-Hour File**

Objective: Verify real-time EQ works on large files

Steps:
1. Load 1-hour WAV (96kHz 24-bit stereo)
2. Select middle region (30 min mark, 5-second range)
3. Open ParametricEQ → Preview
4. Adjust EQ sliders rapidly
5. Measure CPU usage and latency

Expected Results:
- Preview responds instantly to slider changes
- No audio dropouts
- CPU < 30% (typical processing load)
- No significant latency

Failure Criteria (P1):
- Slider lag or delay
- Audio dropouts
- CPU spike > 50%

---

**Test Case: 4.1.2 - RMS Calculation on 1-Hour File**

Objective: Verify RMS analysis doesn't block UI

Steps:
1. Load 1-hour WAV
2. Select entire file
3. Open Normalize dialog
4. Measure time to calculate RMS
5. Verify UI responsive during calculation

Expected Results:
- RMS analysis completes < 1 second
- UI remains responsive
- No blocking on message thread
- Progress indicator shown (if implemented)

Failure Criteria (P1):
- Calculation > 5 seconds
- UI frozen during analysis
- No progress indication

---

### 4.2 Memory Usage Testing

**Test Case: 4.2.1 - Preview Buffer Memory Reasonable**

Objective: Verify preview buffer doesn't cause excessive memory

Steps:
1. Monitor memory before: Note baseline
2. Open Normalize dialog
3. Select entire file (1-hour)
4. Click Preview
5. Monitor memory during preview
6. Calculate memory used for preview buffer

Expected Results:
- Preview buffer < 50MB (1 hour × 96kHz × 4 bytes stereo = ~27MB)
- No memory leak after preview stop
- Memory freed when dialog closes

Failure Criteria (P1):
- Memory > 100MB for preview
- Memory leak (doesn't free)

---

## Part 5: Regression Testing

### 5.1 Existing Feature Compatibility

**Test Case: 5.1.1 - Existing Fade In/Out Not Affected**

Objective: Verify new features don't break fade operations

Steps:
1. Load file, select region
2. Open Fade In dialog → Apply
3. Verify fade appears in waveform
4. Undo → fade removed
5. Open ParametricEQ → Apply EQ
6. Open Fade Out → Apply
7. Verify both EQ and Fade applied

Expected Results:
- Fade operations work independently
- No conflicts with new features
- Undo chain complete

Failure Criteria (P1):
- Fade broken
- Undo chain corrupted

---

**Test Case: 5.1.2 - Region List Not Affected by Dialogs**

Objective: Verify region operations work with EQ/Normalize

Steps:
1. Load file with regions
2. Select region 1
3. Open ParametricEQ → Apply
4. Verify region still exists, bounds unchanged
5. Create new region
6. Open Normalize → Apply
7. Verify new region exists

Expected Results:
- Region operations independent of DSP
- Regions survive multiple edits
- Region selection preserved

Failure Criteria (P1):
- Regions deleted or corrupted
- Region selection lost

---

## Test Execution Summary

### Severity Distribution

- **P0 (Blocker):** 5 tests
  - Audio applies correctly, undo works, cancel preserves, mode persists correctly

- **P1 (Major):** 35+ tests
  - Real-time parameter updates, loop functionality, mode switching, preview accuracy

- **P2 (Minor):** 10+ tests
  - UI cosmetics, label updates, dialog persistence

- **P3 (Cosmetic):** 5+ tests
  - Button states, color indicators

### Test Environment

- OS: macOS 10.15+
- Audio Device: System audio (or test fixture)
- JUCE Version: 7.x
- WaveEdit Build: Debug and Release

### Success Criteria

**Minimum for Release:**
- All P0 tests PASS
- 95%+ of P1 tests PASS
- No known P0/P1 regressions

**Nice to Have:**
- 100% P0/P1 PASS
- 90%+ P2/P3 PASS

---

## Known Limitations & Edge Cases

1. **Filter Stability:** ParametricEQ filters may ring if Q > 5.0 and gain > 12dB
   - Mitigation: Document in settings, recommend Q < 2.0 for large gains

2. **RMS on Silence:** RMS returns -INF, comparison with -0.1dB target gives -0.1 dB gain
   - Mitigation: Check for silence before normalizing, show warning

3. **Preview Muting:** Only one AudioEngine can preview at a time
   - Mitigation: Users must stop preview before switching documents

4. **Coordinate System:** Preview selection offset requires careful handling
   - Already addressed in code via `setPreviewSelectionOffset()`

---

## Notes for QA Testers

1. **Always test with multiple files:** moans.wav (speech), sine waves (tones), music (complex)
2. **Listen closely during preview:** Artifacts often subtle, not just visible
3. **Check undo history:** Always verify operations appear in menu
4. **Test with loop:** Loop reveals boundary issues and coordinate problems
5. **Verify meters:** Watch level meters during preview for proper gain
6. **Test on both platforms:** macOS and Windows may have different audio behavior

---

## Appendix: Test File Specifications

**Required Test Files:**

- `/TestFiles/moans.wav` - 30 seconds, 44.1kHz, stereo, -6dB peak, speech/vocal
- A sine wave reference file (1 kHz @ 0dB peak)
- A music file with steady compression (-10dB dynamic range)
- A noise/ambience file (-20dB RMS target)

**File Generation Commands (if needed):**

```bash
# 1 kHz sine, 2 seconds, 44.1kHz
sox -n -r 44100 sine_1k.wav synth 2 sine 1000 gain -3

# Pink noise, 10 seconds
sox -n -r 44100 pinknoise.wav synth 10 noise gain -3
```

---

**Document Version:** 1.0
**Date Created:** December 2, 2025
**Last Updated:** December 2, 2025
**Status:** Ready for QA Execution
