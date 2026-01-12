/*
  ==============================================================================

    UIConstants.h
    Created: 2026-01-12
    Author:  ZQ SFX

    Centralized UI constants for colors, fonts, and layout dimensions.
    All UI components should reference these constants for consistency.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

namespace waveedit
{
namespace ui
{

// =============================================================================
// Background Colors
// =============================================================================

/** Primary dialog/panel background */
constexpr juce::uint32 kBackgroundPrimary   = 0xff2b2b2b;

/** Secondary background for text editors, list boxes */
constexpr juce::uint32 kBackgroundSecondary = 0xff2a2a2a;

/** Dialog borders and separators */
constexpr juce::uint32 kBorder              = 0xff3d3d3d;

/** Visible borders for group components (WCAG 3:1 compliant) */
constexpr juce::uint32 kBorderVisible       = 0xff555555;

/** Highlight borders on hover/focus */
constexpr juce::uint32 kBorderHighlight     = 0xff3a3a3a;

/** Card/elevated surface color */
constexpr juce::uint32 kSurface             = 0xff383838;

/** Darker surface for nested panels */
constexpr juce::uint32 kSurfaceDark         = 0xff252525;

// =============================================================================
// Text Colors
// =============================================================================

/** Primary text color (white) */
constexpr juce::uint32 kTextPrimary         = 0xffffffff;

/** Secondary text for descriptions, help (WCAG compliant) */
constexpr juce::uint32 kTextSecondary       = 0xffcccccc;

/** Disabled text/controls */
constexpr juce::uint32 kTextDisabled        = 0xff666666;

/** Emphasis text (light green for values) */
constexpr juce::uint32 kTextEmphasis        = 0xff90ee90;

/** Muted text for very low priority info (WCAG AA compliant ~5.0:1) */
constexpr juce::uint32 kTextMuted           = 0xffaaaaaa;

// =============================================================================
// Accent Colors
// =============================================================================

/** Focus rings, links, highlights */
constexpr juce::uint32 kAccentPrimary       = 0xff4a90d9;

/** Focus ring width in pixels */
constexpr int kFocusRingWidth               = 2;

/** Preview button active state (dark red) */
constexpr juce::uint32 kButtonPreviewActive = 0xffcc0000;

/** Bypass button active state (orange) */
constexpr juce::uint32 kButtonBypassActive  = 0xffff8c00;

/** Selection background */
constexpr juce::uint32 kSelection           = 0xff3a5a8a;

/** Selection with alpha for overlays */
constexpr juce::uint32 kSelectionOverlay    = 0x404080ff;

// =============================================================================
// Waveform Colors
// =============================================================================

/** Waveform display color (green) */
constexpr juce::uint32 kWaveform            = 0xff00ff00;

/** RMS waveform overlay */
constexpr juce::uint32 kWaveformRMS         = 0xff008800;

/** Playback cursor */
constexpr juce::uint32 kCursor              = 0xffff0000;

/** Preview playback cursor */
constexpr juce::uint32 kPreviewCursor       = 0xff00ff00;

// =============================================================================
// Status Colors
// =============================================================================

/** Success/completed state */
constexpr juce::uint32 kStatusSuccess       = 0xff00aa00;

/** Warning state */
constexpr juce::uint32 kStatusWarning       = 0xffffaa00;

/** Error/failed state */
constexpr juce::uint32 kStatusError         = 0xffff0000;

/** Processing/pending state (WCAG AA compliant) */
constexpr juce::uint32 kStatusPending       = 0xffaaaaaa;

// =============================================================================
// Font Sizes
// =============================================================================

/** Dialog title font size */
constexpr float kFontDialogTitle            = 18.0f;

/** Section header font size */
constexpr float kFontSectionHeader          = 16.0f;

/** Body text / labels font size */
constexpr float kFontBody                   = 14.0f;

/** Bold value display font size */
constexpr float kFontBodyBold               = 14.0f;

/** Small text / captions font size */
constexpr float kFontSmall                  = 12.0f;

/** Very small / fine print font size */
constexpr float kFontTiny                   = 10.0f;

/** Monospace text for time displays */
constexpr float kFontMonospace              = 11.0f;

// =============================================================================
// Layout Dimensions
// =============================================================================

/** Padding inside dialogs */
constexpr int kDialogPadding                = 15;

/** Gap between major sections */
constexpr int kSectionGap                   = 10;

/** Gap between rows */
constexpr int kRowGap                       = 5;

/** Gap between buttons */
constexpr int kButtonGap                    = 10;

/** Standard button height */
constexpr int kButtonHeight                 = 30;

/** Standard button width */
constexpr int kButtonWidth                  = 90;

/** Narrow button width (Bypass) */
constexpr int kButtonWidthNarrow            = 70;

/** Wide button width (Start Processing) */
constexpr int kButtonWidthWide              = 120;

/** Standard input field height */
constexpr int kInputHeight                  = 24;

/** Slider text box width */
constexpr int kSliderTextWidth              = 60;

// =============================================================================
// Dialog Sizes
// =============================================================================

/** Standard processing dialog width */
constexpr int kDialogWidthStandard          = 450;

/** Wide dialog width (with preview graphics) */
constexpr int kDialogWidthWide              = 520;

/** Extra wide dialog width (with waveform) */
constexpr int kDialogWidthExtraWide         = 650;

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * @brief Create a JUCE color from a constant
 */
inline juce::Colour colour(juce::uint32 c)
{
    return juce::Colour(c);
}

/**
 * @brief Create a dialog title font
 */
inline juce::Font dialogTitleFont()
{
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return juce::Font(kFontDialogTitle, juce::Font::bold);
    #pragma GCC diagnostic pop
}

/**
 * @brief Create a section header font
 */
inline juce::Font sectionHeaderFont()
{
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return juce::Font(kFontSectionHeader, juce::Font::bold);
    #pragma GCC diagnostic pop
}

/**
 * @brief Create a body text font
 */
inline juce::Font bodyFont()
{
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return juce::Font(kFontBody, juce::Font::plain);
    #pragma GCC diagnostic pop
}

/**
 * @brief Create a bold value font
 */
inline juce::Font boldValueFont()
{
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return juce::Font(kFontBodyBold, juce::Font::bold);
    #pragma GCC diagnostic pop
}

/**
 * @brief Create a small text font
 */
inline juce::Font smallFont()
{
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return juce::Font(kFontSmall, juce::Font::plain);
    #pragma GCC diagnostic pop
}

/**
 * @brief Create a monospace font for time displays
 */
inline juce::Font monospaceFont()
{
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return juce::Font(juce::Font::getDefaultMonospacedFontName(), kFontMonospace, juce::Font::bold);
    #pragma GCC diagnostic pop
}

} // namespace ui
} // namespace waveedit
