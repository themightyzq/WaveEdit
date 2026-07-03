/*
  ==============================================================================

    SidecarNotifications.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

#include "SidecarNotifications.h"
#include "WaveformDisplay.h"
#include "../Utils/Document.h"
#include "../Utils/Settings.h"

namespace
{
    // Settings key: true means the user opted out of the sidecar warning.
    const juce::String kSuppressKey = "warnings.sidecarRequired";
}

namespace SidecarNotifications
{
    void warnIfSidecarNowRequired(Document& doc, juce::Component* parent)
    {
        // One-shot: only fire on the embeddable -> sidecar-required transition.
        if (!doc.takeSidecarRequirementTransition())
            return;

        if (static_cast<bool>(Settings::getInstance().getSetting(kSuppressKey, false)))
            return;

        const juce::String companionName = doc.getFilename() + ".wav.regions.json";

        const juce::String message =
            "This document now uses a companion file. Custom marker/region colors "
            "and names with non-standard characters are stored in\n\n    "
            + companionName
            + "\n\nnext to the audio file. Keep it alongside the WAV when you copy or "
              "move the file, or those extras will be lost. The audio file itself still "
              "carries all marker and region positions on its own.";

        // Custom AlertWindow so we can host a "Don't show this again" toggle.
        auto* aw = new juce::AlertWindow("Companion File Created",
                                         message,
                                         juce::MessageBoxIconType::InfoIcon,
                                         parent);

        auto toggle = std::make_unique<juce::ToggleButton>("Don't show this again");
        toggle->setSize(240, 24);
        auto* togglePtr = toggle.get();
        aw->addCustomComponent(togglePtr);

        aw->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));

        // Keep the toggle alive for the window's lifetime via the callback's
        // capture; AlertWindow does not own custom components. deleteWhen
        // Dismissed deletes the window after the callback runs.
        aw->enterModalState(
            true,
            juce::ModalCallbackFunction::create(
                [togglePtr, kept = std::move(toggle)](int) mutable
                {
                    if (togglePtr->getToggleState())
                        Settings::getInstance().setSetting(kSuppressKey, true);
                }),
            true);
    }

    void promptOnStaleSidecar(Document& doc,
                              juce::Component* parent,
                              std::function<void()> onResolved)
    {
        if (!doc.hasSidecarConflict())
            return;

        auto options =
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::QuestionIcon)
                .withTitle("Markers Changed Outside WaveEdit")
                .withMessage("This file's markers changed outside WaveEdit -- use the "
                             "file's embedded markers or WaveEdit's sidecar?")
                .withButton("Use File's Markers")   // result 1
                .withButton("Use WaveEdit's")       // result 2
                .withButton("Cancel")               // result 0
                .withAssociatedComponent(parent);

        // The prompt is async and the tab can be closed before the user
        // answers -- guard the Document with its WaveformDisplay lifeline
        // (the app-wide C6 idiom) so we never touch a freed document.
        juce::Component::SafePointer<WaveformDisplay> lifeline(&doc.getWaveformDisplay());
        auto* docPtr = &doc;

        juce::AlertWindow::showAsync(
            options,
            [docPtr, lifeline, onResolved](int result)
            {
                if (lifeline.getComponent() == nullptr)
                    return;   // document closed while the prompt was open

                if (result == 1)
                    docPtr->adoptEmbeddedMarkersAndRegions();  // file wins
                else
                    docPtr->clearSidecarConflict();            // keep sidecar (incl. Cancel)

                if (onResolved)
                    onResolved();
            });
    }
}
