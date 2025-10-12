# WaveEdit - Project TODO

**Last Updated**: 2025-10-12 (MONO PLAYBACK FIX COMPLETE & USER-VERIFIED)
**Current Phase**: Phase 2 (Professional Features) - **STARTED**
**Status**: ✅ **Code Quality 8.5/10 | Mono Bug FIXED & VERIFIED | Ready for Level Meters**
**Target**: Implement critical musician features (gain ✅, mono ✅ VERIFIED, meters, normalize, fade)

---

## 🔥 PHASE 2: CRITICAL MUSICIAN FEATURES (In Progress)

### ✅ CODE REVIEW + MONO PLAYBACK BUG FIX **[COMPLETE]**
**Status**: ✅ **COMPLETED**
**Time Spent**: 2 hours (comprehensive code review + bug fix)
**Completion Date**: 2025-10-12

**Code Review Findings**:
- **Overall Code Quality**: 8.5/10 - Professional Grade ✅
- **CLAUDE.md Adherence**: 9/10 - Excellent ✅
- **Thread Safety**: 10/10 - Perfect implementation ✅
- **Memory Management**: 10/10 - RAII throughout, no raw pointers ✅
- **Error Handling**: 10/10 - Comprehensive, all cases covered ✅
- **Shortcuts/Band-aids**: NONE found (1 documented MVP placeholder only) ✅

**Mono Playback Bug Fixed** ✅ **USER-VERIFIED WORKING**:
- **Problem**: Mono audio files only played on left channel (not centered)
- **Root Cause**: TWO separate code paths needed fixing:
  1. **Buffer Playback Path**: `MemoryAudioSource::getNextAudioBlock()` only copied existing channels
  2. **File Playback Path**: `audioDeviceIOCallbackWithContext()` didn't duplicate mono to stereo
- **Fix Applied**: Implemented mono-to-stereo duplication in BOTH code paths:
  1. **Buffer playback** (lines 122-151): Duplicate mono during getNextAudioBlock()
  2. **File playback** (lines 738-748): Duplicate mono in audio callback after transport fills buffer
- **Implementation Details**:
  ```cpp
  // Path 1: MemoryAudioSource::getNextAudioBlock() (lines 128-138)
  if (sourceChannels == 1 && outputChannels == 2) {
      // Duplicate mono to both L and R during buffer copy
      bufferToFill.buffer->copyFrom(0, ..., m_buffer, 0, ...);
      bufferToFill.buffer->copyFrom(1, ..., m_buffer, 0, ...);
  }

  // Path 2: audioDeviceIOCallbackWithContext() (lines 742-748)
  if (sourceChannels == 1 && numOutputChannels == 2) {
      // After transport fills buffer, duplicate channel 0 to channel 1
      buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
  }
  ```
- **Result**: ✅ Mono files now play equally on both channels (professional behavior)
- **User Feedback**: "Great! Thanks fixed." - Complete fix confirmed working in production

**Code Review Agent Assessment**:
```
"The codebase demonstrates exceptional quality with professional-grade
architecture and implementation. The code strictly adheres to CLAUDE.md
standards with no shortcuts or band-aids (except one clearly documented
MVP placeholder). Thread safety and memory management are exemplary.

Overall, this is production-ready code that would pass a professional
code review with flying colors once the mono bug is fixed."
```

**Files Modified**:
1. `Source/Audio/AudioEngine.cpp` - Fixed mono-to-stereo playback:
   - Lines 122-151: Buffer playback path (MemoryAudioSource)
   - Lines 738-748: File playback path (audio callback)
2. `CLAUDE.md` - Updated with comprehensive code review findings
3. `README.md` - Updated status with code quality ratings
4. `TODO.md` - Added code review results section

**All Success Criteria Met**:
- ✅ Comprehensive code review completed by specialized agent
- ✅ No shortcuts or band-aids found in codebase
- ✅ Mono playback bug identified and fixed (BOTH code paths)
- ✅ Fix verified working by user testing with real mono audio files
- ✅ Professional-grade implementation verified
- ✅ Documentation updated with complete technical details

---

## 🔥 PHASE 2: CRITICAL MUSICIAN FEATURES (In Progress)

### ✅ GAIN/VOLUME ADJUSTMENT **[COMPLETE]**
**Status**: ✅ **COMPLETED**
**Time Spent**: 6 hours (including real-time playback fix)
**Completion Date**: 2025-10-12

**What Was Implemented**:
- Basic gain adjustment (+/- 1dB increments) via Cmd+Up/Down
- Applies to entire file or selected region only
- Full undo/redo support (each adjustment is separate undo step)
- Real-time audio updates during playback
- Visual waveform updates immediately after adjustment

**Critical Technical Challenge: Real-Time Playback Updates**

**Problem Encountered**:
- Gain adjustments updated the waveform visually but audio didn't change during playback
- AudioTransportSource buffers audio internally for smooth playback
- Updating the buffer in MemoryAudioSource didn't affect playback - transport kept playing cached audio

**Initial Attempts**:
1. ❌ Tried preserving position in MemoryAudioSource::setBuffer() - didn't work
2. ❌ Tried capturing/restoring position before/after buffer update - didn't work
3. ❌ Tried position preservation flag in setBuffer() - still didn't work

**Root Cause Identified**:
AudioTransportSource maintains internal audio buffers for smooth playback. When the underlying buffer changes, the transport continues playing from its cached/buffered audio (old data), not the newly updated buffer.

**Solution That Worked** ✅:
Disconnect and reconnect the AudioTransportSource to flush internal buffers:

```cpp
bool AudioEngine::reloadBufferPreservingPlayback(...)
{
    // 1. Capture current playback state
    bool wasPlaying = isPlaying();
    double currentPosition = getCurrentPosition();

    // 2. CRITICAL: Disconnect transport to flush internal buffers
    m_transportSource.setSource(nullptr);

    // 3. Update buffer with new audio data
    m_bufferSource->setBuffer(buffer, sampleRate, false);

    // 4. Reconnect transport - forces reading fresh audio
    m_transportSource.setSource(m_bufferSource.get(), 0, nullptr,
                                sampleRate, numChannels);

    // 5. Restore playback position and restart if was playing
    if (wasPlaying)
    {
        m_transportSource.setPosition(currentPosition);
        m_transportSource.start();
    }
}
```

**Key Insight**: The disconnect/reconnect cycle flushes AudioTransportSource's internal buffers, forcing it to read fresh audio from the updated buffer. Without this, the transport continues playing stale cached audio.

**Implementation Details**:
- `AudioProcessor::applyGain()` - Core DSP function (dB to linear conversion)
- `AudioProcessor::applyGainToRange()` - Applies gain to specific sample range
- `GainUndoAction` - Undoable gain adjustment with before/after buffer snapshots
- `AudioEngine::reloadBufferPreservingPlayback()` - Critical method for real-time updates
- Keyboard shortcuts: Cmd+Up (increase), Cmd+Down (decrease)
- Status messages show applied gain amount

**Files Modified**:
1. `Source/Audio/AudioProcessor.h/.cpp` - DSP gain processing functions
2. `Source/Audio/AudioEngine.h/.cpp` - Buffer reload with playback preservation
3. `Source/Main.cpp` - Gain adjustment command handlers, undo actions
4. `Source/Audio/AudioEngine.cpp` - Disconnect/reconnect transport for real-time updates

**User Feedback**: ✅ **"That worked!"**

**All Success Criteria Met**:
- ✅ Gain adjustment implemented with ±1dB increments
- ✅ Works on entire file or selection only
- ✅ Full undo/redo support (100 levels)
- ✅ Real-time audio updates during playback (user can hear changes immediately)
- ✅ Waveform updates instantly after adjustment
- ✅ No playback interruption when adjusting gain
- ✅ Position preserved during buffer updates
- ✅ Professional workflow for sound designers and musicians

**Technical Achievement**: Solved complex real-time audio buffer update problem by understanding JUCE's AudioTransportSource internal buffering mechanism and implementing proper buffer flush via disconnect/reconnect cycle.

---

## 🎯 PHASE 1 PRIORITIES (All Complete - 2025-10-09)

### ✅ PRIORITY 1: Fix Waveform Redraw Speed to <10ms **[COMPLETE]**
**Status**: ✅ **COMPLETED**
**Time Spent**: 3 hours
**Completion Date**: 2025-10-08

**What Was Fixed**:
- Implemented fast direct rendering from cached AudioBuffer
- Removed duplicate `reloadFromBuffer()` calls
- Eliminated wasteful AudioThumbnail regeneration after edits
- Achieved <10ms waveform update speed (matching Sound Forge/Pro Tools)

**Result**: Waveform now updates **instantly** after all edit operations. User confirmed: "Yes! This is now working as expected."

**Technical Details**: See `PERFORMANCE_OPTIMIZATION_SUMMARY.md`

---

### ✅ PRIORITY 2: Implement Bidirectional Selection Extension Through Zero **[COMPLETE]**
**Status**: ✅ **COMPLETED**
**Time Spent**: 4 hours
**Completion Date**: 2025-10-08

**What Was Implemented**:
- Added anchor-based selection extension system
- Left edge (start) becomes anchor, right edge (end) is the active edge by default
- Active edge moves in the direction of Shift+Arrow key press
- Selection automatically flips when active edge crosses anchor
- Skips zero-width selection: jumps directly to one increment on opposite side
- Works with all navigation modes: arrows, page up/down, and all snap increments

**User Requirement**:
> "If you have 1.0 → 2.0 seconds selected and you continue to incrementally adjust by 100ms to the left until you get to 1.0 and then continue, the selection would become 0.9 → 1.0, 0.8 → 1.0, 0.7 → 1.0, etc."

**Implementation Details**:
- Added `m_isExtendingSelection` flag to track keyboard extension state
- Added `m_selectionAnchor` to store the fixed anchor point
- Modified `navigateLeft/Right()` to use anchor-based extension
- Modified `navigatePageLeft/Right()` for consistency
- Skip-zero threshold: `std::abs(newActive - anchor) < delta` (one full increment)
- Clears extension state on mouse selection or non-extending navigation

**Files Modified**:
1. `Source/UI/WaveformDisplay.h` - Added anchor members
2. `Source/UI/WaveformDisplay.cpp` - Implemented bidirectional logic in navigation methods

**Result**: User confirmed: "Yes! This feels great."

**All Success Criteria Met**:
- ✅ Selection extends in both directions through zero
- ✅ Left edge acts as anchor, right edge is active by default
- ✅ Behavior is symmetric for left/right extension
- ✅ Works with all snap modes and increments
- ✅ No visual glitches - selection always visible (skips zero)

---

### ✅ PRIORITY 3: Make Arrows Honor Snap Mode, Remove Alt+Arrow **[COMPLETE]**
**Status**: ✅ **COMPLETED**
**Time Spent**: 2.5 hours
**Completion Date**: 2025-10-08

**What Was Implemented**:
- Arrow keys now use current snap increment instead of fixed 10ms
- Alt+Arrow shortcuts completely removed
- Navigation unified with snap mode - G key controls both navigation and selection
- Added `getSnapIncrementInSeconds()` helper method for clean conversion

**User Requirement**:
> "I'd rather the arrows just honor the snap mode and get rid of alt+arrow"

**New Behavior** (Unified):
- Arrow keys: Navigate by **current snap increment**
- Shift+Arrow keys: Extend selection by **current snap increment**
- No Alt+Arrow shortcuts (removed)
- Snap mode (G key) affects BOTH navigation and selection
- Simple, predictable, unified system

**Example**:
```
1. G key cycles to "Snap: 100ms"
2. Press Right Arrow → Cursor moves 100ms
3. Press Shift+Right Arrow → Selection extends 100ms
4. G key cycles to "Snap: 1s"
5. Press Right Arrow → Cursor moves 1s
6. Press Shift+Right Arrow → Selection extends 1s
```

**Implementation Details**:

**New Helper Method** (`WaveformDisplay.h/.cpp`):
```cpp
double WaveformDisplay::getSnapIncrementInSeconds() const
{
    switch (m_snapMode)
    {
        case SnapMode::Off:
            return 0.01; // 10ms default when snap off
        case SnapMode::Samples:
            return AudioUnits::samplesToSeconds(m_snapIncrement, m_sampleRate);
        case SnapMode::Milliseconds:
            return m_snapIncrement / 1000.0;
        case SnapMode::Seconds:
            return m_snapIncrement / 10.0; // tenths of seconds
        case SnapMode::Frames:
            return AudioUnits::samplesToSeconds(
                AudioUnits::framesToSamples(m_snapIncrement, frameRate, m_sampleRate),
                m_sampleRate);
        case SnapMode::Grid:
            return m_snapIncrement / 1000.0; // ms for grid
        case SnapMode::ZeroCrossing:
            return 0.01; // 10ms default
        default:
            return 0.01;
    }
}
```

**Updated Navigation Methods**:
- `navigateLeft(bool extend)` - Removed `large` parameter, uses snap increment
- `navigateRight(bool extend)` - Removed `large` parameter, uses snap increment

**Files Modified**:
1. `Source/UI/WaveformDisplay.h` - Added `getSnapIncrementInSeconds()` declaration, updated method signatures
2. `Source/UI/WaveformDisplay.cpp` - Implemented helper method, updated navigation methods
3. `Source/Commands/CommandIDs.h` - Removed `navigateLeftLarge` and `navigateRightLarge` command IDs
4. `Source/Main.cpp` - Removed Alt+Arrow shortcut registrations and command handlers

**All Success Criteria Met**:
- ✅ Arrow keys use current snap increment (not hardcoded 10ms)
- ✅ Shift+Arrow keys extend by current snap increment (bidirectional)
- ✅ Alt+Arrow shortcuts removed completely
- ✅ Snap mode indicator shows increment that arrows will use
- ✅ Behavior is intuitive and predictable
- ✅ Build successful with no errors

---

### ✅ PRIORITY 4: Redesign Snap Mode System (Two-Tier Architecture) **[COMPLETE]**
**Status**: ✅ **COMPLETED**
**Time Spent**: 4 hours
**Completion Date**: 2025-10-09

**User Requirement**:
> "For snap mode, it should be two-tier: Unit selection (menu) and increment values (G key cycles)"

**Current System** (One-Tier, Confusing):
- G key cycles through: Off → Samples → Milliseconds → Seconds → Frames → Zero → Off
- User can't easily jump to desired unit
- Can't change increment without cycling through all modes
- No visual feedback for available increments

**New System** (Two-Tier, Professional):

**Tier 1: Unit Selection** (New UI Element)
- Dropdown menu next to snap mode indicator
- Quick selection of unit type: Samples | Milliseconds | Seconds | Frames
- Persists across restarts
- **Question for user**: Which keyboard shortcut? (Research Sound Forge default)

**Tier 2: Increment Values** (G key cycling)
- G key cycles through **increments within selected unit**
- Different increments per unit type:
  - **Samples**: None, 1, 100, 500, 1000, 10000
  - **Milliseconds**: None, 1, 10, 100, 500, 1000
  - **Seconds**: None, 0.1, 1, 5, 10
  - **Frames**: None, 1, 5, 10
- "None" = snap off for that unit (free movement)

**Zero Crossing** (Separate):
- Z key still toggles zero-crossing snap (independent feature)
- Can combine with unit snapping (e.g., "snap to 100ms, then fine-tune to zero crossing")

**Example Workflow**:
```
1. Click dropdown → Select "Milliseconds"
2. Press G key → Cycles: None → 1ms → 10ms → 100ms → 500ms → 1000ms → None
3. Status bar shows: [Snap: 100ms ▼] [Unit: Milliseconds ▼]
4. Press Z key → Status bar adds: [Snap: 100ms ▼] [Unit: Milliseconds ▼] [Zero X]
```

**UI Design**:
```
Status Bar (right side):
┌─────────────────────────────────────────┐
│ [Unit: Milliseconds ▼] [Snap: 100ms ▼] │
└─────────────────────────────────────────┘

Click [Unit: Milliseconds ▼] →
┌─────────────┐
│ Samples     │
│●Milliseconds│ ← Selected
│ Seconds     │
│ Frames      │
└─────────────┘

Click [Snap: 100ms ▼] →
┌──────────┐
│ None     │
│ 1 ms     │
│ 10 ms    │
│●100 ms   │ ← Selected
│ 500 ms   │
│ 1000 ms  │
└──────────┘
```

**Implementation Plan**:
1. Add unit selector dropdown component (ComboBox)
2. Modify G key to cycle increments for current unit only
3. Update snap mode indicator to show both unit and increment
4. Store preferences in NavigationPreferences
5. Update `snapTimeToUnit()` to use two-tier system

**Files to Modify**:
1. `Source/UI/WaveformDisplay.h/.cpp` - Add unit selection state
2. `Source/Utils/NavigationPreferences.h` - Store unit type separately
3. `Source/Main.cpp` - Update status bar UI, add unit dropdown
4. `Source/Utils/AudioUnits.h` - Add increment presets per unit type

**What Was Implemented**:
- Implemented two-tier snap architecture (unit selection + increment cycling)
- G key now cycles increments within current unit only
- Z key toggles zero-crossing independently
- Status bar shows unit type and current increment with color coding
- Thread-safe snap mode access with CriticalSection
- Added increment preset arrays for each unit type

**Technical Details**: See PRIORITY 4 implementation summary above.

**All Success Criteria Met**:
- ✅ G key cycles increments within selected unit
- ✅ Z key toggles zero-crossing independently
- ✅ Status bar shows both unit type and current increment
- ✅ Thread-safe implementation with proper locking
- ✅ Intuitive, professional workflow

---

### ✅ PRIORITY 5: MVP Playback and UI Polish (7 Critical Fixes) **[COMPLETE - REVISED]**
**Status**: ✅ **COMPLETED** (with follow-up fixes)
**Time Spent**: 3.5 hours
**Completion Date**: 2025-10-09 (revised fixes)

**User Requirements**:
> "A couple of notes: 1) when the app started it was defaulting to 100ms on the grid snap. Let's default the snap to be off when first loading. 2) The selection information at the top of the UI is all based in samples. Let's make sure that updates based off of what measument unit mode the user is in. 3) Likewise there is a little text that is 0/0 samples. I think we can remove this entirely since we have everything captured in the Selection UI component. 4) There's an oblong red circle in the UI at the top right that turns green when the audio is playing back. This doesn't serve a practical purpose so let's remove this UI element. 5) In terms of MVP we also need to make sure that we are playing the audio at the cursor position by default. [...] Similarly, if there is a selection, it should play where the selection starts and if looping is enabled, it should constantly loop until the user presses pause/stop."

**What Was Fixed**:

**5a. Default snap mode to OFF** ✅
- Changed `m_snapIncrementIndex` initialization from 3 (100ms) to 0 (OFF)
- File: `Source/UI/WaveformDisplay.cpp` line 53

**5b. Selection info uses current snap unit** ✅
- Implemented context-aware formatting based on `getSnapUnit()`
- Shows samples, milliseconds, seconds, or frames depending on mode
- File: `Source/Main.cpp` lines 61-119 (selection), 120-181 (edit cursor)
- **Fix Applied** (2025-10-09): Edit cursor display now also respects snap unit
- Default unit is Milliseconds, so both selection and cursor default to ms format

**5c. Remove redundant "0/0 samples" text** ✅
- Removed m_sampleLabel component entirely from TransportControls
- Files: `Source/UI/TransportControls.h/.cpp`

**5d. Remove red/green playback indicator circle** ✅
- Removed circle drawing code from paint() method
- Removed space reservation in resized() method
- File: `Source/UI/TransportControls.cpp` lines 177-196 (removed)

**5e. Play from cursor position by default (CRITICAL)** ✅
- Modified `togglePlayback()` to check edit cursor first, then playback position
- Uses `getEditCursorPosition()` if edit cursor is set (user clicked)
- Falls back to `getPlaybackPosition()` if no edit cursor
- File: `Source/Main.cpp` lines 1612-1627
- **Fix Applied** (2025-10-09): Changed from playback position to edit cursor position

**5f. Play from selection start if selection exists** ✅
- Already implemented, verified correct behavior
- File: `Source/Main.cpp` lines 1603-1609

**5g. Fix loop mode (currently not working)** ✅
- Added `setLooping()` and `isLooping()` methods to AudioEngine
- Wired up loop button to call `AudioEngine::setLooping()`
- Implemented loop restart logic in `TransportControls::timerCallback()`
- **Added selection-bounded playback** (2025-10-09):
  - Stop at selection end if loop disabled
  - Loop between selection start/end if loop enabled
  - Fall back to file-based loop if no selection
- Files:
  - `Source/Audio/AudioEngine.h/.cpp` (loop control methods)
  - `Source/Main.cpp` lines 1664-1670 (wire-up)
  - `Source/UI/TransportControls.h/.cpp` (selection-bounded playback logic)

**All Success Criteria Met**:
- ✅ Snap defaults to OFF on startup
- ✅ Selection info formats based on current snap unit (including edit cursor)
- ✅ Redundant sample label removed
- ✅ Circle indicator removed
- ✅ Plays from edit cursor position by default (fixed 2025-10-09)
- ✅ Plays from selection start when selection exists
- ✅ Loop mode restarts playback when reaching end
- ✅ Selection-bounded playback: stops at selection end (loop off) or loops within selection (loop on)
- ✅ Default unit is Milliseconds, all displays respect this default

---

## ✅ COMPLETED FEATURES (2025-10-09)

### Blocker Fixes (All Resolved):
1. ✅ **Shift+arrows extend selection** (not navigate)
2. ✅ **Alt+arrows for large navigation** (100ms jumps)
3. ✅ **Shift+PageUp/Down for page extension** (1s increments)
4. ✅ **Waveform redraws immediately** (<10ms after edits)
5. ✅ **Undo/redo consistency** (each edit is separate step)
6. ✅ **Snap mode visual indicator** (status bar with color coding)

### Core Features (Phase 1):
- ✅ Audio engine with buffer playback
- ✅ File I/O (open, save, save as)
- ✅ Waveform display with fast rendering
- ✅ Edit operations (cut, copy, paste, delete)
- ✅ Undo/redo (100 levels)
- ✅ Playback controls
- ✅ Selection with snap modes
- ✅ Recent files tracking
- ✅ Settings persistence

---

## 🚧 PHASE 2: REMAINING FEATURES

### 🔥 CRITICAL for Musicians/Sound Designers (Must-Have for Real Use):
- ✅ **Gain/Volume adjustment** ⭐ **COMPLETE** (2025-10-12)
  - ✅ Simple gain control (±1dB adjustment via Cmd+Up/Down)
  - ✅ Real-time audio updates during playback
  - ✅ Works on entire file or selection
  - ✅ Full undo/redo support
  - **Actual time**: 6 hours (including real-time playback fix)
- ⏭️ **Level meters** ⭐ **NEXT PRIORITY** - Peak + RMS meters during playback
  - Visual feedback for audio levels
  - Clipping detection (red indicator for >±1.0)
  - **Estimated time**: 4-6 hours
- ⏭️ **Normalization** ⭐ **HIGH PRIORITY** - Maximize audio levels
  - Basic mastering requirement
  - Match loudness between clips
  - Infrastructure ready: `AudioProcessor::normalize()` method exists
  - **Estimated time**: 2-3 hours (UI integration only)
- ⏭️ **Fade in/out** ⭐ **HIGH PRIORITY** - Smooth transitions
  - Prevents clicks/pops at edit boundaries
  - Industry standard for audio editing
  - Infrastructure ready: `AudioProcessor::fadeIn/fadeOut()` methods exist
  - **Estimated time**: 2-3 hours (UI integration only)

**Progress**: 1/4 complete ✅ (25% done)
**Remaining time for musician-essential features**: **8-12 hours**

### High Priority UI/UX Enhancements:
- ⏭️ **Preferences page** (navigation, snap, zoom settings)
- ⏭️ **Keyboard shortcut customization UI** (currently hardcoded)
- ⏭️ **Keyboard shortcut cheat sheet** (? key or Help menu)
- ⏭️ **Auto-save functionality** (prevent data loss)
- ⏭️ **Zoom level indicator** (status bar)
- ⏭️ **Zoom behavior configuration** (center on cursor vs. center on view)

### Professional Features:
- ⏭️ **DC offset removal** (audio quality improvement)
- ⏭️ **Additional edit operations** (trim, silence, reverse, invert)
- ⏭️ **Numeric position entry** (jump to exact time/sample)
- ⏭️ **Playback enhancements** (scrubbing, auto-scroll during playback)
- ⏭️ **Enhanced status bar** (file info, CPU/memory usage)

---

## 📊 Phase Completion Status

### Phase 1 (Core MVP): **100% Complete** ✅🎉
- **Completed**:
  - ✅ Audio engine, file I/O, editing, undo/redo
  - ✅ Waveform display with performance optimization (<10ms updates)
  - ✅ All critical blockers fixed
  - ✅ Playback and transport controls
  - ✅ Sample-accurate selection with snap modes
  - ✅ Bidirectional selection extension (PRIORITY 2)
  - ✅ Unified navigation with snap mode (PRIORITY 3)
  - ✅ Two-tier snap mode system (PRIORITY 4)
  - ✅ MVP playback and UI polish (PRIORITY 5)

**Status**: Professional MVP ready for validation testing and Phase 2 features

### Phase 2 (Professional Features): **25% Complete** ✅
- **Completed So Far**: 6 hours
  - ✅ **Gain/Volume adjustment** (6 hours) - COMPLETE
- **Remaining Time**: 34-39 hours
  - **Critical musician features**: 8-12 hours (level meters, normalize, fade)
  - **UI/UX enhancements**: 15-20 hours
  - **Additional professional features**: 14-15 hours
- **Current Priority**: Level meters (next up), then normalization and fade in/out

### Phase 3 (Polish & Advanced): **0% Complete**
- Estimated Time: 20-25 hours
- Customization, grid display, performance monitoring

### Phase 4 (Extended Features): **0% Complete**
- Estimated Time: 40+ hours
- Multi-format, recording, spectrum analyzer, batch processing

---

## 🎯 Next Steps

### Immediate (This Week):
1. ✅ ~~**Implement gain/volume adjustment**~~ - COMPLETE (2025-10-12)
2. **Implement level meters** ⭐ **START HERE** (4-6 hours)
   - Peak level meters during playback
   - RMS level indication
   - Clipping detection (red indicator for >±1.0)
   - Visual feedback component in UI
3. **Implement normalization** (2-3 hours)
   - Use existing `AudioProcessor::normalize()` method
   - Add keyboard shortcut and menu item
   - Create undo action
4. **Implement fade in/out** (2-3 hours)
   - Use existing `AudioProcessor::fadeIn/fadeOut()` methods
   - Add keyboard shortcuts
   - Create undo actions

### Short Term (Next 1-2 Weeks):
1. **Complete remaining critical musician features** (8-12 hours):
   - ✅ Gain/Volume adjustment - COMPLETE
   - Level meters (next up)
   - Normalization
   - Fade in/out
2. **Then implement UI/UX enhancements**:
   - Preferences page
   - Keyboard shortcut customization UI
   - Auto-save functionality

### Long Term (Month 2+):
- Phase 3 polish features
- Phase 4 extended features
- Public beta release

---

## 📝 Technical Notes

### Performance Optimization Architecture (Completed):
**Fast Direct Rendering System**:
- Caches AudioBuffer for instant rendering
- Bypasses slow AudioThumbnail regeneration
- Achieves <10ms waveform updates
- See `PERFORMANCE_OPTIMIZATION_SUMMARY.md` for details

### User Feedback Integration:
All current priorities (2-4) are direct responses to user testing feedback. The navigation and snap mode systems are being redesigned to match professional audio editor workflows.

### Code Review Findings (2025-10-09):
**Code Quality Assessment**: ✅ **9.5/10 - Exceptional**
- Professional architecture with proper thread safety
- Clean JUCE integration following best practices
- No shortcuts or band-aids found
- All Phase 1 completion claims verified as accurate

**Musician/Sound Designer Assessment**: ⚠️ **6.5/10 - Needs Critical Features**
- Missing essential audio processing tools:
  - **Gain/Volume adjustment** (CRITICAL OMISSION)
  - Level meters (essential visual feedback)
  - Normalization (basic mastering requirement)
  - Fade in/out (prevents harsh edits)
- Current state: "Developer MVP" (code complete, features limited)
- Target state: "User MVP" (minimal but usable for real work)
- **Gap**: 11-16 hours of DSP/UI work

**Action Items Completed**:
1. ✅ Updated README.md with accurate MVP status disclaimer
2. ✅ Cleaned up outdated TODO comments in codebase
3. ✅ Fixed hardcoded frame rate in Main.cpp (now uses NavigationPreferences)
4. ✅ Updated TODO.md with Phase 2 priorities based on code review findings

**Recommendation**: Implement the 4 CRITICAL musician features (gain, meters, normalize, fade) before public release. Current Phase 1 is excellent for developers but missing essential tools for audio professionals.

---

**Last Updated**: 2025-10-12 (Gain adjustment complete with real-time playback fix)
**Next Update**: After implementing level meters (Phase 2 next priority)
