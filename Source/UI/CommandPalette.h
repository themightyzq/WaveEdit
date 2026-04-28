/*
  ==============================================================================

    CommandPalette.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    VS Code / Sublime-style fuzzy command palette for quick access to
    all application commands. Lightweight floating overlay, keyboard-driven.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

/**
 * Floating command palette overlay for fuzzy-searching and executing
 * application commands. Appears at top-center of the main window.
 *
 * Keyboard-driven: type to filter, arrow keys to navigate, Enter to run,
 * Escape to dismiss. Also dismisses on focus loss.
 */
class CommandPalette : public juce::Component,
                       public juce::TextEditor::Listener,
                       public juce::KeyListener
{
public:
    explicit CommandPalette(juce::ApplicationCommandManager& commandManager);
    ~CommandPalette() override;

    /** Show the palette, focus the search field. */
    void show();

    /** Hide the palette. */
    void dismiss();

    /** Returns true if currently visible. */
    bool isShowing() const;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void parentSizeChanged() override;

    // TextEditor::Listener
    void textEditorTextChanged(juce::TextEditor& editor) override;
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void textEditorEscapeKeyPressed(juce::TextEditor& editor) override;

    // KeyListener (for arrow keys in the search field)
    bool keyPressed(const juce::KeyPress& key, juce::Component* source) override;

    static constexpr int kWidth = 500;
    static constexpr int kSearchHeight = 40;
    static constexpr int kItemHeight = 36;
    static constexpr int kMaxVisibleItems = 12;
    static constexpr int kMaxHeight = kSearchHeight + kItemHeight * kMaxVisibleItems;

private:
    struct CommandEntry
    {
        juce::CommandID commandID;
        juce::String name;
        juce::String description;
        juce::String category;
        juce::String shortcutText;
        bool isActive;
        int matchScore;
    };

    void buildCommandList();
    void updateFilteredList();
    void executeSelectedCommand();
    void moveSelection(int delta);
    void ensureSelectionVisible();

    /** Fuzzy match: returns score (0 = no match, higher = better). */
    static int fuzzyMatch(const juce::String& query, const juce::String& text);

    /** Get a colour for a category name. */
    static juce::Colour getCategoryColour(const juce::String& category);

    juce::ApplicationCommandManager& m_commandManager;

    juce::TextEditor m_searchField;

    std::vector<CommandEntry> m_allCommands;
    std::vector<CommandEntry*> m_filteredCommands;

    int m_selectedIndex = 0;
    int m_scrollOffset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CommandPalette)
};
