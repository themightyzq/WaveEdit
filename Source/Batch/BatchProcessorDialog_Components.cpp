/*
  ==============================================================================

    BatchProcessorDialog_Components.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    Split out from BatchProcessorDialog.cpp under CLAUDE.md §7.5
    (file-size cap). Hosts the three helper component / model classes
    used inside the dialog: DSPOperationComponent (one row in the DSP
    chain), DSPChainPanel (the drag-reorderable chain), and
    BatchProcessorDialog::FileListModel (the file-list paint + tooltip
    model). The dialog itself stays in BatchProcessorDialog.cpp.

  ==============================================================================
*/

#include "BatchProcessorDialog.h"
#include "../UI/UIConstants.h"

namespace waveedit
{

// =============================================================================
// DSPOperationComponent
// =============================================================================

DSPOperationComponent::DSPOperationComponent(int index)
    : m_index(index)
    , m_enabledToggle("Enabled")
    , m_paramLabel("paramLabel", "Value:")
    , m_detailsButton("...")
    , m_moveUpButton(juce::String(juce::CharPointer_UTF8("\xe2\x96\xb2")))    // ▲
    , m_moveDownButton(juce::String(juce::CharPointer_UTF8("\xe2\x96\xbc")))  // ▼
    , m_removeButton("X")
{
    // Enabled toggle
    m_enabledToggle.setToggleState(true, juce::dontSendNotification);
    addAndMakeVisible(m_enabledToggle);

    // Operation combo
    m_operationCombo.addItem("Gain", static_cast<int>(BatchDSPOperation::GAIN) + 1);
    m_operationCombo.addItem("Normalize", static_cast<int>(BatchDSPOperation::NORMALIZE) + 1);
    m_operationCombo.addItem("DC Offset", static_cast<int>(BatchDSPOperation::DC_OFFSET) + 1);
    m_operationCombo.addItem("Fade In", static_cast<int>(BatchDSPOperation::FADE_IN) + 1);
    m_operationCombo.addItem("Fade Out", static_cast<int>(BatchDSPOperation::FADE_OUT) + 1);
    m_operationCombo.addItem("Reverse", static_cast<int>(BatchDSPOperation::REVERSE) + 1);
    m_operationCombo.addItem("Invert", static_cast<int>(BatchDSPOperation::INVERT) + 1);
    m_operationCombo.setSelectedId(1);
    m_operationCombo.addListener(this);
    addAndMakeVisible(m_operationCombo);

    // Parameter label
    addAndMakeVisible(m_paramLabel);

    // Parameter slider
    m_paramSlider.setRange(-24.0, 24.0, 0.1);
    m_paramSlider.setValue(0.0);
    m_paramSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    m_paramSlider.onValueChange = [this]()
    {
        if (onSettingsChanged)
            onSettingsChanged();
    };
    addAndMakeVisible(m_paramSlider);

    // Curve combo (for fades)
    m_curveCombo.addItem("Linear", 1);
    m_curveCombo.addItem("Exponential", 2);
    m_curveCombo.addItem("Logarithmic", 3);
    m_curveCombo.addItem("S-Curve", 4);
    m_curveCombo.setSelectedId(1);
    m_curveCombo.onChange = [this]()
    {
        if (onSettingsChanged)
            onSettingsChanged();
    };
    addChildComponent(m_curveCombo);

    // Details button
    m_detailsButton.setTooltip("Show operation details");
    m_detailsButton.onClick = [this]() { showDetailsPopup(); };
    addAndMakeVisible(m_detailsButton);

    // Move up button
    m_moveUpButton.setTooltip("Move operation up");
    m_moveUpButton.onClick = [this]()
    {
        if (onMoveUpClicked)
            onMoveUpClicked();
    };
    addAndMakeVisible(m_moveUpButton);

    // Move down button
    m_moveDownButton.setTooltip("Move operation down");
    m_moveDownButton.onClick = [this]()
    {
        if (onMoveDownClicked)
            onMoveDownClicked();
    };
    addAndMakeVisible(m_moveDownButton);

    // Remove button
    m_removeButton.onClick = [this]()
    {
        if (onRemoveClicked)
            onRemoveClicked();
    };
    addAndMakeVisible(m_removeButton);

    updateParameterVisibility();
}

void DSPOperationComponent::setMoveButtonsEnabled(bool canMoveUp, bool canMoveDown)
{
    m_moveUpButton.setEnabled(canMoveUp);
    m_moveDownButton.setEnabled(canMoveDown);
}

void DSPOperationComponent::setDragHighlight(bool highlighted)
{
    if (m_dragHighlight != highlighted)
    {
        m_dragHighlight = highlighted;
        repaint();
    }
}

void DSPOperationComponent::mouseDown(const juce::MouseEvent& e)
{
    // Only start drag if clicking in the leftmost area (drag handle zone)
    // or if shift is held down to allow dragging from anywhere
    if (e.mods.isShiftDown() || e.x < 15)
    {
        m_dragStartPos = e.getPosition();
        m_isDragging = false;
    }
}

void DSPOperationComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 5 && !m_isDragging)
    {
        // Check if we started in the drag zone
        if (e.mods.isShiftDown() || m_dragStartPos.x < 15)
        {
            m_isDragging = true;

            // Find the DSPChainPanel parent
            if (auto* dspPanel = findParentComponentOfClass<DSPChainPanel>())
            {
                // Create a drag description with our index
                juce::var description;
                description = juce::String("DSPOperation:") + juce::String(m_index);

                // Start the drag
                dspPanel->startDragging(description, this);
            }
        }
    }
}

void DSPOperationComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    m_isDragging = false;
}

void DSPOperationComponent::showDetailsPopup()
{
    auto op = static_cast<BatchDSPOperation>(m_operationCombo.getSelectedId() - 1);

    juce::String title;
    juce::String description;
    juce::String currentValue;

    switch (op)
    {
        case BatchDSPOperation::GAIN:
            title = "Gain Adjustment";
            description = "Adjusts the volume level of the audio.\n\n"
                          "Positive values increase volume (boost).\n"
                          "Negative values decrease volume (cut).\n\n"
                          "Tip: Use normalize instead if you want to hit a specific peak level.";
            currentValue = "Current setting: " + juce::String(m_paramSlider.getValue(), 1) + " dB";
            break;

        case BatchDSPOperation::NORMALIZE:
            title = "Peak Normalization";
            description = "Adjusts the audio so the loudest peak hits the target level.\n\n"
                          "0 dB = Maximum digital level\n"
                          "-0.3 dB = Industry standard headroom\n"
                          "-3 dB = Conservative headroom for lossy encoding\n\n"
                          "Note: This is peak normalization. For loudness normalization,\n"
                          "use a loudness plugin in the plugin chain.";
            currentValue = "Target peak: " + juce::String(m_paramSlider.getValue(), 1) + " dB";
            break;

        case BatchDSPOperation::DC_OFFSET:
            title = "DC Offset Removal";
            description = "Removes any DC offset (constant voltage bias) from the audio.\n\n"
                          "DC offset can cause clicks when editing and reduces headroom.\n"
                          "This operation centers the waveform around zero.\n\n"
                          "Tip: Apply this before normalization for best results.";
            currentValue = "No parameters - automatically removes any DC bias.";
            break;

        case BatchDSPOperation::FADE_IN:
            title = "Fade In";
            description = "Gradually increases volume from silence at the start.\n\n"
                          "Curve types:\n"
                          "• Linear: Constant rate of change\n"
                          "• Exponential: Starts slow, ends fast\n"
                          "• Logarithmic: Starts fast, ends slow\n"
                          "• S-Curve: Smooth start and end";
            currentValue = "Duration: " + juce::String(m_paramSlider.getValue(), 0) + " ms\n"
                          "Curve: " + m_curveCombo.getText();
            break;

        case BatchDSPOperation::FADE_OUT:
            title = "Fade Out";
            description = "Gradually decreases volume to silence at the end.\n\n"
                          "Curve types:\n"
                          "• Linear: Constant rate of change\n"
                          "• Exponential: Starts slow, ends fast\n"
                          "• Logarithmic: Starts fast, ends slow\n"
                          "• S-Curve: Smooth start and end";
            currentValue = "Duration: " + juce::String(m_paramSlider.getValue(), 0) + " ms\n"
                          "Curve: " + m_curveCombo.getText();
            break;

        default:
            title = "Operation Details";
            description = "No additional details available for this operation.";
            currentValue = "";
            break;
    }

    // Build full message
    juce::String message = description;
    if (currentValue.isNotEmpty())
        message += "\n\n" + currentValue;

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        title,
        message,
        "OK"
    );
}

void DSPOperationComponent::paint(juce::Graphics& g)
{
    // Fill background - brighter if drag highlighted
    if (m_dragHighlight)
        g.fillAll(juce::Colour(0xff4a5a4a));  // Subtle green tint for drop target
    else
        g.fillAll(juce::Colour(0xff383838));

    // Border
    g.setColour(m_dragHighlight ? juce::Colour(0xff88aa88) : juce::Colour(0xff505050));
    g.drawRect(getLocalBounds(), 1);

    // Draw drag handle indicator (three horizontal lines on the left)
    g.setColour(juce::Colour(0xff707070));
    auto handleArea = getLocalBounds().removeFromLeft(12);
    int midY = handleArea.getCentreY();
    for (int i = -1; i <= 1; ++i)
    {
        int y = midY + i * 4;
        g.drawHorizontalLine(y, 3.0f, 9.0f);
    }
}

void DSPOperationComponent::resized()
{
    auto area = getLocalBounds().reduced(5);

    m_enabledToggle.setBounds(area.removeFromLeft(70));
    area.removeFromLeft(5);

    m_operationCombo.setBounds(area.removeFromLeft(100));
    area.removeFromLeft(10);

    // Right side buttons: Details | Move Up | Move Down | Remove
    m_removeButton.setBounds(area.removeFromRight(24));
    area.removeFromRight(3);
    m_moveDownButton.setBounds(area.removeFromRight(24));
    area.removeFromRight(3);
    m_moveUpButton.setBounds(area.removeFromRight(24));
    area.removeFromRight(5);
    m_detailsButton.setBounds(area.removeFromRight(28));
    area.removeFromRight(5);

    if (m_curveCombo.isVisible())
    {
        m_curveCombo.setBounds(area.removeFromRight(100));
        area.removeFromRight(5);
    }

    m_paramLabel.setBounds(area.removeFromLeft(50));
    m_paramSlider.setBounds(area);
}

void DSPOperationComponent::comboBoxChanged(juce::ComboBox* comboBox)
{
    if (comboBox == &m_operationCombo)
    {
        updateParameterVisibility();
        if (onSettingsChanged)
            onSettingsChanged();
    }
}

BatchDSPSettings DSPOperationComponent::getSettings() const
{
    BatchDSPSettings settings;
    settings.enabled = m_enabledToggle.getToggleState();
    settings.operation = static_cast<BatchDSPOperation>(m_operationCombo.getSelectedId() - 1);

    switch (settings.operation)
    {
        case BatchDSPOperation::GAIN:
            settings.gainDb = static_cast<float>(m_paramSlider.getValue());
            break;
        case BatchDSPOperation::NORMALIZE:
            settings.normalizeTargetDb = static_cast<float>(m_paramSlider.getValue());
            break;
        case BatchDSPOperation::FADE_IN:
        case BatchDSPOperation::FADE_OUT:
            settings.fadeDurationMs = static_cast<float>(m_paramSlider.getValue());
            settings.fadeType = m_curveCombo.getSelectedId() - 1;
            break;
        default:
            break;
    }

    return settings;
}

void DSPOperationComponent::setSettings(const BatchDSPSettings& settings)
{
    m_enabledToggle.setToggleState(settings.enabled, juce::dontSendNotification);
    m_operationCombo.setSelectedId(static_cast<int>(settings.operation) + 1, juce::dontSendNotification);

    switch (settings.operation)
    {
        case BatchDSPOperation::GAIN:
            m_paramSlider.setValue(settings.gainDb, juce::dontSendNotification);
            break;
        case BatchDSPOperation::NORMALIZE:
            m_paramSlider.setValue(settings.normalizeTargetDb, juce::dontSendNotification);
            break;
        case BatchDSPOperation::FADE_IN:
        case BatchDSPOperation::FADE_OUT:
            m_paramSlider.setValue(settings.fadeDurationMs, juce::dontSendNotification);
            m_curveCombo.setSelectedId(settings.fadeType + 1, juce::dontSendNotification);
            break;
        default:
            break;
    }

    updateParameterVisibility();
}

void DSPOperationComponent::updateParameterVisibility()
{
    auto op = static_cast<BatchDSPOperation>(m_operationCombo.getSelectedId() - 1);

    bool showSlider = true;
    bool showCurve = false;

    switch (op)
    {
        case BatchDSPOperation::GAIN:
            m_paramLabel.setText("Gain (dB):", juce::dontSendNotification);
            m_paramSlider.setRange(-24.0, 24.0, 0.1);
            break;

        case BatchDSPOperation::NORMALIZE:
            m_paramLabel.setText("Target (dB):", juce::dontSendNotification);
            m_paramSlider.setRange(-24.0, 0.0, 0.1);
            m_paramSlider.setValue(-0.3, juce::dontSendNotification);
            break;

        case BatchDSPOperation::DC_OFFSET:
            showSlider = false;
            m_paramLabel.setText("", juce::dontSendNotification);
            break;

        case BatchDSPOperation::FADE_IN:
        case BatchDSPOperation::FADE_OUT:
            m_paramLabel.setText("Duration (ms):", juce::dontSendNotification);
            m_paramSlider.setRange(1.0, 5000.0, 1.0);
            m_paramSlider.setValue(100.0, juce::dontSendNotification);
            showCurve = true;
            break;

        default:
            showSlider = false;
            break;
    }

    m_paramSlider.setVisible(showSlider);
    m_paramLabel.setVisible(showSlider);
    m_curveCombo.setVisible(showCurve);
    resized();
}

// =============================================================================
// DSPChainPanel
// =============================================================================

DSPChainPanel::DSPChainPanel()
    : m_titleLabel("titleLabel", "DSP Processing Chain")
    , m_addButton("+ Add Operation")
{
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_titleLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    #pragma GCC diagnostic pop
    addAndMakeVisible(m_titleLabel);

    m_addButton.onClick = [this]() { addOperation(); };
    addAndMakeVisible(m_addButton);

    m_contentComponent = std::make_unique<juce::Component>();
    m_viewport.setViewedComponent(m_contentComponent.get(), false);
    m_viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(m_viewport);
}

void DSPChainPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
    g.setColour(juce::Colour(0xff3d3d3d));
    g.drawRect(getLocalBounds(), 1);
}

void DSPChainPanel::resized()
{
    auto area = getLocalBounds().reduced(5);

    auto headerArea = area.removeFromTop(25);
    m_titleLabel.setBounds(headerArea.removeFromLeft(200));
    m_addButton.setBounds(headerArea.removeFromRight(120));

    area.removeFromTop(5);
    m_viewport.setBounds(area);

    rebuildLayout();
}

std::vector<BatchDSPSettings> DSPChainPanel::getDSPChain() const
{
    std::vector<BatchDSPSettings> chain;
    for (auto* op : m_operations)
        chain.push_back(op->getSettings());
    return chain;
}

void DSPChainPanel::setDSPChain(const std::vector<BatchDSPSettings>& chain)
{
    m_operations.clear();

    for (size_t i = 0; i < chain.size(); ++i)
    {
        auto* op = new DSPOperationComponent(static_cast<int>(i));
        op->setSettings(chain[i]);

        m_operations.add(op);
        m_contentComponent->addAndMakeVisible(op);
    }

    updateOperationCallbacks();
    rebuildLayout();
}

void DSPChainPanel::addOperation()
{
    int index = m_operations.size();
    auto* op = new DSPOperationComponent(index);

    m_operations.add(op);
    m_contentComponent->addAndMakeVisible(op);

    updateOperationCallbacks();
    rebuildLayout();

    if (onChainChanged)
        onChainChanged();
}

void DSPChainPanel::removeOperation(int index)
{
    if (index >= 0 && index < m_operations.size())
    {
        m_operations.remove(index);
        updateOperationCallbacks();
        rebuildLayout();

        if (onChainChanged)
            onChainChanged();
    }
}

void DSPChainPanel::moveOperation(int index, int direction)
{
    int newIndex = index + direction;

    // Check bounds
    if (index < 0 || index >= m_operations.size())
        return;
    if (newIndex < 0 || newIndex >= m_operations.size())
        return;

    // Swap operations
    m_operations.swap(index, newIndex);

    updateOperationCallbacks();
    rebuildLayout();

    if (onChainChanged)
        onChainChanged();
}

void DSPChainPanel::updateOperationCallbacks()
{
    int numOps = m_operations.size();

    for (int i = 0; i < numOps; ++i)
    {
        int currentIndex = i;
        auto* op = m_operations[i];

        // Update the stored index
        op->setIndex(i);

        op->onRemoveClicked = [this, currentIndex]() { removeOperation(currentIndex); };
        op->onMoveUpClicked = [this, currentIndex]() { moveOperation(currentIndex, -1); };
        op->onMoveDownClicked = [this, currentIndex]() { moveOperation(currentIndex, +1); };
        op->onSettingsChanged = [this]()
        {
            if (onChainChanged)
                onChainChanged();
        };

        // Enable/disable move buttons based on position
        op->setMoveButtonsEnabled(i > 0, i < numOps - 1);
    }
}

void DSPChainPanel::rebuildLayout()
{
    int yPos = 0;
    int rowHeight = 40;
    int width = m_viewport.getWidth() - 20; // Account for scrollbar

    for (auto* op : m_operations)
    {
        op->setBounds(0, yPos, width, rowHeight);
        yPos += rowHeight + 2;
    }

    m_contentComponent->setSize(width, juce::jmax(yPos, m_viewport.getHeight()));
}

// =============================================================================
// DSPChainPanel Drag-and-Drop Support
// =============================================================================

bool DSPChainPanel::isInterestedInDragSource(const SourceDetails& details)
{
    // Accept drags that start with our DSPOperation identifier
    if (auto* desc = details.description.toString().toRawUTF8())
    {
        return juce::String(desc).startsWith("DSPOperation:");
    }
    return false;
}

void DSPChainPanel::itemDragEnter(const SourceDetails& details)
{
    itemDragMove(details);
}

void DSPChainPanel::itemDragMove(const SourceDetails& details)
{
    // Calculate which slot we're hovering over
    auto localPoint = m_contentComponent->getLocalPoint(this, details.localPosition);
    int newDropIndex = getDropIndexFromY(localPoint.y);

    if (newDropIndex != m_dropIndicatorIndex)
    {
        // Clear old highlight
        clearDropIndicator();

        // Set new drop index and highlight
        m_dropIndicatorIndex = newDropIndex;

        // Highlight the target operation
        if (m_dropIndicatorIndex >= 0 && m_dropIndicatorIndex < m_operations.size())
        {
            m_operations[m_dropIndicatorIndex]->setDragHighlight(true);
        }

        repaint();
    }
}

void DSPChainPanel::itemDragExit(const SourceDetails& /*details*/)
{
    clearDropIndicator();
}

void DSPChainPanel::itemDropped(const SourceDetails& details)
{
    // Parse the source index from the description
    juce::String desc = details.description.toString();
    if (desc.startsWith("DSPOperation:"))
    {
        int sourceIndex = desc.substring(13).getIntValue();
        int targetIndex = m_dropIndicatorIndex;

        // Clear the highlight
        clearDropIndicator();

        // Perform the move if valid
        if (sourceIndex != targetIndex && sourceIndex >= 0 && targetIndex >= 0)
        {
            moveOperationTo(sourceIndex, targetIndex);
        }
    }
}

void DSPChainPanel::moveOperationTo(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_operations.size())
        return;
    if (toIndex < 0 || toIndex >= m_operations.size())
        return;
    if (fromIndex == toIndex)
        return;

    // Move the operation
    auto* op = m_operations.removeAndReturn(fromIndex);
    m_operations.insert(toIndex, op);

    updateOperationCallbacks();
    rebuildLayout();

    if (onChainChanged)
        onChainChanged();
}

int DSPChainPanel::getDropIndexFromY(int y) const
{
    if (m_operations.isEmpty())
        return 0;

    int rowHeight = 42;  // 40 + 2 spacing

    int index = y / rowHeight;

    // Clamp to valid range
    return juce::jlimit(0, m_operations.size() - 1, index);
}

void DSPChainPanel::clearDropIndicator()
{
    if (m_dropIndicatorIndex >= 0 && m_dropIndicatorIndex < m_operations.size())
    {
        m_operations[m_dropIndicatorIndex]->setDragHighlight(false);
    }
    m_dropIndicatorIndex = -1;
    repaint();
}

// =============================================================================
// Enhanced FileListModel - Paint with size, duration, and status
// =============================================================================

void BatchProcessorDialog::FileListModel::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                                            int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(m_files.size()))
        return;

    const auto& info = m_files[static_cast<size_t>(rowNumber)];

    // Background
    if (rowIsSelected)
        g.fillAll(juce::Colour(0xff3d6ea5));
    else if (rowNumber % 2 == 0)
        g.fillAll(juce::Colour(0xff2b2b2b));
    else
        g.fillAll(juce::Colour(0xff323232));

    // Status icon
    int iconX = 5;
    int iconSize = 14;
    juce::Colour statusColour;
    juce::String statusIcon;

    switch (info.status)
    {
        case BatchJobStatus::PENDING:
            statusColour = juce::Colour(ui::kStatusPending);
            statusIcon = juce::CharPointer_UTF8("\xe2\x97\x8b");  // ○
            break;
        case BatchJobStatus::LOADING:
        case BatchJobStatus::PROCESSING:
        case BatchJobStatus::SAVING:
            statusColour = juce::Colour(0xffffaa00);
            statusIcon = juce::CharPointer_UTF8("\xe2\x8f\xb3");  // ⏳
            break;
        case BatchJobStatus::COMPLETED:
            statusColour = juce::Colour(0xff00cc00);
            statusIcon = juce::CharPointer_UTF8("\xe2\x9c\x93");  // ✓
            break;
        case BatchJobStatus::FAILED:
            statusColour = juce::Colour(0xffff4444);
            statusIcon = juce::CharPointer_UTF8("\xe2\x9c\x97");  // ✗
            break;
        case BatchJobStatus::SKIPPED:
            statusColour = juce::Colour(0xffaaaa00);
            statusIcon = juce::CharPointer_UTF8("\xe2\x86\x92");  // →
            break;
    }

    g.setColour(statusColour);
    g.drawText(statusIcon, iconX, 0, iconSize, height, juce::Justification::centred);

    // Filename
    int nameX = iconX + iconSize + 5;
    int nameWidth = width - nameX - 120;  // Reserve space for size and duration
    g.setColour(juce::Colours::white);
    g.drawText(info.fileName, nameX, 0, nameWidth, height, juce::Justification::centredLeft, true);

    // Progress bar for processing files
    bool isProcessing = (info.status == BatchJobStatus::LOADING ||
                         info.status == BatchJobStatus::PROCESSING ||
                         info.status == BatchJobStatus::SAVING);

    if (isProcessing && info.progress > 0.0f)
    {
        // Draw progress bar below filename
        int progressBarX = nameX;
        int progressBarY = height - 6;
        int progressBarWidth = nameWidth - 10;
        int progressBarHeight = 4;

        // Background
        g.setColour(juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(static_cast<float>(progressBarX),
                               static_cast<float>(progressBarY),
                               static_cast<float>(progressBarWidth),
                               static_cast<float>(progressBarHeight), 2.0f);

        // Progress fill
        g.setColour(juce::Colour(0xff00aaff));
        int fillWidth = static_cast<int>(progressBarWidth * info.progress);
        g.fillRoundedRectangle(static_cast<float>(progressBarX),
                               static_cast<float>(progressBarY),
                               static_cast<float>(fillWidth),
                               static_cast<float>(progressBarHeight), 2.0f);
    }

    // File size
    int sizeX = nameX + nameWidth + 5;
    int sizeWidth = 55;
    g.setColour(juce::Colour(0xffaaaaaa));
    g.drawText(info.getFormattedSize(), sizeX, 0, sizeWidth, height, juce::Justification::centredRight);

    // Duration (or progress percentage when processing)
    int durationX = sizeX + sizeWidth + 5;
    int durationWidth = 45;

    if (isProcessing)
    {
        g.setColour(juce::Colour(0xff00aaff));
        g.drawText(juce::String(static_cast<int>(info.progress * 100)) + "%",
                   durationX, 0, durationWidth, height, juce::Justification::centredRight);
    }
    else
    {
        g.setColour(juce::Colour(0xff88bbff));
        g.drawText(info.getFormattedDuration(), durationX, 0, durationWidth, height, juce::Justification::centredRight);
    }
}

juce::String BatchProcessorDialog::FileListModel::getTooltipForRow(int row)
{
    if (row < 0 || row >= static_cast<int>(m_files.size()))
        return {};

    const auto& info = m_files[static_cast<size_t>(row)];
    juce::String tooltip;
    tooltip << info.fullPath << "\n";
    tooltip << "Size: " << info.getFormattedSize() << "\n";
    tooltip << "Duration: " << info.getFormattedDuration() << "\n";
    tooltip << "Channels: " << juce::String(info.numChannels) << "\n";
    tooltip << "Sample Rate: " << juce::String(static_cast<int>(info.sampleRate)) << " Hz";
    return tooltip;
}

} // namespace waveedit
