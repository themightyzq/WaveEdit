cmake_minimum_required(VERSION 3.15)

# Project definition
project(WaveEdit VERSION 0.1.0 LANGUAGES C CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add JUCE as a subdirectory
add_subdirectory(JUCE)

# Formats WaveEdit can open, registered with the OS below. m4a decoding is
# CoreAudio-only (open-only, no encoder), so it is advertised on macOS only.
set(WAVEEDIT_DOCUMENT_EXTENSIONS wav flac ogg mp3 aif aiff)
if(APPLE)
    list(APPEND WAVEEDIT_DOCUMENT_EXTENSIONS m4a)
endif()

# If you want to use JUCE's bundled modules
juce_add_gui_app(WaveEdit
    COMPANY_NAME "WaveEdit"
    PRODUCT_NAME "WaveEdit"
    BUNDLE_ID "com.waveedit.app"

    # Register the audio formats WaveEdit can open so the OS offers it for
    # "Open With" / double-click and passes the file path to the app
    # (macOS CFBundleDocumentTypes, Windows file associations).
    DOCUMENT_EXTENSIONS ${WAVEEDIT_DOCUMENT_EXTENSIONS}

    # Icon files (optional, can add later)
    # ICON_BIG ...
    # ICON_SMALL ...
)

# Target sources
target_sources(WaveEdit
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.h
        Source/Controllers/FileController.cpp
        Source/Controllers/FileController.h
        Source/Controllers/DSPController.cpp
        Source/Controllers/DSPController_Advanced.cpp
        Source/Controllers/DSPController.h
        Source/Controllers/RegionController.cpp
        Source/Controllers/RegionController_Export.cpp
        Source/Controllers/RegionController.h
        Source/Controllers/MarkerController.cpp
        Source/Controllers/MarkerController.h
        Source/Controllers/ClipboardController.cpp
        Source/Controllers/ClipboardController.h
        Source/Controllers/PlaybackController.cpp
        Source/Controllers/PlaybackController.h
        Source/Controllers/RecordingController.cpp
        Source/Controllers/RecordingController.h
        Source/Controllers/DialogController.cpp
        Source/Controllers/DialogController.h
        Source/Controllers/PluginController.cpp
        Source/Controllers/PluginController.h
        Source/Audio/AudioEngine.cpp
        Source/Audio/AudioEngine_Preview.cpp
        Source/Audio/AudioEngine_MemorySource.cpp
        Source/Audio/AudioEngine.h
        Source/Audio/AudioBufferManager.cpp
        Source/Audio/AudioBufferManager.h
        Source/Audio/ChannelLayout.h
        Source/Audio/AudioFileManager.cpp
        Source/Audio/AudioFileManager_Cues.cpp
        Source/Audio/AudioFileManager.h
        Source/Audio/AudioProcessor.cpp
        Source/Audio/AudioProcessor.h
        Source/Audio/RecordingEngine.cpp
        Source/Audio/RecordingEngine.h
        Source/DSP/AudioGenerator.cpp
        Source/DSP/AudioGenerator.h
        Source/DSP/DynamicParametricEQ.cpp
        Source/DSP/DynamicParametricEQ.h
        Source/DSP/EQPresetManager.cpp
        Source/DSP/EQPresetManager.h
        Source/DSP/HeadTailRecipe.h
        Source/DSP/HeadTailEngine.cpp
        Source/DSP/HeadTailEngine.h
        Source/DSP/TimePitchEngine.cpp
        Source/DSP/TimePitchEngine.h
        Source/DSP/LoopRecipe.h
        Source/DSP/LoopEngine.cpp
        Source/DSP/LoopEngine.h
        Source/Commands/CommandIDs.h
        Source/Commands/CommandHandler.cpp
        Source/Commands/CommandHandler_GetInfo.cpp
        Source/Commands/CommandHandler.h
        Source/Commands/MenuBuilder.cpp
        Source/Commands/MenuBuilder.h
        Source/Utils/Settings.cpp
        Source/Utils/Settings.h
        Source/Utils/BWFMetadata.cpp
        Source/Utils/BWFMetadata.h
        Source/Utils/iXMLMetadata.cpp
        Source/Utils/iXMLMetadata.h
        Source/Utils/AudioClipboard.cpp
        Source/Utils/AudioClipboard.h
        Source/Utils/AutomationClipboard.h
        Source/Utils/AutoSaveRecovery.cpp
        Source/Utils/AutoSaveRecovery.h
        Source/Utils/ThumbnailDiskCache.cpp
        Source/Utils/ThumbnailDiskCache.h
        Source/Utils/AudioBufferInputSource.cpp
        Source/Utils/AudioBufferInputSource.h
        Source/Utils/ToolbarConfig.cpp
        Source/Utils/ToolbarConfig.h
        Source/Utils/ToolbarManager.cpp
        Source/Utils/ToolbarManager.h
        Source/UI/WaveformDisplay.cpp
        Source/UI/WaveformDisplay_Render.cpp
        Source/UI/WaveformDisplay_Interact.cpp
        Source/UI/SelectionInfoPanel.h
        Source/UI/CallbackDocumentWindow.h
        Source/UI/WaveformDisplay.h
        Source/UI/TransportControls.cpp
        Source/UI/TransportControls.h
        Source/UI/CompactTransport.cpp
        Source/UI/CompactTransport.h
        Source/UI/ToolbarButton.cpp
        Source/UI/ToolbarButton.h
        Source/UI/CustomizableToolbar.cpp
        Source/UI/CustomizableToolbar.h
        Source/UI/ToolbarCustomizationDialog.cpp
        Source/UI/ToolbarCustomizationDialog.h
        Source/UI/Meters.cpp
        Source/UI/Meters.h
        Source/UI/GenerateDialog.cpp
        Source/UI/GenerateDialog.h
        Source/UI/InsertSilenceDialog.cpp
        Source/UI/InsertSilenceDialog.h
        Source/UI/GainDialog.cpp
        Source/UI/GainDialog.h
        Source/UI/ChannelConverterDialog.cpp
        Source/UI/ChannelConverterDialog.h
        Source/UI/ChannelExtractorDialog.cpp
        Source/UI/ChannelExtractorDialog.h
        Source/UI/NewFileDialog.cpp
        Source/UI/NewFileDialog.h
        Source/UI/NormalizeDialog.cpp
        Source/UI/NormalizeDialog.h
        Source/UI/FadeDialog.cpp
        Source/UI/FadeDialog.h
        Source/UI/FadeCurvePreview.cpp
        Source/UI/FadeCurvePreview.h
        Source/UI/RecordingDialog.cpp
        Source/UI/RecordingDialog.h
        Source/UI/GraphicalEQEditor.cpp
        Source/UI/GraphicalEQEditor_Presets.cpp
        Source/UI/GraphicalEQEditor.h
        Source/UI/TabComponent.cpp
        Source/UI/TabComponent.h
        Source/UI/ErrorDialog.cpp
        Source/UI/ErrorDialog.h
        Source/UI/SettingsPanel.cpp
        Source/UI/SettingsPanel.h
        Source/UI/FilePropertiesDialog.cpp
        Source/UI/FilePropertiesDialog.h
        Source/UI/BWFEditorDialog.cpp
        Source/UI/BWFEditorDialog.h
        Source/UI/iXMLEditorDialog.cpp
        Source/UI/iXMLEditorDialog.h
        Source/UI/GoToPositionDialog.cpp
        Source/UI/GoToPositionDialog.h
        Source/UI/KeyboardCheatSheetDialog.cpp
        Source/UI/KeyboardCheatSheetDialog.h
        Source/UI/EditRegionBoundariesDialog.cpp
        Source/UI/EditRegionBoundariesDialog.h
        Source/UI/StripSilenceDialog.cpp
        Source/UI/StripSilenceDialog.h
        Source/UI/HeadTailDialog.cpp
        Source/UI/HeadTailDialog.h
        Source/UI/TimePitchDialog.cpp
        Source/UI/TimePitchDialog.h
        Source/UI/LoopingToolsDialog.cpp
        Source/UI/LoopingToolsDialog.h
        Source/UI/CommandPalette.cpp
        Source/UI/CommandPalette.h
        Source/UI/BatchExportDialog.cpp
        Source/UI/BatchExportDialog.h
        Source/UI/SaveAsOptionsPanel.cpp
        Source/UI/SaveAsOptionsPanel.h
        Source/UI/ShortcutEditorPanel.cpp
        Source/UI/ShortcutEditorPanel.h
        Source/UI/ProgressDialog.cpp
        Source/UI/ProgressDialog.h
        Source/Utils/ProgressCallback.h
        Source/Utils/Document.cpp
        Source/Utils/Document.h
        Source/Utils/DocumentManager.cpp
        Source/Utils/DocumentManager.h
        Source/Utils/Region.cpp
        Source/Utils/Region.h
        Source/Utils/RegionManager.cpp
        Source/Utils/RegionManager.h
        Source/Utils/SidecarPolicy.cpp
        Source/Utils/SidecarPolicy.h
        Source/Utils/RegionExporter.cpp
        Source/Utils/RegionExporter.h
        Source/Utils/Marker.cpp
        Source/Utils/Marker.h
        Source/Utils/MarkerManager.cpp
        Source/Utils/MarkerManager.h
        Source/Utils/KeymapManager.cpp
        Source/Utils/KeymapManager.h
        Source/Utils/UCSCategorySuggester.cpp
        Source/Utils/UCSCategorySuggester_generated.cpp
        Source/Utils/UCSCategorySuggester.h
        Source/Utils/UndoActions/AudioUndoActions.h
        Source/Utils/UndoActions/LevelUndoActions.h
        Source/Utils/UndoActions/RangeUndoActions.h
        Source/Utils/UndoActions/TransformUndoActions.h
        Source/Utils/UndoActions/FadeUndoActions.h
        Source/Utils/UndoActions/RegionUndoActions.h
        Source/Utils/UndoActions/RegionLifecycleUndoActions.h
        Source/Utils/UndoActions/RegionEditUndoActions.h
        Source/Utils/UndoActions/RegionDerivationUndoActions.h
        Source/Utils/UndoActions/MarkerUndoActions.h
        Source/Utils/UndoActions/PluginUndoActions.h
        Source/Utils/UndoActions/ChannelUndoActions.h
        Source/Utils/UndoActions/AutomationUndoActions.h
        Source/UI/RegionDisplay.cpp
        Source/UI/RegionDisplay.h
        Source/UI/SidecarNotifications.cpp
        Source/UI/SidecarNotifications.h
        Source/UI/RegionListPanel.cpp
        Source/UI/RegionListPanel_BatchRename.cpp
        Source/UI/RegionListPanel.h
        Source/UI/MarkerListPanel.cpp
        Source/UI/MarkerListPanel.h
        Source/UI/MarkerDisplay.cpp
        Source/UI/MarkerDisplay.h
        Source/UI/SpectrumAnalyzer.cpp
        Source/UI/SpectrumAnalyzer.h
        Source/UI/PluginManagerDialog.cpp
        Source/UI/PluginManagerDialog.h
        Source/UI/PluginChainPanel.cpp
        Source/UI/PluginChainPanel.h
        Source/UI/PluginChainWindow.cpp
        Source/UI/PluginChainWindow.h
        Source/UI/PluginEditorWindow.cpp
        Source/UI/PluginEditorWindow.h
        Source/UI/AutomationLanesPanel.cpp
        Source/UI/AutomationLanesPanel.h
        Source/UI/OfflinePluginDialog.cpp
        Source/UI/OfflinePluginDialog.h
        Source/UI/UIConstants.h
        Source/UI/WaveEditLookAndFeel.cpp
        Source/UI/WaveEditLookAndFeel.h
        Source/UI/WaveEditTheme.cpp
        Source/UI/WaveEditTheme.h
        Source/UI/ThemeManager.cpp
        Source/UI/ThemeManager.h
        Source/Plugins/PluginManager.cpp
        Source/Plugins/PluginManager_Persistence.cpp
        Source/Plugins/PluginManager.h
        Source/Plugins/PluginChain.cpp
        Source/Plugins/PluginChain.h
        Source/Plugins/PluginChainNode.cpp
        Source/Plugins/PluginChainNode.h
        Source/Plugins/PluginPresetManager.cpp
        Source/Plugins/PluginPresetManager.h
        Source/Plugins/PluginScannerProtocol.h
        Source/Plugins/PluginScannerWorker.cpp
        Source/Plugins/PluginScannerWorker.h
        Source/Plugins/PluginScannerCoordinator.cpp
        Source/Plugins/PluginScannerCoordinator.h
        Source/Plugins/PluginScanState.h
        Source/Plugins/PluginScanDialogs.cpp
        Source/Plugins/PluginScanDialogs.h
        Source/Plugins/PluginPathsPanel.cpp
        Source/Plugins/PluginPathsPanel.h
        Source/Plugins/PluginChainRenderer.cpp
        Source/Plugins/PluginChainRenderer.h
        Source/Automation/AutomationData.h
        Source/Automation/AutomationManager.cpp
        Source/Automation/AutomationManager.h
        Source/Automation/AutomationRecorder.cpp
        Source/Automation/AutomationRecorder.h
        Source/Batch/BatchProcessorSettings.cpp
        Source/Batch/BatchProcessorSettings.h
        Source/Batch/BatchJob.cpp
        Source/Batch/BatchJob.h
        Source/Batch/BatchProcessorEngine.cpp
        Source/Batch/BatchProcessorEngine.h
        Source/Batch/BatchPresetManager.cpp
        Source/Batch/BatchPresetManager.h
        Source/Batch/BatchInputFilter.h
        Source/Batch/BatchProcessorDialog.cpp
        Source/Batch/BatchProcessorDialog_Components.cpp
        Source/Batch/BatchProcessorDialog_Preview.cpp
        Source/Batch/BatchProcessorDialog.h
)

# JUCE modules required for audio editing application
target_link_libraries(WaveEdit
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Preprocessor definitions
target_compile_definitions(WaveEdit
    PRIVATE
        # JUCE settings
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_MP3AUDIOFORMAT=1  # Enable MP3 encoding with LAME
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:WaveEdit,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:WaveEdit,JUCE_VERSION>"

        # Enable modal loops for dialogs (required for BatchExportDialog, FilePropertiesDialog, etc.)
        JUCE_MODAL_LOOPS_PERMITTED=1

        # VST3 Plugin Hosting Support
        JUCE_PLUGINHOST_VST3=1
        # AudioUnit Plugin Hosting Support (macOS only)
        JUCE_PLUGINHOST_AU=1
)

# ----------------------------------------------------------------------------
# Locate SoundTouch (LGPL-2.1) — required for time-stretch / pitch-shift DSP.
# Done BEFORE target_include_directories / target_link_libraries so the
# variables are populated when those calls evaluate ${SOUNDTOUCH_*} on a
# fresh CMake configure (CI does no-cache builds). Bundle/install copy
# steps live further down once the WaveEdit target exists.
# ----------------------------------------------------------------------------
find_library(SOUNDTOUCH_LIBRARY
    NAMES SoundTouch libSoundTouch
    PATHS
        /opt/homebrew/lib            # Apple Silicon Homebrew
        /opt/homebrew/Cellar/sound-touch/*/lib
        /usr/local/lib               # Intel Homebrew
        /usr/local/Cellar/sound-touch/*/lib
        /opt/local/lib               # MacPorts
        /usr/lib
        /usr/lib/x86_64-linux-gnu
        /usr/lib/aarch64-linux-gnu
)

find_path(SOUNDTOUCH_INCLUDE_DIR
    NAMES SoundTouch.h
    PATHS
        /opt/homebrew/include/soundtouch
        /opt/homebrew/Cellar/sound-touch/*/include/soundtouch
        /usr/local/include/soundtouch
        /usr/local/Cellar/sound-touch/*/include/soundtouch
        /opt/local/include/soundtouch
        /usr/include/soundtouch
    # vcpkg + other prefix-style installs put headers under <prefix>/include/soundtouch.
    PATH_SUFFIXES soundtouch
)

if(NOT (SOUNDTOUCH_LIBRARY AND SOUNDTOUCH_INCLUDE_DIR))
    message(FATAL_ERROR
        "SoundTouch not found. Install via: brew install sound-touch (macOS), "
        "apt-get install libsoundtouch-dev (Linux), or vcpkg install "
        "soundtouch:x64-windows + pass -DCMAKE_TOOLCHAIN_FILE=...vcpkg.cmake "
        "(Windows).")
endif()

message(STATUS "Found SoundTouch library: ${SOUNDTOUCH_LIBRARY}")
message(STATUS "Found SoundTouch headers:  ${SOUNDTOUCH_INCLUDE_DIR}")

# Include directories
target_include_directories(WaveEdit
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${SOUNDTOUCH_INCLUDE_DIR}
)

# Link SoundTouch (LGPL-2.1) for time-stretch / pitch-shift DSP.
target_link_libraries(WaveEdit PRIVATE ${SOUNDTOUCH_LIBRARY})

# Platform-specific settings
if(APPLE)
    target_compile_definitions(WaveEdit PRIVATE
        JUCE_COREAUDIO_ENABLED=1
    )
    # macOS 10.13+ required
    set_target_properties(WaveEdit PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.waveedit.app"
        MACOSX_BUNDLE_BUNDLE_NAME "WaveEdit"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
elseif(WIN32)
    target_compile_definitions(WaveEdit PRIVATE
        JUCE_WASAPI_ENABLED=1
        JUCE_DIRECTSOUND_ENABLED=0
    )
elseif(UNIX)
    target_compile_definitions(WaveEdit PRIVATE
        JUCE_ALSA_ENABLED=1
        JUCE_JACK_ENABLED=1
    )
endif()

# ============================================================================
# Bundle LAME MP3 Encoder Library
# ============================================================================
# For end-user releases, bundle LAME library in app bundle/installation
# This provides zero-friction MP3 encoding support without requiring users
# to manually install LAME. LGPL compliance: dynamically linked, source
# code available at https://lame.sourceforge.io/

if(APPLE)
    # Find LAME library - check both Homebrew locations
    find_library(LAME_LIBRARY
        NAMES mp3lame libmp3lame
        PATHS
            /opt/homebrew/lib          # Apple Silicon Homebrew
            /usr/local/lib             # Intel Homebrew
            /opt/local/lib             # MacPorts
        NO_DEFAULT_PATH
    )

    if(LAME_LIBRARY)
        message(STATUS "Found LAME library: ${LAME_LIBRARY}")

        # For Release builds, bundle LAME into app Frameworks
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            add_custom_command(TARGET WaveEdit POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                    "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Frameworks"
                COMMAND ${CMAKE_COMMAND} -E copy
                    "${LAME_LIBRARY}"
                    "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Frameworks/libmp3lame.dylib"
                COMMAND install_name_tool -id
                    "@rpath/libmp3lame.dylib"
                    "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Frameworks/libmp3lame.dylib"
                # Rewrite the executable's load command to the bundled @rpath
                # copy (reads the exe's actual reference; robust to versioning).
                COMMAND bash "${CMAKE_SOURCE_DIR}/cmake/relink_bundled_dylib.sh"
                    "$<TARGET_FILE:WaveEdit>" libmp3lame "@rpath/libmp3lame.dylib"
                COMMENT "Bundling LAME MP3 encoder library and relinking executable"
            )
            message(STATUS "LAME will be bundled in Release builds")
        else()
            message(STATUS "Debug build - LAME not bundled (use system library)")
        endif()
    else()
        message(WARNING "LAME library not found - MP3 encoding will require manual LAME installation")
        message(WARNING "Install with: brew install lame")
    endif()

    # Set rpath to find bundled libraries
    set_target_properties(WaveEdit PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@executable_path/../Frameworks"
    )

elseif(WIN32)
    # Windows: Look for LAME DLL
    find_file(LAME_DLL
        NAMES lame_enc.dll libmp3lame.dll
        PATHS
            "C:/Program Files/LAME"
            "C:/Program Files (x86)/LAME"
            "${CMAKE_SOURCE_DIR}/ThirdParty/lame/bin"
    )

    if(LAME_DLL)
        message(STATUS "Found LAME DLL: ${LAME_DLL}")

        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            add_custom_command(TARGET WaveEdit POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                    "${LAME_DLL}"
                    "$<TARGET_FILE_DIR:WaveEdit>"
                COMMENT "Bundling LAME MP3 encoder DLL"
            )
            message(STATUS "LAME DLL will be bundled in Release builds")
        endif()
    else()
        message(WARNING "LAME DLL not found - MP3 encoding will require manual LAME installation")
        message(WARNING "Download from: https://lame.sourceforge.io/")
    endif()

elseif(UNIX)
    # Linux: Look for LAME shared object
    find_library(LAME_LIBRARY
        NAMES mp3lame libmp3lame
        PATHS
            /usr/lib
            /usr/local/lib
            /usr/lib/x86_64-linux-gnu
            /usr/lib/aarch64-linux-gnu
    )

    if(LAME_LIBRARY)
        message(STATUS "Found LAME library: ${LAME_LIBRARY}")

        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            add_custom_command(TARGET WaveEdit POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                    "${LAME_LIBRARY}"
                    "$<TARGET_FILE_DIR:WaveEdit>"
                COMMENT "Bundling LAME MP3 encoder library"
            )
            message(STATUS "LAME will be bundled in Release builds")
        endif()
    else()
        message(WARNING "LAME library not found - MP3 encoding will require manual LAME installation")
        message(WARNING "Install with: sudo apt-get install libmp3lame0")
    endif()

    # Set rpath to find bundled libraries
    set_target_properties(WaveEdit PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN"
    )
endif()

# ----------------------------------------------------------------------------
# Bundled-library rpath for the app executable. Set unconditionally: LAME,
# SoundTouch, and any future bundled dylib all rely on it, so it must not
# depend on whether one specific optional library was found (M8 -- it was
# previously nested inside the LAME find-block, risking removal on edit).
# Idempotent with the per-lib settings above (same values).
# ----------------------------------------------------------------------------
if(APPLE)
    set_target_properties(WaveEdit PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "@executable_path/../Frameworks")
elseif(UNIX)
    set_target_properties(WaveEdit PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN")
endif()

# ----------------------------------------------------------------------------
# Make LAME actually USED. JUCE's MP3AudioFormat is decode-only, so WaveEdit
# ships its own LAME-backed encoder (Source/Audio/LameMP3AudioFormat.cpp).
# The block above only bundles the dylib; here we find the header, add the
# encoder source, and link+define WAVEEDIT_HAVE_LAME when lib AND header are
# present. Without LAME the encoder TU compiles empty and MP3 export reports
# unavailable at runtime (the build still succeeds). LGPL: dynamically linked.
# ----------------------------------------------------------------------------
find_path(LAME_INCLUDE_DIR
    NAMES lame/lame.h
    PATHS
        /opt/homebrew/opt/lame/include   /opt/homebrew/include
        /usr/local/opt/lame/include      /usr/local/include
        /opt/local/include               /usr/include
        "${CMAKE_SOURCE_DIR}/ThirdParty/lame/include"
)
# Unconditional so EVERY platform (incl. Windows/vcpkg) can link LAME, not just
# the macOS/Linux bundling branches above -- otherwise the gate below is never
# true on Windows and MP3 silently ships disabled.
find_library(LAME_LIBRARY
    NAMES mp3lame libmp3lame
    PATHS
        /opt/homebrew/opt/lame/lib   /opt/homebrew/lib
        /usr/local/opt/lame/lib      /usr/local/lib
        /opt/local/lib               /usr/lib
        /usr/lib/x86_64-linux-gnu    /usr/lib/aarch64-linux-gnu
        "${CMAKE_SOURCE_DIR}/ThirdParty/lame/lib"
)
target_sources(WaveEdit PRIVATE
    Source/Audio/LameMP3AudioFormat.cpp
    Source/Audio/LameMP3AudioFormat.h)
if(LAME_LIBRARY AND LAME_INCLUDE_DIR)
    target_include_directories(WaveEdit PRIVATE ${LAME_INCLUDE_DIR})
    target_link_libraries(WaveEdit PRIVATE ${LAME_LIBRARY})
    target_compile_definitions(WaveEdit PRIVATE WAVEEDIT_HAVE_LAME=1)
    message(STATUS "LAME MP3 encoder: ENABLED (${LAME_LIBRARY})")
    set(WAVEEDIT_MP3_ENABLED TRUE CACHE INTERNAL "")
else()
    message(WARNING "LAME MP3 encoder: DISABLED - lib/header not found; MP3 export unavailable at runtime")
    set(WAVEEDIT_MP3_ENABLED FALSE CACHE INTERNAL "")
endif()

# ============================================================================
# Bundle SoundTouch (LGPL-2.1) for time-stretch / pitch-shift DSP
# ============================================================================
# Same shape as the LAME integration: dynamically linked, source code
# available at https://www.surina.net/soundtouch/. NOTICE has the full
# attribution text. The find_library / find_path calls are higher up the
# file (before target_include_directories) so SOUNDTOUCH_LIBRARY and
# SOUNDTOUCH_INCLUDE_DIR are populated before they are consumed.

if(SOUNDTOUCH_LIBRARY AND SOUNDTOUCH_INCLUDE_DIR)
    if(APPLE AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET WaveEdit POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy
                "${SOUNDTOUCH_LIBRARY}"
                "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Frameworks/libSoundTouch.1.dylib"
            COMMAND install_name_tool -id
                "@rpath/libSoundTouch.1.dylib"
                "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Frameworks/libSoundTouch.1.dylib"
            # Relink the executable to the bundled copy. Reads the exe's
            # actual load command at build time (robust to which symlink
            # find_library resolved); the app bundle already carries an
            # @executable_path/../Frameworks rpath that resolves @rpath.
            COMMAND bash "${CMAKE_SOURCE_DIR}/cmake/relink_bundled_dylib.sh"
                "$<TARGET_FILE:WaveEdit>" libSoundTouch "@rpath/libSoundTouch.1.dylib"
            COMMENT "Bundling SoundTouch library and relinking executable"
        )
    elseif(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
        # On Windows, SOUNDTOUCH_LIBRARY is the import library (.lib); the
        # runtime needs the .dll alongside the .exe. Locate the DLL the same
        # way (vcpkg lays it out under <prefix>/bin).
        find_file(SOUNDTOUCH_DLL
            NAMES SoundTouch.dll
            PATHS
                "${SOUNDTOUCH_INCLUDE_DIR}/../../bin"
                "$ENV{VCPKG_INSTALLATION_ROOT}/installed/x64-windows/bin"
                "C:/vcpkg/installed/x64-windows/bin"
        )
        if(SOUNDTOUCH_DLL)
            message(STATUS "Found SoundTouch DLL: ${SOUNDTOUCH_DLL}")
            add_custom_command(TARGET WaveEdit POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                    "${SOUNDTOUCH_DLL}"
                    "$<TARGET_FILE_DIR:WaveEdit>"
                COMMENT "Bundling SoundTouch DLL")
        else()
            message(WARNING "SoundTouch.dll not found — runtime will need it on PATH")
        endif()
    elseif(UNIX AND CMAKE_BUILD_TYPE STREQUAL "Release")
        add_custom_command(TARGET WaveEdit POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                "${SOUNDTOUCH_LIBRARY}"
                "$<TARGET_FILE_DIR:WaveEdit>"
            COMMENT "Bundling SoundTouch library")
    endif()
endif()

# ============================================================================
# Copy keyboard template resources to app bundle
# ============================================================================
if(APPLE)
    # On macOS, copy templates to app bundle Resources directory
    add_custom_command(TARGET WaveEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Resources/Keymaps"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Templates/Keymaps"
            "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Resources/Keymaps"
        COMMENT "Copying keyboard templates to app bundle"
    )
elseif(WIN32)
    # On Windows, copy templates next to executable
    add_custom_command(TARGET WaveEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:WaveEdit>/Keymaps"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Templates/Keymaps"
            "$<TARGET_FILE_DIR:WaveEdit>/Keymaps"
        COMMENT "Copying keyboard templates to executable directory"
    )
else()
    # On Linux, copy templates next to executable
    add_custom_command(TARGET WaveEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:WaveEdit>/Keymaps"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Templates/Keymaps"
            "$<TARGET_FILE_DIR:WaveEdit>/Keymaps"
        COMMENT "Copying keyboard templates to executable directory"
    )
endif()

# ============================================================================
# Copy toolbar template resources to app bundle
# ============================================================================
if(APPLE)
    # On macOS, copy templates to app bundle Resources directory
    add_custom_command(TARGET WaveEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Resources/Toolbars"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Templates/Toolbars"
            "$<TARGET_BUNDLE_DIR:WaveEdit>/Contents/Resources/Toolbars"
        COMMENT "Copying toolbar templates to app bundle"
    )
elseif(WIN32)
    # On Windows, copy templates next to executable
    add_custom_command(TARGET WaveEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:WaveEdit>/Toolbars"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Templates/Toolbars"
            "$<TARGET_FILE_DIR:WaveEdit>/Toolbars"
        COMMENT "Copying toolbar templates to executable directory"
    )
else()
    # On Linux, copy templates next to executable
    add_custom_command(TARGET WaveEdit POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:WaveEdit>/Toolbars"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Templates/Toolbars"
            "$<TARGET_FILE_DIR:WaveEdit>/Toolbars"
        COMMENT "Copying toolbar templates to executable directory"
    )
endif()

# ============================================================================
# Re-sign the app bundle -- MUST be the last bundle-modifying POST_BUILD step
# ============================================================================
# install_name_tool relinking AND the Keymaps/Toolbars resource copies above
# invalidate the linker's ad-hoc code signature; macOS then SIGKILLs the app on
# launch ("Killed: 9"). Re-sign ad-hoc here, after every other bundle edit.
# (Release notarization, when cut, re-signs with the real cert afterwards.)
if(APPLE AND CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET WaveEdit POST_BUILD
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:WaveEdit>"
        COMMENT "Re-signing app bundle (ad-hoc) after all bundle edits")
endif()

# Copy resources (for future phases - icons, fonts, etc.)
# file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/Source/Assets
#      DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Installation rules (optional)
install(TARGETS WaveEdit
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# ============================================================================
# Automated Testing Infrastructure
# ============================================================================

# Option to enable/disable test building
option(WAVEEDIT_BUILD_TESTS "Build WaveEdit automated tests" ON)

if(WAVEEDIT_BUILD_TESTS)
    message(STATUS "Building WaveEdit automated tests")

    # Create a static library from Source/ files for test linking
    # This avoids having Main.cpp's main() function conflict with test runner
    add_library(WaveEditCore STATIC
        Source/Controllers/FileController.cpp
        Source/Controllers/FileController.h
        Source/Controllers/DSPController.cpp
        Source/Controllers/DSPController_Advanced.cpp
        Source/Controllers/DSPController.h
        Source/Audio/AudioEngine.cpp
        Source/Audio/AudioEngine_Preview.cpp
        Source/Audio/AudioEngine_MemorySource.cpp
        Source/Audio/AudioEngine.h
        Source/Audio/AudioBufferManager.cpp
        Source/Audio/AudioBufferManager.h
        Source/Audio/ChannelLayout.h
        Source/Audio/AudioFileManager.cpp
        Source/Audio/AudioFileManager_Cues.cpp
        Source/Audio/AudioFileManager.h
        Source/Audio/AudioProcessor.cpp
        Source/Audio/AudioProcessor.h
        Source/DSP/AudioGenerator.cpp
        Source/DSP/AudioGenerator.h
        Source/DSP/DynamicParametricEQ.cpp
        Source/DSP/DynamicParametricEQ.h
        Source/DSP/EQPresetManager.cpp
        Source/DSP/EQPresetManager.h
        Source/DSP/HeadTailRecipe.h
        Source/DSP/HeadTailEngine.cpp
        Source/DSP/HeadTailEngine.h
        Source/DSP/TimePitchEngine.cpp
        Source/DSP/TimePitchEngine.h
        Source/DSP/LoopRecipe.h
        Source/DSP/LoopEngine.cpp
        Source/DSP/LoopEngine.h
        Source/Commands/CommandIDs.h
        Source/Commands/CommandHandler.cpp
        Source/Commands/CommandHandler_GetInfo.cpp
        Source/Commands/CommandHandler.h
        Source/Commands/MenuBuilder.cpp
        Source/Commands/MenuBuilder.h
        Source/Utils/Settings.cpp
        Source/Utils/Settings.h
        Source/Utils/BWFMetadata.cpp
        Source/Utils/BWFMetadata.h
        Source/Utils/iXMLMetadata.cpp
        Source/Utils/iXMLMetadata.h
        Source/Utils/AudioClipboard.cpp
        Source/Utils/AudioClipboard.h
        Source/Utils/AutomationClipboard.h
        Source/Utils/AutoSaveRecovery.cpp
        Source/Utils/AutoSaveRecovery.h
        Source/Utils/ThumbnailDiskCache.cpp
        Source/Utils/ThumbnailDiskCache.h
        Source/Utils/AudioBufferInputSource.cpp
        Source/Utils/AudioBufferInputSource.h
        Source/Utils/ToolbarConfig.cpp
        Source/Utils/ToolbarConfig.h
        Source/Utils/ToolbarManager.cpp
        Source/Utils/ToolbarManager.h
        Source/UI/WaveformDisplay.cpp
        Source/UI/WaveformDisplay_Render.cpp
        Source/UI/WaveformDisplay_Interact.cpp
        Source/UI/SelectionInfoPanel.h
        Source/UI/CallbackDocumentWindow.h
        Source/UI/WaveformDisplay.h
        Source/UI/TransportControls.cpp
        Source/UI/TransportControls.h
        Source/UI/CompactTransport.cpp
        Source/UI/CompactTransport.h
        Source/UI/ToolbarButton.cpp
        Source/UI/ToolbarButton.h
        Source/UI/CustomizableToolbar.cpp
        Source/UI/CustomizableToolbar.h
        Source/UI/ToolbarCustomizationDialog.cpp
        Source/UI/ToolbarCustomizationDialog.h
        Source/UI/Meters.cpp
        Source/UI/Meters.h
        Source/UI/GenerateDialog.cpp
        Source/UI/GenerateDialog.h
        Source/UI/InsertSilenceDialog.cpp
        Source/UI/InsertSilenceDialog.h
        Source/UI/GainDialog.cpp
        Source/UI/GainDialog.h
        Source/UI/ChannelConverterDialog.cpp
        Source/UI/ChannelConverterDialog.h
        Source/UI/ChannelExtractorDialog.cpp
        Source/UI/ChannelExtractorDialog.h
        Source/UI/NewFileDialog.cpp
        Source/UI/NewFileDialog.h
        Source/UI/NormalizeDialog.cpp
        Source/UI/NormalizeDialog.h
        Source/UI/FadeDialog.cpp
        Source/UI/FadeDialog.h
        Source/UI/FadeCurvePreview.cpp
        Source/UI/FadeCurvePreview.h
        Source/UI/RecordingDialog.cpp
        Source/UI/RecordingDialog.h
        Source/UI/GraphicalEQEditor.cpp
        Source/UI/GraphicalEQEditor_Presets.cpp
        Source/UI/GraphicalEQEditor.h
        Source/UI/TabComponent.cpp
        Source/UI/TabComponent.h
        Source/UI/ErrorDialog.cpp
        Source/UI/ErrorDialog.h
        Source/UI/SettingsPanel.cpp
        Source/UI/SettingsPanel.h
        Source/UI/FilePropertiesDialog.cpp
        Source/UI/FilePropertiesDialog.h
        Source/UI/BWFEditorDialog.cpp
        Source/UI/BWFEditorDialog.h
        Source/UI/iXMLEditorDialog.cpp
        Source/UI/iXMLEditorDialog.h
        Source/UI/GoToPositionDialog.cpp
        Source/UI/GoToPositionDialog.h
        Source/UI/KeyboardCheatSheetDialog.cpp
        Source/UI/KeyboardCheatSheetDialog.h
        Source/UI/EditRegionBoundariesDialog.cpp
        Source/UI/EditRegionBoundariesDialog.h
        Source/UI/StripSilenceDialog.cpp
        Source/UI/StripSilenceDialog.h
        Source/UI/HeadTailDialog.cpp
        Source/UI/HeadTailDialog.h
        Source/UI/TimePitchDialog.cpp
        Source/UI/TimePitchDialog.h
        Source/Utils/Document.cpp
        Source/Utils/Document.h
        Source/Utils/DocumentManager.cpp
        Source/Utils/DocumentManager.h
        Source/Utils/Region.cpp
        Source/Utils/Region.h
        Source/Utils/RegionManager.cpp
        Source/Utils/RegionManager.h
        Source/Utils/SidecarPolicy.cpp
        Source/Utils/SidecarPolicy.h
        Source/UI/RegionDisplay.cpp
        Source/UI/RegionDisplay.h
        Source/UI/SidecarNotifications.cpp
        Source/UI/SidecarNotifications.h
        Source/UI/RegionListPanel.cpp
        Source/UI/RegionListPanel_BatchRename.cpp
        Source/UI/RegionListPanel.h
        Source/Utils/Marker.cpp
        Source/Utils/Marker.h
        Source/Utils/MarkerManager.cpp
        Source/Utils/MarkerManager.h
        Source/Utils/UCSCategorySuggester.cpp
        Source/Utils/UCSCategorySuggester_generated.cpp
        Source/Utils/UCSCategorySuggester.h
        Source/Utils/UndoActions/AudioUndoActions.h
        Source/Utils/UndoActions/LevelUndoActions.h
        Source/Utils/UndoActions/RangeUndoActions.h
        Source/Utils/UndoActions/TransformUndoActions.h
        Source/Utils/UndoActions/FadeUndoActions.h
        Source/Utils/UndoActions/RegionUndoActions.h
        Source/Utils/UndoActions/RegionLifecycleUndoActions.h
        Source/Utils/UndoActions/RegionEditUndoActions.h
        Source/Utils/UndoActions/RegionDerivationUndoActions.h
        Source/Utils/UndoActions/MarkerUndoActions.h
        Source/Utils/UndoActions/PluginUndoActions.h
        Source/Utils/UndoActions/ChannelUndoActions.h
        Source/Utils/UndoActions/AutomationUndoActions.h
        Source/UI/MarkerDisplay.cpp
        Source/UI/MarkerDisplay.h
        Source/UI/SpectrumAnalyzer.cpp
        Source/UI/SpectrumAnalyzer.h
        Source/UI/PluginManagerDialog.cpp
        Source/UI/PluginManagerDialog.h
        Source/UI/PluginChainPanel.cpp
        Source/UI/PluginChainPanel.h
        Source/UI/PluginChainWindow.cpp
        Source/UI/PluginChainWindow.h
        Source/UI/PluginEditorWindow.cpp
        Source/UI/PluginEditorWindow.h
        Source/UI/AutomationLanesPanel.cpp
        Source/UI/AutomationLanesPanel.h
        Source/UI/OfflinePluginDialog.cpp
        Source/UI/OfflinePluginDialog.h
        Source/UI/UIConstants.h
        Source/UI/WaveEditLookAndFeel.cpp
        Source/UI/WaveEditLookAndFeel.h
        Source/UI/WaveEditTheme.cpp
        Source/UI/WaveEditTheme.h
        Source/UI/ThemeManager.cpp
        Source/UI/ThemeManager.h
        Source/Plugins/PluginManager.cpp
        Source/Plugins/PluginManager_Persistence.cpp
        Source/Plugins/PluginManager.h
        Source/Plugins/PluginChain.cpp
        Source/Plugins/PluginChain.h
        Source/Plugins/PluginChainNode.cpp
        Source/Plugins/PluginChainNode.h
        Source/Plugins/PluginScannerProtocol.h
        Source/Plugins/PluginScannerWorker.cpp
        Source/Plugins/PluginScannerWorker.h
        Source/Plugins/PluginScannerCoordinator.cpp
        Source/Plugins/PluginScannerCoordinator.h
        Source/Plugins/PluginScanState.h
        Source/Plugins/PluginScanDialogs.cpp
        Source/Plugins/PluginScanDialogs.h
        Source/Plugins/PluginPathsPanel.cpp
        Source/Plugins/PluginPathsPanel.h
        Source/Plugins/PluginChainRenderer.cpp
        Source/Plugins/PluginChainRenderer.h
        Source/Plugins/PluginPresetManager.cpp
        Source/Plugins/PluginPresetManager.h
        Source/Automation/AutomationData.h
        Source/Automation/AutomationManager.cpp
        Source/Automation/AutomationManager.h
        Source/Automation/AutomationRecorder.cpp
        Source/Automation/AutomationRecorder.h
        Source/Batch/BatchProcessorSettings.cpp
        Source/Batch/BatchProcessorSettings.h
        Source/Batch/BatchJob.cpp
        Source/Batch/BatchJob.h
        Source/Batch/BatchProcessorEngine.cpp
        Source/Batch/BatchProcessorEngine.h
        Source/Batch/BatchPresetManager.cpp
        Source/Batch/BatchPresetManager.h
        # QA Pass 2: make WaveEditCore link-complete (= app minus Main.cpp) so the
        # test target can exercise controllers/exporters/recording. These were
        # previously app-only, leaving latent unresolved refs (e.g. FileController
        # -> SaveAsOptionsPanel) that only surfaced once a test pulled the object.
        Source/Audio/RecordingEngine.cpp
        Source/Controllers/ClipboardController.cpp
        Source/Controllers/DialogController.cpp
        Source/Controllers/MarkerController.cpp
        Source/Controllers/PlaybackController.cpp
        Source/Controllers/PluginController.cpp
        Source/Controllers/RecordingController.cpp
        Source/Controllers/RegionController.cpp
        Source/Controllers/RegionController_Export.cpp
        Source/Utils/KeymapManager.cpp
        Source/Utils/RegionExporter.cpp
        Source/UI/SaveAsOptionsPanel.cpp
        Source/UI/BatchExportDialog.cpp
        Source/UI/CommandPalette.cpp
        Source/UI/LoopingToolsDialog.cpp
        Source/UI/MarkerListPanel.cpp
        Source/UI/ProgressDialog.cpp
        Source/UI/ShortcutEditorPanel.cpp
        Source/Batch/BatchInputFilter.h
        Source/Batch/BatchProcessorDialog.cpp
        Source/Batch/BatchProcessorDialog_Components.cpp
        Source/Batch/BatchProcessorDialog_Preview.cpp
    )

    # Link WaveEditCore to JUCE modules
    target_link_libraries(WaveEditCore
        PUBLIC
            juce::juce_audio_basics
            juce::juce_audio_devices
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_core
            juce::juce_data_structures
            juce::juce_dsp
            juce::juce_events
            juce::juce_graphics
            juce::juce_gui_basics
            juce::juce_gui_extra
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    # Include directories for WaveEditCore
    target_include_directories(WaveEditCore
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/Source
            ${CMAKE_CURRENT_SOURCE_DIR}/Tests/TestUtils
            ${SOUNDTOUCH_INCLUDE_DIR}
    )

    # Link SoundTouch (LGPL-2.1) — same library used by the WaveEdit binary.
    target_link_libraries(WaveEditCore PUBLIC ${SOUNDTOUCH_LIBRARY})

    # LAME encoder for the core lib / test target (mirrors the app target).
    target_sources(WaveEditCore PRIVATE
        Source/Audio/LameMP3AudioFormat.cpp
        Source/Audio/LameMP3AudioFormat.h)
    if(LAME_LIBRARY AND LAME_INCLUDE_DIR)
        target_include_directories(WaveEditCore PUBLIC ${LAME_INCLUDE_DIR})
        target_link_libraries(WaveEditCore PUBLIC ${LAME_LIBRARY})
        target_compile_definitions(WaveEditCore PUBLIC WAVEEDIT_HAVE_LAME=1)
    endif()

    # Preprocessor definitions for WaveEditCore
    target_compile_definitions(WaveEditCore
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_USE_MP3AUDIOFORMAT=1  # Enable MP3 encoding with LAME
            JUCE_APPLICATION_NAME_STRING="WaveEdit"
            JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
            # Enable modal loops for dialogs (required for DSPController dialog methods)
            JUCE_MODAL_LOOPS_PERMITTED=1
            # VST3 Plugin Hosting Support
            JUCE_PLUGINHOST_VST3=1
            # AudioUnit Plugin Hosting Support (macOS only)
            JUCE_PLUGINHOST_AU=1
    )

    # Platform-specific settings for WaveEditCore
    if(APPLE)
        target_compile_definitions(WaveEditCore PUBLIC
            JUCE_COREAUDIO_ENABLED=1
        )
    elseif(WIN32)
        target_compile_definitions(WaveEditCore PUBLIC
            JUCE_WASAPI_ENABLED=1
            JUCE_DIRECTSOUND_ENABLED=0
        )
    elseif(UNIX)
        target_compile_definitions(WaveEditCore PUBLIC
            JUCE_ALSA_ENABLED=1
            JUCE_JACK_ENABLED=1
        )
    endif()

    # ==============================================================================
    # OPTIONAL: Automated Tests (only if Tests/ directory exists)
    # ==============================================================================
    if(EXISTS "${CMAKE_SOURCE_DIR}/Tests/TestRunner.cpp")
        message(STATUS "Building WaveEdit automated tests")

        # Create test console application using JUCE's console app macro
        juce_add_console_app(WaveEditTests
        COMPANY_NAME "ZQ SFX"
        PRODUCT_NAME "WaveEdit Tests"
        BUNDLE_ID "com.zqsfx.waveedittests"
    )

    # Test source files
    # Note: Add test files as they are created
    target_sources(WaveEditTests
        PRIVATE
            Tests/TestRunner.cpp                                # Main test runner
            Tests/TestUtils/TestAudioFiles.h
            Tests/TestUtils/AudioAssertions.h
            Tests/Unit/AudioEngineTests.cpp                     # Week 2 ✅
            Tests/Unit/AudioBufferManagerTests.cpp              # Week 2 ✅
            Tests/Unit/AudioProcessorTests.cpp                  # Week 2 ✅
            Tests/Unit/FadeCurveTypesTests.cpp                  # Phase 4 ✅ (Fade Curve Types)
            Tests/Unit/HeadTailEngineTests.cpp                  # Head & Tail Engine Tests
            Tests/Unit/TimePitchEngineTests.cpp                 # SoundTouch wrapper
            Tests/Unit/ThemeManagerTests.cpp                    # Color theme system Phase 1
            Tests/Unit/LoopEngineTests.cpp                      # Loop Engine Tests
            Tests/Unit/BWFMetadataTests.cpp                     # Phase 4 ✅ (BWF Metadata System)
            Tests/Unit/BWFEditorDialogTests.cpp                 # Phase 4 ✅ (BWF Editor Dialog)
            Tests/Integration/FileIOTests.cpp                   # Week 3 ✅ (File I/O Integration)
            Tests/Unit/UndoRedoTests.cpp                        # Week 3 ✅ (Undo/Redo Data Integrity)
            Tests/Integration/MultiDocumentTests.cpp            # Week 4 ✅ (Multi-file architecture)
            Tests/Integration/AsyncApplyLifetimeTests.cpp       # C1 UAF lifeline regression guard
            Tests/Unit/AudioGeneratorTests.cpp                  # Generate: tone/noise DSP
            Tests/Integration/GeneratePlacementTests.cpp        # Generate: insert/replace placement + undo
            Tests/Integration/InterFileClipboardTests.cpp       # Week 4 ✅ (Inter-file clipboard)
            Tests/Integration/PlaybackEditingIntegrationTests.cpp # Week 4 ✅ (Playback + Editing)
            Tests/Unit/Phase35FeaturesTests.cpp                 # Week 4 ✅ (Phase 3.5: Time Format, Auto-Save)
            Tests/Integration/EditingToolsTests.cpp             # Week 4 ✅ (Silence, Trim, DC Offset)
            Tests/Unit/GoToPositionDialogTests.cpp              # Week 4 ✅ (Go To Position parsing)
            Tests/Integration/RegionListBatchRenameTests.cpp    # Week 4 ✅ (Batch Rename regions)
            Tests/Integration/SaveAsFormatsTests.cpp            # Phase 4 ✅ (Save As all formats)
            Tests/Integration/MP3EncodeTests.cpp                # LAME MP3 encode round-trip
            Tests/Integration/M4AOpenTests.cpp                  # M4A open gates + CoreAudio decode (mac)
            Tests/Integration/AIFFRoundTripTests.cpp            # AIFF open gates + lossless round-trip
            Tests/Integration/SaveAs8BitTests.cpp               # Phase 4 ✅ (8-bit Save Tests)
            Tests/Integration/SaveSampleFormatTests.cpp         # H1: 32-float sample format

            Tests/Unit/BatchJobTests.cpp                        # Batch Processor tests
            Tests/ChannelSystemTests.cpp                        # Channel system tests (ITU coefficients, extraction, upmix)
            Tests/Unit/RegionManagerTests.cpp                   # Region/Auto Region tests
            Tests/Unit/KeyboardShortcutConflictTests.cpp        # Keymap binding conflicts (JSON-based)
            Tests/Unit/MarkerManagerTests.cpp                   # Marker basic ops + manager + sidecar
            Tests/TestUtils/MockAudioProcessor.h                # Mock plugin processor for tests
            Tests/Unit/AutomationRecorderTests.cpp              # Plugin Parameter Automation Phase 4
            Tests/Unit/AutomationLanesPanelTests.cpp            # Plugin Parameter Automation Phase 5
            Tests/Unit/PluginPresetManagerTests.cpp             # Plugin chain preset format (incl. automation bundle)
            Tests/Unit/CrashRecoveryTests.cpp                   # Crash recovery: detection + supersede
            Tests/Integration/MultiRegionDragTests.cpp          # Multi-region drag/move
            Tests/Integration/RegionListPanelTests.cpp          # RegionListPanel state-side contract
            # --- QA Pass 2 regression tests (2026-05) ---
            Tests/Unit/EQMultichannelTests.cpp                  # H3/H4: multichannel EQ
            Tests/Unit/AudioBufferManagerRegressionTests.cpp    # C1 trim, H1 replaceRange atomicity
            Tests/Integration/AudioFileManagerPass2Tests.cpp    # C11 bext+iXML, C15 resample, M7 escape, M8 overflow
            Tests/Integration/CueChunkTests.cpp                 # WAV cue/adtl embedding + sidecar opt-in
            Tests/Unit/AutoSaveRecoveryPass2Tests.cpp           # M4/M5 autosave path hash + cleanup, H26 drop predicate
            Tests/Unit/DialogInputValidationTests.cpp           # H17/L4 GoTo, H18 gain/normalize NaN/inf
            Tests/Unit/PluginBlacklistTests.cpp                 # M21 exact blacklist match
            Tests/Unit/PluginChainTailTests.cpp                 # H25 batch tail
            Tests/Unit/KeymapCommandResolutionTests.cpp         # H27 command/keymap resolution
            Tests/Unit/RegionUndoIdentityTests.cpp              # H8-H11/H14, C3 region undo by id
            Tests/Unit/RegionExporterPass2Tests.cpp             # C17/H15/H16 export
            Tests/Unit/RegionMarkerJsonValidationTests.cpp      # H22 JSON validation
            Tests/Unit/MarkerJumpUnitsTests.cpp                 # C2 marker jump units
            Tests/Integration/ClipboardChannelMismatchTests.cpp # C12/H5 channel conform, H2 loop selection
            Tests/Unit/AutomationApplyGuardTests.cpp            # H23 apply-guard
            Tests/Unit/AutomationLanesPanelRowIndexTests.cpp    # M16 row index
            Tests/Unit/RecordingEngineTests.cpp                 # M17/M18 recording
            Tests/Unit/HeadTailPreviewTests.cpp                 # HeadTail preview guard
            Tests/Unit/TimePitchDialogTests.cpp                 # TimePitch preview excerpt range
            Tests/Unit/PreviewSystemTests.cpp                   # shared preview helpers (region + fade gain)
            Tests/Unit/PluginChainPersistenceTests.cpp          # Plugin chain state persistence (XML/JSON + named presets)
            Tests/Unit/LargeOperationUndoTests.cpp              # Undo/redo correctness at large (multi-million-sample) buffer sizes
            Tests/Unit/SettingsPersistenceTests.cpp             # UX finding 1: generic settings round-trip + old-format compat
            Tests/Unit/MonitorFoldDownTests.cpp                 # H7: surround monitoring fold-down matrix (ITU-R BS.775)
            Tests/Unit/AutomationCurveRealtimeReadTests.cpp     # H-H2: lock-free automation curve audio-thread read
            # Tests/EndToEnd/CompleteWorkflowTests.cpp          # Week 5
            # Tests/EndToEnd/StressTests.cpp                    # Week 5
            # Tests/Performance/WaveformRenderingBenchmarks.cpp # Week 6
            # Tests/Performance/PlaybackLatencyBenchmarks.cpp   # Week 6
    )

    # Templates directory path for tests that need to load keymap JSON
    target_compile_definitions(WaveEditTests
        PRIVATE
            WAVEEDIT_TEMPLATES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Templates/Keymaps"
    )

    # Link test executable to WaveEditCore and JUCE test utilities
    target_link_libraries(WaveEditTests
        PRIVATE
            WaveEditCore
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    # Include test utilities
    target_include_directories(WaveEditTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/Tests/TestUtils
    )

        message(STATUS "WaveEdit automated tests configured successfully")
        message(STATUS "Build tests with: cmake --build . --target WaveEditTests")
        message(STATUS "Run tests with: ./WaveEdit_artefacts/Debug/WaveEditTests (or Release)")
    else()
        message(STATUS "Tests/ directory not found - skipping test build")
        message(STATUS "Tests are excluded from repository per .gitignore")
    endif()

endif() # WAVEEDIT_BUILD_TESTS
