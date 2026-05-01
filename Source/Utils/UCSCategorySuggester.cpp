/*
  ==============================================================================

    UCSCategorySuggester.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "UCSCategorySuggester.h"
#include <algorithm>

//==============================================================================
UCSCategorySuggester::UCSCategorySuggester()
{
    initializeKeywordMappings();
}

//==============================================================================
std::vector<UCSCategorySuggester::Suggestion> UCSCategorySuggester::suggestCategories(
    const juce::String& filename,
    const juce::String& description,
    const juce::String& keywords,
    int maxSuggestions) const
{
    // Tokenize all input text
    juce::StringArray allTokens;
    allTokens.addArray(tokenize(filename));
    allTokens.addArray(tokenize(description));
    allTokens.addArray(tokenize(keywords));

    // Remove duplicates (case-insensitive)
    juce::StringArray uniqueTokens;
    for (const auto& token : allTokens)
    {
        if (!uniqueTokens.contains(token))
            uniqueTokens.add(token);
    }

    // Calculate match scores for all mappings
    std::vector<Suggestion> suggestions;
    for (const auto& mapping : m_mappings)
    {
        float score = calculateMatchScore(uniqueTokens, mapping);
        if (score > 0.0f)
        {
            suggestions.emplace_back(mapping.category, mapping.subcategory, score);
        }
    }

    // Sort by confidence (highest first)
    std::sort(suggestions.begin(), suggestions.end());

    // Return top N suggestions
    if (suggestions.size() > static_cast<size_t>(maxSuggestions))
        suggestions.resize(maxSuggestions);

    return suggestions;
}

UCSCategorySuggester::Suggestion UCSCategorySuggester::getBestSuggestion(
    const juce::String& filename,
    const juce::String& description,
    const juce::String& keywords) const
{
    auto suggestions = suggestCategories(filename, description, keywords, 1);
    return suggestions.empty() ? Suggestion() : suggestions[0];
}

//==============================================================================
bool UCSCategorySuggester::matchKeyword(const juce::String& keyword,
                                        juce::String& outCategory,
                                        juce::String& outSubcategory) const
{
    juce::String lowercaseKeyword = keyword.toLowerCase();

    for (const auto& mapping : m_mappings)
    {
        if (mapping.keywords.contains(lowercaseKeyword))
        {
            outCategory = mapping.category;
            outSubcategory = mapping.subcategory;
            return true;
        }
    }

    return false;
}

//==============================================================================
juce::StringArray UCSCategorySuggester::getAllCategories() const
{
    juce::StringArray categories;
    for (const auto& mapping : m_mappings)
    {
        if (!categories.contains(mapping.category))
            categories.add(mapping.category);
    }
    return categories;
}

juce::StringArray UCSCategorySuggester::getSubcategories(const juce::String& category) const
{
    juce::String upperCategory = category.toUpperCase();
    juce::StringArray subcategories;

    for (const auto& mapping : m_mappings)
    {
        if (mapping.category == upperCategory && !subcategories.contains(mapping.subcategory))
            subcategories.add(mapping.subcategory);
    }

    return subcategories;
}

//==============================================================================
juce::StringArray UCSCategorySuggester::tokenize(const juce::String& text) const
{
    if (text.isEmpty())
        return {};

    juce::StringArray tokens;
    juce::String current;

    for (int i = 0; i < text.length(); ++i)
    {
        juce::juce_wchar c = text[i];

        // Separators: space, underscore, hyphen, comma, period, slash
        if (c == ' ' || c == '_' || c == '-' || c == ',' || c == '.' || c == '/' || c == '\\')
        {
            if (current.isNotEmpty())
            {
                tokens.add(current.toLowerCase());
                current = juce::String();
            }
        }
        // CamelCase detection: lowercase followed by uppercase
        else if (i > 0 && juce::CharacterFunctions::isLowerCase(text[i - 1]) &&
                 juce::CharacterFunctions::isUpperCase(c))
        {
            if (current.isNotEmpty())
            {
                tokens.add(current.toLowerCase());
                current = juce::String();
            }
            current += c;
        }
        else
        {
            current += c;
        }
    }

    // Add final token
    if (current.isNotEmpty())
        tokens.add(current.toLowerCase());

    return tokens;
}

//==============================================================================
float UCSCategorySuggester::calculateMatchScore(const juce::StringArray& tokens,
                                                const CategoryMapping& mapping) const
{
    if (tokens.isEmpty() || mapping.keywords.isEmpty())
        return 0.0f;

    int matches = 0;
    int totalKeywords = mapping.keywords.size();

    // Count how many mapping keywords appear in input tokens
    for (const auto& keyword : mapping.keywords)
    {
        if (tokens.contains(keyword))
            ++matches;
    }

    // Score = (matches / total keywords) weighted by category specificity
    float baseScore = static_cast<float>(matches) / static_cast<float>(totalKeywords);

    // Boost score if multiple keywords match (higher confidence)
    if (matches > 1)
        baseScore *= 1.2f;

    // Boost score if category keyword itself appears
    if (tokens.contains(mapping.category.toLowerCase()))
        baseScore *= 1.5f;

    // Cap at 1.0
    return juce::jmin(baseScore, 1.0f);
}

// initializeKeywordMappings() body lives in UCSCategorySuggester_generated.cpp
// (auto-generated from UCS v8.2.1 Full List.xlsx, kept separate per CLAUDE.md §7.5).
