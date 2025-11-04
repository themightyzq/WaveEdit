/*
  ==============================================================================

    UCSCategorySuggester.h
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <vector>

/**
 * UCS Category Suggester for WaveEdit.
 *
 * Analyzes filenames, descriptions, and keywords to suggest appropriate
 * Universal Category System (UCS) categories and subcategories.
 *
 * Based on standard UCS taxonomy used by SoundMiner, BaseHead, and
 * professional sound libraries.
 *
 * Example:
 *   "door_wood_creak_close.wav" → Category: "DOOR", Subcategory: "Wood"
 *   "bird_chirp_morning.wav" → Category: "AMBIENCE", Subcategory: "Birdsong"
 */
class UCSCategorySuggester
{
public:
    /**
     * Suggestion result containing category, subcategory, and confidence score.
     */
    struct Suggestion
    {
        juce::String category;      // UCS Category (ALL CAPS)
        juce::String subcategory;   // UCS Subcategory (Title Case)
        float confidence;           // Confidence score (0.0 - 1.0)

        Suggestion() : confidence(0.0f) {}
        Suggestion(const juce::String& cat, const juce::String& subcat, float conf)
            : category(cat), subcategory(subcat), confidence(conf) {}

        bool operator<(const Suggestion& other) const
        {
            return confidence > other.confidence;  // Higher confidence first
        }
    };

    /**
     * Default constructor - initializes keyword mappings.
     */
    UCSCategorySuggester();

    /**
     * Suggests UCS category/subcategory based on input text.
     *
     * Analyzes filename, description, and keywords to find best match.
     *
     * @param filename Original filename (e.g., "door_wood_creak.wav")
     * @param description Optional description text
     * @param keywords Optional comma-separated keywords
     * @param maxSuggestions Maximum number of suggestions to return (default: 3)
     * @return Vector of suggestions sorted by confidence (highest first)
     */
    std::vector<Suggestion> suggestCategories(const juce::String& filename,
                                              const juce::String& description = juce::String(),
                                              const juce::String& keywords = juce::String(),
                                              int maxSuggestions = 3) const;

    /**
     * Gets the best (highest confidence) category suggestion.
     *
     * @param filename Original filename
     * @param description Optional description text
     * @param keywords Optional comma-separated keywords
     * @return Best suggestion, or empty suggestion if no match found
     */
    Suggestion getBestSuggestion(const juce::String& filename,
                                 const juce::String& description = juce::String(),
                                 const juce::String& keywords = juce::String()) const;

    /**
     * Checks if a keyword matches a known UCS category.
     *
     * @param keyword Keyword to check (case-insensitive)
     * @param outCategory Output UCS category (ALL CAPS)
     * @param outSubcategory Output UCS subcategory (Title Case)
     * @return True if match found, false otherwise
     */
    bool matchKeyword(const juce::String& keyword,
                     juce::String& outCategory,
                     juce::String& outSubcategory) const;

    /**
     * Gets all registered UCS categories.
     *
     * @return Array of UCS category names (ALL CAPS)
     */
    juce::StringArray getAllCategories() const;

    /**
     * Gets all subcategories for a given category.
     *
     * @param category UCS category (case-insensitive)
     * @return Array of subcategory names (Title Case)
     */
    juce::StringArray getSubcategories(const juce::String& category) const;

private:
    /**
     * Keyword mapping structure.
     */
    struct CategoryMapping
    {
        juce::String category;       // UCS Category (ALL CAPS)
        juce::String subcategory;    // UCS Subcategory (Title Case)
        juce::StringArray keywords;  // Associated keywords (lowercase)
    };

    /**
     * Tokenizes input text into searchable keywords.
     *
     * Handles underscores, hyphens, camelCase, and common separators.
     *
     * @param text Input text (filename, description, etc.)
     * @return Array of lowercase keywords
     */
    juce::StringArray tokenize(const juce::String& text) const;

    /**
     * Calculates match score between input tokens and category mapping.
     *
     * @param tokens Input keywords
     * @param mapping Category mapping to test
     * @return Match score (0.0 - 1.0)
     */
    float calculateMatchScore(const juce::StringArray& tokens,
                             const CategoryMapping& mapping) const;

    /**
     * Initializes keyword mappings based on UCS taxonomy.
     */
    void initializeKeywordMappings();

    // Keyword mappings: keyword -> (category, subcategory)
    std::vector<CategoryMapping> m_mappings;
};
