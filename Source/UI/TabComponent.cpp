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

#include "TabComponent.h"

//==============================================================================
// TabButton Implementation

TabButton::TabButton(Document* document, int index)
    : m_document(document)
    , m_index(index)
    , m_isSelected(false)
    , m_isHovering(false)
    , m_isHoveringClose(false)
    , m_listener(nullptr)
{
    setSize(kMaxTabWidth, kTabHeight);
}

void TabButton::updateDocument(Document* document)
{
    m_document = document;
    repaint();
}

void TabButton::setSelected(bool selected)
{
    if (m_isSelected != selected)
    {
        m_isSelected = selected;
        repaint();
    }
}

void TabButton::paint(juce::Graphics& g)
{
    if (!m_document)
        return;

    // Tab background
    if (m_isSelected)
    {
        g.fillAll(juce::Colour(0xff3a3a3a)); // Lighter for selected
    }
    else if (m_isHovering)
    {
        g.fillAll(juce::Colour(0xff323232)); // Slightly lighter on hover
    }
    else
    {
        g.fillAll(juce::Colour(0xff2a2a2a)); // Dark background
    }

    // Tab border
    g.setColour(juce::Colour(0xff4a4a4a));
    if (!m_isSelected)
    {
        // Draw right border to separate tabs
        g.drawLine(static_cast<float>(getWidth() - 1), 0.0f,
                  static_cast<float>(getWidth() - 1), static_cast<float>(getHeight()), 1.0f);
    }

    // Selected tab indicator (bottom bar)
    if (m_isSelected)
    {
        g.setColour(juce::Colour(0xff00ff00)); // Green for selected
        g.fillRect(0, getHeight() - 3, getWidth(), 3);
    }

    // Get filename
    juce::String filename = m_document->getFilename();
    if (filename.isEmpty())
        filename = "Untitled";

    // Add modified indicator
    if (m_document->isModified())
        filename = "*" + filename;

    // Calculate text bounds (leave room for close button)
    auto textBounds = getLocalBounds().reduced(kPadding, 0);
    textBounds.removeFromRight(kCloseButtonSize + kPadding);

    // Draw filename
    g.setColour(m_isSelected ? juce::Colours::white : juce::Colours::lightgrey);
    g.setFont(juce::Font(14.0f));
    g.drawFittedText(filename, textBounds, juce::Justification::centredLeft, 1);

    // Draw close button
    m_closeBounds = juce::Rectangle<int>(getWidth() - kCloseButtonSize - kPadding,
                                          (getHeight() - kCloseButtonSize) / 2,
                                          kCloseButtonSize, kCloseButtonSize);

    if (m_isHoveringClose)
    {
        g.setColour(juce::Colour(0xff5a5a5a));
        g.fillRoundedRectangle(m_closeBounds.toFloat(), 2.0f);
    }

    // Draw X icon
    g.setColour(m_isHoveringClose ? juce::Colours::white : juce::Colours::grey);
    const float crossSize = 8.0f;
    const float cx = m_closeBounds.getCentreX();
    const float cy = m_closeBounds.getCentreY();
    const float halfSize = crossSize * 0.5f;

    g.drawLine(cx - halfSize, cy - halfSize, cx + halfSize, cy + halfSize, 2.0f);
    g.drawLine(cx - halfSize, cy + halfSize, cx + halfSize, cy - halfSize, 2.0f);
}

void TabButton::resized()
{
    // Update close button bounds
    m_closeBounds = juce::Rectangle<int>(getWidth() - kCloseButtonSize - kPadding,
                                          (getHeight() - kCloseButtonSize) / 2,
                                          kCloseButtonSize, kCloseButtonSize);
}

void TabButton::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isRightButtonDown())
    {
        if (m_listener)
            m_listener->tabRightClicked(this, event);
    }
    else if (m_closeBounds.contains(event.getPosition()))
    {
        if (m_listener)
            m_listener->tabCloseClicked(this);
    }
    else
    {
        if (m_listener)
            m_listener->tabClicked(this);
    }
}

void TabButton::mouseEnter(const juce::MouseEvent& event)
{
    m_isHovering = true;
    repaint();
}

void TabButton::mouseExit(const juce::MouseEvent& event)
{
    m_isHovering = false;
    m_isHoveringClose = false;
    repaint();
}

//==============================================================================
// TabComponent Implementation

TabComponent::TabComponent(DocumentManager& documentManager)
    : m_documentManager(documentManager)
    , m_scrollBar(false) // Horizontal scroll bar
    , m_needsScrollBar(false)
    , m_scrollOffset(0)
{
    m_documentManager.addListener(this);

    addAndMakeVisible(m_scrollBar);
    m_scrollBar.addListener(this);
    m_scrollBar.setAutoHide(false);
    m_scrollBar.setVisible(false);

    rebuildTabs();
}

TabComponent::~TabComponent()
{
    m_documentManager.removeListener(this);
}

void TabComponent::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(juce::Colour(0xff1a1a1a)); // Darker than tabs

    // Bottom border
    g.setColour(juce::Colour(0xff4a4a4a));
    g.drawLine(0.0f, static_cast<float>(getHeight() - 1), static_cast<float>(getWidth()),
              static_cast<float>(getHeight() - 1), 1.0f);
}

void TabComponent::resized()
{
    updateScrollBar();

    // Position tabs
    int x = -m_scrollOffset;
    for (auto* tab : m_tabs)
    {
        tab->setBounds(x, 0, tab->getWidth(), kTabBarHeight);
        x += tab->getWidth();
    }

    // Position scroll bar at bottom if needed
    if (m_needsScrollBar)
    {
        m_scrollBar.setBounds(0, kTabBarHeight, getWidth(), kScrollBarHeight);
    }
}

void TabComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (m_needsScrollBar)
    {
        // Horizontal scrolling with mouse wheel
        double newPos = m_scrollBar.getCurrentRangeStart() - wheel.deltaX * 50;
        m_scrollBar.setCurrentRangeStart(newPos);
    }
}

void TabComponent::currentDocumentChanged(Document* newDocument)
{
    // Update selected state on all tabs
    for (int i = 0; i < m_tabs.size(); ++i)
    {
        auto* tab = m_tabs[i];
        tab->setSelected(tab->getDocument() == newDocument);
    }

    ensureCurrentTabVisible();
    repaint();
}

void TabComponent::documentAdded(Document* document, int index)
{
    // Create new tab
    auto* tab = new TabButton(document, index);
    tab->setListener(this);
    m_tabs.insert(index, tab);
    addAndMakeVisible(tab);

    // Update indices of subsequent tabs
    for (int i = index + 1; i < m_tabs.size(); ++i)
    {
        m_tabs[i]->setIndex(i);
    }

    updateScrollBar();
    resized();
}

void TabComponent::documentRemoved(Document* document, int index)
{
    // Find and remove the tab
    for (int i = 0; i < m_tabs.size(); ++i)
    {
        if (m_tabs[i]->getDocument() == document)
        {
            m_tabs.remove(i);
            break;
        }
    }

    // Update indices of remaining tabs
    for (int i = 0; i < m_tabs.size(); ++i)
    {
        m_tabs[i]->setIndex(i);
    }

    updateScrollBar();
    resized();
}

void TabComponent::tabClicked(TabButton* tab)
{
    m_documentManager.setCurrentDocumentIndex(tab->getIndex());
}

void TabComponent::tabCloseClicked(TabButton* tab)
{
    // Check if document is modified
    Document* doc = tab->getDocument();
    if (doc && doc->isModified())
    {
        juce::String filename = doc->getFilename();
        if (filename.isEmpty())
            filename = "Untitled";

        int result = juce::AlertWindow::showYesNoCancelBox(
            juce::AlertWindow::WarningIcon,
            "Save Changes?",
            "\"" + filename + "\" has unsaved changes.\nDo you want to save before closing?",
            "Save",
            "Don't Save",
            "Cancel",
            nullptr,  // Component* associatedComponent
            nullptr); // ModalComponentManager::Callback* callback

        if (result == 1) // Save
        {
            // Save the document
            Document* saveDoc = m_documentManager.getDocument(tab->getIndex());
            if (saveDoc && saveDoc->getFile().existsAsFile())
            {
                if (saveDoc->saveFile(saveDoc->getFile()))
                {
                    saveDoc->setModified(false);
                    m_documentManager.closeDocument(saveDoc);
                }
                else
                {
                    // Save failed - don't close
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Save Failed",
                        "Could not save file: " + saveDoc->getFile().getFullPathName());
                }
            }
            else
            {
                // No file path - this shouldn't happen since we checked isModified(),
                // but handle it gracefully
                m_documentManager.closeDocument(saveDoc);
            }
        }
        else if (result == 2) // Don't Save
        {
            m_documentManager.closeDocumentAt(tab->getIndex());
        }
        // result == 0 means Cancel, do nothing
    }
    else
    {
        m_documentManager.closeDocumentAt(tab->getIndex());
    }
}

void TabComponent::tabRightClicked(TabButton* tab, const juce::MouseEvent& event)
{
    showTabContextMenu(tab, event.getScreenPosition());
}

void TabComponent::scrollBarMoved(juce::ScrollBar* scrollBar, double newRangeStart)
{
    m_scrollOffset = static_cast<int>(newRangeStart);
    resized();
}

void TabComponent::updateTabs()
{
    for (int i = 0; i < m_tabs.size(); ++i)
    {
        Document* doc = m_documentManager.getDocument(i);
        if (doc)
        {
            m_tabs[i]->updateDocument(doc);
            m_tabs[i]->setSelected(i == m_documentManager.getCurrentDocumentIndex());
        }
    }
}

void TabComponent::rebuildTabs()
{
    m_tabs.clear();

    for (int i = 0; i < m_documentManager.getNumDocuments(); ++i)
    {
        Document* doc = m_documentManager.getDocument(i);
        if (doc)
        {
            auto* tab = new TabButton(doc, i);
            tab->setListener(this);
            tab->setSelected(i == m_documentManager.getCurrentDocumentIndex());
            m_tabs.add(tab);
            addAndMakeVisible(tab);
        }
    }

    updateScrollBar();
    resized();
}

void TabComponent::updateScrollBar()
{
    int totalWidth = getTotalTabWidth();
    int availableWidth = getWidth();

    m_needsScrollBar = totalWidth > availableWidth;
    m_scrollBar.setVisible(m_needsScrollBar);

    if (m_needsScrollBar)
    {
        m_scrollBar.setRangeLimits(0.0, static_cast<double>(totalWidth));
        m_scrollBar.setCurrentRange(static_cast<double>(m_scrollOffset),
                                   static_cast<double>(availableWidth));
    }
    else
    {
        m_scrollOffset = 0;
    }
}

void TabComponent::showTabContextMenu(TabButton* tab, juce::Point<int> screenPosition)
{
    juce::PopupMenu menu;

    menu.addItem(1, "Close");
    menu.addItem(2, "Close Others");
    menu.addItem(3, "Close All");
    menu.addSeparator();

    // Platform-specific file manager
    #if JUCE_MAC
        menu.addItem(4, "Reveal in Finder");
    #elif JUCE_WINDOWS
        menu.addItem(4, "Show in Explorer");
    #else
        menu.addItem(4, "Show in File Manager");
    #endif

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(juce::Rectangle<int>(screenPosition.x, screenPosition.y, 1, 1)),
                      [this, tab](int result)
                      {
                          if (result == 1) // Close
                          {
                              tabCloseClicked(tab);
                          }
                          else if (result == 2) // Close Others
                          {
                              // Close all tabs except this one
                              for (int i = m_tabs.size() - 1; i >= 0; --i)
                              {
                                  if (m_tabs[i] != tab)
                                  {
                                      m_documentManager.closeDocumentAt(i);
                                  }
                              }
                          }
                          else if (result == 3) // Close All
                          {
                              m_documentManager.closeAllDocuments();
                          }
                          else if (result == 4) // Reveal in file manager
                          {
                              Document* doc = tab->getDocument();
                              if (doc && doc->hasFile())
                              {
                                  doc->getFile().revealToUser();
                              }
                          }
                      });
}

int TabComponent::getTotalTabWidth() const
{
    int total = 0;
    for (auto* tab : m_tabs)
    {
        total += tab->getWidth();
    }
    return total;
}

void TabComponent::ensureCurrentTabVisible()
{
    if (!m_needsScrollBar)
        return;

    int currentIndex = m_documentManager.getCurrentDocumentIndex();
    if (currentIndex >= 0 && currentIndex < m_tabs.size())
    {
        auto* currentTab = m_tabs[currentIndex];
        int tabX = currentTab->getX() + m_scrollOffset;
        int tabRight = tabX + currentTab->getWidth();
        int viewWidth = getWidth();

        if (tabX < 0)
        {
            // Tab is off the left edge, scroll left
            m_scrollBar.setCurrentRangeStart(static_cast<double>(tabX));
        }
        else if (tabRight > viewWidth)
        {
            // Tab is off the right edge, scroll right
            m_scrollBar.setCurrentRangeStart(static_cast<double>(tabRight - viewWidth));
        }
    }
}