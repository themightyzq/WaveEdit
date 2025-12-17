/*
  ==============================================================================

    PluginEditorWindow.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "PluginEditorWindow.h"

// Static member initialization
juce::Array<PluginEditorWindow*> PluginEditorWindow::s_openWindows;

//==============================================================================
// ContentComponent
//==============================================================================

PluginEditorWindow::ContentComponent::ContentComponent(
    PluginEditorWindow& owner,
    std::unique_ptr<juce::AudioProcessorEditor> editor)
    : m_owner(owner), m_editor(std::move(editor))
{
    // Bypass toggle button
    m_bypassButton.setButtonText("Bypass");
    m_bypassButton.setTooltip("Bypass this plugin (audio passes through unchanged)");
    m_bypassButton.onClick = [this]()
    {
        if (m_owner.m_node != nullptr)
        {
            m_owner.m_node->setBypassed(m_bypassButton.getToggleState());
        }
    };
    addAndMakeVisible(m_bypassButton);

    // Latency label
    m_latencyLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa0a0a0));
    m_latencyLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(m_latencyLabel);

    // Add the editor
    if (m_editor != nullptr)
    {
        addAndMakeVisible(m_editor.get());
    }

    // Update initial state
    updateBypassState();

    // Set size based on editor
    if (m_editor != nullptr)
    {
        setSize(juce::jmax(300, m_editor->getWidth()),
                kToolbarHeight + m_editor->getHeight());
    }
    else
    {
        setSize(400, 300);
    }
}

PluginEditorWindow::ContentComponent::~ContentComponent()
{
    m_editor = nullptr;
}

void PluginEditorWindow::ContentComponent::paint(juce::Graphics& g)
{
    // Toolbar background
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(0, 0, getWidth(), kToolbarHeight);

    // Separator line
    g.setColour(juce::Colour(0xff404040));
    g.drawHorizontalLine(kToolbarHeight - 1, 0.0f, static_cast<float>(getWidth()));
}

void PluginEditorWindow::ContentComponent::resized()
{
    auto bounds = getLocalBounds();

    // Toolbar area
    auto toolbar = bounds.removeFromTop(kToolbarHeight).reduced(4, 2);
    m_bypassButton.setBounds(toolbar.removeFromLeft(80));
    m_latencyLabel.setBounds(toolbar.removeFromRight(150));

    // Editor fills rest
    if (m_editor != nullptr)
    {
        m_editor->setBounds(bounds);
    }
}

void PluginEditorWindow::ContentComponent::updateBypassState()
{
    if (m_owner.m_node != nullptr)
    {
        m_bypassButton.setToggleState(m_owner.m_node->isBypassed(), juce::dontSendNotification);

        int latency = m_owner.m_node->getLatencySamples();
        if (latency > 0)
        {
            m_latencyLabel.setText("Latency: " + juce::String(latency) + " samples",
                                    juce::dontSendNotification);
        }
        else
        {
            m_latencyLabel.setText("", juce::dontSendNotification);
        }
    }
}

//==============================================================================
// PluginEditorWindow
//==============================================================================

PluginEditorWindow::PluginEditorWindow(PluginChainNode* node,
                                       juce::ApplicationCommandManager* commandManager)
    : juce::DocumentWindow(
          node != nullptr ? node->getName() : "Plugin Editor",
          juce::Colour(0xff1e1e1e),
          juce::DocumentWindow::allButtons),
      m_node(node),
      m_commandManager(commandManager),
      m_mainCommandTarget(nullptr)
{
    jassert(m_node != nullptr);

    // Set up keyboard shortcut routing to main application
    if (m_commandManager != nullptr)
    {
        addKeyListener(m_commandManager->getKeyMappings());
        m_mainCommandTarget = m_commandManager->getFirstCommandTarget(0);
    }

    // Create the editor
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    if (m_node->hasEditor())
    {
        editor = m_node->createEditor();
    }

    // If no custom editor, create a generic one
    if (editor == nullptr)
    {
        auto* plugin = m_node->getPlugin();
        if (plugin != nullptr)
        {
            editor = std::make_unique<juce::GenericAudioProcessorEditor>(*plugin);
        }
    }

    // Create content component
    m_content = new ContentComponent(*this, std::move(editor));
    setContentOwned(m_content, true);

    setResizable(true, true);
    setUsingNativeTitleBar(true);
    centreWithSize(getWidth(), getHeight());

    // Register this window
    s_openWindows.add(this);
}

PluginEditorWindow::~PluginEditorWindow()
{
    // Clean up the key listener
    if (m_commandManager != nullptr)
        removeKeyListener(m_commandManager->getKeyMappings());

    s_openWindows.removeFirstMatchingValue(this);
}

void PluginEditorWindow::updateBypassState()
{
    if (m_content != nullptr)
    {
        m_content->updateBypassState();
    }
}

void PluginEditorWindow::closeButtonPressed()
{
    delete this;
}

//==============================================================================
// ApplicationCommandTarget overrides

juce::ApplicationCommandTarget* PluginEditorWindow::getNextCommandTarget()
{
    // Chain to MainComponent so it can handle all commands
    return m_mainCommandTarget;
}

void PluginEditorWindow::getAllCommands(juce::Array<juce::CommandID>& commands)
{
    // We don't define our own commands - they're all in MainComponent
    juce::ignoreUnused(commands);
}

void PluginEditorWindow::getCommandInfo(juce::CommandID /*commandID*/, juce::ApplicationCommandInfo& /*result*/)
{
    // We don't define command info - MainComponent does
}

bool PluginEditorWindow::perform(const InvocationInfo& /*info*/)
{
    // We don't handle any commands ourselves
    // Return false so JUCE calls getNextCommandTarget() and tries MainComponent
    return false;
}

//==============================================================================
// Static methods

PluginEditorWindow* PluginEditorWindow::showForNode(PluginChainNode* node,
                                                     juce::ApplicationCommandManager* commandManager)
{
    if (node == nullptr)
    {
        return nullptr;
    }

    // Check if already open
    for (auto* window : s_openWindows)
    {
        if (window->isForNode(node))
        {
            window->toFront(true);
            return window;
        }
    }

    // Create new window
    auto* window = new PluginEditorWindow(node, commandManager);
    window->setVisible(true);
    return window;
}

void PluginEditorWindow::closeAll()
{
    // Copy array since deletion modifies it
    auto windows = s_openWindows;
    for (auto* window : windows)
    {
        delete window;
    }
}

void PluginEditorWindow::closeForNode(PluginChainNode* node)
{
    for (int i = s_openWindows.size() - 1; i >= 0; --i)
    {
        if (s_openWindows[i]->isForNode(node))
        {
            delete s_openWindows[i];
            return;
        }
    }
}
