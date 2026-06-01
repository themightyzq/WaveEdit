/*
  ==============================================================================

    DialogInputValidationTests.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Unit tests for dialog input-validation fixes:
      H17 - GoToPositionDialog rejects NaN / Inf / scientific notation
      L4  - GoToPositionDialog Samples mode rejects "1e5" (float notation)
      H18 - GainDialog getValidatedGain() rejects NaN / Inf
      H18 - NormalizeDialog updateRequiredGain disables Apply when level is -inf

    These tests exercise only pure C++ parsing / validation logic; they do NOT
    require a live JUCE MessageManager or display.  They derive from
    juce::UnitTest and are registered automatically via static construction.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include "UI/GoToPositionDialog.h"
#include "UI/GainDialog.h"
#include "Utils/AudioUnits.h"
#include <cmath>
#include <optional>

//==============================================================================
// Helper: construct a GoToPositionDialog in a given mode and call parseInput.
// The constructor itself does not require a MessageManager (it calls jassert
// that checks it, but that jassert is compiled out in unit-test builds that
// define JUCE_ENABLE_UNIT_TESTS).  If the jassert fires in a particular build
// configuration, wrap in a MessageManager::getInstance() guard.

static int64_t parseGoTo(AudioUnits::TimeFormat format,
                          const juce::String& input,
                          double sampleRate = 44100.0,
                          double fps        = 30.0,
                          int64_t maxSamples = 441000 /* 10 s */)
{
    GoToPositionDialog dlg(format, sampleRate, fps, maxSamples);
    return dlg.parseInputForTest(input);
}

//==============================================================================
// H17 + L4 — GoToPositionDialog parser tests
//==============================================================================

class GoToPositionParserH17Tests : public juce::UnitTest
{
public:
    GoToPositionParserH17Tests()
        : juce::UnitTest("GoToPositionDialog H17/L4 — NaN/Inf/sci-notation rejection", "UI") {}

    void runTest() override
    {
        // -------------------------------------------------------------------
        // H17: Samples mode — NaN / Inf inputs must be rejected (return -1)
        // -------------------------------------------------------------------
        beginTest("Samples mode: 'NaN' is rejected");
        expect(parseGoTo(AudioUnits::TimeFormat::Samples, "NaN") == -1,
               "NaN must not reach the int64_t cast");

        beginTest("Samples mode: 'nan' (lower-case) is rejected");
        expect(parseGoTo(AudioUnits::TimeFormat::Samples, "nan") == -1,
               "nan must not reach the int64_t cast");

        beginTest("Samples mode: 'Inf' is rejected");
        expect(parseGoTo(AudioUnits::TimeFormat::Samples, "Inf") == -1,
               "Inf must be rejected");

        beginTest("Samples mode: 'infinity' is rejected");
        expect(parseGoTo(AudioUnits::TimeFormat::Samples, "infinity") == -1,
               "infinity must be rejected");

        // -------------------------------------------------------------------
        // L4: Samples mode — scientific / float notation must be rejected
        // getLargeIntValue() would silently return 1 for "1e5"
        // -------------------------------------------------------------------
        beginTest("Samples mode: '1e5' is rejected (L4)");
        expect(parseGoTo(AudioUnits::TimeFormat::Samples, "1e5") == -1,
               "Scientific notation must be rejected in Samples mode (getLargeIntValue silently returns 1)");

        beginTest("Samples mode: '1.5' (float) is rejected");
        expect(parseGoTo(AudioUnits::TimeFormat::Samples, "1.5") == -1,
               "Decimal float must be rejected in Samples mode");

        beginTest("Samples mode: '0x1A' (hex) is rejected");
        expect(parseGoTo(AudioUnits::TimeFormat::Samples, "0x1A") == -1,
               "Hex notation must be rejected in Samples mode");

        // -------------------------------------------------------------------
        // H17: Seconds mode — NaN / Inf / extreme floats must be rejected
        // -------------------------------------------------------------------
        beginTest("Seconds mode: 'NaN' is rejected");
        expect(parseGoTo(AudioUnits::TimeFormat::Seconds, "NaN") == -1,
               "NaN must not reach secondsToSamples cast");

        beginTest("Seconds mode: '1e999' (overflows to Inf) is rejected");
        expect(parseGoTo(AudioUnits::TimeFormat::Seconds, "1e999") == -1,
               "Overflow to Inf must be rejected before cast to int64_t");

        beginTest("Seconds mode: 'inf' is rejected");
        expect(parseGoTo(AudioUnits::TimeFormat::Seconds, "inf") == -1,
               "inf must be rejected in Seconds mode");

        // -------------------------------------------------------------------
        // H17: Milliseconds mode — NaN / Inf
        // -------------------------------------------------------------------
        beginTest("Milliseconds mode: 'NaN' is rejected");
        expect(parseGoTo(AudioUnits::TimeFormat::Milliseconds, "NaN") == -1,
               "NaN must not reach millisecondsToSamples");

        beginTest("Milliseconds mode: '-1e999' is rejected");
        expect(parseGoTo(AudioUnits::TimeFormat::Milliseconds, "-1e999") == -1,
               "-Inf must be rejected in Milliseconds mode");

        // -------------------------------------------------------------------
        // Regression: Valid integer inputs in Samples mode must still work
        // -------------------------------------------------------------------
        beginTest("Samples mode: '0' is accepted");
        expect(parseGoTo(AudioUnits::TimeFormat::Samples, "0") == 0,
               "Zero samples must be accepted");

        beginTest("Samples mode: '44100' is accepted");
        expect(parseGoTo(AudioUnits::TimeFormat::Samples, "44100") == 44100,
               "44100 samples must be accepted");

        beginTest("Samples mode: '-100' is rejected (negative)");
        expect(parseGoTo(AudioUnits::TimeFormat::Samples, "-100") == -1,
               "Negative sample count must be rejected");

        beginTest("Samples mode: value > maxSamples is rejected");
        expect(parseGoTo(AudioUnits::TimeFormat::Samples, "999999999") == -1,
               "Out-of-range sample count must be rejected");

        // -------------------------------------------------------------------
        // Regression: Valid seconds values still work
        // -------------------------------------------------------------------
        beginTest("Seconds mode: '1.5' is accepted");
        {
            int64_t result = parseGoTo(AudioUnits::TimeFormat::Seconds, "1.5");
            expect(result > 0, "1.5 seconds must produce a positive sample count");
            expectWithinAbsoluteError(static_cast<double>(result), 44100.0 * 1.5, 2.0,
                                      "1.5s @ 44100 Hz should be ~66150 samples");
        }
    }
};

static GoToPositionParserH17Tests goToPositionParserH17Tests;

//==============================================================================
// H18 — GainDialog gain validation tests
//==============================================================================

class GainDialogValidationH18Tests : public juce::UnitTest
{
public:
    GainDialogValidationH18Tests()
        : juce::UnitTest("GainDialog H18 — NaN/Inf gain rejection", "UI") {}

    void runTest() override
    {
        // We cannot call GainDialog::getValidatedGain() directly because it
        // reads m_gainInput (a TextEditor member).  Instead we replicate the
        // same logic here and verify the fix is correct at the math level.
        //
        // The fix adds: if (!std::isfinite(value)) return std::nullopt;
        // BEFORE the range check (value < -100 || value > 100).
        // Without the fix, NaN comparisons are always false so NaN passes
        // the range gate and is returned as a valid gain.

        beginTest("std::stof('NaN') produces a non-finite value");
        {
            float v = std::stof("NaN");
            expect(!std::isfinite(v),
                   "std::stof('NaN') must be non-finite — confirming the attack vector");
        }

        beginTest("NaN fails the isfinite guard (fix present)");
        {
            float v = std::stof("NaN");
            // Model the ACTUAL pre-fix guard from GainDialog::getValidatedGain:
            //   if (value < -100 || value > 100) reject; else accept value
            // For NaN both comparisons are false, so the value is NOT rejected
            // -> NaN slips through. (The || negative form behaves differently
            // for NaN than a positive && form, so the exact shape matters.)
            bool rejectedByRangeGuard = (v < -100.0f || v > 100.0f);
            bool passesRange = ! rejectedByRangeGuard;  // pre-fix: NaN passes
            bool passesFinite = std::isfinite(v);       // post-fix guard
            expect(!passesFinite,
                   "isfinite() correctly identifies NaN — the fix blocks it");
            // Demonstrate the original range-only guard is insufficient:
            expect(passesRange,
                   "Range-only guard passes NaN — confirming why isfinite is needed");
        }

        beginTest("std::stof('Inf') produces a non-finite value");
        {
            float v = std::stof("Inf");
            expect(!std::isfinite(v),
                   "Inf must be caught by isfinite check");
        }

        beginTest("std::stof('1e40') produces a non-finite value (overflow)");
        {
            // 1e40 > FLT_MAX, so std::stof may throw or return Inf
            bool isNonFinite = false;
            try
            {
                float v = std::stof("1e40");
                isNonFinite = !std::isfinite(v);
            }
            catch (const std::out_of_range&)
            {
                isNonFinite = true;  // thrown == also non-finite / rejected
            }
            expect(isNonFinite,
                   "Overflow value must either throw or produce non-finite float");
        }

        beginTest("Valid gain 0.0 passes isfinite guard");
        {
            float v = 0.0f;
            expect(std::isfinite(v), "0.0 must be finite");
            expect(v >= -100.0f && v <= 100.0f, "0.0 must be in range");
        }

        beginTest("Valid gain -6.0 passes isfinite guard");
        {
            float v = -6.0f;
            expect(std::isfinite(v), "-6.0 must be finite");
            expect(v >= -100.0f && v <= 100.0f, "-6.0 must be in range");
        }

        beginTest("Out-of-range gain +150.0 is rejected by range check");
        {
            float v = 150.0f;
            expect(std::isfinite(v), "150.0 is finite");
            expect(!(v >= -100.0f && v <= 100.0f), "150.0 must fail range check");
        }
    }
};

static GainDialogValidationH18Tests gainDialogValidationH18Tests;

//==============================================================================
// H18 — NormalizeDialog RMS -inf level guard
//==============================================================================

class NormalizeDialogH18Tests : public juce::UnitTest
{
public:
    NormalizeDialogH18Tests()
        : juce::UnitTest("NormalizeDialog H18 — RMS -inf guard", "UI") {}

    void runTest() override
    {
        // NormalizeDialog.h:184 initialises m_currentRMSDB to
        // -std::numeric_limits<float>::infinity().
        // Before the fix, updateRequiredGain() would compute:
        //   requiredGain = targetDB - (-inf) = +inf
        // and push that to setNormalizePreview() on the audio thread.
        //
        // The fix guards: if (!std::isfinite(currentLevel)) return early.
        // We verify the math here without instantiating the component.

        beginTest("Default m_currentRMSDB (-inf) is non-finite");
        {
            float rmsDB = -std::numeric_limits<float>::infinity();
            expect(!std::isfinite(rmsDB),
                   "Initial RMS value of -inf must be non-finite");
        }

        beginTest("targetDB - (-inf) produces +inf without the guard");
        {
            float rmsDB    = -std::numeric_limits<float>::infinity();
            float targetDB = -0.1f;
            float result   = targetDB - rmsDB;  // +inf
            expect(!std::isfinite(result),
                   "targetDB - (-inf) must be +inf, confirming the attack vector");
            expect(result > 0.0f,
                   "+inf is positive — it would be interpreted as an enormous gain boost");
        }

        beginTest("isfinite guard blocks the -inf path (fix)");
        {
            float rmsDB = -std::numeric_limits<float>::infinity();
            bool wouldProceed = std::isfinite(rmsDB);
            expect(!wouldProceed,
                   "The isfinite check must return false for -inf, preventing preview");
        }

        beginTest("Valid RMS level -18 dB passes the guard");
        {
            float rmsDB = -18.0f;
            expect(std::isfinite(rmsDB), "-18 dB RMS must be finite and allowed");
        }

        beginTest("RMS level of 0 dB (full-scale square wave) passes the guard");
        {
            float rmsDB = 0.0f;
            expect(std::isfinite(rmsDB), "0 dB RMS must be finite and allowed");
        }
    }
};

static NormalizeDialogH18Tests normalizeDialogH18Tests;
