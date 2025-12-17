/*
  ==============================================================================

    ToolbarConfig.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "ToolbarConfig.h"

//==============================================================================
// ToolbarButtonConfig Implementation

ToolbarButtonConfig ToolbarButtonConfig::command(const juce::String& buttonId,
                                                  const juce::String& cmdName,
                                                  int customWidth)
{
    ToolbarButtonConfig config;
    config.id = buttonId;
    config.type = ToolbarButtonType::COMMAND;
    config.commandName = cmdName;
    config.width = customWidth;
    return config;
}

ToolbarButtonConfig ToolbarButtonConfig::plugin(const juce::String& buttonId,
                                                 const juce::String& pluginId,
                                                 const juce::String& customTooltip,
                                                 int customWidth)
{
    ToolbarButtonConfig config;
    config.id = buttonId;
    config.type = ToolbarButtonType::PLUGIN;
    config.pluginIdentifier = pluginId;
    config.tooltip = customTooltip;
    config.width = customWidth;
    return config;
}

ToolbarButtonConfig ToolbarButtonConfig::separator(const juce::String& separatorId,
                                                    int customWidth)
{
    ToolbarButtonConfig config;
    config.id = separatorId;
    config.type = ToolbarButtonType::SEPARATOR;
    config.width = customWidth;
    return config;
}

ToolbarButtonConfig ToolbarButtonConfig::spacer(const juce::String& spacerId,
                                                 int minWidth)
{
    ToolbarButtonConfig config;
    config.id = spacerId;
    config.type = ToolbarButtonType::SPACER;
    config.width = minWidth;
    return config;
}

ToolbarButtonConfig ToolbarButtonConfig::transport(const juce::String& transportId,
                                                    int customWidth)
{
    ToolbarButtonConfig config;
    config.id = transportId;
    config.type = ToolbarButtonType::TRANSPORT;
    config.width = customWidth;
    return config;
}

ToolbarButtonConfig ToolbarButtonConfig::fromVar(const juce::var& json)
{
    ToolbarButtonConfig config;

    if (!json.isObject())
        return config;

    auto* obj = json.getDynamicObject();
    if (obj == nullptr)
        return config;

    config.id = obj->getProperty("id").toString();
    config.type = stringToType(obj->getProperty("type").toString());
    config.commandName = obj->getProperty("commandName").toString();
    config.pluginIdentifier = obj->getProperty("pluginIdentifier").toString();
    config.iconName = obj->getProperty("iconName").toString();
    config.tooltip = obj->getProperty("tooltip").toString();

    auto widthVar = obj->getProperty("width");
    if (!widthVar.isVoid())
        config.width = static_cast<int>(widthVar);
    else
    {
        // Default widths based on type
        switch (config.type)
        {
            case ToolbarButtonType::SEPARATOR:
                config.width = 8;
                break;
            case ToolbarButtonType::SPACER:
                config.width = 16;
                break;
            case ToolbarButtonType::TRANSPORT:
                config.width = 180;
                break;
            default:
                config.width = 28;
                break;
        }
    }

    return config;
}

juce::var ToolbarButtonConfig::toVar() const
{
    auto* obj = new juce::DynamicObject();

    obj->setProperty("id", id);
    obj->setProperty("type", typeToString(type));

    if (type == ToolbarButtonType::COMMAND && commandName.isNotEmpty())
        obj->setProperty("commandName", commandName);

    if (type == ToolbarButtonType::PLUGIN && pluginIdentifier.isNotEmpty())
        obj->setProperty("pluginIdentifier", pluginIdentifier);

    if (iconName.isNotEmpty())
        obj->setProperty("iconName", iconName);

    if (tooltip.isNotEmpty())
        obj->setProperty("tooltip", tooltip);

    obj->setProperty("width", width);

    return juce::var(obj);
}

juce::String ToolbarButtonConfig::typeToString(ToolbarButtonType type)
{
    switch (type)
    {
        case ToolbarButtonType::COMMAND:   return "command";
        case ToolbarButtonType::PLUGIN:    return "plugin";
        case ToolbarButtonType::SEPARATOR: return "separator";
        case ToolbarButtonType::SPACER:    return "spacer";
        case ToolbarButtonType::TRANSPORT: return "transport";
        default:                           return "command";
    }
}

ToolbarButtonType ToolbarButtonConfig::stringToType(const juce::String& typeStr)
{
    if (typeStr.equalsIgnoreCase("command"))   return ToolbarButtonType::COMMAND;
    if (typeStr.equalsIgnoreCase("plugin"))    return ToolbarButtonType::PLUGIN;
    if (typeStr.equalsIgnoreCase("separator")) return ToolbarButtonType::SEPARATOR;
    if (typeStr.equalsIgnoreCase("spacer"))    return ToolbarButtonType::SPACER;
    if (typeStr.equalsIgnoreCase("transport")) return ToolbarButtonType::TRANSPORT;
    return ToolbarButtonType::COMMAND;  // Default
}

//==============================================================================
// ToolbarLayout Implementation

ToolbarLayout ToolbarLayout::fromJSON(const juce::File& file)
{
    if (!file.existsAsFile())
        return ToolbarLayout();

    juce::var json = juce::JSON::parse(file);
    if (!json.isObject())
        return ToolbarLayout();

    return fromVar(json);
}

bool ToolbarLayout::saveToJSON(const juce::File& file) const
{
    juce::var json = toVar();
    juce::String jsonString = juce::JSON::toString(json, false);  // false = pretty-printed

    return file.replaceWithText(jsonString);
}

ToolbarLayout ToolbarLayout::fromVar(const juce::var& json)
{
    ToolbarLayout layout;

    if (!json.isObject())
        return layout;

    auto* obj = json.getDynamicObject();
    if (obj == nullptr)
        return layout;

    layout.name = obj->getProperty("name").toString();
    layout.description = obj->getProperty("description").toString();
    layout.version = obj->getProperty("version").toString();

    auto heightVar = obj->getProperty("height");
    if (!heightVar.isVoid())
        layout.height = static_cast<int>(heightVar);

    auto showLabelsVar = obj->getProperty("showLabels");
    if (!showLabelsVar.isVoid())
        layout.showLabels = static_cast<bool>(showLabelsVar);

    auto buttonsArray = obj->getProperty("buttons");
    if (buttonsArray.isArray())
    {
        for (int i = 0; i < buttonsArray.size(); ++i)
        {
            ToolbarButtonConfig buttonConfig = ToolbarButtonConfig::fromVar(buttonsArray[i]);
            if (buttonConfig.id.isNotEmpty())
                layout.buttons.push_back(buttonConfig);
        }
    }

    return layout;
}

juce::var ToolbarLayout::toVar() const
{
    auto* obj = new juce::DynamicObject();

    obj->setProperty("name", name);
    obj->setProperty("description", description);
    obj->setProperty("version", version);
    obj->setProperty("height", height);
    obj->setProperty("showLabels", showLabels);

    juce::var buttonsArray;
    for (const auto& button : buttons)
        buttonsArray.append(button.toVar());
    obj->setProperty("buttons", buttonsArray);

    return juce::var(obj);
}

bool ToolbarLayout::validate(juce::StringArray& errors) const
{
    bool isValid = true;

    // Check basic fields
    if (name.isEmpty())
    {
        errors.add("Layout name is empty");
        isValid = false;
    }

    if (buttons.empty())
    {
        errors.add("Layout has no buttons defined");
        isValid = false;
    }

    // Check for duplicate IDs
    std::set<juce::String> seenIds;
    for (const auto& button : buttons)
    {
        if (button.id.isEmpty())
        {
            errors.add("Button has empty ID");
            isValid = false;
            continue;
        }

        if (seenIds.find(button.id) != seenIds.end())
        {
            errors.add("Duplicate button ID: " + button.id);
            isValid = false;
        }
        else
        {
            seenIds.insert(button.id);
        }

        // Check command buttons have command names
        if (button.type == ToolbarButtonType::COMMAND && button.commandName.isEmpty())
        {
            errors.add("Command button '" + button.id + "' has no commandName");
            isValid = false;
        }

        // Check plugin buttons have identifiers
        if (button.type == ToolbarButtonType::PLUGIN && button.pluginIdentifier.isEmpty())
        {
            errors.add("Plugin button '" + button.id + "' has no pluginIdentifier");
            isValid = false;
        }
    }

    // Check height is reasonable
    if (height < 24 || height > 100)
    {
        errors.add("Layout height " + juce::String(height) + " is outside valid range (24-100)");
        isValid = false;
    }

    return isValid;
}

bool ToolbarLayout::hasTransport() const
{
    for (const auto& button : buttons)
    {
        if (button.type == ToolbarButtonType::TRANSPORT)
            return true;
    }
    return false;
}

const ToolbarButtonConfig* ToolbarLayout::getButton(const juce::String& buttonId) const
{
    for (const auto& button : buttons)
    {
        if (button.id == buttonId)
            return &button;
    }
    return nullptr;
}

int ToolbarLayout::calculateMinimumWidth() const
{
    int totalWidth = 0;
    for (const auto& button : buttons)
        totalWidth += button.width;
    return totalWidth;
}
