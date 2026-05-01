/*
  ==============================================================================

    AutoSaveRecovery.cpp
    Copyright (C) 2025 ZQ SFX

  ==============================================================================
*/

#include "AutoSaveRecovery.h"
#include "Settings.h"

namespace AutoSaveRecovery
{

juce::File getAutoSaveDirectory()
{
    return Settings::getInstance().getSettingsDirectory().getChildFile("autosave");
}

static juce::String prefixFor(const juce::File& originalFile)
{
    return "autosave_" + originalFile.getFileNameWithoutExtension() + "_";
}

juce::Array<juce::File> findOrphanedAutoSaves(const juce::File& autoSaveDir,
                                               const juce::File& originalFile)
{
    juce::Array<juce::File> result;
    if (! autoSaveDir.isDirectory())
        return result;

    const auto basePrefix    = prefixFor(originalFile);
    const auto originalMTime = originalFile.getLastModificationTime();

    juce::Array<juce::File> candidates;
    autoSaveDir.findChildFiles(candidates, juce::File::findFiles, false, "autosave_*.wav");

    for (const auto& f : candidates)
    {
        if (! f.getFileName().startsWith(basePrefix))
            continue;
        if (f.getLastModificationTime() <= originalMTime)
            continue;  // stale: the original was saved after this auto-save
        result.add(f);
    }

    // Newest first.
    struct ByMTimeDesc
    {
        static int compareElements(const juce::File& a, const juce::File& b)
        {
            const auto ta = a.getLastModificationTime();
            const auto tb = b.getLastModificationTime();
            if (ta > tb) return -1;
            if (ta < tb) return 1;
            return 0;
        }
    };
    ByMTimeDesc cmp;
    result.sort(cmp, false);
    return result;
}

juce::Array<juce::File> findOrphanedAutoSaves(const juce::File& originalFile)
{
    return findOrphanedAutoSaves(getAutoSaveDirectory(), originalFile);
}

void deleteAutoSavesFor(const juce::File& autoSaveDir,
                        const juce::File& originalFile)
{
    if (! autoSaveDir.isDirectory())
        return;

    const auto basePrefix = prefixFor(originalFile);

    juce::Array<juce::File> candidates;
    autoSaveDir.findChildFiles(candidates, juce::File::findFiles, false, "autosave_*.wav");
    for (const auto& f : candidates)
        if (f.getFileName().startsWith(basePrefix))
            f.deleteFile();
}

void deleteAutoSavesFor(const juce::File& originalFile)
{
    deleteAutoSavesFor(getAutoSaveDirectory(), originalFile);
}

}  // namespace AutoSaveRecovery
