# QA Test Plan: Fade Curve Visual Thumbnails

## Feature Description
Added visual curve preview thumbnails to FadeIn and FadeOut dialogs, providing immediate visual feedback for curve type selection.

## Implementation Summary
- Created `FadeCurvePreview` component (100x60 pixels)
- Integrated into FadeInDialog and FadeOutDialog
- Shows real-time curve visualization matching selected type
- Dialog width increased from 400px to 520px
- Curves render using same formulas as AudioProcessor

## Test Cases

### 1. Fade In Dialog Visual Preview

**Test Steps:**
1. Open WaveEdit and load an audio file
2. Select a region or entire file
3. Open Process > Fade In
4. Verify curve preview appears to the right of ComboBox
5. Select each curve type and verify visualization:
   - **Linear**: Straight diagonal line from bottom-left to top-right
   - **Exponential**: Gentle curve starting slow, accelerating upward
   - **Logarithmic**: Sharp curve starting fast, decelerating upward
   - **S-Curve**: Sigmoid shape with smooth S pattern

**Expected Results:**
- Preview updates immediately on selection change
- Curve shows fade in pattern (0% → 100%)
- Grid lines visible at 25%, 50%, 75%
- Curve label shows at bottom-right
- Cyan color for curve line

### 2. Fade Out Dialog Visual Preview

**Test Steps:**
1. Open WaveEdit and load an audio file
2. Select a region or entire file
3. Open Process > Fade Out
4. Verify curve preview appears to the right of ComboBox
5. Select each curve type and verify visualization:
   - **Linear**: Straight diagonal line from top-left to bottom-right
   - **Exponential**: Curve starting at 100%, decelerating downward
   - **Logarithmic**: Sharp curve starting at 100%, accelerating downward
   - **S-Curve**: Inverted sigmoid shape

**Expected Results:**
- Preview updates immediately on selection change
- Curve shows fade out pattern (100% → 0%)
- Grid lines visible at 25%, 50%, 75%
- Visual correctly inverted from fade in
- Cyan color for curve line

### 3. Dialog Layout and Sizing

**Test Steps:**
1. Open Fade In dialog
2. Verify dialog width is 520px (increased from 400px)
3. Verify curve preview is positioned correctly
4. Test on smaller screen (1280x720 minimum)
5. Repeat for Fade Out dialog

**Expected Results:**
- Dialog fits comfortably on screen
- No overlapping UI elements
- Curve preview properly aligned
- Buttons remain accessible

### 4. Curve-Audio Consistency

**Test Steps:**
1. Select a curve type in Fade In dialog
2. Preview the audio effect
3. Verify the audible fade matches the visual curve
4. Apply the effect and verify in waveform
5. Repeat for all curve types
6. Repeat entire test for Fade Out dialog

**Expected Results:**
- Visual curve accurately represents audio processing
- Preview audio matches selected curve behavior
- Applied effect matches preview

### 5. Preference Persistence

**Test Steps:**
1. Select "Exponential" in Fade In dialog
2. Close and reopen dialog
3. Verify "Exponential" is still selected
4. Verify curve preview shows exponential curve
5. Select "S-Curve" in Fade Out dialog
6. Close and reopen dialog
7. Verify "S-Curve" is still selected
8. Verify preferences are saved separately for Fade In/Out

**Expected Results:**
- Last selected curve type persists
- Curve preview updates to match saved preference
- Fade In and Fade Out maintain separate preferences

### 6. Performance and Rendering

**Test Steps:**
1. Rapidly switch between curve types
2. Monitor CPU usage
3. Check for visual glitches or lag
4. Resize application window (if resizable)
5. Move dialog around screen

**Expected Results:**
- Smooth curve rendering
- No excessive CPU usage
- No visual artifacts
- Curve maintains quality during movement

## Visual Reference

### Curve Types Visual Guide:
```
LINEAR:           EXPONENTIAL:      LOGARITHMIC:      S-CURVE:
     /                   _/              /                 _/
    /                  _/               /                _/
   /                 _/                /               _/
  /                _/                 /_              /
 /               _/                  /  -           _/
/_____________  /____________       /_____________  /_____________
```

## Known Issues / Limitations
- Dialog width increased by 120px (acceptable trade-off)
- Curve preview is static (no animation)
- Grid lines are subtle (by design)

## Test Environment
- macOS 14.x or later
- 1280x720 minimum screen resolution
- WaveEdit v0.1.0 or later

## Files Changed
- Source/UI/FadeCurvePreview.h (NEW)
- Source/UI/FadeCurvePreview.cpp (NEW)
- Source/UI/FadeInDialog.h
- Source/UI/FadeInDialog.cpp
- Source/UI/FadeOutDialog.h
- Source/UI/FadeOutDialog.cpp
- CMakeLists.txt

## Risk Assessment
- **Low Risk**: Pure UI enhancement, no audio processing changes
- **No Breaking Changes**: Existing functionality preserved
- **Backward Compatible**: Settings migration handled automatically