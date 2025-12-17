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
        commandMap["processParametricEQ"] = CommandIDs::processParametricEQ;
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

    auto icon = createIconForCommand(m_config.commandName);
    if (icon != nullptr)
        m_button->setImages(icon.get());

    m_button->setTooltip(getTooltipText());
    m_button->onClick = [this] { executeCommand(); };

    // Make DrawableButton click-through so parent ToolbarButton receives all mouse events
    // This fixes hover highlight blinking - the parent now handles mouseEnter/mouseExit
    m_button->setInterceptsMouseClicks(false, false);

    addAndMakeVisible(m_button.get());
}

ToolbarButton::~ToolbarButton()
{
}

//==============================================================================
void ToolbarButton::paint(juce::Graphics& g)
{
    // Separators and spacers don't use internal buttons
    if (m_config.type == ToolbarButtonType::SEPARATOR)
    {
        g.setColour(juce::Colour(0xff4a4a4a));
        int xCenter = getWidth() / 2;
        g.drawLine(static_cast<float>(xCenter), 4.0f,
                   static_cast<float>(xCenter), static_cast<float>(getHeight() - 4), 1.0f);
    }
    else if (m_config.type == ToolbarButtonType::COMMAND ||
             m_config.type == ToolbarButtonType::PLUGIN)
    {
        // Draw hover/pressed background for command and plugin buttons
        // Using high-contrast colors for accessibility (WCAG AA compliance)
        if (m_isPressed)
        {
            // Bright blue pressed color - high visibility
            g.setColour(juce::Colour(0xff4a90d9));
            g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f);
        }
        else if (m_isHovered)
        {
            // High-contrast hover highlight - much more visible than before
            // Using a lighter gray (#5a5a5a) on dark background (#2D2D30) for 3:1+ contrast ratio
            g.setColour(juce::Colour(0xff5a5a5a));
            g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 4.0f);

            // Add a subtle border for extra visibility
            g.setColour(juce::Colour(0xff6a6a6a));
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

std::unique_ptr<juce::Drawable> ToolbarButton::createIconForCommand(const juce::String& commandName)
{
    auto drawable = std::make_unique<juce::DrawablePath>();
    juce::Path path;

    // Create 16x16 icons based on command type
    if (commandName.containsIgnoreCase("undo"))
    {
        // Undo: curved arrow left
        path.startNewSubPath(12.0f, 4.0f);
        path.cubicTo(8.0f, 4.0f, 4.0f, 6.0f, 4.0f, 10.0f);
        path.lineTo(4.0f, 12.0f);
        path.startNewSubPath(4.0f, 10.0f);
        path.lineTo(2.0f, 8.0f);
        path.lineTo(6.0f, 8.0f);
    }
    else if (commandName.containsIgnoreCase("redo"))
    {
        // Redo: curved arrow right
        path.startNewSubPath(4.0f, 4.0f);
        path.cubicTo(8.0f, 4.0f, 12.0f, 6.0f, 12.0f, 10.0f);
        path.lineTo(12.0f, 12.0f);
        path.startNewSubPath(12.0f, 10.0f);
        path.lineTo(14.0f, 8.0f);
        path.lineTo(10.0f, 8.0f);
    }
    else if (commandName.containsIgnoreCase("zoomIn"))
    {
        // Magnifier with +
        path.addEllipse(3.0f, 3.0f, 8.0f, 8.0f);
        path.startNewSubPath(10.0f, 10.0f);
        path.lineTo(14.0f, 14.0f);
        path.startNewSubPath(5.0f, 7.0f);
        path.lineTo(9.0f, 7.0f);
        path.startNewSubPath(7.0f, 5.0f);
        path.lineTo(7.0f, 9.0f);
    }
    else if (commandName.containsIgnoreCase("zoomOut"))
    {
        // Magnifier with -
        path.addEllipse(3.0f, 3.0f, 8.0f, 8.0f);
        path.startNewSubPath(10.0f, 10.0f);
        path.lineTo(14.0f, 14.0f);
        path.startNewSubPath(5.0f, 7.0f);
        path.lineTo(9.0f, 7.0f);
    }
    else if (commandName.containsIgnoreCase("zoomFit") || commandName.containsIgnoreCase("zoomSelection"))
    {
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
    }
    else if (commandName.containsIgnoreCase("fadeIn"))
    {
        // Rising diagonal line
        path.startNewSubPath(2.0f, 14.0f);
        path.lineTo(14.0f, 2.0f);
        path.lineTo(14.0f, 14.0f);
        path.closeSubPath();
    }
    else if (commandName.containsIgnoreCase("fadeOut"))
    {
        // Falling diagonal line
        path.startNewSubPath(2.0f, 2.0f);
        path.lineTo(14.0f, 14.0f);
        path.lineTo(2.0f, 14.0f);
        path.closeSubPath();
    }
    else if (commandName.containsIgnoreCase("normalize"))
    {
        // Waveform going to max
        path.startNewSubPath(2.0f, 8.0f);
        path.lineTo(4.0f, 4.0f);
        path.lineTo(6.0f, 12.0f);
        path.lineTo(8.0f, 2.0f);
        path.lineTo(10.0f, 14.0f);
        path.lineTo(12.0f, 4.0f);
        path.lineTo(14.0f, 8.0f);
    }
    else if (commandName.containsIgnoreCase("gain"))
    {
        // dB meter
        path.addRectangle(4.0f, 6.0f, 3.0f, 8.0f);
        path.addRectangle(9.0f, 3.0f, 3.0f, 11.0f);
    }
    else if (commandName.containsIgnoreCase("cut"))
    {
        // Scissors
        path.addEllipse(3.0f, 2.0f, 4.0f, 4.0f);
        path.addEllipse(9.0f, 2.0f, 4.0f, 4.0f);
        path.startNewSubPath(5.0f, 6.0f);
        path.lineTo(11.0f, 14.0f);
        path.startNewSubPath(11.0f, 6.0f);
        path.lineTo(5.0f, 14.0f);
    }
    else if (commandName.containsIgnoreCase("copy"))
    {
        // Two documents
        path.addRectangle(2.0f, 4.0f, 8.0f, 10.0f);
        path.addRectangle(6.0f, 2.0f, 8.0f, 10.0f);
    }
    else if (commandName.containsIgnoreCase("paste"))
    {
        // Clipboard
        path.addRectangle(3.0f, 4.0f, 10.0f, 10.0f);
        path.addRectangle(5.0f, 2.0f, 6.0f, 3.0f);
    }
    else if (commandName.containsIgnoreCase("delete"))
    {
        // X
        path.startNewSubPath(4.0f, 4.0f);
        path.lineTo(12.0f, 12.0f);
        path.startNewSubPath(12.0f, 4.0f);
        path.lineTo(4.0f, 12.0f);
    }
    else if (commandName.containsIgnoreCase("trim"))
    {
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
    }
    else if (commandName.containsIgnoreCase("silence"))
    {
        // Flat line
        path.startNewSubPath(2.0f, 8.0f);
        path.lineTo(14.0f, 8.0f);
    }
    else if (commandName.containsIgnoreCase("plugin"))
    {
        // Plug icon
        path.addRectangle(5.0f, 2.0f, 2.0f, 6.0f);
        path.addRectangle(9.0f, 2.0f, 2.0f, 6.0f);
        path.addRoundedRectangle(3.0f, 8.0f, 10.0f, 6.0f, 2.0f);
    }
    else if (commandName.containsIgnoreCase("EQ"))
    {
        // EQ sliders
        path.addRectangle(3.0f, 4.0f, 2.0f, 10.0f);
        path.addRectangle(7.0f, 2.0f, 2.0f, 12.0f);
        path.addRectangle(11.0f, 6.0f, 2.0f, 8.0f);
    }
    else if (commandName.containsIgnoreCase("fileNew") || commandName.containsIgnoreCase("new"))
    {
        // New document icon
        path.addRectangle(4.0f, 2.0f, 8.0f, 12.0f);
        path.startNewSubPath(4.0f, 2.0f);
        path.lineTo(9.0f, 2.0f);
        path.lineTo(12.0f, 5.0f);
        path.lineTo(12.0f, 14.0f);
    }
    else if (commandName.containsIgnoreCase("fileOpen") || commandName.containsIgnoreCase("open"))
    {
        // Folder icon
        path.startNewSubPath(2.0f, 5.0f);
        path.lineTo(6.0f, 5.0f);
        path.lineTo(7.0f, 3.0f);
        path.lineTo(14.0f, 3.0f);
        path.lineTo(14.0f, 13.0f);
        path.lineTo(2.0f, 13.0f);
        path.closeSubPath();
    }
    else if (commandName.containsIgnoreCase("fileSave") || commandName.containsIgnoreCase("save"))
    {
        // Floppy disk icon
        path.addRoundedRectangle(2.0f, 2.0f, 12.0f, 12.0f, 1.0f);
        path.addRectangle(4.0f, 2.0f, 8.0f, 5.0f);
        path.addRectangle(5.0f, 9.0f, 6.0f, 4.0f);
    }
    else if (commandName.containsIgnoreCase("DCOffset") || commandName.containsIgnoreCase("dc"))
    {
        // DC offset - horizontal line with arrows up/down
        path.startNewSubPath(2.0f, 8.0f);
        path.lineTo(14.0f, 8.0f);
        path.startNewSubPath(6.0f, 4.0f);
        path.lineTo(8.0f, 2.0f);
        path.lineTo(10.0f, 4.0f);
        path.startNewSubPath(6.0f, 12.0f);
        path.lineTo(8.0f, 14.0f);
        path.lineTo(10.0f, 12.0f);
    }
    else if (commandName.containsIgnoreCase("apply"))
    {
        // Checkmark
        path.startNewSubPath(3.0f, 8.0f);
        path.lineTo(6.0f, 11.0f);
        path.lineTo(13.0f, 4.0f);
    }
    else if (commandName.containsIgnoreCase("offline"))
    {
        // Render/process icon (gear)
        path.addEllipse(4.0f, 4.0f, 8.0f, 8.0f);
        path.startNewSubPath(8.0f, 2.0f);
        path.lineTo(8.0f, 4.0f);
        path.startNewSubPath(8.0f, 12.0f);
        path.lineTo(8.0f, 14.0f);
        path.startNewSubPath(2.0f, 8.0f);
        path.lineTo(4.0f, 8.0f);
        path.startNewSubPath(12.0f, 8.0f);
        path.lineTo(14.0f, 8.0f);
    }
    else
    {
        // Default: generic circle button
        path.addEllipse(4.0f, 4.0f, 8.0f, 8.0f);
    }

    drawable->setPath(path);
    drawable->setFill(juce::FillType());
    drawable->setStrokeFill(juce::Colours::white);
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
    : m_width(width)
{
    setSize(width, 36);
    // Make separator click-through so right-clicks pass to parent toolbar
    setInterceptsMouseClicks(false, false);
}

void ToolbarSeparator::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xff4a4a4a));
    int xCenter = getWidth() / 2;
    g.drawLine(static_cast<float>(xCenter), 4.0f,
               static_cast<float>(xCenter), static_cast<float>(getHeight() - 4), 1.0f);
}

//==============================================================================
// ToolbarSpacer

ToolbarSpacer::ToolbarSpacer(int minWidth)
    : m_minWidth(minWidth)
{
    setSize(minWidth, 36);
    // Make spacer click-through so right-clicks pass to parent toolbar
    setInterceptsMouseClicks(false, false);
}

void ToolbarSpacer::paint(juce::Graphics& /*g*/)
{
    // Spacers are transparent
}
