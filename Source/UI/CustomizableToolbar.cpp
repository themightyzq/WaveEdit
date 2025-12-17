/*
  ==============================================================================

    CustomizableToolbar.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "CustomizableToolbar.h"
#include "../Utils/Document.h"
#include "../Audio/AudioEngine.h"
#include "WaveformDisplay.h"

//==============================================================================
CustomizableToolbar::CustomizableToolbar(juce::ApplicationCommandManager& commandManager,
                                         ToolbarManager& toolbarManager)
    : m_commandManager(commandManager)
    , m_toolbarManager(toolbarManager)
{
    // Enable mouse interception for the toolbar to receive right-click context menu events
    // Child components (buttons, separators, spacers) are set to click-through so events bubble up
    setInterceptsMouseClicks(true, false);

    // Register for layout changes
    m_toolbarManager.addListener(this);

    // Load current layout
    loadLayout(m_toolbarManager.getCurrentLayout());

    juce::Logger::writeToLog("CustomizableToolbar: Initialized");
}

CustomizableToolbar::~CustomizableToolbar()
{
    m_toolbarManager.removeListener(this);
}

//==============================================================================
void CustomizableToolbar::paint(juce::Graphics& g)
{
    // Draw toolbar background
    g.fillAll(juce::Colour(0xFF2D2D30));

    // Draw bottom border
    g.setColour(juce::Colour(0xFF1E1E1E));
    g.drawLine(0.0f, static_cast<float>(getHeight() - 1),
               static_cast<float>(getWidth()), static_cast<float>(getHeight() - 1), 1.0f);

    // Draw drag indicator during drag-drop
    if (m_isDragging && m_dragInsertIndex >= 0)
    {
        int insertX = 4;
        int componentIndex = 0;

        for (auto* comp : m_buttonComponents)
        {
            if (componentIndex == m_dragInsertIndex)
                break;
            insertX = comp->getRight() + 2;
            componentIndex++;
        }

        g.setColour(juce::Colours::dodgerblue);
        g.fillRect(insertX - 1, 4, 2, getHeight() - 8);
    }
}

void CustomizableToolbar::resized()
{
    layoutButtons();
}

void CustomizableToolbar::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu())
    {
        showContextMenu(event.getScreenPosition());
    }
}

//==============================================================================
bool CustomizableToolbar::isInterestedInDragSource(const SourceDetails& details)
{
    // Accept drag from toolbar buttons
    return details.description.toString().startsWith("ToolbarButton:");
}

void CustomizableToolbar::itemDragEnter(const SourceDetails& /*details*/)
{
    m_isDragging = true;
    repaint();
}

void CustomizableToolbar::itemDragMove(const SourceDetails& details)
{
    m_dragInsertIndex = getInsertIndexForPosition(details.localPosition.x);
    repaint();
}

void CustomizableToolbar::itemDragExit(const SourceDetails& /*details*/)
{
    m_isDragging = false;
    m_dragInsertIndex = -1;
    repaint();
}

void CustomizableToolbar::itemDropped(const SourceDetails& details)
{
    m_isDragging = false;

    juce::String dragDescription = details.description.toString();
    if (!dragDescription.startsWith("ToolbarButton:"))
    {
        m_dragInsertIndex = -1;
        repaint();
        return;
    }

    // Extract button ID from drag description
    juce::String buttonId = dragDescription.fromFirstOccurrenceOf("ToolbarButton:", false, false);

    // Find current index of dragged button
    int sourceIndex = -1;
    for (int i = 0; i < static_cast<int>(m_currentLayout.buttons.size()); ++i)
    {
        if (m_currentLayout.buttons[static_cast<size_t>(i)].id == buttonId)
        {
            sourceIndex = i;
            break;
        }
    }

    if (sourceIndex >= 0 && m_dragInsertIndex >= 0 && sourceIndex != m_dragInsertIndex)
    {
        // Reorder buttons in layout
        auto movedButton = m_currentLayout.buttons[static_cast<size_t>(sourceIndex)];
        m_currentLayout.buttons.erase(m_currentLayout.buttons.begin() + sourceIndex);

        int adjustedInsertIndex = m_dragInsertIndex;
        if (sourceIndex < m_dragInsertIndex)
            adjustedInsertIndex--;

        m_currentLayout.buttons.insert(
            m_currentLayout.buttons.begin() + adjustedInsertIndex,
            movedButton);

        // Rebuild toolbar with new order
        rebuildToolbar();

        juce::Logger::writeToLog("CustomizableToolbar: Reordered button '" + buttonId +
                                 "' from " + juce::String(sourceIndex) +
                                 " to " + juce::String(adjustedInsertIndex));
    }

    m_dragInsertIndex = -1;
    repaint();
}

//==============================================================================
void CustomizableToolbar::toolbarLayoutChanged(const ToolbarLayout& newLayout)
{
    loadLayout(newLayout);
}

//==============================================================================
void CustomizableToolbar::loadLayout(const ToolbarLayout& layout)
{
    m_currentLayout = layout;
    rebuildToolbar();
    setSize(getWidth(), layout.height);

    // Force immediate visual update
    repaint();

    juce::Logger::writeToLog("CustomizableToolbar: Loaded layout '" + layout.name + "'");
}

void CustomizableToolbar::setDocument(Document* doc)
{
    m_currentDocument = doc;

    // Update compact transport with document's audio engine and waveform display
    if (m_compactTransport != nullptr && doc != nullptr)
    {
        m_compactTransport->setAudioEngine(&doc->getAudioEngine());
        m_compactTransport->setWaveformDisplay(&doc->getWaveformDisplay());
    }
    else if (m_compactTransport != nullptr)
    {
        m_compactTransport->setAudioEngine(nullptr);
        m_compactTransport->setWaveformDisplay(nullptr);
    }
}

//==============================================================================
void CustomizableToolbar::rebuildToolbar()
{
    // Clear existing components
    m_buttonComponents.clear();
    m_compactTransport.reset();

    // Create components for each button in layout
    for (const auto& buttonConfig : m_currentLayout.buttons)
    {
        if (buttonConfig.type == ToolbarButtonType::TRANSPORT)
        {
            // Create compact transport widget
            m_compactTransport = std::make_unique<CompactTransport>();
            addAndMakeVisible(m_compactTransport.get());

            // Set document context if available
            if (m_currentDocument != nullptr)
            {
                m_compactTransport->setAudioEngine(&m_currentDocument->getAudioEngine());
                m_compactTransport->setWaveformDisplay(&m_currentDocument->getWaveformDisplay());
            }
        }
        else
        {
            // Create regular button component
            auto* comp = createButtonComponent(buttonConfig);
            if (comp != nullptr)
            {
                addAndMakeVisible(comp);
                m_buttonComponents.add(comp);
            }
        }
    }

    // Layout components
    layoutButtons();
}

juce::Component* CustomizableToolbar::createButtonComponent(const ToolbarButtonConfig& config)
{
    switch (config.type)
    {
        case ToolbarButtonType::COMMAND:
        case ToolbarButtonType::PLUGIN:
        {
            auto* button = new ToolbarButton(config, &m_commandManager);

            // Set plugin click callback
            if (config.type == ToolbarButtonType::PLUGIN)
            {
                button->onPluginClick = [this](const juce::String& pluginId)
                {
                    handlePluginButtonClick(pluginId);
                };
            }

            return button;
        }

        case ToolbarButtonType::SEPARATOR:
            return new ToolbarSeparator(config.width > 0 ? config.width : 8);

        case ToolbarButtonType::SPACER:
            return new ToolbarSpacer(config.width > 0 ? config.width : 16);

        case ToolbarButtonType::TRANSPORT:
            // Handled separately
            return nullptr;

        default:
            return nullptr;
    }
}

void CustomizableToolbar::layoutButtons()
{
    auto bounds = getLocalBounds().reduced(4, 2);

    // Layout transport first if present
    int transportIndex = -1;
    for (int i = 0; i < static_cast<int>(m_currentLayout.buttons.size()); ++i)
    {
        if (m_currentLayout.buttons[static_cast<size_t>(i)].type == ToolbarButtonType::TRANSPORT)
        {
            transportIndex = i;
            break;
        }
    }

    // First pass: calculate total fixed width and count spacers
    int totalFixedWidth = 0;
    int spacerCount = 0;

    for (size_t i = 0; i < m_currentLayout.buttons.size(); ++i)
    {
        const auto& config = m_currentLayout.buttons[i];

        if (config.type == ToolbarButtonType::TRANSPORT)
        {
            totalFixedWidth += config.width > 0 ? config.width : CompactTransport::kPreferredWidth;
        }
        else if (config.type == ToolbarButtonType::SPACER)
        {
            spacerCount++;
            totalFixedWidth += config.width > 0 ? config.width : 16; // Minimum spacer width
        }
        else if (config.type == ToolbarButtonType::SEPARATOR)
        {
            totalFixedWidth += config.width > 0 ? config.width : 8;
        }
        else
        {
            totalFixedWidth += config.width > 0 ? config.width : 28;
        }
    }

    // Calculate spacer expansion
    int remainingWidth = bounds.getWidth() - totalFixedWidth;
    int spacerExpansion = spacerCount > 0 ? juce::jmax(0, remainingWidth / spacerCount) : 0;

    // Second pass: layout components
    int x = bounds.getX();
    int buttonIndex = 0;

    for (size_t i = 0; i < m_currentLayout.buttons.size(); ++i)
    {
        const auto& config = m_currentLayout.buttons[i];

        if (config.type == ToolbarButtonType::TRANSPORT)
        {
            if (m_compactTransport != nullptr)
            {
                int width = config.width > 0 ? config.width : CompactTransport::kPreferredWidth;
                m_compactTransport->setBounds(x, bounds.getY(), width, bounds.getHeight());
                x += width + 2;
            }
        }
        else if (buttonIndex < m_buttonComponents.size())
        {
            auto* comp = m_buttonComponents[buttonIndex];
            int width = config.width > 0 ? config.width : 28;

            if (config.type == ToolbarButtonType::SPACER)
            {
                width = (config.width > 0 ? config.width : 16) + spacerExpansion;
            }
            else if (config.type == ToolbarButtonType::SEPARATOR)
            {
                width = config.width > 0 ? config.width : 8;
            }

            comp->setBounds(x, bounds.getY(), width, bounds.getHeight());
            x += width + 2;
            buttonIndex++;
        }
    }
}

int CustomizableToolbar::getInsertIndexForPosition(int x) const
{
    int componentX = 4;
    int index = 0;

    // Check transport position
    if (m_compactTransport != nullptr)
    {
        int midPoint = m_compactTransport->getX() + m_compactTransport->getWidth() / 2;
        if (x < midPoint)
            return index;
        componentX = m_compactTransport->getRight() + 2;
        index++;
    }

    // Check button positions
    for (auto* comp : m_buttonComponents)
    {
        int midPoint = comp->getX() + comp->getWidth() / 2;
        if (x < midPoint)
            return index;
        componentX = comp->getRight() + 2;
        index++;
    }

    return index;
}

void CustomizableToolbar::showContextMenu(juce::Point<int> screenPosition)
{
    juce::PopupMenu menu;

    // Layout selection submenu
    juce::PopupMenu layoutMenu;
    auto layouts = m_toolbarManager.getAvailableLayouts();
    juce::String currentLayout = m_toolbarManager.getCurrentLayoutName();

    for (int i = 0; i < layouts.size(); ++i)
    {
        bool isTicked = layouts[i] == currentLayout;
        layoutMenu.addItem(100 + i, layouts[i], true, isTicked);
    }

    menu.addSubMenu("Layout", layoutMenu);
    menu.addSeparator();

    // Customization options
    menu.addItem(1, "Customize Toolbar...");
    menu.addItem(2, "Reset to Default");

    // Show menu at mouse position (not at component location)
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screenPosition.x, screenPosition.y, 1, 1)),
        [this, layouts](int result)
        {
            if (result == 1)
            {
                // Invoke customize toolbar command
                m_commandManager.invokeDirectly(0xE000, false); // toolbarCustomize
            }
            else if (result == 2)
            {
                // Reset to default
                m_toolbarManager.loadLayout("Default");
            }
            else if (result >= 100)
            {
                // Layout selection
                int layoutIndex = result - 100;
                if (layoutIndex >= 0 && layoutIndex < layouts.size())
                {
                    m_toolbarManager.loadLayout(layouts[layoutIndex]);
                }
            }
        });
}

void CustomizableToolbar::handlePluginButtonClick(const juce::String& pluginId)
{
    if (onPluginClick)
    {
        onPluginClick(pluginId);
    }
    else
    {
        juce::Logger::writeToLog("CustomizableToolbar: Plugin button clicked: " + pluginId +
                                 " (no handler registered)");
    }
}
