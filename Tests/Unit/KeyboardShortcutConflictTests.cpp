#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/Commands/CommandIDs.h"
#include "../../Source/Utils/KeymapManager.h"
#include <cassert>
#include <iostream>
#include <map>
#include <vector>

class ConflictTestApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "ConflictTest"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    void initialise(const juce::String&) override {}
    void shutdown() override {}
};

static ConflictTestApp conflictTestApp;

struct KeyCombination
{
    int keyCode;
    juce::ModifierKeys modifiers;
    
    bool operator<(const KeyCombination& other) const
    {
        if (keyCode != other.keyCode)
            return keyCode < other.keyCode;
        return modifiers.getRawFlags() < other.modifiers.getRawFlags();
    }
    
    juce::String toString() const
    {
        juce::String result;
        if (modifiers.isCommandDown()) result += "Cmd+";
        if (modifiers.isShiftDown()) result += "Shift+";
        if (modifiers.isAltDown()) result += "Alt+";
        result += juce::String::charToString((juce::juce_wchar)keyCode);
        return result;
    }
};

std::vector<juce::String> detectConflicts(juce::ApplicationCommandManager& commandManager)
{
    std::map<KeyCombination, std::vector<juce::String>> keyToCommands;
    std::vector<juce::String> conflicts;
    
    juce::Array<juce::CommandID> commands;
    commands.addArray(commandManager.getCommandIDs());
    
    for (auto commandID : commands)
    {
        juce::ApplicationCommandInfo info(commandID);
        commandManager.getCommandInfo(commandID, info);
        
        for (int i = 0; i < info.defaultKeypresses.size(); ++i)
        {
            auto keyPress = info.defaultKeypresses.getReference(i);
            KeyCombination combo;
            combo.keyCode = keyPress.getKeyCode();
            combo.modifiers = keyPress.getModifiers();
            
            keyToCommands[combo].push_back(info.shortName);
        }
    }
    
    for (const auto& pair : keyToCommands)
    {
        if (pair.second.size() > 1)
        {
            juce::String conflict = pair.first.toString() + ": ";
            for (size_t i = 0; i < pair.second.size(); ++i)
            {
                conflict += pair.second[i];
                if (i < pair.second.size() - 1)
                    conflict += ", ";
            }
            conflicts.push_back(conflict);
        }
    }
    
    return conflicts;
}

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI scopedJuce;
    
    KeymapManager keymapManager;
    juce::ApplicationCommandManager commandManager;
    
    // Register all commands (simplified - would need full command registration)
    commandManager.registerAllCommandsForTarget(nullptr);
    
    auto templates = keymapManager.getAvailableTemplates();
    int totalConflicts = 0;
    
    std::cout << "\n=== Keyboard Shortcut Conflict Test ===\n\n";
    
    for (const auto& templateName : templates)
    {
        std::cout << "Testing template: " << templateName << "\n";
        
        if (keymapManager.applyTemplate(templateName, commandManager))
        {
            auto conflicts = detectConflicts(commandManager);
            
            if (conflicts.empty())
            {
                std::cout << "  ✅ NO CONFLICTS\n";
            }
            else
            {
                std::cout << "  ❌ " << conflicts.size() << " conflicts found:\n";
                for (const auto& conflict : conflicts)
                {
                    std::cout << "     - " << conflict << "\n";
                }
                totalConflicts += conflicts.size();
            }
        }
        else
        {
            std::cout << "  ❌ Failed to apply template\n";
        }
        
        std::cout << "\n";
    }
    
    std::cout << "=== Summary ===\n";
    std::cout << "Total conflicts across all templates: " << totalConflicts << "\n";
    
    return (totalConflicts == 0) ? 0 : 1;
}
