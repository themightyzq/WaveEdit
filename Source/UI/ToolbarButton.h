/*
  ==============================================================================

    ToolbarButton.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/ToolbarConfig.h"

/**
 * Individual toolbar button that executes a command when clicked.
 *
 * Supports different button types:
 * - COMMAND: Invokes a CommandID via ApplicationCommandManager
 * - PLUGIN: Opens a specific plugin
 * - SEPARATOR: Visual separator (no interaction)
 * - SPACER: Flexible space (no interaction)
 *
 * The TRANSPORT type is handled by CustomizableToolbar directly
 * using the CompactTransport component.
 *
 * Inherits from TooltipClient to show tooltips on hover.
 */
class ToolbarButton : public juce::Component,
                      public juce::TooltipClient
{
public:
    /**
     * Constructor.
     *
     * @param config Button configuration
     * @param commandManager Reference to command manager for executing commands
     */
    ToolbarButton(const ToolbarButtonConfig& config,
                  juce::ApplicationCommandManager* commandManager);

    ~ToolbarButton() override;

    //==============================================================================
    // Component Overrides

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;

    //==============================================================================
    // TooltipClient Override - shows tooltip for the parent component

    juce::String getTooltip() override;

    //==============================================================================
    // Accessors

    const ToolbarButtonConfig& getConfig() const { return m_config; }
    juce::String getButtonId() const { return m_config.id; }
    ToolbarButtonType getType() const { return m_config.type; }

    /**
     * Set callback for plugin button clicks.
     */
    std::function<void(const juce::String& pluginId)> onPluginClick;

private:
    //==============================================================================
    // Private Members

    ToolbarButtonConfig m_config;
    juce::ApplicationCommandManager* m_commandManager;

    std::unique_ptr<juce::DrawableButton> m_button;
    bool m_isHovered;
    bool m_isPressed;

    //==============================================================================
    // Private Methods

    void executeCommand();

    /**
     * Create drawable icon for the button based on command or custom icon.
     */
    std::unique_ptr<juce::Drawable> createIconForCommand(const juce::String& commandName);

    /**
     * Get tooltip text for the button.
     */
    juce::String getTooltipText() const;

    /**
     * Get the CommandID for the configured command name.
     */
    juce::CommandID getCommandID() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolbarButton)
};

/**
 * Toolbar separator component - visual divider between button groups.
 */
class ToolbarSeparator : public juce::Component
{
public:
    ToolbarSeparator(int width = 8);
    void paint(juce::Graphics& g) override;

private:
    int m_width;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolbarSeparator)
};

/**
 * Toolbar spacer component - flexible space that expands.
 */
class ToolbarSpacer : public juce::Component
{
public:
    ToolbarSpacer(int minWidth = 16);
    void paint(juce::Graphics& g) override;

private:
    int m_minWidth;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToolbarSpacer)
};
