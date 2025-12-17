/*
  ==============================================================================

    PluginChainPanel.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "PluginChainPanel.h"

//==============================================================================
// PluginRowComponent
//==============================================================================

PluginChainPanel::PluginRowComponent::PluginRowComponent(PluginChainPanel& owner)
    : m_owner(owner)
{
    m_bypassButton.setButtonText("Bypass");
    m_bypassButton.setTooltip("Bypass this plugin (disable effect processing)");
    m_bypassButton.setClickingTogglesState(true);
    m_bypassButton.onClick = [this]()
    {
        if (m_node != nullptr && m_owner.m_listener != nullptr)
        {
            bool isBypassed = m_bypassButton.getToggleState();
            m_owner.m_listener->pluginChainPanelBypassPlugin(m_index, isBypassed);
            updateBypassButtonAppearance(isBypassed);
        }
    };
    addAndMakeVisible(m_bypassButton);

    m_editButton.setButtonText("Edit");
    m_editButton.setTooltip("Open plugin editor");
    m_editButton.onClick = [this]()
    {
        if (m_owner.m_listener != nullptr)
        {
            m_owner.m_listener->pluginChainPanelEditPlugin(m_index);
        }
    };
    addAndMakeVisible(m_editButton);

    m_removeButton.setButtonText("X");
    m_removeButton.setTooltip("Remove plugin from chain");
    m_removeButton.onClick = [this]()
    {
        if (m_owner.m_listener != nullptr)
        {
            m_owner.m_listener->pluginChainPanelRemovePlugin(m_index);
        }
    };
    addAndMakeVisible(m_removeButton);

    m_nameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffe0e0e0));
    addAndMakeVisible(m_nameLabel);

    m_latencyLabel.setColour(juce::Label::textColourId, juce::Colour(0xff808080));
    m_latencyLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(m_latencyLabel);
}

void PluginChainPanel::PluginRowComponent::update(int index, PluginChainNode* node)
{
    m_index = index;
    m_node = node;

    if (node != nullptr)
    {
        m_nameLabel.setText(node->getName(), juce::dontSendNotification);
        bool isBypassed = node->isBypassed();
        m_bypassButton.setToggleState(isBypassed, juce::dontSendNotification);
        updateBypassButtonAppearance(isBypassed);

        int latency = node->getLatencySamples();
        if (latency > 0)
        {
            m_latencyLabel.setText("Latency: " + juce::String(latency) + " samples", juce::dontSendNotification);
        }
        else
        {
            m_latencyLabel.setText("", juce::dontSendNotification);
        }
    }
    else
    {
        m_nameLabel.setText("", juce::dontSendNotification);
        m_latencyLabel.setText("", juce::dontSendNotification);
    }
}

void PluginChainPanel::PluginRowComponent::paint(juce::Graphics& g)
{
    // Draw drag handle area on left
    g.setColour(juce::Colour(0xff404040));
    g.fillRect(0, 0, 20, getHeight());

    // Draw grip lines
    g.setColour(juce::Colour(0xff606060));
    int gripY = getHeight() / 2;
    for (int i = -2; i <= 2; ++i)
    {
        g.drawHorizontalLine(gripY + i * 3, 5.0f, 15.0f);
    }
}

void PluginChainPanel::PluginRowComponent::resized()
{
    auto bounds = getLocalBounds();

    // Drag handle area
    bounds.removeFromLeft(24);

    // Remove button on right (keep X small)
    m_removeButton.setBounds(bounds.removeFromRight(32).reduced(4));

    // Edit button
    m_editButton.setBounds(bounds.removeFromRight(50).reduced(4));

    // Bypass button (wider for "Bypass" text)
    m_bypassButton.setBounds(bounds.removeFromRight(60).reduced(4));

    // Plugin info area
    auto infoArea = bounds.reduced(4);
    m_nameLabel.setBounds(infoArea.removeFromTop(infoArea.getHeight() / 2));
    m_latencyLabel.setBounds(infoArea);
}

void PluginChainPanel::PluginRowComponent::mouseDown(const juce::MouseEvent& e)
{
    if (e.x < 24)  // Drag handle area
    {
        m_dragStarted = false;
        m_dragStartPos = e.getPosition();
    }
}

void PluginChainPanel::PluginRowComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (e.x < 24 || m_dragStartPos.x < 24)
    {
        auto distance = e.getDistanceFromDragStart();
        if (!m_dragStarted && distance > 4)
        {
            m_dragStarted = true;
            juce::var dragData;
            dragData.append(m_index);
            m_owner.startDragging(dragData, this, juce::ScaledImage(), true);
        }
    }
}

void PluginChainPanel::PluginRowComponent::updateBypassButtonAppearance(bool isBypassed)
{
    if (isBypassed)
    {
        // Bypassed state: yellow/orange background to make it obvious
        m_bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffcc8800));
        m_bypassButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }
    else
    {
        // Active state: normal button appearance
        m_bypassButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff404040));
        m_bypassButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe0e0e0));
    }
}

//==============================================================================
// DraggableListBox
//==============================================================================

PluginChainPanel::DraggableListBox::DraggableListBox(PluginChainPanel& owner)
    : juce::ListBox(), m_owner(owner)
{
}

bool PluginChainPanel::DraggableListBox::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.isArray() && details.description.size() > 0;
}

void PluginChainPanel::DraggableListBox::itemDragEnter(const SourceDetails& details)
{
    m_dropInsertIndex = getInsertIndexForPosition(details.localPosition.y);
    repaint();
}

void PluginChainPanel::DraggableListBox::itemDragMove(const SourceDetails& details)
{
    m_dropInsertIndex = getInsertIndexForPosition(details.localPosition.y);
    repaint();
}

void PluginChainPanel::DraggableListBox::itemDragExit(const SourceDetails&)
{
    m_dropInsertIndex = -1;
    repaint();
}

void PluginChainPanel::DraggableListBox::itemDropped(const SourceDetails& details)
{
    if (m_dropInsertIndex >= 0 && m_owner.m_listener != nullptr)
    {
        int fromIndex = details.description[0];
        int toIndex = m_dropInsertIndex;

        // Adjust target index if dragging down
        if (fromIndex < toIndex)
        {
            toIndex--;
        }

        if (fromIndex != toIndex)
        {
            m_owner.m_listener->pluginChainPanelMovePlugin(fromIndex, toIndex);
        }
    }

    m_dropInsertIndex = -1;
    repaint();
}

void PluginChainPanel::DraggableListBox::paint(juce::Graphics& g)
{
    juce::ListBox::paint(g);

    // Draw drop indicator
    if (m_dropInsertIndex >= 0)
    {
        int y = m_dropInsertIndex * m_owner.m_rowHeight;
        g.setColour(m_owner.m_accentColour);
        g.fillRect(0, y - 2, getWidth(), 4);
    }
}

int PluginChainPanel::DraggableListBox::getInsertIndexForPosition(int y) const
{
    int numRows = m_owner.getNumRows();
    int index = (y + m_owner.m_rowHeight / 2) / m_owner.m_rowHeight;
    return juce::jlimit(0, numRows, index);
}

//==============================================================================
// PluginChainPanel
//==============================================================================

PluginChainPanel::PluginChainPanel(PluginChain* chain)
    : m_chain(chain), m_listBox(*this)
{
    jassert(m_chain != nullptr);

    // Title label
    m_titleLabel.setText("Plugin Chain", juce::dontSendNotification);
    m_titleLabel.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    m_titleLabel.setColour(juce::Label::textColourId, m_textColour);
    addAndMakeVisible(m_titleLabel);

    // List box
    m_listBox.setModel(this);
    m_listBox.setRowHeight(m_rowHeight);
    m_listBox.setColour(juce::ListBox::backgroundColourId, m_backgroundColour);
    m_listBox.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff404040));
    m_listBox.setOutlineThickness(1);
    addAndMakeVisible(m_listBox);

    // Empty label (shown when no plugins)
    m_emptyLabel.setText("No plugins in chain.\nClick 'Add Plugin' to get started.",
                          juce::dontSendNotification);
    m_emptyLabel.setJustificationType(juce::Justification::centred);
    m_emptyLabel.setColour(juce::Label::textColourId, juce::Colour(0xff808080));
    addAndMakeVisible(m_emptyLabel);

    // Latency label
    m_latencyLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa0a0a0));
    m_latencyLabel.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(m_latencyLabel);

    // Add plugin button
    m_addPluginButton.setButtonText("Add Plugin...");
    m_addPluginButton.onClick = [this]() { onAddPluginClicked(); };
    addAndMakeVisible(m_addPluginButton);

    // Apply to Selection button - prominent green button for main action
    m_applyToSelectionButton.setButtonText("Apply (Cmd+P)");
    m_applyToSelectionButton.setTooltip("Apply plugin chain to selection or entire file (Cmd+P)");
    m_applyToSelectionButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff308040));  // Green
    m_applyToSelectionButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    m_applyToSelectionButton.onClick = [this]() { onApplyToSelectionClicked(); };
    addAndMakeVisible(m_applyToSelectionButton);

    // Bypass all button
    m_bypassAllButton.setButtonText("Bypass All");
    m_bypassAllButton.onClick = [this]() { onBypassAllClicked(); };
    addAndMakeVisible(m_bypassAllButton);

    // Render Options Group
    m_renderOptionsGroup.setText("Render Options");
    m_renderOptionsGroup.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0xff444444));
    m_renderOptionsGroup.setColour(juce::GroupComponent::textColourId, m_textColour);
    addAndMakeVisible(m_renderOptionsGroup);

    // Convert to stereo checkbox
    m_convertToStereoCheckbox.setButtonText("Convert to stereo");
    m_convertToStereoCheckbox.setTooltip("Convert mono file to stereo before processing (preserves stereo plugin effects)");
    m_convertToStereoCheckbox.setToggleState(false, juce::dontSendNotification);
    m_convertToStereoCheckbox.setEnabled(false);  // Disabled until we know source is mono
    addAndMakeVisible(m_convertToStereoCheckbox);

    // Include tail checkbox
    m_includeTailCheckbox.setButtonText("Include effect tail");
    m_includeTailCheckbox.setTooltip("Extend selection to include reverb/delay tail");
    m_includeTailCheckbox.setToggleState(false, juce::dontSendNotification);
    m_includeTailCheckbox.onClick = [this]()
    {
        bool enabled = m_includeTailCheckbox.getToggleState();
        m_tailLengthSlider.setEnabled(enabled);
        m_tailLengthLabel.setEnabled(enabled);
    };
    addAndMakeVisible(m_includeTailCheckbox);

    // Tail length label
    m_tailLengthLabel.setText("Tail:", juce::dontSendNotification);
    m_tailLengthLabel.setColour(juce::Label::textColourId, m_textColour);
    m_tailLengthLabel.setEnabled(false);
    addAndMakeVisible(m_tailLengthLabel);

    // Tail length slider
    m_tailLengthSlider.setRange(0.5, 10.0, 0.1);
    m_tailLengthSlider.setValue(2.0);
    m_tailLengthSlider.setTextValueSuffix(" sec");
    m_tailLengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    m_tailLengthSlider.setEnabled(false);
    addAndMakeVisible(m_tailLengthSlider);

    // Listen for chain changes
    m_chain->addChangeListener(this);

    // Initial refresh
    refresh();
}

PluginChainPanel::~PluginChainPanel()
{
    if (m_chain != nullptr)
    {
        m_chain->removeChangeListener(this);
    }
}

void PluginChainPanel::refresh()
{
    m_listBox.updateContent();
    updateLatencyDisplay();

    // Show/hide empty label
    bool isEmpty = m_chain == nullptr || m_chain->isEmpty();
    m_emptyLabel.setVisible(isEmpty);
    m_listBox.setVisible(!isEmpty);

    // Update bypass all state
    if (m_chain != nullptr)
    {
        m_bypassAllButton.setToggleState(m_chain->areAllBypassed(), juce::dontSendNotification);
    }
}

juce::DocumentWindow* PluginChainPanel::showInWindow(bool modal)
{
    auto* window = new juce::DocumentWindow("Plugin Chain",
                                             m_backgroundColour,
                                             juce::DocumentWindow::allButtons);
    window->setContentNonOwned(this, true);
    window->setResizable(true, true);
    window->centreWithSize(400, 500);
    window->setVisible(true);

    if (modal)
    {
        window->enterModalState(true);
    }

    return window;
}

//==============================================================================
// Component overrides

void PluginChainPanel::paint(juce::Graphics& g)
{
    g.fillAll(m_backgroundColour);
}

void PluginChainPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    // Title at top
    m_titleLabel.setBounds(bounds.removeFromTop(30));

    // Render options section at bottom (above buttons)
    const int kRenderOptionsHeight = 70;
    auto renderOptionsArea = bounds.removeFromBottom(kRenderOptionsHeight);
    m_renderOptionsGroup.setBounds(renderOptionsArea);

    // Layout render options inside the group
    auto optionsInner = renderOptionsArea.reduced(10, 18);  // Account for group border
    auto row1 = optionsInner.removeFromTop(24);
    auto row2 = optionsInner.removeFromTop(24);

    // Row 1: Convert to stereo checkbox
    m_convertToStereoCheckbox.setBounds(row1);

    // Row 2: Include tail checkbox + tail length controls
    m_includeTailCheckbox.setBounds(row2.removeFromLeft(160));
    row2.removeFromLeft(10);
    m_tailLengthLabel.setBounds(row2.removeFromLeft(35));
    m_tailLengthSlider.setBounds(row2);

    // Two rows of buttons at bottom
    auto buttonRow2 = bounds.removeFromBottom(36);  // Apply button row
    auto buttonRow1 = bounds.removeFromBottom(36);  // Add/Bypass buttons row

    // Row 1: Add Plugin and Bypass All
    m_addPluginButton.setBounds(buttonRow1.removeFromLeft(120).reduced(4));
    m_bypassAllButton.setBounds(buttonRow1.removeFromLeft(100).reduced(4));
    m_latencyLabel.setBounds(buttonRow1.reduced(4));

    // Row 2: Apply to Selection button (full width, prominent)
    m_applyToSelectionButton.setBounds(buttonRow2.reduced(4));

    // List box fills remaining space
    bounds.removeFromTop(10);
    m_listBox.setBounds(bounds);
    m_emptyLabel.setBounds(bounds);
}

bool PluginChainPanel::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        int selected = m_listBox.getSelectedRow();
        if (selected >= 0 && m_listener != nullptr)
        {
            m_listener->pluginChainPanelRemovePlugin(selected);
            return true;
        }
    }

    if (key == juce::KeyPress::returnKey)
    {
        int selected = m_listBox.getSelectedRow();
        if (selected >= 0 && m_listener != nullptr)
        {
            m_listener->pluginChainPanelEditPlugin(selected);
            return true;
        }
    }

    return false;
}

//==============================================================================
// ListBoxModel overrides

int PluginChainPanel::getNumRows()
{
    return m_chain != nullptr ? m_chain->getNumPlugins() : 0;
}

void PluginChainPanel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height,
                                         bool rowIsSelected)
{
    // Background
    if (rowIsSelected)
    {
        g.fillAll(m_selectedRowColour);
    }
    else if (rowNumber % 2 == 1)
    {
        g.fillAll(m_alternateRowColour);
    }
    else
    {
        g.fillAll(m_backgroundColour);
    }
}

void PluginChainPanel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    m_listBox.selectRow(row);
}

void PluginChainPanel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (m_listener != nullptr)
    {
        m_listener->pluginChainPanelEditPlugin(row);
    }
}

void PluginChainPanel::deleteKeyPressed(int lastRowSelected)
{
    if (m_listener != nullptr && lastRowSelected >= 0)
    {
        m_listener->pluginChainPanelRemovePlugin(lastRowSelected);
    }
}

void PluginChainPanel::returnKeyPressed(int lastRowSelected)
{
    if (m_listener != nullptr && lastRowSelected >= 0)
    {
        m_listener->pluginChainPanelEditPlugin(lastRowSelected);
    }
}

juce::var PluginChainPanel::getDragSourceDescription(const juce::SparseSet<int>& rowsToDescribe)
{
    if (rowsToDescribe.size() > 0)
    {
        juce::var result;
        result.append(rowsToDescribe[0]);
        return result;
    }
    return {};
}

juce::Component* PluginChainPanel::refreshComponentForRow(int rowNumber, bool /*isRowSelected*/,
                                                           juce::Component* existingComponentToUpdate)
{
    PluginRowComponent* row = dynamic_cast<PluginRowComponent*>(existingComponentToUpdate);

    if (row == nullptr)
    {
        row = new PluginRowComponent(*this);
    }

    PluginChainNode* node = nullptr;
    if (m_chain != nullptr && rowNumber < m_chain->getNumPlugins())
    {
        node = m_chain->getPlugin(rowNumber);
    }

    row->update(rowNumber, node);
    return row;
}

//==============================================================================
// ChangeListener override

void PluginChainPanel::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == m_chain)
    {
        juce::MessageManager::callAsync([this]()
        {
            refresh();
        });
    }
}

//==============================================================================
// Private methods

void PluginChainPanel::updateLatencyDisplay()
{
    if (m_chain != nullptr)
    {
        int totalLatency = m_chain->getTotalLatency();
        if (totalLatency > 0)
        {
            m_latencyLabel.setText("Total latency: " + juce::String(totalLatency) + " samples",
                                   juce::dontSendNotification);
        }
        else
        {
            m_latencyLabel.setText("", juce::dontSendNotification);
        }
    }
}

void PluginChainPanel::onAddPluginClicked()
{
    if (m_listener != nullptr)
    {
        m_listener->pluginChainPanelAddPlugin();
    }
}

void PluginChainPanel::onBypassAllClicked()
{
    if (m_listener != nullptr)
    {
        m_listener->pluginChainPanelBypassAll(m_bypassAllButton.getToggleState());
    }
}

void PluginChainPanel::onApplyToSelectionClicked()
{
    if (m_listener != nullptr)
    {
        m_listener->pluginChainPanelApplyToSelection();
    }
}

PluginChainPanel::RenderOptions PluginChainPanel::getRenderOptions() const
{
    RenderOptions options;
    options.convertToStereo = m_convertToStereoCheckbox.getToggleState();
    options.includeTail = m_includeTailCheckbox.getToggleState();
    options.tailLengthSeconds = m_tailLengthSlider.getValue();
    return options;
}

void PluginChainPanel::setSourceIsMono(bool isMono)
{
    m_isSourceMono = isMono;
    m_convertToStereoCheckbox.setEnabled(isMono);

    // Update tooltip to explain why it's disabled
    if (!isMono)
    {
        m_convertToStereoCheckbox.setTooltip("Source is already stereo");
    }
    else
    {
        m_convertToStereoCheckbox.setTooltip("Convert mono file to stereo before processing (preserves stereo plugin effects)");
    }
}
