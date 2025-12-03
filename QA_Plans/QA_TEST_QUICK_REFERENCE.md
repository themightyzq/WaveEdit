# QA Quick Reference: ParametricEQ Preview & RMS Normalize

## Feature 1: ParametricEQ Preview Optimization

### Key Workflows to Test

1. **Real-Time Parameter Updates** (Critical)
   - Rapid slider adjustments during playback → No artifacts
   - All 3 bands independent → No crosstalk
   - Extreme settings (-20dB to +20dB) → Stable filter

2. **Loop Preview Mode** (Critical)
   - Selection boundaries honored
   - Loop restart seamless (no clicks)
   - Parameter changes work during loop

3. **Apply → Undo Chain** (P0 - Blocker)
   - Apply saves changes to buffer
   - Playback reflects changes
   - Undo perfectly reverses
   - Redo re-applies

4. **Multi-Document Preview** (Important)
   - Only one document previews at a time
   - Inactive documents muted
   - Selection offset correct when switching

### Critical Test Files

- `/TestFiles/moans.wav` - Primary test file
- Test with various selections (start, middle, end)
- Test with loop enabled/disabled

### Quick Checklist

- [ ] Slider rapid adjustment → no artifacts
- [ ] All 3 bands adjust independently
- [ ] Loop boundaries sample-accurate
- [ ] Apply → Undo → Redo works perfectly
- [ ] Only one document previewing at a time
- [ ] Preview stops when dialog closes

---

## Feature 2: RMS Normalization

### Key Workflows to Test

1. **Mode Switching** (Critical)
   - Peak ↔ RMS toggle in dropdown
   - All UI labels update (Target Peak vs Target RMS)
   - Required gain recalculates instantly
   - Both Peak and RMS values always visible

2. **RMS Calculation Accuracy** (Critical)
   - RMS always < Peak mathematically
   - Known signals calculated correctly (-3dB for sine)
   - Silent audio returns -INF, not error
   - Works on partial selections

3. **RMS Preview** (Important)
   - Preview applies calculated gain
   - No distortion/clipping
   - Loop mode works
   - Gain matches calculation (±0.1dB)

4. **Settings Persistence** (Important)
   - Mode selection saved (Peak or RMS)
   - Persists across dialog close/reopen
   - Persists across app restart
   - Target level resets to default

### Critical Test Files

- `/TestFiles/moans.wav` with RMS calculation
- Sine wave (RMS should be -3dB)
- Speech file (RMS typically -15 to -12dB below peak)

### Quick Checklist

- [ ] Mode switcher works (Peak ↔ RMS)
- [ ] Labels update on mode change
- [ ] RMS calculation mathematically correct
- [ ] RMS always < Peak
- [ ] Preview applies correct gain
- [ ] Mode persists across sessions
- [ ] Target level doesn't persist (resets)

---

## Critical P0 Tests (Blockers)

These MUST pass or feature is not releasable:

### ParametricEQ
1. **1.5.1** - Apply changes persist after dialog close
2. **1.5.2** - Undo reverses EQ correctly
3. **1.5.3** - Cancel preserves original audio

### RMS Normalize
1. **2.2.1** - RMS calculation accurate on known signal
2. **2.2.3** - RMS handles silence gracefully
3. **2.4.2** - Mode persists across app restart
4. **2.5.2** - Apply uses correct mode (RMS, not Peak)

---

## Common Issues to Watch For

### ParametricEQ
- **Filter Ringing:** High Q + large gains → resonance
  - Test Q > 5.0 with gain > 12dB
- **Coordinate System:** Preview offset must match selection start
  - Watch cursor position during preview
- **Muting:** When switching documents, other previews should stop
  - Verify only one document audible

### RMS Normalize
- **Calculation Error:** RMS >= Peak (mathematically wrong)
  - Always verify RMS < Peak in output
- **Silent Audio:** RMS on silence should be -INF, not NaN
  - Test with 10-second silence region
- **Persistence Bug:** Mode not saved across restart
  - Close app completely, relaunch
- **Wrong Mode Applied:** Applying normalization in wrong mode
  - Verify undo message matches selected mode

---

## Testing Best Practices

1. **Always use Preview First**
   - Listen before applying
   - Verify meter shows expected change
   - Check for artifacts or distortion

2. **Verify Undo Chain**
   - Operation should appear in Edit menu
   - Ctrl+Z should reverse perfectly
   - Ctrl+Shift+Z should redo

3. **Check Both Peak and RMS Display**
   - Both values should always be visible
   - Don't assume one is hidden based on mode

4. **Test Loop Boundaries**
   - Use selection with distinctive markers (e.g., 5.0s to 10.0s)
   - Verify loop restart at exact boundary
   - Listen for clicks at restart point

5. **Cross-File Testing**
   - Test with different audio types (speech, music, tones)
   - Test with different sample rates (44.1kHz, 96kHz)
   - Test with mono and stereo files

---

## Performance Targets

- ParametricEQ: Slider response < 50ms
- RMS Calculation: < 1 second for 1-hour file
- Preview Load: < 500ms for large files
- UI Frame Rate: 60fps minimum during slider adjustment

---

## Severity Ratings

- **P0 (Blocker):** Breaks core functionality (undo, apply, calculate)
- **P1 (Major):** Feature doesn't work as designed (no artifacts, accurate)
- **P2 (Minor):** UI cosmetic issues (label updates, colors)
- **P3 (Cosmetic):** Nice-to-have visual improvements

---

## Pass Criteria for Release

**Minimum Requirements:**
- All P0 tests PASS
- 95%+ of P1 tests PASS
- No regressions in existing features

**Ideal for Polished Release:**
- 100% P0/P1 PASS
- 90%+ P2 PASS (cosmetics)
- 100% regression tests PASS

---

## Test Execution Notes

- Test in both Debug and Release builds
- Test on macOS first, then Windows
- Use both internal audio device and external interface (if available)
- Document any failures with:
  - Exact reproduction steps
  - Expected vs actual behavior
  - Audio artifacts (if any)
  - Screenshots/recordings

---

**Quick Links:**
- Full test plan: `QA_TEST_PLAN_PARAMETRIC_EQ_RMS_NORMALIZE.md`
- ParametricEQ implementation: `/Source/UI/ParametricEQDialog.cpp`
- RMS Normalize implementation: `/Source/UI/NormalizeDialog.cpp`
- AudioEngine preview system: `/Source/Audio/AudioEngine.h`
