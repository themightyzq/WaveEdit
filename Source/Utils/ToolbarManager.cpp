/*
  ==============================================================================

    ToolbarManager.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "ToolbarManager.h"
#include "Settings.h"

//==============================================================================
ToolbarManager::ToolbarManager()
{
    createBuiltInLayouts();
    loadBuiltInTemplates();
    scanUserLayouts();
    loadFromSettings();
}

ToolbarManager::~ToolbarManager()
{
}

//==============================================================================
juce::StringArray ToolbarManager::getAvailableLayouts() const
{
    juce::StringArray layouts;

    // Add built-in layouts
    for (const auto& pair : m_builtInLayouts)
        layouts.add(pair.first);

    // Add user layouts (skip duplicates)
    for (const auto& pair : m_userLayouts)
    {
        bool isDuplicate = false;
        for (const auto& existing : layouts)
        {
            if (existing.equalsIgnoreCase(pair.first))
            {
                isDuplicate = true;
                break;
            }
        }
        if (!isDuplicate)
            layouts.add(pair.first);
    }

    return layouts;
}

juce::String ToolbarManager::getCurrentLayoutName() const
{
    return m_currentLayoutName;
}

bool ToolbarManager::loadLayout(const juce::String& layoutName)
{
    // Check built-in layouts first
    auto builtInIt = m_builtInLayouts.find(layoutName);
    if (builtInIt != m_builtInLayouts.end())
    {
        m_currentLayout = builtInIt->second;
        m_currentLayoutName = layoutName;
        saveToSettings();
        notifyListeners();
        juce::Logger::writeToLog("ToolbarManager: Loaded built-in layout: " + layoutName);
        return true;
    }

    // Check user layouts
    auto userIt = m_userLayouts.find(layoutName);
    if (userIt != m_userLayouts.end())
    {
        m_currentLayout = ToolbarLayout::fromJSON(userIt->second);
        m_currentLayoutName = layoutName;
        saveToSettings();
        notifyListeners();
        juce::Logger::writeToLog("ToolbarManager: Loaded user layout: " + layoutName);
        return true;
    }

    juce::Logger::writeToLog("ToolbarManager: Layout not found: " + layoutName);
    return false;
}

const ToolbarLayout& ToolbarManager::getCurrentLayout() const
{
    return m_currentLayout;
}

bool ToolbarManager::layoutExists(const juce::String& layoutName) const
{
    return m_builtInLayouts.find(layoutName) != m_builtInLayouts.end() ||
           m_userLayouts.find(layoutName) != m_userLayouts.end();
}

//==============================================================================
bool ToolbarManager::importLayout(const juce::File& file, bool makeActive)
{
    ToolbarLayout layout = ToolbarLayout::fromJSON(file);

    if (layout.name.isEmpty())
    {
        juce::Logger::writeToLog("ToolbarManager: Failed to import layout - invalid JSON");
        return false;
    }

    // Validate layout
    juce::StringArray errors;
    if (!layout.validate(errors))
    {
        juce::Logger::writeToLog("ToolbarManager: Layout validation failed:");
        for (const auto& error : errors)
            juce::Logger::writeToLog("  " + error);
        return false;
    }

    // Copy to user layouts directory
    juce::File destFile = getToolbarsDirectory().getChildFile(file.getFileName());
    if (!file.copyFileTo(destFile))
    {
        juce::Logger::writeToLog("ToolbarManager: Failed to copy layout file");
        return false;
    }

    // Add to user layouts map
    m_userLayouts[layout.name] = destFile;

    if (makeActive)
        loadLayout(layout.name);

    juce::Logger::writeToLog("ToolbarManager: Successfully imported layout: " + layout.name);
    return true;
}

bool ToolbarManager::exportCurrentLayout(const juce::File& file) const
{
    return m_currentLayout.saveToJSON(file);
}

bool ToolbarManager::exportLayout(const juce::String& layoutName, const juce::File& file) const
{
    auto builtInIt = m_builtInLayouts.find(layoutName);
    if (builtInIt != m_builtInLayouts.end())
        return builtInIt->second.saveToJSON(file);

    auto userIt = m_userLayouts.find(layoutName);
    if (userIt != m_userLayouts.end())
    {
        ToolbarLayout layout = ToolbarLayout::fromJSON(userIt->second);
        return layout.saveToJSON(file);
    }

    return false;
}

//==============================================================================
bool ToolbarManager::saveCurrentLayoutAs(const juce::String& newName)
{
    if (newName.isEmpty())
        return false;

    // Create a copy with new name
    ToolbarLayout newLayout = m_currentLayout;
    newLayout.name = newName;

    // Save to user layouts directory
    juce::File destFile = getToolbarsDirectory().getChildFile(newName + ".json");
    if (!newLayout.saveToJSON(destFile))
    {
        juce::Logger::writeToLog("ToolbarManager: Failed to save layout: " + newName);
        return false;
    }

    // Add to user layouts map
    m_userLayouts[newName] = destFile;

    juce::Logger::writeToLog("ToolbarManager: Saved layout as: " + newName);
    return true;
}

void ToolbarManager::updateCurrentLayout(const ToolbarLayout& layout)
{
    m_currentLayout = layout;
    m_currentLayoutName = layout.name;

    // Save to user layouts directory if it's a user layout (or create as user layout)
    // This allows modifications to built-in layouts to be saved as user overrides
    juce::File destFile = getToolbarsDirectory().getChildFile(layout.name + ".json");
    if (layout.saveToJSON(destFile))
    {
        m_userLayouts[layout.name] = destFile;
        juce::Logger::writeToLog("ToolbarManager: Updated and saved layout: " + layout.name);
    }

    // Notify listeners so the toolbar updates
    notifyListeners();
}

bool ToolbarManager::deleteLayout(const juce::String& layoutName)
{
    // Cannot delete built-in layouts
    if (isBuiltInLayout(layoutName))
    {
        juce::Logger::writeToLog("ToolbarManager: Cannot delete built-in layout: " + layoutName);
        return false;
    }

    auto userIt = m_userLayouts.find(layoutName);
    if (userIt == m_userLayouts.end())
    {
        juce::Logger::writeToLog("ToolbarManager: Layout not found: " + layoutName);
        return false;
    }

    // Delete the file
    juce::File layoutFile = userIt->second;
    if (!layoutFile.deleteFile())
    {
        juce::Logger::writeToLog("ToolbarManager: Failed to delete layout file: " + layoutName);
        return false;
    }

    // Remove from map
    m_userLayouts.erase(userIt);

    // If current layout was deleted, switch to default
    if (m_currentLayoutName == layoutName)
        loadLayout("Default");

    juce::Logger::writeToLog("ToolbarManager: Deleted layout: " + layoutName);
    return true;
}

bool ToolbarManager::isBuiltInLayout(const juce::String& layoutName) const
{
    return m_builtInLayouts.find(layoutName) != m_builtInLayouts.end();
}

//==============================================================================
void ToolbarManager::saveToSettings()
{
    Settings::getInstance().setSetting("currentToolbar", m_currentLayoutName);
}

void ToolbarManager::loadFromSettings()
{
    juce::String savedLayout = Settings::getInstance().getSetting("currentToolbar", "Default").toString();

    if (layoutExists(savedLayout))
        loadLayout(savedLayout);
    else
        loadLayout("Default");
}

juce::File ToolbarManager::getToolbarsDirectory()
{
    juce::File appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);

#if JUCE_MAC
    juce::File toolbarsDir = appDataDir.getChildFile("Application Support/WaveEdit/Toolbars");
#elif JUCE_WINDOWS
    juce::File toolbarsDir = appDataDir.getChildFile("WaveEdit/Toolbars");
#else
    juce::File toolbarsDir = appDataDir.getChildFile(".config/WaveEdit/Toolbars");
#endif

    if (!toolbarsDir.exists())
        toolbarsDir.createDirectory();

    return toolbarsDir;
}

//==============================================================================
void ToolbarManager::addListener(Listener* listener)
{
    m_listeners.add(listener);
}

void ToolbarManager::removeListener(Listener* listener)
{
    m_listeners.remove(listener);
}

void ToolbarManager::notifyListeners()
{
    // Use async callback to ensure updates work even during modal dialogs
    // This ensures the message queue processes the repaint requests
    auto layout = m_currentLayout;
    juce::MessageManager::callAsync([this, layout]()
    {
        m_listeners.call(&Listener::toolbarLayoutChanged, layout);
    });
}

//==============================================================================
void ToolbarManager::createBuiltInLayouts()
{
    m_builtInLayouts["Default"] = createDefaultLayout();
    m_builtInLayouts["Compact"] = createCompactLayout();
    m_builtInLayouts["DSP Focused"] = createDSPFocusedLayout();
    m_builtInLayouts["Sound Forge"] = createSoundForgeLayout();

    juce::Logger::writeToLog("ToolbarManager: Created " +
                             juce::String(m_builtInLayouts.size()) + " built-in layouts");
}

void ToolbarManager::loadBuiltInTemplates()
{
    // Get the application's Resources directory where templates are bundled
    juce::File bundledToolbarsDir;

#if JUCE_MAC
    auto appFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    bundledToolbarsDir = appFile.getChildFile("Contents/Resources/Toolbars");
#elif JUCE_WINDOWS || JUCE_LINUX
    auto exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    bundledToolbarsDir = exeFile.getParentDirectory().getChildFile("Toolbars");
#endif

    if (!bundledToolbarsDir.exists())
    {
        juce::Logger::writeToLog("ToolbarManager: Bundled toolbars directory not found at: " +
                                 bundledToolbarsDir.getFullPathName());
        return;
    }

    juce::Logger::writeToLog("ToolbarManager: Loading bundled templates from: " +
                             bundledToolbarsDir.getFullPathName());

    auto files = bundledToolbarsDir.findChildFiles(juce::File::findFiles, false, "*.json");

    for (const auto& file : files)
    {
        ToolbarLayout layout = ToolbarLayout::fromJSON(file);
        if (!layout.name.isEmpty())
        {
            // Override programmatic layout with bundled one
            m_builtInLayouts[layout.name] = layout;
            juce::Logger::writeToLog("  Loaded bundled template: " + layout.name);

            // Copy to user directory on first run
            juce::File userLayoutFile = getToolbarsDirectory().getChildFile(file.getFileName());
            if (!userLayoutFile.exists())
            {
                if (file.copyFileTo(userLayoutFile))
                    juce::Logger::writeToLog("  Installed to user directory: " + layout.name);
            }
        }
    }
}

void ToolbarManager::scanUserLayouts()
{
    juce::File toolbarsDir = getToolbarsDirectory();

    auto files = toolbarsDir.findChildFiles(juce::File::findFiles, false, "*.json");
    for (const auto& file : files)
    {
        ToolbarLayout layout = ToolbarLayout::fromJSON(file);
        if (!layout.name.isEmpty())
            m_userLayouts[layout.name] = file;
    }

    juce::Logger::writeToLog("ToolbarManager: Scanned " +
                             juce::String(m_userLayouts.size()) + " user layouts");
}

//==============================================================================
ToolbarLayout ToolbarManager::createDefaultLayout()
{
    ToolbarLayout layout;
    layout.name = "Default";
    layout.description = "Transport + common operations";
    layout.version = "1.0";
    layout.height = 36;
    layout.showLabels = false;

    // Transport widget
    layout.buttons.push_back(ToolbarButtonConfig::transport("transport", 200));
    layout.buttons.push_back(ToolbarButtonConfig::separator("sep1"));

    // Zoom controls
    layout.buttons.push_back(ToolbarButtonConfig::command("zoomIn", "viewZoomIn"));
    layout.buttons.push_back(ToolbarButtonConfig::command("zoomOut", "viewZoomOut"));
    layout.buttons.push_back(ToolbarButtonConfig::command("zoomFit", "viewZoomFit"));
    layout.buttons.push_back(ToolbarButtonConfig::separator("sep2"));

    // Common DSP operations
    layout.buttons.push_back(ToolbarButtonConfig::command("fadeIn", "processFadeIn"));
    layout.buttons.push_back(ToolbarButtonConfig::command("fadeOut", "processFadeOut"));
    layout.buttons.push_back(ToolbarButtonConfig::command("normalize", "processNormalize"));
    layout.buttons.push_back(ToolbarButtonConfig::separator("sep3"));

    // Undo/Redo
    layout.buttons.push_back(ToolbarButtonConfig::command("undo", "editUndo"));
    layout.buttons.push_back(ToolbarButtonConfig::command("redo", "editRedo"));

    return layout;
}

ToolbarLayout ToolbarManager::createCompactLayout()
{
    ToolbarLayout layout;
    layout.name = "Compact";
    layout.description = "Transport only - maximum waveform space";
    layout.version = "1.0";
    layout.height = 36;
    layout.showLabels = false;

    // Just transport widget
    layout.buttons.push_back(ToolbarButtonConfig::transport("transport", 200));

    return layout;
}

ToolbarLayout ToolbarManager::createDSPFocusedLayout()
{
    ToolbarLayout layout;
    layout.name = "DSP Focused";
    layout.description = "All DSP operations readily accessible";
    layout.version = "1.0";
    layout.height = 36;
    layout.showLabels = false;

    // Transport
    layout.buttons.push_back(ToolbarButtonConfig::transport("transport", 200));
    layout.buttons.push_back(ToolbarButtonConfig::separator("sep1"));

    // All DSP operations
    layout.buttons.push_back(ToolbarButtonConfig::command("fadeIn", "processFadeIn"));
    layout.buttons.push_back(ToolbarButtonConfig::command("fadeOut", "processFadeOut"));
    layout.buttons.push_back(ToolbarButtonConfig::command("normalize", "processNormalize"));
    layout.buttons.push_back(ToolbarButtonConfig::command("gain", "processGain"));
    layout.buttons.push_back(ToolbarButtonConfig::command("dcOffset", "processDCOffset"));
    layout.buttons.push_back(ToolbarButtonConfig::separator("sep2"));

    // EQ
    layout.buttons.push_back(ToolbarButtonConfig::command("parametricEQ", "processParametricEQ"));
    layout.buttons.push_back(ToolbarButtonConfig::command("graphicalEQ", "processGraphicalEQ"));
    layout.buttons.push_back(ToolbarButtonConfig::separator("sep3"));

    // Plugin chain
    layout.buttons.push_back(ToolbarButtonConfig::command("pluginChain", "pluginShowChain"));
    layout.buttons.push_back(ToolbarButtonConfig::command("applyChain", "pluginApplyChain"));

    return layout;
}

ToolbarLayout ToolbarManager::createSoundForgeLayout()
{
    ToolbarLayout layout;
    layout.name = "Sound Forge";
    layout.description = "Familiar layout for Sound Forge users";
    layout.version = "1.0";
    layout.height = 36;
    layout.showLabels = false;

    // Transport
    layout.buttons.push_back(ToolbarButtonConfig::transport("transport", 200));
    layout.buttons.push_back(ToolbarButtonConfig::separator("sep1"));

    // Edit operations (like Sound Forge toolbar)
    layout.buttons.push_back(ToolbarButtonConfig::command("undo", "editUndo"));
    layout.buttons.push_back(ToolbarButtonConfig::command("redo", "editRedo"));
    layout.buttons.push_back(ToolbarButtonConfig::separator("sep2"));

    layout.buttons.push_back(ToolbarButtonConfig::command("cut", "editCut"));
    layout.buttons.push_back(ToolbarButtonConfig::command("copy", "editCopy"));
    layout.buttons.push_back(ToolbarButtonConfig::command("paste", "editPaste"));
    layout.buttons.push_back(ToolbarButtonConfig::command("delete", "editDelete"));
    layout.buttons.push_back(ToolbarButtonConfig::separator("sep3"));

    layout.buttons.push_back(ToolbarButtonConfig::command("trim", "editTrim"));
    layout.buttons.push_back(ToolbarButtonConfig::command("silence", "editSilence"));
    layout.buttons.push_back(ToolbarButtonConfig::separator("sep4"));

    // Zoom
    layout.buttons.push_back(ToolbarButtonConfig::command("zoomIn", "viewZoomIn"));
    layout.buttons.push_back(ToolbarButtonConfig::command("zoomOut", "viewZoomOut"));
    layout.buttons.push_back(ToolbarButtonConfig::command("zoomFit", "viewZoomFit"));
    layout.buttons.push_back(ToolbarButtonConfig::command("zoomSel", "viewZoomSelection"));
    layout.buttons.push_back(ToolbarButtonConfig::separator("sep5"));

    // DSP
    layout.buttons.push_back(ToolbarButtonConfig::command("normalize", "processNormalize"));
    layout.buttons.push_back(ToolbarButtonConfig::command("fadeIn", "processFadeIn"));
    layout.buttons.push_back(ToolbarButtonConfig::command("fadeOut", "processFadeOut"));

    return layout;
}
