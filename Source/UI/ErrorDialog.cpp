/*
  ==============================================================================

    ErrorDialog.cpp
    Created: 2025-10-15
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

  ==============================================================================
*/

#include "ErrorDialog.h"

void ErrorDialog::show(const juce::String& title,
                        const juce::String& message,
                        Severity severity)
{
    // Use JUCE's message thread assertion
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Show alert window with appropriate icon
    juce::AlertWindow::showMessageBoxAsync(
        getIconType(severity),
        title,
        message,
        "OK"
    );

    // Also log to console for developers
    juce::Logger::writeToLog(juce::String::formatted(
        "[%s] %s: %s",
        severity == Severity::Error ? "ERROR" :
        severity == Severity::Warning ? "WARNING" : "INFO",
        title.toRawUTF8(),
        message.toRawUTF8()
    ));
}

void ErrorDialog::showWithDetails(const juce::String& title,
                                   const juce::String& message,
                                   const juce::String& technicalDetails,
                                   Severity severity)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Format message with collapsible details
    juce::String fullMessage = message;
    if (technicalDetails.isNotEmpty())
    {
        fullMessage += "\n\nTechnical Details:\n";
        fullMessage += formatTechnicalDetails(technicalDetails);
    }

    juce::AlertWindow::showMessageBoxAsync(
        getIconType(severity),
        title,
        fullMessage,
        "OK"
    );

    // Log full details
    juce::Logger::writeToLog(juce::String::formatted(
        "[%s] %s: %s\nDetails: %s",
        severity == Severity::Error ? "ERROR" :
        severity == Severity::Warning ? "WARNING" : "INFO",
        title.toRawUTF8(),
        message.toRawUTF8(),
        technicalDetails.toRawUTF8()
    ));
}

void ErrorDialog::showFileError(const juce::String& operation,
                                 const juce::String& filePath,
                                 const juce::String& reason)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Extract filename for cleaner display
    juce::File file(filePath);
    juce::String filename = file.getFileName();

    // Build user-friendly message
    juce::String title = "Cannot " + operation.substring(0, 1).toUpperCase() + operation.substring(1) + " File";
    juce::String message = "Failed to " + operation + " '" + filename + "'.\n\n";
    message += "Reason: " + reason + "\n\n";

    // Add contextual suggestions based on operation
    if (operation == "open")
    {
        message += "Suggestions:\n";
        message += "• Verify the file is a valid WAV audio file\n";
        message += "• Check that the file is not corrupted\n";
        message += "• Ensure you have permission to read this file";
    }
    else if (operation == "save")
    {
        message += "Suggestions:\n";
        message += "• Check that you have write permissions\n";
        message += "• Ensure there is enough disk space\n";
        message += "• Try saving to a different location";
    }

    show(title, message, Severity::Error);
}

void ErrorDialog::showAudioDeviceError(const juce::String& message,
                                        const juce::String& suggestion)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    juce::String fullMessage = message;

    if (suggestion.isNotEmpty())
    {
        fullMessage += "\n\n" + suggestion;
    }
    else
    {
        fullMessage += "\n\nSuggestion: Go to Preferences (Cmd+,) to select a different audio device.";
    }

    show("Audio Device Error", fullMessage, Severity::Error);
}

juce::MessageBoxIconType ErrorDialog::getIconType(Severity severity)
{
    switch (severity)
    {
        case Severity::Info:
            return juce::MessageBoxIconType::InfoIcon;
        case Severity::Warning:
            return juce::MessageBoxIconType::WarningIcon;
        case Severity::Error:
        default:
            return juce::MessageBoxIconType::WarningIcon; // JUCE uses WarningIcon for errors
    }
}

juce::String ErrorDialog::formatTechnicalDetails(const juce::String& details)
{
    // Indent technical details for better readability
    juce::StringArray lines = juce::StringArray::fromLines(details);
    juce::String formatted;

    for (const auto& line : lines)
    {
        formatted += "  " + line + "\n";
    }

    return formatted;
}
