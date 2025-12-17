/*
  ==============================================================================

    PluginScanDialogs.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "PluginScanDialogs.h"

//==============================================================================
// PluginScanErrorDialog Implementation
//==============================================================================

PluginScanErrorDialog::PluginScanErrorDialog(const juce::String& pluginName,
                                               const juce::String& errorMessage,
                                               bool isCrash)
    : m_pluginName(pluginName),
      m_errorMessage(errorMessage),
      m_isCrash(isCrash)
{
    // Title
    if (isCrash)
    {
        m_titleLabel.setText("Plugin Crashed During Scan", juce::dontSendNotification);
        m_titleLabel.setColour(juce::Label::textColourId, juce::Colours::orangered);
    }
    else
    {
        m_titleLabel.setText("Plugin Scan Failed", juce::dontSendNotification);
        m_titleLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    }
    m_titleLabel.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Plugin name
    juce::File pluginFile(pluginName);
    juce::String displayName = pluginFile.existsAsFile()
        ? pluginFile.getFileNameWithoutExtension()
        : pluginName;

    m_pluginLabel.setText("Plugin: " + displayName, juce::dontSendNotification);
    m_pluginLabel.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    addAndMakeVisible(m_pluginLabel);

    // Error message
    m_errorLabel.setText("Error: " + errorMessage, juce::dontSendNotification);
    m_errorLabel.setFont(juce::FontOptions(12.0f));
    m_errorLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(m_errorLabel);

    // Hint
    if (isCrash)
    {
        m_hintLabel.setText("This plugin caused the scanner to crash. It may be incompatible "
                            "or corrupted. You can skip it and it will be added to the blacklist.",
                            juce::dontSendNotification);
    }
    else
    {
        m_hintLabel.setText("You can retry scanning this plugin, skip it and continue with "
                            "the remaining plugins, or cancel the entire scan.",
                            juce::dontSendNotification);
    }
    m_hintLabel.setFont(juce::FontOptions(11.0f));
    m_hintLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    m_hintLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_hintLabel);

    // Buttons
    m_retryButton.setButtonText("Retry");
    m_retryButton.onClick = [this]() { onRetryClicked(); };
    // Disable retry for crashes - plugin will just crash again
    m_retryButton.setEnabled(!isCrash);
    addAndMakeVisible(m_retryButton);

    m_skipButton.setButtonText("Skip");
    m_skipButton.onClick = [this]() { onSkipClicked(); };
    addAndMakeVisible(m_skipButton);

    m_cancelButton.setButtonText("Cancel Scan");
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    setSize(450, 220);
}

PluginScanErrorDialog::~PluginScanErrorDialog() = default;

void PluginScanErrorDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));

    // Warning icon area
    auto iconBounds = getLocalBounds().removeFromTop(50).reduced(10);
    g.setColour(m_isCrash ? juce::Colours::orangered : juce::Colours::orange);
    g.setFont(juce::FontOptions(32.0f));
    g.drawText(m_isCrash ? "!" : "?", iconBounds, juce::Justification::centred);
}

void PluginScanErrorDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // Skip icon area
    bounds.removeFromTop(40);

    m_titleLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(15);

    m_pluginLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(5);

    m_errorLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(10);

    m_hintLabel.setBounds(bounds.removeFromTop(40));

    // Buttons at bottom
    auto buttonRow = bounds.removeFromBottom(35);
    int buttonWidth = 100;
    int spacing = 15;
    int totalWidth = buttonWidth * 3 + spacing * 2;
    int startX = (buttonRow.getWidth() - totalWidth) / 2;

    m_retryButton.setBounds(startX, buttonRow.getY(), buttonWidth, 30);
    m_skipButton.setBounds(startX + buttonWidth + spacing, buttonRow.getY(), buttonWidth, 30);
    m_cancelButton.setBounds(startX + (buttonWidth + spacing) * 2, buttonRow.getY(), buttonWidth, 30);
}

void PluginScanErrorDialog::onRetryClicked()
{
    m_result = Result::Retry;
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(static_cast<int>(Result::Retry));
}

void PluginScanErrorDialog::onSkipClicked()
{
    m_result = Result::Skip;
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(static_cast<int>(Result::Skip));
}

void PluginScanErrorDialog::onCancelClicked()
{
    m_result = Result::Cancel;
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(static_cast<int>(Result::Cancel));
}

PluginScanErrorDialog::Result PluginScanErrorDialog::showDialog(
    const juce::String& pluginName,
    const juce::String& errorMessage,
    bool isCrash)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Create the dialog content
    auto dialog = std::make_unique<PluginScanErrorDialog>(pluginName, errorMessage, isCrash);

    // Create the dialog window
    juce::DialogWindow dlg(isCrash ? "Plugin Crashed" : "Plugin Scan Error",
                           juce::Colour(0xff2a2a2a), true, false);
    dlg.setContentOwned(dialog.release(), true);
    dlg.centreWithSize(450, 220);
    dlg.setResizable(false, false);
    dlg.setUsingNativeTitleBar(true);

    // Add to desktop
    dlg.addToDesktop(juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowHasCloseButton);
    dlg.setVisible(true);
    dlg.toFront(true);

    // Run modal loop
    int result = 0;
#if JUCE_MODAL_LOOPS_PERMITTED
    dlg.enterModalState(true);
    result = dlg.runModalLoop();
#else
    jassertfalse; // Modal loops are required
    return Result::Cancel;
#endif

    // Map the result
    switch (result)
    {
        case static_cast<int>(Result::Retry):
            return Result::Retry;
        case static_cast<int>(Result::Skip):
            return Result::Skip;
        case static_cast<int>(Result::Cancel):
        default:
            return Result::Cancel;
    }
}

//==============================================================================
// PluginScanSummaryDialog Implementation
//==============================================================================

PluginScanSummaryDialog::PluginScanSummaryDialog(const PluginScanSummary& summary)
    : m_successCount(summary.successCount),
      m_failedCount(summary.failedCount),
      m_skippedCount(summary.skippedCount),
      m_cachedCount(summary.cachedCount),
      m_totalPlugins(summary.getTotalPluginsFound()),
      m_duration(summary.getScanDuration()),
      m_failedResults(summary.getFailedResults()),
      m_summary(summary)
{
    // Title
    m_titleLabel.setText("Plugin Scan Complete", juce::dontSendNotification);
    m_titleLabel.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Summary
    juce::String summaryText;
    summaryText << "Found " << juce::String(m_totalPlugins) << " plugins in "
                << juce::String(static_cast<int>(m_duration.inSeconds())) << " seconds.\n\n";
    summaryText << "  Scanned successfully: " << juce::String(m_successCount) << "\n";
    summaryText << "  From cache (unchanged): " << juce::String(m_cachedCount) << "\n";
    summaryText << "  Failed: " << juce::String(m_failedCount) << "\n";
    summaryText << "  Skipped/Blacklisted: " << juce::String(m_skippedCount);

    m_summaryLabel.setText(summaryText, juce::dontSendNotification);
    m_summaryLabel.setFont(juce::FontOptions(12.0f));
    m_summaryLabel.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(m_summaryLabel);

    // Details label (if there are failures)
    if (m_failedCount > 0)
    {
        m_detailsLabel.setText("Failed Plugins:", juce::dontSendNotification);
        m_detailsLabel.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
        addAndMakeVisible(m_detailsLabel);

        // Failed plugins table
        m_failedTable.setModel(this);
        m_failedTable.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1e1e1e));
        m_failedTable.setRowHeight(24);
        m_failedTable.getHeader().addColumn("Status", StatusColumn, 60, 50, 80);
        m_failedTable.getHeader().addColumn("Plugin", NameColumn, 200, 100, 400);
        m_failedTable.getHeader().addColumn("Reason", ReasonColumn, 200, 100, 400);
        m_failedTable.getHeader().setStretchToFitActive(true);
        addAndMakeVisible(m_failedTable);
    }

    // Buttons
    m_closeButton.setButtonText("Close");
    m_closeButton.onClick = [this]() { onCloseClicked(); };
    addAndMakeVisible(m_closeButton);

    m_viewLogButton.setButtonText("View Log");
    m_viewLogButton.onClick = [this]() { onViewLogClicked(); };
    m_viewLogButton.setEnabled(true);  // Always enabled - generates log on demand
    addAndMakeVisible(m_viewLogButton);

    int height = m_failedCount > 0 ? 450 : 250;
    setSize(500, height);
}

PluginScanSummaryDialog::~PluginScanSummaryDialog() = default;

void PluginScanSummaryDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));

    // Success/warning icon
    auto iconBounds = getLocalBounds().removeFromTop(50).reduced(10);
    if (m_failedCount == 0)
    {
        g.setColour(juce::Colours::lightgreen);
        g.setFont(juce::FontOptions(32.0f));
        g.drawText(juce::CharPointer_UTF8("\xe2\x9c\x93"), iconBounds, juce::Justification::centred); // ✓
    }
    else
    {
        g.setColour(juce::Colours::orange);
        g.setFont(juce::FontOptions(32.0f));
        g.drawText("!", iconBounds, juce::Justification::centred);
    }
}

void PluginScanSummaryDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // Skip icon area
    bounds.removeFromTop(40);

    m_titleLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(15);

    // Summary text (5 lines)
    m_summaryLabel.setBounds(bounds.removeFromTop(100));
    bounds.removeFromTop(10);

    // Buttons at bottom
    auto buttonRow = bounds.removeFromBottom(35);
    int buttonWidth = 100;
    int spacing = 15;
    m_closeButton.setBounds(buttonRow.getRight() - buttonWidth, buttonRow.getY(), buttonWidth, 30);
    m_viewLogButton.setBounds(buttonRow.getRight() - buttonWidth * 2 - spacing, buttonRow.getY(), buttonWidth, 30);

    bounds.removeFromBottom(10);

    // Failed table (if visible)
    if (m_failedCount > 0)
    {
        m_detailsLabel.setBounds(bounds.removeFromTop(20));
        bounds.removeFromTop(5);
        m_failedTable.setBounds(bounds);
    }
}

int PluginScanSummaryDialog::getNumRows()
{
    return m_failedResults.size();
}

void PluginScanSummaryDialog::paintRowBackground(juce::Graphics& g, int rowNumber,
                                                   int /*width*/, int /*height*/,
                                                   bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(juce::Colour(0xff3a3a3a));
    else if (rowNumber % 2 == 1)
        g.fillAll(juce::Colour(0xff252525));
    else
        g.fillAll(juce::Colour(0xff1e1e1e));
}

void PluginScanSummaryDialog::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                                          int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= m_failedResults.size())
        return;

    const auto& result = m_failedResults[rowNumber];

    g.setFont(juce::FontOptions(12.0f));

    switch (columnId)
    {
        case StatusColumn:
        {
            juce::String statusIcon;
            juce::Colour statusColour;

            switch (result.status)
            {
                case PluginScanResult::Status::Crashed:
                    statusIcon = "CRASH";
                    statusColour = juce::Colours::orangered;
                    break;
                case PluginScanResult::Status::Timeout:
                    statusIcon = "TIMEOUT";
                    statusColour = juce::Colours::orange;
                    break;
                case PluginScanResult::Status::Failed:
                default:
                    statusIcon = "FAIL";
                    statusColour = juce::Colours::indianred;
                    break;
            }

            g.setColour(statusColour);
            g.drawText(statusIcon, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
            break;
        }

        case NameColumn:
            g.setColour(juce::Colours::white);
            g.drawText(result.pluginName, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
            break;

        case ReasonColumn:
            g.setColour(juce::Colours::lightgrey);
            g.drawText(result.errorMessage, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
            break;

        default:
            break;
    }
}

void PluginScanSummaryDialog::selectedRowsChanged(int /*lastRowSelected*/)
{
    // Could show more details about selected failure
}

void PluginScanSummaryDialog::onCloseClicked()
{
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(0);
}

void PluginScanSummaryDialog::onViewLogClicked()
{
    // Generate a scan log file and open it with the system default text editor
    juce::File logDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                            .getChildFile("WaveEdit");
    logDir.createDirectory();

    juce::File logFile = logDir.getChildFile("scan_log.txt");

    // Generate log content
    juce::String logContent;
    logContent << "=== WaveEdit Plugin Scan Report ===" << juce::newLine;
    logContent << "Generated: " << juce::Time::getCurrentTime().toString(true, true) << juce::newLine;
    logContent << juce::newLine;

    logContent << "=== Summary ===" << juce::newLine;
    logContent << "Total Plugins Found: " << m_totalPlugins << juce::newLine;
    logContent << "Scanned Successfully: " << m_successCount << juce::newLine;
    logContent << "From Cache: " << m_cachedCount << juce::newLine;
    logContent << "Failed: " << m_failedCount << juce::newLine;
    logContent << "Skipped/Blacklisted: " << m_skippedCount << juce::newLine;
    logContent << "Scan Duration: " << juce::String(static_cast<int>(m_duration.inSeconds())) << " seconds" << juce::newLine;
    logContent << juce::newLine;

    // List all results
    bool hasSuccess = false;
    bool hasFailed = false;

    for (const auto& result : m_summary.results)
    {
        if (result.isSuccess() || result.descriptions.size() > 0)
            hasSuccess = true;
        if (result.isFailed())
            hasFailed = true;
    }

    if (hasSuccess)
    {
        logContent << "=== Successfully Scanned Plugins ===" << juce::newLine;
        for (const auto& result : m_summary.results)
        {
            if (result.isSuccess() || result.descriptions.size() > 0)
            {
                for (const auto& desc : result.descriptions)
                    logContent << "  [OK] " << desc.name << " (" << result.pluginPath << ")" << juce::newLine;
            }
        }
        logContent << juce::newLine;
    }

    if (hasFailed)
    {
        logContent << "=== Failed Plugins ===" << juce::newLine;
        for (const auto& result : m_summary.results)
        {
            if (result.isFailed())
                logContent << "  [FAILED] " << result.pluginPath << " - " << result.errorMessage << juce::newLine;
        }
        logContent << juce::newLine;
    }

    logContent << "=== End of Report ===" << juce::newLine;

    // Write to file
    if (logFile.replaceWithText(logContent))
    {
        // Open with system default application
        logFile.startAsProcess();
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "View Log",
            "Failed to write log file to:\n" + logFile.getFullPathName());
    }
}

void PluginScanSummaryDialog::showDialog(const PluginScanSummary& summary)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Create the dialog content
    auto dialog = std::make_unique<PluginScanSummaryDialog>(summary);

    // Create the dialog window
    juce::DialogWindow dlg("Scan Summary", juce::Colour(0xff2a2a2a), true, false);
    dlg.setContentOwned(dialog.release(), true);
    dlg.centreWithSize(500, 450);
    dlg.setResizable(true, true);
    dlg.setUsingNativeTitleBar(true);

    // Add to desktop
    dlg.addToDesktop(juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowHasCloseButton);
    dlg.setVisible(true);
    dlg.toFront(true);

    // Run modal loop
#if JUCE_MODAL_LOOPS_PERMITTED
    dlg.enterModalState(true);
    dlg.runModalLoop();
#else
    jassertfalse; // Modal loops are required
#endif
}

//==============================================================================
// PluginTimeoutDialog Implementation
//==============================================================================

PluginTimeoutDialog::PluginTimeoutDialog(const juce::String& pluginName, int timeoutSeconds)
    : m_pluginName(pluginName)
{
    // Title
    m_titleLabel.setText("Plugin Taking Longer Than Expected", juce::dontSendNotification);
    m_titleLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    m_titleLabel.setFont(juce::FontOptions(18.0f).withStyle("Bold"));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Plugin name
    juce::File pluginFile(pluginName);
    juce::String displayName = pluginFile.existsAsFile()
        ? pluginFile.getFileNameWithoutExtension()
        : pluginName;

    m_pluginLabel.setText("Plugin: " + displayName, juce::dontSendNotification);
    m_pluginLabel.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    addAndMakeVisible(m_pluginLabel);

    // Message
    m_messageLabel.setText("This plugin has been loading for " + juce::String(timeoutSeconds) +
                            " seconds. Complex plugins with AI/ML features, large sample "
                            "libraries, or license validation may require extra time.",
                            juce::dontSendNotification);
    m_messageLabel.setFont(juce::FontOptions(12.0f));
    m_messageLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    m_messageLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_messageLabel);

    // Hint
    m_hintLabel.setText("Choose an action:",
                         juce::dontSendNotification);
    m_hintLabel.setFont(juce::FontOptions(11.0f));
    m_hintLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    m_hintLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_hintLabel);

    // Buttons
    m_waitLongerButton.setButtonText("Wait Longer");
    m_waitLongerButton.onClick = [this]() { onWaitLongerClicked(); };
    m_waitLongerButton.setTooltip("Wait another 60 seconds for the plugin to load");
    addAndMakeVisible(m_waitLongerButton);

    m_skipButton.setButtonText("Skip");
    m_skipButton.onClick = [this]() { onSkipClicked(); };
    m_skipButton.setTooltip("Skip this plugin for now (can rescan later)");
    addAndMakeVisible(m_skipButton);

    m_blacklistButton.setButtonText("Always Skip");
    m_blacklistButton.onClick = [this]() { onBlacklistClicked(); };
    m_blacklistButton.setTooltip("Skip and add to blacklist (never scan again)");
    m_blacklistButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff8B4513));
    addAndMakeVisible(m_blacklistButton);

    setSize(500, 260);
}

PluginTimeoutDialog::~PluginTimeoutDialog() = default;

void PluginTimeoutDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));

    // Clock/timer icon area
    auto iconBounds = getLocalBounds().removeFromTop(50).reduced(10);
    g.setColour(juce::Colours::orange);
    g.setFont(juce::FontOptions(32.0f));
    g.drawText(juce::CharPointer_UTF8("\xe2\x8f\xb1"), iconBounds, juce::Justification::centred); // hourglass
}

void PluginTimeoutDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    // Skip icon area
    bounds.removeFromTop(40);

    m_titleLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(15);

    m_pluginLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(10);

    m_messageLabel.setBounds(bounds.removeFromTop(55));
    bounds.removeFromTop(10);

    m_hintLabel.setBounds(bounds.removeFromTop(20));

    // Buttons at bottom
    auto buttonRow = bounds.removeFromBottom(35);
    int buttonWidth = 120;
    int spacing = 15;
    int totalWidth = buttonWidth * 3 + spacing * 2;
    int startX = (buttonRow.getWidth() - totalWidth) / 2;

    m_waitLongerButton.setBounds(startX, buttonRow.getY(), buttonWidth, 30);
    m_skipButton.setBounds(startX + buttonWidth + spacing, buttonRow.getY(), buttonWidth, 30);
    m_blacklistButton.setBounds(startX + (buttonWidth + spacing) * 2, buttonRow.getY(), buttonWidth, 30);
}

void PluginTimeoutDialog::onWaitLongerClicked()
{
    m_result = Result::WaitLonger;
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(static_cast<int>(Result::WaitLonger));
}

void PluginTimeoutDialog::onSkipClicked()
{
    m_result = Result::Skip;
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(static_cast<int>(Result::Skip));
}

void PluginTimeoutDialog::onBlacklistClicked()
{
    m_result = Result::Blacklist;
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState(static_cast<int>(Result::Blacklist));
}

PluginTimeoutDialog::Result PluginTimeoutDialog::showDialog(
    const juce::String& pluginName,
    int timeoutSeconds)
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    // Create the dialog content
    auto dialog = std::make_unique<PluginTimeoutDialog>(pluginName, timeoutSeconds);

    // Create the dialog window
    juce::DialogWindow dlg("Plugin Timeout", juce::Colour(0xff2a2a2a), true, false);
    dlg.setContentOwned(dialog.release(), true);
    dlg.centreWithSize(500, 260);
    dlg.setResizable(false, false);
    dlg.setUsingNativeTitleBar(true);

    // Add to desktop
    dlg.addToDesktop(juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowHasCloseButton);
    dlg.setVisible(true);
    dlg.toFront(true);

    // Run modal loop
    int result = 0;
#if JUCE_MODAL_LOOPS_PERMITTED
    dlg.enterModalState(true);
    result = dlg.runModalLoop();
#else
    jassertfalse; // Modal loops are required
    return Result::Skip;  // Default to skip if no modal support
#endif

    // Map the result
    switch (result)
    {
        case static_cast<int>(Result::WaitLonger):
            return Result::WaitLonger;
        case static_cast<int>(Result::Blacklist):
            return Result::Blacklist;
        case static_cast<int>(Result::Skip):
        default:
            return Result::Skip;
    }
}

//==============================================================================
// PluginScanProgressDialog Implementation
//==============================================================================

PluginScanProgressDialog::PluginScanProgressDialog(CancelCallback onCancel)
    : m_progressBar(m_progressValue),
      m_onCancel(std::move(onCancel))
{
    // Title
    m_titleLabel.setText("Scanning Plugins...", juce::dontSendNotification);
    m_titleLabel.setFont(juce::FontOptions(16.0f).withStyle("Bold"));
    m_titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_titleLabel);

    // Status
    m_statusLabel.setText("Preparing scan...", juce::dontSendNotification);
    m_statusLabel.setFont(juce::FontOptions(12.0f));
    m_statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_statusLabel);

    // Current plugin
    m_currentPluginLabel.setText("", juce::dontSendNotification);
    m_currentPluginLabel.setFont(juce::FontOptions(11.0f));
    m_currentPluginLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    m_currentPluginLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_currentPluginLabel);

    // Progress bar
    m_progressBar.setPercentageDisplay(true);
    m_progressBar.setColour(juce::ProgressBar::backgroundColourId, juce::Colour(0xff333333));
    m_progressBar.setColour(juce::ProgressBar::foregroundColourId, juce::Colour(0xff4a9eff));
    addAndMakeVisible(m_progressBar);

    // Elapsed time
    m_elapsedTimeLabel.setText("Elapsed: 0:00", juce::dontSendNotification);
    m_elapsedTimeLabel.setFont(juce::FontOptions(11.0f));
    m_elapsedTimeLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    m_elapsedTimeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(m_elapsedTimeLabel);

    // Cancel button
    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    addAndMakeVisible(m_cancelButton);

    m_startTime = juce::Time::getCurrentTime();
    startTimer(100);  // Update UI 10 times per second for smooth progress

    setSize(400, 200);
}

PluginScanProgressDialog::~PluginScanProgressDialog()
{
    stopTimer();
}

void PluginScanProgressDialog::setProgressData(float progress, const juce::String& currentPlugin,
                                                 int currentIndex, int totalCount)
{
    // Thread-safe: store values atomically
    m_atomicProgress.store(progress);
    m_atomicCurrentIndex.store(currentIndex);
    m_atomicTotalCount.store(totalCount);
    {
        const juce::ScopedLock sl(m_pluginNameLock);
        m_atomicPluginName = currentPlugin;
    }
    m_hasNewData.store(true);

    // Note: Actual UI update happens in timerCallback() on message thread
}

void PluginScanProgressDialog::updateUIFromAtomics()
{
    // Only called from message thread via timerCallback()
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    if (!m_hasNewData.exchange(false))
        return;  // No new data to display

    float progress = m_atomicProgress.load();
    int currentIndex = m_atomicCurrentIndex.load();
    int totalCount = m_atomicTotalCount.load();
    juce::String pluginName;
    {
        const juce::ScopedLock sl(m_pluginNameLock);
        pluginName = m_atomicPluginName;
    }

    m_progressValue = static_cast<double>(progress);

    m_statusLabel.setText("Scanning plugin " + juce::String(currentIndex + 1) +
                          " of " + juce::String(totalCount),
                          juce::dontSendNotification);

    // Truncate long plugin names
    juce::String displayName = pluginName;
    if (displayName.length() > 50)
        displayName = "..." + displayName.substring(displayName.length() - 47);

    m_currentPluginLabel.setText(displayName, juce::dontSendNotification);

    m_progressBar.repaint();
}

void PluginScanProgressDialog::setComplete()
{
    m_isComplete.store(true);

    // If called from background thread, schedule UI update
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        juce::MessageManager::callAsync([this]()
        {
            stopTimer();
            m_progressValue = 1.0;
            m_statusLabel.setText("Scan complete!", juce::dontSendNotification);
            m_cancelButton.setButtonText("Close");
            m_progressBar.repaint();
        });
    }
    else
    {
        stopTimer();
        m_progressValue = 1.0;
        m_statusLabel.setText("Scan complete!", juce::dontSendNotification);
        m_cancelButton.setButtonText("Close");
        m_progressBar.repaint();
    }
}

void PluginScanProgressDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));
}

void PluginScanProgressDialog::resized()
{
    auto bounds = getLocalBounds().reduced(20);

    m_titleLabel.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(15);

    m_statusLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(5);

    m_currentPluginLabel.setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(10);

    m_progressBar.setBounds(bounds.removeFromTop(25));
    bounds.removeFromTop(10);

    m_elapsedTimeLabel.setBounds(bounds.removeFromTop(18));

    // Cancel button at bottom
    auto buttonRow = bounds.removeFromBottom(35);
    int buttonWidth = 100;
    m_cancelButton.setBounds((buttonRow.getWidth() - buttonWidth) / 2,
                              buttonRow.getY(), buttonWidth, 30);
}

void PluginScanProgressDialog::timerCallback()
{
    // Update UI from atomically-stored progress data
    updateUIFromAtomics();

    // Update elapsed time display
    auto elapsed = juce::Time::getCurrentTime() - m_startTime;
    int seconds = static_cast<int>(elapsed.inSeconds());
    int minutes = seconds / 60;
    seconds = seconds % 60;

    m_elapsedTimeLabel.setText("Elapsed: " + juce::String(minutes) + ":" +
                                juce::String(seconds).paddedLeft('0', 2),
                                juce::dontSendNotification);
}

void PluginScanProgressDialog::onCancelClicked()
{
    if (m_isComplete.load())
    {
        // Just close if complete
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState(0);
    }
    else if (m_onCancel)
    {
        m_onCancel();
    }
}

juce::DialogWindow* PluginScanProgressDialog::showInWindow()
{
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Plugin Scan";
    options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
    options.escapeKeyTriggersCloseButton = false;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.content.setNonOwned(this);

    return options.launchAsync();
}
