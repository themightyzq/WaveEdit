/*
  ==============================================================================

    PluginController.cpp
    Copyright (C) 2025 ZQ SFX

  ==============================================================================
*/

#include "PluginController.h"

#include "ChainWindowListener.h"
#include "DSPController.h"
#include "../Utils/Document.h"
#include "../Audio/AudioEngine.h"
#include "../Plugins/PluginChain.h"
#include "../UI/PluginManagerDialog.h"
#include "../UI/PluginChainWindow.h"
#include "../UI/AutomationLanesPanel.h"
#include "../UI/ErrorDialog.h"

void PluginController::showPluginManagerDialog(Document* currentDoc)
{
    PluginManagerDialog dialog;

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Plugin Manager";
    options.dialogBackgroundColour = juce::Colour(0xff1e1e1e);
    options.content.setNonOwned(&dialog);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;

    options.runModal();

    if (! dialog.wasAddClicked())
        return;

    const auto* selectedPlugin = dialog.getSelectedPlugin();
    if (selectedPlugin == nullptr || currentDoc == nullptr)
        return;

    auto& chain = currentDoc->getAudioEngine().getPluginChain();
    const int index = chain.addPlugin(*selectedPlugin);
    if (index >= 0)
    {
        currentDoc->getAudioEngine().setPluginChainEnabled(true);
    }
    else
    {
        ErrorDialog::show("Plugin Error",
                          "Failed to load plugin: " + selectedPlugin->name);
    }
}

void PluginController::showPluginChainPanel(Document* currentDoc,
                                            juce::ApplicationCommandManager& commandManager,
                                            DSPController& dspController,
                                            std::function<Document*()> currentDocAccessor)
{
    if (currentDoc == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Plugin Chain",
            "Please open an audio file first.",
            "OK");
        return;
    }

    auto& chain = currentDoc->getAudioEngine().getPluginChain();
    auto* chainWindow = new PluginChainWindow(&chain, &currentDoc->getAutomationManager());
    auto* window = chainWindow->showInWindow(&commandManager);

    auto* listener = new ChainWindowListener(
        commandManager,
        &chain,
        &currentDoc->getAudioEngine(),
        window,
        [&dspController, currentDocAccessor](bool stereo, bool tail, double len)
        {
            if (auto* doc = currentDocAccessor())
                dspController.applyPluginChainToSelectionWithOptions(doc, stereo, tail, len);
        });
    chainWindow->setListener(listener);

    m_listeners.add(listener);
}

void PluginController::notifyDocumentClosed(Document* doc)
{
    if (doc == nullptr) return;

    auto* chain = &doc->getAudioEngine().getPluginChain();

    for (int i = m_listeners.size(); --i >= 0;)
    {
        auto* listener = m_listeners.getUnchecked(i);
        if (listener->isForChain(chain))
        {
            listener->documentClosed();
            m_listeners.remove(i);
        }
    }
}

void PluginController::clearListeners()
{
    for (auto* listener : m_listeners)
        listener->documentClosed();
    m_listeners.clear();
}

void PluginController::showAutomationLanesPanel(Document* currentDoc,
                                                juce::ApplicationCommandManager& commandManager,
                                                int scrollToPluginIndex,
                                                int scrollToParameterIndex)
{
    if (currentDoc == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Automation Lanes",
            "Please open an audio file first.",
            "OK");
        return;
    }

    // Existing window? Bring it forward (and rebind to current doc if
    // it's pointing at a different one).
    if (m_automationLanesWindow != nullptr && m_automationLanesPanel != nullptr)
    {
        m_automationLanesWindow->setVisible(true);
        m_automationLanesWindow->toFront(true);
        if (scrollToPluginIndex >= 0 && scrollToParameterIndex >= 0)
            m_automationLanesPanel->scrollToLane(scrollToPluginIndex, scrollToParameterIndex);
        return;
    }

    auto* panel = new AutomationLanesPanel(&currentDoc->getAutomationManager(),
                                            &currentDoc->getAudioEngine(),
                                            &currentDoc->getUndoManager());
    auto* window = panel->showInWindow(&commandManager);
    m_automationLanesPanel  = panel;
    m_automationLanesWindow = window;

    if (scrollToPluginIndex >= 0 && scrollToParameterIndex >= 0)
        panel->scrollToLane(scrollToPluginIndex, scrollToParameterIndex);
}
