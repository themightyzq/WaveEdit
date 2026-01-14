# WaveEdit UI Design Standards

**Version**: 1.0.0
**Last Updated**: 2026-01-12
**Maintainer**: Claude Code

---

## 1. Color Palette

### 1.1 Core Colors

| Name | Hex | Usage |
|------|-----|-------|
| Background Primary | `#2b2b2b` | Dialog backgrounds, panels |
| Background Secondary | `#2a2a2a` | Text editors, list boxes |
| Border | `#3d3d3d` | Dialog borders, separators |
| Border Highlight | `#3a3a3a` | Hover states, selection |
| Surface | `#383838` | Cards, elevated components |

### 1.2 Text Colors

| Name | Hex | Usage |
|------|-----|-------|
| Text Primary | `#ffffff` | Main text, labels |
| Text Secondary | `#cccccc` | Help text, descriptions |
| Text Disabled | `#666666` | Disabled controls |
| Text Emphasis | `#90EE90` | Success states, values |

### 1.3 Accent Colors

| Name | Hex | Usage |
|------|-----|-------|
| Accent Primary | `#4a90d9` | Focus rings, links |
| Button Active | `#cc0000` | Preview active state |
| Button Bypass | `#ff8c00` | Bypass active state |
| Selection | `#3a5a8a` | Selected items |

### 1.4 Waveform Colors

| Name | Hex | Usage |
|------|-----|-------|
| Waveform | `#00ff00` | Audio waveform display |
| Waveform RMS | `#008800` | RMS overlay |
| Cursor | `#ff0000` | Playback cursor |
| Selection | `#4080ff40` | Selection highlight (40% alpha) |

---

## 2. Typography

### 2.1 Font Sizes

| Name | Size | Weight | Usage |
|------|------|--------|-------|
| Dialog Title | 18pt | Bold | Dialog headers |
| Section Header | 16pt | Bold | Section titles |
| Body | 14pt | Normal | Labels, instructions |
| Body Bold | 14pt | Bold | Values, emphasis |
| Small | 12pt | Normal | Help text, captions |
| Monospace | 11pt | Bold | Time displays |

### 2.2 Font API (Recommended)

```cpp
// Use juce::FontOptions (modern API)
auto titleFont = juce::FontOptions(18.0f).withStyle("Bold");
auto bodyFont = juce::FontOptions(14.0f);
auto smallFont = juce::FontOptions(12.0f);
auto monoFont = juce::FontOptions("Monospace", 11.0f).withStyle("Bold");
```

---

## 3. Layout Standards

### 3.1 Dialog Dimensions

| Dialog Type | Width | Height |
|-------------|-------|--------|
| Standard | 450px | 260-380px |
| Wide (with graphics) | 520px | 270-350px |
| Extra Wide (waveform) | 650px | 500-600px |
| Modal Alerts | auto | auto |

### 3.2 Spacing

| Element | Value |
|---------|-------|
| Dialog Padding | 15px |
| Section Gap | 10px |
| Row Gap | 5px |
| Button Gap | 10px |
| Label-Control Gap | 5px |

### 3.3 Processing Dialog Footer

**MANDATORY for all Process menu dialogs.**

```
Layout (left to right):
[Preview 90px] [Bypass 70px] [Loop 60px] --- [Cancel 90px] [Apply 90px]

Heights: 30px
Bottom margin: 10px from dialog edge
```

Implementation:
```cpp
void resized() override
{
    auto bounds = getLocalBounds().reduced(15);

    // Reserve bottom for buttons
    bounds.removeFromTop(bounds.getHeight() - 40);
    auto buttonRow = bounds.removeFromTop(30);

    const int buttonWidth = 90;
    const int buttonSpacing = 10;

    // Left side
    m_previewButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    buttonRow.removeFromLeft(buttonSpacing);
    m_bypassButton.setBounds(buttonRow.removeFromLeft(70));
    buttonRow.removeFromLeft(buttonSpacing);
    m_loopToggle.setBounds(buttonRow.removeFromLeft(60));

    // Right side
    m_applyButton.setBounds(buttonRow.removeFromRight(buttonWidth));
    buttonRow.removeFromRight(buttonSpacing);
    m_cancelButton.setBounds(buttonRow.removeFromRight(buttonWidth));
}
```

---

## 4. Component Specifications

### 4.1 Buttons

| Property | Value |
|----------|-------|
| Height | 24-30px |
| Min Width | 60px |
| Corner Radius | 4px (default) |
| Font Size | 14pt |

**Button States:**
- Default: System button color
- Hover: Lightened 10%
- Pressed: Darkened 10%
- Disabled: 50% opacity
- Focus: 2px accent outline

### 4.2 Sliders

| Property | Value |
|----------|-------|
| Height | 20-24px |
| Thumb Size | 16px |
| Track Height | 6px |
| Text Box Width | 50-60px |

```cpp
slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
slider.setRange(min, max, step);
```

### 4.3 Text Editors

| Property | Value |
|----------|-------|
| Height | 24-28px |
| Font Size | 14pt |
| Padding | 4px |
| Background | #2a2a2a |
| Border | 1px #3a3a3a |

### 4.4 Combo Boxes

| Property | Value |
|----------|-------|
| Height | 24px |
| Min Width | 100px |
| Font Size | 13pt |

### 4.5 Labels

| Property | Value |
|----------|-------|
| Alignment | Left (default) |
| Font Size | 13-14pt |
| Color | White/Primary |

---

## 5. Accessibility Requirements

### 5.1 Keyboard Navigation

**Required for all dialogs:**

| Key | Action |
|-----|--------|
| Tab | Move focus forward |
| Shift+Tab | Move focus backward |
| Return | Activate primary action (Apply) |
| Escape | Cancel/Close dialog |
| Space | Activate focused button |
| Arrow Keys | Adjust slider values |

### 5.2 Focus Indicators

All focusable components MUST show a visible focus ring:

```cpp
void paint(juce::Graphics& g) override
{
    // ... normal painting ...

    if (hasKeyboardFocus(true))
    {
        g.setColour(juce::Colour(0xff4a90d9)); // Accent Primary
        g.drawRect(getLocalBounds(), 2);
    }
}
```

### 5.3 Tooltips

All interactive controls MUST have tooltips:

```cpp
m_button.setTooltip("Description (Shortcut)");
```

Format: `"Action description (keyboard shortcut if any)"`

### 5.4 Color Contrast

| Requirement | Minimum Ratio |
|-------------|---------------|
| Text on background | 4.5:1 |
| Large text (18pt+) | 3:1 |
| Interactive controls | 3:1 |

---

## 6. State Management

### 6.1 Preview Button States

```cpp
// Default state
m_previewButton.setButtonText("Preview");
m_previewButton.setColour(TextButton::buttonColourId, defaultColor);

// Active state
m_previewButton.setButtonText("Stop Preview");
m_previewButton.setColour(TextButton::buttonColourId, juce::Colour(0xffcc0000));
```

### 6.2 Bypass Button States

```cpp
// Default (during preview)
m_bypassButton.setButtonText("Bypass");
m_bypassButton.setColour(TextButton::buttonColourId, defaultColor);

// Active (bypassed)
m_bypassButton.setButtonText("Bypassed");
m_bypassButton.setColour(TextButton::buttonColourId, juce::Colour(0xffff8c00));
```

### 6.3 Persistent State

These settings persist across dialog instances using static variables:

- Loop toggle state
- Last used values (gain, normalize target, etc.)

---

## 7. Dialog Templates

### 7.1 Standard Processing Dialog

```cpp
class ProcessingDialog : public juce::Component
{
public:
    ProcessingDialog()
        : m_previewButton("Preview")
        , m_bypassButton("Bypass")
        , m_cancelButton("Cancel")
        , m_applyButton("Apply")
    {
        // Title
        m_titleLabel.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
        m_titleLabel.setText("Effect Name", juce::dontSendNotification);
        addAndMakeVisible(m_titleLabel);

        // Parameters section
        // ... add parameter controls ...

        // Footer buttons
        m_previewButton.onClick = [this]() { onPreviewClicked(); };
        m_bypassButton.onClick = [this]() { onBypassClicked(); };
        m_cancelButton.onClick = [this]() { onCancelClicked(); };
        m_applyButton.onClick = [this]() { onApplyClicked(); };

        m_bypassButton.setEnabled(false);
        m_loopToggle.setButtonText("Loop");
        m_loopToggle.setToggleState(true, juce::dontSendNotification);

        addAndMakeVisible(m_previewButton);
        addAndMakeVisible(m_bypassButton);
        addAndMakeVisible(m_loopToggle);
        addAndMakeVisible(m_cancelButton);
        addAndMakeVisible(m_applyButton);

        setSize(450, 280);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff2b2b2b));
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(15);

        m_titleLabel.setBounds(bounds.removeFromTop(30));
        bounds.removeFromTop(10);

        // ... layout parameters ...

        // Footer (standardized)
        bounds.removeFromTop(bounds.getHeight() - 40);
        auto buttonRow = bounds.removeFromTop(30);

        m_previewButton.setBounds(buttonRow.removeFromLeft(90));
        buttonRow.removeFromLeft(10);
        m_bypassButton.setBounds(buttonRow.removeFromLeft(70));
        buttonRow.removeFromLeft(10);
        m_loopToggle.setBounds(buttonRow.removeFromLeft(60));

        m_applyButton.setBounds(buttonRow.removeFromRight(90));
        buttonRow.removeFromRight(10);
        m_cancelButton.setBounds(buttonRow.removeFromRight(90));
    }

private:
    juce::Label m_titleLabel;
    juce::TextButton m_previewButton;
    juce::TextButton m_bypassButton;
    juce::ToggleButton m_loopToggle;
    juce::TextButton m_cancelButton;
    juce::TextButton m_applyButton;
};
```

---

## 8. Code Constants Reference

### 8.1 Proposed UIConstants.h

```cpp
#pragma once

namespace waveedit
{
namespace ui
{

// Background colors
constexpr uint32_t kBackgroundPrimary   = 0xff2b2b2b;
constexpr uint32_t kBackgroundSecondary = 0xff2a2a2a;
constexpr uint32_t kBorder              = 0xff3d3d3d;
constexpr uint32_t kBorderHighlight     = 0xff3a3a3a;
constexpr uint32_t kSurface             = 0xff383838;

// Text colors
constexpr uint32_t kTextPrimary         = 0xffffffff;
constexpr uint32_t kTextSecondary       = 0xffcccccc;
constexpr uint32_t kTextDisabled        = 0xff666666;
constexpr uint32_t kTextEmphasis        = 0xff90ee90;

// Accent colors
constexpr uint32_t kAccentPrimary       = 0xff4a90d9;
constexpr uint32_t kButtonActive        = 0xffcc0000;
constexpr uint32_t kButtonBypassed      = 0xffff8c00;
constexpr uint32_t kSelection           = 0xff3a5a8a;

// Font sizes
constexpr float kFontDialogTitle        = 18.0f;
constexpr float kFontSectionHeader      = 16.0f;
constexpr float kFontBody               = 14.0f;
constexpr float kFontSmall              = 12.0f;
constexpr float kFontMonospace          = 11.0f;

// Layout
constexpr int kDialogPadding            = 15;
constexpr int kSectionGap               = 10;
constexpr int kRowGap                   = 5;
constexpr int kButtonGap                = 10;
constexpr int kButtonHeight             = 30;
constexpr int kStandardButtonWidth      = 90;

} // namespace ui
} // namespace waveedit
```

---

---

## 9. Channel Selection & Focus

### 9.1 Channel Label States

Channel labels display different background colors based on state:

| State | Background | Text | Priority |
|-------|-----------|------|----------|
| Solo | `#ddaa00` (Yellow) | White | 1 (Highest) |
| Mute | `#dd3333` (Red) | White | 2 |
| Focused | `#3a7dd4` (Blue) | White | 3 |
| Unfocused (in focus mode) | Transparent | Grey 50% | 4 |
| Default | Transparent | Grey | 5 (Lowest) |

### 9.2 Per-Channel Interactions

| Action | Result |
|--------|--------|
| Double-click waveform | Focus that channel + select entire duration |
| Double-click already-focused channel | Unfocus (return to "all channels") |
| Double-click channel label | Toggle focus for that channel |
| Double-click gap between channels | Focus all channels + select all |

### 9.3 Visual Feedback for Focus Mode

When per-channel focus is active (not all channels focused):

1. **Focused channels**:
   - Cyan highlight border (2px)
   - Full brightness waveform
   - Blue background on label

2. **Unfocused channels**:
   - Dimmed background (#1a1a1a)
   - 40% opacity waveform
   - Dimmed label text

### 9.4 Implementation Pattern

```cpp
// Check if in per-channel focus mode
bool showFocusIndicator = (m_focusedChannels != -1);

// Draw label based on state
if (isSolo)
    labelBgColor = juce::Colour(0xffddaa00);
else if (isMute)
    labelBgColor = juce::Colour(0xffdd3333);
else if (showFocusIndicator && isChannelCurrentlyFocused)
    labelBgColor = juce::Colour(0xff3a7dd4);

// Toggle focus on double-click
if (m_focusedChannels == (1 << channel))
    focusChannel(-1);  // Already focused, unfocus all
else
    focusChannel(channel);  // Focus this channel
```

---

## 10. Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.1.0 | 2026-01-13 | Added per-channel selection documentation |
| 1.0.0 | 2026-01-12 | Initial documentation from UI/UX audit |

---

*Document maintained as part of WaveEdit development standards.*
*See also: CLAUDE.md Section 6.8 for processing dialog requirements.*
