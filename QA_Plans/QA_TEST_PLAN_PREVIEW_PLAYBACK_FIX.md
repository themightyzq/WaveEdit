# QA Test Plan: Preview Playback Bug Fix

## Bug Summary
**Issue**: Preview dialogs (Normalize, Fade In, Fade Out, DC Offset) were playing audio from the beginning of the main file instead of the selected region.

**Root Cause**: Preview mode was never automatically disabled when the preview buffer finished playing.

**Fix**: Modified `AudioEngine::changeListenerCallback` to automatically disable preview mode when transport stream finishes during OFFLINE_BUFFER preview mode.

**Build Path**: `./build/WaveEdit_artefacts/Release/WaveEdit.app`

---

## TEST SETUP (One-Time Setup Before All Tests)

### Required Test Files

Create or obtain the following test audio files for optimal verification:

1. **Primary Test File: "countdown.wav"** (Recommended)
   - **Duration**: 60 seconds
   - **Content**: Voice counting "One... Two... Three... Four..." every 10 seconds
   - **Why**: Makes it obvious which region is playing
   - **Alternative**: Use any file with distinct audio markers at different times

2. **Secondary Test File: "music_with_silence.wav"**
   - **Duration**: 90+ seconds
   - **Content**: Music with 10 seconds of silence at start, middle (30-40s), and end
   - **Why**: Useful for DC offset and silence detection tests

3. **Tertiary Test File: "moans.wav"** (If available in TestFiles/)
   - Use existing test file if it has distinct audio sections

### Setup Steps

1. **Launch WaveEdit**
   ```bash
   cd /Users/zacharylquarles/PROJECTS_Apps/Project_WaveEditor
   ./build/WaveEdit_artefacts/Release/WaveEdit.app/Contents/MacOS/WaveEdit
   ```

2. **Verify System Audio Output**
   - Ensure speakers/headphones are connected and working
   - Set system volume to comfortable listening level (50-70%)

3. **Prepare Test Files**
   - Place test files in `TestFiles/` directory
   - Verify files load without errors

4. **Clear Any Previous State**
   - Close all open files in WaveEdit (if any)
   - Ensure no dialogs are open

---

## TEST WORKFLOW 1: Normalize Dialog Preview (Primary Test)

### Objective
Verify that clicking Preview in the Normalize dialog plays the SELECTED REGION, not the beginning of the file.

### Prerequisites
- Test file loaded: `countdown.wav` (or any file with distinct audio markers)

### Test Steps

**Step 1: Load Test File**
1. Click File → Open (or press Cmd+O)
2. Navigate to `countdown.wav`
3. Click Open
4. **Expected Result**: Waveform displays in main window
5. **Visual Check**: Entire file visible in waveform display

**Step 2: Play Full File (Baseline Check)**
1. Press Spacebar to start playback from beginning
2. Listen for "One... Two... Three..."
3. Press Spacebar to stop after ~5 seconds
4. **Expected Result**: Audio plays from the beginning
5. **Audio Check**: You hear "One..." at the start

**Step 3: Select Middle Region**
1. Click and drag on waveform to select region from 30-40 seconds
2. **Visual Check**: Blue selection highlight appears on waveform
3. **Verify Selection**: Bottom status bar shows selection duration ~10 seconds
4. **Alternative Method**: Press I (Set In Point) at 30s, O (Set Out Point) at 40s

**Step 4: Play Selection (Baseline Check)**
1. Press Spacebar with region selected
2. **Expected Result**: Playback starts at 30 seconds
3. **Audio Check**: You hear "Four..." (not "One...")
4. Press Spacebar to stop

**Step 5: Open Normalize Dialog**
1. Click Process → Normalize (or press N)
2. **Expected Result**: Normalize dialog opens
3. **Visual Check**: Dialog shows:
   - Peak Level field (default: 0.0 dB)
   - Preview button
   - Loop checkbox
   - OK/Cancel buttons

**Step 6: Click Preview (CRITICAL TEST)**
1. Click the "Preview" button
2. **Expected Result**: Audio playback starts IMMEDIATELY
3. **CRITICAL AUDIO CHECK**: You hear "Four..." (the 30-40s region)
4. **FAIL CONDITION**: If you hear "One..." (beginning of file), the bug is NOT fixed
5. Let preview play to completion (~10 seconds)
6. **Expected Result**: Playback stops automatically when region finishes

**Step 7: Verify Clean State After Preview**
1. Wait 2 seconds after preview finishes
2. **Visual Check**: Waveform selection still visible (30-40s)
3. **Expected Result**: No errors in console, no stuck state
4. Press Spacebar in main window
5. **Expected Result**: Main file plays from 30s (selection start)
6. Press Spacebar to stop

**Step 8: Multiple Preview Cycles**
1. In Normalize dialog, click Preview button again
2. **Expected Result**: Audio plays 30-40s region again
3. While preview is playing, click Preview button again (interrupt)
4. **Expected Result**: Previous preview stops, new preview starts from 30s
5. Let preview complete
6. Click Preview a third time
7. **Expected Result**: Audio plays 30-40s region again

**Step 9: Change Normalize Value and Preview**
1. In Normalize dialog, change Peak Level to -6.0 dB
2. Click Preview
3. **Expected Result**: Audio plays 30-40s region at LOWER volume
4. **Audio Check**: Verify volume is quieter than original
5. Let preview complete

**Step 10: Apply and Verify**
1. Click OK in Normalize dialog
2. **Expected Result**: Dialog closes, processing completes
3. **Visual Check**: Waveform updates (30-40s region shows normalized peaks)
4. Press Spacebar
5. **Expected Result**: Main file plays from 30s at normalized volume
6. Press Cmd+Z to undo
7. **Expected Result**: Waveform reverts to original

### Pass/Fail Criteria

**PASS Conditions** (ALL must be true):
- ✅ Step 6: Preview plays SELECTED region (30-40s), NOT beginning of file
- ✅ Step 6: Preview stops automatically when region finishes
- ✅ Step 7: Main file playback works normally after preview
- ✅ Step 8: Multiple preview cycles all play correct region
- ✅ Step 9: Changed parameters affect preview correctly
- ✅ Step 10: Applying changes works without errors

**FAIL Conditions** (ANY indicates bug):
- ❌ Preview plays beginning of file instead of selection
- ❌ Preview doesn't stop automatically (infinite playback)
- ❌ Preview causes main file playback to fail
- ❌ Console shows errors during/after preview
- ❌ Application crashes or freezes

### Severity: **P0 - Critical** (Blocks release if failed)

---

## TEST WORKFLOW 2: Fade In Dialog Preview

### Objective
Verify Fade In preview plays the selected region with fade applied.

### Prerequisites
- Test file loaded: `countdown.wav`

### Test Steps

**Step 1: Select Beginning Region**
1. Click and drag to select 0-10 seconds (start of file)
2. **Visual Check**: Selection starts at very beginning of waveform
3. **Verify Selection**: Status bar shows ~10 seconds

**Step 2: Open Fade In Dialog**
1. Click Process → Fade In (or press I key)
2. **Expected Result**: Fade In dialog opens
3. **Visual Check**: Dialog shows:
   - Fade curve preview graph
   - Curve type dropdown (Linear, Logarithmic, etc.)
   - Preview button
   - Loop checkbox

**Step 3: Preview Default Fade In**
1. Click Preview button
2. **CRITICAL AUDIO CHECK**:
   - Audio starts QUIET and gradually increases to full volume
   - You hear "One..." fading in (0-10s region)
   - You do NOT hear full-volume audio from beginning
3. **FAIL CONDITION**: If audio plays at full volume from the start, fade is not applied
4. Let preview complete (~10 seconds)

**Step 4: Change Fade Curve**
1. In Curve Type dropdown, select "Logarithmic"
2. Click Preview
3. **Expected Result**: Fade curve feels different (faster fade-in)
4. **Audio Check**: Still plays 0-10s region with fade applied

**Step 5: Enable Loop**
1. Check the "Loop" checkbox
2. Click Preview
3. **Expected Result**: Fade In plays repeatedly in a loop
4. **Audio Check**: After 10 seconds, you hear "One..." fading in again
5. Click Preview button again to stop
6. **Expected Result**: Playback stops immediately

**Step 6: Apply Fade In**
1. Uncheck Loop
2. Click OK
3. **Expected Result**: Dialog closes, fade applied to waveform
4. **Visual Check**: Waveform shows amplitude increasing from 0 at start
5. Press Spacebar
6. **Expected Result**: Audio plays from beginning with fade-in effect
7. Press Cmd+Z to undo

### Pass/Fail Criteria

**PASS**:
- ✅ Preview plays selected region (0-10s), not wrong region
- ✅ Fade effect is audible in preview (volume increases)
- ✅ Changing curve type affects preview
- ✅ Loop mode works correctly
- ✅ Preview stops cleanly

**FAIL**:
- ❌ Preview plays wrong region
- ❌ Fade effect not applied in preview
- ❌ Loop mode doesn't work

### Severity: **P0 - Critical**

---

## TEST WORKFLOW 3: Fade Out Dialog Preview

### Objective
Verify Fade Out preview plays the selected region with fade applied.

### Prerequisites
- Test file loaded: `countdown.wav`

### Test Steps

**Step 1: Select End Region**
1. Click and drag to select last 10 seconds of file (50-60s)
2. **Visual Check**: Selection extends to very end of waveform
3. **Tip**: Use End key to jump to end, then Shift+Click to select backward

**Step 2: Open Fade Out Dialog**
1. Click Process → Fade Out (or press O key)
2. **Expected Result**: Fade Out dialog opens
3. **Visual Check**: Similar to Fade In dialog

**Step 3: Preview Default Fade Out**
1. Click Preview button
2. **CRITICAL AUDIO CHECK**:
   - Audio starts at FULL volume and gradually decreases to silence
   - You hear the last counting number fading out (50-60s region)
   - You do NOT hear audio from beginning of file
3. **FAIL CONDITION**: If you hear "One..." or full-volume audio throughout, the bug is present
4. Let preview complete (~10 seconds)
5. **Expected Result**: Playback stops at end of fade (silence)

**Step 4: Change Fade Curve**
1. Select "Exponential" curve type
2. Click Preview
3. **Expected Result**: Fade curve feels different (holds volume longer, then quick fade)
4. **Audio Check**: Still plays 50-60s region

**Step 5: Enable Loop**
1. Check "Loop" checkbox
2. Click Preview
3. **Expected Result**: Fade Out plays, then loops back to start
4. **Audio Check**: You hear full volume → silence → full volume → silence (looping)
5. Click Preview to stop

**Step 6: Apply Fade Out**
1. Uncheck Loop
2. Click OK
3. **Expected Result**: Fade applied to waveform
4. **Visual Check**: Waveform shows amplitude decreasing to 0 at end
5. Press End key, then Spacebar (play from end)
6. **Expected Result**: Audio fades to silence
7. Press Cmd+Z to undo

### Pass/Fail Criteria

**PASS**:
- ✅ Preview plays selected end region (50-60s)
- ✅ Fade effect is audible (volume decreases to silence)
- ✅ Changing curve affects preview
- ✅ Loop works correctly
- ✅ No errors or stuck state

**FAIL**:
- ❌ Preview plays beginning of file
- ❌ Fade effect not applied
- ❌ Audio doesn't fade to silence

### Severity: **P0 - Critical**

---

## TEST WORKFLOW 4: DC Offset Removal Dialog Preview

### Objective
Verify DC Offset preview plays the selected region.

### Prerequisites
- Test file loaded: `countdown.wav` or `music_with_silence.wav`

### Test Steps

**Step 1: Select Middle Region**
1. Select 25-35 seconds (middle region)
2. **Visual Check**: Selection visible in middle of waveform

**Step 2: Open DC Offset Removal Dialog**
1. Click Process → DC Offset Removal (or press D key)
2. **Expected Result**: DC Offset dialog opens
3. **Visual Check**: Dialog shows:
   - Current DC offset value (if any)
   - Preview button
   - Loop checkbox

**Step 3: Preview DC Offset Removal**
1. Click Preview button
2. **CRITICAL AUDIO CHECK**:
   - You hear audio from 25-35s region
   - You do NOT hear "One..." from beginning
3. **FAIL CONDITION**: If you hear beginning of file, bug is NOT fixed
4. Let preview complete (~10 seconds)

**Step 4: Enable Loop**
1. Check "Loop" checkbox
2. Click Preview
3. **Expected Result**: 25-35s region plays in a loop
4. Click Preview to stop

**Step 5: Apply DC Offset Removal**
1. Uncheck Loop
2. Click OK
3. **Expected Result**: Dialog closes
4. **Visual Check**: Waveform may show subtle vertical shift (if DC offset existed)
5. Press Spacebar
6. **Expected Result**: Main file plays from 25s
7. Press Cmd+Z to undo

### Pass/Fail Criteria

**PASS**:
- ✅ Preview plays selected region (25-35s)
- ✅ Loop works
- ✅ No playback errors
- ✅ Main file unaffected after preview

**FAIL**:
- ❌ Preview plays wrong region
- ❌ Loop doesn't work
- ❌ Errors in console

### Severity: **P1 - High** (DC Offset less commonly used, but must work)

---

## TEST WORKFLOW 5: Preview Interruption and Edge Cases

### Objective
Verify preview handles interruptions and edge cases gracefully.

### Prerequisites
- Test file loaded: `countdown.wav`

### Test Steps

**Step 1: Stop Preview Mid-Play**
1. Select 20-30s region
2. Open Normalize dialog (Process → Normalize)
3. Click Preview
4. After 3 seconds (mid-preview), click Preview button again
5. **Expected Result**: First preview stops, second preview starts from beginning (20s)
6. **Audio Check**: No overlap, no glitches
7. Let second preview complete

**Step 2: Close Dialog During Preview**
1. Click Preview in Normalize dialog
2. Immediately click Cancel (while preview is playing)
3. **Expected Result**: Preview stops immediately, dialog closes
4. **Visual Check**: Main waveform still shows selection
5. Press Spacebar
6. **Expected Result**: Main file plays from 20s (selection start)
7. **Audio Check**: No stuck audio, no errors

**Step 3: Switch Tabs During Preview**
1. Open second file (File → Open → choose different file)
2. **Expected Result**: Two tabs visible at bottom
3. Switch to first tab (countdown.wav)
4. Select 15-25s region
5. Open Normalize dialog
6. Click Preview
7. Immediately click second tab (while preview is playing)
8. **Expected Result**: Preview stops when switching tabs
9. Switch back to first tab
10. **Expected Result**: Selection still visible, no errors

**Step 4: Change Selection During Preview**
1. Select 10-20s region
2. Open Fade In dialog
3. Click Preview (preview starts playing 10-20s)
4. While preview is playing, click main waveform to change selection to 30-40s
5. **Expected Result**: Preview continues playing original region (10-20s)
6. Let preview complete
7. Click Preview again
8. **Expected Result**: Preview now plays NEW selection (30-40s)

**Step 5: Preview Empty Selection**
1. Click on waveform (single click, no selection)
2. **Visual Check**: No selection (blue highlight)
3. Try to open Normalize dialog
4. **Expected Result**: Dialog either:
   - Doesn't open (correct behavior), OR
   - Opens but Preview button is disabled, OR
   - Shows error message "No selection"
5. **Note**: Document actual behavior

**Step 6: Preview Entire File (No Selection)**
1. Press Cmd+A (Select All)
2. Open Normalize dialog
3. Click Preview
4. **Expected Result**: Entire file plays in preview
5. **Audio Check**: Plays from beginning to end
6. **Note**: This is valid behavior (entire file is the selection)

### Pass/Fail Criteria

**PASS**:
- ✅ Stopping preview mid-play works cleanly
- ✅ Closing dialog during preview stops playback
- ✅ Tab switching stops preview
- ✅ Changing selection during preview doesn't crash
- ✅ Empty selection handled gracefully
- ✅ Select All preview works

**FAIL**:
- ❌ Preview continues after dialog closed
- ❌ Audio overlap or glitches
- ❌ Crash when changing selection during preview
- ❌ Stuck playback state

### Severity: **P1 - High** (Edge cases must be handled safely)

---

## TEST WORKFLOW 6: Loop Functionality Stress Test

### Objective
Verify loop checkbox works reliably across all preview dialogs.

### Prerequisites
- Test file loaded: `countdown.wav`

### Test Steps

**Step 1: Loop in Normalize Dialog**
1. Select 5-10s region
2. Open Normalize dialog
3. Check "Loop" checkbox
4. Click Preview
5. **Expected Result**: Region plays, then immediately restarts
6. Let it loop 3 times (~15 seconds total)
7. **Audio Check**: Smooth transition at loop point (no clicks/pops)
8. Click Preview to stop
9. Uncheck Loop, click Preview
10. **Expected Result**: Plays once, then stops automatically

**Step 2: Loop in Fade In Dialog**
1. Select 0-5s region
2. Open Fade In dialog
3. Check "Loop"
4. Click Preview
5. **Expected Result**: Fade In plays repeatedly
6. **Audio Check**: Each loop starts quiet and fades to full volume
7. Let loop 3 times
8. Click Preview to stop

**Step 3: Loop in Fade Out Dialog**
1. Select 55-60s region
2. Open Fade Out dialog
3. Check "Loop"
4. Click Preview
5. **Expected Result**: Fade Out plays repeatedly
6. **Audio Check**: Each loop starts full volume and fades to silence
7. Let loop 3 times
8. Click Preview to stop

**Step 4: Loop in DC Offset Dialog**
1. Select 20-25s region
2. Open DC Offset Removal dialog
3. Check "Loop"
4. Click Preview
5. **Expected Result**: Region loops continuously
6. Let loop 3 times
7. Click Preview to stop

### Pass/Fail Criteria

**PASS**:
- ✅ Loop works in all dialogs
- ✅ No clicks/pops at loop transition point
- ✅ Stopping loop works (Click Preview again)
- ✅ Unchecking loop makes preview play once

**FAIL**:
- ❌ Loop doesn't work in any dialog
- ❌ Audible clicks at loop point
- ❌ Cannot stop looping preview

### Severity: **P1 - High** (Loop is a key feature for preview)

---

## TEST WORKFLOW 7: Long Selection Preview

### Objective
Verify preview works correctly with long selections (60+ seconds).

### Prerequisites
- Test file: Long audio file (90+ seconds), or use multiple regions in countdown.wav

### Test Steps

**Step 1: Preview Long Selection**
1. Select entire file (Cmd+A) or 0-60s region
2. Open Normalize dialog
3. Click Preview
4. **Expected Result**: Playback starts immediately
5. Let play for 30 seconds
6. **Audio Check**: Continuous playback, no interruptions
7. Click Preview to stop
8. **Expected Result**: Stops immediately

**Step 2: Skip Through Long Preview**
1. Select 0-60s region
2. Open Fade In dialog
3. Click Preview
4. Let play for 10 seconds
5. Click Preview (stop), then immediately click Preview again (restart)
6. **Expected Result**: Preview restarts from beginning (0s)
7. Let play for 5 seconds
8. Click Cancel (close dialog)
9. **Expected Result**: Preview stops, dialog closes

### Pass/Fail Criteria

**PASS**:
- ✅ Long selections play correctly
- ✅ No memory leaks or performance degradation
- ✅ Can stop long preview at any time

**FAIL**:
- ❌ Preview stutters or glitches
- ❌ Cannot stop long preview
- ❌ Application freezes

### Severity: **P2 - Medium** (Less common, but should work)

---

## TEST WORKFLOW 8: Multi-File Preview (Advanced)

### Objective
Verify preview mode doesn't affect other open files.

### Prerequisites
- Two test files loaded in separate tabs

### Test Steps

**Step 1: Open Two Files**
1. Open `countdown.wav` in Tab 1
2. Open second file in Tab 2 (File → Open)
3. **Visual Check**: Two tabs visible at bottom

**Step 2: Preview in Tab 1**
1. Switch to Tab 1 (countdown.wav)
2. Select 10-20s region
3. Open Normalize dialog
4. Click Preview
5. Let preview complete
6. Click OK to apply

**Step 3: Verify Tab 2 Unaffected**
1. Switch to Tab 2
2. Press Spacebar
3. **Expected Result**: Tab 2 file plays normally
4. **Audio Check**: Correct file plays (not countdown.wav)
5. Press Spacebar to stop

**Step 4: Preview in Tab 2**
1. In Tab 2, select any region
2. Open Fade In dialog
3. Click Preview
4. **Expected Result**: Tab 2's selected region plays (not Tab 1)
5. Click Cancel

**Step 5: Verify Tab 1 Still Works**
1. Switch to Tab 1
2. Press Spacebar
3. **Expected Result**: Normalized region plays correctly
4. **Audio Check**: Changes from Step 2 are audible

### Pass/Fail Criteria

**PASS**:
- ✅ Preview in one tab doesn't affect other tabs
- ✅ Each tab maintains independent playback state
- ✅ No cross-contamination of audio buffers

**FAIL**:
- ❌ Preview plays audio from wrong tab
- ❌ Tab switching causes errors
- ❌ Changes in one tab affect another

### Severity: **P1 - High** (Multi-file support is core feature)

---

## ACCEPTANCE CRITERIA (Overall Pass/Fail Decision)

### Must Pass (P0 - Critical)
All of these must pass for the bug fix to be considered complete:

1. ✅ **TEST WORKFLOW 1 (Normalize)**: Preview plays SELECTED region, not beginning of file
2. ✅ **TEST WORKFLOW 2 (Fade In)**: Preview plays selected region with correct effect
3. ✅ **TEST WORKFLOW 3 (Fade Out)**: Preview plays selected region with correct effect
4. ✅ **TEST WORKFLOW 5 (Interruption)**: Preview stops cleanly when interrupted
5. ✅ **TEST WORKFLOW 6 (Loop)**: Loop functionality works in all dialogs

### Should Pass (P1 - High)
At least 90% of these must pass:

6. ✅ **TEST WORKFLOW 4 (DC Offset)**: Preview plays correct region
7. ✅ **TEST WORKFLOW 5 (Edge Cases)**: All edge cases handled gracefully
8. ✅ **TEST WORKFLOW 8 (Multi-File)**: Preview doesn't affect other tabs

### Nice to Pass (P2 - Medium)
Failures here can be documented as known issues:

9. ✅ **TEST WORKFLOW 7 (Long Selection)**: Long previews work without performance issues

---

## FINAL ACCEPTANCE DECISION

**RELEASE STATUS**:
- **APPROVED FOR RELEASE**: If all P0 tests pass + 90% of P1 tests pass
- **NEEDS REWORK**: If any P0 test fails
- **DEFER TO NEXT VERSION**: If P1 tests have more than 2 failures

---

## TEST EXECUTION CHECKLIST

Before starting tests:
- [ ] WaveEdit built and launched successfully
- [ ] Test files prepared and loadable
- [ ] System audio confirmed working
- [ ] No other audio applications running (avoid conflicts)
- [ ] Console window visible (to check for errors)

During testing:
- [ ] Document actual behavior vs. expected behavior
- [ ] Take screenshots of any failures
- [ ] Note exact steps to reproduce any issues
- [ ] Check console for error messages
- [ ] Monitor CPU/memory usage (Activity Monitor on macOS)

After testing:
- [ ] Close all open files
- [ ] Verify no memory leaks (quit and relaunch)
- [ ] Review all PASS/FAIL results
- [ ] Make final acceptance decision
- [ ] Report findings to developer

---

## CONSOLE ERROR MONITORING

During all tests, monitor the console output for these error patterns:

**Critical Errors** (Immediate Fail):
- "Assertion failed"
- "Segmentation fault"
- "Buffer overrun"
- "Thread deadlock"

**Warnings** (Document but don't fail):
- "Preview mode still active after transport stopped" (This should NOT appear after the fix)
- Audio glitches/pops (may indicate buffer underrun)

**Info Messages** (Normal):
- "Preview started"
- "Preview stopped"
- "Transport state changed"

---

## KNOWN ISSUES TO VERIFY ARE FIXED

These specific issues should NOT occur after the fix:

1. ❌ **OLD BUG**: Preview plays from beginning of file instead of selection
   - **FIX VERIFICATION**: All tests should play correct region

2. ❌ **OLD BUG**: Preview mode never disables after playback finishes
   - **FIX VERIFICATION**: Pressing Spacebar after preview should play main file, not preview buffer

3. ❌ **OLD BUG**: Multiple preview clicks cause audio buffer confusion
   - **FIX VERIFICATION**: Multiple preview cycles should work cleanly (TEST WORKFLOW 5)

---

## ADDITIONAL NOTES

### Best Practices for Testing
- Use headphones for accurate audio verification
- Test in a quiet environment
- Take breaks between test workflows to avoid listener fatigue
- If unsure about audio verification, test multiple times

### Reporting Format
When reporting results, use this format:

```
TEST WORKFLOW 1: Normalize Dialog Preview
Status: PASS / FAIL
Details:
- Step 6: PASS - Preview played 30-40s region correctly
- Step 7: PASS - Main playback worked after preview
- Step 8: PASS - Multiple preview cycles worked
Notes: All audio verification checks passed. No console errors.
```

### Contact
If any test failures occur, provide:
1. Test workflow number and step
2. Expected vs. actual behavior
3. Screenshot (if visual issue)
4. Console error output (if any)
5. Test file used

---

**End of Test Plan**
**Version**: 1.0
**Date**: 2025-11-25
**Bug Fix**: Preview Playback Region Selection
**Target Build**: `./build/WaveEdit_artefacts/Release/WaveEdit.app`
