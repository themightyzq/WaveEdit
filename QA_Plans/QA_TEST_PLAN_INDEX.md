# QA Test Plan Index: ParametricEQ Preview & RMS Normalization

**Document Set Version:** 1.0
**Created:** December 2, 2025
**Purpose:** Comprehensive QA testing for two new audio editor features

---

## Document Overview

This directory contains a complete QA test plan for two integrated features:
1. **ParametricEQ Preview Optimization** - Real-time EQ parameter updates during preview
2. **RMS Normalization Mode** - RMS calculation and normalization with mode persistence

Three coordinated documents guide QA testing at different levels of detail:

---

## 1. Main Test Plan Document
**File:** `QA_TEST_PLAN_PARAMETRIC_EQ_RMS_NORMALIZE.md` (37 KB)

### Contents
- **Executive Summary** - Feature overview and architecture notes
- **Part 1: ParametricEQ Testing** (25 test cases)
  - 1.1 Rapid slider movement (3 tests)
  - 1.2 All three bands (2 tests)
  - 1.3 Extreme settings (3 tests)
  - 1.4 Loop preview mode (2 tests)
  - 1.5 Apply/Undo workflow (3 tests) - **P0 CRITICAL**
  - 1.6 Multi-document interaction (2 tests)
  - 1.7 Parameter persistence (1 test)
  - 1.8 UI responsiveness (3 tests)

- **Part 2: RMS Normalization Testing** (30 test cases)
  - 2.1 Mode switching (3 tests) - **P1 CRITICAL**
  - 2.2 Calculation accuracy (4 tests) - **P0 CRITICAL**
  - 2.3 RMS preview (3 tests)
  - 2.4 Settings persistence (3 tests) - **P0 CRITICAL**
  - 2.5 Multi-mode workflow (2 tests) - **P0 CRITICAL**
  - 2.6 Edge cases (4 tests)
  - 2.7 UI display (3 tests)
  - 2.8 Gain warnings (2 tests)

- **Part 3: Cross-Feature Integration** (2 tests)
- **Part 4: Performance & Stress** (4 tests)
- **Part 5: Regression Testing** (3 tests)

### Key Features
- Detailed reproduction steps for each test
- Clear expected vs actual results
- Specific failure criteria for P0/P1/P2/P3 severity
- Architecture notes explaining design decisions
- Known limitations and edge cases documented

### Recommended Use
**When:** During comprehensive QA execution
**Who:** QA testers, test engineers
**Duration:** 7-8 hours per platform (macOS + Windows)

---

## 2. Quick Reference Guide
**File:** `QA_TEST_QUICK_REFERENCE.md` (5.8 KB)

### Contents
- **Feature 1 Key Workflows** - ParametricEQ essentials
- **Feature 2 Key Workflows** - RMS normalization essentials
- **P0 Test Checklist** - 7 blocking tests (non-negotiable)
- **Common Issues to Watch For** - Known failure modes
- **Testing Best Practices** - How to test effectively
- **Performance Targets** - Expected latency/responsiveness
- **Pass Criteria for Release** - Decision framework

### Key Features
- One-page per feature summary
- Critical workflows highlighted
- Common problems pre-identified
- Quick decision checklist for pass/fail
- References to full test plan

### Recommended Use
**When:** Quick reference during testing, planning test sessions
**Who:** QA leads, test planning
**Duration:** 10 minutes to review

---

## 3. Test Execution Log Template
**File:** `QA_TEST_EXECUTION_LOG.md` (8.6 KB)

### Contents
- **Test Results Summary Table** - Pass rate tracking
- **P0 Test Results** (11 blocking tests) - with checkbox grid
- **P1 Test Results** (34 major tests) - with checkbox grid
- **P2/P3 Results** - cosmetics and minor issues
- **Detailed Failure Log Template** - per-issue documentation
- **Artifact Analysis** - categorization of audio issues
- **Performance Measurements** - metrics collection
- **Platform-Specific Issues** - macOS/Windows notes
- **Undo/Redo Verification** - chain integrity tracking
- **Regression Test Results** - existing feature compatibility
- **QA Sign-Off Section** - final approval/recommendation

### Key Features
- Fill-in checkbox format for easy completion
- Structured failure documentation
- Performance measurement templates
- Multi-platform tracking
- Sign-off and recommendation fields

### Recommended Use
**When:** Recording test execution results
**Who:** QA testers executing tests
**Duration:** Filled out during 7-8 hour test session

---

## 4. Summary & Reference
**File:** `QA_TEST_PLAN_SUMMARY.txt` (11 KB)

### Contents
- **Documents Created** - overview of test deliverables
- **Test Coverage Breakdown** - 25 + 30 tests organized by category
- **Severity Distribution** - P0/P1/P2/P3 breakdown
- **Critical P0 Checklist** - 7 tests that block release
- **Test Execution Workflow** - 6-phase testing plan
- **Key Testing Principles** - 6 critical practices
- **Common Failure Modes** - what to watch for
- **Release Readiness Criteria** - approval standards
- **QA Sign-Off** - completion checklist
- **References** - links to implementation

### Key Features
- ASCII formatted for quick scanning
- Structured section organization
- Clear pass/fail criteria
- Time estimates per testing phase
- Implementation file references

### Recommended Use
**When:** Planning test execution, establishing criteria
**Who:** QA leads, test planning, development managers
**Duration:** 20 minutes to understand scope

---

## Test Plan Structure

```
QA_TEST_PLAN (This Directory)
├── QA_TEST_PLAN_PARAMETRIC_EQ_RMS_NORMALIZE.md
│   ├── Part 1: ParametricEQ Testing (25 tests)
│   ├── Part 2: RMS Normalization (30 tests)
│   ├── Part 3: Integration (2 tests)
│   ├── Part 4: Performance (4 tests)
│   └── Part 5: Regression (3 tests)
│
├── QA_TEST_QUICK_REFERENCE.md
│   ├── Feature 1 Workflows
│   ├── Feature 2 Workflows
│   ├── P0 Critical Checklist (7 tests)
│   └── Testing Best Practices
│
├── QA_TEST_EXECUTION_LOG.md
│   ├── Results Tracking Grid
│   ├── Failure Documentation
│   ├── Performance Measurements
│   └── QA Sign-Off
│
├── QA_TEST_PLAN_SUMMARY.txt
│   ├── Coverage Breakdown
│   ├── Release Criteria
│   └── Execution Timeline
│
└── QA_TEST_PLAN_INDEX.md (this file)
    └── Navigation guide
```

---

## Quick Navigation by Topic

### Finding Tests By Feature

**ParametricEQ Preview Optimization:**
- Main plan: Part 1, sections 1.1-1.8 (25 tests)
- Quick ref: "Feature 1: ParametricEQ Preview Testing"
- P0 tests: 1.5.1, 1.5.2, 1.5.3

**RMS Normalization:**
- Main plan: Part 2, sections 2.1-2.8 (30 tests)
- Quick ref: "Feature 2: RMS Normalization Testing"
- P0 tests: 2.2.1, 2.2.3, 2.4.2, 2.5.2

**Integration & Regression:**
- Main plan: Parts 3-5 (9 tests)
- Quick ref: Not detailed (see main plan)

---

### Finding Tests By Severity

**P0 (Blocker - Release Critical):**
- ParametricEQ: 1.5.1, 1.5.2, 1.5.3 (apply, undo, cancel)
- RMS Normalize: 2.2.1, 2.2.3, 2.4.2, 2.5.2 (calculation, persistence)
- Quick checklist: "Critical P0 Test Checklist" (2 tests per feature)

**P1 (Major - Functionality Required):**
- ParametricEQ: 16 tests covering artifacts, responsiveness, loops
- RMS Normalize: 18 tests covering mode switching, accuracy, preview

**P2/P3 (Minor/Cosmetic):**
- UI cosmetics, label updates, button states
- Less critical but important for polish

---

### Finding Tests By Topic

**Audio Artifacts & Quality:**
- 1.1.1, 1.1.2, 1.1.3 - ParametricEQ rapid adjustments
- 1.3.1, 1.3.2, 1.3.3 - Extreme settings stability
- 2.3.1, 2.3.2 - RMS preview accuracy

**Loop Functionality:**
- 1.4.1, 1.4.2 - ParametricEQ loop preview
- 2.3.3 - RMS loop preview

**Settings Persistence:**
- 1.7.1 - ParametricEQ parameter persistence
- 2.4.1, 2.4.2 - RMS mode persistence

**Multi-Document Interaction:**
- 1.6.1, 1.6.2 - Preview muting and offset
- 3.2.1 - Cross-document preview state

**Performance:**
- 4.1.1, 4.1.2 - Large file handling
- 4.2.1 - Memory usage

---

## Test Execution Workflow

### Phase 1: Setup (30 minutes)
- Build WaveEdit (Debug + Release)
- Prepare test audio files
- Configure audio device
- Verify baseline functionality

### Phase 2: ParametricEQ (2-3 hours)
1. P0 tests: 1.5.1, 1.5.2, 1.5.3 (30 min)
2. P1 critical workflows: Sliders, bands, loops (1 hour)
3. Edge cases and extremes (45 min)
4. Multi-document testing (30 min)

### Phase 3: RMS Normalization (2-3 hours)
1. Mode switching (30 min)
2. Calculation accuracy and P0 tests (45 min)
3. Preview and persistence (45 min)
4. Edge cases (30 min)

### Phase 4: Integration & Regression (1 hour)
- Cross-feature workflows
- Existing feature compatibility
- Multi-file state

### Phase 5: Performance (30 min)
- Large file responsiveness
- Memory profiling
- CPU measurement

### Phase 6: Final Verification (30 min)
- Re-test any failed P0 tests
- Confirm no regressions
- Complete sign-off

**Total: 7-8 hours per platform**

---

## Critical Success Factors

### Must Pass (P0 - Non-Negotiable)
```
ParametricEQ:
  □ 1.5.1 Apply changes persist
  □ 1.5.2 Undo reverses perfectly
  □ 1.5.3 Cancel preserves original

RMS Normalization:
  □ 2.2.1 Calculation accurate
  □ 2.2.3 Silence handled gracefully
  □ 2.4.2 Mode persists across restart
  □ 2.5.2 Apply uses correct mode
```

### Target Pass Rates
- P0: 100% (7/7 tests)
- P1: 95%+ (32/34 tests)
- P2/P3: 90%+ (20/22 tests)
- Overall: 95%+ (59/67 tests)

### Release Decision Framework
- **APPROVE** if all P0 pass + P1 >= 95%
- **HOLD** if any P0 fails or P1 < 95%
- **DELAY** if audio quality issues or regressions found

---

## Using This Test Plan

### For Test Planning
1. Read `QA_TEST_PLAN_SUMMARY.txt` (20 min)
2. Review "Test Execution Workflow" section
3. Establish time and resource requirements
4. Create test schedule using Phase breakdown

### For Test Execution
1. Start with `QA_TEST_QUICK_REFERENCE.md`
2. Follow Phase workflow from summary
3. Use `QA_TEST_PLAN_PARAMETRIC_EQ_RMS_NORMALIZE.md` for detailed steps
4. Record results in `QA_TEST_EXECUTION_LOG.md`

### For Test Management
1. Reference `QA_TEST_PLAN_SUMMARY.txt` for metrics
2. Track P0/P1 pass rates in execution log
3. Use release criteria section for approval
4. Document all failures with full details

### For Regression Testing
1. Run Part 5 tests (regression suite)
2. Verify existing features unaffected
3. Check undo/redo chain intact
4. Confirm no audio quality regressions

---

## Key Metrics

| Metric | Value | Target |
|--------|-------|--------|
| Total Tests | 67+ | — |
| P0 Blocking | 7 | 100% pass |
| P1 Major | 34 | 95%+ pass |
| P2 Minor | 16 | 90%+ pass |
| Estimated Duration | 7-8 hrs | per platform |
| Platforms | 2 | macOS, Windows |
| Critical Workflows | 12 | — |

---

## References

### Implementation Files
- `/Source/UI/ParametricEQDialog.cpp` - EQ dialog implementation
- `/Source/UI/ParametricEQDialog.h` - EQ dialog header
- `/Source/UI/NormalizeDialog.cpp` - Normalize dialog implementation
- `/Source/UI/NormalizeDialog.h` - Normalize dialog header
- `/Source/Audio/AudioEngine.h` - Preview system architecture
- `/Source/DSP/ParametricEQ.cpp` - EQ DSP processor
- `/Source/DSP/ParametricEQ.h` - EQ processor interface
- `/Source/Audio/AudioProcessor.h` - RMS/Peak calculations

### Test Files
- `/TestFiles/moans.wav` - Primary speech test file
- Generated test signals (sine, noise) via sox

### Related Documents
- `CLAUDE.md` - Project AI instructions
- `README.md` - User guide and build instructions
- `TODO.md` - Roadmap and current status

---

## Document Maintenance

**Version:** 1.0
**Created:** December 2, 2025
**Last Updated:** December 2, 2025
**Status:** Ready for QA Execution

### Change Log
- v1.0: Initial comprehensive test plan creation
  - 67+ test cases across 5 test categories
  - 3 supporting documents (quick reference, execution log, summary)
  - Complete P0/P1/P2/P3 severity classification
  - Detailed reproduction steps and failure criteria

### Known Limitations
- Test plan assumes stable build available
- Performance measurements require clean system
- Audio artifacts may vary by platform/device
- Some tests require specific test file characteristics

---

## Questions?

Refer to the appropriate document:
- **"How do I run all tests?"** → QA_TEST_PLAN_SUMMARY.txt
- **"What are P0 tests?"** → QA_TEST_QUICK_REFERENCE.md
- **"How do I document failures?"** → QA_TEST_EXECUTION_LOG.md
- **"What does test 1.5.1 do?"** → QA_TEST_PLAN_PARAMETRIC_EQ_RMS_NORMALIZE.md

---

**This index provides navigation to all QA test materials. Start here, then dive into specific documents based on your role and phase of testing.**
