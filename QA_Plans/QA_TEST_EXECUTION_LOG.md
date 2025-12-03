# QA Test Execution Log: ParametricEQ Preview & RMS Normalize

**Test Session Date:** [DATE]
**QA Tester:** [NAME]
**WaveEdit Build:** [VERSION/COMMIT]
**Platform:** macOS / Windows
**Audio Device:** [Device Name]

---

## Test Results Summary

| Feature | Total Tests | Passed | Failed | Skipped | Pass Rate |
|---------|------------|--------|--------|---------|-----------|
| ParametricEQ Preview | 25 | — | — | — | — |
| RMS Normalization | 30 | — | — | — | — |
| Cross-Feature Integration | 5 | — | — | — | — |
| Performance & Stress | 4 | — | — | — | — |
| Regression Tests | 3 | — | — | — | — |
| **TOTAL** | **67** | **—** | **—** | **—** | **—%** |

---

## P0 (Blocker) Test Results

These tests are release-blocking. All must PASS.

### ParametricEQ P0 Tests

| Test ID | Name | Status | Notes |
|---------|------|--------|-------|
| 1.5.1 | Apply changes persist after dialog close | [ ] PASS / [ ] FAIL | |
| 1.5.2 | Undo reverses EQ correctly | [ ] PASS / [ ] FAIL | |
| 1.5.3 | Cancel preserves original audio | [ ] PASS / [ ] FAIL | |

**Result:** [ ] All P0 Pass ✓ / [ ] Some P0 Failed ✗

---

### RMS Normalize P0 Tests

| Test ID | Name | Status | Notes |
|---------|------|--------|-------|
| 2.2.1 | RMS calculation accurate on known signal | [ ] PASS / [ ] FAIL | |
| 2.2.3 | RMS handles silence gracefully | [ ] PASS / [ ] FAIL | |
| 2.4.2 | Mode persists across app restart | [ ] PASS / [ ] FAIL | |
| 2.5.2 | Apply uses correct mode (not wrong mode) | [ ] PASS / [ ] FAIL | |

**Result:** [ ] All P0 Pass ✓ / [ ] Some P0 Failed ✗

---

## P1 (Major) Test Results

### ParametricEQ P1 Tests (16 tests)

| Test ID | Name | Status | Artifacts? | Notes |
|---------|------|--------|-----------|-------|
| 1.1.1 | Gain slider rapid adjustments | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.1.2 | Frequency slider extreme range | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.1.3 | Q factor rapid adjustment | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.2.1 | Independent band control | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.2.2 | Multi-band simultaneous adjustment | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.3.1 | Maximum boost (+20dB all bands) | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.3.2 | Maximum cut (-20dB all bands) | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.3.3 | Extreme frequency values | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.4.1 | Loop toggle during preview | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.4.2 | Loop boundaries accuracy | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.6.1 | Preview mode mutes other documents | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.6.2 | Preview selection offset correct | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.7.1 | Dialog initialization with current settings | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.8.1 | Button state reflects preview playback | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.8.2 | Value label updates real-time | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |
| 1.8.3 | Dialog responsiveness during preview | [ ] PASS / [ ] FAIL | [ ] Yes / [ ] No | |

**ParametricEQ P1 Pass Rate:** ___ / 16 = ____%

---

### RMS Normalize P1 Tests (18 tests)

| Test ID | Name | Status | Notes |
|---------|------|--------|-------|
| 2.1.1 | Peak to RMS mode switch | [ ] PASS / [ ] FAIL | |
| 2.1.2 | RMS to Peak mode switch | [ ] PASS / [ ] FAIL | |
| 2.1.3 | Mode switch updates all values | [ ] PASS / [ ] FAIL | |
| 2.2.2 | RMS vs Peak on various audio types | [ ] PASS / [ ] FAIL | |
| 2.2.4 | RMS on partial selection | [ ] PASS / [ ] FAIL | |
| 2.3.1 | Preview RMS normalization | [ ] PASS / [ ] FAIL | |
| 2.3.2 | Preview gain calculation accuracy | [ ] PASS / [ ] FAIL | |
| 2.3.3 | RMS preview with loop mode | [ ] PASS / [ ] FAIL | |
| 2.4.1 | Mode persists across dialog instances | [ ] PASS / [ ] FAIL | |
| 2.5.1 | Switch between Peak and RMS during session | [ ] PASS / [ ] FAIL | |
| 2.6.1 | RMS to same level (no change) | [ ] PASS / [ ] FAIL | |
| 2.6.2 | RMS with extreme target (-60dB) | [ ] PASS / [ ] FAIL | |
| 2.6.3 | RMS with extreme target (+20dB) | [ ] PASS / [ ] FAIL | |
| 2.6.4 | RMS with very small selection (<100ms) | [ ] PASS / [ ] FAIL | |
| 2.7.1 | Both Peak and RMS values displayed | [ ] PASS / [ ] FAIL | |
| 2.7.2 | Target label changes with mode | [ ] PASS / [ ] FAIL | |
| 2.8.1 | Required gain color indicators | [ ] PASS / [ ] FAIL | |
| 2.8.2 | Negative gain has no warning | [ ] PASS / [ ] FAIL | |

**RMS Normalize P1 Pass Rate:** ___ / 18 = ____%

---

## P2 (Minor) & P3 (Cosmetic) Results

**Tested:**
- [ ] Button state cosmetics
- [ ] Label positioning and fonts
- [ ] Dialog sizing responsiveness
- [ ] Color consistency
- [ ] Setting persistence details

**Summary:** ___ / ___ = ____%

---

## Detailed Failure Log

For each failed test, document:

### Failure #1

**Test ID:** [e.g., 1.1.1]
**Test Name:** [Name]
**Status:** FAIL
**Severity:** [P0/P1/P2/P3]

**Reproduction Steps:**
1. [Step 1]
2. [Step 2]
3. [Step 3]

**Expected Result:**
[What should happen]

**Actual Result:**
[What actually happened]

**Audio Artifacts:** [ ] Yes [ ] No
**Artifact Type:** [Click/Pop/Distortion/Dropout/Other]

**Screenshots/Recording:**
[Attach if available]

**Root Cause Hypothesis:**
[Your analysis of what went wrong]

**Workaround (if any):**
[Temporary solution for user]

---

### Failure #2

**Test ID:** [e.g., 2.1.1]
**Test Name:** [Name]
**Status:** FAIL
**Severity:** [P0/P1/P2/P3]

**Reproduction Steps:**
1. [Step 1]
2. [Step 2]

**Expected Result:**
[What should happen]

**Actual Result:**
[What actually happened]

**Screenshots/Recording:**
[Attach if available]

**Root Cause Hypothesis:**
[Your analysis]

**Workaround:**
[If any]

---

## Artifact Analysis

For any audible issues, categorize:

| Artifact Type | Count | Severity | Examples |
|---------------|-------|----------|----------|
| Clicks | — | — | [describe] |
| Pops | — | — | [describe] |
| Distortion | — | — | [describe] |
| Dropouts | — | — | [describe] |
| Ringing | — | — | [describe] |
| Noise | — | — | [describe] |

---

## Performance Measurements

### ParametricEQ Performance

**File:** [moans.wav]
**Selection:** [0:00-0:30]

- **Slider Response Time:** ___ ms (target: <50ms)
- **CPU Usage (Peak):** ___ % (target: <30%)
- **CPU Usage (Average):** ___ %
- **UI Frame Rate:** ___ fps (target: 60fps)
- **Audio Dropouts:** [ ] Yes [ ] No (count: ___)

---

### RMS Normalization Performance

**File:** [1-hour audio file]

- **RMS Calculation Time:** ___ ms (target: <1000ms)
- **Preview Buffer Load Time:** ___ ms (target: <500ms)
- **Memory Used by Preview:** ___ MB (target: <50MB)
- **Memory Freed on Close:** [ ] Yes [ ] No

---

## Browser/Platform-Specific Issues

### macOS

**OS Version:** [e.g., 12.5]
**Audio Device:** [Device name]

Issues found:
- [ ] None
- [ ] [Issue 1]
- [ ] [Issue 2]

---

### Windows

**OS Version:** [e.g., Win 11]
**Audio Device:** [Device name]

Issues found:
- [ ] None
- [ ] [Issue 1]
- [ ] [Issue 2]

---

## Undo/Redo Chain Verification

Test that operations appear in Edit menu:

| Operation | Appears in Menu | Undo Works | Redo Works |
|-----------|-----------------|-----------|-----------|
| Apply ParametricEQ | [ ] Yes [ ] No | [ ] Yes [ ] No | [ ] Yes [ ] No |
| Apply RMS Normalize | [ ] Yes [ ] No | [ ] Yes [ ] No | [ ] Yes [ ] No |
| Combined EQ + Norm | [ ] Yes [ ] No | [ ] Yes [ ] No | [ ] Yes [ ] No |

**Notes:** [Any issues with undo history]

---

## Regression Test Results

Verify existing features still work:

| Feature | Status | Notes |
|---------|--------|-------|
| Fade In/Out | [ ] PASS / [ ] FAIL | |
| DC Offset Removal | [ ] PASS / [ ] FAIL | |
| Region List Panel | [ ] PASS / [ ] FAIL | |
| Multi-file Editing | [ ] PASS / [ ] FAIL | |
| Keyboard Shortcuts | [ ] PASS / [ ] FAIL | |

---

## Sign-Off

**QA Tester:** ________________
**Date:** ________________
**Test Session Duration:** _____ hours

**Overall Assessment:**
- [ ] Ready for Release (All P0 Pass, 95%+ P1 Pass)
- [ ] Minor Issues Only (No P0, <3 P1 failures)
- [ ] Significant Issues (Multiple P1 failures)
- [ ] Blocking Issues (P0 failures - DO NOT RELEASE)

**Recommendation:**
[Your assessment and any notes for the development team]

---

## Appendix: Test Environment Checklist

- [ ] WaveEdit built successfully (no compiler warnings)
- [ ] Test audio files available (/TestFiles/moans.wav)
- [ ] Audio device configured and tested
- [ ] Undo history working on other operations
- [ ] No other audio software playing (prevent interference)
- [ ] Latest JUCE version used
- [ ] CMake build configuration correct

---

**Document Version:** 1.0
**Created:** December 2, 2025
