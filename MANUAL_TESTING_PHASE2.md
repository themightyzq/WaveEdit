# Phase 2 Manual Testing Guide - WaveEdit

**Test Date**: 2025-10-13
**Tester**: AI Agent (Claude)
**Build**: Release
**Platform**: macOS

---

## Testing Objectives

Verify all Phase 2 critical musician features work correctly:
1. ✅ Gain/Volume adjustment
2. ✅ Level meters
3. ✅ Normalization
4. ✅ Fade in/out
5. ✅ Process menu keyboard shortcuts

---

## Pre-Test Setup

### Required Test Files
- **Mono WAV**: 24-bit, 96kHz (for mono playback verification)
- **Stereo WAV**: 16/24-bit, 44.1kHz or 48kHz (for standard editing)
- **Variety**: Files with different peak levels (-20dB, -6dB, 0dB)

### Launch Application
```bash
./build-and-run.command
```

---

## Test Suite 1: Gain/Volume Adjustment

### Test 1.1: Basic Gain Increase
**Steps**:
1. Open a WAV file
2. Press Shift+Up (increase gain by 1dB)
3. Observe status message and waveform

**Expected**:
- ✅ Status bar shows "Applied gain: +1.0 dB"
- ✅ Waveform amplitude increases visibly
- ✅ Undo available (Cmd+Z)

**Actual**: [PASS/FAIL]

---

### Test 1.2: Basic Gain Decrease
**Steps**:
1. With file still open
2. Press Shift+Down (decrease gain by 1dB)
3. Observe status message and waveform

**Expected**:
- ✅ Status bar shows "Applied gain: -1.0 dB"
- ✅ Waveform amplitude decreases visibly
- ✅ Undo available (Cmd+Z)

**Actual**: [PASS/FAIL]

---

### Test 1.3: Gain Adjustment During Playback (CRITICAL)
**Steps**:
1. Open a WAV file
2. Press Space to start playback
3. While playing, press Shift+Up multiple times
4. Listen for audio changes

**Expected**:
- ✅ Audio gets louder in real-time (no need to stop/restart)
- ✅ Waveform updates during playback
- ✅ Playback continues smoothly (no glitches)
- ✅ Position preserved after gain adjustment

**Actual**: [PASS/FAIL]

---

### Test 1.4: Selection-Based Gain
**Steps**:
1. Open a WAV file
2. Click and drag to select a region (e.g., 2-5 seconds)
3. Press Shift+Up (increase gain by 1dB)
4. Observe waveform

**Expected**:
- ✅ Only selected region shows amplitude increase
- ✅ Rest of file unchanged
- ✅ Status shows "Applied gain to selection: +1.0 dB"

**Actual**: [PASS/FAIL]

---

### Test 1.5: Multiple Gain Adjustments with Undo
**Steps**:
1. Open a WAV file
2. Press Shift+Up 5 times (+5dB total)
3. Press Cmd+Z 5 times (undo all)
4. Press Cmd+Shift+Z 5 times (redo all)

**Expected**:
- ✅ Each adjustment is a separate undo step
- ✅ Undo reverses each step correctly
- ✅ Redo reapplies each step correctly
- ✅ Waveform returns to original after all undos

**Actual**: [PASS/FAIL]

---

## Test Suite 2: Level Meters

### Test 2.1: Meters Activate During Playback
**Steps**:
1. Open a WAV file
2. Press Space to start playback
3. Observe level meters on right side of UI

**Expected**:
- ✅ Meters light up (green bars)
- ✅ Peak levels (white line) show recent peaks
- ✅ RMS (darker green) shows average level
- ✅ Meters update smoothly (~30fps)

**Actual**: [PASS/FAIL]

---

### Test 2.2: Clipping Detection
**Steps**:
1. Open a WAV file
2. Apply +10dB gain (press Shift+Up 10 times)
3. Press Space to play
4. Observe meters

**Expected**:
- ✅ Meters reach top (yellow/red zone)
- ✅ Red clipping indicator appears when level >±1.0
- ✅ Clipping indicator holds for 3 seconds
- ✅ No audio glitches despite clipping

**Actual**: [PASS/FAIL]

---

### Test 2.3: Meters Reflect Gain Changes in Real-Time
**Steps**:
1. Open a WAV file
2. Press Space to start playback
3. While playing, press Shift+Up 3 times
4. Observe meters

**Expected**:
- ✅ Meters immediately show higher levels after each gain increase
- ✅ No lag between gain adjustment and meter update
- ✅ Audio and meters sync correctly

**Actual**: [PASS/FAIL]

---

### Test 2.4: Mono File Playback with Meters
**Steps**:
1. Open a mono WAV file
2. Press Space to play
3. Observe both left and right meters

**Expected**:
- ✅ Both meters show identical levels (mono centered)
- ✅ No imbalance or left-only playback
- ✅ Meters synchronized

**Actual**: [PASS/FAIL]

---

## Test Suite 3: Normalization

### Test 3.1: Normalize Entire File
**Steps**:
1. Open a WAV file with peak <0dB (e.g., -12dB)
2. Press Cmd+Shift+N (Normalize)
3. Observe waveform and status

**Expected**:
- ✅ Waveform amplitude increases to fill vertical space
- ✅ Status shows "Normalized to 0.0 dB (from -12.X dB)"
- ✅ Peak level is now exactly 0dB (verify with meters during playback)
- ✅ Undo available (Cmd+Z)

**Actual**: [PASS/FAIL]

---

### Test 3.2: Normalize Selection Only
**Steps**:
1. Open a WAV file
2. Select a region (e.g., 1-3 seconds)
3. Press Cmd+Shift+N (Normalize)
4. Observe waveform

**Expected**:
- ✅ Only selected region is normalized
- ✅ Rest of file unchanged
- ✅ Status shows "Normalized selection to 0.0 dB"
- ✅ Clear visual boundary at selection edges

**Actual**: [PASS/FAIL]

---

### Test 3.3: Normalize During Playback
**Steps**:
1. Open a WAV file
2. Press Space to start playback
3. While playing, press Cmd+Shift+N
4. Listen for audio changes

**Expected**:
- ✅ Audio immediately gets louder (real-time update)
- ✅ Playback continues smoothly
- ✅ Position preserved after normalization

**Actual**: [PASS/FAIL]

---

### Test 3.4: Normalize Already-Normalized File
**Steps**:
1. Open a file that's already at 0dB peak
2. Press Cmd+Shift+N
3. Observe behavior

**Expected**:
- ✅ Normalization completes without error
- ✅ File remains unchanged (0dB → 0dB)
- ✅ Status shows "Normalized to 0.0 dB (from 0.0 dB)" or similar

**Actual**: [PASS/FAIL]

---

## Test Suite 4: Fade In

### Test 4.1: Fade In Requires Selection
**Steps**:
1. Open a WAV file
2. Do NOT make a selection
3. Press Cmd+Shift+I (Fade In)

**Expected**:
- ✅ Alert dialog appears: "Please select a region to apply fade in"
- ✅ No fade applied
- ✅ File unchanged

**Actual**: [PASS/FAIL]

---

### Test 4.2: Basic Fade In
**Steps**:
1. Open a WAV file
2. Select the first 2 seconds
3. Press Cmd+Shift+I (Fade In)
4. Observe waveform

**Expected**:
- ✅ Waveform starts at zero amplitude
- ✅ Gradually increases to full amplitude over 2 seconds
- ✅ Linear fade curve (straight diagonal)
- ✅ Status shows "Applied fade in (duration: 2.00s)"

**Actual**: [PASS/FAIL]

---

### Test 4.3: Fade In and Play
**Steps**:
1. Apply fade in to first 2 seconds (Test 4.2)
2. Press Space to play from start
3. Listen carefully

**Expected**:
- ✅ Audio starts at silence (0 volume)
- ✅ Gradually increases to full volume over 2 seconds
- ✅ No clicks or pops at fade start
- ✅ Smooth transition to un-faded audio

**Actual**: [PASS/FAIL]

---

### Test 4.4: Fade In During Playback
**Steps**:
1. Open a WAV file
2. Select first 3 seconds
3. Press Space to start playback
4. While playing, press Cmd+Shift+I

**Expected**:
- ✅ Fade applies in real-time
- ✅ Playback continues smoothly
- ✅ Position preserved

**Actual**: [PASS/FAIL]

---

### Test 4.5: Undo Fade In
**Steps**:
1. Apply fade in
2. Press Cmd+Z (Undo)
3. Observe waveform

**Expected**:
- ✅ Waveform returns to original (no fade)
- ✅ Undo works correctly
- ✅ Redo (Cmd+Shift+Z) reapplies fade

**Actual**: [PASS/FAIL]

---

## Test Suite 5: Fade Out

### Test 5.1: Fade Out Requires Selection
**Steps**:
1. Open a WAV file
2. Do NOT make a selection
3. Press Cmd+Shift+O (Fade Out)

**Expected**:
- ✅ Alert dialog appears: "Please select a region to apply fade out"
- ✅ No fade applied
- ✅ File unchanged

**Actual**: [PASS/FAIL]

---

### Test 5.2: Basic Fade Out
**Steps**:
1. Open a WAV file
2. Select the last 2 seconds
3. Press Cmd+Shift+O (Fade Out)
4. Observe waveform

**Expected**:
- ✅ Waveform ends at zero amplitude
- ✅ Gradually decreases from full amplitude over 2 seconds
- ✅ Linear fade curve (straight diagonal)
- ✅ Status shows "Applied fade out (duration: 2.00s)"

**Actual**: [PASS/FAIL]

---

### Test 5.3: Fade Out and Play
**Steps**:
1. Apply fade out to last 2 seconds (Test 5.2)
2. Play the last 5 seconds of the file
3. Listen carefully

**Expected**:
- ✅ Audio gradually decreases to silence over last 2 seconds
- ✅ Ends at complete silence (0 volume)
- ✅ No clicks or pops at fade end
- ✅ Smooth transition from un-faded audio

**Actual**: [PASS/FAIL]

---

### Test 5.4: Fade Out During Playback
**Steps**:
1. Open a WAV file
2. Select last 3 seconds
3. Press Space to start playback
4. While playing, press Cmd+Shift+O

**Expected**:
- ✅ Fade applies in real-time
- ✅ Playback continues smoothly
- ✅ Position preserved

**Actual**: [PASS/FAIL]

---

### Test 5.5: Undo Fade Out
**Steps**:
1. Apply fade out
2. Press Cmd+Z (Undo)
3. Observe waveform

**Expected**:
- ✅ Waveform returns to original (no fade)
- ✅ Undo works correctly
- ✅ Redo (Cmd+Shift+Z) reapplies fade

**Actual**: [PASS/FAIL]

---

## Test Suite 6: Process Menu Integration

### Test 6.1: Process Menu Keyboard Shortcut
**Steps**:
1. Open a WAV file
2. Press Shift+G (Process Menu shortcut)

**Expected**:
- ✅ Dialog appears with gain shortcut information
- ✅ Shows "Use Shift+Up/Down for quick ±1dB gain adjustments"
- ✅ Helpful and informative message

**Actual**: [PASS/FAIL]

---

### Test 6.2: Process Menu Items Enabled/Disabled
**Steps**:
1. With no file loaded, click "Process" menu
2. Observe menu items
3. Load a file, click "Process" menu again

**Expected**:
- ✅ All Process menu items disabled when no file loaded
- ✅ Normalize enabled when file loaded
- ✅ Fade In/Out disabled when no selection
- ✅ Fade In/Out enabled when selection exists

**Actual**: [PASS/FAIL]

---

### Test 6.3: All Keyboard Shortcuts Work
**Steps**:
1. Open a WAV file
2. Test shortcuts:
   - Shift+G (Process menu info)
   - Shift+Up/Down (Gain)
   - Cmd+Shift+N (Normalize)
   - Cmd+Shift+I (Fade In - requires selection)
   - Cmd+Shift+O (Fade Out - requires selection)

**Expected**:
- ✅ All shortcuts trigger correct actions
- ✅ Shortcuts match Sound Forge conventions
- ✅ Shortcuts shown in menu items

**Actual**: [PASS/FAIL]

---

## Test Suite 7: Complex Workflow Testing

### Test 7.1: Complete Mastering Workflow
**Steps**:
1. Open a raw WAV file with low levels
2. Normalize to 0dB (Cmd+Shift+N)
3. Select first 1 second, apply fade in (Cmd+Shift+I)
4. Select last 1 second, apply fade out (Cmd+Shift+O)
5. Play entire file
6. Save (Cmd+S)

**Expected**:
- ✅ Each operation completes without errors
- ✅ Audio sounds professional (loud, no clicks)
- ✅ File saves correctly
- ✅ All edits persist when reopening

**Actual**: [PASS/FAIL]

---

### Test 7.2: Multiple Undos Across Features
**Steps**:
1. Apply gain +3dB (3x Shift+Up)
2. Normalize (Cmd+Shift+N)
3. Select region, fade in (Cmd+Shift+I)
4. Undo 5 times (Cmd+Z x5)
5. Redo 5 times (Cmd+Shift+Z x5)

**Expected**:
- ✅ Undo steps back through: fade → normalize → gain → gain → gain
- ✅ Each undo restores correct previous state
- ✅ Redo reapplies in correct order
- ✅ Final state matches before undo

**Actual**: [PASS/FAIL]

---

### Test 7.3: Selection-Based Edits Don't Affect Rest
**Steps**:
1. Open a WAV file
2. Play full file, note peak level in meters
3. Select middle 2 seconds
4. Normalize selection (Cmd+Shift+N)
5. Play full file again

**Expected**:
- ✅ Only middle section is louder
- ✅ Start and end sections unchanged (same peak level as before)
- ✅ Clear visual boundary in waveform
- ✅ No artifacts at boundaries

**Actual**: [PASS/FAIL]

---

## Test Suite 8: Edge Cases & Error Handling

### Test 8.1: Fade Entire File
**Steps**:
1. Open a short WAV file (5 seconds)
2. Select all (Cmd+A)
3. Apply fade in (Cmd+Shift+I)

**Expected**:
- ✅ Fade applies to entire file
- ✅ No errors or crashes
- ✅ Waveform shows fade across full duration

**Actual**: [PASS/FAIL]

---

### Test 8.2: Normalize Silent Region
**Steps**:
1. Open a WAV file
2. Select a silent region (or create one by deleting audio)
3. Apply normalize (Cmd+Shift+N)

**Expected**:
- ✅ No crash or error
- ✅ Silent region remains silent (can't amplify silence)
- ✅ Graceful handling (log warning or continue silently)

**Actual**: [PASS/FAIL]

---

### Test 8.3: Rapid Gain Adjustments
**Steps**:
1. Open a WAV file
2. Press Space to play
3. Rapidly press Shift+Up 10 times
4. Observe behavior

**Expected**:
- ✅ Each adjustment applies correctly
- ✅ Audio updates in real-time
- ✅ No glitches or crashes
- ✅ Playback continues smoothly

**Actual**: [PASS/FAIL]

---

### Test 8.4: Undo After Close and Reopen
**Steps**:
1. Open a file, apply gain
2. Save (Cmd+S)
3. Close file (Cmd+W)
4. Reopen same file (Cmd+O)
5. Press Cmd+Z (Undo)

**Expected**:
- ✅ File opens with gain applied (saved state)
- ✅ Undo history cleared (new session)
- ✅ No crash or unexpected behavior

**Actual**: [PASS/FAIL]

---

## Summary Checklist

### All Phase 2 Features
- [ ] Gain adjustment (+/- 1dB increments)
- [ ] Gain adjustment during playback (real-time)
- [ ] Selection-based gain
- [ ] Level meters (peak, RMS, clipping)
- [ ] Normalization (entire file)
- [ ] Normalization (selection only)
- [ ] Fade in (requires selection)
- [ ] Fade out (requires selection)
- [ ] Process menu keyboard shortcuts
- [ ] Undo/redo for all operations

### Critical Success Criteria
- [ ] No crashes or errors during testing
- [ ] All keyboard shortcuts work as documented
- [ ] Real-time playback updates work correctly
- [ ] Undo/redo works for all Phase 2 features
- [ ] Edits persist after save/reopen
- [ ] Professional audio quality (no clicks, pops, or glitches)

### Known Issues Found
*(Document any bugs or unexpected behavior)*

---

## Testing Conclusion

**Overall Status**: [PASS/FAIL/PARTIAL]

**Recommendation**:
- ✅ PASS: All features work correctly → Mark Phase 2 as 100% complete, proceed to Phase 3
- ⚠️ PARTIAL: Minor issues found → Fix issues before proceeding
- ❌ FAIL: Major bugs found → Fix critical bugs immediately

**Next Steps**:
1. [Document any bugs in TODO.md]
2. [Update TODO.md Phase 2 status]
3. [Proceed with Phase 3 or fix issues]

---

**Tester Signature**: ________________
**Date**: 2025-10-13
**Build Version**: 0.1.0-alpha-dev
