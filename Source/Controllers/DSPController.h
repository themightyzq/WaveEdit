/*
  ==============================================================================

    DSPController.h - DSP Application Controller
    Part of WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/Document.h"
#include "../DSP/HeadTailRecipe.h"

/**
 * DSPController handles all DSP application methods for audio editing.
 *
 * Responsibilities:
 * - Quick operations: gain adjustment, normalization, fade in/out, DC offset removal, silence, trim
 * - Dialog-based operations: showing dialogs and applying user input
 * - Plugin operations: offline plugin application and plugin chain processing
 *
 * All methods operate on a Document passed as a parameter, making them testable
 * without UI dependencies.
 */
class DSPController
{
public:
    DSPController();
    ~DSPController() = default;

    // Quick operations (no dialog)
    void applyGainAdjustment(Document* doc, float gainDB, int64_t startSample = -1, int64_t endSample = -1);
    void silenceSelection(Document* doc);
    void trimToSelection(Document* doc);
    void applyFadeIn(Document* doc);
    void applyFadeOut(Document* doc);
    void applyNormalize(Document* doc);
    void applyDCOffset(Document* doc);
    void applyDCOffsetRemoval(Document* doc);

    void reverseSelection(Document* doc);
    void invertSelection(Document* doc);

    // Dialog-based operations
    void showResampleDialog(Document* doc, juce::Component* parent);
    void showGainDialog(Document* doc, juce::Component* parent);
    void showNormalizeDialog(Document* doc, juce::Component* parent);
    void showFadeInDialog(Document* doc, juce::Component* parent);
    void showFadeOutDialog(Document* doc, juce::Component* parent);
    void showParametricEQDialog(Document* doc, juce::Component* parent);
    void showGraphicalEQDialog(Document* doc, juce::Component* parent);
    void showChannelConverterDialog(Document* doc, juce::Component* parent);
    void showChannelExtractorDialog(Document* doc, juce::Component* parent);

    // Head & Tail processing
    void applyHeadTail(Document* doc, const HeadTailRecipe& recipe);
    void showHeadTailDialog(Document* doc, juce::Component* parent);

    // Looping Tools
    void showLoopingToolsDialog(Document* doc, juce::Component* parent);

    // Plugin operations
    void showOfflinePluginDialog(Document* doc, juce::Component* parent);
    void applyPluginChainToSelection(Document* doc);
    void applyPluginChainToSelectionWithOptions(Document* doc, bool convertToStereo, bool includeTail, double tailLengthSeconds);

private:
    void applyPluginChainToSelectionInternal(Document* doc, bool convertToStereo, bool includeTail, double tailLengthSeconds);
    void applyOfflinePluginToSelection(Document* doc,
                                       const juce::PluginDescription& pluginDesc,
                                       const juce::MemoryBlock& pluginState,
                                       int64_t startSample,
                                       int64_t numSamples,
                                       bool convertToStereo,
                                       bool includeTail,
                                       double tailLengthSeconds);

    // Progress dialog threshold for async operations
    static constexpr int64_t kProgressDialogThreshold = 500000;
};
