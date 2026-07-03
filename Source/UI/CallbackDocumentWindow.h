/*
  ==============================================================================

    CallbackDocumentWindow.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    DocumentWindow variant that hides itself on close and notifies an
    optional callback. Extracted from MainComponent.h per the Sec 8.1
    module boundaries.

  ==============================================================================
*/

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

//==============================================================================
/**
 * Custom DocumentWindow that supports a close button callback.
 * Used for spectrum analyzer and other floating windows that need to
 * synchronize their visibility state with menu checkmarks.
 *
 * Thread Safety: The close callback is executed asynchronously on the
 * message thread using MessageManager::callAsync() to ensure safe
 * interaction with audio engine and UI state.
 *
 * @param name Window title
 * @param backgroundColour Window background color
 * @param requiredButtons Button configuration (DocumentWindow::allButtons, etc.)
 * @param onCloseCallback Optional callback invoked when close button pressed (moved for efficiency)
 */
class CallbackDocumentWindow : public juce::DocumentWindow
{
public:
    CallbackDocumentWindow(const juce::String& name,
                          juce::Colour backgroundColour,
                          int requiredButtons,
                          std::function<void()> onCloseCallback = nullptr)
        : juce::DocumentWindow(name, backgroundColour, requiredButtons),
          m_onCloseCallback(std::move(onCloseCallback))  // Move instead of copy
    {
    }

    void closeButtonPressed() override
    {
        // Hide the window instead of deleting it
        setVisible(false);

        // Invoke the callback if provided (already on message thread, but ensure safety)
        if (m_onCloseCallback)
        {
            juce::MessageManager::callAsync(m_onCloseCallback);
        }
    }

private:
    std::function<void()> m_onCloseCallback;
};
