# QA Test Plan: ParametricEQ Dialog Preview Functionality

## Test Date
2025-12-01

## Objective
Verify that the ParametricEQ dialog now has complete preview functionality matching other DSP dialogs.

## Test Setup
1. Launch WaveEdit application
2. Load a test audio file (WAV or FLAC)
3. Select a portion of audio (or use entire file)
4. Open Parametric EQ dialog (Processing → Parametric EQ or shortcut)

## Test Cases

### 1. UI Elements Present
- [ ] Preview button visible (90px width)
- [ ] Loop checkbox visible
- [ ] Three band controls visible (Low Shelf, Mid Peak, High Shelf)
- [ ] Reset, Apply, Cancel buttons present

### 2. Preview Basic Functionality
- [ ] Click Preview button starts playback
- [ ] Audio plays with EQ applied
- [ ] Playback cursor moves correctly
- [ ] Playback stops at end of selection (if Loop unchecked)

### 3. Loop Functionality
- [ ] Check Loop checkbox
- [ ] Click Preview - audio loops continuously
- [ ] Uncheck Loop during playback - stops looping at end of current iteration

### 4. EQ Parameter Changes
- [ ] Adjust Low Shelf gain to +10dB
- [ ] Click Preview - hear bass boost
- [ ] Adjust Mid Peak frequency to 1kHz, gain to -10dB
- [ ] Click Preview - hear mid-range cut
- [ ] Adjust High Shelf gain to +10dB
- [ ] Click Preview - hear treble boost

### 5. Reset Button
- [ ] Make EQ adjustments
- [ ] Click Reset - all gains return to 0dB
- [ ] Click Preview - audio plays flat (no EQ)

### 6. Multiple Preview Sessions
- [ ] Start preview
- [ ] Change EQ parameters
- [ ] Click Preview again - new settings applied
- [ ] Previous playback stops before new one starts

### 7. Dialog Close Behavior
- [ ] Start preview playback
- [ ] Click Cancel - preview stops, dialog closes
- [ ] Open dialog again, start preview
- [ ] Click Apply - preview stops, EQ is applied to file

### 8. Cursor Position Accuracy
- [ ] Select middle portion of file
- [ ] Open EQ dialog and preview
- [ ] Cursor should move within selection bounds only
- [ ] Cursor position matches audio playback position

### 9. Preview Coordinate System
- [ ] Select audio from 5s to 10s
- [ ] Preview should play 5 seconds of audio
- [ ] Loop points should be 0 to 5s in preview buffer
- [ ] Cursor display shows correct file position (5s-10s range)

## Expected Results
- All preview functionality works identically to NormalizeDialog
- Preview uses OFFLINE_BUFFER mode (pre-rendered)
- EQ changes are audible in preview
- No crashes or audio glitches
- Proper cleanup when dialog closes

## Implementation Details Verified
- Constructor takes AudioEngine* and AudioBufferManager* parameters
- Selection start/end passed to constructor
- Preview button callback implemented (onPreviewClicked)
- setPreviewSelectionOffset() called for cursor accuracy
- visibilityChanged() stops preview on dialog hide
- Destructor stops preview on dialog destruction
- getCurrentParameters() helper method implemented

## Code Patterns Followed
- OFFLINE_BUFFER preview mode (like NormalizeDialog)
- Clear loop points before setting new ones
- Configure looping before loading buffer
- Set preview selection offset for cursor positioning
- Preview buffer coordinates are 0-based
- File coordinates maintained for cursor display

## Files Modified
1. `/Users/zacharylquarles/PROJECTS_Apps/Project_WaveEditor/Source/UI/ParametricEQDialog.h`
   - Added AudioEngine* and AudioBufferManager* parameters
   - Added Preview button and Loop checkbox members
   - Added preview-related methods

2. `/Users/zacharylquarles/PROJECTS_Apps/Project_WaveEditor/Source/UI/ParametricEQDialog.cpp`
   - Implemented constructor with preview parameters
   - Added onPreviewClicked() method
   - Added visibilityChanged() override
   - Added getCurrentParameters() helper
   - Destructor cleanup implementation

3. `/Users/zacharylquarles/PROJECTS_Apps/Project_WaveEditor/Source/Main.cpp`
   - Updated ParametricEQDialog::showDialog() call with new parameters

## Build Status
✅ Builds successfully without errors
⚠️ Warnings present (deprecated Font constructor) - existing issue, not related to preview

## Notes
- Preview functionality matches the established pattern from other dialogs
- Uses ParametricEQ::applyEQ() for processing
- Thread-safe implementation on message thread
- Proper resource cleanup implemented