/*
  ==============================================================================

    BatchProcessorDialog.cpp
    Created: 2025
    Author:  ZQ SFX

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

// =============================================================================
// BatchProcessorDialog
// =============================================================================

BatchProcessorDialog::BatchProcessorDialog()
    : m_filesLabel("filesLabel", "Input Files:")
    , m_addFilesButton("Add Files...")
    , m_addFolderButton("Add Folder...")
    , m_removeFilesButton("Remove")
    , m_clearFilesButton("Clear All")
    , m_fileCountLabel("fileCountLabel", "0 files selected")
    , m_outputLabel("outputLabel", "Output Settings")
    , m_outputDirLabel("outputDirLabel", "Output Directory:")
    , m_browseOutputButton("Browse...")
    , m_sameAsSourceToggle("Same as source folder")
    , m_patternLabel("patternLabel", "Naming Pattern:")
    , m_patternHelpLabel("patternHelpLabel", "{filename}, {index}, {index:03}, {date}, {time}, {preset}")
    , m_patternHelpButton("?")
    , m_overwriteToggle("Overwrite existing files")
    , m_formatLabel("formatLabel", "Format:")
    , m_bitDepthLabel("bitDepthLabel", "Bit Depth:")
    , m_sampleRateLabel("sampleRateLabel", "Sample Rate:")
    , m_previewLabel("previewLabel", "Output Preview:")
    , m_presetLabel("presetLabel", "Preset:")
    , m_savePresetButton("Save...")
    , m_deletePresetButton("Delete")
    , m_exportPresetButton("Export...")
    , m_importPresetButton("Import...")
    , m_progressBar(m_progress)
    , m_statusLabel("statusLabel", "Ready")
    , m_previewButton("Preview")
    , m_startButton("Start Processing")
    , m_cancelButton("Cancel")
    , m_closeButton("Close")
{
    // Initialize managers
    m_presetManager = std::make_unique<BatchPresetManager>();
    m_engine = std::make_unique<BatchProcessorEngine>();
    m_engine->addListener(this);

    // File list model (enhanced with size, duration, status)
    m_fileListModel = std::make_unique<FileListModel>(m_fileInfos);

    // =========================================================================
    // File Selection Section
    // =========================================================================
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_filesLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    #pragma GCC diagnostic pop
    addAndMakeVisible(m_filesLabel);

    m_fileListBox.setModel(m_fileListModel.get());
    m_fileListBox.setMultipleSelectionEnabled(true);
    m_fileListBox.setRowHeight(20);
    addAndMakeVisible(m_fileListBox);

    m_addFilesButton.onClick = [this]() { onAddFilesClicked(); };
    addAndMakeVisible(m_addFilesButton);

    m_addFolderButton.onClick = [this]() { onAddFolderClicked(); };
    addAndMakeVisible(m_addFolderButton);

    m_removeFilesButton.onClick = [this]() { onRemoveFilesClicked(); };
    addAndMakeVisible(m_removeFilesButton);

    m_clearFilesButton.onClick = [this]() { onClearFilesClicked(); };
    addAndMakeVisible(m_clearFilesButton);

    addAndMakeVisible(m_fileCountLabel);

    // =========================================================================
    // DSP Chain Section
    // =========================================================================
    m_dspChainPanel = std::make_unique<DSPChainPanel>();
    m_dspChainPanel->onChainChanged = [this]() { updatePreview(); };
    addAndMakeVisible(m_dspChainPanel.get());

    // =========================================================================
    // Plugin Chain Section
    // =========================================================================
    m_usePluginChainToggle.setButtonText("Use Plugin Chain");
    m_usePluginChainToggle.setTooltip("Apply a saved VST/AU plugin chain during batch processing");
    m_usePluginChainToggle.onClick = [this]() { onUsePluginChainToggled(); };
    addAndMakeVisible(m_usePluginChainToggle);

    m_pluginPresetLabel.setText("Preset:", juce::dontSendNotification);
    addAndMakeVisible(m_pluginPresetLabel);

    m_pluginPresetEditor.setReadOnly(true);
    m_pluginPresetEditor.setTooltip("Path to the plugin chain preset file (.wechain)");
    addAndMakeVisible(m_pluginPresetEditor);

    m_browsePluginPresetButton.setButtonText("...");
    m_browsePluginPresetButton.setTooltip("Browse for plugin chain preset");
    m_browsePluginPresetButton.onClick = [this]() { onBrowsePluginPresetClicked(); };
    addAndMakeVisible(m_browsePluginPresetButton);

    m_pluginTailLabel.setText("Tail (sec):", juce::dontSendNotification);
    m_pluginTailLabel.setTooltip("Extra time to capture plugin reverb/delay tails");
    addAndMakeVisible(m_pluginTailLabel);

    m_pluginTailSlider.setRange(0.0, 10.0, 0.1);
    m_pluginTailSlider.setValue(2.0);
    m_pluginTailSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    m_pluginTailSlider.setTooltip("Extra seconds to render after audio ends (for reverb/delay tails)");
    addAndMakeVisible(m_pluginTailSlider);

    // Initially disable plugin controls
    updatePluginChainUI();

    // =========================================================================
    // Output Settings Section
    // =========================================================================
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_outputLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    #pragma GCC diagnostic pop
    addAndMakeVisible(m_outputLabel);

    addAndMakeVisible(m_outputDirLabel);

    m_outputDirEditor.onTextChange = [this]() { updatePreview(); };
    addAndMakeVisible(m_outputDirEditor);

    m_browseOutputButton.onClick = [this]() { onBrowseOutputClicked(); };
    addAndMakeVisible(m_browseOutputButton);

    // Same as source toggle
    m_sameAsSourceToggle.onClick = [this]() { onSameAsSourceToggled(); };
    addAndMakeVisible(m_sameAsSourceToggle);

    addAndMakeVisible(m_patternLabel);

    m_patternEditor.setText("{filename}_processed");
    m_patternEditor.onTextChange = [this]() { updatePreview(); updateOutputPreview(); };
    addAndMakeVisible(m_patternEditor);

    // Pattern help label - larger font (12pt)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_patternHelpLabel.setFont(juce::Font(12.0f));
    #pragma GCC diagnostic pop
    m_patternHelpLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaaaaaa));
    addAndMakeVisible(m_patternHelpLabel);

    // Pattern help button "?"
    m_patternHelpButton.onClick = [this]() { onPatternHelpClicked(); };
    m_patternHelpButton.setTooltip("Show naming pattern help");
    addAndMakeVisible(m_patternHelpButton);

    addAndMakeVisible(m_overwriteToggle);

    // Output Preview section
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_previewLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    #pragma GCC diagnostic pop
    addAndMakeVisible(m_previewLabel);

    m_outputPreviewEditor.setMultiLine(true);
    m_outputPreviewEditor.setReadOnly(true);
    m_outputPreviewEditor.setScrollbarsShown(true);
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_outputPreviewEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    #pragma GCC diagnostic pop
    m_outputPreviewEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1e1e1e));
    addAndMakeVisible(m_outputPreviewEditor);

    addAndMakeVisible(m_formatLabel);
    m_formatCombo.addItem("WAV", 1);
    m_formatCombo.addItem("FLAC", 2);
    m_formatCombo.addItem("OGG", 3);
    m_formatCombo.setSelectedId(1);
    addAndMakeVisible(m_formatCombo);

    addAndMakeVisible(m_bitDepthLabel);
    m_bitDepthCombo.addItem("16-bit", 16);
    m_bitDepthCombo.addItem("24-bit", 24);
    m_bitDepthCombo.addItem("32-bit", 32);
    m_bitDepthCombo.setSelectedId(16);
    addAndMakeVisible(m_bitDepthCombo);

    addAndMakeVisible(m_sampleRateLabel);
    m_sampleRateCombo.addItem("Keep Original", 1);
    m_sampleRateCombo.addItem("44100 Hz", 44100);
    m_sampleRateCombo.addItem("48000 Hz", 48000);
    m_sampleRateCombo.addItem("96000 Hz", 96000);
    m_sampleRateCombo.setSelectedId(1);
    addAndMakeVisible(m_sampleRateCombo);

    // =========================================================================
    // Preset Section
    // =========================================================================
    addAndMakeVisible(m_presetLabel);

    m_presetCombo.onChange = [this]() { onPresetChanged(); };
    addAndMakeVisible(m_presetCombo);

    m_savePresetButton.onClick = [this]() { onSavePresetClicked(); };
    addAndMakeVisible(m_savePresetButton);

    m_deletePresetButton.onClick = [this]() { onDeletePresetClicked(); };
    addAndMakeVisible(m_deletePresetButton);

    m_exportPresetButton.setTooltip("Export preset to file for sharing");
    m_exportPresetButton.onClick = [this]() { onExportPresetClicked(); };
    addAndMakeVisible(m_exportPresetButton);

    m_importPresetButton.setTooltip("Import preset from file");
    m_importPresetButton.onClick = [this]() { onImportPresetClicked(); };
    addAndMakeVisible(m_importPresetButton);

    refreshPresetList();

    // =========================================================================
    // Progress Section
    // =========================================================================
    addAndMakeVisible(m_progressBar);
    addAndMakeVisible(m_statusLabel);

    m_logEditor.setMultiLine(true);
    m_logEditor.setReadOnly(true);
    m_logEditor.setScrollbarsShown(true);
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    m_logEditor.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 11.0f, juce::Font::plain));
    #pragma GCC diagnostic pop
    addAndMakeVisible(m_logEditor);

    // =========================================================================
    // Action Buttons
    // =========================================================================
    m_previewButton.setTooltip("Preview the DSP chain on the selected file");
    m_previewButton.onClick = [this]() { onPreviewClicked(); };
    addAndMakeVisible(m_previewButton);

    m_startButton.onClick = [this]() { onStartClicked(); };
    addAndMakeVisible(m_startButton);

    m_cancelButton.onClick = [this]() { onCancelClicked(); };
    m_cancelButton.setEnabled(false);
    addAndMakeVisible(m_cancelButton);

    m_closeButton.onClick = [this]() { onCloseClicked(); };
    addAndMakeVisible(m_closeButton);

    // =========================================================================
    // Audio Preview Setup
    // =========================================================================
    m_previewDeviceManager.initialiseWithDefaultDevices(0, 2);
    m_previewDeviceManager.addAudioCallback(&m_previewSourcePlayer);

    // Set initial size
    setSize(800, 700);
}

BatchProcessorDialog::~BatchProcessorDialog()
{
    // Clean up preview
    stopPreview();
    cleanupPreview();
    m_previewDeviceManager.removeAudioCallback(&m_previewSourcePlayer);

    if (m_engine)
    {
        m_engine->removeListener(this);
        m_engine->cancelProcessing();
        m_engine->waitForCompletion(5000);
    }
}

void BatchProcessorDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2b2b2b));
}

void BatchProcessorDialog::resized()
{
    auto area = getLocalBounds().reduced(10);

    // =========================================================================
    // Left column (file list)
    // =========================================================================
    auto leftColumn = area.removeFromLeft(340);  // Wider for enhanced file info

    m_filesLabel.setBounds(leftColumn.removeFromTop(25));
    leftColumn.removeFromTop(5);

    auto fileButtonRow = leftColumn.removeFromTop(25);
    int btnWidth = 75;
    m_addFilesButton.setBounds(fileButtonRow.removeFromLeft(btnWidth));
    fileButtonRow.removeFromLeft(5);
    m_addFolderButton.setBounds(fileButtonRow.removeFromLeft(btnWidth + 10));
    fileButtonRow.removeFromLeft(5);
    m_removeFilesButton.setBounds(fileButtonRow.removeFromLeft(btnWidth - 10));
    fileButtonRow.removeFromLeft(5);
    m_clearFilesButton.setBounds(fileButtonRow.removeFromLeft(btnWidth - 10));

    leftColumn.removeFromTop(5);
    m_fileCountLabel.setBounds(leftColumn.removeFromTop(20));

    leftColumn.removeFromTop(5);
    m_fileListBox.setBounds(leftColumn);

    area.removeFromLeft(10);

    // =========================================================================
    // Right column (settings)
    // =========================================================================
    auto rightColumn = area;

    // DSP Chain
    m_dspChainPanel->setBounds(rightColumn.removeFromTop(140));
    rightColumn.removeFromTop(8);

    // Plugin Chain Section
    auto pluginRow1 = rightColumn.removeFromTop(25);
    m_usePluginChainToggle.setBounds(pluginRow1.removeFromLeft(130));
    pluginRow1.removeFromLeft(10);
    m_pluginPresetLabel.setBounds(pluginRow1.removeFromLeft(45));
    m_browsePluginPresetButton.setBounds(pluginRow1.removeFromRight(30));
    pluginRow1.removeFromRight(5);
    m_pluginPresetEditor.setBounds(pluginRow1);

    rightColumn.removeFromTop(3);
    auto pluginRow2 = rightColumn.removeFromTop(25);
    pluginRow2.removeFromLeft(130);  // Align with toggle above
    m_pluginTailLabel.setBounds(pluginRow2.removeFromLeft(60));
    m_pluginTailSlider.setBounds(pluginRow2.removeFromLeft(120));

    rightColumn.removeFromTop(8);

    // Output Settings
    m_outputLabel.setBounds(rightColumn.removeFromTop(25));
    rightColumn.removeFromTop(5);

    // Output directory row with "Same as source" toggle
    auto outputDirRow = rightColumn.removeFromTop(25);
    m_outputDirLabel.setBounds(outputDirRow.removeFromLeft(110));
    m_sameAsSourceToggle.setBounds(outputDirRow.removeFromRight(140));
    outputDirRow.removeFromRight(5);
    m_browseOutputButton.setBounds(outputDirRow.removeFromRight(70));
    outputDirRow.removeFromRight(5);
    m_outputDirEditor.setBounds(outputDirRow);

    rightColumn.removeFromTop(5);

    // Pattern row with help button
    auto patternRow = rightColumn.removeFromTop(25);
    m_patternLabel.setBounds(patternRow.removeFromLeft(110));
    m_patternHelpButton.setBounds(patternRow.removeFromRight(25));
    patternRow.removeFromRight(5);
    m_patternEditor.setBounds(patternRow);

    // Pattern help label (larger font)
    auto helpLabelRow = rightColumn.removeFromTop(18);
    m_patternHelpLabel.setBounds(helpLabelRow.withTrimmedLeft(110));
    rightColumn.removeFromTop(3);

    m_overwriteToggle.setBounds(rightColumn.removeFromTop(25).withTrimmedLeft(110));
    rightColumn.removeFromTop(5);

    auto formatRow = rightColumn.removeFromTop(25);
    m_formatLabel.setBounds(formatRow.removeFromLeft(50));
    m_formatCombo.setBounds(formatRow.removeFromLeft(80));
    formatRow.removeFromLeft(15);
    m_bitDepthLabel.setBounds(formatRow.removeFromLeft(60));
    m_bitDepthCombo.setBounds(formatRow.removeFromLeft(80));
    formatRow.removeFromLeft(15);
    m_sampleRateLabel.setBounds(formatRow.removeFromLeft(80));
    m_sampleRateCombo.setBounds(formatRow.removeFromLeft(100));

    rightColumn.removeFromTop(8);

    // Output Preview section
    m_previewLabel.setBounds(rightColumn.removeFromTop(20));
    rightColumn.removeFromTop(3);
    m_outputPreviewEditor.setBounds(rightColumn.removeFromTop(60));
    rightColumn.removeFromTop(8);

    // Preset Row 1: Label + Combo + Save + Delete
    auto presetRow1 = rightColumn.removeFromTop(25);
    m_presetLabel.setBounds(presetRow1.removeFromLeft(50));
    m_deletePresetButton.setBounds(presetRow1.removeFromRight(55));
    presetRow1.removeFromRight(3);
    m_savePresetButton.setBounds(presetRow1.removeFromRight(55));
    presetRow1.removeFromRight(5);
    m_presetCombo.setBounds(presetRow1);

    rightColumn.removeFromTop(3);

    // Preset Row 2: Export + Import
    auto presetRow2 = rightColumn.removeFromTop(25);
    presetRow2.removeFromLeft(50);  // Align with combo above
    m_exportPresetButton.setBounds(presetRow2.removeFromLeft(70));
    presetRow2.removeFromLeft(5);
    m_importPresetButton.setBounds(presetRow2.removeFromLeft(70));

    rightColumn.removeFromTop(8);

    // Progress
    m_progressBar.setBounds(rightColumn.removeFromTop(20));
    rightColumn.removeFromTop(5);
    m_statusLabel.setBounds(rightColumn.removeFromTop(20));
    rightColumn.removeFromTop(5);

    // Log
    auto logHeight = rightColumn.getHeight() - 40;
    m_logEditor.setBounds(rightColumn.removeFromTop(logHeight));
    rightColumn.removeFromTop(10);

    // Buttons - Preview on left, Start/Cancel/Close on right
    auto buttonRow = rightColumn.removeFromTop(30);

    // Left side: Preview button
    m_previewButton.setBounds(buttonRow.removeFromLeft(80));

    // Right side: Close, Cancel, Start
    m_closeButton.setBounds(buttonRow.removeFromRight(80));
    buttonRow.removeFromRight(10);
    m_cancelButton.setBounds(buttonRow.removeFromRight(80));
    buttonRow.removeFromRight(10);
    m_startButton.setBounds(buttonRow.removeFromRight(120));
}

bool BatchProcessorDialog::showDialog()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Batch Processor";
    options.dialogBackgroundColour = juce::Colour(waveedit::ui::kBackgroundPrimary);
    options.content.setOwned(new BatchProcessorDialog());
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.componentToCentreAround = nullptr;

    // Create the dialog window
    std::unique_ptr<juce::DialogWindow> dlg(options.create());
    dlg->centreWithSize(800, 700);

    #if JUCE_MODAL_LOOPS_PERMITTED
        int result = dlg->runModalLoop();
        return result == 1;
    #else
        dlg->enterModalState(true);
        return false;
    #endif
}

// =============================================================================
// BatchProcessorListener
// =============================================================================

void BatchProcessorDialog::batchProgressChanged(float progress, int currentFile, int totalFiles,
                                                  const juce::String& statusMessage)
{
    m_progress = progress;

    juce::String status = "Processing file " + juce::String(currentFile) + " of "
                         + juce::String(totalFiles) + ": " + statusMessage;
    m_statusLabel.setText(status, juce::dontSendNotification);

    // Update per-file progress
    int fileIndex = currentFile - 1;  // currentFile is 1-based
    if (fileIndex >= 0 && fileIndex < static_cast<int>(m_fileInfos.size()))
    {
        // Calculate per-file progress from overall progress
        // Overall progress = (completed_files + current_file_progress) / total_files
        // So: current_file_progress = (overall_progress * total_files) - completed_files
        float filesCompleted = static_cast<float>(fileIndex);
        float currentFileProgress = (progress * static_cast<float>(totalFiles)) - filesCompleted;
        currentFileProgress = juce::jlimit(0.0f, 1.0f, currentFileProgress);

        m_fileInfos[static_cast<size_t>(fileIndex)].progress = currentFileProgress;
        m_fileInfos[static_cast<size_t>(fileIndex)].status = BatchJobStatus::PROCESSING;
        m_fileListBox.repaintRow(fileIndex);
    }

    repaint();
}

void BatchProcessorDialog::jobCompleted(int jobIndex, const BatchJobResult& result)
{
    juce::String logLine;

    if (result.status == BatchJobStatus::COMPLETED)
    {
        logLine = "[OK] " + result.outputFile.getFileName()
                + " (" + juce::String(result.durationSeconds, 1) + "s)";
    }
    else if (result.status == BatchJobStatus::FAILED)
    {
        logLine = "[FAIL] Job " + juce::String(jobIndex + 1) + ": " + result.errorMessage;
    }
    else if (result.status == BatchJobStatus::SKIPPED)
    {
        logLine = "[SKIP] Job " + juce::String(jobIndex + 1) + ": " + result.errorMessage;
    }

    m_logEditor.moveCaretToEnd();
    m_logEditor.insertTextAtCaret(logLine + "\n");

    // Update file status in the list
    if (jobIndex >= 0 && jobIndex < static_cast<int>(m_fileInfos.size()))
    {
        m_fileInfos[static_cast<size_t>(jobIndex)].status = result.status;
        m_fileListBox.repaintRow(jobIndex);
    }
}

void BatchProcessorDialog::batchCompleted(bool cancelled, int successCount,
                                           int failedCount, int skippedCount)
{
    setProcessingMode(false);

    juce::String summary;
    if (cancelled)
    {
        summary = "Processing cancelled. ";
    }
    else
    {
        summary = "Processing complete. ";
    }

    summary += juce::String(successCount) + " succeeded, "
             + juce::String(failedCount) + " failed, "
             + juce::String(skippedCount) + " skipped.";

    m_statusLabel.setText(summary, juce::dontSendNotification);

    m_logEditor.moveCaretToEnd();
    m_logEditor.insertTextAtCaret("\n" + summary + "\n");

    // Show completion message
    if (!cancelled && failedCount == 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "Batch Processing Complete",
            "Successfully processed " + juce::String(successCount) + " files.",
            "OK"
        );
    }
    else if (failedCount > 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Batch Processing Complete",
            summary + "\n\nCheck the log for details about failed files.",
            "OK"
        );
    }
}

// =============================================================================
// FileDragAndDropTarget
// =============================================================================

bool BatchProcessorDialog::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& file : files)
    {
        juce::File f(file);
        juce::String ext = f.getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aif" || ext == ".aiff" ||
            ext == ".flac" || ext == ".mp3" || ext == ".ogg")
        {
            return true;
        }
        if (f.isDirectory())
            return true;
    }
    return false;
}

void BatchProcessorDialog::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    for (const auto& file : files)
    {
        juce::File f(file);

        if (f.isDirectory())
        {
            auto audioFiles = f.findChildFiles(
                juce::File::findFiles, true,
                "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg"
            );

            for (const auto& audioFile : audioFiles)
            {
                addFileWithInfo(audioFile);
            }
        }
        else
        {
            juce::String ext = f.getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".aif" || ext == ".aiff" ||
                ext == ".flac" || ext == ".mp3" || ext == ".ogg")
            {
                addFileWithInfo(f);
            }
        }
    }

    updateFileList();
}

// =============================================================================
// File List Management
// =============================================================================

void BatchProcessorDialog::onAddFilesClicked()
{
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Select Audio Files",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg"
    );

    auto flags = juce::FileBrowserComponent::openMode |
                 juce::FileBrowserComponent::canSelectFiles |
                 juce::FileBrowserComponent::canSelectMultipleItems;

    m_fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto results = fc.getResults();
        for (const auto& file : results)
        {
            addFileWithInfo(file);
        }
        updateFileList();
    });
}

void BatchProcessorDialog::onAddFolderClicked()
{
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Select Folder",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory)
    );

    auto flags = juce::FileBrowserComponent::openMode |
                 juce::FileBrowserComponent::canSelectDirectories;

    m_fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result.isDirectory())
        {
            auto audioFiles = result.findChildFiles(
                juce::File::findFiles, true,
                "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg"
            );

            for (const auto& file : audioFiles)
            {
                addFileWithInfo(file);
            }
            updateFileList();
        }
    });
}

void BatchProcessorDialog::onRemoveFilesClicked()
{
    auto selectedRows = m_fileListBox.getSelectedRows();
    std::vector<BatchFileInfo> newFiles;

    for (size_t i = 0; i < m_fileInfos.size(); ++i)
    {
        if (!selectedRows.contains(static_cast<int>(i)))
            newFiles.push_back(m_fileInfos[i]);
    }

    m_fileInfos = newFiles;
    updateFileList();
}

void BatchProcessorDialog::onClearFilesClicked()
{
    m_fileInfos.clear();
    updateFileList();
}

void BatchProcessorDialog::updateFileList()
{
    // Sync m_inputFiles with m_fileInfos
    m_inputFiles.clear();
    for (const auto& info : m_fileInfos)
        m_inputFiles.add(info.fullPath);

    m_fileListBox.updateContent();
    m_fileCountLabel.setText(getTotalFileSummary(), juce::dontSendNotification);

    // Update output preview
    updateOutputPreview();
}

BatchFileInfo BatchProcessorDialog::getFileInfo(const juce::File& file)
{
    BatchFileInfo info;
    info.fullPath = file.getFullPathName();
    info.fileName = file.getFileName();
    info.sizeBytes = file.getSize();
    info.status = BatchJobStatus::PENDING;

    // Try to get audio info
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(file));

    if (reader)
    {
        info.sampleRate = reader->sampleRate;
        info.numChannels = static_cast<int>(reader->numChannels);
        if (reader->sampleRate > 0)
            info.durationSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    }

    return info;
}

void BatchProcessorDialog::addFileWithInfo(const juce::File& file)
{
    // Check if already added
    for (const auto& info : m_fileInfos)
    {
        if (info.fullPath == file.getFullPathName())
            return;
    }

    m_fileInfos.push_back(getFileInfo(file));
}

juce::String BatchProcessorDialog::getTotalFileSummary() const
{
    if (m_fileInfos.empty())
        return "0 files selected";

    juce::int64 totalBytes = 0;
    double totalDuration = 0.0;

    for (const auto& info : m_fileInfos)
    {
        totalBytes += info.sizeBytes;
        totalDuration += info.durationSeconds;
    }

    // Format total size
    juce::String sizeStr;
    if (totalBytes < 1024 * 1024)
        sizeStr = juce::String(totalBytes / 1024.0, 1) + " KB";
    else if (totalBytes < 1024 * 1024 * 1024)
        sizeStr = juce::String(totalBytes / (1024.0 * 1024.0), 1) + " MB";
    else
        sizeStr = juce::String(totalBytes / (1024.0 * 1024.0 * 1024.0), 2) + " GB";

    // Format total duration
    int totalSeconds = static_cast<int>(totalDuration);
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    juce::String durationStr;
    if (hours > 0)
        durationStr = juce::String::formatted("%d:%02d:%02d", hours, minutes, seconds);
    else
        durationStr = juce::String::formatted("%d:%02d", minutes, seconds);

    return juce::String(m_fileInfos.size()) + " files (" + sizeStr + ", " + durationStr + ")";
}

// =============================================================================
// Output Settings
// =============================================================================

void BatchProcessorDialog::onBrowseOutputClicked()
{
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Select Output Directory",
        juce::File(m_outputDirEditor.getText())
    );

    auto flags = juce::FileBrowserComponent::openMode |
                 juce::FileBrowserComponent::canSelectDirectories;

    m_fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto result = fc.getResult();
        if (result.isDirectory())
        {
            m_outputDirEditor.setText(result.getFullPathName());
        }
    });
}

void BatchProcessorDialog::onSameAsSourceToggled()
{
    bool sameAsSource = m_sameAsSourceToggle.getToggleState();

    // Enable/disable output directory controls
    m_outputDirEditor.setEnabled(!sameAsSource);
    m_browseOutputButton.setEnabled(!sameAsSource);

    if (sameAsSource)
    {
        m_outputDirEditor.setText("(Same as source file)", false);
        m_outputDirEditor.setColour(juce::TextEditor::textColourId, juce::Colour(ui::kTextMuted));
    }
    else
    {
        if (m_outputDirEditor.getText() == "(Same as source file)")
            m_outputDirEditor.setText("", false);
        m_outputDirEditor.setColour(juce::TextEditor::textColourId, juce::Colour(ui::kTextPrimary));
    }

    updateOutputPreview();
}

void BatchProcessorDialog::onPatternHelpClicked()
{
    juce::String helpText =
        "Output Naming Pattern Tokens:\n"
        "\n"
        "{filename}    - Original filename (without extension)\n"
        "                Example: \"drums.wav\" → \"drums\"\n"
        "\n"
        "{index}       - File index (1, 2, 3, ...)\n"
        "                Example: First file → \"1\"\n"
        "\n"
        "{index:03}    - Zero-padded index (001, 002, 003, ...)\n"
        "                Change 03 to any width: 02 = 01, 04 = 0001\n"
        "\n"
        "{date}        - Current date (YYYY-MM-DD)\n"
        "                Example: \"2026-01-12\"\n"
        "\n"
        "{time}        - Current time (HH-MM-SS)\n"
        "                Example: \"14-30-45\"\n"
        "\n"
        "{preset}      - Name of the selected preset\n"
        "                Example: \"Broadcast Ready\"\n"
        "\n"
        "Examples:\n"
        "  \"{filename}_processed\"     → drums_processed.wav\n"
        "  \"{filename}_{index:03}\"    → drums_001.wav\n"
        "  \"batch_{date}_{index:03}\"  → batch_2026-01-12_001.wav\n"
        "  \"{preset}_{filename}\"      → Broadcast Ready_drums.wav";

    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "Naming Pattern Help",
        helpText,
        "OK"
    );
}

void BatchProcessorDialog::updateOutputPattern()
{
    // Pattern is stored in the text editor
    updateOutputPreview();
}

void BatchProcessorDialog::updatePreview()
{
    // Could show preview of output filenames here
    updateOutputPreview();
}

void BatchProcessorDialog::updateOutputPreview()
{
    if (m_fileInfos.empty())
    {
        m_outputPreviewEditor.setText("Add files to see output preview...");
        return;
    }

    juce::String pattern = m_patternEditor.getText().trim();
    if (pattern.isEmpty())
        pattern = "{filename}";

    bool sameAsSource = m_sameAsSourceToggle.getToggleState();
    juce::String outputDir = m_outputDirEditor.getText().trim();

    // Get format extension
    juce::String ext;
    switch (m_formatCombo.getSelectedId())
    {
        case 1: ext = ".wav"; break;
        case 2: ext = ".flac"; break;
        case 3: ext = ".ogg"; break;
        default: ext = ".wav"; break;
    }

    juce::String previewText;
    int numToShow = juce::jmin(5, static_cast<int>(m_fileInfos.size()));

    BatchProcessorSettings tempSettings;
    tempSettings.outputPattern = pattern;

    for (int i = 0; i < numToShow; ++i)
    {
        const auto& info = m_fileInfos[static_cast<size_t>(i)];
        juce::File inputFile(info.fullPath);

        // Apply naming pattern
        juce::String outputName = tempSettings.applyNamingPattern(
            inputFile, i + 1, m_presetCombo.getText());

        // Determine output directory
        juce::String outputPath;
        if (sameAsSource)
        {
            outputPath = inputFile.getParentDirectory().getChildFile(outputName + ext).getFileName();
        }
        else if (outputDir.isNotEmpty() && outputDir != "(Same as source file)")
        {
            outputPath = outputName + ext;
        }
        else
        {
            outputPath = outputName + ext;
        }

        // Truncate long input names
        juce::String inputName = info.fileName;
        if (inputName.length() > 25)
            inputName = inputName.substring(0, 22) + "...";

        previewText << inputName << "  " << juce::String(juce::CharPointer_UTF8("\xe2\x86\x92")) << "  " << outputPath << "\n";
    }

    if (m_fileInfos.size() > 5)
    {
        previewText << "... and " << juce::String(m_fileInfos.size() - 5) << " more files";
    }

    m_outputPreviewEditor.setText(previewText);
}

// =============================================================================
// Plugin Chain
// =============================================================================

void BatchProcessorDialog::onUsePluginChainToggled()
{
    updatePluginChainUI();
}

void BatchProcessorDialog::onBrowsePluginPresetClicked()
{
    // Get the plugin chain presets directory
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
#if JUCE_MAC
    auto presetsDir = appData.getChildFile("Application Support/WaveEdit/Presets/PluginChains");
#elif JUCE_WINDOWS
    auto presetsDir = appData.getChildFile("WaveEdit/Presets/PluginChains");
#else
    auto presetsDir = appData.getChildFile(".waveedit/presets/pluginchains");
#endif

    if (!presetsDir.exists())
        presetsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Select Plugin Chain Preset",
        presetsDir,
        "*.wechain"
    );

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles;

    m_fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File())
            return;

        m_pluginPresetEditor.setText(file.getFullPathName());
    });
}

void BatchProcessorDialog::updatePluginChainUI()
{
    bool enabled = m_usePluginChainToggle.getToggleState();

    m_pluginPresetLabel.setEnabled(enabled);
    m_pluginPresetEditor.setEnabled(enabled);
    m_browsePluginPresetButton.setEnabled(enabled);
    m_pluginTailLabel.setEnabled(enabled);
    m_pluginTailSlider.setEnabled(enabled);

    // Update alpha for visual feedback
    float alpha = enabled ? 1.0f : 0.5f;
    m_pluginPresetLabel.setAlpha(alpha);
    m_pluginPresetEditor.setAlpha(alpha);
    m_browsePluginPresetButton.setAlpha(alpha);
    m_pluginTailLabel.setAlpha(alpha);
    m_pluginTailSlider.setAlpha(alpha);
}

// =============================================================================
// Preset Management
// =============================================================================

void BatchProcessorDialog::onPresetChanged()
{
    int selectedId = m_presetCombo.getSelectedId();
    if (selectedId > 0)
    {
        juce::String presetName = m_presetCombo.getText();
        loadPreset(presetName);
    }
}

void BatchProcessorDialog::onSavePresetClicked()
{
    juce::AlertWindow dialog("Save Preset",
                              "Enter a name for the preset:",
                              juce::AlertWindow::QuestionIcon);

    dialog.addTextEditor("presetName", "", "Preset Name:");
    dialog.addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (dialog.runModalLoop() == 1)
    {
        juce::String presetName = dialog.getTextEditor("presetName")->getText().trim();

        if (presetName.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Invalid Name",
                "Please enter a name for the preset."
            );
            return;
        }

        if (m_presetManager->isFactoryPreset(presetName))
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Cannot Overwrite",
                "Cannot overwrite factory presets."
            );
            return;
        }

        auto settings = gatherSettings();
        if (m_presetManager->savePreset(presetName, "", settings))
        {
            refreshPresetList();
            m_presetCombo.setText(presetName);
        }
    }
}

void BatchProcessorDialog::onDeletePresetClicked()
{
    juce::String presetName = m_presetCombo.getText();

    if (presetName.isEmpty())
        return;

    if (m_presetManager->isFactoryPreset(presetName))
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Cannot Delete",
            "Cannot delete factory presets."
        );
        return;
    }

    bool confirm = juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::QuestionIcon,
        "Delete Preset",
        "Are you sure you want to delete the preset '" + presetName + "'?",
        "Delete",
        "Cancel"
    );

    if (confirm)
    {
        if (m_presetManager->deletePreset(presetName))
        {
            refreshPresetList();
        }
    }
}

void BatchProcessorDialog::onExportPresetClicked()
{
    juce::String presetName = m_presetCombo.getText();

    if (presetName.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "No Preset Selected",
            "Please select a preset to export."
        );
        return;
    }

    // Create safe filename from preset name
    juce::String defaultFilename = presetName.replaceCharacters("/\\:*?\"<>|", "_________");

    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Export Preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile(defaultFilename + BatchPresetManager::getFileExtension()),
        "*" + BatchPresetManager::getFileExtension()
    );

    auto flags = juce::FileBrowserComponent::saveMode
               | juce::FileBrowserComponent::canSelectFiles
               | juce::FileBrowserComponent::warnAboutOverwriting;

    m_fileChooser->launchAsync(flags, [this, presetName](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File())
            return;  // User cancelled

        // Ensure correct extension
        if (!file.hasFileExtension(BatchPresetManager::getFileExtension()))
            file = file.withFileExtension(BatchPresetManager::getFileExtension().trimCharactersAtStart("."));

        if (m_presetManager->exportPreset(presetName, file))
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Export Successful",
                "Preset '" + presetName + "' exported to:\n" + file.getFullPathName()
            );
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Export Failed",
                "Failed to export preset. Check file permissions."
            );
        }
    });
}

void BatchProcessorDialog::onImportPresetClicked()
{
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Import Preset",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*" + BatchPresetManager::getFileExtension()
    );

    auto flags = juce::FileBrowserComponent::openMode
               | juce::FileBrowserComponent::canSelectFiles;

    m_fileChooser->launchAsync(flags, [this](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File())
            return;  // User cancelled

        juce::String importedName;
        if (m_presetManager->importPreset(file, importedName))
        {
            refreshPresetList();
            m_presetCombo.setText(importedName);

            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "Import Successful",
                "Preset '" + importedName + "' imported successfully."
            );
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "Import Failed",
                "Failed to import preset. The file may be invalid or corrupted."
            );
        }
    });
}

void BatchProcessorDialog::loadPreset(const juce::String& name)
{
    auto* preset = m_presetManager->getPreset(name);
    if (preset == nullptr)
        return;

    // Load DSP chain
    m_dspChainPanel->setDSPChain(preset->settings.dspChain);

    // Load plugin chain settings
    m_usePluginChainToggle.setToggleState(preset->settings.usePluginChain, juce::dontSendNotification);
    m_pluginPresetEditor.setText(preset->settings.pluginChainPresetPath);
    m_pluginTailSlider.setValue(preset->settings.pluginTailSeconds, juce::dontSendNotification);
    updatePluginChainUI();

    // Load output settings
    m_patternEditor.setText(preset->settings.outputPattern);
    m_overwriteToggle.setToggleState(preset->settings.overwriteExisting, juce::dontSendNotification);

    // Load format settings
    const auto& fmt = preset->settings.outputFormat.format;
    if (fmt == "wav")
        m_formatCombo.setSelectedId(1);
    else if (fmt == "flac")
        m_formatCombo.setSelectedId(2);
    else if (fmt == "ogg")
        m_formatCombo.setSelectedId(3);
    else
        m_formatCombo.setSelectedId(1);

    if (preset->settings.outputFormat.bitDepth > 0)
        m_bitDepthCombo.setSelectedId(preset->settings.outputFormat.bitDepth);

    if (preset->settings.outputFormat.sampleRate > 0)
        m_sampleRateCombo.setSelectedId(preset->settings.outputFormat.sampleRate);
    else
        m_sampleRateCombo.setSelectedId(1);
}

void BatchProcessorDialog::refreshPresetList()
{
    m_presetCombo.clear();

    auto names = m_presetManager->getPresetNames();
    int id = 1;
    for (const auto& name : names)
    {
        m_presetCombo.addItem(name, id++);
    }
}

// =============================================================================
// Processing
// =============================================================================

void BatchProcessorDialog::onStartClicked()
{
    if (!validateSettings())
        return;

    auto settings = gatherSettings();

    m_engine->setSettings(settings);

    // Reset file statuses
    for (auto& info : m_fileInfos)
    {
        info.status = BatchJobStatus::PENDING;
        info.progress = 0.0f;
    }
    m_fileListBox.repaint();

    // Clear log
    m_logEditor.clear();
    m_logEditor.insertTextAtCaret("Starting batch processing...\n\n");

    setProcessingMode(true);

    if (!m_engine->startProcessing())
    {
        setProcessingMode(false);
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Processing Failed",
            "Failed to start batch processing. Check settings and try again."
        );
    }
}

void BatchProcessorDialog::onCancelClicked()
{
    if (m_engine && m_engine->isProcessing())
    {
        m_engine->cancelProcessing();
        m_statusLabel.setText("Cancelling...", juce::dontSendNotification);
    }
}

void BatchProcessorDialog::onCloseClicked()
{
    if (m_engine && m_engine->isProcessing())
    {
        bool confirm = juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::QuestionIcon,
            "Processing in Progress",
            "Batch processing is still running. Cancel and close?",
            "Cancel & Close",
            "Keep Running"
        );

        if (!confirm)
            return;

        m_engine->cancelProcessing();
        m_engine->waitForCompletion(5000);
    }

    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
    {
        dw->exitModalState(0);
    }
}

bool BatchProcessorDialog::validateSettings()
{
    if (m_fileInfos.empty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Input Files",
            "Please add at least one audio file to process."
        );
        return false;
    }

    // If not using "same as source", validate output directory
    if (!m_sameAsSourceToggle.getToggleState())
    {
        juce::String outputDir = m_outputDirEditor.getText().trim();
        if (outputDir.isEmpty() || outputDir == "(Same as source file)")
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::WarningIcon,
                "No Output Directory",
                "Please select an output directory or enable 'Same as source folder'."
            );
            return false;
        }

        juce::File outDir(outputDir);
        if (!outDir.exists())
        {
            bool create = juce::AlertWindow::showOkCancelBox(
                juce::AlertWindow::QuestionIcon,
                "Create Directory",
                "Output directory does not exist. Create it?",
                "Create",
                "Cancel"
            );

            if (create)
            {
                if (!outDir.createDirectory())
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Error",
                        "Failed to create output directory."
                    );
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
    }

    juce::String pattern = m_patternEditor.getText().trim();
    if (pattern.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Output Pattern",
            "Please enter an output naming pattern."
        );
        return false;
    }

    return true;
}

BatchProcessorSettings BatchProcessorDialog::gatherSettings()
{
    BatchProcessorSettings settings;

    // Input files
    settings.inputFiles = m_inputFiles;

    // Output directory - check "Same as Source" toggle
    settings.sameAsSource = m_sameAsSourceToggle.getToggleState();
    if (!settings.sameAsSource)
        settings.outputDirectory = juce::File(m_outputDirEditor.getText().trim());
    // If sameAsSource is true, outputDirectory is ignored and each file uses its own parent folder

    // Naming pattern
    settings.outputPattern = m_patternEditor.getText().trim();

    // Overwrite
    settings.overwriteExisting = m_overwriteToggle.getToggleState();

    // DSP chain
    settings.dspChain = m_dspChainPanel->getDSPChain();

    // Plugin chain
    settings.usePluginChain = m_usePluginChainToggle.getToggleState();
    settings.pluginChainPresetPath = m_pluginPresetEditor.getText().trim();
    settings.pluginTailSeconds = m_pluginTailSlider.getValue();

    // Output format
    switch (m_formatCombo.getSelectedId())
    {
        case 1: settings.outputFormat.format = "wav"; break;
        case 2: settings.outputFormat.format = "flac"; break;
        case 3: settings.outputFormat.format = "ogg"; break;
        default: settings.outputFormat.format = "wav"; break;
    }

    settings.outputFormat.bitDepth = m_bitDepthCombo.getSelectedId();

    int sampleRateId = m_sampleRateCombo.getSelectedId();
    settings.outputFormat.sampleRate = (sampleRateId == 1) ? 0 : sampleRateId;

    return settings;
}

void BatchProcessorDialog::setProcessingMode(bool processing)
{
    m_isProcessing = processing;

    m_startButton.setEnabled(!processing);
    m_cancelButton.setEnabled(processing);
    m_closeButton.setEnabled(!processing);
    m_previewButton.setEnabled(!processing);

    m_addFilesButton.setEnabled(!processing);
    m_addFolderButton.setEnabled(!processing);
    m_removeFilesButton.setEnabled(!processing);
    m_clearFilesButton.setEnabled(!processing);
    m_browseOutputButton.setEnabled(!processing);
    m_presetCombo.setEnabled(!processing);
    m_savePresetButton.setEnabled(!processing);
    m_deletePresetButton.setEnabled(!processing);
}

// =============================================================================
// Audio Preview
// =============================================================================

void BatchProcessorDialog::onPreviewClicked()
{
    if (m_isPreviewing)
    {
        stopPreview();
        return;
    }

    // Get selected file from list
    int selectedRow = m_fileListBox.getSelectedRow();
    if (selectedRow < 0 || selectedRow >= static_cast<int>(m_fileInfos.size()))
    {
        // If nothing selected, use first file
        if (m_fileInfos.empty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "No Files",
                "Add files to preview the DSP chain."
            );
            return;
        }
        selectedRow = 0;
    }

    juce::File audioFile(m_fileInfos[selectedRow].fullPath);
    if (!audioFile.existsAsFile())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "File Not Found",
            "The selected file no longer exists."
        );
        return;
    }

    startPreviewPlayback(audioFile);
}

void BatchProcessorDialog::startPreviewPlayback(const juce::File& audioFile)
{
    // Clean up any existing preview
    cleanupPreview();

    // Load the audio file
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(audioFile));

    if (!reader)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "Cannot Read File",
            "Failed to load audio file for preview."
        );
        return;
    }

    // Read the entire file into a buffer
    int numChannels = static_cast<int>(reader->numChannels);
    juce::int64 numSamples = reader->lengthInSamples;
    double sampleRate = reader->sampleRate;

    // Limit preview to first 30 seconds to avoid memory issues
    const juce::int64 maxSamples = static_cast<juce::int64>(30.0 * sampleRate);
    if (numSamples > maxSamples)
        numSamples = maxSamples;

    m_previewBuffer = std::make_unique<juce::AudioBuffer<float>>(
        numChannels, static_cast<int>(numSamples));

    reader->read(m_previewBuffer.get(), 0, static_cast<int>(numSamples), 0, true, true);

    // Apply DSP chain
    auto dspChain = m_dspChainPanel->getDSPChain();
    for (const auto& dsp : dspChain)
    {
        if (!dsp.enabled)
            continue;

        switch (dsp.operation)
        {
            case BatchDSPOperation::GAIN:
            {
                float gainLinear = juce::Decibels::decibelsToGain(dsp.gainDb);
                m_previewBuffer->applyGain(gainLinear);
                break;
            }

            case BatchDSPOperation::NORMALIZE:
            {
                float peak = m_previewBuffer->getMagnitude(0, m_previewBuffer->getNumSamples());
                if (peak > 0.0f)
                {
                    float targetLinear = juce::Decibels::decibelsToGain(dsp.normalizeTargetDb);
                    float gainToApply = targetLinear / peak;
                    m_previewBuffer->applyGain(gainToApply);
                }
                break;
            }

            case BatchDSPOperation::DC_OFFSET:
            {
                for (int ch = 0; ch < m_previewBuffer->getNumChannels(); ++ch)
                {
                    float* data = m_previewBuffer->getWritePointer(ch);
                    int samples = m_previewBuffer->getNumSamples();

                    // Calculate DC offset
                    double sum = 0.0;
                    for (int i = 0; i < samples; ++i)
                        sum += data[i];
                    float dcOffset = static_cast<float>(sum / samples);

                    // Remove it
                    for (int i = 0; i < samples; ++i)
                        data[i] -= dcOffset;
                }
                break;
            }

            case BatchDSPOperation::FADE_IN:
            {
                int fadeSamples = static_cast<int>(dsp.fadeDurationMs * sampleRate / 1000.0);
                fadeSamples = juce::jmin(fadeSamples, m_previewBuffer->getNumSamples());

                for (int ch = 0; ch < m_previewBuffer->getNumChannels(); ++ch)
                {
                    float* data = m_previewBuffer->getWritePointer(ch);
                    for (int i = 0; i < fadeSamples; ++i)
                    {
                        float t = static_cast<float>(i) / static_cast<float>(fadeSamples);
                        // Simple linear fade
                        data[i] *= t;
                    }
                }
                break;
            }

            case BatchDSPOperation::FADE_OUT:
            {
                int fadeSamples = static_cast<int>(dsp.fadeDurationMs * sampleRate / 1000.0);
                int startSample = m_previewBuffer->getNumSamples() - fadeSamples;
                startSample = juce::jmax(0, startSample);
                fadeSamples = m_previewBuffer->getNumSamples() - startSample;

                for (int ch = 0; ch < m_previewBuffer->getNumChannels(); ++ch)
                {
                    float* data = m_previewBuffer->getWritePointer(ch);
                    for (int i = 0; i < fadeSamples; ++i)
                    {
                        float t = 1.0f - (static_cast<float>(i) / static_cast<float>(fadeSamples));
                        data[startSample + i] *= t;
                    }
                }
                break;
            }

            default:
                break;
        }
    }

    // Create audio source from buffer
    m_previewMemorySource = std::make_unique<juce::MemoryAudioSource>(
        *m_previewBuffer, false, false);

    m_previewTransport = std::make_unique<juce::AudioTransportSource>();
    m_previewTransport->setSource(m_previewMemorySource.get(), 0, nullptr, sampleRate, numChannels);

    m_previewSourcePlayer.setSource(m_previewTransport.get());

    // Start playback
    m_previewTransport->start();

    m_isPreviewing = true;
    m_previewButton.setButtonText("Stop");

    m_statusLabel.setText("Previewing: " + audioFile.getFileName(), juce::dontSendNotification);
}

void BatchProcessorDialog::stopPreview()
{
    if (m_previewTransport)
    {
        m_previewTransport->stop();
    }

    m_isPreviewing = false;
    m_previewButton.setButtonText("Preview");
    m_statusLabel.setText("Ready", juce::dontSendNotification);
}

void BatchProcessorDialog::cleanupPreview()
{
    m_previewSourcePlayer.setSource(nullptr);

    if (m_previewTransport)
    {
        m_previewTransport->setSource(nullptr);
        m_previewTransport.reset();
    }

    m_previewMemorySource.reset();
    m_previewBuffer.reset();
}

} // namespace waveedit
