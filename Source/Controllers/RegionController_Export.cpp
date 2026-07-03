/*
  ==============================================================================

    RegionController_Export.cpp
    WaveEdit - Professional Audio Editor
    Copyright (C) 2025 ZQ SFX

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================

    Batch region export orchestration (finding 3 / UX 22 / M4).

    Split out of RegionController.cpp so that file stays under the Sec 7.5 size
    cap and so the heavy export-UI dependencies (ProgressDialog,
    BatchExportDialog) stay out of the region-core translation unit.

    The export itself runs on a background thread via ProgressDialog with a
    working Cancel. Per Sec 6.9 the audio buffer and region data are COPIED
    before being handed to the worker; the worker never touches the live
    Document or any UI. The completion handler (message thread) shows a summary
    that lists failed files by name and offers "Show in Folder".

  ==============================================================================
*/

#include "RegionController.h"
#include "../Utils/Document.h"
#include "../Utils/Region.h"
#include "../Utils/RegionManager.h"
#include "../Utils/RegionExporter.h"
#include "../Utils/ProgressCallback.h"
#include "../UI/BatchExportDialog.h"
#include "../UI/ProgressDialog.h"

#include <memory>

namespace
{
    // Formats up to a handful of failed region names for the summary dialog, so
    // the user sees WHICH files failed instead of "check the console log".
    juce::String formatFailedList(const juce::StringArray& failed)
    {
        const int maxShow = 10;
        juce::String out;
        const int shown = juce::jmin(maxShow, failed.size());
        for (int i = 0; i < shown; ++i)
            out += "  - " + failed[i] + "\n";
        if (failed.size() > maxShow)
            out += "  - ... and " + juce::String(failed.size() - maxShow) + " more\n";
        return out;
    }

    // Shows the post-export summary on the message thread. Offers a
    // "Show in Folder" button (juce::File::revealToUser) whenever at least one
    // file was written.
    void showExportSummary(const RegionExporter::ExportResult& result,
                           const juce::File& outputDir)
    {
        juce::String title;
        juce::String message;
        bool offerReveal = false;
        bool warning = true;

        const int attempted = result.attempted;

        if (result.cancelled)
        {
            title = "Export Cancelled";
            message = "Export cancelled. " + juce::String(result.successCount)
                    + " region" + (result.successCount == 1 ? "" : "s")
                    + " exported before cancelling.";
            if (! result.failedNames.isEmpty())
                message += "\n\nFailed:\n" + formatFailedList(result.failedNames);
            offerReveal = result.successCount > 0;
        }
        else if (result.failedNames.isEmpty() && result.successCount > 0)
        {
            title = "Export Complete";
            message = "Successfully exported " + juce::String(result.successCount)
                    + " region" + (result.successCount == 1 ? "" : "s") + " to:\n\n"
                    + outputDir.getFullPathName();
            offerReveal = true;
            warning = false;
        }
        else if (result.successCount > 0)
        {
            title = "Partial Export";
            message = "Exported " + juce::String(result.successCount) + " of "
                    + juce::String(attempted) + " regions.\n\nFailed:\n"
                    + formatFailedList(result.failedNames);
            offerReveal = true;
        }
        else
        {
            title = "Export Failed";
            message = "Failed to export any regions.";
            if (! result.failedNames.isEmpty())
                message += "\n\nFailed:\n" + formatFailedList(result.failedNames);
        }

        if (result.coercionNote.isNotEmpty())
            message += "\n\nNote: " + result.coercionNote;

        auto options = juce::MessageBoxOptions()
            .withIconType(warning ? juce::MessageBoxIconType::WarningIcon
                                  : juce::MessageBoxIconType::InfoIcon)
            .withTitle(title)
            .withMessage(message)
            .withButton("OK");

        if (offerReveal)
            options = options.withButton("Show in Folder");

        // showAsync button indices are 1-based in add order: OK = 1,
        // "Show in Folder" = 2 (matches the codebase convention).
        juce::AlertWindow::showAsync(options, [outputDir, offerReveal](int result2)
        {
            if (offerReveal && result2 == 2)
                outputDir.revealToUser();
        });
    }
}

void RegionController::showBatchExportDialog(Document* doc, juce::Component* /*parent*/)
{
    if (!doc)
        return;

    // Validate preconditions
    if (!doc->getAudioEngine().isFileLoaded())
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon,
            "No Audio File",
            "Please load an audio file before exporting regions.",
            "OK");
        return;
    }

    if (doc->getRegionManager().getNumRegions() == 0)
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::InfoIcon,
            "No Regions to Export",
            "There are no regions defined in this file.\n\n"
            "Create regions first using:\n"
            "  - R - Create region from selection\n"
            "  - Cmd+Shift+R - Auto-create regions (Strip Silence)",
            "OK");
        return;
    }

    // Show the batch export dialog (format / bit depth / scope / naming).
    auto dialogResult = BatchExportDialog::showDialog(doc->getFile(),
                                                      doc->getRegionManager());
    if (!dialogResult.has_value())
        return;  // User cancelled

    const auto& dlg = dialogResult.value();

    // Translate dialog settings into exporter settings.
    RegionExporter::ExportSettings settings;
    settings.outputDirectory   = dlg.outputDirectory;
    settings.includeRegionName = dlg.includeRegionName;
    settings.includeIndex      = dlg.includeIndex;
    settings.customTemplate    = dlg.customTemplate;
    settings.prefix            = dlg.prefix;
    settings.suffix            = dlg.suffix;
    settings.usePaddedIndex    = dlg.usePaddedIndex;
    settings.suffixBeforeIndex = dlg.suffixBeforeIndex;
    settings.format            = dlg.format;
    settings.bitDepth          = dlg.bitDepth;

    // Selected-subset export: thread the RegionManager's current selection.
    if (dlg.exportSelectedOnly)
    {
        for (int idx : doc->getRegionManager().getSelectedRegionIndices())
            settings.regionIndicesToExport.push_back(idx);

        if (settings.regionIndicesToExport.empty())
        {
            juce::AlertWindow::showMessageBoxAsync(
                juce::AlertWindow::InfoIcon,
                "No Regions Selected",
                "No regions are selected. Select one or more regions, or choose "
                "\"All regions\".",
                "OK");
            return;
        }
    }

    // Sec 6.9: copy the audio buffer and region data BEFORE handing them to the
    // worker thread. The worker owns these copies and never touches the live
    // Document or UI.
    auto bufferCopy = std::make_shared<juce::AudioBuffer<float>>();
    bufferCopy->makeCopyOf(doc->getBufferManager().getBuffer());
    const double sampleRate = doc->getAudioEngine().getSampleRate();

    auto regionsCopy = std::make_shared<RegionManager>();
    for (int i = 0; i < doc->getRegionManager().getNumRegions(); ++i)
        if (const Region* r = doc->getRegionManager().getRegion(i))
            regionsCopy->addRegion(*r);

    const juce::File sourceFile = doc->getFile();
    auto result = std::make_shared<RegionExporter::ExportResult>();

    ProgressDialog::runWithProgress(
        "Exporting Regions",
        [bufferCopy, sampleRate, regionsCopy, sourceFile, settings, result]
            (ProgressCallback progress) -> bool
        {
            // Adapt RegionExporter's (current,total,name) callback onto the
            // ProgressDialog (0..1, status) callback. Returning false from
            // progress() (Cancel) propagates as a stop request.
            auto adapter = [&progress](int current, int total, const juce::String& name) -> bool
            {
                const float p = (total > 0) ? (float) current / (float) total : 0.0f;
                return progress(p, "Exporting: " + name);
            };

            *result = RegionExporter::exportRegionsEx(*bufferCopy, sampleRate,
                                                      *regionsCopy, sourceFile,
                                                      settings, adapter);

            // Always report "completed" so the summary handler runs; the
            // result object carries success/cancel/failure detail.
            return true;
        },
        [result, outputDir = settings.outputDirectory](bool /*success*/)
        {
            showExportSummary(*result, outputDir);
        });
}
