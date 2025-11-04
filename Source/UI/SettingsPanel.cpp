/*
  ==============================================================================

    SettingsPanel.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

  ==============================================================================
*/

// Dialog dimensions
constexpr int DIALOG_WIDTH = 700;
constexpr int DIALOG_HEIGHT = 550;
constexpr int BUTTON_AREA_HEIGHT = 50;
constexpr int BUTTON_WIDTH = 100;
constexpr int BUTTON_SPACING = 10;

#include "SettingsPanel.h"
#include "Utils/Settings.h"
#include "Utils/KeymapManager.h"

//==============================================================================
SettingsPanel::SettingsPanel(juce::AudioDeviceManager& deviceManager,
                              juce::ApplicationCommandManager& commandManager,
                              KeymapManager& keymapManager)
    : m_deviceManager(deviceManager),
      m_commandManager(commandManager),
      m_keymapManager(keymapManager),
      m_waveformColorSelector(juce::ColourSelector::showColourAtTop |
                               juce::ColourSelector::showSliders |
                               juce::ColourSelector::showColourspace),
      m_selectionColorSelector(juce::ColourSelector::showColourAtTop |
                                juce::ColourSelector::showSliders |
                                juce::ColourSelector::showColourspace)
{
    // Create audio settings tab
    auto* audioTab = createAudioSettingsTab();
    m_tabbedComponent.addTab("Audio", juce::Colour(0xff3a3a3a), audioTab, true);

    // Create display settings tab
    auto* displayTab = createDisplaySettingsTab();
    m_tabbedComponent.addTab("Display", juce::Colour(0xff3a3a3a), displayTab, true);

    // Create auto-save settings tab
    auto* autoSaveTab = createAutoSaveSettingsTab();
    m_tabbedComponent.addTab("Auto-Save", juce::Colour(0xff3a3a3a), autoSaveTab, true);

    // Create keyboard shortcuts tab
    auto* shortcutsTab = createKeyboardShortcutsTab();
    m_tabbedComponent.addTab("Keyboard Shortcuts", juce::Colour(0xff3a3a3a), shortcutsTab, true);

    addAndMakeVisible(m_tabbedComponent);

    // Setup buttons
    m_okButton.setButtonText("OK");
    m_okButton.addListener(this);
    addAndMakeVisible(m_okButton);

    m_cancelButton.setButtonText("Cancel");
    m_cancelButton.addListener(this);
    addAndMakeVisible(m_cancelButton);

    m_applyButton.setButtonText("Apply");
    m_applyButton.addListener(this);
    addAndMakeVisible(m_applyButton);

    // Load current settings
    loadSettings();

    setSize(DIALOG_WIDTH, DIALOG_HEIGHT);
}

SettingsPanel::~SettingsPanel()
{
}

//==============================================================================
void SettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));
}

void SettingsPanel::resized()
{
    auto bounds = getLocalBounds();

    // Reserve space for buttons at bottom
    auto buttonArea = bounds.removeFromBottom(BUTTON_AREA_HEIGHT).reduced(BUTTON_SPACING);

    // Layout buttons (right to left: Cancel, Apply, OK)
    m_cancelButton.setBounds(buttonArea.removeFromRight(BUTTON_WIDTH));
    buttonArea.removeFromRight(BUTTON_SPACING);
    m_applyButton.setBounds(buttonArea.removeFromRight(BUTTON_WIDTH));
    buttonArea.removeFromRight(BUTTON_SPACING);
    m_okButton.setBounds(buttonArea.removeFromRight(BUTTON_WIDTH));

    // Tabbed component takes remaining space
    m_tabbedComponent.setBounds(bounds.reduced(BUTTON_SPACING));
}

//==============================================================================
void SettingsPanel::buttonClicked(juce::Button* button)
{
    if (button == &m_okButton)
    {
        saveSettings();

        // Apply keyboard shortcut changes
        if (m_shortcutEditor)
            m_shortcutEditor->applyChanges();

        // Close dialog
        if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
        {
            dialogWindow->exitModalState(1);
        }
    }
    else if (button == &m_cancelButton)
    {
        // Revert keyboard shortcut changes
        if (m_shortcutEditor)
            m_shortcutEditor->revertChanges();

        // Close dialog without saving
        if (auto* dialogWindow = findParentComponentOfClass<juce::DialogWindow>())
        {
            dialogWindow->exitModalState(0);
        }
    }
    else if (button == &m_applyButton)
    {
        saveSettings();

        // Apply keyboard shortcut changes
        if (m_shortcutEditor)
            m_shortcutEditor->applyChanges();

        // Dialog stays open
    }
    else if (button == &m_importTemplateButton)
    {
        // Show file chooser for importing a template
        auto chooser = std::make_shared<juce::FileChooser>("Import Keyboard Template",
                                                             juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                             "*.json");

        auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        chooser->launchAsync(flags, [this, chooser](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                if (m_keymapManager.importTemplate(file))
                {
                    // Reload template selector
                    m_templateSelector.clear();
                    auto availableTemplates = m_keymapManager.getAvailableTemplates();
                    int itemId = 1;
                    for (const auto& templateName : availableTemplates)
                    {
                        m_templateSelector.addItem(templateName, itemId++);
                    }

                    // Select the newly imported template
                    juce::String importedName = file.getFileNameWithoutExtension();
                    for (int i = 0; i < m_templateSelector.getNumItems(); ++i)
                    {
                        if (m_templateSelector.getItemText(i) == importedName)
                        {
                            m_templateSelector.setSelectedId(i + 1, juce::sendNotification);
                            break;
                        }
                    }

                    juce::Logger::writeToLog("Imported keyboard template: " + importedName);
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Import Failed",
                        "Failed to import keyboard template. Check that the file is a valid template JSON.",
                        "OK");
                }
            }
        });
    }
    else if (button == &m_exportTemplateButton)
    {
        // Show file chooser for exporting current template
        juce::String currentTemplate = m_keymapManager.getCurrentTemplateName();
        auto defaultName = currentTemplate + ".json";

        auto chooser = std::make_shared<juce::FileChooser>("Export Keyboard Template",
                                                             juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(defaultName),
                                                             "*.json");

        auto flags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting;

        chooser->launchAsync(flags, [this, chooser](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file != juce::File())
            {
                if (m_keymapManager.exportCurrentTemplate(file))
                {
                    juce::Logger::writeToLog("Exported keyboard template: " + file.getFullPathName());

                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::InfoIcon,
                        "Export Successful",
                        "Keyboard template exported successfully to:\n" + file.getFullPathName(),
                        "OK");
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Export Failed",
                        "Failed to export keyboard template. Check disk space and permissions.",
                        "OK");
                }
            }
        });
    }
}

//==============================================================================
void SettingsPanel::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &m_templateSelector)
    {
        // Switch to selected template
        juce::String selectedTemplate = m_templateSelector.getText();

        if (m_keymapManager.loadTemplate(selectedTemplate))
        {
            juce::Logger::writeToLog("Switched to keyboard template: " + selectedTemplate);

            // Update the shortcut editor to reflect the new template
            if (m_shortcutEditor)
            {
                m_shortcutEditor->refreshCommandList(); // Reload and refresh UI
            }
        }
        else
        {
            juce::Logger::writeToLog("ERROR: Failed to load keyboard template: " + selectedTemplate);

            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Template Load Failed",
                "Failed to load keyboard template '" + selectedTemplate + "'. Reverting to previous template.",
                "OK");

            // Revert to current template
            juce::String currentTemplate = m_keymapManager.getCurrentTemplateName();
            for (int i = 0; i < m_templateSelector.getNumItems(); ++i)
            {
                if (m_templateSelector.getItemText(i) == currentTemplate)
                {
                    m_templateSelector.setSelectedId(i + 1, juce::dontSendNotification);
                    break;
                }
            }
        }
    }
}

//==============================================================================
void SettingsPanel::loadSettings()
{
    auto& settings = Settings::getInstance();

    // Load display settings
    juce::Colour waveformColor = juce::Colour::fromString(
        settings.getSetting("display.waveformColor", "ff00ff00").toString());
    m_waveformColorSelector.setCurrentColour(waveformColor, juce::dontSendNotification);

    juce::Colour selectionColor = juce::Colour::fromString(
        settings.getSetting("display.selectionColor", "8800ff00").toString());
    m_selectionColorSelector.setCurrentColour(selectionColor, juce::dontSendNotification);

    // Load auto-save settings
    bool autoSaveEnabled = settings.getSetting("autoSave.enabled", true);
    m_autoSaveEnabled.setToggleState(autoSaveEnabled, juce::dontSendNotification);

    int autoSaveInterval = settings.getSetting("autoSave.intervalMinutes", 5);
    // Find matching interval in combo box
    for (int i = 0; i < m_autoSaveInterval.getNumItems(); ++i)
    {
        if (m_autoSaveInterval.getItemText(i).getIntValue() == autoSaveInterval)
        {
            m_autoSaveInterval.setSelectedId(i + 1, juce::dontSendNotification);
            break;
        }
    }
}

void SettingsPanel::saveSettings()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    auto& settings = Settings::getInstance();

    // Save display settings
    juce::Colour waveformColor = m_waveformColorSelector.getCurrentColour();
    settings.setSetting("display.waveformColor", waveformColor.toString());

    juce::Colour selectionColor = m_selectionColorSelector.getCurrentColour();
    settings.setSetting("display.selectionColor", selectionColor.toString());

    // Save auto-save settings
    settings.setSetting("autoSave.enabled", m_autoSaveEnabled.getToggleState());

    int selectedInterval = m_autoSaveInterval.getText().getIntValue();
    settings.setSetting("autoSave.intervalMinutes", selectedInterval);

    // Persist to disk
    if (settings.save())
    {
        juce::Logger::writeToLog("Settings saved successfully");
    }
    else
    {
        juce::Logger::writeToLog("ERROR: Failed to save settings");
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Settings Error",
            "Failed to save preferences. Check disk space and permissions.",
            "OK");
    }
}

//==============================================================================
// Helper Components for Tab Layout
// These classes provide proper resized() overrides for tab content layout

/**
 * Audio settings tab container.
 * Manages layout of AudioDeviceSelectorComponent.
 */
class AudioSettingsTab : public juce::Component
{
public:
    AudioSettingsTab(juce::AudioDeviceSelectorComponent& audioSettings)
        : m_audioSettings(audioSettings)
    {
        addAndMakeVisible(m_audioSettings);
    }

    void resized() override
    {
        m_audioSettings.setBounds(getLocalBounds().reduced(10));
    }

private:
    juce::AudioDeviceSelectorComponent& m_audioSettings;
};

juce::Component* SettingsPanel::createAudioSettingsTab()
{
    // Create audio device selector
    m_audioSettings = std::make_unique<juce::AudioDeviceSelectorComponent>(
        m_deviceManager,
        0,  // minAudioInputChannels
        2,  // maxAudioInputChannels
        0,  // minAudioOutputChannels
        2,  // maxAudioOutputChannels
        false,  // showMidiInputOptions
        false,  // showMidiOutputSelector
        false,  // showChannelsAsStereoPairs
        false); // hideAdvancedOptionsWithButton

    auto* container = new AudioSettingsTab(*m_audioSettings);
    container->setSize(650, 400);
    return container;
}

/**
 * Display settings tab container.
 * Manages layout of color selectors for waveform and selection colors.
 */
class DisplaySettingsTab : public juce::Component
{
public:
    DisplaySettingsTab(juce::Label& waveformLabel, juce::ColourSelector& waveformSelector,
                       juce::Label& selectionLabel, juce::ColourSelector& selectionSelector)
        : m_waveformLabel(waveformLabel), m_waveformSelector(waveformSelector),
          m_selectionLabel(selectionLabel), m_selectionSelector(selectionSelector)
    {
        addAndMakeVisible(m_waveformLabel);
        addAndMakeVisible(m_waveformSelector);
        addAndMakeVisible(m_selectionLabel);
        addAndMakeVisible(m_selectionSelector);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);

        // Waveform color section
        auto waveformSection = bounds.removeFromTop(220);
        m_waveformLabel.setBounds(waveformSection.removeFromTop(25));
        m_waveformSelector.setBounds(waveformSection.reduced(0, 5));

        bounds.removeFromTop(10); // Spacing

        // Selection color section
        auto selectionSection = bounds.removeFromTop(220);
        m_selectionLabel.setBounds(selectionSection.removeFromTop(25));
        m_selectionSelector.setBounds(selectionSection.reduced(0, 5));
    }

private:
    juce::Label& m_waveformLabel;
    juce::ColourSelector& m_waveformSelector;
    juce::Label& m_selectionLabel;
    juce::ColourSelector& m_selectionSelector;
};

juce::Component* SettingsPanel::createDisplaySettingsTab()
{
    // Waveform color
    m_waveformColorLabel.setText("Waveform Color:", juce::dontSendNotification);
    m_waveformColorLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    m_waveformColorSelector.setCurrentColour(juce::Colours::green, juce::dontSendNotification);

    // Selection color
    m_selectionColorLabel.setText("Selection Color:", juce::dontSendNotification);
    m_selectionColorLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    m_selectionColorSelector.setCurrentColour(juce::Colour(0x8800ff00), juce::dontSendNotification);

    auto* container = new DisplaySettingsTab(m_waveformColorLabel, m_waveformColorSelector,
                                             m_selectionColorLabel, m_selectionColorSelector);
    container->setSize(650, 400);
    return container;
}

/**
 * Auto-save settings tab container.
 * Manages layout of auto-save toggle and interval selector.
 */
class AutoSaveSettingsTab : public juce::Component
{
public:
    AutoSaveSettingsTab(juce::Label& enableLabel, juce::ToggleButton& enableButton,
                        juce::Label& intervalLabel, juce::ComboBox& intervalCombo)
        : m_enableLabel(enableLabel), m_enableButton(enableButton),
          m_intervalLabel(intervalLabel), m_intervalCombo(intervalCombo)
    {
        addAndMakeVisible(m_enableLabel);
        addAndMakeVisible(m_enableButton);
        addAndMakeVisible(m_intervalLabel);
        addAndMakeVisible(m_intervalCombo);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);

        // Auto-save enabled section
        auto enabledRow = bounds.removeFromTop(30);
        m_enableLabel.setBounds(enabledRow.removeFromLeft(200));
        m_enableButton.setBounds(enabledRow);

        bounds.removeFromTop(10); // Spacing

        // Interval section
        auto intervalRow = bounds.removeFromTop(30);
        m_intervalLabel.setBounds(intervalRow.removeFromLeft(200));
        m_intervalCombo.setBounds(intervalRow.removeFromLeft(150));
    }

private:
    juce::Label& m_enableLabel;
    juce::ToggleButton& m_enableButton;
    juce::Label& m_intervalLabel;
    juce::ComboBox& m_intervalCombo;
};

juce::Component* SettingsPanel::createAutoSaveSettingsTab()
{
    // Auto-save enabled checkbox
    m_autoSaveLabel.setText("Enable Auto-Save:", juce::dontSendNotification);
    m_autoSaveLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    m_autoSaveEnabled.setButtonText("Auto-Save Enabled");
    m_autoSaveEnabled.setToggleState(true, juce::dontSendNotification);

    // Auto-save interval combo box
    m_autoSaveIntervalLabel.setText("Auto-Save Interval:", juce::dontSendNotification);
    m_autoSaveIntervalLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    m_autoSaveInterval.addItem("1 minute", 1);
    m_autoSaveInterval.addItem("5 minutes", 2);
    m_autoSaveInterval.addItem("10 minutes", 3);
    m_autoSaveInterval.addItem("15 minutes", 4);
    m_autoSaveInterval.addItem("30 minutes", 5);
    m_autoSaveInterval.setSelectedId(2, juce::dontSendNotification); // Default to 5 minutes

    auto* container = new AutoSaveSettingsTab(m_autoSaveLabel, m_autoSaveEnabled,
                                              m_autoSaveIntervalLabel, m_autoSaveInterval);
    container->setSize(650, 400);
    return container;
}

/**
 * Keyboard shortcuts tab container.
 * Manages layout of template selector, Import/Export buttons, and ShortcutEditorPanel.
 */
class KeyboardShortcutsTab : public juce::Component
{
public:
    KeyboardShortcutsTab(juce::Label& templateLabel, juce::ComboBox& templateSelector,
                         juce::TextButton& importButton, juce::TextButton& exportButton,
                         ShortcutEditorPanel& shortcutEditor)
        : m_templateLabel(templateLabel),
          m_templateSelector(templateSelector),
          m_importButton(importButton),
          m_exportButton(exportButton),
          m_shortcutEditor(shortcutEditor)
    {
        addAndMakeVisible(m_templateLabel);
        addAndMakeVisible(m_templateSelector);
        addAndMakeVisible(m_importButton);
        addAndMakeVisible(m_exportButton);
        addAndMakeVisible(m_shortcutEditor);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);

        // Template selector row at top
        auto templateRow = bounds.removeFromTop(30);
        m_templateLabel.setBounds(templateRow.removeFromLeft(140));
        templateRow.removeFromLeft(10); // Spacing
        m_templateSelector.setBounds(templateRow.removeFromLeft(250));
        templateRow.removeFromLeft(20); // Spacing
        m_importButton.setBounds(templateRow.removeFromLeft(80));
        templateRow.removeFromLeft(10); // Spacing
        m_exportButton.setBounds(templateRow.removeFromLeft(80));

        bounds.removeFromTop(15); // Spacing between template row and editor

        // Shortcut editor takes remaining space
        m_shortcutEditor.setBounds(bounds);
    }

private:
    juce::Label& m_templateLabel;
    juce::ComboBox& m_templateSelector;
    juce::TextButton& m_importButton;
    juce::TextButton& m_exportButton;
    ShortcutEditorPanel& m_shortcutEditor;
};

juce::Component* SettingsPanel::createKeyboardShortcutsTab()
{
    try
    {
        // Template selector label
        m_templateLabel.setText("Keyboard Template:", juce::dontSendNotification);
        m_templateLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        // Template selector combo box
        auto availableTemplates = m_keymapManager.getAvailableTemplates();

        juce::Logger::writeToLog("SettingsPanel: Populating template selector dropdown");
        juce::Logger::writeToLog("SettingsPanel: KeymapManager returned " +
                                juce::String(availableTemplates.size()) + " templates");

        if (availableTemplates.isEmpty())
        {
            juce::Logger::writeToLog("WARNING: No templates found - adding fallback entry");
            m_templateSelector.addItem("Default", 1);
        }
        else
        {
            int itemId = 1;
            for (const auto& templateName : availableTemplates)
            {
                juce::Logger::writeToLog("  Adding template to dropdown: " + templateName);
                m_templateSelector.addItem(templateName, itemId++);
            }
        }

        juce::Logger::writeToLog("SettingsPanel: Template selector now has " +
                                juce::String(m_templateSelector.getNumItems()) + " items");

        // Set current template
        juce::String currentTemplate = m_keymapManager.getCurrentTemplateName();
        for (int i = 0; i < m_templateSelector.getNumItems(); ++i)
        {
            if (m_templateSelector.getItemText(i) == currentTemplate)
            {
                m_templateSelector.setSelectedId(i + 1, juce::dontSendNotification);
                break;
            }
        }

        m_templateSelector.addListener(this);

        // Import/Export buttons
        m_importTemplateButton.setButtonText("Import...");
        m_importTemplateButton.addListener(this);

        m_exportTemplateButton.setButtonText("Export...");
        m_exportTemplateButton.addListener(this);

        // Create shortcut editor panel
        juce::Logger::writeToLog("Creating ShortcutEditorPanel...");
        m_shortcutEditor = std::make_unique<ShortcutEditorPanel>(m_commandManager);
        juce::Logger::writeToLog("ShortcutEditorPanel created successfully");

        auto* container = new KeyboardShortcutsTab(m_templateLabel, m_templateSelector,
                                                   m_importTemplateButton, m_exportTemplateButton,
                                                   *m_shortcutEditor);
        container->setSize(650, 400);
        return container;
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog("EXCEPTION in createKeyboardShortcutsTab(): " + juce::String(e.what()));

        // Return a simple error message component
        auto* errorLabel = new juce::Label();
        errorLabel->setText("Error loading keyboard shortcuts tab: " + juce::String(e.what()), juce::dontSendNotification);
        errorLabel->setColour(juce::Label::textColourId, juce::Colours::red);
        errorLabel->setSize(650, 400);
        return errorLabel;
    }
    catch (...)
    {
        juce::Logger::writeToLog("UNKNOWN EXCEPTION in createKeyboardShortcutsTab()");

        // Return a simple error message component
        auto* errorLabel = new juce::Label();
        errorLabel->setText("Unknown error loading keyboard shortcuts tab", juce::dontSendNotification);
        errorLabel->setColour(juce::Label::textColourId, juce::Colours::red);
        errorLabel->setSize(650, 400);
        return errorLabel;
    }
}

//==============================================================================
void SettingsPanel::showDialog(juce::Component* parentComponent,
                                juce::AudioDeviceManager& deviceManager,
                                juce::ApplicationCommandManager& commandManager,
                                KeymapManager& keymapManager)
{
    auto* settingsPanel = new SettingsPanel(deviceManager, commandManager, keymapManager);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(settingsPanel);
    options.dialogTitle = "Preferences";
    options.dialogBackgroundColour = juce::Colour(0xff2a2a2a);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.useBottomRightCornerResizer = false;

    // Center over parent component
    if (parentComponent != nullptr)
    {
        auto parentBounds = parentComponent->getScreenBounds();
        auto dialogBounds = juce::Rectangle<int>(0, 0, DIALOG_WIDTH, DIALOG_HEIGHT);
        dialogBounds.setCentre(parentBounds.getCentre());
        options.content->setBounds(dialogBounds);
    }

    // Launch modal dialog
    // Note: launchAsync() creates a modeless window, but the OK/Cancel buttons
    // call exitModalState() which works because the window enters modal state
    // internally when created via LaunchOptions
    options.launchAsync();
}
