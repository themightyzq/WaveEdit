/*
  ==============================================================================

    GraphicalEQEditor_Presets.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Split out from GraphicalEQEditor.cpp under CLAUDE.md §7.5 (file-size
    cap). Hosts the preset-management methods — populating the combo
    list, applying a selection to the live params, the Save dialog flow
    (with overwrite confirmation), and Delete with its confirmation
    cycle. The visualisation, mouse/keyboard interaction, and DSP
    plumbing all stay in GraphicalEQEditor.cpp.

  ==============================================================================
*/

#include "GraphicalEQEditor.h"
#include "../Audio/AudioEngine.h"

void GraphicalEQEditor::refreshPresetList()
{
    m_presetComboBox.clear(juce::dontSendNotification);

    int itemId = 1;

    // Add factory presets section
    auto factoryPresets = EQPresetManager::getFactoryPresetNames();
    if (!factoryPresets.isEmpty())
    {
        m_presetComboBox.addSectionHeading("Factory Presets");

        for (const auto& name : factoryPresets)
        {
            m_presetComboBox.addItem(name, itemId++);
        }

        m_presetComboBox.addSeparator();
    }

    // Add user presets section
    auto userPresets = EQPresetManager::getAvailablePresets();
    if (!userPresets.isEmpty())
    {
        m_presetComboBox.addSectionHeading("User Presets");

        for (const auto& name : userPresets)
        {
            m_presetComboBox.addItem(name, itemId++);
        }
    }

    // Update delete button state - only enabled for user presets
    m_deletePresetButton.setEnabled(false);
}

void GraphicalEQEditor::presetSelected()
{
    juce::String selectedName = m_presetComboBox.getText();

    if (selectedName.isEmpty())
        return;

    // Check if it's a factory preset
    if (EQPresetManager::isFactoryPreset(selectedName))
    {
        m_params = EQPresetManager::getFactoryPreset(selectedName);
        m_deletePresetButton.setEnabled(false);
    }
    else
    {
        // Try to load user preset
        if (EQPresetManager::loadPreset(m_params, selectedName))
        {
            m_deletePresetButton.setEnabled(true);
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Load Failed",
                "Could not load preset: " + selectedName,
                "OK"
            );
            return;
        }
    }

    // Update control points
    updateControlPointPositions();

    // Update EQ processor for visualization
    if (m_eqProcessor)
    {
        m_eqProcessor->setParameters(m_params);
        m_eqProcessor->updateCoefficientsForVisualization();
    }

    // Update preview if active
    if (m_previewActive && m_audioEngine)
    {
        m_audioEngine->setDynamicEQPreview(m_params, true);
    }

    repaint();
}

void GraphicalEQEditor::savePreset()
{
    // Show dialog to get preset name
    auto* alertWindow = new juce::AlertWindow(
        "Save EQ Preset",
        "Enter a name for this preset:",
        juce::MessageBoxIconType::QuestionIcon
    );

    alertWindow->addTextEditor("presetName", "", "Preset Name:");
    alertWindow->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, alertWindow](int result)
        {
            if (result == 1)
            {
                juce::String presetName = alertWindow->getTextEditorContents("presetName").trim();

                if (presetName.isEmpty())
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Invalid Name",
                        "Please enter a valid preset name.",
                        "OK"
                    );
                }
                else if (EQPresetManager::isFactoryPreset(presetName))
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Reserved Name",
                        "Cannot use a factory preset name. Please choose a different name.",
                        "OK"
                    );
                }
                else
                {
                    // Check if preset already exists
                    if (EQPresetManager::presetExists(presetName))
                    {
                        juce::AlertWindow::showOkCancelBox(
                            juce::MessageBoxIconType::QuestionIcon,
                            "Overwrite Preset?",
                            "A preset named \"" + presetName + "\" already exists. Overwrite it?",
                            "Overwrite",
                            "Cancel",
                            nullptr,
                            juce::ModalCallbackFunction::create(
                                [this, presetName](int overwriteResult)
                                {
                                    if (overwriteResult == 1)
                                    {
                                        doSavePreset(presetName);
                                    }
                                }
                            )
                        );
                    }
                    else
                    {
                        doSavePreset(presetName);
                    }
                }
            }

            delete alertWindow;
        }
    ), true);
}

void GraphicalEQEditor::doSavePreset(const juce::String& presetName)
{
    if (EQPresetManager::savePreset(m_params, presetName))
    {
        refreshPresetList();

        // Select the newly saved preset
        for (int i = 0; i < m_presetComboBox.getNumItems(); ++i)
        {
            if (m_presetComboBox.getItemText(i) == presetName)
            {
                m_presetComboBox.setSelectedItemIndex(i, juce::dontSendNotification);
                m_deletePresetButton.setEnabled(true);
                break;
            }
        }

        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Preset Saved",
            "Preset \"" + presetName + "\" saved successfully.",
            "OK"
        );
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Save Failed",
            "Could not save preset. Please check disk permissions.",
            "OK"
        );
    }
}

void GraphicalEQEditor::deletePreset()
{
    juce::String selectedName = m_presetComboBox.getText();

    if (selectedName.isEmpty())
        return;

    // Cannot delete factory presets
    if (EQPresetManager::isFactoryPreset(selectedName))
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Cannot Delete",
            "Factory presets cannot be deleted.",
            "OK"
        );
        return;
    }

    // Confirm deletion
    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::QuestionIcon,
        "Delete Preset?",
        "Are you sure you want to delete the preset \"" + selectedName + "\"?",
        "Delete",
        "Cancel",
        nullptr,
        juce::ModalCallbackFunction::create(
            [this, selectedName](int result)
            {
                if (result == 1)
                {
                    if (EQPresetManager::deletePreset(selectedName))
                    {
                        refreshPresetList();
                        m_deletePresetButton.setEnabled(false);
                    }
                    else
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            "Delete Failed",
                            "Could not delete preset.",
                            "OK"
                        );
                    }
                }
            }
        )
    );
}
