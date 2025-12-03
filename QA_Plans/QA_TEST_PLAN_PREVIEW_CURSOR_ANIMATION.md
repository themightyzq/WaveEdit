# QA Test Plan: Preview Selection Cursor Animation

## Overview
This test plan verifies that the playback cursor animates correctly during preview mode when processing a selection with effects like Gain, showing the cursor moving through the actual selection position rather than jumping to the beginning of the file.

## Test Environment
- WaveEdit build with preview selection offset tracking
- Test audio file with identifiable transients
- Selection in middle of file (not at beginning)

## Test Cases

### TC1: Basic Preview Cursor Animation
**Objective**: Verify cursor animates through selection during preview

**Steps**:
1. Open an audio file
2. Make a selection in the middle of the file (e.g., 5.0s to 10.0s)
3. Open Process → Gain dialog
4. Click Preview button
5. Observe playback cursor

**Expected Results**:
- ✅ Cursor starts at selection start (5.0s)
- ✅ Cursor animates from selection start to selection end
- ✅ Cursor moves smoothly, tracking audio playback
- ✅ Cursor does NOT jump to 0.0s

### TC2: Looped Preview Cursor Animation
**Objective**: Verify cursor loops correctly through selection

**Steps**:
1. Open an audio file
2. Make a selection in the middle (e.g., 3.0s to 5.0s)
3. Open Process → Gain dialog
4. Enable "Loop Preview" checkbox
5. Click Preview button
6. Let preview loop at least twice

**Expected Results**:
- ✅ Cursor animates from 3.0s to 5.0s
- ✅ When reaching 5.0s, cursor jumps back to 3.0s
- ✅ Cursor continues animating through selection
- ✅ Cursor never goes outside selection bounds

### TC3: Preview with Gain Adjustment
**Objective**: Verify cursor continues animating when adjusting gain during preview

**Steps**:
1. Open an audio file
2. Make a selection (e.g., 10.0s to 15.0s)
3. Open Process → Gain dialog
4. Click Preview button
5. While preview is playing, adjust gain slider

**Expected Results**:
- ✅ Cursor continues animating through selection
- ✅ Cursor position remains accurate during gain changes
- ✅ Audio updates in real-time with gain changes
- ✅ Cursor doesn't reset or jump when gain changes

### TC4: Preview Near End of File
**Objective**: Verify cursor animation with selection near file end

**Steps**:
1. Open an audio file
2. Make a selection near the end (last 2 seconds)
3. Open Process → Gain dialog
4. Click Preview button

**Expected Results**:
- ✅ Cursor starts at correct selection position
- ✅ Cursor animates to end of selection
- ✅ Playback stops at selection end
- ✅ No cursor overflow or wrapping

### TC5: Stop and Restart Preview
**Objective**: Verify cursor resets correctly when stopping/starting preview

**Steps**:
1. Open an audio file
2. Make a selection (e.g., 7.0s to 12.0s)
3. Open Process → Gain dialog
4. Click Preview button
5. Let cursor reach middle of selection
6. Click Preview again to stop
7. Click Preview again to restart

**Expected Results**:
- ✅ First preview: cursor animates from 7.0s
- ✅ When stopped: cursor stops at current position
- ✅ When restarted: cursor starts from 7.0s again
- ✅ Cursor animation remains smooth on restart

### TC6: Preview Whole File vs Selection
**Objective**: Verify cursor behavior differs between whole file and selection preview

**Steps**:
1. Open an audio file
2. Clear any selection (Ctrl+D)
3. Open Process → Gain dialog
4. Click Preview (whole file mode)
5. Observe cursor for a few seconds
6. Click Cancel
7. Make a selection in middle of file
8. Open Process → Gain dialog again
9. Click Preview (selection mode)

**Expected Results**:
- ✅ Whole file: cursor starts at 0.0s
- ✅ Whole file: cursor animates through entire file
- ✅ Selection: cursor starts at selection start
- ✅ Selection: cursor only animates through selection range

### TC7: Multiple Document Preview
**Objective**: Verify cursor animation is document-specific

**Steps**:
1. Open two different audio files in tabs
2. In first tab, make selection at 5.0s-8.0s
3. In second tab, make selection at 2.0s-4.0s
4. In first tab, open Gain dialog and preview
5. Switch to second tab while first is previewing
6. Observe cursor in second tab

**Expected Results**:
- ✅ First tab: cursor animates through 5.0s-8.0s
- ✅ Second tab: cursor remains static (not affected)
- ✅ Only the previewing document shows animation
- ✅ No cursor interference between documents

### TC8: Preview After Undo
**Objective**: Verify cursor animation after undoing an edit

**Steps**:
1. Open an audio file
2. Make a selection (e.g., 4.0s to 9.0s)
3. Apply a gain change and commit
4. Undo the change (Ctrl+Z)
5. Open Gain dialog again
6. Click Preview

**Expected Results**:
- ✅ Cursor animates through original selection
- ✅ Selection boundaries are preserved after undo
- ✅ Cursor position calculation remains accurate
- ✅ Preview plays the correct audio range

## Performance Criteria
- Cursor update rate: minimum 30 fps
- No visible lag between audio and cursor position
- Smooth animation without stuttering
- Accurate positioning (within 1 pixel of expected position)

## Known Issues to Watch For
- Cursor jumping to 0.0s when preview starts
- Cursor freezing during preview
- Cursor position drifting from audio over time
- Cursor not returning to selection start when looping

## Test Status
- [ ] TC1: Basic Preview Cursor Animation
- [ ] TC2: Looped Preview Cursor Animation
- [ ] TC3: Preview with Gain Adjustment
- [ ] TC4: Preview Near End of File
- [ ] TC5: Stop and Restart Preview
- [ ] TC6: Preview Whole File vs Selection
- [ ] TC7: Multiple Document Preview
- [ ] TC8: Preview After Undo

## Notes
- Priority focus on selections in middle of file (most common use case)
- Test with various sample rates to ensure timing accuracy
- Verify with both mono and stereo files