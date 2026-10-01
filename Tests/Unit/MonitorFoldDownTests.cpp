/*
  ==============================================================================

    MonitorFoldDownTests.cpp
    Created: 2026-07-03
    Author:  ZQ SFX

    Unit tests for surround monitoring fold-down (QA finding H7).

    Covers the PURE, allocation-free matrix builder
    AudioEngine::buildFoldDownMatrix(), which maps source (file) channels onto
    the device output channels using ITU-R BS.775 coefficients (ITU_Standard
    preset, LFE excluded). The runtime application (renderFoldDownBlock) needs a
    live transport and audio device, so it is not exercised here; instead we
    verify the matrix (the deterministic core) exactly, plus solo-gated folding
    by feeding the matrix a per-channel input vector.

  ==============================================================================
*/

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

#include "Audio/AudioEngine.h"
#include "Audio/ChannelLayout.h"

// ITU-R BS.775 reference coefficients (sourced from ChannelLayout.h; restated
// here only as test tolerances/expectations).
namespace FoldRef
{
    constexpr float kUnity    = waveedit::ITUCoefficients::kUnityGain;   // 0 dB
    constexpr float kMinus3dB = waveedit::ITUCoefficients::kMinus3dB;    // 1/sqrt(2)
    constexpr float kTol      = 1.0e-5f;
}

class MonitorFoldDownTests : public juce::UnitTest
{
public:
    // Category "ChannelSystem": runs alongside the other ITU downmix tests via
    // the TestRunner's runCategory("ChannelSystem") dispatch.
    MonitorFoldDownTests() : juce::UnitTest("Monitor Fold-Down (H7)", "ChannelSystem") {}

    // Apply the matrix to a per-channel input sample vector for a single output.
    static float foldOne(const AudioEngine::FoldDownMatrix& m,
                         const std::vector<float>& in, int outChannel)
    {
        float acc = 0.0f;
        const int n = juce::jmin(m.numIn, static_cast<int>(in.size()));
        for (int i = 0; i < n; ++i)
            acc += in[static_cast<size_t>(i)] * m.m[outChannel][i];
        return acc;
    }

    void runTest() override
    {
        using Matrix = AudioEngine::FoldDownMatrix;

        // ---------------------------------------------------------------------
        beginTest("5.1 -> stereo: exact ITU-R BS.775 coefficients (LFE excluded)");
        {
            // Film/SMPTE 5.1 order: L, R, C, LFE, Ls, Rs
            const Matrix m = AudioEngine::buildFoldDownMatrix(6, 2);
            expectEquals(m.numIn, 6, "5.1 has 6 input channels");
            expectEquals(m.numOut, 2, "stereo has 2 output channels");

            // L (0) -> L only at unity
            expectWithinAbsoluteError(m.m[0][0], FoldRef::kUnity, FoldRef::kTol, "L->L unity");
            expectWithinAbsoluteError(m.m[1][0], 0.0f, FoldRef::kTol, "L->R zero");
            // R (1) -> R only at unity
            expectWithinAbsoluteError(m.m[0][1], 0.0f, FoldRef::kTol, "R->L zero");
            expectWithinAbsoluteError(m.m[1][1], FoldRef::kUnity, FoldRef::kTol, "R->R unity");
            // C (2) -> both at -3 dB
            expectWithinAbsoluteError(m.m[0][2], FoldRef::kMinus3dB, FoldRef::kTol, "C->L -3dB");
            expectWithinAbsoluteError(m.m[1][2], FoldRef::kMinus3dB, FoldRef::kTol, "C->R -3dB");
            // LFE (3) -> excluded (0 to both)
            expectWithinAbsoluteError(m.m[0][3], 0.0f, FoldRef::kTol, "LFE->L excluded");
            expectWithinAbsoluteError(m.m[1][3], 0.0f, FoldRef::kTol, "LFE->R excluded");
            // Ls (4) -> L only at -3 dB
            expectWithinAbsoluteError(m.m[0][4], FoldRef::kMinus3dB, FoldRef::kTol, "Ls->L -3dB");
            expectWithinAbsoluteError(m.m[1][4], 0.0f, FoldRef::kTol, "Ls->R zero");
            // Rs (5) -> R only at -3 dB
            expectWithinAbsoluteError(m.m[0][5], 0.0f, FoldRef::kTol, "Rs->L zero");
            expectWithinAbsoluteError(m.m[1][5], FoldRef::kMinus3dB, FoldRef::kTol, "Rs->R -3dB");
        }

        // ---------------------------------------------------------------------
        beginTest("7.1 -> stereo: side and rear surrounds fold to their sides");
        {
            // Film/SMPTE 7.1 order: L, R, C, LFE, Lss, Rss, Lrs, Rrs
            const Matrix m = AudioEngine::buildFoldDownMatrix(8, 2);
            expectEquals(m.numIn, 8, "7.1 has 8 input channels");

            expectWithinAbsoluteError(m.m[0][0], FoldRef::kUnity, FoldRef::kTol, "L->L unity");
            expectWithinAbsoluteError(m.m[1][1], FoldRef::kUnity, FoldRef::kTol, "R->R unity");
            expectWithinAbsoluteError(m.m[0][2], FoldRef::kMinus3dB, FoldRef::kTol, "C->L -3dB");
            expectWithinAbsoluteError(m.m[1][2], FoldRef::kMinus3dB, FoldRef::kTol, "C->R -3dB");
            expectWithinAbsoluteError(m.m[0][3], 0.0f, FoldRef::kTol, "LFE->L excluded");
            expectWithinAbsoluteError(m.m[1][3], 0.0f, FoldRef::kTol, "LFE->R excluded");
            // Side surrounds
            expectWithinAbsoluteError(m.m[0][4], FoldRef::kMinus3dB, FoldRef::kTol, "Lss->L -3dB");
            expectWithinAbsoluteError(m.m[1][4], 0.0f, FoldRef::kTol, "Lss->R zero");
            expectWithinAbsoluteError(m.m[1][5], FoldRef::kMinus3dB, FoldRef::kTol, "Rss->R -3dB");
            expectWithinAbsoluteError(m.m[0][5], 0.0f, FoldRef::kTol, "Rss->L zero");
            // Rear surrounds
            expectWithinAbsoluteError(m.m[0][6], FoldRef::kMinus3dB, FoldRef::kTol, "Lrs->L -3dB");
            expectWithinAbsoluteError(m.m[1][6], 0.0f, FoldRef::kTol, "Lrs->R zero");
            expectWithinAbsoluteError(m.m[1][7], FoldRef::kMinus3dB, FoldRef::kTol, "Rrs->R -3dB");
            expectWithinAbsoluteError(m.m[0][7], 0.0f, FoldRef::kTol, "Rrs->L zero");
        }

        // ---------------------------------------------------------------------
        beginTest("Quad -> stereo: surrounds fold to their sides at -3 dB");
        {
            // Quad order: L, R, Ls, Rs
            const Matrix m = AudioEngine::buildFoldDownMatrix(4, 2);
            expectEquals(m.numIn, 4, "quad has 4 input channels");

            expectWithinAbsoluteError(m.m[0][0], FoldRef::kUnity, FoldRef::kTol, "L->L unity");
            expectWithinAbsoluteError(m.m[1][1], FoldRef::kUnity, FoldRef::kTol, "R->R unity");
            expectWithinAbsoluteError(m.m[0][2], FoldRef::kMinus3dB, FoldRef::kTol, "Ls->L -3dB");
            expectWithinAbsoluteError(m.m[1][2], 0.0f, FoldRef::kTol, "Ls->R zero");
            expectWithinAbsoluteError(m.m[1][3], FoldRef::kMinus3dB, FoldRef::kTol, "Rs->R -3dB");
            expectWithinAbsoluteError(m.m[0][3], 0.0f, FoldRef::kTol, "Rs->L zero");
        }

        // ---------------------------------------------------------------------
        beginTest("3.0 (LCR) -> stereo: centre to both at -3 dB");
        {
            // LCR order: L, R, C
            const Matrix m = AudioEngine::buildFoldDownMatrix(3, 2);
            expectEquals(m.numIn, 3, "3.0 has 3 input channels");

            expectWithinAbsoluteError(m.m[0][0], FoldRef::kUnity, FoldRef::kTol, "L->L unity");
            expectWithinAbsoluteError(m.m[1][1], FoldRef::kUnity, FoldRef::kTol, "R->R unity");
            expectWithinAbsoluteError(m.m[0][2], FoldRef::kMinus3dB, FoldRef::kTol, "C->L -3dB");
            expectWithinAbsoluteError(m.m[1][2], FoldRef::kMinus3dB, FoldRef::kTol, "C->R -3dB");
        }

        // ---------------------------------------------------------------------
        beginTest("Passthrough identity when source <= outputs (no fold-down)");
        {
            // Stereo on a stereo device: identity, no cross-feed.
            const Matrix st = AudioEngine::buildFoldDownMatrix(2, 2);
            expectWithinAbsoluteError(st.m[0][0], 1.0f, FoldRef::kTol, "stereo L identity");
            expectWithinAbsoluteError(st.m[1][1], 1.0f, FoldRef::kTol, "stereo R identity");
            expectWithinAbsoluteError(st.m[0][1], 0.0f, FoldRef::kTol, "stereo no R->L");
            expectWithinAbsoluteError(st.m[1][0], 0.0f, FoldRef::kTol, "stereo no L->R");

            // Mono on a stereo device: single identity entry (callback duplicates).
            const Matrix mono = AudioEngine::buildFoldDownMatrix(1, 2);
            expectEquals(mono.numIn, 1, "mono has 1 input channel");
            expectWithinAbsoluteError(mono.m[0][0], 1.0f, FoldRef::kTol, "mono identity");

            // Source fewer than outputs (e.g. stereo on a hypothetical 6-out device).
            const Matrix fewer = AudioEngine::buildFoldDownMatrix(2, 6);
            expectWithinAbsoluteError(fewer.m[0][0], 1.0f, FoldRef::kTol, "2<6 L identity");
            expectWithinAbsoluteError(fewer.m[1][1], 1.0f, FoldRef::kTol, "2<6 R identity");
        }

        // ---------------------------------------------------------------------
        beginTest("Solo-gated input folds through BS.775 targets");
        {
            // renderFoldDownBlock() gates SOURCE channels (solo/mute) BEFORE the
            // fold. Model that here: a 5.1 input where only Rs (index 5) survives
            // (as if channel 5 were soloed) must fold to R at -3 dB and L at zero.
            const Matrix m = AudioEngine::buildFoldDownMatrix(6, 2);

            std::vector<float> soloRs(6, 0.0f);
            soloRs[5] = 1.0f;  // only Rs active
            expectWithinAbsoluteError(foldOne(m, soloRs, 0), 0.0f, FoldRef::kTol,
                                      "solo Rs -> L silent");
            expectWithinAbsoluteError(foldOne(m, soloRs, 1), FoldRef::kMinus3dB, FoldRef::kTol,
                                      "solo Rs -> R at -3dB");

            // Solo of the centre (index 2) folds equally to both outputs at -3 dB.
            std::vector<float> soloC(6, 0.0f);
            soloC[2] = 1.0f;
            expectWithinAbsoluteError(foldOne(m, soloC, 0), FoldRef::kMinus3dB, FoldRef::kTol,
                                      "solo C -> L at -3dB");
            expectWithinAbsoluteError(foldOne(m, soloC, 1), FoldRef::kMinus3dB, FoldRef::kTol,
                                      "solo C -> R at -3dB");

            // Solo of the LFE (index 3) produces silence (LFE excluded from fold).
            std::vector<float> soloLfe(6, 0.0f);
            soloLfe[3] = 1.0f;
            expectWithinAbsoluteError(foldOne(m, soloLfe, 0), 0.0f, FoldRef::kTol,
                                      "solo LFE -> L silent (excluded)");
            expectWithinAbsoluteError(foldOne(m, soloLfe, 1), 0.0f, FoldRef::kTol,
                                      "solo LFE -> R silent (excluded)");
        }
    }
};

static MonitorFoldDownTests monitorFoldDownTests;
