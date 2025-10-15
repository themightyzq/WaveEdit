/*
  ==============================================================================

    ErrorDialog.h
    Created: 2025-10-15
    Author:  ZQ SFX
    Copyright (C) 2025 ZQ SFX - All Rights Reserved

  ==============================================================================
*/

#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

/**
 * ErrorDialog - Utility class for displaying user-friendly error messages
 *
 * Replaces silent failures with clear, actionable error dialogs.
 * Follows JUCE best practices for modal dialogs and error handling.
 */
class ErrorDialog
{
public:
    /**
     * Error severity levels for appropriate styling
     */
    enum class Severity
    {
        Info,       // FYI messages (blue icon)
        Warning,    // Warnings (yellow icon)
        Error       // Errors (red icon)
    };

    /**
     * Shows a simple error dialog with just a title and message
     *
     * @param title       Dialog window title
     * @param message     User-friendly error message
     * @param severity    Error severity (affects icon color)
     */
    static void show(const juce::String& title,
                     const juce::String& message,
                     Severity severity = Severity::Error);

    /**
     * Shows an error dialog with optional technical details
     * Displays a "Show Details" button to reveal technical info
     *
     * @param title           Dialog window title
     * @param message         User-friendly error message
     * @param technicalDetails Technical error details (for developers/logs)
     * @param severity        Error severity
     */
    static void showWithDetails(const juce::String& title,
                                const juce::String& message,
                                const juce::String& technicalDetails,
                                Severity severity = Severity::Error);

    /**
     * Shows a file-related error dialog with suggestions
     *
     * @param operation   Operation that failed (e.g., "open", "save")
     * @param filePath    Full path to the file
     * @param reason      Human-readable reason for failure
     */
    static void showFileError(const juce::String& operation,
                               const juce::String& filePath,
                               const juce::String& reason);

    /**
     * Shows an audio device error with action suggestions
     *
     * @param message     User-friendly error message
     * @param suggestion  Suggested action (e.g., "Go to Preferences...")
     */
    static void showAudioDeviceError(const juce::String& message,
                                      const juce::String& suggestion = "");

private:
    /**
     * Gets JUCE MessageBoxIconType from our Severity enum
     */
    static juce::MessageBoxIconType getIconType(Severity severity);

    /**
     * Formats technical details for display
     */
    static juce::String formatTechnicalDetails(const juce::String& details);
};
