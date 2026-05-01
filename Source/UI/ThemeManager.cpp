/*
  ==============================================================================

    ThemeManager.cpp
    Copyright (C) 2025 ZQ SFX

  ==============================================================================
*/

#include "ThemeManager.h"

#include "../Utils/Settings.h"

namespace waveedit
{

namespace
{
    constexpr const char* kSettingsKey      = "display.theme";
    constexpr const char* kDefaultThemeId   = "dark";

    juce::String colourToHex(const juce::Colour& c)
    {
        // Always emit 8-digit ARGB so alpha round-trips. Lower-case for
        // readability in editor diffs.
        return "#" + c.toDisplayString(true).toLowerCase();
    }

    bool tryParseColour(const juce::var& v, juce::Colour& out)
    {
        if (! v.isString())
            return false;
        auto s = v.toString().trim();
        if (s.startsWithChar('#'))
            s = s.substring(1);
        if (s.length() != 6 && s.length() != 8)
            return false;
        // 6-char input → opaque RGB; 8-char input → ARGB.
        const auto value = s.getHexValue64();
        if (s.length() == 6)
            out = juce::Colour(static_cast<juce::uint32>(0xff000000u | (value & 0xffffffu)));
        else
            out = juce::Colour(static_cast<juce::uint32>(value & 0xffffffffu));
        return true;
    }

    void colourField(juce::DynamicObject& obj, const char* key, const juce::Colour& c)
    {
        obj.setProperty(juce::Identifier(key), colourToHex(c));
    }

    bool readColour(const juce::DynamicObject& obj, const char* key,
                    juce::Colour fallback, juce::Colour& out)
    {
        const auto& v = obj.getProperty(juce::Identifier(key));
        if (v.isVoid())
        {
            out = fallback;
            return true;
        }
        return tryParseColour(v, out);
    }
}

ThemeManager& ThemeManager::getInstance()
{
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager()
{
    reloadFromSettings();
}

juce::StringArray ThemeManager::getAvailableIds() const
{
    juce::StringArray ids { "dark", "light", "high-contrast" };
    for (const auto& [id, _] : m_customThemes)
        ids.add(id);
    return ids;
}

void ThemeManager::setActiveById(const juce::String& themeId)
{
    if (themeId == m_current.id)
        return;
    if (! getAvailableIds().contains(themeId))
        return;

    applyById(themeId);
    Settings::getInstance().setSetting(kSettingsKey, themeId);
    sendSynchronousChangeMessage();
}

void ThemeManager::reloadFromSettings()
{
    auto themeId = Settings::getInstance()
                       .getSetting(kSettingsKey, juce::var(kDefaultThemeId))
                       .toString();
    if (! getAvailableIds().contains(themeId))
        themeId = kDefaultThemeId;
    applyById(themeId);
    sendSynchronousChangeMessage();
}

void ThemeManager::applyById(const juce::String& themeId)
{
    if (themeId == "light")
        m_current = BuiltInThemes::light();
    else if (themeId == "high-contrast")
        m_current = BuiltInThemes::highContrast();
    else
    {
        auto it = m_customThemes.find(themeId);
        if (it != m_customThemes.end())
            m_current = it->second;
        else
            m_current = BuiltInThemes::dark();
    }
}

juce::String ThemeManager::themeToJson(const Theme& theme)
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty("id",   theme.id);
    root->setProperty("name", theme.name);

    auto tokens = std::make_unique<juce::DynamicObject>();
    colourField(*tokens, "background",         theme.background);
    colourField(*tokens, "panel",              theme.panel);
    colourField(*tokens, "panelAlternate",     theme.panelAlternate);
    colourField(*tokens, "border",             theme.border);
    colourField(*tokens, "waveformBackground", theme.waveformBackground);
    colourField(*tokens, "waveformLine",       theme.waveformLine);
    colourField(*tokens, "waveformBorder",     theme.waveformBorder);
    colourField(*tokens, "ruler",              theme.ruler);
    colourField(*tokens, "rulerText",          theme.rulerText);
    colourField(*tokens, "gridLine",           theme.gridLine);
    colourField(*tokens, "text",               theme.text);
    colourField(*tokens, "textMuted",          theme.textMuted);
    colourField(*tokens, "accent",             theme.accent);
    colourField(*tokens, "selection",          theme.selection);
    colourField(*tokens, "focus",              theme.focus);
    colourField(*tokens, "warning",            theme.warning);
    colourField(*tokens, "success",            theme.success);
    colourField(*tokens, "error",              theme.error);

    root->setProperty("tokens", juce::var(tokens.release()));
    return juce::JSON::toString(juce::var(root.release()), true);
}

bool ThemeManager::themeFromJson(const juce::String& json, Theme& out, juce::String& errorOut)
{
    juce::var parsed;
    auto result = juce::JSON::parse(json, parsed);
    if (! result.wasOk())
    {
        errorOut = "Malformed JSON: " + result.getErrorMessage();
        return false;
    }
    auto* rootObj = parsed.getDynamicObject();
    if (rootObj == nullptr)
    {
        errorOut = "Theme JSON must be an object";
        return false;
    }

    Theme fallback = BuiltInThemes::dark();
    out = fallback;

    out.id   = rootObj->getProperty("id").toString();
    out.name = rootObj->getProperty("name").toString();
    if (out.id.isEmpty() || out.name.isEmpty())
    {
        errorOut = "Theme JSON missing required `id` or `name`";
        return false;
    }

    const auto& tokensVar = rootObj->getProperty("tokens");
    auto* tokensObj = tokensVar.getDynamicObject();
    if (tokensObj == nullptr)
    {
        errorOut = "Theme JSON missing `tokens` object";
        return false;
    }

    auto check = [&](const char* key, juce::Colour fb, juce::Colour& field) -> bool
    {
        if (! readColour(*tokensObj, key, fb, field))
        {
            errorOut = juce::String("Bad colour value for `") + key + "`";
            return false;
        }
        return true;
    };

    if (! check("background",         fallback.background,         out.background))         return false;
    if (! check("panel",              fallback.panel,              out.panel))              return false;
    if (! check("panelAlternate",     fallback.panelAlternate,     out.panelAlternate))     return false;
    if (! check("border",             fallback.border,             out.border))             return false;
    if (! check("waveformBackground", fallback.waveformBackground, out.waveformBackground)) return false;
    if (! check("waveformLine",       fallback.waveformLine,       out.waveformLine))       return false;
    if (! check("waveformBorder",     fallback.waveformBorder,     out.waveformBorder))     return false;
    if (! check("ruler",              fallback.ruler,              out.ruler))              return false;
    if (! check("rulerText",          fallback.rulerText,          out.rulerText))          return false;
    if (! check("gridLine",           fallback.gridLine,           out.gridLine))           return false;
    if (! check("text",               fallback.text,               out.text))               return false;
    if (! check("textMuted",          fallback.textMuted,          out.textMuted))          return false;
    if (! check("accent",             fallback.accent,             out.accent))             return false;
    if (! check("selection",          fallback.selection,          out.selection))          return false;
    if (! check("focus",              fallback.focus,              out.focus))              return false;
    if (! check("warning",            fallback.warning,            out.warning))            return false;
    if (! check("success",            fallback.success,            out.success))            return false;
    if (! check("error",              fallback.error,              out.error))              return false;

    return true;
}

bool ThemeManager::exportCurrentTheme(const juce::File& file, juce::String& errorOut) const
{
    const auto json = themeToJson(m_current);
    if (! file.replaceWithText(json))
    {
        errorOut = "Could not write to " + file.getFullPathName();
        return false;
    }
    return true;
}

bool ThemeManager::importCustomTheme(const juce::File& file, juce::String& errorOut)
{
    if (! file.existsAsFile())
    {
        errorOut = "File does not exist: " + file.getFullPathName();
        return false;
    }
    Theme imported;
    if (! themeFromJson(file.loadFileAsString(), imported, errorOut))
        return false;

    // Reject ids that collide with built-ins so the user can't shadow them.
    if (imported.id == "dark" || imported.id == "light" || imported.id == "high-contrast")
    {
        errorOut = "Theme id `" + imported.id + "` is reserved for built-ins";
        return false;
    }

    m_customThemes[imported.id] = imported;
    setActiveById(imported.id);
    return true;
}

}  // namespace waveedit
