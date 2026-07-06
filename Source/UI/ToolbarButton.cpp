/*
  ==============================================================================

    ToolbarButton.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "ToolbarButton.h"
#include "../Commands/CommandIDs.h"
#include "ThemeManager.h"

//==============================================================================
// Command name to CommandID mapping (mirrors KeymapManager)
static juce::CommandID getCommandIDForName(const juce::String& commandName)
{
    // Map command names to CommandIDs
    static std::map<juce::String, juce::CommandID> commandMap;

    if (commandMap.empty())
    {
        // File operations
        commandMap["fileNew"] = CommandIDs::fileNew;
        commandMap["fileOpen"] = CommandIDs::fileOpen;
        commandMap["fileSave"] = CommandIDs::fileSave;
        commandMap["fileSaveAs"] = CommandIDs::fileSaveAs;
        commandMap["fileClose"] = CommandIDs::fileClose;

        // Edit operations
        commandMap["editUndo"] = CommandIDs::editUndo;
        commandMap["editRedo"] = CommandIDs::editRedo;
        commandMap["editCut"] = CommandIDs::editCut;
        commandMap["editCopy"] = CommandIDs::editCopy;
        commandMap["editPaste"] = CommandIDs::editPaste;
        commandMap["editDelete"] = CommandIDs::editDelete;
        commandMap["editSelectAll"] = CommandIDs::editSelectAll;
        commandMap["editSilence"] = CommandIDs::editSilence;
        commandMap["editTrim"] = CommandIDs::editTrim;

        // Playback operations
        commandMap["playbackPlay"] = CommandIDs::playbackPlay;
        commandMap["playbackPause"] = CommandIDs::playbackPause;
        commandMap["playbackStop"] = CommandIDs::playbackStop;
        commandMap["playbackLoop"] = CommandIDs::playbackLoop;
        commandMap["playbackRecord"] = CommandIDs::playbackRecord;

        // View operations
        commandMap["viewZoomIn"] = CommandIDs::viewZoomIn;
        commandMap["viewZoomOut"] = CommandIDs::viewZoomOut;
        commandMap["viewZoomFit"] = CommandIDs::viewZoomFit;
        commandMap["viewZoomSelection"] = CommandIDs::viewZoomSelection;
        commandMap["viewZoomOneToOne"] = CommandIDs::viewZoomOneToOne;

        // Processing operations
        commandMap["processFadeIn"] = CommandIDs::processFadeIn;
        commandMap["processFadeOut"] = CommandIDs::processFadeOut;
        commandMap["processNormalize"] = CommandIDs::processNormalize;
        commandMap["processDCOffset"] = CommandIDs::processDCOffset;
        commandMap["processGain"] = CommandIDs::processGain;
        commandMap["processIncreaseGain"] = CommandIDs::processIncreaseGain;
        commandMap["processDecreaseGain"] = CommandIDs::processDecreaseGain;
        commandMap["processGraphicalEQ"] = CommandIDs::processGraphicalEQ;

        // Navigation operations
        commandMap["navigateStart"] = CommandIDs::navigateStart;
        commandMap["navigateEnd"] = CommandIDs::navigateEnd;
        commandMap["navigateGoToPosition"] = CommandIDs::navigateGoToPosition;

        // Region operations
        commandMap["regionAdd"] = CommandIDs::regionAdd;
        commandMap["regionDelete"] = CommandIDs::regionDelete;
        commandMap["regionNext"] = CommandIDs::regionNext;
        commandMap["regionPrevious"] = CommandIDs::regionPrevious;
        commandMap["regionExportAll"] = CommandIDs::regionExportAll;
        commandMap["regionShowList"] = CommandIDs::regionShowList;

        // Marker operations
        commandMap["markerAdd"] = CommandIDs::markerAdd;
        commandMap["markerDelete"] = CommandIDs::markerDelete;
        commandMap["markerNext"] = CommandIDs::markerNext;
        commandMap["markerPrevious"] = CommandIDs::markerPrevious;
        commandMap["markerShowList"] = CommandIDs::markerShowList;

        // Plugin operations
        commandMap["pluginShowChain"] = CommandIDs::pluginShowChain;
        commandMap["pluginAddPlugin"] = CommandIDs::pluginAddPlugin;
        commandMap["pluginApplyChain"] = CommandIDs::pluginApplyChain;
        commandMap["pluginBypassAll"] = CommandIDs::pluginBypassAll;
    }

    auto it = commandMap.find(commandName);
    if (it != commandMap.end())
        return it->second;

    return 0;  // Invalid command
}

//==============================================================================
ToolbarButton::ToolbarButton(const ToolbarButtonConfig& config,
                             juce::ApplicationCommandManager* commandManager)
    : m_config(config),
      m_commandManager(commandManager),
      m_isHovered(false),
      m_isPressed(false)
{
    // Skip button creation for separator/spacer types
    if (m_config.type == ToolbarButtonType::SEPARATOR ||
        m_config.type == ToolbarButtonType::SPACER)
    {
        return;
    }

    // Create button with icon
    m_button = std::make_unique<juce::DrawableButton>(
        m_config.id, juce::DrawableButton::ImageFitted);

    updateIcon();

    m_button->setTooltip(getTooltipText());
    m_button->setTitle(getTooltipText()); // Accessible name for screen readers (icon-only button)
    m_button->onClick = [this] { executeCommand(); };

    // Make DrawableButton click-through so parent ToolbarButton receives all mouse events
    // This fixes hover highlight blinking - the parent now handles mouseEnter/mouseExit
    m_button->setInterceptsMouseClicks(false, false);

    addAndMakeVisible(m_button.get());

    // Re-skin the icon when the active theme changes
    waveedit::ThemeManager::getInstance().addChangeListener(this);
}

ToolbarButton::~ToolbarButton()
{
    waveedit::ThemeManager::getInstance().removeChangeListener(this);
}

//==============================================================================
void ToolbarButton::paint(juce::Graphics& g)
{
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();

    // Separators and spacers don't use internal buttons
    if (m_config.type == ToolbarButtonType::SEPARATOR)
    {
        g.setColour(theme.border);
        int xCenter = getWidth() / 2;
        g.drawLine(static_cast<float>(xCenter), 4.0f,
                   static_cast<float>(xCenter), static_cast<float>(getHeight() - 4), 1.0f);
    }
    else if (m_config.type == ToolbarButtonType::COMMAND ||
             m_config.type == ToolbarButtonType::PLUGIN)
    {
        // Draw hover/pressed background for command and plugin buttons
        if (m_isPressed)
        {
            g.setColour(theme.accent);
            g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f);
        }
        else if (m_isHovered)
        {
            g.setColour(theme.selection);
            g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f);

            g.setColour(theme.focus);
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.5f), 4.0f, 1.0f);
        }
    }
    // Spacers are transparent - nothing to paint
}

void ToolbarButton::resized()
{
    if (m_button != nullptr)
        m_button->setBounds(getLocalBounds().reduced(2));
}

void ToolbarButton::mouseEnter(const juce::MouseEvent& /*event*/)
{
    m_isHovered = true;
    repaint();
}

void ToolbarButton::mouseExit(const juce::MouseEvent& /*event*/)
{
    // Always clear hover state on mouseExit - this ensures highlight
    // clears properly even when moving the mouse quickly between buttons
    m_isHovered = false;
    repaint();
}

void ToolbarButton::mouseDown(const juce::MouseEvent& event)
{
    // Forward right-click to parent for context menu
    if (event.mods.isPopupMenu())
    {
        if (auto* parent = getParentComponent())
        {
            // Convert to parent coordinates and forward the event
            auto parentEvent = event.getEventRelativeTo(parent);
            parent->mouseDown(parentEvent);
        }
        return;
    }

    m_isPressed = true;
    repaint();
}

void ToolbarButton::mouseUp(const juce::MouseEvent& /*event*/)
{
    m_isPressed = false;
    repaint();
}

//==============================================================================
juce::String ToolbarButton::getTooltip()
{
    return getTooltipText();
}

//==============================================================================
void ToolbarButton::executeCommand()
{
    if (m_config.type == ToolbarButtonType::COMMAND && m_commandManager != nullptr)
    {
        juce::CommandID cmdId = getCommandID();
        if (cmdId != 0)
        {
            m_commandManager->invokeDirectly(cmdId, true);
        }
    }
    else if (m_config.type == ToolbarButtonType::PLUGIN)
    {
        if (onPluginClick)
            onPluginClick(m_config.pluginIdentifier);
    }
}

void ToolbarButton::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &waveedit::ThemeManager::getInstance())
        updateIcon();
}

void ToolbarButton::updateIcon()
{
    if (m_button == nullptr)
        return;

    auto icon = createIconForCommand(m_config.commandName);
    if (icon != nullptr)
        m_button->setImages(icon.get());
}

std::unique_ptr<juce::Drawable> ToolbarButton::createIconForCommand(const juce::String& commandName)
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    juce::Path path;

    // Dispatch on the resolved CommandID rather than fragile substring
    // matching. This guarantees distinct commands map to distinct glyphs
    // and removes false-positive hits (e.g. "dc"/"new"/"open" inside
    // unrelated names). Plugin buttons carry no command name, so they
    // resolve to 0 and fall through to the plugin/generic glyph below.
    const juce::CommandID cmdId = getCommandIDForName(commandName);

    // Create 16x16 icons based on the command.
    switch (cmdId)
    {
        case CommandIDs::editUndo:
            // Undo: curved arrow left
            path.startNewSubPath(12.0f, 4.0f);
            path.cubicTo(8.0f, 4.0f, 4.0f, 6.0f, 4.0f, 10.0f);
            path.lineTo(4.0f, 12.0f);
            path.startNewSubPath(4.0f, 10.0f);
            path.lineTo(2.0f, 8.0f);
            path.lineTo(6.0f, 8.0f);
            break;

        case CommandIDs::editRedo:
            // Redo: curved arrow right
            path.startNewSubPath(4.0f, 4.0f);
            path.cubicTo(8.0f, 4.0f, 12.0f, 6.0f, 12.0f, 10.0f);
            path.lineTo(12.0f, 12.0f);
            path.startNewSubPath(12.0f, 10.0f);
            path.lineTo(14.0f, 8.0f);
            path.lineTo(10.0f, 8.0f);
            break;

        case CommandIDs::viewZoomIn:
            // Magnifier with +
            path.addEllipse(3.0f, 3.0f, 8.0f, 8.0f);
            path.startNewSubPath(10.0f, 10.0f);
            path.lineTo(14.0f, 14.0f);
            path.startNewSubPath(5.0f, 7.0f);
            path.lineTo(9.0f, 7.0f);
            path.startNewSubPath(7.0f, 5.0f);
            path.lineTo(7.0f, 9.0f);
            break;

        case CommandIDs::viewZoomOut:
            // Magnifier with -
            path.addEllipse(3.0f, 3.0f, 8.0f, 8.0f);
            path.startNewSubPath(10.0f, 10.0f);
            path.lineTo(14.0f, 14.0f);
            path.startNewSubPath(5.0f, 7.0f);
            path.lineTo(9.0f, 7.0f);
            break;

        case CommandIDs::viewZoomFit:
        case CommandIDs::viewZoomSelection:
            // Four arrows pointing outward
            path.startNewSubPath(2.0f, 2.0f);
            path.lineTo(5.0f, 2.0f);
            path.lineTo(2.0f, 5.0f);
            path.startNewSubPath(14.0f, 2.0f);
            path.lineTo(11.0f, 2.0f);
            path.lineTo(14.0f, 5.0f);
            path.startNewSubPath(2.0f, 14.0f);
            path.lineTo(5.0f, 14.0f);
            path.lineTo(2.0f, 11.0f);
            path.startNewSubPath(14.0f, 14.0f);
            path.lineTo(11.0f, 14.0f);
            path.lineTo(14.0f, 11.0f);
            break;

        case CommandIDs::processFadeIn:
            // Rising diagonal line
            path.startNewSubPath(2.0f, 14.0f);
            path.lineTo(14.0f, 2.0f);
            path.lineTo(14.0f, 14.0f);
            path.closeSubPath();
            break;

        case CommandIDs::processFadeOut:
            // Falling diagonal line
            path.startNewSubPath(2.0f, 2.0f);
            path.lineTo(14.0f, 14.0f);
            path.lineTo(2.0f, 14.0f);
            path.closeSubPath();
            break;

        case CommandIDs::processNormalize:
            // Waveform going to max
            path.startNewSubPath(2.0f, 8.0f);
            path.lineTo(4.0f, 4.0f);
            path.lineTo(6.0f, 12.0f);
            path.lineTo(8.0f, 2.0f);
            path.lineTo(10.0f, 14.0f);
            path.lineTo(12.0f, 4.0f);
            path.lineTo(14.0f, 8.0f);
            break;

        case CommandIDs::processGain:
        case CommandIDs::processIncreaseGain:
        case CommandIDs::processDecreaseGain:
            // dB meter
            path.addRectangle(4.0f, 6.0f, 3.0f, 8.0f);
            path.addRectangle(9.0f, 3.0f, 3.0f, 11.0f);
            break;

        case CommandIDs::editCut:
            // Scissors
            path.addEllipse(3.0f, 2.0f, 4.0f, 4.0f);
            path.addEllipse(9.0f, 2.0f, 4.0f, 4.0f);
            path.startNewSubPath(5.0f, 6.0f);
            path.lineTo(11.0f, 14.0f);
            path.startNewSubPath(11.0f, 6.0f);
            path.lineTo(5.0f, 14.0f);
            break;

        case CommandIDs::editCopy:
            // Two documents
            path.addRectangle(2.0f, 4.0f, 8.0f, 10.0f);
            path.addRectangle(6.0f, 2.0f, 8.0f, 10.0f);
            break;

        case CommandIDs::editPaste:
            // Clipboard
            path.addRectangle(3.0f, 4.0f, 10.0f, 10.0f);
            path.addRectangle(5.0f, 2.0f, 6.0f, 3.0f);
            break;

        case CommandIDs::editDelete:
            // X
            path.startNewSubPath(4.0f, 4.0f);
            path.lineTo(12.0f, 12.0f);
            path.startNewSubPath(12.0f, 4.0f);
            path.lineTo(4.0f, 12.0f);
            break;

        case CommandIDs::editTrim:
            // Crop marks
            path.startNewSubPath(2.0f, 6.0f);
            path.lineTo(2.0f, 2.0f);
            path.lineTo(6.0f, 2.0f);
            path.startNewSubPath(10.0f, 2.0f);
            path.lineTo(14.0f, 2.0f);
            path.lineTo(14.0f, 6.0f);
            path.startNewSubPath(14.0f, 10.0f);
            path.lineTo(14.0f, 14.0f);
            path.lineTo(10.0f, 14.0f);
            path.startNewSubPath(6.0f, 14.0f);
            path.lineTo(2.0f, 14.0f);
            path.lineTo(2.0f, 10.0f);
            break;

        case CommandIDs::editSilence:
            // Flat line
            path.startNewSubPath(2.0f, 8.0f);
            path.lineTo(14.0f, 8.0f);
            break;

        case CommandIDs::processGraphicalEQ:
            // EQ sliders
            path.addRectangle(3.0f, 4.0f, 2.0f, 10.0f);
            path.addRectangle(7.0f, 2.0f, 2.0f, 12.0f);
            path.addRectangle(11.0f, 6.0f, 2.0f, 8.0f);
            break;

        case CommandIDs::fileNew:
            // New document icon
            path.addRectangle(4.0f, 2.0f, 8.0f, 12.0f);
            path.startNewSubPath(4.0f, 2.0f);
            path.lineTo(9.0f, 2.0f);
            path.lineTo(12.0f, 5.0f);
            path.lineTo(12.0f, 14.0f);
            break;

        case CommandIDs::fileOpen:
            // Folder icon
            path.startNewSubPath(2.0f, 5.0f);
            path.lineTo(6.0f, 5.0f);
            path.lineTo(7.0f, 3.0f);
            path.lineTo(14.0f, 3.0f);
            path.lineTo(14.0f, 13.0f);
            path.lineTo(2.0f, 13.0f);
            path.closeSubPath();
            break;

        case CommandIDs::fileSave:
        case CommandIDs::fileSaveAs:
            // Floppy disk icon
            path.addRoundedRectangle(2.0f, 2.0f, 12.0f, 12.0f, 1.0f);
            path.addRectangle(4.0f, 2.0f, 8.0f, 5.0f);
            path.addRectangle(5.0f, 9.0f, 6.0f, 4.0f);
            break;

        case CommandIDs::processDCOffset:
            // DC offset - horizontal line with arrows up/down
            path.startNewSubPath(2.0f, 8.0f);
            path.lineTo(14.0f, 8.0f);
            path.startNewSubPath(6.0f, 4.0f);
            path.lineTo(8.0f, 2.0f);
            path.lineTo(10.0f, 4.0f);
            path.startNewSubPath(6.0f, 12.0f);
            path.lineTo(8.0f, 14.0f);
            path.lineTo(10.0f, 12.0f);
            break;

        case CommandIDs::pluginApplyChain:
            // Checkmark (apply)
            path.startNewSubPath(3.0f, 8.0f);
            path.lineTo(6.0f, 11.0f);
            path.lineTo(13.0f, 4.0f);
            break;

        case CommandIDs::pluginShowChain:
        case CommandIDs::pluginAddPlugin:
        case CommandIDs::pluginBypassAll:
            // Plug icon
            path.addRectangle(5.0f, 2.0f, 2.0f, 6.0f);
            path.addRectangle(9.0f, 2.0f, 2.0f, 6.0f);
            path.addRoundedRectangle(3.0f, 8.0f, 10.0f, 6.0f, 2.0f);
            break;

        default:
            // Unmapped commands and plugin buttons (no command name).
            // Plugin buttons get the plug glyph; everything else a
            // generic circle so distinct icons are at least visibly
            // present rather than silently identical.
            if (m_config.type == ToolbarButtonType::PLUGIN)
            {
                path.addRectangle(5.0f, 2.0f, 2.0f, 6.0f);
                path.addRectangle(9.0f, 2.0f, 2.0f, 6.0f);
                path.addRoundedRectangle(3.0f, 8.0f, 10.0f, 6.0f, 2.0f);
            }
            else
            {
                path.addEllipse(4.0f, 4.0f, 8.0f, 8.0f);
            }
            break;
    }

    drawable->setPath(path);
    drawable->setFill(juce::FillType());
    // Use the theme's text colour so icons recolour with the active
    // theme (white icons would vanish on the Light theme).
    drawable->setStrokeFill(waveedit::ThemeManager::getInstance().getCurrent().text);
    drawable->setStrokeThickness(1.5f);

    return drawable;
}

juce::String ToolbarButton::getTooltipText() const
{
    if (m_config.tooltip.isNotEmpty())
        return m_config.tooltip;

    // Generate tooltip from command name
    juce::String tooltip = m_config.commandName;

    // Remove prefix and convert to readable text
    if (tooltip.startsWith("process"))
        tooltip = tooltip.substring(7);
    else if (tooltip.startsWith("view"))
        tooltip = tooltip.substring(4);
    else if (tooltip.startsWith("edit"))
        tooltip = tooltip.substring(4);
    else if (tooltip.startsWith("file"))
        tooltip = tooltip.substring(4);
    else if (tooltip.startsWith("plugin"))
        tooltip = tooltip.substring(6);

    // Add spaces before capitals
    juce::String formatted;
    for (int i = 0; i < tooltip.length(); ++i)
    {
        juce::juce_wchar c = tooltip[i];
        if (i > 0 && juce::CharacterFunctions::isUpperCase(c))
            formatted += " ";
        formatted += juce::String::charToString(c);
    }

    return formatted;
}

juce::CommandID ToolbarButton::getCommandID() const
{
    return getCommandIDForName(m_config.commandName);
}

//==============================================================================
// ToolbarSeparator

ToolbarSeparator::ToolbarSeparator(int width)
{
    setSize(width, 36);
    // Make separator click-through so right-clicks pass to parent toolbar
    setInterceptsMouseClicks(false, false);
}

void ToolbarSeparator::paint(juce::Graphics& g)
{
    g.setColour(waveedit::ThemeManager::getInstance().getCurrent().border);
    int xCenter = getWidth() / 2;
    g.drawLine(static_cast<float>(xCenter), 4.0f,
               static_cast<float>(xCenter), static_cast<float>(getHeight() - 4), 1.0f);
}

//==============================================================================
// ToolbarSpacer

ToolbarSpacer::ToolbarSpacer(int minWidth)
{
    setSize(minWidth, 36);
    // Make spacer click-through so right-clicks pass to parent toolbar
    setInterceptsMouseClicks(false, false);
}

void ToolbarSpacer::paint(juce::Graphics& /*g*/)
{
    // Spacers are transparent
}
