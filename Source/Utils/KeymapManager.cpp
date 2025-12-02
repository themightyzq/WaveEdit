/*
  ==============================================================================

    KeymapManager.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "KeymapManager.h"
#include "Settings.h"

//==============================================================================
// Shortcut Implementation

juce::KeyPress KeymapManager::Shortcut::toKeyPress() const
{
    if (key.isEmpty())
        return juce::KeyPress();

    int modifierFlags = 0;
    for (const auto& mod : modifiers)
    {
        if (mod.equalsIgnoreCase("cmd") || mod.equalsIgnoreCase("command"))
            modifierFlags |= juce::ModifierKeys::commandModifier;
        else if (mod.equalsIgnoreCase("shift"))
            modifierFlags |= juce::ModifierKeys::shiftModifier;
        else if (mod.equalsIgnoreCase("alt") || mod.equalsIgnoreCase("option"))
            modifierFlags |= juce::ModifierKeys::altModifier;
        else if (mod.equalsIgnoreCase("ctrl") || mod.equalsIgnoreCase("control"))
            modifierFlags |= juce::ModifierKeys::ctrlModifier;
    }

    // Handle special keys
    if (key.equalsIgnoreCase("Space"))
        return juce::KeyPress(juce::KeyPress::spaceKey, modifierFlags, 0);
    else if (key.equalsIgnoreCase("Enter") || key.equalsIgnoreCase("Return"))
        return juce::KeyPress(juce::KeyPress::returnKey, modifierFlags, 0);
    else if (key.equalsIgnoreCase("Escape") || key.equalsIgnoreCase("Esc"))
        return juce::KeyPress(juce::KeyPress::escapeKey, modifierFlags, 0);
    else if (key.equalsIgnoreCase("Tab"))
        return juce::KeyPress(juce::KeyPress::tabKey, modifierFlags, 0);
    else if (key.equalsIgnoreCase("Delete") || key.equalsIgnoreCase("Del"))
        return juce::KeyPress(juce::KeyPress::deleteKey, modifierFlags, 0);
    else if (key.equalsIgnoreCase("Backspace"))
        return juce::KeyPress(juce::KeyPress::backspaceKey, modifierFlags, 0);
    else if (key.equalsIgnoreCase("Up") || key.equalsIgnoreCase("UpArrow"))
        return juce::KeyPress(juce::KeyPress::upKey, modifierFlags, 0);
    else if (key.equalsIgnoreCase("Down") || key.equalsIgnoreCase("DownArrow"))
        return juce::KeyPress(juce::KeyPress::downKey, modifierFlags, 0);
    else if (key.equalsIgnoreCase("Left") || key.equalsIgnoreCase("LeftArrow"))
        return juce::KeyPress(juce::KeyPress::leftKey, modifierFlags, 0);
    else if (key.equalsIgnoreCase("Right") || key.equalsIgnoreCase("RightArrow"))
        return juce::KeyPress(juce::KeyPress::rightKey, modifierFlags, 0);
    else if (key.equalsIgnoreCase("PageUp"))
        return juce::KeyPress(juce::KeyPress::pageUpKey, modifierFlags, 0);
    else if (key.equalsIgnoreCase("PageDown"))
        return juce::KeyPress(juce::KeyPress::pageDownKey, modifierFlags, 0);
    else if (key.equalsIgnoreCase("Home"))
        return juce::KeyPress(juce::KeyPress::homeKey, modifierFlags, 0);
    else if (key.equalsIgnoreCase("End"))
        return juce::KeyPress(juce::KeyPress::endKey, modifierFlags, 0);
    else if (key.startsWith("F") && key.length() >= 2)  // F1-F12
    {
        int fNum = key.substring(1).getIntValue();
        if (fNum >= 1 && fNum <= 12)
            return juce::KeyPress(juce::KeyPress::F1Key + (fNum - 1), modifierFlags, 0);
    }

    // Regular character key
    if (key.length() == 1)
    {
        juce::juce_wchar keyChar = key[0];
        return juce::KeyPress(keyChar, modifierFlags, 0);
    }

    return juce::KeyPress();
}

KeymapManager::Shortcut KeymapManager::Shortcut::fromKeyPress(const juce::KeyPress& keyPress)
{
    Shortcut shortcut;

    // Extract modifiers
    if (keyPress.getModifiers().isCommandDown())
        shortcut.modifiers.add("cmd");
    if (keyPress.getModifiers().isShiftDown())
        shortcut.modifiers.add("shift");
    if (keyPress.getModifiers().isAltDown())
        shortcut.modifiers.add("alt");
    if (keyPress.getModifiers().isCtrlDown())
        shortcut.modifiers.add("ctrl");

    // Extract key
    int keyCode = keyPress.getKeyCode();
    if (keyCode == juce::KeyPress::spaceKey)
        shortcut.key = "Space";
    else if (keyCode == juce::KeyPress::returnKey)
        shortcut.key = "Enter";
    else if (keyCode == juce::KeyPress::escapeKey)
        shortcut.key = "Escape";
    else if (keyCode == juce::KeyPress::tabKey)
        shortcut.key = "Tab";
    else if (keyCode == juce::KeyPress::deleteKey)
        shortcut.key = "Delete";
    else if (keyCode == juce::KeyPress::backspaceKey)
        shortcut.key = "Backspace";
    else if (keyCode == juce::KeyPress::upKey)
        shortcut.key = "Up";
    else if (keyCode == juce::KeyPress::downKey)
        shortcut.key = "Down";
    else if (keyCode == juce::KeyPress::leftKey)
        shortcut.key = "Left";
    else if (keyCode == juce::KeyPress::rightKey)
        shortcut.key = "Right";
    else if (keyCode == juce::KeyPress::pageUpKey)
        shortcut.key = "PageUp";
    else if (keyCode == juce::KeyPress::pageDownKey)
        shortcut.key = "PageDown";
    else if (keyCode == juce::KeyPress::homeKey)
        shortcut.key = "Home";
    else if (keyCode == juce::KeyPress::endKey)
        shortcut.key = "End";
    else if (keyCode >= juce::KeyPress::F1Key && keyCode <= juce::KeyPress::F12Key)
        shortcut.key = "F" + juce::String(keyCode - juce::KeyPress::F1Key + 1);
    else
        shortcut.key = juce::String::charToString((juce::juce_wchar)keyCode);

    return shortcut;
}

juce::String KeymapManager::Shortcut::getDescription() const
{
    if (key.isEmpty())
        return "(none)";

    juce::String desc;
    for (const auto& mod : modifiers)
    {
        if (!desc.isEmpty())
            desc += "+";
        desc += mod.substring(0, 1).toUpperCase() + mod.substring(1);
    }

    if (!desc.isEmpty())
        desc += "+";
    desc += key;

    return desc;
}

//==============================================================================
// Template Implementation

KeymapManager::Template KeymapManager::Template::fromJSON(const juce::File& file)
{
    if (!file.existsAsFile())
        return Template();

    juce::var json = juce::JSON::parse(file);
    if (!json.isObject())
        return Template();

    return fromVar(json);
}

bool KeymapManager::Template::saveToJSON(const juce::File& file) const
{
    juce::var json = toVar();
    juce::String jsonString = juce::JSON::toString(json, false);  // false = pretty-printed with indentation

    return file.replaceWithText(jsonString);
}

KeymapManager::Template KeymapManager::Template::fromVar(const juce::var& json)
{
    Template templ;

    if (!json.isObject())
        return templ;

    auto* obj = json.getDynamicObject();
    if (obj == nullptr)
        return templ;

    templ.name = obj->getProperty("name").toString();
    templ.description = obj->getProperty("description").toString();
    templ.version = obj->getProperty("version").toString();

    auto shortcuts = obj->getProperty("shortcuts");
    if (shortcuts.isObject())
    {
        auto* shortcutsObj = shortcuts.getDynamicObject();
        if (shortcutsObj != nullptr)
        {
            for (auto& prop : shortcutsObj->getProperties())
            {
                juce::String commandName = prop.name.toString();
                auto shortcutVar = prop.value;

                if (shortcutVar.isObject())
                {
                    auto* scObj = shortcutVar.getDynamicObject();
                    Shortcut sc;
                    sc.key = scObj->getProperty("key").toString();

                    auto modsArray = scObj->getProperty("modifiers");
                    if (modsArray.isArray())
                    {
                        for (int i = 0; i < modsArray.size(); ++i)
                            sc.modifiers.add(modsArray[i].toString());
                    }

                    templ.shortcuts[commandName] = sc;
                }
            }
        }
    }

    return templ;
}

juce::var KeymapManager::Template::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("name", name);
    obj->setProperty("description", description);
    obj->setProperty("version", version);

    auto* shortcutsObj = new juce::DynamicObject();
    for (const auto& pair : shortcuts)
    {
        auto* scObj = new juce::DynamicObject();
        scObj->setProperty("key", pair.second.key);

        juce::var modsArray;
        for (const auto& mod : pair.second.modifiers)
            modsArray.append(mod);
        scObj->setProperty("modifiers", modsArray);

        shortcutsObj->setProperty(pair.first, juce::var(scObj));
    }
    obj->setProperty("shortcuts", juce::var(shortcutsObj));

    return juce::var(obj);
}

bool KeymapManager::Template::validate(juce::StringArray& errors) const
{
    bool isValid = true;

    // Check basic fields
    if (name.isEmpty())
    {
        errors.add("Template name is empty");
        isValid = false;
    }

    if (shortcuts.empty())
    {
        errors.add("Template has no shortcuts defined");
        isValid = false;
    }

    // Check for duplicate shortcuts
    std::map<juce::String, juce::String> shortcutToCommand;
    for (const auto& pair : shortcuts)
    {
        juce::String desc = pair.second.getDescription();
        if (shortcutToCommand.find(desc) != shortcutToCommand.end())
        {
            errors.add("Conflict: " + desc + " assigned to both '" +
                      shortcutToCommand[desc] + "' and '" + pair.first + "'");
            isValid = false;
        }
        else
        {
            shortcutToCommand[desc] = pair.first;
        }
    }

    return isValid;
}

//==============================================================================
// KeymapManager Implementation

KeymapManager::KeymapManager(juce::ApplicationCommandManager& commandManager)
    : m_commandManager(commandManager)
{
    loadBuiltInTemplates();
    scanUserTemplates();
    loadFromSettings();
}

KeymapManager::~KeymapManager()
{
}

juce::StringArray KeymapManager::getAvailableTemplates() const
{
    juce::StringArray templates;

    juce::Logger::writeToLog("KeymapManager::getAvailableTemplates() called");
    juce::Logger::writeToLog("  Built-in templates: " + juce::String(m_builtInTemplates.size()));
    juce::Logger::writeToLog("  User templates: " + juce::String(m_userTemplates.size()));

    // Add built-in templates
    for (const auto& pair : m_builtInTemplates)
    {
        templates.add(pair.first);
        juce::Logger::writeToLog("  Added built-in: " + pair.first);
    }

    // Add user templates (only if not already in the list from built-ins)
    // Use case-insensitive comparison to avoid duplicates like "Default" and "default"
    for (const auto& pair : m_userTemplates)
    {
        bool isDuplicate = false;
        for (const auto& existingTemplate : templates)
        {
            if (existingTemplate.equalsIgnoreCase(pair.first))
            {
                isDuplicate = true;
                juce::Logger::writeToLog("  Skipping user template (duplicate): " + pair.first);
                break;
            }
        }

        if (!isDuplicate)
        {
            templates.add(pair.first);
            juce::Logger::writeToLog("  Added user: " + pair.first);
        }
    }

    juce::Logger::writeToLog("  Total templates to return: " + juce::String(templates.size()));
    return templates;
}

juce::String KeymapManager::getCurrentTemplateName() const
{
    return m_currentTemplateName;
}

bool KeymapManager::loadTemplate(const juce::String& templateName)
{
    // Check built-in templates first
    auto builtInIt = m_builtInTemplates.find(templateName);
    if (builtInIt != m_builtInTemplates.end())
    {
        m_currentTemplate = builtInIt->second;
        m_currentTemplateName = templateName;

        // Apply shortcuts to ApplicationCommandManager (THIS IS THE FIX!)
        applyTemplateToCommandManager();

        saveToSettings();
        return true;
    }

    // Check user templates
    auto userIt = m_userTemplates.find(templateName);
    if (userIt != m_userTemplates.end())
    {
        m_currentTemplate = Template::fromJSON(userIt->second);
        m_currentTemplateName = templateName;

        // Apply shortcuts to ApplicationCommandManager (THIS IS THE FIX!)
        applyTemplateToCommandManager();

        saveToSettings();
        return true;
    }

    juce::Logger::writeToLog("KeymapManager: Template not found: " + templateName);
    return false;
}

const KeymapManager::Template& KeymapManager::getCurrentTemplate() const
{
    return m_currentTemplate;
}

bool KeymapManager::templateExists(const juce::String& templateName) const
{
    return m_builtInTemplates.find(templateName) != m_builtInTemplates.end() ||
           m_userTemplates.find(templateName) != m_userTemplates.end();
}

bool KeymapManager::importTemplate(const juce::File& file, bool makeActive)
{
    Template templ = Template::fromJSON(file);

    if (templ.name.isEmpty())
    {
        juce::Logger::writeToLog("KeymapManager: Failed to import template - invalid JSON");
        return false;
    }

    // Validate template
    juce::StringArray errors;
    if (!templ.validate(errors))
    {
        juce::Logger::writeToLog("KeymapManager: Template validation failed:");
        for (const auto& error : errors)
            juce::Logger::writeToLog("  " + error);
        return false;
    }

    // Copy to user templates directory
    juce::File destFile = getTemplatesDirectory().getChildFile(file.getFileName());
    if (!file.copyFileTo(destFile))
    {
        juce::Logger::writeToLog("KeymapManager: Failed to copy template file");
        return false;
    }

    // Add to user templates map
    m_userTemplates[templ.name] = destFile;

    if (makeActive)
        loadTemplate(templ.name);

    juce::Logger::writeToLog("KeymapManager: Successfully imported template: " + templ.name);
    return true;
}

bool KeymapManager::exportCurrentTemplate(const juce::File& file) const
{
    return m_currentTemplate.saveToJSON(file);
}

bool KeymapManager::exportTemplate(const juce::String& templateName, const juce::File& file) const
{
    auto builtInIt = m_builtInTemplates.find(templateName);
    if (builtInIt != m_builtInTemplates.end())
        return builtInIt->second.saveToJSON(file);

    auto userIt = m_userTemplates.find(templateName);
    if (userIt != m_userTemplates.end())
    {
        Template templ = Template::fromJSON(userIt->second);
        return templ.saveToJSON(file);
    }

    return false;
}

KeymapManager::Shortcut KeymapManager::getShortcut(const juce::String& commandName) const
{
    auto it = m_currentTemplate.shortcuts.find(commandName);
    if (it != m_currentTemplate.shortcuts.end())
        return it->second;

    return Shortcut();  // Empty shortcut
}

juce::KeyPress KeymapManager::getKeyPress(juce::CommandID commandID) const
{
    juce::String commandName = getCommandName(commandID);
    Shortcut sc = getShortcut(commandName);
    return sc.toKeyPress();
}

juce::String KeymapManager::findCommandForShortcut(const Shortcut& shortcut) const
{
    juce::String targetDesc = shortcut.getDescription();

    for (const auto& pair : m_currentTemplate.shortcuts)
    {
        if (pair.second.getDescription() == targetDesc)
            return pair.first;
    }

    return juce::String();
}

void KeymapManager::saveToSettings()
{
    Settings::getInstance().setSetting("currentKeymap", m_currentTemplateName);
}

void KeymapManager::loadFromSettings()
{
    juce::String savedTemplate = Settings::getInstance().getSetting("currentKeymap", "Default").toString();

    if (templateExists(savedTemplate))
        loadTemplate(savedTemplate);
    else
        loadTemplate("Default");  // Fall back to default
}

juce::File KeymapManager::getTemplatesDirectory()
{
    juce::File appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);

#if JUCE_MAC
    juce::File templatesDir = appDataDir.getChildFile("Application Support/WaveEdit/Keymaps");
#elif JUCE_WINDOWS
    juce::File templatesDir = appDataDir.getChildFile("WaveEdit/Keymaps");
#else
    juce::File templatesDir = appDataDir.getChildFile(".config/WaveEdit/Keymaps");
#endif

    if (!templatesDir.exists())
        templatesDir.createDirectory();

    return templatesDir;
}

void KeymapManager::loadBuiltInTemplates()
{
    // Get the application's Resources directory where templates are bundled
    juce::File bundledKeymapsDir;

#if JUCE_MAC
    // On macOS, templates are in the app bundle's Resources/Keymaps directory
    auto appFile = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    bundledKeymapsDir = appFile.getChildFile("Contents/Resources/Keymaps");
#elif JUCE_WINDOWS || JUCE_LINUX
    // On Windows/Linux, templates are next to the executable in Keymaps directory
    auto exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    bundledKeymapsDir = exeFile.getParentDirectory().getChildFile("Keymaps");
#endif

    if (!bundledKeymapsDir.exists())
    {
        juce::Logger::writeToLog("KeymapManager: Bundled keymaps directory not found at: " + bundledKeymapsDir.getFullPathName());
        juce::Logger::writeToLog("KeymapManager: No built-in templates available");
        return;
    }

    juce::Logger::writeToLog("KeymapManager: Loading built-in templates from: " + bundledKeymapsDir.getFullPathName());

    // Load all .json template files from the bundled directory
    auto files = bundledKeymapsDir.findChildFiles(juce::File::findFiles, false, "*.json");

    for (const auto& file : files)
    {
        Template templ = Template::fromJSON(file);
        if (!templ.name.isEmpty())
        {
            m_builtInTemplates[templ.name] = templ;
            juce::Logger::writeToLog("  Loaded built-in template: " + templ.name);

            // On first run, copy built-in templates to user directory so they can be customized
            juce::File userTemplateFile = getTemplatesDirectory().getChildFile(file.getFileName());
            if (!userTemplateFile.exists())
            {
                if (file.copyFileTo(userTemplateFile))
                {
                    juce::Logger::writeToLog("  Installed template to user directory: " + templ.name);
                }
                else
                {
                    juce::Logger::writeToLog("  WARNING: Failed to copy template to user directory: " + templ.name);
                }
            }
        }
        else
        {
            juce::Logger::writeToLog("  WARNING: Failed to load template from: " + file.getFileName());
        }
    }

    juce::Logger::writeToLog("KeymapManager: Loaded " + juce::String(m_builtInTemplates.size()) + " built-in templates");
}

void KeymapManager::scanUserTemplates()
{
    juce::File templatesDir = getTemplatesDirectory();

    auto files = templatesDir.findChildFiles(juce::File::findFiles, false, "*.json");
    for (const auto& file : files)
    {
        Template templ = Template::fromJSON(file);
        if (!templ.name.isEmpty())
            m_userTemplates[templ.name] = file;
    }
}

juce::String KeymapManager::getCommandName(juce::CommandID commandID)
{
    // This maps command IDs to command names
    // We'll implement this with a static map
    static std::map<juce::CommandID, juce::String> commandNameMap;

    // Initialize map on first call (lazy initialization)
    if (commandNameMap.empty())
    {
        // File operations
        commandNameMap[CommandIDs::fileNew] = "fileNew";
        commandNameMap[CommandIDs::fileOpen] = "fileOpen";
        commandNameMap[CommandIDs::fileSave] = "fileSave";
        commandNameMap[CommandIDs::fileSaveAs] = "fileSaveAs";
        commandNameMap[CommandIDs::fileClose] = "fileClose";
        commandNameMap[CommandIDs::fileProperties] = "fileProperties";
        commandNameMap[CommandIDs::fileExit] = "fileExit";
        commandNameMap[CommandIDs::filePreferences] = "filePreferences";

        // Edit operations
        commandNameMap[CommandIDs::editUndo] = "editUndo";
        commandNameMap[CommandIDs::editRedo] = "editRedo";
        commandNameMap[CommandIDs::editCut] = "editCut";
        commandNameMap[CommandIDs::editCopy] = "editCopy";
        commandNameMap[CommandIDs::editPaste] = "editPaste";
        commandNameMap[CommandIDs::editDelete] = "editDelete";
        commandNameMap[CommandIDs::editSelectAll] = "editSelectAll";
        commandNameMap[CommandIDs::editSilence] = "editSilence";
        commandNameMap[CommandIDs::editTrim] = "editTrim";

        // Playback operations
        commandNameMap[CommandIDs::playbackPlay] = "playbackPlay";
        commandNameMap[CommandIDs::playbackPause] = "playbackPause";
        commandNameMap[CommandIDs::playbackStop] = "playbackStop";
        commandNameMap[CommandIDs::playbackLoop] = "playbackLoop";
        commandNameMap[CommandIDs::playbackRecord] = "playbackRecord";

        // View operations
        commandNameMap[CommandIDs::viewZoomIn] = "viewZoomIn";
        commandNameMap[CommandIDs::viewZoomOut] = "viewZoomOut";
        commandNameMap[CommandIDs::viewZoomFit] = "viewZoomFit";
        commandNameMap[CommandIDs::viewZoomSelection] = "viewZoomSelection";
        commandNameMap[CommandIDs::viewZoomOneToOne] = "viewZoomOneToOne";
        commandNameMap[CommandIDs::viewCycleTimeFormat] = "viewCycleTimeFormat";
        commandNameMap[CommandIDs::viewAutoScroll] = "viewAutoScroll";
        commandNameMap[CommandIDs::viewZoomToRegion] = "viewZoomToRegion";
        commandNameMap[CommandIDs::viewAutoPreviewRegions] = "viewAutoPreviewRegions";
        commandNameMap[CommandIDs::viewSpectrumAnalyzer] = "viewSpectrumAnalyzer";

        // Processing operations
        commandNameMap[CommandIDs::processFadeIn] = "processFadeIn";
        commandNameMap[CommandIDs::processFadeOut] = "processFadeOut";
        commandNameMap[CommandIDs::processNormalize] = "processNormalize";
        commandNameMap[CommandIDs::processDCOffset] = "processDCOffset";
        commandNameMap[CommandIDs::processGain] = "processGain";
        commandNameMap[CommandIDs::processIncreaseGain] = "processIncreaseGain";
        commandNameMap[CommandIDs::processDecreaseGain] = "processDecreaseGain";
        commandNameMap[CommandIDs::processParametricEQ] = "processParametricEQ";
        commandNameMap[CommandIDs::processGraphicalEQ] = "processGraphicalEQ";

        // Navigation operations
        commandNameMap[CommandIDs::navigateLeft] = "navigateLeft";
        commandNameMap[CommandIDs::navigateRight] = "navigateRight";
        commandNameMap[CommandIDs::navigateStart] = "navigateStart";
        commandNameMap[CommandIDs::navigateEnd] = "navigateEnd";
        commandNameMap[CommandIDs::navigatePageLeft] = "navigatePageLeft";
        commandNameMap[CommandIDs::navigatePageRight] = "navigatePageRight";
        commandNameMap[CommandIDs::navigateHomeVisible] = "navigateHomeVisible";
        commandNameMap[CommandIDs::navigateEndVisible] = "navigateEndVisible";
        commandNameMap[CommandIDs::navigateCenterView] = "navigateCenterView";
        commandNameMap[CommandIDs::navigateGoToPosition] = "navigateGoToPosition";

        // Selection operations
        commandNameMap[CommandIDs::selectExtendLeft] = "selectExtendLeft";
        commandNameMap[CommandIDs::selectExtendRight] = "selectExtendRight";
        commandNameMap[CommandIDs::selectExtendStart] = "selectExtendStart";
        commandNameMap[CommandIDs::selectExtendEnd] = "selectExtendEnd";
        commandNameMap[CommandIDs::selectExtendPageLeft] = "selectExtendPageLeft";
        commandNameMap[CommandIDs::selectExtendPageRight] = "selectExtendPageRight";

        // Snap operations
        commandNameMap[CommandIDs::snapCycleMode] = "snapCycleMode";
        commandNameMap[CommandIDs::snapToggleZeroCrossing] = "snapToggleZeroCrossing";
        commandNameMap[CommandIDs::snapPreferences] = "snapPreferences";

        // Help operations
        commandNameMap[CommandIDs::helpAbout] = "helpAbout";
        commandNameMap[CommandIDs::helpShortcuts] = "helpShortcuts";

        // Tab operations
        commandNameMap[CommandIDs::tabClose] = "tabClose";
        commandNameMap[CommandIDs::tabCloseAll] = "tabCloseAll";
        commandNameMap[CommandIDs::tabNext] = "tabNext";
        commandNameMap[CommandIDs::tabPrevious] = "tabPrevious";
        commandNameMap[CommandIDs::tabSelect1] = "tabSelect1";
        commandNameMap[CommandIDs::tabSelect2] = "tabSelect2";
        commandNameMap[CommandIDs::tabSelect3] = "tabSelect3";
        commandNameMap[CommandIDs::tabSelect4] = "tabSelect4";
        commandNameMap[CommandIDs::tabSelect5] = "tabSelect5";
        commandNameMap[CommandIDs::tabSelect6] = "tabSelect6";
        commandNameMap[CommandIDs::tabSelect7] = "tabSelect7";
        commandNameMap[CommandIDs::tabSelect8] = "tabSelect8";
        commandNameMap[CommandIDs::tabSelect9] = "tabSelect9";

        // Region operations
        commandNameMap[CommandIDs::regionAdd] = "regionAdd";
        commandNameMap[CommandIDs::regionDelete] = "regionDelete";
        commandNameMap[CommandIDs::regionNext] = "regionNext";
        commandNameMap[CommandIDs::regionPrevious] = "regionPrevious";
        commandNameMap[CommandIDs::regionSelectInverse] = "regionSelectInverse";
        commandNameMap[CommandIDs::regionSelectAll] = "regionSelectAll";
        commandNameMap[CommandIDs::regionStripSilence] = "regionStripSilence";
        commandNameMap[CommandIDs::regionExportAll] = "regionExportAll";
        commandNameMap[CommandIDs::regionShowList] = "regionShowList";
        commandNameMap[CommandIDs::regionSnapToZeroCrossing] = "regionSnapToZeroCrossing";
        commandNameMap[CommandIDs::regionNudgeStartLeft] = "regionNudgeStartLeft";
        commandNameMap[CommandIDs::regionNudgeStartRight] = "regionNudgeStartRight";
        commandNameMap[CommandIDs::regionNudgeEndLeft] = "regionNudgeEndLeft";
        commandNameMap[CommandIDs::regionNudgeEndRight] = "regionNudgeEndRight";
        commandNameMap[CommandIDs::regionBatchRename] = "regionBatchRename";
        commandNameMap[CommandIDs::regionMerge] = "regionMerge";
        commandNameMap[CommandIDs::regionSplit] = "regionSplit";
        commandNameMap[CommandIDs::regionCopy] = "regionCopy";
        commandNameMap[CommandIDs::regionPaste] = "regionPaste";

        // Marker operations
        commandNameMap[CommandIDs::markerAdd] = "markerAdd";
        commandNameMap[CommandIDs::markerDelete] = "markerDelete";
        commandNameMap[CommandIDs::markerNext] = "markerNext";
        commandNameMap[CommandIDs::markerPrevious] = "markerPrevious";
        commandNameMap[CommandIDs::markerShowList] = "markerShowList";
    }

    auto it = commandNameMap.find(commandID);
    if (it != commandNameMap.end())
        return it->second;

    return juce::String();
}

juce::CommandID KeymapManager::getCommandID(const juce::String& commandName)
{
    // Reverse mapping from command name to ID
    // Build reverse map from getCommandName()'s map using explicit list of valid command IDs
    static std::map<juce::String, juce::CommandID> reverseMap;

    if (reverseMap.empty())
    {
        // Build reverse map using explicit list of all command IDs
        // This is more efficient than iterating 45,000+ potential IDs
        const std::vector<juce::CommandID> allCommandIDs = {
            // File operations (0x1000-0x10FF)
            CommandIDs::fileNew, CommandIDs::fileOpen, CommandIDs::fileSave,
            CommandIDs::fileSaveAs, CommandIDs::fileClose, CommandIDs::fileProperties,
            CommandIDs::fileExit, CommandIDs::filePreferences,

            // Edit operations (0x2000-0x20FF)
            CommandIDs::editUndo, CommandIDs::editRedo, CommandIDs::editCut,
            CommandIDs::editCopy, CommandIDs::editPaste, CommandIDs::editDelete,
            CommandIDs::editSelectAll, CommandIDs::editSilence, CommandIDs::editTrim,

            // Playback operations (0x3000-0x30FF)
            CommandIDs::playbackPlay, CommandIDs::playbackPause, CommandIDs::playbackStop,
            CommandIDs::playbackLoop, CommandIDs::playbackRecord,

            // View operations (0x4000-0x40FF)
            CommandIDs::viewZoomIn, CommandIDs::viewZoomOut, CommandIDs::viewZoomFit,
            CommandIDs::viewZoomSelection, CommandIDs::viewZoomOneToOne,
            CommandIDs::viewCycleTimeFormat, CommandIDs::viewAutoScroll,
            CommandIDs::viewZoomToRegion, CommandIDs::viewAutoPreviewRegions,
            CommandIDs::viewSpectrumAnalyzer,

            // Processing operations (0x5000-0x50FF)
            CommandIDs::processFadeIn, CommandIDs::processFadeOut,
            CommandIDs::processNormalize, CommandIDs::processDCOffset,
            CommandIDs::processGain, CommandIDs::processIncreaseGain,
            CommandIDs::processDecreaseGain, CommandIDs::processParametricEQ,
            CommandIDs::processGraphicalEQ,

            // Navigation operations (0x6000-0x60FF)
            CommandIDs::navigateLeft, CommandIDs::navigateRight,
            CommandIDs::navigateStart, CommandIDs::navigateEnd,
            CommandIDs::navigatePageLeft, CommandIDs::navigatePageRight,
            CommandIDs::navigateHomeVisible, CommandIDs::navigateEndVisible,
            CommandIDs::navigateCenterView, CommandIDs::navigateGoToPosition,

            // Selection operations (0x7000-0x70FF)
            CommandIDs::selectExtendLeft, CommandIDs::selectExtendRight,
            CommandIDs::selectExtendStart, CommandIDs::selectExtendEnd,
            CommandIDs::selectExtendPageLeft, CommandIDs::selectExtendPageRight,

            // Snap operations (0x8000-0x80FF)
            CommandIDs::snapCycleMode, CommandIDs::snapToggleZeroCrossing,
            CommandIDs::snapPreferences,

            // Help operations (0x9000-0x90FF)
            CommandIDs::helpAbout, CommandIDs::helpShortcuts,

            // Tab operations (0xA000-0xA0FF)
            CommandIDs::tabClose, CommandIDs::tabCloseAll,
            CommandIDs::tabNext, CommandIDs::tabPrevious,
            CommandIDs::tabSelect1, CommandIDs::tabSelect2, CommandIDs::tabSelect3,
            CommandIDs::tabSelect4, CommandIDs::tabSelect5, CommandIDs::tabSelect6,
            CommandIDs::tabSelect7, CommandIDs::tabSelect8, CommandIDs::tabSelect9,

            // Region operations (0xB000-0xB0FF)
            CommandIDs::regionAdd, CommandIDs::regionDelete,
            CommandIDs::regionNext, CommandIDs::regionPrevious,
            CommandIDs::regionSelectInverse, CommandIDs::regionSelectAll,
            CommandIDs::regionStripSilence, CommandIDs::regionExportAll,
            CommandIDs::regionShowList, CommandIDs::regionSnapToZeroCrossing,
            CommandIDs::regionNudgeStartLeft, CommandIDs::regionNudgeStartRight,
            CommandIDs::regionNudgeEndLeft, CommandIDs::regionNudgeEndRight,
            CommandIDs::regionBatchRename, CommandIDs::regionMerge,
            CommandIDs::regionSplit, CommandIDs::regionCopy, CommandIDs::regionPaste,

            // Marker operations (0xC000-0xC0FF)
            CommandIDs::markerAdd, CommandIDs::markerDelete,
            CommandIDs::markerNext, CommandIDs::markerPrevious,
            CommandIDs::markerShowList
        };

        for (const auto& id : allCommandIDs)
        {
            juce::String name = getCommandName(id);
            if (name.isNotEmpty())
                reverseMap[name] = id;
        }
    }

    auto it = reverseMap.find(commandName);
    if (it != reverseMap.end())
        return it->second;

    return 0;  // Invalid command
}

void KeymapManager::applyTemplateToCommandManager()
{
    // Get the KeyPressMappingSet from the ApplicationCommandManager
    auto* keyMappings = m_commandManager.getKeyMappings();
    if (keyMappings == nullptr)
    {
        juce::Logger::writeToLog("KeymapManager: ERROR - Could not get KeyPressMappingSet from ApplicationCommandManager");
        return;
    }

    juce::Logger::writeToLog("KeymapManager: Applying template '" + m_currentTemplateName + "' to ApplicationCommandManager");

    int shortcutsApplied = 0;
    int shortcutsFailed = 0;

    // Iterate through all shortcuts in the current template
    for (const auto& pair : m_currentTemplate.shortcuts)
    {
        const juce::String& commandName = pair.first;
        const Shortcut& shortcut = pair.second;

        // Skip comment entries (used for template organization)
        if (commandName.startsWith("_comment"))
            continue;

        // Get the CommandID for this command name
        juce::CommandID commandID = getCommandID(commandName);
        if (commandID == 0)
        {
            juce::Logger::writeToLog("  WARNING: Unknown command name: " + commandName);
            shortcutsFailed++;
            continue;
        }

        // Clear any existing shortcuts for this command
        keyMappings->clearAllKeyPresses(commandID);

        // Add the new shortcut if it's valid
        if (shortcut.key.isNotEmpty())
        {
            juce::KeyPress keyPress = shortcut.toKeyPress();
            if (keyPress.isValid())
            {
                keyMappings->addKeyPress(commandID, keyPress);
                shortcutsApplied++;
            }
            else
            {
                juce::Logger::writeToLog("  WARNING: Invalid KeyPress for command: " + commandName);
                shortcutsFailed++;
            }
        }
    }

    // Notify the command manager that shortcuts have changed
    m_commandManager.commandStatusChanged();

    juce::Logger::writeToLog("KeymapManager: Applied " + juce::String(shortcutsApplied) +
                            " shortcuts (" + juce::String(shortcutsFailed) + " failed)");
}
