# Regression Test Plan: Preview Coordinate System Fix

**Fix Date:** 2025-11-25
**Modified Files:**
- `/Source/UI/NormalizeDialog.cpp`
- `/Source/UI/FadeInDialog.cpp`
- `/Source/UI/FadeOutDialog.cpp`
- `/Source/UI/DCOffsetDialog.cpp`

**Bug Fixed:** Missing `setLoopPoints()` calls in preview workflows causing preview playback to use incorrect coordinates (file coordinates instead of preview buffer coordinates).

**Fix Applied:** Added `setLoopPoints(0.0, selectionLengthSec)` in PREVIEW BUFFER coordinates for all 4 processing dialogs.

---

## Test Configuration

**Test Audio File Requirements:**
- **File Duration:** 60 seconds minimum
- **Sample Rate:** 44100 Hz or 48000 Hz
- **Channels:** Stereo
- **Format:** WAV (16-bit or 24-bit)
- **Content:** Tone sweep, music, or speech with identifiable start/middle/end sections

**Build Configuration:**
- Build WaveEdit from latest code
- Verify commit includes all 4 dialog fixes
- Test on Debug build for better logging

---

## Category 1: Preview Start/End Position Tests

### TC-POS-001: Preview Plays from Selection Start (Middle of File)

**Dialog:** Normalize
**Test ID:** TC-POS-001
**Priority:** P0 (Critical)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 40.0s (middle 10 seconds)
- Open Process → Normalize dialog

**Test Steps:**
1. Click "Preview" button
2. Observe audio playback
3. Listen to first audio content heard

**Expected Result:**
- Preview plays audio starting from the 30-second mark of the original file
- Preview does NOT play from file start (0 seconds)
- Loop toggle is OFF by default

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Any observations]_

---

### TC-POS-002: Preview Stops at Selection End (Middle of File)

**Dialog:** Fade In
**Test ID:** TC-POS-002
**Priority:** P0 (Critical)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 40.0s (middle 10 seconds)
- Open Process → Fade In dialog

**Test Steps:**
1. Click "Preview" button
2. Let preview play to completion without looping
3. Observe when playback stops

**Expected Result:**
- Preview plays for exactly 10 seconds
- Preview stops at 40-second content (selection end)
- Preview does NOT continue playing beyond selection end

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Any observations]_

---

### TC-POS-003: Preview with Selection at File Start

**Dialog:** Fade Out
**Test ID:** TC-POS-003
**Priority:** P1 (High)

**Preconditions:**
- Load 60-second audio file
- Create selection from 0.0s to 10.0s (first 10 seconds)
- Open Process → Fade Out dialog

**Test Steps:**
1. Click "Preview" button
2. Listen to audio playback
3. Verify playback duration

**Expected Result:**
- Preview plays audio from file start
- Preview stops after exactly 10 seconds
- No gaps or silence at preview start

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Any observations]_

---

### TC-POS-004: Preview with Selection at File End

**Dialog:** DC Offset
**Test ID:** TC-POS-004
**Priority:** P1 (High)

**Preconditions:**
- Load 60-second audio file
- Create selection from 50.0s to 60.0s (last 10 seconds)
- Open Process → Remove DC Offset dialog

**Test Steps:**
1. Click "Preview" button
2. Listen to audio playback
3. Verify playback starts with last 10 seconds of file

**Expected Result:**
- Preview plays audio from the 50-second mark
- Preview plays exactly 10 seconds of content
- Preview does NOT play from file start

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Any observations]_

---

### TC-POS-005: Preview with Very Short Selection

**Dialog:** Normalize
**Test ID:** TC-POS-005
**Priority:** P2 (Medium)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 30.5s (0.5 seconds)
- Open Process → Normalize dialog

**Test Steps:**
1. Click "Preview" button
2. Listen to audio playback
3. Verify playback duration is very short

**Expected Result:**
- Preview plays exactly 0.5 seconds of audio
- Preview starts at 30-second mark
- No playback issues with short duration

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Any observations]_

---

### TC-POS-006: Preview with Full File Selection

**Dialog:** Fade In
**Test ID:** TC-POS-006
**Priority:** P2 (Medium)

**Preconditions:**
- Load 60-second audio file
- Select entire file (Ctrl+A / Cmd+A)
- Verify selection is 0.0s to 60.0s
- Open Process → Fade In dialog

**Test Steps:**
1. Click "Preview" button
2. Let preview play to completion
3. Verify entire file is played

**Expected Result:**
- Preview plays all 60 seconds of audio
- Preview starts at file start
- Preview stops at file end

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Any observations]_

---

## Category 2: Loop Preview Tests

### TC-LOOP-001: Loop Toggle Enabled - Repeats Selection

**Dialog:** Normalize
**Test ID:** TC-LOOP-001
**Priority:** P0 (Critical)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 35.0s (5 seconds)
- Open Process → Normalize dialog

**Test Steps:**
1. Enable "Loop Preview" checkbox
2. Click "Preview" button
3. Let playback run for 20+ seconds
4. Listen for loop behavior

**Expected Result:**
- Preview plays 5-second selection
- After 5 seconds, preview immediately restarts from beginning of selection
- Loop continues seamlessly multiple times
- No gaps or clicks at loop boundary

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Any observations]_

---

### TC-LOOP-002: Loop Toggle Disabled - No Repeat

**Dialog:** Fade Out
**Test ID:** TC-LOOP-002
**Priority:** P0 (Critical)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 35.0s (5 seconds)
- Open Process → Fade Out dialog

**Test Steps:**
1. Verify "Loop Preview" checkbox is OFF (default)
2. Click "Preview" button
3. Wait for playback to complete
4. Observe playback behavior after 5 seconds

**Expected Result:**
- Preview plays 5-second selection once
- Playback stops after 5 seconds
- Preview does NOT restart or loop

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Any observations]_

---

### TC-LOOP-003: Loop Restarts at Selection Start

**Dialog:** Fade In
**Test ID:** TC-LOOP-003
**Priority:** P0 (Critical)

**Preconditions:**
- Load 60-second audio file with distinguishable content (e.g., tone sweep or speech)
- Create selection from 30.0s to 35.0s (5 seconds)
- Open Process → Fade In dialog

**Test Steps:**
1. Enable "Loop Preview" checkbox
2. Click "Preview" button
3. Listen carefully to the first 2 loop iterations
4. Verify loop restart point matches selection start

**Expected Result:**
- First loop: plays 30s-35s content
- Loop boundary: seamless transition
- Second loop: plays 30s-35s content again (NOT 0s-5s content)
- Loop restarts at selection start (30s), not file start (0s)

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Any observations]_

---

### TC-LOOP-004: Loop Toggle State Persists Across Preview Clicks

**Dialog:** Normalize
**Test ID:** TC-LOOP-004
**Priority:** P2 (Medium)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 35.0s (5 seconds)
- Open Process → Normalize dialog

**Test Steps:**
1. Enable "Loop Preview" checkbox
2. Click "Preview" button - verify looping works
3. Click "Preview" again to stop
4. Click "Preview" again
5. Verify looping still enabled

**Expected Result:**
- Loop toggle state persists between preview cycles
- Second preview session still loops
- No need to re-enable loop toggle

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Any observations]_

---

### TC-LOOP-005: Toggling Loop During Preview (If Supported)

**Dialog:** DC Offset
**Test ID:** TC-LOOP-005
**Priority:** P3 (Low)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 35.0s (5 seconds)
- Open Process → Remove DC Offset dialog

**Test Steps:**
1. Click "Preview" button (loop OFF)
2. While preview is playing, enable "Loop Preview" checkbox
3. Observe if loop activates mid-playback

**Expected Result:**
- Either:
  - (A) Loop activates and preview restarts when reaching end, OR
  - (B) Loop toggle has no effect until next preview click (acceptable)
- No crashes or audio glitches

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Behavior depends on UI implementation]_

---

## Category 3: Coordinate System Tests

### TC-COORD-001: Loop Points Set in Preview Buffer Coordinates

**Dialog:** Normalize
**Test ID:** TC-COORD-001
**Priority:** P0 (Critical)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 40.0s (10 seconds)
- Open Process → Normalize dialog

**Test Steps:**
1. Enable "Loop Preview" checkbox
2. Click "Preview" button
3. Check debug logs for `setLoopPoints()` call
4. Verify loop points are 0.0 to 10.0 (NOT 30.0 to 40.0)

**Expected Result:**
- Log shows: `setLoopPoints(0.0, 10.0)` (preview buffer coordinates)
- Log does NOT show: `setLoopPoints(30.0, 40.0)` (file coordinates)
- Playback loops correctly over 10-second selection

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Check JUCE debug console output]_

---

### TC-COORD-002: Preview Buffer Starts at 0.0 Seconds

**Dialog:** Fade In
**Test ID:** TC-COORD-002
**Priority:** P0 (Critical)

**Preconditions:**
- Load 60-second audio file
- Create selection from 45.0s to 50.0s (5 seconds near end)
- Open Process → Fade In dialog

**Test Steps:**
1. Enable "Loop Preview" checkbox
2. Click "Preview" button
3. Check debug logs for preview buffer creation
4. Verify loop start is 0.0 (not 45.0)

**Expected Result:**
- Preview buffer is 5 seconds long
- Preview buffer coordinate system: 0.0s to 5.0s
- Loop point start: 0.0s (preview buffer coordinate)
- Playback starts with content from 45s mark of original file

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Verify via logs or code inspection]_

---

### TC-COORD-003: No Coordinate System Mixing

**Dialog:** Fade Out
**Test ID:** TC-COORD-003
**Priority:** P0 (Critical)

**Preconditions:**
- Load 60-second audio file
- Create selection from 20.0s to 30.0s (10 seconds)
- Open Process → Fade Out dialog

**Test Steps:**
1. Enable "Loop Preview" checkbox
2. Click "Preview" button
3. Verify loop behavior matches selection length (10s)
4. Check that loop does NOT play first 10s of file

**Expected Result:**
- Loop plays 10-second selection (20s-30s content)
- Loop duration is 10 seconds (matching selection)
- NO mixing of file coordinates with buffer coordinates
- Loop does NOT incorrectly play 0s-10s content

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Listen carefully to verify correct audio content]_

---

### TC-COORD-004: clearLoopPoints() Called Before Preview

**Dialog:** DC Offset
**Test ID:** TC-COORD-004
**Priority:** P1 (High)

**Preconditions:**
- Load 60-second audio file
- Create selection from 10.0s to 20.0s
- Open Process → Remove DC Offset dialog

**Test Steps:**
1. Click "Preview" button (loop OFF)
2. Stop preview
3. Enable "Loop Preview" checkbox
4. Click "Preview" button again
5. Check logs for `clearLoopPoints()` call

**Expected Result:**
- `clearLoopPoints()` is called at start of `onPreviewClicked()`
- No stale loop points from previous preview sessions
- Second preview uses fresh loop configuration

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Check JUCE debug console output]_

---

## Category 4: State Restoration Tests

### TC-STATE-001: Preview Mode Returns to DISABLED After Stop

**Dialog:** Normalize
**Test ID:** TC-STATE-001
**Priority:** P1 (High)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 40.0s
- Open Process → Normalize dialog

**Test Steps:**
1. Click "Preview" button
2. Let preview play for 2 seconds
3. Click "Cancel" button to close dialog
4. Check AudioEngine preview mode state

**Expected Result:**
- Dialog destructor calls `setPreviewMode(PreviewMode::DISABLED)`
- AudioEngine is no longer in preview mode
- Ready for normal playback

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Verify via debug logs or code inspection]_

---

### TC-STATE-002: Main Playback Works After Preview

**Dialog:** Fade In
**Test ID:** TC-STATE-002
**Priority:** P0 (Critical)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 40.0s
- Open Process → Fade In dialog

**Test Steps:**
1. Click "Preview" button
2. Let preview loop 2 times
3. Click "Cancel" to close dialog
4. Click main transport "Play" button
5. Verify normal playback from cursor position

**Expected Result:**
- Preview stops and cleans up
- Main playback plays full audio file normally
- No leftover loop points affecting main playback
- Playback position is correct

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Critical for workflow integrity]_

---

### TC-STATE-003: Loop Points Cleared When Exiting Preview

**Dialog:** Fade Out
**Test ID:** TC-STATE-003
**Priority:** P1 (High)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 40.0s
- Open Process → Fade Out dialog

**Test Steps:**
1. Enable "Loop Preview" checkbox
2. Click "Preview" button
3. Let preview loop once
4. Click "Apply" button
5. Check AudioEngine loop state after dialog closes

**Expected Result:**
- Dialog cleanup clears loop points
- AudioEngine looping is disabled
- No stale loop points remain

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Verify via logs or subsequent playback test]_

---

### TC-STATE-004: m_isPlayingFromBuffer Not Affected by Preview

**Dialog:** DC Offset
**Test ID:** TC-STATE-004
**Priority:** P2 (Medium)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 40.0s
- Open Process → Remove DC Offset dialog

**Test Steps:**
1. Click "Preview" button
2. Let preview play
3. Click "Cancel"
4. Verify AudioEngine state flags

**Expected Result:**
- Preview mode uses `PreviewMode::OFFLINE_BUFFER`
- `m_isPlayingFromBuffer` flag is NOT permanently changed
- AudioEngine returns to normal state after preview

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Requires code inspection or debug logging]_

---

### TC-STATE-005: Multiple Preview Sessions in Same Dialog

**Dialog:** Normalize
**Test ID:** TC-STATE-005
**Priority:** P2 (Medium)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 40.0s
- Open Process → Normalize dialog

**Test Steps:**
1. Click "Preview" button
2. Let preview play for 3 seconds
3. Click "Preview" button again (stop)
4. Change target level slider
5. Click "Preview" button again
6. Verify preview plays updated processing

**Expected Result:**
- First preview plays with initial settings
- Second preview plays with updated settings
- No state corruption between preview sessions
- Each preview session clears previous state correctly

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Test state cleanup between previews]_

---

## Category 5: Multi-Dialog Consistency Tests

### TC-MULTI-001: All Dialogs Use Identical Preview Pattern

**Dialogs:** Normalize, Fade In, Fade Out, DC Offset
**Test ID:** TC-MULTI-001
**Priority:** P1 (High)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 35.0s (5 seconds)

**Test Steps:**
1. For each dialog (Normalize, Fade In, Fade Out, DC Offset):
   - Open dialog
   - Enable "Loop Preview"
   - Click "Preview"
   - Listen to loop behavior
   - Verify loop plays 5-second selection
   - Close dialog
2. Compare behavior across all 4 dialogs

**Expected Result:**
- All 4 dialogs have identical preview behavior
- All play from selection start (30s content)
- All loop over 5-second selection
- All use loop points (0.0, 5.0) in preview buffer coordinates
- Consistent user experience across all processing dialogs

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Document any inconsistencies]_

---

### TC-MULTI-002: Same Selection, Different Processing, Same Preview Timing

**Dialogs:** Normalize, Fade In, Fade Out, DC Offset
**Test ID:** TC-MULTI-002
**Priority:** P2 (Medium)

**Preconditions:**
- Load 60-second audio file
- Create selection from 25.0s to 30.0s (5 seconds)

**Test Steps:**
1. Open Normalize dialog
   - Click "Preview" (loop OFF)
   - Measure preview duration with stopwatch
2. Cancel and repeat for Fade In dialog
3. Cancel and repeat for Fade Out dialog
4. Cancel and repeat for DC Offset dialog

**Expected Result:**
- All 4 dialogs preview for exactly 5 seconds
- All start at same audio content (25s mark)
- All stop at same point (30s mark)
- Duration is identical across all dialogs

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Record measured durations]_

---

### TC-MULTI-003: Loop Toggle Position Consistency

**Dialogs:** Normalize, Fade In, Fade Out, DC Offset
**Test ID:** TC-MULTI-003
**Priority:** P3 (Low)

**Preconditions:**
- Launch WaveEdit

**Test Steps:**
1. Open each dialog in sequence
2. Note position of "Loop Preview" checkbox
3. Verify checkbox is in same location for all dialogs

**Expected Result:**
- "Loop Preview" checkbox is in consistent position across all 4 dialogs
- UI layout is uniform
- Professional user experience

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[UI consistency check]_

---

## Category 6: Edge Cases

### TC-EDGE-001: Selection Shorter Than 100ms

**Dialog:** Normalize
**Test ID:** TC-EDGE-001
**Priority:** P2 (Medium)

**Preconditions:**
- Load 60-second audio file
- Create selection of 50ms (0.050 seconds)
- Open Process → Normalize dialog

**Test Steps:**
1. Enable "Loop Preview" checkbox
2. Click "Preview" button
3. Listen for loop behavior with very short selection

**Expected Result:**
- Preview plays 50ms selection
- Loop restarts immediately (20 loops per second)
- No crashes, glitches, or audio artifacts
- Rapid looping is audible and stable

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[May sound like a buzz/drone]_

---

### TC-EDGE-002: Selection at Sample Boundary

**Dialog:** Fade In
**Test ID:** TC-EDGE-002
**Priority:** P3 (Low)

**Preconditions:**
- Load 44100 Hz audio file
- Create selection from exactly 441 samples to 44100 samples (0.01s to 1.0s)
- Open Process → Fade In dialog

**Test Steps:**
1. Click "Preview" button
2. Verify preview duration is exactly 0.99 seconds
3. Check for off-by-one errors

**Expected Result:**
- Preview plays exactly (44100 - 441) / 44100 = 0.99 seconds
- No off-by-one errors in duration
- Loop point end is correct

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Sample-accuracy test]_

---

### TC-EDGE-003: Cancel During Preview Playback

**Dialog:** Fade Out
**Test ID:** TC-EDGE-003
**Priority:** P1 (High)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 40.0s
- Open Process → Fade Out dialog

**Test Steps:**
1. Enable "Loop Preview" checkbox
2. Click "Preview" button
3. While looping, click "Cancel" button
4. Observe cleanup behavior

**Expected Result:**
- Playback stops immediately
- No audio glitches or crashes
- Preview mode set to DISABLED
- AudioEngine returned to normal state

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Test cleanup robustness]_

---

### TC-EDGE-004: Apply During Preview Playback

**Dialog:** DC Offset
**Test ID:** TC-EDGE-004
**Priority:** P1 (High)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 40.0s
- Open Process → Remove DC Offset dialog

**Test Steps:**
1. Click "Preview" button
2. While preview is playing, click "Apply" button
3. Observe behavior and check for crashes

**Expected Result:**
- Preview stops before applying processing
- Processing is applied to correct selection (30s-40s)
- No crashes or audio corruption
- Undo works correctly

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Critical safety test]_

---

### TC-EDGE-005: Rapid Preview Start/Stop Cycles

**Dialog:** Normalize
**Test ID:** TC-EDGE-005
**Priority:** P2 (Medium)

**Preconditions:**
- Load 60-second audio file
- Create selection from 30.0s to 35.0s
- Open Process → Normalize dialog

**Test Steps:**
1. Rapidly click "Preview" button 10 times in 5 seconds
2. Observe system stability
3. Check for memory leaks or resource issues

**Expected Result:**
- AudioEngine handles rapid start/stop gracefully
- No crashes or audio glitches
- Each click toggles preview playback correctly
- No resource exhaustion

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Stress test for cleanup robustness]_

---

### TC-EDGE-006: Loop Preview with Mono vs Stereo Files

**Dialog:** Fade In
**Test ID:** TC-EDGE-006
**Priority:** P2 (Medium)

**Preconditions:**
- Prepare one mono WAV file (60s)
- Prepare one stereo WAV file (60s)

**Test Steps:**
1. Load mono file
   - Create selection 30.0s to 35.0s
   - Open Fade In dialog
   - Enable loop preview
   - Click "Preview"
   - Verify correct behavior
2. Close file
3. Load stereo file
   - Repeat same steps

**Expected Result:**
- Both mono and stereo files preview correctly
- Loop points work identically for both channel configurations
- No channel-related issues

**Actual Result:** _[To be filled during testing]_
**Status:** _[PASS / FAIL / BLOCKED]_
**Notes:** _[Test channel independence]_

---

## Test Summary Template

**Test Date:** _________
**Tester Name:** _________
**Build Version:** _________
**Platform:** [ ] macOS [ ] Windows [ ] Linux

**Total Test Cases:** 31
**Passed:** _____
**Failed:** _____
**Blocked:** _____

**Critical Issues Found:** _____
**Pass Rate:** _____%

---

## Severity Definitions

**P0 (Critical):** Core functionality broken, blocks release
**P1 (High):** Major feature broken, should fix before release
**P2 (Medium):** Minor issue, fix if time permits
**P3 (Low):** Cosmetic or edge case, can defer

---

## Test Execution Notes

1. **Use headphones or good speakers** to clearly hear loop boundaries and audio content
2. **Check JUCE console logs** for coordinate system debug output
3. **Test on target platform** (macOS, Windows, or Linux as applicable)
4. **Document any crashes** with stack traces if available
5. **Record unexpected behaviors** even if not explicitly covered by test cases
6. **Verify fix completeness** by checking all 4 dialog source files

---

## Code Review Checklist

For each dialog, verify the following code pattern exists:

```cpp
void Dialog::onPreviewClicked()
{
    // 0. Stop any current playback FIRST
    if (m_audioEngine->isPlaying())
        m_audioEngine->stop();

    // 1. Clear stale loop points (CRITICAL for coordinate system)
    m_audioEngine->clearLoopPoints();

    // 2. Configure looping based on loop toggle
    bool shouldLoop = m_loopToggle.getToggleState();
    m_audioEngine->setLooping(shouldLoop);

    // 3. Extract selection
    int64_t numSamples = m_selectionEnd - m_selectionStart;
    auto workBuffer = m_bufferManager->getAudioRange(m_selectionStart, numSamples);
    const double sampleRate = m_bufferManager->getSampleRate();
    const int numChannels = workBuffer.getNumChannels();

    // 4. Apply processing to copy

    // 5. Load into preview system
    m_audioEngine->loadPreviewBuffer(workBuffer, sampleRate, numChannels);

    // 6. Set preview mode and position
    m_audioEngine->setPreviewMode(PreviewMode::OFFLINE_BUFFER);
    m_audioEngine->setPosition(0.0);

    // CRITICAL: Set loop points in PREVIEW BUFFER coordinates (0-based)
    double selectionLengthSec = numSamples / sampleRate;
    if (shouldLoop)
    {
        m_audioEngine->setLoopPoints(0.0, selectionLengthSec);
    }

    // 7. Start playback
    m_audioEngine->play();
}
```

**Files to verify:**
- [x] `/Source/UI/NormalizeDialog.cpp` - lines 235-312
- [x] `/Source/UI/FadeInDialog.cpp` - lines 120-166
- [x] `/Source/UI/FadeOutDialog.cpp` - lines 120-166
- [x] `/Source/UI/DCOffsetDialog.cpp` - lines 121-167

---

## Regression Prevention

**Future Dialogs:** When creating new processing dialogs with preview functionality:

1. Copy preview code from one of these 4 dialogs
2. Ensure `clearLoopPoints()` is called first
3. Ensure `setLoopPoints(0.0, selectionLengthSec)` uses preview buffer coordinates
4. Ensure loop points are only set when `shouldLoop == true`
5. Run these regression tests on the new dialog

**Code Review Focus:**
- Look for any `setLoopPoints()` calls using selection start/end (file coordinates) instead of 0.0 and duration
- Verify loop toggle state is respected
- Verify preview mode is set to OFFLINE_BUFFER
- Verify cleanup in destructor and cancel/apply handlers

---

**End of Test Plan**
