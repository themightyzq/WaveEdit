/*
  ==============================================================================

    EQPresetManager.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "EQPresetManager.h"

static const juce::String kPresetExtension = ".weeq";  // WaveEdit EQ

//==============================================================================
EQPresetManager::EQPresetManager()
{
}

EQPresetManager::~EQPresetManager()
{
}

//==============================================================================
// Preset Directories

juce::File EQPresetManager::getPresetDirectory()
{
#if JUCE_MAC
    auto appSupport = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return appSupport.getChildFile("WaveEdit/Presets/EQ");
#elif JUCE_WINDOWS
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return appData.getChildFile("WaveEdit/Presets/EQ");
#else
    auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    return home.getChildFile(".waveedit/presets/eq");
#endif
}

bool EQPresetManager::ensurePresetDirectoryExists()
{
    auto dir = getPresetDirectory();
    if (!dir.exists())
    {
        return dir.createDirectory();
    }
    return true;
}

//==============================================================================
// Preset Operations

bool EQPresetManager::savePreset(const DynamicParametricEQ::Parameters& params,
                                 const juce::String& presetName)
{
    if (presetName.isEmpty())
    {
        DBG("EQPresetManager::savePreset - Empty preset name");
        return false;
    }

    if (!ensurePresetDirectoryExists())
    {
        DBG("EQPresetManager::savePreset - Failed to create preset directory");
        return false;
    }

    auto json = parametersToJson(params);
    if (json.isVoid())
    {
        DBG("EQPresetManager::savePreset - Failed to serialize parameters");
        return false;
    }

    // Add preset metadata
    auto* obj = json.getDynamicObject();
    if (obj != nullptr)
    {
        obj->setProperty("presetName", presetName);
        obj->setProperty("createdAt", juce::Time::getCurrentTime().toISO8601(true));
        obj->setProperty("version", "1.0");
    }

    auto file = getPresetFile(presetName);
    auto jsonString = juce::JSON::toString(json, true);
    return file.replaceWithText(jsonString);
}

bool EQPresetManager::loadPreset(DynamicParametricEQ::Parameters& params,
                                 const juce::String& presetName)
{
    // Check for factory preset first
    if (isFactoryPreset(presetName))
    {
        params = getFactoryPreset(presetName);
        return true;
    }

    auto file = getPresetFile(presetName);
    return importPreset(params, file);
}

bool EQPresetManager::deletePreset(const juce::String& presetName)
{
    // Cannot delete factory presets
    if (isFactoryPreset(presetName))
    {
        DBG("EQPresetManager::deletePreset - Cannot delete factory preset: " + presetName);
        return false;
    }

    auto file = getPresetFile(presetName);
    if (file.existsAsFile())
    {
        return file.deleteFile();
    }
    return false;
}

juce::StringArray EQPresetManager::getAvailablePresets()
{
    juce::StringArray presets;

    auto dir = getPresetDirectory();
    if (dir.isDirectory())
    {
        auto files = dir.findChildFiles(juce::File::findFiles, false, "*" + kPresetExtension);
        for (const auto& file : files)
        {
            presets.add(file.getFileNameWithoutExtension());
        }
    }

    presets.sort(true);  // Case-insensitive alphabetical sort
    return presets;
}

bool EQPresetManager::presetExists(const juce::String& presetName)
{
    if (isFactoryPreset(presetName))
        return true;

    return getPresetFile(presetName).existsAsFile();
}

//==============================================================================
// Export/Import

bool EQPresetManager::exportPreset(const DynamicParametricEQ::Parameters& params,
                                   const juce::File& file)
{
    auto json = parametersToJson(params);
    if (json.isVoid())
    {
        DBG("EQPresetManager::exportPreset - Failed to serialize parameters");
        return false;
    }

    // Add export metadata
    auto* obj = json.getDynamicObject();
    if (obj != nullptr)
    {
        obj->setProperty("exportedAt", juce::Time::getCurrentTime().toISO8601(true));
        obj->setProperty("version", "1.0");
    }

    auto jsonString = juce::JSON::toString(json, true);
    return file.replaceWithText(jsonString);
}

bool EQPresetManager::importPreset(DynamicParametricEQ::Parameters& params,
                                   const juce::File& file)
{
    if (!file.existsAsFile())
    {
        DBG("EQPresetManager::importPreset - File not found: " + file.getFullPathName());
        return false;
    }

    auto jsonString = file.loadFileAsString();
    if (jsonString.isEmpty())
    {
        DBG("EQPresetManager::importPreset - Empty file");
        return false;
    }

    auto json = juce::JSON::parse(jsonString);
    if (json.isVoid())
    {
        DBG("EQPresetManager::importPreset - Invalid JSON");
        return false;
    }

    return jsonToParameters(json, params);
}

//==============================================================================
// Factory Presets

juce::StringArray EQPresetManager::getFactoryPresetNames()
{
    return juce::StringArray({
        "Flat",
        "Default",
        "Vocal Presence",
        "De-Muddy",
        "Warmth",
        "Bright",
        "Bass Boost",
        "Low Shelf",
        "Low Cut",
        "High Shelf",
        "High Cut"
    });
}

bool EQPresetManager::isFactoryPreset(const juce::String& name)
{
    return getFactoryPresetNames().contains(name);
}

DynamicParametricEQ::Parameters EQPresetManager::getFactoryPreset(const juce::String& name)
{
    if (name == "Flat")           return createFlatPreset();
    if (name == "Default")        return createDefaultPreset();
    if (name == "Vocal Presence") return createVocalPresencePreset();
    if (name == "De-Muddy")       return createDeMuddyPreset();
    if (name == "Warmth")         return createWarmthPreset();
    if (name == "Bright")         return createBrightPreset();
    if (name == "Bass Boost")     return createBassBoostPreset();
    if (name == "Low Shelf")      return createLowShelfPreset();
    if (name == "Low Cut")        return createLowCutPreset();
    if (name == "High Shelf")     return createHighShelfPreset();
    if (name == "High Cut")       return createHighCutPreset();

    // Default fallback
    return createFlatPreset();
}

//==============================================================================
// Private

juce::File EQPresetManager::getPresetFile(const juce::String& presetName)
{
    auto dir = getPresetDirectory();
    return dir.getChildFile(presetName + kPresetExtension);
}

juce::var EQPresetManager::parametersToJson(const DynamicParametricEQ::Parameters& params)
{
    auto* obj = new juce::DynamicObject();

    // Parameters object
    auto* paramsObj = new juce::DynamicObject();
    paramsObj->setProperty("outputGain", params.outputGain);

    // Bands array
    juce::Array<juce::var> bandsArray;
    for (const auto& band : params.bands)
    {
        auto* bandObj = new juce::DynamicObject();
        bandObj->setProperty("frequency", band.frequency);
        bandObj->setProperty("gain", band.gain);
        bandObj->setProperty("q", band.q);
        bandObj->setProperty("filterType", filterTypeToString(band.filterType));
        bandObj->setProperty("enabled", band.enabled);
        bandsArray.add(juce::var(bandObj));
    }
    paramsObj->setProperty("bands", bandsArray);

    obj->setProperty("parameters", juce::var(paramsObj));

    return juce::var(obj);
}

bool EQPresetManager::jsonToParameters(const juce::var& json,
                                       DynamicParametricEQ::Parameters& params)
{
    if (!json.isObject())
    {
        DBG("EQPresetManager::jsonToParameters - JSON is not an object");
        return false;
    }

    auto* obj = json.getDynamicObject();
    if (obj == nullptr)
    {
        return false;
    }

    auto paramsVar = obj->getProperty("parameters");
    if (!paramsVar.isObject())
    {
        DBG("EQPresetManager::jsonToParameters - Missing 'parameters' object");
        return false;
    }

    auto* paramsObj = paramsVar.getDynamicObject();
    if (paramsObj == nullptr)
    {
        return false;
    }

    // Clear existing bands
    params.bands.clear();

    // Read output gain
    params.outputGain = paramsObj->getProperty("outputGain");

    // Read bands
    auto bandsVar = paramsObj->getProperty("bands");
    if (bandsVar.isArray())
    {
        auto* bandsArray = bandsVar.getArray();
        if (bandsArray != nullptr)
        {
            for (const auto& bandVar : *bandsArray)
            {
                if (bandVar.isObject())
                {
                    auto* bandObj = bandVar.getDynamicObject();
                    if (bandObj != nullptr)
                    {
                        DynamicParametricEQ::BandParameters band;
                        band.frequency = bandObj->getProperty("frequency");
                        band.gain = bandObj->getProperty("gain");
                        band.q = bandObj->getProperty("q");
                        band.filterType = stringToFilterType(bandObj->getProperty("filterType").toString());
                        band.enabled = bandObj->getProperty("enabled");

                        // Clamp values to valid ranges
                        band.frequency = juce::jlimit(DynamicParametricEQ::MIN_FREQUENCY,
                                                      DynamicParametricEQ::MAX_FREQUENCY,
                                                      band.frequency);
                        band.gain = juce::jlimit(DynamicParametricEQ::MIN_GAIN,
                                                 DynamicParametricEQ::MAX_GAIN,
                                                 band.gain);
                        band.q = juce::jlimit(DynamicParametricEQ::MIN_Q,
                                              DynamicParametricEQ::MAX_Q,
                                              band.q);

                        params.bands.push_back(band);

                        // Limit to MAX_BANDS
                        if (params.bands.size() >= static_cast<size_t>(DynamicParametricEQ::MAX_BANDS))
                            break;
                    }
                }
            }
        }
    }

    return true;
}

juce::String EQPresetManager::filterTypeToString(DynamicParametricEQ::FilterType type)
{
    switch (type)
    {
        case DynamicParametricEQ::FilterType::Bell:      return "Bell";
        case DynamicParametricEQ::FilterType::LowShelf:  return "LowShelf";
        case DynamicParametricEQ::FilterType::HighShelf: return "HighShelf";
        case DynamicParametricEQ::FilterType::LowCut:    return "LowCut";
        case DynamicParametricEQ::FilterType::HighCut:   return "HighCut";
        case DynamicParametricEQ::FilterType::Notch:     return "Notch";
        case DynamicParametricEQ::FilterType::Bandpass:  return "Bandpass";
        default:                                         return "Bell";
    }
}

DynamicParametricEQ::FilterType EQPresetManager::stringToFilterType(const juce::String& str)
{
    if (str == "Bell")      return DynamicParametricEQ::FilterType::Bell;
    if (str == "LowShelf")  return DynamicParametricEQ::FilterType::LowShelf;
    if (str == "HighShelf") return DynamicParametricEQ::FilterType::HighShelf;
    if (str == "LowCut")    return DynamicParametricEQ::FilterType::LowCut;
    if (str == "HighCut")   return DynamicParametricEQ::FilterType::HighCut;
    if (str == "Notch")     return DynamicParametricEQ::FilterType::Notch;
    if (str == "Bandpass")  return DynamicParametricEQ::FilterType::Bandpass;
    return DynamicParametricEQ::FilterType::Bell;  // Default
}

//==============================================================================
// Factory Preset Generators

DynamicParametricEQ::Parameters EQPresetManager::createFlatPreset()
{
    DynamicParametricEQ::Parameters params;
    params.outputGain = 0.0f;
    params.bands.clear();
    return params;
}

DynamicParametricEQ::Parameters EQPresetManager::createDefaultPreset()
{
    DynamicParametricEQ::Parameters params;
    params.outputGain = 0.0f;
    params.bands.clear();

    // Low shelf at 100Hz
    DynamicParametricEQ::BandParameters lowShelf;
    lowShelf.frequency = 100.0f;
    lowShelf.gain = 0.0f;
    lowShelf.q = DynamicParametricEQ::DEFAULT_Q;
    lowShelf.filterType = DynamicParametricEQ::FilterType::LowShelf;
    lowShelf.enabled = true;
    params.bands.push_back(lowShelf);

    // Bell at 1kHz
    DynamicParametricEQ::BandParameters midBell;
    midBell.frequency = 1000.0f;
    midBell.gain = 0.0f;
    midBell.q = 1.0f;
    midBell.filterType = DynamicParametricEQ::FilterType::Bell;
    midBell.enabled = true;
    params.bands.push_back(midBell);

    // High shelf at 8kHz
    DynamicParametricEQ::BandParameters highShelf;
    highShelf.frequency = 8000.0f;
    highShelf.gain = 0.0f;
    highShelf.q = DynamicParametricEQ::DEFAULT_Q;
    highShelf.filterType = DynamicParametricEQ::FilterType::HighShelf;
    highShelf.enabled = true;
    params.bands.push_back(highShelf);

    return params;
}

DynamicParametricEQ::Parameters EQPresetManager::createVocalPresencePreset()
{
    DynamicParametricEQ::Parameters params;
    params.outputGain = 0.0f;
    params.bands.clear();

    // High-pass filter at 80Hz (remove rumble)
    DynamicParametricEQ::BandParameters hpf;
    hpf.frequency = 80.0f;
    hpf.gain = 0.0f;
    hpf.q = DynamicParametricEQ::DEFAULT_Q;
    hpf.filterType = DynamicParametricEQ::FilterType::LowCut;
    hpf.enabled = true;
    params.bands.push_back(hpf);

    // Cut muddiness at 300Hz
    DynamicParametricEQ::BandParameters mudCut;
    mudCut.frequency = 300.0f;
    mudCut.gain = -3.0f;
    mudCut.q = 1.0f;
    mudCut.filterType = DynamicParametricEQ::FilterType::Bell;
    mudCut.enabled = true;
    params.bands.push_back(mudCut);

    // Presence boost at 3kHz
    DynamicParametricEQ::BandParameters presence;
    presence.frequency = 3000.0f;
    presence.gain = 3.0f;
    presence.q = 1.5f;
    presence.filterType = DynamicParametricEQ::FilterType::Bell;
    presence.enabled = true;
    params.bands.push_back(presence);

    // Air at 10kHz
    DynamicParametricEQ::BandParameters air;
    air.frequency = 10000.0f;
    air.gain = 2.0f;
    air.q = DynamicParametricEQ::DEFAULT_Q;
    air.filterType = DynamicParametricEQ::FilterType::HighShelf;
    air.enabled = true;
    params.bands.push_back(air);

    return params;
}

DynamicParametricEQ::Parameters EQPresetManager::createDeMuddyPreset()
{
    DynamicParametricEQ::Parameters params;
    params.outputGain = 0.0f;
    params.bands.clear();

    // High-pass at 40Hz
    DynamicParametricEQ::BandParameters hpf;
    hpf.frequency = 40.0f;
    hpf.gain = 0.0f;
    hpf.q = DynamicParametricEQ::DEFAULT_Q;
    hpf.filterType = DynamicParametricEQ::FilterType::LowCut;
    hpf.enabled = true;
    params.bands.push_back(hpf);

    // Cut mud at 250Hz
    DynamicParametricEQ::BandParameters mudCut;
    mudCut.frequency = 250.0f;
    mudCut.gain = -4.0f;
    mudCut.q = 1.2f;
    mudCut.filterType = DynamicParametricEQ::FilterType::Bell;
    mudCut.enabled = true;
    params.bands.push_back(mudCut);

    // Slight clarity boost at 4kHz
    DynamicParametricEQ::BandParameters clarity;
    clarity.frequency = 4000.0f;
    clarity.gain = 1.0f;
    clarity.q = 1.0f;
    clarity.filterType = DynamicParametricEQ::FilterType::Bell;
    clarity.enabled = true;
    params.bands.push_back(clarity);

    return params;
}

DynamicParametricEQ::Parameters EQPresetManager::createWarmthPreset()
{
    DynamicParametricEQ::Parameters params;
    params.outputGain = 0.0f;
    params.bands.clear();

    // Low shelf boost at 80Hz
    DynamicParametricEQ::BandParameters lowShelf;
    lowShelf.frequency = 80.0f;
    lowShelf.gain = 3.0f;
    lowShelf.q = DynamicParametricEQ::DEFAULT_Q;
    lowShelf.filterType = DynamicParametricEQ::FilterType::LowShelf;
    lowShelf.enabled = true;
    params.bands.push_back(lowShelf);

    // Slight cut at 400Hz (reduce boxy)
    DynamicParametricEQ::BandParameters boxyCut;
    boxyCut.frequency = 400.0f;
    boxyCut.gain = -1.0f;
    boxyCut.q = 1.0f;
    boxyCut.filterType = DynamicParametricEQ::FilterType::Bell;
    boxyCut.enabled = true;
    params.bands.push_back(boxyCut);

    // Gentle high shelf rolloff at 10kHz
    DynamicParametricEQ::BandParameters highRolloff;
    highRolloff.frequency = 10000.0f;
    highRolloff.gain = -2.0f;
    highRolloff.q = DynamicParametricEQ::DEFAULT_Q;
    highRolloff.filterType = DynamicParametricEQ::FilterType::HighShelf;
    highRolloff.enabled = true;
    params.bands.push_back(highRolloff);

    return params;
}

DynamicParametricEQ::Parameters EQPresetManager::createBrightPreset()
{
    DynamicParametricEQ::Parameters params;
    params.outputGain = 0.0f;
    params.bands.clear();

    // Slight low cut at 200Hz
    DynamicParametricEQ::BandParameters lowCut;
    lowCut.frequency = 200.0f;
    lowCut.gain = -1.0f;
    lowCut.q = DynamicParametricEQ::DEFAULT_Q;
    lowCut.filterType = DynamicParametricEQ::FilterType::LowShelf;
    lowCut.enabled = true;
    params.bands.push_back(lowCut);

    // Presence boost at 8kHz
    DynamicParametricEQ::BandParameters presence;
    presence.frequency = 8000.0f;
    presence.gain = 2.0f;
    presence.q = 1.0f;
    presence.filterType = DynamicParametricEQ::FilterType::Bell;
    presence.enabled = true;
    params.bands.push_back(presence);

    // Air boost at 12kHz
    DynamicParametricEQ::BandParameters air;
    air.frequency = 12000.0f;
    air.gain = 3.0f;
    air.q = DynamicParametricEQ::DEFAULT_Q;
    air.filterType = DynamicParametricEQ::FilterType::HighShelf;
    air.enabled = true;
    params.bands.push_back(air);

    return params;
}

DynamicParametricEQ::Parameters EQPresetManager::createBassBoostPreset()
{
    DynamicParametricEQ::Parameters params;
    params.outputGain = 0.0f;
    params.bands.clear();

    // Low shelf boost at 60Hz
    DynamicParametricEQ::BandParameters lowShelf;
    lowShelf.frequency = 60.0f;
    lowShelf.gain = 6.0f;
    lowShelf.q = DynamicParametricEQ::DEFAULT_Q;
    lowShelf.filterType = DynamicParametricEQ::FilterType::LowShelf;
    lowShelf.enabled = true;
    params.bands.push_back(lowShelf);

    // Cut mud at 300Hz to keep it tight
    DynamicParametricEQ::BandParameters mudCut;
    mudCut.frequency = 300.0f;
    mudCut.gain = -2.0f;
    mudCut.q = 1.0f;
    mudCut.filterType = DynamicParametricEQ::FilterType::Bell;
    mudCut.enabled = true;
    params.bands.push_back(mudCut);

    return params;
}

DynamicParametricEQ::Parameters EQPresetManager::createLowShelfPreset()
{
    DynamicParametricEQ::Parameters params;
    params.outputGain = 0.0f;
    params.bands.clear();

    // Single low shelf at 200Hz with gentle cut (reduce low end)
    DynamicParametricEQ::BandParameters band;
    band.frequency = 200.0f;
    band.gain = -3.0f;
    band.q = DynamicParametricEQ::DEFAULT_Q;
    band.filterType = DynamicParametricEQ::FilterType::LowShelf;
    band.enabled = true;
    params.bands.push_back(band);

    return params;
}

DynamicParametricEQ::Parameters EQPresetManager::createLowCutPreset()
{
    DynamicParametricEQ::Parameters params;
    params.outputGain = 0.0f;
    params.bands.clear();

    // High-pass filter at 80Hz (removes rumble/sub-bass)
    DynamicParametricEQ::BandParameters band;
    band.frequency = 80.0f;
    band.gain = 0.0f;  // Gain not used for cut filters
    band.q = DynamicParametricEQ::DEFAULT_Q;
    band.filterType = DynamicParametricEQ::FilterType::LowCut;
    band.enabled = true;
    params.bands.push_back(band);

    return params;
}

DynamicParametricEQ::Parameters EQPresetManager::createHighShelfPreset()
{
    DynamicParametricEQ::Parameters params;
    params.outputGain = 0.0f;
    params.bands.clear();

    // Single high shelf at 8kHz with gentle cut (reduce highs)
    DynamicParametricEQ::BandParameters band;
    band.frequency = 8000.0f;
    band.gain = -3.0f;
    band.q = DynamicParametricEQ::DEFAULT_Q;
    band.filterType = DynamicParametricEQ::FilterType::HighShelf;
    band.enabled = true;
    params.bands.push_back(band);

    return params;
}

DynamicParametricEQ::Parameters EQPresetManager::createHighCutPreset()
{
    DynamicParametricEQ::Parameters params;
    params.outputGain = 0.0f;
    params.bands.clear();

    // Low-pass filter at 12kHz (removes harsh highs/hiss)
    DynamicParametricEQ::BandParameters band;
    band.frequency = 12000.0f;
    band.gain = 0.0f;  // Gain not used for cut filters
    band.q = DynamicParametricEQ::DEFAULT_Q;
    band.filterType = DynamicParametricEQ::FilterType::HighCut;
    band.enabled = true;
    params.bands.push_back(band);

    return params;
}
