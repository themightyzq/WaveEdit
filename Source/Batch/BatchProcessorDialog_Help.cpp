/*
  ==============================================================================

    BatchProcessorDialog_Help.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Split out from BatchProcessorDialog.cpp under CLAUDE.md Sec 7.5
    (file-size cap). Hosts the naming-pattern help popup text, which is long
    but self-contained. The dialog itself stays in BatchProcessorDialog.cpp.

  ==============================================================================
*/

#include "BatchProcessorDialog.h"

namespace waveedit
{

void BatchProcessorDialog::onPatternHelpClicked()
{
    juce::String helpText =
        "Output Naming Pattern Tokens:\n"
        "\n"
        "{filename}    - Original filename (without extension)\n"
        "                Example: \"drums.wav\" -> \"drums\"\n"
        "\n"
        "{index}       - File index (1, 2, 3, ...)\n"
        "                Example: First file -> \"1\"\n"
        "\n"
        "{index:03}    - Zero-padded index (001, 002, 003, ...)\n"
        "                Change 03 to any width: 02 = 01, 04 = 0001\n"
        "\n"
        "{date}        - Current date (YYYY-MM-DD)\n"
        "                Example: \"2026-01-12\"\n"
        "\n"
        "{time}        - Current time (HH-MM-SS)\n"
        "                Example: \"14-30-45\"\n"
        "\n"
        "{preset}      - Name of the selected preset\n"
        "                Example: \"Broadcast Ready\"\n"
        "\n"
        "{samplerate}  - Output sample rate in Hz (e.g. \"48000\")\n"
        "{bitdepth}    - Output bit depth in bits (e.g. \"24\")\n"
        "{channels}    - Output channel count (e.g. \"2\")\n"
        "\n"
        "Examples:\n"
        "  \"{filename}_processed\"     -> drums_processed.wav\n"
        "  \"{filename}_{index:03}\"    -> drums_001.wav\n"
        "  \"batch_{date}_{index:03}\"  -> batch_2026-01-12_001.wav\n"
        "  \"{preset}_{filename}\"      -> Broadcast Ready_drums.wav";

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Naming Pattern Help",
        helpText,
        "OK"
    );
}

} // namespace waveedit
