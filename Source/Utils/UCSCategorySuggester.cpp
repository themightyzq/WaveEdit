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

//==============================================================================
void UCSCategorySuggester::initializeKeywordMappings()
{
    m_mappings.clear();

    // Official UCS v8.2.1 Category Mappings
    // Generated from UCS v8.2.1 Full List.xlsx
    // Format: {Category, Subcategory, {keywords...}}

    // AIR Category
    m_mappings.push_back({"AIR", "BLOW",
        {"air", "aerate", "aerosol", "airhose", "balloon", "beat", "bellows", "blast", "blow", "blower", "blowgun", "blown", "blowpipe", "blows", "blowtube", "bluff", "carbon", "co2", "compressed", "depressurize", "dioxide", "duster", "exhaust", "flutter", "gust", "helium", "huff", "inflate", "nitrogen", "oxygen", "puff", "puffed", "purge", "release", "sputter", "vent", "waft", "whiff"}});
    m_mappings.push_back({"AIR", "BURST",
        {"air", "airbed", "airblast", "airgun", "airhose", "blast", "blowhole", "blowout", "burst", "carbon", "chuff", "co2", "dioxide", "discharge", "explosion", "flash", "gas", "helium", "jet", "kerboom", "nitrogen", "outburst", "oxygen", "poof", "pop", "rush", "seal", "spurt", "surge", "torrent"}});
    m_mappings.push_back({"AIR", "HISS",
        {"air", "hiss", "carbon", "co2", "dioxide", "discharge", "exhaust", "expel", "helium", "hissing", "leak", "nitrogen", "oxygen", "purr", "release", "shush", "sibilate", "whistling"}});
    m_mappings.push_back({"AIR", "MISC",
        {"air", "misc", "airtight", "airway", "carbon", "co2", "dioxide", "gas", "helium", "inflatable", "intake", "miscellaneous", "nitrogen", "oxygen", "sky", "ventilation"}});
    m_mappings.push_back({"AIR", "SUCTION",
        {"air", "aspirate", "aspiration", "carbon", "co2", "consume", "dental", "dioxide", "draw", "helium", "hoover", "ingest", "inspiration", "inspire", "intake", "nitrogen", "oxygen", "pull", "pump", "pumps", "siphon", "suck", "suction", "syphon", "syringe", "vac", "vacuity", "vacuum"}});

    // AIRCRAFT Category
    m_mappings.push_back({"AIRCRAFT", "DOOR",
        {"aircraft", "door", "airplane", "aviation", "boarding", "cabin", "cargo", "cockpit", "emergency", "entrance", "entryway", "exit", "fuselage", "hatch", "helicopter", "jet", "panel", "passenger", "ports", "trapdoor", "wing"}});
    m_mappings.push_back({"AIRCRAFT", "HELICOPTER",
        {"aircraft", "aerochopper", "apache", "autogiro", "autogyro", "bird", "blackhawk", "chopper", "choppers", "copter", "copters", "ghetto", "gyrocopter", "gyroplane", "heli", "helichopper", "helicopter", "helicopteron", "helicoptor", "helijet", "helipad", "helipilot", "heliport", "helo", "helos", "huey", "jetcopter", "lift", "medevac", "medivac", "multirotor", "ornithopter", "police", "rotary-wing", "rotodyne", "rotorcraft", "sar", "sikorsky", "skyhook", "tailwheel", "tiltrotor", "tricopter", "vertical", "vtol", "whirlybird"}});
    m_mappings.push_back({"AIRCRAFT", "INTERIOR",
        {"aircraft", "737", "747", "777", "a310", "a330", "a350", "a380", "aboard", "aeroplane", "airbus", "airliner", "airplane", "aisle", "avionics", "bay", "belly", "bins", "boeing", "bombardier", "bowels", "cabin", "cargo", "cockpit", "compartment", "crew", "dc-10", "deck", "fighter", "flight", "fuselage", "galley", "glider", "hold", "inside", "interior", "jet", "jetliner", "jumbo", "lavatory", "learjet", "midflight", "overhead", "passenger", "zeppelin"}});
    m_mappings.push_back({"AIRCRAFT", "JET",
        {"aircraft", "737", "747", "777", "a310", "a330", "a350", "a380", "aeroplane", "afterburner", "airbus", "airliner", "airplane", "boeing", "bombardier", "cargo", "commercial", "dc-10", "jet", "jetliner", "jumbo", "learjet", "passenger", "plane", "private", "ramjet", "regional", "scramjet", "supersonic", "turbojet", "twinjet", "unducted"}});
    m_mappings.push_back({"AIRCRAFT", "MECHANISM",
        {"aircraft", "mechanism", "actuators", "aerofoil", "aeroplane", "aileron", "ailerons", "airbrake", "airplane", "apron", "arrester", "autopilot", "avionics", "bombsight", "brakes", "cockpit", "column", "control", "cowl", "cowling", "devices", "doors", "elevator", "elevators", "fin", "flap", "flaps", "flight", "gear", "gimbals", "gyroscope", "hook", "hydraulic", "instrument", "landing", "lever", "pedal", "propellers", "reversers", "rudder", "slats", "spoilers", "surfaces", "systems", "throttle", "thrust", "turbines", "yoke"}});
    m_mappings.push_back({"AIRCRAFT", "MILITARY",
        {"aircraft", "a10", "aeroplane", "air", "airplane", "angels", "army", "attack", "blue", "bogey", "bomber", "combat", "drone", "f16", "f18", "f22", "f35", "fighter", "force", "gunship", "interceptor", "jet", "lockheed", "mig", "military", "mustang", "navy", "p51", "p52", "plane", "reconnaissance", "sortie", "spy", "squadron", "stealth", "strike", "surveillance", "thunderbirds", "trainer", "transport", "warbird", "warplane"}});
    m_mappings.push_back({"AIRCRAFT", "MISC",
        {"aircraft", "misc", "aeroplane", "air", "airplane", "balloon", "blimp", "dirigible", "flyer", "flyover", "glider", "hang", "hang-glider", "hot", "land", "liftoff", "parachute", "piloting", "runway", "ultralight", "zeppelin"}});
    m_mappings.push_back({"AIRCRAFT", "PROP",
        {"aircraft", "aeroplane", "airplane", "airscrew", "amphibious", "antique", "beechcraft", "biplane", "bombardier", "bushplane", "cesna", "cherokee", "crop", "cub", "duster", "floatplane", "piper", "plane", "prop", "propeller", "propjet", "seaplane", "stol", "stunt", "triplane", "turboprop", "twin-prop", "vintage"}});
    m_mappings.push_back({"AIRCRAFT", "RADIO CONTROLLED",
        {"aircraft", "radio controlled", "aerial", "airplane", "control", "controlled", "drone", "helicopter", "jet", "model", "quadcopters", "radio", "rc", "remote", "rpa", "rpv", "scale", "uas", "uav", "unmanned"}});
    m_mappings.push_back({"AIRCRAFT", "ROCKET",
        {"aircraft", "rocket", "blastoff", "booster", "hypersonic", "icbm", "jet", "jetpack", "launch", "launchers", "launching", "launchpad", "missile", "missiles", "missle", "nasa", "nuclear", "nuke", "orbit", "payload", "propellant", "propelled", "propellent", "ramjet", "ramjets", "retrorocket", "retrorockets", "rocketeer", "rocketeers", "rocketman", "rocketplane", "rocketry", "rocketship", "shuttle", "skyrocket", "soyuz", "space", "spacelab", "spaceman", "spaceplane", "spaceplanes", "spacex", "sputniks", "suborbital", "thruster", "warhead"}});

    // ALARMS Category
    m_mappings.push_back({"ALARMS", "BELL",
        {"alarms", "bell", "alert", "bank", "burglar", "caution", "clanger", "clock", "crossing", "fire", "notice", "railroad", "reminder", "school", "signal", "striker", "tower", "warning"}});
    m_mappings.push_back({"ALARMS", "BUZZER",
        {"alarms", "buzzer", "alarm", "alert", "burglar", "buzzing", "caution", "door", "doorkeeper", "doorman", "emergency", "entry", "fire", "front", "game", "home", "hospital", "intercom", "notice", "reminder", "security", "show", "signal", "warning"}});
    m_mappings.push_back({"ALARMS", "CLOCK",
        {"alarms", "alarm", "analog", "clock", "digital", "sleep", "snooze", "timepiece", "timer", "wind"}});
    m_mappings.push_back({"ALARMS", "ELECTRONIC",
        {"alarms", "electronic", "alert", "button", "car", "carbon", "caution", "co2", "detector", "digital", "home", "intruder", "intrusion", "keycard", "monoxide", "notice", "panic", "programable", "reminder", "security", "signal", "smoke", "timer", "warning"}});
    m_mappings.push_back({"ALARMS", "MISC",
        {"alarms", "misc", "alert", "anti-theft", "caution", "homing", "miscellaneous", "notice", "reminder", "signal", "warning"}});
    m_mappings.push_back({"ALARMS", "SIREN",
        {"alarms", "ahooga", "air", "air-raid", "blare", "civil", "claxon", "defense", "doppler", "hailer", "hooters", "horn", "klaxon", "raid", "siren", "sirenidae", "tornado"}});

    // AMBIENCE Category
    m_mappings.push_back({"AMBIENCE", "AIR",
        {"ambience", "air", "atmos", "atmosphere", "ambiance", "bg", "background", "calm", "cave", "clear", "clearing", "desert", "field", "forest", "garden", "mountain", "night", "noiselessly", "oasis", "peaceful", "quiet", "sedate", "serene", "silence", "silent", "snowy", "still", "tranquil", "winter"}});
    m_mappings.push_back({"AMBIENCE", "ALPINE",
        {"ambience", "alpen", "alpes", "alpin", "alpine", "alpinist", "alpinists", "alps", "andean", "andes", "atmos", "atmosphere", "ambiance", "bg", "background", "craggy", "downhill", "glacial", "highland", "hilly", "himalayas", "icefalls", "jagged", "matterhorn", "meadow", "montane", "montanic", "mountain", "mountaineer", "mountaineering", "mountainous", "mountains", "mountainscape", "mountainside", "mountaintop", "nordic", "pass", "patagonian", "peak", "peaks", "pyrenean", "pyrenees", "resort", "ridge", "rockies", "rocky", "rugged", "ski", "snowcapped", "snowy"}});
    m_mappings.push_back({"AMBIENCE", "AMUSEMENT",
        {"ambience", "amusement", "atmos", "atmosphere", "ambiance", "bg", "background", "adventure", "arcade", "biopark", "boardwalk", "bouncing", "bumper", "carnival", "carousel", "cars", "casino", "circus", "coaster", "county", "course", "disney", "disneyland", "exploratorium", "fair", "fairground", "ferris", "festival", "flags", "funfair", "funhouse", "gamepark", "go-kart", "golf", "haunted", "hellhouse", "house", "merry-go-round", "midway", "mini", "mini-golf", "park", "pinball", "playland", "roller", "six", "six-flags", "theater", "theme", "tourist"}});
    m_mappings.push_back({"AMBIENCE", "BIRDSONG",
        {"ambience", "atmos", "atmosphere", "ambiance", "bg", "background", "bird", "birdcall", "birdcalls", "birdlife", "birdsong", "birdsongs", "cacophony", "cheeping", "chirping", "chirpings", "chirps", "chittering", "chorus", "dawn", "flocks", "morning", "nightbirds", "nightingales", "peeping", "pinewoods", "pretty", "singing", "songbird", "songbirds", "songful", "soundscape", "sparrows", "spring", "susurration", "susurrus", "thrum", "trilling", "tweeting", "twittering", "twitterings", "vocalisations", "warble", "warbling", "warblings", "whistlings"}});
    m_mappings.push_back({"AMBIENCE", "CELEBRATION",
        {"ambience", "celebration", "anniversary", "atmos", "atmosphere", "ambiance", "bg", "background", "awards", "birthday", "birthdays", "blast", "blowout", "carnival", "celebrants", "ceremonials", "ceremonies", "ceremony", "championships", "commemorate", "commemorating", "commemoration", "commemorations", "commemorative", "concelebrate", "congratulating", "decorations", "drunken", "eid", "elation", "enjoyments", "entertainments", "event", "excitement", "excitements", "exhilaration", "extravaganza", "extravaganzas", "exultation", "exulting", "fairs", "feasts", "felicitating", "felicities", "fest", "festival", "festivals", "festiveness", "festivities", "festivity"}});
    m_mappings.push_back({"AMBIENCE", "CONSTRUCTION",
        {"ambience", "construction", "architect", "architectural", "architecture", "assembly", "atmos", "atmosphere", "ambiance", "bg", "background", "build", "builder", "building", "buildings", "buildup", "built", "carpentry", "construct", "constructing", "constructive", "constructor", "contracting", "contractor", "demolition", "design", "engineering", "erect", "erection", "foundation", "home", "housing", "infrastructure", "installation", "labour", "manufacture", "planning", "plot", "prefab", "preparation", "project", "projects", "provision", "rebuild", "rebuilding", "reconstruct", "reconstruction", "remodel", "reno", "renovation"}});
    m_mappings.push_back({"AMBIENCE", "DESERT",
        {"ambience", "desert", "abandon", "arabian", "arid", "aridity", "aridness", "atacama", "atmos", "atmosphere", "ambiance", "bg", "background", "badland", "badlands", "bare", "barren", "bedouin", "bedouins", "cacti", "cactuses", "canyons", "dearth", "desertlike", "desolate", "dry", "dune", "dunes", "dustbowl", "empty", "expanse", "flatlands", "forsake", "gobi", "godforsaken", "grassless", "hilltop", "hot", "hyperarid", "infertile", "inhospitable", "kalahari", "karakum", "lifeless", "lonely", "mirage", "mirages", "mohave", "mojave", "moonscape"}});
    m_mappings.push_back({"AMBIENCE", "DESIGNED",
        {"ambience", "designed", "artificial", "atmos", "atmosphere", "ambiance", "bg", "background", "constructed", "created", "manufactured"}});
    m_mappings.push_back({"AMBIENCE", "EMERGENCY",
        {"ambience", "emergency", "accident", "assistance", "atmos", "atmosphere", "ambiance", "bg", "background", "burglary", "calamity", "casualty", "catastrophe", "crime", "crises", "crisis", "danger", "disaster", "disasters", "evac", "evacuation", "evacuations", "evidence", "fighting", "fire", "hazard", "hazardous", "help", "incident", "injury", "lifesaving", "mass", "medevac", "medical", "medics", "murder", "natural", "paramedics", "pileup", "plight", "police", "rescue", "resuscitation", "resuscitative", "robbery", "scene", "search", "shooting", "sos", "succor"}});
    m_mappings.push_back({"AMBIENCE", "FANTASY",
        {"ambience", "fantasy", "adventure", "arthurian", "atmos", "atmosphere", "ambiance", "bg", "background", "castle", "costume", "dream", "dreaming", "dreamland", "dreamlands", "dreamlike", "dreams", "dreamscape", "dreamworld", "dwarven", "earth", "elven", "enchanted", "fairy", "fairyland", "fairytale", "fairytales", "fantasia", "fantasist", "fantasizing", "fantastic", "fantasyland", "fictional", "figment", "forest", "hallucination", "illusory", "imaginary", "imagination", "imaginative", "imaginings", "imitation", "kingdom", "lair", "magical", "middle", "mine", "mirage", "mystical", "mythical"}});
    m_mappings.push_back({"AMBIENCE", "FARM",
        {"ambience", "farm", "agrarian", "agricultural", "agriculture", "agro", "agronomic", "atmos", "atmosphere", "ambiance", "bg", "background", "aquaculture", "arable", "barn", "breeding", "cattle", "chickens", "corn", "corral", "cow", "crops", "cultivar", "cultivate", "cultivation", "cultured", "dairy", "farmer", "farmland", "farmstead", "goat", "grain", "grange", "grow", "grower", "harvest", "harvesting", "hatchery", "herding", "homestead", "husbandry", "land", "manufacturer", "nurture", "operation", "operational", "orchard", "pastoral", "pasture", "peasant"}});
    m_mappings.push_back({"AMBIENCE", "FOREST",
        {"ambience", "forest", "alder", "apple", "ash", "atmos", "atmosphere", "ambiance", "bg", "background", "aspen", "beech", "birch", "boreal", "boxwood", "buckeye", "cedar", "cherry", "chestnut", "conifer", "coniferous", "coppice", "coppices", "copse", "copses", "cypress", "deciduous", "deforestation", "dogwood", "elm", "fir", "forester", "foresters", "forestier", "forestland", "forestlands", "forestry", "foresty", "forrest", "forrests", "greenwood", "grove", "hemlock", "hickory", "larch", "lumbering", "magnolia", "mahogany", "maple", "oak"}});
    m_mappings.push_back({"AMBIENCE", "GRASSLAND",
        {"ambience", "grassland", "atmos", "atmosphere", "ambiance", "bg", "background", "cropland", "croplands", "farmland", "field", "floodplain", "grass", "grasses", "grassy", "grazing", "habitat", "habitats", "lowland", "meadow", "meadowland", "meadows", "moorland", "overgrazed", "pasture", "pastureland", "pastures", "plain", "plains", "prairie", "prairies", "range", "rangeland", "rangelands", "sagebrush", "savanna", "savannah", "scrublands", "semiarid", "semidesert", "seminatural", "shortgrass", "shrubland", "shrublands", "steppe", "subalpine", "tallgrass", "tussock", "unforested", "ungrazed"}});
    m_mappings.push_back({"AMBIENCE", "HISTORICAL",
        {"ambience", "historical", "ancestral", "ancient", "anthropological", "antique", "archaeologic", "archaeological", "archeological", "atmos", "atmosphere", "ambiance", "bg", "background", "biblical", "biographical", "castle", "chronological", "chronology", "city", "classic", "earlier", "epoch", "epochal", "factual", "folkloric", "former", "geographic", "geographical", "geological", "geopolitical", "ghost", "gold", "greece", "groundbreaking", "heritage", "historian", "historically", "histories", "historiographic", "historiographical", "historique", "history", "landmark", "legendary", "literary", "longstanding", "medieval", "monument", "monumental"}});
    m_mappings.push_back({"AMBIENCE", "HITECH",
        {"ambience", "hitech", "007", "advanced", "artificial", "atmos", "atmosphere", "ambiance", "bg", "background", "biotechnology", "bond", "center", "control", "cutting-edge", "cyber", "cyberactive", "cybercentric", "cybercriminal", "cybererotic", "cybergenic", "cyberliterate", "cyberoptimistic", "cyberphysical", "cyberpunky", "cyberqueer", "cyberreal", "cybersavvy", "cybersexy", "data", "digital", "futuristic", "high-tech", "innovative", "intelligence", "intercomputer", "internet", "internetlike", "internetted", "james", "lab", "laboratory", "machine", "modern", "multiserver", "multisite", "multiuser", "multiworkstation", "nanotechnology", "online"}});
    m_mappings.push_back({"AMBIENCE", "HOSPITAL",
        {"ambience", "hospital", "admission", "ambulances", "ambulatory", "atmos", "atmosphere", "ambiance", "bg", "background", "beds", "bellevue", "care", "clinic", "clinica", "clinical", "convalesces", "department", "doctor", "doctors", "emergency", "gynecology", "healthcare", "hosp", "hospice", "hospitalier", "hospitalist", "hospitality", "hospitalization", "hospitalizations", "hospitalized", "house", "icu", "infirmaries", "infirmary", "inpatient", "inpatients", "intensive", "laboratory", "maternity", "medevac", "medic", "medical", "mental", "morgue", "mortuary", "neonatal", "nurse", "nurses", "obstetrics"}});
    m_mappings.push_back({"AMBIENCE", "INDUSTRIAL",
        {"ambience", "aerospace", "assembly", "atmos", "atmosphere", "ambiance", "bg", "background", "automotive", "biomedical", "biotechnical", "chemical", "construction", "engineering", "fabrication", "facility", "factories", "factory", "foundry", "heavy", "industrial", "industrialism", "industrialists", "industrialization", "industrialized", "industrializing", "industries", "industry", "institutional", "intellectual", "labour", "line", "machinery", "manufacturing", "mercantile", "metallurgic", "mill", "mining", "park", "petrochem", "petrochemical", "pharmaceutical", "plant", "postindustrial", "power", "press", "printing", "processing", "production", "recycling"}});
    m_mappings.push_back({"AMBIENCE", "INSECT",
        {"ambience", "insect", "atmos", "atmosphere", "ambiance", "bg", "background", "bee", "beehive", "beekeeper", "bug", "chirpy", "cicadas", "cocoon", "crickets", "flies", "fly", "katydids", "locusts", "mosquito", "nest", "nested", "nesting", "pest", "pesticide", "pheromone", "swarm", "wasp"}});
    m_mappings.push_back({"AMBIENCE", "LAKESIDE",
        {"ambience", "lakeside", "atmos", "atmosphere", "ambiance", "bg", "background", "beach", "beachside", "boathouse", "boathouses", "creekside", "dock", "dockside", "fishing", "frontage", "lakefront", "lakehead", "lakes", "lakescape", "lakeshore", "lakeshores", "lakeview", "lakewater", "lapping", "loch", "lochside", "pond", "pondside", "quayside", "reservoir", "riverbank", "riverfront", "riverside", "shore", "shorefront", "shoreland", "shores", "swimming", "waterfront"}});
    m_mappings.push_back({"AMBIENCE", "MARKET",
        {"ambience", "antique", "art", "atmos", "atmosphere", "ambiance", "bg", "background", "bargain", "bazaar", "bazar", "buy", "buying", "christmas", "commerce", "community", "craft", "district", "exchange", "farmers", "fish", "flea", "food", "garage", "grocery", "hawker", "machado", "market", "marketplace", "mart", "mercado", "mercato", "merchandising", "merchant", "merchantable", "merchants", "night", "organic", "public", "purchasing", "sale", "sell", "selling", "shopping", "square", "street", "trade", "trading", "vendor", "world"}});
    m_mappings.push_back({"AMBIENCE", "MISC",
        {"ambience", "misc", "ambiance", "atmospheres", "atmospherical", "atmospherics", "miscellaneous", "surroundings"}});
    m_mappings.push_back({"AMBIENCE", "NAUTICAL",
        {"ambience", "nautical", "aft", "anchorage", "atmos", "atmosphere", "ambiance", "bg", "background", "barnacled", "beachy", "boat", "boating", "boaty", "bow", "buccaneer", "buoy", "buoyage", "deck", "dock", "docks", "harbor", "jetty", "landlubber", "landlubbers", "mapping", "marina", "marine", "mariner", "mariners", "maritime", "mercantile", "midship", "naut", "nautic", "nautique", "naval", "navicular", "navies", "navigable", "navigational", "oceangoing", "oceanic", "oceanographic", "oceanographical", "offshore", "paddling", "pier", "piratic", "piratical"}});
    m_mappings.push_back({"AMBIENCE", "OFFICE",
        {"ambience", "office", "administrative", "agency", "agent", "applicant", "appointment", "attorney", "atmos", "atmosphere", "ambiance", "bg", "background", "authority", "branch", "bureau", "business", "career", "chairmanship", "chambers", "clerical", "commission", "commissioner", "committee", "company", "corporate", "corporation", "council", "cubicle", "delegation", "dental", "department", "desk", "directorate", "employment", "firm", "government", "home", "job", "law", "management", "medical", "ministry", "official", "ombudsman", "organization", "phones", "precinct", "premises", "professional"}});
    m_mappings.push_back({"AMBIENCE", "PARK",
        {"ambience", "park", "arboretum", "area", "atmos", "atmosphere", "ambiance", "bg", "background", "ballfield", "ballpark", "banff", "botanical", "campground", "campsite", "city", "common", "commons", "dog", "esplanade", "gardens", "green", "grounds", "lawn", "national", "nature", "parc", "parcs", "parke", "parkland", "path", "pavilion", "picnic", "playground", "plaza", "public", "recreation", "reserve", "safari", "sandbox", "skate", "space", "square", "state", "suburban", "tract", "trail", "trails", "urban", "visitor"}});
    m_mappings.push_back({"AMBIENCE", "PRISON",
        {"ambience", "arrest", "attica", "atmos", "atmosphere", "ambiance", "bg", "background", "blockhouse", "brig", "captivity", "cellblock", "cellmate", "center", "clink", "confinement", "correction", "correctional", "corrections", "corrective", "county", "criminal", "custodial", "custody", "deprivation", "detained", "detainee", "detainees", "detention", "facility", "federal", "guard", "guardhouse", "gulag", "holding", "imprison", "imprisoned", "imprisonment", "incarcerated", "inmate", "inmates", "institution", "internment", "jail", "jailed", "jailer", "jailers", "jailhouse", "jailing", "jailors"}});
    m_mappings.push_back({"AMBIENCE", "PROTEST",
        {"ambience", "protest", "activism", "angry", "anti-war", "atmos", "atmosphere", "ambiance", "bg", "background", "argue", "black", "boycott", "boycotting", "boycotts", "change", "chanting", "civil", "climate", "control", "crowd", "defiance", "demonstrate", "demonstrating", "demonstration", "demonstrations", "denounce", "denouncement", "denunciation", "disapproval", "discontent", "displeasure", "dispute", "dissatisfaction", "dissent", "encampment", "environmental", "gun", "heckling", "human", "justice", "labor", "lgbtq", "lives", "march", "marches", "matter", "mutiny", "oppose", "opposition"}});
    m_mappings.push_back({"AMBIENCE", "PUBLIC PLACE",
        {"ambience", "public place", "atmos", "atmosphere", "ambiance", "bg", "background", "casino", "center", "cinema", "civic", "coffeeroom", "concessions", "convenience", "concourse", "courthouse", "courtyards", "escape", "fitness", "gallery", "grocery", "gyms", "hostel", "hotel", "hotels", "library", "lobbies", "lunchroom", "mall", "malls", "meeting", "motel", "museum", "museums", "park", "pedestrian", "plaza", "post", "premises", "public", "room", "shopping", "square", "squares", "stores", "supermarket", "theater", "venue"}});
    m_mappings.push_back({"AMBIENCE", "RELIGIOUS",
        {"ambience", "religious", "ashram", "atheist", "atheistic", "atmos", "atmosphere", "ambiance", "bg", "background", "basilica", "belief", "buddhism", "buddhist", "cathedral", "chapel", "choir", "church", "churches", "churchgoing", "churchlike", "churchly", "clergy", "clergyman", "cleric", "clerical", "clerics", "communalistic", "confessional", "convent", "coptic", "creed", "cult", "cultic", "cults", "cultural", "denomination", "devout", "dini", "divine", "ecclesiastic", "ecclesiastical", "evangelic", "evangelical", "faith", "faiths", "fundamentalist", "gurdwara", "holy", "interfaith"}});
    m_mappings.push_back({"AMBIENCE", "RESIDENTIAL",
        {"ambience", "accommodation", "accommodations", "apartment", "apartments", "atmos", "atmosphere", "ambiance", "bg", "background", "brownstone", "buildings", "bungalow", "cabin", "condo", "condominium", "cottage", "domestic", "domicile", "domiciled", "domiciliary", "dormitory", "duplex", "dwellers", "dwelling", "dwellings", "family", "farmhouse", "flat", "flats", "habitation", "habitational", "habitations", "habitative", "home", "homes", "homewards", "house", "houseboat", "household", "houses", "housing", "igloo", "inhabitants", "inhabited", "loft", "mansion", "mobile", "multifamily", "occupancy"}});
    m_mappings.push_back({"AMBIENCE", "RESTAURANT & BAR",
        {"ambience", "restaurant & bar", "automat", "atmos", "atmosphere", "ambiance", "bg", "background", "bakery", "bar", "beer", "bistro", "brasserie", "brewery", "brewpub", "burger", "cafe", "cafeteria", "canteen", "cantina", "cart", "catering", "chophouse", "club", "cocktail", "coffee", "coffeehouse", "commissary", "creperie", "culinary", "deli", "diet", "dietary", "diner", "diners", "dinery", "disco", "dive", "drive-in", "eateries", "eatery", "eating", "fast-food", "food", "foodservice", "foodstore", "gastropub", "grillroom", "hookah", "hostelry"}});
    m_mappings.push_back({"AMBIENCE", "ROOM TONE",
        {"ambience", "air", "ambient", "atmos", "atmosphere", "ambiance", "bg", "background", "attic", "basement", "bathroom", "bedroom", "conference", "conservatory", "dead", "den", "dining", "garage", "hotel", "kitchen", "library", "living", "lobby", "office", "quiet", "room", "roomtone", "room tone", "study", "sunroom", "tone", "waiting", "whirr"}});
    m_mappings.push_back({"AMBIENCE", "RURAL",
        {"ambience", "rural", "atmos", "atmosphere", "ambiance", "bg", "background", "canyon", "coulee", "country", "countryside", "hill", "midwestern", "plateau", "remote", "rolling", "rurale", "rurales", "ruralist", "ruralistic", "ruralness", "rurals", "rustic", "savanna", "scenic", "scrub", "scrubland", "valley"}});
    m_mappings.push_back({"AMBIENCE", "SCHOOL",
        {"ambience", "school", "academic", "academy", "admission", "assembly", "attendance", "atmos", "atmosphere", "ambiance", "bg", "background", "baccalaureate", "blackboard", "boarding", "cafeteria", "campus", "class", "classroom", "college", "collegium", "community", "courses", "curricular", "curriculum", "dormitory", "dropout", "educate", "education", "educational", "elementary", "enrolment", "faculty", "grade", "graduate", "gym", "high", "institute", "institution", "instructional", "international", "junior", "kindergarten", "learn", "learning", "lesson", "lyceum", "middle", "montessori", "nursery"}});
    m_mappings.push_back({"AMBIENCE", "SCIFI",
        {"ambience", "scifi", "alien", "anime", "artificial", "asimov", "atmos", "atmosphere", "ambiance", "bg", "background", "beowulf", "center", "chamber", "cheesy", "city", "command", "cyberpunk", "dystopian", "extraterrestrial", "fiction", "future", "futuristic", "genetic", "gravity", "hi-tec", "high-tech", "holographic", "intelligence", "interstellar", "manga", "mars", "martians", "moon", "moonscape", "nanotechnology", "nerds", "planet", "planets", "portal", "sci", "sci-fi", "science", "singularity", "space", "spaceship", "star", "starwars", "station", "steampunk"}});
    m_mappings.push_back({"AMBIENCE", "SEASIDE",
        {"ambience", "atmos", "atmosphere", "ambiance", "bg", "background", "bathing", "bay", "bayside", "beach", "beaches", "beachfront", "beachline", "beachscape", "beachside", "beachy", "boardwalk", "coast", "coastal", "coastland", "coastline", "coasts", "costal", "cove", "dock", "embankment", "esplanade", "harbor", "harborside", "headland", "inshore", "intertidal", "lighthouse", "marina", "mediterranean", "ocean", "oceanfront", "oceanside", "pier", "promenade", "quay", "quayside", "resort", "sandbeach", "sandy", "sea", "seaboard", "seacliff", "seacoast", "seafront"}});
    m_mappings.push_back({"AMBIENCE", "SPORT",
        {"ambience", "sport", "arena", "athletic", "atmos", "atmosphere", "ambiance", "bg", "background", "baseball", "basketball", "boxing", "challenge", "competition", "complex", "court", "cricket", "event", "facility", "field", "football", "gymnasium", "hockey", "league", "little", "mlb", "mma", "nba", "nfl", "practice", "soccer", "sports", "stadium", "tennis", "track", "training", "wrestling"}});
    m_mappings.push_back({"AMBIENCE", "SUBURBAN",
        {"ambience", "suburban", "affluent", "atmos", "atmosphere", "ambiance", "bg", "background", "barrio", "bucolic", "burbs", "community", "cul-de-sac", "development", "exurb", "exurban", "exurbia", "exurbs", "gated", "hamlet", "insular", "interurban", "middleclass", "neighborhood", "outskirts", "residential", "sprawl", "suburb", "suburbanism", "suburbanite", "suburbanites", "suburbia", "tract", "village"}});
    m_mappings.push_back({"AMBIENCE", "SWAMP",
        {"ambience", "swamp", "atmos", "atmosphere", "ambiance", "bg", "background", "backwater", "backwaters", "bayou", "bayous", "bog", "boggy", "bogs", "bottomland", "bottomlands", "brackish", "everglade", "everglades", "fen", "fenland", "fens", "fenway", "fetid", "flood", "freshwater", "frog", "lagoon", "lowland", "mangrove", "marais", "marsh", "marshes", "marshland", "marshlands", "marshy", "mire", "mires", "moat", "morass", "morasses", "mucky", "mudbank", "muddy", "mudflat", "mudhole", "mudholes", "murk", "muskeg", "pocosin"}});
    m_mappings.push_back({"AMBIENCE", "TOWN",
        {"ambience", "atmos", "atmosphere", "ambiance", "bg", "background", "borough", "boroughs", "burg", "capital", "center", "cities", "ciudad", "commune", "community", "hamlet", "hometown", "local", "localities", "locality", "location", "main", "municipalities", "municipality", "neighborhood", "settlement", "small", "square", "street", "town", "townhall", "townish", "townsfolk", "township", "townspeople", "village", "villages"}});
    m_mappings.push_back({"AMBIENCE", "TRAFFIC",
        {"ambience", "traffic", "atmos", "atmosphere", "ambiance", "bg", "background", "avenue", "boulevard", "bridge", "bys", "car", "cars", "circle", "congestion", "detour", "expressway", "freeway", "highway", "hour", "intersection", "jam", "lane", "motorway", "overpass", "road", "roads", "roundabout", "route", "rush", "signage", "signaling", "street", "toll", "transportation", "travelling", "underpass", "vehicles", "vehicular", "wash", "washy"}});
    m_mappings.push_back({"AMBIENCE", "TRANSPORTATION",
        {"ambience", "transportation", "air", "airfield", "airport", "area", "atmos", "atmosphere", "ambiance", "bg", "background", "booth", "border", "bus", "cable", "car", "center", "concourse", "control", "crossing", "customs", "depot", "ferry", "freight", "garage", "heliport", "highway", "hub", "immigration", "inspection", "metro", "parking", "port", "rail", "rest", "seaport", "station", "stop", "subway", "terminal", "terminus", "toll", "tower", "traffic", "train", "tram", "transit", "transport", "truck", "tube"}});
    m_mappings.push_back({"AMBIENCE", "TROPICAL",
        {"ambience", "amazon", "atmos", "atmosphere", "ambiance", "bg", "background", "biodiversity", "borneo", "bromeliad", "bush", "canopy", "caribbean", "cloud", "congo", "dengue", "equatorial", "exotic", "forest", "hawaiian", "humid", "island", "jungle", "lush", "mangrove", "mediterranean", "midlatitude", "monsoonal", "neotropical", "paradise", "rainforest", "semitropical", "southeast", "subhumid", "subtemperate", "subtropic", "subtropical", "subtropics", "sultry", "tropic", "tropical", "tropicalia", "tropicalian", "tropicalist", "tropics"}});
    m_mappings.push_back({"AMBIENCE", "TUNDRA",
        {"ambience", "tundra", "antarctic", "antarctica", "arctic", "arctics", "atmos", "atmosphere", "ambiance", "bg", "background", "badlands", "barren", "barrens", "dogsled", "dogsleds", "frozen", "glacier", "glaciers", "greenland", "ice", "icebergs", "icebound", "icecap", "icecaps", "icefield", "icefields", "icy", "igloos", "moss", "mosses", "muskeg", "muskegs", "muskox", "north", "permafrost", "polar", "pole", "siberia", "snow", "snowless", "south", "steppes", "subarctic", "treeless", "wastelands", "yukon"}});
    m_mappings.push_back({"AMBIENCE", "UNDERGROUND",
        {"ambience", "underground", "atmos", "atmosphere", "ambiance", "bg", "background", "basement", "belowground", "bomb", "bunker", "buried", "catacomb", "catacombs", "cave", "cavern", "caverns", "caves", "cellar", "crypt", "drips", "dungeon", "echoes", "groundwater", "hideout", "mine", "passage", "passageway", "sewer", "shaft", "shelter", "subsurface", "subterranean", "subterraneous", "subway", "sunken", "tunnel", "tunnels"}});
    m_mappings.push_back({"AMBIENCE", "UNDERWATER",
        {"ambience", "underwater", "aqualung", "aquanaut", "aquanauts", "aquarian", "aquarium", "aquatic", "aquatile", "atlantean", "atmos", "atmosphere", "ambiance", "bg", "background", "bathypelagic", "bathyscaphe", "bathysphere", "bottom", "cave", "chthonian", "chthonic", "coral", "deepsea", "deepwater", "diver", "divers", "frogmen", "kelp", "marine", "neptunian", "ocean", "oceanic", "reef", "sea", "seabed", "seafloor", "seamount", "seawater", "sharks", "shipwreck", "spearfishing", "spelunking", "subaquatic", "subareal", "subcontinental", "submerged", "submergible", "submerging", "submersibles"}});
    m_mappings.push_back({"AMBIENCE", "URBAN",
        {"ambience", "urban", "alley", "atmos", "atmosphere", "ambiance", "bg", "background", "bridge", "bustling", "center", "centre", "cities", "citified", "city", "cityfied", "cityscape", "cityside", "civic", "civilian", "congestion", "cosmopolitan", "dense", "district", "downtown", "financial", "ghetto", "high", "honk", "hour", "inner", "innercity", "intercity", "interurban", "megacity", "metro", "metropolis", "metropolitan", "municipal", "municipalities", "overpass", "pedestrian", "plaza", "public", "rise", "row", "rush", "sidewalk", "skid", "skyscraper"}});
    m_mappings.push_back({"AMBIENCE", "WARFARE",
        {"ambience", "warfare", "air-raid", "assault", "barracks", "atmos", "atmosphere", "ambiance", "bg", "background", "barrage", "base", "battle", "battlefield", "battles", "biowarfare", "blitz", "blitzkrieg", "blitzkriegs", "bombardment", "bombardments", "brigandage", "broadside", "bunker", "cannonade", "center", "checkpoint", "combat", "command", "conflict", "conquest", "engagement", "espionage", "fight", "flak", "foxhole", "fusillade", "hostilities", "invasions", "jihad", "jihads", "militarism", "military", "offensive", "operation", "operational", "operations", "raid", "salvo", "skirmish"}});

    // ANIMALS Category
    m_mappings.push_back({"ANIMALS", "AMPHIBIAN",
        {"animals", "amphibian", "american", "amniotes", "amphibia", "axolotl", "axolotls", "bullfrog", "caecilian", "cane", "chorus", "croaker", "dart", "eastern", "fire-bellied", "frog", "frogs", "leopard", "marbled", "mudfish", "mudpuppies", "newt", "northern", "peeper", "poison", "red-eyed", "salamander", "salamanders", "spotted", "spring", "tadpole", "tadpoles", "terrapins", "toad", "toads", "tree", "western", "yellow-bellied"}});
    m_mappings.push_back({"ANIMALS", "AQUATIC",
        {"animals", "angelfish", "aquatic", "barracuda", "beluga", "cetacean", "clam", "clownfish", "crab", "crabs", "crustacean", "cuttlefish", "dolphin", "dugong", "eel", "eels", "elephant", "fish", "jellyfish", "killer", "lamprey", "lion", "lobster", "lobsters", "manatee", "manta", "marine", "narwhal", "ocean", "octopus", "orca", "otter", "porpoise", "ray", "scallop", "sea", "seahorse", "seal", "shark", "shrimp", "squid", "starfish", "stingray", "turtle", "urchin", "walrus", "whale"}});
    m_mappings.push_back({"ANIMALS", "BAT",
        {"animals", "bat", "belfry", "brown", "chiropteran", "echolocation", "flying", "fox", "fruit", "mammal", "nocturnal", "roosting", "roosts", "vampire", "winged"}});
    m_mappings.push_back({"ANIMALS", "CAT DOMESTIC",
        {"animals", "cat domestic", "abyssinian", "alley", "burmese", "calico", "cat", "catfight", "coon", "egyptian", "fat", "felidae", "feline", "felines", "felis", "hairball", "himalayan", "hiss", "housecat", "kitten", "kittens", "kitties", "kitty", "maine", "meow", "persian", "purr", "pussycat", "siamese", "tabby", "tomcat", "tomcats", "whiskers"}});
    m_mappings.push_back({"ANIMALS", "CAT WILD",
        {"animals", "cat wild", "bengal", "big", "bobcat", "caracal", "cat", "cheetah", "cheetahs", "civet", "civets", "cougar", "cougars", "hiss", "jaguar", "leopard", "leopards", "lion", "lioness", "lionesses", "lions", "lynx", "lynxes", "manx", "ocelot", "ocelots", "panther", "polecats", "puma", "purr", "serval", "snow", "tiger", "tigers", "wildcat", "wildcats"}});
    m_mappings.push_back({"ANIMALS", "DOG", {"animals", "dog", "ucs marke"}});
    m_mappings.push_back({"ANIMALS", "DOG WILD",
        {"animals", "dog wild", "african", "arctic", "bush", "canid", "canids", "canines", "corsac", "coyote", "dhole", "dingo", "dingoes", "dog", "ethiopian", "fennec", "fox", "foxes", "gray", "hyaena", "hyena", "hyenas", "indian", "island", "jackal", "jackals", "lupus", "maned", "red", "wild", "wolf", "wolfish", "wolflike", "wolfs", "wolves"}});
    m_mappings.push_back({"ANIMALS", "FARM",
        {"animals", "farm", "alpaca", "angus", "barnyard", "beef", "bison", "bovine", "bovines", "buffaloes", "bullocks", "bulls", "cattle", "cattlemen", "cow", "domesticated", "domesticates", "donkey", "eew", "farms", "farmsteads", "farmyard", "feedlot", "goat", "goats", "hereford", "hogs", "holstein", "jersey", "lamb", "llama", "mule", "pig", "piggery", "piggies", "piglets", "ram", "reindeer", "ruminant", "ruminants", "sheep", "steer", "wagyu"}});
    m_mappings.push_back({"ANIMALS", "HORSE",
        {"animals", "arabian", "arabians", "ass", "clydesdale", "colt", "donkey", "donkeys", "equestrian", "equestrians", "equid", "equids", "equine", "equines", "equus", "filly", "foal", "foals", "gelding", "geldings", "hoofs", "horse", "horseback", "horseflesh", "horseman", "horsemanship", "horsemen", "horseraces", "horsewoman", "horsey", "horsy", "jockeys", "mare", "mares", "mule", "mules", "mustang", "mustangs", "packhorses", "palominos", "ponies", "pony", "quarter", "racehorse", "racehorses", "riders", "saddles", "sawbuck", "shetland", "stables"}});
    m_mappings.push_back({"ANIMALS", "INSECT",
        {"animals", "insect", "ant", "aphid", "bee", "bees", "beetle", "beetles", "budgie", "bugs", "butterfly", "buzz", "caterpillar", "cicada", "cockroach", "conehead", "cricket", "damselfly", "dragonfly", "entomology", "firefly", "flea", "fly", "gnat", "grasshopper", "hornet", "katydid", "ladybug", "locust", "mantis", "monarch", "mosquito", "mosquitoes", "moth", "pests", "praying", "spider", "termite", "tick", "wasp", "weta"}});
    m_mappings.push_back({"ANIMALS", "MISC", {"animals", "misc", "miscellaneous", "zoology"}});
    m_mappings.push_back({"ANIMALS", "PRIMATE",
        {"animals", "primate", "ape", "apes", "baboon", "baboons", "bonobo", "bonobos", "bushbaby", "capuchin", "chimpanzee", "chimpanzees", "chimps", "colobus", "gibbon", "gibbons", "gorilla", "gorillas", "great", "langur", "langurs", "lemur", "lemurs", "macaque", "macaques", "marmoset", "marmosets", "monkey", "monkeys", "orangutan", "orangutans", "primates", "prosimians", "squirrel", "tarsier", "tarsiers"}});
    m_mappings.push_back({"ANIMALS", "REPTILE",
        {"animals", "reptile", "alligator", "alligators", "bearded", "boas", "caiman", "chameleon", "chameleons", "cobras", "crocodile", "crocodiles", "dragon", "dragons", "gecko", "geckos", "gharial", "gila", "iguana", "iguanas", "komodo", "lizard", "lizards", "monitor", "monitors", "monsters", "pythons", "rattlesnake", "reptiles", "serpents", "skink", "skinks", "snake", "snakes", "terrapin", "tortoise", "tortoises", "turtle", "turtles", "vipers"}});
    m_mappings.push_back({"ANIMALS", "RODENT",
        {"animals", "beaver", "capybara", "chinchilla", "chipmunk", "chipmunks", "dog", "dormice", "gerbil", "gerbils", "gnawing", "gopher", "gophers", "groundhog", "guinea", "hamster", "hamsters", "jerboa", "jerboas", "kangaroo", "lemming", "marmot", "marmots", "mice", "mole", "mouse", "muskrat", "nutria", "opossums", "packrat", "pig", "porcupine", "possums", "prairie", "rat", "rats", "rodent", "rodents", "shrew", "squirrel", "vermin", "vole", "voles"}});
    m_mappings.push_back({"ANIMALS", "WILD",
        {"animals", "wild", "aardvark", "animalism", "animalistic", "anteater", "antelope", "badger", "bear", "bison", "boar", "buffalo", "caribou", "carnivores", "creatures", "critters", "deer", "devil", "elephant", "elk", "endangered", "gazelle", "giraffe", "grisly", "hippo", "hippopotamus", "impala", "kangaroo", "koala", "mammals", "moose", "ox", "panda", "platypus", "polar", "predators", "predatory", "quadrupeds", "raccoon", "red", "reindeer", "rhino", "rhinoceros", "sloth", "tapir", "tasmanian", "wallaby", "warthog", "wildebeest", "wildness"}});

    // ARCHIVED Category
    m_mappings.push_back({"ARCHIVED", "ADR",
        {"archived", "adr", "additional", "automated", "dialog", "dubbing", "loop", "recording", "replacement", "voiceover"}});
    m_mappings.push_back({"ARCHIVED", "ASSET", {"archived", "asset", "game", "resource", "tag"}});
    m_mappings.push_back({"ARCHIVED", "BOUNCE",
        {"archived", "avid", "bounce", "crash", "crashdown", "down", "mix", "recordings", "temp"}});
    m_mappings.push_back({"ARCHIVED", "IMPULSE RESPONSE",
        {"archived", "impulse response", "altiverb", "convolution", "early", "echo", "impulse", "ir", "late", "reflection", "reflections", "response", "reverb", "sample", "tail"}});
    m_mappings.push_back({"ARCHIVED", "LOOP GROUP",
        {"archived", "loop group", "actors", "adr", "group", "loop", "walla"}});
    m_mappings.push_back({"ARCHIVED", "MIX",
        {"archived", "5.1", "7.1", "atmos", "audio", "binaural", "blu-ray", "dolby", "dvd", "final", "imax", "m&e", "master", "mastering", "mix", "mixdown", "nearfield", "premix", "printmaster", "recordings", "remix", "stem", "stereo", "streaming", "surround", "theatrical"}});
    m_mappings.push_back({"ARCHIVED", "PFX", {"archived", "pfx", "effect", "production", "recording", "set"}});
    m_mappings.push_back({"ARCHIVED", "PRODUCTION",
        {"archived", "production", "dialog", "dx", "line", "original", "recording", "set", "take"}});
    m_mappings.push_back({"ARCHIVED", "RAW",
        {"archived", "dat", "files", "original", "raw", "source", "tape", "unaltered", "unedited", "unmodified", "unprocessed", "untreated"}});
    m_mappings.push_back({"ARCHIVED", "REFERENCE",
        {"archived", "reference", "contextual", "demo", "example", "explanatory", "guide", "identification", "informational", "inspiration", "instructional", "materials", "mock-up", "placeholder", "refer", "referenced", "references", "referencing", "researched", "researching", "resources", "sample", "temporary", "track"}});
    m_mappings.push_back({"ARCHIVED", "SCENE",
        {"archived", "documentary", "episode", "footage", "montage", "movie", "prebuilt", "readymade", "scenario", "scene", "segment", "sequence", "series", "tv"}});
    m_mappings.push_back({"ARCHIVED", "TEST TONE",
        {"archived", "test tone", "2-pops", "adr", "audio", "beeps", "bleeping", "calibration", "dolby", "frequency", "generator", "ir", "monitoring", "noise", "pink", "reference", "signal", "sine", "smpte", "square", "sweep", "sweeps", "test", "tones", "wave", "white"}});
    m_mappings.push_back({"ARCHIVED", "TRADEMARKED",
        {"archived", "trademarked", "brand", "branded", "branding", "brandname", "brandnames", "copyright", "copyrightable", "copyrighted", "corporate", "iconic", "infringed", "legal", "licensable", "licensed", "logo", "logos", "patent", "patented", "product", "proprietary", "protected", "registered", "restricted", "slogan", "sound", "trademark", "trademarks", "tradename", "tradenames"}});
    m_mappings.push_back({"ARCHIVED", "WORK IN PROGRESS",
        {"archived", "work in progress", "backlog", "in", "ongoing", "progress", "project", "task.", "temp", "unfinished", "upcoming", "wip", "work", "workflow", "worklist", "worklog"}});
    m_mappings.push_back({"ARCHIVED", "WTF",
        {"archived", "wtf", "baffling", "jokes", "perplexing", "puzzling", "uncategorizable", "unknown"}});

    // BEEPS Category
    m_mappings.push_back({"BEEPS", "APPLIANCE",
        {"beeps", "appliance", "air", "blender", "bread", "cleaner", "coffee", "conditioner", "cooker", "cooktop", "dishwasher", "dishwashers", "dishwashing", "dryer", "electric", "fan", "food", "hair", "heater", "iron", "kettle", "machine", "maker", "microwave", "mixer", "oven", "processor", "refrigerator", "rice", "slow", "stand", "toaster", "vacuum", "washing"}});
    m_mappings.push_back({"BEEPS", "GENERAL",
        {"beeps", "general", "alarms", "alerts", "beep", "beeper", "bleep", "bleeper", "bleeps", "blinks", "blipping", "chimes", "chirps", "confirmation", "diagnostic", "error", "homing", "keypad", "menu", "navigation", "notifications", "pings", "shutdown", "signals", "tones", "warnings"}});
    m_mappings.push_back({"BEEPS", "LOFI",
        {"beeps", "lofi", "8-bit", "analog", "atari", "classic", "colecovision", "distorted", "fuzzy", "nostalgic", "old-school", "retro", "static", "vintage"}});
    m_mappings.push_back({"BEEPS", "MEDICAL",
        {"beeps", "medical", "ecg", "eeg", "ekg", "flatline", "heart", "kdc", "monitor", "oximeter", "pulse", "ultrasound"}});
    m_mappings.push_back({"BEEPS", "TIMER",
        {"beeps", "countdown", "digital", "kitchen", "microwave", "oven", "phone", "timer", "watch"}});
    m_mappings.push_back({"BEEPS", "VEHICLE",
        {"beeps", "vehicle", "alert", "backup", "beep", "blink", "blinker", "forklift", "reverse", "reversing", "signal", "warning"}});

    // BELLS Category
    m_mappings.push_back({"BELLS", "ANIMAL",
        {"bells", "animal", "bell", "collar", "cowbell", "hawkbell", "sheep", "sleigh"}});
    m_mappings.push_back({"BELLS", "DOORBELL",
        {"bells", "chime", "ding", "ding-dong", "dong", "door", "doorbell", "ring"}});
    m_mappings.push_back({"BELLS", "GONG",
        {"bells", "agung", "bowl", "buddhist", "gamelan", "gong", "meditation", "tam-tam", "temple", "tibetan"}});
    m_mappings.push_back({"BELLS", "HANDBELL",
        {"bells", "handbell", "alter", "butler", "dinner", "hand", "jingle", "school", "service"}});
    m_mappings.push_back({"BELLS", "LARGE",
        {"bells", "large", "ben", "big", "cathedral", "church", "liberty", "peal", "temple", "tower"}});
    m_mappings.push_back({"BELLS", "MISC", {"bells", "misc", "bicycle", "fire", "school", "train", "tram", "trolley"}});

    // BIRDS Category
    m_mappings.push_back({"BIRDS", "BIRD OF PREY",
        {"birds", "bird of prey", "avivore", "bald", "birdlover", "birdseye", "birdtrap", "buzzard", "caracara", "condor", "eagle", "eaglehawk", "falcon", "fishhawk", "golden", "goshawk", "harrier", "hawk", "hawkling", "kestrel", "kite", "merlin", "osprey", "owl", "peregrine", "raptor", "red-tailed", "redtail", "sparhawk", "sparrowhawk", "swallow-tailed", "talon", "verreaux", "vulture", "white-tailed"}});
    m_mappings.push_back({"BIRDS", "CROW",
        {"birds", "beak", "blue", "blujay", "cawed", "cawing", "caws", "chough", "choughs", "corvid", "corvine", "crow", "crowe", "grackle", "jackdaw", "jay", "jays", "magpie", "nutcracker", "raven", "ravens", "rook", "rooks", "squawking", "treecreeper", "treepies"}});
    m_mappings.push_back({"BIRDS", "FOWL",
        {"birds", "barnyard", "capon", "chachalacas", "chicken", "chitterling", "chukar", "cockerel", "curassow", "duck", "duckling", "ducks", "eider", "emu", "fowl", "fowle", "gamebirds", "geese", "giblet", "goose", "grebe", "grouse", "guans", "guineafowl", "hen", "hens", "mallard", "muscovy", "ostrich", "partridge", "partridges", "peafowl", "pheasant", "pheasants", "plumage", "poulterer", "poussin", "ptarmigan", "quail", "quails", "rooster", "squab", "swan", "teal", "turkey", "turkeys", "turkies", "waterfowl", "woodcocks"}});
    m_mappings.push_back({"BIRDS", "MISC",
        {"birds", "misc", "ani", "cassowary", "cuckoo", "emu", "hummingbird", "kiwi", "nighthawk", "nightjar", "ostrich", "poorwill", "roadrunner", "woodpecker"}});
    m_mappings.push_back({"BIRDS", "SEA",
        {"birds", "sea", "albatross", "auk", "auklet", "black-legged", "booby", "bowerbird", "brown", "cormorant", "dunlin", "frigatebird", "fulmar", "gannet", "guillemot", "gull", "kittiwake", "migratory", "murrelet", "noddy", "oystercatcher", "pelican", "penguin", "petrel", "plover", "puffin", "razorbill", "sandpiper", "sandplover", "seabird", "seaduck", "seafowl", "seagull", "seahawk", "shearwater", "sheerwater", "shorebird", "skua", "skuas", "tern", "waterfowl", "widowbird", "yellow"}});
    m_mappings.push_back({"BIRDS", "SONGBIRD",
        {"birds", "songbird", "antechinus", "bellbird", "bellbirds", "birdcall", "birdy", "blackbird", "bluebird", "bobwhite", "bowerbird", "bowerbirds", "budgerigar", "bullfinch", "bunting", "canary", "cardinal", "chaffinch", "chat", "chickadee", "chirrups", "cisticola", "cockatiel", "creeper", "dipper", "dove", "finch", "flycatcher", "gnatcatcher", "greenfinch", "grosbeak", "hawfinch", "honeyeater", "kinglet", "lark", "lovebird", "meadowlark", "mockingbird", "nightingale", "nuthatch", "oriole", "oscine", "ovenbird", "parula", "penduline", "pigeon", "robin", "serin", "skylark", "songful"}});
    m_mappings.push_back({"BIRDS", "TROPICAL",
        {"birds", "tropical", "antbird", "aracari", "barbet", "bellbird", "bulbul", "cockatoo", "coquette", "eye", "firebird", "honeycreeper", "hornbill", "ibis", "jacamar", "kookaburra", "lorikeet", "lyrebird", "macaw", "manakin", "mango", "motmot", "mynah", "mynas", "parakeet", "parrot", "parroting", "parrots", "peacock", "potoo", "puffbird", "quetzal", "runner", "shoebill", "skimmer", "sunbird", "tanager", "toucan", "trogon", "tropicbird"}});
    m_mappings.push_back({"BIRDS", "WADING",
        {"birds", "wading", "avocet", "bittern", "blue", "bower", "coot", "crane", "curlew", "dowitcher", "egret", "flamingo", "godwit", "heron", "ibis", "little", "oystercatcher", "plover", "rail", "redshank", "ruff", "sanderling", "sandpiper", "shorebird", "snipe", "sora", "spoonbill", "stork", "wader", "waterbird", "waterfowl", "whimbrel", "wildfowl", "yellowlegs"}});

    // BOATS Category
    m_mappings.push_back({"BOATS", "AIR BOAT",
        {"boats", "air boat", "air", "amphibious", "boat", "fan", "hovercraft", "swamp"}});
    m_mappings.push_back({"BOATS", "BOW WASH", {"boats", "bow wash", "bow", "propeller", "ripple", "wake", "wave"}});
    m_mappings.push_back({"BOATS", "DOOR",
        {"boats", "access", "boat", "bulkhead", "cabin", "door", "ferry", "gangway", "gate", "hatch", "ship", "trapdoor", "watertight"}});
    m_mappings.push_back({"BOATS", "ELECTRIC",
        {"boats", "electric", "dc", "electrical", "motor", "powered", "trolling"}});
    m_mappings.push_back({"BOATS", "FISHING",
        {"boats", "fishing", "angling", "bass", "boat", "crab", "dredger", "fisher", "fisheries", "fisherman", "fishermen", "fishery", "fleet", "gillnetter", "jon", "line", "research", "seiner", "sportfishing", "trawl", "trawler", "trawlers", "vessel", "vessels"}});
    m_mappings.push_back({"BOATS", "HORN",
        {"boats", "air", "blast", "canal", "departure", "ferry", "horn", "ports", "ship", "tugboat", "warning"}});
    m_mappings.push_back({"BOATS", "INTERIOR",
        {"boats", "aboard", "balcony", "belowdecks", "berth", "bilge", "boat", "bowels", "bunk", "cabin", "cargo", "cockpit", "cruise", "engine", "ferries", "ferry", "freighter", "galley", "head", "helm", "inside", "interior", "military", "navy", "room", "sailboat", "salon", "ship", "stateroom", "stowaway", "submarine", "tanker", "tugboat", "yacht"}});
    m_mappings.push_back({"BOATS", "MECHANISM",
        {"boats", "mechanism", "anchor", "cage", "davit", "grapnel", "jigger", "net", "propeller", "pulley", "rigging", "rope", "rudder", "ship", "throttle", "trap", "wheel", "winch", "windlass"}});
    m_mappings.push_back({"BOATS", "MILITARY",
        {"boats", "military", "aircraft", "amphibious", "armada", "attack", "battlecruiser", "battleship", "blockade", "boat", "carrier", "corvette", "craft", "cruiser", "cruisier", "destroyer", "dreadnought", "fast", "fleet", "flotilla", "frigate", "gunboat", "landing", "naval", "patrol", "submarine", "torpedo", "warship"}});
    m_mappings.push_back({"BOATS", "MISC",
        {"boats", "misc", "afloat", "barge", "boatswain", "bosun", "capsize", "capsizing", "paddleboard", "paddleboat"}});
    m_mappings.push_back({"BOATS", "MOTORBOAT",
        {"boats", "motorboat", "bass", "boat", "bowrider", "cabin", "cruiser", "duck", "fishing", "personal", "pontoon", "powerboat", "runabout", "ski", "small", "speedboat"}});
    m_mappings.push_back({"BOATS", "RACING",
        {"boats", "racing", "boat", "cigarette", "drag", "hydrofoil", "hydroplane", "jet", "powerboat", "regatta", "ski"}});
    m_mappings.push_back({"BOATS", "ROWBOAT",
        {"boats", "canoe", "canoes", "cutter", "dinghy", "dory", "gondola", "inflatable", "jon", "kayak", "lifeboat", "longboat", "oar", "outrigger", "paddle", "punt", "raft", "row", "rowboat", "scow", "scull", "skiff", "wherry"}});
    m_mappings.push_back({"BOATS", "SAILBOAT",
        {"boats", "barkentine", "barque", "barquentine", "brig", "brigantine", "catamaran", "centerboard", "clipper", "cutter", "dhow", "dinghy", "junk", "keelboat", "ketch", "longboat", "monohull", "pinnace", "proa", "regatta", "sailboat", "schooner", "shallop", "sloop", "trimaran", "yacht", "yawl"}});
    m_mappings.push_back({"BOATS", "SHIP",
        {"boats", "cargo", "container", "cruise", "ferry", "freighter", "icebreaker", "large", "oil", "passenger", "pirate", "research", "ship", "tanker", "tugboat", "vessel"}});
    m_mappings.push_back({"BOATS", "STEAM",
        {"boats", "steam", "boiler", "engine", "paddlewheel", "riverboat", "ship", "steamboat", "steamer", "steamship", "sternwheeler", "titanic"}});
    m_mappings.push_back({"BOATS", "SUBMARINE",
        {"boats", "auv", "deep-sea", "nuclear-powered", "research", "rov", "sonar", "sub", "submarine", "submersible", "subs", "torpedo", "torpedoing", "u-boat"}});
    m_mappings.push_back({"BOATS", "UNDERWATER", {"boats", "underwater", "recorded", "submerged", "undersea"}});

    // BULLETS Category
    m_mappings.push_back({"BULLETS", "BY",
        {"bullets", "by", "bullet", "fwip", "graze", "subsonic", "supersonic", "whiz-by", "whizby"}});
    m_mappings.push_back({"BULLETS", "IMPACT",
        {"bullets", "armor", "ballistic", "body", "bullet", "bulletproof", "gunshot", "headshot", "hit", "impact", "killshot", "penetration", "pierce", "shot", "strike", "struck", "vest", "wounded"}});
    m_mappings.push_back({"BULLETS", "MISC",
        {"bullets", "misc", "air", "ammunitions", "armor", "bandoliers", "bb", "birdshot", "blanks", "buckshot", "bullet", "cartridges", "dummy", "frangible", "incendiary", "miscellaneous", "paintball", "pellet", "piercing", "reloads", "rifle", "rounds", "rubber", "slug", "subsonic", "supersonic", "tracer"}});
    m_mappings.push_back({"BULLETS", "RICOCHET", {"bullets", "deflect", "ricochet", "ricos", "whizzing"}});
    m_mappings.push_back({"BULLETS", "SHELL",
        {"bullets", "ammo", "ammunition", "cartridge", "casing", "eject", "housing", "shell"}});

    // CARTOON Category
    m_mappings.push_back({"CARTOON", "ANIMAL",
        {"cartoon", "animal", "animated", "call", "cartoony", "comic", "duck", "fake", "hanna", "hunting", "silly", "toon"}});
    m_mappings.push_back({"CARTOON", "BOING",
        {"cartoon", "boing", "animated", "cartoony", "comic", "hanna", "harp", "jaw", "mouth", "pogo", "silly", "spring", "stick", "toon"}});
    m_mappings.push_back({"CARTOON", "CLANG",
        {"cartoon", "clang", "animated", "anvil", "bong", "cartoony", "clink", "comic", "hanna", "metallic", "silly", "toon"}});
    m_mappings.push_back({"CARTOON", "CREAK",
        {"cartoon", "creak", "animated", "cartoony", "comic", "hanna", "screech", "silly", "toon", "wood"}});
    m_mappings.push_back({"CARTOON", "HORN",
        {"cartoon", "horn", "animated", "bugle", "bulb", "cartoony", "clown", "comic", "hanna", "honk", "silly", "toon", "trumpet"}});
    m_mappings.push_back({"CARTOON", "IMPACT",
        {"cartoon", "impact", "animated", "bang", "biff", "boff", "boink", "bonk", "cartoony", "clobber", "comic", "doink", "hanna", "hit", "punch", "shtoink", "silly", "smack", "strike", "thunk", "toon", "wham", "zonk"}});
    m_mappings.push_back({"CARTOON", "MACHINE",
        {"cartoon", "acme", "animated", "apparatus", "cartoony", "comic", "comical", "contraption", "device", "gadget", "gizmo", "goldberg", "hanna", "machine", "rube", "silly", "toon", "trap"}});
    m_mappings.push_back({"CARTOON", "MISC",
        {"cartoon", "misc", "animated", "blorb", "cartoony", "comic", "flump", "hanna", "silly", "squanch", "toon"}});
    m_mappings.push_back({"CARTOON", "MUSICAL",
        {"cartoon", "animated", "ascend", "cartoony", "comic", "descend", "gliss", "hanna", "melodic", "musical", "silly", "toon"}});
    m_mappings.push_back({"CARTOON", "PLUCK",
        {"cartoon", "animated", "cartoony", "comic", "hanna", "plink", "pluck", "silly", "string", "toon"}});
    m_mappings.push_back({"CARTOON", "POP",
        {"cartoon", "animated", "bubble", "bubblegum", "cartoony", "comic", "cup", "gun", "hanna", "mouth", "pop", "pops", "silly", "suction", "toon"}});
    m_mappings.push_back({"CARTOON", "SHAKE",
        {"cartoon", "shake", "animated", "cartoony", "comic", "hanna", "rattle", "silly", "toon", "tremble"}});
    m_mappings.push_back({"CARTOON", "SPLAT",
        {"cartoon", "animated", "cartoony", "comic", "hanna", "silly", "splat", "splort", "squelch", "squish", "toon"}});
    m_mappings.push_back({"CARTOON", "SQUEAK",
        {"cartoon", "animated", "cartoony", "comic", "hanna", "rubber", "silly", "squeak", "toon"}});
    m_mappings.push_back({"CARTOON", "STRETCH",
        {"cartoon", "stretch", "animated", "cartoony", "comic", "elongate", "extend", "hanna", "lengthen", "pull", "silly", "strain", "toon", "yank"}});
    m_mappings.push_back({"CARTOON", "SWISH",
        {"cartoon", "swish", "animated", "cartoony", "comic", "hanna", "silly", "swirl", "swoosh", "toon", "twirl", "whoosh"}});
    m_mappings.push_back({"CARTOON", "TWANG",
        {"cartoon", "twang", "animated", "band", "cartoony", "comic", "hanna", "harp", "jaw", "pluck", "rubber", "ruler", "silly", "string", "toon", "twanging"}});
    m_mappings.push_back({"CARTOON", "VEHICLE",
        {"cartoon", "vehicle", "animated", "backfire", "cartoony", "comic", "contraption.", "flintstones", "hanna", "jetsons", "silly", "toon"}});
    m_mappings.push_back({"CARTOON", "VOCAL",
        {"cartoon", "vocal", "animated", "cartoony", "comic", "grumble", "hanna", "mumble", "mutter", "ramble", "silly", "toon", "voice"}});
    m_mappings.push_back({"CARTOON", "WARBLE",
        {"cartoon", "warble", "animated", "cartoony", "comic", "hanna", "quaver", "silly", "toon", "vibrato", "warbling", "wobble"}});
    m_mappings.push_back({"CARTOON", "WHISTLE",
        {"cartoon", "animated", "blow", "cartoony", "comic", "hanna", "silly", "slide", "toon", "toot", "whistle", "wolf"}});
    m_mappings.push_back({"CARTOON", "ZIP",
        {"cartoon", "animated", "cartoony", "comic", "dart", "flash", "fly", "hanna", "rico", "silly", "toon", "whizz", "zing", "zip", "zippy", "zoom"}});

    // CERAMICS Category
    m_mappings.push_back({"CERAMICS", "BREAK",
        {"ceramics", "apart", "break", "burst", "ceramic", "china", "chip", "clay", "crack", "crockery", "crumble", "crunch", "crush", "demolish", "destroy", "disintegrate", "earthenware", "fracture", "fragment", "porcelain", "pottery", "shatter", "smash", "snap", "splinter", "split", "stoneware", "terracotta", "tile", "ware"}});
    m_mappings.push_back({"CERAMICS", "CRASH & DEBRIS",
        {"ceramics", "crash & debris", "ceramic", "china", "clay", "collision", "crockery", "debris", "earthenware", "fall", "fragments", "porcelain", "pottery", "remains", "rubble", "ruins", "scatter", "shards", "smash", "stoneware", "terracotta", "tile", "trash", "ware", "wreckage"}});
    m_mappings.push_back({"CERAMICS", "FRICTION",
        {"ceramics", "friction", "abrasion", "attrition", "ceramic", "china", "clay", "creak", "crockery", "earthenware", "erosion", "grating", "grinding", "porcelain", "pottery", "rasping", "rubbing", "scouring", "scrape", "scraping", "scratching", "screech", "scuffing", "sliding", "squeak", "stoneware", "stress", "terracotta", "tile", "ware", "wear"}});
    m_mappings.push_back({"CERAMICS", "HANDLE",
        {"ceramics", "caress", "catch", "ceramic", "china", "clasp", "clay", "clench", "clutch", "crockery", "down", "earthenware", "embrace", "fondle", "grab", "grasp", "grip", "handle", "hold", "manipulate", "operate", "palm", "pickup", "porcelain", "pottery", "seize", "set", "stoneware", "take", "terracotta", "tile", "toss", "touch", "use", "ware"}});
    m_mappings.push_back({"CERAMICS", "IMPACT",
        {"ceramics", "bang", "banging", "bash", "bump", "ceramic", "china", "clap", "clay", "clink", "clunk", "collide", "colliding", "collision", "crash", "crashing", "crockery", "drop", "earthenware", "hit", "hitting", "impact", "impacting", "jolt", "knock", "porcelain", "pottery", "pound", "ram", "shock", "slam", "slamming", "smack", "smacking", "stoneware", "strike", "striking", "terracotta", "thrust", "thump", "tile", "ware"}});
    m_mappings.push_back({"CERAMICS", "MISC",
        {"ceramics", "misc", "ceramic", "china", "clay", "crockery", "earthenware", "miscellaneous", "porcelain", "pottery", "stoneware", "terracotta", "tile", "ware"}});
    m_mappings.push_back({"CERAMICS", "MOVEMENT",
        {"ceramics", "movement", "ceramic", "china", "clatter", "clay", "crockery", "drag", "earthenware", "jiggle", "jingle", "move", "porcelain", "pottery", "rattle", "roll", "shake", "shift", "stoneware", "terracotta", "tile", "vibrate", "ware", "wobble"}});
    m_mappings.push_back({"CERAMICS", "TONAL",
        {"ceramics", "bowed", "ceramic", "china", "clay", "crockery", "earthenware", "frequency", "harmonic", "melodic", "melodious", "musical", "ping", "pitch", "porcelain", "pottery", "resonance", "resonant", "ring", "shing", "sonorous", "sound", "stoneware", "terracotta", "tile", "timbre", "tonal", "tone", "ware"}});

    // CHAINS Category
    m_mappings.push_back({"CHAINS", "BREAK",
        {"chains", "bend", "break", "burst", "crack", "fracture", "links", "rupture", "sever", "shatter", "snap", "splinter", "split"}});
    m_mappings.push_back({"CHAINS", "HANDLE",
        {"chains", "catch", "clench", "down", "grab", "grasp", "grip", "handle", "hold", "manipulate", "operate", "palm", "pickup", "pulling", "seize", "set", "take", "throw", "toss", "use"}});
    m_mappings.push_back({"CHAINS", "IMPACT",
        {"chains", "bang", "banging", "colliding", "crash", "crashing", "drop", "hit", "hitting", "impact", "impacting", "pound", "ram", "slam", "slamming", "smack", "smacking", "strike", "striking", "thrust"}});
    m_mappings.push_back({"CHAINS", "MISC",
        {"chains", "misc", "bonds", "links", "manacles", "miscellaneous", "restraints", "shackles"}});
    m_mappings.push_back({"CHAINS", "MOVEMENT",
        {"chains", "movement", "bind", "bound", "clank", "clatter", "drag", "jingle", "shackle", "shackled", "shake", "vibrate"}});

    // CHEMICALS Category
    m_mappings.push_back({"CHEMICALS", "ACID",
        {"chemicals", "acid", "acetic", "acidic", "acrid", "biting", "caustic", "citric", "corrosive", "erosive", "fizz", "hydrochloric", "melt", "sizzle", "sour", "sulfuric", "toxic"}});
    m_mappings.push_back({"CHEMICALS", "MISC",
        {"chemicals", "misc", "agents", "atoms", "chemical", "compound", "compounds", "formulas", "matter", "miscellaneous", "mixtures", "molecules", "reagent", "solutions", "substance", "substances", "toxic"}});
    m_mappings.push_back({"CHEMICALS", "REACTION",
        {"chemicals", "activate", "bubbling", "catalysis", "catalyst", "catalyze", "chemically", "chemistry", "electrolysis", "endothermic", "enzyme", "exothermic", "fermentation", "foaming", "hydrolysis", "inactive", "inert", "react", "reaction", "reactive", "reagent", "transformation"}});

    // CLOCKS Category
    m_mappings.push_back({"CLOCKS", "CHIME",
        {"clocks", "bell", "chime", "clock", "dong", "grandfather", "peal", "ring", "sound", "strike", "striker", "toll", "tolling"}});
    m_mappings.push_back({"CLOCKS", "MECHANICS",
        {"clocks", "mechanics", "apparatus", "clocklike", "clockwork", "clockworks", "cogwheel", "cuckoo", "device", "escapement", "gears", "innards", "insides", "machinery", "mainspring", "mechanisms", "movements", "pendulum", "springs", "ticktock", "timepiece", "watch", "wheels", "winder", "winding", "workings"}});
    m_mappings.push_back({"CLOCKS", "MISC",
        {"clocks", "misc", "chronometers", "miscellaneous", "timekeepers", "timepieces", "timers", "watches"}});
    m_mappings.push_back({"CLOCKS", "TICK",
        {"clocks", "chronometer", "click", "clicking", "clock", "egg", "face", "hands", "numerals", "pendulum", "quartz", "sounds", "stop", "stopwatch", "tick", "tick-tock", "ticking", "time", "timekeeping", "timer", "tock", "tocking", "watch"}});

    // CLOTH Category
    m_mappings.push_back({"CLOTH", "FLAP",
        {"cloth", "apparel", "apron", "banner", "bib", "blanket", "burlap", "canvas", "cape", "clothe", "cotton", "dress", "fabric", "fabrics", "flag", "flannel", "flap", "flapping", "flicker", "flutter", "fluttering", "garment", "gown", "jacket", "jeans", "khaki", "lace", "laundry", "linen", "material", "microfiber", "muslin", "nylon", "oilcloth", "oilskin", "pants", "parachute", "pillowcase", "polyester", "rag", "rayon", "robe", "rustle", "rustling", "sail", "sailcloth", "sheet", "shirt", "silk", "skirt"}});
    m_mappings.push_back({"CLOTH", "HANDLE",
        {"cloth", "apparel", "apron", "banner", "bib", "blanket", "burlap", "canvas", "catch", "clasp", "clench", "clothe", "clutch", "cotton", "down", "dress", "embrace", "fabric", "fabrics", "flag", "flannel", "garment", "gown", "grab", "grasp", "grip", "handle", "hold", "jacket", "jeans", "khaki", "lace", "laundry", "linen", "material", "microfiber", "muslin", "nylon", "oilcloth", "oilskin", "pants", "pickup", "pillowcase", "polyester", "rag", "rayon", "robe", "seize", "set", "sheet"}});
    m_mappings.push_back({"CLOTH", "IMPACT",
        {"cloth", "apparel", "apron", "banner", "bib", "blanket", "bump", "burlap", "canvas", "clothe", "cotton", "dress", "drop", "fabric", "fabrics", "flag", "flannel", "garment", "gown", "hit", "hitting", "impact", "impacting", "jacket", "jeans", "khaki", "lace", "laundry", "linen", "material", "microfiber", "muslin", "nylon", "oilcloth", "oilskin", "pants", "pillowcase", "polyester", "rag", "rayon", "robe", "sheet", "shirt", "silk", "skirt", "smack", "smacking", "strike", "tablecloth", "terry"}});
    m_mappings.push_back({"CLOTH", "MISC",
        {"cloth", "misc", "apparel", "apron", "banner", "bib", "blanket", "burlap", "canvas", "chambray", "clothe", "corduroy", "cotton", "denim", "dress", "fabric", "fabrics", "flag", "flannel", "garment", "gown", "jacket", "jeans", "khaki", "lace", "laundry", "linen", "material", "microfiber", "miscellaneous", "muslin", "nylon", "oilcloth", "oilskin", "pants", "pillowcase", "polyester", "rag", "rayon", "robe", "sheet", "shirt", "silk", "skirt", "tablecloth", "terry", "textile", "towel", "twill", "velvet"}});
    m_mappings.push_back({"CLOTH", "MOVEMENT",
        {"cloth", "adjusting", "apparel", "apron", "arranging", "banner", "bib", "billow", "billowing", "blanket", "bunching", "burlap", "canvas", "clothe", "cotton", "crumpling", "drape", "draping", "dress", "fabric", "fabrics", "flag", "flannel", "flapping", "flow", "flutter", "fluttering", "folding", "garment", "gathering", "gown", "hang", "jacket", "jeans", "khaki", "lace", "laundry", "linen", "material", "microfiber", "movement", "muslin", "nylon", "oilcloth", "oilskin", "pants", "pillowcase", "pleating", "polyester", "positioning"}});
    m_mappings.push_back({"CLOTH", "RIP",
        {"cloth", "apart", "apparel", "apron", "banner", "bib", "blanket", "breach", "burlap", "canvas", "clothe", "cotton", "cut", "dissect", "dress", "fabric", "fabrics", "flag", "flannel", "garment", "gash", "gown", "incise", "jacket", "jeans", "khaki", "lace", "lacerate", "laundry", "linen", "material", "microfiber", "muslin", "nylon", "oilcloth", "oilskin", "pants", "perforate", "pillowcase", "polyester", "puncture", "rag", "rayon", "rend", "rip", "ripped", "ripper", "ripping", "rive", "robe"}});

    // COMMUNICATIONS Category
    m_mappings.push_back({"COMMUNICATIONS", "AUDIO VISUAL",
        {"communications", "audio visual", "16mm", "8mm", "amplification", "amplifier", "audio", "audiovisual", "aural", "av", "betamax", "bias", "bolex", "cables", "camcorder", "camera", "cassette", "cd", "cinematography", "conferencing", "device", "digital", "dollies", "film", "filming", "footage", "hypermedia", "ipod", "laserdisc", "media", "mixer", "monitoring", "motion", "multimedia", "nagra", "picture", "playback", "player", "presentation", "projector", "radiotelephone", "receiver", "recorder", "reel", "render", "slide", "sound", "spooling", "streaming", "system"}});
    m_mappings.push_back({"COMMUNICATIONS", "CAMERA",
        {"communications", "120mm", "35mm", "action", "autofocus", "body", "bulb", "cam", "camara", "camera", "cameraman", "cameramen", "cameraperson", "camra", "canon", "cctv", "digicam", "digital", "dslr", "film", "flash", "format", "fuji", "fujifilm", "hasselblad", "image", "imaging", "instant", "kodak", "leica", "lens", "lense", "mamiya", "medium", "minicam", "nikon", "olympus", "photo", "photog", "photograph", "photographer", "photographic", "photographing", "photography", "picture", "pictures", "point-and-shoot", "polaroid", "sensor", "shutter"}});
    m_mappings.push_back({"COMMUNICATIONS", "CELLPHONE",
        {"communications", "cellphone", "android", "blackberry", "calling", "cameraphone", "carphone", "cell", "cellular", "femtocell", "flip", "galaxy", "handsfree", "iphone", "mobile", "motorola", "nokia", "phone", "pocketphone", "portability", "roaming", "screenphone", "sim", "smartphone", "wireless", "wristphone"}});
    m_mappings.push_back({"COMMUNICATIONS", "MICROPHONE",
        {"communications", "cardioids", "dictaphone", "electret", "feedback", "handling", "headsets", "hydrophone", "hypercardioid", "lapel", "larsen", "lavalier", "lavaliere", "lectern", "megaphone", "mic", "microphone", "mics", "mike", "mikes", "podium", "preamplifier", "tap", "transducer"}});
    m_mappings.push_back({"COMMUNICATIONS", "MISC",
        {"communications", "misc", "comm", "communicator", "fax", "miscellaneous", "newsletter", "reporting", "telecom", "telecommunication", "telecommunications", "telecoms", "telematics", "transmittal", "transmitted"}});
    m_mappings.push_back({"COMMUNICATIONS", "PHONOGRAPH",
        {"communications", "phonograph", "33", "45", "78", "audion", "changer", "cinematograph", "edison", "ep", "gramophone", "gramophones", "graphogram", "graphophone", "jukebox", "jukeboxes", "kinetoscope", "lp", "orchestrion", "phonautograph", "phono", "phonogram", "phonographic", "phonography", "phonorecord", "photophone", "pianola", "player", "portable", "radiogram", "record", "stereophonic", "stereopticon", "stylus", "turntable", "turntables", "victrola", "vinyl"}});
    m_mappings.push_back({"COMMUNICATIONS", "RADIO",
        {"communications", "radio", "airing", "airwaves", "am", "antenna", "bbc", "boombox", "broadcast", "broadcaster", "broadcasting", "car", "clock", "fm", "futz", "hd", "longwave", "newscasting", "portable", "radar", "radial", "radiocast", "radiocommunication", "radiofrequency", "radiophonic", "radiotelegraph", "radiotelegraphy", "radiotelephone", "receiver", "receiving", "shortwave", "side-band", "signal", "siriusxm", "squelch", "station", "tabletop", "transmission", "tuner", "tuning", "waves", "weather", "wireless"}});
    m_mappings.push_back({"COMMUNICATIONS", "STATIC",
        {"communications", "static", "aliasing", "brown", "buzz", "crackle", "crunchy", "electrostatic", "fuzz", "hiss", "hum", "interference", "noise", "noisy", "pink", "radio", "scramble", "squelched", "tv", "white"}});
    m_mappings.push_back({"COMMUNICATIONS", "TELEMETRY",
        {"communications", "telemetry", "analog", "analytics", "code", "comms", "data", "datalink", "datalogger", "dataloggers", "digital", "downlink", "downlinks", "fsk", "gps", "modem", "monitoring", "morse", "ppm", "psk", "ranging", "readout", "readouts", "satellites", "sensors", "sos", "spectrum", "telegraph", "tracker", "trackers", "tracking", "transmission", "uplinking"}});
    m_mappings.push_back({"COMMUNICATIONS", "TELEPHONE",
        {"communications", "telephone", "answerphone", "autodial", "calling", "cord", "cordless", "dial", "dialer", "dialing", "directory", "handset", "helpdesk", "helpline", "hotline", "hotlines", "interphone", "key", "landline", "number", "numero", "pad", "payphone", "phone", "phonecall", "phoned", "phoneline", "phoning", "push-button", "radiotelephone", "radiotelephony", "receiver", "rotary", "speakerphone", "switchboard", "switchboards", "tel", "tele", "telecom", "telecommunication", "telecommunications", "teleconference", "teleconferencing", "telefonica", "telemarketing", "telephonic", "telephonically", "telephonist", "telephonists", "telephony"}});
    m_mappings.push_back({"COMMUNICATIONS", "TELEVISION",
        {"communications", "4k", "advertising", "antenna", "broadcast", "broadcaster", "channel", "commercials", "crt", "curved", "dial", "documentaries", "drama", "emissions", "flatscreen", "futz", "hdr", "lcd", "led", "movies", "news", "newscaster", "oled", "opera", "plasma", "program", "programming", "qled", "remote", "show", "sitcom", "sitcoms", "smart", "soap", "sports", "talk", "teevee", "teleshopping", "televised", "television", "telly", "tube", "tv", "ufh", "vhf", "viewing"}});
    m_mappings.push_back({"COMMUNICATIONS", "TRANSCEIVER",
        {"communications", "transceiver", "allonge", "antenna", "array", "bleep", "broadcast", "cb", "collect", "collecting", "compander", "converter", "cradle", "cross", "directional", "dispatch", "downlink", "ear", "frequency", "ham", "heterodyne", "intercom", "keying", "lead", "modulation", "multipath", "omnidirectional", "racon", "radar", "radio", "radiogram", "radioman", "radios", "radiosonde", "receive", "received", "receiver", "receiving", "rig", "satellite", "signal", "squelch", "talkies", "transmitter", "transponder", "two-way", "walkie", "wavelength", "wireless"}});
    m_mappings.push_back({"COMMUNICATIONS", "TYPEWRITER",
        {"communications", "braillewriter", "brother", "carriage", "corona", "daisywheel", "electric", "keyboard", "knob", "manual", "mimeographs", "olivetti", "olympia", "platen", "platten", "portable", "return", "ribbon", "royal", "smith-corona", "stenograph", "stenographer", "tabulator", "teleprinter", "teletype", "teletypes", "teletypewriters", "treadle", "typeball", "typebar", "typebars", "typeface", "typehead", "typewrite", "typewriter", "typewritten", "typist", "typists", "typograph", "typographer", "underwood"}});

    // COMPUTERS Category
    m_mappings.push_back({"COMPUTERS", "HARD DRIVE",
        {"computers", "hard drive", "boot", "data", "disc", "disk", "firewire", "floppy", "gigabytes", "grind", "hard", "hdd", "ide", "magnetic", "megabytes", "nas", "raid", "sata", "scsi", "search", "ssd", "storage", "terabytes", "thunderbolt"}});
    m_mappings.push_back({"COMPUTERS", "KEYBOARD & MOUSE",
        {"computers", "keyboard & mouse", "bluetooth", "clicking", "cursor", "device", "ergonomic", "input", "keyboard", "keypad", "keys", "keystroke", "magic", "mechanical", "mice", "mouse", "numeric", "optical", "qwerty", "touchpad", "trackball", "trackpad", "typing", "wired", "wireless"}});
    m_mappings.push_back({"COMPUTERS", "MISC",
        {"computers", "misc", "calculation", "computing", "cpu", "cybercrime", "cyberspace", "desktop", "desktops", "gadget", "laptop", "mainframe", "palm", "pc", "peripheral", "pilot", "pocket", "software", "supercomputer", "tablet", "tech"}});

    // CREATURES Category
    m_mappings.push_back({"CREATURES", "AQUATIC",
        {"creatures", "aquatic", "creature", "cthulhu", "giant", "hydra", "kelpie", "kraken", "leviathan", "loch", "marine", "mermaid", "monster", "naga", "ness", "nessie", "oceanic", "sea", "serpent", "siren", "squid"}});
    m_mappings.push_back({"CREATURES", "AVIAN",
        {"creatures", "avian", "alien", "beaked", "birds", "extraterrestrial", "fantasy", "feathered", "flying", "garuda", "griffin", "harpy", "hippogriff", "monster", "phoenix", "roc", "simurgh", "thunderbird", "winged"}});
    m_mappings.push_back({"CREATURES", "BEAST",
        {"creatures", "abomination", "beast", "cerberus", "chupacabra", "creature", "cryptid", "mammoth", "minotaur", "monster", "mumakil", "quadruped", "sasquatch", "unicorn", "wooly", "yeti"}});
    m_mappings.push_back({"CREATURES", "BLOB",
        {"creatures", "amorphous", "blob", "blobby", "formless", "gelatinous", "gloop", "gooey", "goop", "jelly-like", "mass", "monster", "ooze", "shapeless", "slime", "slimy", "viscous"}});
    m_mappings.push_back({"CREATURES", "DINOSAUR",
        {"creatures", "dinosaur", "allosaur", "allosaurus", "ankylosaurus", "anteosaur", "apatosaurus", "archaeopteryx", "archosaur", "argentinosaur", "beastie", "brachiosaurus", "brontosaur", "brontosaurs", "brontosaurus", "carnosaur", "cerapodan", "ceratopsian", "deinosaur", "dino", "dinocarid", "dinoceratan", "dinos", "dinosauriform", "dinosauromorph", "dinosaurus", "dinotherium", "diplodocus", "duckbilled", "exhibition", "fossil", "fossilised", "fossilized", "fossils", "hadrosaur", "hadrosaurs", "hung", "ichthyosaur", "ichthyosaurus", "iguanodon", "jurassic", "mesosaur", "mesosaurid", "mesozoic", "mosasaur", "non", "nothosaur", "ornithischian", "ornithopod", "oviraptor"}});
    m_mappings.push_back({"CREATURES", "DRAGON",
        {"creatures", "chimera", "draco", "drago", "dragon", "dragoness", "dragonlike", "dragonslayer", "drake", "firedrake", "hydra", "naga", "serpent", "wyrm", "wyrmling", "wyvern", "wyverns"}});
    m_mappings.push_back({"CREATURES", "ELEMENTAL",
        {"creatures", "elemental", "air", "alchemic", "alchemical", "arcane", "crystal", "earth", "fire", "giant", "golem", "ice", "monster", "primal", "primeval", "rock", "shadow", "snow", "water", "wood"}});
    m_mappings.push_back({"CREATURES", "ETHEREAL",
        {"creatures", "aethereal", "angel", "apparition", "apparitional", "astral", "banshee", "being", "corporeal", "ephemeral", "ethereal", "floaty", "ghost", "ghostlike", "ghosts", "gossamer", "otherworldly", "phantasmal", "phantasmic", "phantom", "poltergeist", "soul", "specter", "spectral", "spirit", "spirits", "vaporous", "vapory", "wisp", "wispy", "wraith"}});
    m_mappings.push_back({"CREATURES", "HUMANOID",
        {"creatures", "humanoid", "bigfoot", "dead", "elf", "frankenstein", "ghoul", "living", "mummy", "ogre", "orcs", "swamp", "tengu", "thing", "troll", "undead", "vampire", "werewolf", "witch", "zombie"}});
    m_mappings.push_back({"CREATURES", "INSECTOID",
        {"creatures", "insectoid", "ant", "arachnid", "giant", "spider", "wasp"}});
    m_mappings.push_back({"CREATURES", "MISC",
        {"creatures", "misc", "cryptids", "folklore", "miscellaneous", "mythical", "mythological"}});
    m_mappings.push_back({"CREATURES", "MONSTER",
        {"creatures", "monster", "behemoth", "chupacabra", "godzilla", "king", "kong", "rancor", "xenomorph", "yeti"}});
    m_mappings.push_back({"CREATURES", "REPTILIAN",
        {"creatures", "reptilian", "basilisk", "giant", "gorgon", "lizard", "medusa", "serpent", "serpentine", "snake"}});
    m_mappings.push_back({"CREATURES", "SMALL",
        {"creatures", "small", "brownie", "cupid", "elf", "elves", "fairy", "fraggle", "gnome", "gremlin", "hobgoblin", "imp", "leprechaun", "nymph", "pixie", "sprite"}});
    m_mappings.push_back({"CREATURES", "SOURCE",
        {"creatures", "source", "barks", "bellows", "breath", "call", "caw", "chatters", "chitter", "click", "cries", "cry", "effort", "groans", "growls", "grunt", "grunts", "gurgles", "guttural", "hiss", "hisses", "howls", "moans", "mouth", "roars", "scream", "screams", "screech", "screeches", "shrieks", "snarls", "squawk", "squeal", "wails", "whine", "whispers", "yowls"}});

    // CROWDS Category
    m_mappings.push_back({"CROWDS", "ANGRY",
        {"crowds", "angry", "agitated", "audience", "congregation", "demonstrators", "enraged", "furious", "gathering", "hooligan", "horde", "hostile", "incensed", "infuriated", "looters", "mad", "mob", "multitude", "outraged", "protest", "protesters", "riot", "rioters", "rowdy", "shout", "throng", "violent", "wrathful"}});
    m_mappings.push_back({"CROWDS", "APPLAUSE",
        {"crowds", "acclaimed", "adulation", "applaud", "applauded", "applauding", "applause", "appreciated", "appreciating", "appreciative", "audience", "cheer", "cheering", "cheers", "clap", "clapping", "congratulation", "congregation", "encore", "enthusiastic", "gathering", "golf", "grandstand", "grandstander", "grandstanding", "hand", "hand-clapping", "handclap", "horde", "multitude", "ovation", "praise", "slow", "throng"}});
    m_mappings.push_back({"CROWDS", "BATTLE",
        {"crowds", "army", "battalion", "battle", "chants", "charge", "charging", "combat", "conflict", "cries", "dying", "fighting", "horde", "infantry", "massacre", "melee", "multitude", "rally", "screaming", "scuffle", "shootout", "shout", "shouts", "showdown", "throng", "troop", "trooping", "troops", "war", "yells"}});
    m_mappings.push_back({"CROWDS", "CELEBRATION",
        {"crowds", "celebration", "audience", "awards", "birthday", "celebrate", "celebrating", "celebratory", "christmas", "election", "excited", "festive", "holiday", "jubilant", "new", "parade", "party", "revel", "revelers", "rowdy", "victory", "years"}});
    m_mappings.push_back({"CROWDS", "CHEERING",
        {"crowds", "audience", "awards", "celebratory", "chants", "cheerful", "cheering", "concert", "elated", "euphoric", "excited", "exultant", "graduation", "happy", "joyful", "jubilant", "parade", "political", "rally", "rejoicing", "roaring", "rousing", "sports", "stadium", "swells", "victory", "whooping"}});
    m_mappings.push_back({"CROWDS", "CHILDREN",
        {"crowds", "babies", "children", "daycare", "elementary", "highschool", "infants", "juveniles", "kids", "kindergarten", "little", "minors", "ones", "park", "play", "playground", "playgroup", "playing", "recess", "school", "schoolyard", "tag", "toddlers", "tots", "youngsters", "youths"}});
    m_mappings.push_back({"CROWDS", "CONVERSATION",
        {"crowds", "casual", "chat", "chatter", "chitchat", "colloquy", "communication", "confabulation", "conversation", "conversations", "converse", "dialogue", "discussion", "exchange", "informal", "parley", "small", "socializing", "sparse", "talk", "verbalization", "walla"}});
    m_mappings.push_back({"CROWDS", "LAUGHTER",
        {"crowds", "laughter", "amusement", "cackles", "canned", "chortles", "chuckles", "club", "comedy", "funny", "giggles", "guffaws", "ha-ha", "hilarity", "jest", "joke", "laugh", "mirth", "snickers", "track", "uproar"}});
    m_mappings.push_back({"CROWDS", "LOOP GROUP",
        {"crowds", "loop group", "actor", "actors", "adr", "canned", "cast", "conversations", "dubbing", "extras", "group", "indistinct", "loop", "performers", "walla"}});
    m_mappings.push_back({"CROWDS", "MISC", {"crowds", "misc", "miscellaneous"}});
    m_mappings.push_back({"CROWDS", "PANIC",
        {"crowds", "aggressive", "anxiety", "chaos", "cry", "crying", "desperate", "disaster", "disorder", "disorders", "distress", "emergencies", "fear", "fearful", "fleeing", "frantic", "hysteria", "hysterical", "mayhem", "pandemonium", "panic", "panicked", "pleas", "pushing", "rioting", "rout", "scrambling", "scream", "screaming", "shout", "shouting", "terror", "terrorists", "trapped", "victim", "yelling"}});
    m_mappings.push_back({"CROWDS", "QUIET",
        {"crowds", "quiet", "calm", "church", "conversations", "courtroom", "gallery", "hushed", "hushing", "library", "low", "mumbling", "murmur", "murmurs", "museum", "muted", "noiseless", "peaceful", "serene", "silent", "soft", "soundless", "still", "subdued", "talking", "tranquil", "voices", "whispering"}});
    m_mappings.push_back({"CROWDS", "REACTION",
        {"crowds", "aahs", "ahh", "applause", "boo", "booing", "boos", "chanting", "cheers", "excited", "gasps", "hollers", "hoots", "laughter", "murmurs", "ooh", "oohs", "reaction", "response", "shocked", "sighs", "studio", "whistling", "woohoo"}});
    m_mappings.push_back({"CROWDS", "SINGING",
        {"crowds", "singing", "acapella", "barbershop", "carol", "chamber", "chant", "chanted", "chanting", "choir", "chorale", "chorus", "club", "ensemble", "glee", "gospel", "harmony", "quartet", "recital", "vocal", "vocals"}});
    m_mappings.push_back({"CROWDS", "SPORT",
        {"crowds", "sport", "anthems", "applause", "athletics", "audience", "baseball", "basketball", "blowing", "booing", "chanting", "cheer", "cheering", "clapping", "contest", "drumming", "event", "events", "fans", "football", "games", "goal", "hockey", "horn", "insults", "match", "matches", "meets", "olympics", "race", "score", "shouting", "soccer", "spectator", "sporting", "stadium", "stomping", "taunts", "tournament", "victory", "whistling"}});
    m_mappings.push_back({"CROWDS", "WALLA",
        {"crowds", "background", "clamor", "clatter", "commotion", "conversation", "conversations", "din", "group", "hubbub", "indistinct", "murmur", "talk", "voices", "walla"}});

    // DESIGNED Category
    m_mappings.push_back({"DESIGNED", "BASS DIVE",
        {"designed", "bass dive", "bass", "descending", "dive", "downer", "drop", "fall", "low-frequency", "pitch", "rumble", "sub-bass", "subsonic", "trailer", "vibration"}});
    m_mappings.push_back({"DESIGNED", "BOOM",
        {"designed", "bang", "blast", "boom", "bump", "burst", "clap", "crash", "deep", "hit", "hypersonic", "impact", "low", "shockwave", "sting", "supersonic", "thump", "thunderous", "trailer"}});
    m_mappings.push_back({"DESIGNED", "BRAAM", {"designed", "braam", "bramm"}});
    m_mappings.push_back({"DESIGNED", "DISTORTION",
        {"designed", "buzz", "crackle", "distorted", "distortion", "feedback", "fuzz", "hiss", "hum", "noise", "off", "overdrive", "squared", "static"}});
    m_mappings.push_back({"DESIGNED", "DRONE",
        {"designed", "ambient", "drone", "droning", "hum", "humming", "monotonous", "ominous", "pad", "pulsing", "resonant", "sustained", "tension", "texture", "throb", "throbbing", "whirring"}});
    m_mappings.push_back({"DESIGNED", "EERIE",
        {"designed", "apprehension", "bleak", "chilling", "creepy", "disturbing", "eerie", "freaky", "frightful", "ghostly", "grim", "haunting", "horror", "mysterious", "ominous", "otherworldly", "queer", "scary", "shadowed", "sinister", "spooky", "strange", "supernatural", "tense", "tension", "uncanny", "unnatural", "unsettling", "weird"}});
    m_mappings.push_back({"DESIGNED", "ETHEREAL",
        {"designed", "aethereal", "airy", "angelic", "celestial", "divine", "dreamy", "ethereal", "ghostly", "gossamer", "heavenly", "immaterial", "otherworldly", "spectral", "spirit", "surreal", "transcendent", "unearthly", "vaporous", "wispy"}});
    m_mappings.push_back({"DESIGNED", "GRANULAR",
        {"designed", "grained", "grains", "grainy", "granular", "granulated", "gritty", "particles", "paulstretch", "slices", "textured"}});
    m_mappings.push_back({"DESIGNED", "IMPACT",
        {"designed", "blow", "bump", "collision", "crash", "hit", "impact", "jolt", "knock", "slam", "smash", "strike", "thud", "trailer"}});
    m_mappings.push_back({"DESIGNED", "MISC", {"designed", "misc", "miscellaneous"}});
    m_mappings.push_back({"DESIGNED", "MORPH",
        {"designed", "alter", "blend", "change", "convert", "evolving", "metamorphosis", "morph", "mutate", "reshape", "shape", "transfiguration", "transform", "transformation", "transition", "transmutation", "transmute"}});
    m_mappings.push_back({"DESIGNED", "RISER",
        {"designed", "ascend", "ascending", "climbing", "crescendo", "escalate", "frequency", "intensify", "pitch", "reverse", "riser", "rising", "rizer", "soar", "surge", "tension", "tone", "trailer"}});
    m_mappings.push_back({"DESIGNED", "RUMBLE",
        {"designed", "bass", "deep", "earthquake", "grumble", "quake", "rumble", "shake", "subharmonic", "subsonic", "subwoofer", "tremble", "vibrate"}});
    m_mappings.push_back({"DESIGNED", "RHYTHMIC",
        {"designed", "beat", "glitch", "pattern", "pulse", "rhythmic", "staccato", "stutter", "temp"}});
    m_mappings.push_back({"DESIGNED", "SOURCE", {"designed", "material", "raw", "recording", "source"}});
    m_mappings.push_back({"DESIGNED", "STINGER",
        {"designed", "stinger", "cinematic", "piercing", "slam", "stab", "startle", "sting", "stinging", "trailer"}});
    m_mappings.push_back({"DESIGNED", "SYNTHETIC",
        {"designed", "synthetic", "analog", "digital", "granular", "mod", "modular", "synth", "synthesized"}});
    m_mappings.push_back({"DESIGNED", "TONAL",
        {"designed", "tonal", "bowed", "chord", "frequency", "harmonic", "harmonious", "melodic", "melodious", "musical", "note", "ping", "pitch", "resonance", "resonant", "ring", "ringing", "shell", "shing", "shock", "sonorous", "sound", "subjective", "timbre", "tinnitus", "tone", "vibration"}});
    m_mappings.push_back({"DESIGNED", "VOCAL",
        {"designed", "bellow", "enchantment", "ghostly", "howl", "oral", "processed", "roar", "scream", "screech", "spell", "verbal", "vocal", "vocalized", "voiced", "wail", "whisper"}});
    m_mappings.push_back({"DESIGNED", "WHOOSH",
        {"designed", "whoosh", "air", "by", "dopplered", "fast", "flying", "motion", "movement", "rapid", "rushing", "speeding", "swish", "swishes", "swooping", "swoosh", "swooshes", "trailer", "whirr", "whooshed", "whooshes", "whooshing"}});

    // DESTRUCTION Category
    m_mappings.push_back({"DESTRUCTION", "COLLAPSE",
        {"destruction", "break", "cave", "collapse", "crumble", "crumple", "demolish", "demolition", "disintegrate", "down", "failure", "fall", "fell", "implode", "in", "sinkhole", "structure", "topple"}});
    m_mappings.push_back({"DESTRUCTION", "CRASH & DEBRIS",
        {"destruction", "crash & debris", "bender", "break", "car", "crash", "debris", "destroy", "detritus", "fender", "fragments", "implode", "implosion", "pileup", "plane", "rubble", "ruin", "rupture", "shatter", "shipwreck", "smash", "splinter", "train", "wreck", "wreckage"}});
    m_mappings.push_back({"DESTRUCTION", "MISC", {"destruction", "misc", "miscellaneous"}});

    // DIRT & SAND Category
    m_mappings.push_back({"DIRT & SAND", "CRASH & DEBRIS",
        {"dirt & sand", "crash & debris", "airborne", "ashes", "cinder", "cinders", "clay", "cloud", "crumble", "crumbs", "debris", "dirt", "disperse", "dust", "dusty", "earth", "earthy", "gravel", "grime", "grit", "gritty", "particulates", "pebbles", "plume", "pollen", "powdered", "quartz", "sand", "sandstorm", "sawdust", "sediment", "silt", "soil", "soot", "spray", "trickle"}});
    m_mappings.push_back({"DIRT & SAND", "DUST",
        {"dirt & sand", "airborne", "ashes", "cinder", "cinders", "cloud", "crumbs", "debris", "dirt", "disperse", "dust", "dusty", "earth", "earthy", "fine", "flecks", "fragments", "granules", "grime", "grit", "gritty", "motes", "particle", "particles", "particulate", "particulates", "plume", "pollen", "powder", "powdered", "sand", "sandstorm", "sawdust", "sediment", "silt", "small", "soil", "soot", "specks", "tiny"}});
    m_mappings.push_back({"DIRT & SAND", "HANDLE",
        {"dirt & sand", "airborne", "ashes", "catch", "cinder", "cinders", "clench", "crumbs", "dig", "disperse", "earth", "form", "grab", "grasp", "grime", "grip", "grit", "handle", "hold", "manipulate", "mold", "particulates", "pollen", "powdered", "sawdust", "sculpt", "shape", "silt", "soil", "soot", "throw", "toss", "touch", "work"}});
    m_mappings.push_back({"DIRT & SAND", "IMPACT",
        {"dirt & sand", "impact", "airborne", "ashes", "bang", "bump", "cinder", "cinders", "clod", "clump", "crumbs", "disperse", "drop", "dump", "dusty", "earth", "grime", "grit", "heap", "hit", "knock", "lump", "particulates", "pollen", "powdered", "sawdust", "silt", "slam", "soil", "soot", "thud", "thump", "unload"}});
    m_mappings.push_back({"DIRT & SAND", "MISC",
        {"dirt & sand", "misc", "airborne", "ashes", "cinder", "cinders", "crumbs", "disperse", "dusty", "earth", "earthy", "grime", "grit", "gritty", "miscellaneous", "particulates", "pollen", "powdered", "sawdust", "silt", "soil", "soot"}});
    m_mappings.push_back({"DIRT & SAND", "MOVEMENT",
        {"dirt & sand", "movement", "airborne", "ashes", "cascade", "cinder", "cinders", "crumbs", "digging", "disperse", "displacement", "drift", "earth", "flow", "grime", "grit", "move", "particulates", "piling", "pollen", "pour", "pouring", "powdered", "sawdust", "shift", "shifting", "silt", "slide", "soil", "soot", "spill", "swirl", "unload", "upheaval"}});
    m_mappings.push_back({"DIRT & SAND", "TONAL",
        {"dirt & sand", "airborne", "ashes", "cinder", "cinders", "crumbs", "disperse", "dunes", "earth", "frequency", "grime", "grit", "harmonic", "melodic", "melodious", "musical", "particulates", "pitch", "pollen", "powdered", "resonance", "resonant", "sand", "sands", "sawdust", "silt", "singing", "soil", "sonorous", "soot", "sound", "timbre", "tonal", "tone"}});

    // DOORS Category
    m_mappings.push_back({"DOORS", "ANTIQUE",
        {"doors", "abbey", "aged", "ancient", "antiquated", "antique", "barn", "cabin", "castle", "church", "classic", "cottage", "distressed", "door", "farmhouse", "historic", "historical", "old", "old-fashioned", "period", "restored", "retro", "traditional", "victorian", "vintage", "weathered"}});
    m_mappings.push_back({"DOORS", "APPLIANCE",
        {"doors", "appliance", "dishwasher", "dryer", "freezer", "fridge", "laundry", "machine", "microwave", "oven", "refrigerator", "washing"}});
    m_mappings.push_back({"DOORS", "CABINET",
        {"doors", "armoire", "bar", "bathroom", "buffet", "cabinet", "center", "china", "cupboard", "display", "door", "entertainment", "hutch", "kitchen", "linen", "locker", "medicine", "pantry", "storage", "tool", "vanity", "wardrobe", "wine"}});
    m_mappings.push_back({"DOORS", "COMPOSITE", {"doors", "composite", "fiberglass"}});
    m_mappings.push_back({"DOORS", "CREAK",
        {"doors", "castle", "cellar", "creak", "creaky", "dungeon", "groan", "haunted", "metal", "old", "rusty", "scary", "screech", "squeak", "squeaky", "wood"}});
    m_mappings.push_back({"DOORS", "DUNGEON",
        {"doors", "dungeon", "castle", "cellar", "chamber", "crypt", "fortress", "large", "medieval", "oubliette", "passage", "secret", "stronghold", "torture", "tower"}});
    m_mappings.push_back({"DOORS", "ELECTRIC",
        {"doors", "automatic", "door", "electric", "garage", "hangar", "motorized", "power", "soundstage", "warehouse"}});
    m_mappings.push_back({"DOORS", "GATE",
        {"doors", "barricade", "barrier", "cattle", "crossing", "farm", "fence", "garden", "gate", "park", "rail"}});
    m_mappings.push_back({"DOORS", "GLASS", {"doors", "bay", "door", "french", "front", "glass", "patio", "store"}});
    m_mappings.push_back({"DOORS", "HARDWARE",
        {"doors", "bar", "chain", "deadbolt", "door", "doorknob", "fixture", "handle", "hardware", "hinge", "jiggle", "knob", "latch", "lock", "peephole", "picking", "push", "stop"}});
    m_mappings.push_back({"DOORS", "HITECH",
        {"doors", "hitech", "7", "bond", "door", "fort", "futuristic", "high-tech", "james", "knox", "lab", "modern", "safe", "security", "vault"}});
    m_mappings.push_back({"DOORS", "HYDRAULIC & PNEUMATIC",
        {"doors", "hydraulic & pneumatic", "closer", "dock", "door", "hatch", "hydraulic", "loading", "pneumatic", "powered"}});
    m_mappings.push_back({"DOORS", "KNOCK", {"doors", "bang", "knock", "knocker", "rap", "thud", "thump"}});
    m_mappings.push_back({"DOORS", "METAL",
        {"doors", "metal", "door", "electrical", "fire", "fire-door", "garage", "hatch", "panel", "rollup", "screen"}});
    m_mappings.push_back({"DOORS", "MISC", {"doors", "misc", "miscellaneous"}});
    m_mappings.push_back({"DOORS", "PLASTIC", {"doors", "plastic", "john", "porta"}});
    m_mappings.push_back({"DOORS", "PRISON",
        {"doors", "prison", "cell", "confinement", "correctional", "detention", "gaol", "incarceration", "jail", "lockup", "penitentiary", "solitary"}});
    m_mappings.push_back({"DOORS", "REVOLVING",
        {"doors", "airport", "bank", "hotel", "mall", "revolve", "revolving", "rotate", "rotating", "turning"}});
    m_mappings.push_back({"DOORS", "SLIDING", {"doors", "door", "glass", "patio", "shower", "sliding"}});
    m_mappings.push_back({"DOORS", "STONE",
        {"doors", "stone", "castle", "crypt", "entrance", "mausoleum", "secret", "temple", "tomb"}});
    m_mappings.push_back({"DOORS", "SWINGING", {"doors", "swinging", "pulpit", "restaurant", "saloon"}});
    m_mappings.push_back({"DOORS", "WOOD",
        {"doors", "wood", "apartment", "armoires", "barn", "cabin", "church", "closet", "condo", "entrance", "front", "home", "house"}});

    // DRAWERS Category
    m_mappings.push_back({"DRAWERS", "METAL",
        {"drawers", "metal", "cabinet", "cash", "chest", "filing", "rack", "register", "security", "toolbox", "workbench"}});
    m_mappings.push_back({"DRAWERS", "MISC", {"drawers", "misc", "miscellaneous"}});
    m_mappings.push_back({"DRAWERS", "PLASTIC",
        {"drawers", "art", "bin", "classroom", "craft", "desk", "garage", "makeup", "organizer", "parts", "plastic", "rolling", "stackable", "storage", "supply"}});
    m_mappings.push_back({"DRAWERS", "WOOD",
        {"drawers", "wood", "bathroom", "bedroom", "buffet", "desk", "drawer", "dresser", "kitchen", "nightstand"}});

    // ELECTRICITY Category
    m_mappings.push_back({"ELECTRICITY", "ARC",
        {"electricity", "arc", "arch", "ark", "carbon", "coil", "discharge", "electrocute", "flash", "gap", "ignition", "jacobs", "ladder", "lamp", "plasma", "power", "pulse", "spark", "station", "surge", "tesla", "welding"}});
    m_mappings.push_back({"ELECTRICITY", "BUZZ & HUM",
        {"electricity", "buzz & hum", "amp", "buzz", "buzzing", "capacitor", "circuit", "drone", "elec", "electro", "generator", "ground", "hum", "induction", "inductor", "line", "power", "resonance", "speaker", "station", "substation", "transformer", "transmission"}});
    m_mappings.push_back({"ELECTRICITY", "ELECTROMAGNETIC",
        {"electricity", "electromagnetic", "coil", "emf", "field", "induction", "interference", "magnetism", "pickup", "radiation", "radio", "spectrum", "waves"}});
    m_mappings.push_back({"ELECTRICITY", "MISC",
        {"electricity", "misc", "current", "elec", "electrically", "electrician", "electricians", "electrics", "electrification", "miscellaneous", "voltage"}});
    m_mappings.push_back({"ELECTRICITY", "SPARKS",
        {"electricity", "amperage", "burst", "circuit", "crackle", "elec", "electrical", "electrically", "electrician", "electricians", "electrics", "electrification", "influx", "short", "shorting", "sparks", "sputter", "weld", "welding"}});
    m_mappings.push_back({"ELECTRICITY", "ZAP",
        {"electricity", "zap", "bolt", "discharge", "electric", "electrocute", "fence", "gun", "jolt", "shock", "stun", "tase", "taser", "triggers", "zapper", "zaps"}});

    // EQUIPMENT Category
    m_mappings.push_back({"EQUIPMENT", "BRIDLE & TACK",
        {"equipment", "bridle & tack", "bit", "bits", "bridle", "bridles", "chains", "curb", "dog", "guards", "halter", "halters", "harness", "leash", "reins", "saddle", "stirrup", "strap", "tack"}});
    m_mappings.push_back({"EQUIPMENT", "HITECH",
        {"equipment", "hitech", "belt", "cloaking", "device", "field", "force", "space", "spyware", "suit", "utility"}});
    m_mappings.push_back({"EQUIPMENT", "MISC", {"equipment", "misc", "miscellaneous"}});
    m_mappings.push_back({"EQUIPMENT", "RECREATIONAL",
        {"equipment", "recreational", "backpack", "bag", "bags", "balls", "beach", "camp", "camping", "canoes", "chairs", "climb", "climbing", "club", "clubs", "exercise", "fishing", "frisbees", "gear", "golf", "hike", "hiking", "hunting", "jacket", "kites", "life", "mountaineering", "oar", "outfitting", "paddle", "racket", "rackets", "rock", "rods", "ski", "sleeping", "sports", "stove", "tennis", "tent", "tents", "vest"}});
    m_mappings.push_back({"EQUIPMENT", "SPORT",
        {"equipment", "sport", "badminton", "balls", "bars", "baseball", "basketball", "bats", "boxing", "clubs", "cricket", "cycling", "football", "gear", "gloves", "golf", "gymnastics", "helmet", "hockey", "hoops", "hurdle", "jock", "knee", "mats", "mitt", "mouthpiece", "pad", "puck", "rackets", "racquet", "shin", "shoulder", "skates", "ski", "snowboards", "soccerball", "squash", "sticks", "strap", "tennis", "volleyballs", "weightlifting", "yoga"}});
    m_mappings.push_back({"EQUIPMENT", "TACTICAL",
        {"equipment", "tactical", "ammo", "armor", "bandoleer", "belt", "binoculars", "body", "boots", "camouflage", "climbing", "combat", "compass", "duty", "flashlights", "gas", "gear", "gloves", "goggles", "handcuffs", "helmet", "holsters", "mask", "masks", "military", "multi-tools", "night", "nightstick", "police", "rucksacks", "vests", "vision"}});

    // EXPLOSIONS Category
    m_mappings.push_back({"EXPLOSIONS", "DESIGNED",
        {"explosions", "designed", "atomic", "bangs", "blast", "blasted", "blasting", "blasts", "blazes", "blowup", "bombe", "bombing", "bombproof", "bombshell", "boom", "booms", "burst", "bursts", "combustions", "detonation", "detonations", "discharges", "echo", "eruptions", "explo", "fragmentary", "grenade", "ignitions", "kiloton", "nuke", "outbursts", "pyrotechnic", "shockwave", "stylized", "sweetener", "thermonuclear"}});
    m_mappings.push_back({"EXPLOSIONS", "MISC", {"explosions", "misc", "miscellaneous"}});
    m_mappings.push_back({"EXPLOSIONS", "REAL",
        {"explosions", "real", "bang", "bangs", "blast", "blasted", "blasting", "blasts", "blazes", "blowup", "bombe", "bombing", "bombproof", "bombshell", "boom", "booms", "burst", "bursts", "c4", "combustions", "crack", "detonation", "detonations", "discharges", "dynamite", "eruptions", "explo", "explosive", "fragmentary", "grenade", "ignitions", "implosion", "outbursts", "plastic", "pop", "pyrotechnic"}});

    // FARTS Category
    m_mappings.push_back({"FARTS", "DESIGNED",
        {"farts", "designed", "armpit", "balloon", "cushion", "fart", "fartbag", "flarp", "flatulence", "poot", "putty", "raspberry", "razzberry", "sludge", "toot", "whoopie"}});
    m_mappings.push_back({"FARTS", "MISC",
        {"farts", "misc", "animal", "breaking", "fart", "farting", "flatulence", "flatus", "gas", "passing", "poot", "toot", "wind"}});
    m_mappings.push_back({"FARTS", "REAL",
        {"farts", "real", "breaking", "fart", "farting", "flatulence", "flatus", "gas", "human", "passing", "poot", "toot", "wind"}});

    // FIGHT Category
    m_mappings.push_back({"FIGHT", "BODYFALL",
        {"fight", "body", "bodyfall", "collapse", "dive", "drag", "drop", "fall", "falling", "flop", "impact", "nosedive", "plunge", "scuffle", "slam", "slip", "slump", "tackle", "thud", "topple", "tumble"}});
    m_mappings.push_back({"FIGHT", "CLOTH",
        {"fight", "cloth", "arts", "brawl", "combat", "flutter", "grab", "grapple", "grasp", "hit", "judo", "martial", "movement", "rustle", "scuffling", "tackle", "tug", "tussle", "twist"}});
    m_mappings.push_back({"FIGHT", "GRAB",
        {"fight", "grab", "catch", "cling", "clutch", "combat", "embrace", "grapple", "grappling", "grasp", "grip", "hand", "hold", "hook", "hug", "judo", "nab", "pinch", "pluck", "seize", "snag", "snatch", "take", "wrestle"}});
    m_mappings.push_back({"FIGHT", "IMPACT",
        {"fight", "impact", "bang", "biff", "blow", "blows", "body", "bouts", "brawl", "brawling", "counterpunch", "elbows", "fisticuffs", "fists", "haymakers", "hit", "hits", "jab", "jabs", "kick", "knock", "knockdown", "knockout", "knockouts", "knocks", "punch", "ram", "roundhouse", "shots", "shove", "slam", "slap", "slaps", "smack", "sock", "strike", "strikes", "takedown", "thwack", "tko", "uppercut", "wallop", "whack"}});
    m_mappings.push_back({"FIGHT", "MISC",
        {"fight", "misc", "attacking", "battling", "beating", "bout", "brawling", "clash", "combatant", "dispute", "miscellaneous", "quarreling", "sparring"}});

    // FIRE Category
    m_mappings.push_back({"FIRE", "BURNING",
        {"fire", "afire", "aflare", "alight", "arson", "blaze", "blazing", "bonfire", "brush", "burning", "campfire", "char", "combustion", "conflagration", "consume", "consumption", "cremation", "deflagration", "enflame", "fiery", "flames", "flare", "flashpoint", "forest", "hotspot", "incinerate", "incineration", "incinerator", "inferno", "kindle", "kindling", "pyre", "roast", "scorch", "scorching", "sear", "singe", "smoking", "smolder", "smoldering", "structure", "tinder", "wildfire"}});
    m_mappings.push_back({"FIRE", "BURST",
        {"fire", "backdraft", "blast", "burst", "bursting", "combustion", "detonate", "discharge", "discharged", "discharging", "dragon", "engulf", "eruption", "exploded", "explosion", "fiery", "fireball", "flare-up", "flash", "inflaming", "outburst", "over", "release"}});
    m_mappings.push_back({"FIRE", "CRACKLE",
        {"fire", "crackle", "crack", "crackling", "crackly", "crinkle", "pop", "popping", "snap", "snapping", "snappy", "sparkle", "spit"}});
    m_mappings.push_back({"FIRE", "GAS",
        {"fire", "acetylene", "blowtorch", "bunsen", "burner", "butane", "camping", "cookstove", "crematorium", "diesel", "flamethrower", "fuel", "furnace", "gas", "gasoline", "grill", "kerosene", "kitchen", "lp", "methane", "natural", "oilstove", "petrol", "propane", "range", "stove"}});
    m_mappings.push_back({"FIRE", "IGNITE",
        {"fire", "ablaze", "aflame", "arouse", "arson", "asbestos", "combust", "combusted", "combustible", "conflagrate", "emblaze", "enflame", "erupt", "fanned", "fanning", "flame", "flammable", "ignitable", "ignite", "ignited", "igniter", "ignitor", "incite", "inflame", "inflammable", "initiate", "light", "lighter", "match", "reignite", "set", "spark", "stimulate", "trigger", "up", "zippo"}});
    m_mappings.push_back({"FIRE", "MISC", {"fire", "misc", "cinder", "cinders", "extinguisher", "miscellaneous"}});
    m_mappings.push_back({"FIRE", "SIZZLE",
        {"fire", "barbecue", "barbeque", "burn", "crackling", "fizzing", "frying", "fuse", "hissing", "pan", "poker", "popping", "searing", "sizzle", "sizzled", "sizzling", "sparkling"}});
    m_mappings.push_back({"FIRE", "TORCH",
        {"fire", "torch", "beacon", "blazing", "brazier", "candlelight", "cresset", "firebrand", "firestick", "flambeau", "flaming", "flare", "flickering", "lamplighter", "sconce", "signal", "smothered", "smothering", "stick", "taper", "torchbearer", "torcher", "torching", "torchlight", "wick"}});
    m_mappings.push_back({"FIRE", "TURBULENT",
        {"fire", "turbulent", "backdraft", "blaze", "engulf", "fierce", "fierceness", "firestorm", "flame", "fury", "inferno", "swirl", "thrower", "tornado", "violence", "violent"}});
    m_mappings.push_back({"FIRE", "WHOOSH",
        {"fire", "whoosh", "backdraft", "blaze", "fireball", "flare", "molotov", "rush", "whooshed", "whooshing"}});

    // FIREWORKS Category
    m_mappings.push_back({"FIREWORKS", "COMMERCIAL",
        {"fireworks", "commercial", "aerial", "boom", "display", "firework", "independence", "kaboom", "mortar", "pyrotechnic", "pyrotechnical", "pyrotechnics", "rockets", "salute", "shell", "shells"}});
    m_mappings.push_back({"FIREWORKS", "MISC", {"fireworks", "misc", "miscellaneous"}});
    m_mappings.push_back({"FIREWORKS", "RECREATIONAL",
        {"fireworks", "recreational", "bang", "blast", "bomb", "boom", "bottle", "candle", "cherry", "crackle", "firecracker", "fizz", "fountain", "jack", "jumping", "m80", "pop", "popper", "rocket", "roman", "smoke", "snaps", "sparkler", "whizz"}});

    // FOLEY Category
    m_mappings.push_back({"FOLEY", "CLOTH",
        {"foley", "cloth", "cape", "clothing", "dress", "fabric", "flap", "flutter", "jacket", "movement", "pants", "rustle", "shirt", "shorts", "skirt", "textile", "zuzz"}});
    m_mappings.push_back({"FOLEY", "FEET",
        {"foley", "feet", "foot", "footstep", "footsteps", "marching", "running", "scuff", "scuffling", "shoe", "sneaking", "sprinting", "step", "stomping", "surface", "tiptoeing", "trudging", "walking"}});
    m_mappings.push_back({"FOLEY", "HANDS",
        {"foley", "hands", "clapping", "flicking", "grab", "grasping", "handle", "pat", "patting", "rubbing", "scratching", "set", "shaking", "slapping", "snapping", "touching"}});
    m_mappings.push_back({"FOLEY", "MISC", {"foley", "misc", "miscellaneous"}});
    m_mappings.push_back({"FOLEY", "PROP", {"foley", "objects", "prop", "props"}});

    // FOOD & DRINK Category
    m_mappings.push_back({"FOOD & DRINK", "COOKING",
        {"food & drink", "baked", "baking", "barbecuing", "blending", "boil", "boiled", "boiling", "broiling", "burning", "buttering", "canning", "caramelization", "catering", "chopping", "cookery", "cooking", "cookpot", "cuisine", "culinary", "decorating", "defrosting", "dicing", "fry", "gourmet", "grill", "grilling", "mixing", "pantry", "parboiling", "precooking", "preparation", "prepare", "preparing", "prepping", "recipe", "recipes", "reheating", "roasting", "salting", "sauteing", "saut√©ing", "saut‚àö¬©", "seasoning", "sizzle", "slicing", "steam", "steaming", "stew", "stir"}});
    m_mappings.push_back({"FOOD & DRINK", "DRINKING",
        {"food & drink", "drinking", "alcohol", "ale", "beer", "beverage", "beverages", "booze", "boozing", "brew", "chai", "champagne", "chug", "chugging", "cider", "cocktail", "coffee", "coke", "cola", "consumed", "consuming", "consummation", "consumption", "drink", "drinkable", "drinker", "drinkers", "gatorade", "gin", "glug", "gulp", "gulping", "guzzling", "imbibe", "imbibing", "intoxication", "juice", "kombucha", "lemonade", "liquor", "liquoring", "margarita", "martini", "milk", "rum", "sip", "sipping", "slurp", "slurping", "soda"}});
    m_mappings.push_back({"FOOD & DRINK", "EATING",
        {"food & drink", "eating", "appetizer", "bite", "biting", "chew", "chewing", "chomp", "chow", "chowing", "consume", "consuming", "consumption", "crunch", "devouring", "digesting", "dine", "eat", "eaten", "edible", "feast", "feasting", "feed", "feeding", "feeds", "foods", "foodstuffs", "gobbling", "gorging", "gourmet", "grocery", "grub", "gullet", "hungry", "ingest", "ingesting", "ingestion", "lunch", "meal", "meals", "munch", "munching", "nibbling", "noshing", "nourishment", "nutrition", "nutritional", "partaking", "snack", "snacking"}});
    m_mappings.push_back({"FOOD & DRINK", "GLASSWARE",
        {"food & drink", "glassware", "beaker", "beakers", "beer", "beverages", "bottle", "bottles", "carafe", "carafes", "champagne", "clink", "cup", "decanters", "flasks", "flute", "glass", "glasswares", "glasswork", "goblet", "goblets", "highball", "jug", "martini", "mug", "mugs", "pyrex", "shot", "stemware", "teacups", "tumbler", "vase", "vases", "wine", "wineglass", "wineglasses"}});
    m_mappings.push_back({"FOOD & DRINK", "INGREDIENTS",
        {"food & drink", "ingredients", "batter", "beans", "bread", "canned", "cereal", "cheese", "condiments", "contents", "cornstarch", "egg", "eggs", "flour", "food", "fruit", "ghee", "grain", "groceries", "herb", "herbs", "juice", "lard", "lentils", "maize", "meat", "milk", "nuts", "oats", "oleo", "pasta", "pepper", "preserves", "roux", "salt", "seeds", "spices", "suet", "sugar", "treacle", "vanilla", "vegetables", "yeast"}});
    m_mappings.push_back({"FOOD & DRINK", "KITCHENWARE",
        {"food & drink", "kitchenware", "bakeware", "baking", "board", "bowl", "bread", "can", "chef's", "chopping", "colander", "cookie", "cookware", "corkscrew", "cup", "cutting", "dish", "fork", "frying", "grater", "kitchen", "knife", "knives", "ladle", "measuring", "mixing", "opener", "pan", "pans", "paring", "peeler", "pin", "pot", "pots", "rolling", "saucepan", "saucepans", "scissors", "serving", "shears", "sheet", "slotted", "spatula", "spoon", "spoons", "steamer", "stewpan", "stockpot", "strainer", "teakettle"}});
    m_mappings.push_back({"FOOD & DRINK", "MISC",
        {"food & drink", "misc", "banquet", "diet", "dieting", "epicurean", "fare", "garnish", "meal", "miscellaneous"}});
    m_mappings.push_back({"FOOD & DRINK", "POUR",
        {"food & drink", "alcohol", "ale", "bartender", "beer", "chai", "champagne", "cider", "cocktail", "coffee", "coke", "cola", "flow", "flowing", "gatorade", "gin", "juice", "kombucha", "lemonade", "margarita", "martini", "milk", "pour", "pouring", "rum", "serve", "slosh", "soda", "spill", "tea", "tequila", "vodka", "whiskey", "wine"}});
    m_mappings.push_back({"FOOD & DRINK", "TABLEWARE",
        {"food & drink", "tableware", "bowl", "bowls", "coasters", "cups", "cutlery", "dessert", "dinner", "dinnerware", "dishes", "flatware", "fork", "glasses", "knife", "mats", "napkin", "pepper", "place", "plates", "platter", "platters", "rings", "salad", "salt", "saucer", "saucers", "serving", "shaker", "silverware", "soup", "spoon", "sugar", "tablespoon", "teapots", "teaspoon", "tongs", "tray", "trays", "tureens", "utensils"}});

    // FOOTSTEPS Category
    m_mappings.push_back({"FOOTSTEPS", "ANIMAL",
        {"footsteps", "animal", "bolt", "buck", "clack", "claws", "clomp", "footfall", "footprints", "hoof", "hooves", "jump", "lope", "march", "pace", "patter", "paw", "plod", "prance", "rear", "run", "scuff", "scurry", "scurrying", "shuffle", "skip", "sprint", "stamp", "stampede", "step", "steps", "stomp", "stride", "strut", "stumble", "thud", "tracks", "tramp", "tread", "walk"}});
    m_mappings.push_back({"FOOTSTEPS", "CREATURE",
        {"footsteps", "alien", "beast", "clack", "clomp", "creature", "dinosaur", "dragon", "footfall", "march", "monster", "mythical", "pace", "patter", "plod", "scuff", "shuffle", "stamp", "step", "stomp", "stride", "thud", "tramp", "tread"}});
    m_mappings.push_back({"FOOTSTEPS", "HORSE",
        {"footsteps", "horse", "canter", "cantering", "clack", "clip-clop", "clomp", "clop", "clopping", "footfall", "gallop", "hoof", "hooves", "march", "pace", "patter", "plod", "run", "scuff", "shuffle", "stamp", "step", "stomp", "stride", "thud", "tramp", "tread", "trot", "trots", "trotting", "walk"}});
    m_mappings.push_back({"FOOTSTEPS", "HUMAN",
        {"footsteps", "human", "ambling", "clack", "climb", "clomp", "feet", "foot", "footfall", "footstep", "gallivanting", "hike", "hiking", "hobble", "hobbling", "hop", "jog", "jump", "limp", "limping", "march", "marching", "meandering", "moseying", "pace", "pacing", "pad.", "patter", "perambulating", "plod", "plodding", "promenading", "rambling", "roaming", "run", "running", "sashaying", "saunter", "sauntering", "schlepping", "scuff", "scuffling", "shambles", "shuffle", "shuffling", "skip", "sprint", "stamp", "step", "stepping"}});
    m_mappings.push_back({"FOOTSTEPS", "MISC",
        {"footsteps", "misc", "clack", "clomp", "footfall", "march", "miscellaneous", "pace", "patter", "plod", "scuff", "shuffle", "stamp", "step", "stomp", "stride", "thud", "tramp", "tread"}});

    // GAMES Category
    m_mappings.push_back({"GAMES", "ARCADE",
        {"games", "arcade", "air", "claw", "coin-op", "crane", "dance", "foosball", "gallery", "game", "hockey", "machine", "pacman", "pinball", "retro", "revolution", "shooting", "skee-ball", "skeeball", "video", "whack-a-mole"}});
    m_mappings.push_back({"GAMES", "BOARD",
        {"games", "board", "backgammon", "bingo", "boardgame", "checkerboard", "checkers", "chess", "chessboard", "clue", "cribbage", "dominoes", "game", "gameboard", "go", "mahjong", "mancala", "monopoly", "pieces", "risk", "roleplaying", "scrabble", "uno"}});
    m_mappings.push_back({"GAMES", "CASINO",
        {"games", "casino", "baccarat", "betting", "blackjack", "cards", "craps", "dice", "keno", "machine", "machines", "money", "poker", "roulette", "shuffling", "slot", "video", "wheel"}});
    m_mappings.push_back({"GAMES", "MISC",
        {"games", "misc", "a", "bag", "ball", "balloon", "basketball", "bean", "bottle", "bowl", "bowler", "break", "can", "clown", "coin", "crazy", "dart", "darts", "duck", "fish", "frog", "game", "goblet", "high", "hoop", "knock-down", "knockdown", "lucky", "milk", "miscellaneous", "number", "plate", "plinko", "pond", "prize", "ring", "roller", "roulette", "shoot", "shooting", "skee-ball", "star", "strength", "striker", "test", "the", "throw.", "toss", "up", "water"}});
    m_mappings.push_back({"GAMES", "VIDEO",
        {"games", "video", "360", "64", "atari", "console", "fortnite", "gameboy", "halo", "megadrive", "minecraft", "nintendo", "one", "pacman", "playstation", "pong", "ps4", "ps5", "psp", "sega", "snes", "xbox", "zelda"}});

    // GEOTHERMAL Category
    m_mappings.push_back({"GEOTHERMAL", "FUMAROLE",
        {"geothermal", "fissure", "fumarole", "fumarolic", "gaseous", "hydrothermal", "solfatara", "steam", "steam vent", "stufa", "sulfur", "sulphur", "thermal", "vent", "venthole", "vents", "volcanic", "yellowstone"}});
    m_mappings.push_back({"GEOTHERMAL", "GEYSER",
        {"geothermal", "basin", "blowhole", "boil", "boiling", "bubble", "bubbler", "erupt", "erupting", "eruption", "faithful", "geiser", "geothermic", "geyser", "geysir", "old", "scalding", "spout", "spouter", "spray", "strokkur", "thermal", "yellowstone"}});
    m_mappings.push_back({"GEOTHERMAL", "LAVA",
        {"geothermal", "basaltic", "basalts", "calderas", "cindery", "crackle", "craters", "fireballs", "flare", "flow", "igneous", "kilauea", "lava", "lave", "magma", "magmas", "magmatic", "molten", "pumices", "pyroclastic", "pyroclasts", "viscous", "volcan", "volcanically", "volcaniclastic", "volcanics", "volcano", "volcanoes", "volcanos", "vulcanian", "vulcanic", "vulcanism", "yellowstone"}});
    m_mappings.push_back({"GEOTHERMAL", "MISC",
        {"geothermal", "misc", "bubbling", "geotherm", "geothermic", "hot", "hydroelectric", "hydroelectricity", "hydrogeologic", "hydrogeological", "hydrogeology", "hydronic", "hydropower", "hydrovolcanic", "hypothermal", "pool", "spring", "thermal", "thermic", "thermodynamic", "thermoelectric", "thermogenetic", "thermogenic", "thermogenous", "thermogeological", "thermological", "thermosyphon", "thermosyphons", "thermotic", "volcanogenic", "volcanological", "yellowstone"}});
    m_mappings.push_back({"GEOTHERMAL", "MUD POTS",
        {"geothermal", "mud pots", "bubble", "bubbling", "glop", "gurgling", "mud", "paint", "pit", "pot", "tar", "yellowstone"}});

    // GLASS Category
    m_mappings.push_back({"GLASS", "BREAK",
        {"glass", "break", "bulletproof", "burst", "chip", "crack", "crush", "demolish", "destroy", "fracture", "fragment", "frosted", "laminated", "safety", "shatter", "smash", "snap", "splinter", "split", "stained", "tempered", "tinted", "window"}});
    m_mappings.push_back({"GLASS", "CRASH & DEBRIS",
        {"glass", "crash & debris", "broken", "bulletproof", "crash", "debris", "fragment", "fragments", "frosted", "laminated", "pieces", "remains", "rubble", "safety", "shard", "shards", "shattered", "splinters", "stained", "tempered", "tinted", "window"}});
    m_mappings.push_back({"GLASS", "FRICTION",
        {"glass", "friction", "abrasion", "bulletproof", "creak", "frosted", "grinding", "laminated", "rasping", "rubbing", "safety", "scrape", "scraping", "scratching", "screech", "screeching", "scuffing", "sliding", "squeak", "squeaking", "stained", "stress", "tempered", "tinted", "window"}});
    m_mappings.push_back({"GLASS", "HANDLE",
        {"glass", "bulletproof", "catch", "clasp", "clench", "clutch", "down", "embrace", "frosted", "grab", "grapple", "grasp", "grip", "handle", "hold", "laminated", "operate", "pickup", "pull", "safety", "seize", "set", "stained", "take", "tempered", "throw", "tinted", "toss", "use", "window"}});
    m_mappings.push_back({"GLASS", "IMPACT",
        {"glass", "bang", "banging", "blow", "bulletproof", "clink", "collide", "colliding", "drop", "frosted", "hit", "hitting", "impact", "impacting", "jolt", "knock", "laminated", "pound", "ram", "safety", "slam", "slamming", "smack", "smacking", "stained", "strike", "striking", "tempered", "thud", "tinted", "window"}});
    m_mappings.push_back({"GLASS", "MISC",
        {"glass", "misc", "bulletproof", "frosted", "laminated", "miscellaneous", "safety", "stained", "tempered", "tinted", "window"}});
    m_mappings.push_back({"GLASS", "MOVEMENT",
        {"glass", "movement", "bulletproof", "chattering", "clanking", "clatter", "frosted", "jangle", "jiggling", "laminated", "ping", "rattle", "rattling", "roll", "rolled", "rolling", "safety", "shaking", "shifting", "sliding", "stained", "swaying", "swinging", "tempered", "tinted", "vibrating", "window"}});
    m_mappings.push_back({"GLASS", "TONAL",
        {"glass", "blow", "bottle", "bowed", "bowl", "bulletproof", "crystal", "frequency", "frosted", "harmonic", "harmonica", "harmonics", "laminated", "melodic", "melodious", "musical", "ping", "pitch", "resonance", "resonant", "resonate", "ring", "safety", "shing", "sonorous", "sound", "stained", "tempered", "timbre", "tinkle", "tinkling", "tinted", "tonal", "tone", "window"}});

    // GORE Category
    m_mappings.push_back({"GORE", "BLOOD",
        {"gore", "artery", "bleed", "bleeding", "blood", "bloodborne", "bloodshed", "bloody", "cells", "circulatory", "clot", "coagulate", "drip", "flow", "globular", "gush", "hemoglobin", "hemorrhage", "ichor", "lifeblood", "plasma", "platelets", "red", "sanguine", "seep", "serum", "spray", "spurt", "transfusion", "vascular", "white", "wound"}});
    m_mappings.push_back({"GORE", "BONE",
        {"gore", "bone", "bonelike", "bonemeal", "boney", "bony", "break", "broken", "cartilages", "compound", "crunch", "femoral", "femur", "fracture", "gristle", "humerus", "jaw", "jawbone", "knuckle", "knucklebone", "ligament", "marrow", "rib", "shatter", "shinbone", "shinbones", "skeletal", "skeleton", "snap", "spinal", "spine", "tibia", "tooth"}});
    m_mappings.push_back({"GORE", "BURN",
        {"gore", "burn", "acid", "blistered", "brand", "burned", "cauterize", "cauterized", "char", "charred", "peeling", "scald", "scalds", "scorch", "scorched", "sear", "searing", "singe", "singed", "sizzle"}});
    m_mappings.push_back({"GORE", "FLESH",
        {"gore", "carcass", "carnal", "corpse", "corpses", "decaying", "flayed", "flaying", "flesh", "fleshiness", "fleshly", "fleshy", "gnaw", "gristle", "intestine", "meat", "muscle", "peel", "putrefying", "rend", "rip", "rotting", "sinew", "sinews", "sinewy", "skin", "skinless", "skinned", "skins", "tear", "tissue", "wound"}});
    m_mappings.push_back({"GORE", "MISC",
        {"gore", "misc", "beheadings", "bloodbaths", "carnage", "decapitations", "disembowelment", "dismemberment", "dismemberments", "gory", "graphic", "gruesome", "gruesomeness", "macabre", "miscellaneous", "mutilation", "offal", "remains", "scalpings", "slaughter", "viscera"}});
    m_mappings.push_back({"GORE", "OOZE",
        {"gore", "blob", "burbling", "bursting", "congeal", "congealed", "congealing", "discharge", "drippy", "emanate", "exude", "exuded", "exudes", "exuding", "flow", "gelatin", "gobs", "goo", "goopy", "guck", "gunk", "gushes", "jelly", "liquidy", "muck", "mucus", "mush", "ooze", "oozy", "permeate", "pus", "putrefy", "putrefying", "putrescence", "putrescent", "putrid", "seep", "seeped", "seeping", "seeps", "slime", "sliminess", "slithering", "sludge", "slurry", "spewing", "splattered", "spurting", "squishy", "suppurate"}});
    m_mappings.push_back({"GORE", "SOURCE",
        {"gore", "arteries", "bones", "brain", "cartilage", "cord", "entrails", "eyeballs", "guts", "intestines", "ligaments", "limbs", "matter", "muscles", "organs", "raw", "skin", "skull", "source", "spinal", "teeth", "tendons", "veins"}});
    m_mappings.push_back({"GORE", "SPLAT",
        {"gore", "splat", "gack", "glob", "globs", "glop", "goo", "goop", "goosh", "intestine", "juicy", "organ", "plop", "plopping", "plops", "poop", "smear", "snot", "spack", "spattered", "spatters", "spittle", "splatter", "splattered", "splatters", "splatting", "splodge", "splodges", "splosh", "splotch", "splotched", "splotches", "splotching", "squish"}});
    m_mappings.push_back({"GORE", "SQUISH",
        {"gore", "compress", "goosh", "intestine", "juicy", "macerate", "mash", "mashed", "mashing", "moosh", "mush", "mushed", "mushy", "organ", "slop", "splat", "squash", "squashed", "squashing", "squashy", "squeezed", "squeezes", "squeezing", "squelch", "squelchy", "squidge", "squiggle", "squirty", "squish", "squishing", "squishy"}});
    m_mappings.push_back({"GORE", "STAB",
        {"gore", "arrow", "behead", "cut", "dagger", "decapitate", "disembowel", "dismember", "eviscerate", "flay", "impale", "impaling", "jab", "jabbed", "jabbing", "jugular", "knife", "knifed", "knifing", "mutilate", "needle", "penetrate", "pierce", "poke", "prod", "puncture", "shank", "skewer", "slash", "slashing", "slice", "spear", "spike", "stab", "sword", "syringe", "thrust", "vorpal"}});

    // GUNS Category
    m_mappings.push_back({"GUNS", "ANTIQUE",
        {"guns", "antique", "arquebus", "arquebuses", "black", "blunderbuss", "breechloader", "breechloaders", "breechloading", "caplock", "carabine", "civil", "derringer", "derringers", "firearms", "firelock", "flintlock", "gatling", "gun", "gunflint", "harquebus", "historical", "lockplate", "lockwork", "long", "matchlock", "matchlocks", "musket", "musketeer", "musketry", "muskets", "muzzleloader", "muzzleloaders", "muzzleloading", "old", "old-fashioned", "pepperbox", "powder", "powderhorn", "revolutionary", "rifle", "sixgun", "sixguns", "traditional", "victorian", "vintage", "war", "west", "wheellock", "wwi"}});
    m_mappings.push_back({"GUNS", "ARTILLERY",
        {"guns", "anti-aircraft", "anti-tank", "antiaircraft", "antiarmor", "antitank", "armaments", "artillerie", "artillery", "ballistics", "bazooka", "cannon", "field", "firepower", "heavy", "howitzer", "howitzers", "incoming", "launcher", "light", "missile", "mortar", "mortars", "munitions", "naval", "ordnance", "rocket", "shellfire", "shelling", "tank", "volley", "weapons"}});
    m_mappings.push_back({"GUNS", "AUTOMATIC",
        {"guns", "ak-47", "ak47", "automatic", "firearms", "full-auto", "gatlin", "gatling", "gun", "kalashnikov", "m16", "m1919", "m249", "machine", "maxim", "minigun", "sten", "submachine", "submachinegun", "tommy", "uzi"}});
    m_mappings.push_back({"GUNS", "CANNON",
        {"guns", "cannon", "ball", "barrel", "black", "blast", "cannonade", "cannonades", "cannonball", "cannonballs", "cannoneer", "cannoneers", "cannonry", "fuse", "gunpowder", "pirate", "powder", "primer", "projectile", "shot", "siege"}});
    m_mappings.push_back({"GUNS", "HANDLE",
        {"guns", "catch", "chambering", "clearing", "clip", "cock", "cocking", "discharge", "draw", "drawing", "drop", "dry-fire", "dry-firing", "ejecting", "grab", "grasp", "grip", "gun", "gunstock", "handgrip", "handle", "hold", "holster", "holstering", "load", "loading", "throw", "toss", "unloading"}});
    m_mappings.push_back({"GUNS", "HITECH",
        {"guns", "hitech", "7", "biometric", "bond", "firearms", "gun", "james", "night", "rail", "scope", "smart", "spy", "vision"}});
    m_mappings.push_back({"GUNS", "MECHANISM",
        {"guns", "assembly", "barrel", "bolt", "brake", "bullet", "buttplate", "chamber", "clip", "cock", "cocking", "cylinder", "firearms", "firing", "flash", "grip", "guard", "gun", "hammer", "loading", "magazine", "mechanics", "mechanism", "muzzle", "pin", "pistol", "rattle", "release", "safety", "sear", "slide", "stock", "stop", "suppressor", "trigger"}});
    m_mappings.push_back({"GUNS", "MISC", {"guns", "misc", "air", "firearms", "flare", "paintball"}});
    m_mappings.push_back({"GUNS", "PISTOL",
        {"guns", "&", "9mm", "beretta", "browning", "colt", "double-action", "firearms", "glock", "handgun", "handguns", "luger", "pistol", "pistole", "pistolet", "ppk", "revolver", "revolvers", "ruger", "sauer", "semi-automatic", "sidearm", "sig", "single-action", "six-shooter", "smith", "starter", "walther", "wesson"}});
    m_mappings.push_back({"GUNS", "RIFLE",
        {"guns", "ak-47", "ar-15", "arisaka", "assault", "bolt-action", "breechloader", "carabine", "carbine", "carbines", "casull", "firearms", "garand", "gun", "hunting", "lever-action", "long", "luger", "m1", "m16", "mauser", "powell", "remington", "riffle", "rifle", "rifleman", "riflemen", "riflery", "rimfire", "ruger", "semi-automatic", "sniper", "sporting", "winchester"}});
    m_mappings.push_back({"GUNS", "SHOTGUN",
        {"guns", "barrel", "break-action", "coach", "double", "double-barreled", "gun", "mossberg", "over-under", "pump-action", "sawed-off", "scattergun", "shotgun", "slug", "winchester"}});
    m_mappings.push_back({"GUNS", "SUPPRESSED",
        {"guns", "suppressed", "7", "assassin", "bond", "james", "rifle", "silenced", "silencer", "sniper", "spy"}});

    // HORNS Category
    m_mappings.push_back({"HORNS", "AIR POWERED", {"horns", "air powered", "air", "factory", "fog"}});
    m_mappings.push_back({"HORNS", "CELEBRATION",
        {"horns", "celebration", "bugle", "ceremonial", "fanfare", "festive", "heraldic", "party", "processional", "toot", "triumphal", "trumpets", "victory", "vuvuzela"}});
    m_mappings.push_back({"HORNS", "MISC", {"horns", "misc", "bugle", "call", "duck", "elk", "fox", "hunting"}});
    m_mappings.push_back({"HORNS", "TRADITIONAL",
        {"horns", "traditional", "alpenhorn", "alphorn", "alpine", "battle", "bugle", "conch", "hunting", "shell", "shofar", "war"}});

    // HUMAN Category
    m_mappings.push_back({"HUMAN", "BLOW",
        {"human", "adult", "blow", "child", "female", "male", "man", "person", "woman"}});
    m_mappings.push_back({"HUMAN", "BREATH",
        {"human", "adult", "asphyxiate", "asthma", "breath", "breathing", "breaths", "child", "exhalation", "exhale", "exhaled", "exhales", "exhaling", "female", "gasp", "gasping", "heave", "huff", "huffing", "hyperventilate", "hyperventilating", "inhalation", "inhale", "inhaled", "inhales", "inhaling", "labored", "male", "man", "meditation", "pant", "panting", "person", "puffing", "respiration", "sigh", "suffocate", "suffocates", "suffocating", "wheeze", "wheezing", "whoop", "woman"}});
    m_mappings.push_back({"HUMAN", "BURP",
        {"human", "adult", "belch", "belching", "burp", "burping", "child", "female", "hiccup", "male", "man", "person", "woman"}});
    m_mappings.push_back({"HUMAN", "COUGH",
        {"human", "cough", "adult", "ahem", "bronchitis", "child", "choking", "clear", "clearing", "convulse", "coughing", "covid", "expectoration", "female", "gag", "hack", "hacking", "hiccough", "hiccoughing", "hiccoughs", "hoarse", "male", "man", "person", "throat", "wet", "whooping", "woman"}});
    m_mappings.push_back({"HUMAN", "HEARTBEAT",
        {"human", "heartbeat", "adult", "arrhythmia", "arrhythmic", "arrythmia", "beating", "cardiac", "child", "coronary", "ekg", "female", "fetal", "fibrillation", "heart", "male", "man", "palpitate", "palpitates", "palpitating", "palpitation", "palpitations", "person", "pulsate", "pulsated", "pulsates", "pulsating", "pulse", "rate", "rhythm", "woman"}});
    m_mappings.push_back({"HUMAN", "KISS",
        {"human", "adult", "child", "female", "french", "hickey", "kiss", "lips", "make", "male", "man", "necking", "out", "peck", "person", "smooch", "smooching", "snog", "woman"}});
    m_mappings.push_back({"HUMAN", "MISC",
        {"human", "misc", "adult", "child", "female", "male", "man", "miscellaneous", "person", "woman"}});
    m_mappings.push_back({"HUMAN", "PEE",
        {"human", "pee", "adult", "child", "female", "male", "man", "micturition", "peeing", "person", "piss", "tinkle", "urinate", "urinating", "urination", "wee", "wizz", "woman"}});
    m_mappings.push_back({"HUMAN", "SKIN",
        {"human", "adult", "backrub", "brush", "caress", "child", "clap", "dermis", "female", "flick", "grab", "hand", "handshake", "itch", "male", "man", "massage", "pat", "person", "pores", "rash", "rub", "rubbing", "scratch", "scratching", "skin", "slap", "tap", "woman"}});
    m_mappings.push_back({"HUMAN", "SNEEZE",
        {"human", "achoo", "adult", "ah-choo", "ahchoo", "allergic", "allergy", "child", "female", "fever", "hay", "male", "man", "nasal", "person", "sinus", "sneeze", "woman"}});
    m_mappings.push_back({"HUMAN", "SNIFF",
        {"human", "adult", "child", "female", "inhalation", "inhale", "male", "man", "nose", "odor", "person", "smell", "sniff", "sniffing", "sniffle", "snivel", "snort", "snorting", "snuffle", "whiff", "woman"}});
    m_mappings.push_back({"HUMAN", "SNORE",
        {"human", "adult", "apnea", "breathing", "child", "congested", "doze", "dozing", "female", "heavy", "male", "man", "person", "slumber", "snooze", "snore", "snoring", "woman", "zzz"}});
    m_mappings.push_back({"HUMAN", "SPIT",
        {"human", "adult", "child", "drool", "expectoration", "female", "gob", "hawk", "hock", "male", "man", "mucus", "person", "phlegm", "saliva", "slobber", "spew", "spit", "spitoon", "spitting", "spittle", "spittoon", "sputum", "woman"}});
    m_mappings.push_back({"HUMAN", "VOMIT",
        {"human", "adult", "barf", "barfing", "child", "chunder", "expunge", "female", "gag", "gagging", "heave", "hurl", "male", "man", "nausea", "nauseous", "person", "puke", "pukes", "reflex", "reflux", "regurgitate", "regurgitated", "regurgitation", "retch", "retched", "retching", "seasick", "spew", "throwing", "up", "upchuck", "vomit", "vomitus", "woman"}});

    // ICE Category
    m_mappings.push_back({"ICE", "BREAK",
        {"ice", "apart", "break", "burst", "chip", "crack", "crumble", "crunch", "crush", "cube", "demolish", "destroy", "disintegrate", "fracture", "fragment", "iceberg", "icicle", "rips", "shatter", "smash", "snap", "splinter", "split"}});
    m_mappings.push_back({"ICE", "CRASH & DEBRIS",
        {"ice", "crash & debris", "apart", "break", "collision", "crash", "crevasse", "crush", "cube", "debris", "destroy", "fall", "fragments", "iceberg", "icicle", "pulverize", "remains", "rubble", "shards", "shatter", "smash", "wreck"}});
    m_mappings.push_back({"ICE", "FRICTION",
        {"ice", "friction", "abrade", "abrasion", "buff", "creak", "cube", "grind", "grinding", "hone", "icicle", "polish", "rasp", "rasping", "rub", "rubbing", "sand", "scour", "scrape", "scraping", "scratching", "screech", "scuffing", "sliding", "squeak", "stress", "wear"}});
    m_mappings.push_back({"ICE", "HANDLE",
        {"ice", "catch", "clasp", "clench", "clutch", "cube", "down", "embrace", "grab", "grasp", "grip", "handle", "hold", "icicle", "pickup", "pluck", "seize", "set", "take", "throw", "toss"}});
    m_mappings.push_back({"ICE", "IMPACT",
        {"ice", "bang", "banging", "bash", "bump", "chop", "clang", "clap", "clink", "clunk", "collide", "colliding", "crash", "crashing", "cube", "drop", "hit", "hitting", "icicle", "impact", "impacting", "jolt", "knock", "pound", "ram", "slam", "slamming", "smack", "smacking", "smash", "strike", "striking", "thrust"}});
    m_mappings.push_back({"ICE", "MISC",
        {"ice", "misc", "block", "cap", "chill", "crystal", "cube", "floe", "formation", "freeze", "frigid", "frost", "frostbite", "frosty", "frozen", "glacier", "icicle", "icy", "miscellaneous", "shard", "sheet", "solid", "wintry"}});
    m_mappings.push_back({"ICE", "MOVEMENT",
        {"ice", "movement", "calve", "calving", "cube", "drag", "drift", "float", "floating", "floe", "flow", "glide", "icicle", "move", "pile", "rattle", "roll", "shake", "shear", "shearing", "skid", "slide", "slip", "slither"}});
    m_mappings.push_back({"ICE", "TONAL",
        {"ice", "bowed", "cube", "frequency", "harmonic", "icicle", "melodic", "melodious", "musical", "ping", "pitch", "resonance", "resonant", "ring", "shing", "sonorous", "sound", "timbre", "tonal", "tone"}});

    // LASERS Category
    m_mappings.push_back({"LASERS", "BEAM",
        {"lasers", "beam", "blast", "bolt", "emission", "energy", "flash", "gamma", "glare", "gleam", "laser", "lidar", "light", "maser", "projection", "pulse", "radiation", "ray", "shine", "spark", "stream"}});
    m_mappings.push_back({"LASERS", "GUN",
        {"lasers", "gun", "beam", "blaster", "cannon", "phaser", "plasma", "ray", "weapon"}});
    m_mappings.push_back({"LASERS", "IMPACT",
        {"lasers", "beam", "blast", "blaster", "burst", "explode", "hit", "impact", "ray", "sizzle", "strike"}});
    m_mappings.push_back({"LASERS", "MISC", {"lasers", "misc", "beam", "miscellaneous", "ray"}});

    // LEATHER Category
    m_mappings.push_back({"LEATHER", "CREAK",
        {"leather", "cowhide", "creak", "deerskin", "groan", "hide", "nubuck", "pigskin", "saddle", "sheepskin", "squeak", "stress", "stretch", "suede", "tension"}});
    m_mappings.push_back({"LEATHER", "HANDLE",
        {"leather", "carry", "catch", "clasp", "clench", "clutch", "cowhide", "deerskin", "down", "embrace", "grab", "grasp", "grip", "handle", "hide", "hold", "nubuck", "pickup", "pigskin", "seize", "set", "sheepskin", "suede", "take", "throw", "toss", "wield"}});
    m_mappings.push_back({"LEATHER", "IMPACT",
        {"leather", "bang", "banging", "bump", "clap", "collide", "colliding", "cowhide", "crash", "crashing", "deerskin", "grab", "hide", "hit", "hitting", "impact", "impacting", "nubuck", "pigskin", "pound", "punch", "ram", "sheepskin", "slam", "slamming", "smack", "smacking", "strike", "striking", "suede", "thrust", "thud", "thump", "whack"}});
    m_mappings.push_back({"LEATHER", "MISC",
        {"leather", "misc", "animal", "cowhide", "deerskin", "goatskin", "hide", "lambskin", "miscellaneous", "nubuck", "pigskin", "rawhide", "sheepskin", "skin", "suede"}});
    m_mappings.push_back({"LEATHER", "MOVEMENT",
        {"leather", "bend", "cowhide", "deerskin", "flap", "flex", "flop", "hide", "maneuver", "movement", "nubuck", "pigskin", "rotate", "rustle", "sheepskin", "slide", "stretch", "suede", "sway", "swing", "turn", "twist"}});

    // LIQUID & MUD Category
    m_mappings.push_back({"LIQUID & MUD", "BUBBLES",
        {"liquid & mud", "boiling", "bubble", "bubbles", "burbling", "carbonation", "cavitation", "effervesce", "effervescence", "effervescent", "fizz", "foam", "froth", "frothy", "lather", "mud", "pop", "sparkle", "suds"}});
    m_mappings.push_back({"LIQUID & MUD", "IMPACT",
        {"liquid & mud", "hit", "impact", "kerplunk", "mud", "plop", "plunk", "slap", "slosh", "smack", "spatter", "splat", "splatter", "squelch", "squish"}});
    m_mappings.push_back({"LIQUID & MUD", "MISC",
        {"liquid & mud", "misc", "caramel", "flow", "gel", "glue", "gravy", "honey", "miscellaneous", "movement", "oil", "paint", "resin", "slime", "sludge", "syrup", "tar", "viscosity"}});
    m_mappings.push_back({"LIQUID & MUD", "MOVEMENT",
        {"liquid & mud", "movement", "flooding", "goo", "gushing", "leak", "mud", "ooze", "rushing", "seep", "slime", "sloshing", "splashing", "surging", "swirling"}});
    m_mappings.push_back({"LIQUID & MUD", "SUCTION",
        {"liquid & mud", "draw", "glomp", "mud", "plunger", "pull", "slurp", "suck", "suction", "vacuum"}});

    // MACHINES Category
    m_mappings.push_back({"MACHINES", "AMUSEMENT",
        {"machines", "amusement", "apparatus", "bumper", "carousel", "carrousel", "cars", "coaster", "contraption", "device", "drop", "ferris", "flume", "freefall", "funhouse", "haunted", "house", "kiddy", "line", "log", "machinery", "merry-go-round", "nickelodeon", "park", "pinball", "ride", "roller", "rollercoaster", "scrambler", "skeeball", "theme", "thrill", "tilt-a-whirl", "tower", "waterslide", "wheel", "zip"}});
    m_mappings.push_back({"MACHINES", "ANTIQUE",
        {"machines", "abacus", "adding", "antique", "apparatus", "arcade", "automaton", "babbage", "butter", "cash", "churn", "classic", "contraption", "device", "edison", "enigma", "guttenberg", "hand-cranked", "heritage", "historic", "linotype", "loom", "machine", "machinery", "mimeograph", "nostalgic", "old-fashioned", "olive", "press", "printing", "register", "retro", "sewing", "vintage"}});
    m_mappings.push_back({"MACHINES", "APPLIANCE",
        {"machines", "appliance", "air", "apparatus", "blender", "bread", "can", "cleaner", "coffee", "conditioner", "contraption", "cooker", "dehumidifier", "device", "dishwasher", "disposal", "dryer", "electric", "fan", "food", "fridge", "garbage", "griddle", "hair", "heater", "hot", "humidifier", "ice", "iron", "kettle", "machine", "machinery", "maker", "microwave", "mixer", "opener", "oven", "plate", "processor", "refrigerator", "rice", "sewing", "slow", "stand", "toaster", "vacuum", "waffle", "washing"}});
    m_mappings.push_back({"MACHINES", "CONSTRUCTION",
        {"machines", "construction", "apparatus", "cement", "chipper", "compactor", "concrete", "contraption", "crane", "device", "dozer", "driver", "generator", "jackhammer", "lift", "machinery", "mixer", "paver", "pile", "scissor", "steamroller", "trencher", "wood"}});
    m_mappings.push_back({"MACHINES", "ELEVATOR",
        {"machines", "elevator", "apparatus", "contraption", "device", "dumbwaiter", "dumbwaiters", "freight", "hoist", "lift", "lifts", "machinery", "passenger", "paternoster", "platform", "service", "stair", "turbolift"}});
    m_mappings.push_back({"MACHINES", "ESCALATOR",
        {"machines", "escalator", "apparatus", "contraption", "device", "machinery", "moving", "stairs", "travelator", "walkway", "walkways"}});
    m_mappings.push_back({"MACHINES", "FAN",
        {"machines", "fan", "air", "apparatus", "bladeless", "blower", "box", "ceiling", "circulator", "contraption", "device", "exhaust", "industrial", "machinery", "ventilation"}});
    m_mappings.push_back({"MACHINES", "GARDEN",
        {"machines", "garden", "apparatus", "blower", "chainsaw", "contraption", "cultivator", "device", "eater", "edger", "hedge", "irrigation", "landscaping", "lawn", "lawnmower", "leaf", "machinery", "mower", "mulcher", "pole", "pressure", "saw", "tiller", "tillers", "trimmer", "washer", "weed", "whacker"}});
    m_mappings.push_back({"MACHINES", "GYM",
        {"machines", "gym", "apparatus", "bike", "bowflex", "cardio", "climber", "contraption", "device", "elliptical", "exercise", "fitness", "machine", "machinery", "nautilus", "nordictrack", "resistance", "rower", "rowing", "stair", "stationary", "stepper", "trampoline", "treadmill", "weight", "workout"}});
    m_mappings.push_back({"MACHINES", "HITECH",
        {"machines", "hitech", "3d", "7", "apparatus", "arm", "bond", "cnc", "contraption", "cutter", "device", "digital", "gadget", "handheld", "holographic", "james", "laser", "machinery", "plasma", "printer", "quantum", "resin", "robotic", "spy"}});
    m_mappings.push_back({"MACHINES", "HVAC",
        {"machines", "hvac", "air", "aircon", "apparatus", "baseboard", "boiler", "chiller", "climate", "conditioner", "conditioning", "contraption", "control", "cooler", "dehumidifier", "device", "ductless", "evaporative", "filtration", "furnace", "handlers", "heat", "heater", "heating", "humidifier", "humidifiers", "machinery", "mini-split", "pump", "pumps", "purifier", "radiator", "systems", "thermostat", "thermostats", "ventilation"}});
    m_mappings.push_back({"MACHINES", "INDUSTRIAL",
        {"machines", "apparatus", "assembly", "auto", "automation", "contraption", "conveyor", "cutting", "device", "die", "distillery", "duty", "factory", "form", "heavy", "hydraulic", "industrial", "injection", "line", "machine", "machinery", "manufacturing", "milling", "molding", "plant", "press", "printing", "production", "punch", "reactor", "robotics", "robots", "sawmill", "shearing", "smelter", "smeltery", "stamper", "system"}});
    m_mappings.push_back({"MACHINES", "MECHANISM",
        {"machines", "mechanism", "7", "apparatus", "bond", "box", "contraption", "device", "gadget", "gizmo", "goldberg", "james", "machinery", "puzzle", "rube", "spy"}});
    m_mappings.push_back({"MACHINES", "MEDICAL",
        {"machines", "medical", "apparatus", "blood", "cat", "centrifuge", "concentrator", "contraption", "ct", "defibrillator", "dental", "device", "dialysis", "drill", "ecg", "ekg", "glucometers", "heart", "infusion", "insulin", "lab", "life", "machine", "machinery", "monitor", "mri", "nebulizer", "oxygen", "pacemaker", "pet", "pressure", "pump", "pumps", "scan", "support", "test", "ultrasound", "ventilator", "x-ray"}});
    m_mappings.push_back({"MACHINES", "MISC",
        {"machines", "misc", "apparatus", "contraption", "device", "machinery", "miscellaneous"}});
    m_mappings.push_back({"MACHINES", "OFFICE",
        {"machines", "apparatus", "binding", "business", "clock", "contraption", "copier", "copy", "cutter", "device", "dictaphone", "envelope", "facsimile", "fax", "inkjet", "interactive", "label", "laminator", "laser", "letter", "machine", "machinery", "maker", "mimeograph", "office", "opener", "paper", "photocopier", "plotter", "postal", "printer", "scale", "scanner", "shredder", "stenotype", "telefax", "time", "typewriter", "whiteboard", "xerox"}});
    m_mappings.push_back({"MACHINES", "PUMP",
        {"machines", "aerator", "air", "apparatus", "backflow", "blower", "boilers", "centrifugal", "compressor", "contraption", "device", "fuel", "gas", "heat", "impellers", "inflators", "injector", "injects", "jet", "machinery", "piston", "plunger", "pump", "pumper", "pumping", "septic", "siphon", "sprinkler", "submersible", "sump", "syringe", "upflow", "vacuum", "valves", "water", "well", "wells"}});

    // MAGIC Category
    m_mappings.push_back({"MAGIC", "ANGELIC",
        {"magic", "angelic", "appearance", "aura", "beatific", "blessed", "blessedness", "blessings", "bliss", "celestial", "cherub", "creation", "divine", "ethereal", "god", "guidance", "healing", "heavenly", "holy", "presence", "protection", "realm", "righteous", "sacred", "saint", "saintly", "seraph", "seraphic", "spiritual", "supernatural"}});
    m_mappings.push_back({"MAGIC", "ELEMENTAL",
        {"magic", "elemental", "air", "alchemic", "alchemical", "alchemy", "arcane", "earth", "electric", "energy", "fire", "fundamental", "natural", "nature", "planar", "primal", "primeval", "primordial", "spirits", "symbols", "transformation", "water", "wind"}});
    m_mappings.push_back({"MAGIC", "EVIL",
        {"magic", "evil", "bad", "bewitched", "black", "corrupt", "cruel", "dark", "demogorgon", "demonic", "demonifuge", "demonomagy", "depraved", "devil", "devilry", "deviltry", "diablerie", "diabolical", "diabolism", "fiendish", "forbidden", "hag", "hell", "hellbroth", "immoral", "infernal", "loki", "maleficent", "malevolent", "malicious", "malignant", "necromancy", "necronomicon", "nefarious", "nightmare", "occult", "pentacle", "satan", "satanic", "sinful", "sinister", "summon", "unholy", "vile", "villainous", "voodoo", "warlock", "wicked", "witchcraft"}});
    m_mappings.push_back({"MAGIC", "MISC",
        {"magic", "misc", "alchemic", "alchemical", "alchemistic", "alchemy", "amulet", "artifact", "bewitchment", "ceremony", "charming", "conjurer", "conjuring", "elixir", "enchanting", "enchantments", "faerie", "fairie", "fairies", "fairylands", "fairytale", "incantational", "incantations", "magician", "magicians", "magick", "magickal", "magicks", "magique", "miracle", "miracles", "miraculous", "miscellaneous", "mystic", "mysticism", "mystique", "potion", "realm", "ritual", "sorcerous", "sorcery", "spellcraft", "spells", "symbolism", "trick", "trickery", "tricks", "wand", "wiz", "wizardly"}});
    m_mappings.push_back({"MAGIC", "POOF",
        {"magic", "abracadabra", "alakazam", "appearance", "bibbidi-bobbidi-boo", "bim", "blast", "chuff", "disappearance", "foomp", "hocus-pocus", "open", "poof", "presto", "sala", "sesame", "shazam", "sim", "ta-da", "transformation", "transmutation", "vanish", "voila"}});
    m_mappings.push_back({"MAGIC", "SHIMMER",
        {"magic", "shimmer", "aura", "bell", "chime", "gleam", "glimmer", "glimmering", "glint", "glisten", "glitter", "glittering", "gloss", "glow", "glowing", "incandescent", "light", "luminous", "luster", "radiant", "radiate", "reflection", "scintillate", "scintillation", "sheen", "shimmering", "shine", "shiny", "sparkle", "sparkling", "tree", "twinkle", "twinkling"}});
    m_mappings.push_back({"MAGIC", "SPELL",
        {"magic", "spell", "abracadabra", "banishing", "bewitch", "cast", "charm", "conjuration", "conjure", "conjured", "conjuring", "curse", "divination", "enchant", "enchanting", "enchantment", "enchantments", "evocation", "exorcism", "healing", "hex", "illusion", "illusionary", "illusionism", "illusionistic", "illusions", "incantation", "incantational", "incantations", "incantatory", "invocation", "invocations", "mesmerize", "prestidigitation", "protection", "rune", "sorcerer", "sorceries", "sorcerous", "sorcery", "spellacy", "spellbind", "spellbinding", "spellcheck", "spelled", "spelling", "spellings", "spells", "spellwork", "summoned"}});

    // MECHANICAL Category
    m_mappings.push_back({"MECHANICAL", "CLICK",
        {"mechanical", "button", "clack", "click", "clicker", "snap", "tick", "toggle"}});
    m_mappings.push_back({"MECHANICAL", "GEARS",
        {"mechanical", "gears", "axle", "bevel", "cam", "chain", "cluster", "cog", "cogs", "cogwheels", "coupling", "crown", "derailleur", "differential", "drive", "flywheel", "flywheels", "gear", "gearbox", "geared", "gearing", "geartrain", "helical", "herringbone", "hypoid", "idler", "mesh", "meshing", "pinion", "pinions", "planetary", "rack", "reducer", "shaft", "shifter", "silent", "spiral", "sprocket", "sprockets", "spur", "straight-cut", "synchromesh", "tooth", "transmission", "wheels", "worm"}});
    m_mappings.push_back({"MECHANICAL", "HYDRAULIC & PNEUMATIC",
        {"mechanical", "hydraulic & pneumatic", "actuator", "brake", "clutch", "compressor", "conveyor", "cylinder", "damper", "hydraulic", "jack", "pneumatic", "press", "pressurized", "ram", "system", "valve"}});
    m_mappings.push_back({"MECHANICAL", "LATCH",
        {"mechanical", "barrel", "bolt", "cabinet", "cam", "catch", "clasp", "deadlatch", "drawer", "fastener", "gate", "hardware", "hasp", "hook", "interlock", "latch", "lock", "magnetic", "night", "safety", "slam", "slide", "spring", "thumb", "toggle", "toolbox", "window"}});
    m_mappings.push_back({"MECHANICAL", "LEVER",
        {"mechanical", "brake", "cam", "clank", "clunk", "control", "joystick", "lever", "locking", "pedal", "push-pull", "quick-release", "rocker", "switch", "trigger"}});
    m_mappings.push_back({"MECHANICAL", "LOCK",
        {"mechanical", "biometric", "bolt", "cam", "catch", "clasp", "combination", "cylinder", "deadbolt", "electronic", "fastener", "hasp", "keyless", "latch", "lock", "mortise", "padlock", "pin", "security", "tumbler"}});
    m_mappings.push_back({"MECHANICAL", "MISC", {"mechanical", "misc", "miscellaneous"}});
    m_mappings.push_back({"MECHANICAL", "PULLEY",
        {"mechanical", "and", "belt", "block", "chain", "chirp", "hoist", "pulley", "pulley-block", "roll", "rope", "sheave", "squeak", "tackle", "winch"}});
    m_mappings.push_back({"MECHANICAL", "RATCHET",
        {"mechanical", "click", "crank", "detent", "pawl", "ratchet", "snap", "socket", "strap", "tie-down", "winch", "wind", "wrench"}});
    m_mappings.push_back({"MECHANICAL", "RELAY",
        {"mechanical", "relay", "arc", "breaker", "clack", "coil", "contact", "control", "electromagnetic", "flip", "lamp", "solenoid", "switch", "toggle"}});
    m_mappings.push_back({"MECHANICAL", "ROLLER",
        {"mechanical", "roller", "bearing", "belt", "dynamo", "line", "roll", "shaft"}});
    m_mappings.push_back({"MECHANICAL", "SWITCH",
        {"mechanical", "button", "circuit", "clack", "click", "dimmer", "light", "limit", "micro", "push-button", "rocker", "rotary", "safety", "selector", "slide", "switch", "switchgear", "toggle", "wall"}});

    // METAL Category
    m_mappings.push_back({"METAL", "BREAK",
        {"metal", "aluminum", "apart", "beryllium", "brass", "break", "bronze", "burst", "bust", "cadmium", "chip", "chromium", "cleave", "cobalt", "copper", "crack", "crumble", "crunch", "crush", "demolish", "destroy", "disintegrate", "fracture", "fragment", "gallium", "gold", "indium", "iron", "lead", "lithium", "magnesium", "manganese", "mercury", "molybdenum", "nickel", "niobium", "palladium", "platinum", "rend", "rhenium", "rip", "rupture", "separate", "shatter", "silver", "smash", "snap", "splinter", "split", "steel"}});
    m_mappings.push_back({"METAL", "CRASH & DEBRIS",
        {"metal", "crash & debris", "aluminum", "beryllium", "brass", "bronze", "cadmium", "chromium", "clang", "clatter", "cobalt", "collision", "copper", "crash", "debris", "fall", "fragments", "gallium", "gold", "indium", "iron", "lead", "lithium", "magnesium", "manganese", "mercury", "molybdenum", "nickel", "niobium", "palladium", "platinum", "remains", "rhenium", "rubble", "ruins", "shards", "shrapnel", "silver", "smash", "steel", "tin", "titanium", "tungsten", "vanadium.", "wreckage", "zinc"}});
    m_mappings.push_back({"METAL", "FRICTION",
        {"metal", "friction", "abrade", "abrasion", "aluminum", "beryllium", "brass", "bronze", "cadmium", "chafe", "chromium", "cobalt", "copper", "creak", "gallium", "gnash", "gold", "grind", "grinding", "indium", "iron", "lead", "lithium", "magnesium", "manganese", "mercury", "molybdenum", "nickel", "niobium", "palladium", "platinum", "rasp", "rasping", "rhenium", "rub", "rubbing", "scour", "scrape", "scraping", "scratching", "screech", "scuffing", "silver", "sliding", "squeak", "steel", "stress", "tin", "titanium", "tungsten"}});
    m_mappings.push_back({"METAL", "HANDLE",
        {"metal", "aluminum", "beryllium", "brass", "bronze", "cadmium", "catch", "chromium", "clasp", "clench", "clutch", "cobalt", "copper", "down", "embrace", "gallium", "gold", "grab", "grasp", "grip", "handle", "handlebar", "hold", "indium", "iron", "knob", "latch", "lead", "lever", "lithium", "magnesium", "manganese", "mercury", "molybdenum", "nickel", "niobium", "operate", "palladium", "palm", "pickup", "platinum", "pull", "rhenium", "seize", "set", "silver", "steel", "take", "throw", "tin"}});
    m_mappings.push_back({"METAL", "IMPACT",
        {"metal", "aluminum", "bang", "banging", "bash", "beryllium", "brass", "bronze", "bump", "cadmium", "chromium", "clang", "clap", "clunk", "cobalt", "collide", "colliding", "copper", "crash", "crashing", "drop", "dropped", "gallium", "gold", "hit", "hitting", "impact", "impacting", "indium", "iron", "jolt", "knock", "lead", "lithium", "magnesium", "manganese", "mercury", "molybdenum", "nickel", "niobium", "palladium", "platinum", "pound", "ram", "rhenium", "ring", "silver", "slam", "slamming", "smack"}});
    m_mappings.push_back({"METAL", "MISC",
        {"metal", "misc", "aluminum", "beryllium", "brass", "bronze", "cadmium", "chromium", "cobalt", "copper", "gallium", "gold", "indium", "iron", "lead", "lithium", "magnesium", "manganese", "mercury", "miscellaneous", "molybdenum", "nickel", "niobium", "palladium", "platinum", "rhenium", "silver", "steel", "tin", "titanium", "tungsten", "vanadium.", "zinc"}});
    m_mappings.push_back({"METAL", "MOVEMENT",
        {"metal", "movement", "aluminum", "beryllium", "brass", "bronze", "cadmium", "chattering", "chromium", "clatter", "cobalt", "copper", "drag", "gallium", "glide", "gold", "indium", "iron", "jangle", "lead", "lithium", "magnesium", "manganese", "mercury", "molybdenum", "nickel", "niobium", "palladium", "platinum", "rattle", "rattling", "rhenium", "roll", "rolling", "scuffle", "shake", "shaking", "shift", "shuffle", "silver", "slide", "slip", "steel", "sway", "swing", "tin", "titanium", "tungsten", "vanadium.", "vibrating"}});
    m_mappings.push_back({"METAL", "TONAL",
        {"metal", "aluminum", "beryllium", "bowed", "brass", "bronze", "cadmium", "chromium", "cobalt", "copper", "frequency", "gallium", "gold", "harmonic", "indium", "iron", "lead", "lithium", "magnesium", "manganese", "melodic", "melodious", "mercury", "molybdenum", "musical", "nickel", "niobium", "palladium", "ping", "pitch", "platinum", "resonance", "resonant", "rhenium", "ring", "shing", "silver", "sonorous", "sound", "steel", "timbre", "tin", "titanium", "tonal", "tone", "tungsten", "vanadium.", "zinc"}});

    // MOTORS Category
    m_mappings.push_back({"MOTORS", "ANTIQUE",
        {"motors", "antique", "aged", "ancient", "antiquated", "bygone", "classic", "engine", "historic", "kerosene", "old-fashioned", "paraffin", "retro", "steam", "traditional", "vintage", "windmill"}});
    m_mappings.push_back({"MOTORS", "COMBUSTION",
        {"motors", "backfire", "carburation", "carburetor", "combustion", "combustor", "compression", "diesel", "engine", "ethanol", "four-stroke", "gas", "gasoline", "generator", "internal", "manifold", "mechanic", "misfire", "motor", "outboard", "petrol", "rotary", "turboshaft", "two-stroke"}});
    m_mappings.push_back({"MOTORS", "ELECTRIC",
        {"motors", "electric", "ac", "dc", "dentist", "disposal", "drill", "dynamotor", "engine", "garbage", "magneto", "motor", "razor", "stepper", "toothbrush", "volt", "watt", "wheelchair"}});
    m_mappings.push_back({"MOTORS", "MISC", {"motors", "misc", "engine", "miscellaneous"}});
    m_mappings.push_back({"MOTORS", "SERVO",
        {"motors", "servo", "3d", "actuator", "antenna", "camera", "car", "drive", "focus", "motor", "printer", "rc", "robot"}});
    m_mappings.push_back({"MOTORS", "TURBINE",
        {"motors", "turbine", "aircraft", "airplane", "engine", "generator", "hydro", "steam", "turbomachinery", "wind", "windmill"}});

    // MOVEMENT Category
    m_mappings.push_back({"MOVEMENT", "ACTIVITY",
        {"movement", "activity", "cleaning", "moving", "shopping", "unpacking"}});
    m_mappings.push_back({"MOVEMENT", "ANIMAL",
        {"movement", "animal", "charge", "flock", "herd", "migrate", "migration", "pack", "scramble", "stampede", "swarm", "trample"}});
    m_mappings.push_back({"MOVEMENT", "CREATURE",
        {"movement", "creature", "burrowing", "coil", "contort", "crawl", "crawling", "digging", "lunge", "slide", "slither", "squirm", "wriggle", "writhe"}});
    m_mappings.push_back({"MOVEMENT", "CROWD",
        {"movement", "crowd", "amble", "jump", "lie", "march", "mill", "mobilize", "mobilizing", "moves", "pilgrimage", "rout", "run", "scuffle", "shuffle", "sit", "skip", "slip", "stand", "swarm", "trek", "troop", "walk"}});
    m_mappings.push_back({"MOVEMENT", "HUMAN",
        {"movement", "human", "activity", "climb", "crawl", "dance", "exercise", "lie", "mill", "perform", "physical", "scuffle", "shuffle", "sit", "slip", "stand"}});
    m_mappings.push_back({"MOVEMENT", "INSECT",
        {"movement", "insect", "ants", "beehive", "centipede", "crawl", "crawling", "earwig", "hive", "maggot", "maggots", "midge", "millipede", "mite", "pupa", "roach", "skittering", "slither", "spider", "squirm", "swarm", "tsetse", "writhe"}});
    m_mappings.push_back({"MOVEMENT", "MISC", {"movement", "misc", "miscellaneous", "moves"}});
    m_mappings.push_back({"MOVEMENT", "PRESENCE",
        {"movement", "presence", "adjust", "fidget", "light", "milling", "restless", "scuffle", "settle", "shiffling", "shift", "shuffle"}});

    // MUSICAL Category
    m_mappings.push_back({"MUSICAL", "BELLS",
        {"musical", "agogo", "agung", "babendil", "belldom", "bells", "bianzhong", "bonsho", "bourdon", "bowl", "campanology", "carillon", "cowbell", "gamelan", "gong", "hand", "handbells", "nola", "peal", "ring", "singing", "tam-tam", "tintinnabulum", "tubular"}});
    m_mappings.push_back({"MUSICAL", "BRASS",
        {"musical", "brass", "brassy", "bugle", "cornet", "euphonium", "euphorium", "flugelhorn", "french", "horn", "mellophone", "orchestra", "sax", "saxhorn", "sousaphone", "trombone", "trumpet", "tuba"}});
    m_mappings.push_back({"MUSICAL", "CHIME",
        {"musical", "bell", "chime", "chimes", "chiming", "jingle", "shimmer", "tinkle", "tintinnabulation", "tree", "twinkle", "wind"}});
    m_mappings.push_back({"MUSICAL", "CHORAL",
        {"musical", "acappella", "aria", "cantata", "cantor", "cantus", "choir", "choirmaster", "choral", "chorale", "chorus", "church", "ensemble", "glee", "gospel", "harmony", "hymn", "madrigal", "motet", "singer", "singers", "singing", "solo", "soprano", "tenor", "unison", "vocal", "vocalist", "vocals"}});
    m_mappings.push_back({"MUSICAL", "EXPERIMENTAL",
        {"musical", "abstract", "aeolian", "armonica", "artsy", "avant-garde", "baschet", "cactus", "cristal", "electrified", "experimental", "exploratory", "glass", "harp", "innovative", "music", "pyrophone", "radical", "stylophone", "unconventional", "wind"}});
    m_mappings.push_back({"MUSICAL", "INSTRUMENT", {"musical", "instrument"}});
    m_mappings.push_back({"MUSICAL", "KEYED",
        {"musical", "keyed", "accordion", "celesta", "clavichord", "clavinet", "dulcitone", "electric", "fortepiano", "harmonium", "harpsichord", "keyboard", "melodica", "organ", "organetto", "piano", "pianoforte", "regal", "rhodes", "spinet", "virginal", "wurlitzer"}});
    m_mappings.push_back({"MUSICAL", "LOOP",
        {"musical", "drum", "loop", "looped", "loops", "music", "repeat", "repetition", "sample", "sampled", "sounds"}});
    m_mappings.push_back({"MUSICAL", "MISC", {"musical", "misc", "harmonica", "kazoo", "miscellaneous"}});
    m_mappings.push_back({"MUSICAL", "PERCUSSION",
        {"musical", "bass", "beat", "block", "bongo", "cajon", "claves", "conga", "drum", "drumhead", "drumming", "drums", "frame", "kettle", "kettledrum", "kick", "ocean", "percussion", "percussionist", "rhythm", "set", "snare", "taiko", "timpani", "tom", "toms", "tympani", "war", "wood"}});
    m_mappings.push_back({"MUSICAL", "PERCUSSION TUNED",
        {"musical", "percussion tuned", "balaphone", "bell", "belldom", "bianzhong", "celesta", "crotales", "drum", "glass", "glockenspiel", "handpan", "hang", "harmonica", "kalimba", "lithophone", "lyre", "mallet", "marimba", "marimbaphone", "metallophone", "saw", "steel", "vibraphone", "xylophone"}});
    m_mappings.push_back({"MUSICAL", "PERFORMANCE",
        {"musical", "act", "auditions", "ballet", "band", "bandleader", "classical", "compositions", "concert", "concertos", "concerts", "duet", "event", "gig", "live", "musicals", "musician", "musicians", "nightclub", "orchestra", "orchestral", "performance", "performances", "performer", "performs", "philharmonic", "play", "production", "recital", "recitalist", "recitals", "show", "singers", "songwriting", "spectacle", "stage", "stageplay", "street", "symphonic", "symphony", "theatrically", "vocalists"}});
    m_mappings.push_back({"MUSICAL", "PLUCKED",
        {"musical", "plucked", "acoustic", "autoharp", "balalaika", "bandore", "bandura", "banjo", "bass", "bouzouki", "cither", "dulcimer", "guitar", "guzheng", "harp", "harpist", "jaw", "kalimba", "komuz", "kora", "koto", "lute", "lyre", "mandolin", "oud", "piano", "pipa", "pizzicato", "plectrum", "psaltery", "qanun", "rebab", "rota", "samisen", "saz", "shamisen", "sitar", "tambura", "tanbur", "thumb", "twang", "uke", "ukulele", "zither"}});
    m_mappings.push_back({"MUSICAL", "SAMPLE",
        {"musical", "sample", "audio", "bit", "bite", "clip", "excerpt", "fragment", "one-shot", "portion", "raw", "samples", "section", "slice", "snippet", "sound"}});
    m_mappings.push_back({"MUSICAL", "SHAKEN",
        {"musical", "shaken", "cabasa", "castanets", "chajchas", "egg", "gourd", "guiro", "katsa", "maraca", "maracas", "monkey", "rain", "rainstick", "rattles", "seed", "shaker", "shekere", "sistrum", "stick", "tambourine"}});
    m_mappings.push_back({"MUSICAL", "SONG & PHRASE",
        {"musical", "song & phrase", "anthem", "aria", "arrangement", "by", "car", "chorus", "composition", "harmony", "lyric", "lyrical", "melody", "music", "part", "passage", "phrase", "piano", "player", "refrain", "riff", "ringtone", "segment", "singing", "song", "strain", "tune", "verse"}});
    m_mappings.push_back({"MUSICAL", "STINGER",
        {"musical", "stinger", "accent", "fanfare", "flourish", "interjection", "jingle", "music", "news", "punctuation", "stab", "sting", "title"}});
    m_mappings.push_back({"MUSICAL", "STRINGED",
        {"musical", "stringed", "adagio", "bass", "bowed", "cello", "dahu", "double", "erhu", "fiddle", "hardanger", "hurdy-gurdy", "nyckelharpa", "rebec", "viola", "violin", "violoncello"}});
    m_mappings.push_back({"MUSICAL", "SYNTHESIZED",
        {"musical", "synthesized", "analog", "arp", "casio", "controller", "digital", "drum", "electronic", "keyboard", "korg", "machine", "midi", "minimoog", "modular", "moog", "oberheim", "roland", "sequencer", "synth", "synthesizer", "theremin", "virtual", "vocoder", "yamaha"}});
    m_mappings.push_back({"MUSICAL", "TOY", {"musical", "toy", "box", "jack-in-the-box", "music", "musicbox"}});
    m_mappings.push_back({"MUSICAL", "WOODWIND",
        {"musical", "woodwind", "bagpipe", "bansuri", "bass", "basset", "bassoon", "clarinet", "contrabassoon", "cornett", "duduk", "english", "fife", "flageolet", "flute", "hautboy", "horn", "kaval", "krumhorn", "ney", "oboe", "pan", "piccolo", "quena", "recorder", "sarrusophone", "saxophone", "shakuhachi", "suona", "tarogato", "xaphoon", "xiao"}});

    // NATURAL DISASTER Category
    m_mappings.push_back({"NATURAL DISASTER", "AVALANCHE",
        {"natural disaster", "avalanche", "cascades", "crevasse", "earth", "earthslide", "icefall", "icefalls", "landslide", "landslides", "landsliding", "landslip", "lavafall", "mudflow", "mudslide", "rockfall", "rockfalls", "rockslide", "rockslides", "slide", "snow"}});
    m_mappings.push_back({"NATURAL DISASTER", "EARTHQUAKE",
        {"natural disaster", "earthquake", "activity", "aftershock", "aftershocks", "disasters", "earth", "epicenter", "event", "fissure", "foreshock", "foreshocks", "ground", "hypocenter", "hypocentre", "lfe", "quake", "quakes", "rattle", "rumble", "seism", "seismic", "seismism", "seismogram", "seismograms", "seismograph", "seismographic", "seismographs", "seismological", "seismologist", "seismologists", "seismology", "seismometer", "shake", "shakes", "shock", "tectonic", "temblor", "temblors", "tremor", "tremoring", "tremors"}});
    m_mappings.push_back({"NATURAL DISASTER", "MISC",
        {"natural disaster", "misc", "act", "apocalypse", "armageddon", "calamity", "cataclysm", "catastrophe", "disaster", "doomsdate", "doomsday", "drought", "famine", "flood", "force", "god", "majeure", "megastorm", "meltdown", "miscellaneous", "of", "superstorm"}});
    m_mappings.push_back({"NATURAL DISASTER", "TORNADO",
        {"natural disaster", "tornado", "cloud", "cyclone", "devil", "downburst", "downbursts", "dust", "duststorm", "duststorms", "eyewall", "funnel", "microburst", "supercell", "twister", "twisters", "updraft", "vortex", "waterspout", "waterspouts", "whirlwind", "whirlwinds", "windstorm"}});
    m_mappings.push_back({"NATURAL DISASTER", "TSUNAMI",
        {"natural disaster", "catastrophic", "earthquake", "flood", "floods", "giant", "mega", "megatsunami", "sea", "sunami", "supertide", "surge", "tidal", "tsunami", "tsunamigenic", "underwater", "wave"}});
    m_mappings.push_back({"NATURAL DISASTER", "TYPHOON",
        {"natural disaster", "typhoon", "cyclone", "cyclones", "cyclonic", "eyewall", "flood", "floods", "hurricane", "hurricanes", "monsoon", "monsoonal", "monsoons", "severe", "storm", "storm.", "storms", "strong", "surge", "tempest", "tropical", "tyfoon", "typhon", "wind", "windstorm"}});
    m_mappings.push_back({"NATURAL DISASTER", "VOLCANO",
        {"natural disaster", "volcano", "ash", "bombs", "caldera", "calderas", "chasma", "cloud", "crater", "cryovolcano", "erupting", "eruption", "eruptions", "eruptive", "flow", "fumarole", "geothermic", "kilauea", "lahar", "lava", "lavas", "magma", "magmasphere", "magmatic", "mantle", "pinatubo", "pyroclastic", "pyroclastics", "pyroclasts", "rhyolitic", "stratovolcano", "strombolian", "subvolcanic", "tephra", "volcanic", "volcanically", "volcanics", "volcanism", "volcanogenic", "volcanological", "volcanologist", "volcanologists", "volcanology", "vulcan", "vulcanian", "vulcanic", "vulcanism", "vulcano", "vulcanoid", "vulcanology"}});

    // OBJECTS Category
    m_mappings.push_back({"OBJECTS", "BAG",
        {"objects", "bag", "back", "backpack", "briefcase", "camera", "carryall", "clutch", "courier", "drawstring", "duffel", "fanny", "handbag", "knapsack", "messenger", "pack", "pocketbook", "portfolio", "pouch", "purse", "rucksack", "sac", "sachet", "sack", "saddlebag", "satchel", "schoolbag", "shoulder", "sling", "tote"}});
    m_mappings.push_back({"OBJECTS", "BOOK",
        {"objects", "autobiography", "bible", "biography", "book", "bookkeeper", "bookkeeping", "booklet", "bookshop", "bookstore", "brochure", "catalog", "catalogue", "chapter", "codex", "compendium", "compilation", "cookbook", "cover", "daybook", "diary", "dictionary", "directory", "edition", "encyclopedia", "fiction", "guidebook", "handbook", "hardcover", "inventory", "journal", "ledger", "leger", "letters", "literary", "literature", "magazine", "manuscript", "memoir", "non-fiction", "notebook", "novel", "page", "pamphlet", "paperback", "portfolio", "publication", "publishing", "read", "reading"}});
    m_mappings.push_back({"OBJECTS", "COIN",
        {"objects", "coin", "bank", "cent", "change", "clink", "coinage", "currency", "dime", "dollar", "doubloon", "dump", "euro", "flip", "half-dollar", "loose", "money", "nickel", "penny", "piggy", "ping", "pound", "quarter", "rouble", "rupee", "shekel", "silver", "spin", "token", "toss", "yen"}});
    m_mappings.push_back({"OBJECTS", "CONTAINER",
        {"objects", "container", "aquarium", "baggie", "barrel", "basket", "bin", "box", "breadbox", "canister", "canisters", "capsule", "cardboard", "carton", "case", "casket", "chest", "cistern", "contain", "crate", "dispenser", "dumpster", "enclosure", "flask", "holder", "jar", "jars", "jewelry", "lid", "locker", "package", "pill", "plastic", "receptacle", "receptacles", "reliquary", "toolbox", "tray", "trunk", "tupperware", "vase", "vial"}});
    m_mappings.push_back({"OBJECTS", "FASHION",
        {"objects", "fashion", "apparel", "attire", "bandana", "belt", "boots", "brush", "clothing", "clutch", "comb", "couture", "eyeliner", "garment", "glasses", "gloves", "handkerchief", "hat", "headband", "high-heels", "lipstick", "makeup", "mascara", "muffs", "outfit", "robe", "sash", "scarf", "shawl", "shoes", "sneakers", "socks", "sunglasses", "suspenders", "ties", "wallet", "wardrobe", "watches"}});
    m_mappings.push_back({"OBJECTS", "FURNITURE",
        {"objects", "furniture", "antiques", "armoires", "bed", "bedframe", "bench", "bookcase", "bookcases", "bookshelf", "buffet", "cabinet", "cabinets", "chair", "chest", "coffee", "console", "couch", "couches", "credenza", "crib", "desk", "dining", "dresser", "dressers", "end", "footlocker", "futon", "hutch", "loveseat", "mattress", "mattresses", "nightstand", "ottoman", "rocking", "rugs", "shelf", "shelving", "sideboard", "sofa", "sofas", "stool", "table", "upholstery", "wardrobe"}});
    m_mappings.push_back({"OBJECTS", "GARDEN",
        {"objects", "garden", "bath", "bin", "bird", "can", "compost", "composter", "faucet", "feeder", "flower", "gloves", "greenhouse", "hose", "killer", "labels", "plant", "pot", "sculpture", "stake", "stakes", "trellis", "watering", "weed"}});
    m_mappings.push_back({"OBJECTS", "GYM",
        {"objects", "gym", "ball", "band", "bar", "barbell", "bench", "boxing", "dumbbell", "exercise", "fitness", "foam", "gloves", "jump", "kettlebell", "mat", "medicine", "pull-up", "rack", "resistance", "roller", "rope", "stepper", "weight", "yoga"}});
    m_mappings.push_back({"OBJECTS", "HOUSEHOLD",
        {"objects", "household", "broom", "can", "cleaning", "domestic", "home", "house", "mirror", "mop", "rug", "soap", "sponge", "supplies", "trash"}});
    m_mappings.push_back({"OBJECTS", "JEWELRY",
        {"objects", "jewelry", "accessories", "adornment", "adornments", "anklet", "bangles", "baubles", "bead", "beaded", "beads", "beadwork", "bling", "bracelet", "bracelets", "bridal", "brooch", "brooches", "cabochons", "charm", "choker", "crown", "cufflink", "cufflinks", "diamond", "diamonds", "earing", "earring", "earrings", "gem", "gemstone", "gemstones", "hairpin", "jewel", "jeweled", "jeweler", "jewelers", "jewelery", "jeweller", "jewellery", "jewels", "locket", "necklace", "necklaces", "nose", "ornament", "ornaments", "pendant", "pendants", "pin"}});
    m_mappings.push_back({"OBJECTS", "KEYS",
        {"objects", "bicycle", "car", "card", "fob", "house", "key", "keychain", "keyes", "keyholes", "keylock", "keyring", "keys", "keyset", "lanyard", "lock", "locker", "mailbox", "master", "office", "padlock", "safe", "skeleton", "unlock", "unlocking", "unlocks"}});
    m_mappings.push_back({"OBJECTS", "LUGGAGE",
        {"objects", "backpack", "bag", "baggage", "baggages", "bagroom", "bellhop", "carry-on", "carryon", "checked", "cubes", "duffel", "duffels", "duffle", "garment", "knapsack", "knapsacks", "luggage", "packing", "rolling", "rucksack", "samsonite", "satchel", "scale", "skycap", "stow", "stowing", "suitcase", "suitcases", "tag", "toiletry", "trolley", "trunks"}});
    m_mappings.push_back({"OBJECTS", "MEDICAL",
        {"objects", "medical", "aid", "bandage", "blood", "brace", "cast", "clamp", "crutches", "cuff", "epipen", "forceps", "gauss", "gauze", "gloves", "hearing", "mask", "meter", "pills", "pressure", "rubber", "scalpel", "specimen", "stethoscope", "surgical", "swab", "syringe", "thermometer"}});
    m_mappings.push_back({"OBJECTS", "MISC", {"objects", "misc", "miscellaneous"}});
    m_mappings.push_back({"OBJECTS", "OFFICE",
        {"objects", "binder", "business", "calculator", "calendar", "can", "card", "chair", "cutter", "desk", "envelope", "folder", "hole", "notepad", "office", "paper", "paperweight", "post-it", "punch", "report", "scissors", "stapler", "supplies", "trash", "tray", "whiteboard"}});
    m_mappings.push_back({"OBJECTS", "PACKAGING",
        {"objects", "packaging", "bag", "box", "bubble", "cardboard", "corrugated", "crate", "envelope", "foam", "label", "labeling", "labelling", "labels", "mailing", "overwrap", "package", "packagings", "packed", "packing", "padding", "pallet", "paper", "parcel", "peanuts", "shipment", "shipping", "shrink", "strapping", "stretch", "styrofoam", "unwrapping", "wrap", "wrapper", "wrappers", "wrapping", "wrappings"}});
    m_mappings.push_back({"OBJECTS", "TAPE",
        {"objects", "tape", "adhesive", "cellophane", "double", "double-sided", "duct", "electrical", "flypaper", "gaffer", "glue", "masking", "medical", "packing", "scotch", "sellotape", "stick", "sticky", "transparent", "velcro"}});
    m_mappings.push_back({"OBJECTS", "UMBRELLA",
        {"objects", "beach", "brolly", "broly", "bumbershoot", "canopy", "compact", "gamp", "golf", "pagoda", "parasol", "patio", "rain", "sunshade", "travel", "umbrella"}});
    m_mappings.push_back({"OBJECTS", "WHEELED",
        {"objects", "wheeled", "baby", "barrow", "caddie", "carriage", "cart", "caster", "dolly", "gurney", "hand", "handcart", "luggage", "perambulator", "pram", "pushcart", "rolled", "rolling", "shopping", "skateboard", "stroller", "suitcase", "truck", "wagon", "walker", "wheelbarrow", "wheelchair"}});
    m_mappings.push_back({"OBJECTS", "WRITING",
        {"objects", "writing", "blotter", "blotting", "board", "calligraphy", "chalk", "charcoal", "crayon", "drafting", "drawing", "dry", "erase", "eraser", "fountain", "handwriting", "highlighter", "ink", "inscription", "lettering", "letters", "marker", "pen", "pencil", "penned", "penning", "print", "printed", "quill", "script", "sharpie", "signature", "spelled", "spelling", "writer", "written"}});
    m_mappings.push_back({"OBJECTS", "ZIPPER",
        {"objects", "bag", "closure", "closures", "fastener", "jacket", "pants", "slider", "tent", "zip", "zipper", "zips"}});

    // PAPER Category
    m_mappings.push_back({"PAPER", "FLUTTER",
        {"paper", "cardboard", "confetti", "crinkling", "fall", "flap", "flapping", "flittering", "flutter", "fluttering", "kraft", "manila", "papyrus", "parchment", "riffle", "ruffle", "rustle", "rustling", "shake", "tissue", "vellum", "waving"}});
    m_mappings.push_back({"PAPER", "FRICTION",
        {"paper", "friction", "abrading", "cardboard", "count", "flip", "kraft", "manila", "papyrus", "parchment", "rub", "rubbing", "scraping", "scratch", "scratching", "scuffing", "sliding", "tissue", "vellum"}});
    m_mappings.push_back({"PAPER", "HANDLE",
        {"paper", "bill", "browse", "cardboard", "clasp", "clutch", "count", "crease", "crinkle", "crumple", "flip", "fold", "grip", "handle", "holder", "kraft", "manila", "page", "papyrus", "parchment", "peruse", "pull", "roll", "sheet", "throw", "tissue", "toss", "turn", "vellum"}});
    m_mappings.push_back({"PAPER", "IMPACT",
        {"paper", "impact", "bump", "cardboard", "drop", "dropped", "hit", "kraft", "magazine", "manila", "newspaper", "papyrus", "parchment", "punch", "slam", "slap", "smack", "strike", "thump", "tissue", "vellum", "whack"}});
    m_mappings.push_back({"PAPER", "MISC",
        {"paper", "misc", "carbon", "cardstock", "construction", "copy", "craft", "crepe", "deed", "document", "dossier", "kraft", "leaf", "letter", "manila", "manuscript", "miscellaneous", "newsprint", "note", "notebook", "page", "papyrus", "parchment", "printer", "rice", "scroll", "sheet", "stationery", "tissue", "tracing", "vellum", "writing"}});
    m_mappings.push_back({"PAPER", "RIP",
        {"paper", "rip", "cardboard", "kraft", "manila", "papyrus", "parchment", "rend", "scraps", "shred", "tatter", "tear", "tearing", "tears", "tissue", "tore", "vellum"}});
    m_mappings.push_back({"PAPER", "TONAL",
        {"paper", "bowed", "frequency", "harmonic", "kraft", "manila", "melodic", "melodious", "musical", "papyrus", "parchment", "ping", "pitch", "resonance", "resonant", "ring", "shing", "sonorous", "sound", "timbre", "tissue", "tonal", "tone", "vellum"}});

    // PLASTIC Category
    m_mappings.push_back({"PLASTIC", "BREAK",
        {"plastic", "abs", "acetate", "acrylic", "break", "burst", "chloride", "crack", "crunch", "crush", "demolish", "destroy", "fracture", "fragment", "hdpe", "nylon", "polycarbonate", "polypropylene", "polystyrene", "polyvinyl", "pvc", "rip", "shatter", "smash", "snap", "splinter", "split"}});
    m_mappings.push_back({"PLASTIC", "CRASH & DEBRIS",
        {"plastic", "crash & debris", "abs", "acetate", "acrylic", "bits", "break", "chloride", "collision", "crash", "debris", "fall", "fragments", "hdpe", "nylon", "pieces", "polycarbonate", "polypropylene", "polystyrene", "polyvinyl", "pvc", "remains", "rubble", "ruins", "shards", "shatter", "smash", "wreckage"}});
    m_mappings.push_back({"PLASTIC", "FRICTION",
        {"plastic", "abrasion", "abs", "acetate", "acrylic", "chloride", "creak", "creaks", "friction", "grate", "grind", "grinding", "groan", "hdpe", "nylon", "polycarbonate", "polypropylene", "polystyrene", "polyvinyl", "pvc", "rasp", "rasping", "rub", "rubbing", "scrape", "scraping", "scratch", "scratching", "screech", "scuffing", "sliding", "squeak", "squeaks", "stress", "wear"}});
    m_mappings.push_back({"PLASTIC", "HANDLE",
        {"plastic", "abs", "acetate", "acrylic", "catch", "chloride", "clasp", "clench", "clutch", "down", "embrace", "grab", "grasp", "grip", "handle", "hdpe", "hold", "nylon", "operate", "pickup", "pluck", "polycarbonate", "polypropylene", "polystyrene", "polyvinyl", "pvc", "seize", "set", "snap", "take", "throw", "toss", "use"}});
    m_mappings.push_back({"PLASTIC", "IMPACT",
        {"plastic", "abs", "acetate", "acrylic", "bang", "banging", "bash", "bump", "chloride", "clap", "collide", "colliding", "crash", "crashing", "drop", "dropped", "hdpe", "hit", "hitting", "impact", "impacting", "jolt", "knock", "nylon", "polycarbonate", "polypropylene", "polystyrene", "polyvinyl", "pound", "punch", "pvc", "ram", "slam", "slamming", "smack", "smacking", "strike", "striking", "thrust", "thump"}});
    m_mappings.push_back({"PLASTIC", "MISC",
        {"plastic", "misc", "abs", "acetate", "acrylic", "artificial", "chloride", "fiber", "hdpe", "industrial", "man-made", "manufactured", "miscellaneous", "non-biodegradable", "nylon", "plasticine", "polycarbonate", "polyethylene", "polymer", "polypropylene", "polystyrene", "polyurethane", "polyvinyl", "ptfe", "pva", "pvc", "resin", "synthetic"}});
    m_mappings.push_back({"PLASTIC", "MOVEMENT",
        {"plastic", "movement", "abs", "acetate", "acrylic", "agitate", "bump", "chloride", "clatter", "drag", "hdpe", "jangle", "jiggle", "jingle", "nylon", "polycarbonate", "polypropylene", "polystyrene", "polyvinyl", "pvc", "rattle", "roll", "ruffle", "rustle", "shake", "stir", "sway", "vibrate", "wobble"}});
    m_mappings.push_back({"PLASTIC", "TONAL",
        {"plastic", "abs", "acetate", "acrylic", "bowed", "chloride", "frequency", "harmonic", "hdpe", "melodic", "melodious", "musical", "nylon", "ping", "pitch", "polycarbonate", "polypropylene", "polystyrene", "polyvinyl", "pvc", "resonance", "resonant", "ring", "shing", "sonorous", "sound", "timbre", "tonal", "tone"}});

    // RAIN Category
    m_mappings.push_back({"RAIN", "CLOTH",
        {"rain", "awning", "cloth", "cotton", "drizzle", "jacket", "precipitation", "raincoat", "rainwear", "tarp", "tent", "umbrella", "waterproof"}});
    m_mappings.push_back({"RAIN", "CONCRETE",
        {"rain", "concrete", "asphalt", "brick", "cement", "drizzle", "masonry", "path", "pavement", "precipitation", "sidewalk", "street"}});
    m_mappings.push_back({"RAIN", "GENERAL",
        {"rain", "general", "damp", "deluge", "downpour", "drenched", "drizzle", "mist", "monsoon", "precipitate", "precipitation", "rainfall", "raining", "rains", "rainstorm", "rainy", "shower", "sprinkle", "torrent", "wet"}});
    m_mappings.push_back({"RAIN", "GLASS",
        {"rain", "drizzle", "glass", "greenhouse", "pane", "precipitation", "sheet", "skylight", "sunroof", "window", "windshield"}});
    m_mappings.push_back({"RAIN", "INTERIOR",
        {"rain", "drizzle", "indoor", "interior", "precipitation", "rainstorm", "roof", "sunroof", "windshield"}});
    m_mappings.push_back({"RAIN", "METAL",
        {"rain", "can", "car", "drizzle", "gutters", "metal", "ping", "precipitation", "roof", "tin"}});
    m_mappings.push_back({"RAIN", "PLASTIC", {"rain", "drizzle", "plastic", "plexiglass", "precipitation", "vinyl"}});
    m_mappings.push_back({"RAIN", "VEGETATION",
        {"rain", "drizzle", "foliage", "grass", "jungle", "leaf", "leaves", "plants", "precipitation", "rainforest", "tree", "vegetation"}});
    m_mappings.push_back({"RAIN", "WATER",
        {"rain", "water", "drizzle", "droplets", "drops", "lake", "pond", "precipitation", "puddle", "ripples", "surface"}});
    m_mappings.push_back({"RAIN", "WOOD",
        {"rain", "barn", "deck", "drizzle", "floor", "precipitation", "roof", "shed", "wood"}});

    // ROBOTS Category
    m_mappings.push_back({"ROBOTS", "MISC",
        {"robots", "misc", "ai", "android", "androids", "artificial", "automatons", "beings", "bionic", "bot", "cybernetic", "cyborg", "cyborgs", "droid", "droids", "humanoid", "lifeforms", "machines", "mechs", "synthetic"}});
    m_mappings.push_back({"ROBOTS", "MOVEMENT",
        {"robots", "movement", "actuator", "ai", "android", "androids", "automatons", "bionic", "bot", "cybernetic", "cyborg", "cyborgs", "droid", "droids", "hydraulic", "machines", "mechanism", "mechs", "pneumatic", "servo", "solenoid", "synthetic"}});
    m_mappings.push_back({"ROBOTS", "VOCAL",
        {"robots", "vocal", "ai", "android", "androids", "artificial", "automatons", "beep", "beings", "bionic", "bot", "chirp", "cybernetic", "cyborg", "cyborgs", "droid", "droids", "lifeforms", "machines", "mechs", "r2d2", "synthetic"}});

    // ROCKS Category
    m_mappings.push_back({"ROCKS", "BREAK",
        {"rocks", "apart", "basalt", "boulders", "break", "breaks", "burst", "chip", "cobblestones", "crack", "cracks", "crumble", "crunches", "crush", "demolish", "destroy", "disintegrate", "formations", "fossil", "fracture", "fragment", "geology", "gneiss", "granite", "gravel", "gravelly", "hammered", "hammering", "hammers", "igneous", "limestone", "marble", "metamorphic", "minerals", "obsidian", "pebbles", "pickaxe", "pumice", "quarry", "quartzite", "rips", "sandstone", "schist", "scree", "sedimentary", "shale", "shatter", "shatters", "shingles", "slab"}});
    m_mappings.push_back({"ROCKS", "CRASH & DEBRIS",
        {"rocks", "crash & debris", "apart", "avalanche", "basalt", "boulders", "break", "clastic", "cobblestones", "collision", "crack", "crumble", "debris", "disintegrate", "down", "fall", "formations", "fracture", "fragment", "fragments", "geology", "gneiss", "granite", "gravel", "gravelly", "gritty", "igneous", "limestone", "marble", "metamorphic", "minerals", "moraine", "obsidian", "ore", "pebbles", "pumice", "quartzite", "remains", "rockfall", "rockslide", "rocky", "rubble", "ruins", "sandstone", "schist", "scree", "sedimentary", "shale", "shards", "shatter"}});
    m_mappings.push_back({"ROCKS", "FRICTION",
        {"rocks", "friction", "abrade", "abrasion", "basalt", "boulders", "cobblestones", "creaks", "erode", "formations", "geology", "gneiss", "granite", "grate", "gravel", "gravelly", "grind", "grinding", "igneous", "limestone", "marble", "metamorphic", "minerals", "obsidian", "pebbles", "pumice", "quartzite", "rasp", "rasping", "rub", "rubbing", "sandstone", "schist", "scour", "scrape", "scrapes", "scraping", "scratch", "scratching", "scree", "screech", "scuffing", "sedimentary", "shale", "shingles", "slab", "slate", "slates", "sliding", "specimens"}});
    m_mappings.push_back({"ROCKS", "HANDLE",
        {"rocks", "basalt", "boulders", "carry", "catch", "clasp", "clench", "clutch", "cobblestones", "down", "embrace", "formations", "geology", "gneiss", "grab", "granite", "grasp", "gravel", "gravelly", "grip", "handle", "hold", "igneous", "lift", "limestone", "manipulate", "marble", "metamorphic", "minerals", "move", "obsidian", "pebbles", "pickup", "pumice", "quartzite", "sandstone", "schist", "scree", "sedimentary", "seize", "set", "shale", "shingles", "slab", "slate", "slates", "specimens", "stones", "take", "throw"}});
    m_mappings.push_back({"ROCKS", "IMPACT",
        {"rocks", "bang", "banging", "basalt", "bash", "boulders", "bump", "chop", "clang", "clap", "clink", "clunk", "cobblestones", "collide", "colliding", "collision", "crash", "crashing", "drop", "dropped", "formations", "geology", "gneiss", "granite", "gravel", "gravelly", "hit", "hitting", "igneous", "impact", "impacting", "jolt", "knock", "limestone", "marble", "metamorphic", "minerals", "obsidian", "pebbles", "pound", "pumice", "quartzite", "ram", "sandstone", "schist", "scree", "sedimentary", "shale", "shingles", "shock"}});
    m_mappings.push_back({"ROCKS", "MISC",
        {"rocks", "misc", "basalt", "boulders", "cobblestones", "formations", "geology", "gneiss", "granite", "gravel", "gravelly", "igneous", "limestone", "marble", "metamorphic", "minerals", "miscellaneous", "obsidian", "pebbles", "pumice", "quartzite", "sandstone", "schist", "scree", "sedimentary", "shale", "shingles", "slab", "slate", "slates", "specimens", "stones"}});
    m_mappings.push_back({"ROCKS", "MOVEMENT",
        {"rocks", "movement", "agitate", "basalt", "boulders", "cobblestones", "displace", "drag", "fall", "formations", "geology", "glide", "gneiss", "granite", "gravel", "gravelly", "igneous", "jiggle", "jolt", "limestone", "marble", "metamorphic", "minerals", "move", "obsidian", "oscillate", "pebbles", "pumice", "quake", "quartzite", "rattle", "rockslide", "roll", "sandstone", "schist", "scree", "sedimentary", "shake", "shale", "shift", "shingles", "shudder", "slab", "slate", "slates", "slide", "slip", "slipping", "slips", "specimens"}});
    m_mappings.push_back({"ROCKS", "TONAL",
        {"rocks", "basalt", "boulders", "bowed", "cobblestones", "formations", "frequency", "geology", "gneiss", "granite", "gravel", "gravelly", "harmonic", "igneous", "limestone", "marble", "melodic", "melodious", "metamorphic", "minerals", "musical", "obsidian", "pebbles", "ping", "pitch", "pumice", "quartzite", "resonance", "resonant", "ring", "sandstone", "schist", "scree", "sedimentary", "shale", "shing", "shingles", "slab", "slate", "slates", "sonorous", "sound", "specimens", "stones", "timbre", "tonal", "tone"}});

    // ROPE Category
    m_mappings.push_back({"ROPE", "CREAK",
        {"rope", "cable", "cord", "creak", "extend", "lengthen", "line", "paracord", "pull", "straighten", "strain", "stretch", "taut", "tension", "tight", "tug", "twine", "yank"}});
    m_mappings.push_back({"ROPE", "HANDLE",
        {"rope", "anchor", "belay", "bend", "bowline", "braid", "cable", "carry", "catch", "clasp", "clench", "clutch", "coil", "cord", "cords", "down", "garrote", "gather", "grab", "grasp", "grip", "gripping", "handle", "hitching", "hold", "knot", "knotting", "lift", "line", "manipulate", "operate", "paracord", "pickup", "pull", "pulling", "rappel", "secure", "seize", "set", "take", "tether", "tethered", "tethering", "throw", "tie", "tied", "toss", "twine", "tying", "untie"}});
    m_mappings.push_back({"ROPE", "IMPACT",
        {"rope", "bang", "banging", "bump", "cable", "colliding", "cord", "crashing", "drop", "dropped", "flog", "flogging", "hit", "hitting", "impact", "impacting", "line", "noose", "paracord", "slam", "slamming", "slap", "smack", "smacking", "strike", "striking", "thump", "twine"}});
    m_mappings.push_back({"ROPE", "MISC",
        {"rope", "misc", "braids", "cable", "cables", "cord", "cordage", "cords", "halyard", "haul", "hawser", "knots", "lariat", "lash", "lashing", "line", "miscellaneous", "paracord", "rigging", "strand", "string", "tether", "thread", "twine", "wire", "yarn"}});
    m_mappings.push_back({"ROPE", "MOVEMENT",
        {"rope", "movement", "cable", "coil", "coiling", "cord", "dangle", "flail", "hang", "hanging", "line", "paracord", "slack", "slacking", "sway", "swing", "toss", "twine", "uncoil", "unfurl", "unravel", "unroll", "untangle", "untwist", "unwind", "zip", "zipline", "zuzz"}});

    // RUBBER Category
    m_mappings.push_back({"RUBBER", "CRASH & DEBRIS",
        {"rubber", "crash & debris", "collision", "debris", "elastic", "fragments", "latex", "neoprene", "pieces", "plop", "remains", "rubble", "ruins", "scatter", "shards", "silicone", "smash", "wreckage"}});
    m_mappings.push_back({"RUBBER", "FRICTION",
        {"rubber", "friction", "abrasion", "creak", "drag", "elastic", "grinding", "groan", "latex", "neoprene", "rasping", "rub", "rubbing", "scrape", "scraping", "scratching", "screech", "scuffing", "silicone", "sliding", "squeak", "squeaking", "stretch", "stretching", "wear"}});
    m_mappings.push_back({"RUBBER", "HANDLE",
        {"rubber", "catch", "clasp", "clench", "clutch", "condom", "down", "elastic", "embrace", "galoshes", "grab", "grasp", "grip", "handle", "hold", "innertube", "latex", "neoprene", "operate", "pickup", "seize", "set", "silicone", "squeeze", "take", "throw", "toss", "use"}});
    m_mappings.push_back({"RUBBER", "IMPACT",
        {"rubber", "bang", "banging", "bash", "bounce", "bump", "clap", "collide", "colliding", "crash", "crashing", "drop", "dropped", "elastic", "hit", "hitting", "impact", "impacting", "jolt", "knock", "latex", "neoprene", "pound", "punch", "ram", "silicone", "slam", "slamming", "smack", "smacking", "strike", "striking", "thrust", "thump"}});
    m_mappings.push_back({"RUBBER", "MISC",
        {"rubber", "misc", "elastic", "latex", "miscellaneous", "neoprene", "polymer", "silicone", "synthetic"}});
    m_mappings.push_back({"RUBBER", "MOVEMENT",
        {"rubber", "bounce", "drag", "elastic", "elasticity", "flap", "flex", "flop", "latex", "move", "movement", "neoprene", "pull", "silicone", "slide", "slip", "strain", "stretch", "tension", "twist", "yank"}});
    m_mappings.push_back({"RUBBER", "TONAL",
        {"rubber", "bowed", "elastic", "frequency", "harmonic", "latex", "melodic", "melodious", "musical", "neoprene", "ping", "pitch", "resonance", "resonant", "ring", "shing", "silicone", "sonorous", "sound", "timbre", "tonal", "tone"}});

    // SCIFI Category
    m_mappings.push_back({"SCIFI", "ALARM",
        {"scifi", "alarm", "alert", "buzzer", "fiction", "indicator", "notification", "sci-fi", "science", "signal", "warning"}});
    m_mappings.push_back({"SCIFI", "COMPUTER",
        {"scifi", "9000", "accumulator", "ai", "binary", "borg", "center", "command", "computer", "console", "fiction", "hal", "holoscreen", "interface", "matrix", "mcu", "quantum", "sci-fi", "science", "skynet", "touchscreen", "wopr"}});
    m_mappings.push_back({"SCIFI", "DOOR",
        {"scifi", "door", "access", "airlock", "bay", "blast", "doorway", "entrance", "exit", "fiction", "gateway", "hatch", "hatchbay", "opening", "passage", "pod", "portal", "sci-fi", "science", "teleporter"}});
    m_mappings.push_back({"SCIFI", "ENERGY",
        {"scifi", "energy", "atomic", "fiction", "field", "flux", "force", "forcefield", "hologram", "impulse", "particle", "power", "propulsion", "sci-fi", "science", "shield", "solar", "transporter", "vortex"}});
    m_mappings.push_back({"SCIFI", "IMPACT",
        {"scifi", "impact", "blast", "collision", "energy", "fiction", "field", "force", "hit", "jolt", "photon", "sci-fi", "science", "shield", "strike", "wave"}});
    m_mappings.push_back({"SCIFI", "MACHINE",
        {"scifi", "beam", "drive", "exosuit", "fiction", "food", "holodeck", "hologram", "machine", "replicator", "replicators", "sci-fi", "science", "screwdriver", "sonic", "teleporter", "time", "tractor", "warp"}});
    m_mappings.push_back({"SCIFI", "MECHANISM",
        {"scifi", "mechanism", "contraption", "device", "fiction", "gadget", "gizmo", "implement", "sci-fi", "science", "tricorder"}});
    m_mappings.push_back({"SCIFI", "MISC", {"scifi", "misc", "fiction", "miscellaneous", "sci-fi", "science"}});
    m_mappings.push_back({"SCIFI", "RETRO",
        {"scifi", "cheesy", "classic", "dr", "fiction", "nostalgic", "old", "old-school", "retro", "retrofuturistic", "school", "sci-fi", "science", "theremin", "throwback", "vintage", "who"}});
    m_mappings.push_back({"SCIFI", "SPACESHIP",
        {"scifi", "airship", "apollo", "astronaut", "battlecruiser", "cruiser", "destroyer", "enterprise", "extraterrestrial", "falcon", "fiction", "fighter", "galactica", "interstellar", "lem", "millenium", "mothership", "sci-fi", "science", "shuttle", "space", "spacecraft", "spaceship", "spaceships", "star", "starcruiser", "starship", "tie", "transport", "ufo", "vessel", "x-wing", "y-wing"}});
    m_mappings.push_back({"SCIFI", "VEHICLE",
        {"scifi", "vehicle", "bike", "buggy", "fiction", "hoverboard", "hovercraft", "landspeeder", "moon", "podracer", "rover", "sci-fi", "science", "speeder"}});
    m_mappings.push_back({"SCIFI", "WEAPON",
        {"scifi", "weapon", "advanced", "arm", "armament", "artillery", "blaster", "energy", "exotic", "fiction", "firearm", "futuristic", "high-tech", "intergalactic", "lightsaber", "lightsabers", "munition", "plasma", "sci-fi", "science", "space-age"}});

    // SNOW Category
    m_mappings.push_back({"SNOW", "CRASH & DEBRIS",
        {"snow", "crash & debris", "blizzard", "collision", "crash", "debris", "drift", "flurry", "fragments", "powder", "remains", "rubble", "ruins", "shards", "slush", "smash", "snowball", "snowfall", "wreckage"}});
    m_mappings.push_back({"SNOW", "FRICTION",
        {"snow", "friction", "abrasion", "creak", "flurry", "grinding", "powder", "rasping", "rubbing", "scrape", "scraping", "scratching", "screech", "scuffing", "shovel", "sliding", "slush", "squeak", "stress", "wear"}});
    m_mappings.push_back({"SNOW", "HANDLE",
        {"snow", "catch", "clasp", "clench", "clutch", "down", "embrace", "flurry", "form", "grab", "grasp", "grip", "handle", "hold", "mold", "operate", "pickup", "powder", "seize", "set", "shape", "slush", "take", "throw", "toss", "use"}});
    m_mappings.push_back({"SNOW", "IMPACT",
        {"snow", "impact", "bang", "banging", "bash", "clap", "collapse", "collide", "colliding", "crash", "crashing", "drop", "dropped", "drift", "fall", "flurry", "hit", "hitting", "impacting", "jolt", "knock", "pound", "powder", "ram", "slam", "slamming", "slush", "smack", "smacking", "snowball", "strike", "striking", "thrust", "topple"}});
    m_mappings.push_back({"SNOW", "MISC",
        {"snow", "misc", "cover", "flurry", "frost", "miscellaneous", "powder", "sleet", "slush", "snowdrift", "snowflakes", "snowpack"}});
    m_mappings.push_back({"SNOW", "MOVEMENT",
        {"snow", "movement", "blow", "drag", "drift", "flurry", "fumble", "pile", "plow", "powder", "shovel", "slide", "slush", "snowfall", "wade"}});

    // SPORTS Category
    m_mappings.push_back({"SPORTS", "COURT",
        {"sports", "court", "badminton", "basketball", "dodgeball", "dribbling", "fencing", "futsal", "handball", "netball", "paddleball", "pickleball", "racket", "racquetball", "shuffleboard", "squash", "table", "tennis", "volleyball"}});
    m_mappings.push_back({"SPORTS", "FIELD",
        {"sports", "american", "archery", "australian", "baseball", "canadian", "cricket", "field", "football", "frisbee", "gaelic", "hockey", "hurling", "infield", "lacrosse", "outfield", "polo", "rugby", "rules", "soccer", "softball", "ultimate"}});
    m_mappings.push_back({"SPORTS", "GYM",
        {"sports", "gym", "arts", "bodybuilding", "boxing", "calisthenics", "crossfit", "do", "dojo", "fitness", "fu", "gymnastics", "judo", "karate", "kickboxing", "kung", "kwon", "martial", "pilates", "powerlifting", "strongman", "tae", "training", "weight", "weightlifting", "wrestling", "yoga"}});
    m_mappings.push_back({"SPORTS", "INDOOR", {"sports", "indoor", "billiards", "bowling", "darts", "ping", "pong"}});
    m_mappings.push_back({"SPORTS", "MISC",
        {"sports", "misc", "athletics", "competition", "exercise", "fitness", "fun", "games", "golf", "health", "play", "recreation", "training", "velodrome", "well-being"}});
    m_mappings.push_back({"SPORTS", "SKATE",
        {"sports", "skate", "ice", "inline", "push", "roller", "rollerblading", "scooter", "skateboarding", "skatepark", "skater", "skating", "x-games"}});
    m_mappings.push_back({"SPORTS", "TRACK & FIELD",
        {"sports", "track & field", "decathlon", "discus", "hammer", "heptathlon", "high", "hurdles", "javelin", "jump", "long", "long-distance", "marathon", "middle-distance", "pole", "put", "races", "relay", "running", "runs", "shot", "sprint", "sprinting", "throw", "triple", "vault"}});
    m_mappings.push_back({"SPORTS", "WATER",
        {"sports", "aquatics", "canoeing", "diving", "jet", "kayaking", "kiteboarding", "kneeboarding", "paddleboarding", "polo", "rafting", "rowing", "sailing", "scuba", "skiing", "snorkeling", "surfing", "swimming", "volleyball", "wakeboarding", "water", "windsurfing"}});
    m_mappings.push_back({"SPORTS", "WINTER",
        {"sports", "winter", "biathlon", "bobsled", "bobsledding", "climbing", "cross-country", "curling", "dog", "fishing", "hockey", "ice", "jumping", "luge", "racing", "skating", "ski", "skiing", "skis", "sled", "sledding", "snowboarding", "snowmobiling", "snowshoeing", "speed"}});

    // SWOOSHES Category
    m_mappings.push_back({"SWOOSHES", "SWISH",
        {"swooshes", "by", "dart", "flash", "flutter", "fly", "glide", "kungfu", "pass", "race", "rush", "swing", "swipe", "swish", "swishy", "swoop", "swoosh", "whir", "whirl", "whiz", "whoosh", "whooshy", "zip", "zoom"}});
    m_mappings.push_back({"SWOOSHES", "WHOOSH",
        {"swooshes", "dart", "flash", "fly", "glide", "pass", "race", "rush", "swipe", "swishy", "swoop", "swoosh", "whir", "whirl", "whiz", "whoosh", "whooshy", "woosh", "zip", "zoom"}});

    // TOOLS Category
    m_mappings.push_back({"TOOLS", "GARDEN",
        {"tools", "broom", "cultivator", "edger", "fork", "garden", "hand", "hedge", "hoe", "hose", "lawn", "loppers", "mower", "pruner", "pruning", "rake", "shear", "shovel", "spade", "spades", "sprayer", "sprinkler", "tiller", "trimmer", "trowel", "weed", "weeder"}});
    m_mappings.push_back({"TOOLS", "HAND",
        {"tools", "allen", "bar", "chisel", "clamp", "cutter", "drill", "file", "files", "grip", "gun", "hacksaw", "hammer", "hand", "knife", "level", "mallet", "measure", "pliers", "pry", "ratchet", "saw", "scissors", "scraper", "screwdriver", "shears", "snips", "socket", "staple", "tape", "toolbox", "utility", "vise", "wrench"}});
    m_mappings.push_back({"TOOLS", "MISC", {"tools", "misc", "miscellaneous"}});
    m_mappings.push_back({"TOOLS", "PNEUMATIC",
        {"tools", "air", "chisel", "compressor", "drill", "grinder", "gun", "guns", "hammer", "hose", "impact", "jackhammer", "nail", "nailer", "pneumatic", "ratchet", "rivet", "sander", "spray", "stapler", "tool", "wrench"}});
    m_mappings.push_back({"TOOLS", "POWER",
        {"tools", "air", "angle", "belt", "circular", "compressor", "drill", "driver", "grinder", "gun", "heat", "impact", "jigsaw", "miter", "nail", "planer", "power", "press", "reciprocating", "router", "sander", "saw", "screwdriver", "staple", "table", "tool"}});

    // TOYS Category
    m_mappings.push_back({"TOYS", "ELECTRONIC",
        {"toys", "electronic", "car", "controlled", "device", "digital", "entertainment", "furby", "gadget", "game", "gizmo", "interactive", "play", "remote", "screen", "slot", "tech", "toy", "train", "virtual"}});
    m_mappings.push_back({"TOYS", "MECHANICAL",
        {"toys", "mechanical", "box", "erector", "jack-in-the-box", "kaleidoscope", "music", "robot", "slinky", "spring-loaded", "tops", "toy", "wind-up", "winding", "windup", "yo-yo"}});
    m_mappings.push_back({"TOYS", "MISC",
        {"toys", "misc", "action", "animal", "ball", "blocks", "building", "construction", "doll", "figure", "legos", "model", "plaything", "plushie", "puppet", "puzzle", "set", "stuffed"}});

    // TRAINS Category
    m_mappings.push_back({"TRAINS", "BRAKE",
        {"trains", "bake", "brake", "brakes", "braking", "breaks", "grind", "halting", "pads", "rail", "railcar", "railway", "rolling", "screech", "shoes", "stock"}});
    m_mappings.push_back({"TRAINS", "CLACK",
        {"trains", "clack", "clattering", "clickety-clack", "railway", "rattling", "rhythmic", "track", "train", "wheel"}});
    m_mappings.push_back({"TRAINS", "DOOR",
        {"trains", "access", "boxcar", "cab", "caboose", "car", "coach", "door", "hatch", "rail", "railway", "subway", "train"}});
    m_mappings.push_back({"TRAINS", "DIESEL",
        {"trains", "diesel", "diesel-electric", "diesel-powered", "freight", "locomotive", "passenger", "powerhouse", "railcar", "railway", "train"}});
    m_mappings.push_back({"TRAINS", "ELECTRIC",
        {"trains", "electric", "city", "commuter", "inter-city", "light", "line", "maglev", "monorail", "rail", "railway", "regional", "train"}});
    m_mappings.push_back({"TRAINS", "HIGH SPEED",
        {"trains", "high speed", "350", "agv", "ave", "brightline", "bullet", "crh", "eurostar", "express", "hi-tech", "high-speed", "hyperloop", "ice", "maglev", "monorails", "railway", "rapid", "shinkansen", "talgo", "tgv", "train", "x2000"}});
    m_mappings.push_back({"TRAINS", "HORN",
        {"trains", "air", "commuter", "electric", "freight", "high-speed", "horn", "light", "locomotive", "passenger", "rail", "railway", "regional", "subway", "train", "tram"}});
    m_mappings.push_back({"TRAINS", "INTERIOR",
        {"trains", "interior", "aboard", "berth", "cabin", "caboose", "car", "carriage", "city", "coach", "commuter", "compartment", "dining", "freight", "inter-city", "light", "line", "lounge", "luggage", "monorail", "observation", "onboard", "overnight", "passenger", "racks", "rail", "railway", "regional", "rider", "saloon", "seating", "sleeper", "subway", "train"}});
    m_mappings.push_back({"TRAINS", "MECHANISM",
        {"trains", "mechanism", "axles", "bearings", "brakes", "coupler", "couplers", "decouple", "railway", "shunt", "shunter", "suspension", "switcher", "wheels"}});
    m_mappings.push_back({"TRAINS", "MISC", {"trains", "misc", "miscellaneous", "railway"}});
    m_mappings.push_back({"TRAINS", "STEAM",
        {"trains", "boiler", "chuffing", "chugging", "coal", "firebox", "heritage", "hissing", "locomotive", "narrow-gauge", "piston", "railway", "smokestack", "steam", "steam-powered", "steaming", "tender"}});
    m_mappings.push_back({"TRAINS", "SUBWAY",
        {"trains", "city", "commuter", "electric", "mass", "metro", "mrt", "railcar", "railway", "rapid", "subway", "system", "train", "transit", "tube", "tubes", "underground", "urban"}});
    m_mappings.push_back({"TRAINS", "TRAM",
        {"trains", "cable", "car", "funicular", "grip", "light", "mover", "people", "rail", "railway", "streetcar", "tram", "tramcar", "tramway", "trolley", "trolleybus"}});

    // USER INTERFACE Category
    m_mappings.push_back({"USER INTERFACE", "ALERT",
        {"user interface", "advisory", "alarm", "alert", "alertness", "attention", "blip", "caution", "gui", "message", "notice", "notification", "pop-up", "prompt", "signal", "sound", "startup", "text", "tone", "ui", "ux", "warning"}});
    m_mappings.push_back({"USER INTERFACE", "BEEP",
        {"user interface", "alert", "audible", "audio", "beep", "bleep", "boop", "chime", "computer", "gui", "notification", "phone", "prompt", "signal", "sound", "tone", "ui", "ux", "warning"}});
    m_mappings.push_back({"USER INTERFACE", "CLICK",
        {"user interface", "button", "choose", "click", "gui", "interaction", "interface", "menu", "navigation", "pick", "press", "select", "tap", "type", "ui", "ux"}});
    m_mappings.push_back({"USER INTERFACE", "DATA",
        {"user interface", "buffer", "bytes", "content", "data", "document", "download", "file", "gui", "information", "input", "network", "processing", "record", "resource", "thinking", "transfer", "ui", "upload", "ux"}});
    m_mappings.push_back({"USER INTERFACE", "GLITCH",
        {"user interface", "abnormality", "anomaly", "bug", "corrupt", "corruption", "crash", "defect", "distortion", "error", "failure", "fault", "flaw", "glitch", "glitchiness", "gui", "interference", "issue", "kernel", "malfunction", "message", "noise", "panic", "problem", "report", "static", "ui", "ux"}});
    m_mappings.push_back({"USER INTERFACE", "MISC", {"user interface", "misc", "gui", "miscellaneous", "ui", "ux"}});
    m_mappings.push_back({"USER INTERFACE", "MOTION",
        {"user interface", "action", "activity", "animation", "choose", "confirm", "drag", "dynamics", "flick", "gui", "motion", "movement", "navigation", "page", "scroll", "swipe", "transition", "ui", "ux", "zoom"}});

    // VEGETATION Category
    m_mappings.push_back({"VEGETATION", "GRASS",
        {"vegetation", "flora", "foliage", "grass", "grassland", "green", "greenery", "hay", "lawn", "meadow", "moss", "mow", "mown", "pasture", "rustle", "sod", "through", "turf", "walk", "weeds", "wheat"}});
    m_mappings.push_back({"VEGETATION", "LEAVES",
        {"vegetation", "alder", "apple", "ash", "aspen", "beech", "birch", "boxwood", "buckeye", "bushes", "cedar", "cherry", "chestnut", "cypress", "dogwood", "elm", "fall", "falling", "fern", "fir", "flora", "foliage", "fronds", "greenery", "hemlock", "hickory", "larch", "leaf", "leafage", "leaves", "magnolia", "mahogany", "maple", "oak", "petals", "pile", "pine", "poplar", "redwood", "scrub", "sequoia", "shoots", "shrub", "shrubbery", "spruce", "stems", "swirl", "sycamore", "thicket", "underbrush"}});
    m_mappings.push_back({"VEGETATION", "MISC",
        {"vegetation", "misc", "botanical", "flora", "foliage", "greenery", "plant", "shrub", "shrubs"}});
    m_mappings.push_back({"VEGETATION", "TREE",
        {"vegetation", "alder", "apple", "ash", "aspen", "bark", "beech", "birch", "boughs", "boxwood", "branch", "branches", "break", "buckeye", "canopy", "cedar", "cherry", "chestnut", "conifer", "crown", "cypress", "dogwood", "elm", "eucalyptus", "fall", "fir", "foliage", "forest", "greenery", "hemlock", "hickory", "larch", "magnolia", "mahogany", "maple", "oak", "palm", "pine", "poplar", "redwood", "sequoia", "snap", "spruce", "sycamore", "timber", "tree", "trunk", "twig", "willow", "wood"}});

    // VEHICLES Category
    m_mappings.push_back({"VEHICLES", "ALARM",
        {"vehicles", "alarm", "anti-theft", "arm", "auto", "automobile", "car", "chirp", "disarm", "fob", "security", "system"}});
    m_mappings.push_back({"VEHICLES", "ANTIQUE",
        {"vehicles", "a", "antique", "austin", "auto", "automobile", "car", "classic", "collectible", "duesenberg", "historic", "hudson", "model", "nash", "old-fashioned", "packard", "pierce-arrow", "rare", "roadster", "studebaker", "t", "used", "vintage"}});
    m_mappings.push_back({"VEHICLES", "ATV",
        {"vehicles", "all-terrain", "atv", "buggy", "dune", "four-wheeler", "off-road", "quad", "quadricycle", "side-by-side", "three-wheeler", "utility", "utv"}});
    m_mappings.push_back({"VEHICLES", "BICYCLE",
        {"vehicles", "10-speed", "backpedal", "bicycle", "bike", "bmx", "cycle", "derailleur", "downshift", "freewheel", "kickstand", "mountain", "pedal", "pedaler", "racer", "recumbent", "rider", "road", "spoke", "tandem", "ten-speed", "training", "tricycle", "unicycle", "velocipede", "wheelie", "wheels"}});
    m_mappings.push_back({"VEHICLES", "BRAKE",
        {"vehicles", "abs", "anti-lock", "auto", "automobile", "brake", "brakes", "car", "decelerate", "disc", "drum", "grind", "halt", "hydraulic", "rotor", "screech", "squeal", "stop"}});
    m_mappings.push_back({"VEHICLES", "BUS",
        {"vehicles", "bus", "buses", "city", "coach", "coaches", "double-decker", "fleet", "greyhound", "school", "shuttle", "sightseeing", "tour", "transit"}});
    m_mappings.push_back({"VEHICLES", "CAR",
        {"vehicles", "auto", "automobile", "automobiles", "autos", "car", "compact", "convertible", "coupe", "crossover", "hatchback", "luxury", "rental", "sedan", "sports", "station", "subcompact", "taxi", "wagon"}});
    m_mappings.push_back({"VEHICLES", "CONSTRUCTION",
        {"vehicles", "construction", "backhoe", "bulldozer", "concrete", "constructor", "crane", "digger", "dump", "excavator", "forklift", "grader", "loader", "mixer", "paver", "road", "roller", "skid", "truck", "wrecker"}});
    m_mappings.push_back({"VEHICLES", "DOOR",
        {"vehicles", "auto", "automobile", "car", "close", "compact", "convertible", "coupe", "crossover", "door", "drivers", "hatchback", "hood", "liftgate", "luxury", "open", "passenger", "rental", "sedan", "sports", "station", "subcompact", "taxi", "tailgate", "slam", "suv", "truck", "trunk", "van"}});
    m_mappings.push_back({"VEHICLES", "ELECTRIC",
        {"vehicles", "auto", "automobile", "car", "cart", "e-bike", "electric", "electrified", "ev", "go-kart", "gold", "golf", "hybrid", "motorcycle", "plug-in", "rivian", "scooter", "segway", "tesla"}});
    m_mappings.push_back({"VEHICLES", "EMERGENCY",
        {"vehicles", "emergency", "ambulance", "auto", "automobile", "car", "engine", "fire", "firetruck", "first", "hazmat", "k9", "paramedic", "patrol", "police", "rescue", "responder", "search", "swat", "truck", "unit", "van", "vehicle"}});
    m_mappings.push_back({"VEHICLES", "FARM",
        {"vehicles", "agricultural", "bale", "carrier", "cart", "combine", "crop", "cultivator", "drill", "duster", "equipment", "farm", "fertilizer", "forage", "grain", "harvester", "hay", "husker", "irrigation", "irrigator", "livestock", "manure", "plow", "processor", "rake", "seed", "seeder", "sprayer", "spreader", "sprinkler", "tiller", "tractor", "tractors", "truck", "wagon"}});
    m_mappings.push_back({"VEHICLES", "FREIGHT",
        {"vehicles", "big", "box", "cargo", "delivery", "flatbed", "freight", "hauler", "hauling", "livestock", "logging", "lorry", "moving", "penske", "rigs", "semi", "semi-truck", "shipping", "tanker", "transport", "truck", "trucked", "trucking", "trucks", "u-haul"}});
    m_mappings.push_back({"VEHICLES", "GENERIC BY",
        {"vehicles", "generic by", "auto", "automobile", "by", "car", "pass", "passing"}});
    m_mappings.push_back({"VEHICLES", "HORN",
        {"vehicles", "auto", "automobile", "beep", "bus", "car", "honk", "honker", "hooter", "horn", "hour", "motorcycle", "rush", "semi", "suv", "toot", "truck"}});
    m_mappings.push_back({"VEHICLES", "INTERIOR",
        {"vehicles", "auto", "automobile", "bus", "car", "driver", "driving", "inside", "interior", "limousine", "onboard", "passenger", "seat", "semi", "truck", "van", "vehicle"}});
    m_mappings.push_back({"VEHICLES", "JALOPY",
        {"vehicles", "auto", "automobile", "backfire", "banger", "beater", "bucket", "car", "clunker", "false", "heap", "hooptie", "hoopty", "jalopy", "junk", "junker", "lemon", "malfunctioning", "misfire", "misfiring", "old", "ramshackle", "rattletrap", "relic", "rust", "rustbucket", "scrap", "start", "wreck"}});
    m_mappings.push_back({"VEHICLES", "MECHANISM",
        {"vehicles", "mechanism", "adjust", "auto", "automobile", "belt", "box", "brake", "car", "choke", "climate", "clutch", "compartment", "control", "crank", "gas cap", "gear", "gearshift", "glove", "glovebox", "handbrake", "headlights", "ignition", "key", "lever", "mirror", "oil cap", "parking", "rearview", "seat", "seatbelt", "shift", "side", "signal", "steering", "throttle", "trunk", "turn", "vent", "wheel", "window", "windshield", "wiper"}});
    m_mappings.push_back({"VEHICLES", "MILITARY",
        {"vehicles", "amphibious", "apc", "armored", "army", "carrier", "convoy", "hummer", "humvee", "jeep", "jeeps", "military", "panzer", "personnel", "tactical", "tank", "tanks", "transport", "troops"}});
    m_mappings.push_back({"VEHICLES", "MISC",
        {"vehicles", "misc", "auto", "automobile", "car", "limousine", "snowmobile"}});
    m_mappings.push_back({"VEHICLES", "MOTORCYCLE",
        {"vehicles", "bike", "bikers", "bmw", "chopper", "cruiser", "dirt", "dirtbike", "ducati", "handlebar", "harley", "hog", "honda", "indian", "kawasaki", "ktm", "minibike", "moped", "mopeds", "moto", "motocross", "motorbike", "motorbikes", "motorcycle", "motorcyclists", "scooter", "scooters", "scrambler", "sidecar", "sport", "superbike", "supermotard", "suzuki", "touring", "triumph", "two-wheeler", "vespa", "yamaha"}});
    m_mappings.push_back({"VEHICLES", "RACING",
        {"vehicles", "racing", "1", "auto", "automobile", "car", "drag", "dragster", "f1", "formula", "grand", "indy", "monster", "nascar", "prix", "race", "racecar", "rally", "sports", "stock", "supercar", "track", "truck"}});
    m_mappings.push_back({"VEHICLES", "SIREN",
        {"vehicles", "air", "alarm", "ambulance", "auto", "automobile", "blare", "brazen", "call", "car", "cops", "emergency", "fighter", "fire", "firetruck", "hi-lo", "hooter", "horn", "howler", "piercer", "police", "power", "siren", "truck", "wail", "warning", "whoop", "yelp"}});
    m_mappings.push_back({"VEHICLES", "SKID",
        {"vehicles", "abs", "auto", "automobile", "burnout", "car", "careening", "chuff", "drift", "fishtail", "marks", "out", "peel", "screech", "skid", "skidmark", "slide", "spin", "squeal", "swerve", "tire"}});
    m_mappings.push_back({"VEHICLES", "SUSPENSION",
        {"vehicles", "absorber", "absorbers", "air", "anti-roll", "arm", "arms", "auto", "automobile", "bar", "bump", "car", "chassis", "coilovers", "control", "dampers", "independent", "leaf", "macpherson", "multi-link", "pothole", "rattle", "shock", "speed", "springs", "squeak", "stabilizer", "strut", "struts", "suspension", "sway", "torsion", "wishbone"}});
    m_mappings.push_back({"VEHICLES", "TIRE",
        {"vehicles", "all-season", "bicycle", "blackwall", "bridgestone", "car", "dunlops", "firestone", "firestones", "flat", "goodyears", "hubcap", "innertube", "michelin", "motorcycle", "noise", "off-road", "performance", "pirelli", "puncture", "racing", "radial", "rim", "rims", "road", "roll", "rubber", "run-flat", "sidewall", "snow", "spare", "summer", "tire", "tires", "tread", "truck", "tubeless", "tyres", "wheel", "wheels", "whitewall", "winter"}});
    m_mappings.push_back({"VEHICLES", "TRUCK VAN & SUV",
        {"vehicles", "truck van & suv", "box", "camper", "cargo", "conversion", "cream", "delivery", "flatbed", "food", "ice", "microbus", "mini", "minibus", "minivan", "paddy", "panel", "passenger", "pickup", "rv", "step", "suv", "tow", "truck", "van", "vans", "work"}});
    m_mappings.push_back({"VEHICLES", "UTILITY",
        {"vehicles", "utility", "by", "cushman", "gator", "ranger", "rzr", "side", "sxs", "ute", "utv"}});
    m_mappings.push_back({"VEHICLES", "WAGON",
        {"vehicles", "amish", "buckboard", "buggy", "carriage", "carriages", "cart", "chariot", "drawbar", "hayride", "ox", "stagecoach", "waggon", "wagon", "wagons", "wain", "wood"}});
    m_mappings.push_back({"VEHICLES", "WINDOW",
        {"vehicles", "auto", "automatic", "automobile", "car", "drivers", "down", "passenger", "power", "roll", "up", "window"}});

    // VOICES Category
    m_mappings.push_back({"VOICES", "ALIEN",
        {"voices", "alien", "chewbacca", "cosmic", "et", "extraterrestrial", "language", "vocal", "vocalization"}});
    m_mappings.push_back({"VOICES", "BABY",
        {"voices", "baby", "baba", "babble", "bambino", "coo", "cooing", "dada", "fuss", "gaga", "infant", "infantile", "little", "mama", "newborn", "nursing", "nursling", "one", "snuffle", "toddler", "tot", "tyke", "vocal", "vocalization", "wean"}});
    m_mappings.push_back({"VOICES", "CHEER",
        {"voices", "acclaim", "acclamation", "bravo", "celebrating", "cheer", "cheering", "commendation", "encouraging", "holler", "hollering", "horray", "hurrah", "huzzah", "kudos", "ovation", "rooting", "shout", "supporting", "vocal", "vocalization", "woohoo", "yay", "yelling"}});
    m_mappings.push_back({"VOICES", "CHILD",
        {"voices", "adolescent", "child", "children", "juvenile", "kid", "minor", "one", "preteen", "pubescent", "teen", "toddler", "tween", "vocal", "vocalization", "young", "youngling", "youngster", "youth"}});
    m_mappings.push_back({"VOICES", "CRYING",
        {"voices", "crying", "bawl", "bawling", "bemoan", "blubber", "blubbering", "cry", "fuss", "howling", "lamenting", "pout", "sniffling", "sniveling", "sob", "sobbing", "vocal", "vocalization", "wail", "wailing", "weep", "weeping", "whimper", "whimpering", "whining", "wounded"}});
    m_mappings.push_back({"VOICES", "EFFORTS",
        {"voices", "efforts", "effort", "exert", "exertion", "exhale", "gasping", "grunt", "grunting", "heaving", "inhale", "panting", "pushing", "strain", "struggle", "struggling", "vocal", "vocalization", "wheezing"}});
    m_mappings.push_back({"VOICES", "FEMALE",
        {"voices", "chick", "dame", "female", "feminine", "gal", "girl", "lady", "lass", "madam", "miss", "person", "vocal", "vocalization", "woman"}});
    m_mappings.push_back({"VOICES", "FUTZED",
        {"voices", "futzed", "address", "altered", "announcement", "distorted", "manipulated", "megaphone", "modified", "pa", "phone", "processed", "public", "radio", "speaker", "telephone", "tv", "vocal", "vocalization", "walkie-talkie"}});
    m_mappings.push_back({"VOICES", "HISTORICAL",
        {"voices", "annoucement", "broadcast", "decree", "edict", "historical", "lecture", "manifesto", "news", "period", "proclamation", "pronouncement", "speech", "statement", "vintage", "vocal", "vocalization"}});
    m_mappings.push_back({"VOICES", "LAUGH",
        {"voices", "belly", "cackle", "cackling", "chortle", "chortling", "chuckle", "chuckling", "funny", "giggle", "giggling", "guffaw", "guffawing", "haha", "holler", "hoot", "humor", "humour", "hysterical", "joke", "laugh", "laughing", "maniacal", "snicker", "snickering", "snigger", "sniggering", "titter", "tittering", "twitter", "vocal", "vocalization"}});
    m_mappings.push_back({"VOICES", "MALE",
        {"voices", "bloke", "boy", "brother", "chap", "dude", "fellow", "gentleman", "guy", "male", "man", "masculine", "person", "vocal", "vocalization"}});
    m_mappings.push_back({"VOICES", "MISC", {"voices", "misc", "miscellaneous", "vocal", "vocalization"}});
    m_mappings.push_back({"VOICES", "REACTION",
        {"voices", "aahs", "acknowledgement", "ah", "ahh", "ahhh", "answer", "applause", "booing", "boos", "chanting", "cheers", "comeback", "counter", "em", "er", "excited", "feedback", "gasps", "hm", "hmm", "hollers", "hoots", "laughter", "murmurs", "oh", "ooh", "oohs", "ooo", "reaction", "rejoinder", "reply", "response", "retort", "sighs", "vocal", "vocalization", "whistling"}});
    m_mappings.push_back({"VOICES", "SCREAM",
        {"voices", "bellow", "clamour", "death", "fall", "hollar", "holler", "howl", "outcry", "scream", "screech", "shout", "shriek", "squeal", "vocal", "vocalization", "wail", "wilhelm", "yell", "yelp", "yowl"}});
    m_mappings.push_back({"VOICES", "SINGING",
        {"voices", "singing", "acappella", "caroling", "chanting", "chorusing", "crooning", "ditty", "hum", "hymning", "melodizing", "performing", "serenading", "sing", "vocal", "vocalization", "vocalizing"}});
    m_mappings.push_back({"VOICES", "WHISPER",
        {"voices", "faint", "gossip", "grumble", "grumbling", "hush", "hushed", "mumble", "murmur", "mutter", "muttering", "quietly", "secret", "softly", "speak", "subdued", "susurration", "undertones", "vocal", "vocalization", "whisper"}});

    // WATER Category
    m_mappings.push_back({"WATER", "BUBBLES",
        {"water", "bubbles", "aerate", "aerated", "aeration", "aqua", "blub", "boil", "boiling", "boils", "bubble", "bubbler", "bubbling", "bubbly", "carbonation", "cauldron", "cavitation", "effervesce", "effervescence", "effervescent", "foam", "froth", "frothy", "glub", "gurgling", "h20", "hissing", "potion", "scuba", "simmer"}});
    m_mappings.push_back({"WATER", "DRAIN",
        {"water", "aqua", "burble", "culvert", "discharge", "diverter", "downspout", "downspouts", "drain", "drainage", "drainages", "draining", "drainpipes", "drains", "flow", "gurgle", "gutter", "h20", "leaking", "ooze", "outflow", "outlet", "pipe", "runoff", "seepage", "sewer", "shower", "sink", "storm", "stream", "trickle"}});
    m_mappings.push_back({"WATER", "DRIP",
        {"water", "aqua", "cave", "dribble", "drip", "dripping", "drizzle", "drop", "h20", "leak", "leaking", "plip", "plop", "seep", "splash", "sprinkle", "trickle"}});
    m_mappings.push_back({"WATER", "FIZZ",
        {"water", "aqua", "bubble", "bubbling", "carbonation", "coke", "cola", "crackling", "effervescence", "fizz", "fizzle", "fizzling", "fizzy", "foam", "foaming", "froth", "h20", "hiss", "hissing", "perrier", "popping", "seltzer", "sizzle", "sizzling", "snapping", "soda", "sparkle", "sparkling"}});
    m_mappings.push_back({"WATER", "FLOW",
        {"water", "flow", "aqua", "brook", "creek", "current", "gully", "h20", "rill", "river", "rivulet", "runnel", "running", "spring", "stream", "tributary", "watercourse"}});
    m_mappings.push_back({"WATER", "FOUNTAIN",
        {"water", "aqua", "birdbath", "decorative", "feature", "flow", "fountain", "h20", "hiss", "ornamental", "spout", "spray", "trickle", "well", "wishing"}});
    m_mappings.push_back({"WATER", "IMPACT",
        {"water", "impact", "aqua", "belly", "bellyflop", "cannonball", "charge", "collide", "crash", "depth", "flop", "h20", "hit", "slam", "splash", "sploosh"}});
    m_mappings.push_back({"WATER", "LAP",
        {"water", "aqua", "h20", "hull", "lap", "lapping", "ripple", "slap", "slosh", "sloshing"}});
    m_mappings.push_back({"WATER", "MISC", {"water", "misc", "aqua", "h20", "miscellaneous"}});
    m_mappings.push_back({"WATER", "MOVEMENT",
        {"water", "movement", "aqua", "bobbing", "churning", "ebbing", "eddying", "h20", "rippling", "slosh", "splash", "stirring", "sweep", "swirling", "tread", "wade"}});
    m_mappings.push_back({"WATER", "PLUMBING",
        {"water", "aqua", "aqueduct", "auger", "bathtub", "bidet", "channel", "clog", "commode", "conduit", "crapper", "drain", "drainpipe", "faucet", "fixtures", "flush", "grease", "gutter", "h20", "p-trap", "pipage", "pipeline", "pipes", "piping", "plumber", "plumbing", "septic", "sewage", "sewer", "shower", "sink", "spout", "tank", "toilet", "trap", "trough", "valve", "waterworks"}});
    m_mappings.push_back({"WATER", "POUR",
        {"water", "aqua", "bathe", "discharged", "discharging", "dispense", "douse", "dousing", "dowse", "dowsing", "drench", "drenched", "dump", "empty", "fill", "flow", "gush", "h20", "overflow", "pour", "pouring", "sluice", "sluicing", "spill"}});
    m_mappings.push_back({"WATER", "SPLASH",
        {"water", "aqua", "dive", "h20", "kerplunk", "plunge", "showering", "slosh", "spattering", "splash", "splattering", "splish", "splosh", "spraying", "submerge", "swim", "wade"}});
    m_mappings.push_back({"WATER", "SPRAY",
        {"water", "aqua", "gush", "h20", "hose", "hosed", "irrigate", "mist", "spray", "sprinkle", "spritz", "squirt"}});
    m_mappings.push_back({"WATER", "STEAM",
        {"water", "aqua", "condensation", "evaporation", "fog", "h20", "hiss", "mist", "spritz", "sputter", "steam", "superheat", "superheating", "vapor", "vaporization", "wet"}});
    m_mappings.push_back({"WATER", "SURF",
        {"water", "aqua", "beach", "billows", "breakers", "coastline", "h20", "rollers", "seashore", "shoreline", "surf", "surge", "swell", "tide", "wash", "waves", "whitecaps"}});
    m_mappings.push_back({"WATER", "TURBULENT",
        {"water", "agitated", "aqua", "choppy", "churning", "current", "h20", "maelstrom", "pool", "raging", "rapids", "riptide", "roil", "roiled", "rough", "squally", "stormy", "swells", "swirl", "swirling", "tempestuous", "torrential", "tumultuous", "turbulent", "undertow", "violent", "wave", "whirlpool", "white", "whitecaps"}});
    m_mappings.push_back({"WATER", "UNDERWATER",
        {"water", "aqua", "aquatic", "engulfed", "flooded", "flow", "h20", "immersed", "inundated", "plunged", "subaquatic", "subaqueous", "submerged", "submersed", "subsea", "sunken", "undersea", "underwater"}});
    m_mappings.push_back({"WATER", "WATERFALL",
        {"water", "aqua", "cascade", "cascading", "cataract", "dam", "falls", "h20", "niagara", "plunge", "rapids", "torrent", "victoria", "waterfall"}});
    m_mappings.push_back({"WATER", "WAVE",
        {"water", "aqua", "billow", "breaker", "breakers", "breakwater", "crest", "current", "h20", "ocean", "ripples", "roller", "sea", "seashore", "surf", "surge", "swell", "swells", "tides", "wave", "waves", "whitecap"}});

    // WEAPONS Category
    m_mappings.push_back({"WEAPONS", "ARMOR",
        {"weapons", "armor", "armorer", "armory", "armour", "armoured", "armourer", "armoury", "bracers", "brassard", "breastplate", "buckler", "chainmail", "cuirass", "domaru", "gauntlets", "greaves", "haramaki", "hauberk", "helm", "helmet", "mail", "of", "pauldron", "plate", "sabatons", "scale", "shield", "shields", "spaulders", "splint", "suit", "tabard", "vambrace", "visor"}});
    m_mappings.push_back({"WEAPONS", "ARROW",
        {"weapons", "archer", "arrow", "arrowhead", "bolt", "bowyer", "crossbow", "dart", "fletch", "fletcher", "fletching", "flight", "nock", "projectile", "quarrel", "quiver", "shaft"}});
    m_mappings.push_back({"WEAPONS", "AXE",
        {"weapons", "ax", "axe", "blade", "chop", "chopper", "edge", "handle", "hatchet", "head", "throwing", "tomahawk"}});
    m_mappings.push_back({"WEAPONS", "BLUNT",
        {"weapons", "blunt", "ball", "bat", "baton", "bludgeon", "brass", "chain", "club", "cudgel", "flail", "hammer", "joust", "knuckles", "mace", "morning", "nightstick", "nunchuck", "star", "stave", "tonfa", "truncheon", "war", "warhammer"}});
    m_mappings.push_back({"WEAPONS", "BOW",
        {"weapons", "archer", "bow", "bowyer", "box", "compound", "crossbow", "fletcher", "hornbow", "longbow", "recurve", "release", "shortbow", "string", "traditional"}});
    m_mappings.push_back({"WEAPONS", "KNIFE",
        {"weapons", "army", "bayonet", "blade", "bowie", "butcher", "butterfly", "chef", "cleaver", "dagger", "dirk", "hunting", "jack", "kitchen", "knife", "machete", "penknife", "pocket", "pocketknife", "scalpel", "shealth", "steak", "stiletto", "survival", "swiss", "switchblade", "throwing", "trench", "utility"}});
    m_mappings.push_back({"WEAPONS", "MISC", {"weapons", "misc", "boomerang", "slingshot"}});
    m_mappings.push_back({"WEAPONS", "POLEARM",
        {"weapons", "polearm", "fauchard", "glaive", "halberd", "harpoon", "javelin", "lance", "pike", "poleaxe", "pollaxe", "spear"}});
    m_mappings.push_back({"WEAPONS", "SIEGE",
        {"weapons", "ballista", "battering", "bombard", "catapult", "engine", "fire", "mangonel", "mangonon", "onager", "onagro", "petraria", "petrary", "ram", "scorpion", "siege", "springald", "torsion", "tower", "trebuchet", "warwolf"}});
    m_mappings.push_back({"WEAPONS", "SWORD",
        {"weapons", "blade", "broadsword", "claymore", "cutlass", "dagger", "epee", "falchion", "fencing", "foil", "foils", "katana", "longsword", "machete", "pommel", "rapier", "saber", "sabre", "samurai", "scabbard", "scimitar", "slashing", "sword", "swords", "viking"}});
    m_mappings.push_back({"WEAPONS", "WHIP",
        {"weapons", "buggy", "bullwhip", "cat'o'nine", "crop", "flogger", "horse", "lash", "riding", "scourge", "tails", "thong", "whip", "whipcord", "whipcrack"}});

    // WEATHER Category
    m_mappings.push_back({"WEATHER", "HAIL",
        {"weather", "balls", "frozen", "graupel", "hail", "hailstones", "hailstorm", "ice", "pellets", "rain", "sleet"}});
    m_mappings.push_back({"WEATHER", "MISC",
        {"weather", "misc", "atmosphere", "atmospheric", "climate", "conditions", "elements", "forecast", "meteorology", "miscellaneous"}});
    m_mappings.push_back({"WEATHER", "STORM",
        {"weather", "blizzard", "cyclone", "electrical", "gale", "hailstorm", "hurricane", "ice", "monsoon", "rainstorm", "sandstorm", "sleet", "snowstorm", "squall", "storm", "supercell", "tempest", "thunderhead", "thunderstorm", "tornado", "typhoon", "windstorm"}});
    m_mappings.push_back({"WEATHER", "THUNDER",
        {"weather", "boom", "clap", "crack", "crash", "lightening", "lightning", "roar", "roll", "rumble", "thunder", "thunderbolt", "thunderclap", "thunderstorm"}});

    // WHISTLES Category
    m_mappings.push_back({"WHISTLES", "HUMAN", {"whistles", "cheering", "human", "signal", "whistle", "wolf"}});
    m_mappings.push_back({"WHISTLES", "MECHANICAL",
        {"whistles", "mechanical", "bird", "call", "coach", "dog", "lifeguard", "pea", "police", "referee", "shrill", "steam", "teapot", "toot", "train"}});
    m_mappings.push_back({"WHISTLES", "MISC", {"whistles", "misc", "miscellaneous"}});

    // WIND Category
    m_mappings.push_back({"WIND", "DESIGNED",
        {"wind", "artificial", "designed", "machine", "simulated", "synthetic", "tonal"}});
    m_mappings.push_back({"WIND", "GENERAL",
        {"wind", "general", "air", "atmospheric", "breeze", "current", "draft", "flow", "miscellaneous", "windy"}});
    m_mappings.push_back({"WIND", "GUST",
        {"wind", "blast", "blow", "blustery", "breeze", "breezy", "buffet", "buffeting", "crosswind", "downwind", "flurry", "gale", "gales", "gust", "gustation", "gusting", "gusts", "gusty", "headwind", "rush", "strong", "waft", "whirlwinds", "wisps", "zephyr"}});
    m_mappings.push_back({"WIND", "INTERIOR",
        {"wind", "abandoned", "breeze", "current", "door", "draft", "gust", "house", "interior", "moan", "whistle", "window"}});
    m_mappings.push_back({"WIND", "TONAL",
        {"wind", "harmonic", "howl", "howling", "moan", "moaning", "roar", "singing", "tonal", "wail", "whistle"}});
    m_mappings.push_back({"WIND", "TURBULENT",
        {"wind", "buffet", "choppy", "devil", "dust", "gale", "hurricane", "microburst", "slipstream", "squall", "stormy", "strong", "tempest", "tornado", "turbulence", "turbulent", "twister", "typhoon", "unsteady", "violent", "vortex", "whirlwind"}});
    m_mappings.push_back({"WIND", "VEGETATION",
        {"wind", "bending", "blowing", "branches", "foliage", "grass", "leaf", "leaves", "plant", "rustle", "rustling", "soughing", "swaying", "tree", "trees", "vegetation", "whispering"}});

    // WINDOWS Category
    m_mappings.push_back({"WINDOWS", "COVERING",
        {"windows", "covering", "awnings", "blackout", "blind", "blinds", "curtain", "curtains", "drapes", "mini-blinds", "panes", "shades", "shutter", "shutters", "valances", "veils", "venetian"}});
    m_mappings.push_back({"WINDOWS", "HARDWARE",
        {"windows", "hardware", "catches", "cranks", "fasteners", "handles", "hinges", "hooks", "latch", "latches", "lock", "locks", "panes", "sash", "slides", "window"}});
    m_mappings.push_back({"WINDOWS", "KNOCK",
        {"windows", "bang", "knock", "knocking", "pane", "panes", "pound", "pounding", "rap", "rapping", "rattle", "tap", "tapping", "thump"}});
    m_mappings.push_back({"WINDOWS", "METAL", {"windows", "frame", "metal", "panes"}});
    m_mappings.push_back({"WINDOWS", "MISC", {"windows", "misc", "miscellaneous", "panes"}});
    m_mappings.push_back({"WINDOWS", "PLASTIC", {"windows", "frame", "panes", "plastic", "vinyl"}});
    m_mappings.push_back({"WINDOWS", "WOOD", {"windows", "frame", "panes", "wood"}});

    // WINGS Category
    m_mappings.push_back({"WINGS", "BIRD",
        {"wings", "avian", "bird", "feather", "feathered", "flap", "flapping", "flight", "flutter", "flying", "pinion", "wing", "winged", "wingspan", "wingspread"}});
    m_mappings.push_back({"WINGS", "CREATURE",
        {"wings", "angel", "bird", "creature", "dragon", "fairy", "fantastical", "giant", "griffin", "legendary", "monster", "mythical", "phoenix", "sphinx", "supernatural", "wing"}});
    m_mappings.push_back({"WINGS", "INSECT",
        {"wings", "insect", "bee", "beetle", "bug", "butterfly", "cicada", "damselfly", "dragonfly", "firefly", "flies", "fly", "gnats", "grasshopper", "hornet", "insectoid", "katydid", "ladybug", "locust", "mayfly", "mosquito", "moth", "scarab", "wasp"}});
    m_mappings.push_back({"WINGS", "MISC",
        {"wings", "misc", "appendage", "feather", "miscellaneous", "pinion", "winglet", "wingtip"}});

    // WOOD Category
    m_mappings.push_back({"WOOD", "BREAK",
        {"wood", "2x4", "apart", "beam", "board", "break", "breaks", "burst", "chip", "crack", "cracking", "cracks", "crumble", "crunches", "crush", "demolish", "destroy", "disintegrate", "dowel", "fracture", "fractures", "fragment", "hardwood", "joist", "log", "lumber", "plank", "plywood", "rafter", "rips", "shatter", "shattering", "shatters", "smash", "snap", "snapping", "softwood", "splinter", "splintering", "split", "stud", "timber"}});
    m_mappings.push_back({"WOOD", "CRASH & DEBRIS",
        {"wood", "crash & debris", "2x4", "beam", "board", "boards", "broken", "collision", "crash", "debris", "dowel", "fall", "fell", "fragments", "hardwood", "joist", "log", "lumber", "plank", "planks", "plywood", "rafter", "remains", "rubble", "ruins", "shards", "smash", "softwood", "splintered", "splinters", "stud", "timber", "wreckage"}});
    m_mappings.push_back({"WOOD", "FRICTION",
        {"wood", "friction", "2x4", "abrasion", "beam", "board", "creaking", "creaks", "dowel", "grating", "grinding", "hardwood", "joist", "log", "lumber", "plank", "plywood", "rafter", "rasping", "rubbing", "scrapes", "scraping", "scratching", "screech", "screeching", "scuffing", "sliding", "softwood", "squeaks", "stress", "stud", "timber", "wear"}});
    m_mappings.push_back({"WOOD", "HANDLE",
        {"wood", "2x4", "beam", "board", "catch", "clasp", "clench", "clutch", "dowel", "down", "embrace", "grab", "grasp", "grip", "handle", "hardwood", "hold", "joist", "log", "lumber", "operate", "pickup", "plank", "plywood", "rafter", "seize", "set", "softwood", "stud", "take", "throw", "timber", "toss", "use"}});
    m_mappings.push_back({"WOOD", "IMPACT",
        {"wood", "2x4", "bang", "banging", "bash", "beam", "blow", "board", "bonk", "chop", "clang", "clap", "clink", "clunk", "collide", "colliding", "collision", "conk", "crash", "crashing", "dowel", "drop", "dropped", "hardwood", "hit", "hitting", "impact", "impacting", "joist", "jolt", "knock", "log", "lumber", "plank", "plywood", "pound", "rafter", "ram", "slam", "slamming", "smack", "smacking", "softwood", "strike", "striking", "stud", "thrust", "timber"}});
    m_mappings.push_back({"WOOD", "MISC",
        {"wood", "misc", "2x4", "beam", "board", "dowel", "hardwood", "joist", "log", "lumber", "miscellaneous", "plank", "plywood", "rafter", "softwood", "stud", "timber", "tree", "wooden"}});
    m_mappings.push_back({"WOOD", "MOVEMENT",
        {"wood", "movement", "2x4", "beam", "bending", "board", "bowing", "checking", "collapse", "contraction", "cracking", "crowning", "cupping", "deformation", "dowel", "drag", "expansion", "hardwood", "joist", "log", "lumber", "piling", "plank", "plywood", "rafter", "rattle", "rattling", "roll", "shake", "shaking", "shifting", "shrinkage", "softwood", "splitting", "stacking", "stud", "swelling", "timber", "tossing", "twisting", "warping"}});
    m_mappings.push_back({"WOOD", "TONAL",
        {"wood", "2x4", "beam", "board", "bowed", "dowel", "frequency", "hardwood", "harmonic", "joist", "log", "lumber", "melodic", "melodious", "musical", "ping", "pitch", "plank", "plywood", "rafter", "resonance", "resonant", "ring", "shing", "softwood", "sonorous", "sound", "stud", "timber", "timbre", "tonal", "tone"}});

    juce::Logger::writeToLog("UCSCategorySuggester initialized with " +
                            juce::String(m_mappings.size()) + " official UCS v8.2.1 category mappings");
}