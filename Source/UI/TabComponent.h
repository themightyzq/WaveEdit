/*
  ==============================================================================

    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Utils/DocumentManager.h"

/**
 * Tab button component representing a single document tab.
 *
 * Features:
 * - Filename display
 * - Modified indicator (asterisk)
 * - Close button (X)
 * - Selection state visualization
 * - Hover effects
 * - Right-click menu support
 */
class TabButton : public juce::Component
{
public:
    /**
     * Listener interface for tab button events.
     */
    class Listener
    {
    public:
        virtual ~Listener() = default;

        /**
         * Called when the tab is clicked (to select it).
         *
         * @param tab The tab that was clicked
         */
        virtual void tabClicked(TabButton* tab) = 0;

        /**
         * Called when the tab's close button is clicked.
         *
         * @param tab The tab to close
         */
        virtual void tabCloseClicked(TabButton* tab) = 0;

        /**
         * Called when the tab is right-clicked for context menu.
         *
         * @param tab The tab that was right-clicked
         * @param event The mouse event
         */
        virtual void tabRightClicked(TabButton* tab, const juce::MouseEvent& event) = 0;
    };

    /**
     * Creates a new tab button.
     *
     * @param document The document this tab represents
     * @param index The index of this tab in the tab bar
     */
    TabButton(Document* document, int index);

    /**
     * Updates the tab with new document info.
     *
     * @param document The document to display
     */
    void updateDocument(Document* document);

    /**
     * Sets whether this tab is selected (current).
     *
     * @param selected true if this is the current tab
     */
    void setSelected(bool selected);

    /**
     * Gets whether this tab is selected.
     *
     * @return true if this is the current tab
     */
    bool isSelected() const { return m_isSelected; }

    /**
     * Gets the tab's index.
     *
     * @return The index of this tab
     */
    int getIndex() const { return m_index; }

    /**
     * Sets the tab's index (for reordering).
     *
     * @param index The new index
     */
    void setIndex(int index) { m_index = index; }

    /**
     * Gets the document associated with this tab.
     *
     * @return Pointer to the document
     */
    Document* getDocument() const { return m_document; }

    /**
     * Sets the listener for tab events.
     *
     * @param listener The listener to set
     */
    void setListener(Listener* listener) { m_listener = listener; }

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:
    Document* m_document;
    int m_index;
    bool m_isSelected;
    bool m_isHovering;
    bool m_isHoveringClose;
    Listener* m_listener;

    // Close button bounds
    juce::Rectangle<int> m_closeBounds;

    // Tab appearance
    static constexpr int kTabHeight = 32;
    static constexpr int kMinTabWidth = 100;
    static constexpr int kMaxTabWidth = 200;
    static constexpr int kCloseButtonSize = 16;
    static constexpr int kPadding = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TabButton)
};

/**
 * Tab component that displays and manages document tabs.
 *
 * Features:
 * - Visual tab bar at top of window
 * - Click to switch documents
 * - Close buttons on each tab
 * - Modified indicators
 * - Right-click context menu
 * - Scrollable when many tabs open
 * - Keyboard navigation support
 *
 * This component implements DocumentManager::Listener to automatically
 * update when documents are added, removed, or switched.
 */
class TabComponent : public juce::Component,
                     public DocumentManager::Listener,
                     public TabButton::Listener,
                     public juce::ScrollBar::Listener
{
public:
    /**
     * Creates a new tab component.
     *
     * @param documentManager The document manager to observe
     */
    explicit TabComponent(DocumentManager& documentManager);

    /**
     * Destructor. Removes listener from document manager.
     */
    ~TabComponent() override;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    // DocumentManager::Listener implementation
    void currentDocumentChanged(Document* newDocument) override;
    void documentAdded(Document* document, int index) override;
    void documentRemoved(Document* document, int index) override;

    // TabButton::Listener implementation
    void tabClicked(TabButton* tab) override;
    void tabCloseClicked(TabButton* tab) override;
    void tabRightClicked(TabButton* tab, const juce::MouseEvent& event) override;

    // ScrollBar::Listener implementation
    void scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart) override;

private:
    DocumentManager& m_documentManager;
    juce::OwnedArray<TabButton> m_tabs;
    juce::ScrollBar m_scrollBar;
    bool m_needsScrollBar;
    int m_scrollOffset;

    /**
     * Updates all tabs to reflect current document states.
     */
    void updateTabs();

    /**
     * Rebuilds the tab list from scratch.
     */
    void rebuildTabs();

    /**
     * Updates scroll bar visibility and range.
     */
    void updateScrollBar();

    /**
     * Shows context menu for a tab.
     *
     * @param tab The tab that was right-clicked
     * @param screenPosition The screen position for the menu
     */
    void showTabContextMenu(TabButton* tab, juce::Point<int> screenPosition);

    /**
     * Gets the total width needed for all tabs.
     *
     * @return Total width in pixels
     */
    int getTotalTabWidth() const;

    /**
     * Ensures the current tab is visible (scrolls if needed).
     */
    void ensureCurrentTabVisible();

    // Tab appearance
    static constexpr int kTabBarHeight = 32;
    static constexpr int kScrollBarHeight = 8;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TabComponent)
};