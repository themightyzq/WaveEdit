/*
  ==============================================================================

    WaveEditTheme.cpp
    Copyright (C) 2025 ZQ SFX

  ==============================================================================
*/

#include "WaveEditTheme.h"

namespace waveedit
{

namespace BuiltInThemes
{

Theme dark()
{
    Theme t;
    t.id   = "dark";
    t.name = "Dark";

    // Surfaces — match the historical hardcoded values from
    // WaveformDisplay so no visible regression on the default theme.
    t.background        = juce::Colour(0xff1e1e1e);
    t.panel             = juce::Colour(0xff252525);
    t.panelAlternate    = juce::Colour(0xff2a2a2a);
    t.border            = juce::Colour(0xff3a3a3a);

    // Waveform-specific
    t.waveformBackground = juce::Colour(0xff252525);
    t.waveformLine       = juce::Colour(0xff00d4aa);  // existing brand teal
    t.waveformBorder     = juce::Colour(0xff00aaff);  // existing focus cyan
    t.ruler              = juce::Colour(0xff2a2a2a);
    t.rulerText          = juce::Colour(0xffe0e0e0);
    t.gridLine           = juce::Colour(0xff3a3a3a);

    // Foreground / text
    t.text       = juce::Colour(0xffe0e0e0);
    t.textMuted  = juce::Colour(0xff909090);
    t.accent     = juce::Colour(0xff4a90d9);

    // Status
    t.selection = juce::Colour(0x88ffff00);  // semi-transparent yellow
    t.focus     = juce::Colour(0xff00aaff);
    t.warning   = juce::Colour(0xffddaa00);
    t.success   = juce::Colour(0xff66cc77);
    t.error     = juce::Colour(0xffd64545);

    return t;
}

Theme light()
{
    Theme t;
    t.id   = "light";
    t.name = "Light";

    // Surfaces — neutral light palette tuned for daylight readability.
    t.background        = juce::Colour(0xfff4f4f4);
    t.panel             = juce::Colour(0xffffffff);
    t.panelAlternate    = juce::Colour(0xfff0f0f0);
    t.border            = juce::Colour(0xffd0d0d0);

    // Waveform — keep the brand teal but darken slightly for contrast
    // against a light background.
    t.waveformBackground = juce::Colour(0xffffffff);
    t.waveformLine       = juce::Colour(0xff009478);  // darker teal for light bg
    t.waveformBorder     = juce::Colour(0xff0080cc);
    t.ruler              = juce::Colour(0xfff0f0f0);
    t.rulerText          = juce::Colour(0xff333333);
    t.gridLine           = juce::Colour(0xffd0d0d0);

    // Foreground / text
    t.text       = juce::Colour(0xff202020);
    t.textMuted  = juce::Colour(0xff666666);
    t.accent     = juce::Colour(0xff1a6dbb);

    // Status — same hue as Dark, adjusted for contrast.
    t.selection = juce::Colour(0x66ffaa00);
    t.focus     = juce::Colour(0xff0080cc);
    t.warning   = juce::Colour(0xffb88000);
    t.success   = juce::Colour(0xff2a8a3a);
    t.error     = juce::Colour(0xffb83030);

    return t;
}

Theme highContrast()
{
    Theme t;
    t.id   = "high-contrast";
    t.name = "High Contrast";

    // Surfaces — pure black canvases with stark white separators for
    // maximum legibility (WCAG AAA contrast against the foreground tokens).
    t.background        = juce::Colour(0xff000000);
    t.panel             = juce::Colour(0xff000000);
    t.panelAlternate    = juce::Colour(0xff141414);
    t.border            = juce::Colour(0xffffffff);

    // Waveform — neon yellow line on pure black for ~14:1 contrast.
    t.waveformBackground = juce::Colour(0xff000000);
    t.waveformLine       = juce::Colour(0xffffff00);
    t.waveformBorder     = juce::Colour(0xff00ffff);
    t.ruler              = juce::Colour(0xff141414);
    t.rulerText          = juce::Colour(0xffffffff);
    t.gridLine           = juce::Colour(0xff5a5a5a);

    // Foreground / text — full white primary, light grey muted (still > 7:1).
    t.text       = juce::Colour(0xffffffff);
    t.textMuted  = juce::Colour(0xffbbbbbb);
    t.accent     = juce::Colour(0xff00ffff);

    // Status — saturated primaries; selection uses semi-transparent cyan
    // so highlighted ranges remain readable.
    t.selection  = juce::Colour(0x9900ffff);
    t.focus      = juce::Colour(0xff00ffff);
    t.warning    = juce::Colour(0xffffbf00);
    t.success    = juce::Colour(0xff00ff66);
    t.error      = juce::Colour(0xffff5050);

    return t;
}

}  // namespace BuiltInThemes
}  // namespace waveedit
