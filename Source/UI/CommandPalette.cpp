/*
  ==============================================================================

    CommandPalette.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "CommandPalette.h"
#include "../Commands/CommandIDs.h"
#include "ThemeManager.h"
#include <algorithm>

//==============================================================================
CommandPalette::CommandPalette(juce::ApplicationCommandManager& commandManager)
    : m_commandManager(commandManager)
{
    setWantsKeyboardFocus(false);

    m_searchField.setFont(juce::FontOptions(16.0f));
    {
        const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();
        m_searchField.setTextToShowWhenEmpty("Type a command...", theme.textMuted);
        m_searchField.setColour(juce::TextEditor::backgroundColourId,
                                theme.background.brighter(0.05f));
        m_searchField.setColour(juce::TextEditor::textColourId, theme.text);
        m_searchField.setColour(juce::TextEditor::outlineColourId, theme.border);
        m_searchField.setColour(juce::TextEditor::focusedOutlineColourId, theme.accent);
        m_searchField.setColour(juce::CaretComponent::caretColourId, theme.text);
    }
    m_searchField.addListener(this);
    m_searchField.addKeyListener(this);
    addAndMakeVisible(m_searchField);

    setVisible(false);
}

CommandPalette::~CommandPalette()
{
    m_searchField.removeListener(this);
    m_searchField.removeKeyListener(this);
}

//==============================================================================
void CommandPalette::show()
{
    m_searchField.clear();
    buildCommandList();
    updateFilteredList();
    m_selectedIndex = 0;
    m_scrollOffset = 0;

    // Position at top-center of parent
    if (auto* parent = getParentComponent())
    {
        int x = (parent->getWidth() - kWidth) / 2;
        int visibleItems = std::min(static_cast<int>(m_filteredCommands.size()),
                                    kMaxVisibleItems);
        int height = kSearchHeight + kItemHeight * std::max(visibleItems, 1);
        setBounds(x, 8, kWidth, height);
    }

    setVisible(true);
    toFront(true);
    m_searchField.grabKeyboardFocus();
}

void CommandPalette::dismiss()
{
    setVisible(false);
}

bool CommandPalette::isShowing() const
{
    return isVisible();
}

//==============================================================================
void CommandPalette::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto& theme = waveedit::ThemeManager::getInstance().getCurrent();

    // Drop shadow
    g.setColour(juce::Colour(0x40000000));
    g.fillRoundedRectangle(bounds.translated(1.0f, 2.0f), 8.0f);

    // Background
    g.setColour(theme.panel);
    g.fillRoundedRectangle(bounds, 8.0f);

    // Border
    g.setColour(theme.border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    // Command list area
    auto listArea = getLocalBounds();
    listArea.removeFromTop(kSearchHeight);

    if (m_filteredCommands.empty())
    {
        g.setColour(theme.textMuted);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("No matching commands", listArea,
                   juce::Justification::centred);
        return;
    }

    // Draw visible items
    int endIdx = std::min(m_scrollOffset + kMaxVisibleItems,
                          static_cast<int>(m_filteredCommands.size()));

    for (int i = m_scrollOffset; i < endIdx; ++i)
    {
        auto& entry = *m_filteredCommands[static_cast<size_t>(i)];
        int y = kSearchHeight + (i - m_scrollOffset) * kItemHeight;
        auto rowBounds = juce::Rectangle<int>(0, y, getWidth(), kItemHeight);

        // Selected row highlight
        if (i == m_selectedIndex)
        {
            g.setColour(theme.border);
            g.fillRect(rowBounds);
        }

        auto contentBounds = rowBounds.reduced(12, 0);

        // Category badge — colours are workflow-specific, not theme-driven.
        if (entry.category.isNotEmpty())
        {
            auto categoryColour = getCategoryColour(entry.category);
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            int badgeWidth = g.getCurrentFont().getStringWidth(entry.category) + 10;
            auto badgeBounds = contentBounds.removeFromLeft(badgeWidth)
                                   .withSizeKeepingCentre(badgeWidth, 18);

            g.setColour(categoryColour.withAlpha(0.2f));
            g.fillRoundedRectangle(badgeBounds.toFloat(), 3.0f);
            g.setColour(categoryColour);
            g.drawText(entry.category, badgeBounds,
                       juce::Justification::centred);
            contentBounds.removeFromLeft(8);  // Gap after badge
        }

        // Shortcut text (right-aligned)
        if (entry.shortcutText.isNotEmpty())
        {
            g.setFont(juce::FontOptions(11.0f));
            g.setColour(entry.isActive ? theme.textMuted : theme.textMuted.darker(0.4f));
            auto shortcutBounds = contentBounds.removeFromRight(120);
            g.drawText(entry.shortcutText, shortcutBounds,
                       juce::Justification::centredRight);
        }

        // Command name
        g.setFont(juce::FontOptions(14.0f));
        g.setColour(entry.isActive ? theme.text : theme.textMuted);
        g.drawText(entry.name, contentBounds,
                   juce::Justification::centredLeft);
    }
}

void CommandPalette::resized()
{
    auto bounds = getLocalBounds();
    m_searchField.setBounds(bounds.removeFromTop(kSearchHeight).reduced(8, 6));
}

void CommandPalette::parentSizeChanged()
{
    if (isVisible() && getParentComponent())
    {
        int x = (getParentComponent()->getWidth() - kWidth) / 2;
        setTopLeftPosition(x, 8);
    }
}

//==============================================================================
// TextEditor::Listener
//==============================================================================

void CommandPalette::textEditorTextChanged(juce::TextEditor&)
{
    updateFilteredList();
    m_selectedIndex = 0;
    m_scrollOffset = 0;

    // Resize to fit content
    int visibleItems = std::min(static_cast<int>(m_filteredCommands.size()),
                                kMaxVisibleItems);
    int height = kSearchHeight + kItemHeight * std::max(visibleItems, 1);
    setSize(kWidth, height);

    repaint();
}

void CommandPalette::textEditorReturnKeyPressed(juce::TextEditor&)
{
    executeSelectedCommand();
}

void CommandPalette::textEditorEscapeKeyPressed(juce::TextEditor&)
{
    dismiss();
}

//==============================================================================
// KeyListener
//==============================================================================

bool CommandPalette::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    if (key == juce::KeyPress::upKey)
    {
        moveSelection(-1);
        return true;
    }
    if (key == juce::KeyPress::downKey)
    {
        moveSelection(1);
        return true;
    }
    if (key == juce::KeyPress::tabKey)
    {
        dismiss();
        return true;
    }
    return false;
}

//==============================================================================
// Command list building and filtering
//==============================================================================

void CommandPalette::buildCommandList()
{
    m_allCommands.clear();

    // Commands to skip (internal, clutters list)
    std::set<juce::CommandID> skipIDs = {
        CommandIDs::tabSelect1, CommandIDs::tabSelect2,
        CommandIDs::tabSelect3, CommandIDs::tabSelect4,
        CommandIDs::tabSelect5, CommandIDs::tabSelect6,
        CommandIDs::tabSelect7, CommandIDs::tabSelect8,
        CommandIDs::tabSelect9,
        CommandIDs::viewSpectrumFFTSize512,
        CommandIDs::viewSpectrumFFTSize1024,
        CommandIDs::viewSpectrumFFTSize2048,
        CommandIDs::viewSpectrumFFTSize4096,
        CommandIDs::viewSpectrumFFTSize8192,
        CommandIDs::viewSpectrumWindowHann,
        CommandIDs::viewSpectrumWindowHamming,
        CommandIDs::viewSpectrumWindowBlackman,
        CommandIDs::viewSpectrumWindowRectangular
    };

    juce::Array<juce::CommandID> allIDs;
    auto* target = m_commandManager.getFirstCommandTarget(CommandIDs::fileNew);
    if (target)
        target->getAllCommands(allIDs);

    for (auto id : allIDs)
    {
        if (skipIDs.count(id) > 0)
            continue;

        // Skip the command palette itself to avoid recursion
        if (id == CommandIDs::helpCommandPalette)
            continue;

        auto* info = m_commandManager.getCommandForID(id);
        if (info == nullptr)
            continue;

        CommandEntry entry;
        entry.commandID = id;
        entry.name = info->shortName;
        entry.description = info->description;
        entry.category = info->categoryName;
        entry.isActive = (info->flags & juce::ApplicationCommandInfo::isDisabled) == 0;
        entry.matchScore = 0;

        // Get shortcut display string
        auto* keyMappings = m_commandManager.getKeyMappings();
        if (keyMappings)
        {
            auto keys = keyMappings->getKeyPressesAssignedToCommand(id);
            if (!keys.isEmpty())
                entry.shortcutText = keys[0].getTextDescriptionWithIcons();
        }

        if (entry.name.isNotEmpty())
            m_allCommands.push_back(entry);
    }

    // Sort alphabetically by category then name
    std::sort(m_allCommands.begin(), m_allCommands.end(),
        [](const CommandEntry& a, const CommandEntry& b)
        {
            if (a.category != b.category)
                return a.category.compareIgnoreCase(b.category) < 0;
            return a.name.compareIgnoreCase(b.name) < 0;
        });
}

void CommandPalette::updateFilteredList()
{
    m_filteredCommands.clear();

    juce::String query = m_searchField.getText().trim();

    if (query.isEmpty())
    {
        // Show all commands sorted by category
        for (auto& entry : m_allCommands)
        {
            entry.matchScore = 100;
            m_filteredCommands.push_back(&entry);
        }
        return;
    }

    // Score each command and filter
    for (auto& entry : m_allCommands)
    {
        int nameScore = fuzzyMatch(query, entry.name);
        int catScore = fuzzyMatch(query, entry.category) / 2;
        int descScore = fuzzyMatch(query, entry.description) / 4;

        entry.matchScore = std::max({ nameScore, catScore, descScore });

        if (entry.matchScore > 0)
            m_filteredCommands.push_back(&entry);
    }

    // Sort by score descending, then alphabetically for ties
    std::sort(m_filteredCommands.begin(), m_filteredCommands.end(),
        [](const CommandEntry* a, const CommandEntry* b)
        {
            if (a->matchScore != b->matchScore)
                return a->matchScore > b->matchScore;
            return a->name.compareIgnoreCase(b->name) < 0;
        });
}

void CommandPalette::executeSelectedCommand()
{
    if (m_selectedIndex >= 0
        && m_selectedIndex < static_cast<int>(m_filteredCommands.size()))
    {
        auto id = m_filteredCommands[static_cast<size_t>(m_selectedIndex)]->commandID;
        dismiss();
        m_commandManager.invokeDirectly(id, true);
    }
}

void CommandPalette::moveSelection(int delta)
{
    int count = static_cast<int>(m_filteredCommands.size());
    if (count == 0) return;

    m_selectedIndex = juce::jlimit(0, count - 1, m_selectedIndex + delta);
    ensureSelectionVisible();
    repaint();
}

void CommandPalette::ensureSelectionVisible()
{
    if (m_selectedIndex < m_scrollOffset)
        m_scrollOffset = m_selectedIndex;
    else if (m_selectedIndex >= m_scrollOffset + kMaxVisibleItems)
        m_scrollOffset = m_selectedIndex - kMaxVisibleItems + 1;
}

//==============================================================================
// Fuzzy matching
//==============================================================================

int CommandPalette::fuzzyMatch(const juce::String& query, const juce::String& text)
{
    if (query.isEmpty() || text.isEmpty())
        return 0;

    juce::String lowerQuery = query.toLowerCase();
    juce::String lowerText = text.toLowerCase();

    // Exact prefix match — highest priority
    if (lowerText.startsWith(lowerQuery))
        return 1000 + (100 - text.length());

    // Substring match
    if (lowerText.contains(lowerQuery))
        return 500 + (100 - text.length());

    // Fuzzy character matching
    int score = 0;
    int queryIdx = 0;
    int consecutive = 0;
    int queryLen = lowerQuery.length();

    for (int i = 0; i < lowerText.length() && queryIdx < queryLen; ++i)
    {
        if (lowerText[i] == lowerQuery[queryIdx])
        {
            score += 10;

            // Bonus for consecutive characters
            if (consecutive > 0)
                score += 20;

            // Bonus for matching at word starts (uppercase in original,
            // first char, or after space/underscore)
            if (i == 0
                || text[i] == juce::juce_wchar(std::toupper(text[i]))
                || (i > 0 && (text[i - 1] == ' ' || text[i - 1] == '_')))
            {
                score += 50;
            }

            consecutive++;
            queryIdx++;
        }
        else
        {
            consecutive = 0;
        }
    }

    // All query characters must be matched
    if (queryIdx < queryLen)
        return 0;

    return score;
}

//==============================================================================
// Category colours
//==============================================================================

juce::Colour CommandPalette::getCategoryColour(const juce::String& category)
{
    juce::String lower = category.toLowerCase();

    if (lower == "file")        return juce::Colour(0xff5599dd);  // blue
    if (lower == "edit")        return juce::Colour(0xff999999);  // gray
    if (lower == "view")        return juce::Colour(0xff55bbaa);  // teal
    if (lower == "process")     return juce::Colour(0xffaa77cc);  // purple
    if (lower == "playback")    return juce::Colour(0xff55cc77);  // green
    if (lower == "navigation")  return juce::Colour(0xff77aadd);  // light blue
    if (lower == "selection")   return juce::Colour(0xff88aacc);  // steel blue
    if (lower == "plugin")      return juce::Colour(0xffdd7766);  // coral
    if (lower == "plugins")     return juce::Colour(0xffdd7766);  // coral
    if (lower == "region")      return juce::Colour(0xff66cc66);  // green
    if (lower == "marker")      return juce::Colour(0xffddaa44);  // amber
    if (lower == "snap")        return juce::Colour(0xffcc9966);  // tan
    if (lower == "tools")       return juce::Colour(0xff99bb55);  // olive
    if (lower == "help")        return juce::Colour(0xff6699cc);  // sky blue
    if (lower == "tab")         return juce::Colour(0xff8888aa);  // muted purple
    if (lower == "toolbar")     return juce::Colour(0xff888888);  // gray
    if (lower == "batch")       return juce::Colour(0xffbb8855);  // orange

    return juce::Colour(0xff888888);  // default gray
}
